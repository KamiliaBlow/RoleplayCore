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
#include "TransmogrificationUtils.h"


namespace
{
void LogTransmogOutfitOpcodeDebug(WorldSession const* session, char const* opcodeName, size_t payloadSize, std::string const& payloadPreviewHex)
{
    TC_LOG_DEBUG("network.opcode.transmog", "{} [{}] size={} preview(<=128B)={}", opcodeName, session->GetPlayerInfo(), payloadSize, payloadPreviewHex);
}

bool ValidateTransmogOutfitNpc(WorldSession* session, ObjectGuid const& npcGuid, char const* opcodeName)
{
    // The client sends the transmogrifier NPC GUID, just like CMSG_TRANSMOGRIFY_ITEMS.
    // Validate the player can interact with this NPC.
    if (!session->GetPlayer()->GetNPCIfCanInteractWith(npcGuid, UNIT_NPC_FLAG_TRANSMOGRIFIER, UNIT_NPC_FLAG_2_NONE))
    {
        TC_LOG_DEBUG("network.opcode.transmog", "{} rejected [{}]: NPC {} not found or player can't interact with it",
            opcodeName, session->GetPlayerInfo(), npcGuid.ToString());
        return false;
    }
    return true;
}

uint32 FindNextAvailableTransmogSetID(Player const* player)
{
    // Start at 1: the client treats SetID=0 as "no outfit" in TransmogOutfitMetadata
    for (uint32 setId = 1; setId < MAX_EQUIPMENT_SET_INDEX; ++setId)
        if (!player->GetTransmogOutfitBySetID(setId))
            return setId;

    return MAX_EQUIPMENT_SET_INDEX;
}

bool ValidateTransmogOutfitSet(WorldSession* session, EquipmentSetInfo::EquipmentSetData& set)
{
    // Client treats SetID=0 as "no outfit" in TransmogOutfitMetadata.TransmogOutfitID
    if (set.SetID == 0 || set.SetID >= MAX_EQUIPMENT_SET_INDEX)
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

    if (set.SecondaryShoulderApparanceID)
    {
        if (!sItemModifiedAppearanceStore.LookupEntry(set.SecondaryShoulderApparanceID))
        {
            TC_LOG_ERROR("network.opcode.transmog", "Transmog outfit rejected [{}]: invalid secondary shoulder appearance {}",
                session->GetPlayerInfo(), set.SecondaryShoulderApparanceID);
            return false;
        }

        if (!session->GetCollectionMgr()->HasItemAppearance(set.SecondaryShoulderApparanceID).first)
        {
            TC_LOG_ERROR("network.opcode.transmog", "Transmog outfit rejected [{}]: uncollected secondary shoulder appearance {}",
                session->GetPlayerInfo(), set.SecondaryShoulderApparanceID);
            return false;
        }

        set.SecondaryShoulderSlot = 2;
    }
    else
        set.SecondaryShoulderSlot = 0;

    set.IgnoreMask &= 0x7FFFF;

    auto validateIllusion = [session](uint32 enchantId) -> bool
    {
        SpellItemEnchantmentEntry const* illusion = sSpellItemEnchantmentStore.LookupEntry(enchantId);
        if (!illusion)
            return false;

        // Check via TransmogIllusion DB2 first (authoritative for transmog enchants).
        // Some enchantments have Flags=0 in SpellItemEnchantment but are valid transmog
        // illusions via the TransmogIllusion table.
        TransmogIllusionEntry const* transmogIllusion = sDB2Manager.GetTransmogIllusionForEnchantment(enchantId);
        if (transmogIllusion)
        {
            if (!ConditionMgr::IsPlayerMeetingCondition(session->GetPlayer(), transmogIllusion->UnlockConditionID))
                return false;

            return true;
        }

        // Fallback: direct SpellItemEnchantment validation
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

    TC_LOG_DEBUG("network.opcode.transmog", "HandleTransmogrifyItems [{}]: SINGLE-ITEM transmog fired ({} items in packet)",
        GetPlayerInfo(), transmogrifyItems.Items.size());

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

    // Sync individual transmog changes back into the active transmog outfit.
    // HandleTransmogrifyItems modifies item modifiers (which updates the game-world avatar
    // via SetVisibleItemSlot), but the Transmog UI reads from ViewedOutfit update fields.
    // Without this sync, HEAD/MH/OH transmogs done individually won't appear in the UI avatar.
    uint32 activeOutfitID = player->GetActiveTransmogOutfitID();
    if (activeOutfitID)
    {
        if (EquipmentSetInfo::EquipmentSetData* activeOutfit = player->GetMutableTransmogOutfitBySetID(activeOutfitID))
        {
            bool outfitChanged = false;

            for (auto& [item, appearancePair] : transmogItems)
            {
                uint8 slot = item->GetSlot();
                if (slot < EQUIPMENT_SLOT_END && appearancePair.first)
                {
                    activeOutfit->Appearances[slot] = int32(appearancePair.first);
                    activeOutfit->IgnoreMask &= ~(1u << slot);
                    outfitChanged = true;
                    TC_LOG_DEBUG("network.opcode.transmog", "HandleTransmogrifyItems [{}]: syncing IMAID {} to outfit {} equipSlot={}",
                        GetPlayerInfo(), appearancePair.first, activeOutfitID, slot);
                }

                // Sync secondary shoulder appearance
                if (slot == EQUIPMENT_SLOT_SHOULDERS && appearancePair.second)
                {
                    activeOutfit->SecondaryShoulderApparanceID = int32(appearancePair.second);
                    activeOutfit->SecondaryShoulderSlot = EQUIPMENT_SLOT_SHOULDERS;
                    outfitChanged = true;
                    TC_LOG_DEBUG("network.opcode.transmog", "HandleTransmogrifyItems [{}]: syncing secondary shoulder IMAID {} to outfit {}",
                        GetPlayerInfo(), appearancePair.second, activeOutfitID);
                }
            }

            // Also sync enchant illusions for weapons
            for (auto& [item, enchantId] : illusionItems)
            {
                uint8 slot = item->GetSlot();
                if (slot == EQUIPMENT_SLOT_MAINHAND)
                {
                    activeOutfit->Enchants[0] = int32(enchantId);
                    outfitChanged = true;
                }
                else if (slot == EQUIPMENT_SLOT_OFFHAND)
                {
                    activeOutfit->Enchants[1] = int32(enchantId);
                    outfitChanged = true;
                }
            }

            // Sync reset illusions (illusion removed from weapon)
            for (Item* item : resetIllusionItems)
            {
                uint8 slot = item->GetSlot();
                if (slot == EQUIPMENT_SLOT_MAINHAND && activeOutfit->Enchants[0])
                {
                    activeOutfit->Enchants[0] = 0;
                    outfitChanged = true;
                }
                else if (slot == EQUIPMENT_SLOT_OFFHAND && activeOutfit->Enchants[1])
                {
                    activeOutfit->Enchants[1] = 0;
                    outfitChanged = true;
                }
            }

            // Sync reset appearances (transmog removed from slot)
            for (Item* item : resetAppearanceItems)
            {
                uint8 slot = item->GetSlot();
                if (slot < EQUIPMENT_SLOT_END && activeOutfit->Appearances[slot])
                {
                    activeOutfit->Appearances[slot] = 0;
                    activeOutfit->IgnoreMask |= (1u << slot);
                    outfitChanged = true;
                }

                // Clear secondary shoulder when shoulder appearance is reset
                if (slot == EQUIPMENT_SLOT_SHOULDERS && activeOutfit->SecondaryShoulderApparanceID)
                {
                    activeOutfit->SecondaryShoulderApparanceID = 0;
                    activeOutfit->SecondaryShoulderSlot = 0;
                    outfitChanged = true;
                }
            }

            if (outfitChanged)
            {
                TC_LOG_DEBUG("network.opcode.transmog",
                    "HandleTransmogrifyItems [{}]: syncing outfit {} to SetEquipmentSet — IgnoreMask=0x{:X} (THIS TRIGGERS FULL ViewedOutfit REBUILD)",
                    GetPlayerInfo(), activeOutfitID, activeOutfit->IgnoreMask);
                for (uint8 s = 0; s < EQUIPMENT_SLOT_END; ++s)
                    TC_LOG_DEBUG("network.opcode.transmog", "  single-item sync: equipSlot={} Appearances={} ignored={}",
                        s, activeOutfit->Appearances[s], (activeOutfit->IgnoreMask & (1u << s)) != 0);

                player->SetEquipmentSet(*activeOutfit); // persists + re-syncs update fields
                player->SendUpdateToPlayer(player);
                player->ClearUpdateMask(true);
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
        TC_LOG_DEBUG("network.opcode.transmog", "{}", transmogOutfitNew.DiagnosticReadTrace);
        return;
    }

    if (!ValidateTransmogOutfitNpc(this, transmogOutfitNew.Npc, "CMSG_TRANSMOG_OUTFIT_NEW"))
        return;

    EquipmentSetInfo::EquipmentSetData set = transmogOutfitNew.Set;
    set.SetID = FindNextAvailableTransmogSetID(GetPlayer());
    if (set.SetID >= MAX_EQUIPMENT_SET_INDEX)
    {
        TC_LOG_ERROR("network.opcode.transmog", "CMSG_TRANSMOG_OUTFIT_NEW rejected [{}]: no free transmog outfit slots", GetPlayerInfo());
        return;
    }

    // Fill missing slots from player's equipped item transmog modifiers.
    // The 12.x client omits HEAD, BACK, TABARD, MH, and OH from outfit packets.
    Player* player = GetPlayer();
    for (uint8 slot : {EQUIPMENT_SLOT_HEAD, EQUIPMENT_SLOT_BACK, EQUIPMENT_SLOT_TABARD,
                        EQUIPMENT_SLOT_MAINHAND, EQUIPMENT_SLOT_OFFHAND})
    {
        if (set.Appearances[slot] == 0)
        {
            if (Item* item = player->GetItemByPos(INVENTORY_SLOT_BAG_0, slot))
            {
                uint32 transmogId = item->GetModifier(ITEM_MODIFIER_TRANSMOG_APPEARANCE_ALL_SPECS);
                if (transmogId)
                {
                    set.Appearances[slot] = int32(transmogId);
                    set.IgnoreMask &= ~(1u << slot);
                    TC_LOG_DEBUG("network.opcode.transmog", "CMSG_TRANSMOG_OUTFIT_NEW [{}]: filled missing slot {} from equipped item IMAID {}",
                        GetPlayerInfo(), slot, transmogId);
                }
            }
        }
    }

    if (!ValidateTransmogOutfitSet(this, set))
        return;

    GetPlayer()->SetEquipmentSet(set);

    if (EquipmentSetInfo::EquipmentSetData const* savedSet = GetPlayer()->GetTransmogOutfitBySetID(set.SetID))
        set.Guid = savedSet->Guid;

    // Force-flush pending UpdateField changes (TransmogOutfits) to the client BEFORE
    // sending the response packet. SetEquipmentSet -> _SyncTransmogOutfitsToActivePlayerData
    // modifies UpdateFields but they're batched until the next SMSG_UPDATE_OBJECT at tick end.
    // Without this flush, the client receives SMSG_TRANSMOG_OUTFIT_NEW_ENTRY_ADDED first,
    // fires TRANSMOG_OUTFITS_CHANGED, and GetOutfitsInfo() returns stale data — so the
    // outfit list doesn't update until the transmog UI is reopened.
    player->SendUpdateToPlayer(player);
    player->ClearUpdateMask(true);

    WorldPackets::Transmogrification::TransmogOutfitNewEntryAdded response;
    response.SetID = set.SetID;
    response.Guid = set.Guid;
    TC_LOG_DEBUG("network.opcode.transmog", "SMSG_TRANSMOG_OUTFIT_NEW_ENTRY_ADDED [{}]: setId={} guid={}", GetPlayerInfo(), response.SetID, response.Guid);
    SendPacket(response.Write());
}

void WorldSession::HandleTransmogOutfitUpdateInfo(WorldPackets::Transmogrification::TransmogOutfitUpdateInfo& transmogOutfitUpdateInfo)
{
    LogTransmogOutfitOpcodeDebug(this, "CMSG_TRANSMOG_OUTFIT_UPDATE_INFO", transmogOutfitUpdateInfo.PayloadSize, transmogOutfitUpdateInfo.PayloadPreviewHex);

    if (!transmogOutfitUpdateInfo.ParseSuccess)
    {
        TC_LOG_ERROR("network.opcode.transmog", "CMSG_TRANSMOG_OUTFIT_UPDATE_INFO parse failed [{}]: {}", GetPlayerInfo(), transmogOutfitUpdateInfo.ParseError);
        TC_LOG_DEBUG("network.opcode.transmog", "{}", transmogOutfitUpdateInfo.DiagnosticReadTrace);
        return;
    }

    if (!ValidateTransmogOutfitNpc(this, transmogOutfitUpdateInfo.Npc, "CMSG_TRANSMOG_OUTFIT_UPDATE_INFO"))
        return;

    EquipmentSetInfo::EquipmentSetData const* existingSet = GetPlayer()->GetTransmogOutfitBySetID(transmogOutfitUpdateInfo.Set.SetID);
    if (!existingSet || existingSet->Type != EquipmentSetInfo::TRANSMOG)
    {
        TC_LOG_ERROR("network.opcode.transmog", "CMSG_TRANSMOG_OUTFIT_UPDATE_INFO rejected [{}]: unknown transmog set id {}", GetPlayerInfo(), transmogOutfitUpdateInfo.Set.SetID);
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

    // Flush UpdateField changes before response (see HandleTransmogOutfitNew comment)
    GetPlayer()->SendUpdateToPlayer(GetPlayer());
    GetPlayer()->ClearUpdateMask(true);

    WorldPackets::Transmogrification::TransmogOutfitInfoUpdated response;
    response.SetID = updatedSet.SetID;
    response.Guid = updatedSet.Guid;
    TC_LOG_DEBUG("network.opcode.transmog", "SMSG_TRANSMOG_OUTFIT_INFO_UPDATED [{}]: setId={} guid={}", GetPlayerInfo(), response.SetID, response.Guid);
    SendPacket(response.Write());
}

void WorldSession::HandleTransmogOutfitUpdateSlots(WorldPackets::Transmogrification::TransmogOutfitUpdateSlots& transmogOutfitUpdateSlots)
{
    LogTransmogOutfitOpcodeDebug(this, "CMSG_TRANSMOG_OUTFIT_UPDATE_SLOTS", transmogOutfitUpdateSlots.PayloadSize, transmogOutfitUpdateSlots.PayloadPreviewHex);

    if (!transmogOutfitUpdateSlots.ParseSuccess)
    {
        TC_LOG_ERROR("network.opcode.transmog", "CMSG_TRANSMOG_OUTFIT_UPDATE_SLOTS parse failed [{}]: {}", GetPlayerInfo(), transmogOutfitUpdateSlots.ParseError);
        TC_LOG_DEBUG("network.opcode.transmog", "{}", transmogOutfitUpdateSlots.DiagnosticReadTrace);
        return;
    }

    if (!ValidateTransmogOutfitNpc(this, transmogOutfitUpdateSlots.Npc, "CMSG_TRANSMOG_OUTFIT_UPDATE_SLOTS"))
        return;

    EquipmentSetInfo::EquipmentSetData const* existingSet = GetPlayer()->GetTransmogOutfitBySetID(transmogOutfitUpdateSlots.Set.SetID);
    if (!existingSet || existingSet->Type != EquipmentSetInfo::TRANSMOG)
    {
        TC_LOG_ERROR("network.opcode.transmog", "CMSG_TRANSMOG_OUTFIT_UPDATE_SLOTS rejected [{}]: unknown transmog set id {}",
            GetPlayerInfo(), transmogOutfitUpdateSlots.Set.SetID);
        // Diagnostic dump: log every entry in the equipment set map to find out
        // what happened to the set (deleted? wrong type? wrong SetID? missing entirely?)
        for (auto const& [guid, eqInfo] : GetPlayer()->GetEquipmentSets())
        {
            TC_LOG_ERROR("network.opcode.transmog",
                "  equipmentSet dump: guid={} setId={} type={} state={} name='{}'",
                guid, eqInfo.Data.SetID, int32(eqInfo.Data.Type), int32(eqInfo.State), eqInfo.Data.SetName);
        }
        return;
    }

    EquipmentSetInfo::EquipmentSetData updatedSet = *existingSet;
    updatedSet.SetID = transmogOutfitUpdateSlots.Set.SetID;

    // Check if the parsed appearance data has ANY non-zero IMAIDs.
    // Multi-iteration packets (30+ slots) send situation variants where iteration 0
    // is often all-zeros. If the base outfit data is empty, preserve existing appearances.
    bool hasAnyAppearance = false;
    for (uint8 slot = EQUIPMENT_SLOT_START; slot < EQUIPMENT_SLOT_END; ++slot)
    {
        if (transmogOutfitUpdateSlots.Set.Appearances[slot])
        {
            hasAnyAppearance = true;
            break;
        }
    }
    if (!hasAnyAppearance && transmogOutfitUpdateSlots.Set.SecondaryShoulderApparanceID)
        hasAnyAppearance = true;

    if (hasAnyAppearance)
    {
        // Per-slot merge: only overwrite slots that the outfit packet explicitly provides.
        // The 12.x client's CommitAndApplyAllPending() serializer omits HEAD (DT=0),
        // BACK (DT=9), TABARD (DT=10), MH (DT=11), and OH (DT=13/15) from the wire data —
        // those slots always arrive as IMAID=0. Preserve existing data first, then fill
        // missing slots from equipped item transmog modifiers.
        for (uint8 slot = EQUIPMENT_SLOT_START; slot < EQUIPMENT_SLOT_END; ++slot)
        {
            if (transmogOutfitUpdateSlots.Set.Appearances[slot])
            {
                // Incoming packet has a valid IMAID for this slot — use it
                updatedSet.Appearances[slot] = transmogOutfitUpdateSlots.Set.Appearances[slot];
                updatedSet.IgnoreMask &= ~(1u << slot); // clear ignore bit
            }
            else if (existingSet->Appearances[slot])
            {
                // Outfit packet has 0 for this slot but existing outfit has data — preserve it
                updatedSet.Appearances[slot] = existingSet->Appearances[slot];
                updatedSet.IgnoreMask &= ~(1u << slot);
                TC_LOG_DEBUG("network.opcode.transmog", "CMSG_TRANSMOG_OUTFIT_UPDATE_SLOTS [{}]: preserving existing IMAID {} for equipSlot={}",
                    GetPlayerInfo(), existingSet->Appearances[slot], slot);
            }
            else
            {
                // Both incoming and existing are 0 — slot is unused
                updatedSet.IgnoreMask |= (1u << slot);
            }
        }

        // Fill missing slots from player's equipped item transmog modifiers.
        // The 12.x client's CommitAndApplyAllPending() omits HEAD, BACK, TABARD,
        // MH, and OH from outfit packets — those slots always arrive as IMAID=0.
        // Read the active transmog from the player's equipped items to populate them.
        Player* player = GetPlayer();
        for (uint8 slot : {EQUIPMENT_SLOT_HEAD, EQUIPMENT_SLOT_BACK, EQUIPMENT_SLOT_TABARD,
                            EQUIPMENT_SLOT_MAINHAND, EQUIPMENT_SLOT_OFFHAND})
        {
            if (updatedSet.Appearances[slot] == 0)
            {
                if (Item* item = player->GetItemByPos(INVENTORY_SLOT_BAG_0, slot))
                {
                    uint32 transmogId = item->GetModifier(ITEM_MODIFIER_TRANSMOG_APPEARANCE_ALL_SPECS);
                    if (transmogId)
                    {
                        updatedSet.Appearances[slot] = int32(transmogId);
                        updatedSet.IgnoreMask &= ~(1u << slot);
                        TC_LOG_DEBUG("network.opcode.transmog", "CMSG_TRANSMOG_OUTFIT_UPDATE_SLOTS [{}]: filled missing slot {} from equipped item IMAID {}",
                            GetPlayerInfo(), slot, transmogId);
                    }
                }
            }
        }

        // Update secondary shoulder and enchants from incoming packet
        if (transmogOutfitUpdateSlots.Set.SecondaryShoulderApparanceID)
        {
            updatedSet.SecondaryShoulderApparanceID = transmogOutfitUpdateSlots.Set.SecondaryShoulderApparanceID;
            updatedSet.SecondaryShoulderSlot = transmogOutfitUpdateSlots.Set.SecondaryShoulderSlot;
        }
    }
    else
    {
        TC_LOG_DEBUG("network.opcode.transmog", "CMSG_TRANSMOG_OUTFIT_UPDATE_SLOTS [{}]: parsed appearances all-zero (slotCount={}), preserving existing outfit data",
            GetPlayerInfo(), transmogOutfitUpdateSlots.Slots.size());
    }

    // Defer finalization: the TransmogBridge addon message arrives in the very next
    // packet (same Update() cycle) with the correct IMAIDs. Store the pending outfit
    // so FinalizeTransmogBridgePendingOutfit can merge overrides before save/apply.
    // If no addon message arrives (addon not installed), the safety net in
    // WorldSession::Update() finalizes without overrides — backward compatible.
    _transmogBridgePendingOutfit.emplace(TransmogBridgePendingOutfit{std::move(updatedSet), hasAnyAppearance});
    TC_LOG_DEBUG("network.opcode.transmog", "CMSG_TRANSMOG_OUTFIT_UPDATE_SLOTS [{}]: deferred finalization (waiting for TransmogBridge addon message)",
        GetPlayerInfo());
}

namespace
{
// Maps client addon slot index → EquipmentSlot.
// Client indices (from GetViewedOutfitSlotInfo / TransmogOutfitSlot enum):
//   0=HEAD, 1=SHOULDER, 2=SECONDARY_SHOULDER, 3=BACK, 4=CHEST,
//   5=TABARD, 6=SHIRT, 7=WRIST, 8=HANDS, 9=WAIST, 10=LEGS,
//   11=FEET, 12=MAINHAND, 13=OFFHAND
// Slot 2 (secondary shoulder) maps to EQUIPMENT_SLOT_SHOULDERS but is handled
// specially — the override goes to SecondaryShoulderApparanceID, not Appearances[2].
uint8 MapClientSlotToEquipSlot(uint8 clientSlot)
{
    static constexpr uint8 map[] = {
        EQUIPMENT_SLOT_HEAD,        // 0
        EQUIPMENT_SLOT_SHOULDERS,   // 1
        EQUIPMENT_SLOT_SHOULDERS,   // 2 (secondary — caller checks clientSlot==2)
        EQUIPMENT_SLOT_BACK,        // 3
        EQUIPMENT_SLOT_CHEST,       // 4
        EQUIPMENT_SLOT_TABARD,      // 5
        EQUIPMENT_SLOT_BODY,        // 6 (shirt)
        EQUIPMENT_SLOT_WRISTS,      // 7
        EQUIPMENT_SLOT_HANDS,       // 8
        EQUIPMENT_SLOT_WAIST,       // 9
        EQUIPMENT_SLOT_LEGS,        // 10
        EQUIPMENT_SLOT_FEET,        // 11
        EQUIPMENT_SLOT_MAINHAND,    // 12
        EQUIPMENT_SLOT_OFFHAND,     // 13
    };
    return clientSlot < std::size(map) ? map[clientSlot] : EQUIPMENT_SLOT_END;
}
} // anonymous namespace

void WorldSession::FinalizeTransmogBridgePendingOutfit()
{
    if (!_transmogBridgePendingOutfit)
        return;

    auto& pending = *_transmogBridgePendingOutfit;

    // Build bridge override map: clientSlot -> TransmogID
    bool mergedOverrides = false;
    bool bridgeOverrodeSecondary = false; // separate flag — secondary shoulder doesn't use bridgeOverriddenMask
    uint32 bridgeOverriddenMask = 0; // bitmask of equipment slots the bridge explicitly set
    uint32 bridgeClearedMask = 0;    // bitmask of equipment slots the bridge explicitly cleared
    bool bridgeClearedSecondary = false; // secondary shoulder explicitly cleared
    if (!_transmogBridgeOverrides.empty())
    {
        mergedOverrides = true;
        TC_LOG_DEBUG("network.opcode.transmog", "TransmogBridge [{}]: merging {} overrides into pending outfit",
            GetPlayerInfo(), _transmogBridgeOverrides.size());

        for (auto const& ov : _transmogBridgeOverrides)
        {
            // Secondary shoulder (clientSlot 2) — routes to a separate field, not Appearances[]
            if (ov.ClientSlot == 2)
            {
                if (ov.TransmogID > 0)
                {
                    pending.Outfit.SecondaryShoulderApparanceID = ov.TransmogID;
                    pending.Outfit.SecondaryShoulderSlot = 2;
                    bridgeOverrodeSecondary = true;
                    TC_LOG_DEBUG("network.opcode.transmog", "TransmogBridge [{}]: merged secondary shoulder IMAID={}",
                        GetPlayerInfo(), ov.TransmogID);
                }
                else
                {
                    // Explicit clear of secondary shoulder
                    pending.Outfit.SecondaryShoulderApparanceID = 0;
                    pending.Outfit.SecondaryShoulderSlot = 0;
                    bridgeOverrodeSecondary = true;
                    bridgeClearedSecondary = true;
                    TC_LOG_DEBUG("network.opcode.transmog", "TransmogBridge [{}]: cleared secondary shoulder",
                        GetPlayerInfo());
                }
                continue;
            }

            uint8 equipSlot = MapClientSlotToEquipSlot(ov.ClientSlot);
            if (equipSlot >= EQUIPMENT_SLOT_END)
                continue;

            if (ov.TransmogID > 0)
            {
                pending.Outfit.Appearances[equipSlot] = ov.TransmogID;
                pending.Outfit.IgnoreMask &= ~(1u << equipSlot);
                pending.HasAnyAppearance = true;
                bridgeOverriddenMask |= (1u << equipSlot);
                TC_LOG_DEBUG("network.opcode.transmog", "TransmogBridge [{}]: merged clientSlot={} -> equipSlot={} IMAID={}",
                    GetPlayerInfo(), ov.ClientSlot, equipSlot, ov.TransmogID);
            }
            else
            {
                // Explicit clear: user removed transmog from this slot
                pending.Outfit.Appearances[equipSlot] = 0;
                pending.Outfit.IgnoreMask &= ~(1u << equipSlot);
                pending.HasAnyAppearance = true; // need apply pass to clear the item modifier
                bridgeOverriddenMask |= (1u << equipSlot);
                bridgeClearedMask |= (1u << equipSlot);
                TC_LOG_DEBUG("network.opcode.transmog", "TransmogBridge [{}]: clear clientSlot={} -> equipSlot={}",
                    GetPlayerInfo(), ov.ClientSlot, equipSlot);
            }

            // Handle illusion (weapon enchant visual) — only for MH/OH slots
            if (ov.HasIllusion)
            {
                if (equipSlot == EQUIPMENT_SLOT_MAINHAND)
                {
                    pending.Outfit.Enchants[0] = ov.IllusionID;
                    pending.HasAnyAppearance = true;
                    TC_LOG_DEBUG("network.opcode.transmog", "TransmogBridge [{}]: merged MH illusion enchantID={}",
                        GetPlayerInfo(), ov.IllusionID);
                }
                else if (equipSlot == EQUIPMENT_SLOT_OFFHAND)
                {
                    pending.Outfit.Enchants[1] = ov.IllusionID;
                    pending.HasAnyAppearance = true;
                    TC_LOG_DEBUG("network.opcode.transmog", "TransmogBridge [{}]: merged OH illusion enchantID={}",
                        GetPlayerInfo(), ov.IllusionID);
                }
            }
        }

        _transmogBridgeOverrides.clear();
    }

    // Use server's saved outfit as baseline for non-bridge slots.
    // The client's packet data is unreliable for ALL slots (sends stale cached IMAIDs).
    // Only bridge-overridden slots have correct user-intended values.
    if (Player* player = GetPlayer())
    {
        EquipmentSetInfo::EquipmentSetData const* savedOutfit = player->GetTransmogOutfitBySetID(pending.Outfit.SetID);
        if (savedOutfit)
        {
            for (uint8 slot = EQUIPMENT_SLOT_START; slot < EQUIPMENT_SLOT_END; ++slot)
            {
                if (bridgeOverriddenMask & (1u << slot))
                    continue; // bridge has the correct value

                // Restore server's saved value for this slot
                pending.Outfit.Appearances[slot] = savedOutfit->Appearances[slot];
                if (savedOutfit->Appearances[slot])
                    pending.HasAnyAppearance = true;
            }

            // Preserve saved secondary shoulder unless bridge explicitly overrode it.
            // Note: bridgeOverriddenMask tracks Appearances[] slots (primary shoulder = bit 2).
            // Secondary shoulder routes to SecondaryShoulderApparanceID, not Appearances[],
            // so it has its own dedicated flag.
            if (!bridgeOverrodeSecondary && pending.Outfit.SecondaryShoulderApparanceID == 0)
            {
                pending.Outfit.SecondaryShoulderApparanceID = savedOutfit->SecondaryShoulderApparanceID;
                pending.Outfit.SecondaryShoulderSlot = savedOutfit->SecondaryShoulderSlot;
            }

            uint32 bridgeSlotCount = 0;
            for (uint32 tmp = bridgeOverriddenMask; tmp; tmp &= tmp - 1)
                ++bridgeSlotCount;
            TC_LOG_DEBUG("network.opcode.transmog",
                "TransmogBridge [{}]: restored server baseline (bridgeMask=0x{:X}, {} bridge slots)",
                GetPlayerInfo(), bridgeOverriddenMask, bridgeSlotCount);
        }
        else
        {
            TC_LOG_DEBUG("network.opcode.transmog",
                "TransmogBridge [{}]: no saved outfit for setId={} — using client packet data as-is (new outfit)",
                GetPlayerInfo(), pending.Outfit.SetID);
        }
    }

    // Validate, save, apply, respond — single pass with correct IMAIDs
    if (!ValidateTransmogOutfitSet(this, pending.Outfit))
    {
        TC_LOG_DEBUG("network.opcode.transmog", "TransmogBridge [{}]: outfit validation failed after merge",
            GetPlayerInfo());
        _transmogBridgePendingOutfit.reset();
        return;
    }

    // Re-apply explicit clears after validation.
    // ValidateTransmogOutfitSet forces IgnoreMask for Appearances==0 slots,
    // but bridge clears need the slot active so ApplyTransmogOutfitToPlayer
    // will set the item modifier to 0 (removing the transmog).
    if (bridgeClearedMask)
    {
        for (uint8 slot = EQUIPMENT_SLOT_START; slot < EQUIPMENT_SLOT_END; ++slot)
        {
            if (bridgeClearedMask & (1u << slot))
            {
                pending.Outfit.Appearances[slot] = 0;
                pending.Outfit.IgnoreMask &= ~(1u << slot);
            }
        }
        TC_LOG_DEBUG("network.opcode.transmog", "TransmogBridge [{}]: re-applied slot clears after validation (mask=0x{:X})",
            GetPlayerInfo(), bridgeClearedMask);
    }
    if (bridgeClearedSecondary)
    {
        pending.Outfit.SecondaryShoulderApparanceID = 0;
        pending.Outfit.SecondaryShoulderSlot = 0;
    }

    // Diagnostic: verify IgnoreMask state after re-apply — confirms equipment-slot-indexed
    // bridgeClearedMask correctly undid ValidateTransmogOutfitSet's IgnoreMask re-set.
    TC_LOG_DEBUG("network.opcode.transmog",
        "TransmogBridge: post-reapply bridgeClearedMask=0x{:X} bridgeOverriddenMask=0x{:X} final IgnoreMask=0x{:X}",
        bridgeClearedMask, bridgeOverriddenMask, pending.Outfit.IgnoreMask);
    for (uint8 s = 0; s < EQUIPMENT_SLOT_END; ++s)
        TC_LOG_DEBUG("network.opcode.transmog", "  equipSlot={} Appearances={} ignored={}",
            s, pending.Outfit.Appearances[s], (pending.Outfit.IgnoreMask & (1u << s)) != 0);

    GetPlayer()->SetEquipmentSet(pending.Outfit);

    if (pending.HasAnyAppearance)
    {
        if (!ApplyTransmogOutfitToPlayer(GetPlayer(), pending.Outfit))
        {
            _transmogBridgePendingOutfit.reset();
            return;
        }
    }

    // Flush UpdateField changes before response (see HandleTransmogOutfitNew comment)
    Player* player = GetPlayer();
    player->SendUpdateToPlayer(player);
    player->ClearUpdateMask(true);

    WorldPackets::Transmogrification::TransmogOutfitSlotsUpdated response;
    response.SetID = pending.Outfit.SetID;
    response.Guid = pending.Outfit.Guid;
    TC_LOG_DEBUG("network.opcode.transmog", "SMSG_TRANSMOG_OUTFIT_SLOTS_UPDATED [{}]: setId={} guid={} (finalized{})",
        GetPlayerInfo(), response.SetID, response.Guid,
        mergedOverrides ? " with bridge overrides" : "");
    SendPacket(response.Write());

    _transmogBridgePendingOutfit.reset();
}

void WorldSession::HandleTransmogOutfitUpdateSituations(WorldPackets::Transmogrification::TransmogOutfitUpdateSituations& transmogOutfitUpdateSituations)
{
    LogTransmogOutfitOpcodeDebug(this, "CMSG_TRANSMOG_OUTFIT_UPDATE_SITUATIONS", transmogOutfitUpdateSituations.PayloadSize, transmogOutfitUpdateSituations.PayloadPreviewHex);

    if (!transmogOutfitUpdateSituations.ParseSuccess)
    {
        TC_LOG_ERROR("network.opcode.transmog", "CMSG_TRANSMOG_OUTFIT_UPDATE_SITUATIONS parse failed [{}]: {}", GetPlayerInfo(), transmogOutfitUpdateSituations.ParseError);
        TC_LOG_DEBUG("network.opcode.transmog", "{}", transmogOutfitUpdateSituations.DiagnosticReadTrace);
        return;
    }

    if (!ValidateTransmogOutfitNpc(this, transmogOutfitUpdateSituations.Npc, "CMSG_TRANSMOG_OUTFIT_UPDATE_SITUATIONS"))
        return;

    EquipmentSetInfo::EquipmentSetData const* existingSet = GetPlayer()->GetTransmogOutfitBySetID(transmogOutfitUpdateSituations.SetID);
    if (!existingSet || existingSet->Type != EquipmentSetInfo::TRANSMOG)
    {
        TC_LOG_ERROR("network.opcode.transmog", "CMSG_TRANSMOG_OUTFIT_UPDATE_SITUATIONS rejected [{}]: unknown transmog set id {}", GetPlayerInfo(), transmogOutfitUpdateSituations.SetID);
        return;
    }

    TC_LOG_DEBUG("network.opcode.transmog", "CMSG_TRANSMOG_OUTFIT_UPDATE_SITUATIONS [{}]: setId={} situations={}",
        GetPlayerInfo(), transmogOutfitUpdateSituations.SetID, transmogOutfitUpdateSituations.Situations.size());

    for (WorldPackets::Transmogrification::TransmogOutfitSituationEntry const& situation : transmogOutfitUpdateSituations.Situations)
    {
        TC_LOG_DEBUG("network.opcode.transmog", "  situation={} spec={} loadout={} equipmentSet={}",
            situation.SituationID, situation.SpecID, situation.LoadoutID, situation.EquipmentSetID);
    }

    // Build updated set data with new situations
    EquipmentSetInfo::EquipmentSetData updatedSet = *existingSet;
    updatedSet.Situations.clear();
    for (WorldPackets::Transmogrification::TransmogOutfitSituationEntry const& entry : transmogOutfitUpdateSituations.Situations)
    {
        TransmogSituationData sit;
        sit.SituationID = entry.SituationID;
        sit.SpecID = entry.SpecID;
        sit.LoadoutID = entry.LoadoutID;
        sit.EquipmentSetID = entry.EquipmentSetID;
        updatedSet.Situations.push_back(sit);
    }

    GetPlayer()->SetEquipmentSet(updatedSet);

    // Flush UpdateField changes before response (see HandleTransmogOutfitNew comment)
    GetPlayer()->SendUpdateToPlayer(GetPlayer());
    GetPlayer()->ClearUpdateMask(true);

    WorldPackets::Transmogrification::TransmogOutfitSituationsUpdated response;
    response.SetID = transmogOutfitUpdateSituations.SetID;
    response.Guid = updatedSet.Guid;
    TC_LOG_DEBUG("network.opcode.transmog", "SMSG_TRANSMOG_OUTFIT_SITUATIONS_UPDATED [{}]: setId={} guid={}", GetPlayerInfo(), response.SetID, response.Guid);
    SendPacket(response.Write());
}

void WorldSession::SendOpenTransmogrifier(ObjectGuid const& guid)
{
    WorldPackets::NPC::NPCInteractionOpenResult npcInteraction;
    npcInteraction.Npc = guid;
    npcInteraction.InteractionType = PlayerInteractionType::Transmogrifier;
    npcInteraction.Success = true;
    SendPacket(npcInteraction.Write());
}
