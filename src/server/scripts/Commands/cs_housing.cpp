/*
 * This file is part of the TrinityCore Project. See AUTHORS file for Copyright information.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include "ScriptMgr.h"
#include "Chat.h"
#include "ChatCommand.h"
#include "CharacterDatabase.h"
#include "Housing.h"
#include "HousingDefines.h"
#include "HousingMgr.h"
#include "Log.h"
#include "Neighborhood.h"
#include "NeighborhoodMgr.h"
#include "ObjectAccessor.h"
#include "Player.h"
#include "RBAC.h"
#include "StringConvert.h"
#include "WorldSession.h"

using namespace Trinity::ChatCommands;

class housing_commandscript : public CommandScript
{
public:
    housing_commandscript() : CommandScript("housing_commandscript") { }

    std::span<ChatCommandBuilder const> GetCommands() const override
    {
        static ChatCommandTable charterSetCommandTable =
        {
            { "type",  HandleCharterSetTypeCommand,  rbac::RBAC_PERM_COMMAND_HOUSING_CHARTER_TYPE,   Console::No },
            { "owner", HandleCharterSetOwnerCommand, rbac::RBAC_PERM_COMMAND_HOUSING_CHARTER_TYPE,   Console::No },
        };

        static ChatCommandTable charterCommandTable =
        {
            { "create", HandleCharterCreateCommand, rbac::RBAC_PERM_COMMAND_HOUSING_CHARTER_CREATE, Console::No },
            { "set",    charterSetCommandTable },
            { "delete", HandleCharterDeleteCommand, rbac::RBAC_PERM_COMMAND_HOUSING_CHARTER_DELETE, Console::No },
        };

        static ChatCommandTable housingCommandTable =
        {
            { "set level", HandleSetCommand,     rbac::RBAC_PERM_COMMAND_HOUSING,        Console::No },
            { "rewards",   HandleRewardsCommand, rbac::RBAC_PERM_COMMAND_HOUSING,        Console::No },
            { "delete",    HandleDeleteCommand,  rbac::RBAC_PERM_COMMAND_HOUSING_DELETE, Console::No },
            { "charter",   charterCommandTable },
        };

        static ChatCommandTable commandTable =
        {
            { "housing", housingCommandTable },
        };

        return commandTable;
    }

private:
    static bool HandleSetCommand(ChatHandler* handler, uint32 level)
    {
        if (level < 1 || level > MAX_HOUSE_LEVEL)
        {
            handler->PSendSysMessage("Level must be between 1 and %u.", MAX_HOUSE_LEVEL);
            handler->SetSentErrorMessage(true);
            return false;
        }

        Player* target = handler->getSelectedPlayer();
        Player* owner = target ? target : handler->GetSession()->GetPlayer();
        Housing* housing = owner->GetHousing();
        if (!housing || housing->GetHouseGuid().IsEmpty())
        {
            handler->PSendSysMessage("%s has no house.", target ? target->GetName() : "You");
            handler->SetSentErrorMessage(true);
            return false;
        }

        housing->SetLevel(level);
        // AddLevel grants these on the favor grind; do the same here.
        housing->GrantLevelAwards(2, level);
        handler->PSendSysMessage("House level of %s set to %u.", owner->GetName(), level);
        return true;
    }

    static bool HandleRewardsCommand(ChatHandler* handler)
    {
        Player* target = handler->getSelectedPlayer();
        Player* owner = target ? target : handler->GetSession()->GetPlayer();
        Housing* housing = owner->GetHousing();
        if (!housing || housing->GetHouseGuid().IsEmpty())
        {
            handler->PSendSysMessage("%s has no house.", target ? target->GetName() : "You");
            handler->SetSentErrorMessage(true);
            return false;
        }

        // Re-grant past stale rewarded markers left by wiped house tables.
        housing->GrantLevelAwards(2, housing->GetLevel(), true);
        handler->PSendSysMessage("Level rewards of %s's house (level %u) re-granted.", owner->GetName(), housing->GetLevel());
        return true;
    }

    static bool HandleDeleteCommand(ChatHandler* handler)
    {
        Player* target = handler->getSelectedPlayer();
        Player* owner = target ? target : handler->GetSession()->GetPlayer();
        Housing* housing = owner->GetHousing();
        if (!housing || housing->GetHouseGuid().IsEmpty())
        {
            handler->PSendSysMessage("%s has no house.", target ? target->GetName() : "You");
            handler->SetSentErrorMessage(true);
            return false;
        }

        housing->Delete();
        handler->PSendSysMessage("House of %s deleted.", owner->GetName());
        return true;
    }

    // The faction's drawn neighborhood map: without a texture kit the client has no UiMap for
    // it and the house finder's map fails to open.
    static uint32 PickNeighborhoodMapId(Player* player)
    {
        uint32 wantBit = player->GetTeamId() == TEAM_ALLIANCE
            ? NEIGHBORHOOD_MAP_FLAG_ALLIANCE_PURCHASABLE : NEIGHBORHOOD_MAP_FLAG_HORDE_PURCHASABLE;
        for (auto const& [id, data] : sHousingMgr.GetAllNeighborhoodMapData())
        {
            if (!(data.Flags & wantBit))
                continue;
            if (!data.UiTextureKitID)
                continue;
            return id;
        }

        return 0;
    }

    static bool HandleCharterCreateCommand(ChatHandler* handler, Tail tailName)
    {
        std::string name(tailName);
        if (name.empty() || name.size() > HOUSING_MAX_NAME_LENGTH)
        {
            handler->PSendSysMessage("Name must be 1-%u characters.", HOUSING_MAX_NAME_LENGTH);
            handler->SetSentErrorMessage(true);
            return false;
        }

        Player* player = handler->GetSession()->GetPlayer();
        uint32 mapId = PickNeighborhoodMapId(player);
        if (!mapId)
        {
            handler->SendSysMessage("No neighborhood map available for your faction.");
            handler->SetSentErrorMessage(true);
            return false;
        }

        int32 faction = player->GetTeamId() == TEAM_ALLIANCE
            ? NEIGHBORHOOD_FACTION_ALLIANCE : NEIGHBORHOOD_FACTION_HORDE;

        // Private by default: charter-owned, not public, no guild.
        Neighborhood* neighborhood = sNeighborhoodMgr.CreateNeighborhood(player->GetGUID(), name, mapId, faction);
        if (!neighborhood)
        {
            handler->PSendSysMessage("Failed to create neighborhood (you may already own one).");
            handler->SetSentErrorMessage(true);
            return false;
        }

        handler->PSendSysMessage("Neighborhood \"%s\" created (private).", neighborhood->GetName());
        return true;
    }

    static bool HandleCharterSetTypeCommand(ChatHandler* handler, Tail typeAndName)
    {
        // Syntax: .housing charter set type public|guild|private $name - the tail starts with the type keyword.
        std::string_view tail(typeAndName);
        std::string_view type;
        std::string_view name;
        if (auto space = tail.find(' '); space != std::string_view::npos)
        {
            type = tail.substr(0, space);
            name = tail.substr(space + 1);
        }

        if (name.empty())
        {
            handler->SendSysMessage("Syntax: .housing charter set type public|guild|private $name");
            handler->SetSentErrorMessage(true);
            return false;
        }

        Neighborhood* neighborhood = sNeighborhoodMgr.FindNeighborhoodByName(name);
        if (!neighborhood)
        {
            handler->PSendSysMessage("Neighborhood \"%s\" not found.", name);
            handler->SetSentErrorMessage(true);
            return false;
        }

        // Guild conversion needs a guild association; only public <-> private is supported here.
        if (StringEqualI(type, "public"))
        {
            neighborhood->SetPublic(true);
            handler->PSendSysMessage("Neighborhood \"%s\" is now public.", neighborhood->GetName());
            return true;
        }

        if (StringEqualI(type, "private"))
        {
            if (neighborhood->GetGuildId())
            {
                handler->SendSysMessage("Guild neighborhoods cannot become private.");
                handler->SetSentErrorMessage(true);
                return false;
            }

            neighborhood->SetPublic(false);
            handler->PSendSysMessage("Neighborhood \"%s\" is now private.", neighborhood->GetName());
            return true;
        }

        if (StringEqualI(type, "guild"))
        {
            handler->SendSysMessage("Guild neighborhoods can only be created through the guild charter flow.");
            handler->SetSentErrorMessage(true);
            return false;
        }

        handler->SendSysMessage("Unknown type. Use public or private.");
        handler->SetSentErrorMessage(true);
        return false;
    }

    static bool HandleCharterSetOwnerCommand(ChatHandler* handler, Tail playerName)
    {
        std::string name(playerName);
        if (name.empty())
        {
            handler->SendSysMessage("Syntax: .housing charter set owner $playerName");
            handler->SetSentErrorMessage(true);
            return false;
        }

        Player* player = handler->GetSession()->GetPlayer();
        Neighborhood* neighborhood = sNeighborhoodMgr.GetNeighborhoodByOwner(player->GetGUID());
        if (!neighborhood)
        {
            handler->SendSysMessage("You do not own a neighborhood.");
            handler->SetSentErrorMessage(true);
            return false;
        }

        if (neighborhood->GetGuildId())
        {
            handler->SendSysMessage("Guild neighborhoods cannot be transferred with this command.");
            handler->SetSentErrorMessage(true);
            return false;
        }

        // Only same-faction characters may own the neighborhood: resolve the target's race
        // (online players from the object, offline ones from the characters table).
        ObjectGuid newOwnerGuid;
        uint8 race = 0;
        if (Player* target = ObjectAccessor::FindConnectedPlayerByName(name))
        {
            newOwnerGuid = target->GetGUID();
            race = target->GetRace();
        }
        else
        {
            CharacterDatabasePreparedStatement* lookupStmt = CharacterDatabase.GetPreparedStatement(CHAR_SEL_CHARACTER_GUID_RACE_BY_NAME);
            lookupStmt->setString(0, name);
            PreparedQueryResult result = CharacterDatabase.Query(lookupStmt);
            if (!result)
            {
                handler->PSendSysMessage("Character \"%s\" not found.", name);
                handler->SetSentErrorMessage(true);
                return false;
            }

            newOwnerGuid = ObjectGuid::Create<HighGuid::Player>(result->Fetch()[0].GetUInt32());
            race = result->Fetch()[1].GetUInt8();
        }

        if (newOwnerGuid == player->GetGUID())
        {
            handler->SendSysMessage("You already own this neighborhood.");
            handler->SetSentErrorMessage(true);
            return false;
        }

        // The faction gate: the neighborhood's restriction must accept the new owner's team.
        TeamId newOwnerTeam = Player::TeamIdForRace(race);
        switch (neighborhood->GetFactionRestriction())
        {
            case NEIGHBORHOOD_FACTION_ALLIANCE:
                if (newOwnerTeam != TEAM_ALLIANCE)
                {
                    handler->PSendSysMessage("%s cannot own an Alliance neighborhood.", name);
                    handler->SetSentErrorMessage(true);
                    return false;
                }
                break;
            case NEIGHBORHOOD_FACTION_HORDE:
                if (newOwnerTeam != TEAM_HORDE)
                {
                    handler->PSendSysMessage("%s cannot own a Horde neighborhood.", name);
                    handler->SetSentErrorMessage(true);
                    return false;
                }
                break;
            default:
                break;
        }

        if (!sNeighborhoodMgr.SetNeighborhoodOwner(*neighborhood, newOwnerGuid))
        {
            handler->PSendSysMessage("Failed to transfer \"%s\" (the character may already own a neighborhood).", neighborhood->GetName());
            handler->SetSentErrorMessage(true);
            return false;
        }

        handler->PSendSysMessage("Neighborhood \"%s\" transferred to %s.", neighborhood->GetName(), name);
        return true;
    }

    static bool HandleCharterDeleteCommand(ChatHandler* handler, Tail tailName)
    {
        std::string name(tailName);
        if (name.empty())
        {
            handler->SendSysMessage("Syntax: .housing charter delete $name");
            handler->SetSentErrorMessage(true);
            return false;
        }

        Neighborhood* neighborhood = sNeighborhoodMgr.FindNeighborhoodByName(name);
        if (!neighborhood)
        {
            handler->PSendSysMessage("Neighborhood \"%s\" not found.", name);
            handler->SetSentErrorMessage(true);
            return false;
        }

        // Every occupied plot's house goes away with the neighborhood - online owners lose the
        // live object, offline ones lose their rows, and every plot is released.
        uint32 deleted = 0;
        std::vector<ObjectGuid> offlineOwners;
        for (Neighborhood::PlotInfo const& plot : neighborhood->GetPlots())
        {
            if (!plot.IsOccupied())
                continue;

            if (Player* onlineOwner = ObjectAccessor::FindConnectedPlayer(plot.OwnerGuid))
                if (Housing* housing = onlineOwner->GetHousingByOwner(plot.OwnerGuid))
                {
                    housing->Delete();
                    neighborhood->ReleasePlot(plot.OwnerGuid);
                    ++deleted;
                    continue;
                }

            offlineOwners.push_back(plot.OwnerGuid);
        }

        if (!offlineOwners.empty())
        {
            CharacterDatabaseTransaction trans = CharacterDatabase.BeginTransaction();
            for (ObjectGuid ownerGuid : offlineOwners)
            {
                Housing::DeleteFromDB(ownerGuid.GetCounter(), trans);
                neighborhood->ReleasePlot(ownerGuid);
                ++deleted;
            }
            CharacterDatabase.CommitTransaction(trans);
        }

        ObjectGuid neighborhoodGuid = neighborhood->GetGuid();
        sNeighborhoodMgr.DeleteNeighborhood(neighborhoodGuid);

        handler->PSendSysMessage("Neighborhood \"%s\" deleted (%u house(s) removed).", name, deleted);
        return true;
    }
};

void AddSC_housing_commandscript()
{
    new housing_commandscript();
}
