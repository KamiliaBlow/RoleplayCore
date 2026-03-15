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
        housingMap->SpawnHouseForPlot(plotIndex, &newPos,
            static_cast<int32>(housing->GetCoreExteriorComponentID()),
            static_cast<int32>(housing->GetHouseType()),
            fixtureOverrides.empty() ? nullptr : &fixtureOverrides);
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
    statusResponse.AccountGuid = GetBattlenetAccountGUID();
    statusResponse.OwnerPlayerGuid = player->GetGUID();
    statusResponse.NeighborhoodGuid = housing->GetNeighborhoodGuid();
    statusResponse.Status = 0;
    statusResponse.FlagByte = 0xC0; // bit7=Decor, bit6=Room only — Fixture context managed by dedicated ENTER/EXIT response
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
            TC_LOG_DEBUG("housing", "    Houses[{}]: Guid={} MapID={} PlotID={} Level={} NeighborhoodGUID={}",
                i, phData.Houses[i].Guid.ToString(), phData.Houses[i].MapID,
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

    HousingResult result = housing->PlaceDecorWithGuid(housingDecorPlace.DecorGuid, decorEntryId,
        posX, posY, posZ, rotX, rotY, rotZ, rotW, housingDecorPlace.RoomGuid);

    // Spawn decor MeshObject on the map if placement succeeded.
    // Sniff-verified: ALL retail decor is MeshObject (never GO). The server sends an UPDATE_OBJECT
    // CREATE for the MeshObject + FHousingDecor_C immediately after placement.
    if (result == HOUSING_RESULT_SUCCESS)
    {
        if (Housing::PlacedDecor const* newDecor = housing->GetPlacedDecor(housingDecorPlace.DecorGuid))
        {
            if (HousingMap* housingMap = dynamic_cast<HousingMap*>(player->GetMap()))
                housingMap->SpawnDecorItem(housing->GetPlotIndex(), *newDecor, housing->GetHouseGuid());
            else if (HouseInteriorMap* interiorMap = dynamic_cast<HouseInteriorMap*>(player->GetMap()))
                interiorMap->SpawnSingleInteriorDecor(*newDecor, housing->GetHouseGuid());
        }

        // Sniff: UPDATE_OBJECT (BNetAccount with ChangeType=3, HouseGUID=set) arrives BEFORE response.
        GetBattlenetAccount().SendUpdateToPlayer(player);
    }

    WorldPackets::Housing::HousingDecorPlaceResponse response;
    response.PlayerGuid = player->GetGUID();
    response.Field_09 = 0;
    response.DecorGuid = housingDecorPlace.DecorGuid;
    response.Result = static_cast<uint8>(result);
    WorldPacket const* placePkt = response.Write();
    TC_LOG_ERROR("housing", ">>> CMSG_HOUSING_DECOR_PLACE DecorGuid={} EntryId={} Pos=({:.3f},{:.3f},{:.3f}) Rot=({:.3f},{:.3f},{:.3f}) Scale={:.2f} RoomGuid={} AttachParent={}",
        housingDecorPlace.DecorGuid.ToString(), decorEntryId,
        posX, posY, posZ,
        housingDecorPlace.Rotation.Pos.GetPositionX(), housingDecorPlace.Rotation.Pos.GetPositionY(), housingDecorPlace.Rotation.Pos.GetPositionZ(),
        housingDecorPlace.Scale, housingDecorPlace.RoomGuid.ToString(), housingDecorPlace.AttachParentGuid.ToString());
    TC_LOG_ERROR("housing", "<<< SMSG_HOUSING_DECOR_PLACE_RESPONSE ({} bytes): {}",
        placePkt->size(), HexDumpPacket(placePkt));
    TC_LOG_ERROR("housing", "    PlayerGuid={} DecorGuid={} Result={}", response.PlayerGuid.ToString(), response.DecorGuid.ToString(), response.Result);
    SendPacket(placePkt);
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

    HousingResult result = housing->MoveDecor(housingDecorMove.DecorGuid,
        posX, posY, posZ, rotX, rotY, rotZ, rotW);

    // Update decor GO position on the map
    if (result == HOUSING_RESULT_SUCCESS)
    {
        Position newPos(posX, posY, posZ);
        QuaternionData newRot(rotX, rotY, rotZ, rotW);
        if (HousingMap* housingMap = dynamic_cast<HousingMap*>(player->GetMap()))
            housingMap->UpdateDecorPosition(housing->GetPlotIndex(), housingDecorMove.DecorGuid, newPos, newRot);
        else if (HouseInteriorMap* interiorMap = dynamic_cast<HouseInteriorMap*>(player->GetMap()))
            interiorMap->UpdateDecorPosition(housingDecorMove.DecorGuid, newPos, newRot);
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

    HousingResult result = housing->SelectFixtureOption(componentID, 0);

    WorldPackets::Housing::HousingFixtureSetCoreFixtureResponse response;
    response.Result = static_cast<uint8>(result);
    SendPacket(response.Write());

    if (result == HOUSING_RESULT_SUCCESS)
    {
        WorldPackets::Housing::AccountExteriorFixtureCollectionUpdate collectionUpdate;
        collectionUpdate.FixtureID = componentID;
        SendPacket(collectionUpdate.Write());

        // Respawn house visuals so the new fixture is visible immediately.
        // DespawnHouseForPlot removes ALL MeshObjects (house + decor), so we
        // must also respawn decor after rebuilding the house structure.
        if (HousingMap* housingMap = dynamic_cast<HousingMap*>(player->GetMap()))
        {
            uint8 plotIndex = housing->GetPlotIndex();
            auto fixtureOverrides = housing->GetFixtureOverrideMap();
            housingMap->DespawnAllDecorForPlot(plotIndex);
            housingMap->DespawnHouseForPlot(plotIndex);
            housingMap->SpawnHouseForPlot(plotIndex, nullptr,
                static_cast<int32>(housing->GetCoreExteriorComponentID()),
                static_cast<int32>(housing->GetHouseType()),
                fixtureOverrides.empty() ? nullptr : &fixtureOverrides);
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

    HousingResult result = housing->SelectFixtureOption(hookID, componentID);

    WorldPackets::Housing::HousingFixtureCreateFixtureResponse response;
    response.Result = static_cast<uint8>(result);
    SendPacket(response.Write());

    if (result == HOUSING_RESULT_SUCCESS)
    {
        WorldPackets::Housing::AccountExteriorFixtureCollectionUpdate collectionUpdate;
        collectionUpdate.FixtureID = componentID;
        SendPacket(collectionUpdate.Write());

        // Despawn old mesh at hook point and spawn the new fixture component
        if (HousingMap* housingMap = dynamic_cast<HousingMap*>(player->GetMap()))
        {
            uint8 plotIndex = housing->GetPlotIndex();

            if (MeshObject* oldMesh = housingMap->FindMeshObjectByHookID(plotIndex, static_cast<int32>(hookID)))
            {
                TC_LOG_DEBUG("housing", "CMSG_HOUSING_FIXTURE_CREATE_FIXTURE: Despawning old mesh {} at hook {}",
                    oldMesh->GetGUID().ToString(), hookID);
                housingMap->DespawnSingleMeshObject(plotIndex, oldMesh->GetGUID());
            }

            housingMap->SpawnFixtureAtHook(plotIndex, hookID, componentID,
                housing->GetHouseGuid(), static_cast<int32>(housing->GetHouseType()), player);
        }

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

    // Validate ExteriorComponentID against DB2 store
    ExteriorComponentEntry const* componentEntry = sExteriorComponentStore.LookupEntry(componentID);
    if (!componentEntry)
    {
        TC_LOG_DEBUG("housing", "CMSG_HOUSING_FIXTURE_DELETE_FIXTURE ExteriorComponentID {} not found in DB2", componentID);
        WorldPackets::Housing::HousingFixtureDeleteFixtureResponse response;
        response.Result = static_cast<uint8>(HOUSING_RESULT_FIXTURE_NOT_FOUND);
        SendPacket(response.Write());
        return;
    }

    TC_LOG_DEBUG("housing", "CMSG_HOUSING_FIXTURE_DELETE_FIXTURE DB2 lookup: ExteriorComponentID={}, Name='{}', Type={}, Flags={}",
        componentID, componentEntry->Name[DEFAULT_LOCALE] ? componentEntry->Name[DEFAULT_LOCALE] : "", componentEntry->Type, componentEntry->Flags);

    uint32 removedHookID = 0;
    HousingResult result = housing->RemoveFixture(componentID, &removedHookID);

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

            // Spawn default component back at this hook (DB2 default)
            ExteriorComponentEntry const* defaultComp = sHousingMgr.GetComponentAtHook(static_cast<int32>(removedHookID), housing->GetCoreExteriorComponentID());
            if (defaultComp)
            {
                housingMap->SpawnFixtureAtHook(plotIndex, removedHookID, defaultComp->ID,
                    housing->GetHouseGuid(), static_cast<int32>(housing->GetHouseType()), player);
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
        housingMap->DespawnAllDecorForPlot(plotIndex);
        housingMap->DespawnHouseForPlot(plotIndex);
        housingMap->SpawnHouseForPlot(plotIndex, nullptr,
            static_cast<int32>(housing->GetCoreExteriorComponentID()),
            static_cast<int32>(housing->GetHouseType()),
            fixtureOverrides.empty() ? nullptr : &fixtureOverrides);
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
        housingMap->DespawnAllDecorForPlot(plotIndex);
        housingMap->DespawnHouseForPlot(plotIndex);
        housingMap->SpawnHouseForPlot(plotIndex, nullptr,
            static_cast<int32>(housing->GetCoreExteriorComponentID()),
            static_cast<int32>(wmoDataID),
            fixtureOverrides.empty() ? nullptr : &fixtureOverrides);
        housingMap->SpawnAllDecorForPlot(plotIndex, housing);
    }

    // Sniff-verified packet order: SMSG response → UPDATE_OBJECT (~228B)
    WorldPackets::Housing::HousingFixtureSetHouseTypeResponse response;
    response.Result = static_cast<uint8>(HOUSING_RESULT_SUCCESS);
    response.HouseExteriorTypeID = wmoDataID;
    SendPacket(response.Write());

    // Notify account of house type collection update
    WorldPackets::Housing::AccountHouseTypeCollectionUpdate collectionUpdate;
    collectionUpdate.HouseTypeID = wmoDataID;
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

    // Sniff-verified wire format (10B both enter and exit):
    //   Enter: [8-byte OwnerGuid] 00 80
    //   Exit:  [8-byte OwnerGuid] 00 00
    WorldPackets::Housing::HousingRoomSetLayoutEditModeResponse response;
    response.RoomGuid = housing->GetHouseGuid(); // context GUID (sniff: "OwnerGuid")
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

    HousingResult result = housing->PlaceRoom(housingRoomAdd.HouseRoomID, housingRoomAdd.FloorIndex,
        housingRoomAdd.Flags, housingRoomAdd.AutoFurnish);

    WorldPackets::Housing::HousingRoomAddResponse response;
    response.Result = static_cast<uint8>(result);
    SendPacket(response.Write());

    if (result == HOUSING_RESULT_SUCCESS)
    {
        RefreshInteriorRoomVisuals(player, housing);

        WorldPackets::Housing::AccountRoomCollectionUpdate roomUpdate;
        roomUpdate.RoomID = housingRoomAdd.HouseRoomID;
        SendPacket(roomUpdate.Write());
    }

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
        response.Result = static_cast<uint8>(HOUSING_RESULT_HOUSE_NOT_FOUND);
        SendPacket(response.Write());
        return;
    }

    HousingResult result = housing->RemoveRoom(housingRoomRemove.RoomGuid);

    WorldPackets::Housing::HousingRoomRemoveResponse response;
    response.Result = static_cast<uint8>(result);
    response.RoomGuid = housingRoomRemove.RoomGuid;
    SendPacket(response.Write());

    if (result == HOUSING_RESULT_SUCCESS)
        RefreshInteriorRoomVisuals(player, housing);

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

    WorldPackets::Housing::HousingRoomUpdateResponse response;
    response.Result = static_cast<uint8>(result);
    response.RoomGuid = housingRoomRotate.RoomGuid;
    SendPacket(response.Write());

    if (result == HOUSING_RESULT_SUCCESS)
        RefreshInteriorRoomVisuals(player, housing);

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

    if (result == HOUSING_RESULT_SUCCESS)
        RefreshInteriorRoomVisuals(player, housing);

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
        housingRoomSetComponentTheme.HouseThemeID, housingRoomSetComponentTheme.RoomComponentIDs);

    WorldPackets::Housing::HousingRoomSetComponentThemeResponse response;
    response.Result = static_cast<uint8>(result);
    response.RoomGuid = housingRoomSetComponentTheme.RoomGuid;
    response.ThemeSetID = housingRoomSetComponentTheme.HouseThemeID;
    response.ComponentIDs.push_back(housingRoomSetComponentTheme.HouseThemeID);
    SendPacket(response.Write());

    if (result == HOUSING_RESULT_SUCCESS)
    {
        RefreshInteriorRoomVisuals(player, housing);

        WorldPackets::Housing::AccountRoomThemeCollectionUpdate themeUpdate;
        themeUpdate.ThemeID = housingRoomSetComponentTheme.HouseThemeID;
        SendPacket(themeUpdate.Write());
    }

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
        response.Result = static_cast<uint8>(HOUSING_RESULT_HOUSE_NOT_FOUND);
        SendPacket(response.Write());
        return;
    }

    HousingResult result = housing->ApplyRoomWallpaper(housingRoomApplyComponentMaterials.RoomGuid,
        housingRoomApplyComponentMaterials.RoomComponentTextureID, housingRoomApplyComponentMaterials.RoomComponentTypeParam,
        housingRoomApplyComponentMaterials.RoomComponentIDs);

    WorldPackets::Housing::HousingRoomApplyComponentMaterialsResponse response;
    response.Result = static_cast<uint8>(result);
    response.RoomGuid = housingRoomApplyComponentMaterials.RoomGuid;
    response.RoomComponentTextureRecordID = housingRoomApplyComponentMaterials.RoomComponentTextureID;
    response.ComponentIDs.push_back(housingRoomApplyComponentMaterials.RoomComponentTypeParam);
    SendPacket(response.Write());

    if (result == HOUSING_RESULT_SUCCESS)
    {
        RefreshInteriorRoomVisuals(player, housing);

        WorldPackets::Housing::AccountRoomMaterialCollectionUpdate materialUpdate;
        materialUpdate.MaterialID = housingRoomApplyComponentMaterials.RoomComponentTextureID;
        SendPacket(materialUpdate.Write());
    }

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
        response.Result = static_cast<uint8>(HOUSING_RESULT_HOUSE_NOT_FOUND);
        SendPacket(response.Write());
        return;
    }

    HousingResult result = housing->SetDoorType(housingRoomSetDoorType.RoomGuid,
        housingRoomSetDoorType.RoomComponentID, housingRoomSetDoorType.RoomComponentType);

    WorldPackets::Housing::HousingRoomSetDoorTypeResponse response;
    response.Result = static_cast<uint8>(result);
    response.RoomGuid = housingRoomSetDoorType.RoomGuid;
    response.ComponentID = housingRoomSetDoorType.RoomComponentID;
    response.DoorType = housingRoomSetDoorType.RoomComponentType;
    SendPacket(response.Write());

    if (result == HOUSING_RESULT_SUCCESS)
        RefreshInteriorRoomVisuals(player, housing);

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
        response.Result = static_cast<uint8>(HOUSING_RESULT_HOUSE_NOT_FOUND);
        SendPacket(response.Write());
        return;
    }

    HousingResult result = housing->SetCeilingType(housingRoomSetCeilingType.RoomGuid,
        housingRoomSetCeilingType.RoomComponentID, housingRoomSetCeilingType.RoomComponentType);

    WorldPackets::Housing::HousingRoomSetCeilingTypeResponse response;
    response.Result = static_cast<uint8>(result);
    response.RoomGuid = housingRoomSetCeilingType.RoomGuid;
    response.ComponentID = housingRoomSetCeilingType.RoomComponentID;
    response.CeilingType = housingRoomSetCeilingType.RoomComponentType;
    SendPacket(response.Write());

    if (result == HOUSING_RESULT_SUCCESS)
        RefreshInteriorRoomVisuals(player, housing);

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

    Neighborhood* neighborhood = sNeighborhoodMgr.CreateGuildNeighborhood(
        player->GetGUID(), housingSvcsGuildCreateNeighborhood.NeighborhoodName,
        housingSvcsGuildCreateNeighborhood.NeighborhoodTypeID,
        housingSvcsGuildCreateNeighborhood.Flags);

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

    if (!sWorld->getBoolConfig(CONFIG_HOUSING_ENABLE_BUY_HOUSE))
    {
        WorldPackets::Housing::HousingSvcsNeighborhoodReservePlotResponse response;
        response.Result = static_cast<uint8>(HOUSING_RESULT_SERVICE_NOT_AVAILABLE);
        SendPacket(response.Write());
        return;
    }

    // Housing warning gate — check expansion access, level requirements
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
                notification.House.HouseGUID = housing->GetHouseGuid();
                notification.House.OwnerGUID = player->GetGUID();
                notification.House.NeighborhoodGUID = housing->GetNeighborhoodGuid();
                notification.House.HouseLevel = static_cast<uint8>(housing->GetLevel());
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

            // Proactive storage response + Account entity update (retail-verified flow)
            WorldPackets::Housing::HousingDecorRequestStorageResponse storageResp;
            storageResp.ResultCode = static_cast<uint8>(HOUSING_RESULT_SUCCESS);
            SendPacket(storageResp.Write());
            GetBattlenetAccount().SendUpdateToPlayer(player);

            TC_LOG_ERROR("housing", "HandleHousingSvcsNeighborhoodReservePlot: Populated catalog with {} decor types, sent {} FirstTimeDecorAcquisition + storage + Account update",
                uint32(starterDecorWithQty.size()), uint32(starterDecorIds.size()));

            // Auto-place starter decor in the visual room (retail pre-places items).
            // The "Welcome Home" quest requires the player to remove 3 of these items.
            housing->PlaceStarterDecor();
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
                int32 extCompID = static_cast<int32>(housing->GetCoreExteriorComponentID());
                int32 wmoDataID = static_cast<int32>(housing->GetHouseType());
                GameObject* houseGo = nullptr;
                if (housing->HasCustomPosition())
                {
                    Position customPos = housing->GetHousePosition();
                    houseGo = housingMap->SpawnHouseForPlot(plotIndex, &customPos, extCompID, wmoDataID);
                }
                else
                    houseGo = housingMap->SpawnHouseForPlot(plotIndex, nullptr, extCompID, wmoDataID);

                TC_LOG_ERROR("housing", "HandleHousingSvcsNeighborhoodReservePlot: SpawnHouseForPlot for plot {}: {}",
                    plotIndex, houseGo ? houseGo->GetGUID().ToString() : "FAILED");

                // Update PlayerMirrorHouse.MapID so the client knows this house is on the current map.
                // Without this, MapID stays at 0 and the client rejects edit mode.
                player->UpdateHousingMapId(housing->GetHouseGuid(), static_cast<int32>(player->GetMapId()));

                // Mark the player as on their newly purchased plot and send enter spells
                housingMap->SetPlayerCurrentPlot(player->GetGUID(), plotIndex);
                housingMap->SendPlotEnterSpellPackets(player, plotIndex);

                TC_LOG_DEBUG("housing", "HandleHousingSvcsNeighborhoodReservePlot: Sent plot enter spells for plot {} to player {}",
                    plotIndex, player->GetGUID().ToString());

                // Re-send HousingGetCurrentHouseInfoResponse with actual house data.
                // The initial send (during AddPlayerToMap) had no house info since the player hadn't purchased yet.
                WorldPackets::Housing::HousingGetCurrentHouseInfoResponse houseInfo;
                houseInfo.House.HouseGuid = housing->GetHouseGuid();
                houseInfo.House.OwnerGuid = player->GetGUID();
                houseInfo.House.NeighborhoodGuid = housing->GetNeighborhoodGuid();
                houseInfo.House.PlotId = housing->GetPlotIndex();
                houseInfo.House.AccessFlags = housing->GetSettingsFlags();
                houseInfo.House.HasMoveOutTime = false;
                houseInfo.Result = 0;
                SendPacket(houseInfo.Write());

                TC_LOG_ERROR("housing", "HandleHousingSvcsNeighborhoodReservePlot: Re-sent CURRENT_HOUSE_INFO: PlotId={}, HouseGuid={}, NeighborhoodGuid={}",
                    houseInfo.House.PlotId, houseInfo.House.HouseGuid.ToString(), houseInfo.House.NeighborhoodGuid.ToString());
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

        // Refresh mirror data for all online neighborhood members so Houses[] array is updated
        neighborhood->RefreshMirrorDataForOnlineMembers();
    }

    TC_LOG_INFO("housing", "CMSG_HOUSING_SVCS_NEIGHBORHOOD_RESERVE_PLOT PlotIndex: {} (client sent {}), Result: {}",
        plotIndex, housingSvcsNeighborhoodReservePlot.PlotIndex, uint32(result));
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

    // The client's PlotIndex is a roster/array index, NOT the DB2 NeighborhoodPlot PlotIndex.
    // We must resolve the actual DB2 PlotIndex from server-side data.
    uint32 clientPlotIndex = housingSvcsTeleportToPlot.PlotIndex;
    uint32 resolvedPlotIndex = clientPlotIndex; // will be overwritten below

    // Strategy 1: If the player owns a house in this neighborhood, use their server-side PlotIndex
    Housing* playerHousing = player->GetHousing();
    bool isSelfTeleport = false;
    if (playerHousing && playerHousing->GetNeighborhoodGuid() == neighborhood->GetGuid())
    {
        resolvedPlotIndex = playerHousing->GetPlotIndex();
        isSelfTeleport = true;
    }
    else
    {
        // Strategy 2: Find the plot by matching OwnerGuid against neighborhood plots
        ObjectGuid const& ownerGuid = housingSvcsTeleportToPlot.OwnerGuid;
        for (auto const& plotInfo : neighborhood->GetPlots())
        {
            if (!plotInfo.IsOccupied())
                continue;
            // Match against HouseGuid or OwnerGuid (client may send either)
            if (plotInfo.HouseGuid == ownerGuid || plotInfo.OwnerGuid == ownerGuid)
            {
                resolvedPlotIndex = plotInfo.PlotIndex;
                break;
            }
        }
    }

    TC_LOG_INFO("housing", "HandleHousingSvcsTeleportToPlot: ClientPlotIndex={} ResolvedPlotIndex={} OwnerGuid={} TeleportType={} IsSelf={} NeighborhoodGUID={}",
        clientPlotIndex, resolvedPlotIndex, housingSvcsTeleportToPlot.OwnerGuid.ToString(), housingSvcsTeleportToPlot.TeleportType,
        isSelfTeleport, housingSvcsTeleportToPlot.NeighborhoodGuid.ToString());

    // Look up the neighborhood map data for map ID and plot positions
    NeighborhoodMapData const* mapData = sHousingMgr.GetNeighborhoodMapData(neighborhood->GetNeighborhoodMapID());
    if (!mapData)
    {
        WorldPackets::Housing::HousingSvcsNotifyPermissionsFailure response;
        response.FailureType = static_cast<uint8>(HOUSING_RESULT_PLOT_NOT_FOUND);
        SendPacket(response.Write());
        return;
    }

    // Find the DB2 plot matching our resolved PlotIndex
    std::vector<NeighborhoodPlotData const*> plots = sHousingMgr.GetPlotsForMap(neighborhood->GetNeighborhoodMapID());
    NeighborhoodPlotData const* targetPlot = nullptr;
    for (NeighborhoodPlotData const* plot : plots)
    {
        if (plot->PlotIndex == static_cast<int32>(resolvedPlotIndex))
        {
            targetPlot = plot;
            break;
        }
    }
    // Fallback: try client's raw PlotIndex in case it actually matches a DB2 plot
    if (!targetPlot && resolvedPlotIndex != clientPlotIndex)
    {
        for (NeighborhoodPlotData const* plot : plots)
        {
            if (plot->PlotIndex == static_cast<int32>(clientPlotIndex))
            {
                targetPlot = plot;
                break;
            }
        }
    }

    if (targetPlot)
    {
        // Per-house access check: verify visitor has permission to access this plot
        // Owner and same-neighborhood members skip this check
        if (!neighborhood->IsMember(player->GetGUID()))
        {
            uint8 targetIdx = static_cast<uint8>(resolvedPlotIndex);
            Neighborhood::PlotInfo const* plotInfo = neighborhood->GetPlotInfo(targetIdx);
            if (plotInfo && plotInfo->IsOccupied())
            {
                Player* ownerPlayer = ObjectAccessor::FindPlayer(plotInfo->OwnerGuid);
                uint32 settingsFlags = HOUSE_SETTING_DEFAULT;
                if (ownerPlayer && ownerPlayer->GetHousing())
                    settingsFlags = ownerPlayer->GetHousing()->GetSettingsFlags();

                if (!sHousingMgr.CanVisitorAccess(player, ownerPlayer, settingsFlags, false))
                {
                    WorldPackets::Housing::HousingSvcsNotifyPermissionsFailure denied;
                    denied.FailureType = static_cast<uint8>(HOUSING_RESULT_PERMISSION_DENIED);
                    SendPacket(denied.Write());
                    TC_LOG_DEBUG("housing", "HandleHousingSvcsTeleportToPlot: Player {} denied access to plot {} (settingsFlags=0x{:X})",
                        player->GetGUID().ToString(), resolvedPlotIndex, settingsFlags);
                    return;
                }
            }
        }

        player->TeleportTo(mapData->MapID, targetPlot->TeleportPosition[0], targetPlot->TeleportPosition[1],
            targetPlot->TeleportPosition[2], 0.0f);

        TC_LOG_INFO("housing", "CMSG_HOUSING_SVCS_TELEPORT_TO_PLOT: Teleporting player {} to plot {} (clientIdx={}) on map {}",
            player->GetGUID().ToString(), resolvedPlotIndex, clientPlotIndex, mapData->MapID);
    }
    else
    {
        player->TeleportTo(mapData->MapID, mapData->Origin[0], mapData->Origin[1],
            mapData->Origin[2], 0.0f);

        TC_LOG_INFO("housing", "CMSG_HOUSING_SVCS_TELEPORT_TO_PLOT: Plot {} (clientIdx={}) not found, teleporting to neighborhood origin on map {}",
            resolvedPlotIndex, clientPlotIndex, mapData->MapID);
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

void WorldSession::HandleHousingSvcsSetTutorialState(WorldPackets::Housing::HousingSvcsSetTutorialState const& housingSvcsSetTutorialState)
{
    Player* player = GetPlayer();
    if (!player)
        return;

    TC_LOG_DEBUG("housing", "CMSG_HOUSING_SVCS_SET_TUTORIAL_STATE Player: {} TutorialFlags: {}",
        player->GetGUID().ToString(), housingSvcsSetTutorialState.TutorialFlags);

    // Tutorial state is volatile per-session — the client tracks tutorial progress
    // and sends state updates. No server-side persistence required; the tutorial
    // quest (91863) completion is the authoritative progression marker.
}

void WorldSession::HandleHousingSvcsCompleteTutorialStep(WorldPackets::Housing::HousingSvcsCompleteTutorialStep const& housingSvcsCompleteTutorialStep)
{
    Player* player = GetPlayer();
    if (!player)
        return;

    TC_LOG_DEBUG("housing", "CMSG_HOUSING_SVCS_COMPLETE_TUTORIAL_STEP Player: {} StepIndex: {}",
        player->GetGUID().ToString(), housingSvcsCompleteTutorialStep.StepIndex);

    // Tutorial step completion is informational — the actual progression is
    // driven by quest objectives (91863). The client advances its own tutorial
    // state machine; we just acknowledge receipt.
}

void WorldSession::HandleHousingSvcsSkipTutorial(WorldPackets::Housing::HousingSvcsSkipTutorial const& /*housingSvcsSkipTutorial*/)
{
    Player* player = GetPlayer();
    if (!player)
        return;

    TC_LOG_DEBUG("housing", "CMSG_HOUSING_SVCS_SKIP_TUTORIAL Player: {}", player->GetGUID().ToString());

    // Mark tutorial as complete by auto-completing the tutorial quest
    static constexpr uint32 QUEST_MY_FIRST_HOME = 91863;
    if (Quest const* quest = sObjectMgr->GetQuestTemplate(QUEST_MY_FIRST_HOME))
    {
        QuestStatus status = player->GetQuestStatus(QUEST_MY_FIRST_HOME);
        if (status == QUEST_STATUS_INCOMPLETE || status == QUEST_STATUS_COMPLETE)
        {
            player->CompleteQuest(QUEST_MY_FIRST_HOME);
            TC_LOG_INFO("housing", "CMSG_HOUSING_SVCS_SKIP_TUTORIAL: Auto-completed quest {} for player {}",
                QUEST_MY_FIRST_HOME, player->GetGUID().ToString());
        }
    }
}

void WorldSession::HandleHousingSvcsQueryPendingInvites(WorldPackets::Housing::HousingSvcsQueryPendingInvites const& /*housingSvcsQueryPendingInvites*/)
{
    Player* player = GetPlayer();
    if (!player)
        return;

    TC_LOG_DEBUG("housing", "CMSG_HOUSING_SVCS_QUERY_PENDING_INVITES Player: {}", player->GetGUID().ToString());

    // Find any neighborhood that has a pending invite for this player
    Neighborhood* neighborhood = sNeighborhoodMgr.FindNeighborhoodWithPendingInvite(player->GetGUID());

    WorldPackets::Neighborhood::NeighborhoodGetInvitesResponse response;
    if (neighborhood)
    {
        WorldPackets::Housing::JamNeighborhoodRosterEntry invite;
        invite.PlayerGuid = neighborhood->GetOwnerGuid();
        invite.HouseGuid = neighborhood->GetGuid();
        invite.Timestamp = static_cast<uint64>(GameTime::GetGameTime());
        response.Invites.push_back(invite);
    }
    SendPacket(response.Write());
}

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

    WorldPackets::Housing::HousingSvcsGetPotentialHouseOwnersResponse response;
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

    // Return list of public neighborhoods available through the finder, filtered by faction
    std::vector<Neighborhood*> publicNeighborhoods = sNeighborhoodMgr.GetPublicNeighborhoods();
    uint32 playerTeam = player->GetTeam();

    WorldPackets::Housing::HousingSvcsGetHouseFinderInfoResponse response;
    response.Result = static_cast<uint8>(HOUSING_RESULT_SUCCESS);
    response.Entries.reserve(publicNeighborhoods.size());
    for (Neighborhood const* neighborhood : publicNeighborhoods)
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
        // Field1 | Field2 is a BITMASK of occupied plot indices (IDA: client ORs them at offset 520,
        // then checks (1LL << plotIndex) & bitmask to determine if plot is occupied on the finder map).
        uint64 occupiedBitmask = 0;
        for (auto const& plot : neighborhood->GetPlots())
        {
            if (plot.IsOccupied() && plot.PlotIndex < 64)
                occupiedBitmask |= (uint64(1) << plot.PlotIndex);
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
            TC_LOG_INFO("housing", "  PLOT[{}]: occupied owner={} house={} bnet={}",
                i, plot.OwnerGuid.ToString(), plot.HouseGuid.ToString(), plot.OwnerBnetGuid.ToString());
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
        house.HouseLevel = static_cast<uint8>(plot.PlotIndex); // Client uses HouseLevel (offset 48) as hash key for plot lookup
        house.PlotIndex = plot.PlotIndex;
        response.Neighborhood.Houses.push_back(std::move(house));

        TC_LOG_INFO("housing", "  DETAIL_HOUSE: plotIndex={} houseLevel={} houseGuid={} ownerGuid={}",
            plot.PlotIndex, static_cast<uint8>(plot.PlotIndex), plot.HouseGuid.ToString(), plot.OwnerGuid.ToString());
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
    mirrorEntity.SendUpdateToPlayer(player);

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
            response.AccountGuid = plotInfo->OwnerBnetGuid; // BNetAccount GUID of plot owner
            response.OwnerPlayerGuid = plotInfo->OwnerGuid; // Plot owner's Player GUID
            response.Status = 0; // Visitor is outside (exterior)
            // Visitors get no editing permissions (FlagByte stays 0)
        }
        else if (ownHousing)
        {
            // On own plot
            response.HouseGuid = ownHousing->GetHouseGuid();
            response.AccountGuid = GetBattlenetAccountGUID();
            response.OwnerPlayerGuid = player->GetGUID();
            response.NeighborhoodGuid = ownHousing->GetNeighborhoodGuid();
            response.Status = ownHousing->IsInInterior() ? 1 : 0;
            response.FlagByte = 0xE0; // bit7=houseEditing, bit6=plotEntry, bit5=houseEntry (sniff-verified)
        }
    }
    else if (ownHousing)
    {
        // Not on any tracked plot, fall back to own housing data
        response.HouseGuid = ownHousing->GetHouseGuid();
        response.AccountGuid = GetBattlenetAccountGUID();
        response.OwnerPlayerGuid = player->GetGUID();
        response.NeighborhoodGuid = ownHousing->GetNeighborhoodGuid();
        response.Status = ownHousing->IsInInterior() ? 1 : 0;
        response.FlagByte = 0xE0; // bit7=houseEditing, bit6=plotEntry, bit5=houseEntry (sniff-verified)
    }
    // No house and not on a plot: all fields stay at defaults (empty GUIDs, Status=0).
    WorldPacket const* statusPkt = response.Write();
    TC_LOG_ERROR("housing", ">>> CMSG_HOUSING_HOUSE_STATUS (visitedPlot: {})", visitedPlot);
    TC_LOG_ERROR("housing", "<<< SMSG_HOUSING_HOUSE_STATUS_RESPONSE ({} bytes): {}",
        statusPkt->size(), HexDumpPacket(statusPkt));
    TC_LOG_ERROR("housing", "    HouseGuid={} AccountGuid={} OwnerPlayerGuid={} NeighborhoodGuid={} Status={} FlagByte=0x{:02X}",
        response.HouseGuid.ToString(), response.AccountGuid.ToString(),
        response.OwnerPlayerGuid.ToString(), response.NeighborhoodGuid.ToString(),
        response.Status, response.FlagByte);
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
    int8 currentPlot = housingMap ? housingMap->GetPlayerCurrentPlot(player->GetGUID()) : -1;

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
                    housingMap->DespawnAllDecorForPlot(plotIndex);
                    housingMap->DespawnHouseForPlot(plotIndex);
                    housingMap->SpawnHouseForPlot(plotIndex, nullptr,
                        static_cast<int32>(housing->GetCoreExteriorComponentID()),
                        static_cast<int32>(styleID),
                        fixtureOverrides.empty() ? nullptr : &fixtureOverrides);
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

void WorldSession::HandleHousingSvcsGetPlayerHousesInfoAlt(WorldPackets::Housing::HousingSvcsGetPlayerHousesInfoAlt const& housingSvcsGetPlayerHousesInfoAlt)
{
    Player* player = GetPlayer();
    if (!player)
        return;

    TC_LOG_DEBUG("housing", "CMSG_HOUSING_SVCS_GET_PLAYER_HOUSES_INFO_ALT Player: {} TargetPlayerGuid: {}",
        player->GetGUID().ToString(), housingSvcsGetPlayerHousesInfoAlt.PlayerGuid.ToString());

    WorldPackets::Housing::HousingSvcsGetPlayerHousesInfoResponse response;
    response.Result = static_cast<uint8>(HOUSING_RESULT_SUCCESS);

    // Look up the target player's housing (may be self or another online player)
    Player* targetPlayer = ObjectAccessor::FindPlayer(housingSvcsGetPlayerHousesInfoAlt.PlayerGuid);
    if (!targetPlayer)
        targetPlayer = player; // Fall back to self if target not online

    Housing* housing = targetPlayer->GetHousing();
    if (housing && !housing->GetHouseGuid().IsEmpty())
    {
        WorldPackets::Housing::JamCliHouse house;
        house.OwnerGUID = targetPlayer->GetGUID();
        house.HouseGUID = housing->GetHouseGuid();
        house.NeighborhoodGUID = housing->GetNeighborhoodGuid();
        house.HouseLevel = static_cast<uint8>(housing->GetLevel());
        house.PlotIndex = housing->GetPlotIndex();
        response.Houses.push_back(house);
    }

    SendPacket(response.Write());
}

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
        mirrorEntity.SendUpdateToPlayer(player);
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

void WorldSession::HandleHousingSvcsChangeHouseCosmeticOwner(WorldPackets::Housing::HousingSvcsChangeHouseCosmeticOwnerRequest const& housingSvcsChangeHouseCosmeticOwner)
{
    Player* player = GetPlayer();
    if (!player)
        return;

    TC_LOG_DEBUG("housing", "CMSG_HOUSING_SVCS_CHANGE_HOUSE_COSMETIC_OWNER Player: {} HouseGuid: {} NewOwnerGuid: {}",
        player->GetGUID().ToString(), housingSvcsChangeHouseCosmeticOwner.HouseGuid.ToString(),
        housingSvcsChangeHouseCosmeticOwner.NewOwnerGuid.ToString());

    Housing* housing = player->GetHousing();

    // Only the house owner can change the cosmetic owner
    if (!housing || housing->GetHouseGuid() != housingSvcsChangeHouseCosmeticOwner.HouseGuid)
    {
        TC_LOG_DEBUG("housing", "CMSG_HOUSING_SVCS_CHANGE_HOUSE_COSMETIC_OWNER: Permission denied — not house owner");
        return;
    }

    // Verify target player exists (can be self to reclaim cosmetic ownership)
    ObjectGuid newOwnerGuid = housingSvcsChangeHouseCosmeticOwner.NewOwnerGuid;
    if (!newOwnerGuid.IsEmpty() && newOwnerGuid != player->GetGUID())
    {
        Player* targetPlayer = ObjectAccessor::FindPlayer(newOwnerGuid);
        if (!targetPlayer)
        {
            TC_LOG_DEBUG("housing", "CMSG_HOUSING_SVCS_CHANGE_HOUSE_COSMETIC_OWNER: Target player {} not found",
                newOwnerGuid.ToString());
            return;
        }
    }

    // Persist the cosmetic owner change
    housing->SetCosmeticOwnerGuid(newOwnerGuid);

    WorldPackets::Housing::HousingSvcsChangeHouseCosmeticOwner response;
    response.HouseGuid = housingSvcsChangeHouseCosmeticOwner.HouseGuid;
    response.NewOwnerGuid = newOwnerGuid;
    SendPacket(response.Write());

    TC_LOG_INFO("housing", "CMSG_HOUSING_SVCS_CHANGE_HOUSE_COSMETIC_OWNER: Player {} changed cosmetic owner of house {} to {}",
        player->GetName(), housing->GetHouseGuid().ToString(), newOwnerGuid.ToString());
}

void WorldSession::HandleHousingSvcsQueryHouseLevelFavor(WorldPackets::Housing::HousingSvcsQueryHouseLevelFavor const& housingSvcsQueryHouseLevelFavor)
{
    Player* player = GetPlayer();
    if (!player)
        return;

    TC_LOG_DEBUG("housing", "CMSG_HOUSING_SVCS_QUERY_HOUSE_LEVEL_FAVOR Player: {} HouseGuid: {}",
        player->GetGUID().ToString(), housingSvcsQueryHouseLevelFavor.HouseGuid.ToString());

    Housing* housing = player->GetHousing();

    WorldPackets::Housing::HousingSvcsUpdateHousesLevelFavor response;
    response.Type = 0; // Query response type

    if (housing && housing->GetHouseGuid() == housingSvcsQueryHouseLevelFavor.HouseGuid)
    {
        response.Field1 = static_cast<uint32>(housing->GetFavor());
        response.Field2 = static_cast<uint32>(housing->GetLevel());

        WorldPackets::Housing::HousingSvcsUpdateHousesLevelFavor::LevelFavorEntry entry;
        entry.OwnerGUID = player->GetGUID();
        entry.HouseGUID = housing->GetHouseGuid();
        entry.NeighborhoodGUID = housing->GetNeighborhoodGuid();
        entry.FavorAmount = static_cast<uint32>(housing->GetFavor());
        entry.Level = static_cast<uint32>(housing->GetLevel());
        response.Entries.push_back(std::move(entry));
    }

    SendPacket(response.Write());
}

void WorldSession::HandleHousingSvcsGuildAddHouse(WorldPackets::Housing::HousingSvcsGuildAddHouse const& housingSvcsGuildAddHouse)
{
    Player* player = GetPlayer();
    if (!player)
        return;

    TC_LOG_DEBUG("housing", "CMSG_HOUSING_SVCS_GUILD_ADD_HOUSE Player: {} HouseGuid: {}",
        player->GetGUID().ToString(), housingSvcsGuildAddHouse.HouseGuid.ToString());

    Housing* housing = player->GetHousing();
    if (!housing || housing->GetHouseGuid() != housingSvcsGuildAddHouse.HouseGuid)
    {
        TC_LOG_DEBUG("housing", "CMSG_HOUSING_SVCS_GUILD_ADD_HOUSE: House not found or mismatch");
        return;
    }

    Guild* guild = player->GetGuild();
    if (!guild)
    {
        TC_LOG_DEBUG("housing", "CMSG_HOUSING_SVCS_GUILD_ADD_HOUSE: Player not in guild");
        return;
    }

    // Associate the player's neighborhood with the guild
    Neighborhood* neighborhood = sNeighborhoodMgr.GetNeighborhood(housing->GetNeighborhoodGuid());
    if (neighborhood)
    {
        // Only the neighborhood owner can link it to a guild
        if (neighborhood->IsOwner(player->GetGUID()))
            neighborhood->SetGuildId(guild->GetId());
        else
        {
            TC_LOG_DEBUG("housing", "CMSG_HOUSING_SVCS_GUILD_ADD_HOUSE: Player {} is not neighborhood owner", player->GetName());
            return;
        }
    }

    WorldPackets::Housing::HousingSvcsGuildAddHouseNotification response;
    response.House.HouseGUID = housingSvcsGuildAddHouse.HouseGuid;
    response.House.OwnerGUID = player->GetGUID();
    if (housing)
    {
        response.House.NeighborhoodGUID = housing->GetNeighborhoodGuid();
        response.House.HouseLevel = static_cast<uint8>(housing->GetLevel());
    }
    SendPacket(response.Write());

    TC_LOG_INFO("housing", "CMSG_HOUSING_SVCS_GUILD_ADD_HOUSE: Player {} added house {} to guild {} (neighborhood {})",
        player->GetName(), housing->GetHouseGuid().ToString(), guild->GetName(),
        housing->GetNeighborhoodGuid().ToString());
}

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
    if (housing && !housing->GetHouseGuid().IsEmpty())
    {
        response.HouseGuid = housing->GetHouseGuid();
        response.AccountGuid = GetBattlenetAccountGUID();
        response.OwnerPlayerGuid = player->GetGUID();
        response.NeighborhoodGuid = housing->GetNeighborhoodGuid();
        response.Status = 1; // Active
        response.FlagByte = 0xC0; // bit7=Decor, bit6=Room only — Fixture context managed by dedicated ENTER/EXIT response
    }
    else
    {
        response.HouseGuid = ObjectGuid::Empty;
        response.AccountGuid = GetBattlenetAccountGUID();
        response.OwnerPlayerGuid = player->GetGUID();
        response.NeighborhoodGuid = ObjectGuid::Empty;
        response.Status = 0; // No house
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
        json += ",\"wallpaperId\":" + std::to_string(rooms[i]->WallpaperId);
        json += ",\"materialId\":" + std::to_string(rooms[i]->MaterialId) + "}";
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
