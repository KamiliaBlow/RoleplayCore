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

#include "Shop2Service.h"
#include "BattlePayMgr.h"
#include "Config.h"
#include "CryptoRandom.h"
#include "IoContext.h"
#include "Log.h"
#include "Shop2SslContext.h"
#include "StringFormat.h"
#include "Util.h"
#include <rapidjson/document.h>
#include <rapidjson/stringbuffer.h>
#include <rapidjson/writer.h>
#include <algorithm>

using namespace std::string_view_literals;

// ---------------------------------------------------------------------------------------------
// shop2 web service.
//
// Contract source: c:\dumps\SHOP2_API_SCHEMA_68275.md (recovered from the 12.0.7.68275 client's own
// RapidJSON parsers in WowBattleNet/CatalogUtilities.cpp). The rules that will silently break the
// storefront if violated, and where they are honoured here:
//
//   * every route is POST + application/json, answered 200            -> RegisterRoute / SendJson
//   * "error" MUST be present at the root of every API response       -> WriteErrorNull on all routes
//     (an absent key aborts the client-side parse outright)
//   * storeId is the literal 4 in the client's request                -> we do not depend on it
//   * a product's "prices" array may hold AT MOST ONE element         -> WriteProduct emits 0 or 1
//     (two or more discards the ENTIRE catalog response)
//   * a non-empty "paginationToken" makes the client loop forever     -> never emitted
//   * a product survives only if !hasStoreSetting || hasRegionalSetting-> "storeSetting": null
//   * prices/balances are STRINGS, *TimeMs / *Id are non-negative numbers
// ---------------------------------------------------------------------------------------------

namespace Shop2
{
namespace
{
using JsonWriter = rapidjson::Writer<rapidjson::StringBuffer>;

constexpr std::string_view DEFAULT_PLACEMENT_ID = "WOW_SHOP_MAIN";
constexpr std::string_view DEFAULT_LOCALE = "enUS";

// BattlepayDisplayFlags bit that means "do not show a price line" (mirrors BattlePayMgr.cpp).
constexpr uint32 DISPLAY_FLAG_HIDDEN_PRICE = 8;

void WriteKeyString(JsonWriter& writer, std::string_view key, std::string_view value)
{
    writer.Key(key.data(), rapidjson::SizeType(key.length()));
    writer.String(value.data(), rapidjson::SizeType(value.length()));
}

void WriteKeyUint(JsonWriter& writer, std::string_view key, uint32 value)
{
    writer.Key(key.data(), rapidjson::SizeType(key.length()));
    writer.Uint(value);
}

void WriteKeyUint64(JsonWriter& writer, std::string_view key, uint64 value)
{
    writer.Key(key.data(), rapidjson::SizeType(key.length()));
    writer.Uint64(value);
}

void WriteKey(JsonWriter& writer, std::string_view key)
{
    writer.Key(key.data(), rapidjson::SizeType(key.length()));
}

// "error": null - mandatory at the root of EVERY API response (schema section 0).
void WriteErrorNull(JsonWriter& writer)
{
    WriteKey(writer, "error");
    writer.Null();
}

// A well-formed error envelope. The client's NON_RETRYABLE_FAILURE helper accepts it (all three
// members must be strings) and the caller then returns early instead of mis-parsing our payload.
void WriteErrorObject(JsonWriter& writer, std::string_view code, std::string_view message)
{
    WriteKey(writer, "error");
    writer.StartObject();
    WriteKeyString(writer, "category", "NON_RETRYABLE_FAILURE");
    WriteKeyString(writer, "code", code);
    WriteKeyString(writer, "message", message);
    writer.EndObject();
}

// Best-effort request parse; shop2 request bodies are small and always JSON except for /sso.
bool ParseJsonBody(std::string const& body, rapidjson::Document& doc)
{
    if (body.empty())
        return false;

    doc.Parse(body.c_str(), body.length());
    return !doc.HasParseError() && doc.IsObject();
}

std::string ReadStringMember(rapidjson::Document const& doc, char const* key, std::string_view fallback)
{
    auto itr = doc.FindMember(key);
    if (itr == doc.MemberEnd() || !itr->value.IsString())
        return std::string(fallback);

    return std::string(itr->value.GetString(), itr->value.GetStringLength());
}

// The client sends the placement ids it wants a page for; echoing them back is what makes the page
// it renders the page it asked for.
std::vector<std::string> ReadPlacementIds(rapidjson::Document const& doc)
{
    std::vector<std::string> ids;
    auto itr = doc.FindMember("placementIds");
    if (itr != doc.MemberEnd() && itr->value.IsArray())
    {
        for (rapidjson::Value const& v : itr->value.GetArray())
            if (v.IsString())
                ids.emplace_back(v.GetString(), v.GetStringLength());
    }

    if (ids.empty())
        ids.emplace_back(DEFAULT_PLACEMENT_ID);

    return ids;
}

// Maps our deliverable payload onto the client's ProductType enum (schema section 3, note 2).
// Only three of our deliverable types have an unambiguous counterpart; everything else falls back to
// "Services" (enum 9) rather than an unknown string (enum 15). `shop_product` has no product-type
// column yet - adding one is the clean fix and is listed as follow-up work.
std::string_view DeriveProductType(ShopProduct const& product)
{
    bool hasToken = false;
    bool hasGameTime = false;
    bool hasService = false;
    for (ShopDeliverable const& d : product.Deliverables)
    {
        switch (d.Type)
        {
            case 3: hasToken = true; break;
            case 4: hasGameTime = true; break;
            case 5: hasService = true; break;
            default: break;
        }
    }

    if (hasToken)
        return "WoW Token";
    if (hasGameTime)
        return "Access";
    if (hasService)
        return "Services";

    return "Services";
}

// Renders a ShopProduct's price into the two strings the shop2 wire wants.
//
// shop2 amounts are opaque strings, so we publish the product's price in the units of ITS OWN
// currency and spell that currency out in the localized text. There is no exchange rate between our
// currencies and the "USD" the client asks for; currencyCode below is the code we were configured
// with, purely so the field is populated.
void RenderPrice(ShopProduct const& product, std::string_view currencyCode, CatalogEntry& entry)
{
    if (product.DisplayFlags & DISPLAY_FLAG_HIDDEN_PRICE)
        return;                                         // admin asked for no price line -> prices: []

    if (product.HasDisplayPrice)
    {
        // DB override is the legacy wire's fixed-point value (/100000).
        uint64 const units = product.DisplayPrice / 100000;
        entry.HasPrice = true;
        entry.Amount = std::to_string(units);
        entry.LocalizedAmount = Trinity::StringFormat("{} {}", units, currencyCode);
        return;
    }

    switch (product.Currency)
    {
        case 0:                                         // free
            break;
        case 1:                                         // gold, stored in copper
        {
            uint64 const gold = product.Price / 10000;
            entry.HasPrice = true;
            entry.Amount = std::to_string(gold);
            entry.LocalizedAmount = Trinity::StringFormat("{} Gold", gold);
            break;
        }
        case 2:                                         // item token
            entry.HasPrice = true;
            entry.Amount = std::to_string(product.PriceItemCount);
            entry.LocalizedAmount = Trinity::StringFormat("{} x item {}", product.PriceItemCount, product.PriceItemId);
            break;
        case 3:                                         // custom currency (no name in the model)
        default:
            entry.HasPrice = true;
            entry.Amount = std::to_string(product.Price);
            entry.LocalizedAmount = entry.Amount;
            break;
    }
}

// One element of GetProductsByStoreId's "products". Every array/object the parser requires is
// emitted, empty where we have nothing to say - a missing one aborts the whole response.
void WriteProduct(JsonWriter& writer, ShopProduct const& product, CatalogEntry const& entry,
    std::string_view currencyCode, std::string_view locale)
{
    writer.StartObject();

    WriteKeyUint(writer, "productId", product.ProductID);
    WriteKeyString(writer, "name", product.Name);
    WriteKey(writer, "isServerValidationRequired");
    writer.Bool(true);                                  // BattlePayMgr::IsPurchasable is the authority
    WriteKeyUint(writer, "termTypeId", 0);
    WriteKeyUint(writer, "termDuration", 0);
    WriteKeyUint(writer, "serviceItemId", 0);

    WriteKey(writer, "licenses");
    writer.StartArray();
    writer.EndArray();

    // AT MOST ONE element - two or more and the client discards the entire catalog response.
    WriteKey(writer, "prices");
    writer.StartArray();
    if (entry.HasPrice)
    {
        writer.StartObject();
        WriteKeyString(writer, "currencyCode", currencyCode);
        WriteKeyUint(writer, "currencyTypeId", 1);
        WriteKeyString(writer, "currentPrice", entry.Amount);           // STRING, not a number
        WriteKeyString(writer, "originalPrice", entry.Amount);
        WriteKeyString(writer, "localizedCurrentPrice", entry.LocalizedAmount);
        WriteKeyString(writer, "localizedOriginalPrice", entry.LocalizedAmount);
        writer.EndObject();
    }
    writer.EndArray();

    WriteKey(writer, "localization");
    writer.StartObject();
    WriteKeyString(writer, "locale", locale);
    WriteKeyString(writer, "name", product.Name);
    WriteKeyString(writer, "description", product.Description);
    writer.EndObject();

    // null keeps the product through the client's silent filter (!hasStoreSetting || hasRegionalSetting).
    WriteKey(writer, "storeSetting");
    writer.Null();

    WriteKey(writer, "productBundles");
    writer.StartArray();
    writer.EndArray();

    WriteKey(writer, "productTypeAttributes");
    writer.StartArray();
    writer.StartObject();
    WriteKeyString(writer, "typeAttributeName", "ProductType");
    WriteKeyString(writer, "typeAttributeValue", DeriveProductType(product));
    writer.EndObject();
    writer.EndArray();

    WriteKey(writer, "productAvailabilities");
    if (!product.AvailableFrom && !product.AvailableUntil)
    {
        writer.Null();                                  // explicit IsNull short-circuit in the parser
    }
    else
    {
        writer.StartArray();
        writer.StartObject();
        if (product.AvailableFrom)
            WriteKeyUint64(writer, "startTimeMs", uint64(product.AvailableFrom) * 1000);
        if (product.AvailableUntil)
        {
            WriteKeyUint64(writer, "displayEndTimeMs", uint64(product.AvailableUntil) * 1000);
            WriteKeyUint64(writer, "purchaseEndTimeMs", uint64(product.AvailableUntil) * 1000);
        }
        writer.EndObject();
        writer.EndArray();
    }

    WriteKey(writer, "virtualCurrencyGrants");
    writer.StartArray();
    writer.EndArray();

    // The key must exist; null is accepted and skips the block. We ship no artwork host yet.
    WriteKey(writer, "storeContent");
    writer.Null();

    WriteKey(writer, "relatedProducts");
    writer.StartArray();
    writer.EndArray();

    WriteKey(writer, "productSubscriptions");
    writer.StartArray();
    writer.EndArray();

    writer.EndObject();
}
}

Shop2Service& Shop2Service::Instance()
{
    static Shop2Service instance;
    return instance;
}

bool Shop2Service::Start(Trinity::Asio::IoContext& ioContext)
{
    if (!sConfigMgr->GetBoolDefault("Shop.Shop2Enabled", false))
    {
        TC_LOG_INFO("server.http.shop2", "shop2: disabled (Shop.Shop2Enabled = 0), web service not started.");
        return true;
    }

    std::string const certificateFile = sConfigMgr->GetStringDefault("Shop.Shop2CertificatesFile", "");
    std::string const privateKeyFile = sConfigMgr->GetStringDefault("Shop.Shop2PrivateKeyFile", "");
    std::string const privateKeyPassword = sConfigMgr->GetStringDefault("Shop.Shop2PrivateKeyPassword", "");

    if (!SslContext::Initialize(certificateFile, privateKeyFile, privateKeyPassword))
    {
        TC_LOG_ERROR("server.http.shop2", "shop2: TLS material could not be loaded - the shop2 web service will NOT start. "
            "The client only ever connects with https://, so there is no plaintext fallback. "
            "Point Shop.Shop2CertificatesFile / Shop.Shop2PrivateKeyFile at a valid certificate and key, or set Shop.Shop2Enabled = 0.");
        return false;
    }

    _bindIp = sConfigMgr->GetStringDefault("Shop.Shop2BindIP", "0.0.0.0");
    _port = uint16(sConfigMgr->GetIntDefault("Shop.Shop2Port", 8082));
    _currencyCode = sConfigMgr->GetStringDefault("Shop.Shop2CurrencyCode", "USD");
    if (_currencyCode.empty())
        _currencyCode = "USD";

    int32 threadCount = sConfigMgr->GetIntDefault("Shop.Shop2NetworkThreads", 1);
    if (threadCount <= 0)
    {
        TC_LOG_ERROR("server.http.shop2", "shop2: Shop.Shop2NetworkThreads must be greater than 0, got {}.", threadCount);
        return false;
    }

    if (!HttpService::StartNetwork(ioContext, _bindIp, _port, threadCount))
    {
        TC_LOG_ERROR("server.http.shop2", "shop2: failed to bind the web service to {}:{}.", _bindIp, _port);
        return false;
    }

    _running = true;

    using Trinity::Net::Http::RequestHandlerFlag;

    // The dispatcher matches request targets by exact string, so each route is registered with and
    // without a trailing slash (the host URL may be configured either way).
    auto registerRoute = [this](std::string_view path, RequestHandlerResult (Shop2Service::*handler)(std::shared_ptr<Shop2HttpSession>, HttpRequestContext&) const)
    {
        auto invoke = [this, handler](std::shared_ptr<Shop2HttpSession> session, HttpRequestContext& context)
        {
            return (this->*handler)(std::move(session), context);
        };

        RegisterHandler(boost::beast::http::verb::post, path, invoke);
        RegisterHandler(boost::beast::http::verb::post, std::string(path).append("/"), invoke);
    };

    registerRoute("/sso"sv, &Shop2Service::HandleSso);
    registerRoute("/StorefrontService/v1/GetCurrentPages"sv, &Shop2Service::HandleGetCurrentPages);
    registerRoute("/ProductCatalogService/v1/GetProductsByStoreId"sv, &Shop2Service::HandleGetProductsByStoreId);
    registerRoute("/PurchaseService/v1/GetAccountDefaultCurrency"sv, &Shop2Service::HandleGetAccountDefaultCurrency);
    registerRoute("/VirtualCurrencyLedgerService/v1/GetBalance"sv, &Shop2Service::HandleGetBalance);
    registerRoute("/PurchaseService/v1/QuoteDynamicBundle"sv, &Shop2Service::HandleQuoteDynamicBundle);
    registerRoute("/PublicOrderService/v1/GetOrderStatusByExternalTransactionId"sv, &Shop2Service::HandleGetOrderStatus);

    RebuildCatalog();

    TC_LOG_INFO("server.http.shop2", "shop2: web service listening on https://{}:{} ({} thread(s)), currency '{}'.",
        _bindIp, _port, threadCount, _currencyCode);
    for (std::string_view route : { "/sso"sv, "/StorefrontService/v1/GetCurrentPages"sv,
        "/ProductCatalogService/v1/GetProductsByStoreId"sv, "/PurchaseService/v1/GetAccountDefaultCurrency"sv,
        "/VirtualCurrencyLedgerService/v1/GetBalance"sv, "/PurchaseService/v1/QuoteDynamicBundle"sv,
        "/PublicOrderService/v1/GetOrderStatusByExternalTransactionId"sv })
        TC_LOG_INFO("server.http.shop2", "shop2:   POST {}", route);

    std::string const advertisedRequests = sConfigMgr->GetStringDefault("Shop.Shop2HostUrlRequests", "");
    std::string const advertisedAuth = sConfigMgr->GetStringDefault("Shop.Shop2HostUrlAuth", "");
    if (advertisedRequests.empty() || advertisedAuth.empty())
        TC_LOG_ERROR("server.http.shop2", "shop2: the service is running but Shop.Shop2HostUrlRequests / Shop.Shop2HostUrlAuth are not both set - "
            "the client is never told where to find it and will keep using its built-in Blizzard defaults.");
    else
        TC_LOG_INFO("server.http.shop2", "shop2: advertised to clients as requests='{}' auth='{}'.", advertisedRequests, advertisedAuth);

    return true;
}

void Shop2Service::Stop()
{
    if (!_running)
        return;

    _running = false;
    HttpService::StopNetwork();

    TC_LOG_INFO("server.http.shop2", "shop2: web service stopped.");
}

std::shared_ptr<CatalogSnapshot const> Shop2Service::GetSnapshot() const
{
    std::scoped_lock lock(_snapshotMutex);
    return _snapshot;
}

void Shop2Service::BuildSnapshot(std::vector<ShopProduct const*> const& products, time_t now)
{
    // Everything the HTTP workers will read is rendered here into an immutable snapshot, so
    // `.reload shop_catalog` can swap the catalog without ever racing a live request.
    auto snapshot = std::make_shared<CatalogSnapshot>();

    rapidjson::StringBuffer buffer;
    JsonWriter writer(buffer);

    writer.StartObject();
    WriteErrorNull(writer);

    WriteKey(writer, "metaData");
    writer.StartObject();
    WriteKeyUint64(writer, "serverTimeMs", uint64(now) * 1000);
    writer.EndObject();

    WriteKey(writer, "products");
    writer.StartArray();
    for (ShopProduct const* product : products)
    {
        CatalogEntry entry;
        entry.ProductId = product->ProductID;
        entry.Name = product->Name;
        RenderPrice(*product, _currencyCode, entry);

        WriteProduct(writer, *product, entry, _currencyCode, DEFAULT_LOCALE);

        snapshot->Entries.push_back(std::move(entry));
    }
    writer.EndArray();

    // No "paginationToken": a non-empty one makes the client re-request forever.
    writer.EndObject();

    snapshot->ProductsResponse.assign(buffer.GetString(), buffer.GetSize());

    {
        std::scoped_lock lock(_snapshotMutex);
        _snapshot = std::move(snapshot);
    }

    TC_LOG_INFO("server.http.shop2", "shop2: catalog snapshot rebuilt - {} product(s), {} bytes.",
        products.size(), GetSnapshot()->ProductsResponse.size());
}

namespace
{
Trinity::Net::Http::RequestHandlerResult SendJson(Trinity::Net::Http::RequestContext& context, std::string body)
{
    context.response.result(boost::beast::http::status::ok);
    context.response.set(boost::beast::http::field::content_type, "application/json");
    context.response.body() = std::move(body);
    return Trinity::Net::Http::RequestHandlerResult::Handled;
}
}

// 1. POST {shop2HostUrlAuth}/sso
// Request is application/x-www-form-urlencoded; the client reads exactly one member of the response
// ("access_token") and no expiry at all. Any non-empty string unblocks the queued API calls.
Shop2Service::RequestHandlerResult Shop2Service::HandleSso(std::shared_ptr<Shop2HttpSession> session, HttpRequestContext& context) const
{
    std::string const token = ByteArrayToHexStr(Trinity::Crypto::GetRandomBytes<32>());

    TC_LOG_DEBUG("server.http.shop2", "{} shop2 /sso -> issuing bearer token", session->GetClientInfo());

    rapidjson::StringBuffer buffer;
    JsonWriter writer(buffer);
    writer.StartObject();
    WriteKeyString(writer, "access_token", token);
    writer.EndObject();

    return SendJson(context, std::string(buffer.GetString(), buffer.GetSize()));
}

// 2. POST {shop2HostUrlRequests}/StorefrontService/v1/GetCurrentPages
// One page per requested placement, one section, one product collection whose items point at our
// catalog products. Every array/object the parser gates on is present; any single failure discards
// the whole response client-side.
Shop2Service::RequestHandlerResult Shop2Service::HandleGetCurrentPages(std::shared_ptr<Shop2HttpSession> /*session*/, HttpRequestContext& context) const
{
    rapidjson::Document request;
    std::vector<std::string> placementIds;
    std::string locale{ DEFAULT_LOCALE };
    if (ParseJsonBody(context.request.body(), request))
    {
        placementIds = ReadPlacementIds(request);
        locale = ReadStringMember(request, "locale", DEFAULT_LOCALE);
    }
    else
        placementIds.emplace_back(DEFAULT_PLACEMENT_ID);

    std::shared_ptr<CatalogSnapshot const> snapshot = GetSnapshot();

    rapidjson::StringBuffer buffer;
    JsonWriter writer(buffer);

    writer.StartObject();
    WriteErrorNull(writer);

    WriteKey(writer, "placements");
    writer.StartArray();
    for (std::string const& placementId : placementIds)
    {
        writer.StartObject();
        WriteKeyString(writer, "placementId", placementId);
        WriteKeyUint64(writer, "nextScheduledPageDisplayTimeMs", 0);

        WriteKey(writer, "page");
        writer.StartObject();
        WriteKeyString(writer, "pageId", "1");

        WriteKey(writer, "sections");
        writer.StartArray();
        writer.StartObject();
        WriteKeyString(writer, "sectionId", "1");
        WriteKeyUint(writer, "orderInPage", 0);

        WriteKey(writer, "attributes");
        writer.StartArray();
        writer.StartObject();
        WriteKeyString(writer, "sectionAttributeKey", "layout");        // keys must be unique
        WriteKeyString(writer, "sectionAttributeValue", "grid");
        writer.EndObject();
        writer.EndArray();

        WriteKey(writer, "localization");
        writer.StartObject();
        WriteKeyString(writer, "locale", locale);
        WriteKeyString(writer, "name", "Featured");
        WriteKeyString(writer, "description", "");
        writer.EndObject();

        WriteKey(writer, "productCollections");
        writer.StartArray();
        writer.StartObject();
        WriteKeyString(writer, "productCollectionId", "1");
        WriteKeyUint(writer, "orderInSection", 0);
        WriteKeyUint(writer, "personalizationStatus", 0);

        WriteKey(writer, "items");
        writer.StartArray();
        if (snapshot)
        {
            uint32 order = 0;
            for (CatalogEntry const& entry : snapshot->Entries)
            {
                writer.StartObject();
                WriteKeyUint(writer, "productCollectionTypeId", 1);
                WriteKeyUint(writer, "productCollectionItemTypeId", 1);
                WriteKeyUint(writer, "productCollectionItemValue", entry.ProductId);
                WriteKeyUint(writer, "orderInProductCollection", order++);
                WriteKeyUint(writer, "personalizedPercentageDiscount", 0);
                WriteKeyUint64(writer, "displayEndTimeMs", 0);
                WriteKeyString(writer, "productClaimsToken", "");
                writer.EndObject();
            }
        }
        writer.EndArray();

        writer.EndObject();     // product collection
        writer.EndArray();      // productCollections

        writer.EndObject();     // section
        writer.EndArray();      // sections

        writer.EndObject();     // page
        writer.EndObject();     // placement
    }
    writer.EndArray();
    writer.EndObject();

    return SendJson(context, std::string(buffer.GetString(), buffer.GetSize()));
}

// 3. POST {shop2HostUrlRequests}/ProductCatalogService/v1/GetProductsByStoreId
// Pre-rendered by RebuildCatalog(); a request for a later page can never happen because we never
// hand out a paginationToken.
Shop2Service::RequestHandlerResult Shop2Service::HandleGetProductsByStoreId(std::shared_ptr<Shop2HttpSession> /*session*/, HttpRequestContext& context) const
{
    if (std::shared_ptr<CatalogSnapshot const> snapshot = GetSnapshot())
        return SendJson(context, snapshot->ProductsResponse);

    // Should not happen (Start() builds a snapshot), but never answer with a body the client cannot parse.
    rapidjson::StringBuffer buffer;
    JsonWriter writer(buffer);
    writer.StartObject();
    WriteErrorObject(writer, "CATALOG_UNAVAILABLE", "The shop catalog has not been built yet.");
    writer.EndObject();
    return SendJson(context, std::string(buffer.GetString(), buffer.GetSize()));
}

// 4. POST {shop2HostUrlRequests}/PurchaseService/v1/GetAccountDefaultCurrency
// This route has its own stricter check: "error" must exist AND be null (an error object is not
// tolerated here), and "currencyAlphaCode" must be a string.
Shop2Service::RequestHandlerResult Shop2Service::HandleGetAccountDefaultCurrency(std::shared_ptr<Shop2HttpSession> /*session*/, HttpRequestContext& context) const
{
    rapidjson::StringBuffer buffer;
    JsonWriter writer(buffer);
    writer.StartObject();
    WriteErrorNull(writer);
    WriteKeyString(writer, "currencyAlphaCode", _currencyCode);
    writer.EndObject();

    return SendJson(context, std::string(buffer.GetString(), buffer.GetSize()));
}

// 5. POST {shop2HostUrlRequests}/VirtualCurrencyLedgerService/v1/GetBalance
// We run no virtual-currency ledger, so the balance is a well-formed zero. Note that
// "balance.balance" is a STRING and "softCapped" sits at the ROOT, not inside "balance".
Shop2Service::RequestHandlerResult Shop2Service::HandleGetBalance(std::shared_ptr<Shop2HttpSession> /*session*/, HttpRequestContext& context) const
{
    rapidjson::StringBuffer buffer;
    JsonWriter writer(buffer);
    writer.StartObject();
    WriteErrorNull(writer);
    WriteKey(writer, "balance");
    writer.StartObject();
    WriteKeyString(writer, "balance", "0");
    writer.EndObject();
    WriteKey(writer, "softCapped");
    writer.Bool(false);
    writer.EndObject();

    return SendJson(context, std::string(buffer.GetString(), buffer.GetSize()));
}

// 6. POST {shop2HostUrlRequests}/PurchaseService/v1/QuoteDynamicBundle
// PARTIALLY RECOVERED. The four result scalars and the presence of the components/discounts arrays
// are verified; the per-element field types inside those arrays are NOT, so we emit them empty and
// invent nothing. The request body is logged at INFO so the remaining shape can be completed from a
// real client request.
Shop2Service::RequestHandlerResult Shop2Service::HandleQuoteDynamicBundle(std::shared_ptr<Shop2HttpSession> session, HttpRequestContext& context) const
{
    TC_LOG_INFO("server.http.shop2", "{} shop2 QuoteDynamicBundle request body: {}", session->GetClientInfo(), context.request.body());

    uint32 productId = 0;
    rapidjson::Document request;
    if (ParseJsonBody(context.request.body(), request))
    {
        auto itr = request.FindMember("productId");
        if (itr != request.MemberEnd() && itr->value.IsUint64())
            productId = uint32(itr->value.GetUint64());
    }

    CatalogEntry const* entry = nullptr;
    if (std::shared_ptr<CatalogSnapshot const> snapshot = GetSnapshot())
    {
        auto found = std::ranges::find_if(snapshot->Entries, [productId](CatalogEntry const& e) { return e.ProductId == productId; });
        if (found != snapshot->Entries.end())
            entry = &*found;
    }

    rapidjson::StringBuffer buffer;
    JsonWriter writer(buffer);
    writer.StartObject();

    if (!entry || !entry->HasPrice)
    {
        // A well-formed error envelope is better than a "result" we would have to make up.
        WriteErrorObject(writer, "PRODUCT_NOT_QUOTABLE",
            Trinity::StringFormat("No priced shop product with id {} is currently on sale.", productId));
        writer.EndObject();
        return SendJson(context, std::string(buffer.GetString(), buffer.GetSize()));
    }

    WriteErrorNull(writer);
    WriteKey(writer, "result");
    writer.StartObject();
    WriteKeyString(writer, "amount", entry->Amount);
    WriteKeyString(writer, "localizedAmount", entry->LocalizedAmount);
    WriteKeyString(writer, "currencyCode", _currencyCode);
    WriteKeyString(writer, "quoteToken", ByteArrayToHexStr(Trinity::Crypto::GetRandomBytes<16>()));
    WriteKey(writer, "components");
    writer.StartArray();
    writer.EndArray();
    WriteKey(writer, "discounts");
    writer.StartArray();
    writer.EndArray();
    writer.EndObject();
    writer.EndObject();

    return SendJson(context, std::string(buffer.GetString(), buffer.GetSize()));
}

// 7. POST {shop2HostUrlRequests}/PublicOrderService/v1/GetOrderStatusByExternalTransactionId
// "simpleStatus" and "productId" are both optional and their enum values are unknown, so we send
// neither - only the mandatory envelope. We have no order ledger behind this route yet.
Shop2Service::RequestHandlerResult Shop2Service::HandleGetOrderStatus(std::shared_ptr<Shop2HttpSession> session, HttpRequestContext& context) const
{
    TC_LOG_INFO("server.http.shop2", "{} shop2 GetOrderStatusByExternalTransactionId request body: {}",
        session->GetClientInfo(), context.request.body());

    rapidjson::StringBuffer buffer;
    JsonWriter writer(buffer);
    writer.StartObject();
    WriteErrorNull(writer);
    writer.EndObject();

    return SendJson(context, std::string(buffer.GetString(), buffer.GetSize()));
}
}
