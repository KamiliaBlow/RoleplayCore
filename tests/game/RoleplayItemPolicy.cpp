#include "ItemTemplate.h"
#include "Item.h"

#include "catch2/catch_test_macros.hpp"

TEST_CASE("RP carrier flag hides native sets and sockets without changing equipment identity", "[roleplay][items]")
{
    ItemEntry basic{};
    basic.ID = 1900001;
    basic.ClassID = ITEM_CLASS_ARMOR;
    basic.SubclassID = ITEM_SUBCLASS_ARMOR_CLOTH;

    ItemSparseEntry sparse{};
    sparse.InventoryType = INVTYPE_CHEST;
    sparse.ItemSet = 77;
    sparse.SocketType[0] = SOCKET_COLOR_RED;

    ItemTemplate item{};
    item.BasicData = &basic;
    item.ExtendedData = &sparse;
    item.FlagsCu = ITEM_FLAGS_CU_FOLLOW_LOOT_RULES;

    REQUIRE_FALSE(item.IsRoleplayOnly());
    REQUIRE(item.GetItemSet() == 77);
    REQUIRE(item.GetSocketColor(0) == SOCKET_COLOR_RED);

    item.FlagsCu |= ITEM_FLAGS_CU_RP_ONLY;

    REQUIRE(item.IsRoleplayOnly());
    REQUIRE(item.GetItemSet() == 0);
    REQUIRE(item.GetSocketColor(0) == 0);
    REQUIRE(item.GetId() == basic.ID);
    REQUIRE(item.GetClass() == ITEM_CLASS_ARMOR);
    REQUIRE(item.GetSubClass() == ITEM_SUBCLASS_ARMOR_CLOTH);
    REQUIRE(item.GetInventoryType() == INVTYPE_CHEST);
    REQUIRE(item.HasFlag(ITEM_FLAGS_CU_FOLLOW_LOOT_RULES));

    // Shared DB2 records are not mutated; ordinary templates keep native behavior.
    item.FlagsCu &= ~ITEM_FLAGS_CU_RP_ONLY;
    REQUIRE(item.GetItemSet() == 77);
    REQUIRE(item.GetSocketColor(0) == SOCKET_COLOR_RED);
}

TEST_CASE("Free supplies never acquire a vendor value from quality or an upgrade", "[roleplay][items]")
{
    ItemEntry basic{};
    basic.ClassID = ITEM_CLASS_WEAPON;
    ItemSparseEntry sparse{};
    sparse.SellPrice = 5;
    sparse.Flags[1] = ITEM_FLAG2_OVERRIDE_GOLD_COST;
    ItemTemplate item{};
    item.BasicData = &basic;
    item.ExtendedData = &sparse;
    REQUIRE(Item::GetSellPrice(&item, 1, 1) == 5);
    item.FlagsCu |= ITEM_FLAGS_CU_NO_VENDOR_VALUE;
    for (uint32 quality = 0; quality <= 7; ++quality)
        REQUIRE(Item::GetSellPrice(&item, quality, 100) == 0);
    sparse.Flags[1] = 0;
    REQUIRE(Item::GetSellPrice(&item, 5, 100) == 0);
    REQUIRE(sparse.SellPrice == 5);
}
