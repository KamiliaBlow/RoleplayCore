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

// --- Decor System ---

void HousingDecorSetEditMode::Read()
{
    _worldPacket >> Bits<1>(Active);
}

void HousingDecorPlace::Read()
{
    _worldPacket >> HouseGuid;
    _worldPacket >> DecorEntryID;
    _worldPacket >> Pos;
    _worldPacket >> RotationX;
    _worldPacket >> RotationY;
    _worldPacket >> RotationZ;
    _worldPacket >> RotationW;
}

void HousingDecorMove::Read()
{
    _worldPacket >> HouseGuid;
    _worldPacket >> DecorGuid;
    _worldPacket >> Pos;
    _worldPacket >> RotationX;
    _worldPacket >> RotationY;
    _worldPacket >> RotationZ;
    _worldPacket >> RotationW;
}

void HousingDecorRemove::Read()
{
    _worldPacket >> HouseGuid;
    _worldPacket >> DecorGuid;
}

void HousingDecorLock::Read()
{
    _worldPacket >> HouseGuid;
    _worldPacket >> DecorGuid;
}

void HousingDecorSetDyeSlots::Read()
{
    _worldPacket >> HouseGuid;
    _worldPacket >> DecorGuid;
    _worldPacket >> Size<uint32>(DyeSlots);
    for (uint32& dyeSlot : DyeSlots)
        _worldPacket >> dyeSlot;
}

void HousingDecorDeleteFromStorage::Read()
{
    _worldPacket >> HouseGuid;
    _worldPacket >> CatalogEntryID;
}

void HousingDecorDeleteFromStorageById::Read()
{
    _worldPacket >> HouseGuid;
    _worldPacket >> CatalogEntryID;
}

void HousingDecorRequestStorage::Read()
{
    _worldPacket >> HouseGuid;
}

void HousingDecorRedeemDeferredDecor::Read()
{
    _worldPacket >> HouseGuid;
    _worldPacket >> CatalogEntryID;
}

// --- Fixture System ---

void HousingFixtureSetEditMode::Read()
{
    _worldPacket >> Bits<1>(Active);
}

void HousingFixtureSetCoreFixture::Read()
{
    _worldPacket >> HouseGuid;
    _worldPacket >> FixturePointID;
    _worldPacket >> OptionID;
}

void HousingFixtureCreateFixture::Read()
{
    _worldPacket >> HouseGuid;
    _worldPacket >> FixturePointID;
    _worldPacket >> OptionID;
}

void HousingFixtureDeleteFixture::Read()
{
    _worldPacket >> HouseGuid;
    _worldPacket >> FixturePointID;
}

// --- Room System ---

void HousingRoomSetLayoutEditMode::Read()
{
    _worldPacket >> Bits<1>(Active);
}

void HousingRoomAdd::Read()
{
    _worldPacket >> HouseGuid;
    _worldPacket >> RoomID;
    _worldPacket >> SlotIndex;
    _worldPacket >> Orientation;
    _worldPacket >> Bits<1>(Mirrored);
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
    _worldPacket >> NewSlotIndex;
    _worldPacket >> SwapRoomGuid;
    _worldPacket >> SwapSlotIndex;
}

void HousingRoomSetComponentTheme::Read()
{
    _worldPacket >> RoomGuid;
    _worldPacket >> ThemeSetID;
    _worldPacket >> Size<uint32>(ComponentIDs);
    for (uint32& componentID : ComponentIDs)
        _worldPacket >> componentID;
}

void HousingRoomApplyComponentMaterials::Read()
{
    _worldPacket >> RoomGuid;
    _worldPacket >> WallpaperID;
    _worldPacket >> MaterialID;
    _worldPacket >> Size<uint32>(ComponentIDs);
    for (uint32& componentID : ComponentIDs)
        _worldPacket >> componentID;
}

void HousingRoomSetDoorType::Read()
{
    _worldPacket >> RoomGuid;
    _worldPacket >> DoorTypeID;
    _worldPacket >> DoorSlot;
}

void HousingRoomSetCeilingType::Read()
{
    _worldPacket >> RoomGuid;
    _worldPacket >> CeilingTypeID;
    _worldPacket >> CeilingSlot;
}

// --- Housing Services System ---

void HousingSvcsGuildCreateNeighborhood::Read()
{
    _worldPacket >> NeighborhoodMapID;
    _worldPacket >> FactionID;
    _worldPacket >> SizedString::BitsSize<7>(Name);

    _worldPacket >> SizedString::Data(Name);
}

void HousingSvcsNeighborhoodReservePlot::Read()
{
    _worldPacket >> NeighborhoodGuid;
    _worldPacket >> PlotIndex;
    _worldPacket >> Bits<1>(HasPreference);
}

void HousingSvcsRelinquishHouse::Read()
{
    _worldPacket >> HouseGuid;
}

void HousingSvcsUpdateHouseSettings::Read()
{
    _worldPacket >> HouseGuid;
    _worldPacket >> OptionalInit(SettingID);
    _worldPacket >> OptionalInit(TargetGuid);

    if (SettingID)
        _worldPacket >> *SettingID;

    if (TargetGuid)
        _worldPacket >> *TargetGuid;
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
    _worldPacket >> PlotGuid;
    _worldPacket >> PlotIndex;
    _worldPacket >> Flags;
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

void HousingGetCurrentHouseInfo::Read()
{
    _worldPacket >> HouseGuid;
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

WorldPacket const* AccountHousingRoomAdded::Write()
{
    _worldPacket << uint32(RoomID);

    return &_worldPacket;
}

WorldPacket const* AccountHousingFixtureAdded::Write()
{
    _worldPacket << uint32(FixtureID);

    return &_worldPacket;
}

WorldPacket const* AccountHousingThemeAdded::Write()
{
    _worldPacket << uint32(ThemeID);

    return &_worldPacket;
}

WorldPacket const* AccountHousingRoomComponentTextureAdded::Write()
{
    _worldPacket << uint32(TextureID);

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
    return &_worldPacket;
}

WorldPacket const* HousingSvcsPlayerViewHousesResponse::Write()
{
    _worldPacket << uint32(Result);
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
    return &_worldPacket;
}

WorldPacket const* HousingSvcsGetHouseFinderNeighborhoodResponse::Write()
{
    _worldPacket << uint32(Result);
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
    _worldPacket << uint32(Result);
    return &_worldPacket;
}

WorldPacket const* HousingGetCurrentHouseInfoResponse::Write()
{
    _worldPacket << uint32(Result);
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
