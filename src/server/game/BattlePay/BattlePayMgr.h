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

#ifndef TRINITYCORE_BATTLE_PAY_MGR_H
#define TRINITYCORE_BATTLE_PAY_MGR_H

#include "Define.h"
#include "DatabaseEnvFwd.h"
#include "ObjectGuid.h"
#include <ctime>
#include <map>
#include <string>
#include <unordered_map>
#include <vector>

class Player;

// Payment method of a shop product (ShopProduct::Currency). Distinct from the client's CurrencyTypes.db2
// ids - this is how the SERVER charges. REAL_MONEY is the Rail A marker: such a product is never charged
// on the wire; its checkout goes to the shop2 web overlay (or is granted instantly for QA) per
// Shop.RealMoney.Mode. The COMMERCE_MASTER_PLAN's "CurrencyTypesID == 0 (real money)" maps to this value,
// because value 0 here already means "free".
enum ShopCurrency : uint8
{
    SHOP_CURRENCY_FREE       = 0,   // no cost
    SHOP_CURRENCY_GOLD       = 1,   // copper
    SHOP_CURRENCY_ITEM_TOKEN = 2,   // a token item (PriceItemId x PriceItemCount)
    SHOP_CURRENCY_CUSTOM     = 3,   // custom currency amount
    SHOP_CURRENCY_REAL_MONEY = 4    // real money -> Rail A (web checkout); never charged on the wire
};

// Server behaviour when a real-money (Rail A) product's checkout is opened. Read from the
// Shop.RealMoney.Mode config string; see BattlePayMgr::GetRealMoneyMode.
enum ShopRealMoneyMode : uint8
{
    SHOP_REAL_MONEY_WEB      = 0,   // let the client web-checkout; the realm sends no game response
    SHOP_REAL_MONEY_INSTANT  = 1,   // grant server-side immediately with no charge (private-realm QA)
    SHOP_REAL_MONEY_DISABLED = 2    // refuse: no SSO token, no grant
};

// A single deliverable payload of a shop product (>1 per product = a bundle).
struct ShopDeliverable
{
    uint8  Type  = 0;   // 1 item | 2 spell | 3 WoW Token | 4 game-time (reserved) | 5 service
    uint32 Id    = 0;   // itemId / spellId / 0 (token) / days / serviceType
    uint32 Count = 1;
};

// Entitlement ("distribution") lifecycle. Only AVAILABLE is ever put on the wire: it is the sole status
// byte observed in a real capture (the 68275 distribution record carries 1 at record offset 8). The rest
// are server-side bookkeeping; their numeric values follow the client's own DistributionStatus ordering
// so that emitting them later needs no renumbering, but nothing here depends on that.
enum ShopEntitlementStatus : uint8
{
    SHOP_ENTITLEMENT_NONE      = 0,
    SHOP_ENTITLEMENT_AVAILABLE = 1,     // owned, unassigned - the only status sent to the client
    SHOP_ENTITLEMENT_CLAIMED   = 2,     // transient: an assign holds the compare-and-swap token
    SHOP_ENTITLEMENT_BOUND     = 3,     // assigned to a character; delivered at that character's next login
    SHOP_ENTITLEMENT_FINISHED  = 4,     // delivered; terminal
    SHOP_ENTITLEMENT_REVOKED   = 5      // withdrawn / refunded; terminal
};

// The catalog's own deliverable vocabulary for a Value Added Service, as carried in the `id` column of
// a type-5 `shop_product_deliverable` row (see BuildDeliverable in BattlePayHandler.cpp). Only the boost
// is implemented; the others are listed because the same column names them and a product row may
// legitimately carry one, in which case the entitlement is sold but refuses to redeem.
enum ShopServiceType : uint8
{
    SHOP_SERVICE_NONE               = 0,
    SHOP_SERVICE_CHARACTER_BOOST    = 1,
    SHOP_SERVICE_NAME_CHANGE        = 5,
    SHOP_SERVICE_FACTION_CHANGE     = 6,
    SHOP_SERVICE_RACE_CHANGE        = 8,
    SHOP_SERVICE_CHARACTER_TRANSFER = 11
};

// One `character_inventory` row of the boost target, in its own slot space (bag = 0).
struct ShopBoostInventorySlot
{
    uint64 ItemGuid = 0;
    uint32 ItemId   = 0;
};

// Everything a character boost needs to know about its target. The boost is applied from the GLUE
// SCREEN, so the target is offline and none of this can be read off a Player - it is all read out of
// the character database first, before any entitlement is claimed and before anything is written.
struct ShopBoostTarget
{
    ObjectGuid Guid;
    uint32 AccountId = 0;
    uint8  ClassId   = 0;
    uint8  RaceId    = 0;
    uint8  Level     = 0;
    uint8  BackpackSlots = 0;                            // characters.inventorySlots: usable backpack slots
    std::map<uint8, ShopBoostInventorySlot> OwnSlots;    // slot -> occupant, bag = 0 only
};

// One row of `account_battlepay_entitlement`: a purchased-but-unapplied product.
struct ShopEntitlement
{
    uint64 DistributionID = 0;      // wire id; high 32 bits = realm id (same discipline as PurchaseID)
    uint32 ProductID      = 0;
    uint8  ServiceType    = 0;      // 0 = deferred delivery of the product payload; else a VAS service type
    uint8  Status         = SHOP_ENTITLEMENT_NONE;
    uint64 PurchaseID     = 0;
    time_t CreateTime     = 0;
};

// An admin-defined shop product (row of `shop_product` + its `shop_product_deliverable` rows).
// The catalog wire can only express name/description/price/flags; everything else here is enforced
// server-side at purchase time (IsPurchasable) or used to decide slot assignment.
struct ShopProduct
{
    uint32 ProductID = 0;           // admin id (routing frees it from the blob's fixed slot ids)
    bool   Enabled   = true;
    std::string Name;
    std::string Description;
    uint8  Currency  = 1;           // ShopCurrency: 0 free | 1 gold | 2 item-token | 3 custom | 4 real-money(web)
    uint64 Price     = 0;           // copper (currency 1) or currency amount (3)
    uint32 PriceItemId = 0;         // currency 2: token item
    uint32 PriceItemCount = 0;
    bool   HasDisplayPrice = false; // wire fixed-point /100000 override (NULL in DB => derived)
    uint64 DisplayPrice = 0;
    uint32 DisplayFlags = 0;        // BattlepayDisplayFlags (8 HiddenPrice, 256 HideWhenOwned)
    uint32 GroupId   = 0;           // stored; rendered only once the ShopEntry region is cracked (SH-7)
    int32  Ordering  = 0;           // slot-assignment priority (lower = earlier slot)
    bool   Featured  = false;
    time_t AvailableFrom  = 0;      // 0 = always
    time_t AvailableUntil = 0;      // 0 = always
    uint8  ReqLevel  = 0;
    int8   ReqFaction = -1;         // -1 any, else TeamId (0 alliance, 1 horde)
    bool   HideIfOwned = false;
    uint32 PlayerConditionId = 0;
    std::string Comment;
    std::vector<ShopDeliverable> Deliverables;
};

// In-game Shop (BattlePay / StoreUI) backend + catalog administration.
//
// The captured 68275 GET_PRODUCT_LIST_RESPONSE blob is a TEMPLATE. LoadCatalog() reskins its 9
// simple-shape slots from the `shop_product` DB rows via the byte-exact BattlePayCatalogWriter and
// records a slot->product routing map, so the offer set is DB-driven and reloadable without a restart.
// Everything the wire cannot carry (enable/disable, windows, level/faction/owned/condition gates) is
// enforced at purchase time by IsPurchasable().
class TC_GAME_API BattlePayMgr
{
public:
    static BattlePayMgr* instance();

    // Loads the raw template + distribution blobs from <DataDir>/battlepay/.
    void Load();

    // Reads shop_product / shop_product_deliverable / shop_slot_override, assembles the catalog blob
    // from the template, and builds the routing map. Call after Load().
    void LoadCatalog();

    // Atomic re-run of LoadProducts + LoadCatalog on the world thread (.reload shop_catalog / .shop).
    void Reload();

    bool HasCatalog() const { return !_productListBlob.empty(); }
    std::vector<uint8> const& GetProductListBlob() const { return _productListBlob; }

    // Bumped every time the catalog blob is (re)built; drives the once-per-session send throttle.
    uint32 GetCatalogGeneration() const { return _catalogGeneration; }

    // Distribution list (see Load()); unblocks the client's shop panel.
    bool HasDistributionList() const { return !_distributionListBlob.empty(); }
    std::vector<uint8> const& GetDistributionListBlob() const { return _distributionListBlob; }

    // Purchase routing: the client buys by the advertised (slot) productID, which the assembly kept;
    // this resolves it to the admin ShopProduct. Unrouted (placeholder) slots return nullptr.
    ShopProduct const* GetProductByAdvertisedId(uint32 advertisedProductId) const;
    // Direct lookup by admin productId (used by the .shop commands).
    ShopProduct const* GetProduct(uint32 adminProductId) const;
    std::unordered_map<uint32, ShopProduct> const& GetProducts() const { return _products; }

    // Purchase-time authority for everything the wire cannot express.
    bool IsPurchasable(ShopProduct const& product, Player* player, time_t now) const;
    // True if the product's payload is entirely spells the player already knows (never charge for nothing).
    static bool IsAlreadyFullyOwned(ShopProduct const& product, Player* player);

    // Earliest future availability-window boundary (0 = none); World tick rebuilds when it passes.
    time_t GetNextRebuildTime() const { return _nextRebuildTime; }
    void RebuildIfDue(time_t now);

    // Dry-run assembly summary for `.shop preview` / `.shop list`.
    std::string BuildStatusReport() const;

    // Allocates the next PurchaseID. The counter is seeded from the ledger at startup (survives restart)
    // and namespaced by realm id in its high 32 bits, so PurchaseIDs never collide across realms sharing
    // one auth DB (C-13/C-22). One worldserver per realm means the in-memory increment is race-free.
    uint64 GeneratePurchaseID() { return ++_purchaseCounter; }

    // Persists a completed purchase to the shared account_battlepay_purchase ledger (auth DB), the home
    // that answers CMSG_BATTLE_PAY_GET_PURCHASE_LIST. ProductID may legitimately be 0 (C-32).
    void RecordPurchase(uint32 accountId, uint64 purchaseID, int32 status, int32 resultCode,
        uint32 productID, uint64 basePrice, uint64 userPrice);

    // ---- Entitlements ("distributions") -------------------------------------------------------------
    // True if `product` cannot be delivered on the spot and must become an entitlement instead: it
    // carries a service deliverable (type 5), or `player` is null because we are at character select.
    static bool NeedsEntitlement(ShopProduct const& product, Player* player);
    // The VAS service type a product's type-5 deliverable names, or 0 if it has none.
    static uint8 GetServiceType(ShopProduct const& product);

    // How the realm treats a real-money (Rail A) checkout, from the Shop.RealMoney.Mode config string
    // ("web" | "instant" | "disabled"; unknown values fall back to the default). See the OPEN_CHECKOUT
    // rail branch in HandleBattlePayOpenCheckout.
    static ShopRealMoneyMode GetRealMoneyMode();

    // Allocates the next DistributionID (realm-namespaced, seeded from the store at startup).
    uint64 GenerateDistributionID() { return ++_distributionCounter; }

    // Synchronously inserts an AVAILABLE entitlement. Returns 0 on failure. Synchronous because the
    // caller charges for it immediately afterwards and must not charge for a row that does not exist.
    uint64 CreateEntitlement(uint32 accountId, uint32 productId, uint8 serviceType, uint64 purchaseId);

    // Compare-and-swap claim of an AVAILABLE entitlement for `accountId`, binding it to a character.
    // Writes a random token under a `status = 1` guard, then reads the row back and succeeds only if OUR
    // token survived - so two racing assigns (replay, or a second realm on the shared auth DB) can never
    // both win. On success `outToken` holds the token the caller must quote to commit or roll back.
    // On failure `outResult` carries the client PurchaseResult code explaining why.
    bool ClaimEntitlement(uint32 accountId, uint64 distributionId, uint32 realmId, uint64 targetCharacter,
        uint64& outToken, ShopEntitlement& outEntitlement, int32& outResult);

    // Moves an entitlement between statuses under a (status, claimToken) guard. Used to commit a claim
    // (2 -> 3), roll one back (2 -> 1) and consume a bound one at delivery (3 -> 4).
    bool TransitionEntitlement(uint64 distributionId, uint8 fromStatus, uint64 fromToken,
        uint8 toStatus, uint64 toToken);

    // ---- Character boost (service type 1) ------------------------------------------------------------
    // The level a boost takes a character to, and the boost type the glue screen advertises. Both are
    // config, not constants, because neither is derivable from anything this server owns.
    static uint8 GetCharacterBoostLevel();
    static int32 GetCharacterBoostType();
    // True if this product's service deliverable is a character boost.
    static bool IsCharacterBoostProduct(ShopProduct const& product);

    // Builds - but does not commit - the single character-database transaction that turns `target` into
    // a boosted character: level and specialization, the CharacterLoadout starter kit, the original
    // equipment displaced into the backpack rather than deleted, the character-select equipment cache so
    // the glue screen shows the new look, and the `character_shop_boost` record the enumeration reads.
    //
    // Returns a null transaction and writes nothing at all if the kit cannot be sourced or cannot be
    // placed - the caller must treat that as a failed boost and give the entitlement back.
    CharacterDatabaseTransaction BuildCharacterBoostTransaction(ShopBoostTarget const& target,
        uint32 specializationId, uint64 distributionId, uint32 productId) const;

private:
    BattlePayMgr() = default;
    ~BattlePayMgr() = default;
    BattlePayMgr(BattlePayMgr const&) = delete;
    BattlePayMgr& operator=(BattlePayMgr const&) = delete;

    bool LoadBlobFile(std::string const& fileName, std::vector<uint8>& out);
    void LoadProducts();                                   // fills _products from the DB
    // Assembles the catalog blob into `outBlob` and the routing into `outRouting`; returns false on
    // a fatal error (no template / self-check failure). Used by LoadCatalog and BuildStatusReport.
    bool AssembleCatalog(std::vector<uint8>& outBlob, std::unordered_map<uint32, uint32>& outRouting,
        std::string* report) const;

    std::vector<uint8> _templateBlob;
    std::vector<uint8> _productListBlob;
    std::vector<uint8> _distributionListBlob;
    uint64 _distributionCounter = 0;
    std::unordered_map<uint32, ShopProduct> _products;         // admin productId -> product
    std::unordered_map<uint32, uint32> _slotRouting;           // advertised (slot) productId -> admin id
    std::unordered_map<uint8, uint32> _slotOverrides;          // slotIndex -> admin id (0 = placeholder)
    uint64 _purchaseCounter = 0;
    uint32 _catalogGeneration = 0;
    time_t _nextRebuildTime = 0;
};

#define sBattlePayMgr BattlePayMgr::instance()

#endif // TRINITYCORE_BATTLE_PAY_MGR_H
