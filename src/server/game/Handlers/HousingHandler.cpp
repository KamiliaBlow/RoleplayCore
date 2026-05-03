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

#include "WorldSession.h"
#include "Account.h"
#include "MovementPackets.h"
#include "HousingNeighborhoodMirrorEntity.h"
#include "HousingPlayerHouseEntity.h"
#include <cmath>
#include "DatabaseEnv.h"
#include "GameTime.h"
#include "RealmList.h"
#include "AreaTrigger.h"
#include "DB2Stores.h"
#include "Guild.h"
#include "GuildMgr.h"
#include "HouseInteriorMap.h"
#include "Housing.h"
#include "HousingDefines.h"
#include "HouseInteriorMap.h"
#include "HousingMap.h"
#include "HousingMgr.h"
#include "MeshObject.h"
#include "HousingPackets.h"
#include "Log.h"
#include "Neighborhood.h"
#include "NeighborhoodCharter.h"
#include "NeighborhoodMgr.h"
#include "ObjectAccessor.h"
#include "ObjectMgr.h"
#include "Player.h"
#include "CharacterCache.h"
#include "SocialMgr.h"
#include "Spell.h"
#include "SpellAuraDefines.h"
#include "SpellMgr.h"
#include "SpellPackets.h"
#include "UpdateData.h"
#include "World.h"
#include "WorldStatePackets.h"

namespace
{
    std::string HexDumpPacket(WorldPacket const* packet, size_t maxBytes = 128)
    {
        if (!packet || packet->size() == 0)
            return "(empty)";
        size_t len = std::min(packet->size(), maxBytes);
        std::string result;
        result.reserve(len * 3 + 32);
        uint8 const* raw = packet->data();
        for (size_t i = 0; i < len; ++i)
        {
            if (i > 0 && i % 32 == 0)
                result += "\n  ";
            else if (i > 0)
                result += ' ';
            result += fmt::format("{:02X}", raw[i]);
        }
        if (len < packet->size())
            result += fmt::format(" ...({} more)", packet->size() - len);
        return result;
    }

    std::string GuidHex(ObjectGuid const& guid)
    {
        return fmt::format("lo={:016X} hi={:016X}", guid.GetRawValue(0), guid.GetRawValue(1));
    }

    // Sends manual SMSG_AURA_UPDATE + SMSG_SPELL_START + SMSG_SPELL_GO for a housing
    // spell that doesn't exist in our DB2/spell data. The sniff shows these spells use:
    //   AURA_UPDATE: CastID matches SPELL_START/SPELL_GO CastID
    //   SPELL_START: Target.Flags=0 (Self), CastTime=0
    //   SPELL_GO: Target.Flags=2 (Unit), HitTargets={self}, CastTime=getMSTime(), LogData filled
    void SendManualHousingSpellPackets(Player* player, uint32 spellId, uint8 auraSlot,
        uint8 auraActiveFlags, uint32 spellStartCastFlags, uint32 spellGoCastFlags,
        uint32 spellGoCastFlagsEx = 16, uint32 spellGoCastFlagsEx2 = 4)
    {
        // Generate a CastID GUID shared across AURA_UPDATE, SPELL_START, and SPELL_GO
        ObjectGuid castId = ObjectGuid::Create<HighGuid::Cast>(
            SPELL_CAST_SOURCE_NORMAL, player->GetMapId(), spellId,
            player->GetMap()->GenerateLowGuid<HighGuid::Cast>());

        // 1. SMSG_AURA_UPDATE — apply the aura (CastID must match spell packets)
        {
            WorldPackets::Spells::AuraUpdate auraUpdate;
            auraUpdate.UpdateAll = false;
            auraUpdate.UnitGUID = player->GetGUID();

            WorldPackets::Spells::AuraInfo auraInfo;
            auraInfo.Slot = auraSlot;
            auraInfo.AuraData.emplace();
            auraInfo.AuraData->CastID = castId;
            auraInfo.AuraData->SpellID = spellId;
            auraInfo.AuraData->Flags = AFLAG_NOCASTER;
            auraInfo.AuraData->ActiveFlags = auraActiveFlags;
            auraInfo.AuraData->CastLevel = 36;
            auraInfo.AuraData->Applications = 0;
            auraUpdate.Auras.push_back(std::move(auraInfo));

            player->SendDirectMessage(auraUpdate.Write());
        }

        // 2. SMSG_SPELL_START
        {
            WorldPackets::Spells::SpellStart spellStart;
            spellStart.Cast.CasterGUID = player->GetGUID();
            spellStart.Cast.CasterUnit = player->GetGUID();
            spellStart.Cast.CastID = castId;
            spellStart.Cast.SpellID = spellId;
            spellStart.Cast.CastFlags = spellStartCastFlags;
            spellStart.Cast.CastTime = 0;
            // Target.Flags = 0 (Self) — default

            player->SendDirectMessage(spellStart.Write());
        }

        // 3. SMSG_SPELL_GO (CombatLogServerPacket — has LogData)
        {
            WorldPackets::Spells::SpellGo spellGo;
            spellGo.Cast.CasterGUID = player->GetGUID();
            spellGo.Cast.CasterUnit = player->GetGUID();
            spellGo.Cast.CastID = castId;
            spellGo.Cast.SpellID = spellId;
            spellGo.Cast.CastFlags = spellGoCastFlags;
            spellGo.Cast.CastFlagsEx = spellGoCastFlagsEx;
            spellGo.Cast.CastFlagsEx2 = spellGoCastFlagsEx2;
            spellGo.Cast.CastTime = getMSTime();
            spellGo.Cast.Target.Flags = TARGET_FLAG_UNIT;
            spellGo.Cast.HitTargets.push_back(player->GetGUID());
            spellGo.Cast.HitStatus.emplace_back(uint8(0));
            spellGo.LogData.Initialize(player);

            player->SendDirectMessage(spellGo.Write());
        }

        TC_LOG_DEBUG("housing", "  Sent manual AURA_UPDATE + SPELL_START + SPELL_GO for spell {} (slot {}, CastID={})",
            spellId, auraSlot, castId.ToString());
    }

    // Refreshes all room MeshObjects in the player's interior instance after a room
    // data change (add, remove, rotate, move, theme, material, door, ceiling).
    void RefreshInteriorRoomVisuals(Player* player, Housing* housing)
    {
        HouseInteriorMap* interiorMap = dynamic_cast<HouseInteriorMap*>(player->GetMap());
        if (!interiorMap)
            return;

        // Use player's team for faction theme, matching HouseInteriorMap::AddPlayerToMap pattern.
        // NeighborhoodMapData::FactionRestriction is a bitmask (3 = both factions) and doesn't
        // map to the enum values expected by GetFactionDefaultThemeID().
        int32 faction = (player->GetTeamId() == TEAM_ALLIANCE)
            ? NEIGHBORHOOD_FACTION_ALLIANCE : NEIGHBORHOOD_FACTION_HORDE;

        interiorMap->DespawnAllRoomMeshObjects();
        interiorMap->SpawnRoomMeshObjects(housing, faction);
    }


    // Checks whether the player is eligible for housing features.
    // Returns a bitmask of HousingWarningFlag reasons if restrictions apply.
    uint32 ShouldShowHousingWarning(Player const* player)
    {
        uint32 warnings = HOUSING_WARNING_NONE;

        // Check expansion access — housing requires The War Within (expansion 10)
        if (player->GetSession()->GetExpansion() < HOUSING_REQUIRED_EXPANSION)
            warnings |= HOUSING_WARNING_EXPANSION_REQUIRED;

        // Check minimum level
        if (player->GetLevel() < HOUSING_MIN_PLAYER_LEVEL)
            warnings |= HOUSING_WARNING_LEVEL_TOO_LOW;

        return warnings;
    }
}

// ============================================================
// Decline Neighborhood Invites
// ============================================================

void WorldSession::HandleDeclineNeighborhoodInvites(WorldPackets::Housing::DeclineNeighborhoodInvites const& declineNeighborhoodInvites)
{
    if (declineNeighborhoodInvites.Allow)
        GetPlayer()->SetPlayerFlagEx(PLAYER_FLAGS_EX_AUTO_DECLINE_NEIGHBORHOOD);
    else
        GetPlayer()->RemovePlayerFlagEx(PLAYER_FLAGS_EX_AUTO_DECLINE_NEIGHBORHOOD);
}

// ============================================================
// House Exterior System
// ============================================================

void WorldSession::HandleHouseExteriorSetHousePosition(WorldPackets::Housing::HouseExteriorCommitPosition const& houseExteriorCommitPosition)
{
    Player* player = GetPlayer();
    if (!player)
        return;

    Housing* housing = player->GetHousing();
    if (!housing)
    {
        WorldPackets::Housing::HouseExteriorSetHousePositionResponse response;
        response.Result = static_cast<uint8>(HOUSING_RESULT_HOUSE_NOT_FOUND);
        SendPacket(response.Write());
        return;
    }

    if (!houseExteriorCommitPosition.HasPosition)
    {
        // HasPosition=false: the client is cancelling the position change, just acknowledge
        WorldPackets::Housing::HouseExteriorSetHousePositionResponse response;
        response.Result = static_cast<uint8>(HOUSING_RESULT_SUCCESS);
        response.HouseGuid = housing->GetHouseGuid();
        SendPacket(response.Write());
        return;
    }

    float posX = houseExteriorCommitPosition.PositionX;
    float posY = houseExteriorCommitPosition.PositionY;
    float posZ = houseExteriorCommitPosition.PositionZ;

    // Validate coordinate sanity
    if (!std::isfinite(posX) || !std::isfinite(posY) || !std::isfinite(posZ))
    {
        WorldPackets::Housing::HouseExteriorSetHousePositionResponse response;
        response.Result = static_cast<uint8>(HOUSING_RESULT_BOUNDS_FAILURE_PLOT);
        response.HouseGuid = housing->GetHouseGuid();
        SendPacket(response.Write());
        return;
    }

    // Convert quaternion to facing angle for server-side storage
    // The client sends a full quaternion; extract yaw as the facing angle
    float facing = std::atan2(
        2.0f * (houseExteriorCommitPosition.RotationW * houseExteriorCommitPosition.RotationZ +
                houseExteriorCommitPosition.RotationX * houseExteriorCommitPosition.RotationY),
        1.0f - 2.0f * (houseExteriorCommitPosition.RotationY * houseExteriorCommitPosition.RotationY +
                        houseExteriorCommitPosition.RotationZ * houseExteriorCommitPosition.RotationZ));

    // Persist the house position to the database
    housing->SetHousePosition(posX, posY, posZ, facing);

    // Despawn and respawn house structure at new position (must also respawn decor since DespawnHouseForPlot removes all MeshObjects)
    if (HousingMap* housingMap = dynamic_cast<HousingMap*>(player->GetMap()))
    {
        uint8 plotIndex = housing->GetPlotIndex();

        // Despawn old house (door GO + all MeshObjects including decor)
        housingMap->DespawnAllDecorForPlot(plotIndex);
        housingMap->DespawnHouseForPlot(plotIndex);

        // Respawn at new position with current exterior component, house type, and fixture selections
        Position newPos(posX, posY, posZ, facing);
        auto fixtureOverrides = housing->GetFixtureOverrideMap();
        auto rootOverrides = housing->GetRootComponentOverrides();
        housingMap->SpawnHouseForPlot(plotIndex, &newPos,
            static_cast<int32>(housing->GetCoreExteriorComponentID()),
            static_cast<int32>(housing->GetHouseType()),
            fixtureOverrides.empty() ? nullptr : &fixtureOverrides,
            rootOverrides.empty() ? nullptr : &rootOverrides);
        housingMap->SpawnAllDecorForPlot(plotIndex, housing);
    }

    // Send response
    WorldPackets::Housing::HouseExteriorSetHousePositionResponse response;
    response.Result = static_cast<uint8>(HOUSING_RESULT_SUCCESS);
    response.HouseGuid = housing->GetHouseGuid();
    SendPacket(response.Write());

    // Sniff-verified: every exterior mutation is followed by an inline UPDATE_OBJECT
    // containing updated entity field data (player + house entity + fixture MeshObjects)
    SendFixtureUpdateObject(player, housing);

    TC_LOG_INFO("housing", "CMSG_HOUSE_EXTERIOR_SET_HOUSE_POSITION: Player {} repositioned house at ({:.1f}, {:.1f}, {:.1f}, {:.2f}) rot=({:.3f},{:.3f},{:.3f},{:.3f})",
        player->GetGUID().ToString(), posX, posY, posZ, facing,
        houseExteriorCommitPosition.RotationX, houseExteriorCommitPosition.RotationY,
        houseExteriorCommitPosition.RotationZ, houseExteriorCommitPosition.RotationW);
}

void WorldSession::HandleHouseExteriorLock(WorldPackets::Housing::HouseExteriorLock const& houseExteriorLock)
{
    Player* player = GetPlayer();
    if (!player)
        return;

    Housing* housing = player->GetHousing();
    if (!housing)
    {
        WorldPackets::Housing::HouseExteriorLockResponse response;
        response.Result = static_cast<uint8>(HOUSING_RESULT_HOUSE_NOT_FOUND);
        SendPacket(response.Write());
        return;
    }

    // Persist the exterior lock state
    housing->SetExteriorLocked(houseExteriorLock.Locked);

    WorldPackets::Housing::HouseExteriorLockResponse response;
    response.Result = static_cast<uint8>(HOUSING_RESULT_SUCCESS);
    response.FixtureEntityGuid = houseExteriorLock.HouseGuid;
    response.EditorPlayerGuid = player->GetGUID();
    response.Active = houseExteriorLock.Locked;
    SendPacket(response.Write());

    // Sniff-verified: lock operations also send an inline UPDATE_OBJECT
    SendFixtureUpdateObject(player, housing);

    TC_LOG_INFO("housing", "CMSG_HOUSE_EXTERIOR_LOCK HouseGuid: {}, Locked: {} for player {}",
        houseExteriorLock.HouseGuid.ToString(), houseExteriorLock.Locked, player->GetGUID().ToString());
}

// ============================================================
// House Interior System
// ============================================================

void WorldSession::HandleHouseInteriorLeaveHouse(WorldPackets::Housing::HouseInteriorLeaveHouse const& /*houseInteriorLeaveHouse*/)
{
    Player* player = GetPlayer();
    if (!player)
        return;

    // A visitor may not own a house of their own — this handler still needs
    // to work so they can leave. Own-interior housing is used only for the
    // HouseStatus emission (which we tailor to the visited house below);
    // positional data comes from the HouseInteriorMap's stored source fields.
    Housing* housing = player->GetHousing();
    HouseInteriorMap* interiorMap = dynamic_cast<HouseInteriorMap*>(player->GetMap());
    bool isVisit = interiorMap && interiorMap->GetOwnerGuid() != player->GetGUID();

    // Clear editing mode and interior state — only own housing carries that
    // state (visitors can't be in edit mode in someone else's house anyway).
    if (housing)
    {
        housing->SetEditorMode(HOUSING_EDITOR_MODE_NONE);
        housing->SetInInterior(false);
    }

    // 12.0.5: SMSG_HOUSE_INTERIOR_LEAVE_HOUSE_RESPONSE no longer exists.
    // The client reacts to the PlayerHouseInfoComponent.CurrentHouse field being
    // cleared via UPDATE_OBJECT on the player (SetCurrentHouse(Empty) elsewhere).
    if (Player* p = GetPlayer())
        p->SetCurrentHouse(ObjectGuid::Empty);

    // HouseStatus targets the HOUSE the player was in — for visitors, Bob's
    // house, not their own. Resolve the visited house's GUIDs via the
    // interior map's owner lookup. Own-interior uses own housing as before.
    WorldPackets::Housing::HousingHouseStatusResponse statusResponse;
    statusResponse.Status = 0;
    if (isVisit)
    {
        ObjectGuid ownerGuid = interiorMap->GetOwnerGuid();
        for (Neighborhood* nbh : sNeighborhoodMgr.GetNeighborhoodsForPlayer(ownerGuid))
        {
            for (Neighborhood::PlotInfo const& plot : nbh->GetPlots())
            {
                if (plot.OwnerGuid == ownerGuid && plot.IsOccupied())
                {
                    statusResponse.HouseGuid = plot.HouseGuid;
                    statusResponse.AccountGuid = plot.OwnerBnetGuid;
                    statusResponse.OwnerPlayerGuid = plot.OwnerGuid;
                    statusResponse.NeighborhoodGuid = nbh->GetGuid();
                    break;
                }
            }
            if (!statusResponse.HouseGuid.IsEmpty())
                break;
        }
        statusResponse.PermissionFlags = 0xC0; // bit7=Decor, bit6=Room only on interior visit
    }
    else if (housing)
    {
        statusResponse.HouseGuid = housing->GetHouseGuid();
        statusResponse.AccountGuid = GetBattlenetAccountGUID();
        statusResponse.OwnerPlayerGuid = player->GetGUID();
        statusResponse.NeighborhoodGuid = housing->GetNeighborhoodGuid();
        statusResponse.PermissionFlags = 0xC0;
    }
    SendPacket(statusResponse.Write());

    // Teleport player back to the neighborhood map at the plot's visitor landing point.
    // Try to use the HouseInteriorMap's stored source info first (most reliable),
    // then fall back to resolving from the Housing object's neighborhood.
    uint32 worldMapId = 0;
    uint8 plotIndex = housing ? housing->GetPlotIndex() : INVALID_PLOT_INDEX;
    uint32 neighborhoodMapId = 0;

    // Preferred path: get the source neighborhood from the HouseInteriorMap itself
    if (HouseInteriorMap* interiorMap = dynamic_cast<HouseInteriorMap*>(player->GetMap()))
    {
        worldMapId = interiorMap->GetSourceNeighborhoodMapId();
        plotIndex = interiorMap->GetSourcePlotIndex();
    }

    // Fallback: resolve from the Housing object's neighborhood GUID
    if (worldMapId == 0)
    {
        Neighborhood* neighborhood = sNeighborhoodMgr.ResolveNeighborhood(housing->GetNeighborhoodGuid(), player);
        if (neighborhood)
        {
            neighborhoodMapId = neighborhood->GetNeighborhoodMapID();
            worldMapId = sHousingMgr.GetWorldMapIdByNeighborhoodMapId(neighborhoodMapId);
        }
    }

    // Last resort fallback
    if (worldMapId == 0)
    {
        worldMapId = 2735; // Alliance Founder's Point default
        TC_LOG_ERROR("housing", "CMSG_HOUSE_INTERIOR_LEAVE_HOUSE: Could not resolve neighborhood world map, "
            "falling back to {}", worldMapId);
    }

    // Resolve the NeighborhoodMapId for the world map to look up plot data
    if (neighborhoodMapId == 0)
        neighborhoodMapId = sHousingMgr.GetNeighborhoodMapIdByWorldMap(worldMapId);

    // Compute exit position: house center + door hook offset + exit point offset.
    // This places the player in front of the door they entered through.
    float exitX = 0.0f, exitY = 0.0f, exitZ = 0.0f, exitO = 0.0f;
    bool foundExitPoint = false;

    if (neighborhoodMapId != 0)
    {
        std::vector<NeighborhoodPlotData const*> plots = sHousingMgr.GetPlotsForMap(neighborhoodMapId);
        for (NeighborhoodPlotData const* plot : plots)
        {
            if (plot->PlotIndex != static_cast<int32>(plotIndex))
                continue;

            float hx = plot->HousePosition[0];
            float hy = plot->HousePosition[1];
            float hz = plot->HousePosition[2];

            // Compute house facing (same as SpawnHouseForPlot / RespawnDoorGOAtHook)
            float hFacing = plot->HouseRotation[2];
            if (plot->HouseRotation[0] == 0.0f && plot->HouseRotation[1] == 0.0f && plot->HouseRotation[2] == 0.0f)
                hFacing = std::atan2(plot->CornerstonePosition[1] - hy, plot->CornerstonePosition[0] - hx);

            // Find the door hook + exit point from the player's fixture overrides
            auto fixtureOverrides = housing->GetFixtureOverrideMap();
            uint32 baseCompID = static_cast<uint32>(housing->GetCoreExteriorComponentID());
            auto const* baseHooks = sHousingMgr.GetHooksOnComponent(baseCompID);
            if (baseHooks)
            {
                for (ExteriorComponentHookEntry const* hook : *baseHooks)
                {
                    if (!hook || hook->ExteriorComponentTypeID != HOUSING_FIXTURE_TYPE_DOOR)
                        continue;
                    auto ovrItr = fixtureOverrides.find(hook->ID);
                    if (ovrItr == fixtureOverrides.end())
                        continue;

                    // Door hook found — use hook position + exit point offset
                    float localX = hook->Position[0];
                    float localY = hook->Position[1];
                    float localZ = hook->Position[2];

                    ExteriorComponentExitPointEntry const* exitPt = sHousingMgr.GetExitPoint(ovrItr->second);
                    if (exitPt)
                    {
                        localX += exitPt->Position[0];
                        localY += exitPt->Position[1];
                        localZ += exitPt->Position[2];
                    }

                    float cosFacing = std::cos(hFacing);
                    float sinFacing = std::sin(hFacing);
                    exitX = hx + localX * cosFacing - localY * sinFacing;
                    exitY = hy + localX * sinFacing + localY * cosFacing;
                    exitZ = hz + localZ;
                    exitO = hFacing;
                    foundExitPoint = true;
                    break;
                }
            }

            // Fallback: use plot's TeleportPosition if no door exit point found
            if (!foundExitPoint)
            {
                exitX = plot->TeleportPosition[0];
                exitY = plot->TeleportPosition[1];
                exitZ = plot->TeleportPosition[2];
                exitO = plot->TeleportFacing;
                foundExitPoint = true;
            }
            break;
        }
    }

    if (!foundExitPoint)
    {
        // Last resort: use neighborhood center
        NeighborhoodMapData const* mapData = sHousingMgr.GetNeighborhoodMapData(neighborhoodMapId);
        if (mapData)
        {
            exitX = mapData->Origin[0];
            exitY = mapData->Origin[1];
            exitZ = mapData->Origin[2];
        }
        TC_LOG_WARN("housing", "CMSG_HOUSE_INTERIOR_LEAVE_HOUSE: No exit point for plotIndex {}, "
            "using neighborhood center", plotIndex);
    }

    player->TeleportTo(worldMapId, exitX, exitY, exitZ, exitO);

    TC_LOG_INFO("housing", "CMSG_HOUSE_INTERIOR_LEAVE_HOUSE: Player {} teleporting back to map {} at ({:.1f}, {:.1f}, {:.1f})",
        player->GetGUID().ToString(), worldMapId, exitX, exitY, exitZ);
}

// ============================================================
// Decor System
// ============================================================

void WorldSession::HandleHousingDecorSetEditMode(WorldPackets::Housing::HousingDecorSetEditMode const& housingDecorSetEditMode)
{
    TC_LOG_ERROR("housing", ">>> CMSG_HOUSING_DECOR_SET_EDIT_MODE Active={}", housingDecorSetEditMode.Active);

    Player* player = GetPlayer();
    if (!player)
        return;

    Housing* housing = player->GetHousing();
    if (!housing)
    {
        TC_LOG_ERROR("housing", "HandleHousingDecorSetEditMode: GetHousing() returned null for player {}",
            player->GetGUID().ToString());
        WorldPackets::Housing::HousingDecorSetEditModeResponse response;
        response.Result = HOUSING_RESULT_HOUSE_NOT_FOUND;
        SendPacket(response.Write());
        return;
    }

    HousingEditorMode targetMode = housingDecorSetEditMode.Active ? HOUSING_EDITOR_MODE_BASIC_DECOR : HOUSING_EDITOR_MODE_NONE;

    TC_LOG_DEBUG("housing", "  HouseGuid={} PlotGuid={} NeighborhoodGuid={}",
        housing->GetHouseGuid().ToString(), housing->GetPlotGuid().ToString(), housing->GetNeighborhoodGuid().ToString());

    if (!player->m_playerHouseInfoComponentData.has_value())
    {
        TC_LOG_ERROR("housing", "HandleHousingDecorSetEditMode: PlayerHouseInfoComponentData NOT initialized for player {}",
            player->GetGUID().ToString());
        WorldPackets::Housing::HousingDecorSetEditModeResponse response;
        response.HouseGuid = housing->GetHouseGuid();
        response.BNetAccountGuid = GetBattlenetAccountGUID();
        response.Result = HOUSING_RESULT_HOUSE_NOT_FOUND;
        SendPacket(response.Write());
        return;
    }

    {
        UF::PlayerHouseInfoComponentData const& phData = *player->m_playerHouseInfoComponentData;
        TC_LOG_DEBUG("housing", "  BEFORE SetEditorMode: EditorMode={} HouseCount={}",
            uint32(*phData.EditorMode), phData.Houses.size());
        for (uint32 i = 0; i < phData.Houses.size(); ++i)
        {
            TC_LOG_DEBUG("housing", "    Houses[{}]: HouseGUID={} MapID={} PlotID={} Level={} NeighborhoodGUID={}",
                i, phData.Houses[i].HouseGUID.ToString(), phData.Houses[i].MapID,
                phData.Houses[i].PlotID, phData.Houses[i].Level,
                phData.Houses[i].NeighborhoodGUID.ToString());
        }
    }

    // Set edit mode via UpdateField — client needs both the UpdateField change AND the SMSG response
    housing->SetEditorMode(targetMode);

    // Wire format: PackedGUID HouseGuid + PackedGUID BNetAccountGuid
    // + uint32 AllowedEditor.size() + uint8 Result + [PackedGUID AllowedEditors...]
    WorldPackets::Housing::HousingDecorSetEditModeResponse response;
    response.HouseGuid = housing->GetHouseGuid();
    response.BNetAccountGuid = GetBattlenetAccountGUID();
    response.Result = HOUSING_RESULT_SUCCESS;

    if (housingDecorSetEditMode.Active)
    {
        // --- Edit mode ENTER ---
        // Packet order: AURA_UPDATE(1263303) → SPELL_START(1263303) → SPELL_GO(1263303)
        //   → EDIT_MODE_RESPONSE → UPDATE_OBJECT(EditorMode=1 + BNetAccount/FHousingStorage_C)

        // 1. Apply edit mode aura + spell cast packets (spell 1263303)
        if (sSpellMgr->GetSpellInfo(SPELL_HOUSING_EDIT_MODE_AURA, DIFFICULTY_NONE))
        {
            player->CastSpell(player, SPELL_HOUSING_EDIT_MODE_AURA, true);
        }
        else
        {
            // Spell not in DB2 — send manual AURA_UPDATE + SPELL_START + SPELL_GO
            SendManualHousingSpellPackets(player, SPELL_HOUSING_EDIT_MODE_AURA,
                /*auraSlot=*/51, /*auraActiveFlags=*/15,
                /*spellStartCastFlags=*/CAST_FLAG_PENDING | CAST_FLAG_HAS_TRAJECTORY | CAST_FLAG_UNKNOWN_3 | CAST_FLAG_UNKNOWN_4,  // 15
                /*spellGoCastFlags=*/CAST_FLAG_PENDING | CAST_FLAG_UNKNOWN_3 | CAST_FLAG_UNKNOWN_4 | CAST_FLAG_UNKNOWN_9 | CAST_FLAG_UNKNOWN_10);  // 781
        }

        // 2. Build response with AllowedEditor containing the player
        response.AllowedEditor.push_back(player->GetGUID());

        // 3. Send the edit mode response BEFORE the UpdateObject
        WorldPacket const* editModePkt = response.Write();
        TC_LOG_ERROR("housing", "<<< SMSG_HOUSING_DECOR_SET_EDIT_MODE_RESPONSE ({} bytes): {}",
            editModePkt->size(), HexDumpPacket(editModePkt));
        TC_LOG_ERROR("housing", "    HouseGuid={} BNetAccountGuid={} AllowedEditors={} Result={}",
            response.HouseGuid.ToString(), response.BNetAccountGuid.ToString(),
            uint32(response.AllowedEditor.size()), response.Result);
        SendPacket(editModePkt);

        // Sniff-verified: retail sets UNIT_FLAG_PACIFIED, UNIT_FLAG2_NO_ACTIONS,
        // and SilencedSchoolMask=127 during edit mode. These are sent in the same
        // UPDATE_OBJECT that carries EditorMode=1. The client's housing editor
        // specifically expects these flags alongside EditorMode.
        player->SetUnitFlag(UNIT_FLAG_PACIFIED);
        player->SetUnitFlag2(UNIT_FLAG2_NO_ACTIONS);
        player->ReplaceAllSilencedSchoolMask(SPELL_SCHOOL_MASK_ALL);

        // 4. Populate FHousingStorage_C on the Account entity.
        // The client correlates MeshObject FHousingDecor_C.DecorGUID with entries in
        // FHousingStorage_C to build its placed decor list for the targeting system.
        // Without this, the client has no decor to target and selection is impossible.
        // Reset the populated flag so storage entries are re-pushed on every edit mode
        // entry — the client may clear its decor list when exiting editor mode, so we
        // must ensure the Account VALUES_UPDATE always carries the full storage map.
        housing->ResetStoragePopulated();
        housing->PopulateCatalogStorageEntries();

        // 4b. Refresh budget values on the HousingPlayerHouseEntity so the client
        // receives up-to-date max budgets alongside the storage data.
        housing->SyncUpdateFields();

        // 5. Send Player + Account + HousingPlayerHouseEntity in a SINGLE SMSG_UPDATE_OBJECT.
        // Sniff-verified: retail sends EditorMode=1, FHousingStorage_C, and budget data
        // in the same UPDATE_OBJECT. The client reads EditorMode from PlayerHouseInfoComponentData
        // to gate ClickTarget (flag 16) in ClientHousingDecorSystem and reads budgets +
        // storage entries together to compute placed/remaining decor counts.
        // NOTE: BaseEntity::SendUpdateToPlayer is const and does NOT call
        // BuildUpdateChangesMask(), so ContentsChangedMask would be 0 and the
        // VALUES_UPDATE empty. We must compute masks explicitly before building.
        {
            player->BuildUpdateChangesMask();
            GetBattlenetAccount().BuildUpdateChangesMask();
            GetHousingPlayerHouseEntity().BuildUpdateChangesMask();

            UpdateData updateData(player->GetMapId());
            WorldPacket updatePacket;

            // Player VALUES_UPDATE (EditorMode=1 + UNIT_FLAG_PACIFIED + UNIT_FLAG2_NO_ACTIONS)
            player->BuildValuesUpdateBlockForPlayer(&updateData, player);

            // Account entity: ALWAYS send CREATE (not VALUES_UPDATE) when entering edit mode.
            // The initial Account CREATE (during login's SendInitSelf) has NO FHousingStorage_C
            // data. PopulateCatalogStorageEntries() added Decor map entries above, and sending
            // a VALUES_UPDATE for a MapUpdateField that was empty at CREATE time may not
            // properly convey the new entries to the client. CREATE includes all current values.
            // The client handles receiving a second CREATE for an existing entity gracefully.
            GetBattlenetAccount().BuildCreateUpdateBlockForPlayer(&updateData, player);
            player->m_clientGUIDs.insert(GetBattlenetAccount().GetGUID());

            // ALWAYS send CREATE for HousingPlayerHouseEntity when entering edit mode.
            // Same reasoning as Account entity above: the initial CREATE during login may
            // not have had budget values populated yet, and VALUES_UPDATE only includes
            // changed fields — which may be empty if the values haven't changed since last
            // sync. CREATE includes ALL current field values (budgets, level, favor, etc.).
            GetHousingPlayerHouseEntity().BuildCreateUpdateBlockForPlayer(&updateData, player);
            player->m_clientGUIDs.insert(GetHousingPlayerHouseEntity().GetGUID());

            // Include CREATE for ALL decor MeshObjects in this same UPDATE_OBJECT packet.
            // The client correlates MeshObject FHousingDecor_C.DecorGUID with Account
            // FHousingStorage_C entries to build the Placed Decor list. MeshObjects that
            // were CREATEd via normal grid visibility (separate earlier packet) arrived
            // BEFORE FHousingStorage_C was populated, so the client doesn't associate them
            // with decor entries. Re-sending CREATE in this packet (alongside the Account
            // entity) ensures the client has all data in the same context.
            {
                uint32 meshCreateCount = 0;

                // Exterior map: decor from GetDecorGuidMap()
                if (HousingMap* housingMap = dynamic_cast<HousingMap*>(player->GetMap()))
                {
                    for (auto const& [decorGuid, meshObjGuid] : housingMap->GetDecorGuidMap())
                    {
                        MeshObject* meshObj = housingMap->GetMeshObject(meshObjGuid);
                        if (!meshObj || !meshObj->IsInWorld())
                            continue;

                        meshObj->BuildCreateUpdateBlockForPlayer(&updateData, player);
                        player->m_clientGUIDs.insert(meshObjGuid);
                        ++meshCreateCount;
                    }
                }
                // Interior map: decor from interior _decorGuidToObjGuid
                else if (HouseInteriorMap* interiorMap = dynamic_cast<HouseInteriorMap*>(player->GetMap()))
                {
                    for (auto const& [decorGuid, meshObjGuid] : interiorMap->GetDecorGuidMap())
                    {
                        MeshObject* meshObj = interiorMap->GetMeshObject(meshObjGuid);
                        if (!meshObj || !meshObj->IsInWorld())
                            continue;

                        meshObj->BuildCreateUpdateBlockForPlayer(&updateData, player);
                        player->m_clientGUIDs.insert(meshObjGuid);
                        ++meshCreateCount;
                    }
                }

                if (meshCreateCount > 0)
                    TC_LOG_DEBUG("housing", "  EditMode: Force-sent {} decor MeshObject CREATEs to player {}", meshCreateCount, player->GetGUID().ToString());
            }

            updateData.BuildPacket(&updatePacket);
            player->SendDirectMessage(&updatePacket);

            // Clear change masks AND remove from _updateObjects to prevent duplicate
            // VALUES_UPDATE on next map tick (causes "Object update failed" on client).
            player->ClearUpdateMask(false);
            GetBattlenetAccount().ClearUpdateMask(true);
            GetHousingPlayerHouseEntity().ClearUpdateMask(true);
        }

        // Diagnostic: log placed decor GUIDs from Housing vs what's on spawned MeshObjects.
        // This helps identify mismatches between MeshObject FHousingDecor_C.DecorGUID
        // and Account FHousingStorage_C.Decor map keys that prevent click targeting.
        {
            uint32 meshDecorCount = 0;
            uint32 meshInWorld = 0;
            uint32 meshHasFrag = 0;
            uint32 meshAtClient = 0;

            // Collect the decor GUID map from whichever map type the player is on
            std::unordered_map<ObjectGuid, ObjectGuid> const* decorMap = nullptr;
            Map* playerMap = player->GetMap();
            if (HousingMap* housingMap = dynamic_cast<HousingMap*>(playerMap))
                decorMap = &housingMap->GetDecorGuidMap();
            else if (HouseInteriorMap* interiorMap = dynamic_cast<HouseInteriorMap*>(playerMap))
                decorMap = &interiorMap->GetDecorGuidMap();

            if (decorMap)
            {
                for (auto const& [decorGuid, meshObjGuid] : *decorMap)
                {
                    MeshObject* meshObj = playerMap->GetMeshObject(meshObjGuid);
                    bool inWorld = meshObj && meshObj->IsInWorld();
                    bool hasFrag = meshObj && meshObj->HasHousingDecorData();
                    bool atClient = player->m_clientGUIDs.count(meshObjGuid) > 0;
                    if (inWorld) ++meshInWorld;
                    if (hasFrag) ++meshHasFrag;
                    if (atClient) ++meshAtClient;

                    TC_LOG_DEBUG("housing", "  [DIAG] MeshDecor: decorKey={} meshGuid={} inWorld={} hasFrag={} atClient={}",
                        decorGuid.ToString(), meshObjGuid.ToString(), inWorld, hasFrag, atClient);
                    ++meshDecorCount;
                }
            }

            // Also log the placed decor GUIDs from the Housing object (what's in the Account storage)
            uint32 totalPlaced = 0;
            uint32 matchCount = 0;
            for (auto const& [decorGuid, decor] : housing->GetPlacedDecorMap())
            {
                bool hasMeshObject = decorMap && decorMap->count(decorGuid) > 0;
                if (hasMeshObject) ++matchCount;
                ++totalPlaced;
                TC_LOG_DEBUG("housing", "  [DIAG] HousingDecor: decorGuid={} entryId={} room={} hasMesh={}",
                    decorGuid.ToString(), decor.DecorEntryId, decor.RoomGuid.ToString(), hasMeshObject);
            }

            TC_LOG_DEBUG("housing", "  [DIAG] Summary: meshTracked={} meshInWorld={} meshHasFrag={} meshAtClient={} "
                "housingPlaced={} matched={} storagePop={}",
                meshDecorCount, meshInWorld, meshHasFrag, meshAtClient,
                totalPlaced, matchCount, housing->IsStoragePopulated());
        }

        // Play the plot boundary spell visual on the player's plot AT.
        // This activates the glowing border decal around the plot when in edit mode.
        if (HousingMap* housingMap = dynamic_cast<HousingMap*>(player->GetMap()))
        {
            if (AreaTrigger* plotAt = housingMap->GetPlotAreaTrigger(housing->GetPlotIndex()))
                plotAt->PlaySpellVisual(510142);
        }

        TC_LOG_DEBUG("housing", "  EditMode ENTER: PlayerGUID={} BNetAccountGuid={}",
            player->GetGUID().ToString(), response.BNetAccountGuid.ToString());
    }
    else
    {
        // --- Edit mode EXIT ---
        // Packet order: AURA_UPDATE → EDIT_MODE_RESPONSE → UPDATE_OBJECT

        // 1. Remove edit mode aura
        if (sSpellMgr->GetSpellInfo(SPELL_HOUSING_EDIT_MODE_AURA, DIFFICULTY_NONE))
        {
            player->RemoveAurasDueToSpell(SPELL_HOUSING_EDIT_MODE_AURA);
        }
        else
        {
            // Spell not in DB2 — send aura removal manually (empty AuraData = HasAura=False)
            WorldPackets::Spells::AuraUpdate auraUpdate;
            auraUpdate.UpdateAll = false;
            auraUpdate.UnitGUID = player->GetGUID();

            WorldPackets::Spells::AuraInfo auraInfo;
            auraInfo.Slot = 51;
            auraUpdate.Auras.push_back(std::move(auraInfo));

            player->SendDirectMessage(auraUpdate.Write());
        }

        // 2. Clear unit flags set during edit mode enter
        player->RemoveUnitFlag(UNIT_FLAG_PACIFIED);
        player->RemoveUnitFlag2(UNIT_FLAG2_NO_ACTIONS);
        player->ReplaceAllSilencedSchoolMask(SpellSchoolMask(0));

        // 3. Send the edit mode response (empty AllowedEditor = exit)
        WorldPacket const* exitModePkt = response.Write();
        TC_LOG_ERROR("housing", "<<< SMSG_HOUSING_DECOR_SET_EDIT_MODE_RESPONSE EXIT ({} bytes): {}",
            exitModePkt->size(), HexDumpPacket(exitModePkt));
        TC_LOG_ERROR("housing", "    HouseGuid={} BNetAccountGuid={} AllowedEditors=0 Result={}",
            response.HouseGuid.ToString(), response.BNetAccountGuid.ToString(), response.Result);
        SendPacket(exitModePkt);

        // 4. Send Player UPDATE_OBJECT with EditorMode=0 + cleared unit flags immediately.
        // Must call BuildUpdateChangesMask() since BaseEntity::SendUpdateToPlayer is const.
        {
            player->BuildUpdateChangesMask();

            UpdateData updateData(player->GetMapId());
            WorldPacket updatePacket;
            player->BuildValuesUpdateBlockForPlayer(&updateData, player);
            updateData.BuildPacket(&updatePacket);
            player->SendDirectMessage(&updatePacket);

            player->ClearUpdateMask(false);
        }

        // Clear Account entity dirty state on EXIT. During edit mode, decor operations
        // (place/move/remove) modify FHousingStorage_C which marks the Account dirty.
        // Without this, Map::SendObjectUpdates() sends a stale VALUES_UPDATE on the
        // next tick, which the client rejects ("Object update failed for BNetAccount").
        GetBattlenetAccount().ClearUpdateMask(true);
        GetHousingPlayerHouseEntity().ClearUpdateMask(true);

        TC_LOG_DEBUG("housing", "  EditMode EXIT: BNetAccountGuid={}",
            response.BNetAccountGuid.ToString());
    }

    if (player->m_playerHouseInfoComponentData.has_value())
    {
        UF::PlayerHouseInfoComponentData const& phData = *player->m_playerHouseInfoComponentData;
        TC_LOG_ERROR("housing", "  AFTER SetEditMode: EditorMode={} (target={}), HouseCount={}",
            uint32(*phData.EditorMode), uint32(targetMode), phData.Houses.size());
    }
}

void WorldSession::HandleHousingDecorPlace(WorldPackets::Housing::HousingDecorPlace const& housingDecorPlace)
{
    Player* player = GetPlayer();
    if (!player)
        return;

    Housing* housing = player->GetHousing();
    if (!housing)
    {
        WorldPackets::Housing::HousingDecorPlaceResponse response;
        response.Result = static_cast<uint8>(HOUSING_RESULT_HOUSE_NOT_FOUND);
        SendPacket(response.Write());
        return;
    }

    // Look up the entry ID from pending placements (set during RedeemDeferredDecor/StartPlacingNewDecor).
    // If the client already has the item in storage (e.g. pre-populated on login), it sends PLACE
    // directly without REDEEM_DEFERRED first. In that case, extract decorEntryId from the Housing GUID.
    // subType=1 Housing GUIDs encode arg2=decorEntryId in bits [31:0] of the high word.
    uint32 decorEntryId = housing->GetPendingPlacementEntryId(housingDecorPlace.DecorGuid);
    if (!decorEntryId)
    {
        decorEntryId = static_cast<uint32>(housingDecorPlace.DecorGuid.GetRawValue(1) & 0xFFFFFFFF);
        if (!decorEntryId)
        {
            TC_LOG_ERROR("housing", "CMSG_HOUSING_DECOR_PLACE: No pending placement and could not extract EntryId from DecorGuid {}", housingDecorPlace.DecorGuid.ToString());
            WorldPackets::Housing::HousingDecorPlaceResponse response;
            response.PlayerGuid = player->GetGUID();
            response.DecorGuid = housingDecorPlace.DecorGuid;
            response.Result = static_cast<uint8>(HOUSING_RESULT_DECOR_NOT_FOUND);
            SendPacket(response.Write());
            return;
        }

        TC_LOG_DEBUG("housing", "CMSG_HOUSING_DECOR_PLACE: No pending placement for DecorGuid {}, extracted EntryId {} from GUID", housingDecorPlace.DecorGuid.ToString(), decorEntryId);
    }

    // Client sends Euler angles (via TaggedPosition<XYZ> Rotation) — convert to quaternion
    float yaw = housingDecorPlace.Rotation.Pos.GetPositionX();
    float pitch = housingDecorPlace.Rotation.Pos.GetPositionY();
    float roll = housingDecorPlace.Rotation.Pos.GetPositionZ();
    float halfYaw = yaw * 0.5f, halfPitch = pitch * 0.5f, halfRoll = roll * 0.5f;
    float cy = std::cos(halfYaw), sy = std::sin(halfYaw);
    float cp = std::cos(halfPitch), sp = std::sin(halfPitch);
    float cr = std::cos(halfRoll), sr = std::sin(halfRoll);
    float rotW = cy * cp * cr + sy * sp * sr;
    float rotX = cy * cp * sr - sy * sp * cr;
    float rotY = sy * cp * sr + cy * sp * cr;
    float rotZ = sy * cp * cr - cy * sp * sr;

    float posX = housingDecorPlace.Position.Pos.GetPositionX();
    float posY = housingDecorPlace.Position.Pos.GetPositionY();
    float posZ = housingDecorPlace.Position.Pos.GetPositionZ();

    // On the interior map, if the client sends empty RoomGuid, assign to the first
    // visual room so the decor is tracked as interior and SpawnSingleInteriorDecor works.
    ObjectGuid roomGuid = housingDecorPlace.RoomGuid;
    if (roomGuid.IsEmpty() && dynamic_cast<HouseInteriorMap*>(player->GetMap()))
    {
        for (Housing::Room const* room : housing->GetRooms())
        {
            HouseRoomData const* rd = sHousingMgr.GetHouseRoomData(room->RoomEntryId);
            if (rd && !rd->IsBaseRoom())
            {
                roomGuid = room->Guid;
                break;
            }
        }
    }


    HousingResult result = housing->PlaceDecorWithGuid(housingDecorPlace.DecorGuid, decorEntryId,
        posX, posY, posZ, rotX, rotY, rotZ, rotW, roomGuid);

    // CRITICAL: Send PLACE_RESPONSE BEFORE spawning the MeshObject.
    // The client's placement state machine needs the response to finalize the current
    // placement before receiving the MeshObject CREATE. Wrong order causes the preview
    // to snap to camera on subsequent placements ("flies to camera" bug).
    WorldPackets::Housing::HousingDecorPlaceResponse response;
    response.PlayerGuid = player->GetGUID();
    response.Field_09 = 0;
    response.DecorGuid = housingDecorPlace.DecorGuid;
    response.Result = static_cast<uint8>(result);
    SendPacket(response.Write());

    TC_LOG_DEBUG("housing", "CMSG_HOUSING_DECOR_PLACE DecorGuid={} EntryId={} Result={}",
        housingDecorPlace.DecorGuid.ToString(), decorEntryId, uint32(result));

    // THEN spawn the MeshObject + update Account entity
    if (result == HOUSING_RESULT_SUCCESS)
    {
        if (Housing::PlacedDecor const* newDecor = housing->GetPlacedDecor(housingDecorPlace.DecorGuid))
        {
            if (HousingMap* housingMap = dynamic_cast<HousingMap*>(player->GetMap()))
                housingMap->SpawnDecorItem(housing->GetPlotIndex(), *newDecor, housing->GetHouseGuid());
            else if (HouseInteriorMap* interiorMap = dynamic_cast<HouseInteriorMap*>(player->GetMap()))
                interiorMap->SpawnSingleInteriorDecor(*newDecor, housing->GetHouseGuid());
        }

        GetBattlenetAccount().SendUpdateToPlayer(player);
    }
}

void WorldSession::HandleHousingDecorMove(WorldPackets::Housing::HousingDecorMove const& housingDecorMove)
{
    Player* player = GetPlayer();
    if (!player)
        return;

    Housing* housing = player->GetHousing();
    if (!housing)
    {
        WorldPackets::Housing::HousingDecorMoveResponse response;
        response.PlayerGuid = player->GetGUID();
        response.Result = static_cast<uint8>(HOUSING_RESULT_HOUSE_NOT_FOUND);
        SendPacket(response.Write());
        return;
    }

    // Client sends Euler angles (via TaggedPosition<XYZ> Rotation) — convert to quaternion
    float yaw = housingDecorMove.Rotation.Pos.GetPositionX();
    float pitch = housingDecorMove.Rotation.Pos.GetPositionY();
    float roll = housingDecorMove.Rotation.Pos.GetPositionZ();
    float halfYaw = yaw * 0.5f, halfPitch = pitch * 0.5f, halfRoll = roll * 0.5f;
    float cy = std::cos(halfYaw), sy = std::sin(halfYaw);
    float cp = std::cos(halfPitch), sp = std::sin(halfPitch);
    float cr = std::cos(halfRoll), sr = std::sin(halfRoll);
    float rotW = cy * cp * cr + sy * sp * sr;
    float rotX = cy * cp * sr - sy * sp * cr;
    float rotY = sy * cp * sr + cy * sp * cr;
    float rotZ = sy * cp * cr - cy * sp * sr;

    float posX = housingDecorMove.Position.Pos.GetPositionX();
    float posY = housingDecorMove.Position.Pos.GetPositionY();
    float posZ = housingDecorMove.Position.Pos.GetPositionZ();

    float scale = housingDecorMove.Scale;

    HousingResult result = housing->MoveDecor(housingDecorMove.DecorGuid,
        posX, posY, posZ, rotX, rotY, rotZ, rotW, scale);

    // Update decor MeshObject position + scale on the map
    if (result == HOUSING_RESULT_SUCCESS)
    {
        Position newPos(posX, posY, posZ);
        QuaternionData newRot(rotX, rotY, rotZ, rotW);
        if (HousingMap* housingMap = dynamic_cast<HousingMap*>(player->GetMap()))
            housingMap->UpdateDecorPosition(housing->GetPlotIndex(), housingDecorMove.DecorGuid, newPos, newRot, scale);
        else if (HouseInteriorMap* interiorMap = dynamic_cast<HouseInteriorMap*>(player->GetMap()))
            interiorMap->UpdateDecorPosition(housingDecorMove.DecorGuid, newPos, newRot, scale);
    }

    WorldPackets::Housing::HousingDecorMoveResponse response;
    response.PlayerGuid = player->GetGUID();
    response.DecorGuid = housingDecorMove.DecorGuid;
    response.Result = static_cast<uint8>(result);
    WorldPacket const* movePkt = response.Write();
    TC_LOG_ERROR("housing", ">>> CMSG_HOUSING_DECOR_MOVE DecorGuid={} Pos=({:.3f},{:.3f},{:.3f}) Rot=({:.3f},{:.3f},{:.3f}) Scale={:.2f} RoomGuid={} AttachParent={}",
        housingDecorMove.DecorGuid.ToString(), posX, posY, posZ, yaw, pitch, roll,
        housingDecorMove.Scale, housingDecorMove.RoomGuid.ToString(), housingDecorMove.AttachParentGuid.ToString());
    TC_LOG_ERROR("housing", "<<< SMSG_HOUSING_DECOR_MOVE_RESPONSE ({} bytes): {}",
        movePkt->size(), HexDumpPacket(movePkt));
    TC_LOG_ERROR("housing", "    PlayerGuid={} DecorGuid={} Result={}", response.PlayerGuid.ToString(), response.DecorGuid.ToString(), response.Result);
    SendPacket(movePkt);
}

void WorldSession::HandleHousingDecorRemove(WorldPackets::Housing::HousingDecorRemove const& housingDecorRemove)
{
    Player* player = GetPlayer();
    if (!player)
        return;

    Housing* housing = player->GetHousing();
    if (!housing)
    {
        WorldPackets::Housing::HousingDecorRemoveResponse response;
        response.DecorGuid = housingDecorRemove.DecorGuid;
        response.Result = static_cast<uint8>(HOUSING_RESULT_HOUSE_NOT_FOUND);
        SendPacket(response.Write());
        return;
    }

    // Capture plotIndex and source info before RemoveDecor (which erases the placed entry)
    uint8 plotIndex = housing->GetPlotIndex();
    ObjectGuid decorGuid = housingDecorRemove.DecorGuid;
    uint8 removedSourceType = DECOR_SOURCE_STANDARD;
    std::string removedSourceValue;
    if (auto const* placedDecor = housing->GetPlacedDecor(decorGuid))
    {
        removedSourceType = placedDecor->SourceType;
        removedSourceValue = placedDecor->SourceValue;
    }

    HousingResult result = housing->RemoveDecor(decorGuid);

    // Despawn the decor GO from the map and update Account entity
    if (result == HOUSING_RESULT_SUCCESS)
    {
        // Support both exterior (HousingMap) and interior (HouseInteriorMap)
        if (HousingMap* housingMap = dynamic_cast<HousingMap*>(player->GetMap()))
            housingMap->DespawnDecorItem(plotIndex, decorGuid);
        else if (HouseInteriorMap* interiorMap = dynamic_cast<HouseInteriorMap*>(player->GetMap()))
            interiorMap->DespawnDecorItem(decorGuid);

        // Sniff: RemoveDecor deletes the Account entry, but retail keeps it with HouseGUID=Empty
        // Re-add the entry with HouseGUID=Empty to return it to storage, preserving source info
        Battlenet::Account& account = GetBattlenetAccount();
        account.SetHousingDecorStorageEntry(decorGuid, ObjectGuid::Empty, removedSourceType, removedSourceValue);
        account.SendUpdateToPlayer(player);
    }

    // Wire format: PackedGUID DecorGUID + PackedGUID UnkGUID + uint32 Field_13 + uint8 Result
    WorldPackets::Housing::HousingDecorRemoveResponse response;
    response.DecorGuid = decorGuid;
    // UnkGUID and Field_13 stay at defaults (empty/0)
    response.Result = static_cast<uint8>(result);
    WorldPacket const* removePkt = response.Write();
    TC_LOG_ERROR("housing", ">>> CMSG_HOUSING_DECOR_REMOVE DecorGuid={}", decorGuid.ToString());
    TC_LOG_ERROR("housing", "<<< SMSG_HOUSING_DECOR_REMOVE_RESPONSE ({} bytes): {}",
        removePkt->size(), HexDumpPacket(removePkt));
    TC_LOG_ERROR("housing", "    DecorGuid={} Result={}", decorGuid.ToString(), uint32(result));
    SendPacket(removePkt);
}

void WorldSession::HandleHousingDecorLock(WorldPackets::Housing::HousingDecorLock const& housingDecorLock)
{
    Player* player = GetPlayer();
    if (!player)
        return;

    Housing* housing = player->GetHousing();
    if (!housing)
    {
        WorldPackets::Housing::HousingDecorLockResponse response;
        response.DecorGuid = housingDecorLock.DecorGuid;
        response.PlayerGuid = player->GetGUID();
        response.Result = static_cast<uint8>(HOUSING_RESULT_HOUSE_NOT_FOUND);
        SendPacket(response.Write());
        return;
    }

    // Use client's requested lock state (not toggle)
    Housing::PlacedDecor const* decor = housing->GetPlacedDecor(housingDecorLock.DecorGuid);
    if (!decor)
    {
        WorldPackets::Housing::HousingDecorLockResponse response;
        response.DecorGuid = housingDecorLock.DecorGuid;
        response.PlayerGuid = player->GetGUID();
        response.Result = static_cast<uint8>(HOUSING_RESULT_DECOR_NOT_FOUND);
        SendPacket(response.Write());
        return;
    }

    HousingResult result = housing->SetDecorLocked(housingDecorLock.DecorGuid, housingDecorLock.Locked);

    TC_LOG_ERROR("housing", ">>> CMSG_HOUSING_DECOR_LOCK DecorGuid={} Locked={} (entry: {})",
        housingDecorLock.DecorGuid.ToString(), housingDecorLock.Locked, decor->DecorEntryId);

    // Wire format: DecorGUID + PlayerGUID + uint32 Field_16 + uint8 Result + Bits(Locked, Field_17)
    WorldPackets::Housing::HousingDecorLockResponse response;
    response.DecorGuid = housingDecorLock.DecorGuid;
    response.PlayerGuid = player->GetGUID();
    response.Result = static_cast<uint8>(result);
    response.Locked = (result == HOUSING_RESULT_SUCCESS) && housingDecorLock.Locked;
    response.Field_17 = true;
    WorldPacket const* lockPkt = response.Write();
    TC_LOG_ERROR("housing", "<<< SMSG_HOUSING_DECOR_LOCK_RESPONSE ({} bytes): {}",
        lockPkt->size(), HexDumpPacket(lockPkt));
    TC_LOG_ERROR("housing", "    DecorGuid={} PlayerGuid={} Field_16={} Result={} Locked={} Field_17={}",
        response.DecorGuid.ToString(), response.PlayerGuid.ToString(),
        response.Field_16, response.Result, response.Locked, response.Field_17);
    SendPacket(lockPkt);
}

void WorldSession::HandleHousingDecorSetDyeSlots(WorldPackets::Housing::HousingDecorSetDyeSlots const& housingDecorSetDyeSlots)
{
    Player* player = GetPlayer();
    if (!player)
        return;

    Housing* housing = player->GetHousing();
    if (!housing)
    {
        WorldPackets::Housing::HousingDecorSystemSetDyeSlotsResponse response;
        response.DecorGuid = housingDecorSetDyeSlots.DecorGuid;
        response.Result = static_cast<uint8>(HOUSING_RESULT_HOUSE_NOT_FOUND);
        SendPacket(response.Write());
        return;
    }

    std::array<uint32, MAX_HOUSING_DYE_SLOTS> dyeSlots = {};
    for (size_t i = 0; i < housingDecorSetDyeSlots.DyeColorID.size() && i < MAX_HOUSING_DYE_SLOTS; ++i)
        dyeSlots[i] = static_cast<uint32>(housingDecorSetDyeSlots.DyeColorID[i]);

    HousingResult result = housing->CommitDecorDyes(housingDecorSetDyeSlots.DecorGuid, dyeSlots);

    WorldPackets::Housing::HousingDecorSystemSetDyeSlotsResponse response;
    response.DecorGuid = housingDecorSetDyeSlots.DecorGuid;
    response.Result = static_cast<uint8>(result);
    SendPacket(response.Write());

    TC_LOG_INFO("housing", "CMSG_HOUSING_DECOR_COMMIT_DYES DecorGuid: {}, Result: {}",
        housingDecorSetDyeSlots.DecorGuid.ToString(), uint32(result));
}

void WorldSession::HandleHousingDecorDeleteFromStorage(WorldPackets::Housing::HousingDecorDeleteFromStorage const& housingDecorDeleteFromStorage)
{
    Player* player = GetPlayer();
    if (!player)
        return;

    Housing* housing = player->GetHousing();
    if (!housing)
    {
        WorldPackets::Housing::HousingDecorDeleteFromStorageResponse response;
        response.Result = static_cast<uint8>(HOUSING_RESULT_HOUSE_NOT_FOUND);
        SendPacket(response.Write());
        return;
    }

    HousingResult result = HOUSING_RESULT_SUCCESS;
    for (ObjectGuid const& decorGuid : housingDecorDeleteFromStorage.DecorGuids)
    {
        HousingResult r = housing->RemoveDecor(decorGuid);
        if (r != HOUSING_RESULT_SUCCESS)
            result = r;
    }

    WorldPackets::Housing::HousingDecorDeleteFromStorageResponse response;
    response.Result = static_cast<uint8>(result);
    SendPacket(response.Write());

    TC_LOG_INFO("housing", "CMSG_HOUSING_DECOR_DELETE_FROM_STORAGE Count: {}, Result: {}",
        uint32(housingDecorDeleteFromStorage.DecorGuids.size()), uint32(result));
}

void WorldSession::HandleHousingDecorDeleteFromStorageById(WorldPackets::Housing::HousingDecorDeleteFromStorageById const& housingDecorDeleteFromStorageById)
{
    Player* player = GetPlayer();
    if (!player)
        return;

    Housing* housing = player->GetHousing();
    if (!housing)
    {
        WorldPackets::Housing::HousingDecorDeleteFromStorageResponse response;
        response.Result = static_cast<uint8>(HOUSING_RESULT_HOUSE_NOT_FOUND);
        SendPacket(response.Write());
        return;
    }

    HousingResult result = housing->DestroyAllCopies(housingDecorDeleteFromStorageById.DecorRecID);

    WorldPackets::Housing::HousingDecorDeleteFromStorageResponse response;
    response.Result = static_cast<uint8>(result);
    SendPacket(response.Write());

    TC_LOG_INFO("housing", "CMSG_HOUSING_DECOR_DELETE_FROM_STORAGE_BY_ID DecorRecID: {}, Result: {}",
        housingDecorDeleteFromStorageById.DecorRecID, uint32(result));
}

void WorldSession::HandleHousingDecorRequestStorage(WorldPackets::Housing::HousingDecorRequestStorage const& housingDecorRequestStorage)
{
    Player* player = GetPlayer();
    if (!player)
        return;

    TC_LOG_DEBUG("housing", "CMSG_HOUSING_DECOR_REQUEST_STORAGE: Player {} HouseGuid {}",
        player->GetGUID().ToString(), housingDecorRequestStorage.HouseGuid.ToString());

    Housing* housing = player->GetHousing();
    if (!housing)
    {
        WorldPackets::Housing::HousingDecorRequestStorageResponse response;
        response.ResultCode = static_cast<uint8>(HOUSING_RESULT_HOUSE_NOT_FOUND);
        SendPacket(response.Write());
        TC_LOG_ERROR("housing", "CMSG_HOUSING_DECOR_REQUEST_STORAGE: Player {} has no house",
            player->GetGUID().ToString());
        return;
    }

    // Retail-verified flow (sniff instances 1 & 2):
    //   1. SMSG_HOUSING_DECOR_REQUEST_STORAGE_RESPONSE (4 bytes: 00 00 00 80)
    //   2. SMSG_UPDATE_OBJECT with BNetAccount entity (FHousingStorage_C fragment)
    //   3. SMSG_HOUSING_SVCS_GET_PLAYER_HOUSES_INFO_RESPONSE
    // The storage response is an acknowledgement (always Flags=0x80, BNetAccountGuid=Empty).
    // Actual decor data is delivered via the Account entity's FHousingStorage_C fragment.

    // 1. Send storage acknowledgement
    WorldPackets::Housing::HousingDecorRequestStorageResponse response;
    response.ResultCode = static_cast<uint8>(HOUSING_RESULT_SUCCESS);
    SendPacket(response.Write());

    // 2. Populate catalog (unplaced) entries into Account entity, refresh budgets,
    //    then send Account + HousingPlayerHouseEntity + decor MeshObjects in a SINGLE
    //    UPDATE_OBJECT. The Account must be sent as CREATE (not VALUES_UPDATE) because
    //    the initial login CREATE had an empty FHousingStorage_C.Decor MapUpdateField —
    //    a VALUES_UPDATE for an initially-empty map field does not deliver new keys.
    //    Decor MeshObjects are bundled so the client can correlate FHousingDecor_C.DecorGUID
    //    with FHousingStorage_C entries in one pass.
    housing->PopulateCatalogStorageEntries();
    housing->SyncUpdateFields();
    {
        UpdateData updateData(player->GetMapId());
        WorldPacket updatePacket;

        // Account as CREATE (full FHousingStorage_C with Decor map)
        GetBattlenetAccount().BuildCreateUpdateBlockForPlayer(&updateData, player);
        player->m_clientGUIDs.insert(GetBattlenetAccount().GetGUID());

        // HousingPlayerHouseEntity (budgets)
        if (player->HaveAtClient(&GetHousingPlayerHouseEntity()))
            GetHousingPlayerHouseEntity().BuildValuesUpdateBlockForPlayer(&updateData, player);
        else
        {
            GetHousingPlayerHouseEntity().BuildCreateUpdateBlockForPlayer(&updateData, player);
            player->m_clientGUIDs.insert(GetHousingPlayerHouseEntity().GetGUID());
        }

        // Bundle ALL decor MeshObject CREATEs
        uint32 meshCreateCount = 0;
        Map* playerMap = player->GetMap();

        std::unordered_map<ObjectGuid, ObjectGuid> const* decorMap = nullptr;
        if (HousingMap* housingMap = dynamic_cast<HousingMap*>(playerMap))
            decorMap = &housingMap->GetDecorGuidMap();
        else if (HouseInteriorMap* interiorMap = dynamic_cast<HouseInteriorMap*>(playerMap))
            decorMap = &interiorMap->GetDecorGuidMap();

        if (decorMap)
        {
            for (auto const& [decorGuid, meshObjGuid] : *decorMap)
            {
                MeshObject* meshObj = playerMap->GetMeshObject(meshObjGuid);
                if (!meshObj || !meshObj->IsInWorld())
                    continue;

                meshObj->BuildCreateUpdateBlockForPlayer(&updateData, player);
                player->m_clientGUIDs.insert(meshObjGuid);
                ++meshCreateCount;
            }
        }

        updateData.BuildPacket(&updatePacket);
        player->SendDirectMessage(&updatePacket);

        GetBattlenetAccount().ClearUpdateMask(true);
        GetHousingPlayerHouseEntity().ClearUpdateMask(true);

        TC_LOG_DEBUG("housing", "CMSG_HOUSING_DECOR_REQUEST_STORAGE: Sent Account CREATE + {} decor MeshObject CREATEs", meshCreateCount);
    }

    // 3. Send GET_PLAYER_HOUSES_INFO_RESPONSE
    WorldPackets::Housing::HousingSvcsGetPlayerHousesInfoResponse housesInfoResponse;
    for (Housing const* playerHousing : player->GetAllHousings())
    {
        WorldPackets::Housing::JamCliHouse house;
        house.OwnerGUID = player->GetGUID();
        house.HouseGUID = playerHousing->GetHouseGuid();
        house.NeighborhoodGUID = playerHousing->GetNeighborhoodGuid();
        house.HouseLevel = static_cast<uint8>(playerHousing->GetLevel());
        house.PlotIndex = playerHousing->GetPlotIndex();
        housesInfoResponse.Houses.push_back(std::move(house));
    }
    SendPacket(housesInfoResponse.Write());

    TC_LOG_DEBUG("housing", "CMSG_HOUSING_DECOR_REQUEST_STORAGE: Sent STORAGE_RSP + Account update + HOUSES_INFO"
        " | HouseGuid={} CatalogEntries={} HouseCount={}",
        housingDecorRequestStorage.HouseGuid.ToString(),
        uint32(housing->GetCatalogEntries().size()), uint32(housesInfoResponse.Houses.size()));
}

void WorldSession::HandleHousingDecorRedeemDeferredDecor(WorldPackets::Housing::HousingDecorRedeemDeferredDecor const& housingDecorRedeemDeferredDecor)
{
    Player* player = GetPlayer();
    if (!player)
        return;

    uint32 decorEntryId = housingDecorRedeemDeferredDecor.DeferredDecorID;
    uint32 sequenceIndex = housingDecorRedeemDeferredDecor.RedemptionToken;

    TC_LOG_ERROR("housing", ">>> CMSG_HOUSING_DECOR_REDEEM_DEFERRED DeferredDecorID={} RedemptionToken={}",
        decorEntryId, sequenceIndex);

    Housing* housing = player->GetHousing();
    if (!housing)
    {
        WorldPackets::Housing::HousingRedeemDeferredDecorResponse response;
        response.Result = static_cast<uint8>(HOUSING_RESULT_HOUSE_NOT_FOUND);
        response.SequenceIndex = sequenceIndex;
        SendPacket(response.Write());
        return;
    }

    // Verify the deferred decor entry exists in DB2
    HouseDecorData const* decorData = sHousingMgr.GetHouseDecorData(decorEntryId);
    if (!decorData)
    {
        WorldPackets::Housing::HousingRedeemDeferredDecorResponse response;
        response.Result = static_cast<uint8>(HOUSING_RESULT_DECOR_NOT_FOUND);
        response.SequenceIndex = sequenceIndex;
        SendPacket(response.Write());
        return;
    }

    // Add the deferred decor to the player's catalog/storage (SourceType=3 = deferred)
    HousingResult result = housing->AddToCatalog(decorEntryId, DECOR_SOURCE_DEFERRED);
    if (result != HOUSING_RESULT_SUCCESS)
    {
        WorldPackets::Housing::HousingRedeemDeferredDecorResponse response;
        response.Result = static_cast<uint8>(result);
        response.SequenceIndex = sequenceIndex;
        SendPacket(response.Write());
        return;
    }

    // Generate a unique Housing GUID for the newly redeemed decor item.
    // Sniff-verified format: subType=1, arg1=realmId, arg2=decorEntryId, counter=unique
    // subType=0 hits the default case in ObjectGuidFactory::CreateHousing → returns Empty!
    uint64 catalogGuidBase = player->GetGUID().GetCounter() * 100000;
    uint32 instanceIndex = 0;
    for (auto const* entry : housing->GetCatalogEntries())
    {
        if (entry->DecorEntryId == decorEntryId)
        {
            instanceIndex = entry->Count - 1; // Count was just incremented by AddToCatalog
            break;
        }
    }
    uint64 uniqueId = catalogGuidBase + decorEntryId * 100 + instanceIndex;
    ObjectGuid decorGuid = ObjectGuid::Create<HighGuid::Housing>(
        /*subType*/ 1,
        /*arg1*/ sRealmList->GetCurrentRealmId().Realm,
        /*arg2*/ decorEntryId,
        uniqueId);

    // Push the new decor entry to the Account entity's FHousingStorage_C fragment.
    // Sniff: SourceType=3 marks it as redeemed from deferred queue. HouseGUID=empty (not yet placed).
    Battlenet::Account& account = GetBattlenetAccount();
    account.SetHousingDecorStorageEntry(decorGuid, ObjectGuid::Empty, 3);

    // Sniff-verified packet order:
    // 1. SMSG_HOUSING_REDEEM_DEFERRED_DECOR_RESPONSE (DecorGuid + Status=0 + SequenceIndex)
    // 2. SMSG_UPDATE_OBJECT (BNetAccount entity with new Decor entry, ChangeType=1, SourceType=3)
    WorldPackets::Housing::HousingRedeemDeferredDecorResponse response;
    response.DecorGuid = decorGuid;
    response.Result = 0;
    response.SequenceIndex = sequenceIndex;
    WorldPacket const* redeemPkt = response.Write();
    TC_LOG_ERROR("housing", "<<< SMSG_HOUSING_REDEEM_DEFERRED_DECOR_RESPONSE ({} bytes): {}",
        redeemPkt->size(), HexDumpPacket(redeemPkt));
    TC_LOG_ERROR("housing", "    DecorGuid={} Result={} SequenceIndex={}", decorGuid.ToString(), response.Result, sequenceIndex);
    SendPacket(redeemPkt);

    // Push Account entity update to deliver the FHousingStorage_C change to the client
    account.SendUpdateToPlayer(player);

    // Persist the new catalog entry to DB (crash safety)
    if (instanceIndex == 0)
    {
        // First copy of this decor — INSERT new row
        CharacterDatabasePreparedStatement* stmt = CharacterDatabase.GetPreparedStatement(CHAR_INS_CHARACTER_HOUSING_CATALOG);
        uint8 idx = 0;
        stmt->setUInt64(idx++, player->GetGUID().GetCounter());
        stmt->setUInt32(idx++, decorEntryId);
        stmt->setUInt32(idx++, 1);
        stmt->setUInt8(idx++, DECOR_SOURCE_DEFERRED);
        stmt->setString(idx++, std::string{});
        CharacterDatabase.Execute(stmt);
    }
    else
    {
        // Additional copy — UPDATE existing row count
        CharacterDatabasePreparedStatement* stmt = CharacterDatabase.GetPreparedStatement(CHAR_UPD_CHARACTER_HOUSING_CATALOG_COUNT);
        stmt->setUInt32(0, instanceIndex + 1);
        stmt->setUInt64(1, player->GetGUID().GetCounter());
        stmt->setUInt32(2, decorEntryId);
        CharacterDatabase.Execute(stmt);
    }

    TC_LOG_ERROR("housing", "    Player {} redeemed decor entry={} → GUID={} (SourceType=3, Seq={}, instanceIdx={})",
        player->GetGUID().ToString(), decorEntryId, decorGuid.ToString(), sequenceIndex, instanceIndex);
}

// ============================================================
// Fixture System
// ============================================================

// Sniff-verified helper: After any fixture mutation (SetHouseType, SetCoreFixture,
// CreateFixture, etc.), retail sends an UPDATE_OBJECT carrying the changed MeshObject
// and house entity data. This sends it inline so the client gets it immediately
// rather than waiting for the next map tick.
void WorldSession::SendFixtureUpdateObject(Player* player, Housing* housing)
{
    if (!player || !housing)
        return;

    player->BuildUpdateChangesMask();
    GetHousingPlayerHouseEntity().BuildUpdateChangesMask();

    UpdateData updateData(player->GetMapId());
    WorldPacket updatePacket;

    // Player VALUES_UPDATE (editor mode / flags)
    player->BuildValuesUpdateBlockForPlayer(&updateData, player);

    // House entity VALUES_UPDATE (budget/type fields)
    if (player->HaveAtClient(&GetHousingPlayerHouseEntity()))
        GetHousingPlayerHouseEntity().BuildValuesUpdateBlockForPlayer(&updateData, player);
    else
    {
        GetHousingPlayerHouseEntity().BuildCreateUpdateBlockForPlayer(&updateData, player);
        player->m_clientGUIDs.insert(GetHousingPlayerHouseEntity().GetGUID());
    }

    // Include CREATE for any new MeshObjects spawned by the mutation
    if (HousingMap* housingMap = dynamic_cast<HousingMap*>(player->GetMap()))
    {
        uint8 plotIndex = housing->GetPlotIndex();
        auto const& meshMap = housingMap->GetPlotMeshObjects();
        auto meshItr = meshMap.find(plotIndex);
        if (meshItr != meshMap.end())
        {
            for (ObjectGuid const& meshGuid : meshItr->second)
            {
                MeshObject* meshObj = housingMap->GetMeshObject(meshGuid);
                if (!meshObj || !meshObj->IsInWorld())
                    continue;

                if (player->HaveAtClient(meshObj))
                    meshObj->BuildValuesUpdateBlockForPlayer(&updateData, player);
                else
                {
                    meshObj->BuildCreateUpdateBlockForPlayer(&updateData, player);
                    player->m_clientGUIDs.insert(meshGuid);
                }
            }
        }
    }

    updateData.BuildPacket(&updatePacket);
    player->SendDirectMessage(&updatePacket);

    player->ClearUpdateMask(false);
    GetHousingPlayerHouseEntity().ClearUpdateMask(true);
}

void WorldSession::HandleHousingFixtureSetEditMode(WorldPackets::Housing::HousingFixtureSetEditMode const& housingFixtureSetEditMode)
{
    Player* player = GetPlayer();
    if (!player)
        return;

    Housing* housing = player->GetHousing();
    if (!housing)
    {
        WorldPackets::Housing::HousingFixtureSetEditModeResponse response;
        response.Result = static_cast<uint8>(HOUSING_RESULT_HOUSE_NOT_FOUND);
        SendPacket(response.Write());
        return;
    }

    bool entering = housingFixtureSetEditMode.Active;

    // Client enum HouseEditorMode: 4=Customize (interior), 6=ExteriorCustomization (fixture).
    housing->SetEditorMode(entering ? HOUSING_EDITOR_MODE_EXTERIOR_CUSTOMIZATION : HOUSING_EDITOR_MODE_NONE);

    // Sniff-verified: retail sets UNIT_FLAG_PACIFIED, UNIT_FLAG2_NO_ACTIONS,
    // and SilencedSchoolMask=127 during ALL editor modes (decor, fixture, room layout).
    // These prevent casting/actions and are part of the UPDATE_OBJECT sent to the client.
    if (entering)
    {
        player->SetUnitFlag(UNIT_FLAG_PACIFIED);
        player->SetUnitFlag2(UNIT_FLAG2_NO_ACTIONS);
        player->ReplaceAllSilencedSchoolMask(SPELL_SCHOOL_MASK_ALL);
    }
    else
    {
        player->RemoveUnitFlag(UNIT_FLAG_PACIFIED);
        player->RemoveUnitFlag2(UNIT_FLAG2_NO_ACTIONS);
        player->ReplaceAllSilencedSchoolMask(SpellSchoolMask(0));
    }

    // Find the exterior root MeshObject GUID (Housing/3-HousingFixture) for HOUSE_EXTERIOR_LOCK_RESPONSE.
    ObjectGuid fixtureEntityGuid;
    if (HousingMap* housingMap = dynamic_cast<HousingMap*>(player->GetMap()))
    {
        uint8 plotIndex = housing->GetPlotIndex();
        auto const& meshMap = housingMap->GetPlotMeshObjects();
        auto meshItr = meshMap.find(plotIndex);
        if (meshItr != meshMap.end())
        {
            for (ObjectGuid const& meshGuid : meshItr->second)
            {
                MeshObject* meshObj = housingMap->GetMeshObject(meshGuid);
                if (meshObj && meshObj->IsExteriorRoot())
                {
                    fixtureEntityGuid = meshGuid;
                    break;
                }
            }
        }
    }

    TC_LOG_DEBUG("housing", "HandleHousingFixtureSetEditMode {}: fixtureEntity={} player={}",
        entering ? "ENTER" : "EXIT", fixtureEntityGuid.ToString(), player->GetGUID().ToString());

    // Prepare catalog/storage data before sending packets (enter only).
    if (entering)
    {
        housing->ResetStoragePopulated();
        housing->PopulateCatalogStorageEntries();
        housing->SyncUpdateFields();
    }

    // CRITICAL: Clear Account entity dirty state BEFORE sending any packets.
    // PopulateCatalogStorageEntries() modifies FHousingStorage_C which marks the
    // Account entity dirty. Unlike decor edit mode, fixture edit mode doesn't send
    // the Account entity as CREATE here. If we leave it dirty, Map::SendObjectUpdates()
    // will send a VALUES_UPDATE on the next tick, which the client rejects because
    // MapUpdateField entries can't be added via VALUES_UPDATE when initially empty.
    // Also clear on EXIT to prevent any lingering dirty state from decor operations.
    GetBattlenetAccount().ClearUpdateMask(true);

    // ======================================================================
    // Sniff-verified retail packet sequence (build 66337):
    //   #10161 S->C SMSG_UPDATE_OBJECT (56B)                — editor mode field change
    //   #10163 S->C SMSG_HOUSE_EXTERIOR_LOCK_RESPONSE (19B) — FixtureEntityGUID + PlayerGUID + Active
    //   #10164 S->C SMSG_MOVE_SET_COMPOUND_STATE (32B)      — ROOT + DISABLE_GRAVITY (enter) or UNROOT + ENABLE_GRAVITY (exit)
    //   #10170 S->C SMSG_HOUSING_FIXTURE_SET_EDIT_MODE_RESPONSE (11B) — Empty + PlayerGUID + Result
    //   (second UPDATE_OBJECT follows)
    // ======================================================================

    // Play/remove the plot boundary spell visual on the player's plot AT.
    // Sniff-verified: the glowing border decal is visible in ALL edit modes (decor, fixture, room).
    if (HousingMap* housingMap = dynamic_cast<HousingMap*>(player->GetMap()))
    {
        if (AreaTrigger* plotAt = housingMap->GetPlotAreaTrigger(housing->GetPlotIndex()))
        {
            if (entering)
                plotAt->PlaySpellVisual(510142);
        }
    }

    // 1) UPDATE_OBJECT — editor mode field change
    {
        player->BuildUpdateChangesMask();
        UpdateData updateData(player->GetMapId());
        WorldPacket updatePacket;
        player->BuildValuesUpdateBlockForPlayer(&updateData, player);
        updateData.BuildPacket(&updatePacket);
        player->SendDirectMessage(&updatePacket);
        player->ClearUpdateMask(false);
    }

    // 2) SMSG_HOUSE_EXTERIOR_LOCK_RESPONSE — tells client the fixture entity is locked for editing
    {
        WorldPackets::Housing::HouseExteriorLockResponse lockResponse;
        lockResponse.FixtureEntityGuid = fixtureEntityGuid;
        lockResponse.EditorPlayerGuid = player->GetGUID();
        lockResponse.Result = static_cast<uint8>(HOUSING_RESULT_SUCCESS);
        lockResponse.Active = entering;
        SendPacket(lockResponse.Write());
    }

    // 3) SMSG_MOVE_SET_COMPOUND_STATE — root + disable gravity on enter, unroot + enable gravity on exit
    //    Also update server-side movement flags so movement validation stays consistent.
    {
        if (entering)
        {
            player->RemoveUnitMovementFlag(MOVEMENTFLAG_MASK_MOVING);
            player->AddUnitMovementFlag(MOVEMENTFLAG_ROOT);
            player->StopMoving();
            player->AddUnitMovementFlag(MOVEMENTFLAG_DISABLE_GRAVITY);
            player->RemoveUnitMovementFlag(MOVEMENTFLAG_SWIMMING | MOVEMENTFLAG_SPLINE_ELEVATION);
        }
        else
        {
            player->RemoveUnitMovementFlag(MOVEMENTFLAG_ROOT);
            player->RemoveUnitMovementFlag(MOVEMENTFLAG_DISABLE_GRAVITY);
        }

        WorldPackets::Movement::MoveSetCompoundState compoundState;
        compoundState.MoverGUID = player->GetGUID();
        if (entering)
        {
            compoundState.StateChanges.emplace_back(SMSG_MOVE_ROOT, player->m_movementCounter++);
            compoundState.StateChanges.emplace_back(SMSG_MOVE_DISABLE_GRAVITY, player->m_movementCounter++);
        }
        else
        {
            compoundState.StateChanges.emplace_back(SMSG_MOVE_UNROOT, player->m_movementCounter++);
            compoundState.StateChanges.emplace_back(SMSG_MOVE_ENABLE_GRAVITY, player->m_movementCounter++);
        }
        SendPacket(compoundState.Write());
    }

    // 4) SMSG_HOUSING_FIXTURE_SET_EDIT_MODE_RESPONSE
    //    HouseGuid always empty. EditorPlayerGuid = player on enter, empty on exit.
    //    Client compares EditorPlayerGuid against stored reference: match → enter, empty → exit.
    {
        WorldPackets::Housing::HousingFixtureSetEditModeResponse response;
        // HouseGuid intentionally left empty — sniff-verified: always 00 00
        if (entering)
            response.EditorPlayerGuid = player->GetGUID();
        response.Result = static_cast<uint8>(HOUSING_RESULT_SUCCESS);
        SendPacket(response.Write());
    }

    // 5) Second UPDATE_OBJECT — sniff-verified: carries unit flags (PACIFIED, NO_ACTIONS,
    //    SilencedSchoolMask) that were set above. Client expects this after the response.
    {
        player->BuildUpdateChangesMask();
        UpdateData updateData(player->GetMapId());
        WorldPacket updatePacket;
        player->BuildValuesUpdateBlockForPlayer(&updateData, player);
        updateData.BuildPacket(&updatePacket);
        player->SendDirectMessage(&updatePacket);
        player->ClearUpdateMask(false);
    }

    // 6) Re-CREATE fixture entities now that the client's fixture manager is active.
    //
    // At plot entry, FlagByte=0xE0 sets multiple HouseStatus bits → the cascade function
    // defaults to state=0 → vf5(0) → state+1048=0. CREATE_BASIC_HOUSE_RESPONSE is gated
    // on state+1048!=0, so the rebuild never runs and state+96/+104 (house GUID) stays empty.
    // Fixture entity CREATEs from plot entry fire the CREATE callback, but it compares the
    // entity's FHousingFixture_C::HouseGUID against the empty state+96/+104 → mismatch → skip.
    //
    // Now the client has processed EDIT_MODE_RESPONSE: state+1048=6, rebuild has run,
    // state+96/+104 is populated. Send CREATE_BASIC_HOUSE_RESPONSE (teardown+rebuild for
    // a clean slate) then re-CREATE all fixture MeshObjects. The CREATE callback will
    // match house GUIDs → create HousingFixturePointFrame objects → fire
    // HOUSING_FIXTURE_POINT_FRAME_ADDED Lua events → UI populates hook points.
    if (entering)
    {
        // CREATE_BASIC_HOUSE_RESPONSE — now that state+1048=6, the handler passes
        // the gate check and runs teardown+rebuild for a clean fixture manager state.
        {
            WorldPackets::Housing::HousingFixtureCreateBasicHouseResponse fixtureInit;
            fixtureInit.Result = static_cast<uint8>(HOUSING_RESULT_SUCCESS);
            SendPacket(fixtureInit.Write());
        }

        // Re-CREATE all fixture MeshObjects for the player's plot.
        if (HousingMap* housingMap = dynamic_cast<HousingMap*>(player->GetMap()))
        {
            uint8 plotIndex = housing->GetPlotIndex();
            auto const& meshMap = housingMap->GetPlotMeshObjects();
            auto meshItr = meshMap.find(plotIndex);
            if (meshItr != meshMap.end())
            {
                UpdateData fixtureUpdate(player->GetMapId());
                uint32 fixtureCreateCount = 0;

                for (ObjectGuid const& meshGuid : meshItr->second)
                {
                    MeshObject* meshObj = housingMap->GetMeshObject(meshGuid);
                    if (meshObj && meshObj->IsInWorld() && meshObj->m_housingFixtureData.has_value())
                    {
                        meshObj->BuildCreateUpdateBlockForPlayer(&fixtureUpdate, player);
                        player->m_clientGUIDs.insert(meshGuid);
                        ++fixtureCreateCount;
                    }
                }

                if (fixtureCreateCount > 0)
                {
                    WorldPacket fixturePacket;
                    fixtureUpdate.BuildPacket(&fixturePacket);
                    player->SendDirectMessage(&fixturePacket);
                }

                TC_LOG_DEBUG("housing", "HandleHousingFixtureSetEditMode: Re-CREATE {} fixture MeshObjects for plot {}",
                    fixtureCreateCount, plotIndex);
            }
        }
    }
}

void WorldSession::HandleHousingFixtureSetCoreFixture(WorldPackets::Housing::HousingFixtureSetCoreFixture const& housingFixtureSetCoreFixture)
{
    Player* player = GetPlayer();
    if (!player)
        return;

    Housing* housing = player->GetHousing();
    if (!housing)
    {
        WorldPackets::Housing::HousingFixtureSetCoreFixtureResponse response;
        response.Result = static_cast<uint8>(HOUSING_RESULT_HOUSE_NOT_FOUND);
        SendPacket(response.Write());
        return;
    }

    // Validate ExteriorComponentID against DB2 store
    uint32 componentID = housingFixtureSetCoreFixture.ExteriorComponentID;

    TC_LOG_INFO("housing", "CMSG_HOUSING_FIXTURE_SET_CORE_FIXTURE: FixtureGuid={} ExteriorComponentID={} (store has {} entries)",
        housingFixtureSetCoreFixture.FixtureGuid.ToString(), componentID, sExteriorComponentStore.GetNumRows());

    ExteriorComponentEntry const* componentEntry = sExteriorComponentStore.LookupEntry(componentID);
    if (!componentEntry)
    {
        TC_LOG_INFO("housing", "CMSG_HOUSING_FIXTURE_SET_CORE_FIXTURE ExteriorComponentID {} not found in DB2 (store has {} entries)",
            componentID, sExteriorComponentStore.GetNumRows());
        WorldPackets::Housing::HousingFixtureSetCoreFixtureResponse response;
        response.Result = static_cast<uint8>(HOUSING_RESULT_FIXTURE_NOT_FOUND);
        SendPacket(response.Write());
        return;
    }

    TC_LOG_INFO("housing", "CMSG_HOUSING_FIXTURE_SET_CORE_FIXTURE DB2 lookup OK: ExteriorComponentID={}, Name='{}', Type={}, Size={}, Flags={}, ParentCompID={}, WmoDataID={}",
        componentID, componentEntry->Name[DEFAULT_LOCALE] ? componentEntry->Name[DEFAULT_LOCALE] : "",
        componentEntry->Type, componentEntry->Size, componentEntry->Flags,
        componentEntry->ParentComponentID, componentEntry->HouseExteriorWmoDataID);

    std::vector<uint32> removedHookIDs;
    HousingResult result = housing->SelectFixtureOption(componentID, 0, &removedHookIDs);

    WorldPackets::Housing::HousingFixtureSetCoreFixtureResponse response;
    response.Result = static_cast<uint8>(result);
    SendPacket(response.Write());

    if (result == HOUSING_RESULT_SUCCESS)
    {
        WorldPackets::Housing::AccountExteriorFixtureCollectionUpdate collectionUpdate;
        collectionUpdate.AddSingle(componentID);
        SendPacket(collectionUpdate.Write());

        // Respawn house visuals so the new fixture is visible immediately.
        // DespawnHouseForPlot removes ALL MeshObjects (house + decor), so we
        // must also respawn decor after rebuilding the house structure.
        if (HousingMap* housingMap = dynamic_cast<HousingMap*>(player->GetMap()))
        {
            uint8 plotIndex = housing->GetPlotIndex();
            auto fixtureOverrides = housing->GetFixtureOverrideMap();
            auto rootOverrides = housing->GetRootComponentOverrides();
            housingMap->DespawnAllDecorForPlot(plotIndex);
            housingMap->DespawnHouseForPlot(plotIndex);
            housingMap->SpawnHouseForPlot(plotIndex, nullptr,
                static_cast<int32>(housing->GetCoreExteriorComponentID()),
                static_cast<int32>(housing->GetHouseType()),
                fixtureOverrides.empty() ? nullptr : &fixtureOverrides,
                rootOverrides.empty() ? nullptr : &rootOverrides);
            housingMap->SpawnAllDecorForPlot(plotIndex, housing);
        }

        // Sniff-verified: UPDATE_OBJECT follows the response
        SendFixtureUpdateObject(player, housing);
    }

    TC_LOG_DEBUG("housing", "CMSG_HOUSING_FIXTURE_SET_CORE_FIXTURE FixtureGuid={} ExteriorComponentID={} Result={}",
        housingFixtureSetCoreFixture.FixtureGuid.ToString(), componentID, uint32(result));
}

void WorldSession::HandleHousingFixtureCreateFixture(WorldPackets::Housing::HousingFixtureCreateFixture const& housingFixtureCreateFixture)
{
    Player* player = GetPlayer();
    if (!player)
        return;

    Housing* housing = player->GetHousing();
    if (!housing)
    {
        WorldPackets::Housing::HousingFixtureCreateFixtureResponse response;
        response.Result = static_cast<uint8>(HOUSING_RESULT_HOUSE_NOT_FOUND);
        SendPacket(response.Write());
        return;
    }

    uint32 hookID = housingFixtureCreateFixture.ExteriorComponentHookID;
    uint32 componentID = housingFixtureCreateFixture.ExteriorComponentID;

    // Validate ExteriorComponentHook against DB2 store (which hook point on the house)
    ExteriorComponentHookEntry const* hookEntry = sExteriorComponentHookStore.LookupEntry(hookID);
    if (!hookEntry)
    {
        TC_LOG_DEBUG("housing", "CMSG_HOUSING_FIXTURE_CREATE_FIXTURE ExteriorComponentHookID {} not found in DB2", hookID);
        WorldPackets::Housing::HousingFixtureCreateFixtureResponse response;
        response.Result = static_cast<uint8>(HOUSING_RESULT_FIXTURE_NOT_FOUND);
        SendPacket(response.Write());
        return;
    }

    TC_LOG_DEBUG("housing", "CMSG_HOUSING_FIXTURE_CREATE_FIXTURE DB2 lookup: HookID={}, Position=({:.1f},{:.1f},{:.1f}), TypeID={}, ParentComponentID={}",
        hookID, hookEntry->Position[0], hookEntry->Position[1], hookEntry->Position[2],
        hookEntry->ExteriorComponentTypeID, hookEntry->ExteriorComponentID);

    // Validate ExteriorComponent against DB2 store (which component to install at the hook)
    ExteriorComponentEntry const* compEntry = sExteriorComponentStore.LookupEntry(componentID);
    if (!compEntry)
    {
        TC_LOG_DEBUG("housing", "CMSG_HOUSING_FIXTURE_CREATE_FIXTURE ExteriorComponentID {} not found in DB2", componentID);
        WorldPackets::Housing::HousingFixtureCreateFixtureResponse response;
        response.Result = static_cast<uint8>(HOUSING_RESULT_FIXTURE_NOT_FOUND);
        SendPacket(response.Write());
        return;
    }

    TC_LOG_DEBUG("housing", "  -> ExteriorComponent: ID={}, Name='{}', Type={}, Size={}, Flags={}, ParentCompID={}, ModelFileDataID={}",
        compEntry->ID, compEntry->Name[DEFAULT_LOCALE] ? compEntry->Name[DEFAULT_LOCALE] : "", compEntry->Type, compEntry->Size,
        compEntry->Flags, compEntry->ParentComponentID, compEntry->ModelFileDataID);

    std::vector<uint32> removedHookIDs;
    HousingResult result = housing->SelectFixtureOption(hookID, componentID, &removedHookIDs);

    // Spawn the fixture BEFORE sending the response so we can populate FixtureGuid.
    // The client's CREATE_FIXTURE_RESPONSE handler uses this GUID to identify the new entity.
    ObjectGuid newFixtureGuid;
    if (result == HOUSING_RESULT_SUCCESS)
    {
        WorldPackets::Housing::AccountExteriorFixtureCollectionUpdate collectionUpdate;
        collectionUpdate.AddSingle(componentID);
        SendPacket(collectionUpdate.Write());

        if (HousingMap* housingMap = dynamic_cast<HousingMap*>(player->GetMap()))
        {
            uint8 plotIndex = housing->GetPlotIndex();

            // Despawn meshes at ALL conflict hooks (old door at different hook, or old fixture at same hook)
            for (uint32 removedHook : removedHookIDs)
            {
                if (MeshObject* conflictMesh = housingMap->FindMeshObjectByHookID(plotIndex, static_cast<int32>(removedHook)))
                {
                    TC_LOG_DEBUG("housing", "CMSG_HOUSING_FIXTURE_CREATE_FIXTURE: Despawning conflict mesh {} at hook {}",
                        conflictMesh->GetGUID().ToString(), removedHook);
                    housingMap->DespawnSingleMeshObject(plotIndex, conflictMesh->GetGUID());
                }
            }

            // Despawn any mesh at the target hook
            if (MeshObject* oldMesh = housingMap->FindMeshObjectByHookID(plotIndex, static_cast<int32>(hookID)))
            {
                TC_LOG_DEBUG("housing", "CMSG_HOUSING_FIXTURE_CREATE_FIXTURE: Despawning old mesh {} at hook {}",
                    oldMesh->GetGUID().ToString(), hookID);
                housingMap->DespawnSingleMeshObject(plotIndex, oldMesh->GetGUID());
            }

            // Spawn new fixture mesh
            MeshObject* newMesh = housingMap->SpawnFixtureAtHook(plotIndex, hookID, componentID,
                housing->GetHouseGuid(), static_cast<int32>(housing->GetHouseType()), player);
            if (newMesh)
                newFixtureGuid = newMesh->GetFixtureGuid();

            // If this is a door (type=11), respawn the clickable door GO at the hook position.
            // Also respawn if a door was displaced from a different hook.
            TC_LOG_DEBUG("housing", "CMSG_HOUSING_FIXTURE_CREATE_FIXTURE: compType={} hookID={} componentID={} removedHooks={}",
                compEntry->Type, hookID, componentID, uint32(removedHookIDs.size()));
            if (compEntry->Type == HOUSING_FIXTURE_TYPE_DOOR)
            {
                housingMap->RespawnDoorGOAtHook(plotIndex, hookID, componentID, housing, player);
            }
            else
            {
                // Check if we displaced a door — if so, the door GO needs to be removed
                for (uint32 removedHook : removedHookIDs)
                {
                    ExteriorComponentHookEntry const* removedHookEntry = sExteriorComponentHookStore.LookupEntry(removedHook);
                    if (removedHookEntry && removedHookEntry->ExteriorComponentTypeID == HOUSING_FIXTURE_TYPE_DOOR)
                    {
                        // Door was removed — despawn the door GO (no new door to spawn)
                        housingMap->DespawnDoorGO(plotIndex);
                        break;
                    }
                }
            }
        }
    }

    // Send response with the fixture's Housing GUID (empty on failure)
    WorldPackets::Housing::HousingFixtureCreateFixtureResponse response;
    response.Result = static_cast<uint8>(result);
    response.FixtureGuid = newFixtureGuid;
    SendPacket(response.Write());

    if (result == HOUSING_RESULT_SUCCESS)
    {
        // Sniff-verified: UPDATE_OBJECT (~279B) follows the response
        SendFixtureUpdateObject(player, housing);
    }

    TC_LOG_DEBUG("housing", "CMSG_HOUSING_FIXTURE_CREATE_FIXTURE HookID={} ComponentID={} Result={}",
        hookID, componentID, uint32(result));
}

void WorldSession::HandleHousingFixtureDeleteFixture(WorldPackets::Housing::HousingFixtureDeleteFixture const& housingFixtureDeleteFixture)
{
    Player* player = GetPlayer();
    if (!player)
        return;

    Housing* housing = player->GetHousing();
    if (!housing)
    {
        WorldPackets::Housing::HousingFixtureDeleteFixtureResponse response;
        response.Result = static_cast<uint8>(HOUSING_RESULT_HOUSE_NOT_FOUND);
        SendPacket(response.Write());
        return;
    }

    uint32 componentID = housingFixtureDeleteFixture.ExteriorComponentID;
    uint32 originalID = componentID; // preserve original for RemoveFixture key lookup

    // The client may send either an ExteriorComponentID or an ExteriorComponentHookID
    // depending on the fixture type. Try the ExteriorComponent store first, then fall back
    // to resolving via ExteriorComponentHook → ExteriorComponent for DB2 validation.
    ExteriorComponentEntry const* componentEntry = sExteriorComponentStore.LookupEntry(componentID);
    if (!componentEntry)
    {
        // Try as a HookID — resolve to the parent ExteriorComponentID for validation only.
        // Keep originalID as the hookID for RemoveFixture (fixtures are keyed by hookID).
        ExteriorComponentHookEntry const* hookEntry = sExteriorComponentHookStore.LookupEntry(componentID);
        if (hookEntry)
        {
            componentEntry = sExteriorComponentStore.LookupEntry(hookEntry->ExteriorComponentID);
            if (componentEntry)
            {
                TC_LOG_DEBUG("housing", "CMSG_HOUSING_FIXTURE_DELETE_FIXTURE: Resolved hookID {} → ExteriorComponentID {} (using hookID as key)",
                    componentID, hookEntry->ExteriorComponentID);
                // DON'T overwrite componentID — keep the hookID for RemoveFixture
            }
        }
    }

    if (!componentEntry)
    {
        TC_LOG_DEBUG("housing", "CMSG_HOUSING_FIXTURE_DELETE_FIXTURE ExteriorComponentID/HookID {} not found in DB2", componentID);
        WorldPackets::Housing::HousingFixtureDeleteFixtureResponse response;
        response.Result = static_cast<uint8>(HOUSING_RESULT_FIXTURE_NOT_FOUND);
        SendPacket(response.Write());
        return;
    }

    TC_LOG_DEBUG("housing", "CMSG_HOUSING_FIXTURE_DELETE_FIXTURE DB2 lookup: ExteriorComponentID={}, Name='{}', Type={}, Flags={}",
        componentID, componentEntry->Name[DEFAULT_LOCALE] ? componentEntry->Name[DEFAULT_LOCALE] : "", componentEntry->Type, componentEntry->Flags);

    // Use originalID (hookID when client sent a hook, componentID otherwise) for fixture lookup.
    // RemoveFixture searches by key first (hookID), then by OptionId (componentID).
    uint32 removedHookID = 0;
    HousingResult result = housing->RemoveFixture(originalID, &removedHookID);

    WorldPackets::Housing::HousingFixtureDeleteFixtureResponse response;
    response.Result = static_cast<uint8>(result);
    response.FixtureGuid = housingFixtureDeleteFixture.FixtureGuid;
    SendPacket(response.Write());

    if (result == HOUSING_RESULT_SUCCESS)
    {
        // Targeted fixture mesh removal: only despawn the mesh at the removed hook,
        // then spawn the default component back. No full house rebuild needed.
        if (HousingMap* housingMap = dynamic_cast<HousingMap*>(player->GetMap()))
        {
            uint8 plotIndex = housing->GetPlotIndex();

            // Remove the player's custom mesh at this hook
            if (MeshObject* oldMesh = housingMap->FindMeshObjectByHookID(plotIndex, static_cast<int32>(removedHookID)))
            {
                TC_LOG_DEBUG("housing", "CMSG_HOUSING_FIXTURE_DELETE_FIXTURE: Despawning mesh {} at hook {}",
                    oldMesh->GetGUID().ToString(), removedHookID);
                housingMap->DespawnSingleMeshObject(plotIndex, oldMesh->GetGUID());
            }

            // Do NOT spawn a default fixture back — the user selected "None" to remove it.
            // The hook point should remain empty so the client shows the fixture point UI again.
            // If this was a door, remove the door GO — but only if no other door
            // override remains. When the client MOVES a door (CREATE-at-new-hook
            // immediately followed by DELETE-at-old-hook), the new GO is already
            // on the map; blindly despawning here would wipe it.
            if (componentEntry && componentEntry->Type == HOUSING_FIXTURE_TYPE_DOOR)
            {
                bool anotherDoorExists = false;
                for (auto const& [hookID, compID] : housing->GetFixtureOverrideMap())
                {
                    ExteriorComponentEntry const* otherComp = sExteriorComponentStore.LookupEntry(compID);
                    if (otherComp && otherComp->Type == HOUSING_FIXTURE_TYPE_DOOR)
                    {
                        anotherDoorExists = true;
                        break;
                    }
                }
                if (!anotherDoorExists)
                    housingMap->DespawnDoorGO(plotIndex);
            }
        }

        // Sniff-verified: UPDATE_OBJECT follows the response
        SendFixtureUpdateObject(player, housing);
    }

    TC_LOG_DEBUG("housing", "CMSG_HOUSING_FIXTURE_DELETE_FIXTURE FixtureGuid={} ExteriorComponentID={} Hook={} Result={}",
        housingFixtureDeleteFixture.FixtureGuid.ToString(), componentID, removedHookID, uint32(result));
}

void WorldSession::HandleHousingFixtureSetHouseSize(WorldPackets::Housing::HousingFixtureSetHouseSize const& housingFixtureSetHouseSize)
{
    Player* player = GetPlayer();
    if (!player)
        return;

    Housing* housing = player->GetHousing();
    if (!housing)
    {
        WorldPackets::Housing::HousingFixtureSetHouseSizeResponse response;
        response.Result = static_cast<uint8>(HOUSING_RESULT_HOUSE_NOT_FOUND);
        SendPacket(response.Write());
        return;
    }

    // Validate house size against HousingFixtureSize enum (1=Any, 2=Small, 3=Medium, 4=Large)
    uint8 requestedSize = housingFixtureSetHouseSize.Size;
    if (requestedSize < HOUSING_FIXTURE_SIZE_ANY || requestedSize > HOUSING_FIXTURE_SIZE_LARGE)
    {
        WorldPackets::Housing::HousingFixtureSetHouseSizeResponse response;
        response.Result = static_cast<uint8>(HOUSING_RESULT_HOUSE_EXTERIOR_SIZE_NOT_AVAILABLE);
        SendPacket(response.Write());

        TC_LOG_INFO("housing", "CMSG_HOUSING_FIXTURE_SET_HOUSE_SIZE HouseGuid: {}, Size: {} REJECTED (invalid size)",
            housingFixtureSetHouseSize.HouseGuid.ToString(), requestedSize);
        return;
    }

    // Reject if already that size
    if (requestedSize == housing->GetHouseSize())
    {
        WorldPackets::Housing::HousingFixtureSetHouseSizeResponse response;
        response.Result = static_cast<uint8>(HOUSING_RESULT_HOUSE_EXTERIOR_ALREADY_THAT_SIZE);
        SendPacket(response.Write());

        TC_LOG_INFO("housing", "CMSG_HOUSING_FIXTURE_SET_HOUSE_SIZE HouseGuid: {}, Size: {} REJECTED (already that size)",
            housingFixtureSetHouseSize.HouseGuid.ToString(), requestedSize);
        return;
    }

    // Persist the new house size
    housing->SetHouseSize(requestedSize);

    // Respawn house MeshObjects with updated size (must also respawn decor since DespawnHouseForPlot removes all MeshObjects)
    if (HousingMap* housingMap = dynamic_cast<HousingMap*>(player->GetMap()))
    {
        uint8 plotIndex = housing->GetPlotIndex();
        auto fixtureOverrides = housing->GetFixtureOverrideMap();
        auto rootOverrides = housing->GetRootComponentOverrides();
        housingMap->DespawnAllDecorForPlot(plotIndex);
        housingMap->DespawnHouseForPlot(plotIndex);
        housingMap->SpawnHouseForPlot(plotIndex, nullptr,
            static_cast<int32>(housing->GetCoreExteriorComponentID()),
            static_cast<int32>(housing->GetHouseType()),
            fixtureOverrides.empty() ? nullptr : &fixtureOverrides,
            rootOverrides.empty() ? nullptr : &rootOverrides);
        housingMap->SpawnAllDecorForPlot(plotIndex, housing);
    }

    WorldPackets::Housing::HousingFixtureSetHouseSizeResponse response;
    response.Result = static_cast<uint8>(HOUSING_RESULT_SUCCESS);
    response.Size = requestedSize;
    SendPacket(response.Write());

    // Sniff-verified: UPDATE_OBJECT follows the response
    SendFixtureUpdateObject(player, housing);

    TC_LOG_DEBUG("housing", "CMSG_HOUSING_FIXTURE_SET_HOUSE_SIZE HouseGuid={} Size={} Result=SUCCESS",
        housingFixtureSetHouseSize.HouseGuid.ToString(), requestedSize);
}

void WorldSession::HandleHousingFixtureSetHouseType(WorldPackets::Housing::HousingFixtureSetHouseType const& housingFixtureSetHouseType)
{
    Player* player = GetPlayer();
    if (!player)
        return;

    Housing* housing = player->GetHousing();
    if (!housing)
    {
        WorldPackets::Housing::HousingFixtureSetHouseTypeResponse response;
        response.Result = static_cast<uint8>(HOUSING_RESULT_HOUSE_NOT_FOUND);
        SendPacket(response.Write());
        return;
    }

    uint32 wmoDataID = housingFixtureSetHouseType.HouseExteriorWmoDataID;

    // Validate the requested house type exists in the HouseExteriorWmoData DB2 store
    HouseExteriorWmoData const* wmoData = sHousingMgr.GetHouseExteriorWmoData(wmoDataID);
    if (!wmoData)
    {
        WorldPackets::Housing::HousingFixtureSetHouseTypeResponse response;
        response.Result = static_cast<uint8>(HOUSING_RESULT_HOUSE_EXTERIOR_TYPE_NOT_FOUND);
        SendPacket(response.Write());

        TC_LOG_INFO("housing", "CMSG_HOUSING_FIXTURE_SET_HOUSE_TYPE HouseGuid: {}, WmoDataID: {} REJECTED (not found in DB2)",
            housingFixtureSetHouseType.HouseGuid.ToString(), wmoDataID);
        return;
    }

    // Reject if already that type
    if (wmoDataID == housing->GetHouseType())
    {
        WorldPackets::Housing::HousingFixtureSetHouseTypeResponse response;
        response.Result = static_cast<uint8>(HOUSING_RESULT_HOUSE_EXTERIOR_ALREADY_THAT_TYPE);
        SendPacket(response.Write());

        TC_LOG_INFO("housing", "CMSG_HOUSING_FIXTURE_SET_HOUSE_TYPE HouseGuid: {}, WmoDataID: {} REJECTED (already that type)",
            housingFixtureSetHouseType.HouseGuid.ToString(), wmoDataID);
        return;
    }

    // Persist the new house type
    housing->SetHouseType(wmoDataID);

    // Respawn house MeshObjects with updated type (must also respawn decor since DespawnHouseForPlot removes all MeshObjects)
    if (HousingMap* housingMap = dynamic_cast<HousingMap*>(player->GetMap()))
    {
        uint8 plotIndex = housing->GetPlotIndex();
        auto fixtureOverrides = housing->GetFixtureOverrideMap();
        auto rootOverrides = housing->GetRootComponentOverrides();
        housingMap->DespawnAllDecorForPlot(plotIndex);
        housingMap->DespawnHouseForPlot(plotIndex);
        housingMap->SpawnHouseForPlot(plotIndex, nullptr,
            static_cast<int32>(housing->GetCoreExteriorComponentID()),
            static_cast<int32>(wmoDataID),
            fixtureOverrides.empty() ? nullptr : &fixtureOverrides,
            rootOverrides.empty() ? nullptr : &rootOverrides);
        housingMap->SpawnAllDecorForPlot(plotIndex, housing);
    }

    // Sniff-verified packet order: SMSG response → UPDATE_OBJECT (~228B)
    WorldPackets::Housing::HousingFixtureSetHouseTypeResponse response;
    response.Result = static_cast<uint8>(HOUSING_RESULT_SUCCESS);
    response.HouseExteriorTypeID = wmoDataID;
    SendPacket(response.Write());

    // Notify account of house type collection update
    WorldPackets::Housing::AccountHouseTypeCollectionUpdate collectionUpdate;
    collectionUpdate.AddSingle(wmoDataID);
    SendPacket(collectionUpdate.Write());

    // Sniff-verified: UPDATE_OBJECT follows the response, carrying updated MeshObject
    // data for the new house type. Send inline so client gets it immediately.
    SendFixtureUpdateObject(player, housing);

    TC_LOG_DEBUG("housing", "CMSG_HOUSING_FIXTURE_SET_HOUSE_TYPE HouseGuid={} WmoDataID={} Result=SUCCESS",
        housingFixtureSetHouseType.HouseGuid.ToString(), wmoDataID);
}

// ============================================================
// Room System
// ============================================================

void WorldSession::HandleHousingRoomSetLayoutEditMode(WorldPackets::Housing::HousingRoomSetLayoutEditMode const& housingRoomSetLayoutEditMode)
{
    Player* player = GetPlayer();
    if (!player)
        return;

    Housing* housing = player->GetHousing();
    if (!housing)
    {
        WorldPackets::Housing::HousingRoomSetLayoutEditModeResponse response;
        response.Result = static_cast<uint8>(HOUSING_RESULT_HOUSE_NOT_FOUND);
        SendPacket(response.Write());
        return;
    }

    housing->SetEditorMode(housingRoomSetLayoutEditMode.Active ? HOUSING_EDITOR_MODE_LAYOUT : HOUSING_EDITOR_MODE_NONE);

    // Sniff-verified: retail sets UNIT_FLAG_PACIFIED, UNIT_FLAG2_NO_ACTIONS,
    // and SilencedSchoolMask=127 during layout edit mode. These prevent casting/actions
    // and are included in the same UpdateObject that carries EditorMode.
    if (housingRoomSetLayoutEditMode.Active)
    {
        player->SetUnitFlag(UNIT_FLAG_PACIFIED);
        player->SetUnitFlag2(UNIT_FLAG2_NO_ACTIONS);
        player->ReplaceAllSilencedSchoolMask(SPELL_SCHOOL_MASK_ALL);
    }
    else
    {
        player->RemoveUnitFlag(UNIT_FLAG_PACIFIED);
        player->RemoveUnitFlag2(UNIT_FLAG2_NO_ACTIONS);
        player->ReplaceAllSilencedSchoolMask(SpellSchoolMask(0));
    }

    // Play/remove the plot boundary spell visual on the player's plot AT.
    // Sniff-verified: the glowing border decal is visible in ALL edit modes (decor, fixture, room).
    if (HousingMap* housingMap = dynamic_cast<HousingMap*>(player->GetMap()))
    {
        if (AreaTrigger* plotAt = housingMap->GetPlotAreaTrigger(housing->GetPlotIndex()))
        {
            if (housingRoomSetLayoutEditMode.Active)
                plotAt->PlaySpellVisual(510142);
        }
    }

    // Sniff-verified (build 66838) wire format (10B both enter and exit):
    //   Enter: [PackedGUID PlayerGuid] 00 80
    //   Exit:  [PackedGUID PlayerGuid] 00 00
    WorldPackets::Housing::HousingRoomSetLayoutEditModeResponse response;
    response.PlayerGuid = player->GetGUID(); // Sniff-verified: retail sends Player GUID
    response.Result = static_cast<uint8>(HOUSING_RESULT_SUCCESS);
    response.Active = housingRoomSetLayoutEditMode.Active;
    SendPacket(response.Write());

    // Sync entity data for layout mode (enter needs budget values)
    if (housingRoomSetLayoutEditMode.Active)
    {
        housing->ResetStoragePopulated();
        housing->PopulateCatalogStorageEntries();
        housing->SyncUpdateFields();
    }

    // Sniff-verified: UPDATE_OBJECT (~56B) follows response for BOTH enter AND exit.
    // This carries EditorMode + UNIT_FLAG_PACIFIED + UNIT_FLAG2_NO_ACTIONS + SilencedSchoolMask.
    // On enter, also include account/entity data for the layout editor budgets.
    {
        player->BuildUpdateChangesMask();

        UpdateData updateData(player->GetMapId());
        WorldPacket updatePacket;
        player->BuildValuesUpdateBlockForPlayer(&updateData, player);

        if (housingRoomSetLayoutEditMode.Active)
        {
            GetBattlenetAccount().BuildUpdateChangesMask();
            GetHousingPlayerHouseEntity().BuildUpdateChangesMask();

            if (player->HaveAtClient(&GetBattlenetAccount()))
                GetBattlenetAccount().BuildValuesUpdateBlockForPlayer(&updateData, player);
            else
            {
                GetBattlenetAccount().BuildCreateUpdateBlockForPlayer(&updateData, player);
                player->m_clientGUIDs.insert(GetBattlenetAccount().GetGUID());
            }

            if (player->HaveAtClient(&GetHousingPlayerHouseEntity()))
                GetHousingPlayerHouseEntity().BuildValuesUpdateBlockForPlayer(&updateData, player);
            else
            {
                GetHousingPlayerHouseEntity().BuildCreateUpdateBlockForPlayer(&updateData, player);
                player->m_clientGUIDs.insert(GetHousingPlayerHouseEntity().GetGUID());
            }
        }

        updateData.BuildPacket(&updatePacket);
        player->SendDirectMessage(&updatePacket);

        player->ClearUpdateMask(false);
        if (housingRoomSetLayoutEditMode.Active)
        {
            GetBattlenetAccount().ClearUpdateMask(true);
            GetHousingPlayerHouseEntity().ClearUpdateMask(true);
        }
    }

    TC_LOG_DEBUG("housing", "CMSG_HOUSING_ROOM_SET_LAYOUT_EDIT_MODE Active={}", housingRoomSetLayoutEditMode.Active);
}

void WorldSession::HandleHousingRoomAdd(WorldPackets::Housing::HousingRoomAdd const& housingRoomAdd)
{
    Player* player = GetPlayer();
    if (!player)
        return;

    Housing* housing = player->GetHousing();
    if (!housing)
    {
        WorldPackets::Housing::HousingRoomAddResponse response;
        response.Result = static_cast<uint8>(HOUSING_RESULT_HOUSE_NOT_FOUND);
        SendPacket(response.Write());
        return;
    }

    // The CMSG sends TargetDoorComponentID — find which room owns this door,
    // determine the door's direction, and compute the 2D grid position for the new room.
    int32 newGridX = 0, newGridY = 0, newFloorIndex = 0;
    uint32 nextSlot = 0;
    {
        // Find the source room that owns the target door component
        uint32 doorCompId = housingRoomAdd.TargetDoorComponentID;
        for (auto const& [guid, room] : housing->GetRoomsMap())
        {
            if (room.SlotIndex >= nextSlot)
                nextSlot = room.SlotIndex + 1;

            // Check if this room contains the target door component
            HouseRoomData const* rd = sHousingMgr.GetHouseRoomData(room.RoomEntryId);
            if (!rd) continue;
            std::vector<RoomComponentData> const* comps = sHousingMgr.GetRoomComponents(rd->RoomWmoDataID);
            if (!comps) continue;

            for (auto const& comp : *comps)
            {
                if (comp.ID == doorCompId)
                {
                    // Compute new room position from door offsets.
                    // newCenter = sourceCenter + sourceDoorOffset - newDoorOffset
                    // where newDoorOffset is the OPPOSITE door of the new room.
                    // Source door at +12 → new room's left door at -12 → spacing = 24.
                    // Source door at +3 → new room's left door at -12 → spacing = 15.
                    float sourceDoorOffset = 0.0f;
                    float newDoorOffset = 0.0f;

                    // Find the new room's wall in the opposite direction.
                    // Don't filter by ConnectionType — stairwell walls have CT=0.
                    // Use the LARGEST offset wall (furthest boundary) for spacing.
                    HouseRoomData const* newRd = sHousingMgr.GetHouseRoomData(housingRoomAdd.HouseRoomID);
                    std::vector<RoomComponentData> const* newComps = newRd ? sHousingMgr.GetRoomComponents(newRd->RoomWmoDataID) : nullptr;

                    // Helper: find the most extreme wall offset in a direction
                    auto findMaxWallOffset = [&](std::vector<RoomComponentData> const* cs, int axis, bool negative) -> float
                    {
                        float best = 0.0f;
                        if (!cs) return best;
                        for (auto const& nc : *cs)
                        {
                            if (nc.Type != 1) continue; // walls only
                            float v = (axis == 0) ? nc.OffsetPos[0] : nc.OffsetPos[1];
                            if (negative && v < -0.5f && v < best) best = v;
                            if (!negative && v > 0.5f && v > best) best = v;
                        }
                        return best;
                    };

                    if (comp.OffsetPos[0] > 0.5f) // source door faces +X
                    {
                        sourceDoorOffset = comp.OffsetPos[0];
                        newDoorOffset = findMaxWallOffset(newComps, 0, true); // -X wall
                        newGridX = room.GridX + static_cast<int32>(sourceDoorOffset - newDoorOffset);
                        newGridY = room.GridY;
                    }
                    else if (comp.OffsetPos[0] < -0.5f) // source door faces -X
                    {
                        sourceDoorOffset = comp.OffsetPos[0];
                        newDoorOffset = findMaxWallOffset(newComps, 0, false); // +X wall
                        newGridX = room.GridX + static_cast<int32>(sourceDoorOffset - newDoorOffset);
                        newGridY = room.GridY;
                    }
                    else if (comp.OffsetPos[1] > 0.5f) // source door faces +Y
                    {
                        sourceDoorOffset = comp.OffsetPos[1];
                        newDoorOffset = findMaxWallOffset(newComps, 1, true); // -Y wall
                        newGridX = room.GridX;
                        newGridY = room.GridY + static_cast<int32>(sourceDoorOffset - newDoorOffset);
                    }
                    else if (comp.OffsetPos[1] < -0.5f) // source door faces -Y
                    {
                        sourceDoorOffset = comp.OffsetPos[1];
                        newDoorOffset = findMaxWallOffset(newComps, 1, false); // +Y wall
                        newGridX = room.GridX;
                        newGridY = room.GridY + static_cast<int32>(sourceDoorOffset - newDoorOffset);
                    }

                    // FloorIndex = floor NUMBER (0=ground, 1=floor2, ...).
                    // Sniff-verified: the client's editor uses FloorIndex as a small
                    // integer for its floor selector UI; world Z is computed as
                    // FloorIndex × FLOOR_HEIGHT_Y (12 yards) during room spawn.
                    // Stairwell room sits on the CURRENT floor; a room attached via a
                    // stairwell's CEILING door (Z>1) goes one floor up.
                    if (std::abs(comp.OffsetPos[2]) > 1.0f)
                        newFloorIndex = room.FloorIndex + 1;
                    else
                        newFloorIndex = room.FloorIndex;

                    TC_LOG_INFO("housing", "ROOM_ADD: sourceDoor={:.1f} newDoor={:.1f} doorZ={:.1f} -> gridX={} gridY={} floorZ={}",
                        sourceDoorOffset, newDoorOffset, comp.OffsetPos[2], newGridX, newGridY, newFloorIndex);
                    goto foundDoor;
                }
            }
        }
        // Fallback: stack linearly if door not found
        newGridX = static_cast<int32>(nextSlot);
        newGridY = 0;
        foundDoor:;
    }

    ObjectGuid newRoomGuid;
    HousingResult result = housing->PlaceRoom(housingRoomAdd.HouseRoomID, nextSlot,
        /*orientation*/ 0, /*mirrored*/ false, &newRoomGuid, newGridX, newGridY, newFloorIndex);

    WorldPackets::Housing::HousingRoomAddResponse response;
    response.Result = static_cast<uint8>(result);
    response.PlayerGuid = player->GetGUID(); // Sniff-verified: retail sends Player GUID, not room GUID
    SendPacket(response.Write());

    if (result == HOUSING_RESULT_SUCCESS)
    {
        // Blizzlike: a stairwell is one physical unit split across TWO room entities
        // at the same XY — the stairwell room at current floor + an "Empty Stairwell
        // Room" (ID=48) 12 yards above. Each has its own geobox so decor can be
        // placed on BOTH floors independently, and the upper room's ceiling sits
        // at world Z=24 (2×floor height), matching sniff observations.
        HouseRoomData const* addedRoom = sHousingMgr.GetHouseRoomData(housingRoomAdd.HouseRoomID);
        if (addedRoom && addedRoom->HasStairs())
        {
            constexpr uint32 STAIRWELL_EMPTY_ROOM_ID = 48;
            ObjectGuid upperRoomGuid;
            HousingResult upperRes = housing->PlaceRoom(STAIRWELL_EMPTY_ROOM_ID, nextSlot + 1,
                /*orientation*/ 0, /*mirrored*/ false, &upperRoomGuid,
                newGridX, newGridY, newFloorIndex + 1);
            TC_LOG_INFO("housing", "ROOM_ADD: stairwell partner (ID={}) placed at (gridX={}, gridY={}, floor={}) result={}",
                STAIRWELL_EMPTY_ROOM_ID, newGridX, newGridY, newFloorIndex + 1, uint32(upperRes));
        }

        if (HouseInteriorMap* interiorMap = dynamic_cast<HouseInteriorMap*>(player->GetMap()))
        {
            int32 faction = (player->GetTeamId() == TEAM_ALLIANCE)
                ? NEIGHBORHOOD_FACTION_ALLIANCE : NEIGHBORHOOD_FACTION_HORDE;

            // Spawn only the NEW room's entities (incremental).
            // SpawnRoomMeshObjects skips rooms with existing MeshObjects/RoomEntities.
            interiorMap->SpawnRoomMeshObjects(housing, faction);

            // Update the source room's wall at the connecting door:
            // 1. Replace the Cosmetic wall MeshObject with DoorwayWall+Doorway pair
            // 2. Update the HousingRoomEntity's door AttachedRoomGUID
            if (!newRoomGuid.IsEmpty())
            {
                // Find the source room that owns the door component
                for (auto const& [guid, rm] : housing->GetRoomsMap())
                {
                    if (guid == newRoomGuid) continue;
                    HouseRoomData const* rd = sHousingMgr.GetHouseRoomData(rm.RoomEntryId);
                    if (!rd) continue;
                    std::vector<RoomComponentData> const* cs = sHousingMgr.GetRoomComponents(rd->RoomWmoDataID);
                    if (!cs) continue;
                    for (auto const& c : *cs)
                    {
                        if (c.ID == housingRoomAdd.TargetDoorComponentID)
                        {
                            // Replace wall with doorway — stairwell rooms connect
                            // HORIZONTALLY through walls, same as any other room.
                            interiorMap->ReplaceWallWithDoorway(guid, housingRoomAdd.TargetDoorComponentID,
                                faction, rm, newRoomGuid);
                            // Update door connection data
                            for (HousingRoomEntity* re : interiorMap->GetRoomEntities())
                            {
                                if (re && re->IsInWorld())
                                    re->UpdateDoorConnection(housingRoomAdd.TargetDoorComponentID, newRoomGuid);
                            }
                            goto doneUpdate;
                        }
                    }
                }
                doneUpdate:;
            }
        }

        WorldPackets::Housing::AccountRoomCollectionUpdate roomUpdate;
        roomUpdate.AddSingle(housingRoomAdd.HouseRoomID);
        SendPacket(roomUpdate.Write());
    }

    TC_LOG_INFO("housing", "CMSG_HOUSING_ROOM_ADD DoorComponentID: {}, HouseRoomID: {}, FloorIndex: {}, Result: {}",
        housingRoomAdd.TargetDoorComponentID, housingRoomAdd.HouseRoomID, housingRoomAdd.FloorIndex, uint32(result));
}

void WorldSession::HandleHousingRoomRemove(WorldPackets::Housing::HousingRoomRemove const& housingRoomRemove)
{
    Player* player = GetPlayer();
    if (!player)
        return;

    Housing* housing = player->GetHousing();
    if (!housing)
    {
        WorldPackets::Housing::HousingRoomRemoveResponse response;
        response.Result = static_cast<uint8>(HOUSING_RESULT_HOUSE_NOT_FOUND);
        SendPacket(response.Write());
        return;
    }

    // Collect info BEFORE RemoveRoom erases data
    std::vector<ObjectGuid> roomDecorGuids;
    for (auto const* decor : housing->GetAllPlacedDecor())
    {
        if (decor && decor->RoomGuid == housingRoomRemove.RoomGuid)
            roomDecorGuids.push_back(decor->Guid);
    }

    // Find adjacent rooms that had their connecting wall skipped because this room existed.
    // Those walls need to be restored after this room is removed.
    // Rule: parent rooms (lower slotIndex) skip their wall when a child (higher slot) exists.
    // So we look for neighbors with LOWER slotIndex — they need wall restoration.
    struct WallRestore { ObjectGuid roomGuid; uint32 doorCompID; };
    std::vector<WallRestore> wallsToRestore;
    {
        auto removedItr = housing->GetRoomsMap().find(housingRoomRemove.RoomGuid);
        if (removedItr != housing->GetRoomsMap().end())
        {
            Housing::Room const& removedRoom = removedItr->second;
            HouseRoomData const* removedRd = sHousingMgr.GetHouseRoomData(removedRoom.RoomEntryId);
            if (removedRd)
            {
                // For each neighbor with lower slot, find which of THEIR door components
                // was skipped because this room was connected
                for (auto const& [nGuid, nRoom] : housing->GetRoomsMap())
                {
                    if (nGuid == housingRoomRemove.RoomGuid)
                        continue;
                    if (nRoom.SlotIndex >= removedRoom.SlotIndex)
                        continue; // only restore walls on rooms with LOWER slot

                    HouseRoomData const* nRd = sHousingMgr.GetHouseRoomData(nRoom.RoomEntryId);
                    if (!nRd) continue;
                    std::vector<RoomComponentData> const* nComps = sHousingMgr.GetRoomComponents(nRd->RoomWmoDataID);
                    if (!nComps) continue;

                    for (auto const& nc : *nComps)
                    {
                        if (nc.ConnectionType == 0) continue;
                        // Check if this door faces the removed room's position
                        float doorWorldX = nRoom.GridX + nc.OffsetPos[0];
                        float doorWorldY = nRoom.GridY + nc.OffsetPos[1];
                        float dx = static_cast<float>(removedRoom.GridX) - doorWorldX;
                        float dy = static_cast<float>(removedRoom.GridY) - doorWorldY;
                        if (std::abs(dx) < 15.0f && std::abs(dy) < 15.0f)
                        {
                            wallsToRestore.push_back({ nGuid, nc.ID });
                            break;
                        }
                    }
                }
            }
        }
    }

    // Stairwells are stacked pairs — if removing a base stairwell, also remove
    // the partner room directly above (same XY, FloorIndex+1). Vice versa for
    // partner removal. Without this, whichever one stays behind is orphaned and
    // the graph-connectivity check blocks future removals.
    ObjectGuid pairedRoomGuid;
    {
        auto itr = housing->GetRoomsMap().find(housingRoomRemove.RoomGuid);
        if (itr != housing->GetRoomsMap().end())
        {
            Housing::Room const& rm = itr->second;
            HouseRoomData const* rd = sHousingMgr.GetHouseRoomData(rm.RoomEntryId);
            if (rd && rd->HasStairs())
            {
                for (auto const& [gGuid, gRm] : housing->GetRoomsMap())
                {
                    if (gGuid == housingRoomRemove.RoomGuid) continue;
                    if (gRm.GridX != rm.GridX || gRm.GridY != rm.GridY) continue;
                    if (std::abs(gRm.FloorIndex - rm.FloorIndex) != 1) continue;
                    HouseRoomData const* gRd = sHousingMgr.GetHouseRoomData(gRm.RoomEntryId);
                    if (gRd && gRd->HasStairs())
                    {
                        pairedRoomGuid = gGuid;
                        break;
                    }
                }
            }
        }
    }

    // Collect decor and despawn info for the paired room BEFORE removal
    std::vector<ObjectGuid> pairedDecorGuids;
    if (!pairedRoomGuid.IsEmpty())
    {
        for (auto const* decor : housing->GetAllPlacedDecor())
            if (decor && decor->RoomGuid == pairedRoomGuid)
                pairedDecorGuids.push_back(decor->Guid);
        // Remove the partner first so the main removal's graph-connectivity
        // check doesn't flag the leftover as unreachable.
        housing->RemoveRoom(pairedRoomGuid);
    }

    HousingResult result = housing->RemoveRoom(housingRoomRemove.RoomGuid);

    WorldPackets::Housing::HousingRoomRemoveResponse response;
    response.Result = static_cast<uint8>(result);
    response.RoomGuid = housingRoomRemove.RoomGuid;
    SendPacket(response.Write());

    if (result == HOUSING_RESULT_SUCCESS)
    {
        if (HouseInteriorMap* interiorMap = dynamic_cast<HouseInteriorMap*>(player->GetMap()))
        {
            // Despawn decor visuals
            for (ObjectGuid const& decorGuid : roomDecorGuids)
                interiorMap->DespawnDecorItem(decorGuid);
            for (ObjectGuid const& decorGuid : pairedDecorGuids)
                interiorMap->DespawnDecorItem(decorGuid);

            // Despawn room entities (including paired stairwell partner, if any)
            interiorMap->DespawnRoomEntities(housingRoomRemove.RoomGuid);
            if (!pairedRoomGuid.IsEmpty())
                interiorMap->DespawnRoomEntities(pairedRoomGuid);

            // Restore walls on adjacent rooms that were skipped
            int32 faction = (player->GetTeamId() == TEAM_ALLIANCE)
                ? NEIGHBORHOOD_FACTION_ALLIANCE : NEIGHBORHOOD_FACTION_HORDE;
            for (auto const& restore : wallsToRestore)
            {
                auto roomItr = housing->GetRoomsMap().find(restore.roomGuid);
                if (roomItr == housing->GetRoomsMap().end())
                    continue;
                // Respawn just the wall component that was skipped
                std::vector<uint32> compIDs = { restore.doorCompID };
                interiorMap->RespawnRoomComponentsForTheme(restore.roomGuid, faction,
                    roomItr->second, &compIDs, static_cast<int32>(roomItr->second.ThemeId));

                TC_LOG_INFO("housing", "ROOM_REMOVE: Restored wall compID={} on room {} (was skipped for removed room)",
                    restore.doorCompID, restore.roomGuid.ToString());
            }
        }
    }

    TC_LOG_INFO("housing", "CMSG_HOUSING_ROOM_REMOVE_ROOM RoomGuid: {}, Result: {}",
        housingRoomRemove.RoomGuid.ToString(), uint32(result));
}

void WorldSession::HandleHousingRoomRotate(WorldPackets::Housing::HousingRoomRotate const& housingRoomRotate)
{
    Player* player = GetPlayer();
    if (!player)
        return;

    Housing* housing = player->GetHousing();
    if (!housing)
    {
        WorldPackets::Housing::HousingRoomUpdateResponse response;
        response.Result = static_cast<uint8>(HOUSING_RESULT_HOUSE_NOT_FOUND);
        SendPacket(response.Write());
        return;
    }

    HousingResult result = housing->RotateRoom(housingRoomRotate.RoomGuid, housingRoomRotate.Clockwise);

    // Stairwell pair: if the rotated room is part of a stairwell stack, rotate
    // its partner too so both rooms stay aligned (same orientation, same XY).
    ObjectGuid pairedRoomGuid;
    if (result == HOUSING_RESULT_SUCCESS)
    {
        auto itr = housing->GetRoomsMap().find(housingRoomRotate.RoomGuid);
        if (itr != housing->GetRoomsMap().end())
        {
            Housing::Room const& rm = itr->second;
            HouseRoomData const* rd = sHousingMgr.GetHouseRoomData(rm.RoomEntryId);
            if (rd && rd->HasStairs())
            {
                for (auto const& [gGuid, gRm] : housing->GetRoomsMap())
                {
                    if (gGuid == housingRoomRotate.RoomGuid) continue;
                    if (gRm.GridX != rm.GridX || gRm.GridY != rm.GridY) continue;
                    if (std::abs(gRm.FloorIndex - rm.FloorIndex) != 1) continue;
                    HouseRoomData const* gRd = sHousingMgr.GetHouseRoomData(gRm.RoomEntryId);
                    if (gRd && gRd->HasStairs())
                    {
                        pairedRoomGuid = gGuid;
                        break;
                    }
                }
            }
        }
        if (!pairedRoomGuid.IsEmpty())
            housing->RotateRoom(pairedRoomGuid, housingRoomRotate.Clockwise);
    }

    WorldPackets::Housing::HousingRoomUpdateResponse response;
    response.Result = static_cast<uint8>(result);
    response.RoomGuid = housingRoomRotate.RoomGuid;
    SendPacket(response.Write());

    if (result == HOUSING_RESULT_SUCCESS)
    {
        // Sniff-verified (build 66838): retail UPDATE_OBJECT after rotation contains CREATE/UPDATE
        // blocks for ALL child mesh objects (walls, floor, ceiling), not just the parent room entity.
        // Despawn the rotated room's entities then re-spawn them with the new orientation.
        if (HouseInteriorMap* interiorMap = dynamic_cast<HouseInteriorMap*>(player->GetMap()))
        {
            interiorMap->DespawnRoomEntities(housingRoomRotate.RoomGuid);
            if (!pairedRoomGuid.IsEmpty())
                interiorMap->DespawnRoomEntities(pairedRoomGuid);

            int32 faction = (player->GetTeamId() == TEAM_ALLIANCE)
                ? NEIGHBORHOOD_FACTION_ALLIANCE : NEIGHBORHOOD_FACTION_HORDE;
            interiorMap->SpawnRoomMeshObjects(housing, faction);
        }
    }

    TC_LOG_INFO("housing", "CMSG_HOUSING_ROOM_ROTATE RoomGuid: {}, Clockwise: {}, Result: {}",
        housingRoomRotate.RoomGuid.ToString(), housingRoomRotate.Clockwise, uint32(result));
}

void WorldSession::HandleHousingRoomMoveRoom(WorldPackets::Housing::HousingRoomMoveRoom const& housingRoomMoveRoom)
{
    Player* player = GetPlayer();
    if (!player)
        return;

    Housing* housing = player->GetHousing();
    if (!housing)
    {
        WorldPackets::Housing::HousingRoomUpdateResponse response;
        response.Result = static_cast<uint8>(HOUSING_RESULT_HOUSE_NOT_FOUND);
        SendPacket(response.Write());
        return;
    }

    HousingResult result = housing->MoveRoom(housingRoomMoveRoom.RoomGuid, housingRoomMoveRoom.TargetSlotIndex,
        housingRoomMoveRoom.TargetGuid, housingRoomMoveRoom.FloorIndex);

    WorldPackets::Housing::HousingRoomUpdateResponse response;
    response.Result = static_cast<uint8>(result);
    response.RoomGuid = housingRoomMoveRoom.RoomGuid;
    SendPacket(response.Write());

    // RefreshInteriorRoomVisuals crashes on same-GUID DESTROY+CREATE; the response
    // packet alone is enough for the client to update its layout, full visual refresh
    // happens on relog.
    TC_LOG_INFO("housing", "CMSG_HOUSING_ROOM_MOVE RoomGuid: {}, TargetSlotIndex: {}, Result: {}",
        housingRoomMoveRoom.RoomGuid.ToString(), housingRoomMoveRoom.TargetSlotIndex, uint32(result));
}

void WorldSession::HandleHousingRoomSetComponentTheme(WorldPackets::Housing::HousingRoomSetComponentTheme const& housingRoomSetComponentTheme)
{
    Player* player = GetPlayer();
    if (!player)
        return;

    Housing* housing = player->GetHousing();
    if (!housing)
    {
        WorldPackets::Housing::HousingRoomSetComponentThemeResponse response;
        response.Result = static_cast<uint8>(HOUSING_RESULT_HOUSE_NOT_FOUND);
        SendPacket(response.Write());
        return;
    }

    HousingResult result = housing->ApplyRoomTheme(housingRoomSetComponentTheme.RoomGuid,
        housingRoomSetComponentTheme.HouseThemeID, housingRoomSetComponentTheme.OptionIDs);

    WorldPackets::Housing::HousingRoomSetComponentThemeResponse response;
    response.Result = static_cast<uint8>(result);
    response.RoomGuid = housingRoomSetComponentTheme.RoomGuid;
    response.ThemeSetID = housingRoomSetComponentTheme.HouseThemeID;
    response.OptionIDs = housingRoomSetComponentTheme.OptionIDs;
    SendPacket(response.Write());

    // Theme changes require different 3D models (FileDataIDs), so DESTROY old meshes
    // and CREATE new ones with new GUIDs. Sniff shows walls disappearing/reappearing.
    // Filter compIDs by type: only respawn components that match the dominant type in the
    // request. "Apply wall style to all" sends all compIDs including floor/ceiling, but
    // only wall components should change. We detect this by checking component types.
    if (result == HOUSING_RESULT_SUCCESS)
    {
        if (HouseInteriorMap* interiorMap = dynamic_cast<HouseInteriorMap*>(player->GetMap()))
        {
            int32 faction = (player->GetTeamId() == TEAM_ALLIANCE)
                ? NEIGHBORHOOD_FACTION_ALLIANCE : NEIGHBORHOOD_FACTION_HORDE;
            auto const& rooms = housing->GetRoomsMap();
            auto roomItr = rooms.find(housingRoomSetComponentTheme.RoomGuid);
            if (roomItr != rooms.end())
            {
                // Filter: only respawn wall-type components from the list.
                // The client's "apply to all walls" sends ALL compIDs including
                // floor/ceiling, but those should keep their current theme.
                std::vector<uint32> wallOnlyCompIDs;
                for (uint32 cid : housingRoomSetComponentTheme.OptionIDs)
                {
                    RoomComponentEntry const* compEntry = sRoomComponentStore.LookupEntry(cid);
                    if (compEntry && (compEntry->Type == HOUSING_ROOM_COMPONENT_WALL
                        || compEntry->Type == HOUSING_ROOM_COMPONENT_DOORWAY_WALL
                        || compEntry->Type == HOUSING_ROOM_COMPONENT_DOORWAY))
                        wallOnlyCompIDs.push_back(cid);
                }

                interiorMap->RespawnRoomComponentsForTheme(housingRoomSetComponentTheme.RoomGuid, faction,
                    roomItr->second, wallOnlyCompIDs.empty() ? &housingRoomSetComponentTheme.OptionIDs : &wallOnlyCompIDs,
                    housingRoomSetComponentTheme.HouseThemeID);
            }
        }
    }

    TC_LOG_INFO("housing", "CMSG_HOUSING_ROOM_SET_COMPONENT_THEME RoomGuid: {}, HouseThemeID: {}, OptionCount: {}, Result: {}",
        housingRoomSetComponentTheme.RoomGuid.ToString(), housingRoomSetComponentTheme.HouseThemeID,
        housingRoomSetComponentTheme.OptionIDs.size(), uint32(result));
}

void WorldSession::HandleHousingRoomApplyComponentMaterials(WorldPackets::Housing::HousingRoomApplyComponentMaterials const& housingRoomApplyComponentMaterials)
{
    Player* player = GetPlayer();
    if (!player)
        return;

    Housing* housing = player->GetHousing();
    if (!housing)
    {
        WorldPackets::Housing::HousingRoomApplyComponentMaterialsResponse response;
        response.Result = static_cast<uint8>(HOUSING_RESULT_HOUSE_NOT_FOUND);
        SendPacket(response.Write());
        return;
    }

    HousingResult result = housing->ApplyRoomMaterial(housingRoomApplyComponentMaterials.RoomGuid,
        housingRoomApplyComponentMaterials.RoomComponentTextureID,
        housingRoomApplyComponentMaterials.ColorOverride,
        housingRoomApplyComponentMaterials.OptionIDs);

    WorldPackets::Housing::HousingRoomApplyComponentMaterialsResponse response;
    response.Result = static_cast<uint8>(result);
    response.RoomGuid = housingRoomApplyComponentMaterials.RoomGuid;
    response.RoomComponentTextureID = housingRoomApplyComponentMaterials.RoomComponentTextureID;
    response.OptionIDs = housingRoomApplyComponentMaterials.OptionIDs;
    SendPacket(response.Write());

    // Material/texture changes: UPDATE_OBJECT with new textureID (no model change)
    if (result == HOUSING_RESULT_SUCCESS)
    {
        if (HouseInteriorMap* interiorMap = dynamic_cast<HouseInteriorMap*>(player->GetMap()))
        {
            int32 textureID = static_cast<int32>(housingRoomApplyComponentMaterials.RoomComponentTextureID);
            auto const& rooms = housing->GetRoomsMap();
            auto roomItr = rooms.find(housingRoomApplyComponentMaterials.RoomGuid);
            if (roomItr != rooms.end())
                interiorMap->UpdateRoomComponentTextures(housingRoomApplyComponentMaterials.RoomGuid,
                    roomItr->second, &housingRoomApplyComponentMaterials.OptionIDs, textureID);
        }
    }

    TC_LOG_INFO("housing", "CMSG_HOUSING_ROOM_APPLY_COMPONENT_MATERIALS RoomGuid: {}, TextureID: {}, ColorOverride: {}, OptionCount: {}, Result: {}",
        housingRoomApplyComponentMaterials.RoomGuid.ToString(), housingRoomApplyComponentMaterials.RoomComponentTextureID,
        housingRoomApplyComponentMaterials.ColorOverride, housingRoomApplyComponentMaterials.OptionIDs.size(), uint32(result));
}

void WorldSession::HandleHousingRoomSetDoorType(WorldPackets::Housing::HousingRoomSetDoorType const& housingRoomSetDoorType)
{
    Player* player = GetPlayer();
    if (!player)
        return;

    Housing* housing = player->GetHousing();
    if (!housing)
    {
        WorldPackets::Housing::HousingRoomSetDoorTypeResponse response;
        response.Result = static_cast<uint8>(HOUSING_RESULT_HOUSE_NOT_FOUND);
        SendPacket(response.Write());
        return;
    }

    HousingResult result = housing->SetDoorType(housingRoomSetDoorType.RoomGuid,
        housingRoomSetDoorType.ThemeOptionID, housingRoomSetDoorType.DoorType);

    WorldPackets::Housing::HousingRoomSetDoorTypeResponse response;
    response.Result = static_cast<uint8>(result);
    response.RoomGuid = housingRoomSetDoorType.RoomGuid;
    response.ComponentID = housingRoomSetDoorType.ThemeOptionID;
    response.DoorType = housingRoomSetDoorType.DoorType;
    SendPacket(response.Write());

    // Door type selects between door model variants via RoomCompID.
    // Different FileDataIDs per variant, so respawn with correct model.
    if (result == HOUSING_RESULT_SUCCESS)
    {
        if (HouseInteriorMap* interiorMap = dynamic_cast<HouseInteriorMap*>(player->GetMap()))
        {
            int32 faction = (player->GetTeamId() == TEAM_ALLIANCE)
                ? NEIGHBORHOOD_FACTION_ALLIANCE : NEIGHBORHOOD_FACTION_HORDE;
            auto const& rooms = housing->GetRoomsMap();
            auto roomItr = rooms.find(housingRoomSetDoorType.RoomGuid);
            if (roomItr != rooms.end())
            {
                std::vector<uint32> compIDs = { housingRoomSetDoorType.ThemeOptionID };
                interiorMap->RespawnRoomComponentsForTheme(housingRoomSetDoorType.RoomGuid, faction,
                    roomItr->second, &compIDs, static_cast<int32>(roomItr->second.ThemeId),
                    -1, housingRoomSetDoorType.DoorType);
            }
        }
    }

    TC_LOG_INFO("housing", "CMSG_HOUSING_ROOM_SET_DOOR_TYPE RoomGuid: {}, ThemeOptionID: {}, DoorType: {}, Result: {}",
        housingRoomSetDoorType.RoomGuid.ToString(), housingRoomSetDoorType.ThemeOptionID, housingRoomSetDoorType.DoorType, uint32(result));
}

void WorldSession::HandleHousingRoomSetCeilingType(WorldPackets::Housing::HousingRoomSetCeilingType const& housingRoomSetCeilingType)
{
    Player* player = GetPlayer();
    if (!player)
        return;

    Housing* housing = player->GetHousing();
    if (!housing)
    {
        WorldPackets::Housing::HousingRoomSetCeilingTypeResponse response;
        response.Result = static_cast<uint8>(HOUSING_RESULT_HOUSE_NOT_FOUND);
        SendPacket(response.Write());
        return;
    }

    HousingResult result = housing->SetCeilingType(housingRoomSetCeilingType.RoomGuid,
        housingRoomSetCeilingType.ThemeOptionID, housingRoomSetCeilingType.CeilingType);

    WorldPackets::Housing::HousingRoomSetCeilingTypeResponse response;
    response.Result = static_cast<uint8>(result);
    response.RoomGuid = housingRoomSetCeilingType.RoomGuid;
    response.ComponentID = housingRoomSetCeilingType.ThemeOptionID;
    response.CeilingType = housingRoomSetCeilingType.CeilingType;
    SendPacket(response.Write());

    // Ceiling type selects between model variants (normal=RoomCompID 0, vaulted=RoomCompID 1).
    // Different FileDataIDs per variant, so we must respawn with the correct model.
    // overrideRoomCompID filters which option to spawn (only the one matching CeilingType).
    if (result == HOUSING_RESULT_SUCCESS)
    {
        if (HouseInteriorMap* interiorMap = dynamic_cast<HouseInteriorMap*>(player->GetMap()))
        {
            int32 faction = (player->GetTeamId() == TEAM_ALLIANCE)
                ? NEIGHBORHOOD_FACTION_ALLIANCE : NEIGHBORHOOD_FACTION_HORDE;
            auto const& rooms = housing->GetRoomsMap();
            auto roomItr = rooms.find(housingRoomSetCeilingType.RoomGuid);
            if (roomItr != rooms.end())
            {
                std::vector<uint32> compIDs = { housingRoomSetCeilingType.ThemeOptionID };
                interiorMap->RespawnRoomComponentsForTheme(housingRoomSetCeilingType.RoomGuid, faction,
                    roomItr->second, &compIDs, static_cast<int32>(roomItr->second.ThemeId),
                    -1, housingRoomSetCeilingType.CeilingType);
            }
        }
    }

    TC_LOG_INFO("housing", "CMSG_HOUSING_ROOM_SET_CEILING_TYPE RoomGuid: {}, ThemeOptionID: {}, CeilingType: {}, Result: {}",
        housingRoomSetCeilingType.RoomGuid.ToString(), housingRoomSetCeilingType.ThemeOptionID, housingRoomSetCeilingType.CeilingType, uint32(result));
}

// ============================================================
// Housing Services System
// ============================================================

void WorldSession::HandleHousingSvcsGuildCreateNeighborhood(WorldPackets::Housing::HousingSvcsGuildCreateNeighborhood const& housingSvcsGuildCreateNeighborhood)
{
    Player* player = GetPlayer();
    if (!player)
        return;

    if (!sWorld->getBoolConfig(CONFIG_HOUSING_ENABLE_CREATE_GUILD_NEIGHBORHOOD))
    {
        WorldPackets::Housing::HousingSvcsCreateCharterNeighborhoodResponse response;
        response.TrailingResult = static_cast<uint8>(HOUSING_RESULT_SERVICE_NOT_AVAILABLE);
        SendPacket(response.Write());
        return;
    }

    // Validate name (profanity/length check using charter name rules)
    if (!ObjectMgr::IsValidCharterName(housingSvcsGuildCreateNeighborhood.NeighborhoodName))
    {
        WorldPackets::Housing::HousingSvcsCreateCharterNeighborhoodResponse response;
        response.TrailingResult = static_cast<uint8>(HOUSING_RESULT_FILTER_REJECTED);
        SendPacket(response.Write());
        return;
    }

    // Validate guild membership and size
    Guild* guild = sGuildMgr->GetGuildById(player->GetGuildId());
    if (!guild)
    {
        WorldPackets::Housing::HousingSvcsCreateCharterNeighborhoodResponse response;
        response.TrailingResult = static_cast<uint8>(HOUSING_RESULT_GENERIC_FAILURE);
        SendPacket(response.Write());
        return;
    }

    static constexpr uint32 MIN_GUILD_SIZE_FOR_NEIGHBORHOOD = 3;
    static constexpr uint32 MAX_GUILD_SIZE_FOR_NEIGHBORHOOD = 1000;

    if (guild->GetMembersCount() < MIN_GUILD_SIZE_FOR_NEIGHBORHOOD)
    {
        WorldPackets::Housing::HousingSvcsCreateCharterNeighborhoodResponse response;
        response.TrailingResult = static_cast<uint8>(HOUSING_RESULT_GENERIC_FAILURE);
        SendPacket(response.Write());
        TC_LOG_INFO("housing", "CMSG_HOUSING_SVCS_GUILD_CREATE_NEIGHBORHOOD: Guild too small ({} < {})",
            guild->GetMembersCount(), MIN_GUILD_SIZE_FOR_NEIGHBORHOOD);
        return;
    }

    if (guild->GetMembersCount() > MAX_GUILD_SIZE_FOR_NEIGHBORHOOD)
    {
        WorldPackets::Housing::HousingSvcsCreateCharterNeighborhoodResponse response;
        response.TrailingResult = static_cast<uint8>(HOUSING_RESULT_GENERIC_FAILURE);
        SendPacket(response.Write());
        TC_LOG_INFO("housing", "CMSG_HOUSING_SVCS_GUILD_CREATE_NEIGHBORHOOD: Guild too large ({} > {})",
            guild->GetMembersCount(), MAX_GUILD_SIZE_FOR_NEIGHBORHOOD);
        return;
    }

    // Per binary RE (see HousingPackets.h), the second numeric field on the wire is
    // SecondaryID (likely HouseStyle/Theme ID) and not a faction ID. Derive the
    // faction restriction from the player's team instead.
    Neighborhood* neighborhood = sNeighborhoodMgr.CreateGuildNeighborhood(
        player->GetGUID(), housingSvcsGuildCreateNeighborhood.NeighborhoodName,
        housingSvcsGuildCreateNeighborhood.NeighborhoodTypeID,
        player->GetTeam());

    WorldPackets::Housing::HousingSvcsCreateCharterNeighborhoodResponse response;
    response.TrailingResult = static_cast<uint8>(neighborhood ? HOUSING_RESULT_SUCCESS : HOUSING_RESULT_GENERIC_FAILURE);
    if (neighborhood)
    {
        response.Neighborhood.NeighborhoodGUID = neighborhood->GetGuid();
        response.Neighborhood.OwnerGUID = player->GetGUID();
        response.Neighborhood.Name = housingSvcsGuildCreateNeighborhood.NeighborhoodName;
    }
    SendPacket(response.Write());

    // Send guild notification to all guild members
    if (neighborhood)
    {
        if (Guild* guild = sGuildMgr->GetGuildById(player->GetGuildId()))
        {
            WorldPackets::Housing::HousingSvcsGuildCreateNeighborhoodNotification notification;
            notification.NeighborhoodGuid = neighborhood->GetGuid();
            notification.Name = housingSvcsGuildCreateNeighborhood.NeighborhoodName;
            guild->BroadcastPacket(notification.Write());
        }
    }

    TC_LOG_INFO("housing", "CMSG_HOUSING_SVCS_GUILD_CREATE_NEIGHBORHOOD Name: {}, Result: {}",
        housingSvcsGuildCreateNeighborhood.NeighborhoodName, neighborhood ? "success" : "failed");
}

void WorldSession::HandleHousingSvcsNeighborhoodReservePlot(WorldPackets::Housing::HousingSvcsNeighborhoodReservePlot const& housingSvcsNeighborhoodReservePlot)
{
    Player* player = GetPlayer();
    if (!player)
        return;

    // Reservation = a 5-minute hold on a plot. Per retail behavior:
    //   - any player can reserve a plot (even if they already own a house elsewhere)
    //   - reservation just blocks OTHER players from buying/reserving for 5 minutes
    //   - the actual purchase/move is a separate action via the cornerstone UI
    //     (CMSG_NEIGHBORHOOD_BUY_HOUSE / CMSG_NEIGHBORHOOD_MOVE_HOUSE)
    //
    // Earlier TC implementation called Neighborhood::PurchasePlot here, which
    // permanently assigned the plot AND created a Housing object — the wrong
    // semantics for a reservation. The whole buy-side flow (Housing creation,
    // starter-decor placement, plot spawn, guild notification, kill credit,
    // CURRENT_HOUSE_INFO refresh, spell cast) belongs in HandleNeighborhoodBuyHouse,
    // not here.

    if (!sWorld->getBoolConfig(CONFIG_HOUSING_ENABLE_BUY_HOUSE))
    {
        WorldPackets::Housing::HousingSvcsNeighborhoodReservePlotResponse response;
        response.Result = static_cast<uint8>(HOUSING_RESULT_SERVICE_NOT_AVAILABLE);
        SendPacket(response.Write());
        return;
    }

    uint32 housingWarnings = ShouldShowHousingWarning(player);
    if (housingWarnings != HOUSING_WARNING_NONE)
    {
        HousingResult failReason = HOUSING_RESULT_GENERIC_FAILURE;
        if (housingWarnings & HOUSING_WARNING_EXPANSION_REQUIRED)
            failReason = HOUSING_RESULT_MISSING_EXPANSION_ACCESS;

        WorldPackets::Housing::HousingSvcsNeighborhoodReservePlotResponse response;
        response.Result = static_cast<uint8>(failReason);
        SendPacket(response.Write());
        return;
    }

    Neighborhood* neighborhood = sNeighborhoodMgr.ResolveNeighborhood(housingSvcsNeighborhoodReservePlot.NeighborhoodGuid, player);
    if (!neighborhood)
    {
        WorldPackets::Housing::HousingSvcsNeighborhoodReservePlotResponse response;
        response.Result = static_cast<uint8>(HOUSING_RESULT_NEIGHBORHOOD_NOT_FOUND);
        SendPacket(response.Write());
        return;
    }

    uint8 plotIndex = housingSvcsNeighborhoodReservePlot.PlotIndex;

    TC_LOG_INFO("housing", "CMSG_HOUSING_SVCS_NEIGHBORHOOD_RESERVE_PLOT PlotIndex={} NeighborhoodGuid={}",
        plotIndex, housingSvcsNeighborhoodReservePlot.NeighborhoodGuid.ToString());

    // ReservePlot returns false when the plot is already permanently occupied
    // OR currently reserved by someone else. Map both to clear error codes.
    HousingResult result;
    if (plotIndex >= MAX_NEIGHBORHOOD_PLOTS)
        result = HOUSING_RESULT_PLOT_NOT_FOUND;
    else if (neighborhood->GetPlots()[plotIndex].IsOccupied())
        result = HOUSING_RESULT_PLOT_NOT_VACANT;
    else if (!neighborhood->ReservePlot(player->GetGUID(), plotIndex))
        result = HOUSING_RESULT_PLOT_RESERVATION_COOLDOWN;
    else
        result = HOUSING_RESULT_SUCCESS;

    WorldPackets::Housing::HousingSvcsNeighborhoodReservePlotResponse response;
    response.Result = static_cast<uint8>(result);
    SendPacket(response.Write());

    TC_LOG_INFO("housing", "CMSG_HOUSING_SVCS_NEIGHBORHOOD_RESERVE_PLOT PlotIndex: {}, Result: {}",
        plotIndex, uint32(result));
}

void WorldSession::HandleHousingSvcsRelinquishHouse(WorldPackets::Housing::HousingSvcsRelinquishHouse const& /*housingSvcsRelinquishHouse*/)
{
    Player* player = GetPlayer();
    if (!player)
        return;

    if (!sWorld->getBoolConfig(CONFIG_HOUSING_ENABLE_DELETE_HOUSE))
    {
        WorldPackets::Housing::HousingSvcsRelinquishHouseResponse response;
        response.Result = static_cast<uint8>(HOUSING_RESULT_SERVICE_NOT_AVAILABLE);
        SendPacket(response.Write());
        return;
    }

    Housing* housing = player->GetHousing();
    if (!housing)
    {
        WorldPackets::Housing::HousingSvcsRelinquishHouseResponse response;
        response.Result = static_cast<uint8>(HOUSING_RESULT_HOUSE_NOT_FOUND);
        SendPacket(response.Write());
        return;
    }

    // Capture data BEFORE deletion
    ObjectGuid houseGuid = housing->GetHouseGuid();
    ObjectGuid neighborhoodGuid = housing->GetNeighborhoodGuid();
    uint8 plotIndex = INVALID_PLOT_INDEX;

    Neighborhood* neighborhood = sNeighborhoodMgr.GetNeighborhood(neighborhoodGuid);
    if (neighborhood)
    {
        // Find the player's plot index
        Neighborhood::Member const* member = neighborhood->GetMember(player->GetGUID());
        if (member)
            plotIndex = member->PlotIndex;
    }

    // Step 1: Despawn all entities on the map BEFORE deleting housing data
    if (plotIndex != INVALID_PLOT_INDEX)
    {
        if (HousingMap* housingMap = dynamic_cast<HousingMap*>(player->GetMap()))
        {
            housingMap->DespawnAllDecorForPlot(plotIndex);
            housingMap->DespawnAllMeshObjectsForPlot(plotIndex);
            housingMap->DespawnRoomForPlot(plotIndex);
            housingMap->DespawnHouseForPlot(plotIndex);
            housingMap->SetPlotOwnershipState(plotIndex, false);
        }
    }

    // Step 2: Remove from neighborhood membership (evicts from plots array)
    if (neighborhood)
        neighborhood->EvictPlayer(player->GetGUID());

    // Step 3: Delete Housing object (removes from player and DB)
    player->DeleteHousing(neighborhoodGuid);

    // Step 4: Send response
    WorldPackets::Housing::HousingSvcsRelinquishHouseResponse response;
    response.Result = static_cast<uint8>(HOUSING_RESULT_SUCCESS);
    response.HouseGuid = houseGuid;
    SendPacket(response.Write());

    // Step 5: Request client to reload housing data
    WorldPackets::Housing::HousingSvcRequestPlayerReloadData reloadData;
    SendPacket(reloadData.Write());

    // Step 6: Broadcast roster update to remaining members and refresh mirror data
    if (neighborhood)
    {
        WorldPackets::Neighborhood::NeighborhoodRosterResidentUpdate rosterUpdate;
        rosterUpdate.Residents.push_back({ player->GetGUID(), 2 /*Removed*/, false });
        neighborhood->BroadcastPacket(rosterUpdate.Write(), player->GetGUID());

        neighborhood->RefreshMirrorDataForOnlineMembers();
    }

    // Step 7: Send guild notification for house removal
    if (!houseGuid.IsEmpty())
    {
        if (Guild* guild = sGuildMgr->GetGuildById(player->GetGuildId()))
        {
            WorldPackets::Housing::HousingSvcsGuildRemoveHouseNotification notification;
            notification.House.HouseGUID = houseGuid;
            notification.House.OwnerGUID = player->GetGUID();
            guild->BroadcastPacket(notification.Write());
        }
    }

    TC_LOG_INFO("housing", "CMSG_HOUSING_SVCS_RELINQUISH_HOUSE: Player {} relinquished house {} on plot {} in neighborhood {}",
        player->GetGUID().ToString(), houseGuid.ToString(), plotIndex, neighborhoodGuid.ToString());
}

void WorldSession::HandleHousingSvcsUpdateHouseSettings(WorldPackets::Housing::HousingSvcsUpdateHouseSettings const& housingSvcsUpdateHouseSettings)
{
    Player* player = GetPlayer();
    if (!player)
        return;

    Housing* housing = player->GetHousing();
    if (!housing)
    {
        WorldPackets::Housing::HousingSvcsUpdateHouseSettingsResponse response;
        response.Result = static_cast<uint8>(HOUSING_RESULT_HOUSE_NOT_FOUND);
        SendPacket(response.Write());
        return;
    }

    // Ownership check — only the house owner can change settings
    if (housingSvcsUpdateHouseSettings.HouseGuid != housing->GetHouseGuid())
    {
        WorldPackets::Housing::HousingSvcsUpdateHouseSettingsResponse response;
        response.Result = static_cast<uint8>(HOUSING_RESULT_PERMISSION_DENIED);
        response.House.HouseGUID = housingSvcsUpdateHouseSettings.HouseGuid;
        response.House.OwnerGUID = player->GetGUID();
        response.House.HouseLevel = static_cast<uint8>(housing->GetLevel());
        response.House.PlotIndex = housing->GetPlotIndex();
        SendPacket(response.Write());
        return;
    }

    if (housingSvcsUpdateHouseSettings.PlotSettingsID)
    {
        uint32 newFlags = *housingSvcsUpdateHouseSettings.PlotSettingsID & HOUSE_SETTING_VALID_MASK;
        housing->SaveSettings(newFlags);
    }

    WorldPackets::Housing::HousingSvcsUpdateHouseSettingsResponse response;
    response.Result = static_cast<uint8>(HOUSING_RESULT_SUCCESS);
    response.House.HouseGUID = housingSvcsUpdateHouseSettings.HouseGuid;
    response.House.OwnerGUID = player->GetGUID();
    response.House.NeighborhoodGUID = housing->GetNeighborhoodGuid();
    response.House.HouseLevel = static_cast<uint8>(housing->GetLevel());
    response.House.PlotIndex = housing->GetPlotIndex();
    response.SettingsFlags = housing->GetSettingsFlags();
    SendPacket(response.Write());

    // Settings changes (visibility, permissions) require house finder data refresh
    WorldPackets::Housing::HousingSvcsHouseFinderForceRefresh forceRefresh;
    SendPacket(forceRefresh.Write());

    // Broadcast updated house info to other players on the same map so they see the new AccessFlags
    HousingMap* housingMap = dynamic_cast<HousingMap*>(player->GetMap());
    if (housingMap)
    {
        WorldPackets::Housing::HousingGetCurrentHouseInfoResponse houseInfoUpdate;
        houseInfoUpdate.House.HouseGuid = housing->GetHouseGuid();
        houseInfoUpdate.House.OwnerGuid = player->GetGUID();
        houseInfoUpdate.House.NeighborhoodGuid = housing->GetNeighborhoodGuid();
        houseInfoUpdate.House.PlotId = housing->GetPlotIndex();
        houseInfoUpdate.House.AccessFlags = housing->GetSettingsFlags();
        houseInfoUpdate.Result = 0;
        WorldPacket const* updatePkt = houseInfoUpdate.Write();

        Map::PlayerList const& players = housingMap->GetPlayers();
        for (auto const& pair : players)
        {
            if (Player* otherPlayer = pair.GetSource())
            {
                if (otherPlayer != player)
                    otherPlayer->SendDirectMessage(updatePkt);
            }
        }
    }

    TC_LOG_INFO("housing", "CMSG_HOUSING_SVCS_UPDATE_HOUSE_SETTINGS HouseGuid: {} NewFlags: 0x{:03X}",
        housingSvcsUpdateHouseSettings.HouseGuid.ToString(), housing->GetSettingsFlags());
}

void WorldSession::HandleHousingSvcsPlayerViewHousesByPlayer(WorldPackets::Housing::HousingSvcsPlayerViewHousesByPlayer const& housingSvcsPlayerViewHousesByPlayer)
{
    Player* player = GetPlayer();
    if (!player)
        return;

    // Look up neighborhoods the target player belongs to and return their houses
    std::vector<Neighborhood*> neighborhoods = sNeighborhoodMgr.GetNeighborhoodsForPlayer(housingSvcsPlayerViewHousesByPlayer.PlayerGuid);

    WorldPackets::Housing::HousingSvcsPlayerViewHousesResponse response;
    response.Result = static_cast<uint8>(HOUSING_RESULT_SUCCESS);
    for (Neighborhood const* neighborhood : neighborhoods)
    {
        for (auto const& plot : neighborhood->GetPlots())
        {
            if (!plot.IsOccupied() || plot.OwnerGuid != housingSvcsPlayerViewHousesByPlayer.PlayerGuid)
                continue;
            WorldPackets::Housing::JamCliHouse house;
            house.OwnerGUID = plot.OwnerGuid;
            house.HouseGUID = plot.HouseGuid;
            house.NeighborhoodGUID = neighborhood->GetGuid();
            house.PlotIndex = plot.PlotIndex;
            response.Houses.push_back(std::move(house));
        }
    }
    SendPacket(response.Write());

    TC_LOG_INFO("housing", "CMSG_HOUSING_SVCS_PLAYER_VIEW_HOUSES_BY_PLAYER PlayerGuid: {}, FoundHouses: {}",
        housingSvcsPlayerViewHousesByPlayer.PlayerGuid.ToString(), uint32(response.Houses.size()));
}

void WorldSession::HandleHousingSvcsPlayerViewHousesByBnetAccount(WorldPackets::Housing::HousingSvcsPlayerViewHousesByBnetAccount const& housingSvcsPlayerViewHousesByBnetAccount)
{
    Player* player = GetPlayer();
    if (!player)
        return;

    // Find all neighborhoods where the queried BNet account has a plot (owns a house)
    std::vector<Neighborhood*> neighborhoods = sNeighborhoodMgr.GetNeighborhoodsByBnetAccount(housingSvcsPlayerViewHousesByBnetAccount.BnetAccountGuid);

    WorldPackets::Housing::HousingSvcsPlayerViewHousesResponse response;
    response.Result = static_cast<uint8>(HOUSING_RESULT_SUCCESS);
    for (Neighborhood const* neighborhood : neighborhoods)
    {
        for (auto const& plot : neighborhood->GetPlots())
        {
            if (!plot.IsOccupied())
                continue;
            WorldPackets::Housing::JamCliHouse house;
            house.OwnerGUID = plot.OwnerGuid;
            house.HouseGUID = plot.HouseGuid;
            house.NeighborhoodGUID = neighborhood->GetGuid();
            house.PlotIndex = plot.PlotIndex;
            response.Houses.push_back(std::move(house));
        }
    }
    SendPacket(response.Write());

    TC_LOG_INFO("housing", "CMSG_HOUSING_SVCS_PLAYER_VIEW_HOUSES_BY_BNET_ACCOUNT BnetAccountGuid: {}, FoundHouses: {}",
        housingSvcsPlayerViewHousesByBnetAccount.BnetAccountGuid.ToString(), uint32(response.Houses.size()));
}

void WorldSession::HandleHousingSvcsGetPlayerHousesInfo(WorldPackets::Housing::HousingSvcsGetPlayerHousesInfo const& /*housingSvcsGetPlayerHousesInfo*/)
{
    Player* player = GetPlayer();
    if (!player)
        return;

    TC_LOG_INFO("housing", ">>> CMSG_HOUSING_SVCS_GET_PLAYER_HOUSES_INFO received (Player: {})", player->GetGUID().ToString());

    WorldPackets::Housing::HousingSvcsGetPlayerHousesInfoResponse response;
    for (Housing const* housing : player->GetAllHousings())
    {
        WorldPackets::Housing::JamCliHouse house;
        house.OwnerGUID = player->GetGUID();
        house.HouseGUID = housing->GetHouseGuid();
        house.NeighborhoodGUID = housing->GetNeighborhoodGuid();
        house.HouseLevel = static_cast<uint8>(housing->GetLevel());
        house.PlotIndex = housing->GetPlotIndex();
        response.Houses.push_back(house);
    }
    for (auto const& h : response.Houses)
    {
        TC_LOG_ERROR("network", "  [DASHBOARD] JamCliHouse: Owner={} House={} Neighborhood={} Level={} Plot={}",
            h.OwnerGUID.ToString(), h.HouseGUID.ToString(), h.NeighborhoodGUID.ToString(),
            h.HouseLevel, h.PlotIndex);
    }

    WorldPacket const* pkt = response.Write();

    // Hex dump for debugging dashboard display issue
    {
        std::string hex;
        for (size_t i = 0; i < pkt->size() && i < 120; ++i)
        {
            char buf[4];
            snprintf(buf, sizeof(buf), "%02X ", (*pkt)[i]);
            hex += buf;
        }
        TC_LOG_ERROR("network", "SMSG_HOUSING_SVCS_GET_PLAYER_HOUSES_INFO_RESPONSE ({} bytes): {}", pkt->size(), hex);
    }

    SendPacket(pkt);

    TC_LOG_INFO("housing", "SMSG_HOUSING_SVCS_GET_PLAYER_HOUSES_INFO_RESPONSE sent (HouseCount: {})",
        uint32(response.Houses.size()));
}

void WorldSession::HandleHousingSvcsTeleportToPlot(WorldPackets::Housing::HousingSvcsTeleportToPlot const& housingSvcsTeleportToPlot)
{
    Player* player = GetPlayer();
    if (!player)
        return;

    // If the player is currently inside a house interior, this dashboard teleport
    // is an alternative exit path. The interior-door exit handler (HandleHouseInterior
    // LeaveHouse) emits SMSG_HOUSE_INTERIOR_LEAVE_HOUSE_RESPONSE + a 0xC0 HOUSE_STATUS
    // before teleporting, which flips client-side UI state out of "inside" context.
    // User-observed: exit via door keeps the neighborhood map pins correct; exit via
    // this dashboard handler (without those packets) leaves the map showing wrong
    // ownership. Match the protocol so every interior-exit path looks identical on
    // the wire.
    if (dynamic_cast<HouseInteriorMap*>(player->GetMap()))
    {
        if (Housing* interiorHousing = player->GetHousing())
        {
            interiorHousing->SetEditorMode(HOUSING_EDITOR_MODE_NONE);
            interiorHousing->SetInInterior(false);

            // 12.0.5: SMSG_HOUSE_INTERIOR_LEAVE_HOUSE_RESPONSE gone. Clear
            // PlayerHouseInfoComponent.CurrentHouse so the client fires its
            // house-exit callback via the UpdateField-change mechanism.
            player->SetCurrentHouse(ObjectGuid::Empty);

            WorldPackets::Housing::HousingHouseStatusResponse statusResponse;
            statusResponse.HouseGuid = interiorHousing->GetHouseGuid();
            statusResponse.AccountGuid = GetBattlenetAccountGUID();
            statusResponse.OwnerPlayerGuid = player->GetGUID();
            statusResponse.NeighborhoodGuid = interiorHousing->GetNeighborhoodGuid();
            statusResponse.Status = 0;
            statusResponse.PermissionFlags = 0xC0;
            SendPacket(statusResponse.Write());
        }
    }

    Neighborhood* neighborhood = sNeighborhoodMgr.ResolveNeighborhood(housingSvcsTeleportToPlot.NeighborhoodGuid, player);
    if (!neighborhood)
    {
        WorldPackets::Housing::HousingSvcsNotifyPermissionsFailure response;
        response.FailureType = static_cast<uint8>(HOUSING_RESULT_NEIGHBORHOOD_NOT_FOUND);
        SendPacket(response.Write());
        return;
    }

    // Access check: verify the player has permission to visit this neighborhood
    // Owner/member always allowed; non-members must check house settings
    if (!neighborhood->IsMember(player->GetGUID()))
    {
        // Non-member: check if the neighborhood is public
        if (!neighborhood->IsPublic())
        {
            WorldPackets::Housing::HousingSvcsNotifyPermissionsFailure response;
            response.FailureType = static_cast<uint8>(HOUSING_RESULT_PERMISSION_DENIED);
            SendPacket(response.Write());
            TC_LOG_DEBUG("housing", "HandleHousingSvcsTeleportToPlot: Player {} denied access to private neighborhood {}",
                player->GetGUID().ToString(), neighborhood->GetGuid().ToString());
            return;
        }
    }

    // The client sends the DB2 NeighborhoodPlot.PlotIndex directly (0-54 per map).
    // Confirmed via IDA decompilation: C_Housing.TeleportHome() passes plotID from
    // PushHouseFinderPlotInfo which reads the DB2 PlotIndex field. Sniff-verified:
    // PlotIndex=41 in CMSG matches DB2 entry for NeighborhoodMapID=1.
    uint32 plotIndex = housingSvcsTeleportToPlot.PlotIndex;

    TC_LOG_INFO("housing", "HandleHousingSvcsTeleportToPlot: PlotIndex={} OwnerGuid={} TeleportType={} NeighborhoodGUID={}",
        plotIndex, housingSvcsTeleportToPlot.OwnerGuid.ToString(), housingSvcsTeleportToPlot.TeleportType,
        housingSvcsTeleportToPlot.NeighborhoodGuid.ToString());

    // Look up the neighborhood map data for map ID and plot positions
    NeighborhoodMapData const* mapData = sHousingMgr.GetNeighborhoodMapData(neighborhood->GetNeighborhoodMapID());
    if (!mapData)
    {
        WorldPackets::Housing::HousingSvcsNotifyPermissionsFailure response;
        response.FailureType = static_cast<uint8>(HOUSING_RESULT_PLOT_NOT_FOUND);
        SendPacket(response.Write());
        return;
    }

    // Find the DB2 plot matching the client's PlotIndex
    std::vector<NeighborhoodPlotData const*> plots = sHousingMgr.GetPlotsForMap(neighborhood->GetNeighborhoodMapID());
    NeighborhoodPlotData const* targetPlot = nullptr;
    for (NeighborhoodPlotData const* plot : plots)
    {
        if (plot->PlotIndex == static_cast<int32>(plotIndex))
        {
            targetPlot = plot;
            break;
        }
    }

    if (targetPlot)
    {
        // Per-house access check: verify visitor has permission to access this plot.
        // Owner can be offline — fall back to the persisted plotInfo->HouseSettingsFlags
        // (mirrored from character_housing.settingsFlags at neighborhood preload).
        if (!neighborhood->IsMember(player->GetGUID()))
        {
            Neighborhood::PlotInfo const* plotInfo = neighborhood->GetPlotInfo(static_cast<uint8>(plotIndex));
            if (plotInfo && plotInfo->IsOccupied())
            {
                Player* ownerPlayer = ObjectAccessor::FindPlayer(plotInfo->OwnerGuid);
                uint32 settingsFlags = (ownerPlayer && ownerPlayer->GetHousing())
                    ? ownerPlayer->GetHousing()->GetSettingsFlags()
                    : plotInfo->HouseSettingsFlags;

                if (!sHousingMgr.CanVisitorAccessPlot(player, plotInfo->OwnerGuid, settingsFlags, false))
                {
                    WorldPackets::Housing::HousingSvcsNotifyPermissionsFailure denied;
                    denied.FailureType = static_cast<uint8>(HOUSING_RESULT_PERMISSION_DENIED);
                    SendPacket(denied.Write());
                    TC_LOG_DEBUG("housing", "HandleHousingSvcsTeleportToPlot: Player {} denied access to plot {} (settingsFlags=0x{:X})",
                        player->GetGUID().ToString(), plotIndex, settingsFlags);
                    return;
                }
            }
        }

        player->TeleportTo(mapData->MapID, targetPlot->TeleportPosition[0], targetPlot->TeleportPosition[1],
            targetPlot->TeleportPosition[2], 0.0f);

        TC_LOG_INFO("housing", "CMSG_HOUSING_SVCS_TELEPORT_TO_PLOT: Teleporting player {} to plot {} on map {}",
            player->GetGUID().ToString(), plotIndex, mapData->MapID);
    }
    else
    {
        player->TeleportTo(mapData->MapID, mapData->Origin[0], mapData->Origin[1],
            mapData->Origin[2], 0.0f);

        TC_LOG_INFO("housing", "CMSG_HOUSING_SVCS_TELEPORT_TO_PLOT: Plot {} not found in DB2, teleporting to neighborhood origin on map {}",
            plotIndex, mapData->MapID);
    }
}

void WorldSession::HandleHousingSvcsStartTutorial(WorldPackets::Housing::HousingSvcsStartTutorial const& /*housingSvcsStartTutorial*/)
{
    Player* player = GetPlayer();
    if (!player)
        return;

    // Housing warning gate — check expansion access, level requirements
    uint32 housingWarnings = ShouldShowHousingWarning(player);
    if (housingWarnings != HOUSING_WARNING_NONE)
    {
        HousingResult failReason = HOUSING_RESULT_GENERIC_FAILURE;
        if (housingWarnings & HOUSING_WARNING_EXPANSION_REQUIRED)
            failReason = HOUSING_RESULT_MISSING_EXPANSION_ACCESS;

        WorldPackets::Housing::HousingSvcsNotifyPermissionsFailure failResponse;
        failResponse.FailureType = static_cast<uint8>(failReason);
        SendPacket(failResponse.Write());

        TC_LOG_DEBUG("housing", "CMSG_HOUSING_SVCS_START_TUTORIAL: Player {} blocked by housing warning (flags=0x{:X})",
            player->GetGUID().ToString(), housingWarnings);
        return;
    }

    if (!sWorld->getBoolConfig(CONFIG_HOUSING_TUTORIALS_ENABLED))
    {
        WorldPackets::Housing::HousingSvcsNotifyPermissionsFailure failResponse;
        failResponse.FailureType = static_cast<uint8>(HOUSING_RESULT_SERVICE_NOT_AVAILABLE);
        SendPacket(failResponse.Write());
        return;
    }

    // Step 1: Find or create a tutorial neighborhood for the player's faction.
    // The tutorial only needs a neighborhood to exist so the map instance can be
    // created. It does NOT grant membership — that happens when the player buys a plot.
    Neighborhood* neighborhood = sNeighborhoodMgr.FindOrCreatePublicNeighborhood(player->GetTeam());

    if (neighborhood)
    {
        TC_LOG_INFO("housing", "CMSG_HOUSING_SVCS_START_TUTORIAL: Player {} assigned to neighborhood '{}' ({})",
            player->GetGUID().ToString(), neighborhood->GetName(), neighborhood->GetGuid().ToString());

        // Send empty house status — the player has no house yet during tutorial.
        // HouseStatus=1 would tell the client "you own a house" which prevents
        // the Cornerstone purchase UI from showing. Neighborhood context is
        // provided separately via SMSG_HOUSING_GET_CURRENT_HOUSE_INFO_RESPONSE
        // when the player enters the HousingMap.
        WorldPackets::Housing::HousingHouseStatusResponse statusResponse;
        SendPacket(statusResponse.Write());
    }
    else
    {
        TC_LOG_ERROR("housing", "CMSG_HOUSING_SVCS_START_TUTORIAL: Failed to find/create tutorial neighborhood for player {}",
            player->GetGUID().ToString());

        // Notify client of failure
        WorldPackets::Housing::HousingSvcsNotifyPermissionsFailure failResponse;
        failResponse.FailureType = static_cast<uint8>(HOUSING_RESULT_NEIGHBORHOOD_NOT_FOUND);
        SendPacket(failResponse.Write());
        return;
    }

    // Step 2: Auto-accept the "My First Home" quest (91863) so the player can
    // progress through the tutorial by interacting with the steward NPC.
    // Skip if already completed (account-wide warband quest) or already in quest log.
    static constexpr uint32 QUEST_MY_FIRST_HOME = 91863;
    if (Quest const* quest = sObjectMgr->GetQuestTemplate(QUEST_MY_FIRST_HOME))
    {
        QuestStatus status = player->GetQuestStatus(QUEST_MY_FIRST_HOME);
        if (status == QUEST_STATUS_NONE)
        {
            // Quest not in log and not yet rewarded — safe to add
            if (player->CanAddQuest(quest, true))
            {
                player->AddQuestAndCheckCompletion(quest, nullptr);
                TC_LOG_INFO("housing", "CMSG_HOUSING_SVCS_START_TUTORIAL: Auto-accepted quest {} for player {}",
                    QUEST_MY_FIRST_HOME, player->GetGUID().ToString());
            }
        }
        else if (status == QUEST_STATUS_REWARDED)
        {
            TC_LOG_DEBUG("housing", "CMSG_HOUSING_SVCS_START_TUTORIAL: Quest {} already completed (warband) for player {}, skipping",
                QUEST_MY_FIRST_HOME, player->GetGUID().ToString());
        }
        else
        {
            TC_LOG_DEBUG("housing", "CMSG_HOUSING_SVCS_START_TUTORIAL: Quest {} already in log (status {}) for player {}, skipping",
                QUEST_MY_FIRST_HOME, uint32(status), player->GetGUID().ToString());
        }
    }

    // Step 3: Teleport the player to the housing neighborhood via faction-specific spell.
    // Alliance: 1258476 -> Founder's Point (map 2735)
    // Horde:    1258484 -> Razorwind Shores (map 2736)
    static constexpr uint32 SPELL_HOUSING_TUTORIAL_ALLIANCE = 1258476;
    static constexpr uint32 SPELL_HOUSING_TUTORIAL_HORDE    = 1258484;

    uint32 spellId = player->GetTeam() == HORDE
        ? SPELL_HOUSING_TUTORIAL_HORDE
        : SPELL_HOUSING_TUTORIAL_ALLIANCE;

    player->CastSpell(player, spellId, false);

    TC_LOG_INFO("housing", "CMSG_HOUSING_SVCS_START_TUTORIAL: Player {} ({}) casting tutorial spell {}",
        player->GetGUID().ToString(), player->GetTeam() == HORDE ? "Horde" : "Alliance", spellId);
}

// Removed 2026-04-24: HandleHousingSvcsSetTutorialState / CompleteTutorialStep /
// SkipTutorial / QueryPendingInvites — no matching C_Housing Lua API in 12.0.5
// (IDA-verified: only StartTutorial is real). The tutorial quest 91863 completion
// is handled by the normal quest reward path when the player finishes the quest.

void WorldSession::HandleHousingDecorConfirmPreviewPlacement(WorldPackets::Housing::HousingDecorConfirmPreviewPlacement const& housingDecorConfirmPreviewPlacement)
{
    Player* player = GetPlayer();
    if (!player)
        return;

    TC_LOG_DEBUG("housing", "CMSG_HOUSING_DECOR_CONFIRM_PREVIEW_PLACEMENT Player: {} DecorGuid: {}",
        player->GetGUID().ToString(), housingDecorConfirmPreviewPlacement.DecorGuid.ToString());

    // Preview placement confirmation is an ACK from the client after
    // receiving HousingDecorPlacementPreviewResponse with no restrictions.
    // The actual placement is committed via PlaceDecor/MoveDecor CMSGs —
    // this opcode just signals the client is proceeding with the placement.
    // No server-side action or response needed.
}

void WorldSession::HandleHousingSvcsAcceptNeighborhoodOwnership(WorldPackets::Housing::HousingSvcsAcceptNeighborhoodOwnership const& housingSvcsAcceptNeighborhoodOwnership)
{
    Player* player = GetPlayer();
    if (!player)
        return;

    Neighborhood* neighborhood = sNeighborhoodMgr.ResolveNeighborhood(housingSvcsAcceptNeighborhoodOwnership.NeighborhoodGuid, player);
    if (!neighborhood)
    {
        WorldPackets::Housing::HousingSvcsAcceptNeighborhoodOwnershipResponse response;
        response.Result = static_cast<uint8>(HOUSING_RESULT_NEIGHBORHOOD_NOT_FOUND);
        SendPacket(response.Write());
        return;
    }

    ObjectGuid previousOwnerGuid = neighborhood->GetOwnerGuid();
    HousingResult result = neighborhood->AcceptOwnershipTransfer(player->GetGUID());

    WorldPackets::Housing::HousingSvcsAcceptNeighborhoodOwnershipResponse response;
    response.Result = static_cast<uint8>(result);
    response.NeighborhoodGuid = housingSvcsAcceptNeighborhoodOwnership.NeighborhoodGuid;
    SendPacket(response.Write());

    if (result == HOUSING_RESULT_SUCCESS)
    {
        // Broadcast ownership transfer to all members
        WorldPackets::Housing::HousingSvcsNeighborhoodOwnershipTransferredResponse transferNotification;
        transferNotification.Result = static_cast<uint8>(result);
        transferNotification.OwnerGUID = player->GetGUID();
        transferNotification.HouseGUID = ObjectGuid::Empty;
        transferNotification.AccountGUID = GetAccountGUID();
        transferNotification.HouseLevel = 0;
        neighborhood->BroadcastPacket(transferNotification.Write(), player->GetGUID());

        // Broadcast roster update with role changes
        WorldPackets::Neighborhood::NeighborhoodRosterResidentUpdate rosterUpdate;
        rosterUpdate.Residents.push_back({ player->GetGUID(), 1 /*RoleChanged*/, true /*new owner = privileged*/ });
        rosterUpdate.Residents.push_back({ previousOwnerGuid, 1 /*RoleChanged*/, true /*demoted to manager, still privileged*/ });
        neighborhood->BroadcastPacket(rosterUpdate.Write());

        // Ownership change is a major data change — request client to reload housing data
        WorldPackets::Housing::HousingSvcRequestPlayerReloadData reloadData;
        SendPacket(reloadData.Write());

        // Previous owner also needs to reload
        if (Player* prevOwner = ObjectAccessor::FindPlayer(previousOwnerGuid))
        {
            WorldPackets::Housing::HousingSvcRequestPlayerReloadData prevReload;
            prevOwner->SendDirectMessage(prevReload.Write());
        }
    }

    TC_LOG_INFO("housing", "CMSG_HOUSING_SVCS_ACCEPT_NEIGHBORHOOD_OWNERSHIP: Result={} NeighborhoodGuid={} PreviousOwner={}",
        uint32(result), housingSvcsAcceptNeighborhoodOwnership.NeighborhoodGuid.ToString(),
        previousOwnerGuid.ToString());
}

void WorldSession::HandleHousingSvcsRejectNeighborhoodOwnership(WorldPackets::Housing::HousingSvcsRejectNeighborhoodOwnership const& housingSvcsRejectNeighborhoodOwnership)
{
    Player* player = GetPlayer();
    if (!player)
        return;

    Neighborhood* neighborhood = sNeighborhoodMgr.ResolveNeighborhood(housingSvcsRejectNeighborhoodOwnership.NeighborhoodGuid, player);
    HousingResult result = HOUSING_RESULT_NEIGHBORHOOD_NOT_FOUND;
    if (neighborhood)
        result = neighborhood->RejectOwnershipTransfer(player->GetGUID());

    WorldPackets::Housing::HousingSvcsRejectNeighborhoodOwnershipResponse response;
    response.Result = static_cast<uint8>(result);
    response.NeighborhoodGuid = housingSvcsRejectNeighborhoodOwnership.NeighborhoodGuid;
    SendPacket(response.Write());

    // Notify the original owner that the transfer was rejected
    if (result == HOUSING_RESULT_SUCCESS && neighborhood)
    {
        if (Player* owner = ObjectAccessor::FindPlayer(neighborhood->GetOwnerGuid()))
        {
            WorldPackets::Housing::HousingSvcsRejectNeighborhoodOwnershipResponse ownerNotify;
            ownerNotify.Result = static_cast<uint8>(HOUSING_RESULT_SUCCESS);
            ownerNotify.NeighborhoodGuid = housingSvcsRejectNeighborhoodOwnership.NeighborhoodGuid;
            owner->SendDirectMessage(ownerNotify.Write());
        }
    }

    TC_LOG_INFO("housing", "CMSG_HOUSING_SVCS_REJECT_NEIGHBORHOOD_OWNERSHIP: Player {} rejected ownership of neighborhood {} (result={})",
        player->GetGUID().ToString(), housingSvcsRejectNeighborhoodOwnership.NeighborhoodGuid.ToString(), uint32(result));
}

void WorldSession::HandleHousingSvcsGetPotentialHouseOwners(WorldPackets::Housing::HousingSvcsGetPotentialHouseOwners const& /*housingSvcsGetPotentialHouseOwners*/)
{
    Player* player = GetPlayer();
    if (!player)
        return;

    // Get the player's neighborhood and return members eligible for ownership
    Housing* housing = player->GetHousing();
    if (!housing)
    {
        WorldPackets::Housing::HousingSvcsGetPotentialHouseOwnersResponse response;
        SendPacket(response.Write()); // empty array — no Result byte in wire format
        return;
    }

    Neighborhood* neighborhood = sNeighborhoodMgr.GetNeighborhood(housing->GetNeighborhoodGuid());
    if (!neighborhood)
    {
        WorldPackets::Housing::HousingSvcsGetPotentialHouseOwnersResponse response;
        SendPacket(response.Write()); // empty array
        return;
    }

    std::vector<Neighborhood::Member> const& members = neighborhood->GetMembers();
    TC_LOG_INFO("housing", "CMSG_HOUSING_SVCS_GET_POTENTIAL_HOUSE_OWNERS: Neighborhood has {} members",
        uint32(members.size()));

    // Sniff-verified format: PlayerName is "<CharacterName>-<RealmNormalizedName>"
    // (cross-realm display name format). Examples from retail sniff:
    //   "Anondk-AltarofStorms", "Dahuntermon-AltarofStorms", "Insanedk-Trollbane",
    //   "Pewpewer-Khadgar", "Insanee-Gul'dan".
    // Falls back to bare character name if the realm record is unavailable.
    std::string realmSuffix;
    if (std::shared_ptr<Realm const> currentRealm = sRealmList->GetCurrentRealm())
        realmSuffix = "-" + currentRealm->NormalizedName;

    WorldPackets::Housing::HousingSvcsGetPotentialHouseOwnersResponse response;
    response.PotentialOwners.reserve(members.size());
    for (auto const& member : members)
    {
        WorldPackets::Housing::HousingSvcsGetPotentialHouseOwnersResponse::PotentialOwnerData ownerData;
        ownerData.PlayerGuid = member.PlayerGuid;
        if (Player* memberPlayer = ObjectAccessor::FindPlayer(member.PlayerGuid))
            ownerData.PlayerName = memberPlayer->GetName() + realmSuffix;
        response.PotentialOwners.push_back(std::move(ownerData));
    }
    SendPacket(response.Write());
}

void WorldSession::HandleHousingSvcsGetHouseFinderInfo(WorldPackets::Housing::HousingSvcsGetHouseFinderInfo const& /*housingSvcsGetHouseFinderInfo*/)
{
    Player* player = GetPlayer();
    if (!player)
        return;

    // Return list of public neighborhoods available through the finder, filtered by faction
    std::vector<Neighborhood*> publicNeighborhoods = sNeighborhoodMgr.GetPublicNeighborhoods();
    uint32 playerTeam = player->GetTeam();

    WorldPackets::Housing::HousingSvcsGetHouseFinderInfoResponse response;
    response.Result = static_cast<uint8>(HOUSING_RESULT_SUCCESS);
    response.Entries.reserve(publicNeighborhoods.size());
    for (Neighborhood* neighborhood : publicNeighborhoods)
    {
        // Faction filter: skip neighborhoods that don't match the player's faction
        int32 factionRestriction = neighborhood->GetFactionRestriction();
        if (factionRestriction != NEIGHBORHOOD_FACTION_NONE)
        {
            if ((factionRestriction == NEIGHBORHOOD_FACTION_HORDE && playerTeam != HORDE) ||
                (factionRestriction == NEIGHBORHOOD_FACTION_ALLIANCE && playerTeam != ALLIANCE))
                continue;
        }

        WorldPackets::Housing::JamCliHouseFinderNeighborhood entry;
        entry.NeighborhoodGUID = neighborhood->GetGuid();
        entry.OwnerGUID = neighborhood->GetOwnerGuid();
        entry.Name = neighborhood->GetName();
        // Field1 | Field2 is a BITMASK of "unavailable" plots (IDA: client ORs them at offset 520,
        // then checks (1LL << plotIndex) & bitmask to determine if plot is occupied on the finder map).
        // Include both permanently-occupied plots AND plots currently held by ANOTHER player's
        // 5-minute reservation, so the user can't keep clicking Reserve on the same plot
        // when someone else has already locked it. The viewer's own reservation stays
        // marked-available so they can still act on it via the cornerstone.
        uint64 occupiedBitmask = 0;
        for (auto const& plot : neighborhood->GetPlots())
        {
            if (plot.IsOccupied() && plot.PlotIndex < 64)
                occupiedBitmask |= (uint64(1) << plot.PlotIndex);
        }
        for (uint8 plotIdx = 0; plotIdx < 64; ++plotIdx)
        {
            if (occupiedBitmask & (uint64(1) << plotIdx))
                continue; // already counted as permanently occupied
            if (!neighborhood->GetPlotReserverOther(plotIdx, player->GetGUID()).IsEmpty())
                occupiedBitmask |= (uint64(1) << plotIdx);
        }
        entry.Field1 = occupiedBitmask;
        entry.Field2 = 0;
        entry.ExtraFlags = 0x20; // Retail sniff: finder list entries always have ExtraFlags=0x20

        // Retail LIST response has an EMPTY Houses array — the client only needs houses in
        // the DETAIL response (HandleHousingSvcsGetHouseFinderNeighborhood). Populating
        // Houses here causes the client to not render occupied plot markers on the finder map.

        response.Entries.push_back(std::move(entry));
    }

    SendPacket(response.Write());

    TC_LOG_INFO("housing", "CMSG_HOUSING_SVCS_GET_HOUSE_FINDER_INFO: player={} team={} total_public={} sent={}",
        player->GetName(), playerTeam, uint32(publicNeighborhoods.size()), uint32(response.Entries.size()));
    for (auto const& entry : response.Entries)
    {
        TC_LOG_INFO("housing", "  FINDER_LIST entry: nbGuid={} owner={} name='{}' occupiedBitmask=0x{:016X} "
            "houses={} extraFlags=0x{:02X}",
            entry.NeighborhoodGUID.ToString(), entry.OwnerGUID.ToString(), entry.Name,
            entry.Field1, uint32(entry.Houses.size()), entry.ExtraFlags);
    }
}

void WorldSession::HandleHousingSvcsGetHouseFinderNeighborhood(WorldPackets::Housing::HousingSvcsGetHouseFinderNeighborhood const& housingSvcsGetHouseFinderNeighborhood)
{
    Player* player = GetPlayer();
    if (!player)
        return;

    Neighborhood const* neighborhood = sNeighborhoodMgr.ResolveNeighborhood(housingSvcsGetHouseFinderNeighborhood.NeighborhoodGuid, player);
    if (!neighborhood)
    {
        WorldPackets::Housing::HousingSvcsGetHouseFinderNeighborhoodResponse response;
        response.Result = static_cast<uint8>(HOUSING_RESULT_NEIGHBORHOOD_NOT_FOUND);
        SendPacket(response.Write());
        return;
    }

    TC_LOG_INFO("housing", "CMSG_HOUSING_SVCS_GET_HOUSE_FINDER_NEIGHBORHOOD: '{}' guid={} MapID:{} Members:{} Public:{} OccupiedPlots:{}",
        neighborhood->GetName(), neighborhood->GetGuid().ToString(),
        neighborhood->GetNeighborhoodMapID(),
        neighborhood->GetMemberCount(), neighborhood->IsPublic(),
        neighborhood->GetOccupiedPlotCount());

    // Dump all plot states for debugging
    for (uint8 i = 0; i < MAX_NEIGHBORHOOD_PLOTS; ++i)
    {
        auto const& plot = neighborhood->GetPlots()[i];
        if (plot.IsOccupied())
        {
            TC_LOG_INFO("housing", "  PLOT[{}]: occupied owner={} house={} bnet={} level={} favor={} name='{}'",
                i, plot.OwnerGuid.ToString(), plot.HouseGuid.ToString(), plot.OwnerBnetGuid.ToString(),
                plot.HouseLevel, plot.HouseFavor, plot.HouseName);
        }
    }

    // Build single JamCliHouseFinderNeighborhood with houses array for occupied plots
    WorldPackets::Housing::HousingSvcsGetHouseFinderNeighborhoodResponse response;
    response.Result = static_cast<uint8>(HOUSING_RESULT_SUCCESS);
    response.Neighborhood.NeighborhoodGUID = neighborhood->GetGuid();
    response.Neighborhood.OwnerGUID = neighborhood->GetOwnerGuid();
    response.Neighborhood.Name = neighborhood->GetName();

    // Field1 | Field2 is a BITMASK of occupied plot indices (IDA: client ORs them at offset 520,
    // then checks (1LL << plotIndex) & bitmask to determine if plot is occupied on the finder map).
    uint64 occupiedBitmask = 0;
    for (auto const& plot : neighborhood->GetPlots())
    {
        if (plot.IsOccupied() && plot.PlotIndex < 64)
            occupiedBitmask |= (uint64(1) << plot.PlotIndex);
    }
    response.Neighborhood.Field1 = occupiedBitmask;
    response.Neighborhood.Field2 = 0;
    response.Neighborhood.ExtraFlags = 0x20; // Retail sniff: finder detail always has ExtraFlags=0x20

    TC_LOG_INFO("housing", "  DETAIL: occupiedBitmask=0x{:016X} occupiedCount={}",
        occupiedBitmask, neighborhood->GetOccupiedPlotCount());

    for (auto const& plot : neighborhood->GetPlots())
    {
        if (!plot.IsOccupied() || plot.OwnerGuid.IsEmpty())
            continue;

        WorldPackets::Housing::JamCliHouse house;
        house.HouseGUID = plot.HouseGuid;
        house.OwnerGUID = plot.OwnerGuid;
        house.NeighborhoodGUID = neighborhood->GetGuid();
        // The client uses the HouseLevel field (struct +48, uint8 on the wire for
        // this path) as the per-plot hash key when it populates its internal
        // JamCliHouse table on the regular neighborhood map. Each entry MUST carry
        // a unique value or the client's hash collides and only one plot icon
        // renders — verified empirically: setting this to the real HouseLevel
        // (which is typically 1 for every plot) made every other plot vanish
        // from the map. PlotIndex (0..54, unique per neighborhood) is the safe
        // choice and is what the pre-regression working state shipped.
        house.HouseLevel = static_cast<uint8>(plot.PlotIndex);
        house.PlotIndex = plot.PlotIndex;
        response.Neighborhood.Houses.push_back(std::move(house));

        TC_LOG_INFO("housing", "  DETAIL_HOUSE: plotIndex={} houseLevel={} houseGuid={} ownerGuid={}",
            plot.PlotIndex, static_cast<uint8>(plot.PlotIndex),
            plot.HouseGuid.ToString(), plot.OwnerGuid.ToString());
    }

    TC_LOG_INFO("housing", "  DETAIL: sending {} houses in Houses[] array", uint32(response.Neighborhood.Houses.size()));
    SendPacket(response.Write());

    // Populate the Housing/4 entity with this neighborhood's mirror data so the
    // client's internal house list stays in sync for plot resolution.
    HousingNeighborhoodMirrorEntity& mirrorEntity = GetHousingNeighborhoodMirrorEntity();
    mirrorEntity.SetName(neighborhood->GetName());
    mirrorEntity.SetOwnerGUID(neighborhood->GetOwnerGuid());

    mirrorEntity.ClearHouses();
    for (auto const& plot : neighborhood->GetPlots())
    {
        if (plot.IsOccupied() && !plot.HouseGuid.IsEmpty())
            mirrorEntity.AddHouse(plot.HouseGuid, plot.OwnerGuid);
        else
            mirrorEntity.AddHouse(ObjectGuid::Empty, ObjectGuid::Empty);
    }

    // Count what we're sending on the mirror entity
    uint32 mirrorOccupied = 0;
    uint32 mirrorEmpty = 0;
    for (auto const& plot : neighborhood->GetPlots())
    {
        if (plot.IsOccupied() && !plot.HouseGuid.IsEmpty())
            ++mirrorOccupied;
        else
            ++mirrorEmpty;
    }
    TC_LOG_INFO("housing", "  MIRROR: sending {} occupied + {} empty = {} total slots",
        mirrorOccupied, mirrorEmpty, mirrorOccupied + mirrorEmpty);

    mirrorEntity.ClearManagers();
    for (auto const& member : neighborhood->GetMembers())
    {
        if (member.Role == NEIGHBORHOOD_ROLE_MANAGER || member.Role == NEIGHBORHOOD_ROLE_OWNER)
        {
            ObjectGuid bnetGuid;
            if (Player* mgr = ObjectAccessor::FindPlayer(member.PlayerGuid))
                bnetGuid = mgr->GetSession()->GetBattlenetAccountGUID();
            mirrorEntity.AddManager(bnetGuid, member.PlayerGuid);
        }
    }
    // Wholesale re-push; retail uses CREATE for this (sniff-verified).
    mirrorEntity.SendCreateToPlayer(player);

    TC_LOG_INFO("housing", "  MIRROR: update sent to player {}", player->GetName());
}

void WorldSession::HandleHousingSvcsGetBnetFriendNeighborhoods(WorldPackets::Housing::HousingSvcsGetBnetFriendNeighborhoods const& housingSvcsGetBnetFriendNeighborhoods)
{
    Player* player = GetPlayer();
    if (!player)
        return;

    PlayerSocial* social = player->GetSocial();
    if (!social)
    {
        WorldPackets::Housing::HousingSvcsGetBnetFriendNeighborhoodsResponse response;
        response.Result = static_cast<uint8>(HOUSING_RESULT_SUCCESS);
        SendPacket(response.Write());
        return;
    }

    // Build response using JamHousingSearchResult format (same as HouseFinderInfo).
    // Iterate all neighborhoods and check if any plot owner is on the player's friend list.
    WorldPackets::Housing::HousingSvcsGetBnetFriendNeighborhoodsResponse response;
    response.Result = static_cast<uint8>(HOUSING_RESULT_SUCCESS);

    std::vector<Neighborhood*> allNeighborhoods = sNeighborhoodMgr.GetAllNeighborhoods();
    for (Neighborhood const* neighborhood : allNeighborhoods)
    {
        bool hasFriend = false;
        for (auto const& plot : neighborhood->GetPlots())
        {
            if (!plot.IsOccupied() || plot.OwnerGuid.IsEmpty())
                continue;

            if (!social->HasFriend(plot.OwnerGuid))
                continue;

            hasFriend = true;
            break;
        }

        if (!hasFriend)
            continue;

        WorldPackets::Housing::JamCliHouseFinderNeighborhood entry;
        entry.NeighborhoodGUID = neighborhood->GetGuid();
        entry.OwnerGUID = neighborhood->GetOwnerGuid();
        entry.Name = neighborhood->GetName();
        auto plotsForMap = sHousingMgr.GetPlotsForMap(neighborhood->GetNeighborhoodMapID());
        uint32 totalPlots = !plotsForMap.empty() ? static_cast<uint32>(plotsForMap.size()) : MAX_NEIGHBORHOOD_PLOTS;
        uint32 availPlots = totalPlots - neighborhood->GetOccupiedPlotCount();
        entry.SetPlotCounts(availPlots, totalPlots);
        entry.Field2 = static_cast<uint64>(neighborhood->GetNeighborhoodMapID());

        for (auto const& plot2 : neighborhood->GetPlots())
        {
            if (!plot2.IsOccupied() || plot2.OwnerGuid.IsEmpty())
                continue;
            WorldPackets::Housing::JamCliHouse house;
            house.HouseGUID = plot2.HouseGuid;
            house.OwnerGUID = plot2.OwnerGuid;
            house.NeighborhoodGUID = neighborhood->GetGuid();
            house.PlotIndex = plot2.PlotIndex;
            entry.Houses.push_back(std::move(house));
        }
        response.Entries.push_back(std::move(entry));
    }

    SendPacket(response.Write());

    TC_LOG_INFO("housing", "CMSG_HOUSING_SVCS_GET_BNET_FRIEND_NEIGHBORHOODS BnetAccountGuid: {}, FriendNeighborhoods: {}",
        housingSvcsGetBnetFriendNeighborhoods.BnetAccountGuid.ToString(), uint32(response.Entries.size()));
}

void WorldSession::HandleHousingSvcsDeleteAllNeighborhoodInvites(WorldPackets::Housing::HousingSvcsDeleteAllNeighborhoodInvites const& /*housingSvcsDeleteAllNeighborhoodInvites*/)
{
    Player* player = GetPlayer();
    if (!player)
        return;

    // Decline all pending neighborhood invitations through the house finder
    // This sets the auto-decline flag so no new invites are received
    player->SetPlayerFlagEx(PLAYER_FLAGS_EX_AUTO_DECLINE_NEIGHBORHOOD);

    WorldPackets::Housing::HousingSvcsDeleteAllNeighborhoodInvitesResponse response;
    response.Result = static_cast<uint8>(HOUSING_RESULT_SUCCESS);
    SendPacket(response.Write());

    TC_LOG_INFO("housing", "CMSG_HOUSING_SVCS_DELETE_ALL_NEIGHBORHOOD_INVITES: Player {} declined all invitations",
        player->GetGUID().ToString());
}

// ============================================================
// Housing Misc
// ============================================================

void WorldSession::HandleHousingHouseStatus(WorldPackets::Housing::HousingHouseStatus const& /*housingHouseStatus*/)
{
    Player* player = GetPlayer();
    if (!player)
        return;

    // CRITICAL TRACE: log that the client polled for housing status.
    // If this line never appears in the log after interior entry, the client's
    // housing system context was never activated (missing 0x56000E init packet).
    bool isInterior = player->GetMap() && dynamic_cast<HouseInteriorMap*>(player->GetMap());
    TC_LOG_ERROR("housing", ">>> CMSG_HOUSING_HOUSE_STATUS: CLIENT POLLED! player={} map={} isInterior={} pos=({:.1f},{:.1f},{:.1f})",
        player->GetGUID().ToString(), player->GetMapId(), isInterior,
        player->GetPositionX(), player->GetPositionY(), player->GetPositionZ());

    WorldPackets::Housing::HousingHouseStatusResponse response;

    // First check if the player is on their own plot (use their Housing data directly)
    Housing* ownHousing = player->GetHousing();

    // Check what plot the player is currently visiting via area trigger tracking.
    // On the interior map (HouseInteriorMap), there's no HousingMap plot tracking — use
    // the player's own plot index directly since they're always inside their own house.
    HousingMap* housingMap = dynamic_cast<HousingMap*>(player->GetMap());
    int8 visitedPlot = -1;
    if (housingMap)
        visitedPlot = housingMap->GetPlayerCurrentPlot(player->GetGUID());
    else if (isInterior && ownHousing)
        visitedPlot = static_cast<int8>(ownHousing->GetPlotIndex());

    if (visitedPlot >= 0 && housingMap && housingMap->GetNeighborhood())
    {
        Neighborhood* neighborhood = housingMap->GetNeighborhood();
        Neighborhood::PlotInfo const* plotInfo = neighborhood->GetPlotInfo(static_cast<uint8>(visitedPlot));

        if (plotInfo && plotInfo->OwnerGuid != player->GetGUID())
        {
            // Visiting someone else's plot — return that plot's house data
            response.HouseGuid = plotInfo->HouseGuid;
            response.AccountGuid = plotInfo->OwnerBnetGuid;
            response.OwnerPlayerGuid = plotInfo->OwnerGuid;
            response.NeighborhoodGuid = neighborhood->GetGuid();
            response.Status = 0;
            response.PermissionFlags = 0x40; // visitor: plot-entry only
        }
        else if (ownHousing)
        {
            response.HouseGuid = ownHousing->GetHouseGuid();
            response.AccountGuid = GetBattlenetAccountGUID();
            response.OwnerPlayerGuid = player->GetGUID();
            response.NeighborhoodGuid = ownHousing->GetNeighborhoodGuid();
            response.Status = 0;
            response.PermissionFlags = 0xE0;
        }
    }
    else if (ownHousing)
    {
        response.HouseGuid = ownHousing->GetHouseGuid();
        response.AccountGuid = GetBattlenetAccountGUID();
        response.OwnerPlayerGuid = player->GetGUID();
        response.NeighborhoodGuid = ownHousing->GetNeighborhoodGuid();
        response.Status = 0;
        response.PermissionFlags = 0xE0;
    }
    WorldPacket const* statusPkt = response.Write();
    TC_LOG_ERROR("housing", ">>> CMSG_HOUSING_HOUSE_STATUS (visitedPlot: {})", visitedPlot);
    TC_LOG_ERROR("housing", "<<< SMSG_HOUSING_HOUSE_STATUS_RESPONSE ({} bytes): {}",
        statusPkt->size(), HexDumpPacket(statusPkt));
    TC_LOG_ERROR("housing", "    HouseGuid={} AccountGuid={} OwnerPlayerGuid={} NeighborhoodGuid={} Status={} Permissions=0x{:02X}",
        response.HouseGuid.ToString(), response.AccountGuid.ToString(),
        response.OwnerPlayerGuid.ToString(), response.NeighborhoodGuid.ToString(),
        response.Status, response.PermissionFlags);
    SendPacket(statusPkt);
}

void WorldSession::HandleHousingGetPlayerPermissions(WorldPackets::Housing::HousingGetPlayerPermissions const& housingGetPlayerPermissions)
{
    Player* player = GetPlayer();
    if (!player)
        return;

    TC_LOG_INFO("housing", ">>> CMSG_HOUSING_GET_PLAYER_PERMISSIONS received (HouseGuid: {})",
        housingGetPlayerPermissions.HouseGuid.has_value() ? housingGetPlayerPermissions.HouseGuid->ToString() : "none");

    Housing* housing = player->GetHousing();

    WorldPackets::Housing::HousingGetPlayerPermissionsResponse response;
    if (housing)
    {
        response.HouseGuid = housing->GetHouseGuid();

        // Client sends the HouseGuid it wants permissions for.
        // If it matches our house, we're the owner.
        ObjectGuid requestedHouseGuid = housingGetPlayerPermissions.HouseGuid.value_or(housing->GetHouseGuid());
        bool isOwner = (requestedHouseGuid == housing->GetHouseGuid());

        if (isOwner)
        {
            // House owner gets full permissions
            // Sniff-verified: owner permissions are 0xE0 (bits 5,6,7)
            response.ResultCode = 0;
            response.PermissionFlags = 0xE0;
        }
        else
        {
            // Visitor on another player's plot — check stored settings
            response.ResultCode = 0;
            response.PermissionFlags = 0x00;

            HousingMap* housingMap = dynamic_cast<HousingMap*>(player->GetMap());
            if (housingMap)
            {
                int8 visitedPlot = housingMap->GetPlayerCurrentPlot(player->GetGUID());
                if (visitedPlot >= 0)
                {
                    Neighborhood* neighborhood = housingMap->GetNeighborhood();
                    if (neighborhood)
                    {
                        Neighborhood::PlotInfo const* plotInfo = neighborhood->GetPlotInfo(static_cast<uint8>(visitedPlot));
                        if (plotInfo && plotInfo->IsOccupied())
                        {
                            Housing* plotHousing = housingMap->GetHousingForPlayer(plotInfo->OwnerGuid);
                            if (plotHousing)
                            {
                                response.HouseGuid = plotHousing->GetHouseGuid();
                                Player* ownerPlayer = ObjectAccessor::FindPlayer(plotInfo->OwnerGuid);
                                bool hasAccess = ownerPlayer && sHousingMgr.CanVisitorAccess(player, ownerPlayer, plotHousing->GetSettingsFlags(), false);
                                response.PermissionFlags = hasAccess ? 0x40 : 0x00;
                            }
                        }
                    }
                }
            }
        }
    }
    else
    {
        response.ResultCode = 0;
        response.PermissionFlags = 0;
    }
    WorldPacket const* permPkt = response.Write();
    TC_LOG_ERROR("housing", "<<< SMSG_HOUSING_GET_PLAYER_PERMISSIONS_RESPONSE ({} bytes): {}",
        permPkt->size(), HexDumpPacket(permPkt));
    TC_LOG_ERROR("housing", "    HouseGuid={} ResultCode={} PermissionFlags=0x{:02X}",
        response.HouseGuid.ToString(), response.ResultCode, response.PermissionFlags);
    SendPacket(permPkt);
}

void WorldSession::HandleHousingGetCurrentHouseInfo(WorldPackets::Housing::HousingGetCurrentHouseInfo const& /*housingGetCurrentHouseInfo*/)
{
    Player* player = GetPlayer();
    if (!player)
        return;

    TC_LOG_INFO("housing", ">>> CMSG_HOUSING_GET_CURRENT_HOUSE_INFO received");

    HousingMap* housingMap = dynamic_cast<HousingMap*>(player->GetMap());
    bool isInterior = player->GetMap() && dynamic_cast<HouseInteriorMap*>(player->GetMap());
    int8 currentPlot = -1;
    if (housingMap)
        currentPlot = housingMap->GetPlayerCurrentPlot(player->GetGUID());
    else if (isInterior)
    {
        if (Housing* housing = player->GetHousing())
            currentPlot = static_cast<int8>(housing->GetPlotIndex());
    }

    WorldPackets::Housing::HousingGetCurrentHouseInfoResponse response;

    if (currentPlot >= 0 && housingMap && housingMap->GetNeighborhood())
    {
        // Player is on a specific plot — return info about THAT plot's house
        Neighborhood* neighborhood = housingMap->GetNeighborhood();
        Neighborhood::PlotInfo const* plotInfo = neighborhood->GetPlotInfo(static_cast<uint8>(currentPlot));

        if (plotInfo && plotInfo->IsOccupied())
        {
            // Find the plot owner's housing data for AccessFlags
            Housing* plotHousing = nullptr;
            if (plotInfo->OwnerGuid == player->GetGUID())
                plotHousing = player->GetHousing();
            else if (Player* ownerPlayer = ObjectAccessor::FindPlayer(plotInfo->OwnerGuid))
                plotHousing = ownerPlayer->GetHousing();

            response.House.HouseGuid = plotInfo->HouseGuid;
            response.House.OwnerGuid = plotInfo->OwnerGuid;
            response.House.NeighborhoodGuid = neighborhood->GetGuid();
            response.House.PlotId = static_cast<uint8>(currentPlot);
            response.House.AccessFlags = plotHousing ? plotHousing->GetSettingsFlags() : HOUSE_SETTING_DEFAULT;
        }
        else
        {
            // On an unoccupied plot
            response.House.OwnerGuid = player->GetGUID();
            response.House.NeighborhoodGuid = neighborhood->GetGuid();
            response.House.PlotId = static_cast<uint8>(currentPlot);
        }
    }
    else if (Housing* housing = player->GetHousing())
    {
        // Not on any tracked plot — fall back to player's own house data
        response.House.HouseGuid = housing->GetHouseGuid();
        response.House.OwnerGuid = player->GetGUID();
        response.House.NeighborhoodGuid = housing->GetNeighborhoodGuid();
        response.House.PlotId = housing->GetPlotIndex();
        response.House.AccessFlags = housing->GetSettingsFlags();
    }
    else if (housingMap)
    {
        // No house, no tracked plot
        response.House.OwnerGuid = player->GetGUID();
        if (Neighborhood* neighborhood = housingMap->GetNeighborhood())
            response.House.NeighborhoodGuid = neighborhood->GetGuid();
    }
    response.Result = 0;
    WorldPacket const* houseInfoPkt = response.Write();
    TC_LOG_ERROR("housing", "<<< SMSG_HOUSING_GET_CURRENT_HOUSE_INFO_RESPONSE ({} bytes) currentPlot={} HouseGuid={} OwnerGuid={} NeighborhoodGuid={} PlotId={} AccessFlags={}",
        houseInfoPkt->size(), currentPlot,
        response.House.HouseGuid.ToString(), response.House.OwnerGuid.ToString(),
        response.House.NeighborhoodGuid.ToString(), response.House.PlotId, response.House.AccessFlags);
    SendPacket(houseInfoPkt);
}

void WorldSession::HandleHousingResetKioskMode(WorldPackets::Housing::HousingResetKioskMode const& /*housingResetKioskMode*/)
{
    Player* player = GetPlayer();
    if (!player)
        return;

    // Delete the context-aware housing (current neighborhood)
    if (Housing const* housing = player->GetHousing())
        player->DeleteHousing(housing->GetNeighborhoodGuid());

    WorldPackets::Housing::HousingResetKioskModeResponse response;
    response.Result = static_cast<uint8>(HOUSING_RESULT_SUCCESS);
    SendPacket(response.Write());

    TC_LOG_INFO("housing", "CMSG_HOUSING_RESET_KIOSK_MODE processed for player {}",
        player->GetGUID().ToString());
}

// ============================================================
// Other Housing CMSG
// ============================================================

void WorldSession::HandleQueryNeighborhoodInfo(WorldPackets::Housing::QueryNeighborhoodInfo const& queryNeighborhoodInfo)
{
    Player* player = GetPlayer();
    if (!player)
        return;

    WorldPackets::Housing::QueryNeighborhoodNameResponse response;

    Neighborhood const* neighborhood = sNeighborhoodMgr.ResolveNeighborhood(queryNeighborhoodInfo.NeighborhoodGuid, player);
    if (neighborhood)
    {
        // Use the canonical neighborhood GUID, not the client's (which may be a GO GUID or empty)
        response.NeighborhoodGuid = neighborhood->GetGuid();
        response.Result = true;
        response.NeighborhoodName = neighborhood->GetName();
    }
    else
    {
        response.NeighborhoodGuid = queryNeighborhoodInfo.NeighborhoodGuid;
        response.Result = false;
    }

    WorldPacket const* namePkt = response.Write();
    SendPacket(namePkt);

    TC_LOG_DEBUG("housing", "SMSG_QUERY_NEIGHBORHOOD_NAME_RESPONSE Result={}, Name='{}', NeighborhoodGuid: {}",
        response.Result, response.NeighborhoodName, response.NeighborhoodGuid.ToString());
}

void WorldSession::HandleInvitePlayerToNeighborhood(WorldPackets::Housing::InvitePlayerToNeighborhood const& invitePlayerToNeighborhood)
{
    Player* player = GetPlayer();
    if (!player)
        return;

    Neighborhood* neighborhood = sNeighborhoodMgr.ResolveNeighborhood(invitePlayerToNeighborhood.NeighborhoodGuid, player);
    if (!neighborhood)
    {
        WorldPackets::Neighborhood::NeighborhoodInviteResidentResponse response;
        response.Result = static_cast<uint8>(HOUSING_RESULT_NEIGHBORHOOD_NOT_FOUND);
        SendPacket(response.Write());
        return;
    }

    if (!neighborhood->IsManager(player->GetGUID()) && !neighborhood->IsOwner(player->GetGUID()))
    {
        WorldPackets::Neighborhood::NeighborhoodInviteResidentResponse response;
        response.Result = static_cast<uint8>(HOUSING_RESULT_GENERIC_FAILURE);
        SendPacket(response.Write());
        return;
    }

    HousingResult result = neighborhood->InviteResident(player->GetGUID(), invitePlayerToNeighborhood.PlayerGuid);

    WorldPackets::Neighborhood::NeighborhoodInviteResidentResponse response;
    response.Result = static_cast<uint8>(result);
    response.InviteeGuid = invitePlayerToNeighborhood.PlayerGuid;
    SendPacket(response.Write());

    // Notify the invitee that they received a neighborhood invite
    if (result == HOUSING_RESULT_SUCCESS)
    {
        if (Player* invitee = ObjectAccessor::FindPlayer(invitePlayerToNeighborhood.PlayerGuid))
        {
            WorldPackets::Neighborhood::NeighborhoodInviteNotification notification;
            notification.NeighborhoodGuid = invitePlayerToNeighborhood.NeighborhoodGuid;
            invitee->SendDirectMessage(notification.Write());
        }
    }

    TC_LOG_INFO("housing", "CMSG_INVITE_PLAYER_TO_NEIGHBORHOOD PlayerGuid: {}, Result: {}",
        invitePlayerToNeighborhood.PlayerGuid.ToString(), uint32(result));
}

void WorldSession::HandleGuildGetOthersOwnedHouses(WorldPackets::Housing::GuildGetOthersOwnedHouses const& guildGetOthersOwnedHouses)
{
    Player* player = GetPlayer();
    if (!player)
        return;

    // Look up houses owned by the specified player (typically a guild member)
    std::vector<Neighborhood*> neighborhoods = sNeighborhoodMgr.GetNeighborhoodsForPlayer(guildGetOthersOwnedHouses.PlayerGuid);

    WorldPackets::Housing::HousingSvcsGuildGetHousingInfoResponse response;
    for (Neighborhood* neighborhood : neighborhoods)
    {
        WorldPackets::Housing::JamCliHouseFinderNeighborhood entry;
        entry.NeighborhoodGUID = neighborhood->GetGuid();
        entry.OwnerGUID = neighborhood->GetOwnerGuid();
        entry.Name = neighborhood->GetName();
        for (auto const& plot : neighborhood->GetPlots())
        {
            if (!plot.IsOccupied())
                continue;
            WorldPackets::Housing::JamCliHouse house;
            house.HouseGUID = plot.HouseGuid;
            house.OwnerGUID = plot.OwnerGuid;
            house.NeighborhoodGUID = neighborhood->GetGuid();
            house.PlotIndex = plot.PlotIndex;
            entry.Houses.push_back(std::move(house));
        }
        response.Neighborhoods.push_back(std::move(entry));
    }
    SendPacket(response.Write());

    TC_LOG_INFO("housing", "CMSG_GUILD_GET_OTHERS_OWNED_HOUSES PlayerGuid: {}, FoundNeighborhoods: {}",
        guildGetOthersOwnedHouses.PlayerGuid.ToString(), uint32(neighborhoods.size()));
}

// ============================================================
// Photo Sharing Authorization
// ============================================================

void WorldSession::HandleHousingPhotoSharingCompleteAuthorization(WorldPackets::Housing::HousingPhotoSharingCompleteAuthorization const& /*packet*/)
{
    Player* player = GetPlayer();
    if (!player)
        return;

    Housing* housing = player->GetHousing();
    WorldPackets::Housing::HousingPhotoSharingAuthorizationResult response;

    if (!housing || housing->GetHouseGuid().IsEmpty())
    {
        response.Result = static_cast<uint8>(HOUSING_RESULT_HOUSE_NOT_FOUND);
        SendPacket(response.Write());
        return;
    }

    // Track authorization state on the Housing object (per-session, volatile).
    // Actual screenshot hosting requires an external CDN — server only tracks the auth grant.
    housing->SetPhotoSharingAuthorized(true);
    response.Result = static_cast<uint8>(HOUSING_RESULT_SUCCESS);
    SendPacket(response.Write());

    TC_LOG_DEBUG("housing", "CMSG_HOUSING_PHOTO_SHARING_COMPLETE_AUTHORIZATION Player: {} authorized photo sharing for house {}",
        player->GetGUID().ToString(), housing->GetHouseGuid().ToString());
}

void WorldSession::HandleHousingPhotoSharingClearAuthorization(WorldPackets::Housing::HousingPhotoSharingClearAuthorization const& /*packet*/)
{
    Player* player = GetPlayer();
    if (!player)
        return;

    Housing* housing = player->GetHousing();
    WorldPackets::Housing::HousingPhotoSharingAuthorizationClearedResult response;

    if (!housing || housing->GetHouseGuid().IsEmpty())
    {
        response.Result = static_cast<uint8>(HOUSING_RESULT_HOUSE_NOT_FOUND);
        SendPacket(response.Write());
        return;
    }

    housing->SetPhotoSharingAuthorized(false);
    response.Result = static_cast<uint8>(HOUSING_RESULT_SUCCESS);
    SendPacket(response.Write());

    TC_LOG_DEBUG("housing", "CMSG_HOUSING_PHOTO_SHARING_CLEAR_AUTHORIZATION Player: {} cleared photo sharing for house {}",
        player->GetGUID().ToString(), housing->GetHouseGuid().ToString());
}

// ============================================================
// Decor Licensing / Refund Handlers
// ============================================================

void WorldSession::HandleGetAllLicensedDecorQuantities(WorldPackets::Housing::GetAllLicensedDecorQuantities const& /*getAllLicensedDecorQuantities*/)
{
    Player* player = GetPlayer();
    if (!player)
        return;

    TC_LOG_DEBUG("housing", "CMSG_GET_ALL_LICENSED_DECOR_QUANTITIES Player: {}", player->GetGUID().ToString());

    WorldPackets::Housing::GetAllLicensedDecorQuantitiesResponse response;

    Housing* housing = player->GetHousing();
    if (housing)
    {
        // Populate from the player's catalog (persisted decor they own)
        for (Housing::CatalogEntry const* entry : housing->GetCatalogEntries())
        {
            WorldPackets::Housing::JamLicensedDecorQuantity qty;
            qty.DecorID = entry->DecorEntryId;
            qty.Quantity = entry->Count;
            response.Quantities.push_back(qty);
        }

        // Merge starter decor that may not be in catalog yet (safety net)
        auto starterDecor = sHousingMgr.GetStarterDecorWithQuantities(player->GetTeam());
        for (auto const& [decorId, quantity] : starterDecor)
        {
            bool found = false;
            for (auto const& existing : response.Quantities)
            {
                if (existing.DecorID == decorId)
                {
                    found = true;
                    break;
                }
            }
            if (!found)
            {
                WorldPackets::Housing::JamLicensedDecorQuantity qty;
                qty.DecorID = decorId;
                qty.Quantity = quantity;
                response.Quantities.push_back(qty);
            }
        }
    }

    SendPacket(response.Write());

    TC_LOG_DEBUG("housing", "SMSG_GET_ALL_LICENSED_DECOR_QUANTITIES_RESPONSE sent to Player: {} with {} quantities",
        player->GetGUID().ToString(), response.Quantities.size());
}

void WorldSession::HandleGetDecorRefundList(WorldPackets::Housing::GetDecorRefundList const& /*getDecorRefundList*/)
{
    Player* player = GetPlayer();
    if (!player)
        return;

    TC_LOG_DEBUG("housing", "CMSG_GET_DECOR_REFUND_LIST Player: {}", player->GetGUID().ToString());

    WorldPackets::Housing::GetDecorRefundListResponse response;

    Housing* housing = player->GetHousing();
    if (housing)
    {
        time_t now = GameTime::GetGameTime();
        constexpr time_t REFUND_WINDOW = 2 * HOUR;

        for (auto const& [guid, decor] : housing->GetPlacedDecorMap())
        {
            if (decor.PlacementTime > 0 && (now - decor.PlacementTime) < REFUND_WINDOW)
            {
                WorldPackets::Housing::JamClientRefundableDecor refund;
                refund.DecorID = decor.DecorEntryId;
                refund.RefundPrice = 0;
                refund.ExpiryTime = static_cast<uint64>(decor.PlacementTime + REFUND_WINDOW);
                refund.Flags = 0;
                response.Decors.push_back(refund);
            }
        }
    }

    SendPacket(response.Write());

    TC_LOG_DEBUG("housing", "SMSG_GET_DECOR_REFUND_LIST_RESPONSE sent to Player: {} with {} decors",
        player->GetGUID().ToString(), response.Decors.size());
}

void WorldSession::HandleBulkRefund(WorldPackets::Housing::BulkRefund const& bulkRefund)
{
    Player* player = GetPlayer();
    if (!player)
        return;

    TC_LOG_INFO("housing", "CMSG_BULK_REFUND Player: {} DecorGUIDs: {}",
        player->GetGUID().ToString(), bulkRefund.DecorGUIDs.size());

    Housing* housing = player->GetHousing();
    if (!housing)
    {
        WorldPackets::Housing::BulkRefundResponse response;
        response.Result = static_cast<uint8>(BULK_REFUND_RESULT_INVALID_REQUEST);
        SendPacket(response.Write());
        return;
    }

    if (bulkRefund.DecorGUIDs.empty())
    {
        WorldPackets::Housing::BulkRefundResponse response;
        response.Result = static_cast<uint8>(BULK_REFUND_RESULT_INVALID_REQUEST);
        SendPacket(response.Write());
        return;
    }

    // Validate all GUIDs exist and are within the refund window before refunding any.
    // This is atomic: if any GUID fails validation, the entire batch fails.
    time_t now = GameTime::GetGameTime();
    constexpr time_t REFUND_WINDOW = 2 * HOUR;

    for (ObjectGuid const& decorGuid : bulkRefund.DecorGUIDs)
    {
        Housing::PlacedDecor const* placedDecor = housing->GetPlacedDecor(decorGuid);
        if (!placedDecor)
        {
            TC_LOG_DEBUG("housing", "CMSG_BULK_REFUND: DecorGUID {} not found in placed decor", decorGuid.ToString());
            WorldPackets::Housing::BulkRefundResponse response;
            response.Result = static_cast<uint8>(BULK_REFUND_RESULT_INVALID_REQUEST);
            SendPacket(response.Write());
            return;
        }

        if (placedDecor->PlacementTime == 0 || (now - placedDecor->PlacementTime) >= REFUND_WINDOW)
        {
            TC_LOG_DEBUG("housing", "CMSG_BULK_REFUND: DecorGUID {} outside refund window (placed {}s ago)",
                decorGuid.ToString(), placedDecor->PlacementTime > 0 ? (now - placedDecor->PlacementTime) : -1);
            WorldPackets::Housing::BulkRefundResponse response;
            response.Result = static_cast<uint8>(BULK_REFUND_RESULT_REFUND_WINDOW_EXPIRED);
            SendPacket(response.Write());
            return;
        }
    }

    // All GUIDs validated — proceed with refund.
    // Each decor is removed and returned to catalog (same as individual RemoveDecor).
    uint8 plotIndex = housing->GetPlotIndex();
    uint32 refundedCount = 0;

    for (ObjectGuid const& decorGuid : bulkRefund.DecorGUIDs)
    {
        // Capture source info before removal
        uint8 removedSourceType = DECOR_SOURCE_STANDARD;
        std::string removedSourceValue;
        if (Housing::PlacedDecor const* placedDecor = housing->GetPlacedDecor(decorGuid))
        {
            removedSourceType = placedDecor->SourceType;
            removedSourceValue = placedDecor->SourceValue;
        }

        HousingResult result = housing->RemoveDecor(decorGuid);
        if (result != HOUSING_RESULT_SUCCESS)
        {
            TC_LOG_WARN("housing", "CMSG_BULK_REFUND: RemoveDecor failed for {} with result {} (after validation passed)",
                decorGuid.ToString(), uint32(result));
            continue;
        }

        // Despawn the decor entity from the map
        if (HousingMap* housingMap = dynamic_cast<HousingMap*>(player->GetMap()))
            housingMap->DespawnDecorItem(plotIndex, decorGuid);
        else if (HouseInteriorMap* interiorMap = dynamic_cast<HouseInteriorMap*>(player->GetMap()))
            interiorMap->DespawnDecorItem(decorGuid);

        // Return to storage in Account entity (same as individual remove)
        Battlenet::Account& account = GetBattlenetAccount();
        account.SetHousingDecorStorageEntry(decorGuid, ObjectGuid::Empty, removedSourceType, removedSourceValue);

        ++refundedCount;
    }

    // Send single batch update to client after all removals
    if (refundedCount > 0)
        GetBattlenetAccount().SendUpdateToPlayer(player);

    WorldPackets::Housing::BulkRefundResponse response;
    response.Result = static_cast<uint8>(BULK_REFUND_RESULT_SUCCESS);
    SendPacket(response.Write());

    TC_LOG_INFO("housing", "CMSG_BULK_REFUND: Player {} refunded {}/{} decors",
        player->GetName(), refundedCount, bulkRefund.DecorGUIDs.size());
}

void WorldSession::HandleHousingDecorStartPlacingNewDecor(WorldPackets::Housing::HousingDecorStartPlacingNewDecor const& housingDecorStartPlacingNewDecor)
{
    Player* player = GetPlayer();
    if (!player)
        return;

    uint32 catalogEntryId = housingDecorStartPlacingNewDecor.CatalogEntryID;

    TC_LOG_DEBUG("housing", "CMSG_HOUSING_DECOR_START_PLACING_NEW_DECOR Player: {} CatalogEntryID: {} Field_4: {}",
        player->GetGUID().ToString(), catalogEntryId, housingDecorStartPlacingNewDecor.Field_4);

    WorldPackets::Housing::HousingDecorStartPlacingNewDecorResponse response;
    response.DecorGuid = ObjectGuid::Empty;
    response.Field_13 = housingDecorStartPlacingNewDecor.Field_4;

    Housing* housing = player->GetHousing();
    if (!housing)
    {
        response.Result = static_cast<uint8>(HOUSING_RESULT_HOUSE_NOT_FOUND);
        SendPacket(response.Write());
        return;
    }

    HousingResult result;
    ObjectGuid decorGuid = housing->StartPlacingNewDecor(catalogEntryId, result);

    response.Result = static_cast<uint8>(result);
    response.DecorGuid = decorGuid;

    SendPacket(response.Write());

    TC_LOG_DEBUG("housing", "SMSG_HOUSING_DECOR_START_PLACING_NEW_DECOR_RESPONSE Player: {} DecorGuid: {} Result: {}",
        player->GetGUID().ToString(), decorGuid.ToString(), response.Result);
}

void WorldSession::HandleHousingDecorCatalogCreateSearcher(WorldPackets::Housing::HousingDecorCatalogCreateSearcher const& housingDecorCatalogCreateSearcher)
{
    Player* player = GetPlayer();
    if (!player)
        return;

    TC_LOG_DEBUG("housing", "CMSG_HOUSING_DECOR_CATALOG_CREATE_SEARCHER Player: {} Owner: {}",
        player->GetGUID().ToString(), housingDecorCatalogCreateSearcher.Owner.ToString());

    Housing* housing = player->GetHousing();

    WorldPackets::Housing::HousingDecorCatalogCreateSearcherResponse response;
    response.Owner = housingDecorCatalogCreateSearcher.Owner;
    response.Result = (housing && !housing->GetHouseGuid().IsEmpty())
        ? static_cast<uint8>(HOUSING_RESULT_SUCCESS)
        : static_cast<uint8>(HOUSING_RESULT_PERMISSION_DENIED);
    SendPacket(response.Write());
}

void WorldSession::HandleGetLastCatalogFetch(WorldPackets::Housing::GetLastCatalogFetch const& /*getLastCatalogFetch*/)
{
    // Sniff-verified (build 66337): retail DOES respond with SMSG_LAST_CATALOG_FETCH_RESPONSE
    // containing a uint64 Unix timestamp. This corrects the earlier finding that "retail never
    // responds" — that was from an older build. Build 66337 sends it 5-6 times per session.
    TC_LOG_DEBUG("housing", "CMSG_GET_LAST_CATALOG_FETCH from player {}",
        GetPlayer() ? GetPlayer()->GetGUID().ToString() : "null");

    WorldPackets::Housing::LastCatalogFetchResponse response;
    response.Timestamp = uint64(GameTime::GetGameTime());
    SendPacket(response.Write());
}

void WorldSession::HandleUpdateLastCatalogFetch(WorldPackets::Housing::UpdateLastCatalogFetch const& /*updateLastCatalogFetch*/)
{
    // Sniff-verified (build 66337): retail responds with SMSG_LAST_CATALOG_FETCH_RESPONSE
    // to BOTH GetLastCatalogFetch AND UpdateLastCatalogFetch. 8-byte timestamp payload.
    TC_LOG_DEBUG("housing", "CMSG_UPDATE_LAST_CATALOG_FETCH from player {}",
        GetPlayer() ? GetPlayer()->GetGUID().ToString() : "null");

    WorldPackets::Housing::LastCatalogFetchResponse response;
    response.Timestamp = uint64(GameTime::GetGameTime());
    SendPacket(response.Write());
}

void WorldSession::HandleHousingRequestEditorAvailability(WorldPackets::Housing::HousingRequestEditorAvailability const& housingRequestEditorAvailability)
{
    Player* player = GetPlayer();
    if (!player)
        return;

    TC_LOG_DEBUG("housing", "CMSG_HOUSING_REQUEST_EDITOR_AVAILABILITY Player: {} HouseGuid: {} Field_0: {}",
        player->GetGUID().ToString(), housingRequestEditorAvailability.HouseGuid.ToString(),
        housingRequestEditorAvailability.Field_0);

    Housing* housing = player->GetHousing();
    bool canEdit = housing && !housing->GetHouseGuid().IsEmpty();

    WorldPackets::Housing::HousingEditorAvailabilityResponse response;
    response.HouseGuid = housingRequestEditorAvailability.HouseGuid;
    response.Result = canEdit ? static_cast<uint8>(HOUSING_RESULT_SUCCESS) : static_cast<uint8>(HOUSING_RESULT_PERMISSION_DENIED);
    response.Field_09 = canEdit ? 224 : 0;

    SendPacket(response.Write());

    TC_LOG_DEBUG("housing", "SMSG_HOUSING_EDITOR_AVAILABILITY_RESPONSE Player: {} HouseGuid: {} Result: {} Field_09: {}",
        player->GetGUID().ToString(), response.HouseGuid.ToString(), response.Result, response.Field_09);
}

// ============================================================
// Phase 7 — Decor Handlers
// ============================================================

void WorldSession::HandleHousingDecorUpdateDyeSlot(WorldPackets::Housing::HousingDecorUpdateDyeSlot const& housingDecorUpdateDyeSlot)
{
    Player* player = GetPlayer();
    if (!player)
        return;

    TC_LOG_DEBUG("housing", "CMSG_HOUSING_DECOR_UPDATE_DYE_SLOT Player: {} DecorGuid: {} SlotIndex: {} DyeColorID: {}",
        player->GetGUID().ToString(), housingDecorUpdateDyeSlot.DecorGuid.ToString(),
        housingDecorUpdateDyeSlot.SlotIndex, housingDecorUpdateDyeSlot.DyeColorID);

    Housing* housing = player->GetHousing();
    if (!housing)
    {
        WorldPackets::Housing::HousingDecorSystemSetDyeSlotsResponse response;
        response.DecorGuid = housingDecorUpdateDyeSlot.DecorGuid;
        response.Result = static_cast<uint8>(HOUSING_RESULT_HOUSE_NOT_FOUND);
        SendPacket(response.Write());
        return;
    }

    if (housingDecorUpdateDyeSlot.SlotIndex >= MAX_HOUSING_DYE_SLOTS)
    {
        WorldPackets::Housing::HousingDecorSystemSetDyeSlotsResponse response;
        response.DecorGuid = housingDecorUpdateDyeSlot.DecorGuid;
        response.Result = static_cast<uint8>(HOUSING_RESULT_MISSING_DYE);
        SendPacket(response.Write());
        return;
    }

    Housing::PlacedDecor const* decor = housing->GetPlacedDecor(housingDecorUpdateDyeSlot.DecorGuid);
    if (!decor)
    {
        WorldPackets::Housing::HousingDecorSystemSetDyeSlotsResponse response;
        response.DecorGuid = housingDecorUpdateDyeSlot.DecorGuid;
        response.Result = static_cast<uint8>(HOUSING_RESULT_DECOR_NOT_FOUND);
        SendPacket(response.Write());
        return;
    }

    // Update single slot, preserve the others
    std::array<uint32, MAX_HOUSING_DYE_SLOTS> dyeSlots = decor->DyeSlots;
    dyeSlots[housingDecorUpdateDyeSlot.SlotIndex] = housingDecorUpdateDyeSlot.DyeColorID;

    HousingResult result = housing->CommitDecorDyes(housingDecorUpdateDyeSlot.DecorGuid, dyeSlots);

    WorldPackets::Housing::HousingDecorSystemSetDyeSlotsResponse response;
    response.DecorGuid = housingDecorUpdateDyeSlot.DecorGuid;
    response.Result = static_cast<uint8>(result);
    SendPacket(response.Write());
}

void WorldSession::HandleHousingDecorStartPlacingFromSource(WorldPackets::Housing::HousingDecorStartPlacingFromSource const& housingDecorStartPlacingFromSource)
{
    Player* player = GetPlayer();
    if (!player)
        return;

    TC_LOG_DEBUG("housing", "CMSG_HOUSING_DECOR_START_PLACING_FROM_SOURCE Player: {} SourceType: {} SourceID: {}",
        player->GetGUID().ToString(), housingDecorStartPlacingFromSource.SourceType,
        housingDecorStartPlacingFromSource.SourceID);

    Housing* housing = player->GetHousing();

    WorldPackets::Housing::HousingDecorStartPlacingNewDecorResponse response;
    response.DecorGuid = ObjectGuid::Empty;
    response.Field_13 = 0;

    if (!housing)
    {
        response.Result = static_cast<uint8>(HOUSING_RESULT_HOUSE_NOT_FOUND);
        SendPacket(response.Write());
        return;
    }

    HousingResult result;
    ObjectGuid decorGuid = housing->StartPlacingNewDecor(housingDecorStartPlacingFromSource.SourceID, result);
    response.Result = static_cast<uint8>(result);
    response.DecorGuid = decorGuid;
    SendPacket(response.Write());
}

void WorldSession::HandleHousingDecorCleanupModeToggle(WorldPackets::Housing::HousingDecorCleanupModeToggle const& housingDecorCleanupModeToggle)
{
    Player* player = GetPlayer();
    if (!player)
        return;

    TC_LOG_DEBUG("housing", "CMSG_HOUSING_DECOR_CLEANUP_MODE_TOGGLE Player: {} Enabled: {}",
        player->GetGUID().ToString(), housingDecorCleanupModeToggle.Enabled);

    // Cleanup mode is a client-side UI state; server just acknowledges
    TC_LOG_DEBUG("housing", "Player {} cleanup mode toggled to {}",
        player->GetGUID().ToString(), housingDecorCleanupModeToggle.Enabled ? "enabled" : "disabled");
}

void WorldSession::HandleHousingDecorBatchOperation(WorldPackets::Housing::HousingDecorBatchOperation const& housingDecorBatchOperation)
{
    Player* player = GetPlayer();
    if (!player)
        return;

    TC_LOG_DEBUG("housing", "CMSG_HOUSING_DECOR_BATCH_OPERATION Player: {} OperationType: {} Count: {}",
        player->GetGUID().ToString(), housingDecorBatchOperation.OperationType,
        uint32(housingDecorBatchOperation.DecorGuids.size()));

    Housing* housing = player->GetHousing();
    uint32 processedCount = 0;

    if (housing)
    {
        for (ObjectGuid const& decorGuid : housingDecorBatchOperation.DecorGuids)
        {
            switch (housingDecorBatchOperation.OperationType)
            {
                case 0: // Remove
                    housing->RemoveDecor(decorGuid);
                    ++processedCount;
                    break;
                case 1: // Lock
                    housing->SetDecorLocked(decorGuid, true);
                    ++processedCount;
                    break;
                case 2: // Unlock
                    housing->SetDecorLocked(decorGuid, false);
                    ++processedCount;
                    break;
                default:
                    TC_LOG_DEBUG("housing", "Unknown batch operation type {} for decor {}",
                        housingDecorBatchOperation.OperationType, decorGuid.ToString());
                    break;
            }
        }
    }

    WorldPackets::Housing::HousingDecorBatchOperationResponse response;
    response.Result = static_cast<uint8>(housing ? HOUSING_RESULT_SUCCESS : HOUSING_RESULT_HOUSE_NOT_FOUND);
    response.ProcessedCount = processedCount;
    SendPacket(response.Write());
}

void WorldSession::HandleHousingDecorPlacementPreview(WorldPackets::Housing::HousingDecorPlacementPreview const& housingDecorPlacementPreview)
{
    Player* player = GetPlayer();
    if (!player)
        return;

    TC_LOG_DEBUG("housing", "CMSG_HOUSING_DECOR_PLACEMENT_PREVIEW Player: {} DecorGuid: {} Pos: ({}, {}, {})",
        player->GetGUID().ToString(), housingDecorPlacementPreview.DecorGuid.ToString(),
        housingDecorPlacementPreview.PreviewPosition.Pos.GetPositionX(),
        housingDecorPlacementPreview.PreviewPosition.Pos.GetPositionY(),
        housingDecorPlacementPreview.PreviewPosition.Pos.GetPositionZ());

    Housing* housing = player->GetHousing();
    uint32 restrictionFlags = 0;
    HousingResult result = HOUSING_RESULT_SUCCESS;

    if (!housing)
    {
        result = HOUSING_RESULT_HOUSE_NOT_FOUND;
    }
    else
    {
        // Plot bounds validation: retail uses (-35,-30,-1.01) to (35,30,125.01) with 10.0 buffer
        constexpr float PLOT_MIN_X = -35.0f, PLOT_MIN_Y = -30.0f, PLOT_MIN_Z = -1.01f;
        constexpr float PLOT_MAX_X = 35.0f, PLOT_MAX_Y = 30.0f, PLOT_MAX_Z = 125.01f;
        constexpr float BOUNDS_BUFFER = 10.0f; // housingDecor_PlotBoundsBuffer CVar default

        float x = housingDecorPlacementPreview.PreviewPosition.Pos.GetPositionX();
        float y = housingDecorPlacementPreview.PreviewPosition.Pos.GetPositionY();
        float z = housingDecorPlacementPreview.PreviewPosition.Pos.GetPositionZ();

        if (x < PLOT_MIN_X - BOUNDS_BUFFER || x > PLOT_MAX_X + BOUNDS_BUFFER ||
            y < PLOT_MIN_Y - BOUNDS_BUFFER || y > PLOT_MAX_Y + BOUNDS_BUFFER ||
            z < PLOT_MIN_Z || z > PLOT_MAX_Z)
        {
            restrictionFlags |= 0x01; // BOUNDS_FAILURE_PLOT
        }
    }

    WorldPackets::Housing::HousingDecorPlacementPreviewResponse response;
    response.Result = static_cast<uint8>(result);
    response.RestrictionFlags = restrictionFlags;
    SendPacket(response.Write());
}

// ============================================================
// Phase 7 — Fixture Handlers
// ============================================================

void WorldSession::HandleHousingFixtureCreateBasicHouse(WorldPackets::Housing::HousingFixtureCreateBasicHouse const& housingFixtureCreateBasicHouse)
{
    Player* player = GetPlayer();
    if (!player)
        return;

    TC_LOG_DEBUG("housing", "CMSG_HOUSING_FIXTURE_CREATE_BASIC_HOUSE Player={} PlotGuid={} HouseStyleID={}",
        player->GetGUID().ToString(), housingFixtureCreateBasicHouse.PlotGuid.ToString(),
        housingFixtureCreateBasicHouse.HouseStyleID);

    Housing* housing = player->GetHousing();
    if (!housing)
    {
        WorldPackets::Housing::HousingFixtureCreateBasicHouseResponse response;
        response.Result = static_cast<uint8>(HOUSING_RESULT_HOUSE_NOT_FOUND);
        SendPacket(response.Write());
        return;
    }

    // If player already has a house, apply the style and return success.
    // This opcode is an alternative creation path from the fixture edit UI.
    // The primary creation path is HandleNeighborhoodBuyHouse.
    if (!housing->GetHouseGuid().IsEmpty())
    {
        // Apply house style if provided and different from current
        uint32 styleID = housingFixtureCreateBasicHouse.HouseStyleID;
        if (styleID != 0 && styleID != housing->GetHouseType())
        {
            HouseExteriorWmoData const* wmoData = sHousingMgr.GetHouseExteriorWmoData(styleID);
            if (wmoData)
            {
                housing->SetHouseType(styleID);

                // Respawn house visuals with new type (must also respawn decor since DespawnHouseForPlot removes all MeshObjects)
                if (HousingMap* housingMap = dynamic_cast<HousingMap*>(player->GetMap()))
                {
                    uint8 plotIndex = housing->GetPlotIndex();
                    auto fixtureOverrides = housing->GetFixtureOverrideMap();
                    auto rootOverrides = housing->GetRootComponentOverrides();
                    housingMap->DespawnAllDecorForPlot(plotIndex);
                    housingMap->DespawnHouseForPlot(plotIndex);
                    housingMap->SpawnHouseForPlot(plotIndex, nullptr,
                        static_cast<int32>(housing->GetCoreExteriorComponentID()),
                        static_cast<int32>(styleID),
                        fixtureOverrides.empty() ? nullptr : &fixtureOverrides,
                        rootOverrides.empty() ? nullptr : &rootOverrides);
                    housingMap->SpawnAllDecorForPlot(plotIndex, housing);
                }
            }
        }

        WorldPackets::Housing::HousingFixtureCreateBasicHouseResponse response;
        response.Result = static_cast<uint8>(HOUSING_RESULT_SUCCESS);
        SendPacket(response.Write());

        // Send inline UPDATE_OBJECT with updated house data
        SendFixtureUpdateObject(player, housing);
        return;
    }

    // Player has Housing object but no house GUID — shouldn't happen in normal flow.
    // The house should have been created via HandleNeighborhoodBuyHouse.
    WorldPackets::Housing::HousingFixtureCreateBasicHouseResponse response;
    response.Result = static_cast<uint8>(HOUSING_RESULT_PLOT_NOT_FOUND);
    SendPacket(response.Write());
}

void WorldSession::HandleHousingFixtureDeleteHouse(WorldPackets::Housing::HousingFixtureDeleteHouse const& housingFixtureDeleteHouse)
{
    Player* player = GetPlayer();
    if (!player)
        return;

    if (!sWorld->getBoolConfig(CONFIG_HOUSING_ENABLE_DELETE_HOUSE))
    {
        WorldPackets::Housing::HousingFixtureDeleteHouseResponse response;
        response.Result = static_cast<uint8>(HOUSING_RESULT_SERVICE_NOT_AVAILABLE);
        SendPacket(response.Write());
        return;
    }

    TC_LOG_DEBUG("housing", "CMSG_HOUSING_FIXTURE_DELETE_HOUSE Player: {} HouseGuid: {}",
        player->GetGUID().ToString(), housingFixtureDeleteHouse.HouseGuid.ToString());

    Housing* housing = player->GetHousing();
    WorldPackets::Housing::HousingFixtureDeleteHouseResponse response;

    if (!housing || housing->GetHouseGuid() != housingFixtureDeleteHouse.HouseGuid)
    {
        response.Result = static_cast<uint8>(HOUSING_RESULT_HOUSE_NOT_FOUND);
        SendPacket(response.Write());
        return;
    }

    // Exit edit mode if active
    if (housing->GetEditorMode() != HOUSING_EDITOR_MODE_NONE)
        housing->SetEditorMode(HOUSING_EDITOR_MODE_NONE);

    // Exit interior if inside
    if (housing->IsInInterior())
        housing->SetInInterior(false);

    ObjectGuid houseGuid = housing->GetHouseGuid();
    ObjectGuid neighborhoodGuid = housing->GetNeighborhoodGuid();
    uint8 plotIndex = housing->GetPlotIndex();

    Neighborhood* neighborhood = sNeighborhoodMgr.GetNeighborhood(neighborhoodGuid);

    // Step 1: Despawn all entities on the map BEFORE deleting housing data
    if (plotIndex != INVALID_PLOT_INDEX)
    {
        if (HousingMap* housingMap = dynamic_cast<HousingMap*>(player->GetMap()))
        {
            housingMap->DespawnAllDecorForPlot(plotIndex);
            housingMap->DespawnAllMeshObjectsForPlot(plotIndex);
            housingMap->DespawnRoomForPlot(plotIndex);
            housingMap->DespawnHouseForPlot(plotIndex);
            housingMap->SetPlotOwnershipState(plotIndex, false);
        }
    }

    // Step 2: Remove from neighborhood membership
    if (neighborhood)
        neighborhood->EvictPlayer(player->GetGUID());

    // Step 3: Delete house data from DB and clear in-memory state
    housing->Delete();

    // Step 4: Send response
    response.Result = static_cast<uint8>(HOUSING_RESULT_SUCCESS);
    SendPacket(response.Write());

    // Step 5: Request client to reload housing data
    WorldPackets::Housing::HousingSvcRequestPlayerReloadData reloadData;
    SendPacket(reloadData.Write());

    // Step 6: Broadcast roster update to remaining members and refresh mirror data
    if (neighborhood)
    {
        WorldPackets::Neighborhood::NeighborhoodRosterResidentUpdate rosterUpdate;
        rosterUpdate.Residents.push_back({ player->GetGUID(), 2 /*Removed*/, false });
        neighborhood->BroadcastPacket(rosterUpdate.Write(), player->GetGUID());

        neighborhood->RefreshMirrorDataForOnlineMembers();
    }

    TC_LOG_INFO("housing", "CMSG_HOUSING_FIXTURE_DELETE_HOUSE: Player {} deleted house {} from plot {} in neighborhood {}",
        player->GetName(), houseGuid.ToString(), plotIndex, neighborhoodGuid.ToString());
}

// ============================================================
// Phase 7 — Housing Services Handlers
// ============================================================

void WorldSession::HandleHousingSvcsRequestPermissionsCheck(WorldPackets::Housing::HousingSvcsRequestPermissionsCheck const& /*housingSvcsRequestPermissionsCheck*/)
{
    Player* player = GetPlayer();
    if (!player)
        return;

    TC_LOG_DEBUG("housing", "CMSG_HOUSING_SVCS_REQUEST_PERMISSIONS_CHECK Player: {}",
        player->GetGUID().ToString());

    // Server-push trigger — no direct response needed
    // Permissions are checked and sent proactively when the player's housing state changes
}

void WorldSession::HandleHousingSvcsClearPlotReservation(WorldPackets::Housing::HousingSvcsClearPlotReservation const& housingSvcsClearPlotReservation)
{
    Player* player = GetPlayer();
    if (!player)
        return;

    TC_LOG_DEBUG("housing", "CMSG_HOUSING_SVCS_CLEAR_PLOT_RESERVATION Player: {} NeighborhoodGuid: {}",
        player->GetGUID().ToString(), housingSvcsClearPlotReservation.NeighborhoodGuid.ToString());

    Neighborhood* neighborhood = sNeighborhoodMgr.ResolveNeighborhood(housingSvcsClearPlotReservation.NeighborhoodGuid, player);

    WorldPackets::Housing::HousingSvcsClearPlotReservationResponse response;
    response.NeighborhoodGuid = housingSvcsClearPlotReservation.NeighborhoodGuid;

    if (!neighborhood)
    {
        response.Result = static_cast<uint8>(HOUSING_RESULT_NEIGHBORHOOD_NOT_FOUND);
    }
    else
    {
        neighborhood->ClearReservation(player->GetGUID());
        response.Result = static_cast<uint8>(HOUSING_RESULT_SUCCESS);
    }

    SendPacket(response.Write());
}

// Removed 2026-04-24: HandleHousingSvcsGetPlayerHousesInfoAlt — duplicate of the real
// CMSG_HOUSING_SVCS_GET_PLAYER_HOUSES_INFO (0x330013, see master). Use that handler.

void WorldSession::HandleHousingSvcsGetRosterData(WorldPackets::Housing::HousingSvcsGetRosterData const& housingSvcsGetRosterData)
{
    Player* player = GetPlayer();
    if (!player)
        return;

    TC_LOG_DEBUG("housing", "CMSG_HOUSING_SVCS_GET_ROSTER_DATA Player: {} NeighborhoodGuid: {}",
        player->GetGUID().ToString(), housingSvcsGetRosterData.NeighborhoodGuid.ToString());

    // Reuse PlayerViewHousesResponse (IDA: uint32(count) + uint8(result) + JamCliHouse[count])
    WorldPackets::Housing::HousingSvcsPlayerViewHousesResponse response;
    response.Result = static_cast<uint8>(HOUSING_RESULT_SUCCESS);

    Neighborhood* neighborhood = sNeighborhoodMgr.ResolveNeighborhood(housingSvcsGetRosterData.NeighborhoodGuid, player);
    if (neighborhood)
    {
        for (auto const& plotInfo : neighborhood->GetPlots())
        {
            if (!plotInfo.IsOccupied())
                continue;
            WorldPackets::Housing::JamCliHouse house;
            house.OwnerGUID = plotInfo.OwnerGuid;
            house.HouseGUID = plotInfo.HouseGuid;
            house.NeighborhoodGUID = neighborhood->GetGuid();
            house.PlotIndex = plotInfo.PlotIndex;
            response.Houses.push_back(house);
        }

        // Populate the Housing/4 entity with this neighborhood's mirror data so the
        // client's internal house list stays in sync for plot resolution.
        HousingNeighborhoodMirrorEntity& mirrorEntity = GetHousingNeighborhoodMirrorEntity();
        mirrorEntity.SetName(neighborhood->GetName());
        mirrorEntity.SetOwnerGUID(neighborhood->GetOwnerGuid());

        mirrorEntity.ClearHouses();
        for (auto const& plot : neighborhood->GetPlots())
        {
            if (plot.IsOccupied() && !plot.HouseGuid.IsEmpty())
                mirrorEntity.AddHouse(plot.HouseGuid, plot.OwnerGuid);
            else
                mirrorEntity.AddHouse(ObjectGuid::Empty, ObjectGuid::Empty);
        }

        mirrorEntity.ClearManagers();
        for (auto const& member : neighborhood->GetMembers())
        {
            if (member.Role == NEIGHBORHOOD_ROLE_MANAGER || member.Role == NEIGHBORHOOD_ROLE_OWNER)
            {
                ObjectGuid bnetGuid;
                if (Player* mgr = ObjectAccessor::FindPlayer(member.PlayerGuid))
                    bnetGuid = mgr->GetSession()->GetBattlenetAccountGUID();
                mirrorEntity.AddManager(bnetGuid, member.PlayerGuid);
            }
        }
        // Wholesale re-push; retail uses CREATE for this (sniff-verified).
        mirrorEntity.SendCreateToPlayer(player);
    }

    SendPacket(response.Write());
}

void WorldSession::HandleHousingSvcsRosterUpdateSubscribe(WorldPackets::Housing::HousingSvcsRosterUpdateSubscribe const& /*housingSvcsRosterUpdateSubscribe*/)
{
    Player* player = GetPlayer();
    if (!player)
        return;

    TC_LOG_DEBUG("housing", "CMSG_HOUSING_SVCS_ROSTER_UPDATE_SUBSCRIBE Player: {}",
        player->GetGUID().ToString());

    // Send the current roster state as an immediate response so the client has up-to-date data.
    // The client expects a NeighborhoodRosterResidentUpdate with the full member list.
    Housing* housing = player->GetHousing();
    if (!housing || housing->GetNeighborhoodGuid().IsEmpty())
        return;

    Neighborhood* neighborhood = sNeighborhoodMgr.GetNeighborhood(housing->GetNeighborhoodGuid());
    if (!neighborhood)
        return;

    WorldPackets::Neighborhood::NeighborhoodRosterResidentUpdate update;
    for (auto const& member : neighborhood->GetMembers())
    {
        WorldPackets::Neighborhood::NeighborhoodRosterResidentUpdate::ResidentEntry entry;
        entry.PlayerGuid = member.PlayerGuid;
        entry.UpdateType = 0; // Added
        entry.IsPrivileged = (member.Role != NEIGHBORHOOD_ROLE_RESIDENT);
        update.Residents.push_back(entry);
    }
    SendPacket(update.Write());
}

// Removed 2026-04-24: HandleHousingSvcsChangeHouseCosmeticOwner — IDA 12.0.5 shows
// C_Housing.SaveHouseSettings (matching CMSG_HOUSING_SVCS_UPDATE_HOUSE_SETTINGS at
// 0x33000B) is the opcode that saves the cosmetic owner. The cosmetic owner change
// logic (ownership validation + persistence + SMSG response) belongs in the
// UPDATE_HOUSE_SETTINGS handler when/if we extend it. The response SMSG
// SMSG_HOUSING_SVCS_CHANGE_HOUSE_COSMETIC_OWNER (0x540010) is still a real opcode.

void WorldSession::HandleHousingSvcsQueryHouseLevelFavor(WorldPackets::Housing::HousingSvcsQueryHouseLevelFavor const& housingSvcsQueryHouseLevelFavor)
{
    Player* player = GetPlayer();
    if (!player)
        return;

    TC_LOG_DEBUG("housing", "CMSG_HOUSING_SVCS_QUERY_HOUSE_LEVEL_FAVOR Player: {} HouseGuid: {}",
        player->GetGUID().ToString(), housingSvcsQueryHouseLevelFavor.HouseGuid.ToString());

    Housing* housing = player->GetHousing();

    WorldPackets::Housing::HousingSvcsUpdateHousesLevelFavor response;
    response.Result = 0; // Query response type

    if (housing && housing->GetHouseGuid() == housingSvcsQueryHouseLevelFavor.HouseGuid)
    {
        response.ChangeAmount = static_cast<uint32>(housing->GetFavor());
        response.Reason = static_cast<uint32>(housing->GetLevel()); // surfaces level via Reason field — clients display both
        response.HouseGUID = housing->GetHouseGuid();
        response.NewFavorTotal = static_cast<int64>(housing->GetFavor());
    }

    SendPacket(response.Write());
}

// Removed 2026-04-24: HandleHousingSvcsGuildAddHouse — no matching 12.0.5 Lua API.
// The notification SMSG (SMSG_HOUSING_SVCS_GUILD_ADD_HOUSE_NOTIFICATION = 0x540012)
// is still a real opcode and can be fired from other contexts (e.g. when a guild
// member's house is auto-linked via neighborhood guild-settings).

void WorldSession::HandleHousingSvcsGuildAppendNeighborhood(WorldPackets::Housing::HousingSvcsGuildAppendNeighborhood const& housingSvcsGuildAppendNeighborhood)
{
    Player* player = GetPlayer();
    if (!player)
        return;

    TC_LOG_DEBUG("housing", "CMSG_HOUSING_SVCS_GUILD_APPEND_NEIGHBORHOOD Player: {} NeighborhoodGuid: {}",
        player->GetGUID().ToString(), housingSvcsGuildAppendNeighborhood.NeighborhoodGuid.ToString());

    Guild* guild = player->GetGuild();
    if (!guild)
    {
        TC_LOG_DEBUG("housing", "CMSG_HOUSING_SVCS_GUILD_APPEND_NEIGHBORHOOD: Player not in guild");
        return;
    }

    Neighborhood* neighborhood = sNeighborhoodMgr.GetNeighborhood(housingSvcsGuildAppendNeighborhood.NeighborhoodGuid);
    if (!neighborhood)
    {
        TC_LOG_DEBUG("housing", "CMSG_HOUSING_SVCS_GUILD_APPEND_NEIGHBORHOOD: Neighborhood not found");
        return;
    }

    // Only the neighborhood owner or manager can append it to a guild
    if (!neighborhood->IsOwner(player->GetGUID()) && !neighborhood->IsManager(player->GetGUID()))
    {
        TC_LOG_DEBUG("housing", "CMSG_HOUSING_SVCS_GUILD_APPEND_NEIGHBORHOOD: Permission denied — not owner/manager");
        return;
    }

    neighborhood->SetGuildId(guild->GetId());

    WorldPackets::Housing::HousingSvcsGuildAppendNeighborhoodNotification response;
    response.Neighborhood.NeighborhoodGUID = housingSvcsGuildAppendNeighborhood.NeighborhoodGuid;
    if (neighborhood)
    {
        response.Neighborhood.OwnerGUID = neighborhood->GetOwnerGuid();
        response.Neighborhood.Name = neighborhood->GetName();
    }
    SendPacket(response.Write());

    TC_LOG_INFO("housing", "CMSG_HOUSING_SVCS_GUILD_APPEND_NEIGHBORHOOD: Player {} appended neighborhood {} to guild {}",
        player->GetName(), housingSvcsGuildAppendNeighborhood.NeighborhoodGuid.ToString(), guild->GetName());
}

void WorldSession::HandleHousingSvcsGuildRenameNeighborhood(WorldPackets::Housing::HousingSvcsGuildRenameNeighborhood const& housingSvcsGuildRenameNeighborhood)
{
    Player* player = GetPlayer();
    if (!player)
        return;

    TC_LOG_DEBUG("housing", "CMSG_HOUSING_SVCS_GUILD_RENAME_NEIGHBORHOOD Player: {} NeighborhoodGuid: {} NewName: '{}'",
        player->GetGUID().ToString(), housingSvcsGuildRenameNeighborhood.NeighborhoodGuid.ToString(),
        housingSvcsGuildRenameNeighborhood.NewName);

    // Validate name length and content
    std::string newName = housingSvcsGuildRenameNeighborhood.NewName;
    if (newName.empty() || newName.size() > HOUSING_MAX_NAME_LENGTH)
    {
        TC_LOG_DEBUG("housing", "CMSG_HOUSING_SVCS_GUILD_RENAME_NEIGHBORHOOD: Invalid name length {}", newName.size());
        return;
    }

    if (!ObjectMgr::IsValidCharterName(newName) || sObjectMgr->IsReservedName(newName))
    {
        TC_LOG_DEBUG("housing", "CMSG_HOUSING_SVCS_GUILD_RENAME_NEIGHBORHOOD: Name rejected by filter");
        return;
    }

    // Validate guild membership
    Guild* guild = player->GetGuild();
    if (!guild)
    {
        TC_LOG_DEBUG("housing", "CMSG_HOUSING_SVCS_GUILD_RENAME_NEIGHBORHOOD: Player not in guild");
        return;
    }

    // Perform the rename via NeighborhoodMgr
    HousingMap* housingMap = dynamic_cast<HousingMap*>(player->GetMap());
    if (housingMap)
    {
        Neighborhood* neighborhood = housingMap->GetNeighborhood();
        if (neighborhood && neighborhood->GetGuid() == housingSvcsGuildRenameNeighborhood.NeighborhoodGuid)
        {
            if (neighborhood->IsOwner(player->GetGUID()) || neighborhood->IsManager(player->GetGUID()))
            {
                neighborhood->SetName(newName);
                neighborhood->RefreshMirrorDataForOnlineMembers();

                // Broadcast name invalidation to all neighborhood members so they re-query the name
                WorldPackets::Housing::InvalidateNeighborhoodName invalidate;
                invalidate.NeighborhoodGuid = neighborhood->GetGuid();
                neighborhood->BroadcastPacket(invalidate.Write());

                TC_LOG_INFO("housing", "CMSG_HOUSING_SVCS_GUILD_RENAME_NEIGHBORHOOD: Player {} renamed neighborhood {} to '{}'",
                    player->GetName(), neighborhood->GetGuid().ToString(), newName);
            }
            else
            {
                TC_LOG_DEBUG("housing", "CMSG_HOUSING_SVCS_GUILD_RENAME_NEIGHBORHOOD: Permission denied — not owner/manager");
                return;
            }
        }
    }

    WorldPackets::Housing::HousingSvcsGuildRenameNeighborhoodNotification response;
    response.NewName = newName;
    SendPacket(response.Write());
}

void WorldSession::HandleHousingSvcsGuildGetHousingInfo(WorldPackets::Housing::HousingSvcsGuildGetHousingInfo const& housingSvcsGuildGetHousingInfo)
{
    Player* player = GetPlayer();
    if (!player)
        return;

    TC_LOG_DEBUG("housing", "CMSG_HOUSING_SVCS_GUILD_GET_HOUSING_INFO Player: {} GuildGuid: {}",
        player->GetGUID().ToString(), housingSvcsGuildGetHousingInfo.GuildGuid.ToString());

    WorldPackets::Housing::HousingSvcsGuildGetHousingInfoResponse response;

    Guild* guild = sGuildMgr->GetGuildByGuid(housingSvcsGuildGetHousingInfo.GuildGuid);
    if (!guild)
        guild = player->GetGuild();

    if (guild)
    {
        // Look up the guild's designated neighborhood
        Neighborhood* guildNeighborhood = sNeighborhoodMgr.GetNeighborhoodByGuildId(guild->GetId());
        if (guildNeighborhood)
        {
            WorldPackets::Housing::JamCliHouseFinderNeighborhood entry;
            entry.NeighborhoodGUID = guildNeighborhood->GetGuid();
            entry.OwnerGUID = guildNeighborhood->GetOwnerGuid();
            entry.Name = guildNeighborhood->GetName();

            // Add all houses in this neighborhood
            for (auto const& plot : guildNeighborhood->GetPlots())
            {
                if (!plot.IsOccupied())
                    continue;
                WorldPackets::Housing::JamCliHouse house;
                house.HouseGUID = plot.HouseGuid;
                house.OwnerGUID = plot.OwnerGuid;
                house.NeighborhoodGUID = guildNeighborhood->GetGuid();
                house.PlotIndex = plot.PlotIndex;
                entry.Houses.push_back(std::move(house));
            }
            response.Neighborhoods.push_back(std::move(entry));
        }
        else
        {
            // Fallback: if requesting player is in the guild and has housing, return their info
            if (player->GetGuildId() == guild->GetId())
            {
                Housing* housing = player->GetHousing();
                if (housing)
                {
                    Neighborhood* n = sNeighborhoodMgr.GetNeighborhood(housing->GetNeighborhoodGuid());
                    if (n)
                    {
                        WorldPackets::Housing::JamCliHouseFinderNeighborhood entry;
                        entry.NeighborhoodGUID = n->GetGuid();
                        entry.OwnerGUID = n->GetOwnerGuid();
                        entry.Name = n->GetName();
                        WorldPackets::Housing::JamCliHouse house;
                        house.HouseGUID = housing->GetHouseGuid();
                        house.OwnerGUID = player->GetGUID();
                        house.NeighborhoodGUID = n->GetGuid();
                        house.HouseLevel = static_cast<uint8>(housing->GetLevel());
                        entry.Houses.push_back(std::move(house));
                        response.Neighborhoods.push_back(std::move(entry));
                    }
                }
            }
        }
    }

    SendPacket(response.Write());
}

// ============================================================
// Phase 7 — Housing System Handlers
// ============================================================

void WorldSession::HandleHousingSystemHouseStatusQuery(WorldPackets::Housing::HousingSystemHouseStatusQuery const& /*housingSystemHouseStatusQuery*/)
{
    Player* player = GetPlayer();
    if (!player)
        return;

    TC_LOG_DEBUG("housing", "CMSG_HOUSING_SYSTEM_HOUSE_STATUS_QUERY Player: {}",
        player->GetGUID().ToString());

    Housing* housing = player->GetHousing();
    WorldPackets::Housing::HousingHouseStatusResponse response;
    response.AccountGuid = GetBattlenetAccountGUID();
    response.OwnerPlayerGuid = player->GetGUID();
    if (housing && !housing->GetHouseGuid().IsEmpty())
    {
        response.HouseGuid = housing->GetHouseGuid();
        response.NeighborhoodGuid = housing->GetNeighborhoodGuid();
        response.Status = 1;
        response.PermissionFlags = 0xE0; // owner: all three permissions
    }
    SendPacket(response.Write());
}

void WorldSession::HandleHousingSystemGetHouseInfoAlt(WorldPackets::Housing::HousingSystemGetHouseInfoAlt const& housingSystemGetHouseInfoAlt)
{
    Player* player = GetPlayer();
    if (!player)
        return;

    TC_LOG_DEBUG("housing", "CMSG_HOUSING_SYSTEM_GET_HOUSE_INFO_ALT Player: {} HouseGuid: {}",
        player->GetGUID().ToString(), housingSystemGetHouseInfoAlt.HouseGuid.ToString());

    Housing* housing = player->GetHousing();
    WorldPackets::Housing::HousingGetCurrentHouseInfoResponse response;

    if (housing && !housing->GetHouseGuid().IsEmpty())
    {
        response.House.HouseGuid = housing->GetHouseGuid();
        response.House.OwnerGuid = player->GetGUID();
        response.House.NeighborhoodGuid = housing->GetNeighborhoodGuid();
        response.House.PlotId = housing->GetPlotIndex();
        response.House.AccessFlags = housing->GetSettingsFlags();
        response.House.HasMoveOutTime = false;
        response.Result = static_cast<uint8>(HOUSING_RESULT_SUCCESS);
    }
    else
    {
        response.Result = static_cast<uint8>(HOUSING_RESULT_HOUSE_NOT_FOUND);
    }

    SendPacket(response.Write());
}

void WorldSession::HandleHousingSystemHouseSnapshot(WorldPackets::Housing::HousingSystemHouseSnapshot const& housingSystemHouseSnapshot)
{
    Player* player = GetPlayer();
    if (!player)
        return;

    TC_LOG_DEBUG("housing", "CMSG_HOUSING_SYSTEM_HOUSE_SNAPSHOT Player: {} HouseGuid: {} SnapshotType: {}",
        player->GetGUID().ToString(), housingSystemHouseSnapshot.HouseGuid.ToString(),
        housingSystemHouseSnapshot.SnapshotType);

    Housing* housing = player->GetHousing();

    WorldPackets::Housing::HousingSystemHouseSnapshotResponse response;
    response.Result = (housing && housing->GetHouseGuid() == housingSystemHouseSnapshot.HouseGuid)
        ? static_cast<uint8>(HOUSING_RESULT_SUCCESS)
        : static_cast<uint8>(HOUSING_RESULT_PERMISSION_DENIED);
    SendPacket(response.Write());
}

void WorldSession::HandleHousingSystemExportHouse(WorldPackets::Housing::HousingSystemExportHouse const& housingSystemExportHouse)
{
    Player* player = GetPlayer();
    if (!player)
        return;

    TC_LOG_DEBUG("housing", "CMSG_HOUSING_SYSTEM_EXPORT_HOUSE Player: {} HouseGuid: {}",
        player->GetGUID().ToString(), housingSystemExportHouse.HouseGuid.ToString());

    Housing* housing = player->GetHousing();
    if (!housing || housing->GetHouseGuid() != housingSystemExportHouse.HouseGuid)
    {
        WorldPackets::Housing::HousingExportHouseResponse response;
        response.Result = static_cast<uint8>(HOUSING_RESULT_PERMISSION_DENIED);
        response.HouseGuid = housingSystemExportHouse.HouseGuid;
        SendPacket(response.Write());
        return;
    }

    // Serialize house layout as JSON
    std::string json = "{";
    json += "\"level\":" + std::to_string(housing->GetLevel());
    json += ",\"houseType\":" + std::to_string(housing->GetHouseType());
    json += ",\"houseSize\":" + std::to_string(housing->GetHouseSize());

    // Export rooms
    json += ",\"rooms\":[";
    auto rooms = housing->GetRooms();
    for (size_t i = 0; i < rooms.size(); ++i)
    {
        if (i > 0) json += ",";
        json += "{\"entryId\":" + std::to_string(rooms[i]->RoomEntryId);
        json += ",\"slotIndex\":" + std::to_string(rooms[i]->SlotIndex);
        json += ",\"orientation\":" + std::to_string(rooms[i]->Orientation);
        json += ",\"mirrored\":" + std::string(rooms[i]->Mirrored ? "true" : "false");
        json += ",\"themeId\":" + std::to_string(rooms[i]->ThemeId);
        json += ",\"wallTextureId\":" + std::to_string(rooms[i]->WallTextureId);
        json += ",\"floorTextureId\":" + std::to_string(rooms[i]->FloorTextureId);
        json += ",\"ceilingTextureId\":" + std::to_string(rooms[i]->CeilingTextureId) + "}";
    }
    json += "]";

    // Export placed decor
    json += ",\"decor\":[";
    auto decor = housing->GetAllPlacedDecor();
    for (size_t i = 0; i < decor.size(); ++i)
    {
        if (i > 0) json += ",";
        json += "{\"entryId\":" + std::to_string(decor[i]->DecorEntryId);
        json += ",\"x\":" + std::to_string(decor[i]->PosX);
        json += ",\"y\":" + std::to_string(decor[i]->PosY);
        json += ",\"z\":" + std::to_string(decor[i]->PosZ);
        json += ",\"rotX\":" + std::to_string(decor[i]->RotationX);
        json += ",\"rotY\":" + std::to_string(decor[i]->RotationY);
        json += ",\"rotZ\":" + std::to_string(decor[i]->RotationZ);
        json += ",\"rotW\":" + std::to_string(decor[i]->RotationW) + "}";
    }
    json += "]";

    // Export fixtures
    json += ",\"fixtures\":[";
    auto fixtures = housing->GetFixtures();
    for (size_t i = 0; i < fixtures.size(); ++i)
    {
        if (i > 0) json += ",";
        json += "{\"pointId\":" + std::to_string(fixtures[i]->FixturePointId);
        json += ",\"optionId\":" + std::to_string(fixtures[i]->OptionId) + "}";
    }
    json += "]}";

    WorldPackets::Housing::HousingExportHouseResponse response;
    response.Result = static_cast<uint8>(HOUSING_RESULT_SUCCESS);
    response.HouseGuid = housingSystemExportHouse.HouseGuid;
    response.HasExportString = true;
    response.ExportString = json;
    SendPacket(response.Write());
}

void WorldSession::HandleHousingSystemUpdateHouseInfo(WorldPackets::Housing::HousingSystemUpdateHouseInfo const& housingSystemUpdateHouseInfo)
{
    Player* player = GetPlayer();
    if (!player)
        return;

    TC_LOG_DEBUG("housing", "CMSG_HOUSING_SYSTEM_UPDATE_HOUSE_INFO Player: {} HouseGuid: {} InfoType: {} Name: '{}' Desc: '{}'",
        player->GetGUID().ToString(), housingSystemUpdateHouseInfo.HouseGuid.ToString(),
        housingSystemUpdateHouseInfo.InfoType, housingSystemUpdateHouseInfo.HouseName,
        housingSystemUpdateHouseInfo.HouseDescription);

    Housing* housing = player->GetHousing();
    if (!housing || housing->GetHouseGuid() != housingSystemUpdateHouseInfo.HouseGuid)
    {
        TC_LOG_DEBUG("housing", "CMSG_HOUSING_SYSTEM_UPDATE_HOUSE_INFO: House not found or ownership mismatch");
        return;
    }

    // Validate name
    if (housingSystemUpdateHouseInfo.HouseName.length() > HOUSING_MAX_NAME_LENGTH)
    {
        TC_LOG_DEBUG("housing", "CMSG_HOUSING_SYSTEM_UPDATE_HOUSE_INFO: Name too long ({})", housingSystemUpdateHouseInfo.HouseName.length());
        return;
    }

    if (!housingSystemUpdateHouseInfo.HouseName.empty() &&
        (!ObjectMgr::IsValidCharterName(housingSystemUpdateHouseInfo.HouseName) || sObjectMgr->IsReservedName(housingSystemUpdateHouseInfo.HouseName)))
    {
        TC_LOG_DEBUG("housing", "CMSG_HOUSING_SYSTEM_UPDATE_HOUSE_INFO: Name rejected by filter");
        return;
    }

    // Persist name and description to DB
    housing->SetHouseNameDescription(housingSystemUpdateHouseInfo.HouseName, housingSystemUpdateHouseInfo.HouseDescription);

    WorldPackets::Housing::HousingUpdateHouseInfo response;
    response.HouseName = housingSystemUpdateHouseInfo.HouseName;
    response.HouseDescription = housingSystemUpdateHouseInfo.HouseDescription;
    response.Result = 0;
    SendPacket(response.Write());

    TC_LOG_INFO("housing", "CMSG_HOUSING_SYSTEM_UPDATE_HOUSE_INFO: Player {} updated house {} name='{}' desc='{}'",
        player->GetName(), housing->GetHouseGuid().ToString(),
        housing->GetHouseName(), housing->GetHouseDescription());
}
