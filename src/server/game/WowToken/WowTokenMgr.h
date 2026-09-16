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

#ifndef TRINITYCORE_WOW_TOKEN_MGR_H
#define TRINITYCORE_WOW_TOKEN_MGR_H

#include "Define.h"
#include <ctime>
#include <string>
#include <unordered_map>
#include <vector>

enum WowTokenState : uint8
{
    // Owned by an account and sellable on the token market.
    WOW_TOKEN_STATE_AUCTIONABLE = 0,
    // Owned by an account and redeemable.
    WOW_TOKEN_STATE_CONSUMABLE  = 1,
    // Currently listed on the token market, so owned by nobody for display purposes.
    WOW_TOKEN_STATE_LISTED      = 2
};

struct WowToken
{
    uint64 Id            = 0;
    uint32 OwnerAccount  = 0;   // owning account; the seller's account while WOW_TOKEN_STATE_LISTED
    WowTokenState State  = WOW_TOKEN_STATE_AUCTIONABLE;
    uint64 Price         = 0;   // copper; only meaningful while WOW_TOKEN_STATE_LISTED
    time_t CreateTime    = 0;
    uint64 SellerGuid    = 0;   // character lowguid that listed the token, so sale proceeds can be mailed
};

// Account-level WoW Token holdings and the token market.
//
// Holdings live in the auth database because the client asks for them at character select, before any
// Player exists (CMSG_COMMERCE_TOKEN_GET_COUNT is answered at STATUS_AUTHED - see WOW_TOKEN_RE_68275.md
// for the sniff evidence). Everything the handlers put on the wire is read back out of this state; no
// count, price or eligibility answer is synthesised.
class TC_GAME_API WowTokenMgr
{
public:
    static WowTokenMgr* instance();

    void Load();

    // Token ids held by an account, split the way the client's two parallel uint64 lists expect.
    std::vector<uint64> GetAuctionableTokens(uint32 accountId) const;
    std::vector<uint64> GetConsumableTokens(uint32 accountId) const;

    // Market state.
    uint32 GetListedTokenCount() const;
    uint64 GetLowestListingPrice() const;

    WowToken const* GetToken(uint64 tokenId) const;

    // First token an account holds in the given state, or nullptr. Used by the market handlers which
    // operate on the account's holdings rather than trusting a client-supplied token id.
    WowToken const* GetFirstToken(uint32 accountId, WowTokenState state) const;

    // Creates a token owned by an account and persists it. Returns the new token id.
    uint64 CreateToken(uint32 accountId, WowTokenState state);

    // Moves a token between states (and optionally owners), persisting the change.
    bool SetTokenState(uint64 tokenId, WowTokenState state, uint32 ownerAccount, uint64 price = 0);

    // ---- Token market (gated by WowToken.Market.Enabled) --------------------------------------------
    // Lists an account's auctionable token on the market at 'price'. 'sellerCharGuid' is the character
    // lowguid that will receive the gold when the listing sells. Returns false if the token is not an
    // auctionable holding of that account.
    bool ListToken(uint64 tokenId, uint32 sellerAccount, uint64 sellerCharGuid, uint64 price);

    // Removes the cheapest market listing and hands it to 'buyerAccount' as a consumable token. On success
    // returns the token id and reports the listing's price and the seller's character guid; returns 0 when
    // nothing is listed.
    uint64 TakeCheapestListing(uint32 buyerAccount, uint64& outPrice, uint64& outSellerCharGuid);

    // Consumes (deletes) a redeemed token owned by an account. Returns false if the token is not a
    // consumable holding of that account.
    bool RedeemToken(uint64 tokenId, uint32 accountId);

    // Generates a fresh single sign-on token, of the form "<REGION>-<32 lowercase hex>-<bnet account id>"
    // (observed retail body length 45, e.g. "US-" + 32 hex + "-" + a 9-digit account id). The hex half is
    // 16 random bytes; the account-id suffix is stable per account. See COMMERCE_AUDIT C-20.
    static std::string GenerateSsoToken(uint32 bnetAccountId);

    // Retail issues the SSO token with a four hour lifetime (observed expiry - issued == 14400s).
    static constexpr uint32 SSO_TOKEN_DURATION = 4 * 60 * 60;

private:
    WowTokenMgr() = default;
    ~WowTokenMgr() = default;
    WowTokenMgr(WowTokenMgr const&) = delete;
    WowTokenMgr& operator=(WowTokenMgr const&) = delete;

    // Writes the token's mutable columns (account, state, price, seller_guid) back to the auth DB.
    void Persist(WowToken const& token);

    std::unordered_map<uint64, WowToken> _tokens;
    uint64 _maxTokenId = 0;
};

#define sWowTokenMgr WowTokenMgr::instance()

#endif // TRINITYCORE_WOW_TOKEN_MGR_H
