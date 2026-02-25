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

#include "ScriptMgr.h"
#include "GameObjectAI.h"
#include "Group.h"
#include "Guild.h"
#include "Housing.h"
#include "HousingMap.h"
#include "HousingMgr.h"
#include "HousingPackets.h"
#include "Neighborhood.h"
#include "ObjectAccessor.h"
#include "Player.h"
#include "SocialMgr.h"

enum
{
    HOUSING_DOOR_ENTRY = 602702
};

// Script for the housing front door GO (entry 602702).
// When a player clicks the door, teleport them to the house interior
// using the TeleportPosition from the NeighborhoodPlot DB2 data.
// The interior is a WMO interior within the same neighborhood map.
class go_housing_door : public GameObjectScript
{
public:
    go_housing_door() : GameObjectScript("go_housing_door") { }

    struct go_housing_doorAI : public GameObjectAI
    {
        go_housing_doorAI(GameObject* go) : GameObjectAI(go) { }

        bool OnGossipHello(Player* player) override
        {
            if (!player || !player->IsInWorld())
                return true;

            HousingMap* housingMap = dynamic_cast<HousingMap*>(me->GetMap());
            if (!housingMap)
                return true;

            // Find which plot this door belongs to
            int8 plotIndex = housingMap->GetPlotIndexForHouseGO(me->GetGUID());
            if (plotIndex < 0)
            {
                TC_LOG_ERROR("housing", "go_housing_door: Door GO {} not found in any plot",
                    me->GetGUID().ToString());
                return true;
            }

            // Get the plot's teleport position from DB2 data
            Neighborhood* neighborhood = housingMap->GetNeighborhood();
            if (!neighborhood)
                return true;

            uint32 neighborhoodMapId = neighborhood->GetNeighborhoodMapID();
            std::vector<NeighborhoodPlotData const*> plots = sHousingMgr.GetPlotsForMap(neighborhoodMapId);

            NeighborhoodPlotData const* plotData = nullptr;
            for (NeighborhoodPlotData const* plot : plots)
            {
                if (static_cast<int8>(plot->PlotIndex) == plotIndex)
                {
                    plotData = plot;
                    break;
                }
            }

            if (!plotData)
            {
                TC_LOG_ERROR("housing", "go_housing_door: No NeighborhoodPlotData for plotIndex {} in map {}",
                    plotIndex, neighborhoodMapId);
                return true;
            }

            // Validate teleport position is non-zero
            if (plotData->TeleportPosition[0] == 0.0f &&
                plotData->TeleportPosition[1] == 0.0f &&
                plotData->TeleportPosition[2] == 0.0f)
            {
                TC_LOG_ERROR("housing", "go_housing_door: TeleportPosition is (0,0,0) for plot {} - "
                    "DB2 data may not be populated", plotIndex);
                return true;
            }

            // Check visitor access permissions if this isn't the player's own plot
            Neighborhood::PlotInfo const* plotInfo = neighborhood->GetPlotInfo(static_cast<uint8>(plotIndex));
            if (plotInfo && plotInfo->OwnerGuid != player->GetGUID())
            {
                Player* owner = ObjectAccessor::FindPlayer(plotInfo->OwnerGuid);
                if (owner)
                {
                    Housing const* ownerHousing = owner->GetHousing();
                    if (ownerHousing && !sHousingMgr.CanVisitorAccess(player, owner, ownerHousing->GetSettingsFlags(), true))
                    {
                        TC_LOG_DEBUG("housing", "go_housing_door: Player {} denied interior access to plot {} (owner {} flags 0x{:X})",
                            player->GetGUID().ToString(), plotIndex, plotInfo->OwnerGuid.ToString(),
                            ownerHousing->GetSettingsFlags());
                        return true;
                    }
                }
            }

            // Animate the door
            me->UseDoorOrButton();

            Housing* housing = player->GetHousing();

            // Send SMSG_HOUSE_INTERIOR_ENTER_HOUSE to trigger client interior transition
            if (housing)
            {
                housing->SetInInterior(true);

                WorldPackets::Housing::HouseInteriorEnterHouse enterHouse;
                enterHouse.HouseGuid = housing->GetHouseGuid();
                player->SendDirectMessage(enterHouse.Write());
            }

            // Teleport player to the interior entry point (same map, different position)
            player->NearTeleportTo(
                plotData->TeleportPosition[0],
                plotData->TeleportPosition[1],
                plotData->TeleportPosition[2],
                plotData->TeleportFacing);

            // Send house status response to update client context
            if (housing)
            {
                WorldPackets::Housing::HousingHouseStatusResponse statusResponse;
                statusResponse.HouseGuid = housing->GetHouseGuid();
                statusResponse.PlotGuid = housing->GetPlotGuid();
                statusResponse.NeighborhoodGuid = housing->GetNeighborhoodGuid();
                statusResponse.Status = 1; // Interior
                player->SendDirectMessage(statusResponse.Write());
            }

            TC_LOG_INFO("housing", "go_housing_door: Player {} entered house interior at plot {} "
                "teleport=({:.1f}, {:.1f}, {:.1f}, {:.2f})",
                player->GetGUID().ToString(), plotIndex,
                plotData->TeleportPosition[0], plotData->TeleportPosition[1],
                plotData->TeleportPosition[2], plotData->TeleportFacing);

            return true;
        }
    };

    GameObjectAI* GetAI(GameObject* go) const override
    {
        return new go_housing_doorAI(go);
    }
};

void AddSC_go_housing_door()
{
    new go_housing_door();
}
