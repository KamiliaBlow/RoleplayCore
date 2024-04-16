/*
* Copyright (C) 2010 - 2020 Eluna Lua Engine <http://emudevs.com/>
* This program is free software licensed under GPL version 3
* Please see the included DOCS/LICENSE.md for more information
*/

#ifndef ITEMMETHODS_H
#define ITEMMETHODS_H

/***
 * Inherits all methods from: [Object]
 */
namespace LuaItem
{
    /**
     * Returns 'true' if the [Item] is soulbound, 'false' otherwise
     *
     * @return bool isSoulBound
     */
    int IsSoulBound(lua_State* L, Item* item)
    {
        Eluna::Push(L, item->IsSoulBound());
        return 1;
    }

    /**
     * Returns 'true' if the [Item] is account bound, 'false' otherwise
     *
     * @return bool isAccountBound
     */
    int IsBoundAccountWide(lua_State* L, Item* item)
    {
        Eluna::Push(L, item->IsBoundAccountWide());
        return 1;
    }

    /**
     * Returns 'true' if the [Item] is bound to a [Player] by an enchant, 'false' otehrwise
     *
     * @return bool isBoundByEnchant
     */
    int IsBoundByEnchant(lua_State* L, Item* item)
    {
        Eluna::Push(L, item->IsBoundByEnchant());
        return 1;
    }

    /**
     * Returns 'true' if the [Item] is not bound to the [Player] specified, 'false' otherwise
     *
     * @param [Player] player : the [Player] object to check the item against
     * @return bool isNotBound
     */
    int IsNotBoundToPlayer(lua_State* L, Item* item)
    {
        Player* player = Eluna::CHECKOBJ<Player>(L, 2);

        Eluna::Push(L, item->IsBindedNotWith(player));
        return 1;
    }

    /**
     * Returns 'true' if the [Item] is locked, 'false' otherwise
     *
     * @return bool isLocked
     */
    int IsLocked(lua_State* L, Item* item)
    {
        Eluna::Push(L, item->IsLocked());
        return 1;
    }

    /**
     * Returns 'true' if the [Item] is a bag, 'false' otherwise
     *
     * @return bool isBag
     */
    int IsBag(lua_State* L, Item* item)
    {
        Eluna::Push(L, item->IsBag());
        return 1;
    }

    /**
     * Returns 'true' if the [Item] is a currency token, 'false' otherwise
     *
     * @return bool isCurrencyToken
     */
    int IsCurrencyToken(lua_State* L, Item* item)
    {
        Eluna::Push(L, item->IsCurrencyToken());
        return 1;
    }

    /**
     * Returns 'true' if the [Item] is a not an empty bag, 'false' otherwise
     *
     * @return bool isNotEmptyBag
     */
    int IsNotEmptyBag(lua_State* L, Item* item)
    {
        Eluna::Push(L, item->IsNotEmptyBag());
        return 1;
    }

    /**
     * Returns 'true' if the [Item] is broken, 'false' otherwise
     *
     * @return bool isBroken
     */
    int IsBroken(lua_State* L, Item* item)
    {
        Eluna::Push(L, item->IsBroken());
        return 1;
    }

    /**
     * Returns 'true' if the [Item] can be traded, 'false' otherwise
     *
     * @return bool isTradeable
     */
    int CanBeTraded(lua_State* L, Item* item)
    {
        bool mail = Eluna::CHECKVAL<bool>(L, 2, false);

        Eluna::Push(L, item->CanBeTraded(mail));
        return 1;
    }

    /**
     * Returns 'true' if the [Item] is currently in a trade window, 'false' otherwise
     *
     * @return bool isInTrade
     */
    int IsInTrade(lua_State* L, Item* item)
    {
        Eluna::Push(L, item->IsInTrade());
        return 1;
    }

    /**
     * Returns 'true' if the [Item] is currently in a bag, 'false' otherwise
     *
     * @return bool isInBag
     */
    int IsInBag(lua_State* L, Item* item)
    {
        Eluna::Push(L, item->IsInBag());
        return 1;
    }

    /**
     * Returns 'true' if the [Item] is currently equipped, 'false' otherwise
     *
     * @return bool isEquipped
     */
    int IsEquipped(lua_State* L, Item* item)
    {
        Eluna::Push(L, item->IsEquipped());
        return 1;
    }

    /**
     * Returns 'true' if the [Item] has the [Quest] specified tied to it, 'false' otherwise
     *
     * @param uint32 questId : the [Quest] id to be checked
     * @return bool hasQuest
     */
    int HasQuest(lua_State* L, Item* item)
    {
        uint32 quest = Eluna::CHECKVAL<uint32>(L, 2);

        Eluna::Push(L, item->hasQuest(quest));
        return 1;
    }

    /**
     * Returns 'true' if the [Item] is a potion, 'false' otherwise
     *
     * @return bool isPotion
     */
    int IsPotion(lua_State* L, Item* item)
    {
        Eluna::Push(L, item->IsPotion());
        return 1;
    }

    /**
     * Returns 'true' if the [Item] is a conjured consumable, 'false' otherwise
     *
     * @return bool isConjuredConsumable
     */
    int IsConjuredConsumable(lua_State* L, Item* item)
    {
        Eluna::Push(L, item->IsConjuredConsumable());
        return 1;
    }

    /**
     * Returns 'true' if the refund period has expired for this [Item], 'false' otherwise
     *
     * @return bool isRefundExpired
     */
    int IsRefundExpired(lua_State* L, Item* item)
    {
        Eluna::Push(L, item->IsRefundExpired());
        return 1;
    }

    /**
     * Returns the chat link of the [Item]
     *
     * <pre>
     * enum LocaleConstant
     * {
     *     LOCALE_enUS = 0,
     *     LOCALE_koKR = 1,
     *     LOCALE_frFR = 2,
     *     LOCALE_deDE = 3,
     *     LOCALE_zhCN = 4,
     *     LOCALE_zhTW = 5,
     *     LOCALE_esES = 6,
     *     LOCALE_esMX = 7,
     *     LOCALE_ruRU = 8
     * };
     * </pre>
     *
     * @param [LocaleConstant] locale = DEFAULT_LOCALE : locale to return the [Item]'s name in
     * @return string itemLink
     */
    int GetItemLink(lua_State* L, Item* item)
    {
        uint8 locale = Eluna::CHECKVAL<uint8>(L, 2, DEFAULT_LOCALE);
        if (locale >= TOTAL_LOCALES)
            return luaL_argerror(L, 2, "valid LocaleConstant expected");

        const ItemTemplate* temp = item->GetTemplate();
        std::string name = temp->GetName(static_cast<LocaleConstant>(locale));

        // \124cffa335ee\124Hitem:174164::::::::120::::2:4824:1517:\124h[Breastplate of Twilight Decimation]\124h\124r
        std::ostringstream oss;
        oss << "|c" << std::hex << ItemQualityColors[temp->GetQuality()] << std::dec <<
            "|Hitem:" << temp->GetId() << "::::::::" << (uint32)item->GetOwner()->GetLevel()
            << ":::::::" << "|h[" << name << "]|h|r";

        Eluna::Push(L, oss.str());
        return 1;
    }

    int GetOwnerGUID(lua_State* L, Item* item)
    {
        Eluna::Push(L, item->GetOwnerGUID());
        return 1;
    }

    /**
     * Returns the [Player] who currently owns the [Item]
     *
     * @return [Player] player : the [Player] who owns the [Item]
     */
    int GetOwner(lua_State* L, Item* item)
    {
        Eluna::Push(L, item->GetOwner());
        return 1;
    }

    /**
     * Returns the [Item]s stack count
     *
     * @return uint32 count
     */
    int GetCount(lua_State* L, Item* item)
    {
        Eluna::Push(L, item->GetCount());
        return 1;
    }

    /**
     * Returns the [Item]s max stack count
     *
     * @return uint32 maxCount
     */
    int GetMaxStackCount(lua_State* L, Item* item)
    {
        Eluna::Push(L, item->GetMaxStackCount());
        return 1;
    }

    /**
     * Returns the [Item]s current slot
     *
     * @return uint8 slot
     */
    int GetSlot(lua_State* L, Item* item)
    {
        Eluna::Push(L, item->GetSlot());
        return 1;
    }

    /**
     * Returns the [Item]s current bag slot
     *
     * @return uint8 bagSlot
     */
    int GetBagSlot(lua_State* L, Item* item)
    {
        Eluna::Push(L, item->GetBagSlot());
        return 1;
    }

    /**
     * Returns the [Item]s enchantment ID by enchant slot specified
     *
     * @param [EnchantmentSlot] enchantSlot : the enchant slot specified
     * @return uint32 enchantId : the id of the enchant slot specified
     */
    int GetEnchantmentId(lua_State* L, Item* item)
    {
        uint32 enchant_slot = Eluna::CHECKVAL<uint32>(L, 2);

        if (enchant_slot >= MAX_INSPECTED_ENCHANTMENT_SLOT)
            return luaL_argerror(L, 2, "valid EnchantmentSlot expected");

        Eluna::Push(L, item->GetEnchantmentId(EnchantmentSlot(enchant_slot)));
        return 1;
    }

    /**
     * Returns class of the [Item]
     *
     * @return uint32 class
     */
    int GetClass(lua_State* L, Item* item)
    {
        Eluna::Push(L, item->GetTemplate()->GetClass());
        return 1;
    }

    /**
     * Returns subclass of the [Item]
     *
     * @return uint32 subClass
     */
    int GetSubClass(lua_State* L, Item* item)
    {
        Eluna::Push(L, item->GetTemplate()->GetSubClass());
        return 1;
    }

    /**
     * Returns the name of the [Item]
     *
     * @return string name
     */
    int GetName(lua_State* L, Item* item)
    {
        Eluna::Push(L, item->GetTemplate()->GetName((LocaleConstant)0));
        return 1;
    }

    /**
     * Returns the display ID of the [Item]
     *
     * @return uint32 displayId
     */
    int GetDisplayId(lua_State* L, Item* item)
    {
        Eluna::Push(L, item->GetTemplate()->ExtendedData->Display.Str[(LocaleConstant)0]);
        return 1;
    }

    /**
     * Returns the quality of the [Item]
     *
     * @return uint32 quality
     */
    int GetQuality(lua_State* L, Item* item)
    {
        Eluna::Push(L, item->GetTemplate()->GetQuality());
        return 1;
    }

    /**
     * Returns the default purchase count of the [Item]
     *
     * @return uint32 count
     */
    int GetBuyCount(lua_State* L, Item* item)
    {
        Eluna::Push(L, item->GetTemplate()->GetBuyCount());
        return 1;
    }

    /**
     * Returns the purchase price of the [Item]
     *
     * @return uint32 price
     */
    int GetBuyPrice(lua_State* L, Item* item)
    {
        Eluna::Push(L, item->GetTemplate()->GetBuyPrice());
        return 1;
    }

    /**
     * Returns the sell price of the [Item]
     *
     * @return uint32 price
     */
    int GetSellPrice(lua_State* L, Item* item)
    {
        Eluna::Push(L, item->GetTemplate()->GetSellPrice());
        return 1;
    }

    /**
     * Returns the inventory type of the [Item]
     *
     * @return uint32 inventoryType
     */
    int GetInventoryType(lua_State* L, Item* item)
    {
        Eluna::Push(L, item->GetTemplate()->GetInventoryType());
        return 1;
    }

    /**
     * Returns the [Player] classes allowed to use this [Item]
     *
     * @return uint32 allowableClass
     */
    int GetAllowableClass(lua_State* L, Item* item)
    {
        Eluna::Push(L, item->GetTemplate()->GetAllowableClass());
        return 1;
    }

    /**
     * Returns the [Player] races allowed to use this [Item]
     *
     * @return uint32 allowableRace
     */
    int GetAllowableRace(lua_State* /*L*/, Item* item)
    {
        item->GetTemplate()->GetAllowableRace();
        return 1;
    }

    /**
     * Returns the [Item]s level
     *
     * @return uint32 itemLevel
     */
    int GetItemLevel(lua_State* L, Item* item)
    {
        Eluna::Push(L, item->GetTemplate()->GetBaseItemLevel());
        return 1;
    }

    /**
     * Returns the minimum level required to use this [Item]
     *
     * @return uint32 requiredLevel
     */
    int GetRequiredLevel(lua_State* L, Item* item)
    {
        Eluna::Push(L, item->GetTemplate()->GetBaseRequiredLevel());
        return 1;
    }

    /**
     * Returns the item set ID of this [Item]
     *
     * @return uint32 itemSetId
     */
    int GetItemSet(lua_State* L, Item* item)
    {
        Eluna::Push(L, item->GetTemplate()->GetItemSet());
        return 1;
    }

    /**
     * Returns the bag size of this [Item], 0 if [Item] is not a bag
     *
     * @return uint32 bagSize
     */
    int GetBagSize(lua_State* L, Item* item)
    {
        if (Bag* bag = item->ToBag())
            Eluna::Push(L, bag->GetBagSize());
        else
            Eluna::Push(L, 0);
        return 1;
    }

    /**
     * Sets the [Player] specified as the owner of the [Item]
     *
     * @param [Player] player : the [Player] specified
     */
    int SetOwner(lua_State* L, Item* item)
    {
        Player* player = Eluna::CHECKOBJ<Player>(L, 2);

        item->SetOwnerGUID(player->GET_GUID());
        return 0;
    }

    /**
     * Sets the binding of the [Item] to 'true' or 'false'
     *
     * @param bool setBinding
     */
    int SetBinding(lua_State* L, Item* item)
    {
        bool soulbound = Eluna::CHECKVAL<bool>(L, 2);

        item->SetBinding(soulbound);
        item->SetState(ITEM_CHANGED, item->GetOwner());

        return 0;
    }

    /**
     * Sets the stack count of the [Item]
     *
     * @param uint32 count
     */
    int SetCount(lua_State* L, Item* item)
    {
        uint32 count = Eluna::CHECKVAL<uint32>(L, 2);
        item->SetCount(count);
        return 0;
    }

    /**
     * Sets the specified enchantment of the [Item] to the specified slot
     *
     * @param uint32 enchantId : the ID of the enchant to be applied
     * @param uint32 enchantSlot : the slot for the enchant to be applied to
     * @return bool enchantmentSuccess : if enchantment is successfully set to specified slot, returns 'true', otherwise 'false'
     */
    int SetEnchantment(lua_State* L, Item* item)
    {
        Player* owner = item->GetOwner();
        if (!owner)
        {
            Eluna::Push(L, false);
            return 1;
        }

        uint32 enchant = Eluna::CHECKVAL<uint32>(L, 2);
        if (!sSpellItemEnchantmentStore.LookupEntry(enchant))
        {
            Eluna::Push(L, false);
            return 1;
        }

        EnchantmentSlot slot = (EnchantmentSlot)Eluna::CHECKVAL<uint32>(L, 3);
        if (slot >= MAX_INSPECTED_ENCHANTMENT_SLOT)
            return luaL_argerror(L, 2, "valid EnchantmentSlot expected");

        owner->ApplyEnchantment(item, slot, false);
        item->SetEnchantment(slot, enchant, 0, 0);
        owner->ApplyEnchantment(item, slot, true);
        Eluna::Push(L, true);
        return 1;
    }

    /* OTHER */
    /**
     * Removes an enchant from the [Item] by the specified slot
     *
     * @param uint32 enchantSlot : the slot for the enchant to be removed from
     * @return bool enchantmentRemoved : if enchantment is successfully removed from specified slot, returns 'true', otherwise 'false'
     */
    int ClearEnchantment(lua_State* L, Item* item)
    {
        Player* owner = item->GetOwner();
        if (!owner)
        {
            Eluna::Push(L, false);
            return 1;
        }

        EnchantmentSlot slot = (EnchantmentSlot)Eluna::CHECKVAL<uint32>(L, 2);
        if (slot >= MAX_INSPECTED_ENCHANTMENT_SLOT)
            return luaL_argerror(L, 2, "valid EnchantmentSlot expected");

        if (!item->GetEnchantmentId(slot))
        {
            Eluna::Push(L, false);
            return 1;
        }

        owner->ApplyEnchantment(item, slot, false);
        item->ClearEnchantment(slot);
        Eluna::Push(L, true);
        return 1;
    }

    /**
     * Saves the [Item] to the database
     */
    int SaveToDB(lua_State* /*L*/, Item* item)
    {
        CharacterDatabaseTransaction trans = CharacterDatabaseTransaction(nullptr);
        item->SaveToDB(trans);
        return 0;
    }
};
#endif
