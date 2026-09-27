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
#include "GameObject.h"
#include "GameObjectAI.h"
#include "Group.h"
#include "Guild.h"
#include "HouseInteriorMap.h"
#include "Housing.h"
#include "HousingDefines.h"
#include "HousingMap.h"
#include "HousingMgr.h"
#include "HousingPackets.h"
#include "Log.h"
#include "Neighborhood.h"
#include "NeighborhoodMgr.h"
#include "ObjectAccessor.h"
#include "Player.h"
#include "SocialMgr.h"
#include "SpellMgr.h"
#include "SpellScript.h"

namespace
{
    [[maybe_unused]] constexpr uint32 HOUSING_DOOR_ENTRY    = 586576;  // retail "Founder's Point Front Door"
    // HOUSE_INTERIOR_MAP_ID now comes from HousingDefines.h (merged from ADV e004d7a4bf) — removed
    // the local duplicate that used to live here to avoid two independent copies of the same 2783.

    // Interior spawn position from NeighborhoodMap ID=7 (sniff-confirmed)
    constexpr float INTERIOR_SPAWN_X = -1000.0f;
    constexpr float INTERIOR_SPAWN_Y = -1000.0f;
    constexpr float INTERIOR_SPAWN_Z = 0.1f;
    constexpr float INTERIOR_SPAWN_O = 0.0f;
}

// Sends a player standing in a house interior out to the plot's TeleportPosition (next to the cornerstone), where retail
// puts them too (12.1.0.69933 sniff: SMSG_NEW_WORLD at NeighborhoodPlot.TeleportPosition after "Leave House").
static void TeleportOutOfHouseInterior(Player* player, HouseInteriorMap* interiorMap)
{
    // Resolve the exit based on the HOUSE this interior belongs to,
    // not on the player's own housing. When a visitor exits a
    // neighbour's house the destination plot is the house owner's
    // plot, not the visitor's.
    ObjectGuid houseOwner = interiorMap->GetOwnerGuid();
    Neighborhood* nbh = nullptr;
    uint8 ownerPlotIndex = INVALID_PLOT_INDEX;
    for (Neighborhood* cand : sNeighborhoodMgr.GetNeighborhoodsForPlayer(houseOwner))
    {
        for (Neighborhood::PlotInfo const& plot : cand->GetPlots())
        {
            if (plot.OwnerGuid == houseOwner && plot.IsOccupied())
            {
                nbh = cand;
                ownerPlotIndex = plot.PlotIndex;
                break;
            }
        }
        if (nbh)
            break;
    }

    // Fall back to the visitor's own housing when the owner lookup
    // fails (shouldn't happen — the owner exists by construction
    // since the interior map was created for them).
    if (!nbh)
    {
        if (Housing* own = player->GetHousing())
        {
            nbh = sNeighborhoodMgr.GetNeighborhood(own->GetNeighborhoodGuid());
            ownerPlotIndex = own->GetPlotIndex();
        }
    }

    // The neighborhood's world map and the plot's TeleportPosition, both from DB2 (NeighborhoodMap / NeighborhoodPlot).
    // Without them there is no plot to return to: send the player home instead of to made-up coordinates.
    uint32 destMapId = nbh ? sHousingMgr.GetWorldMapIdByNeighborhoodMapId(nbh->GetNeighborhoodMapID()) : 0;
    NeighborhoodPlotData const* exitPlot = nullptr;
    if (nbh)
        for (NeighborhoodPlotData const* plot : sHousingMgr.GetPlotsForMap(nbh->GetNeighborhoodMapID()))
            if (plot->PlotIndex == static_cast<int32>(ownerPlotIndex))
                exitPlot = plot;

    if (!destMapId || !exitPlot)
    {
        TC_LOG_ERROR("housing", "go_housing_door: no neighborhood/plot to leave {} to (owner {}) - sending the player home",
            player->GetGUID().ToString(), houseOwner.ToString());
        player->TeleportTo(player->m_homebind);
        return;
    }

    float exitX = exitPlot->TeleportPosition[0];
    float exitY = exitPlot->TeleportPosition[1];
    float exitZ = exitPlot->TeleportPosition[2];

    TC_LOG_DEBUG("housing", "go_housing_door: Teleporting {} from interior (owner {}) to map {} plot {} at ({:.1f},{:.1f},{:.1f})",
        player->GetGUID().ToString(), houseOwner.ToString(), destMapId, ownerPlotIndex, exitX, exitY, exitZ);

    player->TeleportTo(destMapId, exitX, exitY, exitZ, player->GetOrientation());
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

            // Interior side: retail leaves through spell 1234193 ("Leave House", effect 343) cast right after the door
            // opens; the client runs its own house-exit cleanup (editor camera included) off that cast. The
            // teleport itself is done by spell_housing_leave_house.
            if (HouseInteriorMap* interiorMap = dynamic_cast<HouseInteriorMap*>(me->GetMap()))
            {
                if (sSpellMgr->GetSpellInfo(SPELL_HOUSING_LEAVE_HOUSE, DIFFICULTY_NONE))
                    player->CastSpell(player, SPELL_HOUSING_LEAVE_HOUSE, true);
                else
                    TeleportOutOfHouseInterior(player, interiorMap);
                return true;
            }

            HousingMap* housingMap = dynamic_cast<HousingMap*>(me->GetMap());
            if (!housingMap)
            {
                TC_LOG_ERROR("housing", "go_housing_door: Map {} is NOT a HousingMap or HouseInteriorMap", me->GetMapId());
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
            bool isVisit = plotInfo && plotInfo->OwnerGuid != player->GetGUID();
            if (isVisit)
            {
                // Permissions check. Prefer the live Housing object when the owner
                // is online (the settingsFlags may have changed since the last DB
                // write); fall back to the value mirrored onto PlotInfo at load.
                //
                // H-11: this used CanVisitorAccess, which returns false whenever
                // `owner` is null - so every interior visit was refused while the
                // owner was offline, regardless of their settings. CanVisitorAccessPlot
                // resolves guild membership through CharacterCache and neighborhood
                // membership through the Neighborhood objects, so it answers the same
                // question with the owner logged out. It is the same function the
                // teleport handler uses; the plot AreaTrigger now uses it too.
                uint32 settingsFlags = plotInfo->HouseSettingsFlags;
                if (Player* owner = ObjectAccessor::FindPlayer(plotInfo->OwnerGuid))
                    if (Housing const* oh = owner->GetHousing())
                        settingsFlags = oh->GetSettingsFlags();

                if (!sHousingMgr.CanVisitorAccessPlot(player, plotInfo->OwnerGuid, settingsFlags, true))
                {
                    TC_LOG_DEBUG("housing", "go_housing_door: Player {} denied interior access to plot {} "
                        "(owner {} flags 0x{:X})",
                        player->GetGUID().ToString(), plotIndex, plotInfo->OwnerGuid.ToString(),
                        settingsFlags);
                    return true;
                }

                // Route the teleport to the OWNER's interior instance (MapManager
                // reads this before selecting the HouseInteriorMap instance id).
                player->SetHouseVisitTarget(plotInfo->OwnerGuid);
            }

            // Animate the door
            me->UseDoorOrButton();

            // Mark interior BEFORE teleport so the AT leave handler (which fires
            // during the async teleport) knows not to send FlagByte=0x00 and
            // erase the interior's editor state.
            if (Housing* housing = player->GetHousing())
                housing->SetInInterior(true);

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

// 1234193 - Leave House
class spell_housing_leave_house : public SpellScript
{
    void HandleLeave(SpellEffIndex /*effIndex*/)
    {
        Player* player = GetHitUnit() ? GetHitUnit()->ToPlayer() : nullptr;
        if (!player)
            return;

        if (HouseInteriorMap* interiorMap = dynamic_cast<HouseInteriorMap*>(player->GetMap()))
            TeleportOutOfHouseInterior(player, interiorMap);
    }

    void Register() override
    {
        OnEffectHitTarget += SpellEffectFn(spell_housing_leave_house::HandleLeave, EFFECT_0, SPELL_EFFECT_343);
    }
};

// 1233637 - Teleport Home
// 1265142 - Visit House
class spell_housing_plot_teleport : public SpellScript
{
    // The plot to land on is chosen when the cast starts (HousingHandler: CMSG_HOUSING_SVCS_TELEPORT_TO_PLOT, the house
    // finder's reservation). It lies on another map, so it cannot be an explicit cast target - CheckCast would refuse it
    // as out of the spell's self range - and joins the teleport effect only now, after the cast bar.
    void SetPlotDestination(SpellEffIndex effIndex)
    {
        Optional<WorldLocation> dest = sHousingMgr.TakePendingPlotTeleport(GetCaster()->GetGUID());
        if (!dest)
        {
            PreventHitDefaultEffect(effIndex);
            return;
        }

        GetSpell()->m_targets.SetDst(dest->GetPositionX(), dest->GetPositionY(), dest->GetPositionZ(), dest->GetOrientation(), dest->GetMapId());
        if (WorldLocation* hitDest = GetHitDest())
            *hitDest = *dest;
    }

    void Register() override
    {
        OnEffectHitTarget += SpellEffectFn(spell_housing_plot_teleport::SetPlotDestination, EFFECT_ALL, SPELL_EFFECT_TELEPORT_UNITS);
    }
};

void AddSC_go_housing_door()
{
    new go_housing_door();
    RegisterSpellScript(spell_housing_leave_house);
    RegisterSpellScript(spell_housing_plot_teleport);
}
