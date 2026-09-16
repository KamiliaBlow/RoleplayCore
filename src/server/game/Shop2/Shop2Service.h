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

#ifndef TRINITYCORE_SHOP2_SERVICE_H
#define TRINITYCORE_SHOP2_SERVICE_H

#include "HttpService.h"
#include "Shop2HttpSession.h"
#include <ctime>
#include <mutex>
#include <string>
#include <vector>

struct ShopProduct;

namespace Shop2
{
// One product as the shop2 storefront needs to see it, rendered once on the world thread.
struct CatalogEntry
{
    uint32 ProductId = 0;
    std::string Name;
    bool HasPrice = false;
    std::string Amount;             // wire "currentPrice" - a STRING per the schema
    std::string LocalizedAmount;    // wire "localizedCurrentPrice" - display text
};

// Immutable snapshot of the catalog. Built on the world thread by RebuildCatalog(), consumed by the
// HTTP worker threads through a shared_ptr, so `.reload shop_catalog` can swap the catalog under
// live requests without a data race on BattlePayMgr's product map.
struct CatalogSnapshot
{
    std::string ProductsResponse;       // complete GetProductsByStoreId body, ready to send
    std::vector<CatalogEntry> Entries;  // display order; drives GetCurrentPages and QuoteDynamicBundle
};

// The "shop2" web service the MODERN in-game Shop talks to.
//
// The 12.0.7 client reaches its modern store purely over HTTPS; the endpoints come from the
// shop2HostUrlRequests / shop2HostUrlAuth mirror vars we push in SMSG_MIRROR_VARS (see
// WorldSession::SendMirrorVars), which is why pointing it at us needs no hosts file and no
// hostname impersonation. The request/response contract implemented here was reverse engineered
// from the client's own RapidJSON parsers and is documented in c:\dumps\SHOP2_API_SCHEMA_68275.md;
// every rule that document marks load-bearing is enforced in Shop2Service.cpp.
class TC_GAME_API Shop2Service final : public Trinity::Net::Http::HttpService<Shop2HttpSession>
{
public:
    using RequestHandlerResult = Trinity::Net::Http::RequestHandlerResult;
    using HttpRequestContext = Trinity::Net::Http::RequestContext;

    Shop2Service() : HttpService("shop2") { }

    static Shop2Service& Instance();

    // Reads the Shop.Shop2* config, loads TLS material and binds the listener. Returns false only on
    // a real failure - a disabled service returns true without starting anything.
    bool Start(Trinity::Asio::IoContext& ioContext);
    void Stop();

    bool IsRunning() const { return _running; }

    // Re-renders the catalog snapshot from BattlePayMgr. Must be called on the world thread
    // (BattlePayMgr::LoadCatalog does this for us); a no-op when the service is not running.
    // Lives in Shop2Catalog.cpp - it is the only part of this service that reads world state.
    void RebuildCatalog();

    // Renders a snapshot from an already-filtered, already-ordered product list. Kept free of any
    // world dependency so the wire rendering can be exercised on its own.
    void BuildSnapshot(std::vector<ShopProduct const*> const& productsInDisplayOrder, time_t now);

private:
    std::shared_ptr<CatalogSnapshot const> GetSnapshot() const;

    // Routes. Every one of them answers 200 + application/json with "error" present at the root -
    // an absent "error" key makes the client abort the parse outright.
    RequestHandlerResult HandleSso(std::shared_ptr<Shop2HttpSession> session, HttpRequestContext& context) const;
    RequestHandlerResult HandleGetCurrentPages(std::shared_ptr<Shop2HttpSession> session, HttpRequestContext& context) const;
    RequestHandlerResult HandleGetProductsByStoreId(std::shared_ptr<Shop2HttpSession> session, HttpRequestContext& context) const;
    RequestHandlerResult HandleGetAccountDefaultCurrency(std::shared_ptr<Shop2HttpSession> session, HttpRequestContext& context) const;
    RequestHandlerResult HandleGetBalance(std::shared_ptr<Shop2HttpSession> session, HttpRequestContext& context) const;
    RequestHandlerResult HandleQuoteDynamicBundle(std::shared_ptr<Shop2HttpSession> session, HttpRequestContext& context) const;
    RequestHandlerResult HandleGetOrderStatus(std::shared_ptr<Shop2HttpSession> session, HttpRequestContext& context) const;

    bool _running = false;
    uint16 _port = 0;
    std::string _bindIp;
    std::string _currencyCode;          // wire currencyAlphaCode / prices[].currencyCode

    mutable std::mutex _snapshotMutex;
    std::shared_ptr<CatalogSnapshot const> _snapshot;
};
}

#define sShop2Service Shop2::Shop2Service::Instance()

#endif // TRINITYCORE_SHOP2_SERVICE_H
