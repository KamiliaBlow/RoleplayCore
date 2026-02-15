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

#include "HousingPackets.h"
#include "PacketOperators.h"

// ============================================================
// Housing namespace (Decor, Fixture, Room, Services, Misc)
// ============================================================
namespace WorldPackets::Housing
{

// --- House Exterior System ---

void HouseExteriorCommitPosition::Read()
{
    _worldPacket >> HouseGuid;
    _worldPacket >> PlotGuid;
    _worldPacket >> PositionX;
    _worldPacket >> PositionY;
    _worldPacket >> PositionZ;
    _worldPacket >> Facing;
}

// --- Decor System ---

void HousingDecorSetEditMode::Read()
{
    _worldPacket >> Bits<1>(Active);
}

void HousingDecorPlace::Read()
{
    _worldPacket >> DecorGuid;
    _worldPacket >> PositionX;
    _worldPacket >> PositionY;
    _worldPacket >> PositionZ;
    _worldPacket >> RotationX;
    _worldPacket >> RotationY;
    _worldPacket >> RotationZ;
    _worldPacket >> RotationW;
    _worldPacket >> RoomGuid;
    _worldPacket >> FixtureGuid;
    _worldPacket >> AttachParentGuid;
    _worldPacket >> DecorRecID;
}

void HousingDecorMove::Read()
{
    _worldPacket >> DecorGuid;
    _worldPacket >> PositionX;
    _worldPacket >> PositionY;
    _worldPacket >> PositionZ;
    _worldPacket >> RotationX;
    _worldPacket >> RotationY;
    _worldPacket >> RotationZ;
    _worldPacket >> RotationW;
    _worldPacket >> RoomGuid;
    _worldPacket >> FixtureGuid;
    _worldPacket >> AttachParentGuid;
    _worldPacket >> DecorRecID;
    _worldPacket >> Flags;
    _worldPacket >> Field_86;
    _worldPacket >> Bits<1>(AttachToParent);
}

void HousingDecorRemove::Read()
{
    _worldPacket >> DecorGuid;
}

void HousingDecorLock::Read()
{
    _worldPacket >> DecorGuid;
    _worldPacket >> Bits<1>(Locked);
    _worldPacket >> Bits<1>(AnchorLocked);
}

void HousingDecorSetDyeSlots::Read()
{
    _worldPacket >> DecorGuid;
    for (int32& dyeColor : DyeColorID)
        _worldPacket >> dyeColor;
}

void HousingDecorDeleteFromStorage::Read()
{
    uint32 count = 0;
    _worldPacket >> Bits<32>(count);
    DecorGuids.resize(count);
    for (ObjectGuid& guid : DecorGuids)
        _worldPacket >> guid;
}

void HousingDecorDeleteFromStorageById::Read()
{
    _worldPacket >> DecorRecID;
}

void HousingDecorRequestStorage::Read()
{
    _worldPacket >> HouseGuid;
}

void HousingDecorRedeemDeferredDecor::Read()
{
    _worldPacket >> DeferredDecorID;
    _worldPacket >> RedemptionToken;
}

// --- Fixture System ---

void HousingFixtureSetEditMode::Read()
{
    _worldPacket >> Bits<1>(Active);
}

void HousingFixtureSetCoreFixture::Read()
{
    _worldPacket >> FixtureGuid;
    _worldPacket >> ExteriorComponentID;
}

void HousingFixtureCreateFixture::Read()
{
    _worldPacket >> AttachParentGuid;
    _worldPacket >> RoomGuid;
    _worldPacket >> ExteriorComponentType;
    _worldPacket >> ExteriorComponentHookID;
}

void HousingFixtureDeleteFixture::Read()
{
    _worldPacket >> FixtureGuid;
    _worldPacket >> RoomGuid;
    _worldPacket >> ExteriorComponentID;
}

void HousingFixtureSetHouseSize::Read()
{
    _worldPacket >> HouseGuid;
    _worldPacket >> Size;
}

void HousingFixtureSetHouseType::Read()
{
    _worldPacket >> HouseGuid;
    _worldPacket >> HouseExteriorWmoDataID;
}

void HouseExteriorLock::Read()
{
    _worldPacket >> HouseGuid;
    _worldPacket >> PlotGuid;
    _worldPacket >> NeighborhoodGuid;
    _worldPacket >> Bits<1>(Locked);
}

// --- Room System ---

void HousingRoomSetLayoutEditMode::Read()
{
    _worldPacket >> Bits<1>(Active);
}

void HousingRoomAdd::Read()
{
    _worldPacket >> HouseGuid;
    _worldPacket >> HouseRoomID;
    _worldPacket >> Flags;
    _worldPacket >> FloorIndex;
    _worldPacket >> Bits<1>(AutoFurnish);
}

void HousingRoomRemove::Read()
{
    _worldPacket >> RoomGuid;
}

void HousingRoomRotate::Read()
{
    _worldPacket >> RoomGuid;
    _worldPacket >> Bits<1>(Clockwise);
}

void HousingRoomMoveRoom::Read()
{
    _worldPacket >> RoomGuid;
    _worldPacket >> TargetSlotIndex;
    _worldPacket >> TargetGuid;
    _worldPacket >> FloorIndex;
}

void HousingRoomSetComponentTheme::Read()
{
    _worldPacket >> RoomGuid;
    _worldPacket >> Size<uint32>(RoomComponentIDs);
    _worldPacket >> HouseThemeID;
    for (uint32& componentID : RoomComponentIDs)
        _worldPacket >> componentID;
}

void HousingRoomApplyComponentMaterials::Read()
{
    _worldPacket >> RoomGuid;
    _worldPacket >> Size<uint32>(RoomComponentIDs);
    _worldPacket >> RoomComponentTextureID;
    _worldPacket >> RoomComponentTypeParam;
    for (uint32& componentID : RoomComponentIDs)
        _worldPacket >> componentID;
}

void HousingRoomSetDoorType::Read()
{
    _worldPacket >> RoomGuid;
    _worldPacket >> RoomComponentID;
    _worldPacket >> RoomComponentType;
}

void HousingRoomSetCeilingType::Read()
{
    _worldPacket >> RoomGuid;
    _worldPacket >> RoomComponentID;
    _worldPacket >> RoomComponentType;
}

// --- Housing Services System ---

void HousingSvcsGuildCreateNeighborhood::Read()
{
    _worldPacket >> NeighborhoodTypeID;
    _worldPacket >> Flags;
    _worldPacket >> SizedString::BitsSize<7>(NeighborhoodName);

    _worldPacket >> SizedString::Data(NeighborhoodName);
}

void HousingSvcsNeighborhoodReservePlot::Read()
{
    _worldPacket >> NeighborhoodGuid;
    _worldPacket >> PlotIndex;
    _worldPacket >> Bits<1>(Reserve);
}

void HousingSvcsRelinquishHouse::Read()
{
    _worldPacket >> HouseGuid;
}

void HousingSvcsUpdateHouseSettings::Read()
{
    _worldPacket >> HouseGuid;
    _worldPacket >> OptionalInit(PlotSettingsID);
    _worldPacket >> OptionalInit(VisitorPermissionGuid);

    if (PlotSettingsID)
        _worldPacket >> *PlotSettingsID;

    if (VisitorPermissionGuid)
        _worldPacket >> *VisitorPermissionGuid;
}

void HousingSvcsPlayerViewHousesByPlayer::Read()
{
    _worldPacket >> PlayerGuid;
}

void HousingSvcsPlayerViewHousesByBnetAccount::Read()
{
    _worldPacket >> BnetAccountGuid;
}

void HousingSvcsTeleportToPlot::Read()
{
    _worldPacket >> NeighborhoodGuid;
    _worldPacket >> OwnerGuid;
    _worldPacket >> PlotIndex;
    _worldPacket >> TeleportType;
}

void HousingSvcsAcceptNeighborhoodOwnership::Read()
{
    _worldPacket >> NeighborhoodGuid;
}

void HousingSvcsRejectNeighborhoodOwnership::Read()
{
    _worldPacket >> NeighborhoodGuid;
}

void HousingSvcsGetHouseFinderNeighborhood::Read()
{
    _worldPacket >> NeighborhoodGuid;
}

void HousingSvcsGetBnetFriendNeighborhoods::Read()
{
    _worldPacket >> BnetAccountGuid;
}

// --- Housing Misc ---
// HousingGetCurrentHouseInfo::Read() and HousingHouseStatus::Read() are empty (inline in header)

void HousingGetPlayerPermissions::Read()
{
    _worldPacket >> OptionalInit(PlayerGuid);

    if (PlayerGuid)
        _worldPacket >> *PlayerGuid;
}

void HousingSvcsGetPotentialHouseOwners::Read()
{
    _worldPacket >> NeighborhoodGuid;
}

// --- Other Housing CMSG ---

void DeclineNeighborhoodInvites::Read()
{
    _worldPacket >> Bits<1>(Allow);
}

void QueryNeighborhoodInfo::Read()
{
    _worldPacket >> NeighborhoodGuid;
}

void InvitePlayerToNeighborhood::Read()
{
    _worldPacket >> PlayerGuid;
    _worldPacket >> NeighborhoodGuid;
}

void GuildGetOthersOwnedHouses::Read()
{
    _worldPacket >> PlayerGuid;
}

// --- SMSG Packets ---

WorldPacket const* QueryNeighborhoodNameResponse::Write()
{
    _worldPacket << NeighborhoodGuid;
    _worldPacket << Bits<1>(Allow);

    if (Allow)
    {
        _worldPacket << SizedString::BitsSize<7>(Name);
        _worldPacket.FlushBits();

        _worldPacket << SizedString::Data(Name);
    }
    else
        _worldPacket.FlushBits();

    return &_worldPacket;
}

WorldPacket const* InvalidateNeighborhoodName::Write()
{
    _worldPacket << NeighborhoodGuid;

    return &_worldPacket;
}

// ============================================================
// House Exterior SMSG Responses (0x50xxxx)
// ============================================================

WorldPacket const* HouseExteriorLockResponse::Write()
{
    _worldPacket << uint32(Result);
    return &_worldPacket;
}

WorldPacket const* HouseExteriorSetHousePositionResponse::Write()
{
    _worldPacket << uint32(Result);
    return &_worldPacket;
}

// ============================================================
// Housing Decor SMSG Responses (0x51xxxx)
// ============================================================

WorldPacket const* HousingDecorSetEditModeResponse::Write()
{
    _worldPacket << uint32(Result);
    return &_worldPacket;
}

WorldPacket const* HousingDecorMoveResponse::Write()
{
    _worldPacket << uint32(Result);
    return &_worldPacket;
}

WorldPacket const* HousingDecorPlaceResponse::Write()
{
    _worldPacket << uint32(Result);
    _worldPacket << DecorGuid;
    return &_worldPacket;
}

WorldPacket const* HousingDecorRemoveResponse::Write()
{
    _worldPacket << uint32(Result);
    return &_worldPacket;
}

WorldPacket const* HousingDecorLockResponse::Write()
{
    _worldPacket << uint32(Result);
    _worldPacket << DecorGuid;
    _worldPacket.WriteBit(Locked);
    _worldPacket.FlushBits();
    return &_worldPacket;
}

WorldPacket const* HousingDecorDeleteFromStorageResponse::Write()
{
    _worldPacket << uint32(Result);
    return &_worldPacket;
}

WorldPacket const* HousingDecorRequestStorageResponse::Write()
{
    _worldPacket << uint32(Result);
    _worldPacket << uint32(Entries.size());
    for (auto const& entry : Entries)
    {
        _worldPacket << uint32(entry.DecorEntryId);
        _worldPacket << uint32(entry.Count);
    }
    return &_worldPacket;
}

WorldPacket const* HousingDecorAddToHouseChestResponse::Write()
{
    _worldPacket << uint32(Result);
    return &_worldPacket;
}

WorldPacket const* HousingDecorSystemSetDyeSlotsResponse::Write()
{
    _worldPacket << uint32(Result);
    return &_worldPacket;
}

WorldPacket const* HousingRedeemDeferredDecorResponse::Write()
{
    _worldPacket << uint32(Result);
    return &_worldPacket;
}

WorldPacket const* HousingFirstTimeDecorAcquisition::Write()
{
    _worldPacket << uint32(DecorEntryID);
    return &_worldPacket;
}

// ============================================================
// Housing Fixture SMSG Responses (0x52xxxx)
// ============================================================

WorldPacket const* HousingFixtureSetEditModeResponse::Write()
{
    _worldPacket << uint32(Result);
    return &_worldPacket;
}

WorldPacket const* HousingFixtureCreateBasicHouseResponse::Write()
{
    _worldPacket << uint32(Result);
    _worldPacket << HouseGuid;
    return &_worldPacket;
}

WorldPacket const* HousingFixtureDeleteHouseResponse::Write()
{
    _worldPacket << uint32(Result);
    return &_worldPacket;
}

WorldPacket const* HousingFixtureSetHouseSizeResponse::Write()
{
    _worldPacket << uint32(Result);
    return &_worldPacket;
}

WorldPacket const* HousingFixtureSetHouseTypeResponse::Write()
{
    _worldPacket << uint32(Result);
    return &_worldPacket;
}

WorldPacket const* HousingFixtureSetCoreFixtureResponse::Write()
{
    _worldPacket << uint32(Result);
    return &_worldPacket;
}

WorldPacket const* HousingFixtureCreateFixtureResponse::Write()
{
    _worldPacket << uint32(Result);
    return &_worldPacket;
}

WorldPacket const* HousingFixtureDeleteFixtureResponse::Write()
{
    _worldPacket << uint32(Result);
    return &_worldPacket;
}

// ============================================================
// Housing Room SMSG Responses (0x53xxxx)
// ============================================================

WorldPacket const* HousingRoomSetLayoutEditModeResponse::Write()
{
    _worldPacket << uint32(Result);
    return &_worldPacket;
}

WorldPacket const* HousingRoomAddResponse::Write()
{
    _worldPacket << uint32(Result);
    _worldPacket << RoomGuid;
    return &_worldPacket;
}

WorldPacket const* HousingRoomRemoveResponse::Write()
{
    _worldPacket << uint32(Result);
    return &_worldPacket;
}

WorldPacket const* HousingRoomUpdateResponse::Write()
{
    _worldPacket << uint32(Result);
    return &_worldPacket;
}

WorldPacket const* HousingRoomSetComponentThemeResponse::Write()
{
    _worldPacket << uint32(Result);
    return &_worldPacket;
}

WorldPacket const* HousingRoomApplyComponentMaterialsResponse::Write()
{
    _worldPacket << uint32(Result);
    return &_worldPacket;
}

WorldPacket const* HousingRoomSetDoorTypeResponse::Write()
{
    _worldPacket << uint32(Result);
    return &_worldPacket;
}

WorldPacket const* HousingRoomSetCeilingTypeResponse::Write()
{
    _worldPacket << uint32(Result);
    return &_worldPacket;
}

// ============================================================
// Housing Services SMSG Responses (0x54xxxx)
// ============================================================

WorldPacket const* HousingSvcsNotifyPermissionsFailure::Write()
{
    _worldPacket << uint32(Result);
    return &_worldPacket;
}

WorldPacket const* HousingSvcsGuildCreateNeighborhoodNotification::Write()
{
    _worldPacket << NeighborhoodGuid;
    return &_worldPacket;
}

WorldPacket const* HousingSvcsCreateCharterNeighborhoodResponse::Write()
{
    _worldPacket << uint32(Result);
    _worldPacket << NeighborhoodGuid;
    return &_worldPacket;
}

WorldPacket const* HousingSvcsNeighborhoodReservePlotResponse::Write()
{
    _worldPacket << uint32(Result);
    return &_worldPacket;
}

WorldPacket const* HousingSvcsClearPlotReservationResponse::Write()
{
    _worldPacket << uint32(Result);
    return &_worldPacket;
}

WorldPacket const* HousingSvcsRelinquishHouseResponse::Write()
{
    _worldPacket << uint32(Result);
    return &_worldPacket;
}

WorldPacket const* HousingSvcsCancelRelinquishHouseResponse::Write()
{
    _worldPacket << uint32(Result);
    return &_worldPacket;
}

WorldPacket const* HousingSvcsGetPlayerHousesInfoResponse::Write()
{
    _worldPacket << uint32(Result);
    _worldPacket << uint32(Houses.size());
    for (auto const& house : Houses)
    {
        _worldPacket << house.HouseGuid;
        _worldPacket << house.NeighborhoodGuid;
        _worldPacket << uint8(house.PlotIndex);
        _worldPacket << uint32(house.Level);
    }
    return &_worldPacket;
}

WorldPacket const* HousingSvcsPlayerViewHousesResponse::Write()
{
    _worldPacket << uint32(Result);
    _worldPacket << uint32(Neighborhoods.size());
    for (auto const& neighborhood : Neighborhoods)
    {
        _worldPacket << neighborhood.NeighborhoodGuid;
        _worldPacket << uint32(neighborhood.MapID);
        _worldPacket << SizedString::BitsSize<8>(neighborhood.Name);
    }
    _worldPacket.FlushBits();
    for (auto const& neighborhood : Neighborhoods)
        _worldPacket << SizedString::Data(neighborhood.Name);
    return &_worldPacket;
}

WorldPacket const* HousingSvcsChangeHouseCosmeticOwner::Write()
{
    _worldPacket << HouseGuid;
    _worldPacket << NewOwnerGuid;
    return &_worldPacket;
}

WorldPacket const* HousingSvcsUpdateHousesLevelFavor::Write()
{
    _worldPacket << HouseGuid;
    _worldPacket << uint32(Level);
    _worldPacket << uint64(Favor);
    return &_worldPacket;
}

WorldPacket const* HousingSvcsGuildAddHouseNotification::Write()
{
    _worldPacket << HouseGuid;
    return &_worldPacket;
}

WorldPacket const* HousingSvcsGuildRemoveHouseNotification::Write()
{
    _worldPacket << HouseGuid;
    return &_worldPacket;
}

WorldPacket const* HousingSvcsGuildAppendNeighborhoodNotification::Write()
{
    _worldPacket << NeighborhoodGuid;
    return &_worldPacket;
}

WorldPacket const* HousingSvcsGuildRenameNeighborhoodNotification::Write()
{
    _worldPacket << NeighborhoodGuid;
    _worldPacket << SizedString::BitsSize<7>(NewName);
    _worldPacket.FlushBits();
    _worldPacket << SizedString::Data(NewName);
    return &_worldPacket;
}

WorldPacket const* HousingSvcsGuildGetHousingInfoResponse::Write()
{
    _worldPacket << uint32(Result);
    return &_worldPacket;
}

WorldPacket const* HousingSvcsAcceptNeighborhoodOwnershipResponse::Write()
{
    _worldPacket << uint32(Result);
    return &_worldPacket;
}

WorldPacket const* HousingSvcsRejectNeighborhoodOwnershipResponse::Write()
{
    _worldPacket << uint32(Result);
    return &_worldPacket;
}

WorldPacket const* HousingSvcsNeighborhoodOwnershipTransferredResponse::Write()
{
    _worldPacket << NeighborhoodGuid;
    _worldPacket << NewOwnerGuid;
    return &_worldPacket;
}

WorldPacket const* HousingSvcsGetPotentialHouseOwnersResponse::Write()
{
    _worldPacket << uint32(Result);
    _worldPacket << uint32(PotentialOwners.size());
    for (auto const& owner : PotentialOwners)
    {
        _worldPacket << owner.PlayerGuid;
        _worldPacket << SizedString::BitsSize<7>(owner.PlayerName);
    }
    _worldPacket.FlushBits();
    for (auto const& owner : PotentialOwners)
        _worldPacket << SizedString::Data(owner.PlayerName);
    return &_worldPacket;
}

WorldPacket const* HousingSvcsUpdateHouseSettingsResponse::Write()
{
    _worldPacket << uint32(Result);
    return &_worldPacket;
}

WorldPacket const* HousingSvcsGetHouseFinderInfoResponse::Write()
{
    _worldPacket << uint32(Result);
    _worldPacket << uint32(Entries.size());
    for (auto const& entry : Entries)
    {
        _worldPacket << entry.NeighborhoodGuid;
        _worldPacket << uint32(entry.MapID);
        _worldPacket << uint32(entry.AvailablePlots);
        _worldPacket << uint32(entry.TotalPlots);
        _worldPacket << uint8(entry.SuggestionReason);
        _worldPacket << SizedString::BitsSize<8>(entry.NeighborhoodName);
    }
    _worldPacket.FlushBits();
    for (auto const& entry : Entries)
        _worldPacket << SizedString::Data(entry.NeighborhoodName);
    return &_worldPacket;
}

WorldPacket const* HousingSvcsGetHouseFinderNeighborhoodResponse::Write()
{
    _worldPacket << uint32(Result);
    _worldPacket << NeighborhoodGuid;
    _worldPacket << uint32(Plots.size());
    for (auto const& plot : Plots)
    {
        _worldPacket << uint8(plot.PlotIndex);
        _worldPacket << uint64(plot.Cost);
        _worldPacket.WriteBit(plot.IsAvailable);
        _worldPacket << SizedString::BitsSize<7>(plot.OwnerName);
    }
    _worldPacket.FlushBits();
    _worldPacket << SizedString::BitsSize<8>(NeighborhoodName);
    _worldPacket.FlushBits();
    _worldPacket << SizedString::Data(NeighborhoodName);
    for (auto const& plot : Plots)
        _worldPacket << SizedString::Data(plot.OwnerName);
    return &_worldPacket;
}

WorldPacket const* HousingSvcsGetBnetFriendNeighborhoodsResponse::Write()
{
    _worldPacket << uint32(Result);
    return &_worldPacket;
}

WorldPacket const* HousingSvcsHouseFinderForceRefresh::Write()
{
    return &_worldPacket;
}

WorldPacket const* HousingSvcRequestPlayerReloadData::Write()
{
    return &_worldPacket;
}

WorldPacket const* HousingSvcsDeleteAllNeighborhoodInvitesResponse::Write()
{
    _worldPacket << uint32(Result);
    return &_worldPacket;
}

// ============================================================
// Housing General SMSG Responses (0x55xxxx)
// ============================================================

WorldPacket const* HousingHouseStatusResponse::Write()
{
    _worldPacket << HouseGuid;
    _worldPacket << NeighborhoodGuid;
    _worldPacket << OwnerGuid;
    _worldPacket << PlotGuid;
    _worldPacket << uint8(Status);
    _worldPacket << uint8(Flags);
    return &_worldPacket;
}

WorldPacket const* HousingGetCurrentHouseInfoResponse::Write()
{
    _worldPacket << uint32(Result);
    _worldPacket << HouseGuid;
    _worldPacket << NeighborhoodGuid;
    _worldPacket << uint8(PlotIndex);
    _worldPacket << uint32(Level);
    _worldPacket << uint64(Favor);
    _worldPacket << uint32(SettingsFlags);
    _worldPacket << uint32(DecorCount);
    _worldPacket << uint32(RoomCount);
    _worldPacket << uint32(FixtureCount);
    return &_worldPacket;
}

WorldPacket const* HousingExportHouseResponse::Write()
{
    _worldPacket << uint32(Result);
    return &_worldPacket;
}

WorldPacket const* HousingGetPlayerPermissionsResponse::Write()
{
    _worldPacket << uint32(Result);
    _worldPacket << uint32(Permissions);
    return &_worldPacket;
}

WorldPacket const* HousingResetKioskModeResponse::Write()
{
    _worldPacket << uint32(Result);
    return &_worldPacket;
}

// ============================================================
// Account/Licensing SMSG (0x42xxxx / 0x5Fxxxx)
// ============================================================

WorldPacket const* AccountExteriorFixtureCollectionUpdate::Write()
{
    _worldPacket << uint32(FixtureID);
    return &_worldPacket;
}

WorldPacket const* AccountHouseTypeCollectionUpdate::Write()
{
    _worldPacket << uint32(HouseTypeID);
    return &_worldPacket;
}

WorldPacket const* InvalidateNeighborhood::Write()
{
    _worldPacket << NeighborhoodGuid;
    return &_worldPacket;
}

} // namespace WorldPackets::Housing

// ============================================================
// Neighborhood namespace (Charter, Management)
// ============================================================
namespace WorldPackets::Neighborhood
{

// --- Neighborhood Charter System ---

void NeighborhoodCharterCreate::Read()
{
    _worldPacket >> NeighborhoodMapID;
    _worldPacket >> FactionFlags;
    _worldPacket >> SizedString::BitsSize<7>(Name);

    _worldPacket >> SizedString::Data(Name);
}

void NeighborhoodCharterEdit::Read()
{
    _worldPacket >> NeighborhoodMapID;
    _worldPacket >> FactionFlags;
    _worldPacket >> SizedString::BitsSize<7>(Name);

    _worldPacket >> SizedString::Data(Name);
}

void NeighborhoodCharterAddSignature::Read()
{
    _worldPacket >> CharterGuid;
}

void NeighborhoodCharterSendSignatureRequest::Read()
{
    _worldPacket >> TargetPlayerGuid;
}

// --- Neighborhood Management System ---

void NeighborhoodUpdateName::Read()
{
    _worldPacket >> NeighborhoodGuid;
    _worldPacket >> SizedString::BitsSize<7>(NewName);

    _worldPacket >> SizedString::Data(NewName);
}

void NeighborhoodSetPublicFlag::Read()
{
    _worldPacket >> NeighborhoodGuid;
    _worldPacket >> Bits<1>(IsPublic);
}

void NeighborhoodAddSecondaryOwner::Read()
{
    _worldPacket >> NeighborhoodGuid;
    _worldPacket >> PlayerGuid;
}

void NeighborhoodRemoveSecondaryOwner::Read()
{
    _worldPacket >> NeighborhoodGuid;
    _worldPacket >> PlayerGuid;
}

void NeighborhoodInviteResident::Read()
{
    _worldPacket >> NeighborhoodGuid;
    _worldPacket >> PlayerGuid;
}

void NeighborhoodCancelInvitation::Read()
{
    _worldPacket >> NeighborhoodGuid;
    _worldPacket >> InviteeGuid;
}

void NeighborhoodPlayerDeclineInvite::Read()
{
    _worldPacket >> NeighborhoodGuid;
}

void NeighborhoodPlayerGetInvite::Read()
{
    _worldPacket >> NeighborhoodGuid;
}

void NeighborhoodGetInvites::Read()
{
    _worldPacket >> NeighborhoodGuid;
}

void NeighborhoodBuyHouse::Read()
{
    _worldPacket >> NeighborhoodGuid;
    _worldPacket >> PlotIndex;
}

void NeighborhoodMoveHouse::Read()
{
    _worldPacket >> SourcePlotGuid;
    _worldPacket >> NewPlotIndex;
}

void NeighborhoodOpenCornerstoneUI::Read()
{
    _worldPacket >> CornerstoneGuid;
}

void NeighborhoodOfferOwnership::Read()
{
    _worldPacket >> NeighborhoodGuid;
    _worldPacket >> NewOwnerGuid;
}

void NeighborhoodGetRoster::Read()
{
    _worldPacket >> NeighborhoodGuid;
}

void NeighborhoodEvictPlot::Read()
{
    _worldPacket >> NeighborhoodGuid;
    _worldPacket >> PlotGuid;
}

// ============================================================
// Neighborhood Charter SMSG Responses (0x5Bxxxx)
// ============================================================

WorldPacket const* NeighborhoodCharterUpdateResponse::Write()
{
    _worldPacket << uint32(Result);
    return &_worldPacket;
}

WorldPacket const* NeighborhoodCharterOpenUIResponse::Write()
{
    _worldPacket << uint32(Result);
    return &_worldPacket;
}

WorldPacket const* NeighborhoodCharterSignRequest::Write()
{
    _worldPacket << CharterGuid;
    _worldPacket << RequesterGuid;
    return &_worldPacket;
}

WorldPacket const* NeighborhoodCharterAddSignatureResponse::Write()
{
    _worldPacket << uint32(Result);
    return &_worldPacket;
}

WorldPacket const* NeighborhoodCharterOpenConfirmationUIResponse::Write()
{
    _worldPacket << uint32(Result);
    return &_worldPacket;
}

WorldPacket const* NeighborhoodCharterSignatureRemovedNotification::Write()
{
    _worldPacket << CharterGuid;
    _worldPacket << SignerGuid;
    return &_worldPacket;
}

// ============================================================
// Neighborhood Management SMSG Responses (0x5Cxxxx)
// ============================================================

WorldPacket const* NeighborhoodPlayerEnterPlot::Write()
{
    _worldPacket << PlayerGuid;
    _worldPacket << PlotGuid;
    return &_worldPacket;
}

WorldPacket const* NeighborhoodPlayerLeavePlot::Write()
{
    _worldPacket << PlayerGuid;
    _worldPacket << PlotGuid;
    return &_worldPacket;
}

WorldPacket const* NeighborhoodEvictPlayerResponse::Write()
{
    _worldPacket << PlayerGuid;
    return &_worldPacket;
}

WorldPacket const* NeighborhoodUpdateNameResponse::Write()
{
    _worldPacket << uint32(Result);
    return &_worldPacket;
}

WorldPacket const* NeighborhoodUpdateNameNotification::Write()
{
    _worldPacket << NeighborhoodGuid;
    _worldPacket << SizedString::BitsSize<7>(NewName);
    _worldPacket.FlushBits();
    _worldPacket << SizedString::Data(NewName);
    return &_worldPacket;
}

WorldPacket const* NeighborhoodAddSecondaryOwnerResponse::Write()
{
    _worldPacket << uint32(Result);
    return &_worldPacket;
}

WorldPacket const* NeighborhoodRemoveSecondaryOwnerResponse::Write()
{
    _worldPacket << uint32(Result);
    return &_worldPacket;
}

WorldPacket const* NeighborhoodBuyHouseResponse::Write()
{
    _worldPacket << uint32(Result);
    return &_worldPacket;
}

WorldPacket const* NeighborhoodMoveHouseResponse::Write()
{
    _worldPacket << uint32(Result);
    return &_worldPacket;
}

WorldPacket const* NeighborhoodOpenCornerstoneUIResponse::Write()
{
    _worldPacket << uint32(Result);
    return &_worldPacket;
}

WorldPacket const* NeighborhoodInviteResidentResponse::Write()
{
    _worldPacket << uint32(Result);
    return &_worldPacket;
}

WorldPacket const* NeighborhoodCancelInvitationResponse::Write()
{
    _worldPacket << uint32(Result);
    return &_worldPacket;
}

WorldPacket const* NeighborhoodDeclineInvitationResponse::Write()
{
    _worldPacket << uint32(Result);
    return &_worldPacket;
}

WorldPacket const* NeighborhoodPlayerGetInviteResponse::Write()
{
    _worldPacket << uint32(Result);
    return &_worldPacket;
}

WorldPacket const* NeighborhoodGetInvitesResponse::Write()
{
    _worldPacket << uint32(Result);
    _worldPacket << uint32(Invites.size());
    for (auto const& invite : Invites)
    {
        _worldPacket << invite.InviteeGuid;
        _worldPacket << invite.InviterGuid;
        _worldPacket << uint32(invite.InviteTime);
    }
    return &_worldPacket;
}

WorldPacket const* NeighborhoodInviteNotification::Write()
{
    _worldPacket << NeighborhoodGuid;
    _worldPacket << InviterGuid;
    _worldPacket << SizedString::BitsSize<7>(NeighborhoodName);
    _worldPacket.FlushBits();
    _worldPacket << SizedString::Data(NeighborhoodName);
    return &_worldPacket;
}

WorldPacket const* NeighborhoodOfferOwnershipResponse::Write()
{
    _worldPacket << uint32(Result);
    return &_worldPacket;
}

WorldPacket const* NeighborhoodGetRosterResponse::Write()
{
    _worldPacket << uint32(Result);
    _worldPacket << uint32(Members.size());
    for (auto const& member : Members)
    {
        _worldPacket << member.PlayerGuid;
        _worldPacket << uint8(member.Role);
        _worldPacket << uint8(member.PlotIndex);
        _worldPacket << uint32(member.JoinTime);
    }
    return &_worldPacket;
}

WorldPacket const* NeighborhoodRosterResidentUpdate::Write()
{
    _worldPacket << PlayerGuid;
    _worldPacket << NeighborhoodGuid;
    return &_worldPacket;
}

WorldPacket const* NeighborhoodInviteNameLookupResult::Write()
{
    _worldPacket << uint32(Result);
    _worldPacket << PlayerGuid;
    _worldPacket << SizedString::BitsSize<7>(PlayerName);
    _worldPacket.FlushBits();
    _worldPacket << SizedString::Data(PlayerName);
    return &_worldPacket;
}

WorldPacket const* NeighborhoodEvictPlotResponse::Write()
{
    _worldPacket << uint32(Result);
    return &_worldPacket;
}

WorldPacket const* NeighborhoodEvictPlotNotice::Write()
{
    _worldPacket << NeighborhoodGuid;
    _worldPacket << PlotGuid;
    return &_worldPacket;
}

} // namespace WorldPackets::Neighborhood
