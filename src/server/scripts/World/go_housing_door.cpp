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

#include "GameObject.h"
#include "GameObjectAI.h"
#include "HouseInteriorMap.h"
#include "Housing.h"
#include "HousingDefines.h"
#include "HousingMap.h"
#include "HousingMgr.h"
#include "HousingPackets.h"
#include "Log.h"
#include "MapManager.h"
#include "Neighborhood.h"
#include "NeighborhoodMgr.h"
#include "ObjectAccessor.h"
#include "Player.h"
#include "ScriptMgr.h"
#include "SpellMgr.h"
#include "SpellScript.h"

namespace
{
    // Interior spawn position from NeighborhoodMap ID=7 (sniff-confirmed)
    constexpr float INTERIOR_SPAWN_X = -1000.0f;
    constexpr float INTERIOR_SPAWN_Y = -1000.0f;
    constexpr float INTERIOR_SPAWN_Z = 0.1f;
    constexpr float INTERIOR_SPAWN_O = 0.0f;
}

// Teleports a player out of a house interior to the plot's TeleportPosition next to the cornerstone.
static void TeleportOutOfHouseInterior(Player* player, HouseInteriorMap* interiorMap)
{
    // Resolve the exit through the interior's owner: a visitor returns to the house owner's plot.
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

    // Fall back to the player's own housing when the owner lookup fails.
    if (!nbh)
    {
        if (Housing* own = player->GetHousing())
        {
            nbh = sNeighborhoodMgr.GetNeighborhood(own->GetNeighborhoodGuid());
            ownerPlotIndex = own->GetPlotIndex();
        }
    }

    // Destination comes from DB2; without a plot to return to, send the player home.
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

    // Several neighborhoods share a world map; the house's own one is the instance whose id is its GUID counter.
    uint32 const neighborhoodId = static_cast<uint32>(nbh->GetGuid().GetCounter());
    if (!sMapMgr->FindOrCreateHousingMap(destMapId, neighborhoodId))
    {
        player->TeleportTo(player->m_homebind);
        return;
    }

    player->TeleportTo(TeleportLocation{ .Location = WorldLocation(destMapId, exitX, exitY, exitZ, player->GetOrientation()),
        .InstanceId = neighborhoodId });
}

// Housing front door GO (entry 602702): teleports the player to the house's interior instance.
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

            // Interior side: retail leaves through spell 1234193; fall back to a manual teleport when it is not in the DB.
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
                TC_LOG_ERROR("housing", "go_housing_door: Map {} is not a HousingMap or HouseInteriorMap", me->GetMapId());
                return true;
            }

            // Resolve the door's plot: tracked doors first, then the player's current plot, then the nearest plot.
            int8 plotIndex = housingMap->GetPlotIndexForHouseGO(me->GetGUID());
            if (plotIndex < 0)
            {
                plotIndex = housingMap->GetPlayerCurrentPlot(player->GetGUID());
                if (plotIndex < 0)
                {
                    Neighborhood* nbh = housingMap->GetNeighborhood();
                    if (nbh)
                    {
                        float bestDist = std::numeric_limits<float>::max();
                        float doorX = me->GetPositionX();
                        float doorY = me->GetPositionY();
                        for (NeighborhoodPlotData const* plot : sHousingMgr.GetPlotsForMap(nbh->GetNeighborhoodMapID()))
                        {
                            float dx = doorX - plot->HousePosition[0];
                            float dy = doorY - plot->HousePosition[1];
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
                    TC_LOG_ERROR("housing", "go_housing_door: Could not determine plot for door GO {} (player {} at {:.1f},{:.1f},{:.1f})",
                        me->GetGUID().ToString(), player->GetGUID().ToString(),
                        me->GetPositionX(), me->GetPositionY(), me->GetPositionZ());
                    return true;
                }

                TC_LOG_DEBUG("housing", "go_housing_door: Door GO {} not tracked, resolved plot {} via fallback",
                    me->GetGUID().ToString(), plotIndex);
            }

            Neighborhood* neighborhood = housingMap->GetNeighborhood();
            if (!neighborhood)
            {
                TC_LOG_ERROR("housing", "go_housing_door: Neighborhood is NULL on mapId={}", housingMap->GetId());
                return true;
            }

            Neighborhood::PlotInfo const* plotInfo = neighborhood->GetPlotInfo(static_cast<uint8>(plotIndex));
            // Houses belong to the account: another character of the account enters it as its owner.
            bool const accountHouse = plotInfo && player->GetHousingByOwner(plotInfo->OwnerGuid);
            bool isVisit = plotInfo && plotInfo->OwnerGuid != player->GetGUID() && !accountHouse;
            if (accountHouse && plotInfo->OwnerGuid != player->GetGUID())
                player->SetHouseVisitTarget(plotInfo->OwnerGuid); // route to the buyer's interior instance
            if (isVisit)
            {
                // Prefer the owner's live settings when online; fall back to the PlotInfo mirror.
                uint32 settingsFlags = plotInfo->HouseSettingsFlags;
                if (Player* owner = ObjectAccessor::FindPlayer(plotInfo->OwnerGuid))
                    if (Housing const* oh = owner->GetHousing())
                        settingsFlags = oh->GetSettingsFlags();

                if (!sHousingMgr.CanVisitorAccessPlot(player, plotInfo->OwnerGuid, settingsFlags, true))
                {
                    TC_LOG_DEBUG("housing", "go_housing_door: Player {} denied interior access to plot {} (owner {} flags 0x{:X})",
                        player->GetGUID().ToString(), plotIndex, plotInfo->OwnerGuid.ToString(), settingsFlags);

                    // Retail refusal: PERMISSIONS_FAILURE with FailureType PERMISSION_DENIED, ErrorCode 0.
                    WorldPackets::Housing::HousingSvcsNotifyPermissionsFailure failure;
                    failure.FailureType = static_cast<uint8>(HOUSING_RESULT_PERMISSION_DENIED);
                    failure.ErrorCode = 0;
                    player->SendDirectMessage(failure.Write());
                    return true;
                }

                // Route the teleport to the owner's interior instance (MapManager reads this).
                player->SetHouseVisitTarget(plotInfo->OwnerGuid);
            }

            me->UseDoorOrButton();

            // Mark the interior before the teleport; the AT leave handler fires during the async transfer.
            if (Housing* housing = player->GetHousing())
                housing->SetInInterior(true);

            if (!player->TeleportTo(HOUSE_INTERIOR_MAP_ID,
                INTERIOR_SPAWN_X, INTERIOR_SPAWN_Y, INTERIOR_SPAWN_Z, INTERIOR_SPAWN_O))
            {
                TC_LOG_ERROR("housing", "go_housing_door: TeleportTo FAILED - player {} to map {} from plot {}",
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
    // The plot was chosen at cast start and may lie on another map; the script performs the teleport itself.
    void TeleportToPlot(SpellEffIndex effIndex)
    {
        PreventHitDefaultEffect(effIndex);

        Player* player = GetHitPlayer();
        if (!player)
            return;

        Optional<HousingMgr::PendingPlotTeleport> pending = sHousingMgr.TakePendingPlotTeleport(player->GetGUID());
        if (!pending || !sMapMgr->FindOrCreateHousingMap(pending->Dest.GetMapId(), pending->NeighborhoodId))
            return;

        player->TeleportTo(TeleportLocation{ .Location = pending->Dest, .InstanceId = pending->NeighborhoodId }, TELE_TO_SPELL, GetSpellInfo()->Id);
    }

    void Register() override
    {
        OnEffectHitTarget += SpellEffectFn(spell_housing_plot_teleport::TeleportToPlot, EFFECT_ALL, SPELL_EFFECT_TELEPORT_UNITS);
    }
};

// Decor doors (e.g. 527736): every use flips the state; GO_FLAG_IN_USE must clear after ~3 s
// or the client refuses further uses.
struct go_housing_decor_door : public GameObjectAI
{
    static constexpr uint32 IN_USE_DURATION = 3 * IN_MILLISECONDS;

    go_housing_decor_door(GameObject* go) : GameObjectAI(go), _inUseTimer(0) { }

    bool OnGossipHello(Player* /*player*/) override
    {
        if (_inUseTimer)
            return true;

        me->SetFlag(GO_FLAG_IN_USE);
        me->RemoveDynamicFlag(GO_DYNFLAG_LO_STATE_TRANSITION_ANIM_DONE);
        me->SetGoState(me->GetGoState() == GO_STATE_READY ? GO_STATE_ACTIVE : GO_STATE_READY);
        _inUseTimer = IN_USE_DURATION;
        return true;
    }

    void UpdateAI(uint32 diff) override
    {
        if (!_inUseTimer)
            return;

        if (_inUseTimer > diff)
        {
            _inUseTimer -= diff;
            return;
        }

        _inUseTimer = 0;
        me->RemoveFlag(GO_FLAG_IN_USE);
    }

private:
    uint32 _inUseTimer;
};

// Decor lights/fireplaces (Goober with empty data, e.g. 527890 fireplace, 527892 chandelier): every use flips the GO state.
struct go_housing_decor_toggle : public GameObjectAI
{
    go_housing_decor_toggle(GameObject* go) : GameObjectAI(go) { }

    bool OnGossipHello(Player* /*player*/) override
    {
        me->RemoveDynamicFlag(GO_DYNFLAG_LO_STATE_TRANSITION_ANIM_DONE);
        me->SetGoState(me->GetGoState() == GO_STATE_READY ? GO_STATE_ACTIVE : GO_STATE_READY);
        return true;
    }
};

void AddSC_go_housing_door()
{
    new go_housing_door();
    RegisterGameObjectAI(go_housing_decor_door);
    RegisterGameObjectAI(go_housing_decor_toggle);
    RegisterSpellScript(spell_housing_leave_house);
    RegisterSpellScript(spell_housing_plot_teleport);
}
