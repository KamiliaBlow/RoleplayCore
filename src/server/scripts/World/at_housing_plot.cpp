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

#include "ChatPackets.h"
#include "ScriptMgr.h"
#include "SpellMgr.h"
#include "AreaTrigger.h"
#include "AreaTriggerAI.h"
#include "EventProcessor.h"
#include "Housing.h"
#include "HousingDefines.h"
#include "HousingMap.h"
#include "HousingMgr.h"
#include "HousingPackets.h"
#include "Log.h"
#include "WorldSession.h"
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

        // Houses belong to the account: a plot bought by another character of the account is the player's own.
        bool isOwnPlot = !ownerGuid.IsEmpty() && (player->GetGUID() == ownerGuid || player->GetHousingByOwner(ownerGuid));

        // Visitor access permission check — only matters for plots with an owner.
        //
        // H-11: the check used to sit inside `if (Player* owner = FindPlayer(...))`,
        // so an offline owner meant the check was skipped entirely and access was
        // GRANTED — the exact opposite of the door script, which refused every visit
        // while the owner was offline. One setting, two implementations, opposite
        // answers. Both now use CanVisitorAccessPlot, which handles the offline owner,
        // and both fall back to the settings mirrored onto PlotInfo at load.
        if (!isOwnPlot && !ownerGuid.IsEmpty())
        {
            uint32 settingsFlags = plotInfo ? plotInfo->HouseSettingsFlags : HOUSE_SETTING_DEFAULT;
            if (Player* owner = ObjectAccessor::FindPlayer(ownerGuid))
                if (Housing const* ownerHousing = owner->GetHousing())
                    settingsFlags = ownerHousing->GetSettingsFlags();

            if (!sHousingMgr.CanVisitorAccessPlot(player, ownerGuid, settingsFlags, false))
            {
                TC_LOG_DEBUG("housing", "at_housing_plot: Player {} denied plot access (owner {} flags 0x{:X})",
                    player->GetGUID().ToString(), ownerGuid.ToString(), settingsFlags);

                // The trespasser IS physically inside the plot AT: track them so the eviction
                // timer can tell "still here" from "walked away" (OnUnitExit clears the mark
                // via ClearPlayerCurrentPlot, which cancels the pending eviction).
                housingMap->SetPlayerCurrentPlot(player->GetGUID(), static_cast<uint8>(plotIdx));

                // Retail eviction sequence (dump 2026-10-02 12:25): self-cast warning spell
                // 1245416 whose aura ticks 5s, the warning as CHAT_MSG_RAID_BOSS_WHISPER
                // (sender = the visitor, SpellID 1245416), teleport to the neighborhood entry
                // when the aura expires.
                if (sSpellMgr->GetSpellInfo(SPELL_HOUSING_PLOT_EVICT_WARNING, DIFFICULTY_NONE))
                    player->CastSpell(player, SPELL_HOUSING_PLOT_EVICT_WARNING, true);

                {
                    WorldPackets::Chat::Chat warning;
                    warning.Initialize(CHAT_MSG_RAID_BOSS_WHISPER, LANG_UNIVERSAL, player, player,
                        player->GetSession()->GetTrinityString(HOUSING_STRING_PLOT_ACCESS_DENIED));
                    warning.SpellID = SPELL_HOUSING_PLOT_EVICT_WARNING;
                    player->SendDirectMessage(warning.Write());
                }

                // Eviction target: the plot's own TeleportPosition (retail kicks to the
                // plot cornerstone, not the map entry portal); map origin as fallback.
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

                        // Walked off the plot during the warning: eviction canceled (the AT
                        // exit cleared the tracking mark and removed the warning aura).
                        HousingMap* hMap = dynamic_cast<HousingMap*>(visitor->GetMap());
                        if (!hMap || hMap->GetPlayerCurrentPlot(guid) != plotIdx)
                            return;

                        // Cancel conditions: the owner granted access while the warning was
                        // ticking (retail cancels the eviction in that case rather than
                        // teleporting a permitted visitor), or the plot changed hands.
                        Neighborhood* nbh = hMap ? hMap->GetNeighborhood() : nullptr;
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
            Housing const* ownerHousing = player->GetHousingByOwner(ownerGuid);
            if (!ownerHousing)
                if (Player* plotOwner = ObjectAccessor::FindPlayer(ownerGuid))
                    ownerHousing = plotOwner->GetHousingByOwner(ownerGuid);

            if (ownerHousing)
            {
                WorldPackets::Housing::HousingHouseStatusResponse statusResponse;
                statusResponse.HouseGuid = ownerHousing->GetHouseGuid();
                // The OWNER's battle.net account, not the viewer's: retail HouseStatus pairs the
                // house GUID with its owner's BNetAccount (capture 2026-10-02 12:25, HouseOwner-
                // AccountGUID low == HouseGUID low for every visited plot). Sending the viewer's
                // own account here made the client treat a visited house as the viewer's - the
                // front-door gate then skipped its visitor check, no ERR_HOUSING_ACTION_NOENTRY.
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
                    // Live settings when the owner is online (ownerHousing), PlotInfo mirror otherwise
                    // (H-11). The blueprint grant rides along with the visit grant - see
                    // HOUSING_PERMISSIONS_BLUEPRINT.
                    uint32 visitorSettings = plotInfo ? plotInfo->HouseSettingsFlags : HOUSE_SETTING_DEFAULT;
                    if (ownerHousing)
                        visitorSettings = ownerHousing->GetSettingsFlags();
                    permResponse.PermissionFlags = HOUSING_PERMISSIONS_VISITOR
                        | (sHousingMgr.CanVisitorExportBlueprint(player, ownerGuid, visitorSettings)
                            ? HOUSING_PERMISSIONS_BLUEPRINT : 0);
                }
                player->SendDirectMessage(permResponse.Write());

                TC_LOG_DEBUG("housing", "at_housing_plot: Sent HouseStatus+Permissions for player {} (own={}, flags=0x{:X})",
                    player->GetGUID().ToString(), isOwnPlot, permResponse.PermissionFlags);
            }
        }

        // Cosmetic phase shift: owner entering own plot removes 16 cosmetic phases
        // after a ~10 second delay (sniff-verified retail behavior).
        if (isOwnPlot)
        {
            ObjectGuid playerGuid = player->GetGUID();
            player->m_Events.AddEventAtOffset([playerGuid, plotIdx]()
            {
                Player* p = ObjectAccessor::FindPlayer(playerGuid);
                if (!p || !p->IsInWorld())
                    return;

                // Only while still on that plot. Retail never changes phases inside the house (one
                // PHASE_SHIFT_CHANGE per map arrival); a late shift there left the camera behind on the way out.
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

        // Remove plot-auras (manual packets, spells aren't in DB2).
        housingMap->SendPlotLeaveAuraRemoval(player);

        // Walking off the plot during the eviction warning cancels it: drop the warning
        // spell (the delayed teleport re-checks access and no-ops for a permitted visitor).
        player->RemoveAura(SPELL_HOUSING_PLOT_EVICT_WARNING);

        housingMap->ClearPlayerCurrentPlot(player->GetGUID());

        // 12.0.5 plot-leave: clear PlayerHouseInfoComponent.CurrentHouse so the client's
        // NeighborhoodSystem TLS drops its "on plot" flag.
        player->SetCurrentHouse(ObjectGuid::Empty);

        // Clear editor contexts (Decor, Room, Fixture) by sending FlagByte=0x00
        // HouseStatusResponse. Skip when leaving the plot is the result of entering
        // the interior — the map transfer would erase interior editor state otherwise.
        if (isOwnPlot)
        {
            if (Housing* housing = player->GetHousing())
            {
                if (!housing->IsInInterior())
                {
                    // Walking off the plot closes the editor on the client without a CMSG. Drop it here as well,
                    // otherwise the next plot entry reports it active and the client opens it again.
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

        // Restore cosmetic phases when owner leaves.
        if (isOwnPlot)
        {
            ObjectGuid playerGuid = player->GetGUID();
            player->m_Events.AddEventAtOffset([playerGuid, plotIdx]()
            {
                Player* p = ObjectAccessor::FindPlayer(playerGuid);
                if (!p || !p->IsInWorld())
                    return;

                // Only while still in the neighborhood and off that plot - not after walking into the house
                // (retail never changes phases inside it) or back onto the plot.
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
