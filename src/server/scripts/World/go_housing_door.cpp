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
#include "ObjectAccessor.h"
#include "Player.h"
#include "ScriptMgr.h"
#include "SpellScript.h"
#include "WorldSession.h"

namespace
{
    // Interior spawn position from NeighborhoodMap ID=7 (sniff-confirmed)
    constexpr float INTERIOR_SPAWN_X = -1000.0f;
    constexpr float INTERIOR_SPAWN_Y = -1000.0f;
    constexpr float INTERIOR_SPAWN_Z = 0.1f;
    constexpr float INTERIOR_SPAWN_O = 0.0f;
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

            // Interior side: the door and the "Leave House" button share one exit path -
            // it clears editor/interior state and emits the exit HOUSE_STATUS flip.
            if (dynamic_cast<HouseInteriorMap*>(me->GetMap()))
            {
                player->GetSession()->LeaveHouseInterior();
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

            // Arrival anchors to the owner's placed front door - entering players appear by it
            // wherever it was moved. An offline owner's house still exists as the PlotInfo
            // snapshot; without a door there either, the fixed entry hall spawn stands in.
            ObjectGuid const ownerGuid = plotInfo && !plotInfo->OwnerGuid.IsEmpty() ? plotInfo->OwnerGuid : player->GetGUID();
            Housing const* ownerHousing = player->GetHousingByOwner(ownerGuid);
            if (!ownerHousing)
                if (Player* ownerPlayer = ObjectAccessor::FindConnectedPlayer(ownerGuid))
                    ownerHousing = ownerPlayer->GetHousingByOwner(ownerGuid);

            Position entryPos(INTERIOR_SPAWN_X, INTERIOR_SPAWN_Y, INTERIOR_SPAWN_Z, INTERIOR_SPAWN_O);
            if (ownerHousing)
                entryPos = ownerHousing->GetInteriorEntryPosition();
            else if (plotInfo)
            {
                std::vector<Housing::Room const*> snapshotRooms;
                for (Housing::Room const& room : plotInfo->Rooms)
                    snapshotRooms.push_back(&room);

                for (Housing::PlacedDecor const& decor : plotInfo->Decor)
                    if (IsStarterDoorDecor(decor.DecorEntryId))
                    {
                        entryPos = Housing::ComputeInteriorEntryPosition(decor, snapshotRooms);
                        break;
                    }
            }

            // Retail redirects on the interior entry too: each house visit gets its own client
            // session, so editor state from inside never outlives the exit redirect.
            player->GetSession()->RequestWorldRedirect();

            if (!player->TeleportTo(HOUSE_INTERIOR_MAP_ID,
                entryPos.GetPositionX(), entryPos.GetPositionY(), entryPos.GetPositionZ(), entryPos.GetOrientation()))
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

        player->GetSession()->LeaveHouseInterior();
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
