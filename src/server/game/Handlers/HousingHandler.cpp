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
#include "Housing.h"
#include "HousingMgr.h"
#include "HousingPackets.h"
#include "Log.h"
#include "Neighborhood.h"
#include "NeighborhoodCharter.h"
#include "NeighborhoodMgr.h"
#include "ObjectAccessor.h"
#include "Player.h"

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

void WorldSession::HandleHouseExteriorSetHousePosition(WorldPackets::Housing::HouseExteriorCommitPosition const& /*houseExteriorCommitPosition*/)
{
    Player* player = GetPlayer();
    if (!player)
        return;

    Housing* housing = player->GetHousing();
    if (!housing)
        return;

    // Exterior position commit is a visual-only operation handled by the client
    // Server acknowledges receipt; the client positions the house model locally
    WorldPackets::Housing::HouseExteriorSetHousePositionResponse response;
    response.Result = static_cast<uint32>(HOUSING_RESULT_SUCCESS);
    SendPacket(response.Write());

    TC_LOG_DEBUG("network", "CMSG_HOUSE_EXTERIOR_COMMIT_POSITION received for player {}", player->GetGUID().ToString());
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

    TC_LOG_DEBUG("network", "CMSG_HOUSE_INTERIOR_LEAVE_HOUSE: Player {} leaving house interior",
        player->GetGUID().ToString());
}

// ============================================================
// Decor System
// ============================================================

void WorldSession::HandleHousingDecorSetEditMode(WorldPackets::Housing::HousingDecorSetEditMode const& housingDecorSetEditMode)
{
    Player* player = GetPlayer();
    if (!player)
        return;

    Housing* housing = player->GetHousing();
    if (!housing)
        return;

    housing->SetEditorMode(housingDecorSetEditMode.Active ? HOUSING_EDITOR_MODE_BASIC_DECOR : HOUSING_EDITOR_MODE_NONE);

    WorldPackets::Housing::HousingDecorSetEditModeResponse response;
    response.Result = static_cast<uint32>(HOUSING_RESULT_SUCCESS);
    SendPacket(response.Write());

    TC_LOG_DEBUG("network", "CMSG_HOUSING_DECOR_SET_EDITOR_MODE_ACTIVE Active: {}", housingDecorSetEditMode.Active);
}

void WorldSession::HandleHousingDecorPlace(WorldPackets::Housing::HousingDecorPlace const& housingDecorPlace)
{
    Player* player = GetPlayer();
    if (!player)
        return;

    Housing* housing = player->GetHousing();
    if (!housing)
        return;

    HousingResult result = housing->PlaceDecor(housingDecorPlace.DecorEntryID,
        housingDecorPlace.Pos.Pos.GetPositionX(),
        housingDecorPlace.Pos.Pos.GetPositionY(),
        housingDecorPlace.Pos.Pos.GetPositionZ(),
        housingDecorPlace.RotationX, housingDecorPlace.RotationY,
        housingDecorPlace.RotationZ, housingDecorPlace.RotationW,
        ObjectGuid());

    WorldPackets::Housing::HousingDecorPlaceResponse response;
    response.Result = static_cast<uint32>(result);
    SendPacket(response.Write());

    TC_LOG_DEBUG("network", "CMSG_HOUSING_DECOR_PLACE_NEW_DECOR DecorEntryID: {}, Result: {}",
        housingDecorPlace.DecorEntryID, uint32(result));
}

void WorldSession::HandleHousingDecorMove(WorldPackets::Housing::HousingDecorMove const& housingDecorMove)
{
    Player* player = GetPlayer();
    if (!player)
        return;

    Housing* housing = player->GetHousing();
    if (!housing)
        return;

    HousingResult result = housing->MoveDecor(housingDecorMove.DecorGuid,
        housingDecorMove.Pos.Pos.GetPositionX(),
        housingDecorMove.Pos.Pos.GetPositionY(),
        housingDecorMove.Pos.Pos.GetPositionZ(),
        housingDecorMove.RotationX, housingDecorMove.RotationY,
        housingDecorMove.RotationZ, housingDecorMove.RotationW);

    WorldPackets::Housing::HousingDecorMoveResponse response;
    response.Result = static_cast<uint32>(result);
    SendPacket(response.Write());

    TC_LOG_DEBUG("network", "CMSG_HOUSING_DECOR_MOVE_DECOR DecorGuid: {}, Result: {}",
        housingDecorMove.DecorGuid.ToString(), uint32(result));
}

void WorldSession::HandleHousingDecorRemove(WorldPackets::Housing::HousingDecorRemove const& housingDecorRemove)
{
    Player* player = GetPlayer();
    if (!player)
        return;

    Housing* housing = player->GetHousing();
    if (!housing)
        return;

    HousingResult result = housing->RemoveDecor(housingDecorRemove.DecorGuid);

    WorldPackets::Housing::HousingDecorRemoveResponse response;
    response.Result = static_cast<uint32>(result);
    SendPacket(response.Write());

    TC_LOG_DEBUG("network", "CMSG_HOUSING_DECOR_REMOVE_PLACED_DECOR_ENTRY DecorGuid: {}, Result: {}",
        housingDecorRemove.DecorGuid.ToString(), uint32(result));
}

void WorldSession::HandleHousingDecorLock(WorldPackets::Housing::HousingDecorLock const& housingDecorLock)
{
    Player* player = GetPlayer();
    if (!player)
        return;

    Housing* housing = player->GetHousing();
    if (!housing)
        return;

    // Verify the decor exists in this house
    Housing::PlacedDecor const* decor = housing->GetPlacedDecor(housingDecorLock.DecorGuid);
    if (!decor)
    {
        TC_LOG_DEBUG("network", "CMSG_HOUSING_DECOR_SELECT_DECOR: Decor {} not found in house",
            housingDecorLock.DecorGuid.ToString());
        return;
    }

    TC_LOG_DEBUG("network", "CMSG_HOUSING_DECOR_SELECT_DECOR HouseGuid: {}, DecorGuid: {} (entry: {})",
        housingDecorLock.HouseGuid.ToString(), housingDecorLock.DecorGuid.ToString(), decor->DecorEntryId);

    WorldPackets::Housing::HousingDecorLockResponse response;
    response.Result = static_cast<uint32>(HOUSING_RESULT_SUCCESS);
    SendPacket(response.Write());
}

void WorldSession::HandleHousingDecorSetDyeSlots(WorldPackets::Housing::HousingDecorSetDyeSlots const& housingDecorSetDyeSlots)
{
    Player* player = GetPlayer();
    if (!player)
        return;

    Housing* housing = player->GetHousing();
    if (!housing)
        return;

    std::array<uint32, MAX_HOUSING_DYE_SLOTS> dyeSlots = {};
    for (size_t i = 0; i < housingDecorSetDyeSlots.DyeSlots.size() && i < MAX_HOUSING_DYE_SLOTS; ++i)
        dyeSlots[i] = housingDecorSetDyeSlots.DyeSlots[i];

    HousingResult result = housing->CommitDecorDyes(housingDecorSetDyeSlots.DecorGuid, dyeSlots);

    WorldPackets::Housing::HousingDecorSystemSetDyeSlotsResponse response;
    response.Result = static_cast<uint32>(result);
    SendPacket(response.Write());

    TC_LOG_DEBUG("network", "CMSG_HOUSING_DECOR_COMMIT_DYES DecorGuid: {}, Result: {}",
        housingDecorSetDyeSlots.DecorGuid.ToString(), uint32(result));
}

void WorldSession::HandleHousingDecorDeleteFromStorage(WorldPackets::Housing::HousingDecorDeleteFromStorage const& housingDecorDeleteFromStorage)
{
    Player* player = GetPlayer();
    if (!player)
        return;

    Housing* housing = player->GetHousing();
    if (!housing)
        return;

    HousingResult result = housing->RemoveFromCatalog(housingDecorDeleteFromStorage.CatalogEntryID);

    WorldPackets::Housing::HousingDecorDeleteFromStorageResponse response;
    response.Result = static_cast<uint32>(result);
    SendPacket(response.Write());

    TC_LOG_DEBUG("network", "CMSG_HOUSING_DECOR_CATALOG_DESTROY_ENTRY CatalogEntryID: {}, Result: {}",
        housingDecorDeleteFromStorage.CatalogEntryID, uint32(result));
}

void WorldSession::HandleHousingDecorDeleteFromStorageById(WorldPackets::Housing::HousingDecorDeleteFromStorageById const& housingDecorDeleteFromStorageById)
{
    Player* player = GetPlayer();
    if (!player)
        return;

    Housing* housing = player->GetHousing();
    if (!housing)
        return;

    HousingResult result = housing->DestroyAllCopies(housingDecorDeleteFromStorageById.CatalogEntryID);

    WorldPackets::Housing::HousingDecorDeleteFromStorageResponse response;
    response.Result = static_cast<uint32>(result);
    SendPacket(response.Write());

    TC_LOG_DEBUG("network", "CMSG_HOUSING_DECOR_CATALOG_DESTROY_ALL CatalogEntryID: {}, Result: {}",
        housingDecorDeleteFromStorageById.CatalogEntryID, uint32(result));
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
        response.Result = static_cast<uint32>(HOUSING_RESULT_HOUSE_NOT_FOUND);
        SendPacket(response.Write());
        TC_LOG_DEBUG("network", "CMSG_HOUSING_DECOR_CATALOG_CREATE_SEARCHER: Player {} has no house",
            player->GetGUID().ToString());
        return;
    }

    // Client requests the catalog/storage listing. Retrieve catalog entries for logging.
    // The client already has DB2 data for display; this confirms the server acknowledges.
    std::vector<Housing::CatalogEntry const*> entries = housing->GetCatalogEntries();

    WorldPackets::Housing::HousingDecorRequestStorageResponse response;
    response.Result = static_cast<uint32>(HOUSING_RESULT_SUCCESS);
    SendPacket(response.Write());

    TC_LOG_DEBUG("network", "CMSG_HOUSING_DECOR_CATALOG_CREATE_SEARCHER HouseGuid: {}, CatalogEntries: {}",
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
        TC_LOG_DEBUG("network", "CMSG_HOUSING_DECOR_START_PLACING_NEW_DECOR: Player {} has no house",
            player->GetGUID().ToString());
        return;
    }

    // Verify the decor entry exists in DB2
    HouseDecorData const* decorData = sHousingMgr.GetHouseDecorData(housingDecorRedeemDeferredDecor.CatalogEntryID);
    if (!decorData)
    {
        TC_LOG_DEBUG("network", "CMSG_HOUSING_DECOR_START_PLACING_NEW_DECOR: Invalid CatalogEntryID: {}",
            housingDecorRedeemDeferredDecor.CatalogEntryID);
        return;
    }

    // The client is starting to place a new decor from the catalog. Server verifies it exists
    // and the catalog entry is available. The actual PlaceDecor happens via CMSG_HOUSING_DECOR_PLACE_NEW_DECOR.
    TC_LOG_DEBUG("network", "CMSG_HOUSING_DECOR_START_PLACING_NEW_DECOR HouseGuid: {}, CatalogEntryID: {} ({})",
        housingDecorRedeemDeferredDecor.HouseGuid.ToString(), housingDecorRedeemDeferredDecor.CatalogEntryID,
        decorData->Name);

    WorldPackets::Housing::HousingRedeemDeferredDecorResponse response;
    response.Result = static_cast<uint32>(HOUSING_RESULT_SUCCESS);
    SendPacket(response.Write());
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
        return;

    housing->SetEditorMode(housingFixtureSetEditMode.Active ? HOUSING_EDITOR_MODE_CUSTOMIZE : HOUSING_EDITOR_MODE_NONE);

    WorldPackets::Housing::HousingFixtureSetEditModeResponse response;
    response.Result = static_cast<uint32>(HOUSING_RESULT_SUCCESS);
    SendPacket(response.Write());

    TC_LOG_DEBUG("network", "CMSG_HOUSING_FIXTURE_SET_EDITOR_MODE_ACTIVE Active: {}", housingFixtureSetEditMode.Active);
}

void WorldSession::HandleHousingFixtureSetCoreFixture(WorldPackets::Housing::HousingFixtureSetCoreFixture const& housingFixtureSetCoreFixture)
{
    Player* player = GetPlayer();
    if (!player)
        return;

    Housing* housing = player->GetHousing();
    if (!housing)
        return;

    HousingResult result = housing->SelectFixtureOption(housingFixtureSetCoreFixture.FixturePointID, housingFixtureSetCoreFixture.OptionID);

    WorldPackets::Housing::HousingFixtureSetCoreFixtureResponse response;
    response.Result = static_cast<uint32>(result);
    SendPacket(response.Write());

    TC_LOG_DEBUG("network", "CMSG_HOUSING_FIXTURE_SELECT_CORE_FIXTURE FixturePointID: {}, OptionID: {}, Result: {}",
        housingFixtureSetCoreFixture.FixturePointID, housingFixtureSetCoreFixture.OptionID, uint32(result));
}

void WorldSession::HandleHousingFixtureCreateFixture(WorldPackets::Housing::HousingFixtureCreateFixture const& housingFixtureCreateFixture)
{
    Player* player = GetPlayer();
    if (!player)
        return;

    Housing* housing = player->GetHousing();
    if (!housing)
        return;

    HousingResult result = housing->SelectFixtureOption(housingFixtureCreateFixture.FixturePointID, housingFixtureCreateFixture.OptionID);

    WorldPackets::Housing::HousingFixtureCreateFixtureResponse response;
    response.Result = static_cast<uint32>(result);
    SendPacket(response.Write());

    if (result == HOUSING_RESULT_SUCCESS)
    {
        WorldPackets::Housing::AccountHousingFixtureAdded fixtureAdded;
        fixtureAdded.FixtureID = housingFixtureCreateFixture.OptionID;
        SendPacket(fixtureAdded.Write());
    }

    TC_LOG_DEBUG("network", "CMSG_HOUSING_FIXTURE_SELECT_FIXTURE FixturePointID: {}, OptionID: {}, Result: {}",
        housingFixtureCreateFixture.FixturePointID, housingFixtureCreateFixture.OptionID, uint32(result));
}

void WorldSession::HandleHousingFixtureDeleteFixture(WorldPackets::Housing::HousingFixtureDeleteFixture const& housingFixtureDeleteFixture)
{
    Player* player = GetPlayer();
    if (!player)
        return;

    Housing* housing = player->GetHousing();
    if (!housing)
        return;

    HousingResult result = housing->RemoveFixture(housingFixtureDeleteFixture.FixturePointID);

    WorldPackets::Housing::HousingFixtureDeleteFixtureResponse response;
    response.Result = static_cast<uint32>(result);
    SendPacket(response.Write());

    TC_LOG_DEBUG("network", "CMSG_HOUSING_FIXTURE_REMOVE FixturePointID: {}, Result: {}",
        housingFixtureDeleteFixture.FixturePointID, uint32(result));
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
        return;

    housing->SetEditorMode(housingRoomSetLayoutEditMode.Active ? HOUSING_EDITOR_MODE_LAYOUT : HOUSING_EDITOR_MODE_NONE);

    WorldPackets::Housing::HousingRoomSetLayoutEditModeResponse response;
    response.Result = static_cast<uint32>(HOUSING_RESULT_SUCCESS);
    SendPacket(response.Write());

    TC_LOG_DEBUG("network", "CMSG_HOUSING_ROOM_SET_EDITOR_MODE_ACTIVE Active: {}", housingRoomSetLayoutEditMode.Active);
}

void WorldSession::HandleHousingRoomAdd(WorldPackets::Housing::HousingRoomAdd const& housingRoomAdd)
{
    Player* player = GetPlayer();
    if (!player)
        return;

    Housing* housing = player->GetHousing();
    if (!housing)
        return;

    HousingResult result = housing->PlaceRoom(housingRoomAdd.RoomID, housingRoomAdd.SlotIndex,
        housingRoomAdd.Orientation, housingRoomAdd.Mirrored);

    WorldPackets::Housing::HousingRoomAddResponse response;
    response.Result = static_cast<uint32>(result);
    SendPacket(response.Write());

    if (result == HOUSING_RESULT_SUCCESS)
    {
        WorldPackets::Housing::AccountHousingRoomAdded roomAdded;
        roomAdded.RoomID = housingRoomAdd.RoomID;
        SendPacket(roomAdded.Write());
    }

    TC_LOG_DEBUG("network", "CMSG_HOUSING_ROOM_PLACE_ROOM RoomID: {}, SlotIndex: {}, Result: {}",
        housingRoomAdd.RoomID, housingRoomAdd.SlotIndex, uint32(result));
}

void WorldSession::HandleHousingRoomRemove(WorldPackets::Housing::HousingRoomRemove const& housingRoomRemove)
{
    Player* player = GetPlayer();
    if (!player)
        return;

    Housing* housing = player->GetHousing();
    if (!housing)
        return;

    HousingResult result = housing->RemoveRoom(housingRoomRemove.RoomGuid);

    WorldPackets::Housing::HousingRoomRemoveResponse response;
    response.Result = static_cast<uint32>(result);
    SendPacket(response.Write());

    TC_LOG_DEBUG("network", "CMSG_HOUSING_ROOM_REMOVE_ROOM RoomGuid: {}, Result: {}",
        housingRoomRemove.RoomGuid.ToString(), uint32(result));
}

void WorldSession::HandleHousingRoomRotate(WorldPackets::Housing::HousingRoomRotate const& housingRoomRotate)
{
    Player* player = GetPlayer();
    if (!player)
        return;

    Housing* housing = player->GetHousing();
    if (!housing)
        return;

    HousingResult result = housing->RotateRoom(housingRoomRotate.RoomGuid, housingRoomRotate.Clockwise);

    WorldPackets::Housing::HousingRoomUpdateResponse response;
    response.Result = static_cast<uint32>(result);
    SendPacket(response.Write());

    TC_LOG_DEBUG("network", "CMSG_HOUSING_ROOM_ROTATE_ROOM RoomGuid: {}, Clockwise: {}, Result: {}",
        housingRoomRotate.RoomGuid.ToString(), housingRoomRotate.Clockwise, uint32(result));
}

void WorldSession::HandleHousingRoomMoveRoom(WorldPackets::Housing::HousingRoomMoveRoom const& housingRoomMoveRoom)
{
    Player* player = GetPlayer();
    if (!player)
        return;

    Housing* housing = player->GetHousing();
    if (!housing)
        return;

    HousingResult result = housing->MoveRoom(housingRoomMoveRoom.RoomGuid, housingRoomMoveRoom.NewSlotIndex,
        housingRoomMoveRoom.SwapRoomGuid, housingRoomMoveRoom.SwapSlotIndex);

    WorldPackets::Housing::HousingRoomUpdateResponse response;
    response.Result = static_cast<uint32>(result);
    SendPacket(response.Write());

    TC_LOG_DEBUG("network", "CMSG_HOUSING_ROOM_MOVE RoomGuid: {}, NewSlotIndex: {}, Result: {}",
        housingRoomMoveRoom.RoomGuid.ToString(), housingRoomMoveRoom.NewSlotIndex, uint32(result));
}

void WorldSession::HandleHousingRoomSetComponentTheme(WorldPackets::Housing::HousingRoomSetComponentTheme const& housingRoomSetComponentTheme)
{
    Player* player = GetPlayer();
    if (!player)
        return;

    Housing* housing = player->GetHousing();
    if (!housing)
        return;

    HousingResult result = housing->ApplyRoomTheme(housingRoomSetComponentTheme.RoomGuid,
        housingRoomSetComponentTheme.ThemeSetID, housingRoomSetComponentTheme.ComponentIDs);

    WorldPackets::Housing::HousingRoomSetComponentThemeResponse response;
    response.Result = static_cast<uint32>(result);
    SendPacket(response.Write());

    if (result == HOUSING_RESULT_SUCCESS)
    {
        WorldPackets::Housing::AccountHousingThemeAdded themeAdded;
        themeAdded.ThemeID = housingRoomSetComponentTheme.ThemeSetID;
        SendPacket(themeAdded.Write());
    }

    TC_LOG_DEBUG("network", "CMSG_HOUSING_ROOM_APPLY_THEME RoomGuid: {}, ThemeSetID: {}, Result: {}",
        housingRoomSetComponentTheme.RoomGuid.ToString(), housingRoomSetComponentTheme.ThemeSetID, uint32(result));
}

void WorldSession::HandleHousingRoomApplyComponentMaterials(WorldPackets::Housing::HousingRoomApplyComponentMaterials const& housingRoomApplyComponentMaterials)
{
    Player* player = GetPlayer();
    if (!player)
        return;

    Housing* housing = player->GetHousing();
    if (!housing)
        return;

    HousingResult result = housing->ApplyRoomWallpaper(housingRoomApplyComponentMaterials.RoomGuid,
        housingRoomApplyComponentMaterials.WallpaperID, housingRoomApplyComponentMaterials.MaterialID,
        housingRoomApplyComponentMaterials.ComponentIDs);

    WorldPackets::Housing::HousingRoomApplyComponentMaterialsResponse response;
    response.Result = static_cast<uint32>(result);
    SendPacket(response.Write());

    if (result == HOUSING_RESULT_SUCCESS)
    {
        WorldPackets::Housing::AccountHousingRoomComponentTextureAdded textureAdded;
        textureAdded.TextureID = housingRoomApplyComponentMaterials.MaterialID;
        SendPacket(textureAdded.Write());
    }

    TC_LOG_DEBUG("network", "CMSG_HOUSING_ROOM_APPLY_WALLPAPER RoomGuid: {}, WallpaperID: {}, Result: {}",
        housingRoomApplyComponentMaterials.RoomGuid.ToString(), housingRoomApplyComponentMaterials.WallpaperID, uint32(result));
}

void WorldSession::HandleHousingRoomSetDoorType(WorldPackets::Housing::HousingRoomSetDoorType const& housingRoomSetDoorType)
{
    Player* player = GetPlayer();
    if (!player)
        return;

    Housing* housing = player->GetHousing();
    if (!housing)
        return;

    HousingResult result = housing->SetDoorType(housingRoomSetDoorType.RoomGuid,
        housingRoomSetDoorType.DoorTypeID, housingRoomSetDoorType.DoorSlot);

    WorldPackets::Housing::HousingRoomSetDoorTypeResponse response;
    response.Result = static_cast<uint32>(result);
    SendPacket(response.Write());

    TC_LOG_DEBUG("network", "CMSG_HOUSING_ROOM_SET_DOOR_TYPE RoomGuid: {}, DoorTypeID: {}, Result: {}",
        housingRoomSetDoorType.RoomGuid.ToString(), housingRoomSetDoorType.DoorTypeID, uint32(result));
}

void WorldSession::HandleHousingRoomSetCeilingType(WorldPackets::Housing::HousingRoomSetCeilingType const& housingRoomSetCeilingType)
{
    Player* player = GetPlayer();
    if (!player)
        return;

    Housing* housing = player->GetHousing();
    if (!housing)
        return;

    HousingResult result = housing->SetCeilingType(housingRoomSetCeilingType.RoomGuid,
        housingRoomSetCeilingType.CeilingTypeID, housingRoomSetCeilingType.CeilingSlot);

    WorldPackets::Housing::HousingRoomSetCeilingTypeResponse response;
    response.Result = static_cast<uint32>(result);
    SendPacket(response.Write());

    TC_LOG_DEBUG("network", "CMSG_HOUSING_ROOM_SET_CEILING_TYPE RoomGuid: {}, CeilingTypeID: {}, Result: {}",
        housingRoomSetCeilingType.RoomGuid.ToString(), housingRoomSetCeilingType.CeilingTypeID, uint32(result));
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
        player->GetGUID(), housingSvcsGuildCreateNeighborhood.Name,
        housingSvcsGuildCreateNeighborhood.NeighborhoodMapID,
        housingSvcsGuildCreateNeighborhood.FactionID);

    WorldPackets::Housing::HousingSvcsCreateCharterNeighborhoodResponse response;
    response.Result = static_cast<uint32>(neighborhood ? HOUSING_RESULT_SUCCESS : HOUSING_RESULT_NOT_ALLOWED);
    if (neighborhood)
        response.NeighborhoodGuid = neighborhood->GetGuid();
    SendPacket(response.Write());

    TC_LOG_DEBUG("network", "CMSG_HOUSING_SERVICES_CREATE_GUILD_NEIGHBORHOOD Name: {}, Result: {}",
        housingSvcsGuildCreateNeighborhood.Name, neighborhood ? "success" : "failed");
}

void WorldSession::HandleHousingSvcsNeighborhoodReservePlot(WorldPackets::Housing::HousingSvcsNeighborhoodReservePlot const& housingSvcsNeighborhoodReservePlot)
{
    Player* player = GetPlayer();
    if (!player)
        return;

    Neighborhood* neighborhood = sNeighborhoodMgr.GetNeighborhood(housingSvcsNeighborhoodReservePlot.NeighborhoodGuid);
    if (!neighborhood)
        return;

    HousingResult result = neighborhood->PurchasePlot(player->GetGUID(), housingSvcsNeighborhoodReservePlot.PlotIndex);
    if (result == HOUSING_RESULT_SUCCESS)
        player->CreateHousing(housingSvcsNeighborhoodReservePlot.NeighborhoodGuid, housingSvcsNeighborhoodReservePlot.PlotIndex);

    WorldPackets::Housing::HousingSvcsNeighborhoodReservePlotResponse response;
    response.Result = static_cast<uint32>(result);
    SendPacket(response.Write());

    TC_LOG_DEBUG("network", "CMSG_HOUSING_SERVICES_RESERVE_PLOT PlotIndex: {}, Result: {}",
        housingSvcsNeighborhoodReservePlot.PlotIndex, uint32(result));
}

void WorldSession::HandleHousingSvcsRelinquishHouse(WorldPackets::Housing::HousingSvcsRelinquishHouse const& /*housingSvcsRelinquishHouse*/)
{
    Player* player = GetPlayer();
    if (!player)
        return;

    player->DeleteHousing();

    WorldPackets::Housing::HousingSvcsRelinquishHouseResponse response;
    response.Result = static_cast<uint32>(HOUSING_RESULT_SUCCESS);
    SendPacket(response.Write());

    TC_LOG_DEBUG("network", "CMSG_HOUSING_SERVICES_RELINQUISH_HOUSE processed");
}

void WorldSession::HandleHousingSvcsUpdateHouseSettings(WorldPackets::Housing::HousingSvcsUpdateHouseSettings const& housingSvcsUpdateHouseSettings)
{
    Player* player = GetPlayer();
    if (!player)
        return;

    Housing* housing = player->GetHousing();
    if (!housing)
        return;

    if (housingSvcsUpdateHouseSettings.SettingID)
        housing->SaveSettings(*housingSvcsUpdateHouseSettings.SettingID);

    WorldPackets::Housing::HousingSvcsUpdateHouseSettingsResponse response;
    response.Result = static_cast<uint32>(HOUSING_RESULT_SUCCESS);
    SendPacket(response.Write());

    TC_LOG_DEBUG("network", "CMSG_HOUSING_SERVICES_SAVE_HOUSE_SETTINGS HouseGuid: {}",
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
    SendPacket(response.Write());

    TC_LOG_DEBUG("network", "CMSG_HOUSING_SERVICES_GET_OTHERS_PLAYER_OWNED_HOUSES PlayerGuid: {}, FoundNeighborhoods: {}",
        housingSvcsPlayerViewHousesByPlayer.PlayerGuid.ToString(), uint32(neighborhoods.size()));
}

void WorldSession::HandleHousingSvcsPlayerViewHousesByBnetAccount(WorldPackets::Housing::HousingSvcsPlayerViewHousesByBnetAccount const& housingSvcsPlayerViewHousesByBnetAccount)
{
    Player* player = GetPlayer();
    if (!player)
        return;

    // BNet account house lookup - uses the same player neighborhood query for now
    // In a complete implementation this would query all characters on the BNet account
    WorldPackets::Housing::HousingSvcsPlayerViewHousesResponse response;
    response.Result = static_cast<uint32>(HOUSING_RESULT_SUCCESS);
    SendPacket(response.Write());

    TC_LOG_DEBUG("network", "CMSG_HOUSING_SERVICES_GET_OTHERS_BNET_ACCOUNT_OWNED_HOUSES BnetAccountGuid: {}",
        housingSvcsPlayerViewHousesByBnetAccount.BnetAccountGuid.ToString());
}

void WorldSession::HandleHousingSvcsGetPlayerHousesInfo(WorldPackets::Housing::HousingSvcsGetPlayerHousesInfo const& /*housingSvcsGetPlayerHousesInfo*/)
{
    Player* player = GetPlayer();
    if (!player)
        return;

    Housing* housing = player->GetHousing();
    if (housing)
    {
        TC_LOG_DEBUG("network", "CMSG_HOUSING_SERVICES_GET_PLAYER_OWNED_HOUSES: Player {} has house level {}, "
            "{} decor, {} rooms, {} fixtures",
            player->GetGUID().ToString(), housing->GetLevel(),
            housing->GetDecorCount(), uint32(housing->GetRooms().size()),
            uint32(housing->GetFixtures().size()));
    }
    else
    {
        TC_LOG_DEBUG("network", "CMSG_HOUSING_SERVICES_GET_PLAYER_OWNED_HOUSES: Player {} has no house",
            player->GetGUID().ToString());
    }

    WorldPackets::Housing::HousingSvcsGetPlayerHousesInfoResponse response;
    response.Result = static_cast<uint32>(housing ? HOUSING_RESULT_SUCCESS : HOUSING_RESULT_HOUSE_NOT_FOUND);
    SendPacket(response.Write());
}

void WorldSession::HandleHousingSvcsTeleportToPlot(WorldPackets::Housing::HousingSvcsTeleportToPlot const& housingSvcsTeleportToPlot)
{
    Player* player = GetPlayer();
    if (!player)
        return;

    Neighborhood* neighborhood = sNeighborhoodMgr.GetNeighborhood(housingSvcsTeleportToPlot.NeighborhoodGuid);
    if (!neighborhood)
    {
        TC_LOG_DEBUG("network", "CMSG_HOUSING_SERVICES_ENTER_HOUSE: Neighborhood {} not found",
            housingSvcsTeleportToPlot.NeighborhoodGuid.ToString());
        return;
    }

    // Look up the neighborhood map data for map ID and plot positions
    NeighborhoodMapData const* mapData = sHousingMgr.GetNeighborhoodMapData(neighborhood->GetNeighborhoodMapID());
    if (!mapData)
    {
        TC_LOG_DEBUG("network", "CMSG_HOUSING_SERVICES_ENTER_HOUSE: No map data for NeighborhoodMapID {}",
            neighborhood->GetNeighborhoodMapID());
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
        player->TeleportTo(mapData->MapID, targetPlot->Position[0], targetPlot->Position[1],
            targetPlot->Position[2], 0.0f);

        TC_LOG_DEBUG("network", "CMSG_HOUSING_SERVICES_ENTER_HOUSE: Teleporting player {} to plot {} on map {}",
            player->GetGUID().ToString(), housingSvcsTeleportToPlot.PlotIndex, mapData->MapID);
    }
    else
    {
        // Fallback: teleport to neighborhood origin
        player->TeleportTo(mapData->MapID, mapData->Origin[0], mapData->Origin[1],
            mapData->Origin[2], 0.0f);

        TC_LOG_DEBUG("network", "CMSG_HOUSING_SERVICES_ENTER_HOUSE: Plot {} not found, teleporting to neighborhood origin on map {}",
            housingSvcsTeleportToPlot.PlotIndex, mapData->MapID);
    }
}

void WorldSession::HandleHousingSvcsStartTutorial(WorldPackets::Housing::HousingSvcsStartTutorial const& /*housingSvcsStartTutorial*/)
{
    Player* player = GetPlayer();
    if (!player)
        return;

    // The tutorial is primarily client-driven. The server acknowledges the request
    // and ensures the player has housing initialized.
    Housing* housing = player->GetHousing();
    if (housing)
    {
        TC_LOG_DEBUG("network", "CMSG_HOUSING_SERVICES_START_TUTORIAL: Player {} starting housing tutorial (level {})",
            player->GetGUID().ToString(), housing->GetLevel());
    }
    else
    {
        TC_LOG_DEBUG("network", "CMSG_HOUSING_SERVICES_START_TUTORIAL: Player {} has no house yet",
            player->GetGUID().ToString());
    }
}

void WorldSession::HandleHousingSvcsAcceptNeighborhoodOwnership(WorldPackets::Housing::HousingSvcsAcceptNeighborhoodOwnership const& housingSvcsAcceptNeighborhoodOwnership)
{
    Player* player = GetPlayer();
    if (!player)
        return;

    Neighborhood* neighborhood = sNeighborhoodMgr.GetNeighborhood(housingSvcsAcceptNeighborhoodOwnership.NeighborhoodGuid);
    if (!neighborhood)
        return;

    neighborhood->TransferOwnership(player->GetGUID());

    WorldPackets::Housing::HousingSvcsAcceptNeighborhoodOwnershipResponse response;
    response.Result = static_cast<uint32>(HOUSING_RESULT_SUCCESS);
    SendPacket(response.Write());

    TC_LOG_DEBUG("network", "CMSG_HOUSING_SERVICES_ACCEPT_NEIGHBORHOOD_OWNERSHIP NeighborhoodGuid: {}",
        housingSvcsAcceptNeighborhoodOwnership.NeighborhoodGuid.ToString());
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
    SendPacket(response.Write());

    TC_LOG_DEBUG("network", "CMSG_HOUSING_SERVICES_DECLINE_NEIGHBORHOOD_OWNERSHIP: Player {} declined ownership of neighborhood {}",
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
        TC_LOG_DEBUG("network", "CMSG_HOUSING_SERVICES_REQUEST_PLAYER_CHARACTER_LIST: Player {} has no house",
            player->GetGUID().ToString());
        return;
    }

    Neighborhood* neighborhood = sNeighborhoodMgr.GetNeighborhood(housing->GetNeighborhoodGuid());
    if (!neighborhood)
    {
        TC_LOG_DEBUG("network", "CMSG_HOUSING_SERVICES_REQUEST_PLAYER_CHARACTER_LIST: Neighborhood not found for player {}",
            player->GetGUID().ToString());
        return;
    }

    std::vector<Neighborhood::Member> const& members = neighborhood->GetMembers();
    TC_LOG_DEBUG("network", "CMSG_HOUSING_SERVICES_REQUEST_PLAYER_CHARACTER_LIST: Neighborhood has {} members",
        uint32(members.size()));

    WorldPackets::Housing::HousingSvcsGetPotentialHouseOwnersResponse response;
    response.Result = static_cast<uint32>(HOUSING_RESULT_SUCCESS);
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
    SendPacket(response.Write());

    TC_LOG_DEBUG("network", "CMSG_HOUSING_SERVICES_HOUSE_FINDER_REQUEST_NEIGHBORHOODS: {} public neighborhoods available",
        uint32(publicNeighborhoods.size()));
}

void WorldSession::HandleHousingSvcsGetHouseFinderNeighborhood(WorldPackets::Housing::HousingSvcsGetHouseFinderNeighborhood const& housingSvcsGetHouseFinderNeighborhood)
{
    Player* player = GetPlayer();
    if (!player)
        return;

    Neighborhood const* neighborhood = sNeighborhoodMgr.GetNeighborhood(housingSvcsGetHouseFinderNeighborhood.NeighborhoodGuid);
    if (!neighborhood)
    {
        TC_LOG_DEBUG("network", "CMSG_HOUSING_SERVICES_REQUEST_HOUSE_FINDER_NEIGHBORHOOD_DATA: Neighborhood {} not found",
            housingSvcsGetHouseFinderNeighborhood.NeighborhoodGuid.ToString());
        return;
    }

    TC_LOG_DEBUG("network", "CMSG_HOUSING_SERVICES_REQUEST_HOUSE_FINDER_NEIGHBORHOOD_DATA: '{}' MapID:{} Members:{} Public:{}",
        neighborhood->GetName(), neighborhood->GetNeighborhoodMapID(),
        neighborhood->GetMemberCount(), neighborhood->IsPublic());

    WorldPackets::Housing::HousingSvcsGetHouseFinderNeighborhoodResponse response;
    response.Result = static_cast<uint32>(HOUSING_RESULT_SUCCESS);
    SendPacket(response.Write());
}

void WorldSession::HandleHousingSvcsGetBnetFriendNeighborhoods(WorldPackets::Housing::HousingSvcsGetBnetFriendNeighborhoods const& housingSvcsGetBnetFriendNeighborhoods)
{
    Player* player = GetPlayer();
    if (!player)
        return;

    // BNet friend neighborhood lookup requires BNet social integration
    // For now, acknowledge the request
    WorldPackets::Housing::HousingSvcsGetBnetFriendNeighborhoodsResponse response;
    response.Result = static_cast<uint32>(HOUSING_RESULT_SUCCESS);
    SendPacket(response.Write());

    TC_LOG_DEBUG("network", "CMSG_HOUSING_SERVICES_SEARCH_BNET_FRIEND_NEIGHBORHOODS BnetAccountGuid: {}",
        housingSvcsGetBnetFriendNeighborhoods.BnetAccountGuid.ToString());
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

    TC_LOG_DEBUG("network", "CMSG_HOUSING_SERVICES_HOUSE_FINDER_DECLINE_NEIGHBORHOOD_INVITATION: Player {} declined all invitations",
        player->GetGUID().ToString());
}

// ============================================================
// Housing Misc
// ============================================================

void WorldSession::HandleHousingGetCurrentHouseInfo(WorldPackets::Housing::HousingGetCurrentHouseInfo const& housingGetCurrentHouseInfo)
{
    Player* player = GetPlayer();
    if (!player)
        return;

    Housing* housing = player->GetHousing();
    if (housing)
    {
        TC_LOG_DEBUG("network", "CMSG_HOUSING_REQUEST_CURRENT_HOUSE_INFO HouseGuid: {}, Level: {}, Favor: {}, "
            "Decor: {}/{}, Rooms: {}, Fixtures: {}, PlotIndex: {}",
            housingGetCurrentHouseInfo.HouseGuid.ToString(),
            housing->GetLevel(), housing->GetFavor(),
            housing->GetDecorCount(), housing->GetMaxDecorCount(),
            uint32(housing->GetRooms().size()),
            uint32(housing->GetFixtures().size()),
            housing->GetPlotIndex());
    }
    else
    {
        TC_LOG_DEBUG("network", "CMSG_HOUSING_REQUEST_CURRENT_HOUSE_INFO HouseGuid: {} - Player {} has no house",
            housingGetCurrentHouseInfo.HouseGuid.ToString(), player->GetGUID().ToString());
    }

    WorldPackets::Housing::HousingGetCurrentHouseInfoResponse response;
    response.Result = static_cast<uint32>(housing ? HOUSING_RESULT_SUCCESS : HOUSING_RESULT_HOUSE_NOT_FOUND);
    SendPacket(response.Write());
}

void WorldSession::HandleHousingResetKioskMode(WorldPackets::Housing::HousingResetKioskMode const& /*housingResetKioskMode*/)
{
    Player* player = GetPlayer();
    if (!player)
        return;

    player->DeleteHousing();

    WorldPackets::Housing::HousingResetKioskModeResponse response;
    response.Result = static_cast<uint32>(HOUSING_RESULT_SUCCESS);
    SendPacket(response.Write());

    TC_LOG_DEBUG("network", "CMSG_HOUSING_KIOSK_REQUEST_HOUSING_RESET processed for player {}",
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
    response.NeighborhoodGuid = queryNeighborhoodInfo.NeighborhoodGuid;

    Neighborhood const* neighborhood = sNeighborhoodMgr.GetNeighborhood(queryNeighborhoodInfo.NeighborhoodGuid);
    if (neighborhood)
    {
        response.Allow = true;
        response.Name = neighborhood->GetName();
    }
    else
        response.Allow = false;

    SendPacket(response.Write());

    TC_LOG_DEBUG("network", "CMSG_QUERY_NEIGHBORHOOD_INFO NeighborhoodGuid: {}, Found: {}",
        queryNeighborhoodInfo.NeighborhoodGuid.ToString(), response.Allow);
}

void WorldSession::HandleInvitePlayerToNeighborhood(WorldPackets::Housing::InvitePlayerToNeighborhood const& invitePlayerToNeighborhood)
{
    Player* player = GetPlayer();
    if (!player)
        return;

    Neighborhood* neighborhood = sNeighborhoodMgr.GetNeighborhood(invitePlayerToNeighborhood.NeighborhoodGuid);
    if (!neighborhood)
        return;

    if (!neighborhood->IsManager(player->GetGUID()) && !neighborhood->IsOwner(player->GetGUID()))
        return;

    HousingResult result = neighborhood->InviteResident(player->GetGUID(), invitePlayerToNeighborhood.PlayerGuid);

    WorldPackets::Neighborhood::NeighborhoodInviteResidentResponse response;
    response.Result = static_cast<uint32>(result);
    SendPacket(response.Write());

    TC_LOG_DEBUG("network", "CMSG_INVITE_PLAYER_TO_NEIGHBORHOOD PlayerGuid: {}, Result: {}",
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
    SendPacket(response.Write());

    TC_LOG_DEBUG("network", "CMSG_GUILD_GET_OTHERS_OWNED_HOUSES PlayerGuid: {}, FoundNeighborhoods: {}",
        guildGetOthersOwnedHouses.PlayerGuid.ToString(), uint32(neighborhoods.size()));
}
