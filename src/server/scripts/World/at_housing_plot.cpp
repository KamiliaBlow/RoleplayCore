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
#include "ObjectAccessor.h"
#include "PhasingHandler.h"
#include "Player.h"
#include "UpdateData.h"
#include "WorldSession.h"

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

        ObjectGuid ownerGuid;
        uint32 plotId = 0;
        if (at->m_housingPlotAreaTriggerData.has_value())
        {
            ownerGuid = at->m_housingPlotAreaTriggerData->HouseOwnerGUID;
            plotId = at->m_housingPlotAreaTriggerData->PlotID;
        }

        bool isOwnPlot = !ownerGuid.IsEmpty() && player->GetGUID() == ownerGuid;

        // Check visitor access permissions for exterior (plot) access
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

        // Check if this player was already placed on this plot by
        // HousingMap::AddPlayerToMap's deferred ENTER_PLOT event.
        // If so, skip duplicate ENTER_PLOT and spell packets but still do cosmetic phases.
        int8 currentPlot = housingMap->GetPlayerCurrentPlot(player->GetGUID());
        bool alreadyOnPlot = (currentPlot == static_cast<int8>(plotId));

        if (!alreadyOnPlot)
        {
            // Track which plot the player is on
            housingMap->SetPlayerCurrentPlot(player->GetGUID(), static_cast<uint8>(plotId));

            // Retail pattern: UPDATE_OBJECT (VALUES on AT) at same timestamp as ENTER_PLOT.
            // The client's ENTER_PLOT handler looks up the AT GUID in the entity table and
            // stores the entity handle at NeighborhoodSystem+24. The fixture manager then
            // reads HouseGUID from the AT's FHousingPlotAreaTrigger_C fragment (at offset +24).
            //
            // Ensure the AT entity + its FHousingPlotAreaTrigger_C data is on the client
            // BEFORE sending ENTER_PLOT, otherwise the entity table lookup fails.
            {
                UpdateData atUpdate(player->GetMapId());
                if (player->HaveAtClient(at))
                    at->BuildValuesUpdateBlockForPlayer(&atUpdate, player);
                else
                {
                    at->BuildCreateUpdateBlockForPlayer(&atUpdate, player);
                    player->m_clientGUIDs.insert(at->GetGUID());
                }
                if (atUpdate.HasData())
                {
                    WorldPacket atPacket;
                    atUpdate.BuildPacket(&atPacket);
                    player->SendDirectMessage(&atPacket);
                }
            }

            WorldPackets::Neighborhood::NeighborhoodPlayerEnterPlot enterPlot;
            enterPlot.NeighborhoodEntityGuid = at->GetGUID();
            player->SendDirectMessage(enterPlot.Write());

            // Send manual spell packets (spells 1239847, 469226, 1266699 don't exist in DB2)
            housingMap->SendPlotEnterSpellPackets(player, static_cast<uint8>(plotId));
        }

        // Re-send HouseStatusResponse + GetPlayerPermissionsResponse after ENTER_PLOT.
        // The client's ENTER_PLOT handler (vtable[22]) resets the editor state including
        // the stored HouseGuid, so we must re-establish it via HouseStatusResponse (vtable[25])
        // then arm the editor gate check via GetPlayerPermissionsResponse (vtable[24]).
        // Without this sequence, the editor gate check (a1[76] && a1[72]) is always false
        // and no editor mode (Decor, Room, Fixture) can be activated.
        if (!ownerGuid.IsEmpty())
        {
            Player* plotOwner = (isOwnPlot) ? player : ObjectAccessor::FindPlayer(ownerGuid);
            Housing const* ownerHousing = plotOwner ? plotOwner->GetHousing() : nullptr;

            if (ownerHousing)
            {
                // HouseStatusResponse: restores HouseGuid on client after vtable[22] cleared it
                WorldPackets::Housing::HousingHouseStatusResponse statusResponse;
                statusResponse.HouseGuid = ownerHousing->GetHouseGuid();
                statusResponse.AccountGuid = player->GetSession()->GetBattlenetAccountGUID();
                statusResponse.OwnerPlayerGuid = ownerGuid;
                statusResponse.NeighborhoodGuid = ownerHousing->GetNeighborhoodGuid();
                statusResponse.Status = 0;
                statusResponse.FlagByte = 0xE0; // bit7=houseEditing, bit6=plotEntry, bit5=houseEntry
                player->SendDirectMessage(statusResponse.Write());

                // GetPlayerPermissionsResponse: arms the editor gate (vtable[24] sets a1[72..76])
                WorldPackets::Housing::HousingGetPlayerPermissionsResponse permResponse;
                permResponse.HouseGuid = ownerHousing->GetHouseGuid();
                permResponse.ResultCode = 0;
                permResponse.PermissionFlags = isOwnPlot ? 0xE0 : 0x40; // owner=all, visitor=plotEntry only
                player->SendDirectMessage(permResponse.Write());

                TC_LOG_DEBUG("housing", "at_housing_plot: Sent HouseStatus+Permissions for player {} (own={}, flags=0x{:X})",
                    player->GetGUID().ToString(), isOwnPlot, isOwnPlot ? 0xE0 : 0x40);
            }
        }

        // Cosmetic phase shift: when entering own plot, remove 16 cosmetic phases
        // after a ~10 second delay (sniff-verified retail behavior).
        // Only applies to the plot owner, not visitors.
        // Always schedule even if alreadyOnPlot (AddPlayerToMap doesn't handle phases).
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

        TC_LOG_DEBUG("housing", "at_housing_plot: Player {} entered plot {} AT {} (own: {}, owner: {}, proactive: {})",
            player->GetGUID().ToString(), plotId, at->GetGUID().ToString(), isOwnPlot,
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

        ObjectGuid ownerGuid;
        if (at->m_housingPlotAreaTriggerData.has_value())
            ownerGuid = at->m_housingPlotAreaTriggerData->HouseOwnerGUID;

        bool isOwnPlot = !ownerGuid.IsEmpty() && player->GetGUID() == ownerGuid;

        // Remove plot auras via manual packets (spells don't exist in DB2)
        if (housingMap)
            housingMap->SendPlotLeaveAuraRemoval(player);

        // Clear plot tracking
        if (housingMap)
            housingMap->ClearPlayerCurrentPlot(player->GetGUID());

        // Notify the client that the player has left the plot
        WorldPackets::Neighborhood::NeighborhoodPlayerLeavePlot leavePlot;
        player->SendDirectMessage(leavePlot.Write());

        // Send HouseStatusResponse with FlagByte=0x00 to clear all editing contexts (Decor, Room, Fixture).
        // Without this, the editor buttons remain visible after leaving the plot.
        if (isOwnPlot)
        {
            if (Housing const* housing = player->GetHousing())
            {
                WorldPackets::Housing::HousingHouseStatusResponse statusResponse;
                statusResponse.HouseGuid = housing->GetHouseGuid();
                statusResponse.AccountGuid = player->GetSession()->GetBattlenetAccountGUID();
                statusResponse.OwnerPlayerGuid = player->GetGUID();
                statusResponse.NeighborhoodGuid = housing->GetNeighborhoodGuid();
                statusResponse.Status = 0;
                statusResponse.FlagByte = 0x00; // Clear all editing contexts
                player->SendDirectMessage(statusResponse.Write());

                TC_LOG_DEBUG("housing", "at_housing_plot: Sent FlagByte=0x00 HouseStatusResponse for plot owner {} leaving plot",
                    player->GetGUID().ToString());
            }
        }

        // Cosmetic phase shift: when leaving own plot, restore 16 cosmetic phases
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
                    PhasingHandler::AddPhase(p, HOUSING_COSMETIC_PHASES[i], false);

                PhasingHandler::SendToPlayer(p);

                TC_LOG_DEBUG("housing", "at_housing_plot: Restored {} cosmetic phases for plot owner {}",
                    HOUSING_COSMETIC_PHASE_COUNT, playerGuid.ToString());
            }, Milliseconds(HOUSING_COSMETIC_PHASE_DELAY_MS));
        }

        TC_LOG_DEBUG("housing", "at_housing_plot: Player {} left plot AT {} (own: {})",
            player->GetGUID().ToString(), at->GetGUID().ToString(), isOwnPlot);
    }
};

void AddSC_at_housing_plot()
{
    RegisterAreaTriggerAI(at_housing_plot);
}
