#include "custom_display_handler.h"
#include "TransmogMgr.h"

namespace RoleplayCore
{
    // Initialization of static constants
    const std::map<DisplayType, EquipmentSlots> DisplayHandler::m_item_slots = {
        {DISPLAY_TYPE_HEAD, EQUIPMENT_SLOT_HEAD},
        {DISPLAY_TYPE_SHOULDERS, EQUIPMENT_SLOT_SHOULDERS},
        {DISPLAY_TYPE_SHIRT, EQUIPMENT_SLOT_BODY},
        {DISPLAY_TYPE_CHEST, EQUIPMENT_SLOT_CHEST},
        {DISPLAY_TYPE_WAIST, EQUIPMENT_SLOT_WAIST},
        {DISPLAY_TYPE_PANTS, EQUIPMENT_SLOT_LEGS},
        {DISPLAY_TYPE_BOOTS, EQUIPMENT_SLOT_FEET},
        {DISPLAY_TYPE_WRISTS, EQUIPMENT_SLOT_WRISTS},
        {DISPLAY_TYPE_HANDS, EQUIPMENT_SLOT_HANDS},
        {DISPLAY_TYPE_BACK, EQUIPMENT_SLOT_BACK},
        {DISPLAY_TYPE_TABARD, EQUIPMENT_SLOT_TABARD},
        {DISPLAY_TYPE_MAIN, EQUIPMENT_SLOT_MAINHAND},
        {DISPLAY_TYPE_RANGED, EQUIPMENT_SLOT_MAINHAND},
        {DISPLAY_TYPE_SHIELD, EQUIPMENT_SLOT_OFFHAND},
        {DISPLAY_TYPE_OFF, EQUIPMENT_SLOT_OFFHAND},
    };

    const std::map<InventoryType, DisplayType> DisplayHandler::m_inv_type_to_disp_type = {
        {INVTYPE_HEAD, DISPLAY_TYPE_HEAD},
        {INVTYPE_SHOULDERS, DISPLAY_TYPE_SHOULDERS},
        {INVTYPE_BODY, DISPLAY_TYPE_SHIRT},
        {INVTYPE_CHEST, DISPLAY_TYPE_CHEST},
        {INVTYPE_ROBE, DISPLAY_TYPE_CHEST},
        {INVTYPE_WAIST, DISPLAY_TYPE_WAIST},
        {INVTYPE_LEGS, DISPLAY_TYPE_PANTS},
        {INVTYPE_FEET, DISPLAY_TYPE_BOOTS},
        {INVTYPE_WRISTS, DISPLAY_TYPE_WRISTS},
        {INVTYPE_HANDS, DISPLAY_TYPE_HANDS},
        {INVTYPE_CLOAK, DISPLAY_TYPE_BACK},
        {INVTYPE_TABARD, DISPLAY_TYPE_TABARD},
        {INVTYPE_WEAPONMAINHAND, DISPLAY_TYPE_MAIN},
        {INVTYPE_WEAPON, DISPLAY_TYPE_MAIN},
        {INVTYPE_WEAPONOFFHAND, DISPLAY_TYPE_OFF},
        {INVTYPE_SHIELD, DISPLAY_TYPE_SHIELD},
        {INVTYPE_RANGEDRIGHT, DISPLAY_TYPE_RANGED},
        {INVTYPE_RANGED, DISPLAY_TYPE_RANGED},
    };

    const std::map<DisplayType, std::vector<DisplayType>> DisplayHandler::m_display_map = {
        {DISPLAY_TYPE_HEAD, {DISPLAY_TYPE_HEAD}},
        {DISPLAY_TYPE_CHEST, {DISPLAY_TYPE_BACK, DISPLAY_TYPE_CHEST, DISPLAY_TYPE_SHIRT, DISPLAY_TYPE_HANDS, DISPLAY_TYPE_WRISTS, DISPLAY_TYPE_BOOTS, DISPLAY_TYPE_TABARD, DISPLAY_TYPE_PANTS}},
        {DISPLAY_TYPE_SHIRT, {DISPLAY_TYPE_BACK, DISPLAY_TYPE_CHEST, DISPLAY_TYPE_SHIRT, DISPLAY_TYPE_HANDS, DISPLAY_TYPE_WRISTS, DISPLAY_TYPE_BOOTS, DISPLAY_TYPE_TABARD, DISPLAY_TYPE_PANTS}},
        {DISPLAY_TYPE_TABARD, {DISPLAY_TYPE_WAIST, DISPLAY_TYPE_CHEST, DISPLAY_TYPE_SHIRT, DISPLAY_TYPE_BOOTS, DISPLAY_TYPE_TABARD, DISPLAY_TYPE_PANTS}},
        {DISPLAY_TYPE_BACK, {DISPLAY_TYPE_BACK, DISPLAY_TYPE_CHEST, DISPLAY_TYPE_PANTS}},
        {DISPLAY_TYPE_WRISTS, {DISPLAY_TYPE_BACK, DISPLAY_TYPE_WRISTS, DISPLAY_TYPE_HANDS, DISPLAY_TYPE_SHIRT, DISPLAY_TYPE_CHEST}},
        {DISPLAY_TYPE_HANDS, {DISPLAY_TYPE_WRISTS, DISPLAY_TYPE_HANDS, DISPLAY_TYPE_SHIRT, DISPLAY_TYPE_CHEST}},
        {DISPLAY_TYPE_WAIST, {DISPLAY_TYPE_WAIST, DISPLAY_TYPE_SHIRT}},
        {DISPLAY_TYPE_SHOULDERS, {DISPLAY_TYPE_SHOULDERS}},
        {DISPLAY_TYPE_PANTS, {DISPLAY_TYPE_PANTS, DISPLAY_TYPE_CHEST, DISPLAY_TYPE_BOOTS}},
        {DISPLAY_TYPE_BOOTS, {DISPLAY_TYPE_BOOTS, DISPLAY_TYPE_PANTS, DISPLAY_TYPE_CHEST}},
        {DISPLAY_TYPE_OFF, {DISPLAY_TYPE_OFF, DISPLAY_TYPE_RANGED, DISPLAY_TYPE_SHIELD, DISPLAY_TYPE_MAIN}},
        {DISPLAY_TYPE_RANGED, {DISPLAY_TYPE_OFF, DISPLAY_TYPE_RANGED, DISPLAY_TYPE_SHIELD, DISPLAY_TYPE_MAIN}},
        {DISPLAY_TYPE_SHIELD, {DISPLAY_TYPE_OFF, DISPLAY_TYPE_RANGED, DISPLAY_TYPE_SHIELD, DISPLAY_TYPE_MAIN}},
        {DISPLAY_TYPE_MAIN, {DISPLAY_TYPE_OFF, DISPLAY_TYPE_RANGED, DISPLAY_TYPE_SHIELD, DISPLAY_TYPE_MAIN}},
    };

    void DisplayHandler::Display(ChatHandler* handler, DisplayType type, uint32 id, uint32 bonus, bool isSecondary)
    {
        if (!handler || !handler->GetPlayer())
            return;

        auto itemSlot = m_item_slots.at(type);
        auto player = handler->GetPlayer();
        auto item = player->GetItemByPos(INVENTORY_SLOT_BAG_0, itemSlot);

        if (!item)
        {
            handler->SendSysMessage("Slot is empty.");
            return;
        }

        if (id == 0)
        {
            ResetItem(item);

            if (isSecondary)
                item->SetModifier(ITEM_MODIFIER_TRANSMOG_SECONDARY_APPEARANCE_ALL_SPECS, 0);
            else
                item->SetModifier(ITEM_MODIFIER_TRANSMOG_APPEARANCE_ALL_SPECS, 0);

            player->SetVisibleItemSlot(itemSlot, item);
            item->ClearBonuses();
            item->SetState(ITEM_CHANGED, player);
            return;
        }

        auto itemTemplate = sObjectMgr->GetItemTemplate(id);
        if (!itemTemplate)
        {
            handler->SendSysMessage("Target item not found.");
            return;
        }

        auto applyArtifactAppearance = [&](uint32 artifactAppearanceId) -> bool
        {
            if (!item->GetTemplate()->GetArtifactID() || isSecondary)
                return false;

            ArtifactAppearanceEntry const* artifactAppearance = sArtifactAppearanceStore.LookupEntry(artifactAppearanceId);
            if (!artifactAppearance)
                return false;

            ArtifactAppearanceSetEntry const* artifactAppearanceSet = sArtifactAppearanceSetStore.LookupEntry(artifactAppearance->ArtifactAppearanceSetID);
            if (!artifactAppearanceSet || artifactAppearanceSet->ArtifactID != item->GetTemplate()->GetArtifactID())
                return false;

            item->SetAppearanceModId(artifactAppearance->ItemAppearanceModifierID);
            item->SetModifier(ITEM_MODIFIER_ARTIFACT_APPEARANCE_ID, artifactAppearance->ID);
            item->SetState(ITEM_CHANGED, player);
            player->SetVisibleItemSlot(itemSlot, item);

            Item* childItem = player->GetChildItemByGuid(item->GetChildItem());
            if (childItem)
            {
                childItem->SetAppearanceModId(artifactAppearance->ItemAppearanceModifierID);
                childItem->SetState(ITEM_CHANGED, player);
                player->SetVisibleItemSlot(childItem->GetSlot(), childItem);
            }

            return true;
        };

        if (bonus != 0 && applyArtifactAppearance(bonus))
            return;

        if (bonus != 0)
        {
            if (ItemModifiedAppearanceEntry const* modifiedAppearance = TransmogMgr::GetItemModifiedAppearance(id, bonus))
                if (modifiedAppearance->ItemAppearanceID != 0)
                {
                    ApplyModifiedAppearance(modifiedAppearance->ID, modifiedAppearance->ItemAppearanceID, item, player, handler, itemSlot, isSecondary);
                    return;
                }
        }

        for (ItemModifiedAppearanceEntry const* modifiedAppearance : sItemModifiedAppearanceStore)
        {
            if (modifiedAppearance->ItemID != id || modifiedAppearance->ItemAppearanceID == 0)
                continue;

            if (ApplyModifiedAppearance(modifiedAppearance->ID, modifiedAppearance->ItemAppearanceID, item, player, handler, itemSlot, isSecondary))
                return;
        }

        handler->SendSysMessage("Target item has no appearance.");
    }

    void DisplayHandler::ResetItem(Item* item)
    {
        if (!item)
            return;

        // Reset all appearance modifiers
        static const std::array<ItemModifier, 8> modifiers = {
            ITEM_MODIFIER_TRANSMOG_SECONDARY_APPEARANCE_SPEC_1,
            ITEM_MODIFIER_TRANSMOG_SECONDARY_APPEARANCE_SPEC_2,
            ITEM_MODIFIER_TRANSMOG_SECONDARY_APPEARANCE_SPEC_3,
            ITEM_MODIFIER_TRANSMOG_SECONDARY_APPEARANCE_SPEC_4,
            ITEM_MODIFIER_TRANSMOG_APPEARANCE_SPEC_1,
            ITEM_MODIFIER_TRANSMOG_APPEARANCE_SPEC_2,
            ITEM_MODIFIER_TRANSMOG_APPEARANCE_SPEC_3,
            ITEM_MODIFIER_TRANSMOG_APPEARANCE_SPEC_4
        };

        for (auto mod : modifiers)
            item->SetModifier(mod, 0);
    }

    bool DisplayHandler::ApplyModifiedAppearance(int modApID, int apID, Item* item, Player* player, ChatHandler* handler, EquipmentSlots itemSlot, bool isSecondary)
    {
        if (!item || !player || !handler || apID == 0)
            return false;

        auto targetAppearance = sItemAppearanceStore.LookupEntry(apID);
        if (!targetAppearance)
            return false;

        auto targetType = static_cast<DisplayType>(targetAppearance->DisplayType);
        auto playerType = GetDisplayType(item);

        if (targetType == DISPLAY_TYPE_NONE || playerType == DISPLAY_TYPE_NONE)
        {
            handler->SendSysMessage("Target item is not part of equipment.");
            return false;
        }

        // Check if the target item type is compatible with the player's item type
        const auto& validSlots = m_display_map.find(playerType);
        if (validSlots == m_display_map.end())
            return false;

        // Use std::find to search in a vector instead of std::find in a list
        bool itemIsValid = std::find(validSlots->second.begin(), validSlots->second.end(), targetType) != validSlots->second.end();

        if (!itemIsValid)
        {
            handler->SendSysMessage("Target item type does not match your item type.");
            return false;
        }

        // Reset all modifiers before installing new ones
        ResetItem(item);

        // Set the appropriate modifier
        if (isSecondary)
        {
            // Keeping the current main appear
            item->SetModifier(ITEM_MODIFIER_TRANSMOG_APPEARANCE_ALL_SPECS,
                             item->GetModifier(ITEM_MODIFIER_TRANSMOG_APPEARANCE_ALL_SPECS));
            // Set the secondary appear
            item->SetModifier(ITEM_MODIFIER_TRANSMOG_SECONDARY_APPEARANCE_ALL_SPECS, modApID);
        }
        else
        {
            item->SetModifier(ITEM_MODIFIER_TRANSMOG_APPEARANCE_ALL_SPECS, modApID);
        }

        // Updating item status and display
        item->SetState(ITEM_CHANGED, player);
        player->SetVisibleItemSlot(itemSlot, item);

        return true;
    }

    DisplayType DisplayHandler::GetDisplayType(Item* item)
    {
        if (!item)
            return DISPLAY_TYPE_NONE;

        DisplayType result = DISPLAY_TYPE_NONE;

        // Check if the item has a modified appearance
        if (auto appearanceModifiedEntry = item->GetItemModifiedAppearance())
        {
            if (auto appearanceEntry = sItemAppearanceStore.LookupEntry(appearanceModifiedEntry->ItemAppearanceID))
            {
                return static_cast<DisplayType>(appearanceEntry->DisplayType);
            }
        }
        // If no modified type is found, use the type from the item template
        auto inventoryType = item->GetTemplate()->GetInventoryType();
        auto typeIter = m_inv_type_to_disp_type.find(inventoryType);

        if (typeIter != m_inv_type_to_disp_type.end())
            result = typeIter->second;

        return result;
    }
};

void AddSC_CustomDisplayHandler()
{
    // Just initialize the singleton
    RoleplayCore::DisplayHandler::GetInstance();
}
