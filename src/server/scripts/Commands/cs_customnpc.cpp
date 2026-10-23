#include "Chat.h"
#include "ChatCommand.h"
#include "ScriptMgr.h"
#include "RBAC.h"
#include "ObjectMgr.h" // sObjectManager
#include "RolePlay.h"
#include "CreatureOutfit.h" // CreatureOutfit, shared_ptr
#include "Player.h" // EquipmentSlots
#include "SharedDefines.h" // Gender
#include "Creature.h" // Creature
#include "Language.h"
#include "TransmogMgr.h"
#include "TemporarySummon.h"
#include "Transport.h"
#include "PhasingHandler.h"
#include "ObjectAccessor.h"
#include "DB2Structure.h"
#include <boost/algorithm/string.hpp>
#include <sstream>

using namespace Trinity::ChatCommands;

class customnpc_commandscript : public CommandScript
{
public:
    customnpc_commandscript() : CommandScript("customnpc_commandscript") { }

    std::span<ChatCommandBuilder const> GetCommands() const override
    {
        static ChatCommandTable customNpcSetFaceCommandTable =
        {
            { "self",       HandleCustomNpcSetFaceSelfCommand,       LANG_COMMAND_CUSTOMNPC_SET_FACE_HELP, rbac::RBAC_PERM_COMMAND_CUSTOMNPC_SET_FACE, Console::No },
            { "option",     HandleCustomNpcSetFaceOptionCommand,     LANG_COMMAND_CUSTOMNPC_SET_FACE_HELP, rbac::RBAC_PERM_COMMAND_CUSTOMNPC_SET_FACE, Console::No },
            { "copyoption", HandleCustomNpcSetFaceCopyOptionCommand, LANG_COMMAND_CUSTOMNPC_SET_FACE_HELP, rbac::RBAC_PERM_COMMAND_CUSTOMNPC_SET_FACE, Console::No },
            { "rawoption",  HandleCustomNpcSetFaceRawOptionCommand,  LANG_COMMAND_CUSTOMNPC_SET_FACE_HELP, rbac::RBAC_PERM_COMMAND_CUSTOMNPC_SET_FACE, Console::No },
            { "",           HandleCustomNpcSetFaceIndexedCommand,    LANG_COMMAND_CUSTOMNPC_SET_FACE_HELP, rbac::RBAC_PERM_COMMAND_CUSTOMNPC_SET_FACE, Console::No },
        };
        static ChatCommandTable customNpcRaceCommandTable =
        {
            { "list",       HandleCustomNpcRaceListCommand,          LANG_COMMAND_CUSTOMNPC_RACE_LIST_HELP, rbac::RBAC_PERM_COMMAND_CUSTOMNPC_SET_RACE, Console::No },
            { "options",    HandleCustomNpcRaceOptionsCommand,       LANG_COMMAND_CUSTOMNPC_RACE_OPTIONS_HELP, rbac::RBAC_PERM_COMMAND_CUSTOMNPC_SET_RACE, Console::No },
        };
        static ChatCommandTable customNpcFaceCommandTable =
        {
            { "list",       HandleCustomNpcFaceListCommand,          LANG_COMMAND_CUSTOMNPC_FACE_LIST_HELP, rbac::RBAC_PERM_COMMAND_CUSTOMNPC_SET_FACE, Console::No },
            { "choices",    HandleCustomNpcFaceChoicesCommand,       LANG_COMMAND_CUSTOMNPC_FACE_CHOICES_HELP, rbac::RBAC_PERM_COMMAND_CUSTOMNPC_SET_FACE, Console::No },
        };
        static ChatCommandTable customNpcModelAddCommandTable =
        {
            { "blank",      HandleCustomNpcModelAddBlankCommand,     LANG_COMMAND_CUSTOMNPC_MODEL_ADD_HELP, rbac::RBAC_PERM_COMMAND_CUSTOMNPC_CREATE, Console::No },
            { "self",       HandleCustomNpcModelAddSelfCommand,      LANG_COMMAND_CUSTOMNPC_MODEL_ADD_HELP, rbac::RBAC_PERM_COMMAND_CUSTOMNPC_CREATE, Console::No },
            { "player",     HandleCustomNpcModelAddPlayerCommand,    LANG_COMMAND_CUSTOMNPC_MODEL_ADD_HELP, rbac::RBAC_PERM_COMMAND_CUSTOMNPC_CREATE, Console::No },
            { "target",     HandleCustomNpcModelAddTargetCommand,    LANG_COMMAND_CUSTOMNPC_MODEL_ADD_HELP, rbac::RBAC_PERM_COMMAND_CUSTOMNPC_CREATE, Console::No },
            { "custom",     HandleCustomNpcModelAddCustomCommand,    LANG_COMMAND_CUSTOMNPC_MODEL_ADD_HELP, rbac::RBAC_PERM_COMMAND_CUSTOMNPC_CREATE, Console::No },
        };
        static ChatCommandTable customNpcModelCommandTable =
        {
            { "list",       HandleCustomNpcModelListCommand,         LANG_COMMAND_CUSTOMNPC_MODEL_LIST_HELP, rbac::RBAC_PERM_COMMAND_CUSTOMNPC_CREATE, Console::No },
            { "add",        customNpcModelAddCommandTable },
            { "copy",       HandleCustomNpcModelCopyCommand,         LANG_COMMAND_CUSTOMNPC_MODEL_COPY_HELP, rbac::RBAC_PERM_COMMAND_CUSTOMNPC_CREATE, Console::No },
            { "remove",     HandleCustomNpcModelRemoveCommand,       LANG_COMMAND_CUSTOMNPC_REMOVE_VARIATION_HELP, rbac::RBAC_PERM_COMMAND_CUSTOMNPC_REMOVE_VARIATION, Console::No },
            { "apply",      HandleCustomNpcModelApplyCommand,        LANG_COMMAND_CUSTOMNPC_MODEL_APPLY_HELP, rbac::RBAC_PERM_COMMAND_CUSTOMNPC_SPAWN, Console::No },
        };
        static ChatCommandTable customNpcEquipCopyCommandTable =
        {
            { "custom",     HandleCustomNpcEquipCopyCustomCommand,   LANG_COMMAND_CUSTOMNPC_EQUIP_COPY_HELP, rbac::RBAC_PERM_COMMAND_CUSTOMNPC_EQUIP_ARMOR, Console::No },
        };
        static ChatCommandTable customNpcSetCommandTable =
        {
            { "displayid", HandleCustomNpcSetDisplayIdCommand,     LANG_COMMAND_CUSTOMNPC_SET_DISPLAYID_HELP, rbac::RBAC_PERM_COMMAND_CUSTOMNPC_SET_DISPLAYID, Console::No },
            { "face",      customNpcSetFaceCommandTable },
            { "gender",    HandleCustomNpcSetGenderCommand,        LANG_COMMAND_CUSTOMNPC_SET_GENDER_HELP,    rbac::RBAC_PERM_COMMAND_CUSTOMNPC_SET_GENDER,    Console::No },
            { "guild",     HandleCustomNpcSetGuildCommand,         LANG_COMMAND_CUSTOMNPC_SET_GUILD_HELP,     rbac::RBAC_PERM_COMMAND_CUSTOMNPC_SET_GUILD,     Console::No },
            { "name",      HandleCustomNpcSetDisplayNameCommand,   LANG_COMMAND_CUSTOMNPC_SET_NAME_HELP,      rbac::RBAC_PERM_COMMAND_CUSTOMNPC_SET_DISPLAYNAME, Console::No },
            { "race",      HandleCustomNpcSetRaceCommand,          LANG_COMMAND_CUSTOMNPC_SET_RACE_HELP,      rbac::RBAC_PERM_COMMAND_CUSTOMNPC_SET_RACE,      Console::No },
            { "scale",     HandleCustomNpcSetScaleCommand,         LANG_COMMAND_CUSTOMNPC_SET_SCALE_HELP,     rbac::RBAC_PERM_COMMAND_CUSTOMNPC_SET_SCALE,     Console::No },
            { "standstate",HandleCustomNpcSetStandStateCommand,    LANG_COMMAND_CUSTOMNPC_SET_STANDSTATE_HELP, rbac::RBAC_PERM_COMMAND_CUSTOMNPC_SET_DISPLAYID, Console::No },
            { "subname",   HandleCustomNpcSetSubNameCommand,       LANG_COMMAND_CUSTOMNPC_SET_SUBNAME_HELP,   rbac::RBAC_PERM_COMMAND_CUSTOMNPC_SET_SUBNAME,   Console::No },
            { "tameable",  HandleCustomNpcSetTameableCommand,      LANG_COMMAND_CUSTOMNPC_SET_TAMEABLE_HELP,  rbac::RBAC_PERM_COMMAND_CUSTOMNPC_SET_TAMEABLE,  Console::No },
        };
        static ChatCommandTable customNpcEquipCommandTable =
        {
            { "armor",     HandleCustomNpcEquipArmorCommand,       LANG_COMMAND_CUSTOMNPC_EQUIP_ARMOR_HELP,  rbac::RBAC_PERM_COMMAND_CUSTOMNPC_EQUIP_ARMOR,  Console::No },
            { "copy",      customNpcEquipCopyCommandTable },
            { "left",      HandleCustomNpcEquipLeftHandCommand,    LANG_COMMAND_CUSTOMNPC_EQUIP_WEAPON_HELP, rbac::RBAC_PERM_COMMAND_CUSTOMNPC_EQUIP_LEFT,   Console::No },
            { "ranged",    HandleCustomNpcEquipRangedCommand,      LANG_COMMAND_CUSTOMNPC_EQUIP_WEAPON_HELP, rbac::RBAC_PERM_COMMAND_CUSTOMNPC_EQUIP_RANGED, Console::No },
            { "right",     HandleCustomNpcEquipRightHandCommand,   LANG_COMMAND_CUSTOMNPC_EQUIP_WEAPON_HELP, rbac::RBAC_PERM_COMMAND_CUSTOMNPC_EQUIP_RIGHT,  Console::No },
        };
        static ChatCommandTable customNpcUnequipCommandTable =
        {
            { "armor",     HandleCustomNpcUnequipArmorCommand,     LANG_COMMAND_CUSTOMNPC_UNEQUIP_ARMOR_HELP,  rbac::RBAC_PERM_COMMAND_CUSTOMNPC_UNEQUIP_ARMOR,  Console::No },
            { "left",      HandleCustomNpcUnequipLeftHandCommand,  LANG_COMMAND_CUSTOMNPC_UNEQUIP_WEAPON_HELP, rbac::RBAC_PERM_COMMAND_CUSTOMNPC_UNEQUIP_LEFT,   Console::No },
            { "ranged",    HandleCustomNpcUnequipRangedCommand,    LANG_COMMAND_CUSTOMNPC_UNEQUIP_WEAPON_HELP, rbac::RBAC_PERM_COMMAND_CUSTOMNPC_UNEQUIP_RANGED, Console::No },
            { "right",     HandleCustomNpcUnequipRightHandCommand, LANG_COMMAND_CUSTOMNPC_UNEQUIP_WEAPON_HELP, rbac::RBAC_PERM_COMMAND_CUSTOMNPC_UNEQUIP_RIGHT,  Console::No },
        };
        static ChatCommandTable CustomNpcSpawnCommandTable = {
            { "temp",      HandleCustomNpcSpawnTempCommand,        LANG_COMMAND_CUSTOMNPC_SPAWN_TEMP_HELP, rbac::RBAC_PERM_COMMAND_CUSTOMNPC_SPAWN, Console::No },
            { "",          HandleCustomNpcSpawnCommand,            LANG_COMMAND_CUSTOMNPC_SPAWN_HELP,      rbac::RBAC_PERM_COMMAND_CUSTOMNPC_SPAWN, Console::No },
        };

        static ChatCommandTable CustomNpcRemoveCommandTable = {
            { "variation", HandleCustomNpcRemoveVariationCommand, LANG_COMMAND_CUSTOMNPC_REMOVE_VARIATION_HELP, rbac::RBAC_PERM_COMMAND_CUSTOMNPC_REMOVE_VARIATION, Console::No },
        };

        static ChatCommandTable customNpcCloneCommandTable =
        {
            { "player",    HandleCustomNpcClonePlayerCommand,      LANG_COMMAND_CUSTOMNPC_CLONE_HELP, rbac::RBAC_PERM_COMMAND_CUSTOMNPC_CREATE, Console::No },
            { "target",    HandleCustomNpcCloneTargetCommand,      LANG_COMMAND_CUSTOMNPC_CLONE_TARGET_HELP, rbac::RBAC_PERM_COMMAND_CUSTOMNPC_CREATE, Console::No },
            { "custom",    HandleCustomNpcCloneCustomCommand,      LANG_COMMAND_CUSTOMNPC_CLONE_CUSTOM_HELP, rbac::RBAC_PERM_COMMAND_CUSTOMNPC_CREATE, Console::No },
            { "entry",     HandleCustomNpcCloneEntryCommand,       LANG_COMMAND_CUSTOMNPC_CLONE_ENTRY_HELP, rbac::RBAC_PERM_COMMAND_CUSTOMNPC_CREATE, Console::No },
            { "",          HandleCustomNpcCloneCommand,            LANG_COMMAND_CUSTOMNPC_CLONE_HELP,        rbac::RBAC_PERM_COMMAND_CUSTOMNPC_CREATE, Console::No },
        };

        static ChatCommandTable customNpcCommandTable =
        {
            // No blank "" invoker: bare `.cnpc` / `.help cnpc` use TC auto-help (USAGE + subcommands).
            { "add",       HandleCustomNpcCreateCommand,           LANG_COMMAND_CUSTOMNPC_ADD_HELP,    rbac::RBAC_PERM_COMMAND_CUSTOMNPC_CREATE, Console::No },
            { "apply",     HandleCustomNpcApplyCommand,            LANG_COMMAND_CUSTOMNPC_APPLY_HELP,  rbac::RBAC_PERM_COMMAND_CUSTOMNPC_SPAWN, Console::No },
            { "clone",     customNpcCloneCommandTable },
            { "delete",    HandleCustomNpcDeleteCommand,           LANG_COMMAND_CUSTOMNPC_DELETE_HELP, rbac::RBAC_PERM_COMMAND_CUSTOMNPC_DELETE, Console::No },
            { "diagnose",  HandleCustomNpcDiagnoseCommand,         LANG_COMMAND_CUSTOMNPC_DIAGNOSE_HELP, rbac::RBAC_PERM_COMMAND_CUSTOMNPC_CREATE, Console::No },
            { "equip",     customNpcEquipCommandTable },
            { "face",      customNpcFaceCommandTable },
            { "help",      HandleCustomNpcHelpCommand,             rbac::RBAC_PERM_COMMAND_CUSTOMNPC_CREATE, Console::No },
            { "import",    HandleCustomNpcImportCommand,           LANG_COMMAND_CUSTOMNPC_IMPORT_HELP, rbac::RBAC_PERM_COMMAND_CUSTOMNPC_CREATE, Console::No },
            { "list",      HandleCustomNpcListCommand,             LANG_COMMAND_CUSTOMNPC_LIST_HELP,   rbac::RBAC_PERM_COMMAND_CUSTOMNPC_CREATE, Console::No },
            { "model",     customNpcModelCommandTable },
            { "race",      customNpcRaceCommandTable },
            { "reload",    HandleCustomNpcReloadCommand,           LANG_COMMAND_CUSTOMNPC_RELOAD_HELP, rbac::RBAC_PERM_COMMAND_CUSTOMNPC_CREATE, Console::No },
            { "refresh",   HandleCustomNpcRefreshCommand,          LANG_COMMAND_CUSTOMNPC_REFRESH_HELP, rbac::RBAC_PERM_COMMAND_CUSTOMNPC_CREATE, Console::No },
            { "remove",    CustomNpcRemoveCommandTable },
            { "set",       customNpcSetCommandTable },
            { "spawn",     CustomNpcSpawnCommandTable },
            { "unequip",   customNpcUnequipCommandTable }
        };
        static ChatCommandTable commandTable =
        {
            { "customnpc", customNpcCommandTable },
            { "cnpc",      customNpcCommandTable }
        };
        return commandTable;
    }

    static bool HandleCustomNpcHelpCommand(ChatHandler* handler)
    {
        // Compact TC auto-help (same as bare `.cnpc` / `.help cnpc`); not trinity_string 11060 flood.
        Trinity::ChatCommands::SendCommandHelpFor(*handler, "cnpc");
        return true;
    }

    static bool EnsureCanEditCustomNpc(ChatHandler* handler, std::string const& key)
    {
        Player* player = handler->GetPlayer();
        bool allowAdmin = handler->GetSession() && handler->GetSession()->GetSecurity() >= SEC_ADMINISTRATOR;
        if (sRoleplay->CanEditCustomNpc(key, player, allowAdmin))
        {
            sRoleplay->EnsureCustomNpcOwner(key, player);
            return true;
        }

        std::string owner = sRoleplay->GetCustomNpcOwnerDisplay(key);
        handler->PSendSysMessage("Custom NPC %s belongs to bnet account %s.", key.c_str(), owner.c_str());
        handler->SetSentErrorMessage(true);
        return false;
    }

    static bool IsSelfEquipArg(Variant<Hyperlink<item>, uint32, std::string_view> const& itemArg)
    {
        if (std::string_view const* text = std::get_if<std::string_view>(&itemArg))
            return StringEqualI(std::string(*text), "self");

        return false;
    }

    static ItemTemplate const* ResolveItemTemplate(Variant<Hyperlink<item>, uint32, std::string_view> const& itemArg)
    {
        if (Hyperlink<::item> const* itemLinkData = std::get_if<Hyperlink<::item>>(&itemArg))
            return (*itemLinkData)->Item;

        if (uint32 const* itemId = std::get_if<uint32>(&itemArg))
            return sObjectMgr->GetItemTemplate(*itemId);

        return nullptr;
    }

    static bool ResolveArmorSlotName(std::string const& slotName, EquipmentSlots& slot, bool& allSlots)
    {
        allSlots = false;
        if (StringEqualI(slotName, "all"))
        {
            allSlots = true;
            return true;
        }

        slot = EQUIPMENT_SLOT_END;
        if (slotName == "head") slot = EQUIPMENT_SLOT_HEAD;
        else if (slotName == "shoulders") slot = EQUIPMENT_SLOT_SHOULDERS;
        else if (slotName == "body") slot = EQUIPMENT_SLOT_BODY;
        else if (slotName == "chest") slot = EQUIPMENT_SLOT_CHEST;
        else if (slotName == "waist") slot = EQUIPMENT_SLOT_WAIST;
        else if (slotName == "legs") slot = EQUIPMENT_SLOT_LEGS;
        else if (slotName == "feet") slot = EQUIPMENT_SLOT_FEET;
        else if (slotName == "wrists") slot = EQUIPMENT_SLOT_WRISTS;
        else if (slotName == "hands") slot = EQUIPMENT_SLOT_HANDS;
        else if (slotName == "tabard") slot = EQUIPMENT_SLOT_TABARD;
        else if (slotName == "back") slot = EQUIPMENT_SLOT_BACK;

        return slot != EQUIPMENT_SLOT_END;
    }

    static Optional<uint8> ParseStandStateName(std::string const& name)
    {
        if (StringEqualI(name, "stand")) return UNIT_STAND_STATE_STAND;
        if (StringEqualI(name, "sit")) return UNIT_STAND_STATE_SIT;
        if (StringEqualI(name, "sleep")) return UNIT_STAND_STATE_SLEEP;
        if (StringEqualI(name, "kneel")) return UNIT_STAND_STATE_KNEEL;
        if (StringEqualI(name, "dead")) return UNIT_STAND_STATE_DEAD;
        if (StringEqualI(name, "submerged")) return UNIT_STAND_STATE_SUBMERGED;

        if (Optional<uint32> value = Trinity::StringTo<uint32>(name))
        {
            if (*value <= UNIT_STAND_STATE_SUBMERGED)
                return uint8(*value);
        }

        return {};
    }

    static bool ValidateCustomNpcVariation(ChatHandler* handler, std::string const& name, uint8& variation, bool equipmentVariation)
    {
        if (variation < 1)
            variation = 1;

        uint8 modelCount = equipmentVariation
            ? sRoleplay->GetEquipmentVariationCountForNpc(name)
            : sRoleplay->GetModelVariationCountForNpc(name);

        if ((modelCount + 1) < variation)
        {
            handler->PSendSysMessage(equipmentVariation
                ? "The highest equipment set variation for Custom NPC '%s' is '%u'. The highest variation that can be added at the moment is '%u'."
                : "The highest model variation for Custom NPC '%s' is '%u'. The highest variation that can be added at the moment is '%u'.",
                name.c_str(), modelCount, modelCount + 1);
            handler->SetSentErrorMessage(true);
            return false;
        }

        return true;
    }

    static bool HandleCustomNpcCreateCommand(ChatHandler* handler, Tail displayName)
    {
        std::string displayNameString(displayName);
        boost::trim(displayNameString);
        if (displayNameString.empty())
        {
            handler->SendSysMessage("Usage: .cnpc add <display name>");
            handler->SetSentErrorMessage(true);
            return false;
        }

        std::string key = sRoleplay->GenerateCustomNpcKey(handler->GetPlayer());
        if (key.empty())
        {
            handler->SendSysMessage("Custom NPC key was not generated: your session has no bnet account id.");
            handler->SetSentErrorMessage(true);
            return false;
        }

        if (!sRoleplay->CreateBlankCustomNpc(handler->GetPlayer(), key, displayNameString))
        {
            handler->PSendSysMessage("Blank custom NPC %s was not created. Check server log category 'roleplay' for details.", displayNameString.c_str());
            handler->SetSentErrorMessage(true);
            return false;
        }

        handler->PSendSysMessage("Blank custom NPC %s created with key %s.", displayNameString.c_str(), key.c_str());
        return true;
    }

    static bool HandleCustomNpcCloneCommand(ChatHandler* handler, Tail displayName)
    {
        std::string displayNameString(displayName);
        boost::trim(displayNameString);
        if (displayNameString.empty())
        {
            handler->SendSysMessage("Usage: .cnpc clone <display name>");
            handler->SetSentErrorMessage(true);
            return false;
        }

        Player* source = handler->GetPlayer();
        std::string key = sRoleplay->GenerateCustomNpcKey(handler->GetPlayer());
        if (key.empty())
        {
            handler->SendSysMessage("Custom NPC key was not generated: your session has no bnet account id.");
            handler->SetSentErrorMessage(true);
            return false;
        }

        if (!sRoleplay->CreateCustomNpcFromPlayer(source, key, displayNameString, handler->GetPlayer()))
        {
            handler->PSendSysMessage("Custom NPC %s was not cloned. Check server log category 'roleplay' for details.", displayNameString.c_str());
            handler->SetSentErrorMessage(true);
            return false;
        }

        handler->PSendSysMessage("Custom NPC %s cloned from player %s with key %s.", displayNameString.c_str(), source->GetName().c_str(), key.c_str());
        return true;
    }

    static bool HandleCustomNpcClonePlayerCommand(ChatHandler* handler, PlayerIdentifier target, Tail displayName)
    {
        std::string displayNameString(displayName);
        boost::trim(displayNameString);
        if (displayNameString.empty())
        {
            handler->SendSysMessage("Usage: .cnpc clone player <player> <display name>");
            handler->SetSentErrorMessage(true);
            return false;
        }

        Player* source = target.GetConnectedPlayer();
        if (!source)
        {
            handler->PSendSysMessage("Player %s is not online.", target.GetName().c_str());
            handler->SetSentErrorMessage(true);
            return false;
        }

        std::string key = sRoleplay->GenerateCustomNpcKey(handler->GetPlayer());
        if (key.empty())
        {
            handler->SendSysMessage("Custom NPC key was not generated: your session has no bnet account id.");
            handler->SetSentErrorMessage(true);
            return false;
        }

        if (!sRoleplay->CreateCustomNpcFromPlayer(source, key, displayNameString, handler->GetPlayer()))
        {
            handler->PSendSysMessage("Custom NPC %s was not cloned. Check server log category 'roleplay' for details.", displayNameString.c_str());
            handler->SetSentErrorMessage(true);
            return false;
        }

        handler->PSendSysMessage("Custom NPC %s cloned from player %s with key %s.", displayNameString.c_str(), source->GetName().c_str(), key.c_str());
        return true;
    }

    static bool HandleCustomNpcCloneTargetCommand(ChatHandler* handler, Tail displayName)
    {
        std::string displayNameString(displayName);
        boost::trim(displayNameString);
        if (displayNameString.empty())
        {
            handler->SendSysMessage("Usage: .cnpc clone target <display name>");
            handler->SetSentErrorMessage(true);
            return false;
        }

        Creature* target = handler->getSelectedCreature();
        if (!target)
        {
            handler->SendSysMessage("Select a creature with outfit first.");
            handler->SetSentErrorMessage(true);
            return false;
        }

        std::string key = sRoleplay->GenerateCustomNpcKey(handler->GetPlayer());
        if (key.empty())
        {
            handler->SendSysMessage("Custom NPC key was not generated: your session has no bnet account id.");
            handler->SetSentErrorMessage(true);
            return false;
        }

        if (!sRoleplay->CreateCustomNpcFromCreature(target, key, displayNameString, handler->GetPlayer()))
        {
            handler->PSendSysMessage("Custom NPC %s was not cloned from target. Check server log category 'roleplay' for details.", displayNameString.c_str());
            handler->SetSentErrorMessage(true);
            return false;
        }

        handler->PSendSysMessage("Custom NPC %s cloned from selected creature with key %s.", displayNameString.c_str(), key.c_str());
        return true;
    }

    static bool HandleCustomNpcImportCommand(ChatHandler* handler, uint32 entry, std::string const& key)
    {
        if (!sRoleplay->ImportCustomNpcFromEntry(entry, key))
        {
            handler->PSendSysMessage("Entry %u was not imported as Custom NPC %s. Check server log category 'roleplay' for details.", entry, key.c_str());
            handler->SetSentErrorMessage(true);
            return false;
        }
        sRoleplay->EnsureCustomNpcOwner(key, handler->GetPlayer());

        handler->PSendSysMessage("Entry %u registered as Custom NPC %s.", entry, key.c_str());
        return true;
    }

    static bool HandleCustomNpcReloadCommand(ChatHandler* handler, Tail keyOrAll)
    {
        std::string arg = keyOrAll.data();
        boost::trim(arg);
        if (arg.empty())
        {
            handler->SendSysMessage("Usage: .cnpc reload <key|all>");
            handler->SetSentErrorMessage(true);
            return false;
        }

        bool ok;
        if (StringEqualI(arg, "all"))
        {
            CustomNpcReloadSummary summary = sRoleplay->ReloadAllCustomNpcsFromDb();
            handler->PSendSysMessage("Reloaded %u custom NPC(s), %u failed. See Server.log category 'roleplay' for details.",
                summary.successCount, summary.failedCount);
            return true;
        }
        else
            ok = sRoleplay->ReloadCustomNpcFromDb(arg);

        if (!ok)
        {
            handler->PSendSysMessage("Custom NPC %s was not reloaded. Check server log category 'roleplay' for details.", arg.c_str());
            handler->SetSentErrorMessage(true);
            return false;
        }

        handler->PSendSysMessage("Custom NPC %s reloaded from database.", arg.c_str());
        return true;
    }

    static bool HandleCustomNpcApplyCommand(ChatHandler* handler, uint8 variation)
    {
        if (variation < 1)
        {
            handler->SendSysMessage("Variation must be 1 or higher.");
            handler->SetSentErrorMessage(true);
            return false;
        }

        Player* player = handler->GetPlayer();
        ObjectGuid::LowType selectedGuid = sRoleplay->GetSelectedCreatureGuidFromPlayer(player->GetGUID().GetCounter());
        Creature* creature = selectedGuid ? sRoleplay->GetAnyCreature(selectedGuid) : nullptr;
        if (!creature)
        {
            handler->SendSysMessage("No selected creature found. Use .npc select first.");
            handler->SetSentErrorMessage(true);
            return false;
        }

        std::string const* key = sRoleplay->GetCustomNpcKeyForEntry(creature->GetEntry());
        if (!key)
        {
            handler->SendSysMessage("Selected creature is not a custom NPC.");
            handler->SetSentErrorMessage(true);
            return false;
        }

        uint8 modelCount = sRoleplay->GetModelVariationCountForNpc(*key);
        if (variation > modelCount)
        {
            handler->PSendSysMessage("Custom NPC %s has only %u model variation(s). Variation %u does not exist.",
                key->c_str(), modelCount, variation);
            handler->SetSentErrorMessage(true);
            return false;
        }

        if (!sRoleplay->ApplyCustomNpcToCreature(*key, creature, variation))
        {
            handler->PSendSysMessage("Failed to apply variation %u to selected creature.", variation);
            handler->SetSentErrorMessage(true);
            return false;
        }

        handler->PSendSysMessage("Applied custom NPC %s variation %u to selected creature.", key->c_str(), variation);
        return true;
    }

    static bool HandleCustomNpcListCommand(ChatHandler* handler)
    {
        CustomNpcDataContainer const customNpcs = sRoleplay->GetCustomNpcContainer();
        if (customNpcs.empty())
        {
            handler->SendSysMessage("No custom NPCs registered.");
            return true;
        }

        handler->PSendSysMessage("Registered custom NPCs: %u", uint32(customNpcs.size()));
        for (auto const& pair : customNpcs)
        {
            CustomNpcData const& npc = pair.second;
            std::string owner = sRoleplay->GetCustomNpcOwnerDisplay(npc.key);
            handler->PSendSysMessage("- %s (entry %u, owner %s, models %u, equip %u, spawns %u)",
                npc.key.c_str(), npc.templateId, owner.c_str(), sRoleplay->GetModelVariationCountForNpc(npc.key),
                sRoleplay->GetEquipmentVariationCountForNpc(npc.key), uint32(npc.spawns.size()));
        }
        return true;
    }

    static bool HandleCustomNpcSpawnCommand(ChatHandler* handler, std::string const& name)
    {
        if (!sRoleplay->CustomNpcNameExists(name)) {
            handler->PSendSysMessage("There is no Custom NPC with the name: %s", name.c_str());
            handler->SetSentErrorMessage(true);
            return false;
        }

        Player* chr = handler->GetSession()->GetPlayer();
        Map* map = chr->GetMap();
        uint32 id = sRoleplay->GetEntryIdForNpc(name);

        if (Transport* trans = dynamic_cast<Transport*>(chr->GetTransport()))
        {
            ObjectGuid::LowType guid = sObjectMgr->GenerateCreatureSpawnId();
            CreatureData& data = sObjectMgr->NewOrExistCreatureData(guid);
            data.spawnId = guid;
            data.spawnGroupData = sObjectMgr->GetDefaultSpawnGroup();
            data.id = id;
            data.spawnPoint.Relocate(chr->GetTransOffsetX(), chr->GetTransOffsetY(), chr->GetTransOffsetZ(), chr->GetTransOffsetO());
            if (Creature* creature = trans->CreateNPCPassenger(guid, &data))
            {
                creature->SaveToDB(trans->GetGOInfo()->moTransport.SpawnMap, { map->GetDifficultyID() });
                sObjectMgr->AddCreatureToGrid(&data);
            }
            return true;
        }

        Creature* creature = Creature::CreateCreature(id, map, chr->GetPosition(), InitialPhaseShift(chr));
        if (!creature)
        {
            handler->PSendSysMessage("Could not spawn customnpc '%s', this can happen when an unknown displayid is set.", name.c_str());
            handler->SetSentErrorMessage(true);
            return false;
        }

        uint8 variation = creature->GetCurrentEquipmentId();
        if (!variation)
            variation = 1;

        creature->SaveToDB(map->GetId(), { map->GetDifficultyID() });
        ObjectGuid::LowType db_guid = creature->GetSpawnId();

        creature->CleanupsBeforeDelete();
        delete creature;

        creature = Creature::CreateCreatureFromDB(db_guid, map, true, true);
        if (!creature)
        {
            handler->PSendSysMessage("Could not spawn customnpc '%u', this can happen when an unknown displayid is set.", id);
            handler->SetSentErrorMessage(true);
            return false;
        }

        sObjectMgr->AddCreatureToGrid(sObjectMgr->GetCreatureData(db_guid));

        sRoleplay->SaveCreature(creature);
        sRoleplay->SetCreatureSelectionForPlayer(chr->GetGUID().GetCounter(), creature->GetSpawnId());

        sRoleplay->LoadCustomNpcSpawn(id, db_guid);
        handler->PSendSysMessage("Custom NPC %s spawned (variation %u).", name.c_str(), variation);
        return true;
    }

    static bool HandleCustomNpcSpawnTempCommand(ChatHandler* handler, std::string const& key)
    {
        if (!sRoleplay->CustomNpcNameExists(key)) {
            handler->PSendSysMessage("There is no Custom NPC with the name: %s", key.c_str());
            handler->SetSentErrorMessage(true);
            return false;
        }

        Player* chr = handler->GetSession()->GetPlayer();
        chr->SummonCreature(sRoleplay->GetEntryIdForNpc(key), chr->GetPosition(), TEMPSUMMON_CORPSE_DESPAWN, 30s);
        handler->PSendSysMessage("Custom NPC %s temporarily summoned (random variation if multiple exist).", key.c_str());
        return true;
    }

    static bool HandleCustomNpcSetRaceCommand(ChatHandler* handler, std::string const& name, Races race, Optional<uint8> variationId)
    {
        if (!sRoleplay->CustomNpcNameExists(name)) {
            handler->PSendSysMessage("There is no Custom NPC with the name: %s", name.c_str());
            handler->SetSentErrorMessage(true);
            return false;
        }
        if (!EnsureCanEditCustomNpc(handler, name))
            return false;

        uint8 variation = variationId.value_or(1);
        if (variation < 1) {
            variation = 1;
        }

        uint8 modelCount = sRoleplay->GetModelVariationCountForNpc(name);
        if ((modelCount + 1) < variation) {
            handler->PSendSysMessage("The highest model variation for Custom NPC '%s' is '%u'. The highest variation that can be added at the moment is '%u'.", name.c_str(), modelCount, modelCount + 1);
            handler->SetSentErrorMessage(true);
            return false;
        }

        std::string error;
        if (!sRoleplay->SetCustomNpcOutfitRace(name, variation, race, error))
        {
            handler->PSendSysMessage("Failed to set race for %s: %s", name.c_str(), error.c_str());
            handler->SetSentErrorMessage(true);
            return false;
        }

        handler->PSendSysMessage("Race for NPC %s, model variation '%u' set to %u!", name.c_str(), variation, race);
        return true;
    }

    static bool HandleCustomNpcSetGenderCommand(ChatHandler* handler, std::string const& name, uint8 gender, Optional<uint8> variationId)
    {
        if (!sRoleplay->CustomNpcNameExists(name)) {
            handler->PSendSysMessage("There is no Custom NPC with the name: %s", name.c_str());
            handler->SetSentErrorMessage(true);
            return false;
        }
        if (!EnsureCanEditCustomNpc(handler, name))
            return false;

        uint8 variation = variationId.value_or(1);
        if (variation < 1) {
            variation = 1;
        }

        uint8 modelCount = sRoleplay->GetModelVariationCountForNpc(name);
        if ((modelCount + 1) < variation) {
            handler->PSendSysMessage("The highest model variation for Custom NPC '%s' is '%u'. The highest variation that can be added at the moment is '%u'.", name.c_str(), modelCount, modelCount + 1);
            handler->SetSentErrorMessage(true);
            return false;
        }

        std::string error;
        if (!sRoleplay->SetCustomNpcOutfitGender(name, variation, gender == 0 ? GENDER_MALE : GENDER_FEMALE, error))
        {
            handler->PSendSysMessage("Failed to set gender for %s: %s", name.c_str(), error.c_str());
            handler->SetSentErrorMessage(true);
            return false;
        }

        handler->PSendSysMessage("Gender for NPC %s, model variation '%u' set to %u!", name.c_str(), variation, gender);
        return true;
    }

    static bool HandleCustomNpcSetDisplayNameCommand(ChatHandler* handler, std::string const& name, Tail displayName)
    {
        if (!sRoleplay->CustomNpcNameExists(name)) {
            handler->PSendSysMessage("There is no Custom NPC with the name: %s", name.c_str());
            handler->SetSentErrorMessage(true);
            return false;
        }
        if (!EnsureCanEditCustomNpc(handler, name))
            return false;

        std::string displayNameString(displayName);
        boost::trim(displayNameString);
        if (displayNameString.empty()) {
            handler->PSendSysMessage("You must provide a name after the key i.e. `.cnpc set name mynpc 7th Legion Infantry`");
            handler->SetSentErrorMessage(true);
            return false;
        }

        sRoleplay->SetCustomNpcName(name, displayNameString);
        handler->PSendSysMessage("Name for NPC %s set to %s!", name.c_str(), displayNameString.c_str());
        return true;
    }

    static bool HandleCustomNpcSetSubNameCommand(ChatHandler* handler, std::string const& name, Tail subName)
    {
        if (!sRoleplay->CustomNpcNameExists(name)) {
            handler->PSendSysMessage("There is no Custom NPC with the name: %s", name.c_str());
            handler->SetSentErrorMessage(true);
            return false;
        }
        if (!EnsureCanEditCustomNpc(handler, name))
            return false;

        std::string subNameString(subName);
        boost::trim(subNameString);
        if (subNameString.empty()) {
            handler->PSendSysMessage("You must provide a subname after the key i.e. `.cnpc set subname mynpc 7th Legion`");
            handler->SetSentErrorMessage(true);
            return false;
        }

        sRoleplay->SetCustomNpcSubName(name, subNameString);
        handler->PSendSysMessage("Subname for NPC %s set to %s!", name.c_str(), subNameString.c_str());
        return true;
    }

    static bool HandleCustomNpcEquipArmorCommand(ChatHandler* handler, std::string const& name, std::string const& slotName, Variant<Hyperlink<item>, uint32, std::string_view> const& itemArg, Optional<uint8> variationId, Optional<uint32> modAppearanceId)
    {
        if (!sRoleplay->CustomNpcNameExists(name)) {
            handler->PSendSysMessage("There is no Custom NPC with the name: %s", name.c_str());
            handler->SetSentErrorMessage(true);
            return false;
        }
        if (!EnsureCanEditCustomNpc(handler, name))
            return false;

        uint8 variation = variationId.value_or(1);
        if (!ValidateCustomNpcVariation(handler, name, variation, false))
            return false;

        EquipmentSlots slot = EQUIPMENT_SLOT_END;
        bool allSlots = false;
        if (!ResolveArmorSlotName(slotName, slot, allSlots))
        {
            handler->PSendSysMessage("Unrecognized slot '%s'. Slot must be all or one of head|shoulders|body|chest|waist|legs|feet|wrists|hands|tabard|back.", slotName.c_str());
            handler->SetSentErrorMessage(true);
            return false;
        }

        if (IsSelfEquipArg(itemArg))
        {
            if (allSlots)
                sRoleplay->CopyCustomNpcArmorFromPlayer(name, variation, handler->GetPlayer());
            else
                sRoleplay->CopyCustomNpcArmorSlotFromPlayer(name, variation, slot, handler->GetPlayer());

            handler->PSendSysMessage("Visible armor copied from yourself to custom NPC %s, model variation '%u'!", name.c_str(), variation);
            return true;
        }

        if (allSlots)
        {
            handler->SendSysMessage("Usage: .cnpc equip armor <key> <slot|all> <itemlink|itemId|self> [variation] [appearanceMod]");
            handler->SetSentErrorMessage(true);
            return false;
        }

        ItemTemplate const* item = ResolveItemTemplate(itemArg);
        if (!item)
        {
            handler->SendSysMessage(LANG_COMMAND_NEEDITEMSEND);
            handler->SetSentErrorMessage(true);
            return false;
        }

        uint32 appearanceMod = modAppearanceId.value_or(0);
        if (!modAppearanceId.has_value())
        {
            for (ItemModifiedAppearanceEntry const* appearanceModEntry : sItemModifiedAppearanceStore)
            {
                if ((uint32)appearanceModEntry->ItemID == item->GetId() && appearanceModEntry->OrderIndex == 0)
                {
                    appearanceMod = appearanceModEntry->ItemAppearanceModifierID;
                    break;
                }
            }
        }

        int32 displayId = sRoleplay->GetItemDisplayIdFromTemplate(item, appearanceMod);
        if (!displayId)
        {
            handler->SendSysMessage("The item has no visible appearance for the selected slot.");
            handler->SetSentErrorMessage(true);
            return false;
        }

        sRoleplay->SetCustomNpcOutfitEquipmentSlot(name, variation, slot, displayId);
        handler->PSendSysMessage("Armor equipped to custom NPC %s slot %s, model variation '%u'!", name.c_str(), slotName.c_str(), variation);
        return true;
    }

    static bool HandleCustomNpcEquipLeftHandCommand(ChatHandler* handler, std::string const& name, Variant<Hyperlink<item>, uint32, std::string_view> const& itemArg, Optional<uint8> variationId, Optional<uint32> modAppearanceId)
    {
        if (!sRoleplay->CustomNpcNameExists(name)) {
            handler->PSendSysMessage("There is no Custom NPC with the name: %s", name.c_str());
            handler->SetSentErrorMessage(true);
            return false;
        }
        if (!EnsureCanEditCustomNpc(handler, name))
            return false;

        uint8 variation = variationId.value_or(1);
        if (variation < 1) {
            variation = 1;
        }

        uint8 modelCount = sRoleplay->GetEquipmentVariationCountForNpc(name);
        if ((modelCount + 1) < variation) {
            handler->PSendSysMessage("The highest equipment set variation for Custom NPC '%s' is '%u'. The highest variation that can be added at the moment is '%u'.", name.c_str(), modelCount, modelCount + 1);
            handler->SetSentErrorMessage(true);
            return false;
        }

        if (IsSelfEquipArg(itemArg))
        {
            if (!sRoleplay->CopyCustomNpcWeaponFromPlayer(name, variation, 1, handler->GetPlayer()))
            {
                handler->SendSysMessage("You have no visible left-hand item to copy.");
                handler->SetSentErrorMessage(true);
                return false;
            }

            handler->PSendSysMessage("Left-hand weapon copied from yourself to custom NPC %s, equipment variation '%u'!", name.c_str(), variation);
            return true;
        }

        ItemTemplate const* item = ResolveItemTemplate(itemArg);
        if (!item)
        {
            handler->SendSysMessage(LANG_COMMAND_NEEDITEMSEND);
            handler->SetSentErrorMessage(true);
            return false;
        }

        if (!(item->IsWeapon() || item->GetInventoryType() == INVTYPE_HOLDABLE || item->GetInventoryType() == INVTYPE_SHIELD)) {
            handler->SendSysMessage("The item needs to be a weapon, holdable item or shield.");
            handler->SetSentErrorMessage(true);
            return false;
        }

        sRoleplay->SetCustomNpcLeftHand(name, variation, item->GetId(), modAppearanceId.value_or(0));
        handler->PSendSysMessage("Weapon equipped to custom NPC %s, equipment variation '%u'!", name.c_str(), variation);
        return true;
    }

    static bool HandleCustomNpcEquipRightHandCommand(ChatHandler* handler, std::string const& name, Variant<Hyperlink<item>, uint32, std::string_view> const& itemArg, Optional<uint8> variationId, Optional<uint32> modAppearanceId)
    {
        if (!sRoleplay->CustomNpcNameExists(name)) {
            handler->PSendSysMessage("There is no Custom NPC with the name: %s", name.c_str());
            handler->SetSentErrorMessage(true);
            return false;
        }
        if (!EnsureCanEditCustomNpc(handler, name))
            return false;

        uint8 variation = variationId.value_or(1);
        if (variation < 1) {
            variation = 1;
        }

        uint8 modelCount = sRoleplay->GetEquipmentVariationCountForNpc(name);
        if ((modelCount + 1) < variation) {
            handler->PSendSysMessage("The highest equipment set variation for Custom NPC '%s' is '%u'. The highest variation that can be added at the moment is '%u'.", name.c_str(), modelCount, modelCount + 1);
            handler->SetSentErrorMessage(true);
            return false;
        }

        if (IsSelfEquipArg(itemArg))
        {
            if (!sRoleplay->CopyCustomNpcWeaponFromPlayer(name, variation, 0, handler->GetPlayer()))
            {
                handler->SendSysMessage("You have no visible right-hand item to copy.");
                handler->SetSentErrorMessage(true);
                return false;
            }

            handler->PSendSysMessage("Right-hand weapon copied from yourself to custom NPC %s, equipment variation '%u'!", name.c_str(), variation);
            return true;
        }

        ItemTemplate const* item = ResolveItemTemplate(itemArg);
        if (!item)
        {
            handler->SendSysMessage(LANG_COMMAND_NEEDITEMSEND);
            handler->SetSentErrorMessage(true);
            return false;
        }

        if (!(item->IsWeapon() || item->GetInventoryType() == INVTYPE_HOLDABLE || item->GetInventoryType() == INVTYPE_SHIELD)) {
            handler->SendSysMessage("The item needs to be a weapon, holdable item or shield.");
            handler->SetSentErrorMessage(true);
            return false;
        }

        sRoleplay->SetCustomNpcRightHand(name, variation, item->GetId(), modAppearanceId.value_or(0));
        handler->PSendSysMessage("Weapon equipped to custom NPC %s, equipment variation '%u'!", name.c_str(), variation);
        return true;
    }

    static bool HandleCustomNpcEquipRangedCommand(ChatHandler* handler, std::string const& name, Variant<Hyperlink<item>, uint32, std::string_view> const& itemArg, Optional<uint8> variationId, Optional<uint32> modAppearanceId)
    {
        if (!sRoleplay->CustomNpcNameExists(name)) {
            handler->PSendSysMessage("There is no Custom NPC with the name: %s", name.c_str());
            handler->SetSentErrorMessage(true);
            return false;
        }
        if (!EnsureCanEditCustomNpc(handler, name))
            return false;

        uint8 variation = variationId.value_or(1);
        if (variation < 1) {
            variation = 1;
        }

        uint8 modelCount = sRoleplay->GetEquipmentVariationCountForNpc(name);
        if ((modelCount + 1) < variation) {
            handler->PSendSysMessage("The highest equipment set variation for Custom NPC '%s' is '%u'. The highest variation that can be added at the moment is '%u'.", name.c_str(), modelCount, modelCount + 1);
            handler->SetSentErrorMessage(true);
            return false;
        }

        if (IsSelfEquipArg(itemArg))
        {
            if (!sRoleplay->CopyCustomNpcWeaponFromPlayer(name, variation, 2, handler->GetPlayer()))
            {
                handler->SendSysMessage("You have no visible ranged item to copy.");
                handler->SetSentErrorMessage(true);
                return false;
            }

            handler->PSendSysMessage("Ranged weapon copied from yourself to custom NPC %s, equipment variation '%u'!", name.c_str(), variation);
            return true;
        }

        ItemTemplate const* item = ResolveItemTemplate(itemArg);
        if (!item)
        {
            handler->SendSysMessage(LANG_COMMAND_NEEDITEMSEND);
            handler->SetSentErrorMessage(true);
            return false;
        }

        if (!(item->IsRangedWeapon())) {
            handler->SendSysMessage("The item needs to be a ranged weapon.");
            handler->SetSentErrorMessage(true);
            return false;
        }

        sRoleplay->SetCustomNpcRanged(name, variation, item->GetId(), modAppearanceId.value_or(0));
        handler->PSendSysMessage("Ranged weapon equipped to custom NPC %s, equipment variation '%u'!", name.c_str(), variation);
        return true;
    }

    static bool HandleCustomNpcSetFaceSelfCommand(ChatHandler* handler, std::string const& name, Optional<uint8> variationId)
    {
        if (!sRoleplay->CustomNpcNameExists(name)) {
            handler->PSendSysMessage("There is no Custom NPC with the name: %s", name.c_str());
            handler->SetSentErrorMessage(true);
            return false;
        }
        if (!EnsureCanEditCustomNpc(handler, name))
            return false;

        uint8 variation = variationId.value_or(1);
        if (!ValidateCustomNpcVariation(handler, name, variation, false))
            return false;

        std::string error;
        if (!sRoleplay->SetCustomNpcFaceFromSelf(name, variation, handler->GetPlayer(), error))
        {
            handler->PSendSysMessage("Failed to copy face for %s: %s", name.c_str(), error.c_str());
            handler->SetSentErrorMessage(true);
            return false;
        }

        handler->PSendSysMessage("Custom NPC %s, model variation %u has copied your face!", name.c_str(), variation);
        return true;
    }

    static bool HandleCustomNpcSetFaceOptionCommand(ChatHandler* handler, std::string const& name, uint32 optionIndex, uint32 choiceIndex, Optional<uint8> variationId)
    {
        if (!sRoleplay->CustomNpcNameExists(name)) {
            handler->PSendSysMessage("There is no Custom NPC with the name: %s", name.c_str());
            handler->SetSentErrorMessage(true);
            return false;
        }
        if (!EnsureCanEditCustomNpc(handler, name))
            return false;

        uint8 variation = variationId.value_or(1);
        if (!ValidateCustomNpcVariation(handler, name, variation, false))
            return false;

        std::string error;
        if (!sRoleplay->SetCustomNpcCustomizationByIndex(name, variation, optionIndex, choiceIndex, error))
        {
            handler->PSendSysMessage("Failed to set face for %s: %s", name.c_str(), error.c_str());
            handler->SetSentErrorMessage(true);
            return false;
        }

        handler->PSendSysMessage("Custom NPC %s, model variation %u: option index %u set to choice index %u.", name.c_str(), variation, optionIndex, choiceIndex);
        return true;
    }

    static bool HandleCustomNpcSetFaceCopyOptionCommand(ChatHandler* handler, std::string const& name, uint32 optionIndex, Optional<uint8> variationId)
    {
        if (!sRoleplay->CustomNpcNameExists(name)) {
            handler->PSendSysMessage("There is no Custom NPC with the name: %s", name.c_str());
            handler->SetSentErrorMessage(true);
            return false;
        }
        if (!EnsureCanEditCustomNpc(handler, name))
            return false;

        uint8 variation = variationId.value_or(1);
        if (!ValidateCustomNpcVariation(handler, name, variation, false))
            return false;

        std::string error;
        if (!sRoleplay->CopyCustomNpcCustomizationOptionByIndex(name, variation, optionIndex, handler->GetPlayer(), error))
        {
            handler->PSendSysMessage("Failed to copy face option for %s: %s", name.c_str(), error.c_str());
            handler->SetSentErrorMessage(true);
            return false;
        }

        handler->PSendSysMessage("Custom NPC %s, model variation %u: option index %u copied from your character.", name.c_str(), variation, optionIndex);
        return true;
    }

    static bool HandleCustomNpcSetFaceRawOptionCommand(ChatHandler* handler, std::string const& name, uint32 optionId, uint32 choiceId, Optional<uint8> variationId)
    {
        if (!sRoleplay->CustomNpcNameExists(name)) {
            handler->PSendSysMessage("There is no Custom NPC with the name: %s", name.c_str());
            handler->SetSentErrorMessage(true);
            return false;
        }
        if (!EnsureCanEditCustomNpc(handler, name))
            return false;

        uint8 variation = variationId.value_or(1);
        if (!ValidateCustomNpcVariation(handler, name, variation, false))
            return false;

        std::string error;
        if (!sRoleplay->SetCustomNpcCustomizationOption(name, variation, optionId, choiceId, error))
        {
            handler->PSendSysMessage("Failed to set face for %s: %s", name.c_str(), error.c_str());
            handler->SetSentErrorMessage(true);
            return false;
        }

        handler->PSendSysMessage("Custom NPC %s, model variation %u: raw option %u set to raw choice %u.", name.c_str(), variation, optionId, choiceId);
        return true;
    }

    static bool HandleCustomNpcSetStandStateCommand(ChatHandler* handler, std::string const& name, std::string const& standStateName)
    {
        if (!sRoleplay->CustomNpcNameExists(name)) {
            handler->PSendSysMessage("There is no Custom NPC with the name: %s", name.c_str());
            handler->SetSentErrorMessage(true);
            return false;
        }
        if (!EnsureCanEditCustomNpc(handler, name))
            return false;

        Optional<uint8> standState = ParseStandStateName(standStateName);
        if (!standState)
        {
            handler->PSendSysMessage("Unrecognized stand state '%s'. Use stand|sit|sleep|kneel|dead|submerged or 0..9.", standStateName.c_str());
            handler->SetSentErrorMessage(true);
            return false;
        }

        sRoleplay->SetCustomNpcStandState(name, *standState);
        handler->PSendSysMessage("Custom NPC %s stand state set to %u.", name.c_str(), *standState);
        return true;
    }

    static bool HandleCustomNpcDeleteCommand(ChatHandler* handler, std::string const& name)
    {
        if (!sRoleplay->CustomNpcNameExists(name)) {
            handler->PSendSysMessage("There is no Custom NPC with the name: %s", name.c_str());
            handler->SetSentErrorMessage(true);
            return false;
        }
        if (!EnsureCanEditCustomNpc(handler, name))
            return false;

        sRoleplay->DeleteCustomNpc(name);
        handler->PSendSysMessage("Custom NPC %s has been deleted!", name.c_str());
        return true;
    }

    static bool HandleCustomNpcUnequipArmorCommand(ChatHandler* handler, std::string const& name, std::string const& slotName, Optional<uint8> variationId)
    {
        if (!sRoleplay->CustomNpcNameExists(name)) {
            handler->PSendSysMessage("There is no Custom NPC with the name: %s", name.c_str());
            handler->SetSentErrorMessage(true);
            return false;
        }
        if (!EnsureCanEditCustomNpc(handler, name))
            return false;

        uint8 variation = variationId.value_or(1);
        if (variation < 1) {
            variation = 1;
        }

        uint8 modelCount = sRoleplay->GetModelVariationCountForNpc(name);
        if ((modelCount + 1) < variation) {
            handler->PSendSysMessage("The highest model variation for Custom NPC '%s' is '%u'. The highest variation that can be added at the moment is '%u'.", name.c_str(), modelCount, modelCount + 1);
            handler->SetSentErrorMessage(true);
            return false;
        }

        EquipmentSlots slot = EQUIPMENT_SLOT_END;
        bool allSlots = false;
        if (!ResolveArmorSlotName(slotName, slot, allSlots) || allSlots)
        {
            handler->PSendSysMessage("Unrecognized slot '%s'. Slot must be one of head|shoulders|body|chest|waist|legs|feet|wrists|hands|tabard|back.", slotName.c_str());
            handler->SetSentErrorMessage(true);
            return false;
        }

        if (slot == EQUIPMENT_SLOT_END) {
            handler->PSendSysMessage("Unrecognized slot '%s'. Slot must be one of head|shoulders|body|chest|waist|legs|feet|wrists|hands|tabard|back.", slotName.c_str());
            handler->SetSentErrorMessage(true);
            return false;
        }

        sRoleplay->SetCustomNpcOutfitEquipmentSlot(name, variation, slot, 0);
        handler->PSendSysMessage("Armorslot '%s' unequipped from custom NPC %s, model variation '%u'!", slotName.c_str(), name.c_str(), variation);
        return true;
    }

    static bool HandleCustomNpcUnequipLeftHandCommand(ChatHandler* handler, std::string const& name, Optional<uint8> variationId)
    {
        if (!sRoleplay->CustomNpcNameExists(name)) {
            handler->PSendSysMessage("There is no Custom NPC with the name: %s", name.c_str());
            handler->SetSentErrorMessage(true);
            return false;
        }
        if (!EnsureCanEditCustomNpc(handler, name))
            return false;

        uint8 variation = variationId.value_or(1);
        if (variation < 1) {
            variation = 1;
        }

        uint8 modelCount = sRoleplay->GetEquipmentVariationCountForNpc(name);
        if ((modelCount + 1) < variation) {
            handler->PSendSysMessage("The highest equipment set variation for Custom NPC '%s' is '%u'. The highest variation that can be added at the moment is '%u'.", name.c_str(), modelCount, modelCount + 1);
            handler->SetSentErrorMessage(true);
            return false;
        }

        sRoleplay->SetCustomNpcLeftHand(name, variation, 0, 0);
        handler->PSendSysMessage("Left hand unequipped from custom NPC %s, equipment variation '%u'!", name.c_str(), variation);
        return true;
    }

    static bool HandleCustomNpcUnequipRightHandCommand(ChatHandler* handler, std::string const& name, Optional<uint8> variationId)
    {
        if (!sRoleplay->CustomNpcNameExists(name)) {
            handler->PSendSysMessage("There is no Custom NPC with the name: %s", name.c_str());
            handler->SetSentErrorMessage(true);
            return false;
        }
        if (!EnsureCanEditCustomNpc(handler, name))
            return false;

        uint8 variation = variationId.value_or(1);
        if (variation < 1) {
            variation = 1;
        }

        uint8 modelCount = sRoleplay->GetEquipmentVariationCountForNpc(name);
        if ((modelCount + 1) < variation) {
            handler->PSendSysMessage("The highest equipment set variation for Custom NPC '%s' is '%u'. The highest variation that can be added at the moment is '%u'.", name.c_str(), modelCount, modelCount + 1);
            handler->SetSentErrorMessage(true);
            return false;
        }

        sRoleplay->SetCustomNpcRightHand(name, variation, 0, 0);
        handler->PSendSysMessage("Right hand unequipped from custom NPC %s, equipment variation '%u'!", name.c_str(), variation);
        return true;
    }

    static bool HandleCustomNpcUnequipRangedCommand(ChatHandler* handler, std::string const& name, Optional<uint8> variationId)
    {
        if (!sRoleplay->CustomNpcNameExists(name)) {
            handler->PSendSysMessage("There is no Custom NPC with the name: %s", name.c_str());
            handler->SetSentErrorMessage(true);
            return false;
        }
        if (!EnsureCanEditCustomNpc(handler, name))
            return false;

        uint8 variation = variationId.value_or(1);
        if (variation < 1) {
            variation = 1;
        }

        uint8 modelCount = sRoleplay->GetEquipmentVariationCountForNpc(name);
        if ((modelCount + 1) < variation) {
            handler->PSendSysMessage("The highest equipment set variation for Custom NPC '%s' is '%u'. The highest variation that can be added at the moment is '%u'.", name.c_str(), modelCount, modelCount + 1);
            handler->SetSentErrorMessage(true);
            return false;
        }

        sRoleplay->SetCustomNpcRanged(name, variation, 0, 0);
        handler->PSendSysMessage("Ranged weapon unequipped from custom NPC %s, equipment variation '%u'!", name.c_str(), variation);
        return true;
    }

    static bool HandleCustomNpcSetDisplayIdCommand(ChatHandler* handler, std::string const& name, uint32 displayId, Optional<uint8> variationId)
    {
        if (!sRoleplay->CustomNpcNameExists(name)) {
            handler->PSendSysMessage("There is no Custom NPC with the name: %s", name.c_str());
            handler->SetSentErrorMessage(true);
            return false;
        }
        if (!EnsureCanEditCustomNpc(handler, name))
            return false;

        uint8 variation = variationId.value_or(1);
        if (variation < 1) {
            variation = 1;
        }

        uint8 modelCount = sRoleplay->GetModelVariationCountForNpc(name);
        if ((modelCount + 1) < variation) {
            handler->PSendSysMessage("The highest model variation for Custom NPC '%s' is '%u'. The highest variation that can be added at the moment is '%u'.", name.c_str(), modelCount, modelCount + 1);
            handler->SetSentErrorMessage(true);
            return false;
        }

        sRoleplay->SetCustomNpcDisplayId(name, variation, displayId);
        handler->PSendSysMessage("Custom NPC %s, model variation %u now has displayId %u!", name.c_str(), variation, displayId);
        return true;
    }

    static bool HandleCustomNpcSetGuildCommand(ChatHandler* handler, std::string const& name, Optional<uint8> variationId)
    {
        if (!sRoleplay->CustomNpcNameExists(name)) {
            handler->PSendSysMessage("There is no Custom NPC with the name: %s", name.c_str());
            handler->SetSentErrorMessage(true);
            return false;
        }
        if (!EnsureCanEditCustomNpc(handler, name))
            return false;

        uint8 variation = variationId.value_or(1);
        if (variation < 1) {
            variation = 1;
        }

        uint8 modelCount = sRoleplay->GetModelVariationCountForNpc(name);
        if ((modelCount + 1) < variation) {
            handler->PSendSysMessage("The highest model variation for Custom NPC '%s' is '%u'. The highest variation that can be added at the moment is '%u'.", name.c_str(), modelCount, modelCount + 1);
            handler->SetSentErrorMessage(true);
            return false;
        }

        sRoleplay->SetCustomNpcGuild(name, variation, handler->GetPlayer()->GetGuildId());
        handler->PSendSysMessage("Custom NPC %s, model variation %u has copied your guild!", name.c_str(), variation);
        return true;
    }

    static bool HandleCustomNpcSetScaleCommand(ChatHandler* handler, std::string const& name, float scale, Optional<uint8> variationId)
    {
        if (!sRoleplay->CustomNpcNameExists(name)) {
            handler->PSendSysMessage("There is no Custom NPC with the name: %s", name.c_str());
            handler->SetSentErrorMessage(true);
            return false;
        }
        if (!EnsureCanEditCustomNpc(handler, name))
            return false;

        uint8 variation = variationId.value_or(1);
        if (variation < 1) {
            variation = 1;
        }

        uint8 modelCount = sRoleplay->GetModelVariationCountForNpc(name);
        if ((modelCount + 1) < variation) {
            handler->PSendSysMessage("The highest model variation for Custom NPC '%s' is '%u'. The highest variation that can be added at the moment is '%u'.", name.c_str(), modelCount, modelCount + 1);
            handler->SetSentErrorMessage(true);
            return false;
        }

        sRoleplay->SetCustomNpcModelScale(name, variation, scale);
        handler->PSendSysMessage("Custom NPC %s, model variation %u has been set to scale %f!", name.c_str(), variation, scale);
        return true;
    }

    static bool HandleCustomNpcSetTameableCommand(ChatHandler* handler, std::string const& name, uint8 on)
    {
        if (!sRoleplay->CustomNpcNameExists(name)) {
            handler->PSendSysMessage("There is no Custom NPC with the name: %s", name.c_str());
            handler->SetSentErrorMessage(true);
            return false;
        }
        if (!EnsureCanEditCustomNpc(handler, name))
            return false;

        if (on) {
            sRoleplay->SetCustomNpcTameable(name, true);
            handler->PSendSysMessage("Custom NPC %s has been made tameable.", name.c_str());
        }
        else {
            sRoleplay->SetCustomNpcTameable(name, false);
            handler->PSendSysMessage("Custom NPC %s has been made untameable.", name.c_str());
        }
        return true;
    }

    static bool HandleCustomNpcRemoveVariationCommand(ChatHandler* handler, std::string const& name, uint8 variationId)
    {
        if (!sRoleplay->CustomNpcNameExists(name)) {
            handler->PSendSysMessage("There is no Custom NPC with the name: %s", name.c_str());
            handler->SetSentErrorMessage(true);
            return false;
        }
        if (!EnsureCanEditCustomNpc(handler, name))
            return false;

        if (variationId < 1) {
            variationId = 1;
        }

        if (variationId == 1) {
            handler->PSendSysMessage("Variation 1 for a custom NPC can not be removed. Use .cnpc delete to delete a custom NPC in its entirety.");
            handler->SetSentErrorMessage(true);
            return false;
        }

        uint8 modelCount = sRoleplay->GetModelVariationCountForNpc(name);
        if (modelCount < variationId)
        {
            handler->PSendSysMessage("Custom NPC %s only has %u variations.", name.c_str(), variationId);
            handler->SetSentErrorMessage(true);
            return false;
        }

        sRoleplay->RemoveCustomNpcVariation(name, variationId);
        handler->PSendSysMessage("Custom NPC variation %u for template %s has been removed.", variationId, name.c_str());

        return true;
    }

    static bool HandleCustomNpcRaceListCommand(ChatHandler* handler)
    {
        auto races = sRoleplay->BuildCustomNpcRaceList(false);
        handler->SendSysMessage("Custom NPC races (usable with .cnpc set race when ChrModel exists):");
        for (CustomNpcRaceDescriptor const& race : races)
        {
            if (race.available)
            {
                handler->PSendSysMessage("%u: %s (raceId=%u, male model=%u display=%u, female model=%u display=%u)",
                    race.listIndex, race.name.c_str(), uint32(race.race),
                    race.maleChrModelId, race.maleDisplayId, race.femaleChrModelId, race.femaleDisplayId);
            }
            else
            {
                handler->PSendSysMessage("%u: %s (raceId=%u) [unavailable: missing ChrModel]",
                    race.listIndex, race.name.c_str(), uint32(race.race));
            }
        }
        return true;
    }

    static bool HandleCustomNpcRaceOptionsCommand(ChatHandler* handler, Races race, uint8 gender)
    {
        if (gender > 1)
        {
            handler->SendSysMessage("Gender must be 0 (male) or 1 (female).");
            handler->SetSentErrorMessage(true);
            return false;
        }

        if (!sDB2Manager.GetChrModel(race, gender == 0 ? GENDER_MALE : GENDER_FEMALE))
        {
            handler->PSendSysMessage("Race %u has no ChrModel for gender %u.", uint32(race), uint32(gender));
            handler->SetSentErrorMessage(true);
            return false;
        }

        CreatureOutfit probe(race, gender == 0 ? GENDER_MALE : GENDER_FEMALE);
        probe.Class = CLASS_WARRIOR;

        LocaleConstant locale = handler->GetSessionDbcLocale();
        auto options = sRoleplay->BuildCustomizationOptionList(race, gender == 0 ? GENDER_MALE : GENDER_FEMALE);
        handler->PSendSysMessage("Customization options for race %u gender %u:", uint32(race), uint32(gender));
        for (uint32 i = 0; i < options.size(); ++i)
        {
            uint32 choiceCount = uint32(sRoleplay->BuildCustomizationChoiceListForOutfit(options[i]->ID, probe).size());
            handler->PSendSysMessage("%u: %s (optionId=%u, choices 1..%u)",
                i + 1, sRoleplay->GetCustomizationOptionLabel(options[i], locale).c_str(), options[i]->ID, choiceCount);
        }
        return true;
    }

    static bool HandleCustomNpcFaceListCommand(ChatHandler* handler, std::string const& key, Optional<uint8> variationId)
    {
        if (!sRoleplay->CustomNpcNameExists(key))
        {
            handler->PSendSysMessage("There is no Custom NPC with the name: %s", key.c_str());
            handler->SetSentErrorMessage(true);
            return false;
        }

        uint8 variation = variationId.value_or(1);
        CreatureOutfit const* outfit = sRoleplay->GetCustomNpcOutfit(key, variation);
        if (!outfit)
        {
            handler->PSendSysMessage("Custom NPC %s variation %u has no fake outfit.", key.c_str(), variation);
            handler->SetSentErrorMessage(true);
            return false;
        }

        LocaleConstant locale = handler->GetSessionDbcLocale();
        auto options = sRoleplay->BuildCustomizationOptionList(Races(outfit->GetRace()), Gender(outfit->GetGender()));
        handler->PSendSysMessage("Customization options for %s variation %u:", key.c_str(), variation);
        for (uint32 i = 0; i < options.size(); ++i)
        {
            uint32 currentChoice = sRoleplay->FindCustomizationChoiceIndex(*outfit, options[i]);
            uint32 choiceCount = uint32(sRoleplay->BuildCustomizationChoiceListForOutfit(options[i]->ID, *outfit).size());
            handler->PSendSysMessage("%u: %s (optionId=%u, current choice %u/%u)",
                i + 1, sRoleplay->GetCustomizationOptionLabel(options[i], locale).c_str(), options[i]->ID,
                currentChoice, choiceCount);
        }
        return true;
    }

    static bool HandleCustomNpcFaceChoicesCommand(ChatHandler* handler, std::string const& key, uint32 optionIndex, Optional<uint8> variationId)
    {
        if (!sRoleplay->CustomNpcNameExists(key))
        {
            handler->PSendSysMessage("There is no Custom NPC with the name: %s", key.c_str());
            handler->SetSentErrorMessage(true);
            return false;
        }

        uint8 variation = variationId.value_or(1);
        CreatureOutfit const* outfit = sRoleplay->GetCustomNpcOutfit(key, variation);
        if (!outfit)
        {
            handler->PSendSysMessage("Custom NPC %s variation %u has no fake outfit.", key.c_str(), variation);
            handler->SetSentErrorMessage(true);
            return false;
        }

        auto options = sRoleplay->BuildCustomizationOptionList(Races(outfit->GetRace()), Gender(outfit->GetGender()));
        if (optionIndex < 1 || optionIndex > options.size())
        {
            handler->SendSysMessage("Option index is out of range. Use .cnpc face list first.");
            handler->SetSentErrorMessage(true);
            return false;
        }

        ChrCustomizationOptionEntry const* option = options[optionIndex - 1];
        auto choices = sRoleplay->BuildCustomizationChoiceListForOutfit(option->ID, *outfit);
        LocaleConstant locale = handler->GetSessionDbcLocale();
        handler->PSendSysMessage("Choices for option %u (%s):", optionIndex, sRoleplay->GetCustomizationOptionLabel(option, locale).c_str());
        for (uint32 i = 0; i < choices.size(); ++i)
        {
            handler->PSendSysMessage("%u: %s (choiceId=%u)", i + 1,
                sRoleplay->GetCustomizationChoiceLabel(choices[i], locale).c_str(), choices[i]->ID);
        }
        return true;
    }

    static bool HandleCustomNpcSetFaceIndexedCommand(ChatHandler* handler, std::string const& key, uint32 optionIndex, uint32 choiceIndex, Optional<uint8> variationId)
    {
        if (!sRoleplay->CustomNpcNameExists(key))
        {
            handler->PSendSysMessage("There is no Custom NPC with the name: %s", key.c_str());
            handler->SetSentErrorMessage(true);
            return false;
        }
        if (!EnsureCanEditCustomNpc(handler, key))
            return false;

        uint8 variation = variationId.value_or(1);
        std::string error;
        if (!sRoleplay->SetCustomNpcCustomizationByIndex(key, variation, optionIndex, choiceIndex, error))
        {
            handler->PSendSysMessage("Failed to set face for %s: %s", key.c_str(), error.c_str());
            handler->SetSentErrorMessage(true);
            return false;
        }

        handler->PSendSysMessage("Custom NPC %s variation %u: option index %u set to choice index %u.", key.c_str(), variation, optionIndex, choiceIndex);
        return true;
    }

    static bool HandleCustomNpcRefreshCommand(ChatHandler* handler, Tail keyOrAll)
    {
        std::string arg = keyOrAll.data();
        boost::trim(arg);
        if (arg.empty())
        {
            handler->SendSysMessage("Usage: .cnpc refresh <key|all> [variation]");
            handler->SetSentErrorMessage(true);
            return false;
        }

        if (StringEqualI(arg, "all"))
        {
            CustomNpcDataContainer const customNpcs = sRoleplay->GetCustomNpcContainer();
            uint32 count = 0;
            for (auto const& pair : customNpcs)
            {
                if (sRoleplay->RefreshCustomNpcCreatures(pair.first))
                    ++count;
            }
            handler->PSendSysMessage("Refreshed %u custom NPC(s) in world.", count);
            return true;
        }

        if (!sRoleplay->RefreshCustomNpcCreatures(arg))
        {
            handler->PSendSysMessage("Custom NPC %s was not refreshed.", arg.c_str());
            handler->SetSentErrorMessage(true);
            return false;
        }

        handler->PSendSysMessage("Custom NPC %s refreshed in world.", arg.c_str());
        return true;
    }

    static bool HandleCustomNpcDiagnoseCommand(ChatHandler* handler, std::string const& key)
    {
        std::ostringstream report;
        if (!sRoleplay->DiagnoseCustomNpc(key, report))
        {
            handler->SendSysMessage(report.str().c_str());
            handler->SetSentErrorMessage(true);
            return false;
        }

        handler->SendSysMessage(report.str().c_str());
        return true;
    }

    static bool HandleCustomNpcModelListCommand(ChatHandler* handler, std::string const& key)
    {
        if (!sRoleplay->CustomNpcNameExists(key))
        {
            handler->PSendSysMessage("There is no Custom NPC with the name: %s", key.c_str());
            handler->SetSentErrorMessage(true);
            return false;
        }

        uint32 entry = sRoleplay->GetEntryIdForNpc(key);
        CreatureTemplate const* cTemplate = sObjectMgr->GetCreatureTemplate(entry);
        if (!cTemplate)
        {
            handler->SendSysMessage("Template not found.");
            handler->SetSentErrorMessage(true);
            return false;
        }

        handler->PSendSysMessage("Model variations for %s (entry %u):", key.c_str(), entry);
        for (uint8 i = 0; i < cTemplate->Models.size(); ++i)
        {
            CreatureModel const& model = cTemplate->Models[i];
            handler->PSendSysMessage("%u: displayId=%u scale=%.2f%s", uint32(i + 1), model.CreatureDisplayID, model.DisplayScale,
                CreatureOutfit::IsFake(model.CreatureDisplayID) ? " (outfit)" : "");
        }
        return true;
    }

    static bool HandleCustomNpcModelAddBlankCommand(ChatHandler* handler, std::string const& key, Optional<uint8> sourceVariation)
    {
        if (!EnsureCanEditCustomNpc(handler, key))
            return false;

        uint8 newVariation = 0;
        std::string error;
        if (!sRoleplay->AddCustomNpcModelVariationBlank(key, sourceVariation, newVariation, error))
        {
            handler->PSendSysMessage("Failed: %s", error.c_str());
            handler->SetSentErrorMessage(true);
            return false;
        }

        handler->PSendSysMessage("Added blank model variation %u to %s.", newVariation, key.c_str());
        return true;
    }

    static bool HandleCustomNpcModelAddSelfCommand(ChatHandler* handler, std::string const& key)
    {
        if (!EnsureCanEditCustomNpc(handler, key))
            return false;

        uint8 newVariation = 0;
        std::string error;
        if (!sRoleplay->AddCustomNpcModelVariationFromPlayer(key, handler->GetPlayer(), newVariation, error))
        {
            handler->PSendSysMessage("Failed: %s", error.c_str());
            handler->SetSentErrorMessage(true);
            return false;
        }

        handler->PSendSysMessage("Added model variation %u from yourself to %s.", newVariation, key.c_str());
        return true;
    }

    static bool HandleCustomNpcModelAddPlayerCommand(ChatHandler* handler, std::string const& key, PlayerIdentifier player)
    {
        if (!EnsureCanEditCustomNpc(handler, key))
            return false;

        Player* source = player.GetConnectedPlayer();
        if (!source)
        {
            handler->SendSysMessage("Player not found.");
            handler->SetSentErrorMessage(true);
            return false;
        }

        uint8 newVariation = 0;
        std::string error;
        if (!sRoleplay->AddCustomNpcModelVariationFromPlayer(key, source, newVariation, error))
        {
            handler->PSendSysMessage("Failed: %s", error.c_str());
            handler->SetSentErrorMessage(true);
            return false;
        }

        handler->PSendSysMessage("Added model variation %u from %s to %s.", newVariation, source->GetName().c_str(), key.c_str());
        return true;
    }

    static bool HandleCustomNpcModelAddTargetCommand(ChatHandler* handler, std::string const& key)
    {
        if (!EnsureCanEditCustomNpc(handler, key))
            return false;

        Creature* target = handler->getSelectedCreature();
        if (!target)
        {
            handler->SendSysMessage("No creature selected.");
            handler->SetSentErrorMessage(true);
            return false;
        }

        uint8 newVariation = 0;
        std::string error;
        if (!sRoleplay->AddCustomNpcModelVariationFromCreature(key, target, newVariation, error))
        {
            handler->PSendSysMessage("Failed: %s", error.c_str());
            handler->SetSentErrorMessage(true);
            return false;
        }

        handler->PSendSysMessage("Added model variation %u from target to %s.", newVariation, key.c_str());
        return true;
    }

    static bool HandleCustomNpcModelAddCustomCommand(ChatHandler* handler, std::string const& key, std::string const& sourceKey, Optional<uint8> sourceVariation)
    {
        if (!EnsureCanEditCustomNpc(handler, key))
            return false;

        uint8 newVariation = 0;
        std::string error;
        if (!sRoleplay->AddCustomNpcModelVariationFromCustomNpc(key, sourceKey, sourceVariation, newVariation, error))
        {
            handler->PSendSysMessage("Failed: %s", error.c_str());
            handler->SetSentErrorMessage(true);
            return false;
        }

        handler->PSendSysMessage("Added model variation %u from custom NPC %s to %s.", newVariation, sourceKey.c_str(), key.c_str());
        return true;
    }

    static bool HandleCustomNpcModelCopyCommand(ChatHandler* handler, std::string const& key, uint8 fromVariation, Optional<uint8> toVariation)
    {
        if (!EnsureCanEditCustomNpc(handler, key))
            return false;

        uint8 actualVariation = 0;
        std::string error;
        if (!sRoleplay->CopyCustomNpcModelVariation(key, fromVariation, toVariation, actualVariation, error))
        {
            handler->PSendSysMessage("Failed: %s", error.c_str());
            handler->SetSentErrorMessage(true);
            return false;
        }

        handler->PSendSysMessage("Copied model variation %u to variation %u on %s.", fromVariation, actualVariation, key.c_str());
        return true;
    }

    static bool HandleCustomNpcModelRemoveCommand(ChatHandler* handler, std::string const& key, uint8 variationId)
    {
        return HandleCustomNpcRemoveVariationCommand(handler, key, variationId);
    }

    static bool HandleCustomNpcModelApplyCommand(ChatHandler* handler, uint8 variation)
    {
        return HandleCustomNpcApplyCommand(handler, variation);
    }

    static bool HandleCustomNpcCloneCustomCommand(ChatHandler* handler, std::string const& sourceKey, Tail displayName, Optional<uint8> sourceVariation)
    {
        std::string name = displayName.data();
        boost::trim(name);
        if (name.empty())
        {
            handler->SendSysMessage("Display name is required.");
            handler->SetSentErrorMessage(true);
            return false;
        }

        std::string newKey = sRoleplay->GenerateCustomNpcKey(handler->GetPlayer());
        std::string error;
        if (!sRoleplay->CloneCustomNpcFromKey(sourceKey, newKey, name, handler->GetPlayer(), sourceVariation, error))
        {
            handler->PSendSysMessage("Clone failed: %s", error.c_str());
            handler->SetSentErrorMessage(true);
            return false;
        }

        handler->PSendSysMessage("Cloned custom NPC %s to new key %s (%s).", sourceKey.c_str(), newKey.c_str(), name.c_str());
        return true;
    }

    static bool HandleCustomNpcCloneEntryCommand(ChatHandler* handler, uint32 entry, Tail displayName, Optional<uint8> variation)
    {
        std::string name = displayName.data();
        boost::trim(name);
        if (name.empty())
        {
            handler->SendSysMessage("Display name is required.");
            handler->SetSentErrorMessage(true);
            return false;
        }

        std::string newKey = sRoleplay->GenerateCustomNpcKey(handler->GetPlayer());
        std::string error;
        if (!sRoleplay->CloneCustomNpcFromEntry(entry, newKey, name, handler->GetPlayer(), variation, error))
        {
            handler->PSendSysMessage("Clone failed: %s", error.c_str());
            handler->SetSentErrorMessage(true);
            return false;
        }

        handler->PSendSysMessage("Cloned entry %u to custom NPC %s (%s).", entry, newKey.c_str(), name.c_str());
        return true;
    }

    static bool HandleCustomNpcEquipCopyCustomCommand(ChatHandler* handler, std::string const& key, std::string const& sourceKey, uint8 variation, Optional<uint8> sourceEquipmentVariation)
    {
        if (!EnsureCanEditCustomNpc(handler, key))
            return false;

        std::string error;
        if (!sRoleplay->CopyCustomNpcEquipmentFromCustomNpc(key, sourceKey, variation, sourceEquipmentVariation, error))
        {
            handler->PSendSysMessage("Failed: %s", error.c_str());
            handler->SetSentErrorMessage(true);
            return false;
        }

        handler->PSendSysMessage("Copied equipment variation to %s variation %u.", key.c_str(), variation);
        return true;
    }
};

void AddSC_customnpc_commandscript()
{
    new customnpc_commandscript();
}
