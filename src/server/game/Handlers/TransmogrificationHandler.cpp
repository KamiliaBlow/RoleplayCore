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
#include "CollectionMgr.h"
#include "ConditionMgr.h"
#include "DB2Stores.h"
#include "Item.h"
#include "Log.h"
#include "NPCPackets.h"
#include "ObjectMgr.h"
#include "Player.h"
#include "SpellMgr.h"
#include "TransmogrificationPackets.h"


namespace
{
void LogTransmogOutfitOpcodeDebug(WorldSession const* session, char const* opcodeName, size_t payloadSize, std::string const& payloadPreviewHex)
{
    TC_LOG_DEBUG("network.opcode.transmog", "{} [{}] size={} preview(<=128B)={}", opcodeName, session->GetPlayerInfo(), payloadSize, payloadPreviewHex);
}

<<<<<<< ours
=======
bool ValidateTransmogOutfitPlayerGuid(WorldSession* session, ObjectGuid const& playerGuid, char const* opcodeName)
{
    if (!playerGuid || playerGuid == session->GetPlayer()->GetGUID())
        return true;

    TC_LOG_ERROR("network.opcode.transmog", "{} rejected [{}]: packet player guid {} does not match session player guid {}",
        opcodeName, session->GetPlayerInfo(), playerGuid.ToString(), session->GetPlayer()->GetGUID().ToString());
    return false;
}

uint32 FindNextAvailableTransmogSetID(Player const* player)
{
    for (uint32 setId = 0; setId < MAX_EQUIPMENT_SET_INDEX; ++setId)
        if (!player->GetTransmogOutfitBySetID(setId))
            return setId;

    return MAX_EQUIPMENT_SET_INDEX;
}

>>>>>>> theirs
bool ValidateTransmogOutfitSet(WorldSession* session, EquipmentSetInfo::EquipmentSetData& set)
{
    if (set.SetID >= MAX_EQUIPMENT_SET_INDEX)
    {
        TC_LOG_ERROR("network.opcode.transmog", "Transmog outfit rejected [{}]: invalid SetID {}", session->GetPlayerInfo(), set.SetID);
        return false;
    }

    set.Type = EquipmentSetInfo::TRANSMOG;

    for (uint8 i = 0; i < EQUIPMENT_SLOT_END; ++i)
    {
        set.Pieces[i].Clear();

        if (set.IgnoreMask & (1 << i))
        {
            set.Appearances[i] = 0;
            continue;
        }

        if (set.Appearances[i])
        {
            if (!sItemModifiedAppearanceStore.LookupEntry(set.Appearances[i]))
            {
                TC_LOG_ERROR("network.opcode.transmog", "Transmog outfit rejected [{}]: invalid appearance {} in slot {}", session->GetPlayerInfo(), set.Appearances[i], i);
                return false;
            }

            auto [hasAppearance, isTemporary] = session->GetCollectionMgr()->HasItemAppearance(set.Appearances[i]);
            if (!hasAppearance)
            {
                TC_LOG_ERROR("network.opcode.transmog", "Transmog outfit rejected [{}]: uncollected appearance {} in slot {}", session->GetPlayerInfo(), set.Appearances[i], i);
                return false;
            }
        }
        else
            set.IgnoreMask |= (1 << i);
    }

    set.IgnoreMask &= 0x7FFFF;

    auto validateIllusion = [session](uint32 enchantId) -> bool
    {
        SpellItemEnchantmentEntry const* illusion = sSpellItemEnchantmentStore.LookupEntry(enchantId);
        if (!illusion)
            return false;

        if (!illusion->ItemVisual || !illusion->GetFlags().HasFlag(SpellItemEnchantmentFlags::AllowTransmog))
            return false;

        if (!ConditionMgr::IsPlayerMeetingCondition(session->GetPlayer(), illusion->TransmogUseConditionID))
            return false;

        if (illusion->ScalingClassRestricted > 0 && uint8(illusion->ScalingClassRestricted) != session->GetPlayer()->GetClass())
            return false;

        return true;
    };

    if (set.Enchants[0] && !validateIllusion(set.Enchants[0]))
    {
        TC_LOG_ERROR("network.opcode.transmog", "Transmog outfit rejected [{}]: invalid main-hand enchant {}", session->GetPlayerInfo(), set.Enchants[0]);
        return false;
    }

    if (set.Enchants[1] && !validateIllusion(set.Enchants[1]))
    {
        TC_LOG_ERROR("network.opcode.transmog", "Transmog outfit rejected [{}]: invalid off-hand enchant {}", session->GetPlayerInfo(), set.Enchants[1]);
        return false;
    }

    return true;
}
}

void WorldSession::HandleTransmogrifyItems(WorldPackets::Transmogrification::TransmogrifyItems& transmogrifyItems)
{
    Player* player = GetPlayer();
    // Validate
    if (!player->GetNPCIfCanInteractWith(transmogrifyItems.Npc, UNIT_NPC_FLAG_TRANSMOGRIFIER, UNIT_NPC_FLAG_2_NONE))
    {
        TC_LOG_DEBUG("network", "WORLD: HandleTransmogrifyItems - {} not found or player can't interact with it.", transmogrifyItems.Npc.ToString());
        return;
    }

    int64 cost = 0;
    std::unordered_map<Item*, std::pair<uint32, uint32>> transmogItems;
    std::unordered_map<Item*, uint32> illusionItems;

    std::vector<Item*> resetAppearanceItems;
    std::vector<Item*> resetIllusionItems;
    std::vector<uint32> bindAppearances;

    auto validateAndStoreTransmogItem = [&](Item* itemTransmogrified, uint32 itemModifiedAppearanceId, bool isSecondary)
    {
        ItemModifiedAppearanceEntry const* itemModifiedAppearance = sItemModifiedAppearanceStore.LookupEntry(itemModifiedAppearanceId);
        if (!itemModifiedAppearance)
        {
            TC_LOG_DEBUG("network", "WORLD: HandleTransmogrifyItems - {}, Name: {} tried to transmogrify using invalid appearance ({}).", player->GetGUID().ToString(), player->GetName(), itemModifiedAppearanceId);
            return false;
        }

        if (isSecondary && itemTransmogrified->GetTemplate()->GetInventoryType() != INVTYPE_SHOULDERS)
        {
            TC_LOG_DEBUG("network", "WORLD: HandleTransmogrifyItems - {}, Name: {} tried to transmogrify secondary appearance to non-shoulder item.", player->GetGUID().ToString(), player->GetName());
            return false;
        }

        auto [hasAppearance, isTemporary] = GetCollectionMgr()->HasItemAppearance(itemModifiedAppearanceId);
        if (!hasAppearance)
        {
            TC_LOG_DEBUG("network", "WORLD: HandleTransmogrifyItems - {}, Name: {} tried to transmogrify using appearance he has not collected ({}).", player->GetGUID().ToString(), player->GetName(), itemModifiedAppearanceId);
            return false;
        }
        ItemTemplate const* itemTemplate = sObjectMgr->GetItemTemplate(itemModifiedAppearance->ItemID);
        if (player->CanUseItem(itemTemplate) != EQUIP_ERR_OK)
        {
            TC_LOG_DEBUG("network", "WORLD: HandleTransmogrifyItems - {}, Name: {} tried to transmogrify using appearance he can never use ({}).", player->GetGUID().ToString(), player->GetName(), itemModifiedAppearanceId);
            return false;
        }

        // validity of the transmogrification items
        if (!Item::CanTransmogrifyItemWithItem(itemTransmogrified, itemModifiedAppearance))
        {
            TC_LOG_DEBUG("network", "WORLD: HandleTransmogrifyItems - {}, Name: {} failed CanTransmogrifyItemWithItem ({} with appearance {}).", player->GetGUID().ToString(), player->GetName(), itemTransmogrified->GetEntry(), itemModifiedAppearanceId);
            return false;
        }

        if (!isSecondary)
            transmogItems[itemTransmogrified].first = itemModifiedAppearanceId;
        else
            transmogItems[itemTransmogrified].second = itemModifiedAppearanceId;

        if (isTemporary)
            bindAppearances.push_back(itemModifiedAppearanceId);

        return true;
    };

    for (WorldPackets::Transmogrification::TransmogrifyItem const& transmogItem : transmogrifyItems.Items)
    {
        // slot of the transmogrified item
        if (transmogItem.Slot >= EQUIPMENT_SLOT_END)
        {
            TC_LOG_DEBUG("network", "WORLD: HandleTransmogrifyItems - Player ({}, name: {}) tried to transmogrify wrong slot ({}) when transmogrifying items.", player->GetGUID().ToString(), player->GetName(), transmogItem.Slot);
            return;
        }

        // transmogrified item
        Item* itemTransmogrified = player->GetItemByPos(INVENTORY_SLOT_BAG_0, transmogItem.Slot);
        if (!itemTransmogrified)
        {
            TC_LOG_DEBUG("network", "WORLD: HandleTransmogrifyItems - Player ({}, name: {}) tried to transmogrify an invalid item in a valid slot (slot: {}).", player->GetGUID().ToString(), player->GetName(), transmogItem.Slot);
            return;
        }

        if (transmogItem.ItemModifiedAppearanceID || transmogItem.SecondaryItemModifiedAppearanceID > 0)
        {
            if (transmogItem.ItemModifiedAppearanceID > 0 && !validateAndStoreTransmogItem(itemTransmogrified, transmogItem.ItemModifiedAppearanceID, false))
                 return;

            if (transmogItem.SecondaryItemModifiedAppearanceID > 0 && !validateAndStoreTransmogItem(itemTransmogrified, transmogItem.SecondaryItemModifiedAppearanceID, true))
                return;

            // add cost
            cost += itemTransmogrified->GetSellPrice(_player);
        }
        else
            resetAppearanceItems.push_back(itemTransmogrified);

        if (transmogItem.SpellItemEnchantmentID)
        {
            if (transmogItem.Slot != EQUIPMENT_SLOT_MAINHAND && transmogItem.Slot != EQUIPMENT_SLOT_OFFHAND)
            {
                TC_LOG_DEBUG("network", "WORLD: HandleTransmogrifyItems - {}, Name: {} tried to transmogrify illusion into non-weapon slot ({}).", player->GetGUID().ToString(), player->GetName(), transmogItem.Slot);
                return;
            }

            TransmogIllusionEntry const* illusion = sDB2Manager.GetTransmogIllusionForEnchantment(transmogItem.SpellItemEnchantmentID);
            if (!illusion)
            {
                TC_LOG_DEBUG("network", "WORLD: HandleTransmogrifyItems - {}, Name: {} tried to transmogrify illusion using invalid enchant ({}).", player->GetGUID().ToString(), player->GetName(), transmogItem.SpellItemEnchantmentID);
                return;
            }

            if (!ConditionMgr::IsPlayerMeetingCondition(player, illusion->UnlockConditionID))
            {
                TC_LOG_DEBUG("network", "WORLD: HandleTransmogrifyItems - {}, Name: {} tried to transmogrify illusion using not allowed enchant ({}).", player->GetGUID().ToString(), player->GetName(), transmogItem.SpellItemEnchantmentID);
                return;
            }

            illusionItems[itemTransmogrified] = transmogItem.SpellItemEnchantmentID;
            cost += illusion->TransmogCost;
        }
        else
            resetIllusionItems.push_back(itemTransmogrified);
    }

    if (!player->HasAuraType(SPELL_AURA_REMOVE_TRANSMOG_COST) && cost) // 0 cost if reverting look
    {
        if (!player->HasEnoughMoney(cost))
            return;
        player->ModifyMoney(-cost);
    }

    // Everything is fine, proceed
    for (auto& transmogPair : transmogItems)
    {
        Item* transmogrified = transmogPair.first;

        if (!transmogrifyItems.CurrentSpecOnly)
        {
            transmogrified->SetModifier(ITEM_MODIFIER_TRANSMOG_APPEARANCE_ALL_SPECS, transmogPair.second.first);
            transmogrified->SetModifier(ITEM_MODIFIER_TRANSMOG_APPEARANCE_SPEC_1, 0);
            transmogrified->SetModifier(ITEM_MODIFIER_TRANSMOG_APPEARANCE_SPEC_2, 0);
            transmogrified->SetModifier(ITEM_MODIFIER_TRANSMOG_APPEARANCE_SPEC_3, 0);
            transmogrified->SetModifier(ITEM_MODIFIER_TRANSMOG_APPEARANCE_SPEC_4, 0);
			transmogrified->SetModifier(ITEM_MODIFIER_TRANSMOG_APPEARANCE_SPEC_5, 0);

            transmogrified->SetModifier(ITEM_MODIFIER_TRANSMOG_SECONDARY_APPEARANCE_ALL_SPECS, transmogPair.second.second);
            transmogrified->SetModifier(ITEM_MODIFIER_TRANSMOG_SECONDARY_APPEARANCE_SPEC_1, 0);
            transmogrified->SetModifier(ITEM_MODIFIER_TRANSMOG_SECONDARY_APPEARANCE_SPEC_2, 0);
            transmogrified->SetModifier(ITEM_MODIFIER_TRANSMOG_SECONDARY_APPEARANCE_SPEC_3, 0);
            transmogrified->SetModifier(ITEM_MODIFIER_TRANSMOG_SECONDARY_APPEARANCE_SPEC_4, 0);
			transmogrified->SetModifier(ITEM_MODIFIER_TRANSMOG_SECONDARY_APPEARANCE_SPEC_5, 0);
        }
        else
        {
            if (!transmogrified->GetModifier(ITEM_MODIFIER_TRANSMOG_APPEARANCE_SPEC_1))
                transmogrified->SetModifier(ITEM_MODIFIER_TRANSMOG_APPEARANCE_SPEC_1, transmogrified->GetModifier(ITEM_MODIFIER_TRANSMOG_APPEARANCE_ALL_SPECS));
            if (!transmogrified->GetModifier(ITEM_MODIFIER_TRANSMOG_APPEARANCE_SPEC_2))
                transmogrified->SetModifier(ITEM_MODIFIER_TRANSMOG_APPEARANCE_SPEC_2, transmogrified->GetModifier(ITEM_MODIFIER_TRANSMOG_APPEARANCE_ALL_SPECS));
            if (!transmogrified->GetModifier(ITEM_MODIFIER_TRANSMOG_APPEARANCE_SPEC_3))
                transmogrified->SetModifier(ITEM_MODIFIER_TRANSMOG_APPEARANCE_SPEC_3, transmogrified->GetModifier(ITEM_MODIFIER_TRANSMOG_APPEARANCE_ALL_SPECS));
            if (!transmogrified->GetModifier(ITEM_MODIFIER_TRANSMOG_APPEARANCE_SPEC_4))
                transmogrified->SetModifier(ITEM_MODIFIER_TRANSMOG_APPEARANCE_SPEC_4, transmogrified->GetModifier(ITEM_MODIFIER_TRANSMOG_APPEARANCE_ALL_SPECS));
			if (!transmogrified->GetModifier(ITEM_MODIFIER_TRANSMOG_APPEARANCE_SPEC_5))
                transmogrified->SetModifier(ITEM_MODIFIER_TRANSMOG_APPEARANCE_SPEC_5, transmogrified->GetModifier(ITEM_MODIFIER_TRANSMOG_APPEARANCE_ALL_SPECS));

            if (!transmogrified->GetModifier(ITEM_MODIFIER_TRANSMOG_SECONDARY_APPEARANCE_SPEC_1))
                transmogrified->SetModifier(ITEM_MODIFIER_TRANSMOG_SECONDARY_APPEARANCE_SPEC_1, transmogrified->GetModifier(ITEM_MODIFIER_TRANSMOG_SECONDARY_APPEARANCE_ALL_SPECS));
            if (!transmogrified->GetModifier(ITEM_MODIFIER_TRANSMOG_SECONDARY_APPEARANCE_SPEC_2))
                transmogrified->SetModifier(ITEM_MODIFIER_TRANSMOG_SECONDARY_APPEARANCE_SPEC_2, transmogrified->GetModifier(ITEM_MODIFIER_TRANSMOG_SECONDARY_APPEARANCE_ALL_SPECS));
            if (!transmogrified->GetModifier(ITEM_MODIFIER_TRANSMOG_SECONDARY_APPEARANCE_SPEC_3))
                transmogrified->SetModifier(ITEM_MODIFIER_TRANSMOG_SECONDARY_APPEARANCE_SPEC_3, transmogrified->GetModifier(ITEM_MODIFIER_TRANSMOG_SECONDARY_APPEARANCE_ALL_SPECS));
            if (!transmogrified->GetModifier(ITEM_MODIFIER_TRANSMOG_SECONDARY_APPEARANCE_SPEC_4))
                transmogrified->SetModifier(ITEM_MODIFIER_TRANSMOG_SECONDARY_APPEARANCE_SPEC_4, transmogrified->GetModifier(ITEM_MODIFIER_TRANSMOG_SECONDARY_APPEARANCE_ALL_SPECS));
			if (!transmogrified->GetModifier(ITEM_MODIFIER_TRANSMOG_SECONDARY_APPEARANCE_SPEC_5))
                transmogrified->SetModifier(ITEM_MODIFIER_TRANSMOG_SECONDARY_APPEARANCE_SPEC_5, transmogrified->GetModifier(ITEM_MODIFIER_TRANSMOG_SECONDARY_APPEARANCE_ALL_SPECS));

            transmogrified->SetModifier(AppearanceModifierSlotBySpec[player->GetActiveTalentGroup()], transmogPair.second.first);
            transmogrified->SetModifier(SecondaryAppearanceModifierSlotBySpec[player->GetActiveTalentGroup()], transmogPair.second.second);
        }

        player->SetVisibleItemSlot(transmogrified->GetSlot(), transmogrified);

        transmogrified->SetNotRefundable(player);
        transmogrified->ClearSoulboundTradeable(player);
        transmogrified->SetState(ITEM_CHANGED, player);
    }

    for (auto& illusionPair : illusionItems)
    {
        Item* transmogrified = illusionPair.first;

        if (!transmogrifyItems.CurrentSpecOnly)
        {
            transmogrified->SetModifier(ITEM_MODIFIER_ENCHANT_ILLUSION_ALL_SPECS, illusionPair.second);
            transmogrified->SetModifier(ITEM_MODIFIER_ENCHANT_ILLUSION_SPEC_1, 0);
            transmogrified->SetModifier(ITEM_MODIFIER_ENCHANT_ILLUSION_SPEC_2, 0);
            transmogrified->SetModifier(ITEM_MODIFIER_ENCHANT_ILLUSION_SPEC_3, 0);
            transmogrified->SetModifier(ITEM_MODIFIER_ENCHANT_ILLUSION_SPEC_4, 0);
			transmogrified->SetModifier(ITEM_MODIFIER_ENCHANT_ILLUSION_SPEC_5, 0);
        }
        else
        {
            if (!transmogrified->GetModifier(ITEM_MODIFIER_ENCHANT_ILLUSION_SPEC_1))
                transmogrified->SetModifier(ITEM_MODIFIER_ENCHANT_ILLUSION_SPEC_1, transmogrified->GetModifier(ITEM_MODIFIER_ENCHANT_ILLUSION_ALL_SPECS));
            if (!transmogrified->GetModifier(ITEM_MODIFIER_ENCHANT_ILLUSION_SPEC_2))
                transmogrified->SetModifier(ITEM_MODIFIER_ENCHANT_ILLUSION_SPEC_2, transmogrified->GetModifier(ITEM_MODIFIER_ENCHANT_ILLUSION_ALL_SPECS));
            if (!transmogrified->GetModifier(ITEM_MODIFIER_ENCHANT_ILLUSION_SPEC_3))
                transmogrified->SetModifier(ITEM_MODIFIER_ENCHANT_ILLUSION_SPEC_3, transmogrified->GetModifier(ITEM_MODIFIER_ENCHANT_ILLUSION_ALL_SPECS));
            if (!transmogrified->GetModifier(ITEM_MODIFIER_ENCHANT_ILLUSION_SPEC_4))
                transmogrified->SetModifier(ITEM_MODIFIER_ENCHANT_ILLUSION_SPEC_4, transmogrified->GetModifier(ITEM_MODIFIER_ENCHANT_ILLUSION_ALL_SPECS));
			if (!transmogrified->GetModifier(ITEM_MODIFIER_ENCHANT_ILLUSION_SPEC_5))
                transmogrified->SetModifier(ITEM_MODIFIER_ENCHANT_ILLUSION_SPEC_5, transmogrified->GetModifier(ITEM_MODIFIER_ENCHANT_ILLUSION_ALL_SPECS));
            transmogrified->SetModifier(IllusionModifierSlotBySpec[player->GetActiveTalentGroup()], illusionPair.second);
        }

        player->SetVisibleItemSlot(transmogrified->GetSlot(), transmogrified);

        transmogrified->SetNotRefundable(player);
        transmogrified->ClearSoulboundTradeable(player);
        transmogrified->SetState(ITEM_CHANGED, player);
    }

    for (Item* item : resetAppearanceItems)
    {
        if (!transmogrifyItems.CurrentSpecOnly)
        {
            item->SetModifier(ITEM_MODIFIER_TRANSMOG_APPEARANCE_ALL_SPECS, 0);
            item->SetModifier(ITEM_MODIFIER_TRANSMOG_APPEARANCE_SPEC_1, 0);
            item->SetModifier(ITEM_MODIFIER_TRANSMOG_APPEARANCE_SPEC_2, 0);
            item->SetModifier(ITEM_MODIFIER_TRANSMOG_APPEARANCE_SPEC_3, 0);
            item->SetModifier(ITEM_MODIFIER_TRANSMOG_APPEARANCE_SPEC_4, 0);
			item->SetModifier(ITEM_MODIFIER_TRANSMOG_APPEARANCE_SPEC_5, 0);

            item->SetModifier(ITEM_MODIFIER_TRANSMOG_SECONDARY_APPEARANCE_ALL_SPECS, 0);
            item->SetModifier(ITEM_MODIFIER_TRANSMOG_SECONDARY_APPEARANCE_SPEC_1, 0);
            item->SetModifier(ITEM_MODIFIER_TRANSMOG_SECONDARY_APPEARANCE_SPEC_2, 0);
            item->SetModifier(ITEM_MODIFIER_TRANSMOG_SECONDARY_APPEARANCE_SPEC_3, 0);
            item->SetModifier(ITEM_MODIFIER_TRANSMOG_SECONDARY_APPEARANCE_SPEC_4, 0);
			item->SetModifier(ITEM_MODIFIER_TRANSMOG_SECONDARY_APPEARANCE_SPEC_5, 0);
        }
        else
        {
            if (!item->GetModifier(ITEM_MODIFIER_TRANSMOG_APPEARANCE_SPEC_1))
                item->SetModifier(ITEM_MODIFIER_TRANSMOG_APPEARANCE_SPEC_1, item->GetModifier(ITEM_MODIFIER_TRANSMOG_APPEARANCE_ALL_SPECS));
            if (!item->GetModifier(ITEM_MODIFIER_TRANSMOG_APPEARANCE_SPEC_2))
                item->SetModifier(ITEM_MODIFIER_TRANSMOG_APPEARANCE_SPEC_2, item->GetModifier(ITEM_MODIFIER_TRANSMOG_APPEARANCE_ALL_SPECS));
            if (!item->GetModifier(ITEM_MODIFIER_TRANSMOG_APPEARANCE_SPEC_3))
                item->SetModifier(ITEM_MODIFIER_TRANSMOG_APPEARANCE_SPEC_3, item->GetModifier(ITEM_MODIFIER_TRANSMOG_APPEARANCE_ALL_SPECS));
            if (!item->GetModifier(ITEM_MODIFIER_TRANSMOG_APPEARANCE_SPEC_4))
                item->SetModifier(ITEM_MODIFIER_TRANSMOG_APPEARANCE_SPEC_4, item->GetModifier(ITEM_MODIFIER_TRANSMOG_APPEARANCE_ALL_SPECS));
			if (!item->GetModifier(ITEM_MODIFIER_TRANSMOG_APPEARANCE_SPEC_5))
                item->SetModifier(ITEM_MODIFIER_TRANSMOG_APPEARANCE_SPEC_5, item->GetModifier(ITEM_MODIFIER_TRANSMOG_APPEARANCE_ALL_SPECS));

            if (!item->GetModifier(ITEM_MODIFIER_TRANSMOG_SECONDARY_APPEARANCE_SPEC_1))
                item->SetModifier(ITEM_MODIFIER_TRANSMOG_SECONDARY_APPEARANCE_SPEC_1, item->GetModifier(ITEM_MODIFIER_TRANSMOG_SECONDARY_APPEARANCE_ALL_SPECS));
            if (!item->GetModifier(ITEM_MODIFIER_TRANSMOG_SECONDARY_APPEARANCE_SPEC_2))
                item->SetModifier(ITEM_MODIFIER_TRANSMOG_SECONDARY_APPEARANCE_SPEC_2, item->GetModifier(ITEM_MODIFIER_TRANSMOG_SECONDARY_APPEARANCE_ALL_SPECS));
            if (!item->GetModifier(ITEM_MODIFIER_TRANSMOG_SECONDARY_APPEARANCE_SPEC_3))
                item->SetModifier(ITEM_MODIFIER_TRANSMOG_SECONDARY_APPEARANCE_SPEC_3, item->GetModifier(ITEM_MODIFIER_TRANSMOG_SECONDARY_APPEARANCE_ALL_SPECS));
            if (!item->GetModifier(ITEM_MODIFIER_TRANSMOG_SECONDARY_APPEARANCE_SPEC_4))
                item->SetModifier(ITEM_MODIFIER_TRANSMOG_SECONDARY_APPEARANCE_SPEC_4, item->GetModifier(ITEM_MODIFIER_TRANSMOG_SECONDARY_APPEARANCE_ALL_SPECS));
			if (!item->GetModifier(ITEM_MODIFIER_TRANSMOG_SECONDARY_APPEARANCE_SPEC_5))
                item->SetModifier(ITEM_MODIFIER_TRANSMOG_SECONDARY_APPEARANCE_SPEC_5, item->GetModifier(ITEM_MODIFIER_TRANSMOG_SECONDARY_APPEARANCE_ALL_SPECS));

            item->SetModifier(AppearanceModifierSlotBySpec[player->GetActiveTalentGroup()], 0);
            item->SetModifier(SecondaryAppearanceModifierSlotBySpec[player->GetActiveTalentGroup()], 0);
            item->SetModifier(ITEM_MODIFIER_ENCHANT_ILLUSION_ALL_SPECS, 0);
        }

        item->SetState(ITEM_CHANGED, player);
        player->SetVisibleItemSlot(item->GetSlot(), item);
    }

    for (Item* item : resetIllusionItems)
    {
        if (!transmogrifyItems.CurrentSpecOnly)
        {
            item->SetModifier(ITEM_MODIFIER_ENCHANT_ILLUSION_ALL_SPECS, 0);
            item->SetModifier(ITEM_MODIFIER_ENCHANT_ILLUSION_SPEC_1, 0);
            item->SetModifier(ITEM_MODIFIER_ENCHANT_ILLUSION_SPEC_2, 0);
            item->SetModifier(ITEM_MODIFIER_ENCHANT_ILLUSION_SPEC_3, 0);
            item->SetModifier(ITEM_MODIFIER_ENCHANT_ILLUSION_SPEC_4, 0);
			item->SetModifier(ITEM_MODIFIER_ENCHANT_ILLUSION_SPEC_5, 0);
        }
        else
        {
            if (!item->GetModifier(ITEM_MODIFIER_ENCHANT_ILLUSION_SPEC_1))
                item->SetModifier(ITEM_MODIFIER_ENCHANT_ILLUSION_SPEC_1, item->GetModifier(ITEM_MODIFIER_ENCHANT_ILLUSION_ALL_SPECS));
            if (!item->GetModifier(ITEM_MODIFIER_ENCHANT_ILLUSION_SPEC_2))
                item->SetModifier(ITEM_MODIFIER_ENCHANT_ILLUSION_SPEC_2, item->GetModifier(ITEM_MODIFIER_ENCHANT_ILLUSION_ALL_SPECS));
            if (!item->GetModifier(ITEM_MODIFIER_ENCHANT_ILLUSION_SPEC_3))
                item->SetModifier(ITEM_MODIFIER_ENCHANT_ILLUSION_SPEC_3, item->GetModifier(ITEM_MODIFIER_ENCHANT_ILLUSION_ALL_SPECS));
            if (!item->GetModifier(ITEM_MODIFIER_ENCHANT_ILLUSION_SPEC_4))
                item->SetModifier(ITEM_MODIFIER_ENCHANT_ILLUSION_SPEC_4, item->GetModifier(ITEM_MODIFIER_ENCHANT_ILLUSION_ALL_SPECS));
			if (!item->GetModifier(ITEM_MODIFIER_ENCHANT_ILLUSION_SPEC_5))
                item->SetModifier(ITEM_MODIFIER_ENCHANT_ILLUSION_SPEC_5, item->GetModifier(ITEM_MODIFIER_ENCHANT_ILLUSION_ALL_SPECS));

            item->SetModifier(IllusionModifierSlotBySpec[player->GetActiveTalentGroup()], 0);
            item->SetModifier(ITEM_MODIFIER_TRANSMOG_APPEARANCE_ALL_SPECS, 0);
        }

        item->SetState(ITEM_CHANGED, player);
        player->SetVisibleItemSlot(item->GetSlot(), item);
    }

    for (uint32 itemModifedAppearanceId : bindAppearances)
    {
        std::unordered_set<ObjectGuid> itemsProvidingAppearance = GetCollectionMgr()->GetItemsProvidingTemporaryAppearance(itemModifedAppearanceId);
        for (ObjectGuid const& itemGuid : itemsProvidingAppearance)
        {
            if (Item* item = player->GetItemByGuid(itemGuid))
            {
                item->SetNotRefundable(player);
                item->ClearSoulboundTradeable(player);
                GetCollectionMgr()->AddItemAppearance(item);
            }
        }
    }
}


void WorldSession::HandleTransmogOutfitNew(WorldPackets::Transmogrification::TransmogOutfitNew& transmogOutfitNew)
{
    LogTransmogOutfitOpcodeDebug(this, "CMSG_TRANSMOG_OUTFIT_NEW", transmogOutfitNew.PayloadSize, transmogOutfitNew.PayloadPreviewHex);

    if (!transmogOutfitNew.ParseSuccess)
    {
        TC_LOG_ERROR("network.opcode.transmog", "CMSG_TRANSMOG_OUTFIT_NEW parse failed [{}]: {}", GetPlayerInfo(), transmogOutfitNew.ParseError);
<<<<<<< ours
        return;
    }

    EquipmentSetInfo::EquipmentSetData set = transmogOutfitNew.Set;
=======
        TC_LOG_DEBUG("network.opcode.transmog", "{}", transmogOutfitNew.DiagnosticReadTrace);
        return;
    }

    if (!ValidateTransmogOutfitPlayerGuid(this, transmogOutfitNew.PlayerGuid, "CMSG_TRANSMOG_OUTFIT_NEW"))
        return;

    EquipmentSetInfo::EquipmentSetData set = transmogOutfitNew.Set;
    set.SetID = FindNextAvailableTransmogSetID(GetPlayer());
    if (set.SetID >= MAX_EQUIPMENT_SET_INDEX)
    {
        TC_LOG_ERROR("network.opcode.transmog", "CMSG_TRANSMOG_OUTFIT_NEW rejected [{}]: no free transmog outfit slots", GetPlayerInfo());
        return;
    }

>>>>>>> theirs
    if (!ValidateTransmogOutfitSet(this, set))
        return;

    GetPlayer()->SetEquipmentSet(set);
<<<<<<< ours
=======

    if (EquipmentSetInfo::EquipmentSetData const* savedSet = GetPlayer()->GetTransmogOutfitBySetID(set.SetID))
        set.Guid = savedSet->Guid;

    WorldPackets::Transmogrification::TransmogOutfitNewEntryAdded response;
    response.SetID = set.SetID;
    response.Guid = set.Guid;
    SendPacket(response.Write());
>>>>>>> theirs
}

void WorldSession::HandleTransmogOutfitUpdateInfo(WorldPackets::Transmogrification::TransmogOutfitUpdateInfo& transmogOutfitUpdateInfo)
{
    LogTransmogOutfitOpcodeDebug(this, "CMSG_TRANSMOG_OUTFIT_UPDATE_INFO", transmogOutfitUpdateInfo.PayloadSize, transmogOutfitUpdateInfo.PayloadPreviewHex);

    if (!transmogOutfitUpdateInfo.ParseSuccess)
    {
        TC_LOG_ERROR("network.opcode.transmog", "CMSG_TRANSMOG_OUTFIT_UPDATE_INFO parse failed [{}]: {}", GetPlayerInfo(), transmogOutfitUpdateInfo.ParseError);
<<<<<<< ours
        return;
    }

    EquipmentSetInfo::EquipmentSetData const* existingSet = GetPlayer()->GetEquipmentSetData(transmogOutfitUpdateInfo.Set.Guid);
    if (!existingSet || existingSet->Type != EquipmentSetInfo::TRANSMOG)
    {
        TC_LOG_ERROR("network.opcode.transmog", "CMSG_TRANSMOG_OUTFIT_UPDATE_INFO rejected [{}]: unknown transmog set guid {}", GetPlayerInfo(), transmogOutfitUpdateInfo.Set.Guid);
=======
        TC_LOG_DEBUG("network.opcode.transmog", "{}", transmogOutfitUpdateInfo.DiagnosticReadTrace);
        return;
    }

    if (!ValidateTransmogOutfitPlayerGuid(this, transmogOutfitUpdateInfo.PlayerGuid, "CMSG_TRANSMOG_OUTFIT_UPDATE_INFO"))
        return;

    EquipmentSetInfo::EquipmentSetData const* existingSet = GetPlayer()->GetTransmogOutfitBySetID(transmogOutfitUpdateInfo.Set.SetID);
    if (!existingSet || existingSet->Type != EquipmentSetInfo::TRANSMOG)
    {
        TC_LOG_ERROR("network.opcode.transmog", "CMSG_TRANSMOG_OUTFIT_UPDATE_INFO rejected [{}]: unknown transmog set id {}", GetPlayerInfo(), transmogOutfitUpdateInfo.Set.SetID);
>>>>>>> theirs
        return;
    }

    EquipmentSetInfo::EquipmentSetData updatedSet = *existingSet;
    updatedSet.SetID = transmogOutfitUpdateInfo.Set.SetID;
    updatedSet.SetName = transmogOutfitUpdateInfo.Set.SetName;
    updatedSet.SetIcon = transmogOutfitUpdateInfo.Set.SetIcon;
    updatedSet.IgnoreMask = transmogOutfitUpdateInfo.Set.IgnoreMask;

    if (!ValidateTransmogOutfitSet(this, updatedSet))
        return;

    GetPlayer()->SetEquipmentSet(updatedSet);
<<<<<<< ours
=======

    WorldPackets::Transmogrification::TransmogOutfitInfoUpdated response;
    response.SetID = updatedSet.SetID;
    response.Guid = updatedSet.Guid;
    SendPacket(response.Write());
>>>>>>> theirs
}

void WorldSession::HandleTransmogOutfitUpdateSlots(WorldPackets::Transmogrification::TransmogOutfitUpdateSlots& transmogOutfitUpdateSlots)
{
    LogTransmogOutfitOpcodeDebug(this, "CMSG_TRANSMOG_OUTFIT_UPDATE_SLOTS", transmogOutfitUpdateSlots.PayloadSize, transmogOutfitUpdateSlots.PayloadPreviewHex);

    if (!transmogOutfitUpdateSlots.ParseSuccess)
    {
        TC_LOG_ERROR("network.opcode.transmog", "CMSG_TRANSMOG_OUTFIT_UPDATE_SLOTS parse failed [{}]: {}", GetPlayerInfo(), transmogOutfitUpdateSlots.ParseError);
<<<<<<< ours
        return;
    }

    EquipmentSetInfo::EquipmentSetData const* existingSet = GetPlayer()->GetEquipmentSetData(transmogOutfitUpdateSlots.Set.Guid);
    if (!existingSet || existingSet->Type != EquipmentSetInfo::TRANSMOG)
    {
        TC_LOG_ERROR("network.opcode.transmog", "CMSG_TRANSMOG_OUTFIT_UPDATE_SLOTS rejected [{}]: unknown transmog set guid {}", GetPlayerInfo(), transmogOutfitUpdateSlots.Set.Guid);
=======
        TC_LOG_DEBUG("network.opcode.transmog", "{}", transmogOutfitUpdateSlots.DiagnosticReadTrace);
        return;
    }

    if (!ValidateTransmogOutfitPlayerGuid(this, transmogOutfitUpdateSlots.PlayerGuid, "CMSG_TRANSMOG_OUTFIT_UPDATE_SLOTS"))
        return;

    EquipmentSetInfo::EquipmentSetData const* existingSet = GetPlayer()->GetTransmogOutfitBySetID(transmogOutfitUpdateSlots.Set.SetID);
    if (!existingSet || existingSet->Type != EquipmentSetInfo::TRANSMOG)
    {
        TC_LOG_ERROR("network.opcode.transmog", "CMSG_TRANSMOG_OUTFIT_UPDATE_SLOTS rejected [{}]: unknown transmog set id {}", GetPlayerInfo(), transmogOutfitUpdateSlots.Set.SetID);
>>>>>>> theirs
        return;
    }

    EquipmentSetInfo::EquipmentSetData updatedSet = *existingSet;
    updatedSet.SetID = transmogOutfitUpdateSlots.Set.SetID;
    updatedSet.IgnoreMask = transmogOutfitUpdateSlots.Set.IgnoreMask;
    updatedSet.Appearances = transmogOutfitUpdateSlots.Set.Appearances;
<<<<<<< ours
    updatedSet.Enchants = transmogOutfitUpdateSlots.Set.Enchants;
=======
>>>>>>> theirs

    if (!ValidateTransmogOutfitSet(this, updatedSet))
        return;

    GetPlayer()->SetEquipmentSet(updatedSet);
<<<<<<< ours
=======

    WorldPackets::Transmogrification::TransmogOutfitSlotsUpdated response;
    response.SetID = updatedSet.SetID;
    response.Guid = updatedSet.Guid;
    SendPacket(response.Write());
>>>>>>> theirs
}

void WorldSession::HandleTransmogOutfitUpdateSituations(WorldPackets::Transmogrification::TransmogOutfitUpdateSituations& transmogOutfitUpdateSituations)
{
    LogTransmogOutfitOpcodeDebug(this, "CMSG_TRANSMOG_OUTFIT_UPDATE_SITUATIONS", transmogOutfitUpdateSituations.PayloadSize, transmogOutfitUpdateSituations.PayloadPreviewHex);

    if (!transmogOutfitUpdateSituations.ParseSuccess)
    {
        TC_LOG_ERROR("network.opcode.transmog", "CMSG_TRANSMOG_OUTFIT_UPDATE_SITUATIONS parse failed [{}]: {}", GetPlayerInfo(), transmogOutfitUpdateSituations.ParseError);
<<<<<<< ours
        return;
    }

    EquipmentSetInfo::EquipmentSetData const* existingSet = GetPlayer()->GetEquipmentSetData(transmogOutfitUpdateSituations.Guid);
    if (!existingSet || existingSet->Type != EquipmentSetInfo::TRANSMOG)
    {
        TC_LOG_ERROR("network.opcode.transmog", "CMSG_TRANSMOG_OUTFIT_UPDATE_SITUATIONS rejected [{}]: unknown transmog set guid {}", GetPlayerInfo(), transmogOutfitUpdateSituations.Guid);
        return;
    }

    TC_LOG_DEBUG("network.opcode.transmog", "CMSG_TRANSMOG_OUTFIT_UPDATE_SITUATIONS [{}]: setId={} situations={} (stored as no-op; TODO implement situation persistence/acks)",
        GetPlayerInfo(), transmogOutfitUpdateSituations.SetID, transmogOutfitUpdateSituations.Situations.size());

    GetPlayer()->SetEquipmentSet(*existingSet);
=======
        TC_LOG_DEBUG("network.opcode.transmog", "{}", transmogOutfitUpdateSituations.DiagnosticReadTrace);
        return;
    }

    if (!ValidateTransmogOutfitPlayerGuid(this, transmogOutfitUpdateSituations.PlayerGuid, "CMSG_TRANSMOG_OUTFIT_UPDATE_SITUATIONS"))
        return;

    EquipmentSetInfo::EquipmentSetData const* existingSet = GetPlayer()->GetTransmogOutfitBySetID(transmogOutfitUpdateSituations.SetID);
    if (!existingSet || existingSet->Type != EquipmentSetInfo::TRANSMOG)
    {
        TC_LOG_ERROR("network.opcode.transmog", "CMSG_TRANSMOG_OUTFIT_UPDATE_SITUATIONS rejected [{}]: unknown transmog set id {}", GetPlayerInfo(), transmogOutfitUpdateSituations.SetID);
        return;
    }

    TC_LOG_DEBUG("network.opcode.transmog", "CMSG_TRANSMOG_OUTFIT_UPDATE_SITUATIONS [{}]: setId={} situations={} (stored as no-op; TODO implement situation persistence)",
        GetPlayerInfo(), transmogOutfitUpdateSituations.SetID, transmogOutfitUpdateSituations.Situations.size());

    for (WorldPackets::Transmogrification::TransmogOutfitSituationEntry const& situation : transmogOutfitUpdateSituations.Situations)
    {
        TC_LOG_DEBUG("network.opcode.transmog", "  situation={} spec={} loadout={} equipmentSet={}",
            situation.SituationID, situation.SpecID, situation.LoadoutID, situation.EquipmentSetID);
    }

    WorldPackets::Transmogrification::TransmogOutfitSituationsUpdated response;
    response.SetID = transmogOutfitUpdateSituations.SetID;
    response.Guid = existingSet->Guid;
    SendPacket(response.Write());
>>>>>>> theirs
}

void WorldSession::SendOpenTransmogrifier(ObjectGuid const& guid)
{
    WorldPackets::NPC::NPCInteractionOpenResult npcInteraction;
    npcInteraction.Npc = guid;
    npcInteraction.InteractionType = PlayerInteractionType::Transmogrifier;
    npcInteraction.Success = true;
    SendPacket(npcInteraction.Write());
}
