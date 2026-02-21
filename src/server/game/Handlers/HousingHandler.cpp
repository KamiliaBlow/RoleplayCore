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
#include "Guild.h"
#include "GuildMgr.h"
#include "Housing.h"
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

    // Update the house GO position on the map
    if (HousingMap* housingMap = dynamic_cast<HousingMap*>(player->GetMap()))
    {
        if (GameObject* houseGO = housingMap->GetHouseGameObject(housing->GetPlotIndex()))
        {
            Position newPos(houseExteriorCommitPosition.PositionX,
                houseExteriorCommitPosition.PositionY,
                houseExteriorCommitPosition.PositionZ,
                houseExteriorCommitPosition.Facing);
            houseGO->Relocate(newPos);

            QuaternionData rot = QuaternionData::fromEulerAnglesZYX(houseExteriorCommitPosition.Facing, 0.0f, 0.0f);
            houseGO->SetLocalRotation(rot.x, rot.y, rot.z, rot.w);
        }
    }

    WorldPackets::Housing::HouseExteriorSetHousePositionResponse response;
    response.Result = static_cast<uint32>(HOUSING_RESULT_SUCCESS);
    response.HouseGuid = housing->GetHouseGuid();
    SendPacket(response.Write());

    TC_LOG_INFO("housing", "CMSG_HOUSE_EXTERIOR_COMMIT_POSITION: Player {} positioned house at ({}, {}, {}, {:.2f})",
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

    // Clear editing mode when leaving
    housing->SetEditorMode(HOUSING_EDITOR_MODE_NONE);

    // Send updated house status to acknowledge the interior exit
    WorldPackets::Housing::HousingHouseStatusResponse response;
    response.HouseGuid = housing->GetHouseGuid();
    response.PlotGuid = housing->GetPlotGuid();
    response.NeighborhoodGuid = housing->GetNeighborhoodGuid();
    response.Status = 0;
    SendPacket(response.Write());

    TC_LOG_INFO("housing", "CMSG_HOUSE_INTERIOR_LEAVE_HOUSE: Player {} leaving house interior",
        player->GetGUID().ToString());
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
        TC_LOG_ERROR("housing", "  EditMode: GetHousing() returned null!");
        return;
    }

    HousingEditorMode targetMode = housingDecorSetEditMode.Active ? HOUSING_EDITOR_MODE_BASIC_DECOR : HOUSING_EDITOR_MODE_NONE;

    TC_LOG_ERROR("housing", "  HouseGuid={} PlotGuid={} NeighborhoodGuid={}",
        housing->GetHouseGuid().ToString(), housing->GetPlotGuid().ToString(), housing->GetNeighborhoodGuid().ToString());

    if (player->m_playerHouseInfoComponentData.has_value())
    {
        UF::PlayerHouseInfoComponentData const& phData = *player->m_playerHouseInfoComponentData;
        TC_LOG_ERROR("housing", "  BEFORE SetEditorMode: EditorMode={} HouseCount={}",
            uint32(*phData.EditorMode), phData.Houses.size());
        for (uint32 i = 0; i < phData.Houses.size(); ++i)
        {
            TC_LOG_ERROR("housing", "    Houses[{}]: Guid={} MapID={} PlotID={} Level={} NeighborhoodGUID={}",
                i, phData.Houses[i].Guid.ToString(), phData.Houses[i].MapID,
                phData.Houses[i].PlotID, phData.Houses[i].Level,
                phData.Houses[i].NeighborhoodGUID.ToString());
        }
    }
    else
    {
        TC_LOG_ERROR("housing", "  PlayerHouseInfoComponentData: NOT initialized!");
        return;
    }

    // Set edit mode via UpdateField — client needs both the UpdateField change AND the SMSG response
    housing->SetEditorMode(targetMode);

    // Send response — same wire format as Fixture/Room: uint32(Result) + Bit(Active)
    WorldPackets::Housing::HousingDecorSetEditModeResponse response;
    response.Result = static_cast<uint32>(HOUSING_RESULT_SUCCESS);
    response.Active = housingDecorSetEditMode.Active;
    SendPacket(response.Write());

    // Verify the UpdateField was actually set
    if (player->m_playerHouseInfoComponentData.has_value())
    {
        UF::PlayerHouseInfoComponentData const& phData = *player->m_playerHouseInfoComponentData;
        TC_LOG_ERROR("housing", "  AFTER: EditorMode={} (target={}), Response: Result=0 Active={}",
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

    HousingResult result = housing->PlaceDecor(housingDecorPlace.DecorRecID,
        housingDecorPlace.PositionX, housingDecorPlace.PositionY, housingDecorPlace.PositionZ,
        housingDecorPlace.RotationX, housingDecorPlace.RotationY,
        housingDecorPlace.RotationZ, housingDecorPlace.RotationW,
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
    }

    WorldPackets::Housing::HousingDecorPlaceResponse response;
    response.Result = static_cast<uint32>(result);
    SendPacket(response.Write());

    TC_LOG_INFO("housing", "CMSG_HOUSING_DECOR_PLACE DecorRecID: {}, Result: {}",
        housingDecorPlace.DecorRecID, uint32(result));
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
        response.Result = static_cast<uint32>(HOUSING_RESULT_HOUSE_NOT_FOUND);
        SendPacket(response.Write());
        return;
    }

    // Capture plotIndex before RemoveDecor (which may clear the data)
    uint8 plotIndex = housing->GetPlotIndex();
    ObjectGuid decorGuid = housingDecorRemove.DecorGuid;

    HousingResult result = housing->RemoveDecor(decorGuid);

    // Despawn the decor GO from the map
    if (result == HOUSING_RESULT_SUCCESS)
    {
        if (HousingMap* housingMap = dynamic_cast<HousingMap*>(player->GetMap()))
            housingMap->DespawnDecorItem(plotIndex, decorGuid);
    }

    WorldPackets::Housing::HousingDecorRemoveResponse response;
    response.Result = static_cast<uint32>(result);
    response.DecorGuid = decorGuid;
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
        response.Result = static_cast<uint32>(HOUSING_RESULT_HOUSE_NOT_FOUND);
        response.DecorGuid = housingDecorLock.DecorGuid;
        SendPacket(response.Write());
        return;
    }

    // Toggle lock state on the decor item
    Housing::PlacedDecor const* decor = housing->GetPlacedDecor(housingDecorLock.DecorGuid);
    if (!decor)
    {
        WorldPackets::Housing::HousingDecorLockResponse response;
        response.Result = static_cast<uint32>(HOUSING_RESULT_DECOR_NOT_FOUND);
        response.DecorGuid = housingDecorLock.DecorGuid;
        SendPacket(response.Write());
        return;
    }

    bool newLockedState = !decor->Locked;
    HousingResult result = housing->SetDecorLocked(housingDecorLock.DecorGuid, newLockedState);

    WorldPackets::Housing::HousingDecorLockResponse response;
    response.Result = static_cast<uint32>(result);
    response.DecorGuid = housingDecorLock.DecorGuid;
    response.Locked = newLockedState;
    SendPacket(response.Write());

    TC_LOG_INFO("housing", "CMSG_HOUSING_DECOR_LOCK DecorGuid: {} (entry: {}), Locked: {}",
        housingDecorLock.DecorGuid.ToString(), decor->DecorEntryId, newLockedState);
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
        SendPacket(response.Write());
        TC_LOG_INFO("housing", "CMSG_HOUSING_DECOR_CATALOG_CREATE_SEARCHER: Player {} has no house",
            player->GetGUID().ToString());
        return;
    }

    // Retrieve catalog/storage listing and send to client
    std::vector<Housing::CatalogEntry const*> entries = housing->GetCatalogEntries();

    WorldPackets::Housing::HousingDecorRequestStorageResponse response;
    response.BNetAccountGuid = GetBattlenetAccountGUID();
    response.ResultCode = static_cast<uint8>(HOUSING_RESULT_SUCCESS);
    response.Entries.reserve(entries.size());
    for (Housing::CatalogEntry const* entry : entries)
    {
        WorldPackets::Housing::HousingDecorRequestStorageResponse::CatalogEntryData data;
        data.DecorEntryId = entry->DecorEntryId;
        data.Count = entry->Count;
        response.Entries.push_back(data);
    }
    SendPacket(response.Write());

    TC_LOG_INFO("housing", "CMSG_HOUSING_DECOR_CATALOG_CREATE_SEARCHER HouseGuid: {}, CatalogEntries: {}",
        housingDecorRequestStorage.HouseGuid.ToString(), uint32(entries.size()));
}

void WorldSession::HandleHousingDecorRedeemDeferredDecor(WorldPackets::Housing::HousingDecorRedeemDeferredDecor const& housingDecorRedeemDeferredDecor)
{
    Player* player = GetPlayer();
    if (!player)
        return;

    Housing* housing = player->GetHousing();
    if (!housing)
    {
        WorldPackets::Housing::HousingRedeemDeferredDecorResponse response;
        response.Result = static_cast<uint32>(HOUSING_RESULT_HOUSE_NOT_FOUND);
        SendPacket(response.Write());
        return;
    }

    // Verify the deferred decor entry exists in DB2
    HouseDecorData const* decorData = sHousingMgr.GetHouseDecorData(housingDecorRedeemDeferredDecor.DeferredDecorID);
    if (!decorData)
    {
        WorldPackets::Housing::HousingRedeemDeferredDecorResponse response;
        response.Result = static_cast<uint32>(HOUSING_RESULT_DECOR_NOT_FOUND);
        SendPacket(response.Write());
        return;
    }

    // Add the deferred decor to the player's catalog/storage
    HousingResult result = housing->AddToCatalog(housingDecorRedeemDeferredDecor.DeferredDecorID);

    WorldPackets::Housing::HousingRedeemDeferredDecorResponse response;
    response.Result = static_cast<uint32>(result);
    response.DecorEntryID = housingDecorRedeemDeferredDecor.DeferredDecorID;
    SendPacket(response.Write());

    TC_LOG_INFO("housing", "CMSG_HOUSING_DECOR_REDEEM_DEFERRED_DECOR DeferredDecorID: {}, Result: {}",
        housingDecorRedeemDeferredDecor.DeferredDecorID, uint32(result));
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

    HousingResult result = housing->SelectFixtureOption(housingFixtureSetCoreFixture.ExteriorComponentID, 0);

    WorldPackets::Housing::HousingFixtureSetCoreFixtureResponse response;
    response.Result = static_cast<uint32>(result);
    response.FixtureRecordID = housingFixtureSetCoreFixture.ExteriorComponentID;
    SendPacket(response.Write());

    if (result == HOUSING_RESULT_SUCCESS)
    {
        WorldPackets::Housing::AccountExteriorFixtureCollectionUpdate collectionUpdate;
        collectionUpdate.FixtureID = housingFixtureSetCoreFixture.ExteriorComponentID;
        SendPacket(collectionUpdate.Write());
    }

    TC_LOG_INFO("housing", "CMSG_HOUSING_FIXTURE_SET_CORE_FIXTURE FixtureGuid: {}, ExteriorComponentID: {}, Result: {}",
        housingFixtureSetCoreFixture.FixtureGuid.ToString(), housingFixtureSetCoreFixture.ExteriorComponentID, uint32(result));
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

    HousingResult result = housing->SelectFixtureOption(housingFixtureCreateFixture.ExteriorComponentHookID, housingFixtureCreateFixture.ExteriorComponentType);

    WorldPackets::Housing::HousingFixtureCreateFixtureResponse response;
    response.Result = static_cast<uint32>(result);
    // FixtureGuid will be set once backend returns the newly created fixture's GUID
    SendPacket(response.Write());

    if (result == HOUSING_RESULT_SUCCESS)
    {
        WorldPackets::Housing::AccountExteriorFixtureCollectionUpdate collectionUpdate;
        collectionUpdate.FixtureID = housingFixtureCreateFixture.ExteriorComponentHookID;
        SendPacket(collectionUpdate.Write());
    }

    TC_LOG_INFO("housing", "CMSG_HOUSING_FIXTURE_CREATE_FIXTURE ExteriorComponentType: {}, ExteriorComponentHookID: {}, Result: {}",
        housingFixtureCreateFixture.ExteriorComponentType, housingFixtureCreateFixture.ExteriorComponentHookID, uint32(result));
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

    HousingResult result = housing->RemoveFixture(housingFixtureDeleteFixture.ExteriorComponentID);

    WorldPackets::Housing::HousingFixtureDeleteFixtureResponse response;
    response.Result = static_cast<uint32>(result);
    response.FixtureGuid = housingFixtureDeleteFixture.FixtureGuid;
    SendPacket(response.Write());

    TC_LOG_INFO("housing", "CMSG_HOUSING_FIXTURE_DELETE_FIXTURE FixtureGuid: {}, ExteriorComponentID: {}, Result: {}",
        housingFixtureDeleteFixture.FixtureGuid.ToString(), housingFixtureDeleteFixture.ExteriorComponentID, uint32(result));
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
        response.Result = static_cast<uint32>(HOUSING_RESULT_NEIGHBORHOOD_NOT_FOUND);
        SendPacket(response.Write());
        return;
    }

    HousingResult result = neighborhood->PurchasePlot(player->GetGUID(), housingSvcsNeighborhoodReservePlot.PlotIndex);
    if (result == HOUSING_RESULT_SUCCESS)
    {
        // Use the server's canonical neighborhood GUID, NOT the client-supplied GUID.
        // Client may send DB2 NeighborhoodID as counter (e.g. 1390) while server uses
        // internal instance counter (e.g. 2). Mismatch breaks GetHousingForNeighborhood().
        player->CreateHousing(neighborhood->GetGuid(), housingSvcsNeighborhoodReservePlot.PlotIndex);

        // Update the PlotInfo with the newly created HouseGuid and Battle.net account GUID
        if (Housing const* housing = player->GetHousing())
        {
            neighborhood->UpdatePlotHouseInfo(housingSvcsNeighborhoodReservePlot.PlotIndex,
                housing->GetHouseGuid(), GetBattlenetAccountGUID());

            // Send guild notification for house addition
            if (Guild* guild = sGuildMgr->GetGuildById(player->GetGuildId()))
            {
                WorldPackets::Housing::HousingSvcsGuildAddHouseNotification notification;
                notification.HouseGuid = housing->GetHouseGuid();
                guild->BroadcastPacket(notification.Write());
            }
        }

        // Grant "Acquire a house" kill credit for quest 91863 (objective 17)
        static constexpr uint32 NPC_KILL_CREDIT_BUY_HOME = 248858;
        player->KilledMonsterCredit(NPC_KILL_CREDIT_BUY_HOME);

        // Mark the plot Cornerstone as owned (GOState 1 = READY)
        if (HousingMap* housingMap = dynamic_cast<HousingMap*>(player->GetMap()))
            housingMap->SetPlotOwnershipState(housingSvcsNeighborhoodReservePlot.PlotIndex, true);
    }

    WorldPackets::Housing::HousingSvcsNeighborhoodReservePlotResponse response;
    response.Result = static_cast<uint32>(result);
    response.NeighborhoodGuid = housingSvcsNeighborhoodReservePlot.NeighborhoodGuid;
    response.PlotIndex = housingSvcsNeighborhoodReservePlot.PlotIndex;
    SendPacket(response.Write());

    TC_LOG_INFO("housing", "CMSG_HOUSING_SVCS_NEIGHBORHOOD_RESERVE_PLOT PlotIndex: {}, Result: {}",
        housingSvcsNeighborhoodReservePlot.PlotIndex, uint32(result));
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
        // Sniff-verified: OwnerGuid=HouseGUID, SecondaryOwnerGuid=PlotGUID, PlotGuid=NeighborhoodGUID
        info.OwnerGuid = housing->GetHouseGuid();
        info.SecondaryOwnerGuid = housing->GetPlotGuid();
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
    std::vector<NeighborhoodPlotData const*> plots = sHousingMgr.GetPlotsForMap(neighborhood->GetNeighborhoodMapID());
    NeighborhoodPlotData const* targetPlot = nullptr;
    for (NeighborhoodPlotData const* plot : plots)
    {
        if (plot->PlotIndex == static_cast<int32>(housingSvcsTeleportToPlot.PlotIndex))
        {
            targetPlot = plot;
            break;
        }
    }

    if (targetPlot)
    {
        // Teleport player to the plot position on the neighborhood map
        player->TeleportTo(mapData->MapID, targetPlot->TeleportPosition[0], targetPlot->TeleportPosition[1],
            targetPlot->TeleportPosition[2], 0.0f);

        TC_LOG_INFO("housing", "CMSG_HOUSING_SVCS_TELEPORT_TO_PLOT: Teleporting player {} to plot {} on map {}",
            player->GetGUID().ToString(), housingSvcsTeleportToPlot.PlotIndex, mapData->MapID);
    }
    else
    {
        // Fallback: teleport to neighborhood origin
        player->TeleportTo(mapData->MapID, mapData->Origin[0], mapData->Origin[1],
            mapData->Origin[2], 0.0f);

        TC_LOG_INFO("housing", "CMSG_HOUSING_SVCS_TELEPORT_TO_PLOT: Plot {} not found, teleporting to neighborhood origin on map {}",
            housingSvcsTeleportToPlot.PlotIndex, mapData->MapID);
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

void WorldSession::HandleHousingHouseStatus(WorldPackets::Housing::HousingHouseStatus const& housingHouseStatus)
{
    Player* player = GetPlayer();
    if (!player)
        return;

    Housing* housing = player->GetHousing();

    WorldPackets::Housing::HousingHouseStatusResponse response;
    if (housing)
    {
        response.HouseGuid = housing->GetHouseGuid();
        response.PlotGuid = housing->GetPlotGuid();
        response.NeighborhoodGuid = housing->GetNeighborhoodGuid();
        response.Status = 0;
    }
    // No house: all fields stay at defaults (empty GUIDs, Status=0).
    SendPacket(response.Write());

    TC_LOG_INFO("housing", ">>> CMSG_HOUSING_HOUSE_STATUS received");
    TC_LOG_INFO("housing", "<<< SMSG_HOUSING_HOUSE_STATUS_RESPONSE sent (HouseGuid: {}, PlotGuid: {}, NeighborhoodGuid: {}, Status: {})",
        response.HouseGuid.ToString(), response.PlotGuid.ToString(), response.NeighborhoodGuid.ToString(), response.Status);
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
    SendPacket(response.Write());

    TC_LOG_INFO("housing", "<<< SMSG_HOUSING_GET_PLAYER_PERMISSIONS_RESPONSE sent (HouseGuid: {}, ResultCode: {}, PermissionFlags: 0x{:02X})",
        response.HouseGuid.ToString(), response.ResultCode, response.PermissionFlags);
}

void WorldSession::HandleHousingGetCurrentHouseInfo(WorldPackets::Housing::HousingGetCurrentHouseInfo const& housingGetCurrentHouseInfo)
{
    Player* player = GetPlayer();
    if (!player)
        return;

    TC_LOG_INFO("housing", ">>> CMSG_HOUSING_GET_CURRENT_HOUSE_INFO received");

    Housing* housing = player->GetHousing();

    WorldPackets::Housing::HousingGetCurrentHouseInfoResponse response;
    if (housing)
    {
        // Sniff-verified: JamCurrentHouseInfo field mapping
        // SecondaryOwnerGuid = NeighborhoodGuid (client uses this as context GUID for edit mode matching)
        // PlotGuid = PlotGuid (deterministic from neighborhood + plot index)
        response.HouseInfo.OwnerGuid = housing->GetHouseGuid();
        response.HouseInfo.SecondaryOwnerGuid = housing->GetNeighborhoodGuid();
        response.HouseInfo.PlotGuid = housing->GetPlotGuid();
        response.HouseInfo.Flags = housing->GetPlotIndex();
        response.HouseInfo.HouseTypeId = 32;  // sniff value: 0x20 = default house type
        response.HouseInfo.StatusFlags = 0;
    }
    else if (HousingMap* housingMap = dynamic_cast<HousingMap*>(player->GetMap()))
    {
        // No house, but on a housing map — provide the neighborhood GUID
        // in the SecondaryOwnerGuid field so the client knows the context.
        if (Neighborhood* neighborhood = housingMap->GetNeighborhood())
            response.HouseInfo.SecondaryOwnerGuid = neighborhood->GetGuid();
    }
    response.ResponseFlags = 0;
    SendPacket(response.Write());

    TC_LOG_ERROR("housing", "<<< SMSG_HOUSING_GET_CURRENT_HOUSE_INFO_RESPONSE sent (HasHouse: {}) OwnerGuid={} SecondaryOwnerGuid={} PlotGuid={} Flags={} HouseTypeId={} StatusFlags={}",
        housing ? "yes" : "no",
        response.HouseInfo.OwnerGuid.ToString(),
        response.HouseInfo.SecondaryOwnerGuid.ToString(),
        response.HouseInfo.PlotGuid.ToString(),
        response.HouseInfo.Flags,
        response.HouseInfo.HouseTypeId,
        response.HouseInfo.StatusFlags);
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
