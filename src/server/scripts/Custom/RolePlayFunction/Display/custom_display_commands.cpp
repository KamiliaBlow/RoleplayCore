#include "custom_display_handler.h"
#include "Define.h"
#include "Chat.h"
#include "ChatCommand.h"
#include "Common.h"

namespace RoleplayCore
{
    using namespace Trinity::ChatCommands;

    class DisplayCommands : public CommandScript
    {
    public:
        DisplayCommands() : CommandScript("player_display_commands") { }

        std::span<ChatCommandBuilder const> GetCommands() const override
        {
            static ChatCommandTable displayCommandTable =
            {
                { "head",      rbac::RBAC_PERM_COMMAND_DISP_HEAD,       false, Display<DISPLAY_TYPE_HEAD>,            "" },
                { "shoulders", rbac::RBAC_PERM_COMMAND_DISP_SHOULDERS,  false, Display<DISPLAY_TYPE_SHOULDERS>,       "" },
                { "lshoulder", rbac::RBAC_PERM_COMMAND_DISP_SHOULDERS,  false, Display<DISPLAY_TYPE_SHOULDERS, true>, "" },
                { "shirt",     rbac::RBAC_PERM_COMMAND_DISP_SHIRT,      false, Display<DISPLAY_TYPE_SHIRT>,           "" },
                { "chest",     rbac::RBAC_PERM_COMMAND_DISP_CHEST,      false, Display<DISPLAY_TYPE_CHEST>,           "" },
                { "waist",     rbac::RBAC_PERM_COMMAND_DISP_WAIST,      false, Display<DISPLAY_TYPE_WAIST>,           "" },
                { "legs",      rbac::RBAC_PERM_COMMAND_DISP_LEGS,       false, Display<DISPLAY_TYPE_PANTS>,           "" },
                { "feet",      rbac::RBAC_PERM_COMMAND_DISP_FEET,       false, Display<DISPLAY_TYPE_BOOTS>,           "" },
                { "wrists",    rbac::RBAC_PERM_COMMAND_DISP_WRISTS,     false, Display<DISPLAY_TYPE_WRISTS>,          "" },
                { "hands",     rbac::RBAC_PERM_COMMAND_DISP_HANDS,      false, Display<DISPLAY_TYPE_HANDS>,           "" },
                { "back",      rbac::RBAC_PERM_COMMAND_DISP_BACK,       false, Display<DISPLAY_TYPE_BACK>,            "" },
                { "tabard",    rbac::RBAC_PERM_COMMAND_DISP_TABARD,     false, Display<DISPLAY_TYPE_TABARD>,          "" },
                { "mainhand",  rbac::RBAC_PERM_COMMAND_DISP_MAINHAND,   false, Display<DISPLAY_TYPE_MAIN>,            "" },
                { "offhand",   rbac::RBAC_PERM_COMMAND_DISP_OFFHAND,    false, Display<DISPLAY_TYPE_OFF>,             "" },
            };

            static ChatCommandTable commandTable =
            {
                { "display", displayCommandTable},
            };

            return commandTable;
        }

        // Optimized template method for processing display commands
        template <DisplayType T, bool secondary = false>
        static bool Display(ChatHandler* handler, char const* args)
        {
            if (!handler || !args)
                return false;

            char const* id = handler->extractKeyFromLink((char*)args, "Hitem");
            if (!id)
                return false;

            // Retrieve item ID
            uint32 itemId = static_cast<uint32>(strtoul(id, nullptr, 10));

            // Retrieve the bonus, if specified
            uint32 bonus = 0;
            char* bonusStr = strtok(NULL, " ");
            if (bonusStr)
                bonus = strtol(bonusStr, NULL, 10);

            // Call the display handler
            DisplayHandler::GetInstance().Display(handler, T, itemId, bonus, secondary);
            return true;
        }
    };
}

void AddSC_CustomDisplayCommands() { new RoleplayCore::DisplayCommands(); }
