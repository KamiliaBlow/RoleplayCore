#include "TransmogrificationUtils.h"
#include "Item.h"
#include "Log.h"
#include "Player.h"
#include "SpellAuraDefines.h"

bool ApplyTransmogOutfitToPlayer(Player* player, EquipmentSetInfo::EquipmentSetData const& outfit)
{
    // --- Phase 1: Calculate gold cost ---
    int64 cost = 0;

    for (uint8 slot = EQUIPMENT_SLOT_START; slot < EQUIPMENT_SLOT_END; ++slot)
    {
        if (outfit.IgnoreMask & (1u << slot))
            continue;

        Item* item = player->GetItemByPos(INVENTORY_SLOT_BAG_0, slot);
        if (!item)
            continue;

        int32 newAppearance = outfit.Appearances[slot];
        int32 curAppearance = item->GetModifier(ITEM_MODIFIER_TRANSMOG_APPEARANCE_ALL_SPECS);

        bool appearanceChanged = (newAppearance != curAppearance);

        // Check secondary shoulder change
        bool secondaryChanged = false;
        if (slot == EQUIPMENT_SLOT_SHOULDERS)
        {
            int32 curSecondary = item->GetModifier(ITEM_MODIFIER_TRANSMOG_SECONDARY_APPEARANCE_ALL_SPECS);
            secondaryChanged = (outfit.SecondaryShoulderApparanceID != curSecondary);
        }

        // Check enchant illusion change
        bool illusionChanged = false;
        if (slot == EQUIPMENT_SLOT_MAINHAND)
            illusionChanged = (uint32(outfit.Enchants[0]) != item->GetModifier(ITEM_MODIFIER_ENCHANT_ILLUSION_ALL_SPECS));
        else if (slot == EQUIPMENT_SLOT_OFFHAND)
            illusionChanged = (uint32(outfit.Enchants[1]) != item->GetModifier(ITEM_MODIFIER_ENCHANT_ILLUSION_ALL_SPECS));

        if (appearanceChanged || secondaryChanged || illusionChanged)
            cost += item->GetSellPrice(player);
    }

    // --- Phase 2: Charge gold ---
    if (!player->HasAuraType(SPELL_AURA_REMOVE_TRANSMOG_COST) && cost)
    {
        if (!player->HasEnoughMoney(cost))
        {
            TC_LOG_DEBUG("network.opcode.transmog", "ApplyTransmogOutfitToPlayer [{}]: not enough gold (need {}, have {})",
                player->GetGUID().ToString(), cost, player->GetMoney());
            return false;
        }
        player->ModifyMoney(-cost);
    }

    // --- Phase 3: Apply appearances ---
    for (uint8 slot = EQUIPMENT_SLOT_START; slot < EQUIPMENT_SLOT_END; ++slot)
    {
        if (outfit.IgnoreMask & (1u << slot))
            continue;

        Item* item = player->GetItemByPos(INVENTORY_SLOT_BAG_0, slot);
        if (!item)
            continue;

        int32 appearanceId = outfit.Appearances[slot];

        // Set or clear primary appearance (ALL_SPECS + clear per-spec)
        item->SetModifier(ITEM_MODIFIER_TRANSMOG_APPEARANCE_ALL_SPECS, appearanceId);
        item->SetModifier(ITEM_MODIFIER_TRANSMOG_APPEARANCE_SPEC_1, 0);
        item->SetModifier(ITEM_MODIFIER_TRANSMOG_APPEARANCE_SPEC_2, 0);
        item->SetModifier(ITEM_MODIFIER_TRANSMOG_APPEARANCE_SPEC_3, 0);
        item->SetModifier(ITEM_MODIFIER_TRANSMOG_APPEARANCE_SPEC_4, 0);
        item->SetModifier(ITEM_MODIFIER_TRANSMOG_APPEARANCE_SPEC_5, 0);

        // Secondary shoulder appearance
        if (slot == EQUIPMENT_SLOT_SHOULDERS)
        {
            item->SetModifier(ITEM_MODIFIER_TRANSMOG_SECONDARY_APPEARANCE_ALL_SPECS, outfit.SecondaryShoulderApparanceID);
            item->SetModifier(ITEM_MODIFIER_TRANSMOG_SECONDARY_APPEARANCE_SPEC_1, 0);
            item->SetModifier(ITEM_MODIFIER_TRANSMOG_SECONDARY_APPEARANCE_SPEC_2, 0);
            item->SetModifier(ITEM_MODIFIER_TRANSMOG_SECONDARY_APPEARANCE_SPEC_3, 0);
            item->SetModifier(ITEM_MODIFIER_TRANSMOG_SECONDARY_APPEARANCE_SPEC_4, 0);
            item->SetModifier(ITEM_MODIFIER_TRANSMOG_SECONDARY_APPEARANCE_SPEC_5, 0);
        }

        // Enchant illusions (mainhand / offhand)
        if (slot == EQUIPMENT_SLOT_MAINHAND)
        {
            item->SetModifier(ITEM_MODIFIER_ENCHANT_ILLUSION_ALL_SPECS, outfit.Enchants[0]);
            item->SetModifier(ITEM_MODIFIER_ENCHANT_ILLUSION_SPEC_1, 0);
            item->SetModifier(ITEM_MODIFIER_ENCHANT_ILLUSION_SPEC_2, 0);
            item->SetModifier(ITEM_MODIFIER_ENCHANT_ILLUSION_SPEC_3, 0);
            item->SetModifier(ITEM_MODIFIER_ENCHANT_ILLUSION_SPEC_4, 0);
            item->SetModifier(ITEM_MODIFIER_ENCHANT_ILLUSION_SPEC_5, 0);
        }
        if (slot == EQUIPMENT_SLOT_OFFHAND)
        {
            item->SetModifier(ITEM_MODIFIER_ENCHANT_ILLUSION_ALL_SPECS, outfit.Enchants[1]);
            item->SetModifier(ITEM_MODIFIER_ENCHANT_ILLUSION_SPEC_1, 0);
            item->SetModifier(ITEM_MODIFIER_ENCHANT_ILLUSION_SPEC_2, 0);
            item->SetModifier(ITEM_MODIFIER_ENCHANT_ILLUSION_SPEC_3, 0);
            item->SetModifier(ITEM_MODIFIER_ENCHANT_ILLUSION_SPEC_4, 0);
            item->SetModifier(ITEM_MODIFIER_ENCHANT_ILLUSION_SPEC_5, 0);
        }

        player->SetVisibleItemSlot(slot, item);
        item->SetNotRefundable(player);
        item->ClearSoulboundTradeable(player);
        item->SetState(ITEM_CHANGED, player);
    }

    TC_LOG_DEBUG("network.opcode.transmog", "ApplyTransmogOutfitToPlayer [{}]: applied outfit (cost={}, ignoreMask=0x{:X})",
        player->GetGUID().ToString(), cost, outfit.IgnoreMask);

    return true;
}
