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

#include "BattlePayMgr.h"
#include "BattlePayCatalogWriter.h"
#include "Config.h"
#include "ConditionMgr.h"
#include "DatabaseEnv.h"
#include "DB2Stores.h"
#include "DBCEnums.h"
#include "GameTime.h"
#include "Item.h"
#include "ItemTemplate.h"
#include "Log.h"
#include "ObjectMgr.h"
#include "Player.h"
#include "Random.h"
#include "Realm.h"
#include "RealmList.h"
#include "Shop2Service.h"
#include "StringFormat.h"
#include "Timer.h"
#include "TransmogMgr.h"
#include "World.h"
#include <algorithm>
#include <array>
#include <fstream>
#include <memory>
#include <set>
#include <utility>

namespace
{
    constexpr uint32 DISPLAY_FLAG_HIDDEN_PRICE   = 8;
    constexpr uint32 DISPLAY_FLAG_HIDE_WHEN_OWNED = 256;


    bool InWindow(ShopProduct const& p, time_t now)
    {
        return (p.AvailableFrom == 0 || now >= p.AvailableFrom)
            && (p.AvailableUntil == 0 || now <= p.AvailableUntil);
    }

    // ---- Character boost kit -------------------------------------------------------------------------
    //
    // The kit is not invented here: it is whatever CharacterLoadout.db2 already ships for the boost
    // purpose. These two numbers only IDENTIFY those rows - Purpose 23 is the boost purpose (9 is the
    // new-character purpose CharacterLoadoutEntry::IsForNewCharacter() already names) and the boost
    // ItemContext is the one the client stamps on boost-granted items so their bonus lists scale.
    // Every candidate row is still validated against the target's class and race before it is used, and
    // if the client data carries no such row the boost fails rather than inventing gear.
    constexpr int32 SHOP_BOOST_LOADOUT_PURPOSE = 23;
    constexpr ItemContext SHOP_BOOST_ITEM_CONTEXT = ItemContext::Character_Boost_Shadowlands_50;

    bool IsBoostLoadoutFor(CharacterLoadoutEntry const* loadout, uint8 classId, uint8 raceId)
    {
        return loadout
            && loadout->Purpose == SHOP_BOOST_LOADOUT_PURPOSE
            && ItemContext(loadout->ItemContext) == SHOP_BOOST_ITEM_CONTEXT
            && uint8(loadout->ChrClassID) == classId
            && (loadout->RaceMask.IsEmpty() || loadout->RaceMask.HasRace(raceId));
    }

    CharacterLoadoutEntry const* SelectBoostLoadout(uint8 classId, uint8 raceId)
    {
        // Lowest ID wins so the choice is stable across restarts and across realms; there is no
        // per-specialization loadout to pick from at this purpose, only per class (and sometimes race).
        CharacterLoadoutEntry const* best = nullptr;
        for (CharacterLoadoutEntry const* loadout : sCharacterLoadoutStore)
            if (IsBoostLoadoutFor(loadout, classId, raceId) && (!best || loadout->ID < best->ID))
                best = loadout;

        return best;
    }

    // The equipment slots an item of this inventory type can occupy, in preference order. Empty means
    // the item is not equipment and belongs in the backpack.
    std::vector<uint8> GetEquipmentSlotsFor(ItemTemplate const* proto)
    {
        switch (proto->GetInventoryType())
        {
            case INVTYPE_HEAD:              return { EQUIPMENT_SLOT_HEAD };
            case INVTYPE_NECK:              return { EQUIPMENT_SLOT_NECK };
            case INVTYPE_SHOULDERS:         return { EQUIPMENT_SLOT_SHOULDERS };
            case INVTYPE_BODY:              return { EQUIPMENT_SLOT_BODY };
            case INVTYPE_CHEST:
            case INVTYPE_ROBE:              return { EQUIPMENT_SLOT_CHEST };
            case INVTYPE_WAIST:             return { EQUIPMENT_SLOT_WAIST };
            case INVTYPE_LEGS:              return { EQUIPMENT_SLOT_LEGS };
            case INVTYPE_FEET:              return { EQUIPMENT_SLOT_FEET };
            case INVTYPE_WRISTS:            return { EQUIPMENT_SLOT_WRISTS };
            case INVTYPE_HANDS:             return { EQUIPMENT_SLOT_HANDS };
            case INVTYPE_FINGER:            return { EQUIPMENT_SLOT_FINGER1, EQUIPMENT_SLOT_FINGER2 };
            case INVTYPE_TRINKET:           return { EQUIPMENT_SLOT_TRINKET1, EQUIPMENT_SLOT_TRINKET2 };
            case INVTYPE_CLOAK:             return { EQUIPMENT_SLOT_BACK };
            case INVTYPE_TABARD:            return { EQUIPMENT_SLOT_TABARD };
            case INVTYPE_SHIELD:
            case INVTYPE_WEAPONOFFHAND:
            case INVTYPE_HOLDABLE:          return { EQUIPMENT_SLOT_OFFHAND };
            case INVTYPE_WEAPONMAINHAND:    return { EQUIPMENT_SLOT_MAINHAND };
            case INVTYPE_2HWEAPON:          return { EQUIPMENT_SLOT_MAINHAND };
            case INVTYPE_WEAPON:            return { EQUIPMENT_SLOT_MAINHAND, EQUIPMENT_SLOT_OFFHAND };
            case INVTYPE_RANGED:
            case INVTYPE_RANGEDRIGHT:       return { EQUIPMENT_SLOT_MAINHAND };
            default:                        return { };
        }
    }

    // Rewrites `character_select_screen_equipment_cache` for an offline character.
    //
    // This cache - not `character_inventory` - is what CHAR_SEL_ENUM joins to fill the VisibleItems of
    // the character-selection model, and the core's only other writer is
    // Player::_SaveCharacterSelectOutfit, which runs on a LOGIN SAVE. A boost applied at the glue screen
    // never passes through that, so without this the boosted character would go on showing its old,
    // pre-boost gear on the selection screen until it had been played once.
    //
    // Transmog is deliberately not consulted: the character is offline, so there is no active outfit to
    // read, and the default appearance of what it now wears is the honest answer.
    void WriteCharacterSelectEquipment(ObjectGuid characterGuid,
        std::array<uint32, EQUIPMENT_SLOT_END> const& equippedItemIds, CharacterDatabaseTransaction trans)
    {
        CharacterDatabasePreparedStatement* del = CharacterDatabase.GetPreparedStatement(CHAR_DEL_CHARACTER_SELECT_EQUIPMENT_CACHE_CUSTOMIZATIONS);
        del->setUInt64(0, characterGuid.GetCounter());
        trans->Append(del);

        CharacterDatabasePreparedStatement* ins = CharacterDatabase.GetPreparedStatement(CHAR_INS_CHARACTER_SELECT_EQUIPMENT_CACHE_CUSTOMIZATIONS);
        ins->setUInt64(0, characterGuid.GetCounter());

        for (uint8 slot = EQUIPMENT_SLOT_START; slot < EQUIPMENT_SLOT_END; ++slot)
        {
            uint32 const itemId = equippedItemIds[slot];
            uint32 visibleItemId = 0;
            uint8 subClass = 0;
            uint8 inventoryType = uint8(INVTYPE_NON_EQUIP);
            int32 displayId = 0;

            if (itemId)
            {
                if (ItemModifiedAppearanceEntry const* appearance = TransmogMgr::GetDefaultItemModifiedAppearance(itemId))
                {
                    if (ItemEntry const* itemEntry = sItemStore.LookupEntry(appearance->ItemID))
                    {
                        visibleItemId = itemEntry->ID;
                        subClass = itemEntry->SubclassID;
                        inventoryType = uint8(itemEntry->InventoryType);
                    }

                    if (ItemAppearanceEntry const* itemAppearance = sItemAppearanceStore.LookupEntry(appearance->ItemAppearanceID))
                        displayId = itemAppearance->ItemDisplayInfoID;
                }
                else if (ItemTemplate const* proto = sObjectMgr->GetItemTemplate(itemId))
                {
                    // No appearance record (a container, a non-transmoggable item): the client still
                    // wants the item's own identity, it just has nothing to draw for it.
                    visibleItemId = itemId;
                    subClass = uint8(proto->GetSubClass());
                    inventoryType = uint8(proto->GetInventoryType());
                }
            }

            // The same eight columns per slot, in the same order, as Player::_SaveCharacterSelectOutfit.
            uint8 const base = 1 + (slot * 8);
            ins->setUInt32(base + 0, itemId);
            ins->setUInt32(base + 1, visibleItemId);
            ins->setUInt8(base + 2, subClass);
            ins->setUInt8(base + 3, inventoryType);
            ins->setUInt32(base + 4, uint32(displayId));
            ins->setUInt32(base + 5, 0);                    // DisplayEnchantID: offline, no illusion applied
            ins->setInt32(base + 6, 0);                     // SecondaryItemModifiedAppearanceID
            ins->setUInt8(base + 7, 0);                     // SheatheCategory
        }

        trans->Append(ins);
    }
}

BattlePayMgr* BattlePayMgr::instance()
{
    static BattlePayMgr instance;
    return &instance;
}

bool BattlePayMgr::LoadBlobFile(std::string const& fileName, std::vector<uint8>& out)
{
    out.clear();

    std::string const path = sWorld->GetDataPath() + "battlepay/" + fileName;
    std::ifstream in(path, std::ios::binary | std::ios::ate);
    if (!in)
    {
        TC_LOG_INFO("server.loading", "BattlePay: no blob at '{}'.", path);
        return false;
    }

    std::streamsize const size = in.tellg();
    if (size <= 0)
    {
        TC_LOG_ERROR("server.loading", "BattlePay: blob '{}' is empty.", path);
        return false;
    }

    in.seekg(0, std::ios::beg);
    out.resize(static_cast<size_t>(size));
    if (!in.read(reinterpret_cast<char*>(out.data()), size))
    {
        TC_LOG_ERROR("server.loading", "BattlePay: failed reading blob '{}'.", path);
        out.clear();
        return false;
    }

    return true;
}

void BattlePayMgr::Load()
{
    uint32 const oldMSTime = getMSTime();

    if (LoadBlobFile("product_list_68275.bin", _templateBlob))
    {
        TC_LOG_INFO("server.loading", "BattlePay: loaded {}-byte catalog template in {} ms.",
            _templateBlob.size(), GetMSTimeDiffToNow(oldMSTime));

        // Trust the reskin path only if the writer reproduces the template byte-exact.
        if (!BattlePayCatalogWriter::SelfCheck(_templateBlob))
            TC_LOG_ERROR("server.loading", "BattlePay: catalog writer self-check FAILED; catalog will be served verbatim (no DB reskin).");
    }
    else
        TC_LOG_INFO("server.loading", "BattlePay: no catalog template - the in-game Shop will open empty.");

    // The distribution list unblocks the client's shop panel (StoreFrame_IsLoading). Replay the
    // captured 68275 blob; absence is non-fatal (the panel just keeps waiting on HasDistributionList).
    if (LoadBlobFile("distribution_list_68275.bin", _distributionListBlob))
        TC_LOG_INFO("server.loading", "BattlePay: loaded {}-byte distribution list.", _distributionListBlob.size());

    // Seed the persistent PurchaseID counter from the ledger so ids survive restarts and never repeat
    // (C-13). Namespace this realm's ids in the high 32 bits so multiple realms sharing one auth DB never
    // collide (C-22): this realm allocates only within [realmBase, realmBase + 0xFFFFFFFF].
    uint64 const realmBase = uint64(sRealmList->GetCurrentRealmId().Realm) << 32;
    LoginDatabasePreparedStatement* stmt = LoginDatabase.GetPreparedStatement(LOGIN_SEL_BATTLEPAY_PURCHASE_MAXID);
    stmt->setUInt64(0, realmBase);
    stmt->setUInt64(1, realmBase | UI64LIT(0xFFFFFFFF));
    PreparedQueryResult maxIdResult = LoginDatabase.Query(stmt);
    if (maxIdResult && !(*maxIdResult)[0].IsNull())
        _purchaseCounter = (*maxIdResult)[0].GetUInt64();
    else
        _purchaseCounter = realmBase;

    // DistributionIDs are allocated on exactly the same terms as PurchaseIDs: persistent, monotonic and
    // namespaced by realm, so an id the client is holding never comes to mean a different entitlement.
    LoginDatabasePreparedStatement* distStmt = LoginDatabase.GetPreparedStatement(LOGIN_SEL_BATTLEPAY_ENTITLEMENT_MAXID);
    distStmt->setUInt64(0, realmBase);
    distStmt->setUInt64(1, realmBase | UI64LIT(0xFFFFFFFF));
    PreparedQueryResult maxDistResult = LoginDatabase.Query(distStmt);
    if (maxDistResult && !(*maxDistResult)[0].IsNull())
        _distributionCounter = (*maxDistResult)[0].GetUInt64();
    else
        _distributionCounter = realmBase;
}

void BattlePayMgr::RecordPurchase(uint32 accountId, uint64 purchaseID, int32 status, int32 resultCode,
    uint32 productID, uint64 basePrice, uint64 userPrice)
{
    LoginDatabasePreparedStatement* stmt = LoginDatabase.GetPreparedStatement(LOGIN_INS_BATTLEPAY_PURCHASE);
    stmt->setUInt64(0, purchaseID);
    stmt->setUInt32(1, accountId);
    stmt->setUInt32(2, productID);
    stmt->setInt32(3, status);
    stmt->setInt32(4, resultCode);
    stmt->setUInt64(5, basePrice);
    stmt->setUInt64(6, userPrice);
    stmt->setInt64(7, GameTime::GetGameTime());
    stmt->setString(8, std::string());     // walletName: empty on this core (sent record-final on the wire)
    LoginDatabase.Execute(stmt);
}

uint8 BattlePayMgr::GetServiceType(ShopProduct const& product)
{
    for (ShopDeliverable const& d : product.Deliverables)
        if (d.Type == 5)
            return uint8(d.Id);
    return 0;
}

bool BattlePayMgr::NeedsEntitlement(ShopProduct const& product, Player* player)
{
    // A service deliverable never has an immediate form: its target is a character chosen after the
    // purchase, so it always becomes an entitlement even for a logged-in buyer.
    if (GetServiceType(product) != 0)
        return true;

    // Otherwise it is only deferred because there is nobody to deliver to - i.e. we are at character
    // select. A product with no deliverables at all is display-only and never reaches here.
    return player == nullptr && !product.Deliverables.empty();
}

uint64 BattlePayMgr::CreateEntitlement(uint32 accountId, uint32 productId, uint8 serviceType, uint64 purchaseId)
{
    uint64 const distributionId = GenerateDistributionID();
    time_t const now = GameTime::GetGameTime();

    LoginDatabasePreparedStatement* stmt = LoginDatabase.GetPreparedStatement(LOGIN_INS_BATTLEPAY_ENTITLEMENT);
    stmt->setUInt64(0, distributionId);
    stmt->setUInt32(1, accountId);
    stmt->setUInt32(2, productId);
    stmt->setUInt8(3, serviceType);
    stmt->setUInt8(4, SHOP_ENTITLEMENT_AVAILABLE);
    stmt->setUInt64(5, purchaseId);
    stmt->setInt64(6, now);
    stmt->setInt64(7, now);
    LoginDatabase.DirectExecute(stmt);

    // Read it back: a synchronous insert that silently failed (duplicate id after a hand-edited table,
    // lost connection) must not be reported as an entitlement the buyer now owns and can be charged for.
    LoginDatabasePreparedStatement* verify = LoginDatabase.GetPreparedStatement(LOGIN_SEL_BATTLEPAY_ENTITLEMENT_BY_ID);
    verify->setUInt64(0, distributionId);
    PreparedQueryResult result = LoginDatabase.Query(verify);
    if (!result || (*result)[0].GetUInt32() != accountId)
    {
        TC_LOG_ERROR("network", "BattlePay: failed to persist entitlement {} (account {}, product {}).",
            distributionId, accountId, productId);
        return 0;
    }

    TC_LOG_INFO("network", "BattlePay: created entitlement {} for account {} (product {}, service {}).",
        distributionId, accountId, productId, serviceType);
    return distributionId;
}

bool BattlePayMgr::ClaimEntitlement(uint32 accountId, uint64 distributionId, uint32 realmId, uint64 targetCharacter,
    uint64& outToken, ShopEntitlement& outEntitlement, int32& outResult)
{
    // Client PurchaseResult codes, straight out of the client's own generated API documentation
    // (Blizzard_APIDocumentationGenerated/BattlepayConstantsDocumentation.lua).
    constexpr int32 RESULT_DISTRIBUTION_NOT_FOUND       = 19;
    constexpr int32 RESULT_DISTRIBUTION_ALREADY_ASSIGNED = 20;
    constexpr int32 RESULT_DISTRIBUTION_WRONG_ACCOUNT   = 65;

    outResult = RESULT_DISTRIBUTION_NOT_FOUND;

    // A token of 0 is the "unclaimed" sentinel in the table, so never hand one out.
    uint64 token = (uint64(rand32()) << 32) | rand32();
    if (!token)
        token = 1;

    // Compare-and-swap: claim only if the row is still AVAILABLE and still ours.
    LoginDatabasePreparedStatement* claim = LoginDatabase.GetPreparedStatement(LOGIN_UPD_BATTLEPAY_ENTITLEMENT_CLAIM);
    claim->setUInt64(0, token);
    claim->setUInt32(1, realmId);
    claim->setUInt64(2, targetCharacter);
    claim->setInt64(3, GameTime::GetGameTime());
    claim->setUInt64(4, distributionId);
    claim->setUInt32(5, accountId);
    LoginDatabase.DirectExecute(claim);

    // Read the row back and proceed only if OUR token is the one sitting there. This is what makes the
    // claim atomic without an affected-rows count: a racing claimant either never wrote (the status
    // guard rejected it) or wrote first, in which case the token we read back is not ours and we lose.
    LoginDatabasePreparedStatement* verify = LoginDatabase.GetPreparedStatement(LOGIN_SEL_BATTLEPAY_ENTITLEMENT_BY_ID);
    verify->setUInt64(0, distributionId);
    PreparedQueryResult result = LoginDatabase.Query(verify);
    if (!result)
        return false;

    Field* fields = result->Fetch();
    uint32 const rowAccount = fields[0].GetUInt32();
    uint8 const rowStatus   = fields[3].GetUInt8();
    uint64 const rowToken   = fields[4].GetUInt64();

    if (rowAccount != accountId)
    {
        outResult = RESULT_DISTRIBUTION_WRONG_ACCOUNT;
        return false;
    }
    if (rowStatus != SHOP_ENTITLEMENT_CLAIMED || rowToken != token)
    {
        // Either it was already spent, or another claimant got there first.
        outResult = rowStatus == SHOP_ENTITLEMENT_AVAILABLE
            ? RESULT_DISTRIBUTION_NOT_FOUND : RESULT_DISTRIBUTION_ALREADY_ASSIGNED;
        return false;
    }

    outToken = token;
    outEntitlement.DistributionID = distributionId;
    outEntitlement.ProductID      = fields[1].GetUInt32();
    outEntitlement.ServiceType    = fields[2].GetUInt8();
    outEntitlement.Status         = rowStatus;
    outResult = 0;
    return true;
}

bool BattlePayMgr::TransitionEntitlement(uint64 distributionId, uint8 fromStatus, uint64 fromToken,
    uint8 toStatus, uint64 toToken)
{
    LoginDatabasePreparedStatement* stmt = LoginDatabase.GetPreparedStatement(LOGIN_UPD_BATTLEPAY_ENTITLEMENT_STATUS);
    stmt->setUInt8(0, toStatus);
    stmt->setUInt64(1, toToken);
    stmt->setInt64(2, GameTime::GetGameTime());
    stmt->setUInt64(3, distributionId);
    stmt->setUInt8(4, fromStatus);
    stmt->setUInt64(5, fromToken);
    LoginDatabase.DirectExecute(stmt);

    LoginDatabasePreparedStatement* verify = LoginDatabase.GetPreparedStatement(LOGIN_SEL_BATTLEPAY_ENTITLEMENT_BY_ID);
    verify->setUInt64(0, distributionId);
    PreparedQueryResult result = LoginDatabase.Query(verify);
    if (!result)
        return false;

    Field* fields = result->Fetch();
    return fields[3].GetUInt8() == toStatus && fields[4].GetUInt64() == toToken;
}

uint8 BattlePayMgr::GetCharacterBoostLevel()
{
    uint32 const configured = sWorld->getIntConfig(CONFIG_SHOP_CHARACTER_BOOST_LEVEL);
    uint32 const maxLevel = sWorld->getIntConfig(CONFIG_MAX_PLAYER_LEVEL);
    return uint8(std::min(configured, maxLevel));
}

int32 BattlePayMgr::GetCharacterBoostType()
{
    return int32(sWorld->getIntConfig(CONFIG_SHOP_CHARACTER_BOOST_TYPE));
}

ShopRealMoneyMode BattlePayMgr::GetRealMoneyMode()
{
    // Read the string directly from the config: World has no string-config array, and this is queried
    // only on OPEN_CHECKOUT of a real-money product (rare), so a per-call lookup is fine. Default
    // "instant" so a fresh test realm can buy real-money products for QA without a web backend.
    std::string const mode = sConfigMgr->GetStringDefault("Shop.RealMoney.Mode", "instant");
    if (mode == "web")
        return SHOP_REAL_MONEY_WEB;
    if (mode == "disabled")
        return SHOP_REAL_MONEY_DISABLED;
    if (mode == "instant")
        return SHOP_REAL_MONEY_INSTANT;

    TC_LOG_ERROR("server.loading", "BattlePay: Shop.RealMoney.Mode = '{}' is not one of web|instant|disabled; "
        "using 'instant'.", mode);
    return SHOP_REAL_MONEY_INSTANT;
}

bool BattlePayMgr::IsCharacterBoostProduct(ShopProduct const& product)
{
    return GetServiceType(product) == SHOP_SERVICE_CHARACTER_BOOST;
}

// Turns an offline character into a boosted one, as one transaction the caller commits.
//
// NOTHING IS DELETED. The original equipment is displaced into free backpack slots rather than
// destroyed, because a boost that eats the gear a player levelled in is a bug report we can never
// answer. That is also why the whole placement is decided in memory FIRST and the function bails out
// with an empty transaction if it does not fit: a half-placed kit would be worse than no kit.
//
// Statement order inside the transaction matters. `character_inventory` carries a UNIQUE key on
// (guid, bag, slot), so the displaced originals are moved to their (already free) backpack slots
// BEFORE the kit is written into the equipment slots they just vacated. At no point do two rows claim
// the same slot, so no REPLACE ever silently deletes another item's row.
CharacterDatabaseTransaction BattlePayMgr::BuildCharacterBoostTransaction(ShopBoostTarget const& target,
    uint32 specializationId, uint64 distributionId, uint32 productId) const
{
    CharacterLoadoutEntry const* loadout = SelectBoostLoadout(target.ClassId, target.RaceId);
    if (!loadout)
    {
        TC_LOG_ERROR("network", "BattlePay: no CharacterLoadout with purpose {} / item context {} for class {} race {}; "
            "cannot boost {}.", SHOP_BOOST_LOADOUT_PURPOSE, uint32(SHOP_BOOST_ITEM_CONTEXT),
            target.ClassId, target.RaceId, target.Guid.ToString());
        return nullptr;
    }

    std::vector<uint32> kit;
    for (CharacterLoadoutItemEntry const* row : sCharacterLoadoutItemStore)
        if (row->CharacterLoadoutID == loadout->ID)
            kit.push_back(row->ItemID);

    if (kit.empty())
    {
        TC_LOG_ERROR("network", "BattlePay: CharacterLoadout {} has no items; cannot boost {}.",
            loadout->ID, target.Guid.ToString());
        return nullptr;
    }

    // ---- Plan the placement in memory ---------------------------------------------------------------
    std::set<uint8> occupied;
    for (auto const& ownSlot : target.OwnSlots)
        occupied.insert(ownSlot.first);

    // Only the backpack slots this character has actually unlocked - writing past inventorySlots would
    // put items where the client has no slot to draw, and the login inventory load would have to rescue
    // them by mail.
    uint8 const backpackEnd = uint8(std::min<uint32>(uint32(INVENTORY_SLOT_ITEM_START) + target.BackpackSlots,
        uint32(INVENTORY_SLOT_ITEM_END)));
    auto takeBackpackSlot = [&occupied, backpackEnd]() -> uint8
    {
        for (uint8 slot = INVENTORY_SLOT_ITEM_START; slot < backpackEnd; ++slot)
            if (occupied.insert(slot).second)
                return slot;
        return NULL_SLOT;
    };

    std::vector<std::pair<uint64, uint8>> moves;             // item guid -> new backpack slot
    std::vector<std::pair<uint32, uint8>> placements;        // kit item id -> slot
    std::array<uint32, EQUIPMENT_SLOT_END> visibleItems = { };
    for (auto const& [slot, occupant] : target.OwnSlots)
        if (slot < EQUIPMENT_SLOT_END)
            visibleItems[slot] = occupant.ItemId;

    for (uint32 itemId : kit)
    {
        ItemTemplate const* proto = sObjectMgr->GetItemTemplate(itemId);
        if (!proto)
        {
            TC_LOG_ERROR("network", "BattlePay: CharacterLoadout {} names item {}, which has no template; "
                "cannot boost {}.", loadout->ID, itemId, target.Guid.ToString());
            return nullptr;
        }

        uint8 chosen = NULL_SLOT;
        for (uint8 slot : GetEquipmentSlotsFor(proto))
        {
            // Free slot: take it. Occupied slot: take it too, but only by moving what is there into
            // the backpack first - and only if the backpack has room, otherwise try the next slot.
            if (occupied.insert(slot).second)
            {
                chosen = slot;
                break;
            }

            auto const occupant = target.OwnSlots.find(slot);
            if (occupant == target.OwnSlots.end())
                continue;                                   // already re-used by an earlier kit piece

            if (std::any_of(moves.begin(), moves.end(),
                [&](std::pair<uint64, uint8> const& move) { return move.first == occupant->second.ItemGuid; }))
                continue;                                   // already displaced by an earlier kit piece

            uint8 const spare = takeBackpackSlot();
            if (spare == NULL_SLOT)
                continue;

            moves.emplace_back(occupant->second.ItemGuid, spare);
            // It is no longer worn, so it leaves the character-select look; the kit piece that took its
            // place puts its own item id back in below.
            if (slot < EQUIPMENT_SLOT_END)
                visibleItems[slot] = 0;
            chosen = slot;
            break;
        }

        if (chosen == NULL_SLOT)
            chosen = takeBackpackSlot();                    // not equipment, or every candidate slot lost

        if (chosen == NULL_SLOT)
        {
            TC_LOG_ERROR("network", "BattlePay: {} has no room for the boost kit (loadout {}); the boost was "
                "not applied.", target.Guid.ToString(), loadout->ID);
            return nullptr;
        }

        placements.emplace_back(itemId, chosen);
        if (chosen < EQUIPMENT_SLOT_END)
            visibleItems[chosen] = itemId;
    }

    // ---- Write it -----------------------------------------------------------------------------------
    CharacterDatabaseTransaction trans = CharacterDatabase.BeginTransaction();

    for (auto const& [itemGuid, newSlot] : moves)
    {
        CharacterDatabasePreparedStatement* stmt = CharacterDatabase.GetPreparedStatement(CHAR_REP_INVENTORY_ITEM);
        stmt->setUInt64(0, target.Guid.GetCounter());
        stmt->setUInt64(1, UI64LIT(0));
        stmt->setUInt8(2, newSlot);
        stmt->setUInt64(3, itemGuid);
        trans->Append(stmt);
    }

    for (auto const& [itemId, slot] : placements)
    {
        // The boost ItemContext is what gives these items their bonus lists (and with them their item
        // level) - Item::CreateItem resolves them from the client data, so no bonus id is invented here.
        std::unique_ptr<Item> item(Item::CreateItem(itemId, 1, SHOP_BOOST_ITEM_CONTEXT, nullptr));
        if (!item)
        {
            TC_LOG_ERROR("network", "BattlePay: could not create boost kit item {} for {}; the boost was "
                "not applied.", itemId, target.Guid.ToString());
            return nullptr;
        }

        item->SetOwnerGUID(target.Guid);
        item->SaveToDB(trans);

        CharacterDatabasePreparedStatement* stmt = CharacterDatabase.GetPreparedStatement(CHAR_REP_INVENTORY_ITEM);
        stmt->setUInt64(0, target.Guid.GetCounter());
        stmt->setUInt64(1, UI64LIT(0));
        stmt->setUInt8(2, slot);
        stmt->setUInt64(3, item->GetGUID().GetCounter());
        trans->Append(stmt);
    }

    CharacterDatabasePreparedStatement* levelStmt = CharacterDatabase.GetPreparedStatement(CHAR_UPD_SHOP_BOOST_CHARACTER);
    levelStmt->setUInt8(0, GetCharacterBoostLevel());
    levelStmt->setUInt32(1, specializationId);
    levelStmt->setUInt64(2, target.Guid.GetCounter());
    trans->Append(levelStmt);

    WriteCharacterSelectEquipment(target.Guid, visibleItems, trans);

    CharacterDatabasePreparedStatement* boostStmt = CharacterDatabase.GetPreparedStatement(CHAR_REP_SHOP_BOOST);
    boostStmt->setUInt64(0, target.Guid.GetCounter());
    boostStmt->setUInt32(1, productId);
    boostStmt->setUInt64(2, distributionId);
    boostStmt->setUInt32(3, specializationId);
    boostStmt->setUInt8(4, 0);                              // boosted, not a pending class trial
    boostStmt->setInt64(5, GameTime::GetGameTime());
    trans->Append(boostStmt);

    TC_LOG_INFO("network", "BattlePay: boosting {} to level {} (class {}, race {}, spec {}) with loadout {}: "
        "{} kit item(s), {} original item(s) displaced into the backpack.", target.Guid.ToString(),
        GetCharacterBoostLevel(), target.ClassId, target.RaceId, specializationId, loadout->ID,
        placements.size(), moves.size());

    return trans;
}

void BattlePayMgr::LoadProducts()
{
    _products.clear();
    _slotOverrides.clear();

    //                                                    0          1        2     3            4         5      6            7               8             9             10       11        12        13                             14                              15        16          17           18                 19
    QueryResult result = WorldDatabase.Query("SELECT productId, enabled, name, description, currency, price, priceItemId, priceItemCount, displayPrice, displayFlags, groupId, ordering, featured, UNIX_TIMESTAMP(availableFrom), UNIX_TIMESTAMP(availableUntil), reqLevel, reqFaction, hideIfOwned, playerConditionId, comment FROM shop_product");
    if (result)
    {
        do
        {
            Field* f = result->Fetch();
            ShopProduct p;
            p.ProductID      = f[0].GetUInt32();
            p.Enabled        = f[1].GetUInt8() != 0;
            p.Name           = f[2].GetString();
            p.Description    = f[3].GetString();
            p.Currency       = f[4].GetUInt8();
            p.Price          = f[5].GetUInt64();
            p.PriceItemId    = f[6].GetUInt32();
            p.PriceItemCount = f[7].GetUInt32();
            if (!f[8].IsNull())
            {
                p.HasDisplayPrice = true;
                p.DisplayPrice = f[8].GetUInt64();
            }
            p.DisplayFlags       = f[9].GetUInt32();
            p.GroupId            = f[10].GetUInt32();
            p.Ordering           = f[11].GetInt32();
            p.Featured           = f[12].GetUInt8() != 0;
            if (!f[13].IsNull())
                p.AvailableFrom = time_t(f[13].GetInt64());
            if (!f[14].IsNull())
                p.AvailableUntil = time_t(f[14].GetInt64());
            p.ReqLevel           = f[15].GetUInt8();
            p.ReqFaction         = f[16].GetInt8();
            p.HideIfOwned        = f[17].GetUInt8() != 0;
            p.PlayerConditionId  = f[18].GetUInt32();
            p.Comment            = f[19].GetString();
            _products[p.ProductID] = std::move(p);
        }
        while (result->NextRow());

        if (QueryResult deliverables = WorldDatabase.Query("SELECT productId, seq, type, id, count FROM shop_product_deliverable ORDER BY productId, seq"))
        {
            do
            {
                Field* f = deliverables->Fetch();
                uint32 const productId = f[0].GetUInt32();
                auto itr = _products.find(productId);
                if (itr == _products.end())
                {
                    TC_LOG_ERROR("sql.sql", "BattlePay: shop_product_deliverable references unknown productId {} - skipped.", productId);
                    continue;
                }
                ShopDeliverable dv;
                dv.Type  = f[2].GetUInt8();
                dv.Id    = f[3].GetUInt32();
                dv.Count = f[4].GetUInt32();
                if (!dv.Count)
                    dv.Count = 1;
                itr->second.Deliverables.push_back(dv);
            }
            while (deliverables->NextRow());
        }
    }

    if (QueryResult overrides = WorldDatabase.Query("SELECT slotIndex, productId FROM shop_slot_override"))
    {
        do
        {
            Field* f = overrides->Fetch();
            _slotOverrides[f[0].GetUInt8()] = f[1].GetUInt32();
        }
        while (overrides->NextRow());
    }

    TC_LOG_INFO("server.loading", "BattlePay: loaded {} shop products, {} slot overrides.", _products.size(), _slotOverrides.size());
}

bool BattlePayMgr::AssembleCatalog(std::vector<uint8>& outBlob, std::unordered_map<uint32, uint32>& outRouting,
    std::string* report) const
{
    outRouting.clear();

    if (_templateBlob.empty())
        return false;

    // The writer now decodes ALL FOUR arrays (94 products, 116 deliverables, 21 groups, 97 shop
    // entries) with a byte-exact round trip, instead of only the first 9 "simple shape" records.
    BattlePayCatalog catalog;
    std::string parseError;
    if (!BattlePayCatalogWriter::Parse(_templateBlob, catalog, &parseError))
    {
        TC_LOG_ERROR("server.loading", "BattlePay: catalog template did not parse ({}); serving it verbatim.", parseError);
        outBlob = _templateBlob;
        return false;
    }

    time_t const now = GameTime::GetGameTime();
    std::string const placeholderName = std::string(sConfigMgr->GetStringDefault("Shop.PlaceholderName", "Currently unavailable"));

    // Products pinned to a specific slot are excluded from the automatic fill so they show once only.
    std::unordered_map<uint32, bool> pinned;
    for (auto const& [slot, productId] : _slotOverrides)
        if (productId)
            pinned[productId] = true;

    // Candidate set = enabled + in-window, not pinned; sorted featured DESC, ordering ASC, productId ASC.
    std::vector<ShopProduct const*> candidates;
    for (auto const& [id, product] : _products)
        if (product.Enabled && InWindow(product, now) && pinned.find(id) == pinned.end())
            candidates.push_back(&product);

    std::sort(candidates.begin(), candidates.end(), [](ShopProduct const* a, ShopProduct const* b)
    {
        if (a->Featured != b->Featured) return a->Featured > b->Featured;
        if (a->Ordering != b->Ordering) return a->Ordering < b->Ordering;
        return a->ProductID < b->ProductID;
    });

    auto reskin = [&](BattlePayCatalogProduct& rec, ShopProduct const& product)
    {
        uint64 displayPrice;
        if (product.HasDisplayPrice)
            displayPrice = product.DisplayPrice;
        else if (product.Currency == 1)                 // gold: copper -> shop fixed-point /100000
            displayPrice = (product.Price / 10000) * 100000;
        else
            displayPrice = 0;

        // Only what the admin row actually asks for. The two synthesised bits below are DISABLED because
        // their values were never verified against the client:
        //
        //   DISPLAY_FLAG_HIDE_WHEN_OWNED = 256  and  DISPLAY_FLAG_HIDDEN_PRICE = 8
        //
        // They were harmless while Flags was (incorrectly) being written into DisplayInfo.modelSceneID -
        // nothing read them. Now that Flags reaches the client's real BattlepayDisplayFlags, ORing 256 in
        // for every product (all 66 admin rows carry hideIfOwned = 1) made pets and mounts the player does
        // NOT own render as already owned. So bit 256 does not mean "hide when owned"; its true meaning is
        // unknown.
        //
        // Do not re-enable either bit until BattlepayDisplayFlags is recovered from the client. Emitting a
        // flag whose meaning we are guessing at is exactly how this regression happened.
        // Do NOT overwrite the shipped Product.Flags. It is a SEPARATE, unreflected enum - not
        // BattlepayDisplayFlags - and its bits 1/3 drive `buyableHere`. Writing our admin DisplayFlags
        // (0 for all 66 rows) over it clears buyableHere and makes a pinned product unpurchasable.
        // Admin display flags belong on DisplayInfo.Flags, which is what sharedData.flags actually
        // loads from (client RVA 0x23D0DA7); Product.flags is never read for display.
        uint32 const displayInfoFlags = product.DisplayFlags;

        rec.NormalPriceFixedPoint  = displayPrice;
        rec.CurrentPriceFixedPoint = displayPrice;
        // rec.Flags deliberately left as shipped - see above.

        // Name and description live in the product's DisplayInfo, not on the product record. 54 of the
        // 94 shipped records have no DisplayInfo at all - that is exactly why their purchase
        // confirmation showed a nil name and the generic INV_Misc_Note_02 fallback icon.
        if (!rec.DisplayInfo)
            rec.DisplayInfo = BattlePayCatalogWriter::MakeDisplayInfo(product.Name, product.Description);
        else
        {
            rec.DisplayInfo->Name1 = product.Name;
            rec.DisplayInfo->Name3 = product.Description;
        }

        // Enum.BattlepayDisplayFlags (client registrar RVA 0x13EF840, 12 values):
        //   1 Expansion, 2 CardDoesNotShowModel, 4 CardAlwaysShowsTexture, 8 HiddenPrice,
        //   16 UseHorizontalLayoutForFullCard, 32/64 Deprecated, 128 UseSquareIconBorder,
        //   256 HideWhenOwned, 512 RafReward, 1024 ShowFancyToast, 2048 UseIconBorderWithOverrideTexture.
        // These belong on DisplayInfo.Flags. 256 only hides an ALREADY-owned entry from the grid; it is
        // not what made everything look owned (that was Product.Eligibility - see below).
        if (displayInfoFlags)
            rec.DisplayInfo->Flags = displayInfoFlags;
    };

    size_t candIdx = 0;
    for (size_t slot = 0; slot < catalog.Products.size(); ++slot)
    {
        // The id the client actually purchases by. The previous code read record+81, which is really
        // DisplayInfo.fileDataID - the card ARTWORK id - so the routing map was built from FileDataIDs
        // and never resolved. Purchases only worked because GetProductByAdvertisedId falls through to
        // GetProduct(id).
        uint32 const slotProductId = catalog.Products[slot].ProductID;
        ShopProduct const* assigned = nullptr;

        auto ovr = _slotOverrides.find(uint8(slot));
        if (ovr != _slotOverrides.end())
        {
            if (ovr->second != 0)                       // pinned; 0 = forced placeholder
                assigned = GetProduct(ovr->second);
        }
        // NOTE: no automatic slot filling any more. The catalog we ship already describes 94 real
        // products with our own gold prices patched in, and shop_product is keyed on those same
        // wire productIDs, so every card resolves on its own. Auto-assigning candidates to slots
        // here would stamp one product's name and price onto a different product's card - harmless
        // when only 9 rows existed, actively wrong now there are 66. Explicit shop_slot_override
        // pins still work above for anyone who wants to re-badge a specific slot.

        if (assigned)
        {
            reskin(catalog.Products[slot], *assigned);
            outRouting[slotProductId] = assigned->ProductID;
            if (report)
                report->append(Trinity::StringFormat("  slot {}: [{}] '{}' -> product {} (price {}, {}{})\n",
                    slot, slotProductId, assigned->Name, assigned->ProductID, assigned->Price,
                    assigned->Enabled ? "enabled" : "disabled", assigned->Featured ? ", featured" : ""));
        }
        else
        {
            // Not pinned. Leave the shipped record's price alone - the blob already describes a real
            // product with our gold price patched in, and shop_product is keyed on the same wire
            // productID. Only give it a DisplayInfo when it has none, so the confirmation dialog can
            // render a name instead of erroring on nil.
            if (!catalog.Products[slot].DisplayInfo)
            {
                if (ShopProduct const* known = GetProduct(slotProductId))
                    catalog.Products[slot].DisplayInfo = BattlePayCatalogWriter::MakeDisplayInfo(known->Name, known->Description);
                else
                    catalog.Products[slot].DisplayInfo = BattlePayCatalogWriter::MakeDisplayInfo(placeholderName, std::string());
            }
            if (report)
                report->append(Trinity::StringFormat("  slot {}: [{}] <placeholder - not purchasable>\n", slot, slotProductId));
        }
    }

    if (report && candIdx < candidates.size())
        report->append(Trinity::StringFormat("  OVERFLOW: {} enabled product(s) could not be shown (only {} slots).\n",
            candidates.size() - candIdx, catalog.Products.size()));

    // THE OWNERSHIP GATE. The captured retail catalog marks products the CAPTURING account already owned:
    //   - Product.Eligibility == 2 (Enum.PurchaseEligibility.Owned), which Lua's
    //     StoreFrame_IsCompletelyOwned reads via sharedData.eligibility and which greys the Buy button;
    //     the client also promotes Eligibility 1 (PartiallyOwned) to Owned unless Product.flags bit
    //     15/16 is set, so 12 records are affected, not just the 7 marked 2.
    //   - Deliverable.AlreadyOwns == 1, which IsProductAlreadyOwned (client RVA 0x23CC780) walks via
    //     Product.DeliverableIDs. C_StoreSecure.PurchaseProduct (RVA 0x23CDFD0) consults it and then
    //     NEVER SENDS THE CMSG, and GetProducts (RVA 0x23D1A06) hides owned products outright.
    //
    // BOTH have to be cleared. Clearing only Eligibility restores the Buy button but clicking it does
    // nothing, because the deliverable check silently swallows the purchase.
    //
    // This is not a display preference - it is stale state from someone else's account that we were
    // shipping back verbatim. Ownership on THIS realm is decided by IsPurchasable() at purchase time.
    for (BattlePayCatalogProduct& product : catalog.Products)
        product.Eligibility = 0;                        // PurchaseEligibility::Ok

    for (BattlePayDeliverable& deliverable : catalog.Deliverables)
    {
        deliverable.AlreadyOwns = 0;
        for (BattlePayDeliverableChoice& choice : deliverable.Choices)
            choice.AlreadyOwns = 0;
    }

    outBlob = BattlePayCatalogWriter::Serialize(catalog);
    return true;
}

void BattlePayMgr::LoadCatalog()
{
    uint32 const oldMSTime = getMSTime();

    LoadProducts();

    std::vector<uint8> blob;
    std::unordered_map<uint32, uint32> routing;
    if (_products.empty())
    {
        // No DB catalog: serve the raw template so the shop still opens (nothing purchasable).
        _productListBlob = _templateBlob;
        _slotRouting.clear();
        TC_LOG_INFO("server.loading", "BattlePay: no shop_product rows; serving the catalog template verbatim.");
    }
    else if (AssembleCatalog(blob, routing, nullptr))
    {
        _productListBlob = std::move(blob);
        _slotRouting = std::move(routing);
        TC_LOG_INFO("server.loading", "BattlePay: assembled {}-byte catalog ({} routed slots) in {} ms.",
            _productListBlob.size(), _slotRouting.size(), GetMSTimeDiffToNow(oldMSTime));
    }
    else
    {
        _productListBlob = _templateBlob;               // assembly failed: fall back to verbatim
        _slotRouting.clear();
    }

    ++_catalogGeneration;

    // Schedule the next automatic rebuild at the earliest future window boundary (restart-free rotation).
    time_t const now = GameTime::GetGameTime();
    _nextRebuildTime = 0;
    for (auto const& [id, product] : _products)
    {
        for (time_t boundary : { product.AvailableFrom, product.AvailableUntil })
            if (boundary > now && (_nextRebuildTime == 0 || boundary < _nextRebuildTime))
                _nextRebuildTime = boundary;
    }

    // The modern ("shop2") storefront serves the same product set over HTTPS. Re-render its snapshot
    // here, on the world thread, so its HTTP workers never read _products while we are rewriting it.
    sShop2Service.RebuildCatalog();
}

void BattlePayMgr::Reload()
{
    LoadCatalog();
}

void BattlePayMgr::RebuildIfDue(time_t now)
{
    if (_nextRebuildTime != 0 && now >= _nextRebuildTime)
    {
        TC_LOG_INFO("server.loading", "BattlePay: availability window boundary reached; rebuilding catalog.");
        Reload();
    }
}

ShopProduct const* BattlePayMgr::GetProduct(uint32 adminProductId) const
{
    auto itr = _products.find(adminProductId);
    return itr != _products.end() ? &itr->second : nullptr;
}

ShopProduct const* BattlePayMgr::GetProductByAdvertisedId(uint32 advertisedProductId) const
{
    // A slot override wins when one is pinned...
    if (auto route = _slotRouting.find(advertisedProductId); route != _slotRouting.end())
        return GetProduct(route->second);

    // ...otherwise the id the client sent IS the catalog's Product.productID, and shop_product is
    // keyed on exactly that. This fallback is what makes the whole catalog sellable instead of only
    // the pinned slots. Previously an unrouted id returned nullptr and the purchase refused, which
    // was every purchase: the old writer model was off by 4 bytes at the header, so the values that
    // went into shop_product.productId were displayInfo.fileDataIDs, not product ids, and no client
    // id could ever match them.
    return GetProduct(advertisedProductId);
}

bool BattlePayMgr::IsAlreadyFullyOwned(ShopProduct const& product, Player* player)
{
    if (product.Deliverables.empty())
        return false;
    for (ShopDeliverable const& d : product.Deliverables)
    {
        if (d.Type != 2)                                // only spell-only bundles count as "ownable"
            return false;
        if (!player->HasSpell(d.Id))
            return false;
    }
    return true;
}

bool BattlePayMgr::IsPurchasable(ShopProduct const& product, Player* player, time_t now) const
{
    if (!player)
        return false;
    if (!product.Enabled || !InWindow(product, now))
        return false;
    if (product.ReqLevel && player->GetLevel() < product.ReqLevel)
        return false;
    if (product.ReqFaction >= 0 && int8(player->GetTeamId()) != product.ReqFaction)
        return false;
    if (product.HideIfOwned && IsAlreadyFullyOwned(product, player))
        return false;
    if (product.PlayerConditionId && !ConditionMgr::IsPlayerMeetingCondition(player, product.PlayerConditionId))
        return false;
    return true;
}

std::string BattlePayMgr::BuildStatusReport() const
{
    std::string report = Trinity::StringFormat("In-game Shop catalog: {} product(s), template {} bytes.\n",
        _products.size(), _templateBlob.size());

    std::vector<uint8> blob;
    std::unordered_map<uint32, uint32> routing;
    AssembleCatalog(blob, routing, &report);
    report.append(Trinity::StringFormat("Assembled blob: {} bytes, generation {}.", blob.size(), _catalogGeneration));
    return report;
}
