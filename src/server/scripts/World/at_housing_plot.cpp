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

#include "AreaTrigger.h"
#include "AreaTriggerAI.h"
#include "ChatPackets.h"
#include "EventProcessor.h"
#include "Housing.h"
#include "HousingDefines.h"
#include "HousingMap.h"
#include "HousingMgr.h"
#include "HousingPackets.h"
#include "Log.h"
#include "Neighborhood.h"
#include "ObjectAccessor.h"
#include "PhasingHandler.h"
#include "Player.h"
#include "ScriptMgr.h"
#include "SpellMgr.h"
#include "WorldSession.h"

// Plot occupancy rides PlayerHouseInfoComponentData.CurrentHouse (HouseGuid on enter, empty on exit).
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

        int8 plotIdx = housingMap->GetPlotIndexForAreaTrigger(at->GetGUID());
        if (plotIdx < 0)
        {
            TC_LOG_DEBUG("housing", "at_housing_plot: AT {} not registered as a plot AT - ignoring enter",
                at->GetGUID().ToString());
            return;
        }

        Neighborhood const* nbh = housingMap->GetNeighborhood();
        Neighborhood::PlotInfo const* plotInfo = nbh ? nbh->GetPlotInfo(static_cast<uint8>(plotIdx)) : nullptr;

        ObjectGuid ownerGuid = plotInfo ? plotInfo->OwnerGuid : ObjectGuid::Empty;
        ObjectGuid houseGuid = plotInfo ? plotInfo->HouseGuid : ObjectGuid::Empty;

        // Houses belong to the account: a plot bought by another character of the account is the player's own.
        bool isOwnPlot = !ownerGuid.IsEmpty() && (player->GetGUID() == ownerGuid || player->GetHousingByOwner(ownerGuid));

        if (!isOwnPlot && !ownerGuid.IsEmpty())
        {
            // Prefer the owner's live settings; fall back to the flags mirrored onto PlotInfo when they are offline.
            uint32 settingsFlags = plotInfo ? plotInfo->HouseSettingsFlags : HOUSE_SETTING_DEFAULT;
            if (Player* owner = ObjectAccessor::FindPlayer(ownerGuid))
                if (Housing const* ownerHousing = owner->GetHousing())
                    settingsFlags = ownerHousing->GetSettingsFlags();

            if (!sHousingMgr.CanVisitorAccessPlot(player, ownerGuid, settingsFlags, false))
            {
                TC_LOG_DEBUG("housing", "at_housing_plot: Player {} denied plot access (owner {} flags 0x{:X})",
                    player->GetGUID().ToString(), ownerGuid.ToString(), settingsFlags);

                // Track the trespasser so the delayed eviction can tell "still here" from "walked away".
                housingMap->SetPlayerCurrentPlot(player->GetGUID(), static_cast<uint8>(plotIdx));

                // Retail eviction: warning spell 1245416 (5 s aura), denial as RAID_BOSS_WHISPER, teleport on expiry.
                if (sSpellMgr->GetSpellInfo(SPELL_HOUSING_PLOT_EVICT_WARNING, DIFFICULTY_NONE))
                    player->CastSpell(player, SPELL_HOUSING_PLOT_EVICT_WARNING, true);

                {
                    WorldPackets::Chat::Chat warning;
                    warning.Initialize(CHAT_MSG_RAID_BOSS_WHISPER, LANG_UNIVERSAL, player, player,
                        player->GetSession()->GetTrinityString(HOUSING_STRING_PLOT_ACCESS_DENIED));
                    warning.SpellID = SPELL_HOUSING_PLOT_EVICT_WARNING;
                    player->SendDirectMessage(warning.Write());
                }

                // Retail kicks to the plot's own TeleportPosition (the cornerstone), map origin as fallback.
                uint32 const mapId = player->GetMapId();
                float x = 0.0f, y = 0.0f, z = 0.0f, o = 0.0f;
                bool haveTarget = false;
                NeighborhoodMapData const* nmData = sHousingMgr.GetNeighborhoodMapDataForWorldMap(mapId);
                if (nmData)
                {
                    x = nmData->Origin[0]; y = nmData->Origin[1]; z = nmData->Origin[2]; o = nmData->EntryRotation;
                    haveTarget = true;
                    for (NeighborhoodPlotData const* plotData : sHousingMgr.GetPlotsForMap(nmData->ID))
                        if (plotData->PlotIndex == plotIdx)
                        {
                            x = plotData->TeleportPosition[0]; y = plotData->TeleportPosition[1]; z = plotData->TeleportPosition[2];
                            o = plotData->TeleportFacing;
                            break;
                        }
                }

                if (haveTarget)
                    player->m_Events.AddEventAtOffset([guid = player->GetGUID(), mapId, plotIdx, x, y, z, o]()
                    {
                        Player* visitor = ObjectAccessor::FindConnectedPlayer(guid);
                        if (!visitor || visitor->GetMapId() != mapId)
                            return;

                        // Walked off the plot during the warning: the exit cleared the tracking mark.
                        HousingMap* hMap = dynamic_cast<HousingMap*>(visitor->GetMap());
                        if (!hMap || hMap->GetPlayerCurrentPlot(guid) != plotIdx)
                            return;

                        // Cancel if the plot changed hands, the visitor became an owner, or access was granted meanwhile.
                        Neighborhood* nbh = hMap->GetNeighborhood();
                        Neighborhood::PlotInfo const* evictedPlot = nbh ? nbh->GetPlotInfo(static_cast<uint8>(plotIdx)) : nullptr;
                        if (!evictedPlot || !evictedPlot->IsOccupied())
                            return;
                        if (visitor->GetGUID() == evictedPlot->OwnerGuid || visitor->GetHousingByOwner(evictedPlot->OwnerGuid))
                            return;
                        uint32 flags = evictedPlot->HouseSettingsFlags;
                        if (Player* owner = ObjectAccessor::FindPlayer(evictedPlot->OwnerGuid))
                            if (Housing const* ownerHousing = owner->GetHousing())
                                flags = ownerHousing->GetSettingsFlags();
                        if (sHousingMgr.CanVisitorAccessPlot(visitor, evictedPlot->OwnerGuid, flags, false))
                            return;

                        visitor->TeleportTo(mapId, x, y, z, o);
                    }, HOUSING_PLOT_EVICT_DELAY);
                return;
            }
        }

        // De-dup: HousingMap::AddPlayerToMap may have already tracked players who logged out on a plot.
        int8 currentPlot = housingMap->GetPlayerCurrentPlot(player->GetGUID());
        bool alreadyOnPlot = (currentPlot == plotIdx);

        // Always write CurrentHouse even when unchanged so the field-change callback stays wired.
        player->SetCurrentHouse(houseGuid);

        if (!alreadyOnPlot)
        {
            housingMap->SetPlayerCurrentPlot(player->GetGUID(), static_cast<uint8>(plotIdx));

            // Plot-enter spells (1239847, 469226, 1266699) do not exist in DB2 and are sent as manual packets.
            housingMap->SendPlotEnterSpellPackets(player, static_cast<uint8>(plotIdx));
        }

        // HouseStatus + Permissions keep the client's editor-mode gate armed after plot entry.
        if (!ownerGuid.IsEmpty())
        {
            Housing const* ownerHousing = player->GetHousingByOwner(ownerGuid);
            if (!ownerHousing)
                if (Player* plotOwner = ObjectAccessor::FindPlayer(ownerGuid))
                    ownerHousing = plotOwner->GetHousingByOwner(ownerGuid);

            if (ownerHousing)
            {
                WorldPackets::Housing::HousingHouseStatusResponse statusResponse;
                statusResponse.HouseGuid = ownerHousing->GetHouseGuid();
                // The OWNER's battle.net account, not the viewer's, or the client skips the visitor check.
                statusResponse.AccountGuid = plotInfo && !plotInfo->OwnerBnetGuid.IsEmpty()
                    ? plotInfo->OwnerBnetGuid
                    : player->GetSession()->GetBattlenetAccountGUID();
                statusResponse.OwnerPlayerGuid = ownerGuid;
                statusResponse.Status = 0;
                statusResponse.EditModeFlags = isOwnPlot ? ownerHousing->GetEditModeStatusFlags() : 0;
                player->SendDirectMessage(statusResponse.Write());

                WorldPackets::Housing::HousingGetPlayerPermissionsResponse permResponse;
                permResponse.HouseGuid = ownerHousing->GetHouseGuid();
                permResponse.ResultCode = 0;
                if (isOwnPlot)
                    permResponse.PermissionFlags = HOUSING_PERMISSIONS_OWNER;
                else
                {
                    uint32 visitorSettings = ownerHousing->GetSettingsFlags();
                    permResponse.PermissionFlags = HOUSING_PERMISSIONS_VISITOR
                        | (sHousingMgr.CanVisitorExportBlueprint(player, ownerGuid, visitorSettings)
                            ? HOUSING_PERMISSIONS_BLUEPRINT : 0);
                }
                player->SendDirectMessage(permResponse.Write());

                TC_LOG_DEBUG("housing", "at_housing_plot: Sent HouseStatus+Permissions for player {} (own={}, flags=0x{:X})",
                    player->GetGUID().ToString(), isOwnPlot, permResponse.PermissionFlags);
            }
        }

        // Owner entering their own plot drops the cosmetic phases after a delay.
        if (isOwnPlot)
        {
            ObjectGuid playerGuid = player->GetGUID();
            player->m_Events.AddEventAtOffset([playerGuid, plotIdx]()
            {
                Player* p = ObjectAccessor::FindPlayer(playerGuid);
                if (!p || !p->IsInWorld())
                    return;

                // Only while still on that plot.
                HousingMap* map = dynamic_cast<HousingMap*>(p->GetMap());
                if (!map || map->GetPlayerCurrentPlot(playerGuid) != plotIdx)
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

        // Houses belong to the account: a plot bought by another character of the account is the player's own.
        bool isOwnPlot = !ownerGuid.IsEmpty() && (player->GetGUID() == ownerGuid || player->GetHousingByOwner(ownerGuid));

        // Plot-enter auras come from manual packets (no DB2 spells) and are removed manually on leave.
        housingMap->SendPlotLeaveAuraRemoval(player);

        // Leaving the plot during the eviction warning cancels the pending teleport.
        player->RemoveAura(SPELL_HOUSING_PLOT_EVICT_WARNING);

        housingMap->ClearPlayerCurrentPlot(player->GetGUID());

        player->SetCurrentHouse(ObjectGuid::Empty);

        if (isOwnPlot)
        {
            if (Housing* housing = player->GetHousing())
            {
                // Skip when leaving towards the interior; otherwise close the server-side editor too.
                if (!housing->IsInInterior())
                {
                    if (housing->GetEditorMode() != HOUSING_EDITOR_MODE_NONE)
                    {
                        housing->SetEditorMode(HOUSING_EDITOR_MODE_NONE);
                        player->RemoveUnitFlag(UNIT_FLAG_PACIFIED);
                        player->RemoveUnitFlag2(UNIT_FLAG2_NO_ACTIONS);
                        player->ReplaceAllSilencedSchoolMask(SpellSchoolMask(0));
                    }

                    WorldPackets::Housing::HousingHouseStatusResponse statusResponse;
                    statusResponse.HouseGuid = housing->GetHouseGuid();
                    statusResponse.AccountGuid = player->GetSession()->GetBattlenetAccountGUID();
                    statusResponse.OwnerPlayerGuid = player->GetGUID();
                    statusResponse.Status = 0;
                    player->SendDirectMessage(statusResponse.Write());

                    TC_LOG_DEBUG("housing", "at_housing_plot: Sent HouseStatusResponse for plot owner {} leaving plot",
                        player->GetGUID().ToString());
                }
            }
        }

        // Restore the cosmetic phases when the owner leaves.
        if (isOwnPlot)
        {
            ObjectGuid playerGuid = player->GetGUID();
            player->m_Events.AddEventAtOffset([playerGuid, plotIdx]()
            {
                Player* p = ObjectAccessor::FindPlayer(playerGuid);
                if (!p || !p->IsInWorld())
                    return;

                // Only while still in the neighborhood and off that plot.
                HousingMap* map = dynamic_cast<HousingMap*>(p->GetMap());
                if (!map || map->GetPlayerCurrentPlot(playerGuid) == plotIdx)
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
