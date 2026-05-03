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
#include "AreaTrigger.h"
#include "AreaTriggerAI.h"
#include "EventProcessor.h"
#include "Housing.h"
#include "HousingDefines.h"
#include "HousingMap.h"
#include "HousingMgr.h"
#include "HousingPackets.h"
#include "Log.h"
#include "Neighborhood.h"
#include "NeighborhoodMgr.h"
#include "ObjectAccessor.h"
#include "PhasingHandler.h"
#include "Player.h"

// 12.0.5 plot-entry mechanism:
//   - No more SMSG_NEIGHBORHOOD_PLAYER_ENTER_PLOT / LEAVE_PLOT opcodes (removed in
//     TC commit 4c14988 / WoW build 12.0.5.67114).
//   - No more FHousingPlotAreaTrigger_C entity fragment on the plot AT.
//   - Plot ownership / "am I on a plot" is communicated via the
//     PlayerHouseInfoComponentData.CurrentHouse UpdateField on the Player entity.
//     Server writes the plot's HouseGuid to CurrentHouse on enter and clears it
//     on exit; the client observes the UPDATE_OBJECT change to track occupancy.
struct at_housing_plot : AreaTriggerAI
{
    using AreaTriggerAI::AreaTriggerAI;

    void OnUnitEnter(Unit* unit) override
    {
        Player* player = unit->ToPlayer();
        if (!player)
            return;

        HousingMap* housingMap = dynamic_cast<HousingMap*>(player->GetMap());
        if (!housingMap)
            return;

        // Resolve which plot this AT represents from the HousingMap's AT registry.
        int8 plotIdx = housingMap->GetPlotIndexForAreaTrigger(at->GetGUID());
        if (plotIdx < 0)
        {
            TC_LOG_DEBUG("housing", "at_housing_plot: AT {} not registered as a plot AT — ignoring enter",
                at->GetGUID().ToString());
            return;
        }

        Neighborhood const* nbh = housingMap->GetNeighborhood();
        Neighborhood::PlotInfo const* plotInfo = nbh ? nbh->GetPlotInfo(static_cast<uint8>(plotIdx)) : nullptr;

        ObjectGuid ownerGuid = plotInfo ? plotInfo->OwnerGuid : ObjectGuid::Empty;
        ObjectGuid houseGuid = plotInfo ? plotInfo->HouseGuid : ObjectGuid::Empty;

        bool isOwnPlot = !ownerGuid.IsEmpty() && player->GetGUID() == ownerGuid;

        // Visitor access permission check — only matters for plots with an owner.
        if (!isOwnPlot && !ownerGuid.IsEmpty())
        {
            if (Player* owner = ObjectAccessor::FindPlayer(ownerGuid))
            {
                if (Housing const* ownerHousing = owner->GetHousing())
                {
                    if (!sHousingMgr.CanVisitorAccess(player, owner, ownerHousing->GetSettingsFlags(), false))
                    {
                        TC_LOG_DEBUG("housing", "at_housing_plot: Player {} denied plot access (owner {} flags 0x{:X})",
                            player->GetGUID().ToString(), ownerGuid.ToString(), ownerHousing->GetSettingsFlags());
                        return;
                    }
                }
            }
        }

        // De-dup: HousingMap::AddPlayerToMap may have already pushed the CurrentHouse
        // update during the initial entity flush for players who logged out on a plot.
        int8 currentPlot = housingMap->GetPlayerCurrentPlot(player->GetGUID());
        bool alreadyOnPlot = (currentPlot == plotIdx);

        // 12.0.5 plot-entry: write the plot's HouseGuid to PlayerHouseInfoComponent.CurrentHouse.
        // The UPDATE_OBJECT carrying this change replaces the removed
        // SMSG_NEIGHBORHOOD_PLAYER_ENTER_PLOT opcode; the client reads CurrentHouse to
        // populate its NeighborhoodSystem TLS (+280) "am I on a plot" state.
        // Always invoked — SetCurrentHouse short-circuits when the value is unchanged, so
        // logged-in-on-plot players (alreadyOnPlot=true via HousingMap::SetPlayerCurrentPlot
        // at AddPlayerToMap) still get the field-change callback wired correctly.
        player->SetCurrentHouse(houseGuid);

        if (!alreadyOnPlot)
        {
            housingMap->SetPlayerCurrentPlot(player->GetGUID(), static_cast<uint8>(plotIdx));

            // Plot-enter spell packets (1239847, 469226, 1266699) still apply — those
            // spells don't exist in DB2 so we send them via manual packets.
            housingMap->SendPlotEnterSpellPackets(player, static_cast<uint8>(plotIdx));
        }

        // HouseStatusResponse + Permissions keep the editor-mode gate armed on the client.
        // These opcodes were NOT touched by 12.0.5 — still required after plot entry so the
        // editor-gate check (a1[76] && a1[72]) evaluates true.
        if (!ownerGuid.IsEmpty())
        {
            Player* plotOwner = isOwnPlot ? player : ObjectAccessor::FindPlayer(ownerGuid);
            Housing const* ownerHousing = plotOwner ? plotOwner->GetHousing() : nullptr;

            if (ownerHousing)
            {
                WorldPackets::Housing::HousingHouseStatusResponse statusResponse;
                statusResponse.HouseGuid = ownerHousing->GetHouseGuid();
                statusResponse.AccountGuid = player->GetSession()->GetBattlenetAccountGUID();
                statusResponse.OwnerPlayerGuid = ownerGuid;
                statusResponse.NeighborhoodGuid = ownerHousing->GetNeighborhoodGuid();
                statusResponse.Status = 0;
                statusResponse.PermissionFlags = isOwnPlot ? 0xE0 : 0x40; // owner gets full, visitor gets plot-entry only
                player->SendDirectMessage(statusResponse.Write());

                WorldPackets::Housing::HousingGetPlayerPermissionsResponse permResponse;
                permResponse.HouseGuid = ownerHousing->GetHouseGuid();
                permResponse.ResultCode = 0;
                permResponse.PermissionFlags = isOwnPlot ? 0xE0 : 0x40;
                player->SendDirectMessage(permResponse.Write());

                TC_LOG_DEBUG("housing", "at_housing_plot: Sent HouseStatus+Permissions for player {} (own={}, flags=0x{:X})",
                    player->GetGUID().ToString(), isOwnPlot, isOwnPlot ? 0xE0 : 0x40);
            }
        }

        // Cosmetic phase shift: owner entering own plot removes 16 cosmetic phases
        // after a ~10 second delay (sniff-verified retail behavior).
        if (isOwnPlot)
        {
            ObjectGuid playerGuid = player->GetGUID();
            player->m_Events.AddEventAtOffset([playerGuid]()
            {
                Player* p = ObjectAccessor::FindPlayer(playerGuid);
                if (!p || !p->IsInWorld())
                    return;

                for (uint32 i = 0; i < HOUSING_COSMETIC_PHASE_COUNT; ++i)
                    PhasingHandler::RemovePhase(p, HOUSING_COSMETIC_PHASES[i], false);

                PhasingHandler::SendToPlayer(p);

                TC_LOG_DEBUG("housing", "at_housing_plot: Removed {} cosmetic phases for plot owner {}",
                    HOUSING_COSMETIC_PHASE_COUNT, playerGuid.ToString());
            }, Milliseconds(HOUSING_COSMETIC_PHASE_DELAY_MS));
        }

        TC_LOG_DEBUG("housing", "at_housing_plot: Player {} entered plot {} AT {} (own={}, owner={}, dedup={})",
            player->GetGUID().ToString(), plotIdx, at->GetGUID().ToString(), isOwnPlot,
            ownerGuid.IsEmpty() ? "none" : ownerGuid.ToString(), alreadyOnPlot);
    }

    void OnUnitExit(Unit* unit, AreaTriggerExitReason reason) override
    {
        if (reason != AreaTriggerExitReason::NotInside)
            return;

        Player* player = unit->ToPlayer();
        if (!player)
            return;

        HousingMap* housingMap = dynamic_cast<HousingMap*>(player->GetMap());
        if (!housingMap)
            return;

        int8 plotIdx = housingMap->GetPlotIndexForAreaTrigger(at->GetGUID());
        Neighborhood const* nbh = housingMap->GetNeighborhood();
        Neighborhood::PlotInfo const* plotInfo = (nbh && plotIdx >= 0)
            ? nbh->GetPlotInfo(static_cast<uint8>(plotIdx)) : nullptr;
        ObjectGuid ownerGuid = plotInfo ? plotInfo->OwnerGuid : ObjectGuid::Empty;

        bool isOwnPlot = !ownerGuid.IsEmpty() && player->GetGUID() == ownerGuid;

        // Remove plot-auras (manual packets, spells aren't in DB2).
        housingMap->SendPlotLeaveAuraRemoval(player);

        housingMap->ClearPlayerCurrentPlot(player->GetGUID());

        // 12.0.5 plot-leave: clear PlayerHouseInfoComponent.CurrentHouse so the client's
        // NeighborhoodSystem TLS drops its "on plot" flag.
        player->SetCurrentHouse(ObjectGuid::Empty);

        // Clear editor contexts (Decor, Room, Fixture) by sending FlagByte=0x00
        // HouseStatusResponse. Skip when leaving the plot is the result of entering
        // the interior — the map transfer would erase interior editor state otherwise.
        if (isOwnPlot)
        {
            if (Housing const* housing = player->GetHousing())
            {
                if (!housing->IsInInterior())
                {
                    WorldPackets::Housing::HousingHouseStatusResponse statusResponse;
                    statusResponse.HouseGuid = housing->GetHouseGuid();
                    statusResponse.AccountGuid = player->GetSession()->GetBattlenetAccountGUID();
                    statusResponse.OwnerPlayerGuid = player->GetGUID();
                    statusResponse.NeighborhoodGuid = housing->GetNeighborhoodGuid();
                    statusResponse.Status = 0;
                    statusResponse.PermissionFlags = 0x00; // leaving plot — clear all permissions
                    player->SendDirectMessage(statusResponse.Write());

                    TC_LOG_DEBUG("housing", "at_housing_plot: Sent HouseStatusResponse(PermissionFlags=0) for plot owner {} leaving plot",
                        player->GetGUID().ToString());
                }
            }
        }

        // Restore cosmetic phases when owner leaves.
        if (isOwnPlot)
        {
            ObjectGuid playerGuid = player->GetGUID();
            player->m_Events.AddEventAtOffset([playerGuid]()
            {
                Player* p = ObjectAccessor::FindPlayer(playerGuid);
                if (!p || !p->IsInWorld())
                    return;

                for (uint32 i = 0; i < HOUSING_COSMETIC_PHASE_COUNT; ++i)
                    PhasingHandler::AddPhase(p, HOUSING_COSMETIC_PHASES[i], false);

                PhasingHandler::SendToPlayer(p);

                TC_LOG_DEBUG("housing", "at_housing_plot: Restored {} cosmetic phases for plot owner {}",
                    HOUSING_COSMETIC_PHASE_COUNT, playerGuid.ToString());
            }, Milliseconds(HOUSING_COSMETIC_PHASE_DELAY_MS));
        }

        TC_LOG_DEBUG("housing", "at_housing_plot: Player {} left plot AT {} (own={})",
            player->GetGUID().ToString(), at->GetGUID().ToString(), isOwnPlot);
    }
};

void AddSC_at_housing_plot()
{
    RegisterAreaTriggerAI(at_housing_plot);
}
