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

} // namespace WorldPackets::Neighborhood
