#pragma once  // Use instead of manual detection of the reclosure protection

#include "Define.h"
#include "Chat.h"
#include "Item.h"
#include "ItemBonusMgr.h"
#include "ItemEnchantmentMgr.h"
#include "Player.h"

namespace RoleplayCore
{
    enum DisplayType : uint32 {
        DISPLAY_TYPE_HEAD = 0,
        DISPLAY_TYPE_SHOULDERS = 1,
        DISPLAY_TYPE_SHIRT = 2,
        DISPLAY_TYPE_CHEST = 3,
        DISPLAY_TYPE_WAIST = 4,
        DISPLAY_TYPE_PANTS = 5,
        DISPLAY_TYPE_BOOTS = 6,
        DISPLAY_TYPE_WRISTS = 7,
        DISPLAY_TYPE_HANDS = 8,
        DISPLAY_TYPE_BACK = 9,
        DISPLAY_TYPE_TABARD = 10,
        DISPLAY_TYPE_MAIN = 11,
        DISPLAY_TYPE_RANGED = 12,
        DISPLAY_TYPE_SHIELD = 13,
        DISPLAY_TYPE_OFF = 15,
        DISPLAY_TYPE_NONE = 999,
    };

    class DisplayHandler
    {
    public:
        static DisplayHandler& GetInstance()
        {
            static DisplayHandler instance;
            return instance;
        }

        // Removed copying constructor and assignment operator
        DisplayHandler(DisplayHandler const&) = delete;
        void operator=(DisplayHandler const&) = delete;

        // Public methods
        void Display(ChatHandler* handler, DisplayType type, uint32 id, uint32 bonus, bool isSecondary);

    private:
        // Hidden constructor for singleton
        DisplayHandler() = default;

        void ResetItem(Item* item);
        bool ApplyModifiedAppearance(int modApID, int apID, Item* item, Player* player, ChatHandler* handler, EquipmentSlots itemSlot, bool isSecondary);
        DisplayType GetDisplayType(Item* item);

        // Constants are better declared as static constexpr to improve performance
        static const std::map<DisplayType, EquipmentSlots> m_item_slots;
        static const std::map<InventoryType, DisplayType> m_inv_type_to_disp_type;
        static const std::map<DisplayType, std::vector<DisplayType>> m_display_map; // Use vector instead of list for quick access
    };
}
