/*
 * This file is part of the TrinityCore Project. See AUTHORS file for Copyright information
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include "WorldSession.h"
#include "BattlePayMgr.h"
#include "BattlePayPackets.h"
#include "CharacterCache.h"
#include "DatabaseEnv.h"
#include "DB2Stores.h"
#include "DBCEnums.h"
#include "GameTime.h"
#include "Item.h"
#include "ItemTemplate.h"
#include "ItemEnchantmentMgr.h"
#include "Log.h"
#include "Mail.h"
#include "MiscPackets.h"
#include "ObjectMgr.h"
#include "Player.h"
#include "QueryHolder.h"
#include "RealmList.h"
#include "VasTransferMgr.h"
#include "World.h"
#include "WowTokenMgr.h"
#include <algorithm>
#include <memory>
#include <set>
#include "Timer.h"

namespace
{
    // BattlepayPurchaseStatus / PurchaseResult (extracted from the client enum registrar).
    // STATUS_DONE = 6 per the live 68974 purchase list (TESTER_SNIFF2_LINDORMI_MINE,
    // dump_12.0.7.68974_2026-08-08_02-54-06): all completed purchases carry Status=6, not the
    // enum-registrar Done=3 we previously assumed; a failed VAS flow showed status=12/result=63.
    constexpr int32 STATUS_DONE   = 6;
    constexpr int32 STATUS_FAILED = 4;
    // The client will not open the confirmation prompt, and PurchaseProductConfirm refuses to send the
    // response at all (Buy AND Cancel), unless a JamBattlePayPurchase record for this purchaseID exists
    // in its list with status == 9 and resultCode == 0. Verified at 0x13EB72C / 0x13EB736.
    constexpr int32 STATUS_CONFIRMATION_PENDING = 9;
    constexpr int32 RESULT_OK                       = 0;
    constexpr int32 RESULT_NOT_ENOUGH_BALANCE       = 29;
    constexpr int32 RESULT_PRODUCT_NOT_PURCHASABLE  = 57;

    // Distribution-specific PurchaseResult codes, taken from the client's own generated API
    // documentation (Blizzard_APIDocumentationGenerated/BattlepayConstantsDocumentation.lua), so these
    // values are the client's, not ours.
    constexpr int32 RESULT_NOT_ALLOWED_IN_GLUE_SCREEN = 27;   // ErrorBuyingProductNotAllowedInGlueScreen
    constexpr int32 RESULT_DISTRIBUTION_NOT_FOUND     = 19;   // ErrorDistributionObjectNotFound
    constexpr int32 RESULT_DISTRIBUTION_INVALID_TARGET = 42;  // ErrorDistributionObjectInvalidTarget

    // A single distribution list must stay inside the wire's 11-bit count field; this is far below that
    // and keeps one account from ever pushing a multi-kilobyte list.
    constexpr size_t MAX_LISTED_ENTITLEMENTS = 64;

    // Delivers `count` of `itemId` in full: as much as fits into the player's bags, the remainder by
    // mail (retail parity - a near-full inventory must never turn a full-price purchase into a partial
    // delivery, see audit C-06). Returns false only if the item template is invalid; once past that,
    // bags + mail together always take the whole quantity so the caller may charge safely.
    bool BattlePayDeliverItem(Player* player, uint32 itemId, uint32 count)
    {
        ItemTemplate const* proto = sObjectMgr->GetItemTemplate(itemId);
        if (!proto || !count)
            return false;

        uint32 noSpaceCount = 0;
        ItemPosCountVec dest;
        InventoryResult const msg = player->CanStoreNewItem(NULL_BAG, NULL_SLOT, dest, itemId, count, &noSpaceCount);
        uint32 const toBags = (msg == EQUIP_ERR_OK) ? count : (count - noSpaceCount);

        if (toBags && !dest.empty())
        {
            if (Item* item = player->StoreNewItem(dest, itemId, true, GenerateItemRandomBonusListId(itemId)))
                player->SendNewItem(item, toBags, true, false);
        }

        uint32 remainder = count - toBags;
        if (remainder)
        {
            // Mail the overflow in max-stack chunks.
            CharacterDatabaseTransaction trans = CharacterDatabase.BeginTransaction();
            MailDraft draft("In-game Shop Purchase", "The items you purchased did not all fit in your bags; the remainder is attached.");
            uint32 const maxStack = std::max<uint32>(1u, proto->GetMaxStackSize());
            uint32 attached = 0;
            while (remainder && attached < MAX_MAIL_ITEMS)
            {
                uint32 const stackCount = std::min(remainder, maxStack);
                Item* mailItem = Item::CreateItem(itemId, stackCount, ItemContext::NONE, player);
                if (!mailItem)
                    break;
                mailItem->SaveToDB(trans);
                draft.AddItem(mailItem);
                remainder -= stackCount;
                ++attached;
            }
            draft.SendMailTo(trans, MailReceiver(player, player->GetGUID().GetCounter()), MailSender(MAIL_NORMAL, 0, MAIL_STATIONERY_GM));
            CharacterDatabase.CommitTransaction(trans);

            if (remainder)
            {
                // More than a full mail's worth left over - refuse rather than silently swallow the rest.
                TC_LOG_ERROR("network", "BattlePay: item {} x{} exceeded bag + mail capacity for {}; {} undelivered.",
                    itemId, count, player->GetName(), remainder);
                return false;
            }
        }
        return true;
    }
}

// In-game Shop (BattlePay). P0: reply to the catalog request with the captured, client-validated
// product list so the shop opens and displays real products. If no catalog blob is loaded we send
// nothing (shop opens empty) rather than fabricating wire.
void WorldSession::HandleBattlePayGetProductList(WorldPackets::BattlePay::GetProductList& /*getProductList*/)
{
    if (!sWorld->getBoolConfig(CONFIG_SHOP_ENABLED))
        return;

    if (!sBattlePayMgr->HasCatalog())
    {
        TC_LOG_DEBUG("network", "BattlePay: GetProductList from {} but no catalog loaded.", GetPlayerInfo());
        return;
    }

    // Throttle the 58 KB blob: serve it at most once per catalog generation for this session. The client
    // re-requests on every shop open, so without this a STATUS_AUTHED session could pull it repeatedly.
    // A `.reload shop_catalog` bumps the generation, so a fresh catalog still reaches the next request.
    uint32 const generation = sBattlePayMgr->GetCatalogGeneration();
    if (_battlePayCatalogGeneration == generation)
    {
        TC_LOG_DEBUG("network", "BattlePay: GetProductList from {} already served catalog gen {}.", GetPlayerInfo(), generation);
        return;
    }

    WorldPackets::BattlePay::ProductListResponse response;
    response.RawData = &sBattlePayMgr->GetProductListBlob();
    SendPacket(response.Write());
    _battlePayCatalogGeneration = generation;
}

// Drives the purchase to completion for a given productID: validate, charge (gold or a token item),
// grant, then send the client the start-purchase ack + a PurchaseUpdate (Done/Failed). The granted
// item/spell reaches the client over the normal item/collection packets regardless of the shop UI.
void WorldSession::BattlePayProcessPurchase(uint32 productID)
{
    Player* player = GetPlayer();
    uint64 const purchaseID = sBattlePayMgr->GeneratePurchaseID();

    auto respond = [&](int32 status, int32 resultCode, uint64 price)
    {
        WorldPackets::BattlePay::StartPurchaseResponse ack;
        ack.PurchaseID = purchaseID;
        ack.ResultB = uint32(resultCode);
        SendPacket(ack.Write());

        WorldPackets::BattlePay::PurchaseUpdate update;
        WorldPackets::BattlePay::PurchaseRecord& rec = update.Purchases.emplace_back();
        rec.PurchaseID = purchaseID;
        rec.Status = status;
        rec.ResultCode = resultCode;
        rec.ProductID = productID;
        rec.BasePrice = price;
        rec.UserPrice = price;
        SendPacket(update.Write());

        // Persist completed purchases to the shared ledger so GetPurchaseList answers from real history
        // and the PurchaseID is durable across restarts (C-13/C-32). Failed attempts are not recorded.
        if (status == STATUS_DONE)
            sBattlePayMgr->RecordPurchase(GetAccountId(), purchaseID, status, resultCode, productID, price, price);
    };

    // Resolve the advertised (slot) productID to its admin product via the catalog routing map.
    // Placeholder / unrouted slots have no product -> not purchasable. The lookup itself needs no
    // player, so it also works at character select.
    ShopProduct const* product = sBattlePayMgr->GetProductByAdvertisedId(productID);
    if (!product)
    {
        // Most cards in the shipped catalog are UNTOUCHED retail records - only the slots we reskin
        // are routed to a real ShopProduct. Buying any other card lands here. Logged at INFO because
        // it is the difference between "the wire is broken" and "that item is not one of ours".
        TC_LOG_INFO("network", "BattlePay: {} tried to buy product {}, which is not routed to a shop_product "
            "(catalog card is an un-reskinned retail entry).", GetPlayerInfo(), productID);
        respond(STATUS_FAILED, RESULT_PRODUCT_NOT_PURCHASABLE, 0);
        return;
    }

    time_t const now = GameTime::GetGameTime();
    bool const entitlementsEnabled = sWorld->getBoolConfig(CONFIG_SHOP_ENTITLEMENTS_ENABLED);

    // A product with no deliverables is display-only (e.g. the template's showcase mounts/pets): visible
    // in the catalog but not for sale, so it must never report a successful purchase.
    if (product->Deliverables.empty())
    {
        respond(STATUS_FAILED, RESULT_PRODUCT_NOT_PURCHASABLE, 0);
        return;
    }

    // ---- Character select: no player, so nothing can be granted and nothing can be charged ----------
    //
    // This is the case the Shop hits when it is opened from the glue screen. Previously it always failed
    // (the product lookup was gated on `player`). With entitlements on, the purchase instead creates an
    // owned-but-unapplied entitlement the player assigns to a character later.
    //
    // Only free products can be bought here, and that is a real constraint rather than a shortcut: the
    // price is denominated in a character's gold, and at character select there is no character to take
    // it from. Anything with a price is refused with the client's own
    // ErrorBuyingProductNotAllowedInGlueScreen, which is exactly what that code is for.
    if (!player)
    {
        if (!entitlementsEnabled)
        {
            TC_LOG_INFO("network", "BattlePay: {} tried to buy product {} at character select, but "
                "Shop.Entitlements.Enabled is off.", GetPlayerInfo(), productID);
            respond(STATUS_FAILED, RESULT_NOT_ALLOWED_IN_GLUE_SCREEN, 0);
            return;
        }

        if (!product->Enabled || (product->AvailableFrom && now < product->AvailableFrom)
            || (product->AvailableUntil && now > product->AvailableUntil))
        {
            respond(STATUS_FAILED, RESULT_PRODUCT_NOT_PURCHASABLE, 0);
            return;
        }

        if (product->Currency != 0 || product->Price || product->PriceItemId)
        {
            TC_LOG_INFO("network", "BattlePay: {} tried to buy priced product {} at character select; only "
                "free products can be bought without a character to charge.", GetPlayerInfo(), productID);
            respond(STATUS_FAILED, RESULT_NOT_ALLOWED_IN_GLUE_SCREEN, 0);
            return;
        }

        int32 const result = BattlePayCreateEntitlement(*product, purchaseID);
        respond(result == RESULT_OK ? STATUS_DONE : STATUS_FAILED, result, 0);
        return;
    }

    // Authoritative server-side gate for everything the wire cannot express (enabled/window/level/
    // faction/hideIfOwned/condition). Also refuse a spell-only product the player already fully owns
    // so a repeat purchase never takes gold for nothing (audit C-05), regardless of the HideIfOwned flag.
    if (!sBattlePayMgr->IsPurchasable(*product, player, now)
        || BattlePayMgr::IsAlreadyFullyOwned(*product, player))
    {
        respond(STATUS_FAILED, RESULT_PRODUCT_NOT_PURCHASABLE, 0);
        return;
    }

    // A service deliverable (type 5) has no immediate form at all: its target is a character chosen
    // AFTER the purchase, which is precisely what an entitlement is for. With entitlements on, such a
    // product is sold as an entitlement even to a logged-in buyer.
    bool const deferred = entitlementsEnabled && BattlePayMgr::NeedsEntitlement(*product, player);

    // Reserved / unavailable deliverable types abort the whole purchase BEFORE charging: type 4 (game
    // time) is schema-reserved with no delivery impl, and type 5 (service) lands here only when
    // entitlements are off. Type 3 (WoW Token) is delivered through WowTokenMgr in the grant loop below.
    if (!deferred)
    {
        for (ShopDeliverable const& d : product->Deliverables)
        {
            if (d.Type < 1 || d.Type > 3)
            {
                TC_LOG_DEBUG("network", "BattlePay: product {} has unsupported deliverable type {} - refused.", productID, d.Type);
                respond(STATUS_FAILED, RESULT_PRODUCT_NOT_PURCHASABLE, 0);
                return;
            }
        }
    }

    // Cost check by currency (1 = gold copper, 2 = item token).
    if (product->Currency == 1 && product->Price && !player->HasEnoughMoney(product->Price))
    {
        respond(STATUS_FAILED, RESULT_NOT_ENOUGH_BALANCE, product->Price);
        return;
    }
    if (product->Currency == 2 && product->PriceItemId && !player->HasItemCount(product->PriceItemId, product->PriceItemCount))
    {
        respond(STATUS_FAILED, RESULT_NOT_ENOUGH_BALANCE, product->Price);
        return;
    }

    // ---- Deferred (entitlement) purchase by a logged-in player -------------------------------------
    //
    // Charge first and commit it synchronously, THEN create the entitlement. This inverts the normal
    // grant-before-charge rule for the same reason the WoW Token path does: the entitlement is written
    // to the auth DB immediately and durably, while the buyer's gold would otherwise sit in Player
    // memory until the next periodic save. Charging last would mean a crash in that (minutes-long)
    // window left the account holding a paid-for entitlement that was never paid for - the server
    // losing value, which is the one outcome we never accept. The surviving failure is "charged, no
    // entitlement", and that path refunds explicitly below.
    if (deferred)
    {
        if (product->Currency == 1 && product->Price)
            player->ModifyMoney(-int64(product->Price));
        if (product->Currency == 2 && product->PriceItemId)
            player->DestroyItemCount(product->PriceItemId, product->PriceItemCount, true);

        {
            CharacterDatabaseTransaction trans = CharacterDatabase.BeginTransaction();
            player->SaveInventoryAndGoldToDB(trans);
            CharacterDatabase.DirectCommitTransaction(trans);
        }

        int32 const result = BattlePayCreateEntitlement(*product, purchaseID);
        if (result != RESULT_OK)
        {
            // Give the price back: the player paid and got nothing.
            if (product->Currency == 1 && product->Price)
                player->ModifyMoney(int64(product->Price));
            if (product->Currency == 2 && product->PriceItemId)
                BattlePayDeliverItem(player, product->PriceItemId, product->PriceItemCount);

            CharacterDatabaseTransaction trans = CharacterDatabase.BeginTransaction();
            player->SaveInventoryAndGoldToDB(trans);
            CharacterDatabase.DirectCommitTransaction(trans);

            TC_LOG_ERROR("network", "BattlePay: entitlement for product {} could not be created for {}; "
                "the price was refunded.", productID, GetPlayerInfo());
            respond(STATUS_FAILED, result, product->Price);
            return;
        }

        TC_LOG_INFO("network", "BattlePay: {} purchased product {} ({}) as an entitlement for {} (currency {}).",
            GetPlayerInfo(), productID, product->Name, product->Price, product->Currency);
        respond(STATUS_DONE, RESULT_OK, product->Price);
        return;
    }

    // Grant first; only charge if every deliverable succeeds so we never take payment without delivering.
    // Exception: a WoW Token (type 3) is charged *before* creation and committed synchronously - see the
    // anti-abuse note in that case; chargeSettled records that so the generic post-grant charge is skipped.
    bool granted = true;
    bool chargeSettled = false;
    for (ShopDeliverable const& d : product->Deliverables)
    {
        switch (d.Type)
        {
            case 1: // item - full delivery to bags, overflow to mail (no partial-stack-at-full-price)
                if (!BattlePayDeliverItem(player, d.Id, d.Count))
                    granted = false;
                break;
            case 2: // spell (mount / toy / appearance) - LearnSpell routes it into the account-wide
                    // collection via CollectionMgr; LearnSpell no-ops if a bundled spell is already known
                if (!player->HasSpell(d.Id))
                    player->LearnSpell(d.Id, false);
                break;
            case 3: // WoW Token - the retail acquisition path: bought from the Shop, then account-sellable.
                    // This is THE shop<->token synergy: the catalog-admin deliverable drives WowTokenMgr.
                    // ANTI-ABUSE (C-07, TK-5, the audit's #1 finding): a token is persisted to the AUTH DB
                    // the instant it is created, while the buyer's gold otherwise stays in Player memory
                    // until the next periodic character save. A crash in that (minutes-long) window would
                    // leave the account holding the token with the gold never taken - a free token / free
                    // gold duplication. Close the window: charge the cost and commit it to the character DB
                    // *synchronously first*, then create the token. The only surviving crash outcome is
                    // "gold taken, token not created" - a refundable player loss, never "keep gold AND
                    // token" (the server never loses value).
                if (product->Currency == 1 && product->Price)
                    player->ModifyMoney(-int64(product->Price));
                if (product->Currency == 2 && product->PriceItemId)
                    player->DestroyItemCount(product->PriceItemId, product->PriceItemCount, true);
                {
                    CharacterDatabaseTransaction trans = CharacterDatabase.BeginTransaction();
                    player->SaveInventoryAndGoldToDB(trans);
                    CharacterDatabase.DirectCommitTransaction(trans);
                }
                chargeSettled = true;

                for (uint32 i = 0; i < std::max<uint32>(d.Count, 1u); ++i)
                    sWowTokenMgr->CreateToken(GetAccountId(), WOW_TOKEN_STATE_AUCTIONABLE);

                // Confirmed trigger for this push: the account's token holdings changed.
                SendCommerceTokenUpdate();
                break;
            default:
                break;
        }
        if (!granted)
            break;
    }

    if (!granted)
    {
        TC_LOG_DEBUG("network", "BattlePay: grant failed for product {} ({}), {} not charged.",
            productID, product->Name, GetPlayerInfo());
        respond(STATUS_FAILED, RESULT_PRODUCT_NOT_PURCHASABLE, product->Price);
        return;
    }

    // Types 1/2 are charged here after a successful grant; a type-3 token already settled its charge
    // synchronously above (chargeSettled), so it must not be charged again.
    if (!chargeSettled)
    {
        if (product->Currency == 1 && product->Price)
            player->ModifyMoney(-int64(product->Price));
        if (product->Currency == 2 && product->PriceItemId)
            player->DestroyItemCount(product->PriceItemId, product->PriceItemCount, true);
    }

    TC_LOG_INFO("network", "BattlePay: {} purchased product {} ({}) for {} (currency {}).",
        GetPlayerInfo(), productID, product->Name, product->Price, product->Currency);

    // Announce the delivery before the completing PURCHASE_UPDATE - see SendBattlePayDeliveryNotifications.
    SendBattlePayDeliveryNotifications(*product, purchaseID);

    respond(STATUS_DONE, RESULT_OK, product->Price);
}

// Tells the client that the payload of `product` has just been handed over, so its collection UI
// refreshes instead of waiting for a reload.
//
// WHY HERE: this is called from the two places where delivery actually happens - the immediate grant loop
// in BattlePayProcessPurchase, and the entitlement redemption loop in RedeemBattlePayEntitlements. Both
// call it AFTER every deliverable has succeeded and after the charge has settled, and BEFORE the
// completing SMSG_BATTLE_PAY_PURCHASE_UPDATE goes out. That ordering is deliberate: the client's Shop
// closes the purchase on the PURCHASE_UPDATE, so the collection refresh has to have been requested
// before then for the new mount or toy to be present when the frame comes back. It also means a purchase
// that fails or gets refunded never announces a delivery, because those paths return earlier.
//
// The per-deliverable opcode is chosen by what was actually granted, but note (see BattlePayPackets.h)
// that the 12.0.7 client routes MOUNT_DELIVERED and COLLECTION_ITEM_DELIVERED to the same handler and the
// same Lua event, so the distinction is honesty about what we sent rather than a behavioural difference.
void WorldSession::SendBattlePayDeliveryNotifications(ShopProduct const& product, uint64 purchaseID)
{
    for (ShopDeliverable const& d : product.Deliverables)
    {
        OpcodeServer opcode;
        switch (d.Type)
        {
            case 1:     // item / toy -> a collection item
                opcode = SMSG_BATTLE_PAY_COLLECTION_ITEM_DELIVERED;
                break;
            case 2:     // spell: a mount spell is a mount, anything else lands in the collection
                opcode = sDB2Manager.GetMount(d.Id) ? SMSG_BATTLE_PAY_MOUNT_DELIVERED
                                                    : SMSG_BATTLE_PAY_COLLECTION_ITEM_DELIVERED;
                break;
            default:    // type 3 (WoW Token) already pushes its own SendCommerceTokenUpdate; nothing else
                        // in this vocabulary delivers into a collection.
                continue;
        }

        WorldPackets::BattlePay::DeliveryNotification notification(opcode);
        SendPacket(notification.Write());
    }

    WorldPackets::BattlePay::DeliveryEnded ended;
    ended.PurchaseID = purchaseID;
    ended.Products.emplace_back().ProductID = product.ProductID;
    SendPacket(ended.Write());
}

void WorldSession::HandleBattlePayStartPurchase(WorldPackets::BattlePay::StartPurchase& startPurchase)
{
    if (!sWorld->getBoolConfig(CONFIG_SHOP_ENABLED))
        return;

    // The productID scalar is a strong candidate (setter is Warden-obfuscated). Log all three fields so a
    // live purchase confirms which scalar is the productID; GetProduct() also guards against a wrong guess.
    TC_LOG_INFO("network", "BattlePay: StartPurchase from {}: clientToken={} productID={} flag={}",
        GetPlayerInfo(), startPurchase.ClientToken, startPurchase.ProductID, uint32(startPurchase.Flag));

    // ANTI-ABUSE (C-13): collapse replayed / double-clicked purchases to a single charge. CMSG_START_PURCHASE
    // is craftable by any logged-in client and there is no client-supplied idempotency key. A lagged
    // double-click sends two CMSGs which the world thread runs back-to-back, so the first has already
    // completed (and charged) before the second begins - an in-flight flag alone cannot see it. A short
    // per-session throttle does: reject a second StartPurchase within BATTLEPAY_PURCHASE_THROTTLE_MS.
    // The duplicate is dropped silently (no charge, no ack) so the first purchase's response still drives
    // the UI; the in-flight flag additionally guards against any future re-entrancy. Applied to both the
    // direct and the two-step confirmation path (the timestamp is stamped before either runs).
    static constexpr uint32 BATTLEPAY_PURCHASE_THROTTLE_MS = 2000;
    uint32 const now = getMSTime();
    if (_battlePayPurchaseInFlight ||
        (_lastBattlePayPurchaseMSTime && getMSTimeDiff(_lastBattlePayPurchaseMSTime, now) < BATTLEPAY_PURCHASE_THROTTLE_MS))
    {
        TC_LOG_DEBUG("network", "BattlePay: throttled duplicate StartPurchase from {} (product {}).",
            GetPlayerInfo(), startPurchase.ProductID);
        return;
    }
    _lastBattlePayPurchaseMSTime = now;

    // Two-step retail confirmation flow, and the default: stash the pending product, prompt the client, and
    // complete the purchase only when it answers CMSG_BATTLE_PAY_CONFIRM_PURCHASE_RESPONSE.
    //
    // The branch below is a fallback for operators who deliberately disable the handshake, and it does NOT
    // finish the transaction from the client's point of view. Blizzard_Shared_StoreUISecure sets
    // WaitingOnConfirmation when Buy is pressed and clears it only on STORE_CONFIRM_PURCHASE, which nothing
    // but SMSG_BATTLE_PAY_CONFIRM_PURCHASE raises - STORE_PURCHASE_LIST_UPDATED does not. So the direct path
    // charges the player, delivers the goods, and leaves the shop spinning on "Connecting to the shop".
    //
    // The old comment here said this was off by default because the confirm layout was inferred. That is
    // stale: the layout was recovered and verified against the client, and it is the direct path that is
    // now known to be the incomplete one.
    if (sWorld->getBoolConfig(CONFIG_SHOP_PURCHASE_CONFIRMATION))
    {
        Player* player = GetPlayer();
        ShopProduct const* product = player ? sBattlePayMgr->GetProductByAdvertisedId(startPurchase.ProductID) : nullptr;
        if (!product)
        {
            WorldPackets::BattlePay::StartPurchaseResponse ack;
            ack.PurchaseID = sBattlePayMgr->GeneratePurchaseID();
            ack.ResultB = uint32(RESULT_PRODUCT_NOT_PURCHASABLE);
            SendPacket(ack.Write());
            return;
        }

        uint64 const purchaseID = sBattlePayMgr->GeneratePurchaseID();
        _battlePayPendingProductID = startPurchase.ProductID;
        _battlePayConfirmToken = uint32(purchaseID) | 0x1u;     // non-zero token = a purchase is pending

        // ORDER MATTERS. The confirmation dialog reads the product name, wallet and both prices out of
        // the client's own purchase RECORD, not out of the confirm packet - GetConfirmationInfo returns
        // nil when no record for this purchaseID exists, and the Lua then hides the frame. So the record
        // has to arrive first: StartPurchaseResponse, then a PURCHASE_UPDATE carrying one record in
        // status 9, and only then the 12-byte confirm packet.
        // Sending the confirm packet on its own (what we did before) is why the client never answered.
        {
            WorldPackets::BattlePay::StartPurchaseResponse ack;
            ack.PurchaseID = purchaseID;
            ack.ResultB = uint32(RESULT_OK);
            SendPacket(ack.Write());
        }

        {
            // Prices are echoed back by the client from what it DISPLAYS, so keep them whole cents -
            // sub-cent precision can never compare equal on the way back.
            uint64 const price = product->Currency == 1 ? (product->Price / 10000) * 100 : 0;

            WorldPackets::BattlePay::PurchaseUpdate update;
            WorldPackets::BattlePay::PurchaseRecord& rec = update.Purchases.emplace_back();
            rec.PurchaseID = purchaseID;
            rec.Status     = STATUS_CONFIRMATION_PENDING;
            rec.ResultCode = int32(RESULT_OK);
            rec.ProductID  = startPurchase.ProductID;
            rec.BasePrice  = price;
            rec.UserPrice  = price;
            SendPacket(update.Write());
        }

        WorldPackets::BattlePay::ConfirmPurchase confirm;
        confirm.PurchaseID = purchaseID;
        confirm.ServerToken = _battlePayConfirmToken;
        SendPacket(confirm.Write());
        return;
    }

    _battlePayPurchaseInFlight = true;
    BattlePayProcessPurchase(startPurchase.ProductID);
    _battlePayPurchaseInFlight = false;
}

void WorldSession::HandleBattlePayConfirmPurchaseResponse(WorldPackets::BattlePay::ConfirmPurchaseResponse& confirmPurchaseResponse)
{
    if (!sWorld->getBoolConfig(CONFIG_SHOP_ENABLED))
        return;

    // Ignore stale/unsolicited responses (guards against a replayed confirm double-charging).
    if (!_battlePayConfirmToken || confirmPurchaseResponse.ServerToken != _battlePayConfirmToken)
    {
        TC_LOG_DEBUG("network", "BattlePay: ConfirmPurchaseResponse from {} with unexpected token {} (pending {}).",
            GetPlayerInfo(), confirmPurchaseResponse.ServerToken, _battlePayConfirmToken);
        return;
    }

    uint32 const productID = _battlePayPendingProductID;
    _battlePayConfirmToken = 0;          // consume the pending purchase before doing any work
    _battlePayPendingProductID = 0;

    if (!confirmPurchaseResponse.Confirmed)
    {
        TC_LOG_INFO("network", "BattlePay: {} cancelled purchase of product {}.", GetPlayerInfo(), productID);
        return;
    }

    BattlePayProcessPurchase(productID);
}

void WorldSession::HandleBattlePayOpenCheckout(WorldPackets::BattlePay::OpenCheckout& openCheckout)
{
    if (!sWorld->getBoolConfig(CONFIG_SHOP_ENABLED))
        return;

    // Two payment rails, decided by the checked-out product (resolve the advertised productID -> catalog):
    //
    // Rail B - IN-GAME CURRENCY. A product we route whose Currency is anything but real money is bought on
    //   the wire. Keep the proven handshake: SMSG_GENERATE_SSO_TOKEN_RESPONSE echoing the request's
    //   ClientToken 1:1 (COMMERCE_AUDIT C-09 / WOW_TOKEN_RE_68275.md). Answering from here - not from an
    //   unsolicited push at login - is what lets checkouts #2+ get a reply.
    //
    // Rail A - REAL MONEY. Either a product explicitly flagged Currency 4 (real money), OR an un-reskinned
    //   retail catalog card we do not route to a shop_product (e.g. the Midnight expansion, ProductID
    //   0x417070). The 12.1.0.69382 sniff proves the client gets NO game-packet response for such a
    //   product and opens the shop2 web overlay itself; the old 1:1 SSO echo here was only ever correct for
    //   in-game-currency items. Behaviour is governed by Shop.RealMoney.Mode.
    ShopProduct const* product = sBattlePayMgr->GetProductByAdvertisedId(openCheckout.ProductID);
    if (product && product->Currency != SHOP_CURRENCY_REAL_MONEY)
    {
        SendGenerateSsoToken(openCheckout.ClientToken);
        return;
    }

    switch (BattlePayMgr::GetRealMoneyMode())
    {
        case SHOP_REAL_MONEY_INSTANT:
        {
            // Grant the product server-side with no charge (private-realm QA default). Reuse the proven
            // delivery path: a real-money product carries Currency 4, which the charge checks (== gold,
            // == item token) never match, so nothing is deducted. An unrouted retail card has no
            // shop_product to grant - honest no-op rather than a faked success.
            if (!product)
            {
                TC_LOG_INFO("network", "BattlePay: {} opened a real-money checkout for unrouted product {} with "
                    "Shop.RealMoney.Mode=instant, but no shop_product is seeded for it - nothing to grant.",
                    GetPlayerInfo(), openCheckout.ProductID);
                return;
            }

            // The client resends OPEN_CHECKOUT on each web-payment failure (7 retries in the capture);
            // throttle identical rapid retries so instant mode grants exactly once, reusing the same
            // per-session guard as the StartPurchase path.
            static constexpr uint32 BATTLEPAY_INSTANT_THROTTLE_MS = 2000;
            uint32 const now = getMSTime();
            if (_battlePayPurchaseInFlight ||
                (_lastBattlePayPurchaseMSTime && getMSTimeDiff(_lastBattlePayPurchaseMSTime, now) < BATTLEPAY_INSTANT_THROTTLE_MS))
            {
                TC_LOG_DEBUG("network", "BattlePay: throttled duplicate real-money instant checkout from {} (product {}).",
                    GetPlayerInfo(), openCheckout.ProductID);
                return;
            }
            _lastBattlePayPurchaseMSTime = now;

            TC_LOG_INFO("network", "BattlePay: {} real-money checkout for product {} granted instantly "
                "(Shop.RealMoney.Mode=instant, no charge).", GetPlayerInfo(), openCheckout.ProductID);

            _battlePayPurchaseInFlight = true;
            BattlePayProcessPurchase(openCheckout.ProductID);
            _battlePayPurchaseInFlight = false;
            return;
        }
        case SHOP_REAL_MONEY_DISABLED:
            // Refuse politely: no SSO token, no grant. The client opened its own web overlay on
            // OPEN_CHECKOUT and the realm cannot recall that, but it neither authenticates the web session
            // nor delivers anything, so the purchase cannot complete. Logged for the operator.
            TC_LOG_INFO("network", "BattlePay: {} attempted a real-money checkout for product {} but "
                "Shop.RealMoney.Mode=disabled - refused (no SSO, no grant).", GetPlayerInfo(), openCheckout.ProductID);
            return;
        case SHOP_REAL_MONEY_WEB:
        default:
            // Sniff-accurate: the client already opened the shop2 HTTPS overlay from OPEN_CHECKOUT, so the
            // game connection sends NO response. Completion, if any, arrives from the web backend (Rail A3).
            TC_LOG_DEBUG("network", "BattlePay: {} opened a real-money web checkout for product {} (client token {}); "
                "no game response (Shop.RealMoney.Mode=web).", GetPlayerInfo(), openCheckout.ProductID, openCheckout.ClientToken);
            return;
    }
}

// CMSG_CATALOG_SHOP_LICENSE_GAME_DATA_REQUEST. The client sends this mid-checkout carrying its own
// license / game data (variable body: 876/140/52/36/24 bytes in the 12.1.0.69382 capture). Retail answers
// on SMSG opcode 0x4202C1 - which our opcode map currently mis-names SMSG_VAS_GET_QUEUE_MINUTES_RESPONSE
// (the sniff's 0x4502C1 is that opcode plus the sniff's fixed 0x030000 server-opcode bias) - with an
// 11465/1025/423/403/40-byte payload.
//
// That response is a nested reflection bitstream whose FIELD LAYOUT is not recovered - the capture gives
// only the sizes, not the member types. Emitting a guessed layout on that opcode would risk the client's
// reflection parser rejecting it and disconnecting, which is exactly the "do not invent wire" hazard. So
// this handler does the honest, safe thing: it makes the opcode an EXPECTED, real handler (no more
// STATUS_UNHANDLED / Handle_NULL), logs the body size so a future capture can be matched to a variant, and
// sends nothing. The 69382 capture shows the checkout proceeds (the client drives its own web overlay from
// OPEN_CHECKOUT) without the realm answering this, so withholding the un-modeled response does not stall
// the client. When the response wire is recovered, this is where it is sent.
void WorldSession::HandleCatalogShopLicenseGameDataRequest(WorldPackets::BattlePay::CatalogShopLicenseGameDataRequest& request)
{
    if (!sWorld->getBoolConfig(CONFIG_SHOP_ENABLED))
        return;

    TC_LOG_INFO("network", "BattlePay: CATALOG_SHOP_LICENSE_GAME_DATA_REQUEST from {} ({} body bytes). The paired "
        "response (SMSG 0x4202C1) is an un-modeled reflection payload and is deliberately not answered - see the "
        "handler note. If the client stalls waiting for it, capture the response bytes and model them here.",
        GetPlayerInfo(), request.Data.size());
}

namespace
{
    // Renders one product as the JamBattlePayDeliverable the client parses. The same struct is embedded
    // in a DistributionObject and listed by SMSG_SYNC_WOW_ENTITLEMENTS, and both must describe a product
    // identically - so both go through here. Returns false when the product is unknown or display-only,
    // i.e. there is no deliverable to write at all.
    //
    // `DeliverableID` is the product id because this core has no separate deliverable-id namespace -
    // shop_product_deliverable is keyed (productId, seq). The client does not need it to resolve
    // anything: the full deliverable record travels inline with it, exactly as in the capture.
    bool BuildDeliverable(uint32 productID, WorldPackets::BattlePay::DistributionDeliverable& deliverable)
    {
        ShopProduct const* product = sBattlePayMgr->GetProduct(productID);
        if (!product || product->Deliverables.empty())
            return false;

        deliverable.DeliverableID = productID;
        deliverable.Name = product->Name.substr(0, 255);        // client buffer is char[256]

        // Map our deliverable vocabulary onto the catalog's. A type-5 (service) row carries the
        // catalog's own deliverable type in `id` (1 CharacterBoost, 5 NameChange, 6 FactionChange,
        // 8 RaceChange, 11 CharacterTransfer), so it passes straight through.
        ShopDeliverable const& first = product->Deliverables.front();
        switch (first.Type)
        {
            case 1:                                             // item
                deliverable.Type = 14;                          // Item/Toy
                deliverable.ItemID = first.Id;
                deliverable.Quantity = first.Count;
                break;
            case 2:                                             // spell (mount / toy / appearance)
                deliverable.Type = 3;                           // Mount
                deliverable.MountSpellID = first.Id;
                break;
            case 3:                                             // WoW Token
                deliverable.Type = 4;                           // WowToken
                deliverable.Quantity = first.Count;
                break;
            case 5:                                             // service
                deliverable.Type = first.Id;
                // A CharacterBoost deliverable also carries a BoostID, and the client compares it with
                // the ActiveBoostType / TrialBoostType the glue screen was given - a mismatch describes
                // one boost in the character list and a different one in the Shop. Same source for both
                // (Shop.CharacterBoost.BoostType) so they cannot drift apart.
                if (first.Id == SHOP_SERVICE_CHARACTER_BOOST)
                    deliverable.BoostID = uint32(BattlePayMgr::GetCharacterBoostType());
                break;
            default:
                break;
        }

        return true;
    }

    // Renders one entitlement as the wire object the client parses. The structure is proven (see
    // DistributionObject in BattlePayPackets.h); what is OURS to choose is which values go in it.
    void BuildDistributionObject(ShopEntitlement const& entitlement, WorldPackets::BattlePay::DistributionObject& out)
    {
        out.DistributionID = entitlement.DistributionID;
        out.Status         = entitlement.Status;
        out.DeliverableID  = entitlement.ProductID;
        out.PurchaseID     = entitlement.PurchaseID;
        // LicenseGameAccountGUID / TargetPlayer stay empty: an unassigned entitlement has no target, and
        // the capture's licence guid has no server-side meaning we could reproduce honestly.

        WorldPackets::BattlePay::DistributionDeliverable deliverable;
        if (!BuildDeliverable(entitlement.ProductID, deliverable))
            return;                                 // hasDeliverable stays 0 - structurally valid

        out.Deliverable = std::move(deliverable);
    }
}

// Sent unsolicited at session start (character select) and again at login. There is no CMSG that
// requests it - the client has no such opcode in this build. The client's StoreFrame_IsLoading gate
// keeps the Shop on "Loading, please wait" until C_StoreSecure.HasDistributionList() flips, which this
// response does.
//
// With entitlements off this replays the captured blob exactly as before. With entitlements on it
// answers from this account's real, unapplied entitlements - so the load has to finish first.
void WorldSession::SendBattlePayDistributionList()
{
    if (!sWorld->getBoolConfig(CONFIG_SHOP_ENABLED))
        return;

    if (sWorld->getBoolConfig(CONFIG_SHOP_ENTITLEMENTS_ENABLED))
    {
        LoadBattlePayEntitlements(true);
        return;
    }

    if (!sBattlePayMgr->HasDistributionList())
        return;

    WorldPackets::BattlePay::GetDistributionListResponse response;
    response.RawData = &sBattlePayMgr->GetDistributionListBlob();
    SendPacket(response.Write());
}

// Refreshes this session's cached entitlements from the account store, optionally pushing the list
// afterwards. Async like the purchase list; the callback runs on the world thread via _queryProcessor,
// which is serviced at character select as well as in world.
void WorldSession::LoadBattlePayEntitlements(bool sendList)
{
    if (!sWorld->getBoolConfig(CONFIG_SHOP_ENABLED) || !sWorld->getBoolConfig(CONFIG_SHOP_ENTITLEMENTS_ENABLED))
        return;

    LoginDatabasePreparedStatement* stmt = LoginDatabase.GetPreparedStatement(LOGIN_SEL_BATTLEPAY_ENTITLEMENT_ACCOUNT);
    stmt->setUInt32(0, GetAccountId());

    _queryProcessor.AddCallback(LoginDatabase.AsyncQuery(stmt)
        .WithPreparedCallback([this, sendList](PreparedQueryResult result)
    {
        _battlePayEntitlements.clear();
        if (result)
        {
            do
            {
                Field* fields = result->Fetch();
                ShopEntitlement& e = _battlePayEntitlements.emplace_back();
                e.DistributionID = fields[0].GetUInt64();
                e.ProductID      = fields[1].GetUInt32();
                e.ServiceType    = fields[2].GetUInt8();
                e.Status         = fields[3].GetUInt8();
                e.PurchaseID     = fields[4].GetUInt64();
                e.CreateTime     = fields[5].GetInt64();
            }
            while (result->NextRow());
        }

        // The glue screen's boost UI is driven by FeatureSystemStatusGlueScreen, which has already gone
        // out by the time the first load finishes - and it goes stale again the moment a boost is bought
        // or spent. Re-send it whenever what we would advertise no longer matches what we did advertise,
        // so "Boost a character" appears (and disappears) without a relog. Only at character select: in
        // world this is the wrong packet entirely.
        if (!GetPlayer() && HasBattlePayCharacterBoost() != _shopBoostAdvertised)
            SendFeatureSystemStatusGlueScreen();

        if (sendList)
            SendBattlePayDistributionListNow();
    }));
}

void WorldSession::SendBattlePayDistributionListNow()
{
    WorldPackets::BattlePay::GetDistributionListResponse response;
    response.BuildFromObjects = true;
    response.Result = RESULT_OK;

    for (ShopEntitlement const& e : _battlePayEntitlements)
    {
        if (e.Status != SHOP_ENTITLEMENT_AVAILABLE)
            continue;
        if (response.Distributions.size() >= MAX_LISTED_ENTITLEMENTS)
            break;
        BuildDistributionObject(e, response.Distributions.emplace_back());
    }

    SendPacket(response.Write());

    // The captures pair these two: the entitlement ledger goes out in the same block as the
    // distribution list, at character select and again on entering the world.
    SendBattlePayEntitlementSync();
}

// Pushes one entitlement so the client fires PRODUCT_DISTRIBUTIONS_UPDATED and refreshes its token row.
void WorldSession::SendBattlePayDistributionUpdate(ShopEntitlement const& entitlement)
{
    WorldPackets::BattlePay::DistributionUpdate update;
    BuildDistributionObject(entitlement, update.Distribution);
    SendPacket(update.Write());
}

// The account's entitlement ledger - "what this account has bought" - which is what lets the Shop show
// a product as already owned rather than offering it again.
//
// This is deliberately a WIDER set than the distribution list. The distribution list carries only
// entitlements still awaiting assignment (AVAILABLE); ownership does not end when an entitlement is
// applied to a character, so anything that reached CLAIMED, BOUND or FINISHED still belongs in the
// ledger. REVOKED is the one status excluded: a refunded purchase is no longer owned.
//
// Every field we cannot source is sent as zero, which is exactly what the captures show for all 92 of
// the 93 rows we decoded: a permanent, fully-available entitlement with no expiry and no manual review.
// Nothing here is invented - the deliverable definitions come from our own catalog, and the entitlement
// rows from our own battlepay_entitlement table.
void WorldSession::SendBattlePayEntitlementSync()
{
    if (!sWorld->getBoolConfig(CONFIG_SHOP_ENABLED) || !sWorld->getBoolConfig(CONFIG_SHOP_ENTITLEMENTS_ENABLED))
        return;

    WorldPackets::BattlePay::SyncWowEntitlements sync;

    // One row per owned deliverable. An account can hold several entitlements for the same product
    // (buy the same pet twice), but the ledger is keyed by deliverable and every captured body had
    // strictly ascending, unique DeliverableIDs - so collapse duplicates rather than emit a shape the
    // client has never been sent.
    std::set<uint32> seen;
    for (ShopEntitlement const& e : _battlePayEntitlements)
    {
        if (e.Status == SHOP_ENTITLEMENT_NONE || e.Status == SHOP_ENTITLEMENT_REVOKED)
            continue;
        if (sync.Entitlements.size() >= MAX_LISTED_ENTITLEMENTS)
            break;
        if (!seen.insert(e.ProductID).second)
            continue;

        WorldPackets::BattlePay::DistributionDeliverable deliverable;
        if (!BuildDeliverable(e.ProductID, deliverable))
            continue;                               // display-only or vanished product: nothing to own

        WorldPackets::BattlePay::AccountEntitlement entitlement;
        entitlement.DeliverableID = e.ProductID;
        // ExpireDate / DisplayExpireDate / UnitsRemaining / ManualReviewStatus stay zero: our
        // entitlements never expire, are not metered, and are not manually reviewed.

        sync.Entitlements.emplace_back(entitlement, std::move(deliverable));
    }

    // The client is only ever sent this ascending by DeliverableID.
    std::sort(sync.Entitlements.begin(), sync.Entitlements.end(),
        [](auto const& left, auto const& right) { return left.first.DeliverableID < right.first.DeliverableID; });

    SendPacket(sync.Write());
}

// Turns a purchase into an owned-but-unapplied entitlement instead of an immediate grant. Returns the
// PurchaseResult to report; RESULT_OK means the entitlement exists and the caller may charge.
int32 WorldSession::BattlePayCreateEntitlement(ShopProduct const& product, uint64 purchaseID)
{
    // Cap what one account may hoard. Free products at character select are otherwise an unbounded row
    // generator (the 2 s purchase throttle limits the rate, not the total), and the wire cannot show
    // more than this anyway.
    size_t available = 0;
    for (ShopEntitlement const& e : _battlePayEntitlements)
        if (e.Status == SHOP_ENTITLEMENT_AVAILABLE)
            ++available;

    if (available >= MAX_LISTED_ENTITLEMENTS)
    {
        TC_LOG_INFO("network", "BattlePay: {} already holds {} unapplied entitlements; refusing another.",
            GetPlayerInfo(), available);
        return RESULT_PRODUCT_NOT_PURCHASABLE;
    }

    uint64 const distributionId = sBattlePayMgr->CreateEntitlement(GetAccountId(), product.ProductID,
        BattlePayMgr::GetServiceType(product), purchaseID);
    if (!distributionId)
        return RESULT_PRODUCT_NOT_PURCHASABLE;

    ShopEntitlement& e = _battlePayEntitlements.emplace_back();
    e.DistributionID = distributionId;
    e.ProductID      = product.ProductID;
    e.ServiceType    = BattlePayMgr::GetServiceType(product);
    e.Status         = SHOP_ENTITLEMENT_AVAILABLE;
    e.PurchaseID     = purchaseID;
    e.CreateTime     = GameTime::GetGameTime();

    SendBattlePayDistributionUpdate(e);
    SendBattlePayDistributionListNow();
    return RESULT_OK;
}

// Applies one owned entitlement to a character the client has chosen.
//
// The request's four fields are structurally proven from the client's serializer, but the packet has
// never been seen on the wire, so nothing here trusts the field NAMES either: the DistributionID must
// be one this server issued to THIS account, and the target must be a character THIS session
// enumerated. A crafted or misread packet therefore cannot grant anything.
void WorldSession::HandleBattlePayDistributionAssignToTarget(WorldPackets::BattlePay::DistributionAssignToTarget& assign)
{
    if (!sWorld->getBoolConfig(CONFIG_SHOP_ENABLED) || !sWorld->getBoolConfig(CONFIG_SHOP_ENTITLEMENTS_ENABLED))
        return;

    // Diagnostic first: this is the first build in which we answer this opcode at all, and this log is
    // what will confirm the field reading against a real client.
    TC_LOG_INFO("network", "BattlePay: DistributionAssignToTarget from {}: clientToken={} distributionID={} "
        "target={} productChoice={}", GetPlayerInfo(), assign.ClientToken, assign.DistributionID,
        assign.TargetCharacter.ToString(), assign.ProductChoice);

    if (!sWorld->getBoolConfig(CONFIG_SHOP_ENTITLEMENT_ASSIGN_ENABLED))
    {
        TC_LOG_INFO("network", "BattlePay: assignment ignored - Shop.Entitlements.AssignEnabled is off.");
        return;
    }

    auto respond = [&](int32 result)
    {
        WorldPackets::BattlePay::StartDistributionAssignToTargetResponse response;
        response.Result = uint32(result);
        response.DistributionID = assign.DistributionID;
        SendPacket(response.Write());
    };

    // 1. Is it ours, and still unapplied? The cache is authoritative for "what this account owns"; the
    //    compare-and-swap below is authoritative for "nobody else took it first".
    ShopEntitlement const* wanted = nullptr;
    for (ShopEntitlement const& e : _battlePayEntitlements)
        if (e.Status == SHOP_ENTITLEMENT_AVAILABLE && e.DistributionID == assign.DistributionID)
            wanted = &e;

    if (!wanted)
    {
        TC_LOG_INFO("network", "BattlePay: {} named entitlement {}, which is not an available entitlement of "
            "this account ({} cached).", GetPlayerInfo(), assign.DistributionID, _battlePayEntitlements.size());
        respond(RESULT_DISTRIBUTION_NOT_FOUND);
        return;
    }

    // A character boost is not delivered by binding it to a character and handing the payload over at
    // that character's next login - it rewrites the character while it is still logged out, and its own
    // opcode (CMSG_CHARACTER_UPGRADE_START) carries the specialization the player picked, which this
    // packet's ProductChoice is only assumed to. Refuse it here rather than binding an entitlement that
    // the redemption path would then have to hand straight back.
    if (wanted->ServiceType == SHOP_SERVICE_CHARACTER_BOOST)
    {
        TC_LOG_INFO("network", "BattlePay: {} tried to assign character-boost entitlement {}; boosts are applied "
            "through CMSG_CHARACTER_UPGRADE_START, not through an assign.", GetPlayerInfo(), assign.DistributionID);
        respond(RESULT_DISTRIBUTION_INVALID_TARGET);
        return;
    }

    // 2. Is the target really one of this account's characters? _legitCharacters is filled from the
    //    character enumeration, so this also rejects a guid belonging to somebody else entirely.
    if (assign.TargetCharacter.IsEmpty() || !IsLegitCharacterForAccount(assign.TargetCharacter))
    {
        TC_LOG_INFO("network", "BattlePay: {} tried to assign entitlement {} to {}, which is not a character "
            "of this account.", GetPlayerInfo(), assign.DistributionID, assign.TargetCharacter.ToString());
        respond(RESULT_DISTRIBUTION_INVALID_TARGET);
        return;
    }

    uint64 const distributionId = wanted->DistributionID;
    uint64 const purchaseId = wanted->PurchaseID;

    // 3. Claim it. This is the only step that may never run twice, and the compare-and-swap in the
    //    manager guarantees that: a replayed assign, or a second realm on the shared auth DB, loses the
    //    race and lands here with a failure instead of a second grant.
    uint64 claimToken = 0;
    ShopEntitlement claimed;
    int32 claimResult = 0;
    if (!sBattlePayMgr->ClaimEntitlement(GetAccountId(), distributionId, sRealmList->GetCurrentRealmId().Realm,
        assign.TargetCharacter.GetCounter(), claimToken, claimed, claimResult))
    {
        TC_LOG_INFO("network", "BattlePay: {} could not claim entitlement {} (result {}).",
            GetPlayerInfo(), distributionId, claimResult);
        respond(claimResult);
        LoadBattlePayEntitlements(true);            // resync: our cache was stale
        return;
    }

    // 4. Commit the claim as BOUND. The payload is handed over the next time that character logs in -
    //    the target is offline right now (this is character select), and delivering through the proven
    //    online path beats hand-writing another character's inventory.
    if (!sBattlePayMgr->TransitionEntitlement(distributionId, SHOP_ENTITLEMENT_CLAIMED, claimToken,
        SHOP_ENTITLEMENT_BOUND, 0))
    {
        // Could not finish the hand-off: put it back, so the player still owns what they paid for.
        sBattlePayMgr->TransitionEntitlement(distributionId, SHOP_ENTITLEMENT_CLAIMED, claimToken,
            SHOP_ENTITLEMENT_AVAILABLE, 0);
        TC_LOG_ERROR("network", "BattlePay: failed to bind entitlement {} to {}; returned it to the account.",
            distributionId, assign.TargetCharacter.ToString());
        respond(RESULT_PRODUCT_NOT_PURCHASABLE);
        LoadBattlePayEntitlements(true);
        return;
    }

    TC_LOG_INFO("network", "BattlePay: {} assigned entitlement {} (product {}) to {}; it will be delivered "
        "at that character's next login.", GetPlayerInfo(), distributionId, claimed.ProductID,
        assign.TargetCharacter.ToString());

    respond(RESULT_OK);

    // Tell the client the entitlement moved on, then resend the (now shorter) list.
    claimed.Status = SHOP_ENTITLEMENT_BOUND;
    claimed.PurchaseID = purchaseId;
    SendBattlePayDistributionUpdate(claimed);
    LoadBattlePayEntitlements(true);
}

// Delivers every entitlement bound to the character that just entered the world.
//
// Ordering is deliberately mark-then-deliver, the inverse of the normal grant-before-charge rule and for
// the same reason the WoW Token path inverts it: the entitlement is already paid for and already durable
// in the auth DB, so the only failure worth engineering against is delivering it twice. Consuming the row
// first means a crash mid-delivery costs the player one payload (refundable by a GM) instead of letting a
// relog loop mint goods forever.
void WorldSession::RedeemBattlePayEntitlements()
{
    if (!sWorld->getBoolConfig(CONFIG_SHOP_ENABLED) || !sWorld->getBoolConfig(CONFIG_SHOP_ENTITLEMENTS_ENABLED))
        return;

    Player* player = GetPlayer();
    if (!player)
        return;

    LoginDatabasePreparedStatement* stmt = LoginDatabase.GetPreparedStatement(LOGIN_SEL_BATTLEPAY_ENTITLEMENT_PENDING_CHAR);
    stmt->setUInt32(0, sRealmList->GetCurrentRealmId().Realm);
    stmt->setUInt64(1, player->GetGUID().GetCounter());

    _queryProcessor.AddCallback(LoginDatabase.AsyncQuery(stmt)
        .WithPreparedCallback([this](PreparedQueryResult result)
    {
        if (!result)
            return;

        Player* target = GetPlayer();
        if (!target || !target->IsInWorld())
            return;

        do
        {
            Field* fields = result->Fetch();
            uint64 const distributionId = fields[0].GetUInt64();
            uint32 const productId      = fields[1].GetUInt32();
            uint8 const serviceType     = fields[2].GetUInt8();
            uint64 const purchaseId     = fields[3].GetUInt64();

            // Consume first (see the ordering note above). A losing racer simply finds nothing to do.
            if (!sBattlePayMgr->TransitionEntitlement(distributionId, SHOP_ENTITLEMENT_BOUND, 0,
                SHOP_ENTITLEMENT_FINISHED, 0))
            {
                TC_LOG_INFO("network", "BattlePay: entitlement {} was already consumed; skipping.", distributionId);
                continue;
            }

            if (serviceType != 0)
            {
                // A VAS service (rename, faction/race change, transfer) that has no login-time delivery.
                // The one service this core does perform - the character boost - never reaches here: it
                // is applied at character select by CMSG_CHARACTER_UPGRADE_START and goes straight from
                // AVAILABLE to FINISHED, and the assign path refuses to bind one. Anything else is
                // refused loudly rather than silently swallowed, and handed back so nothing is lost.
                sBattlePayMgr->TransitionEntitlement(distributionId, SHOP_ENTITLEMENT_FINISHED, 0,
                    SHOP_ENTITLEMENT_AVAILABLE, 0);
                TC_LOG_ERROR("network", "BattlePay: entitlement {} names VAS service type {}, which this core "
                    "cannot perform; returned it to account {}.", distributionId, serviceType, GetAccountId());
                continue;
            }

            ShopProduct const* product = sBattlePayMgr->GetProduct(productId);
            if (!product || product->Deliverables.empty())
            {
                sBattlePayMgr->TransitionEntitlement(distributionId, SHOP_ENTITLEMENT_FINISHED, 0,
                    SHOP_ENTITLEMENT_AVAILABLE, 0);
                TC_LOG_ERROR("network", "BattlePay: entitlement {} points at product {}, which no longer exists "
                    "or has no deliverables; returned it to account {}.", distributionId, productId, GetAccountId());
                continue;
            }

            for (ShopDeliverable const& d : product->Deliverables)
            {
                switch (d.Type)
                {
                    case 1:
                        BattlePayDeliverItem(target, d.Id, d.Count);
                        break;
                    case 2:
                        if (!target->HasSpell(d.Id))
                            target->LearnSpell(d.Id, false);
                        break;
                    case 3:
                        for (uint32 i = 0; i < std::max<uint32>(d.Count, 1u); ++i)
                            sWowTokenMgr->CreateToken(GetAccountId(), WOW_TOKEN_STATE_AUCTIONABLE);
                        SendCommerceTokenUpdate();
                        break;
                    default:
                        TC_LOG_ERROR("network", "BattlePay: entitlement {} carries unsupported deliverable type {}.",
                            distributionId, d.Type);
                        break;
                }
            }

            // Same delivery announcement as the immediate-purchase path: the payload has just landed in
            // this character's collection, so tell the client to refresh it.
            SendBattlePayDeliveryNotifications(*product, purchaseId);

            TC_LOG_INFO("network", "BattlePay: delivered entitlement {} (product {} '{}') to {}.",
                distributionId, productId, product->Name, target->GetName());
        }
        while (result->NextRow());
    }));
}

// ---- Character boost -------------------------------------------------------------------------------
//
// Retail sells a boost as a product, then applies it from the GLUE SCREEN to a character that is not
// logged in. Everything below is shaped by that one fact: there is no Player to grant anything to, so
// the boost is a set of writes to the character database, and the client is told about it through
// CMSG_CHARACTER_UPGRADE_START / SMSG_CHARACTER_UPGRADE_STARTED / _COMPLETE / _ABORTED rather than
// through the normal delivery notifications.
//
// It re-uses the entitlement machinery unchanged: a boost product carries a type-5 deliverable naming
// service type 1, so NeedsEntitlement() already routes its purchase into an AVAILABLE entitlement, and
// the compare-and-swap claim already guarantees one entitlement can only ever be spent once. What this
// adds is the redemption that RedeemBattlePayEntitlements() previously had to refuse.

namespace
{
    // Everything about the target that has to be read BEFORE the entitlement is claimed: if the session
    // goes away between the read and the write, an unclaimed entitlement is still owned, whereas a
    // claimed one would be stuck.
    class CharacterBoostQueryHolder : public CharacterDatabaseQueryHolder
    {
    public:
        enum
        {
            TARGET,
            INVENTORY,

            MAX
        };

        CharacterBoostQueryHolder() { SetSize(MAX); }

        bool Initialize(ObjectGuid::LowType characterGuid)
        {
            bool result = true;

            CharacterDatabasePreparedStatement* stmt = CharacterDatabase.GetPreparedStatement(CHAR_SEL_SHOP_BOOST_TARGET);
            stmt->setUInt64(0, characterGuid);
            result &= SetPreparedQuery(TARGET, stmt);

            stmt = CharacterDatabase.GetPreparedStatement(CHAR_SEL_SHOP_BOOST_INVENTORY);
            stmt->setUInt64(0, characterGuid);
            result &= SetPreparedQuery(INVENTORY, stmt);

            return result;
        }
    };
}

bool WorldSession::HasBattlePayCharacterBoost() const
{
    for (ShopEntitlement const& entitlement : _battlePayEntitlements)
        if (entitlement.Status == SHOP_ENTITLEMENT_AVAILABLE && entitlement.ServiceType == SHOP_SERVICE_CHARACTER_BOOST)
            return true;

    return false;
}

void WorldSession::HandleCharacterUpgradeStart(WorldPackets::BattlePay::CharacterUpgradeStart& upgradeStart)
{
    // Diagnostic first, like the assign handler: this opcode has never been seen on our wire either, and
    // this line is what confirms the (client-serializer-derived) field reading against a real client.
    TC_LOG_INFO("network", "BattlePay: CharacterUpgradeStart from {}: character={} specialization={}",
        GetPlayerInfo(), upgradeStart.CharacterGUID.ToString(), upgradeStart.SpecializationID);

    ObjectGuid const target = upgradeStart.CharacterGUID;

    // Told, not dropped. The client's boost UI waits on an answer, and a silent return would leave it
    // waiting forever - which is exactly the failure mode a stub would have produced.
    auto abort = [this, target](char const* why)
    {
        TC_LOG_INFO("network", "BattlePay: character boost of {} refused - {}.", target.ToString(), why);

        WorldPackets::BattlePay::CharacterUpgradeAborted aborted;
        aborted.CharacterGUID = target;
        SendPacket(aborted.Write());
    };

    if (!sWorld->getBoolConfig(CONFIG_SHOP_ENABLED) || !sWorld->getBoolConfig(CONFIG_SHOP_ENTITLEMENTS_ENABLED))
    {
        abort("the Shop or its entitlement model is disabled");
        return;
    }

    // Applying an entitlement to a character is gated by the same config as the assign path, for the
    // same reason: it is the step that actually spends something.
    if (!sWorld->getBoolConfig(CONFIG_SHOP_ENTITLEMENT_ASSIGN_ENABLED))
    {
        abort("Shop.Entitlements.AssignEnabled is off");
        return;
    }

    // A boost writes the target's level, inventory and equipment cache straight into the database. If
    // that character were loaded, the running Player would overwrite all of it at its next save. The
    // client only sends this from the glue screen, so refusing here costs nothing and closes the hole.
    if (GetPlayer())
    {
        abort("a character is in world; a boost may only be applied from character select");
        return;
    }

    // The target must be one of the characters THIS session enumerated - the same check the assign path
    // makes, and what stops a crafted packet from boosting somebody else's character. A class trial is
    // also accepted: the client creates one and boosts it immediately, which can outrun the
    // re-enumeration that would have put it in _legitCharacters, and that set only ever holds characters
    // this session created. Either way the target's `account` column is re-checked below before
    // anything is claimed or written, so this is a cheap first filter, not the authority.
    if (target.IsEmpty() || (!IsLegitCharacterForAccount(target) && !IsCharacterShopTrial(target.GetCounter())))
    {
        abort("it is not a character of this account");
        return;
    }

    if (IsCharacterShopBoosted(target.GetCounter()))
    {
        abort("it has already been boosted");
        return;
    }

    ChrSpecializationEntry const* specialization = sChrSpecializationStore.LookupEntry(upgradeStart.SpecializationID);
    if (!specialization)
    {
        abort("the requested specialization does not exist");
        return;
    }

    // Which entitlement pays for it. Oldest first: _battlePayEntitlements is loaded ORDER BY id, so an
    // account holding several boosts spends the one it has held longest.
    ShopEntitlement const* boost = nullptr;
    for (ShopEntitlement const& entitlement : _battlePayEntitlements)
    {
        if (entitlement.Status == SHOP_ENTITLEMENT_AVAILABLE && entitlement.ServiceType == SHOP_SERVICE_CHARACTER_BOOST)
        {
            boost = &entitlement;
            break;
        }
    }

    if (!boost)
    {
        abort("this account owns no unapplied character boost");
        return;
    }

    uint64 const distributionId = boost->DistributionID;
    uint64 const purchaseId = boost->PurchaseID;
    uint32 const productId = boost->ProductID;
    uint32 const specializationId = upgradeStart.SpecializationID;

    std::shared_ptr<CharacterBoostQueryHolder> holder = std::make_shared<CharacterBoostQueryHolder>();
    if (!holder->Initialize(target.GetCounter()))
    {
        abort("its character data could not be queried");
        return;
    }

    AddQueryHolderCallback(CharacterDatabase.DelayQueryHolder(holder)).AfterComplete(
        [this, target, specializationId, distributionId, purchaseId, productId](SQLQueryHolderBase const& queryResult)
    {
        ApplyBattlePayCharacterBoost(static_cast<CharacterDatabaseQueryHolder const&>(queryResult), target,
            specializationId, distributionId, purchaseId, productId);
    });
}

// Second half of the boost, once the target's current state has been read.
//
// ORDERING. The entitlement is claimed (compare-and-swap) only after every validation has passed, and
// the write is committed only after the claim succeeded. If the commit fails the claim is handed back,
// so the account keeps what it paid for. The one irreversible moment is the transition to FINISHED
// after a successful commit: if that fails the character IS boosted and the entitlement is left
// CLAIMED, which is loud in the log and repairable by hand - the alternative, rolling the entitlement
// back to AVAILABLE, would hand out a second free boost.
void WorldSession::ApplyBattlePayCharacterBoost(CharacterDatabaseQueryHolder const& queryResult, ObjectGuid target,
    uint32 specializationId, uint64 distributionId, uint64 purchaseId, uint32 productId)
{
    auto abort = [this, target](char const* why)
    {
        TC_LOG_INFO("network", "BattlePay: character boost of {} refused - {}.", target.ToString(), why);

        WorldPackets::BattlePay::CharacterUpgradeAborted aborted;
        aborted.CharacterGUID = target;
        SendPacket(aborted.Write());
    };

    PreparedQueryResult targetResult = queryResult.GetPreparedResult(CharacterBoostQueryHolder::TARGET);
    if (!targetResult)
    {
        abort("it no longer exists");
        return;
    }

    ShopBoostTarget boostTarget;
    boostTarget.Guid = target;

    Field* fields = targetResult->Fetch();
    boostTarget.AccountId = fields[0].GetUInt32();
    boostTarget.ClassId = fields[1].GetUInt8();
    boostTarget.RaceId = fields[2].GetUInt8();
    boostTarget.Level = fields[3].GetUInt8();
    boostTarget.BackpackSlots = fields[4].GetUInt8();

    // Re-check ownership against the database, not just against the enumeration cache.
    if (boostTarget.AccountId != GetAccountId())
    {
        abort("it belongs to another account");
        return;
    }

    ChrSpecializationEntry const* specialization = sChrSpecializationStore.LookupEntry(specializationId);
    if (!specialization || specialization->ClassID != boostTarget.ClassId)
    {
        abort("the requested specialization is not one of that character's class");
        return;
    }

    uint8 const boostLevel = BattlePayMgr::GetCharacterBoostLevel();
    if (boostTarget.Level >= boostLevel)
    {
        abort("it is already at or above the boost level");
        return;
    }

    if (PreparedQueryResult inventoryResult = queryResult.GetPreparedResult(CharacterBoostQueryHolder::INVENTORY))
    {
        do
        {
            Field* slotFields = inventoryResult->Fetch();
            ShopBoostInventorySlot& occupant = boostTarget.OwnSlots[slotFields[0].GetUInt8()];
            occupant.ItemGuid = slotFields[1].GetUInt64();
            occupant.ItemId = slotFields[2].GetUInt32();
        }
        while (inventoryResult->NextRow());
    }

    // Spend the entitlement. This is the only step that may never run twice, and the manager's
    // compare-and-swap is what guarantees it.
    uint64 claimToken = 0;
    ShopEntitlement claimed;
    int32 claimResult = 0;
    if (!sBattlePayMgr->ClaimEntitlement(GetAccountId(), distributionId, sRealmList->GetCurrentRealmId().Realm,
        target.GetCounter(), claimToken, claimed, claimResult))
    {
        TC_LOG_INFO("network", "BattlePay: {} could not claim boost entitlement {} (result {}).",
            GetPlayerInfo(), distributionId, claimResult);
        abort("its boost entitlement could not be claimed");
        LoadBattlePayEntitlements(true);                    // resync: our cache was stale
        return;
    }

    CharacterDatabaseTransaction trans = sBattlePayMgr->BuildCharacterBoostTransaction(boostTarget,
        specializationId, distributionId, productId);
    if (!trans)
    {
        // Nothing was written - give the entitlement straight back.
        sBattlePayMgr->TransitionEntitlement(distributionId, SHOP_ENTITLEMENT_CLAIMED, claimToken,
            SHOP_ENTITLEMENT_AVAILABLE, 0);
        abort("the boost could not be prepared; the entitlement was returned to the account");
        LoadBattlePayEntitlements(true);
        return;
    }

    // 1. STARTED - paid for, about to be written.
    WorldPackets::BattlePay::CharacterUpgradeStarted started;
    started.CharacterGUID = target;
    SendPacket(started.Write());

    // 2. DISTRIBUTION_UPDATE - the entitlement is no longer available to spend on anything else.
    claimed.Status = SHOP_ENTITLEMENT_CLAIMED;
    claimed.PurchaseID = purchaseId;
    SendBattlePayDistributionUpdate(claimed);

    AddTransactionCallback(CharacterDatabase.AsyncCommitTransaction(trans)).AfterComplete(
        [this, target, distributionId, purchaseId, productId, claimToken, boostLevel](bool success)
    {
        if (!success)
        {
            sBattlePayMgr->TransitionEntitlement(distributionId, SHOP_ENTITLEMENT_CLAIMED, claimToken,
                SHOP_ENTITLEMENT_AVAILABLE, 0);
            TC_LOG_ERROR("network", "BattlePay: the boost transaction for {} failed to commit; entitlement {} "
                "was returned to account {}.", target.ToString(), distributionId, GetAccountId());

            WorldPackets::BattlePay::CharacterUpgradeAborted aborted;
            aborted.CharacterGUID = target;
            SendPacket(aborted.Write());
            LoadBattlePayEntitlements(true);
            return;
        }

        if (!sBattlePayMgr->TransitionEntitlement(distributionId, SHOP_ENTITLEMENT_CLAIMED, claimToken,
            SHOP_ENTITLEMENT_FINISHED, 0))
        {
            // The character is boosted; the ledger did not close. Never roll back here - that would let
            // the same entitlement pay for a second boost. Loud, and repairable by hand.
            TC_LOG_ERROR("network", "BattlePay: {} was boosted but entitlement {} could not be closed and is "
                "left CLAIMED (account {}). It must not be returned to the account.",
                target.ToString(), distributionId, GetAccountId());
        }

        _shopBoostedCharacters.insert(target.GetCounter());
        _shopTrialCharacters.erase(target.GetCounter());
        sCharacterCache->UpdateCharacterLevel(target, boostLevel);

        // 3. COMPLETE - durably written.
        WorldPackets::BattlePay::CharacterUpgradeComplete complete;
        complete.CharacterGUID = target;
        SendPacket(complete.Write());

        // 4. INVALIDATE_PLAYER - everything the client had cached about this character (its level, its
        //    gear, its model) is now wrong.
        WorldPackets::Misc::InvalidatePlayer invalidate;
        invalidate.Guid = target;
        SendPacket(invalidate.Write());

        ShopEntitlement finished;
        finished.DistributionID = distributionId;
        finished.ProductID = productId;
        finished.ServiceType = SHOP_SERVICE_CHARACTER_BOOST;
        finished.Status = SHOP_ENTITLEMENT_FINISHED;
        finished.PurchaseID = purchaseId;
        SendBattlePayDistributionUpdate(finished);

        TC_LOG_INFO("network", "BattlePay: {} boosted {} to level {} with entitlement {} (product {}).",
            GetPlayerInfo(), target.ToString(), boostLevel, distributionId, productId);

        // The account's owned set changed, and so did the character list the glue screen is showing.
        LoadBattlePayEntitlements(true);
        SendCharacterEnum();
    });
}

void WorldSession::HandleBattlePayGetPurchaseList(WorldPackets::BattlePay::GetPurchaseList& /*getPurchaseList*/)
{
    // The client polls this whenever the Shop is opened and blocks its purchase UI until it gets a reply.
    // Answer with this account's real purchase history from the shared ledger (C-13). ProductID may be 0
    // (a valid value, C-32) so it is never filtered. walletName is always sent empty/record-final.
    if (!sWorld->getBoolConfig(CONFIG_SHOP_ENABLED))
        return;

    LoginDatabasePreparedStatement* stmt = LoginDatabase.GetPreparedStatement(LOGIN_SEL_BATTLEPAY_PURCHASE_ACCOUNT);
    stmt->setUInt32(0, GetAccountId());

    _queryProcessor.AddCallback(LoginDatabase.AsyncQuery(stmt)
        .WithPreparedCallback([this](PreparedQueryResult result)
    {
        WorldPackets::BattlePay::GetPurchaseListResponse response;
        response.Result = 0;    // PurchaseResult::Ok

        if (result)
        {
            do
            {
                Field* fields = result->Fetch();
                WorldPackets::BattlePay::PurchaseRecord& rec = response.Purchases.emplace_back();
                rec.PurchaseID  = fields[0].GetUInt64();
                rec.Status      = fields[1].GetInt32();
                rec.ResultCode  = fields[2].GetInt32();
                rec.ProductID   = fields[3].GetUInt32();
                rec.BasePrice   = fields[4].GetUInt64();
                rec.UserPrice   = fields[5].GetUInt64();
                rec.TimeCreated = fields[6].GetInt64();
            }
            while (result->NextRow());
        }

        SendPacket(response.Write());
    }));
}

// VAS (Value Added Services) status queries.
//
// These are the only two VAS opcodes a retail client actually sends unprompted - both appear at character
// select in every capture on this machine, and both have empty bodies. They were registered STATUS_IGNORED,
// which in this core is a bare `break;` in WorldSession: the packet was dropped without even a log line, so
// the client's VAS caches were never refreshed and never cleared.
//
// Answering with "nothing in flight" is the accurate state of this realm, and it is byte-identical to what
// retail sends when a player has no pending purchase.
void WorldSession::HandleUpdateVasPurchaseStates(WorldPackets::BattlePay::UpdateVasPurchaseStates& /*packet*/)
{
    WorldPackets::BattlePay::EnumVasPurchaseStatesResponse response;
    SendPacket(response.Write());
}

void WorldSession::HandleVasGetServiceStatus(WorldPackets::BattlePay::VasGetServiceStatus& /*packet*/)
{
    // ServiceStatus 0: this realm offers no paid character services. The low nibble's meaning is unknown,
    // so it is left 0 rather than given an invented value.
    WorldPackets::BattlePay::VasGetServiceStatusResponse response;
    SendPacket(response.Write());
}

void WorldSession::HandleGetVasAccountCharacterList(WorldPackets::BattlePay::GetVasAccountCharacterList& packet)
{
    // The account's characters, offered as VAS-transfer candidates. The verified per-entry wire carries the
    // character guid, the owning account guid, the home realm's virtual address and the character name; the
    // remaining per-entry fields (four flag bytes, an 8-byte housing key, a trailing uint32) have no proven
    // offline meaning, so they stay 0. RealmName is left empty (the worldserver exposes no clean realm-name
    // accessor here) - it is display-only and not required to identify a character.
    WorldPackets::BattlePay::GetVasAccountCharacterListResult result;
    result.Field1 = packet.Field1;   // echo the request token so the client can correlate the answer

    CharacterDatabasePreparedStatement* stmt = CharacterDatabase.GetPreparedStatement(CHAR_SEL_VAS_ACCOUNT_CHARACTER_LIST);
    stmt->setUInt32(0, GetAccountId());
    if (PreparedQueryResult res = CharacterDatabase.Query(stmt))
    {
        uint32 const virtualRealm = GetVirtualRealmAddress();
        ObjectGuid const accountGuid = GetAccountGUID();
        do
        {
            Field* fields = res->Fetch();
            WorldPackets::BattlePay::VasAccountCharacterInfo info;
            info.CharacterGUID = ObjectGuid::Create<HighGuid::Player>(fields[0].GetUInt64());
            info.AccountGUID = accountGuid;
            info.VirtualRealmAddress = virtualRealm;
            info.CharacterName = fields[1].GetString();
            result.Characters.push_back(std::move(info));
        }
        while (res->NextRow());
    }

    SendPacket(result.Write());
}

void WorldSession::HandleGetVasTransferTargetRealmList(WorldPackets::BattlePay::GetVasTransferTargetRealmList& packet)
{
    // The realms a character may be transferred to: every other realm this bnetserver fronts (our setup runs
    // three). Enumerated from the shared auth realmlist, excluding this realm and any that are offline. Each
    // entry mirrors the bnet RealmEntry the client already consumes at login - wowRealmAddress (the value the
    // client sends back to target the transfer) is realm.Id.GetAddress(); the remaining fields are populated
    // best-effort from realm id/flags/population and are live-test-pending on the exact field order.
    WorldPackets::BattlePay::GetVasTransferTargetRealmListResult result;
    result.Field1 = packet.Field1;   // echo the request token for correlation

    Battlenet::RealmHandle const currentRealm = sRealmList->GetCurrentRealmId();

    LoginDatabasePreparedStatement* stmt = LoginDatabase.GetPreparedStatement(LOGIN_SEL_REALMLIST);
    if (PreparedQueryResult res = LoginDatabase.Query(stmt))
    {
        do
        {
            Field* fields = res->Fetch();
            uint32 const realmId = fields[0].GetUInt32();
            uint8 const flag = fields[8].GetUInt8();
            uint8 const region = fields[13].GetUInt8();
            uint8 const battlegroup = fields[14].GetUInt8();

            if (realmId == currentRealm.Realm)          // not this realm
                continue;
            if (flag & Trinity::Legacy::REALM_FLAG_OFFLINE)   // only realms that are up can receive a transfer
                continue;

            Battlenet::RealmHandle const handle(region, battlegroup, realmId);

            WorldPackets::BattlePay::VasTargetRealmInfo entry;
            entry.WowRealmAddress = handle.GetAddress();
            entry.RealmId = realmId;
            entry.Flags = flag;
            entry.PopulationState = uint32(fields[11].GetFloat());   // population column
            entry.CategoryId = fields[9].GetUInt8();                 // timezone column
            entry.RealmName = fields[1].GetString();
            result.Realms.push_back(std::move(entry));
        }
        while (res->NextRow());
    }

    SendPacket(result.Write());
}

void WorldSession::HandleVasCheckTransferOk(WorldPackets::BattlePay::VasCheckTransferOk& packet)
{
    // Whether a VAS transfer is permitted for the requested context. This realm has no transfer network, so
    // the honest, byte-correct answer is an empty target-account list; Field1 echoes the request context so
    // the client can correlate the answer.
    WorldPackets::BattlePay::VasCheckTransferOkResponse response;
    response.Field1 = packet.Field1;
    SendPacket(response.Write());
}

void WorldSession::HandleBattlePayDistributionAssignVas(WorldPackets::BattlePay::BattlePayDistributionAssignVas& packet)
{
    // Assign an ALREADY-PURCHASED VAS distribution to a target. RE of the client serializer (sub_7FF72907F4B0)
    // showed this opcode carries a u64 DistributionID (+0x28) and target guids but NO realm address - the
    // realm is chosen earlier, at START_VAS_PURCHASE. The distribution it references only exists after a
    // web-side purchase settles, which this realm has none of, so there is nothing to assign: answer honestly
    // with "distribution not found" rather than drop the packet. (The paid-character-transfer flow itself does
    // NOT use this opcode - it drives START_VAS_PURCHASE, handled above; this is the shop's assign-a-bought-
    // distribution path.)
    TC_LOG_INFO("network", "BattlePay: DistributionAssignVas from {}: distributionId-token={} - no VAS "
        "distribution to assign.", GetPlayerInfo(), packet.Token);

    WorldPackets::BattlePay::BattlePayDistributionAssignVasResponse response;
    response.Field1 = packet.Token;
    response.Result = uint32(RESULT_DISTRIBUTION_NOT_FOUND);
    SendPacket(response.Write());
}

void WorldSession::HandleBattlePayStartVasPurchase(WorldPackets::BattlePay::BattlePayStartVasPurchase& packet)
{
    // The paid-character-transfer flow's request (Lua AssignPCTDistribution): the selected character, the VAS
    // service type, the TARGET realm's wowRealmAddress, and an IsValidationOnly bool (the flow validates, then
    // commits). The character is the sole Player-type guid among the four the packet carries. Resolve the realm
    // and drive VasTransferMgr - validate-only on the validation pass, a real cross-DB move on the commit pass -
    // then answer with the SMSG the flow waits on (ASSIGN_VAS_RESPONSE: token, storeError, VasTransactionPurchaseResult).
    ObjectGuid const character = packet.GetCharacterGuid();
    uint32 const targetRealmId = packet.TargetRealmAddress ? Battlenet::RealmHandle(packet.TargetRealmAddress).Realm : 0;

    // VasTransactionPurchaseResult values (Enum, from BattlepayConstantsDocumentation): 0 success, 2 no
    // character, 5 invalid destination realm, 20 unique-key (a same-name character already on the target -
    // exactly WoW's only hard transfer rejection), 43 locked-for-VAS (character online), 20011 realm not
    // eligible, 44 generic.
    auto mapResult = [](VasTransferMgr::TransferResult r) -> uint32
    {
        switch (r)
        {
            case VasTransferMgr::TRANSFER_OK:               return 0;
            case VasTransferMgr::TRANSFER_ERR_CHAR_NOT_FOUND: return 2;
            case VasTransferMgr::TRANSFER_ERR_NO_TARGET:    return 5;
            case VasTransferMgr::TRANSFER_ERR_NO_SOURCE:    return 20011;
            case VasTransferMgr::TRANSFER_ERR_IN_WORLD:     return 43;
            case VasTransferMgr::TRANSFER_ERR_NAME_TAKEN:   return 20;
            case VasTransferMgr::TRANSFER_ERR_GUID_COLLISION: return 20;
            case VasTransferMgr::TRANSFER_ERR_DB:           return 44;
            default:                                        return 44;
        }
    };

    auto respond = [&](uint32 vasResult)
    {
        WorldPackets::BattlePay::BattlePayDistributionAssignVasResponse response;
        response.Field1 = packet.SequenceId;   // token the client correlates the answer to
        response.Field2 = 0;                    // storeError: no store-layer error
        response.Result = vasResult;            // VasTransactionPurchaseResult
        SendPacket(response.Write());
    };

    if (!sVasTransferMgr->IsEnabled())
    {
        TC_LOG_INFO("network", "BattlePay: StartVasPurchase from {}: character transfer is not configured "
            "(VAS.TransferRealmDatabases).", GetPlayerInfo());
        respond(20011);   // realm not eligible
        return;
    }

    if (character.IsEmpty())
    {
        respond(2);       // no character guid in the request
        return;
    }

    std::string name;
    VasTransferMgr::TransferResult const result = sVasTransferMgr->TransferCharacter(
        character.GetCounter(), targetRealmId, &name, nullptr, packet.IsValidationOnly);

    TC_LOG_INFO("network", "BattlePay: StartVasPurchase from {}: seq={} serviceType={} character={} '{}' "
        "targetRealm={} validateOnly={} -> {} (vasResult {}).", GetPlayerInfo(), packet.SequenceId,
        packet.ServiceType, character.ToString(), name, targetRealmId, packet.IsValidationOnly,
        VasTransferMgr::ResultString(result), mapResult(result));

    respond(mapResult(result));
}

void WorldSession::HandleCharacterCheckUpgrade(WorldPackets::BattlePay::CharacterCheckUpgrade& /*packet*/)
{
    // The client polls whether this account can spend a character boost. Eligibility is already carried by the
    // account's owned boost entitlements (delivered with the purchase list), and the boost is applied by its own
    // opcode (CMSG_CHARACTER_UPGRADE_START, implemented), so this poll needs no separate answer. Logged rather
    // than silently dropped.
    TC_LOG_DEBUG("network", "BattlePay: CharacterCheckUpgrade from {} - boost eligibility comes from the owned "
        "entitlement list; no separate response.", GetPlayerInfo());
}

void WorldSession::HandleCharacterUpgradeManualUnrevokeRequest(WorldPackets::BattlePay::CharacterUpgradeManualUnrevokeRequest& packet)
{
    // Undo a revoked boost on a character. That requires a revoked boost entitlement, which exists only after a
    // web-side purchase/refund cycle this realm has none of, so there is nothing to unrevoke. Answer with the
    // real result wire (client parser sub_7FF72A663530: a single uint32; no guid) - the Lua handler treats 0 as
    // success and any non-zero as failure (ERROR_MANUAL_UNREVOKE_FAILURE), so a non-zero code is the honest
    // "nothing to unrevoke" answer rather than falsely reporting success.
    TC_LOG_INFO("network", "BattlePay: CharacterUpgradeManualUnrevokeRequest from {}: character={} - no revoked "
        "boost to unrevoke; returning failure.", GetPlayerInfo(), packet.CharacterGUID.ToString());

    WorldPackets::BattlePay::CharacterUpgradeManualUnrevokeResult result;
    result.Result = 1;   // non-zero == failure (client shows the generic unrevoke-failure dialog)
    SendPacket(result.Write());
}

void WorldSession::HandleVasGetQueueMinutes(WorldPackets::BattlePay::VasGetQueueMinutes& packet)
{
    // Estimated queue time for a VAS service. This realm processes transfers synchronously (the GM/transfer path
    // runs inline), so the honest estimate is zero. Wire recovered from the client parser sub_7FF72AE70450:
    // { uint64 Handle; uint32 QueueMinutes } - echo the request's correlation handle so the client matches it.
    WorldPackets::BattlePay::VasGetQueueMinutesResponse response;
    response.Handle = packet.Handle;
    response.QueueMinutes = 0;
    SendPacket(response.Write());
}

void WorldSession::HandleBattlePayAckFailedResponse(WorldPackets::BattlePay::BattlePayAckFailedResponse& packet)
{
    // The client acknowledges a purchase result it treated as failed. Fire-and-forget: there is nothing to send
    // back, but it is read and logged rather than silently dropped.
    TC_LOG_DEBUG("network", "BattlePay: AckFailedResponse from {}: serverToken={}.", GetPlayerInfo(), packet.ServerToken);
}

void WorldSession::HandleBattlePayRequestPriceInfo(WorldPackets::BattlePay::BattlePayRequestPriceInfo& packet)
{
    // The client asks for a product's price. This core delivers prices inline with the product list
    // (BattlePayGetProductList), so a product the client can see already carries its DisplayPrice; there is no
    // separate price message to send. Logged for wire confirmation.
    TC_LOG_DEBUG("network", "BattlePay: RequestPriceInfo from {}: product={} - price is carried by the product list.",
        GetPlayerInfo(), packet.ProductID);
}

void WorldSession::HandleBattlePayCancelOpenCheckout(WorldPackets::BattlePay::BattlePayCancelOpenCheckout& /*packet*/)
{
    // The client abandons a checkout it opened. The realm holds no server-side reservation for an open checkout
    // (the web overlay owns that state), so there is nothing to release; acknowledged by log only.
    TC_LOG_DEBUG("network", "BattlePay: CancelOpenCheckout from {}.", GetPlayerInfo());
}
