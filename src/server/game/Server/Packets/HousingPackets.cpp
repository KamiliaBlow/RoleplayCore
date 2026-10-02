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
#include "Log.h"
#include "PacketOperators.h"

namespace WorldPackets::Housing
{
void HouseExteriorCommitPosition::Read()
{
    // Position and yaw are relative to the plot room.
    _worldPacket >> HouseGuid;
    _worldPacket >> AccountGuid;
    _worldPacket >> PositionX;
    _worldPacket >> PositionY;
    _worldPacket >> PositionZ;
    _worldPacket >> Facing;
}

void HousingDecorSetEditMode::Read()
{
    _worldPacket >> Bits<1>(Active);
}

void HousingPhotoSharingCompleteAuthorization::Read()
{
    _worldPacket >> SizedString::BitsSize<6>(Token);
    _worldPacket.ResetBitPos();
    _worldPacket >> SizedString::Data(Token);
}

void HousingDecorPlace::Read()
{
    _worldPacket >> DecorGuid;
    _worldPacket >> Position;
    _worldPacket >> Rotation;
    for (float& component : Quaternion)
        _worldPacket >> component;
    _worldPacket >> Scale;
    _worldPacket >> AttachParentGuid;
    _worldPacket >> RoomGuid;
    _worldPacket >> AnchorMeshObjectGuid;
    _worldPacket >> AttachPoint;
}

void HousingDecorMove::Read()
{
    _worldPacket >> DecorGuid;
    _worldPacket >> Position;
    _worldPacket >> Rotation;
    for (float& component : Quaternion)
        _worldPacket >> component;
    _worldPacket >> Scale;
    _worldPacket >> AttachParentGuid;
    _worldPacket >> RoomGuid;
    _worldPacket >> Field_70;
    _worldPacket >> Field_80;
    _worldPacket >> Field_85;
    _worldPacket >> Field_86;
    _worldPacket >> Bits<1>(IsBasicMove);
}

void HousingDecorRemove::Read()
{
    _worldPacket >> DecorGuid;
}

void HousingDecorLock::Read()
{
    _worldPacket >> DecorGuid;
    _worldPacket >> Bits<1>(Locked);
    _worldPacket >> Bits<1>(Field_49);
}

void HousingDecorSetPet::Read()
{
    _worldPacket >> DecorGuid;
    _worldPacket >> PetGuid;
    _worldPacket >> Flag;
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
    _worldPacket >> Bits<5>(count);
    DecorGuids.resize(count);
    for (ObjectGuid& guid : DecorGuids)
        _worldPacket >> guid;
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

void HousingFixtureSetEditMode::Read()
{
    _worldPacket >> Bits<1>(Active);
}

void HousingFixtureSetCoreFixture::Read()
{
    _worldPacket >> FixtureGuid;
    _worldPacket >> ExteriorComponentID;
    _worldPacket >> Flags;
}

void HousingFixtureCreateFixture::Read()
{
    _worldPacket >> AttachParentGuid;
    _worldPacket >> HookEntityGuid;
    _worldPacket >> ExteriorComponentHookID;
    _worldPacket >> ExteriorComponentID;
    _worldPacket >> Flags;
}

void HousingFixtureDeleteFixture::Read()
{
    _worldPacket >> FixtureGuid;
    _worldPacket >> RoomGuid;
    _worldPacket >> ExteriorComponentID;
    _worldPacket >> Flags;
}

void HousingFixtureSetHouseSize::Read()
{
    _worldPacket >> HouseGuid;
    _worldPacket >> Size;
    _worldPacket >> Flags;
}

void HousingFixtureSetHouseType::Read()
{
    _worldPacket >> HouseGuid;
    _worldPacket >> HouseExteriorWmoDataID;
    _worldPacket >> Flags;
}

void HouseExteriorLock::Read()
{
    _worldPacket >> HouseGuid;
    _worldPacket >> PlotGuid;
    _worldPacket >> NeighborhoodGuid;
    _worldPacket >> Bits<1>(Locked);
}

void HousingRoomSetLayoutEditMode::Read()
{
    _worldPacket >> Bits<1>(Active);
}

void HousingRoomAdd::Read()
{
    _worldPacket >> SourceRoomGuid;
    _worldPacket >> TargetDoorComponentID;
    _worldPacket >> HouseRoomID;
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
    _worldPacket >> BoundedSize<uint32>(OptionIDs);
    _worldPacket >> HouseThemeID;
    for (uint32& optionID : OptionIDs)
        _worldPacket >> optionID;
}

void HousingRoomApplyComponentMaterials::Read()
{
    _worldPacket >> RoomGuid;
    _worldPacket >> BoundedSize<uint32>(OptionIDs);
    _worldPacket >> ColorOverride;
    _worldPacket >> RoomComponentTextureID;
    _worldPacket >> ComponentSlot;
    for (uint32& optionID : OptionIDs)
        _worldPacket >> optionID;
}

void HousingRoomSetDoorType::Read()
{
    _worldPacket >> RoomGuid;
    _worldPacket >> ThemeOptionID;
    _worldPacket >> DoorType;
}

void HousingRoomSetCeilingType::Read()
{
    _worldPacket >> RoomGuid;
    _worldPacket >> ThemeOptionID;
    _worldPacket >> CeilingType;
}

void HousingSvcsGuildCreateNeighborhood::Read()
{
    _worldPacket >> NeighborhoodTypeID;
    _worldPacket >> SecondaryID;
    _worldPacket >> SizedCString::BitsSize<8>(NeighborhoodName);
    _worldPacket >> SizedCString::Data(NeighborhoodName);
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
    _worldPacket >> OptionalInit(NewOwnerGuid);

    if (PlotSettingsID)
        _worldPacket >> *PlotSettingsID;

    if (NewOwnerGuid)
        _worldPacket >> *NewOwnerGuid;
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

void HousingSvcsHouseFinderIgnoreNeighborhood::Read()
{
    _worldPacket >> NeighborhoodGuid;
}

WorldPacket const* HousingSvcsIgnoreNeighborhoodInviteResponse::Write()
{
    _worldPacket.WriteBit(Success);
    _worldPacket.FlushBits();
    _worldPacket << NeighborhoodGuid;

    return &_worldPacket;
}

void HousingSvcsGetBnetFriendNeighborhoods::Read()
{
    _worldPacket >> BnetAccountGuid;
}

void HousingGetPlayerPermissions::Read()
{
    _worldPacket >> OptionalInit(HouseGuid);

    if (HouseGuid)
        _worldPacket >> *HouseGuid;
}

void HousingSvcsGetPotentialHouseOwners::Read()
{
    _worldPacket >> HouseGuid;
}

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
    _worldPacket >> SizedString::BitsSize<6>(PlayerName);
    _worldPacket >> SizedString::Data(PlayerName);
}

void GuildGetOthersOwnedHouses::Read()
{
    _worldPacket >> PlayerGuid;
}

WorldPacket const* QueryNeighborhoodNameResponse::Write()
{
    _worldPacket << NeighborhoodGuid;
    _worldPacket << uint8(Result ? 0x80 : 0x00);
    if (Result)
    {
        _worldPacket << SizedString::BitsSize<8>(NeighborhoodName);
        _worldPacket << SizedString::Data(NeighborhoodName);
    }

    return &_worldPacket;
}

WorldPacket const* InvalidateNeighborhoodName::Write()
{
    _worldPacket << NeighborhoodGuid;

    return &_worldPacket;
}

WorldPacket const* HouseExteriorLockResponse::Write()
{
    _worldPacket << FixtureEntityGuid;
    _worldPacket << EditorPlayerGuid;
    _worldPacket << uint8(Result);
    _worldPacket.WriteBit(Active);
    _worldPacket.FlushBits();

    return &_worldPacket;
}

WorldPacket const* HouseExteriorSetHousePositionResponse::Write()
{
    _worldPacket << uint8(Result);
    _worldPacket << HouseGuid;

    return &_worldPacket;
}

WorldPacket const* LastCatalogFetchResponse::Write()
{
    _worldPacket << uint64(Timestamp);

    return &_worldPacket;
}

WorldPacket const* HousingDecorSetEditModeResponse::Write()
{
    _worldPacket << HouseGuid;
    _worldPacket << BNetAccountGuid;
    _worldPacket << uint32(AllowedEditor.size());
    _worldPacket << uint8(Result);

    for (ObjectGuid const& guid : AllowedEditor)
        _worldPacket << guid;

    return &_worldPacket;
}

WorldPacket const* HousingDecorMoveResponse::Write()
{
    _worldPacket << PlayerGuid;
    _worldPacket << uint32(Field_09);
    _worldPacket << DecorGuid;
    _worldPacket << uint8(Result);
    _worldPacket << uint8(Field_26 ? 0x80 : 0x00);

    return &_worldPacket;
}

WorldPacket const* HousingDecorPlaceResponse::Write()
{
    _worldPacket << PlayerGuid;
    _worldPacket << uint32(Field_09);
    _worldPacket << DecorGuid;
    _worldPacket << uint8(Result);

    return &_worldPacket;
}

WorldPacket const* HousingDecorRemoveResponse::Write()
{
    _worldPacket << DecorGuid;
    _worldPacket << UnkGUID;
    _worldPacket << uint32(Field_13);
    _worldPacket << uint8(Result);

    return &_worldPacket;
}

WorldPacket const* HousingDecorLockResponse::Write()
{
    _worldPacket << DecorGuid;
    _worldPacket << PlayerGuid;
    _worldPacket << uint32(Field_16);
    _worldPacket << uint8(Result);
    uint8 flags = 0;
    if (Locked)
        flags |= 0x80;
    if (Field_17)
        flags |= 0x40;
    _worldPacket << uint8(flags);

    return &_worldPacket;
}

WorldPacket const* HousingDecorDeleteFromStorageResponse::Write()
{
    _worldPacket << uint8(Result);

    return &_worldPacket;
}

WorldPacket const* HousingDecorRequestStorageResponse::Write()
{
    _worldPacket << BNetAccountGuid;
    _worldPacket << uint8(ResultCode);
    _worldPacket << uint8(Flags);

    return &_worldPacket;
}

WorldPacket const* HousingDecorAddToHouseChestResponse::Write()
{
    _worldPacket << uint8(Success ? 0x80 : 0x00);
    _worldPacket << uint32(DecorGuids.size());
    for (ObjectGuid const& guid : DecorGuids)
        _worldPacket << guid;

    return &_worldPacket;
}

WorldPacket const* HousingDecorSystemSetDyeSlotsResponse::Write()
{
    _worldPacket << DecorGuid;
    _worldPacket << uint8(Result);

    return &_worldPacket;
}

WorldPacket const* HousingRedeemDeferredDecorResponse::Write()
{
    _worldPacket << DecorGuid;
    _worldPacket << uint8(Result);
    _worldPacket << uint32(SequenceIndex);

    return &_worldPacket;
}

WorldPacket const* HousingFirstTimeDecorAcquisition::Write()
{
    _worldPacket << uint32(DecorEntryID);

    return &_worldPacket;
}

WorldPacket const* HousingFixtureSetEditModeResponse::Write()
{
    _worldPacket << HouseGuid;
    _worldPacket << EditorPlayerGuid;
    _worldPacket << uint8(Result);

    return &_worldPacket;
}

WorldPacket const* HousingFixtureCreateBasicHouseResponse::Write()
{
    _worldPacket << uint8(Result);

    return &_worldPacket;
}

WorldPacket const* HousingFixtureSetHouseSizeResponse::Write()
{
    _worldPacket << uint8(Result);
    _worldPacket << uint8(Size);

    return &_worldPacket;
}

WorldPacket const* HousingFixtureSetHouseTypeResponse::Write()
{
    _worldPacket << uint8(Result);
    _worldPacket << uint32(HouseExteriorTypeID);
    _worldPacket << uint8(ExtraField);

    return &_worldPacket;
}

WorldPacket const* HousingFixtureSetCoreFixtureResponse::Write()
{
    _worldPacket << uint8(Result);

    return &_worldPacket;
}

WorldPacket const* HousingFixtureCreateFixtureResponse::Write()
{
    _worldPacket << FixtureGuid;
    _worldPacket << uint8(Result);

    return &_worldPacket;
}

WorldPacket const* HousingFixtureDeleteFixtureResponse::Write()
{
    _worldPacket << FixtureGuid;
    _worldPacket << uint8(Result);

    return &_worldPacket;
}

WorldPacket const* HousingRoomSetLayoutEditModeResponse::Write()
{
    _worldPacket << PlayerGuid;
    _worldPacket << uint8(Result);
    _worldPacket << uint8(Active ? 0x80 : 0x00);

    return &_worldPacket;
}

WorldPacket const* HousingRoomAddResponse::Write()
{
    _worldPacket << PlayerGuid;
    _worldPacket << uint8(Result);

    return &_worldPacket;
}

WorldPacket const* HousingRoomRemoveResponse::Write()
{
    _worldPacket << RoomGuid;
    _worldPacket << PlayerGuid;
    _worldPacket << uint8(Result);

    return &_worldPacket;
}

WorldPacket const* HousingRoomUpdateResponse::Write()
{
    _worldPacket << RoomGuid;
    _worldPacket << uint8(Result);

    return &_worldPacket;
}

WorldPacket const* HousingRoomSetComponentThemeResponse::Write()
{
    _worldPacket << RoomGuid;
    _worldPacket << uint32(OptionIDs.size());
    _worldPacket << uint32(ThemeSetID);
    _worldPacket << uint8(Result);
    for (uint32 optionID : OptionIDs)
        _worldPacket << optionID;

    return &_worldPacket;
}

WorldPacket const* HousingRoomApplyComponentMaterialsResponse::Write()
{
    // ColorOverride is NOT echoed in the response — only TextureID.
    _worldPacket << RoomGuid;
    _worldPacket << uint32(OptionIDs.size());
    _worldPacket << uint32(RoomComponentTextureID);
    _worldPacket << uint8(Result);
    for (uint32 optionID : OptionIDs)
        _worldPacket << optionID;

    return &_worldPacket;
}

WorldPacket const* HousingRoomSetDoorTypeResponse::Write()
{
    _worldPacket << RoomGuid;
    _worldPacket << uint32(ComponentID);
    _worldPacket << uint8(DoorType);
    _worldPacket << uint8(Result);

    return &_worldPacket;
}

WorldPacket const* HousingRoomSetCeilingTypeResponse::Write()
{
    _worldPacket << RoomGuid;
    _worldPacket << uint32(ComponentID);
    _worldPacket << uint8(CeilingType);
    _worldPacket << uint8(Result);

    return &_worldPacket;
}

// Forward declarations for static helpers (defined below)
static void WriteJamCliHouse(WorldPacket& packet, JamCliHouse const& house);
static void WriteJamCliHouseFinderNeighborhoodBase(WorldPacket& packet, JamCliHouseFinderNeighborhood const& entry);
static void WriteJamCliHouseFinderNeighborhood(WorldPacket& packet, JamCliHouseFinderNeighborhood const& entry);

WorldPacket const* HousingSvcsNotifyPermissionsFailure::Write()
{
    _worldPacket << uint8(FailureType);
    _worldPacket << uint8(ErrorCode);

    return &_worldPacket;
}

WorldPacket const* HousingSvcsGuildCreateNeighborhoodNotification::Write()
{
    _worldPacket << NeighborhoodGuid;
    _worldPacket << uint8(Flag);
    uint8 nameLen = static_cast<uint8>(std::min<size_t>(Name.size() + 1, 255));
    _worldPacket << uint8(nameLen);
    if (nameLen > 0)
        _worldPacket.append(Name.c_str(), nameLen);

    return &_worldPacket;
}

WorldPacket const* HousingSvcsCreateCharterNeighborhoodResponse::Write()
{
    WriteJamCliHouseFinderNeighborhoodBase(_worldPacket, Neighborhood);
    _worldPacket << uint8(TrailingResult);

    return &_worldPacket;
}

WorldPacket const* HousingSvcsNeighborhoodReservePlotResponse::Write()
{
    _worldPacket << uint8(Result);

    return &_worldPacket;
}

WorldPacket const* HousingSvcsRelinquishHouseResponse::Write()
{
    _worldPacket << uint8(Result);
    _worldPacket << HouseGuid;
    _worldPacket << NeighborhoodGuid;

    return &_worldPacket;
}

WorldPacket const* HousingSvcsCancelRelinquishHouseResponse::Write()
{
    _worldPacket << uint32(Field1);
    _worldPacket << HouseGuid;
    _worldPacket << uint8(Result);

    return &_worldPacket;
}

ByteBuffer& operator<<(ByteBuffer& data, HouseInfo const& houseInfo)
{
    data << houseInfo.HouseGuid;
    data << houseInfo.OwnerGuid;
    data << houseInfo.NeighborhoodGuid;
    data << houseInfo.PlotId;
    data << houseInfo.AccessFlags;
    data << uint8(houseInfo.HasMoveOutTime ? 0x80 : 0x00);
    if (houseInfo.HasMoveOutTime)
        data << uint64(houseInfo.MoveOutTime);

    return data;
}

static void WriteJamCliHouse(WorldPacket& packet, JamCliHouse const& house)
{
    packet << house.HouseGUID;
    packet << house.OwnerGUID;
    packet << house.NeighborhoodGUID;
    packet << uint8(house.PlotIndex);
    packet << uint32(house.HouseSettingFlags);
    packet << uint8(house.HasOptionalField ? 0x80 : 0x00);
    if (house.HasOptionalField)
        packet << uint64(house.OptionalValue);
}

// Name length counts the NUL terminator.
static void WriteJamCliHouseFinderNeighborhoodBase(WorldPacket& packet, JamCliHouseFinderNeighborhood const& entry)
{
    packet << entry.NeighborhoodGUID;
    packet << entry.OwnerGUID;
    packet << uint64(entry.Field1);
    packet << uint64(entry.Field2);
    packet << uint32(entry.Houses.size());
    for (auto const& house : entry.Houses)
        WriteJamCliHouse(packet, house);
    uint8 nameLen = static_cast<uint8>(std::min<size_t>(entry.Name.size() + 1, 255));
    packet << uint8(nameLen);
    packet << uint8(entry.BoolFlag ? 0x80 : 0x00);
    if (nameLen > 0)
        packet.append(entry.Name.c_str(), nameLen);
}

static void WriteJamCliHouseFinderNeighborhood(WorldPacket& packet, JamCliHouseFinderNeighborhood const& entry)
{
    WriteJamCliHouseFinderNeighborhoodBase(packet, entry);
    packet << uint64(entry.ExtraField);
    packet << uint8(entry.ExtraFlags);
}

WorldPacket const* HousingSvcsGetPlayerHousesInfoResponse::Write()
{
    _worldPacket << uint32(Houses.size());
    _worldPacket << uint8(Result);
    for (auto const& house : Houses)
        WriteJamCliHouse(_worldPacket, house);

    return &_worldPacket;
}

WorldPacket const* HousingSvcsPlayerViewHousesResponse::Write()
{
    _worldPacket << uint32(Houses.size());
    _worldPacket << uint8(Result);
    for (auto const& house : Houses)
        WriteJamCliHouse(_worldPacket, house);

    return &_worldPacket;
}

WorldPacket const* HousingSvcsChangeHouseCosmeticOwner::Write()
{
    _worldPacket << uint8(Result);
    _worldPacket << HouseGuid;
    _worldPacket << NewOwnerGuid;

    return &_worldPacket;
}

WorldPacket const* HousingSvcsUpdateHousesLevelFavor::Write()
{
    _worldPacket << uint8(Result);
    _worldPacket << uint32(ChangeAmount);
    _worldPacket << uint32(Reason);
    _worldPacket << uint32(Houses.size());
    for (HouseLevelFavor const& house : Houses)
    {
        _worldPacket << house.BnetAccount;
        _worldPacket << house.NeighborhoodGUID;
        _worldPacket << house.HouseGUID;
        _worldPacket << int32(house.HouseLevel);         // low dword of NewFavorTotal
        _worldPacket << int32(house.FavorValue);         // high dword of NewFavorTotal
        _worldPacket << uint8(house.UpdateSource);
        _worldPacket << uint32(house.SourceDataDecorID);
        _worldPacket << uint8(house.IsAdditive ? 0x80 : 0x00);
    }

    return &_worldPacket;
}

WorldPacket const* HousingSvcsGuildAddHouseNotification::Write()
{
    WriteJamCliHouse(_worldPacket, House);

    return &_worldPacket;
}

WorldPacket const* HousingSvcsGuildRemoveHouseNotification::Write()
{
    WriteJamCliHouse(_worldPacket, House);

    return &_worldPacket;
}

WorldPacket const* HousingSvcsGuildRenameNeighborhoodNotification::Write()
{
    uint8 nameLen = static_cast<uint8>(std::min<size_t>(NewName.size() + 1, 255));
    _worldPacket << uint8(nameLen);
    if (nameLen > 0)
        _worldPacket.append(NewName.c_str(), nameLen);

    return &_worldPacket;
}

WorldPacket const* HousingSvcsGuildGetHousingInfoResponse::Write()
{
    _worldPacket << uint32(Neighborhoods.size());
    for (auto const& entry : Neighborhoods)
        WriteJamCliHouseFinderNeighborhoodBase(_worldPacket, entry);

    return &_worldPacket;
}

WorldPacket const* HousingSvcsAcceptNeighborhoodOwnershipResponse::Write()
{
    // Non-zero shows ERR_HOUSING_RESULT_GENERIC_FAILURE client-side.
    _worldPacket << uint8(Result);

    return &_worldPacket;
}

WorldPacket const* HousingSvcsRejectNeighborhoodOwnershipResponse::Write()
{
    _worldPacket << uint8(Result);

    return &_worldPacket;
}

WorldPacket const* HousingSvcsNeighborhoodOwnershipTransferredResponse::Write()
{
    if (Result == 0)
    {
        constexpr uint8 blobSize = 3 * ObjectGuid::BytesSize + 1;
        _worldPacket << uint8((blobSize << 2) | (Result & 0x03));
        for (ObjectGuid const& guid : { OwnerGUID, HouseGUID, AccountGUID })
        {
            auto rawValue = guid.GetRawValue();
            _worldPacket.append(rawValue.data(), rawValue.size());
        }
        _worldPacket << uint8(HouseLevel);
    }
    else
        _worldPacket << uint8(Result & 0x03);

    return &_worldPacket;
}

WorldPacket const* HousingSvcsGetPotentialHouseOwnersResponse::Write()
{
    _worldPacket << uint32(PotentialOwners.size());
    for (auto const& owner : PotentialOwners)
    {
        _worldPacket << owner.PlayerGuid;
        _worldPacket << uint32(owner.ClassID);
        _worldPacket << uint8(owner.Error);
        uint32 nameLen = static_cast<uint32>(owner.CharacterName.size());
        _worldPacket << uint8(nameLen >> 1);
        _worldPacket << uint8((nameLen & 1) << 7);
        if (nameLen > 0)
            _worldPacket.append(owner.CharacterName.c_str(), nameLen);
    }

    return &_worldPacket;
}

WorldPacket const* HousingSvcsUpdateHouseSettingsResponse::Write()
{
    JamCliHouse house = House;
    house.HouseSettingFlags = SettingsFlags;
    _worldPacket << uint8(Result);
    WriteJamCliHouse(_worldPacket, house);

    return &_worldPacket;
}

WorldPacket const* HousingSvcsGetHouseFinderInfoResponse::Write()
{
    _worldPacket.WriteBit(Result != 0);
    _worldPacket.FlushBits();
    _worldPacket << uint32(Entries.size());
    for (auto const& entry : Entries)
        WriteJamCliHouseFinderNeighborhood(_worldPacket, entry);

    return &_worldPacket;
}

WorldPacket const* HousingSvcsGetHouseFinderNeighborhoodResponse::Write()
{
    _worldPacket.WriteBit(Result != 0);
    _worldPacket.FlushBits();
    WriteJamCliHouseFinderNeighborhood(_worldPacket, Neighborhood);

    return &_worldPacket;
}

WorldPacket const* HousingSvcsGetBnetFriendNeighborhoodsResponse::Write()
{
    _worldPacket << uint8(Result);
    _worldPacket << uint32(Entries.size());
    for (auto const& entry : Entries)
        WriteJamCliHouseFinderNeighborhood(_worldPacket, entry);

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
    _worldPacket << uint8(Result);

    return &_worldPacket;
}

static void WriteInviteEntry(WorldPacket& packet, InviteEntry const& entry)
{
    packet << uint64(entry.Timestamp);
    packet << entry.PlayerGuid;
    packet << entry.HouseGuid;
    packet << uint64(entry.ExtraData);
}

WorldPacket const* HousingHouseStatusResponse::Write()
{
    _worldPacket << HouseGuid;
    _worldPacket << AccountGuid;
    _worldPacket << OwnerPlayerGuid;
    _worldPacket << LockedDecorGuid;
    _worldPacket << uint8(Status);
    _worldPacket << uint8(EditModeFlags);

    return &_worldPacket;
}

WorldPacket const* HousingGetCurrentHouseInfoResponse::Write()
{
    WriteJamCliHouse(_worldPacket, House);
    _worldPacket << uint8(Result);

    return &_worldPacket;
}

WorldPacket const* HousingGetPlayerPermissionsResponse::Write()
{
    _worldPacket << HouseGuid;
    _worldPacket << uint8(ResultCode);
    _worldPacket << uint8(PermissionFlags);

    return &_worldPacket;
}

WorldPacket const* HousingResetKioskModeResponse::Write()
{
    _worldPacket << uint8(Result);

    return &_worldPacket;
}

void HousingResetHouse::Read()
{
    _worldPacket >> ResetScope;
}

WorldPacket const* HousingResetHouseResponse::Write()
{
    _worldPacket << uint8(Result);

    return &_worldPacket;
}

// Shared writer for the five SMSG_ACCOUNT_*_COLLECTION_UPDATE opcodes.
void AccountCollectionUpdateBase::WriteCollection()
{
    _worldPacket << uint8(IsIncrementalUpdate ? 0x80 : 0x00);
    _worldPacket << uint32(IDs.size());
    _worldPacket << uint32(StateFlags.size());

    for (uint32 id : IDs)
        _worldPacket << uint32(id);

    for (bool flag : StateFlags)
        _worldPacket.WriteBit(flag);
    if (!StateFlags.empty())
        _worldPacket.FlushBits();
}

WorldPacket const* AccountExteriorFixtureCollectionUpdate::Write()
{
    WriteCollection();
    return &_worldPacket;
}

WorldPacket const* AccountHouseTypeCollectionUpdate::Write()
{
    WriteCollection();
    return &_worldPacket;
}

WorldPacket const* AccountRoomCollectionUpdate::Write()
{
    WriteCollection();
    return &_worldPacket;
}

WorldPacket const* AccountRoomThemeCollectionUpdate::Write()
{
    WriteCollection();
    return &_worldPacket;
}

WorldPacket const* AccountRoomMaterialCollectionUpdate::Write()
{
    WriteCollection();

    return &_worldPacket;
}

WorldPacket const* InvalidateNeighborhood::Write()
{
    _worldPacket << NeighborhoodGuid;

    return &_worldPacket;
}

WorldPacket const* GetDecorRefundListResponse::Write()
{
    _worldPacket << uint32(Decors.size());
    for (auto const& decor : Decors)
    {
        _worldPacket << uint32(decor.DecorID);
        _worldPacket << uint64(decor.RefundPrice);
        _worldPacket << uint64(decor.ExpiryTime);
        _worldPacket << uint32(decor.Flags);
    }

    return &_worldPacket;
}

void BulkRefund::Read()
{
    // sanity cap: the retail client only ever refunds one refund window's worth
    constexpr uint32 MaxRefundableDecor = 500;

    uint32 count = 0;
    _worldPacket >> count;

    if (count > MaxRefundableDecor)
        count = MaxRefundableDecor;

    DecorGUIDs.resize(count);
    for (uint32 i = 0; i < count; ++i)
        _worldPacket >> DecorGUIDs[i];
}

WorldPacket const* BulkRefundResponse::Write()
{
    _worldPacket << uint32(Result);

    return &_worldPacket;
}

WorldPacket const* GetAllLicensedDecorQuantitiesResponse::Write()
{
    _worldPacket << uint32(Quantities.size());
    for (auto const& qty : Quantities)
    {
        _worldPacket << uint32(qty.HouseDecorID);
        _worldPacket << uint32(qty.PlacedQuantity);
        _worldPacket << uint32(qty.StoredQuantity);
    }

    return &_worldPacket;
}

WorldPacket const* LicensedDecorQuantitiesUpdate::Write()
{
    _worldPacket << uint32(Quantities.size());
    for (auto const& qty : Quantities)
    {
        _worldPacket << uint32(qty.HouseDecorID);
        _worldPacket << uint32(qty.PlacedQuantity);
        _worldPacket << uint32(qty.StoredQuantity);
    }

    return &_worldPacket;
}

WorldPacket const* InitiativeServiceStatus::Write()
{
    _worldPacket.WriteBit(ServiceEnabled);
    _worldPacket.FlushBits();

    return &_worldPacket;
}

WorldPacket const* GetPlayerInitiativeInfoResult::Write()
{
    // The data block is only emitted when (Flags >> 6) == 1.
    _worldPacket << NeighborhoodGUID;
    _worldPacket << uint8(Flags);

    if ((Flags >> 6) == 1)
    {
        _worldPacket << int64(RemainingDuration);
        _worldPacket << int32(CurrentInitiativeID);
        _worldPacket << int32(CurrentMilestoneID);
        _worldPacket << int32(CurrentCycleID);
        _worldPacket << float(ProgressRequired);
        _worldPacket << float(CurrentProgress);
        _worldPacket << float(PlayerTotalContribution);

        _worldPacket << uint32(Tasks.size());
        for (auto const& task : Tasks)
        {
            _worldPacket << uint32(task.TaskID);
            _worldPacket << uint32(task.Progress);
        }
    }

    return &_worldPacket;
}

WorldPacket const* GetInitiativeActivityLogResult::Write()
{
    _worldPacket << NeighborhoodGuid;
    _worldPacket << uint32(CompletedTasks.size());
    for (auto const& entry : CompletedTasks)
    {
        _worldPacket << entry.PlayerGuid;
        _worldPacket << entry.TargetGuid;
        _worldPacket << uint32(entry.ContributionAmount);
        _worldPacket << uint64(entry.CompletionTime);
        _worldPacket << uint32(entry.TaskID);
    }

    return &_worldPacket;
}

WorldPacket const* InitiativeTaskComplete::Write()
{
    // The client takes the whole body as an opaque blob; retail sends a single u32.
    _worldPacket << uint32(TaskID);

    return &_worldPacket;
}

WorldPacket const* InitiativeComplete::Write()
{
    _worldPacket << uint32(InitiativeID);

    return &_worldPacket;
}

WorldPacket const* ClearInitiativeTaskCriteriaProgress::Write()
{
    _worldPacket << uint32(CriteriaIDs.size());
    for (uint64 id : CriteriaIDs)
        _worldPacket << uint64(id);

    return &_worldPacket;
}

WorldPacket const* GetInitiativeRewardsResult::Write()
{
    _worldPacket << uint32(Result);
    _worldPacket << SourceGuid;
    _worldPacket << TargetGuid;

    return &_worldPacket;
}

WorldPacket const* InitiativeRewardAvailable::Write()
{
    _worldPacket << uint32(RewardGuids.size());
    for (ObjectGuid const& guid : RewardGuids)
        _worldPacket << guid;

    return &_worldPacket;
}

WorldPacket const* HousingPhotoSharingAuthorizationResult::Write()
{
    _worldPacket << uint8(Result);
    uint8 length = static_cast<uint8>(std::min<size_t>(PartnerName.size(), 0x7F));
    _worldPacket << uint8(length << 1);
    if (length > 0)
        _worldPacket.append(reinterpret_cast<uint8 const*>(PartnerName.data()), length);

    return &_worldPacket;
}

WorldPacket const* HousingPhotoSharingAuthorizationClearedResult::Write()
{
    _worldPacket << uint8(Result);

    return &_worldPacket;
}

WorldPacket const* CraftingHouseHelloResponse::Write()
{
    _worldPacket << Guid;
    uint8 flags = 0;
    if (Field0)
        flags |= 0x80;
    if (OpenForBusiness)
        flags |= 0x40;
    _worldPacket << uint8(flags);

    return &_worldPacket;
}

WorldPacket const* GuildOthersOwnedHousesResult::Write()
{
    _worldPacket << uint8(Result);
    _worldPacket << GuildGuid;
    _worldPacket << uint32(Houses.size());
    for (auto const& house : Houses)
        WriteJamCliHouse(_worldPacket, house);

    return &_worldPacket;
}

WorldPacket const* HousingSvcsNeighborhoodUpdateNameNotification::Write()
{
    _worldPacket << NeighborhoodGuid;
    _worldPacket << SizedString::BitsSize<8>(NewName); // size prefix is 8 packed bits, not 7
    _worldPacket.FlushBits();
    _worldPacket << SizedString::Data(NewName);

    return &_worldPacket;
}
} // namespace WorldPackets::Housing

namespace WorldPackets::Neighborhood
{
void NeighborhoodCharterCreate::Read()
{
    _worldPacket >> NeighborhoodMapID;
    _worldPacket >> FactionFlags;
    _worldPacket >> SizedCString::BitsSize<8>(Name);

    _worldPacket >> SizedCString::Data(Name);
}

void NeighborhoodCharterEdit::Read()
{
    _worldPacket >> NeighborhoodMapID;
    _worldPacket >> FactionFlags;
    _worldPacket >> SizedCString::BitsSize<8>(Name);

    _worldPacket >> SizedCString::Data(Name);
}

void NeighborhoodCharterAddSignature::Read()
{
    _worldPacket >> CharterGuid;
}

void NeighborhoodCharterSendSignatureRequest::Read()
{
    _worldPacket >> TargetPlayerGuid;
}

void NeighborhoodUpdateName::Read()
{
    _worldPacket >> SizedCString::BitsSize<8>(NewName);

    _worldPacket >> SizedCString::Data(NewName);
}

void NeighborhoodSetPublicFlag::Read()
{
    _worldPacket >> NeighborhoodGuid;
    _worldPacket >> Bits<1>(IsPublic);
}

void NeighborhoodAddSecondaryOwner::Read()
{
    _worldPacket >> PlayerGuid;
}

void NeighborhoodRemoveSecondaryOwner::Read()
{
    _worldPacket >> PlayerGuid;
}

void NeighborhoodInviteResident::Read()
{
    _worldPacket >> PlayerGuid;
}

void NeighborhoodCancelInvitation::Read()
{
    _worldPacket >> InviteeGuid;
}

void NeighborhoodPlayerDeclineInvite::Read()
{
    _worldPacket >> NeighborhoodGuid;
}

void NeighborhoodBuyHouse::Read()
{
    _worldPacket >> CornerstoneGuid;
    _worldPacket >> HouseGuid;
}

void NeighborhoodMoveHouse::Read()
{
    _worldPacket >> CornerstoneGuid;
    _worldPacket >> HouseGuid;
}

void NeighborhoodOpenCornerstoneUI::Read()
{
    _worldPacket >> PlotIndex;
    _worldPacket >> NeighborhoodGuid;
}

void NeighborhoodOfferOwnership::Read()
{
    _worldPacket >> NewOwnerGuid;
}

void NeighborhoodGetRoster::Read()
{
    _worldPacket >> NeighborhoodGuid;
}

void NeighborhoodEvictPlot::Read()
{
    _worldPacket >> PlotIndex;
    _worldPacket >> NeighborhoodGuid;
}

WorldPacket const* NeighborhoodCharterUpdateResponse::Write()
{
    // The leading field is a full uint8(Result) the client tests != 0, not a bit.
    _worldPacket << uint8(Result);
    _worldPacket << CharterGuid;
    _worldPacket << uint32(MapID);
    _worldPacket << uint32(SignatureCount);
    _worldPacket << uint32(Signers.size());
    _worldPacket << uint32(Unknown);
    for (ObjectGuid const& signer : Signers)
        _worldPacket << signer;
    uint8 charterNameLen = static_cast<uint8>(std::min<size_t>(NeighborhoodName.size() + 1, 255));
    _worldPacket << uint8(charterNameLen);
    _worldPacket.append(NeighborhoodName.c_str(), charterNameLen);

    return &_worldPacket;
}

WorldPacket const* NeighborhoodCharterOpenUIResponse::Write()
{
    _worldPacket << uint8(Result);
    _worldPacket << CharterGuid;
    _worldPacket << uint32(MapID);
    _worldPacket << uint32(SignatureCount);
    _worldPacket << uint32(Signers.size());
    _worldPacket << uint32(Unknown);
    for (ObjectGuid const& signer : Signers)
        _worldPacket << signer;
    uint8 charterNameLen = static_cast<uint8>(std::min<size_t>(NeighborhoodName.size() + 1, 255));
    _worldPacket << uint8(charterNameLen);
    _worldPacket.append(NeighborhoodName.c_str(), charterNameLen);

    return &_worldPacket;
}

WorldPacket const* NeighborhoodCharterSignRequest::Write()
{
    _worldPacket << uint8(Result);
    _worldPacket << CharterGuid;
    _worldPacket << uint32(MapID);
    _worldPacket << uint32(Unknown);
    uint8 charterNameLen = static_cast<uint8>(std::min<size_t>(NeighborhoodName.size() + 1, 255));
    _worldPacket << uint8(charterNameLen);
    _worldPacket.append(NeighborhoodName.c_str(), charterNameLen);

    return &_worldPacket;
}

WorldPacket const* NeighborhoodCharterAddSignatureResponse::Write()
{
    _worldPacket << uint8(Result);
    _worldPacket << CharterGuid;

    return &_worldPacket;
}

WorldPacket const* NeighborhoodCharterOpenConfirmationUIResponse::Write()
{
    _worldPacket << uint8(Result);
    _worldPacket << uint32(Field1);
    _worldPacket << uint32(Field2);
    uint8 charterNameLen = static_cast<uint8>(std::min<size_t>(NeighborhoodName.size() + 1, 255));
    _worldPacket << uint8(charterNameLen);
    _worldPacket.append(NeighborhoodName.c_str(), charterNameLen);

    return &_worldPacket;
}

WorldPacket const* NeighborhoodCharterSignatureRemovedNotification::Write()
{
    _worldPacket << CharterGuid;

    return &_worldPacket;
}

WorldPacket const* NeighborhoodEvictPlayerResponse::Write()
{
    // Unverified: the client consumes this body as opaque bytes without decoding fields.
    _worldPacket << PlayerGuid;

    return &_worldPacket;
}

WorldPacket const* NeighborhoodUpdateNameResponse::Write()
{
    _worldPacket << uint8(Result);

    return &_worldPacket;
}

WorldPacket const* NeighborhoodAddSecondaryOwnerResponse::Write()
{
    _worldPacket << PlayerGuid;
    _worldPacket << uint8(Result);

    return &_worldPacket;
}

WorldPacket const* NeighborhoodRemoveSecondaryOwnerResponse::Write()
{
    _worldPacket << PlayerGuid;
    _worldPacket << uint8(Result);

    return &_worldPacket;
}

WorldPacket const* NeighborhoodBuyHouseResponse::Write()
{
    WriteJamCliHouse(_worldPacket, House);
    _worldPacket << uint8(Result);

    return &_worldPacket;
}

WorldPacket const* NeighborhoodMoveHouseResponse::Write()
{
    WriteJamCliHouse(_worldPacket, House);
    _worldPacket << MoveTransactionGuid;
    _worldPacket << uint8(Result);

    return &_worldPacket;
}

WorldPacket const* NeighborhoodOpenCornerstoneUIResponse::Write()
{
    _worldPacket << uint32(PlotIndex);          // echoed from the CMSG, not a result code
    _worldPacket << PlotOwnerGuid;              // player GUID when owned, empty when unclaimed
    _worldPacket << NeighborhoodGuid;           // housing GUID when owned, empty when unclaimed
    _worldPacket << uint64(Cost);               // purchase price
    _worldPacket << uint8(PurchaseStatus);      // 73 = purchasable, 0 = not; client checks == 73
    _worldPacket << CornerstoneGuid;            // cornerstone game object

    // The hasExistingHouse bit flips the cornerstone button from Buy to Move when the player already owns a house.
    bool const hasExistingHouse = ExistingHouse.has_value();
    _worldPacket << Bits<1>(IsPlotOwned);
    _worldPacket << SizedCString::BitsSize<8>(NeighborhoodName);
    _worldPacket << OptionalInit(AlternatePrice);
    _worldPacket << Bits<1>(CanPurchase);
    _worldPacket.WriteBit(hasExistingHouse);
    _worldPacket << Bits<1>(HasResidents);
    _worldPacket << OptionalInit(StatusValue);
    _worldPacket << Bits<1>(IsInitiative);
    _worldPacket.FlushBits();

    _worldPacket << SizedCString::Data(NeighborhoodName);

    if (AlternatePrice)
        _worldPacket << uint64(*AlternatePrice);

    if (hasExistingHouse)
        WriteJamCliHouse(_worldPacket, *ExistingHouse);

    if (StatusValue)
        _worldPacket << uint32(*StatusValue);

    return &_worldPacket;
}

WorldPacket const* NeighborhoodInviteResidentResponse::Write()
{
    _worldPacket << uint8(Result);
    _worldPacket << InviteeGuid;

    return &_worldPacket;
}

WorldPacket const* NeighborhoodCancelInvitationResponse::Write()
{
    _worldPacket << uint8(Result);
    _worldPacket << InviteeGuid;

    return &_worldPacket;
}

WorldPacket const* NeighborhoodDeclineInvitationResponse::Write()
{
    // The GUID is the invited player's guid client-side.
    _worldPacket << uint8(Result);
    _worldPacket << NeighborhoodGuid;

    return &_worldPacket;
}

WorldPacket const* NeighborhoodPlayerGetInviteResponse::Write()
{
    _worldPacket << uint8(Result);
    Housing::WriteInviteEntry(_worldPacket, Entry);

    return &_worldPacket;
}

WorldPacket const* NeighborhoodGetInvitesResponse::Write()
{
    _worldPacket << uint8(Result);
    _worldPacket << uint32(Invites.size());
    for (auto const& invite : Invites)
        Housing::WriteInviteEntry(_worldPacket, invite);

    return &_worldPacket;
}

WorldPacket const* NeighborhoodInviteNotification::Write()
{
    _worldPacket << NeighborhoodGuid;

    return &_worldPacket;
}

WorldPacket const* NeighborhoodOfferOwnershipResponse::Write()
{
    _worldPacket << uint8(Result);

    return &_worldPacket;
}

WorldPacket const* NeighborhoodGetRosterResponse::Write()
{
    _worldPacket << uint8(Result);

    bool const hasNeighborhood = !GroupNeighborhoodGuid.IsEmpty();
    _worldPacket << uint32(hasNeighborhood ? 1 : 0);
    if (hasNeighborhood)
    {
        _worldPacket << GroupNeighborhoodGuid;
        _worldPacket << GroupOwnerGuid;
        _worldPacket << uint64(0);
        _worldPacket << uint64(0);

        uint32 houseCount = 0;
        for (RosterMemberData const& member : Members)
            if (!member.HouseGuid.IsEmpty())
                ++houseCount;
        _worldPacket << uint32(houseCount);
        for (RosterMemberData const& member : Members)
        {
            if (member.HouseGuid.IsEmpty())
                continue;
            Housing::JamCliHouse house;
            house.HouseGUID = member.HouseGuid;
            house.OwnerGUID = member.PlayerGuid;
            house.NeighborhoodGUID = GroupNeighborhoodGuid;
            house.PlotIndex = member.PlotIndex;
            house.HouseSettingFlags = member.HouseSettingFlags;
            Housing::WriteJamCliHouse(_worldPacket, house);
        }

        uint8 const nameLen = NeighborhoodName.empty() ? 0 : static_cast<uint8>(std::min<size_t>(NeighborhoodName.size() + 1, 255));
        _worldPacket << uint8(nameLen);
        _worldPacket << uint8(0);
        if (nameLen)
            _worldPacket.append(reinterpret_cast<uint8 const*>(NeighborhoodName.c_str()), nameLen);
    }

    _worldPacket << uint32(Members.size());
    for (RosterMemberData const& member : Members)
    {
        _worldPacket << member.PlayerGuid;
        _worldPacket << uint8(member.ResidentType);
        _worldPacket << uint8(member.IsOnline ? 0x80 : 0x00);
    }

    _worldPacket << uint8(0);                       // no trailing guid

    return &_worldPacket;
}

WorldPacket const* NeighborhoodRosterResidentUpdate::Write()
{
    _worldPacket << uint32(Residents.size());
    for (ResidentEntry const& resident : Residents)
    {
        _worldPacket << resident.PlayerGuid;
        _worldPacket << uint8(resident.ResidentType);
        _worldPacket << uint8(resident.IsOnline ? 0x80 : 0x00);
    }

    return &_worldPacket;
}

WorldPacket const* NeighborhoodInviteNameLookupResult::Write()
{
    _worldPacket << uint8(Result);
    _worldPacket << PlayerGuid;

    return &_worldPacket;
}

WorldPacket const* NeighborhoodEvictPlotResponse::Write()
{
    // The GUID is the evicted plot/house guid client-side.
    _worldPacket << uint8(Result);
    _worldPacket << NeighborhoodGuid;

    return &_worldPacket;
}

WorldPacket const* NeighborhoodEvictPlotNotice::Write()
{
    _worldPacket << uint32(PlotId);
    _worldPacket << NeighborhoodGuid;
    _worldPacket << PlotGuid;

    return &_worldPacket;
}

void GetAvailableInitiativeRequest::Read()
{
    _worldPacket >> NeighborhoodGuid;
}

void GetInitiativeActivityLogRequest::Read()
{
    _worldPacket >> NeighborhoodGuid;
}

void InitiativeUpdateActiveNeighborhood::Read()
{
    _worldPacket >> NeighborhoodGuid;
}
} // namespace WorldPackets::Neighborhood
