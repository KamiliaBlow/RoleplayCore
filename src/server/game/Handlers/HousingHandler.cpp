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
#include <cmath>
#include "DatabaseEnv.h"
#include "RealmList.h"
#include "AreaTrigger.h"
#include "DB2Stores.h"
#include "Guild.h"
#include "GuildMgr.h"
#include "HouseInteriorMap.h"
#include "Housing.h"
#include "HousingDefines.h"
#include "HousingMap.h"
#include "HousingMgr.h"
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
#include "SpellAuraDefines.h"
#include "SpellMgr.h"
#include "SpellPackets.h"

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
        response.Result = static_cast<uint32>(HOUSING_RESULT_HOUSE_NOT_FOUND);
        SendPacket(response.Write());
        return;
    }

    // Persist the house position to the database
    housing->SetHousePosition(
        houseExteriorCommitPosition.PositionX,
        houseExteriorCommitPosition.PositionY,
        houseExteriorCommitPosition.PositionZ,
        houseExteriorCommitPosition.Facing);

    // Despawn and respawn house structure at new position
    if (HousingMap* housingMap = dynamic_cast<HousingMap*>(player->GetMap()))
    {
        uint8 plotIndex = housing->GetPlotIndex();

        // Despawn old house (door GO + all MeshObjects)
        housingMap->DespawnHouseForPlot(plotIndex);

        // Respawn at new position with current exterior component and house type
        Position newPos(houseExteriorCommitPosition.PositionX,
            houseExteriorCommitPosition.PositionY,
            houseExteriorCommitPosition.PositionZ,
            houseExteriorCommitPosition.Facing);

        housingMap->SpawnHouseForPlot(plotIndex, &newPos,
            static_cast<int32>(housing->GetCoreExteriorComponentID()),
            static_cast<int32>(housing->GetHouseType()));
    }

    WorldPackets::Housing::HouseExteriorSetHousePositionResponse response;
    response.Result = static_cast<uint32>(HOUSING_RESULT_SUCCESS);
    response.HouseGuid = housing->GetHouseGuid();
    SendPacket(response.Write());

    TC_LOG_INFO("housing", "CMSG_HOUSE_EXTERIOR_COMMIT_POSITION: Player {} repositioned house at ({}, {}, {}, {:.2f})",
        player->GetGUID().ToString(),
        houseExteriorCommitPosition.PositionX, houseExteriorCommitPosition.PositionY,
        houseExteriorCommitPosition.PositionZ, houseExteriorCommitPosition.Facing);
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
        response.Result = static_cast<uint32>(HOUSING_RESULT_HOUSE_NOT_FOUND);
        SendPacket(response.Write());
        return;
    }

    // Persist the exterior lock state
    housing->SetExteriorLocked(houseExteriorLock.Locked);

    WorldPackets::Housing::HouseExteriorLockResponse response;
    response.Result = static_cast<uint32>(HOUSING_RESULT_SUCCESS);
    response.HouseGuid = houseExteriorLock.HouseGuid;
    response.Locked = houseExteriorLock.Locked;
    SendPacket(response.Write());

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

    Housing* housing = player->GetHousing();
    if (!housing)
        return;

    // Clear editing mode and interior state when leaving
    housing->SetEditorMode(HOUSING_EDITOR_MODE_NONE);
    housing->SetInInterior(false);

    // Send SMSG_HOUSE_INTERIOR_LEAVE_HOUSE_RESPONSE with ExitingHouse reason
    WorldPackets::Housing::HouseInteriorLeaveHouseResponse leaveResponse;
    leaveResponse.TeleportReason = 9; // HousingTeleportReason::ExitingHouse
    SendPacket(leaveResponse.Write());

    // Send updated house status with Status=0 (exterior)
    WorldPackets::Housing::HousingHouseStatusResponse statusResponse;
    statusResponse.HouseGuid = housing->GetHouseGuid();
    statusResponse.NeighborhoodGuid = GetBattlenetAccountGUID();
    statusResponse.OwnerPlayerGuid = player->GetGUID();
    statusResponse.Status = 0;
    SendPacket(statusResponse.Write());

    // Teleport player back to the neighborhood map at the plot's visitor landing point.
    // Try to use the HouseInteriorMap's stored source info first (most reliable),
    // then fall back to resolving from the Housing object's neighborhood.
    uint32 worldMapId = 0;
    uint8 plotIndex = housing->GetPlotIndex();
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

    // Get the plot's visitor landing position (TeleportPosition) for the exit point
    float exitX = 0.0f, exitY = 0.0f, exitZ = 0.0f, exitO = 0.0f;
    bool foundPlot = false;

    if (neighborhoodMapId != 0)
    {
        std::vector<NeighborhoodPlotData const*> plots = sHousingMgr.GetPlotsForMap(neighborhoodMapId);
        for (NeighborhoodPlotData const* plot : plots)
        {
            if (plot->PlotIndex == static_cast<int32>(plotIndex))
            {
                exitX = plot->TeleportPosition[0];
                exitY = plot->TeleportPosition[1];
                exitZ = plot->TeleportPosition[2];
                exitO = plot->TeleportFacing;
                foundPlot = true;
                break;
            }
        }
    }

    if (!foundPlot)
    {
        // Fallback: use neighborhood center
        NeighborhoodMapData const* mapData = sHousingMgr.GetNeighborhoodMapData(neighborhoodMapId);
        if (mapData)
        {
            exitX = mapData->Origin[0];
            exitY = mapData->Origin[1];
            exitZ = mapData->Origin[2];
        }
        TC_LOG_WARN("housing", "CMSG_HOUSE_INTERIOR_LEAVE_HOUSE: No plot data for plotIndex {}, "
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
    TC_LOG_DEBUG("housing", ">>> CMSG_HOUSING_DECOR_SET_EDIT_MODE Active={}", housingDecorSetEditMode.Active);

    Player* player = GetPlayer();
    if (!player)
        return;

    Housing* housing = player->GetHousing();
    if (!housing)
    {
        TC_LOG_ERROR("housing", "HandleHousingDecorSetEditMode: GetHousing() returned null for player {}",
            player->GetGUID().ToString());
        WorldPackets::Housing::HousingDecorSetEditModeResponse response;
        response.Result = static_cast<uint8>(HOUSING_RESULT_HOUSE_NOT_FOUND);
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
        response.HouseGuid2 = housing->GetNeighborhoodGuid();
        response.Result = static_cast<uint8>(HOUSING_RESULT_HOUSE_NOT_FOUND);
        SendPacket(response.Write());
        return;
    }

    {
        UF::PlayerHouseInfoComponentData const& phData = *player->m_playerHouseInfoComponentData;
        TC_LOG_DEBUG("housing", "  BEFORE SetEditorMode: EditorMode={} HouseCount={}",
            uint32(*phData.EditorMode), phData.Houses.size());
        for (uint32 i = 0; i < phData.Houses.size(); ++i)
        {
            TC_LOG_DEBUG("housing", "    Houses[{}]: Guid={} MapID={} PlotID={} Level={} NeighborhoodGUID={}",
                i, phData.Houses[i].Guid.ToString(), phData.Houses[i].MapID,
                phData.Houses[i].PlotID, phData.Houses[i].Level,
                phData.Houses[i].NeighborhoodGUID.ToString());
        }
    }

    // Set edit mode via UpdateField — client needs both the UpdateField change AND the SMSG response
    housing->SetEditorMode(targetMode);

    // Sniff-verified wire format: PackedGUID HouseGuid + PackedGUID BNetAccountGuid
    // + uint32 DecorCount + uint8 Result + DecorCount × PackedGUID DecorGUIDs
    // Sniff byte-level analysis: HouseGuid2 has hi byte7=0x78 = HighGuid::BNetAccount (30)
    WorldPackets::Housing::HousingDecorSetEditModeResponse response;
    response.HouseGuid = housing->GetHouseGuid();
    response.HouseGuid2 = GetBattlenetAccountGUID();
    response.Result = static_cast<uint8>(HOUSING_RESULT_SUCCESS);

    if (housingDecorSetEditMode.Active)
    {
        // --- Edit mode ENTER ---
        // Sniff-verified packet order: AURA_UPDATE → SPELL_START → SPELL_GO → EDIT_MODE_RESPONSE → UPDATE_OBJECT

        // 1. Apply edit mode aura (spell 1263303) — creates "phased-out" visual effect
        // Sniff: aura at slot 51, Flags=NoCaster, ActiveFlags=15, CastLevel=36
        if (sSpellMgr->GetSpellInfo(SPELL_HOUSING_EDIT_MODE_AURA, DIFFICULTY_NONE))
        {
            player->CastSpell(player, SPELL_HOUSING_EDIT_MODE_AURA, true);
        }
        else
        {
            // Spell not in DB2 — manually construct and send the aura update packet
            WorldPackets::Spells::AuraUpdate auraUpdate;
            auraUpdate.UpdateAll = false;
            auraUpdate.UnitGUID = player->GetGUID();

            WorldPackets::Spells::AuraInfo auraInfo;
            auraInfo.Slot = 51;
            auraInfo.AuraData.emplace();
            auraInfo.AuraData->SpellID = SPELL_HOUSING_EDIT_MODE_AURA;
            auraInfo.AuraData->Flags = AFLAG_NOCASTER;
            auraInfo.AuraData->ActiveFlags = 15;
            auraInfo.AuraData->CastLevel = 36;
            auraInfo.AuraData->Applications = 0;
            auraUpdate.Auras.push_back(std::move(auraInfo));

            player->SendDirectMessage(auraUpdate.Write());
            TC_LOG_DEBUG("housing", "  Edit mode aura {} sent via manual SMSG_AURA_UPDATE (spell not in DB2)",
                SPELL_HOUSING_EDIT_MODE_AURA);
        }

        // 2. DecorGUIDs: sniff shows the PLAYER's own GUID (not placed decor GUIDs).
        // The client checks if its own GUID is present to decide enter vs exit.
        response.DecorGuids.push_back(player->GetGUID());

        // 3. Send the edit mode response BEFORE the UpdateObject
        SendPacket(response.Write());

        // 4. Apply edit mode unit flags (sniff: Flags adds PACIFIED, Flags2 adds NO_ACTIONS,
        // SilencedSchoolMask=127 prevents all spellcasting while in editor)
        player->SetUnitFlag(UNIT_FLAG_PACIFIED);
        player->SetUnitFlag2(UNIT_FLAG2_NO_ACTIONS);
        player->ReplaceAllSilencedSchoolMask(SPELL_SCHOOL_MASK_ALL);

        // 5. Send BNetAccount entity update with FHousingStorage_C fragment.
        // The storage data was populated during Housing::LoadFromDB. Trigger an update
        // to ensure the client has the latest decor inventory for the edit mode UI.
        // Sniff: BNetAccount CreateObject1 only sent on FIRST enter; SendUpdateToPlayer
        // handles this automatically via HaveAtClient check.
        GetBattlenetAccount().SendUpdateToPlayer(player);

        TC_LOG_DEBUG("housing", "  EditMode ENTER: PlayerGUID={} HouseGuid2={} flags set (PACIFIED|NO_ACTIONS|SilencedAll)",
            player->GetGUID().ToString(), response.HouseGuid2.ToString());
    }
    else
    {
        // --- Edit mode EXIT ---
        // Sniff-verified packet order: AURA_UPDATE → EDIT_MODE_RESPONSE → UPDATE_OBJECT
        // No SPELL_START/SPELL_GO on exit.

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

        // 2. Send the edit mode response (DecorCount=0 = exit)
        SendPacket(response.Write());

        // 3. Remove edit mode unit flags
        player->RemoveUnitFlag(UNIT_FLAG_PACIFIED);
        player->RemoveUnitFlag2(UNIT_FLAG2_NO_ACTIONS);
        player->ReplaceAllSilencedSchoolMask(SpellSchoolMask(0));

        TC_LOG_DEBUG("housing", "  EditMode EXIT: flags cleared, HouseGuid2={}",
            response.HouseGuid2.ToString());
    }

    TC_LOG_DEBUG("housing", "  EDIT_MODE_RESPONSE: HouseGuid={} HouseGuid2={} DecorCount={} Result={}",
        response.HouseGuid.ToString(), response.HouseGuid2.ToString(),
        uint32(response.DecorGuids.size()), response.Result);

    if (player->m_playerHouseInfoComponentData.has_value())
    {
        UF::PlayerHouseInfoComponentData const& phData = *player->m_playerHouseInfoComponentData;
        TC_LOG_DEBUG("housing", "  AFTER: EditorMode={} (target={}), Active={}",
            uint32(*phData.EditorMode), uint32(targetMode), housingDecorSetEditMode.Active);
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
        response.Result = static_cast<uint32>(HOUSING_RESULT_HOUSE_NOT_FOUND);
        SendPacket(response.Write());
        return;
    }

    // Client sends Euler angles (Yaw/Pitch/Roll) — convert to quaternion for PlaceDecor
    float halfYaw = housingDecorPlace.Yaw * 0.5f;
    float halfPitch = housingDecorPlace.Pitch * 0.5f;
    float halfRoll = housingDecorPlace.Roll * 0.5f;
    float cy = std::cos(halfYaw), sy = std::sin(halfYaw);
    float cp = std::cos(halfPitch), sp = std::sin(halfPitch);
    float cr = std::cos(halfRoll), sr = std::sin(halfRoll);
    float rotW = cy * cp * cr + sy * sp * sr;
    float rotX = cy * cp * sr - sy * sp * cr;
    float rotY = sy * cp * sr + cy * sp * cr;
    float rotZ = sy * cp * cr - cy * sp * sr;

    HousingResult result = housing->PlaceDecor(housingDecorPlace.DecorRecID,
        housingDecorPlace.PositionX, housingDecorPlace.PositionY, housingDecorPlace.PositionZ,
        rotX, rotY, rotZ, rotW,
        housingDecorPlace.RoomGuid);

    // Spawn decor GO on the map if placement succeeded
    if (result == HOUSING_RESULT_SUCCESS)
    {
        // Find the most recently placed decor (highest GUID)
        ObjectGuid newestDecorGuid;
        for (auto const& [guid, decor] : housing->GetPlacedDecorMap())
        {
            if (decor.DecorEntryId == housingDecorPlace.DecorRecID &&
                (newestDecorGuid.IsEmpty() || guid.GetCounter() > newestDecorGuid.GetCounter()))
                newestDecorGuid = guid;
        }

        if (!newestDecorGuid.IsEmpty())
        {
            if (Housing::PlacedDecor const* newDecor = housing->GetPlacedDecor(newestDecorGuid))
            {
                if (HousingMap* housingMap = dynamic_cast<HousingMap*>(player->GetMap()))
                    housingMap->SpawnDecorItem(housing->GetPlotIndex(), *newDecor, housing->GetHouseGuid());
            }
        }

        // Sniff: UPDATE_OBJECT (BNetAccount with ChangeType=3, HouseGUID=set) arrives BEFORE response.
        // PlaceDecor already called SetHousingDecorStorageEntry internally; flush now.
        GetBattlenetAccount().SendUpdateToPlayer(player);
    }

    // Sniff-verified (26 bytes): PackedGUID PlayerGuid + uint32 Result + PackedGUID DecorGuid + uint8 Status
    WorldPackets::Housing::HousingDecorPlaceResponse response;
    response.PlayerGuid = player->GetGUID();
    response.Result = static_cast<uint32>(result);
    response.DecorGuid = housingDecorPlace.DecorGuid;
    response.Status = 0;
    SendPacket(response.Write());

    TC_LOG_INFO("housing", "CMSG_HOUSING_DECOR_PLACE DecorGuid: {} DecorRecID: {} Result: {} Pos: ({}, {}, {}) Yaw: {} Scale: {}",
        housingDecorPlace.DecorGuid.ToString(), housingDecorPlace.DecorRecID, uint32(result),
        housingDecorPlace.PositionX, housingDecorPlace.PositionY, housingDecorPlace.PositionZ,
        housingDecorPlace.Yaw, housingDecorPlace.Scale);
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
        response.Result = static_cast<uint32>(HOUSING_RESULT_HOUSE_NOT_FOUND);
        SendPacket(response.Write());
        return;
    }

    HousingResult result = housing->MoveDecor(housingDecorMove.DecorGuid,
        housingDecorMove.PositionX, housingDecorMove.PositionY, housingDecorMove.PositionZ,
        housingDecorMove.RotationX, housingDecorMove.RotationY,
        housingDecorMove.RotationZ, housingDecorMove.RotationW);

    // Update decor GO position on the map
    if (result == HOUSING_RESULT_SUCCESS)
    {
        if (HousingMap* housingMap = dynamic_cast<HousingMap*>(player->GetMap()))
        {
            Position newPos(housingDecorMove.PositionX, housingDecorMove.PositionY, housingDecorMove.PositionZ);
            QuaternionData newRot(housingDecorMove.RotationX, housingDecorMove.RotationY,
                housingDecorMove.RotationZ, housingDecorMove.RotationW);
            housingMap->UpdateDecorPosition(housing->GetPlotIndex(), housingDecorMove.DecorGuid, newPos, newRot);
        }
    }

    WorldPackets::Housing::HousingDecorMoveResponse response;
    response.Result = static_cast<uint32>(result);
    response.DecorGuid = housingDecorMove.DecorGuid;
    SendPacket(response.Write());

    TC_LOG_INFO("housing", "CMSG_HOUSING_DECOR_MOVE_DECOR DecorGuid: {}, Result: {}",
        housingDecorMove.DecorGuid.ToString(), uint32(result));
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

    // Capture plotIndex before RemoveDecor (which may clear the data)
    uint8 plotIndex = housing->GetPlotIndex();
    ObjectGuid decorGuid = housingDecorRemove.DecorGuid;

    HousingResult result = housing->RemoveDecor(decorGuid);

    // Despawn the decor GO from the map and update Account entity
    if (result == HOUSING_RESULT_SUCCESS)
    {
        if (HousingMap* housingMap = dynamic_cast<HousingMap*>(player->GetMap()))
            housingMap->DespawnDecorItem(plotIndex, decorGuid);

        // Sniff: RemoveDecor deletes the Account entry, but retail keeps it with HouseGUID=Empty
        // Re-add the entry with HouseGUID=Empty to return it to storage
        Battlenet::Account& account = GetBattlenetAccount();
        account.SetHousingDecorStorageEntry(decorGuid, ObjectGuid::Empty, 0);
        account.SendUpdateToPlayer(player);
    }

    WorldPackets::Housing::HousingDecorRemoveResponse response;
    response.DecorGuid = decorGuid;
    response.Result = static_cast<uint8>(result);
    SendPacket(response.Write());

    TC_LOG_INFO("housing", "CMSG_HOUSING_DECOR_REMOVE_PLACED_DECOR_ENTRY DecorGuid: {}, Result: {}",
        decorGuid.ToString(), uint32(result));
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
        response.LockState = 0;
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
        response.LockState = 0;
        SendPacket(response.Write());
        return;
    }

    HousingResult result = housing->SetDecorLocked(housingDecorLock.DecorGuid, housingDecorLock.Locked);

    // Sniff wire format: DecorGuid + PlayerGuid + uint8 LockState
    // LockState encodes 2 bits: 0xC0 = locked+anchor, 0x40 = unlocked+anchor
    WorldPackets::Housing::HousingDecorLockResponse response;
    response.DecorGuid = housingDecorLock.DecorGuid;
    response.PlayerGuid = player->GetGUID();
    if (result == HOUSING_RESULT_SUCCESS)
        response.LockState = housingDecorLock.Locked ? 0xC0 : 0x40;
    else
        response.LockState = 0;
    SendPacket(response.Write());

    TC_LOG_INFO("housing", "CMSG_HOUSING_DECOR_LOCK DecorGuid: {} (entry: {}), Locked: {}, LockState: 0x{:02X}",
        housingDecorLock.DecorGuid.ToString(), decor->DecorEntryId, housingDecorLock.Locked, response.LockState);
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
        response.Result = static_cast<uint32>(HOUSING_RESULT_HOUSE_NOT_FOUND);
        SendPacket(response.Write());
        return;
    }

    std::array<uint32, MAX_HOUSING_DYE_SLOTS> dyeSlots = {};
    for (size_t i = 0; i < housingDecorSetDyeSlots.DyeColorID.size() && i < MAX_HOUSING_DYE_SLOTS; ++i)
        dyeSlots[i] = static_cast<uint32>(housingDecorSetDyeSlots.DyeColorID[i]);

    HousingResult result = housing->CommitDecorDyes(housingDecorSetDyeSlots.DecorGuid, dyeSlots);

    WorldPackets::Housing::HousingDecorSystemSetDyeSlotsResponse response;
    response.Result = static_cast<uint32>(result);
    response.DecorGuid = housingDecorSetDyeSlots.DecorGuid;
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
        response.Result = static_cast<uint32>(HOUSING_RESULT_HOUSE_NOT_FOUND);
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
    response.Result = static_cast<uint32>(result);
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
        response.Result = static_cast<uint32>(HOUSING_RESULT_HOUSE_NOT_FOUND);
        SendPacket(response.Write());
        return;
    }

    HousingResult result = housing->DestroyAllCopies(housingDecorDeleteFromStorageById.DecorRecID);

    WorldPackets::Housing::HousingDecorDeleteFromStorageResponse response;
    response.Result = static_cast<uint32>(result);
    SendPacket(response.Write());

    TC_LOG_INFO("housing", "CMSG_HOUSING_DECOR_DELETE_FROM_STORAGE_BY_ID DecorRecID: {}, Result: {}",
        housingDecorDeleteFromStorageById.DecorRecID, uint32(result));
}

void WorldSession::HandleHousingDecorRequestStorage(WorldPackets::Housing::HousingDecorRequestStorage const& housingDecorRequestStorage)
{
    Player* player = GetPlayer();
    if (!player)
        return;

    Housing* housing = player->GetHousing();
    if (!housing)
    {
        WorldPackets::Housing::HousingDecorRequestStorageResponse response;
        response.BNetAccountGuid = GetBattlenetAccountGUID();
        response.ResultCode = static_cast<uint8>(HOUSING_RESULT_HOUSE_NOT_FOUND);
        response.HasData = false;
        SendPacket(response.Write());
        TC_LOG_INFO("housing", "CMSG_HOUSING_DECOR_REQUEST_STORAGE: Player {} has no house",
            player->GetGUID().ToString());
        return;
    }

    // IDA-verified: SMSG_HOUSING_DECOR_REQUEST_STORAGE_RESPONSE is an acknowledgement
    // (PackedGUID + uint8 ResultCode + uint8 Flags). Actual decor entries are delivered
    // via UpdateObject housing data fragments, not inline in this packet.
    std::vector<Housing::CatalogEntry const*> entries = housing->GetCatalogEntries();

    WorldPackets::Housing::HousingDecorRequestStorageResponse response;
    response.BNetAccountGuid = GetBattlenetAccountGUID();
    response.ResultCode = static_cast<uint8>(HOUSING_RESULT_SUCCESS);
    response.HasData = !entries.empty();
    SendPacket(response.Write());

    TC_LOG_INFO("housing", "CMSG_HOUSING_DECOR_REQUEST_STORAGE HouseGuid: {}, CatalogEntries: {}",
        housingDecorRequestStorage.HouseGuid.ToString(), uint32(entries.size()));
}

void WorldSession::HandleHousingDecorRedeemDeferredDecor(WorldPackets::Housing::HousingDecorRedeemDeferredDecor const& housingDecorRedeemDeferredDecor)
{
    Player* player = GetPlayer();
    if (!player)
        return;

    uint32 decorEntryId = housingDecorRedeemDeferredDecor.DeferredDecorID;
    uint32 sequenceIndex = housingDecorRedeemDeferredDecor.RedemptionToken;

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

    // Add the deferred decor to the player's catalog/storage
    HousingResult result = housing->AddToCatalog(decorEntryId);
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
    SendPacket(response.Write());

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

    TC_LOG_INFO("housing", "CMSG_HOUSING_DECOR_REDEEM_DEFERRED_DECOR: Player {} redeemed decor {} → GUID {} (SourceType=3, Seq={})",
        player->GetGUID().ToString(), decorEntryId, decorGuid.ToString(), sequenceIndex);
}

// ============================================================
// Fixture System
// ============================================================

void WorldSession::HandleHousingFixtureSetEditMode(WorldPackets::Housing::HousingFixtureSetEditMode const& housingFixtureSetEditMode)
{
    Player* player = GetPlayer();
    if (!player)
        return;

    Housing* housing = player->GetHousing();
    if (!housing)
    {
        WorldPackets::Housing::HousingFixtureSetEditModeResponse response;
        response.Result = static_cast<uint32>(HOUSING_RESULT_HOUSE_NOT_FOUND);
        SendPacket(response.Write());
        return;
    }

    housing->SetEditorMode(housingFixtureSetEditMode.Active ? HOUSING_EDITOR_MODE_CUSTOMIZE : HOUSING_EDITOR_MODE_NONE);

    WorldPackets::Housing::HousingFixtureSetEditModeResponse response;
    response.Result = static_cast<uint32>(HOUSING_RESULT_SUCCESS);
    response.Active = housingFixtureSetEditMode.Active;
    SendPacket(response.Write());

    TC_LOG_INFO("housing", "CMSG_HOUSING_FIXTURE_SET_EDITOR_MODE_ACTIVE Active: {}", housingFixtureSetEditMode.Active);
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
        response.Result = static_cast<uint32>(HOUSING_RESULT_HOUSE_NOT_FOUND);
        SendPacket(response.Write());
        return;
    }

    // Validate ExteriorComponentID against DB2 store
    uint32 componentID = housingFixtureSetCoreFixture.ExteriorComponentID;
    ExteriorComponentEntry const* componentEntry = sExteriorComponentStore.LookupEntry(componentID);
    if (!componentEntry)
    {
        TC_LOG_DEBUG("housing", "CMSG_HOUSING_FIXTURE_SET_CORE_FIXTURE ExteriorComponentID {} not found in DB2",
            componentID);
        WorldPackets::Housing::HousingFixtureSetCoreFixtureResponse response;
        response.Result = static_cast<uint32>(HOUSING_RESULT_FIXTURE_NOT_FOUND);
        SendPacket(response.Write());
        return;
    }

    TC_LOG_DEBUG("housing", "CMSG_HOUSING_FIXTURE_SET_CORE_FIXTURE DB2 lookup: ExteriorComponentID={}, Name='{}', Type={}, Slot={}, HookID={}, ComponentGroupID={}, TypeID={}",
        componentID, componentEntry->Name[DEFAULT_LOCALE], componentEntry->Type, componentEntry->Slot,
        componentEntry->HookID, componentEntry->ComponentGroupID, componentEntry->ExteriorComponentTypeID);

    // Validate ExteriorComponentType if present
    if (componentEntry->ExteriorComponentTypeID)
    {
        ExteriorComponentTypeEntry const* typeEntry = sExteriorComponentTypeStore.LookupEntry(componentEntry->ExteriorComponentTypeID);
        if (typeEntry)
            TC_LOG_DEBUG("housing", "  -> ExteriorComponentType: ID={}, Name='{}', Flags={}", typeEntry->ID, typeEntry->Name[DEFAULT_LOCALE], typeEntry->Flags);
        else
            TC_LOG_DEBUG("housing", "  -> ExteriorComponentTypeID {} NOT FOUND in DB2", componentEntry->ExteriorComponentTypeID);
    }

    HousingResult result = housing->SelectFixtureOption(componentID, 0);

    WorldPackets::Housing::HousingFixtureSetCoreFixtureResponse response;
    response.Result = static_cast<uint32>(result);
    response.FixtureRecordID = componentID;
    SendPacket(response.Write());

    if (result == HOUSING_RESULT_SUCCESS)
    {
        WorldPackets::Housing::AccountExteriorFixtureCollectionUpdate collectionUpdate;
        collectionUpdate.FixtureID = componentID;
        SendPacket(collectionUpdate.Write());
    }

    TC_LOG_DEBUG("housing", "CMSG_HOUSING_FIXTURE_SET_CORE_FIXTURE FixtureGuid: {}, ExteriorComponentID: {}, Result: {}",
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
        response.Result = static_cast<uint32>(HOUSING_RESULT_HOUSE_NOT_FOUND);
        SendPacket(response.Write());
        return;
    }

    uint32 hookID = housingFixtureCreateFixture.ExteriorComponentHookID;
    uint32 componentType = housingFixtureCreateFixture.ExteriorComponentType;

    // Validate ExteriorComponentHook against DB2 store
    ExteriorComponentHookEntry const* hookEntry = sExteriorComponentHookStore.LookupEntry(hookID);
    if (!hookEntry)
    {
        TC_LOG_DEBUG("housing", "CMSG_HOUSING_FIXTURE_CREATE_FIXTURE ExteriorComponentHookID {} not found in DB2", hookID);
        WorldPackets::Housing::HousingFixtureCreateFixtureResponse response;
        response.Result = static_cast<uint32>(HOUSING_RESULT_FIXTURE_NOT_FOUND);
        SendPacket(response.Write());
        return;
    }

    TC_LOG_DEBUG("housing", "CMSG_HOUSING_FIXTURE_CREATE_FIXTURE DB2 lookup: HookID={}, Position=({:.1f},{:.1f},{:.1f}), TypeID={}, ParentComponentID={}",
        hookID, hookEntry->Position[0], hookEntry->Position[1], hookEntry->Position[2],
        hookEntry->ExteriorComponentTypeID, hookEntry->ExteriorComponentID);

    // Validate ExteriorComponentType against DB2 store
    if (componentType)
    {
        ExteriorComponentTypeEntry const* typeEntry = sExteriorComponentTypeStore.LookupEntry(componentType);
        if (typeEntry)
            TC_LOG_DEBUG("housing", "  -> ExteriorComponentType: ID={}, Name='{}', Flags={}", typeEntry->ID, typeEntry->Name[DEFAULT_LOCALE], typeEntry->Flags);
        else
        {
            TC_LOG_DEBUG("housing", "CMSG_HOUSING_FIXTURE_CREATE_FIXTURE ExteriorComponentType {} not found in DB2", componentType);
            WorldPackets::Housing::HousingFixtureCreateFixtureResponse response;
            response.Result = static_cast<uint32>(HOUSING_RESULT_FIXTURE_NOT_FOUND);
            SendPacket(response.Write());
            return;
        }
    }

    HousingResult result = housing->SelectFixtureOption(hookID, componentType);

    WorldPackets::Housing::HousingFixtureCreateFixtureResponse response;
    response.Result = static_cast<uint32>(result);
    SendPacket(response.Write());

    if (result == HOUSING_RESULT_SUCCESS)
    {
        WorldPackets::Housing::AccountExteriorFixtureCollectionUpdate collectionUpdate;
        collectionUpdate.FixtureID = hookID;
        SendPacket(collectionUpdate.Write());
    }

    TC_LOG_DEBUG("housing", "CMSG_HOUSING_FIXTURE_CREATE_FIXTURE ExteriorComponentType: {}, ExteriorComponentHookID: {}, Result: {}",
        componentType, hookID, uint32(result));
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
        response.Result = static_cast<uint32>(HOUSING_RESULT_HOUSE_NOT_FOUND);
        SendPacket(response.Write());
        return;
    }

    uint32 componentID = housingFixtureDeleteFixture.ExteriorComponentID;

    // Validate ExteriorComponentID against DB2 store
    ExteriorComponentEntry const* componentEntry = sExteriorComponentStore.LookupEntry(componentID);
    if (!componentEntry)
    {
        TC_LOG_DEBUG("housing", "CMSG_HOUSING_FIXTURE_DELETE_FIXTURE ExteriorComponentID {} not found in DB2", componentID);
        WorldPackets::Housing::HousingFixtureDeleteFixtureResponse response;
        response.Result = static_cast<uint32>(HOUSING_RESULT_FIXTURE_NOT_FOUND);
        SendPacket(response.Write());
        return;
    }

    TC_LOG_DEBUG("housing", "CMSG_HOUSING_FIXTURE_DELETE_FIXTURE DB2 lookup: ExteriorComponentID={}, Name='{}', Type={}, Slot={}",
        componentID, componentEntry->Name[DEFAULT_LOCALE], componentEntry->Type, componentEntry->Slot);

    HousingResult result = housing->RemoveFixture(componentID);

    WorldPackets::Housing::HousingFixtureDeleteFixtureResponse response;
    response.Result = static_cast<uint32>(result);
    response.FixtureGuid = housingFixtureDeleteFixture.FixtureGuid;
    SendPacket(response.Write());

    TC_LOG_DEBUG("housing", "CMSG_HOUSING_FIXTURE_DELETE_FIXTURE FixtureGuid: {}, ExteriorComponentID: {}, Result: {}",
        housingFixtureDeleteFixture.FixtureGuid.ToString(), componentID, uint32(result));
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
        response.Result = static_cast<uint32>(HOUSING_RESULT_HOUSE_NOT_FOUND);
        SendPacket(response.Write());
        return;
    }

    // Validate house size against HousingFixtureSize enum (1=Any, 2=Small, 3=Medium, 4=Large)
    uint8 requestedSize = housingFixtureSetHouseSize.Size;
    if (requestedSize < HOUSING_FIXTURE_SIZE_ANY || requestedSize > HOUSING_FIXTURE_SIZE_LARGE)
    {
        WorldPackets::Housing::HousingFixtureSetHouseSizeResponse response;
        response.Result = static_cast<uint32>(HOUSING_RESULT_HOUSE_EXTERIOR_SIZE_NOT_AVAILABLE);
        SendPacket(response.Write());

        TC_LOG_INFO("housing", "CMSG_HOUSING_FIXTURE_SET_HOUSE_SIZE HouseGuid: {}, Size: {} REJECTED (invalid size)",
            housingFixtureSetHouseSize.HouseGuid.ToString(), requestedSize);
        return;
    }

    // Reject if already that size
    if (requestedSize == housing->GetHouseSize())
    {
        WorldPackets::Housing::HousingFixtureSetHouseSizeResponse response;
        response.Result = static_cast<uint32>(HOUSING_RESULT_HOUSE_EXTERIOR_ALREADY_THAT_SIZE);
        SendPacket(response.Write());

        TC_LOG_INFO("housing", "CMSG_HOUSING_FIXTURE_SET_HOUSE_SIZE HouseGuid: {}, Size: {} REJECTED (already that size)",
            housingFixtureSetHouseSize.HouseGuid.ToString(), requestedSize);
        return;
    }

    // Persist the new house size
    housing->SetHouseSize(requestedSize);

    // Respawn house MeshObjects with updated size
    if (HousingMap* housingMap = dynamic_cast<HousingMap*>(player->GetMap()))
    {
        uint8 plotIndex = housing->GetPlotIndex();
        housingMap->DespawnHouseForPlot(plotIndex);
        housingMap->SpawnHouseForPlot(plotIndex, nullptr,
            static_cast<int32>(housing->GetCoreExteriorComponentID()),
            static_cast<int32>(housing->GetHouseType()));
    }

    WorldPackets::Housing::HousingFixtureSetHouseSizeResponse response;
    response.Result = static_cast<uint32>(HOUSING_RESULT_SUCCESS);
    response.Size = requestedSize;
    SendPacket(response.Write());

    TC_LOG_INFO("housing", "CMSG_HOUSING_FIXTURE_SET_HOUSE_SIZE HouseGuid: {}, Size: {}, Result: SUCCESS",
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
        response.Result = static_cast<uint32>(HOUSING_RESULT_HOUSE_NOT_FOUND);
        SendPacket(response.Write());
        return;
    }

    uint32 wmoDataID = housingFixtureSetHouseType.HouseExteriorWmoDataID;

    // Validate the requested house type exists in the HouseExteriorWmoData DB2 store
    HouseExteriorWmoData const* wmoData = sHousingMgr.GetHouseExteriorWmoData(wmoDataID);
    if (!wmoData)
    {
        WorldPackets::Housing::HousingFixtureSetHouseTypeResponse response;
        response.Result = static_cast<uint32>(HOUSING_RESULT_HOUSE_EXTERIOR_TYPE_NOT_FOUND);
        SendPacket(response.Write());

        TC_LOG_INFO("housing", "CMSG_HOUSING_FIXTURE_SET_HOUSE_TYPE HouseGuid: {}, WmoDataID: {} REJECTED (not found in DB2)",
            housingFixtureSetHouseType.HouseGuid.ToString(), wmoDataID);
        return;
    }

    // Reject if already that type
    if (wmoDataID == housing->GetHouseType())
    {
        WorldPackets::Housing::HousingFixtureSetHouseTypeResponse response;
        response.Result = static_cast<uint32>(HOUSING_RESULT_HOUSE_EXTERIOR_ALREADY_THAT_TYPE);
        SendPacket(response.Write());

        TC_LOG_INFO("housing", "CMSG_HOUSING_FIXTURE_SET_HOUSE_TYPE HouseGuid: {}, WmoDataID: {} REJECTED (already that type)",
            housingFixtureSetHouseType.HouseGuid.ToString(), wmoDataID);
        return;
    }

    // Persist the new house type
    housing->SetHouseType(wmoDataID);

    // Respawn house MeshObjects with updated type
    if (HousingMap* housingMap = dynamic_cast<HousingMap*>(player->GetMap()))
    {
        uint8 plotIndex = housing->GetPlotIndex();
        housingMap->DespawnHouseForPlot(plotIndex);
        housingMap->SpawnHouseForPlot(plotIndex, nullptr,
            static_cast<int32>(housing->GetCoreExteriorComponentID()),
            static_cast<int32>(wmoDataID));
    }

    WorldPackets::Housing::HousingFixtureSetHouseTypeResponse response;
    response.Result = static_cast<uint32>(HOUSING_RESULT_SUCCESS);
    response.HouseExteriorTypeID = static_cast<int32>(wmoDataID);
    SendPacket(response.Write());

    // Notify account of house type collection update
    WorldPackets::Housing::AccountHouseTypeCollectionUpdate collectionUpdate;
    collectionUpdate.HouseTypeID = wmoDataID;
    SendPacket(collectionUpdate.Write());

    TC_LOG_INFO("housing", "CMSG_HOUSING_FIXTURE_SET_HOUSE_TYPE HouseGuid: {}, WmoDataID: {}, Result: SUCCESS",
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
        response.Result = static_cast<uint32>(HOUSING_RESULT_HOUSE_NOT_FOUND);
        SendPacket(response.Write());
        return;
    }

    housing->SetEditorMode(housingRoomSetLayoutEditMode.Active ? HOUSING_EDITOR_MODE_LAYOUT : HOUSING_EDITOR_MODE_NONE);

    WorldPackets::Housing::HousingRoomSetLayoutEditModeResponse response;
    response.Result = static_cast<uint32>(HOUSING_RESULT_SUCCESS);
    response.Active = housingRoomSetLayoutEditMode.Active;
    SendPacket(response.Write());

    TC_LOG_INFO("housing", "CMSG_HOUSING_ROOM_SET_EDITOR_MODE_ACTIVE Active: {}", housingRoomSetLayoutEditMode.Active);
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
        response.Result = static_cast<uint32>(HOUSING_RESULT_HOUSE_NOT_FOUND);
        SendPacket(response.Write());
        return;
    }

    HousingResult result = housing->PlaceRoom(housingRoomAdd.HouseRoomID, housingRoomAdd.FloorIndex,
        housingRoomAdd.Flags, housingRoomAdd.AutoFurnish);

    WorldPackets::Housing::HousingRoomAddResponse response;
    response.Result = static_cast<uint32>(result);
    SendPacket(response.Write());

    TC_LOG_INFO("housing", "CMSG_HOUSING_ROOM_ADD HouseRoomID: {}, FloorIndex: {}, Flags: {}, Result: {}",
        housingRoomAdd.HouseRoomID, housingRoomAdd.FloorIndex, housingRoomAdd.Flags, uint32(result));
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
        response.Result = static_cast<uint32>(HOUSING_RESULT_HOUSE_NOT_FOUND);
        SendPacket(response.Write());
        return;
    }

    HousingResult result = housing->RemoveRoom(housingRoomRemove.RoomGuid);

    WorldPackets::Housing::HousingRoomRemoveResponse response;
    response.Result = static_cast<uint32>(result);
    response.RoomGuid = housingRoomRemove.RoomGuid;
    SendPacket(response.Write());

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
        response.Result = static_cast<uint32>(HOUSING_RESULT_HOUSE_NOT_FOUND);
        SendPacket(response.Write());
        return;
    }

    HousingResult result = housing->RotateRoom(housingRoomRotate.RoomGuid, housingRoomRotate.Clockwise);

    WorldPackets::Housing::HousingRoomUpdateResponse response;
    response.Result = static_cast<uint32>(result);
    response.RoomGuid = housingRoomRotate.RoomGuid;
    SendPacket(response.Write());

    TC_LOG_INFO("housing", "CMSG_HOUSING_ROOM_ROTATE_ROOM RoomGuid: {}, Clockwise: {}, Result: {}",
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
        response.Result = static_cast<uint32>(HOUSING_RESULT_HOUSE_NOT_FOUND);
        SendPacket(response.Write());
        return;
    }

    HousingResult result = housing->MoveRoom(housingRoomMoveRoom.RoomGuid, housingRoomMoveRoom.TargetSlotIndex,
        housingRoomMoveRoom.TargetGuid, housingRoomMoveRoom.FloorIndex);

    WorldPackets::Housing::HousingRoomUpdateResponse response;
    response.Result = static_cast<uint32>(result);
    response.RoomGuid = housingRoomMoveRoom.RoomGuid;
    SendPacket(response.Write());

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
        response.Result = static_cast<uint32>(HOUSING_RESULT_HOUSE_NOT_FOUND);
        SendPacket(response.Write());
        return;
    }

    HousingResult result = housing->ApplyRoomTheme(housingRoomSetComponentTheme.RoomGuid,
        housingRoomSetComponentTheme.HouseThemeID, housingRoomSetComponentTheme.RoomComponentIDs);

    WorldPackets::Housing::HousingRoomSetComponentThemeResponse response;
    response.Result = static_cast<uint32>(result);
    response.RoomGuid = housingRoomSetComponentTheme.RoomGuid;
    response.ComponentID = housingRoomSetComponentTheme.HouseThemeID;
    response.ThemeSetID = housingRoomSetComponentTheme.HouseThemeID;
    SendPacket(response.Write());

    TC_LOG_INFO("housing", "CMSG_HOUSING_ROOM_SET_COMPONENT_THEME RoomGuid: {}, HouseThemeID: {}, Result: {}",
        housingRoomSetComponentTheme.RoomGuid.ToString(), housingRoomSetComponentTheme.HouseThemeID, uint32(result));
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
        response.Result = static_cast<uint32>(HOUSING_RESULT_HOUSE_NOT_FOUND);
        SendPacket(response.Write());
        return;
    }

    HousingResult result = housing->ApplyRoomWallpaper(housingRoomApplyComponentMaterials.RoomGuid,
        housingRoomApplyComponentMaterials.RoomComponentTextureID, housingRoomApplyComponentMaterials.RoomComponentTypeParam,
        housingRoomApplyComponentMaterials.RoomComponentIDs);

    WorldPackets::Housing::HousingRoomApplyComponentMaterialsResponse response;
    response.Result = static_cast<uint32>(result);
    response.RoomGuid = housingRoomApplyComponentMaterials.RoomGuid;
    response.ComponentID = housingRoomApplyComponentMaterials.RoomComponentTypeParam;
    response.RoomComponentTextureRecordID = housingRoomApplyComponentMaterials.RoomComponentTextureID;
    SendPacket(response.Write());

    TC_LOG_INFO("housing", "CMSG_HOUSING_ROOM_APPLY_COMPONENT_MATERIALS RoomGuid: {}, TextureID: {}, Result: {}",
        housingRoomApplyComponentMaterials.RoomGuid.ToString(), housingRoomApplyComponentMaterials.RoomComponentTextureID, uint32(result));
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
        response.Result = static_cast<uint32>(HOUSING_RESULT_HOUSE_NOT_FOUND);
        SendPacket(response.Write());
        return;
    }

    HousingResult result = housing->SetDoorType(housingRoomSetDoorType.RoomGuid,
        housingRoomSetDoorType.RoomComponentID, housingRoomSetDoorType.RoomComponentType);

    WorldPackets::Housing::HousingRoomSetDoorTypeResponse response;
    response.Result = static_cast<uint32>(result);
    response.RoomGuid = housingRoomSetDoorType.RoomGuid;
    response.ComponentID = housingRoomSetDoorType.RoomComponentID;
    response.DoorType = housingRoomSetDoorType.RoomComponentType;
    SendPacket(response.Write());

    TC_LOG_INFO("housing", "CMSG_HOUSING_ROOM_SET_DOOR_TYPE RoomGuid: {}, RoomComponentID: {}, Result: {}",
        housingRoomSetDoorType.RoomGuid.ToString(), housingRoomSetDoorType.RoomComponentID, uint32(result));
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
        response.Result = static_cast<uint32>(HOUSING_RESULT_HOUSE_NOT_FOUND);
        SendPacket(response.Write());
        return;
    }

    HousingResult result = housing->SetCeilingType(housingRoomSetCeilingType.RoomGuid,
        housingRoomSetCeilingType.RoomComponentID, housingRoomSetCeilingType.RoomComponentType);

    WorldPackets::Housing::HousingRoomSetCeilingTypeResponse response;
    response.Result = static_cast<uint32>(result);
    response.RoomGuid = housingRoomSetCeilingType.RoomGuid;
    response.ComponentID = housingRoomSetCeilingType.RoomComponentID;
    response.CeilingType = housingRoomSetCeilingType.RoomComponentType;
    SendPacket(response.Write());

    TC_LOG_INFO("housing", "CMSG_HOUSING_ROOM_SET_CEILING_TYPE RoomGuid: {}, RoomComponentID: {}, Result: {}",
        housingRoomSetCeilingType.RoomGuid.ToString(), housingRoomSetCeilingType.RoomComponentID, uint32(result));
}

// ============================================================
// Housing Services System
// ============================================================

void WorldSession::HandleHousingSvcsGuildCreateNeighborhood(WorldPackets::Housing::HousingSvcsGuildCreateNeighborhood const& housingSvcsGuildCreateNeighborhood)
{
    Player* player = GetPlayer();
    if (!player)
        return;

    Neighborhood* neighborhood = sNeighborhoodMgr.CreateGuildNeighborhood(
        player->GetGUID(), housingSvcsGuildCreateNeighborhood.NeighborhoodName,
        housingSvcsGuildCreateNeighborhood.NeighborhoodTypeID,
        housingSvcsGuildCreateNeighborhood.Flags);

    WorldPackets::Housing::HousingSvcsCreateCharterNeighborhoodResponse response;
    response.Result = static_cast<uint32>(neighborhood ? HOUSING_RESULT_SUCCESS : HOUSING_RESULT_GENERIC_FAILURE);
    if (neighborhood)
        response.NeighborhoodGuid = neighborhood->GetGuid();
    SendPacket(response.Write());

    // Send guild notification to all guild members
    if (neighborhood)
    {
        if (Guild* guild = sGuildMgr->GetGuildById(player->GetGuildId()))
        {
            WorldPackets::Housing::HousingSvcsGuildCreateNeighborhoodNotification notification;
            notification.NeighborhoodGuid = neighborhood->GetGuid();
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

    Neighborhood* neighborhood = sNeighborhoodMgr.ResolveNeighborhood(housingSvcsNeighborhoodReservePlot.NeighborhoodGuid, player);
    if (!neighborhood)
    {
        WorldPackets::Housing::HousingSvcsNeighborhoodReservePlotResponse response;
        response.Result = static_cast<uint8>(HOUSING_RESULT_NEIGHBORHOOD_NOT_FOUND);
        SendPacket(response.Write());
        return;
    }

    // Use the client's PlotIndex directly — the client sends its internal plot ID
    // which may differ from our DB2 PlotIndex values (sequential 0-54 in our SQL
    // vs the actual retail DB2 PlotIndex values the client uses).
    uint8 plotIndex = housingSvcsNeighborhoodReservePlot.PlotIndex;

    TC_LOG_INFO("housing", "HandleHousingSvcsNeighborhoodReservePlot: Using client PlotIndex {} (GUID: {})",
        plotIndex, housingSvcsNeighborhoodReservePlot.NeighborhoodGuid.ToString());

    HousingResult result = neighborhood->PurchasePlot(player->GetGUID(), plotIndex);
    if (result == HOUSING_RESULT_SUCCESS)
    {
        // Use the server's canonical neighborhood GUID, NOT the client-supplied GUID.
        player->CreateHousing(neighborhood->GetGuid(), plotIndex);

        Housing* housing = player->GetHousing();
        HousingMap* housingMap = dynamic_cast<HousingMap*>(player->GetMap());

        // Update the PlotInfo with the newly created HouseGuid and Battle.net account GUID
        if (housing)
        {
            neighborhood->UpdatePlotHouseInfo(plotIndex,
                housing->GetHouseGuid(), GetBattlenetAccountGUID());

            // Send guild notification for house addition
            if (Guild* guild = sGuildMgr->GetGuildById(player->GetGuildId()))
            {
                WorldPackets::Housing::HousingSvcsGuildAddHouseNotification notification;
                notification.HouseGuid = housing->GetHouseGuid();
                guild->BroadcastPacket(notification.Write());
            }
        }

        // Populate starter decor catalog and send notifications
        if (housing)
        {
            auto starterDecorWithQty = sHousingMgr.GetStarterDecorWithQuantities(player->GetTeam());
            for (auto const& [decorId, qty] : starterDecorWithQty)
            {
                for (int32 i = 0; i < qty; ++i)
                    housing->AddToCatalog(decorId);
            }

            // Send FirstTimeDecorAcquisition for unique decor IDs
            std::vector<uint32> starterDecorIds = sHousingMgr.GetStarterDecorIds(player->GetTeam());
            for (uint32 decorId : starterDecorIds)
            {
                WorldPackets::Housing::HousingFirstTimeDecorAcquisition decorAcq;
                decorAcq.DecorEntryID = decorId;
                SendPacket(decorAcq.Write());
            }

            // Proactive storage response (client requested before purchase and got "no house")
            std::vector<Housing::CatalogEntry const*> entries = housing->GetCatalogEntries();
            WorldPackets::Housing::HousingDecorRequestStorageResponse storageResp;
            storageResp.BNetAccountGuid = GetBattlenetAccountGUID();
            storageResp.ResultCode = static_cast<uint8>(HOUSING_RESULT_SUCCESS);
            storageResp.HasData = !entries.empty();
            SendPacket(storageResp.Write());

            TC_LOG_ERROR("housing", "HandleHousingSvcsNeighborhoodReservePlot: Populated catalog with {} decor types, sent {} FirstTimeDecorAcquisition + storage response",
                uint32(starterDecorWithQty.size()), uint32(starterDecorIds.size()));
        }

        // Grant "Acquire a house" kill credit for quest 91863 (objective 17)
        static constexpr uint32 NPC_KILL_CREDIT_BUY_HOME = 248858;
        player->KilledMonsterCredit(NPC_KILL_CREDIT_BUY_HOME);

        if (housingMap)
        {
            // Mark the plot Cornerstone as owned (GOState ACTIVE)
            housingMap->SetPlotOwnershipState(plotIndex, true);

            if (housing)
            {
                // Track the housing on the HousingMap (missed at AddPlayerToMap since house didn't exist yet)
                housingMap->AddPlayerHousing(player->GetGUID(), housing);

                // Update neighborhood PlotInfo with HouseGuid for MeshObject spawning
                if (!housing->GetHouseGuid().IsEmpty())
                    neighborhood->UpdatePlotHouseInfo(plotIndex, housing->GetHouseGuid(), GetBattlenetAccountGUID());

                // Spawn house GO and MeshObjects
                GameObject* houseGo = nullptr;
                if (housing->HasCustomPosition())
                {
                    Position customPos = housing->GetHousePosition();
                    houseGo = housingMap->SpawnHouseForPlot(plotIndex, &customPos);
                }
                else
                    houseGo = housingMap->SpawnHouseForPlot(plotIndex);

                TC_LOG_ERROR("housing", "HandleHousingSvcsNeighborhoodReservePlot: SpawnHouseForPlot for plot {}: {}",
                    plotIndex, houseGo ? houseGo->GetGUID().ToString() : "FAILED");

                // Update PlayerMirrorHouse.MapID so the client knows this house is on the current map.
                // Without this, MapID stays at 0 and the client rejects edit mode.
                player->UpdateHousingMapId(housing->GetHouseGuid(), static_cast<int32>(player->GetMapId()));

                // Update plot AreaTrigger with new ownership info and send ENTER_PLOT
                AreaTrigger* plotAt = housingMap->GetPlotAreaTrigger(plotIndex);
                if (plotAt)
                {
                    plotAt->InitHousingPlotData(plotIndex, player->GetGUID(), housing->GetHouseGuid(), GetBattlenetAccountGUID());

                    // Send ENTER_PLOT so the client has plot context for edit mode.
                    // AddPlayerToMap only sends this if housing existed at map entry time.
                    WorldPackets::Neighborhood::NeighborhoodPlayerEnterPlot enterPlot;
                    enterPlot.PlotAreaTriggerGuid = plotAt->GetGUID();
                    SendPacket(enterPlot.Write());

                    TC_LOG_ERROR("housing", "HandleHousingSvcsNeighborhoodReservePlot: Sent ENTER_PLOT for plot {} AT {} to player {}",
                        plotIndex, plotAt->GetGUID().ToString(), player->GetGUID().ToString());
                }

                // Re-send HousingGetCurrentHouseInfoResponse with actual house data.
                // The initial send (during AddPlayerToMap) had no house info since the player hadn't purchased yet.
                WorldPackets::Housing::HousingGetCurrentHouseInfoResponse houseInfo;
                houseInfo.HouseInfo.OwnerGuid = housing->GetHouseGuid();
                houseInfo.HouseInfo.SecondaryOwnerGuid = player->GetGUID(); // Owner's Player GUID
                houseInfo.HouseInfo.PlotGuid = housing->GetNeighborhoodGuid();
                houseInfo.HouseInfo.Flags = housing->GetPlotIndex();
                houseInfo.HouseInfo.HouseTypeId = 32;
                houseInfo.HouseInfo.StatusFlags = 0;
                houseInfo.ResponseFlags = 0;
                SendPacket(houseInfo.Write());

                TC_LOG_ERROR("housing", "HandleHousingSvcsNeighborhoodReservePlot: Re-sent CURRENT_HOUSE_INFO: Flags(PlotIndex)={}, HouseGuid={}, NeighborhoodGuid={}",
                    houseInfo.HouseInfo.Flags, houseInfo.HouseInfo.OwnerGuid.ToString(), houseInfo.HouseInfo.PlotGuid.ToString());
            }
        }
    }

    WorldPackets::Housing::HousingSvcsNeighborhoodReservePlotResponse response;
    response.Result = static_cast<uint8>(result);
    SendPacket(response.Write());

    // Sniff 12.0.1: After successful reserve, server casts "Visit House" spell (ID 1265142)
    // with CastTime=10000ms, targeting the neighborhood GUID + destination position.
    if (result == HOUSING_RESULT_SUCCESS)
    {
        static constexpr uint32 SPELL_VISIT_HOUSE = 1265142;
        player->CastSpell(player, SPELL_VISIT_HOUSE, true);
    }

    TC_LOG_INFO("housing", "CMSG_HOUSING_SVCS_NEIGHBORHOOD_RESERVE_PLOT PlotIndex: {} (client sent {}), Result: {}",
        plotIndex, housingSvcsNeighborhoodReservePlot.PlotIndex, uint32(result));
}

void WorldSession::HandleHousingSvcsRelinquishHouse(WorldPackets::Housing::HousingSvcsRelinquishHouse const& /*housingSvcsRelinquishHouse*/)
{
    Player* player = GetPlayer();
    if (!player)
        return;

    // Capture house GUID and neighborhood GUID before deletion
    ObjectGuid houseGuid;
    ObjectGuid neighborhoodGuid;
    if (Housing const* housing = player->GetHousing())
    {
        houseGuid = housing->GetHouseGuid();
        neighborhoodGuid = housing->GetNeighborhoodGuid();
    }

    if (!neighborhoodGuid.IsEmpty())
        player->DeleteHousing(neighborhoodGuid);

    WorldPackets::Housing::HousingSvcsRelinquishHouseResponse response;
    response.Result = static_cast<uint32>(HOUSING_RESULT_SUCCESS);
    response.HouseGuid = houseGuid;
    SendPacket(response.Write());

    // House deletion is a major data change — request client to reload housing data
    WorldPackets::Housing::HousingSvcRequestPlayerReloadData reloadData;
    SendPacket(reloadData.Write());

    // Send guild notification for house removal
    if (!houseGuid.IsEmpty())
    {
        if (Guild* guild = sGuildMgr->GetGuildById(player->GetGuildId()))
        {
            WorldPackets::Housing::HousingSvcsGuildRemoveHouseNotification notification;
            notification.HouseGuid = houseGuid;
            guild->BroadcastPacket(notification.Write());
        }
    }

    TC_LOG_INFO("housing", "CMSG_HOUSING_SVCS_RELINQUISH_HOUSE processed");
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
        response.Result = static_cast<uint32>(HOUSING_RESULT_HOUSE_NOT_FOUND);
        SendPacket(response.Write());
        return;
    }

    if (housingSvcsUpdateHouseSettings.PlotSettingsID)
        housing->SaveSettings(*housingSvcsUpdateHouseSettings.PlotSettingsID);

    WorldPackets::Housing::HousingSvcsUpdateHouseSettingsResponse response;
    response.Result = static_cast<uint32>(HOUSING_RESULT_SUCCESS);
    response.HouseGuid = housingSvcsUpdateHouseSettings.HouseGuid;
    response.AccessFlags = housingSvcsUpdateHouseSettings.PlotSettingsID.value_or(0);
    SendPacket(response.Write());

    // Settings changes (visibility, permissions) require house finder data refresh
    WorldPackets::Housing::HousingSvcsHouseFinderForceRefresh forceRefresh;
    SendPacket(forceRefresh.Write());

    TC_LOG_INFO("housing", "CMSG_HOUSING_SVCS_UPDATE_HOUSE_SETTINGS HouseGuid: {}",
        housingSvcsUpdateHouseSettings.HouseGuid.ToString());
}

void WorldSession::HandleHousingSvcsPlayerViewHousesByPlayer(WorldPackets::Housing::HousingSvcsPlayerViewHousesByPlayer const& housingSvcsPlayerViewHousesByPlayer)
{
    Player* player = GetPlayer();
    if (!player)
        return;

    // Look up neighborhoods the target player belongs to
    std::vector<Neighborhood*> neighborhoods = sNeighborhoodMgr.GetNeighborhoodsForPlayer(housingSvcsPlayerViewHousesByPlayer.PlayerGuid);

    WorldPackets::Housing::HousingSvcsPlayerViewHousesResponse response;
    response.Result = static_cast<uint32>(HOUSING_RESULT_SUCCESS);
    response.Neighborhoods.reserve(neighborhoods.size());
    for (Neighborhood const* neighborhood : neighborhoods)
    {
        WorldPackets::Housing::HousingSvcsPlayerViewHousesResponse::NeighborhoodInfoData info;
        info.NeighborhoodGuid = neighborhood->GetGuid();
        info.Name = neighborhood->GetName();
        info.MapID = neighborhood->GetNeighborhoodMapID();
        response.Neighborhoods.push_back(std::move(info));
    }
    SendPacket(response.Write());

    TC_LOG_INFO("housing", "CMSG_HOUSING_SVCS_PLAYER_VIEW_HOUSES_BY_PLAYER PlayerGuid: {}, FoundNeighborhoods: {}",
        housingSvcsPlayerViewHousesByPlayer.PlayerGuid.ToString(), uint32(neighborhoods.size()));
}

void WorldSession::HandleHousingSvcsPlayerViewHousesByBnetAccount(WorldPackets::Housing::HousingSvcsPlayerViewHousesByBnetAccount const& housingSvcsPlayerViewHousesByBnetAccount)
{
    Player* player = GetPlayer();
    if (!player)
        return;

    // Find all neighborhoods where the queried BNet account has a plot (owns a house)
    std::vector<Neighborhood*> neighborhoods = sNeighborhoodMgr.GetNeighborhoodsByBnetAccount(housingSvcsPlayerViewHousesByBnetAccount.BnetAccountGuid);

    WorldPackets::Housing::HousingSvcsPlayerViewHousesResponse response;
    response.Result = static_cast<uint32>(HOUSING_RESULT_SUCCESS);
    response.Neighborhoods.reserve(neighborhoods.size());
    for (Neighborhood const* neighborhood : neighborhoods)
    {
        WorldPackets::Housing::HousingSvcsPlayerViewHousesResponse::NeighborhoodInfoData info;
        info.NeighborhoodGuid = neighborhood->GetGuid();
        info.Name = neighborhood->GetName();
        info.MapID = neighborhood->GetNeighborhoodMapID();
        response.Neighborhoods.push_back(std::move(info));
    }
    SendPacket(response.Write());

    TC_LOG_INFO("housing", "CMSG_HOUSING_SVCS_PLAYER_VIEW_HOUSES_BY_BNET_ACCOUNT BnetAccountGuid: {}, FoundNeighborhoods: {}",
        housingSvcsPlayerViewHousesByBnetAccount.BnetAccountGuid.ToString(), uint32(neighborhoods.size()));
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
        WorldPackets::Housing::JamCurrentHouseInfo info;
        // Sniff+IDA-verified: OwnerGuid=HouseGUID, SecondaryOwnerGuid=OwnerPlayerGUID, PlotGuid=NeighborhoodGUID
        info.OwnerGuid = housing->GetHouseGuid();
        info.SecondaryOwnerGuid = player->GetGUID(); // Owner's Player GUID (HighGuid::Player)
        info.PlotGuid = housing->GetNeighborhoodGuid();
        info.Flags = housing->GetPlotIndex();
        info.HouseTypeId = 32;
        // Set creation timestamp if available (sniff shows StatusFlags=0x80 with uint64 timestamp)
        if (housing->GetCreateTime())
        {
            info.StatusFlags = 0x80;
            info.HouseId = static_cast<uint64>(housing->GetCreateTime());
        }
        response.Houses.push_back(info);
    }
    SendPacket(response.Write());

    for (uint32 i = 0; i < response.Houses.size(); ++i)
    {
        auto const& info = response.Houses[i];
        TC_LOG_ERROR("housing", "<<< GET_PLAYER_HOUSES_INFO[{}]: HouseGuid={}, OwnerGuid={}, NeighborhoodGuid={}, Flags(PlotIndex)={}, HouseType={}, StatusFlags=0x{:02X}",
            i, info.OwnerGuid.ToString(), info.SecondaryOwnerGuid.ToString(), info.PlotGuid.ToString(),
            info.Flags, info.HouseTypeId, info.StatusFlags);
    }
    TC_LOG_INFO("housing", "<<< SMSG_HOUSING_SVCS_GET_PLAYER_HOUSES_INFO_RESPONSE sent (HouseCount: {})",
        uint32(response.Houses.size()));
}

void WorldSession::HandleHousingSvcsTeleportToPlot(WorldPackets::Housing::HousingSvcsTeleportToPlot const& housingSvcsTeleportToPlot)
{
    Player* player = GetPlayer();
    if (!player)
        return;

    Neighborhood* neighborhood = sNeighborhoodMgr.ResolveNeighborhood(housingSvcsTeleportToPlot.NeighborhoodGuid, player);
    if (!neighborhood)
    {
        WorldPackets::Housing::HousingSvcsNotifyPermissionsFailure response;
        response.Result = static_cast<uint16>(HOUSING_RESULT_NEIGHBORHOOD_NOT_FOUND);
        SendPacket(response.Write());
        return;
    }

    // Use the client's PlotIndex directly (may differ from our DB2 PlotIndex)
    uint32 plotIndex = housingSvcsTeleportToPlot.PlotIndex;

    // Also resolve via cornerstone for fallback position lookup
    int32 db2Resolved = sHousingMgr.ResolvePlotIndex(housingSvcsTeleportToPlot.NeighborhoodGuid, neighborhood);

    TC_LOG_INFO("housing", "HandleHousingSvcsTeleportToPlot: Client PlotIndex={}, DB2 resolved={}, GUID={}",
        plotIndex, db2Resolved, housingSvcsTeleportToPlot.NeighborhoodGuid.ToString());

    // Look up the neighborhood map data for map ID and plot positions
    NeighborhoodMapData const* mapData = sHousingMgr.GetNeighborhoodMapData(neighborhood->GetNeighborhoodMapID());
    if (!mapData)
    {
        WorldPackets::Housing::HousingSvcsNotifyPermissionsFailure response;
        response.Result = static_cast<uint16>(HOUSING_RESULT_PLOT_NOT_FOUND);
        SendPacket(response.Write());
        return;
    }

    // Get plots for this neighborhood map and find the target plot
    // Try client PlotIndex first, then DB2 resolved, then cornerstone GO entry
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
    // Fallback: try DB2 resolved PlotIndex
    if (!targetPlot && db2Resolved >= 0 && static_cast<uint32>(db2Resolved) != plotIndex)
    {
        for (NeighborhoodPlotData const* plot : plots)
        {
            if (plot->PlotIndex == db2Resolved)
            {
                targetPlot = plot;
                break;
            }
        }
    }
    // Last resort: cornerstone GO entry
    if (!targetPlot)
    {
        uint32 goEntry = housingSvcsTeleportToPlot.NeighborhoodGuid.GetEntry();
        if (goEntry)
            targetPlot = sHousingMgr.GetPlotByCornerstoneEntry(neighborhood->GetNeighborhoodMapID(), goEntry);
    }

    if (targetPlot)
    {
        player->TeleportTo(mapData->MapID, targetPlot->TeleportPosition[0], targetPlot->TeleportPosition[1],
            targetPlot->TeleportPosition[2], 0.0f);

        TC_LOG_INFO("housing", "CMSG_HOUSING_SVCS_TELEPORT_TO_PLOT: Teleporting player {} to plot {} on map {}",
            player->GetGUID().ToString(), plotIndex, mapData->MapID);
    }
    else
    {
        player->TeleportTo(mapData->MapID, mapData->Origin[0], mapData->Origin[1],
            mapData->Origin[2], 0.0f);

        TC_LOG_INFO("housing", "CMSG_HOUSING_SVCS_TELEPORT_TO_PLOT: Plot {} not found, teleporting to neighborhood origin on map {}",
            plotIndex, mapData->MapID);
    }
}

void WorldSession::HandleHousingSvcsStartTutorial(WorldPackets::Housing::HousingSvcsStartTutorial const& /*housingSvcsStartTutorial*/)
{
    Player* player = GetPlayer();
    if (!player)
        return;

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
        failResponse.Result = static_cast<uint16>(HOUSING_RESULT_NEIGHBORHOOD_NOT_FOUND);
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

void WorldSession::HandleHousingSvcsAcceptNeighborhoodOwnership(WorldPackets::Housing::HousingSvcsAcceptNeighborhoodOwnership const& housingSvcsAcceptNeighborhoodOwnership)
{
    Player* player = GetPlayer();
    if (!player)
        return;

    Neighborhood* neighborhood = sNeighborhoodMgr.ResolveNeighborhood(housingSvcsAcceptNeighborhoodOwnership.NeighborhoodGuid, player);
    if (!neighborhood)
    {
        WorldPackets::Housing::HousingSvcsAcceptNeighborhoodOwnershipResponse response;
        response.Result = static_cast<uint32>(HOUSING_RESULT_NEIGHBORHOOD_NOT_FOUND);
        SendPacket(response.Write());
        return;
    }

    ObjectGuid previousOwnerGuid = neighborhood->GetOwnerGuid();
    neighborhood->TransferOwnership(player->GetGUID());

    WorldPackets::Housing::HousingSvcsAcceptNeighborhoodOwnershipResponse response;
    response.Result = static_cast<uint32>(HOUSING_RESULT_SUCCESS);
    response.NeighborhoodGuid = housingSvcsAcceptNeighborhoodOwnership.NeighborhoodGuid;
    SendPacket(response.Write());

    // Broadcast ownership transfer to all members
    for (auto const& member : neighborhood->GetMembers())
    {
        if (member.PlayerGuid == player->GetGUID())
            continue;
        if (Player* memberPlayer = ObjectAccessor::FindPlayer(member.PlayerGuid))
        {
            WorldPackets::Housing::HousingSvcsNeighborhoodOwnershipTransferredResponse transferNotification;
            transferNotification.NeighborhoodGuid = housingSvcsAcceptNeighborhoodOwnership.NeighborhoodGuid;
            transferNotification.NewOwnerGuid = player->GetGUID();
            memberPlayer->SendDirectMessage(transferNotification.Write());
        }
    }

    // Ownership change is a major data change — request client to reload housing data
    WorldPackets::Housing::HousingSvcRequestPlayerReloadData reloadData;
    SendPacket(reloadData.Write());

    // Previous owner also needs to reload
    if (Player* prevOwner = ObjectAccessor::FindPlayer(previousOwnerGuid))
    {
        WorldPackets::Housing::HousingSvcRequestPlayerReloadData prevReload;
        prevOwner->SendDirectMessage(prevReload.Write());
    }

    TC_LOG_INFO("housing", "CMSG_HOUSING_SVCS_ACCEPT_NEIGHBORHOOD_OWNERSHIP NeighborhoodGuid: {}, PreviousOwner: {}",
        housingSvcsAcceptNeighborhoodOwnership.NeighborhoodGuid.ToString(),
        previousOwnerGuid.ToString());
}

void WorldSession::HandleHousingSvcsRejectNeighborhoodOwnership(WorldPackets::Housing::HousingSvcsRejectNeighborhoodOwnership const& housingSvcsRejectNeighborhoodOwnership)
{
    Player* player = GetPlayer();
    if (!player)
        return;

    // Declining ownership means the offer is invalidated. The current owner remains.
    // This is a notification-only operation; no data change needed.
    WorldPackets::Housing::HousingSvcsRejectNeighborhoodOwnershipResponse response;
    response.Result = static_cast<uint32>(HOUSING_RESULT_SUCCESS);
    response.NeighborhoodGuid = housingSvcsRejectNeighborhoodOwnership.NeighborhoodGuid;
    SendPacket(response.Write());

    TC_LOG_INFO("housing", "CMSG_HOUSING_SVCS_REJECT_NEIGHBORHOOD_OWNERSHIP: Player {} declined ownership of neighborhood {}",
        player->GetGUID().ToString(), housingSvcsRejectNeighborhoodOwnership.NeighborhoodGuid.ToString());
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
        response.Result = static_cast<uint32>(HOUSING_RESULT_HOUSE_NOT_FOUND);
        SendPacket(response.Write());
        return;
    }

    Neighborhood* neighborhood = sNeighborhoodMgr.GetNeighborhood(housing->GetNeighborhoodGuid());
    if (!neighborhood)
    {
        WorldPackets::Housing::HousingSvcsGetPotentialHouseOwnersResponse response;
        response.Result = static_cast<uint32>(HOUSING_RESULT_NEIGHBORHOOD_NOT_FOUND);
        SendPacket(response.Write());
        return;
    }

    std::vector<Neighborhood::Member> const& members = neighborhood->GetMembers();
    TC_LOG_INFO("housing", "CMSG_HOUSING_SVCS_GET_POTENTIAL_HOUSE_OWNERS: Neighborhood has {} members",
        uint32(members.size()));

    WorldPackets::Housing::HousingSvcsGetPotentialHouseOwnersResponse response;
    response.Result = static_cast<uint32>(HOUSING_RESULT_SUCCESS);
    response.PotentialOwners.reserve(members.size());
    for (auto const& member : members)
    {
        WorldPackets::Housing::HousingSvcsGetPotentialHouseOwnersResponse::PotentialOwnerData ownerData;
        ownerData.PlayerGuid = member.PlayerGuid;
        if (Player* memberPlayer = ObjectAccessor::FindPlayer(member.PlayerGuid))
            ownerData.PlayerName = memberPlayer->GetName();
        response.PotentialOwners.push_back(std::move(ownerData));
    }
    SendPacket(response.Write());
}

void WorldSession::HandleHousingSvcsGetHouseFinderInfo(WorldPackets::Housing::HousingSvcsGetHouseFinderInfo const& /*housingSvcsGetHouseFinderInfo*/)
{
    Player* player = GetPlayer();
    if (!player)
        return;

    // Return list of public neighborhoods available through the finder
    std::vector<Neighborhood*> publicNeighborhoods = sNeighborhoodMgr.GetPublicNeighborhoods();

    WorldPackets::Housing::HousingSvcsGetHouseFinderInfoResponse response;
    response.Result = static_cast<uint32>(HOUSING_RESULT_SUCCESS);
    response.Entries.reserve(publicNeighborhoods.size());
    for (Neighborhood const* neighborhood : publicNeighborhoods)
    {
        WorldPackets::Housing::HousingSvcsGetHouseFinderInfoResponse::HouseFinderEntry entry;
        entry.NeighborhoodGuid = neighborhood->GetGuid();
        entry.NeighborhoodName = neighborhood->GetName();
        entry.MapID = neighborhood->GetNeighborhoodMapID();
        entry.TotalPlots = MAX_NEIGHBORHOOD_PLOTS;
        entry.AvailablePlots = MAX_NEIGHBORHOOD_PLOTS - neighborhood->GetOccupiedPlotCount();
        entry.SuggestionReason = 32; // Random
        response.Entries.push_back(std::move(entry));
    }
    SendPacket(response.Write());

    TC_LOG_INFO("housing", "CMSG_HOUSING_SVCS_GET_HOUSE_FINDER_INFO: {} public neighborhoods available",
        uint32(publicNeighborhoods.size()));
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
        response.Result = static_cast<uint32>(HOUSING_RESULT_NEIGHBORHOOD_NOT_FOUND);
        SendPacket(response.Write());
        return;
    }

    TC_LOG_INFO("housing", "CMSG_HOUSING_SVCS_GET_HOUSE_FINDER_NEIGHBORHOOD: '{}' MapID:{} Members:{} Public:{}",
        neighborhood->GetName(), neighborhood->GetNeighborhoodMapID(),
        neighborhood->GetMemberCount(), neighborhood->IsPublic());

    // Iterate ALL DB2 plots for this neighborhood map so the client sees the full 55-plot grid
    std::vector<NeighborhoodPlotData const*> plotDataList = sHousingMgr.GetPlotsForMap(neighborhood->GetNeighborhoodMapID());

    WorldPackets::Housing::HousingSvcsGetHouseFinderNeighborhoodResponse response;
    response.Result = static_cast<uint32>(HOUSING_RESULT_SUCCESS);
    response.NeighborhoodGuid = neighborhood->GetGuid();
    response.NeighborhoodName = neighborhood->GetName();
    response.Plots.reserve(plotDataList.size());
    for (NeighborhoodPlotData const* plotData : plotDataList)
    {
        uint8 plotIndex = static_cast<uint8>(plotData->PlotIndex);
        Neighborhood::PlotInfo const* plotInfo = neighborhood->GetPlotInfo(plotIndex);

        WorldPackets::Housing::HousingSvcsGetHouseFinderNeighborhoodResponse::PlotEntry plotEntry;
        plotEntry.PlotIndex = plotIndex;
        plotEntry.IsAvailable = !plotInfo || plotInfo->OwnerGuid.IsEmpty();
        plotEntry.Cost = plotData->Cost;
        response.Plots.push_back(std::move(plotEntry));
    }
    SendPacket(response.Write());
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
        response.Result = static_cast<uint32>(HOUSING_RESULT_SUCCESS);
        SendPacket(response.Write());
        return;
    }

    // Build response by iterating all neighborhoods and checking if any plot owner
    // is on the requesting player's friend list. Each player can own at most one
    // house per faction (2 houses max), so results are naturally bounded.
    WorldPackets::Housing::HousingSvcsGetBnetFriendNeighborhoodsResponse response;
    response.Result = static_cast<uint32>(HOUSING_RESULT_SUCCESS);

    std::vector<Neighborhood*> allNeighborhoods = sNeighborhoodMgr.GetAllNeighborhoods();
    for (Neighborhood const* neighborhood : allNeighborhoods)
    {
        for (auto const& plot : neighborhood->GetPlots())
        {
            if (!plot.IsOccupied() || plot.OwnerGuid.IsEmpty())
                continue;

            // Check if the plot owner is a friend of the requesting player
            if (!social->HasFriend(plot.OwnerGuid))
                continue;

            // Resolve the friend's character name
            std::string friendName;
            if (!sCharacterCache->GetCharacterNameByGuid(plot.OwnerGuid, friendName))
                continue;

            WorldPackets::Housing::HousingSvcsGetBnetFriendNeighborhoodsResponse::BnetFriendNeighborhoodData data;
            data.NeighborhoodGuid = neighborhood->GetGuid();
            data.FriendName = std::move(friendName);
            data.MapID = neighborhood->GetNeighborhoodMapID();
            response.Neighborhoods.push_back(std::move(data));
            break; // Only list each neighborhood once per friend check
        }
    }

    SendPacket(response.Write());

    TC_LOG_INFO("housing", "CMSG_HOUSING_SVCS_GET_BNET_FRIEND_NEIGHBORHOODS BnetAccountGuid: {}, FriendNeighborhoods: {}",
        housingSvcsGetBnetFriendNeighborhoods.BnetAccountGuid.ToString(), uint32(response.Neighborhoods.size()));
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
    response.Result = static_cast<uint32>(HOUSING_RESULT_SUCCESS);
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

    WorldPackets::Housing::HousingHouseStatusResponse response;

    // First check if the player is on their own plot (use their Housing data directly)
    Housing* ownHousing = player->GetHousing();

    // Check what plot the player is currently visiting via area trigger tracking
    HousingMap* housingMap = dynamic_cast<HousingMap*>(player->GetMap());
    int8 visitedPlot = housingMap ? housingMap->GetPlayerCurrentPlot(player->GetGUID()) : -1;

    if (visitedPlot >= 0 && housingMap && housingMap->GetNeighborhood())
    {
        Neighborhood* neighborhood = housingMap->GetNeighborhood();
        Neighborhood::PlotInfo const* plotInfo = neighborhood->GetPlotInfo(static_cast<uint8>(visitedPlot));

        if (plotInfo && plotInfo->OwnerGuid != player->GetGUID())
        {
            // Visiting someone else's plot — return that plot's house data
            response.HouseGuid = plotInfo->HouseGuid;
            // Sniff byte-level: second field has hi byte7=0x78 = BNetAccount, NOT Housing/Neighborhood
            response.NeighborhoodGuid = plotInfo->OwnerBnetGuid; // BNetAccount GUID of plot owner
            response.OwnerPlayerGuid = plotInfo->OwnerGuid; // Plot owner's Player GUID
            response.Status = 0; // Visitor is outside (exterior)
        }
        else if (ownHousing)
        {
            // On own plot
            response.HouseGuid = ownHousing->GetHouseGuid();
            // Sniff byte-level: second field has hi byte7=0x78 = BNetAccount, NOT Housing/Neighborhood
            response.NeighborhoodGuid = GetBattlenetAccountGUID();
            response.OwnerPlayerGuid = player->GetGUID(); // Own Player GUID
            response.Status = ownHousing->IsInInterior() ? 1 : 0;
        }
    }
    else if (ownHousing)
    {
        // Not on any tracked plot, fall back to own housing data
        response.HouseGuid = ownHousing->GetHouseGuid();
        // Sniff byte-level: second field has hi byte7=0x78 = BNetAccount, NOT Housing/Neighborhood
        response.NeighborhoodGuid = GetBattlenetAccountGUID();
        response.OwnerPlayerGuid = player->GetGUID(); // Own Player GUID
        response.Status = ownHousing->IsInInterior() ? 1 : 0;
    }
    // No house and not on a plot: all fields stay at defaults (empty GUIDs, Status=0).
    WorldPacket const* statusPkt = response.Write();
    TC_LOG_ERROR("housing", ">>> CMSG_HOUSING_HOUSE_STATUS (visitedPlot: {})", visitedPlot);
    TC_LOG_ERROR("housing", "<<< SMSG_HOUSING_HOUSE_STATUS_RESPONSE ({} bytes): {}",
        statusPkt->size(), HexDumpPacket(statusPkt));
    TC_LOG_ERROR("housing", "    HouseGuid={} NeighborhoodGuid={} OwnerPlayerGuid={} Status={}",
        response.HouseGuid.ToString(), response.NeighborhoodGuid.ToString(),
        response.OwnerPlayerGuid.ToString(), response.Status);
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
            // Visitor: evaluate permissions based on house settings
            response.ResultCode = 0;
            response.PermissionFlags = 0x40;  // Sniff-verified: visitors get 0x40
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
    int8 currentPlot = housingMap ? housingMap->GetPlayerCurrentPlot(player->GetGUID()) : -1;

    WorldPackets::Housing::HousingGetCurrentHouseInfoResponse response;

    if (currentPlot >= 0 && housingMap && housingMap->GetNeighborhood())
    {
        // Player is on a specific plot — return info about THAT plot's house
        Neighborhood* neighborhood = housingMap->GetNeighborhood();
        Neighborhood::PlotInfo const* plotInfo = neighborhood->GetPlotInfo(static_cast<uint8>(currentPlot));

        if (plotInfo && plotInfo->IsOccupied())
        {
            // Find the plot owner's housing data for HouseTypeId
            Housing* plotHousing = nullptr;
            if (plotInfo->OwnerGuid == player->GetGUID())
                plotHousing = player->GetHousing();
            else if (Player* ownerPlayer = ObjectAccessor::FindPlayer(plotInfo->OwnerGuid))
                plotHousing = ownerPlayer->GetHousing();

            response.HouseInfo.OwnerGuid = plotInfo->HouseGuid;
            response.HouseInfo.SecondaryOwnerGuid = plotInfo->OwnerGuid; // Plot owner's Player GUID
            response.HouseInfo.PlotGuid = neighborhood->GetGuid();
            response.HouseInfo.Flags = static_cast<uint8>(currentPlot); // AT-tracked PlotID
            response.HouseInfo.HouseTypeId = plotHousing ? plotHousing->GetHouseType() : 32;
            response.HouseInfo.StatusFlags = 0;
        }
        else
        {
            // On an unoccupied plot
            response.HouseInfo.SecondaryOwnerGuid = player->GetGUID();
            response.HouseInfo.PlotGuid = neighborhood->GetGuid();
            response.HouseInfo.Flags = static_cast<uint8>(currentPlot);
        }
    }
    else if (Housing* housing = player->GetHousing())
    {
        // Not on any tracked plot — fall back to player's own house data
        response.HouseInfo.OwnerGuid = housing->GetHouseGuid();
        response.HouseInfo.SecondaryOwnerGuid = player->GetGUID();
        response.HouseInfo.PlotGuid = housing->GetNeighborhoodGuid();
        response.HouseInfo.Flags = housing->GetPlotIndex();
        response.HouseInfo.HouseTypeId = housing->GetHouseType();
        response.HouseInfo.StatusFlags = 0;
    }
    else if (housingMap)
    {
        // No house, no tracked plot
        response.HouseInfo.SecondaryOwnerGuid = player->GetGUID();
        if (Neighborhood* neighborhood = housingMap->GetNeighborhood())
            response.HouseInfo.PlotGuid = neighborhood->GetGuid();
    }
    response.ResponseFlags = 0;
    WorldPacket const* houseInfoPkt = response.Write();
    TC_LOG_ERROR("housing", "<<< SMSG_HOUSING_GET_CURRENT_HOUSE_INFO_RESPONSE ({} bytes): {}",
        houseInfoPkt->size(), HexDumpPacket(houseInfoPkt));
    TC_LOG_ERROR("housing", "    currentPlot={} OwnerGuid={} SecondaryOwnerGuid={} PlotGuid={} Flags={} HouseTypeId={} StatusFlags={} ResponseFlags={}",
        currentPlot,
        response.HouseInfo.OwnerGuid.ToString(),
        response.HouseInfo.SecondaryOwnerGuid.ToString(),
        response.HouseInfo.PlotGuid.ToString(),
        response.HouseInfo.Flags,
        response.HouseInfo.HouseTypeId,
        response.HouseInfo.StatusFlags,
        response.ResponseFlags);
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
    response.Result = static_cast<uint32>(HOUSING_RESULT_SUCCESS);
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
        response.Allow = true;
        response.Name = neighborhood->GetName();
    }
    else
    {
        response.NeighborhoodGuid = queryNeighborhoodInfo.NeighborhoodGuid;
        response.Allow = false;
    }

    WorldPacket const* namePkt = response.Write();
    SendPacket(namePkt);

    TC_LOG_ERROR("housing", "=== SMSG_QUERY_NEIGHBORHOOD_NAME_RESPONSE (0x460012) ===\n"
        "  Allow={}, Name='{}' (len={})\n"
        "  NeighborhoodGuid: {} ({})\n"
        "  Packet size={} bytes, hex:\n  {}",
        response.Allow, response.Name, response.Name.size(),
        response.NeighborhoodGuid.ToString(), GuidHex(response.NeighborhoodGuid),
        namePkt->size(), HexDumpPacket(namePkt));
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
    response.Result = static_cast<uint32>(HOUSING_RESULT_SUCCESS);
    if (!neighborhoods.empty())
    {
        response.NeighborhoodGuid = neighborhoods[0]->GetGuid();
        // Find the target player's house guid in the first neighborhood
        for (auto const& plot : neighborhoods[0]->GetPlots())
        {
            if (!plot.IsOccupied())
                continue;
            if (plot.OwnerGuid == guildGetOthersOwnedHouses.PlayerGuid)
            {
                response.HouseGuid = plot.HouseGuid;
                break;
            }
        }
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

    // Photo sharing authorization grants permission for the player's house photos
    // to be shared with other players through the housing social features.
    WorldPackets::Housing::HousingPhotoSharingAuthorizationResult response;
    response.Result = static_cast<uint32>(HOUSING_RESULT_SUCCESS);
    SendPacket(response.Write());

    TC_LOG_DEBUG("housing", "CMSG_HOUSING_PHOTO_SHARING_COMPLETE_AUTHORIZATION Player: {}",
        player->GetGUID().ToString());
}

void WorldSession::HandleHousingPhotoSharingClearAuthorization(WorldPackets::Housing::HousingPhotoSharingClearAuthorization const& /*packet*/)
{
    Player* player = GetPlayer();
    if (!player)
        return;

    // Clears any existing photo sharing authorization for the player's house,
    // revoking permission for photos to be shared publicly.
    WorldPackets::Housing::HousingPhotoSharingAuthorizationClearedResult response;
    response.Result = static_cast<uint32>(HOUSING_RESULT_SUCCESS);
    SendPacket(response.Write());

    TC_LOG_DEBUG("housing", "CMSG_HOUSING_PHOTO_SHARING_CLEAR_AUTHORIZATION Player: {}",
        player->GetGUID().ToString());
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

    // Returns the player's licensed decor quantities. Since the housing decor licensing
    // system is not yet fully implemented, we return an empty list to satisfy the client.
    // When decor purchasing is implemented, this will query the player's account-wide
    // decor license collection and return quantity data for each owned decor item.
    WorldPackets::Housing::GetAllLicensedDecorQuantitiesResponse response;
    // response.Quantities is empty — no licensed decor yet
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

    // Returns the list of refundable decor items. Since the housing decor purchasing
    // system is not yet fully implemented, we return an empty list. When decor purchasing
    // is implemented, this will return recently purchased decor items that are still within
    // the refund window, including their refund price, expiry time, and flags.
    WorldPackets::Housing::GetDecorRefundListResponse response;
    // response.Decors is empty — no refundable decor yet
    SendPacket(response.Write());

    TC_LOG_DEBUG("housing", "SMSG_GET_DECOR_REFUND_LIST_RESPONSE sent to Player: {} with {} decors",
        player->GetGUID().ToString(), response.Decors.size());
}
