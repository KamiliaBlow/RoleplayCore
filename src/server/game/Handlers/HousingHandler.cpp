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

    TC_LOG_DEBUG("network", "CMSG_HOUSE_EXTERIOR_COMMIT_POSITION received");
}

// ============================================================
// House Interior System
// ============================================================

void WorldSession::HandleHouseInteriorLeaveHouse(WorldPackets::Housing::HouseInteriorLeaveHouse const& /*houseInteriorLeaveHouse*/)
{
    Player* player = GetPlayer();
    if (!player)
        return;

    TC_LOG_DEBUG("network", "CMSG_HOUSE_INTERIOR_LEAVE_HOUSE received");
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

    housing->SetEditorMode(housingDecorSetEditMode.Active ? HOUSING_EDITOR_MODE_DECOR : HOUSING_EDITOR_MODE_NONE);

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

    TC_LOG_DEBUG("network", "CMSG_HOUSING_DECOR_REMOVE_PLACED_DECOR_ENTRY DecorGuid: {}, Result: {}",
        housingDecorRemove.DecorGuid.ToString(), uint32(result));
}

void WorldSession::HandleHousingDecorLock(WorldPackets::Housing::HousingDecorLock const& housingDecorLock)
{
    Player* player = GetPlayer();
    if (!player)
        return;

    TC_LOG_DEBUG("network", "CMSG_HOUSING_DECOR_SELECT_DECOR HouseGuid: {}, DecorGuid: {}",
        housingDecorLock.HouseGuid.ToString(), housingDecorLock.DecorGuid.ToString());
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

    TC_LOG_DEBUG("network", "CMSG_HOUSING_DECOR_CATALOG_DESTROY_ALL CatalogEntryID: {}, Result: {}",
        housingDecorDeleteFromStorageById.CatalogEntryID, uint32(result));
}

void WorldSession::HandleHousingDecorRequestStorage(WorldPackets::Housing::HousingDecorRequestStorage const& housingDecorRequestStorage)
{
    Player* player = GetPlayer();
    if (!player)
        return;

    TC_LOG_DEBUG("network", "CMSG_HOUSING_DECOR_CATALOG_CREATE_SEARCHER HouseGuid: {}",
        housingDecorRequestStorage.HouseGuid.ToString());
}

void WorldSession::HandleHousingDecorRedeemDeferredDecor(WorldPackets::Housing::HousingDecorRedeemDeferredDecor const& housingDecorRedeemDeferredDecor)
{
    Player* player = GetPlayer();
    if (!player)
        return;

    TC_LOG_DEBUG("network", "CMSG_HOUSING_DECOR_START_PLACING_NEW_DECOR HouseGuid: {}, CatalogEntryID: {}",
        housingDecorRedeemDeferredDecor.HouseGuid.ToString(), housingDecorRedeemDeferredDecor.CatalogEntryID);
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

    housing->SetEditorMode(housingFixtureSetEditMode.Active ? HOUSING_EDITOR_MODE_FIXTURE : HOUSING_EDITOR_MODE_NONE);

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

    housing->SetEditorMode(housingRoomSetLayoutEditMode.Active ? HOUSING_EDITOR_MODE_ROOM : HOUSING_EDITOR_MODE_NONE);

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

    TC_LOG_DEBUG("network", "CMSG_HOUSING_SERVICES_RESERVE_PLOT PlotIndex: {}, Result: {}",
        housingSvcsNeighborhoodReservePlot.PlotIndex, uint32(result));
}

void WorldSession::HandleHousingSvcsRelinquishHouse(WorldPackets::Housing::HousingSvcsRelinquishHouse const& /*housingSvcsRelinquishHouse*/)
{
    Player* player = GetPlayer();
    if (!player)
        return;

    player->DeleteHousing();

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

    TC_LOG_DEBUG("network", "CMSG_HOUSING_SERVICES_SAVE_HOUSE_SETTINGS HouseGuid: {}",
        housingSvcsUpdateHouseSettings.HouseGuid.ToString());
}

void WorldSession::HandleHousingSvcsPlayerViewHousesByPlayer(WorldPackets::Housing::HousingSvcsPlayerViewHousesByPlayer const& housingSvcsPlayerViewHousesByPlayer)
{
    Player* player = GetPlayer();
    if (!player)
        return;

    TC_LOG_DEBUG("network", "CMSG_HOUSING_SERVICES_GET_OTHERS_PLAYER_OWNED_HOUSES PlayerGuid: {}",
        housingSvcsPlayerViewHousesByPlayer.PlayerGuid.ToString());
}

void WorldSession::HandleHousingSvcsPlayerViewHousesByBnetAccount(WorldPackets::Housing::HousingSvcsPlayerViewHousesByBnetAccount const& housingSvcsPlayerViewHousesByBnetAccount)
{
    Player* player = GetPlayer();
    if (!player)
        return;

    TC_LOG_DEBUG("network", "CMSG_HOUSING_SERVICES_GET_OTHERS_BNET_ACCOUNT_OWNED_HOUSES BnetAccountGuid: {}",
        housingSvcsPlayerViewHousesByBnetAccount.BnetAccountGuid.ToString());
}

void WorldSession::HandleHousingSvcsGetPlayerHousesInfo(WorldPackets::Housing::HousingSvcsGetPlayerHousesInfo const& /*housingSvcsGetPlayerHousesInfo*/)
{
    Player* player = GetPlayer();
    if (!player)
        return;

    TC_LOG_DEBUG("network", "CMSG_HOUSING_SERVICES_GET_PLAYER_OWNED_HOUSES received");
}

void WorldSession::HandleHousingSvcsTeleportToPlot(WorldPackets::Housing::HousingSvcsTeleportToPlot const& housingSvcsTeleportToPlot)
{
    Player* player = GetPlayer();
    if (!player)
        return;

    TC_LOG_DEBUG("network", "CMSG_HOUSING_SERVICES_ENTER_HOUSE NeighborhoodGuid: {}, PlotGuid: {}, PlotIndex: {}, Flags: {}",
        housingSvcsTeleportToPlot.NeighborhoodGuid.ToString(), housingSvcsTeleportToPlot.PlotGuid.ToString(),
        housingSvcsTeleportToPlot.PlotIndex, housingSvcsTeleportToPlot.Flags);
}

void WorldSession::HandleHousingSvcsStartTutorial(WorldPackets::Housing::HousingSvcsStartTutorial const& /*housingSvcsStartTutorial*/)
{
    Player* player = GetPlayer();
    if (!player)
        return;

    TC_LOG_DEBUG("network", "CMSG_HOUSING_SERVICES_START_TUTORIAL received");
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

    TC_LOG_DEBUG("network", "CMSG_HOUSING_SERVICES_ACCEPT_NEIGHBORHOOD_OWNERSHIP NeighborhoodGuid: {}",
        housingSvcsAcceptNeighborhoodOwnership.NeighborhoodGuid.ToString());
}

void WorldSession::HandleHousingSvcsRejectNeighborhoodOwnership(WorldPackets::Housing::HousingSvcsRejectNeighborhoodOwnership const& housingSvcsRejectNeighborhoodOwnership)
{
    Player* player = GetPlayer();
    if (!player)
        return;

    TC_LOG_DEBUG("network", "CMSG_HOUSING_SERVICES_DECLINE_NEIGHBORHOOD_OWNERSHIP NeighborhoodGuid: {}",
        housingSvcsRejectNeighborhoodOwnership.NeighborhoodGuid.ToString());
}

void WorldSession::HandleHousingSvcsGetPotentialHouseOwners(WorldPackets::Housing::HousingSvcsGetPotentialHouseOwners const& /*housingSvcsGetPotentialHouseOwners*/)
{
    Player* player = GetPlayer();
    if (!player)
        return;

    TC_LOG_DEBUG("network", "CMSG_HOUSING_SERVICES_REQUEST_PLAYER_CHARACTER_LIST received");
}

void WorldSession::HandleHousingSvcsGetHouseFinderInfo(WorldPackets::Housing::HousingSvcsGetHouseFinderInfo const& /*housingSvcsGetHouseFinderInfo*/)
{
    Player* player = GetPlayer();
    if (!player)
        return;

    TC_LOG_DEBUG("network", "CMSG_HOUSING_SERVICES_HOUSE_FINDER_REQUEST_NEIGHBORHOODS received");
}

void WorldSession::HandleHousingSvcsGetHouseFinderNeighborhood(WorldPackets::Housing::HousingSvcsGetHouseFinderNeighborhood const& housingSvcsGetHouseFinderNeighborhood)
{
    Player* player = GetPlayer();
    if (!player)
        return;

    TC_LOG_DEBUG("network", "CMSG_HOUSING_SERVICES_REQUEST_HOUSE_FINDER_NEIGHBORHOOD_DATA NeighborhoodGuid: {}",
        housingSvcsGetHouseFinderNeighborhood.NeighborhoodGuid.ToString());
}

void WorldSession::HandleHousingSvcsGetBnetFriendNeighborhoods(WorldPackets::Housing::HousingSvcsGetBnetFriendNeighborhoods const& housingSvcsGetBnetFriendNeighborhoods)
{
    Player* player = GetPlayer();
    if (!player)
        return;

    TC_LOG_DEBUG("network", "CMSG_HOUSING_SERVICES_SEARCH_BNET_FRIEND_NEIGHBORHOODS BnetAccountGuid: {}",
        housingSvcsGetBnetFriendNeighborhoods.BnetAccountGuid.ToString());
}

void WorldSession::HandleHousingSvcsDeleteAllNeighborhoodInvites(WorldPackets::Housing::HousingSvcsDeleteAllNeighborhoodInvites const& /*housingSvcsDeleteAllNeighborhoodInvites*/)
{
    Player* player = GetPlayer();
    if (!player)
        return;

    TC_LOG_DEBUG("network", "CMSG_HOUSING_SERVICES_HOUSE_FINDER_DECLINE_NEIGHBORHOOD_INVITATION received");
}

// ============================================================
// Housing Misc
// ============================================================

void WorldSession::HandleHousingGetCurrentHouseInfo(WorldPackets::Housing::HousingGetCurrentHouseInfo const& housingGetCurrentHouseInfo)
{
    Player* player = GetPlayer();
    if (!player)
        return;

    TC_LOG_DEBUG("network", "CMSG_HOUSING_REQUEST_CURRENT_HOUSE_INFO HouseGuid: {}",
        housingGetCurrentHouseInfo.HouseGuid.ToString());
}

void WorldSession::HandleHousingResetKioskMode(WorldPackets::Housing::HousingResetKioskMode const& /*housingResetKioskMode*/)
{
    Player* player = GetPlayer();
    if (!player)
        return;

    player->DeleteHousing();

    TC_LOG_DEBUG("network", "CMSG_HOUSING_KIOSK_REQUEST_HOUSING_RESET processed");
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

    TC_LOG_DEBUG("network", "CMSG_INVITE_PLAYER_TO_NEIGHBORHOOD PlayerGuid: {}, Result: {}",
        invitePlayerToNeighborhood.PlayerGuid.ToString(), uint32(result));
}
