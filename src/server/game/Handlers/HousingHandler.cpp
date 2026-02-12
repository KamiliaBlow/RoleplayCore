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
#include "HousingPackets.h"
#include "Log.h"
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

    TC_LOG_DEBUG("network", "CMSG_HOUSING_DECOR_SET_EDITOR_MODE_ACTIVE Active: {}", housingDecorSetEditMode.Active);
}

void WorldSession::HandleHousingDecorPlace(WorldPackets::Housing::HousingDecorPlace const& housingDecorPlace)
{
    Player* player = GetPlayer();
    if (!player)
        return;

    TC_LOG_DEBUG("network", "CMSG_HOUSING_DECOR_PLACE_NEW_DECOR HouseGuid: {}, DecorEntryID: {}, Pos: ({}, {}, {})",
        housingDecorPlace.HouseGuid.ToString(), housingDecorPlace.DecorEntryID,
        housingDecorPlace.Pos.Pos.GetPositionX(), housingDecorPlace.Pos.Pos.GetPositionY(), housingDecorPlace.Pos.Pos.GetPositionZ());
}

void WorldSession::HandleHousingDecorMove(WorldPackets::Housing::HousingDecorMove const& housingDecorMove)
{
    Player* player = GetPlayer();
    if (!player)
        return;

    TC_LOG_DEBUG("network", "CMSG_HOUSING_DECOR_MOVE_DECOR HouseGuid: {}, DecorGuid: {}, Pos: ({}, {}, {})",
        housingDecorMove.HouseGuid.ToString(), housingDecorMove.DecorGuid.ToString(),
        housingDecorMove.Pos.Pos.GetPositionX(), housingDecorMove.Pos.Pos.GetPositionY(), housingDecorMove.Pos.Pos.GetPositionZ());
}

void WorldSession::HandleHousingDecorRemove(WorldPackets::Housing::HousingDecorRemove const& housingDecorRemove)
{
    Player* player = GetPlayer();
    if (!player)
        return;

    TC_LOG_DEBUG("network", "CMSG_HOUSING_DECOR_REMOVE_PLACED_DECOR_ENTRY HouseGuid: {}, DecorGuid: {}",
        housingDecorRemove.HouseGuid.ToString(), housingDecorRemove.DecorGuid.ToString());
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

    TC_LOG_DEBUG("network", "CMSG_HOUSING_DECOR_COMMIT_DYES_FOR_SELECTED_DECOR HouseGuid: {}, DecorGuid: {}, DyeSlotCount: {}",
        housingDecorSetDyeSlots.HouseGuid.ToString(), housingDecorSetDyeSlots.DecorGuid.ToString(),
        housingDecorSetDyeSlots.DyeSlots.size());
}

void WorldSession::HandleHousingDecorDeleteFromStorage(WorldPackets::Housing::HousingDecorDeleteFromStorage const& housingDecorDeleteFromStorage)
{
    Player* player = GetPlayer();
    if (!player)
        return;

    TC_LOG_DEBUG("network", "CMSG_HOUSING_DECOR_CATALOG_DESTROY_ENTRY HouseGuid: {}, CatalogEntryID: {}",
        housingDecorDeleteFromStorage.HouseGuid.ToString(), housingDecorDeleteFromStorage.CatalogEntryID);
}

void WorldSession::HandleHousingDecorDeleteFromStorageById(WorldPackets::Housing::HousingDecorDeleteFromStorageById const& housingDecorDeleteFromStorageById)
{
    Player* player = GetPlayer();
    if (!player)
        return;

    TC_LOG_DEBUG("network", "CMSG_HOUSING_DECOR_CATALOG_DESTROY_ALL_ENTRY_COPIES HouseGuid: {}, CatalogEntryID: {}",
        housingDecorDeleteFromStorageById.HouseGuid.ToString(), housingDecorDeleteFromStorageById.CatalogEntryID);
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

    TC_LOG_DEBUG("network", "CMSG_HOUSING_FIXTURE_SET_EDITOR_MODE_ACTIVE Active: {}", housingFixtureSetEditMode.Active);
}

void WorldSession::HandleHousingFixtureSetCoreFixture(WorldPackets::Housing::HousingFixtureSetCoreFixture const& housingFixtureSetCoreFixture)
{
    Player* player = GetPlayer();
    if (!player)
        return;

    TC_LOG_DEBUG("network", "CMSG_HOUSING_FIXTURE_SELECT_CORE_FIXTURE_OPTION HouseGuid: {}, FixturePointID: {}, OptionID: {}",
        housingFixtureSetCoreFixture.HouseGuid.ToString(), housingFixtureSetCoreFixture.FixturePointID,
        housingFixtureSetCoreFixture.OptionID);
}

void WorldSession::HandleHousingFixtureCreateFixture(WorldPackets::Housing::HousingFixtureCreateFixture const& housingFixtureCreateFixture)
{
    Player* player = GetPlayer();
    if (!player)
        return;

    TC_LOG_DEBUG("network", "CMSG_HOUSING_FIXTURE_SELECT_FIXTURE_OPTION HouseGuid: {}, FixturePointID: {}, OptionID: {}",
        housingFixtureCreateFixture.HouseGuid.ToString(), housingFixtureCreateFixture.FixturePointID,
        housingFixtureCreateFixture.OptionID);
}

void WorldSession::HandleHousingFixtureDeleteFixture(WorldPackets::Housing::HousingFixtureDeleteFixture const& housingFixtureDeleteFixture)
{
    Player* player = GetPlayer();
    if (!player)
        return;

    TC_LOG_DEBUG("network", "CMSG_HOUSING_FIXTURE_REMOVE_FIXTURE_FROM_SELECTED_POINT HouseGuid: {}, FixturePointID: {}",
        housingFixtureDeleteFixture.HouseGuid.ToString(), housingFixtureDeleteFixture.FixturePointID);
}

// ============================================================
// Room System
// ============================================================

void WorldSession::HandleHousingRoomSetLayoutEditMode(WorldPackets::Housing::HousingRoomSetLayoutEditMode const& housingRoomSetLayoutEditMode)
{
    Player* player = GetPlayer();
    if (!player)
        return;

    TC_LOG_DEBUG("network", "CMSG_HOUSING_ROOM_SET_EDITOR_MODE_ACTIVE Active: {}", housingRoomSetLayoutEditMode.Active);
}

void WorldSession::HandleHousingRoomAdd(WorldPackets::Housing::HousingRoomAdd const& housingRoomAdd)
{
    Player* player = GetPlayer();
    if (!player)
        return;

    TC_LOG_DEBUG("network", "CMSG_HOUSING_ROOM_PLACE_ROOM HouseGuid: {}, RoomID: {}, SlotIndex: {}, Orientation: {}, Mirrored: {}",
        housingRoomAdd.HouseGuid.ToString(), housingRoomAdd.RoomID, housingRoomAdd.SlotIndex,
        housingRoomAdd.Orientation, housingRoomAdd.Mirrored);
}

void WorldSession::HandleHousingRoomRemove(WorldPackets::Housing::HousingRoomRemove const& housingRoomRemove)
{
    Player* player = GetPlayer();
    if (!player)
        return;

    TC_LOG_DEBUG("network", "CMSG_HOUSING_ROOM_REMOVE_ROOM RoomGuid: {}", housingRoomRemove.RoomGuid.ToString());
}

void WorldSession::HandleHousingRoomRotate(WorldPackets::Housing::HousingRoomRotate const& housingRoomRotate)
{
    Player* player = GetPlayer();
    if (!player)
        return;

    TC_LOG_DEBUG("network", "CMSG_HOUSING_ROOM_ROTATE_ROOM RoomGuid: {}, Clockwise: {}",
        housingRoomRotate.RoomGuid.ToString(), housingRoomRotate.Clockwise);
}

void WorldSession::HandleHousingRoomMoveRoom(WorldPackets::Housing::HousingRoomMoveRoom const& housingRoomMoveRoom)
{
    Player* player = GetPlayer();
    if (!player)
        return;

    TC_LOG_DEBUG("network", "CMSG_HOUSING_ROOM_MOVE_DRAGGED_ROOM RoomGuid: {}, NewSlotIndex: {}, SwapRoomGuid: {}, SwapSlotIndex: {}",
        housingRoomMoveRoom.RoomGuid.ToString(), housingRoomMoveRoom.NewSlotIndex,
        housingRoomMoveRoom.SwapRoomGuid.ToString(), housingRoomMoveRoom.SwapSlotIndex);
}

void WorldSession::HandleHousingRoomSetComponentTheme(WorldPackets::Housing::HousingRoomSetComponentTheme const& housingRoomSetComponentTheme)
{
    Player* player = GetPlayer();
    if (!player)
        return;

    TC_LOG_DEBUG("network", "CMSG_HOUSING_ROOM_APPLY_THEME RoomGuid: {}, ThemeSetID: {}, ComponentCount: {}",
        housingRoomSetComponentTheme.RoomGuid.ToString(), housingRoomSetComponentTheme.ThemeSetID,
        housingRoomSetComponentTheme.ComponentIDs.size());
}

void WorldSession::HandleHousingRoomApplyComponentMaterials(WorldPackets::Housing::HousingRoomApplyComponentMaterials const& housingRoomApplyComponentMaterials)
{
    Player* player = GetPlayer();
    if (!player)
        return;

    TC_LOG_DEBUG("network", "CMSG_HOUSING_ROOM_APPLY_WALLPAPER RoomGuid: {}, WallpaperID: {}, MaterialID: {}, ComponentCount: {}",
        housingRoomApplyComponentMaterials.RoomGuid.ToString(), housingRoomApplyComponentMaterials.WallpaperID,
        housingRoomApplyComponentMaterials.MaterialID, housingRoomApplyComponentMaterials.ComponentIDs.size());
}

void WorldSession::HandleHousingRoomSetDoorType(WorldPackets::Housing::HousingRoomSetDoorType const& housingRoomSetDoorType)
{
    Player* player = GetPlayer();
    if (!player)
        return;

    TC_LOG_DEBUG("network", "CMSG_HOUSING_ROOM_SET_ROOM_COMPONENT_DOOR_TYPE RoomGuid: {}, DoorTypeID: {}, DoorSlot: {}",
        housingRoomSetDoorType.RoomGuid.ToString(), housingRoomSetDoorType.DoorTypeID,
        housingRoomSetDoorType.DoorSlot);
}

void WorldSession::HandleHousingRoomSetCeilingType(WorldPackets::Housing::HousingRoomSetCeilingType const& housingRoomSetCeilingType)
{
    Player* player = GetPlayer();
    if (!player)
        return;

    TC_LOG_DEBUG("network", "CMSG_HOUSING_ROOM_SET_ROOM_COMPONENT_CEILING_TYPE RoomGuid: {}, CeilingTypeID: {}, CeilingSlot: {}",
        housingRoomSetCeilingType.RoomGuid.ToString(), housingRoomSetCeilingType.CeilingTypeID,
        housingRoomSetCeilingType.CeilingSlot);
}

// ============================================================
// Housing Services System
// ============================================================

void WorldSession::HandleHousingSvcsGuildCreateNeighborhood(WorldPackets::Housing::HousingSvcsGuildCreateNeighborhood const& housingSvcsGuildCreateNeighborhood)
{
    Player* player = GetPlayer();
    if (!player)
        return;

    TC_LOG_DEBUG("network", "CMSG_HOUSING_SERVICES_CREATE_GUILD_NEIGHBORHOOD NeighborhoodMapID: {}, FactionID: {}, Name: {}",
        housingSvcsGuildCreateNeighborhood.NeighborhoodMapID, housingSvcsGuildCreateNeighborhood.FactionID,
        housingSvcsGuildCreateNeighborhood.Name);
}

void WorldSession::HandleHousingSvcsNeighborhoodReservePlot(WorldPackets::Housing::HousingSvcsNeighborhoodReservePlot const& housingSvcsNeighborhoodReservePlot)
{
    Player* player = GetPlayer();
    if (!player)
        return;

    TC_LOG_DEBUG("network", "CMSG_HOUSING_SERVICES_HOUSE_FINDER_REQUEST_RESERVATION_AND_PORT NeighborhoodGuid: {}, PlotIndex: {}, HasPreference: {}",
        housingSvcsNeighborhoodReservePlot.NeighborhoodGuid.ToString(), housingSvcsNeighborhoodReservePlot.PlotIndex,
        housingSvcsNeighborhoodReservePlot.HasPreference);
}

void WorldSession::HandleHousingSvcsRelinquishHouse(WorldPackets::Housing::HousingSvcsRelinquishHouse const& housingSvcsRelinquishHouse)
{
    Player* player = GetPlayer();
    if (!player)
        return;

    TC_LOG_DEBUG("network", "CMSG_HOUSING_SERVICES_RELINQUISH_HOUSE HouseGuid: {}",
        housingSvcsRelinquishHouse.HouseGuid.ToString());
}

void WorldSession::HandleHousingSvcsUpdateHouseSettings(WorldPackets::Housing::HousingSvcsUpdateHouseSettings const& housingSvcsUpdateHouseSettings)
{
    Player* player = GetPlayer();
    if (!player)
        return;

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

    TC_LOG_DEBUG("network", "CMSG_HOUSING_KIOSK_REQUEST_HOUSING_RESET received");
}

// ============================================================
// Other Housing CMSG
// ============================================================

void WorldSession::HandleQueryNeighborhoodInfo(WorldPackets::Housing::QueryNeighborhoodInfo const& queryNeighborhoodInfo)
{
    Player* player = GetPlayer();
    if (!player)
        return;

    TC_LOG_DEBUG("network", "CMSG_QUERY_NEIGHBORHOOD_INFO NeighborhoodGuid: {}",
        queryNeighborhoodInfo.NeighborhoodGuid.ToString());

    WorldPackets::Housing::QueryNeighborhoodNameResponse response;
    response.NeighborhoodGuid = queryNeighborhoodInfo.NeighborhoodGuid;
    response.Allow = false;
    SendPacket(response.Write());
}

void WorldSession::HandleInvitePlayerToNeighborhood(WorldPackets::Housing::InvitePlayerToNeighborhood const& invitePlayerToNeighborhood)
{
    Player* player = GetPlayer();
    if (!player)
        return;

    TC_LOG_DEBUG("network", "CMSG_INVITE_PLAYER_TO_NEIGHBORHOOD PlayerGuid: {}, NeighborhoodGuid: {}",
        invitePlayerToNeighborhood.PlayerGuid.ToString(), invitePlayerToNeighborhood.NeighborhoodGuid.ToString());
}
