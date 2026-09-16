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

#include "WowTokenMgr.h"
#include "CryptoRandom.h"
#include "DatabaseEnv.h"
#include "GameTime.h"
#include "Log.h"
#include "Realm.h"
#include "RealmList.h"
#include "Timer.h"
#include "Util.h"
#include <algorithm>
#include <array>
#include <cctype>

WowTokenMgr* WowTokenMgr::instance()
{
    static WowTokenMgr instance;
    return &instance;
}

void WowTokenMgr::Load()
{
    uint32 oldMSTime = getMSTime();

    _tokens.clear();
    _maxTokenId = 0;

    //                                                     0   1        2      3      4           5
    QueryResult result = LoginDatabase.Query("SELECT id, account, state, price, createTime, seller_guid FROM account_wow_token");
    if (!result)
    {
        TC_LOG_INFO("server.loading", ">> Loaded 0 WoW tokens. The table is empty.");
        return;
    }

    do
    {
        Field* fields = result->Fetch();

        WowToken token;
        token.Id           = fields[0].GetUInt64();
        token.OwnerAccount = fields[1].GetUInt32();
        token.Price        = fields[3].GetUInt64();
        token.CreateTime   = fields[4].GetInt64();
        token.SellerGuid   = fields[5].GetUInt64();

        uint8 state = fields[2].GetUInt8();
        if (state > WOW_TOKEN_STATE_LISTED)
        {
            TC_LOG_ERROR("sql.sql", "Token {} in account_wow_token has invalid state {}, skipped.", token.Id, state);
            continue;
        }

        token.State = WowTokenState(state);

        _maxTokenId = std::max(_maxTokenId, token.Id);
        _tokens[token.Id] = token;
    }
    while (result->NextRow());

    TC_LOG_INFO("server.loading", ">> Loaded {} WoW tokens in {} ms", _tokens.size(), GetMSTimeDiffToNow(oldMSTime));
}

std::vector<uint64> WowTokenMgr::GetAuctionableTokens(uint32 accountId) const
{
    std::vector<uint64> tokenIds;
    for (auto const& [tokenId, token] : _tokens)
        if (token.OwnerAccount == accountId && token.State == WOW_TOKEN_STATE_AUCTIONABLE)
            tokenIds.push_back(tokenId);

    std::sort(tokenIds.begin(), tokenIds.end());
    return tokenIds;
}

std::vector<uint64> WowTokenMgr::GetConsumableTokens(uint32 accountId) const
{
    std::vector<uint64> tokenIds;
    for (auto const& [tokenId, token] : _tokens)
        if (token.OwnerAccount == accountId && token.State == WOW_TOKEN_STATE_CONSUMABLE)
            tokenIds.push_back(tokenId);

    std::sort(tokenIds.begin(), tokenIds.end());
    return tokenIds;
}

uint32 WowTokenMgr::GetListedTokenCount() const
{
    uint32 count = 0;
    for (auto const& [tokenId, token] : _tokens)
        if (token.State == WOW_TOKEN_STATE_LISTED)
            ++count;

    return count;
}

uint64 WowTokenMgr::GetLowestListingPrice() const
{
    uint64 lowest = 0;
    for (auto const& [tokenId, token] : _tokens)
        if (token.State == WOW_TOKEN_STATE_LISTED && (!lowest || token.Price < lowest))
            lowest = token.Price;

    return lowest;
}

WowToken const* WowTokenMgr::GetToken(uint64 tokenId) const
{
    auto itr = _tokens.find(tokenId);
    return itr != _tokens.end() ? &itr->second : nullptr;
}

WowToken const* WowTokenMgr::GetFirstToken(uint32 accountId, WowTokenState state) const
{
    WowToken const* found = nullptr;
    for (auto const& [tokenId, token] : _tokens)
        if (token.OwnerAccount == accountId && token.State == state)
            if (!found || token.Id < found->Id)
                found = &token;

    return found;
}

uint64 WowTokenMgr::CreateToken(uint32 accountId, WowTokenState state)
{
    WowToken token;
    token.Id           = ++_maxTokenId;
    token.OwnerAccount = accountId;
    token.State        = state;
    token.CreateTime   = GameTime::GetGameTime();

    _tokens[token.Id] = token;

    LoginDatabasePreparedStatement* stmt = LoginDatabase.GetPreparedStatement(LOGIN_INS_ACCOUNT_WOW_TOKEN);
    stmt->setUInt64(0, token.Id);
    stmt->setUInt32(1, token.OwnerAccount);
    stmt->setUInt8(2, uint8(token.State));
    stmt->setUInt64(3, token.Price);
    stmt->setInt64(4, token.CreateTime);
    stmt->setUInt64(5, token.SellerGuid);
    LoginDatabase.Execute(stmt);

    return token.Id;
}

bool WowTokenMgr::SetTokenState(uint64 tokenId, WowTokenState state, uint32 ownerAccount, uint64 price /*= 0*/)
{
    auto itr = _tokens.find(tokenId);
    if (itr == _tokens.end())
        return false;

    WowToken& token = itr->second;
    token.State        = state;
    token.OwnerAccount = ownerAccount;
    token.Price        = price;

    Persist(token);
    return true;
}

void WowTokenMgr::Persist(WowToken const& token)
{
    LoginDatabasePreparedStatement* stmt = LoginDatabase.GetPreparedStatement(LOGIN_UPD_ACCOUNT_WOW_TOKEN);
    stmt->setUInt32(0, token.OwnerAccount);
    stmt->setUInt8(1, uint8(token.State));
    stmt->setUInt64(2, token.Price);
    stmt->setUInt64(3, token.SellerGuid);
    stmt->setUInt64(4, token.Id);
    LoginDatabase.Execute(stmt);
}

bool WowTokenMgr::ListToken(uint64 tokenId, uint32 sellerAccount, uint64 sellerCharGuid, uint64 price)
{
    auto itr = _tokens.find(tokenId);
    if (itr == _tokens.end())
        return false;

    WowToken& token = itr->second;
    if (token.OwnerAccount != sellerAccount || token.State != WOW_TOKEN_STATE_AUCTIONABLE)
        return false;

    token.State      = WOW_TOKEN_STATE_LISTED;
    token.Price      = price;
    token.SellerGuid = sellerCharGuid;
    // OwnerAccount stays the seller so proceeds can be routed when the listing sells.

    Persist(token);
    return true;
}

uint64 WowTokenMgr::TakeCheapestListing(uint32 buyerAccount, uint64& outPrice, uint64& outSellerCharGuid)
{
    WowToken* cheapest = nullptr;
    for (auto& [tokenId, token] : _tokens)
        if (token.State == WOW_TOKEN_STATE_LISTED)
            if (!cheapest || token.Price < cheapest->Price || (token.Price == cheapest->Price && token.Id < cheapest->Id))
                cheapest = &token;

    if (!cheapest)
        return 0;

    outPrice          = cheapest->Price;
    outSellerCharGuid = cheapest->SellerGuid;

    cheapest->State        = WOW_TOKEN_STATE_CONSUMABLE;
    cheapest->OwnerAccount = buyerAccount;
    cheapest->Price        = 0;
    cheapest->SellerGuid   = 0;

    Persist(*cheapest);
    return cheapest->Id;
}

bool WowTokenMgr::RedeemToken(uint64 tokenId, uint32 accountId)
{
    auto itr = _tokens.find(tokenId);
    if (itr == _tokens.end())
        return false;

    if (itr->second.OwnerAccount != accountId || itr->second.State != WOW_TOKEN_STATE_CONSUMABLE)
        return false;

    _tokens.erase(itr);

    LoginDatabasePreparedStatement* stmt = LoginDatabase.GetPreparedStatement(LOGIN_DEL_ACCOUNT_WOW_TOKEN);
    stmt->setUInt64(0, tokenId);
    LoginDatabase.Execute(stmt);

    return true;
}

std::string WowTokenMgr::GenerateSsoToken(uint32 bnetAccountId)
{
    // Retail's token is "<REGION>-<32 lowercase hex>-<battlenet account id>" (observed body length 45,
    // e.g. "US-" + 32 hex + "-" + a 9-digit account id). The hex half is 16 random bytes; the prefix is
    // the Battle.net region code of this realm; the account-id suffix is stable per account (C-20).
    std::array<uint8, 16> randomBytes = Trinity::Crypto::GetRandomBytes<16>();

    std::string region;
    switch (sRealmList->GetCurrentRealmId().Region)
    {
        case 1:  region = "US"; break;
        case 2:  region = "KR"; break;
        case 3:  region = "EU"; break;
        case 4:  region = "TW"; break;
        case 5:  region = "CN"; break;
        default: region = "US"; break;
    }

    std::string token = ByteArrayToHexStr(randomBytes);
    std::transform(token.begin(), token.end(), token.begin(), [](char c) { return char(std::tolower(c)); });

    return region + "-" + token + "-" + std::to_string(bnetAccountId);
}
