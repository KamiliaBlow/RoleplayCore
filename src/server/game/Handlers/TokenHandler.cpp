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

#include "TokenPackets.h"
#include "DatabaseEnv.h"
#include "GameTime.h"
#include "Mail.h"
#include "ObjectAccessor.h"
#include "Player.h"
#include "World.h"
#include "WorldSession.h"
#include "WowTokenMgr.h"

 // --- Token TRADE (sell / buy / redeem): the WoW Token market, gated by WowToken.Market.Enabled ---------
 //
 // The two-step sell/buy/redeem handshakes are wire-derived (WOW_TOKEN_RE_68275.md) and implemented below
 // as REAL, server-authoritative token<->gold swaps at a single configured market price
 // (WowToken.Market.Price). The token is modelled as a real Auction House commodity:
 //
 //   * SELL  : an account's auctionable token is listed on the market (state Listed). The seller is paid
 //             when a buyer takes the listing, by in-game mail, exactly like a normal AH sale.
 //   * BUY   : the buyer pays the market price in gold; the cheapest listing becomes the buyer's consumable
 //             token and the gold is mailed to that listing's seller (gold is conserved buyer -> seller).
 //   * REDEEM: a consumable token is consumed. On retail a token redeems into game time or Battle.net
 //             balance, neither of which exists on this core, so the redeem SINK here is GOLD equal to the
 //             market price (the private-realm interpretation of "balance"; see COMMERCE_RETAIL_PARITY.md
 //             §2.3). Because every legitimately-acquired token traces back to a gold Shop purchase
 //             (GrantType-3, a gold SINK of the same price), redeem-for-gold is economy-neutral over a full
 //             buy->sell->redeem loop and cannot mint gold from a token nobody paid for.
 //
 // The whole market is OFF by default (WowToken.Market.Enabled = 0): while off, the handshakes still
 // complete without stalling the client, but the terminal responses carry TOKEN_RESULT_ERROR_DISABLED and
 // no gold or token moves. This is the operator opt-in the master plan (B3) calls for, not a stub - when
 // disabled the code path is inert on purpose; when enabled it is fully functional.
 //
 // A separate, always-on path (NOT gated here) is buying a WoW Token from the in-game Shop for gold, which
 // delivers an auctionable holding via the GrantType-3 BattlePay path (TK-1).
 //
 // Wire caveat: for the AT_MARKET_PRICE / REDEEM_CONFIRMATION confirm packets the dossier names the field
 // WIDTHS but not which u32 is the ClientToken echoed back for response correlation. We read it from the
 // field position documented in TokenPackets.h; if a future sniff shows a different slot, only the dialog
 // correlation (not the wire framing) is affected.

namespace
{
    // The token market's single price (copper). Buy price == sell payout == redeem grant, so there is no
    // arbitrage between the three legs. uint32 config caps this near 429k gold, ample for a private realm.
    uint64 GetTokenMarketPrice()
    {
        return sWorld->getIntConfig(CONFIG_WOW_TOKEN_MARKET_PRICE);
    }

    bool IsTokenMarketEnabled()
    {
        return sWorld->getBoolConfig(CONFIG_WOW_TOKEN_MARKET_ENABLED);
    }
}

void WorldSession::HandleCommerceTokenGetLog(WorldPackets::Token::CommerceTokenGetLog& commerceTokenGetLog)
{
    WorldPackets::Token::CommerceTokenGetLogResponse response;

    response.ClientToken = commerceTokenGetLog.ClientToken;

    // The market transaction log only means something once the token market is wired (deferred, above).
    // Until WowToken.Market.Enabled is turned on, answer ERROR_DISABLED - the honest "market off" state -
    // rather than an empty SUCCESS log that would imply a working-but-empty market.
    response.Result = sWorld->getBoolConfig(CONFIG_WOW_TOKEN_MARKET_ENABLED) ? TOKEN_RESULT_SUCCESS : TOKEN_RESULT_ERROR_DISABLED;

    SendPacket(response.Write());
}

void WorldSession::HandleCommerceTokenGetMarketPrice(WorldPackets::Token::CommerceTokenGetMarketPrice& commerceTokenGetMarketPrice)
{
    WorldPackets::Token::CommerceTokenGetMarketPriceResponse response;

    response.ClientToken = commerceTokenGetMarketPrice.ClientToken;

    // The market price is the cheapest listing actually on the market, not a configured constant. With
    // nothing listed there is no price to quote, which is exactly ERROR_NONE_FOR_SALE.
    if (uint64 lowestPrice = sWowTokenMgr->GetLowestListingPrice())
    {
        response.Result = TOKEN_RESULT_SUCCESS;
        response.Price = lowestPrice;

        // Retail always sends 14400 (4 hours) here - it is a constant in all 9 captured responses, not a
        // statistic derived from sales history. Match it. See COMMERCE_AUDIT C-19.
        response.ExpectedSecondsUntilSold = 14400;
    }
    else
        response.Result = TOKEN_RESULT_ERROR_NONE_FOR_SALE;

    SendPacket(response.Write());
}

void WorldSession::HandleCommerceTokenGetCount(WorldPackets::Token::CommerceTokenGetCount& commerceTokenGetCount)
{
    WorldPackets::Token::CommerceTokenGetCountResponse response;

    response.ClientToken = commerceTokenGetCount.ClientToken;
    response.Result = TOKEN_RESULT_SUCCESS;
    response.AuctionableTokenIDs = sWowTokenMgr->GetAuctionableTokens(GetAccountId());
    response.ConsumableTokenIDs = sWowTokenMgr->GetConsumableTokens(GetAccountId());

    SendPacket(response.Write());
}

void WorldSession::HandleConsumableTokenCanVeteranBuy(WorldPackets::Token::ConsumableTokenCanVeteranBuy& consumableTokenCanVeteranBuy)
{
    // "Veteran buy" is the retail flow where a lapsed-subscription account buys a token with gold to
    // reactivate game time (AccountReactivate.lua). TrinityCore has no subscriptions, so no account is
    // ever eligible - which is exactly what retail reports here as well: all 22 captured responses are
    // {Result = ERROR_DISABLED, RemainingGold = 0}. Answer that verbatim as the default ineligible reply.
    //
    // The success path an eligible account would receive is {Result = SUCCESS (or SUCCESS_NO), gold},
    // where the trailing u64 feeds C_WowTokenGlue.GetAccountRemainingGoldAmount(); it has no server-side
    // counterpart here (no veteran/subscription concept) so it is deliberately not produced. See
    // COMMERCE_AUDIT C-21 / WOW_TOKEN_RE_68275.md.
    WorldPackets::Token::ConsumableTokenCanVeteranBuyResponse response;
    response.ClientToken = consumableTokenCanVeteranBuy.ClientToken;
    response.Result = TOKEN_RESULT_ERROR_DISABLED;
    response.RemainingGoldAmount = 0;
    SendPacket(response.Write());
}

void WorldSession::HandleCanRedeemTokenForBalance(WorldPackets::Token::CanRedeemTokenForBalance& /*canRedeemTokenForBalance*/)
{
    // Deliberately unanswered. SMSG_CAN_REDEEM_TOKEN_FOR_BALANCE_RESPONSE appears in none of the nine
    // 12.0.7 captures while the request appears 19 times, i.e. retail itself stays silent - replying
    // would be less correct than not replying, and Battle.net balance has no server-side counterpart
    // here anyway. The request is still parsed so it is not reported as an unhandled opcode.
}

void WorldSession::SendCommerceTokenUpdate()
{
    WorldPackets::Token::CommerceTokenUpdate tokenUpdate;

    tokenUpdate.AuctionableTokenIDs = sWowTokenMgr->GetAuctionableTokens(GetAccountId());
    tokenUpdate.ConsumableTokenIDs = sWowTokenMgr->GetConsumableTokens(GetAccountId());

    SendPacket(tokenUpdate.Write());
}

void WorldSession::SendGenerateSsoToken(uint32 clientToken)
{
    WorldPackets::Token::GenerateSsoTokenResponse ssoToken;

    // Echo the ClientToken from the CMSG_BATTLE_PAY_OPEN_CHECKOUT that triggered this (1:1 answer).
    ssoToken.ClientToken = clientToken;
    ssoToken.Result = TOKEN_RESULT_SUCCESS;
    ssoToken.Issued = GameTime::GetGameTime();
    ssoToken.Expires = GameTime::GetGameTime() + WowTokenMgr::SSO_TOKEN_DURATION;
    ssoToken.Token = WowTokenMgr::GenerateSsoToken(GetBattlenetAccountId());

    SendPacket(ssoToken.Write());
}

// ---- SELL -------------------------------------------------------------------------------------------

void WorldSession::HandleAuctionableTokenSell(WorldPackets::Token::AuctionableTokenSell& /*auctionableTokenSell*/)
{
    // Step 1 of the sell handshake: the client asks to sell, we ask it to confirm. The confirm-required
    // body is empty (the payload-less Lua event TOKEN_SELL_CONFIRM_REQUIRED). The listing itself is made
    // when the confirm click arrives as CMSG_AUCTIONABLE_TOKEN_SELL_AT_MARKET_PRICE.
    SendPacket(WorldPackets::Token::AuctionableTokenSellConfirmRequired().Write());
}

void WorldSession::HandleAuctionableTokenSellAtMarketPrice(WorldPackets::Token::AuctionableTokenSellAtMarketPrice& auctionableTokenSellAtMarketPrice)
{
    WorldPackets::Token::AuctionableTokenSellAtMarketPriceResponse response;
    response.ClientToken = auctionableTokenSellAtMarketPrice.ClientToken;

    if (!IsTokenMarketEnabled())
        response.Result = TOKEN_RESULT_ERROR_DISABLED;
    else if (uint64 price = GetTokenMarketPrice())
    {
        // List the account's first auctionable token. The server picks the token rather than trusting a
        // client-supplied id, and the seller is paid (by mail) only when the listing actually sells.
        if (WowToken const* token = sWowTokenMgr->GetFirstToken(GetAccountId(), WOW_TOKEN_STATE_AUCTIONABLE))
        {
            if (sWowTokenMgr->ListToken(token->Id, GetAccountId(), GetPlayer()->GetGUID().GetCounter(), price))
                response.Result = TOKEN_RESULT_SUCCESS;
            else
                response.Result = TOKEN_RESULT_ERROR_OTHER;
        }
        else
            response.Result = TOKEN_RESULT_ERROR_NONE_FOR_SALE;
    }
    else
        response.Result = TOKEN_RESULT_ERROR_DISABLED;

    SendPacket(response.Write());

    if (response.Result == TOKEN_RESULT_SUCCESS)
        SendCommerceTokenUpdate();
}

// ---- BUY --------------------------------------------------------------------------------------------

void WorldSession::HandleConsumableTokenBuy(WorldPackets::Token::ConsumableTokenBuy& /*consumableTokenBuy*/)
{
    // Step 1 of the buy handshake: ask the client to confirm. Empty body (Lua event TOKEN_BUY_CONFIRM_REQUIRED).
    SendPacket(WorldPackets::Token::ConsumableTokenBuyChoiceRequired().Write());
}

void WorldSession::HandleConsumableTokenBuyAtMarketPrice(WorldPackets::Token::ConsumableTokenBuyAtMarketPrice& consumableTokenBuyAtMarketPrice)
{
    WorldPackets::Token::ConsumableTokenBuyAtMarketPriceResponse response;
    response.ClientToken = consumableTokenBuyAtMarketPrice.ClientToken;

    Player* player = GetPlayer();
    if (!IsTokenMarketEnabled())
        response.Result = TOKEN_RESULT_ERROR_DISABLED;
    else if (!sWowTokenMgr->GetListedTokenCount())
        response.Result = TOKEN_RESULT_ERROR_NONE_FOR_SALE;
    else if (!player || !player->HasEnoughMoney(sWowTokenMgr->GetLowestListingPrice()))
        response.Result = TOKEN_RESULT_ERROR_OTHER; // not enough gold
    else
    {
        uint64 price = 0;
        uint64 sellerCharGuid = 0;
        uint64 boughtTokenId = sWowTokenMgr->TakeCheapestListing(GetAccountId(), price, sellerCharGuid);
        if (!boughtTokenId)
        {
            response.Result = TOKEN_RESULT_ERROR_NONE_FOR_SALE;
        }
        else
        {
            // Debit the buyer and route the proceeds to the seller's character by in-game mail (works
            // whether or not the seller is online), exactly like a normal Auction House commodity sale.
            player->ModifyMoney(-int64(price));

            if (sellerCharGuid)
            {
                ObjectGuid sellerGuid = ObjectGuid::Create<HighGuid::Player>(sellerCharGuid);
                Player* seller = ObjectAccessor::FindConnectedPlayer(sellerGuid);

                CharacterDatabaseTransaction trans = CharacterDatabase.BeginTransaction();
                MailDraft("WoW Token Sold", "Your WoW Token has sold on the token market.")
                    .AddMoney(price)
                    .SendMailTo(trans, MailReceiver(seller, sellerCharGuid), MailSender(MAIL_AUCTION, 0, MAIL_STATIONERY_AUCTION),
                        MAIL_CHECK_MASK_COPIED, sWorld->getIntConfig(CONFIG_MAIL_DELIVERY_DELAY));
                CharacterDatabase.CommitTransaction(trans);

                if (seller)
                {
                    seller->GetSession()->SendPacket(WorldPackets::Token::AuctionableTokenAuctionSold().Write());
                    seller->GetSession()->SendCommerceTokenUpdate();
                }
            }

            response.Result = TOKEN_RESULT_SUCCESS;
        }
    }

    SendPacket(response.Write());

    if (response.Result == TOKEN_RESULT_SUCCESS)
        SendCommerceTokenUpdate();
}

// ---- REDEEM -----------------------------------------------------------------------------------------

void WorldSession::HandleConsumableTokenRedeem(WorldPackets::Token::ConsumableTokenRedeem& consumableTokenRedeem)
{
    // Step 1 of the redeem handshake: answer with the confirm-required carrying the choice type and the
    // amount that will be granted, so the client's confirmation dialog can display it.
    WorldPackets::Token::ConsumableTokenRedeemConfirmRequired confirm;
    confirm.ClientToken = consumableTokenRedeem.ClientToken;
    confirm.ChoiceType = consumableTokenRedeem.RedeemType;

    if (!IsTokenMarketEnabled())
        confirm.Result = TOKEN_RESULT_ERROR_DISABLED;
    else if (sWowTokenMgr->GetFirstToken(GetAccountId(), WOW_TOKEN_STATE_CONSUMABLE))
    {
        confirm.Result = TOKEN_RESULT_SUCCESS;
        confirm.Amount = GetTokenMarketPrice();
    }
    else
        confirm.Result = TOKEN_RESULT_ERROR_OTHER;

    SendPacket(confirm.Write());
}

void WorldSession::HandleConsumableTokenRedeemConfirmation(WorldPackets::Token::ConsumableTokenRedeemConfirmation& consumableTokenRedeemConfirmation)
{
    WorldPackets::Token::ConsumableTokenRedeemResponse response;
    response.ClientToken = consumableTokenRedeemConfirmation.ClientToken;
    response.ChoiceType = consumableTokenRedeemConfirmation.RedeemType;

    Player* player = GetPlayer();
    if (!IsTokenMarketEnabled())
        response.Result = TOKEN_RESULT_ERROR_DISABLED;
    else
    {
        WowToken const* token = sWowTokenMgr->GetFirstToken(GetAccountId(), WOW_TOKEN_STATE_CONSUMABLE);
        if (!token || !player)
            response.Result = TOKEN_RESULT_ERROR_OTHER;
        else if (!player->ModifyMoney(int64(GetTokenMarketPrice()))) // would exceed the gold cap
            response.Result = TOKEN_RESULT_ERROR_BALANCE_NEAR_CAP;
        else if (sWowTokenMgr->RedeemToken(token->Id, GetAccountId()))
            response.Result = TOKEN_RESULT_SUCCESS;
        else
        {
            // The token vanished between the two steps; undo the gold we just granted so nothing is minted.
            player->ModifyMoney(-int64(GetTokenMarketPrice()));
            response.Result = TOKEN_RESULT_ERROR_OTHER;
        }
    }

    SendPacket(response.Write());

    if (response.Result == TOKEN_RESULT_SUCCESS)
        SendCommerceTokenUpdate();
}
