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

namespace
{
    constexpr uint32 HOUSING_DOOR_ENTRY    = 586576;  // retail "Founder's Point Front Door"
    constexpr uint32 HOUSE_INTERIOR_MAP_ID = 2783;     // "Home Interior" — InstanceType 7 (MAP_HOUSE_INTERIOR)

    // Interior spawn position from NeighborhoodMap ID=7 (sniff-confirmed)
    constexpr float INTERIOR_SPAWN_X = -1000.0f;
    constexpr float INTERIOR_SPAWN_Y = -1000.0f;
    constexpr float INTERIOR_SPAWN_Z = 0.1f;
    constexpr float INTERIOR_SPAWN_O = 0.0f;
}

// Script for the housing front door GO (entry 602702).
// When a player clicks the door, teleport them to the house interior map (MapID 2783).
// The interior is a separate instanced map per player (MAP_HOUSE_INTERIOR = 7),
// NOT a position within the neighborhood map.
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
            {
                TC_LOG_ERROR("housing", "go_housing_door: Map {} is NOT a HousingMap", me->GetMapId());
                return true;
            }

            // Find which plot this door belongs to.
            // Try dynamic door tracking first, then fall back to the player's current
            // plot (set by the at_housing_plot AreaTrigger script). This handles both
            // dynamically-spawned doors and static DB-spawned doors (from gameobject table).
            int8 plotIndex = housingMap->GetPlotIndexForHouseGO(me->GetGUID());
            if (plotIndex < 0)
            {
                // Fallback: use the plot the player is currently standing on
                plotIndex = housingMap->GetPlayerCurrentPlot(player->GetGUID());
                if (plotIndex < 0)
                {
                    // Last resort: find the nearest plot by proximity to the door GO
                    Neighborhood* nbh = housingMap->GetNeighborhood();
                    if (nbh)
                    {
                        uint32 nbhMapId = nbh->GetNeighborhoodMapID();
                        std::vector<NeighborhoodPlotData const*> plots = sHousingMgr.GetPlotsForMap(nbhMapId);
                        float bestDist = std::numeric_limits<float>::max();
                        for (NeighborhoodPlotData const* plot : plots)
                        {
                            float dx = me->GetPositionX() - plot->HousePosition[0];
                            float dy = me->GetPositionY() - plot->HousePosition[1];
                            float dist = dx * dx + dy * dy;
                            if (dist < bestDist)
                            {
                                bestDist = dist;
                                plotIndex = static_cast<int8>(plot->PlotIndex);
                            }
                        }
                    }
                }

                if (plotIndex < 0)
                {
                    TC_LOG_ERROR("housing", "go_housing_door: Could not determine plot for door GO {} "
                        "(player {} at {:.1f},{:.1f},{:.1f})",
                        me->GetGUID().ToString(), player->GetGUID().ToString(),
                        me->GetPositionX(), me->GetPositionY(), me->GetPositionZ());
                    return true;
                }

                TC_LOG_DEBUG("housing", "go_housing_door: Door GO {} not in _houseGameObjects, "
                    "resolved plotIndex={} via fallback",
                    me->GetGUID().ToString(), plotIndex);
            }

            Neighborhood* neighborhood = housingMap->GetNeighborhood();
            if (!neighborhood)
            {
                TC_LOG_ERROR("housing", "go_housing_door: Neighborhood is NULL on mapId={}", housingMap->GetId());
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
                        TC_LOG_DEBUG("housing", "go_housing_door: Player {} denied interior access to plot {} "
                            "(owner {} flags 0x{:X})",
                            player->GetGUID().ToString(), plotIndex, plotInfo->OwnerGuid.ToString(),
                            ownerHousing->GetSettingsFlags());
                        return true;
                    }
                }
            }

            // Animate the door
            me->UseDoorOrButton();

            // Teleport player to the house interior map (Map 2783).
            bool ok = player->TeleportTo(HOUSE_INTERIOR_MAP_ID,
                INTERIOR_SPAWN_X, INTERIOR_SPAWN_Y, INTERIOR_SPAWN_Z, INTERIOR_SPAWN_O);

            if (!ok)
            {
                TC_LOG_ERROR("housing", "go_housing_door: TeleportTo FAILED — player {} → map {} "
                    "from plot {}",
                    player->GetGUID().ToString(), HOUSE_INTERIOR_MAP_ID, plotIndex);
            }
            else
            {
                TC_LOG_INFO("housing", "go_housing_door: Player {} entering interior map {} from plot {}",
                    player->GetGUID().ToString(), HOUSE_INTERIOR_MAP_ID, plotIndex);
            }

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
