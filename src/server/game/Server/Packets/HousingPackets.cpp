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
    _worldPacket >> SizedString::BitsSize<7>(NeighborhoodName);

    _worldPacket >> Flags;
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
    _worldPacket >> OptionalInit(HouseGuid);

    if (HouseGuid)
        _worldPacket >> *HouseGuid;
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
    // Wire format verified against retail 12.0.1 build 65940 (opcode 0x460012)
    // Retail: [PackedGUID] [0x80] [uint8 len] [string bytes]
    // Byte after GUID: bit 7 = Allow, bits 6-0 = zero padding (NOT a length field)
    // The 7-bit field was incorrectly used as SizedString::BitsSize<7> before,
    // causing the client to read NameLen bytes before the uint8 prefix, shifting everything.
    _worldPacket << NeighborhoodGuid;
    _worldPacket << Bits<1>(Allow);
    _worldPacket.FlushBits();

    if (Allow)
    {
        _worldPacket << uint8(Name.size());
        _worldPacket.WriteString(Name);
    }

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
    _worldPacket << HouseGuid;
    _worldPacket.WriteBit(Locked);
    _worldPacket.FlushBits();
    return &_worldPacket;
}

WorldPacket const* HouseExteriorSetHousePositionResponse::Write()
{
    _worldPacket << uint32(Result);
    _worldPacket << HouseGuid;
    return &_worldPacket;
}

// ============================================================
// Housing Decor SMSG Responses (0x51xxxx)
// ============================================================

WorldPacket const* HousingDecorSetEditModeResponse::Write()
{
    // Same wire format as Fixture/Room edit mode responses: uint32(Result) + Bit(Active)
    _worldPacket << uint32(Result);
    _worldPacket.WriteBit(Active);
    _worldPacket.FlushBits();
    return &_worldPacket;
}

WorldPacket const* HousingDecorMoveResponse::Write()
{
    _worldPacket << uint32(Result);
    _worldPacket << DecorGuid;
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
    _worldPacket << DecorGuid;
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
    _worldPacket << DecorGuid;
    return &_worldPacket;
}

WorldPacket const* HousingDecorRequestStorageResponse::Write()
{
    _worldPacket << BNetAccountGuid;
    _worldPacket << uint8(ResultCode);
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
    _worldPacket << DecorGuid;
    return &_worldPacket;
}

WorldPacket const* HousingDecorSystemSetDyeSlotsResponse::Write()
{
    _worldPacket << uint32(Result);
    _worldPacket << DecorGuid;
    return &_worldPacket;
}

WorldPacket const* HousingRedeemDeferredDecorResponse::Write()
{
    _worldPacket << uint32(Result);
    _worldPacket << uint32(DecorEntryID);
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
    _worldPacket.WriteBit(Active);
    _worldPacket.FlushBits();
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
    _worldPacket << HouseGuid;
    return &_worldPacket;
}

WorldPacket const* HousingFixtureSetHouseSizeResponse::Write()
{
    _worldPacket << uint32(Result);
    _worldPacket << uint8(Size);
    return &_worldPacket;
}

WorldPacket const* HousingFixtureSetHouseTypeResponse::Write()
{
    _worldPacket << uint32(Result);
    _worldPacket << int32(HouseExteriorTypeID);
    return &_worldPacket;
}

WorldPacket const* HousingFixtureSetCoreFixtureResponse::Write()
{
    _worldPacket << uint32(Result);
    _worldPacket << int32(FixtureRecordID);
    return &_worldPacket;
}

WorldPacket const* HousingFixtureCreateFixtureResponse::Write()
{
    _worldPacket << uint32(Result);
    _worldPacket << FixtureGuid;
    return &_worldPacket;
}

WorldPacket const* HousingFixtureDeleteFixtureResponse::Write()
{
    _worldPacket << uint32(Result);
    _worldPacket << FixtureGuid;
    return &_worldPacket;
}

// ============================================================
// Housing Room SMSG Responses (0x53xxxx)
// ============================================================

WorldPacket const* HousingRoomSetLayoutEditModeResponse::Write()
{
    _worldPacket << uint32(Result);
    _worldPacket.WriteBit(Active);
    _worldPacket.FlushBits();
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
    _worldPacket << RoomGuid;
    return &_worldPacket;
}

WorldPacket const* HousingRoomUpdateResponse::Write()
{
    _worldPacket << uint32(Result);
    _worldPacket << RoomGuid;
    return &_worldPacket;
}

WorldPacket const* HousingRoomSetComponentThemeResponse::Write()
{
    _worldPacket << uint32(Result);
    _worldPacket << RoomGuid;
    _worldPacket << int32(ComponentID);
    _worldPacket << int32(ThemeSetID);
    return &_worldPacket;
}

WorldPacket const* HousingRoomApplyComponentMaterialsResponse::Write()
{
    _worldPacket << uint32(Result);
    _worldPacket << RoomGuid;
    _worldPacket << int32(ComponentID);
    _worldPacket << int32(RoomComponentTextureRecordID);
    return &_worldPacket;
}

WorldPacket const* HousingRoomSetDoorTypeResponse::Write()
{
    _worldPacket << uint32(Result);
    _worldPacket << RoomGuid;
    _worldPacket << int32(ComponentID);
    _worldPacket << uint8(DoorType);
    return &_worldPacket;
}

WorldPacket const* HousingRoomSetCeilingTypeResponse::Write()
{
    _worldPacket << uint32(Result);
    _worldPacket << RoomGuid;
    _worldPacket << int32(ComponentID);
    _worldPacket << uint8(CeilingType);
    return &_worldPacket;
}

// ============================================================
// Housing Services SMSG Responses (0x54xxxx)
// ============================================================

WorldPacket const* HousingSvcsNotifyPermissionsFailure::Write()
{
    _worldPacket << uint16(Result);
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
    _worldPacket << NeighborhoodGuid;
    _worldPacket << uint8(PlotIndex);
    return &_worldPacket;
}

WorldPacket const* HousingSvcsClearPlotReservationResponse::Write()
{
    _worldPacket << uint32(Result);
    _worldPacket << NeighborhoodGuid;
    return &_worldPacket;
}

WorldPacket const* HousingSvcsRelinquishHouseResponse::Write()
{
    _worldPacket << uint32(Result);
    _worldPacket << HouseGuid;
    return &_worldPacket;
}

WorldPacket const* HousingSvcsCancelRelinquishHouseResponse::Write()
{
    _worldPacket << uint32(Result);
    _worldPacket << HouseGuid;
    return &_worldPacket;
}

static void WriteJamCurrentHouseInfo(WorldPacket& packet, JamCurrentHouseInfo const& info);

WorldPacket const* HousingSvcsGetPlayerHousesInfoResponse::Write()
{
    // Sniff-verified: uint32 Count + uint8 Unknown + JamCurrentHouseInfo per house
    _worldPacket << uint32(Houses.size());
    _worldPacket << uint8(Unknown);
    for (auto const& house : Houses)
        WriteJamCurrentHouseInfo(_worldPacket, house);
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
    // Sniff-verified (36 bytes): uint8 + 4x int32 + PackedGUID + 2x int32 + uint16
    _worldPacket << uint8(Type);
    _worldPacket << int32(PreviousFavor);
    _worldPacket << int32(PreviousLevel);
    _worldPacket << int32(NewLevel);
    _worldPacket << int32(Field4);
    _worldPacket << HouseGuid;
    _worldPacket << int32(PreviousLevelId);
    _worldPacket << int32(NextLevelFavorCost);
    _worldPacket << uint16(Flags);
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
    _worldPacket << NeighborhoodGuid;
    _worldPacket << HouseGuid;
    return &_worldPacket;
}

WorldPacket const* HousingSvcsAcceptNeighborhoodOwnershipResponse::Write()
{
    _worldPacket << uint32(Result);
    _worldPacket << NeighborhoodGuid;
    return &_worldPacket;
}

WorldPacket const* HousingSvcsRejectNeighborhoodOwnershipResponse::Write()
{
    _worldPacket << uint32(Result);
    _worldPacket << NeighborhoodGuid;
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
    _worldPacket << HouseGuid;
    _worldPacket << uint32(AccessFlags);
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
    _worldPacket << uint32(Neighborhoods.size());
    for (auto const& neighborhood : Neighborhoods)
    {
        _worldPacket << neighborhood.NeighborhoodGuid;
        _worldPacket << uint32(neighborhood.MapID);
        _worldPacket << SizedString::BitsSize<8>(neighborhood.FriendName);
    }
    _worldPacket.FlushBits();
    for (auto const& neighborhood : Neighborhoods)
        _worldPacket << SizedString::Data(neighborhood.FriendName);
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
    _worldPacket << NeighborhoodGuid;
    return &_worldPacket;
}

// ============================================================
// Housing General SMSG Responses (0x55xxxx)
// ============================================================

// Helper: Write JamCurrentHouseInfo struct to packet (IDA sub_7FF6F6E0A170)
static void WriteJamCurrentHouseInfo(WorldPacket& packet, JamCurrentHouseInfo const& info)
{
    packet << info.OwnerGuid;
    packet << info.SecondaryOwnerGuid;
    packet << info.PlotGuid;
    packet << uint8(info.Flags);
    packet << uint32(info.HouseTypeId);
    uint8 statusFlags = info.StatusFlags;
    if (info.HouseId)
        statusFlags |= 0x80;
    packet << uint8(statusFlags);
    if (info.HouseId)
        packet << uint64(*info.HouseId);
}

// Helper: Write JamNeighborhoodRosterEntry struct to packet (IDA sub_7FF6F6E0A460)
static void WriteJamNeighborhoodRosterEntry(WorldPacket& packet, JamNeighborhoodRosterEntry const& entry)
{
    packet << uint64(entry.Timestamp);
    packet << entry.PlayerGuid;
    packet << entry.HouseGuid;
    packet << uint64(entry.ExtraData);
}

WorldPacket const* HousingHouseStatusResponse::Write()
{
    // Sniff-verified (0x550000): 3x PackedGUID + uint32 Status
    // GUID3 (NeighborhoodGuid) must match EditMode DecorGuids[0]
    _worldPacket << HouseGuid;
    _worldPacket << PlotGuid;
    _worldPacket << NeighborhoodGuid;
    _worldPacket << uint32(Status);
    return &_worldPacket;
}

WorldPacket const* HousingGetCurrentHouseInfoResponse::Write()
{
    // IDA 12.0 verified (0x550001): JamCurrentHouseInfo + uint8 ResponseFlags
    WriteJamCurrentHouseInfo(_worldPacket, HouseInfo);
    _worldPacket << uint8(ResponseFlags);
    return &_worldPacket;
}

WorldPacket const* HousingExportHouseResponse::Write()
{
    _worldPacket << uint32(Result);
    _worldPacket << HouseGuid;
    _worldPacket << SizedString::BitsSize<11>(ExportData);
    _worldPacket.FlushBits();
    _worldPacket << SizedString::Data(ExportData);
    return &_worldPacket;
}

WorldPacket const* HousingGetPlayerPermissionsResponse::Write()
{
    // IDA 12.0 verified (0x550006): PackedGUID + uint8 ResultCode + uint8 Permissions
    _worldPacket << HouseGuid;
    _worldPacket << uint8(ResultCode);
    _worldPacket << uint8(PermissionFlags);
    return &_worldPacket;
}

WorldPacket const* HousingResetKioskModeResponse::Write()
{
    // IDA 12.0 verified (0x550007): single uint8
    _worldPacket << uint8(Result);
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

WorldPacket const* AccountRoomCollectionUpdate::Write()
{
    _worldPacket << uint32(RoomID);
    return &_worldPacket;
}

WorldPacket const* AccountRoomThemeCollectionUpdate::Write()
{
    _worldPacket << uint32(ThemeID);
    return &_worldPacket;
}

WorldPacket const* AccountRoomMaterialCollectionUpdate::Write()
{
    _worldPacket << uint32(MaterialID);
    return &_worldPacket;
}

WorldPacket const* InvalidateNeighborhood::Write()
{
    _worldPacket << NeighborhoodGuid;
    return &_worldPacket;
}

// ============================================================
// Initiative System SMSG Responses (0x4203xx)
// ============================================================

WorldPacket const* InitiativeServiceStatus::Write()
{
    _worldPacket.WriteBit(ServiceEnabled);
    _worldPacket.FlushBits();
    return &_worldPacket;
}

WorldPacket const* GetPlayerInitiativeInfoResult::Write()
{
    _worldPacket << uint32(Result);
    return &_worldPacket;
}

WorldPacket const* GetInitiativeActivityLogResult::Write()
{
    _worldPacket << uint32(Result);
    return &_worldPacket;
}

WorldPacket const* HousingPhotoSharingAuthorizationResult::Write()
{
    _worldPacket << uint32(Result);
    return &_worldPacket;
}

WorldPacket const* HousingPhotoSharingAuthorizationClearedResult::Write()
{
    _worldPacket << uint32(Result);
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
    _worldPacket >> PlotIndex;
    _worldPacket >> NeighborhoodGuid;
    _worldPacket >> PlotGuid;
}

void NeighborhoodMoveHouse::Read()
{
    _worldPacket >> NeighborhoodGuid;
    _worldPacket >> PlotGuid;
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

// ============================================================
// Neighborhood Charter SMSG Responses (0x5Bxxxx)
// ============================================================

WorldPacket const* NeighborhoodCharterUpdateResponse::Write()
{
    _worldPacket << uint32(Result);
    _worldPacket << CharterGuid;
    return &_worldPacket;
}

WorldPacket const* NeighborhoodCharterOpenUIResponse::Write()
{
    _worldPacket << uint32(Result);
    _worldPacket << CharterGuid;
    _worldPacket << uint32(NeighborhoodMapID);
    _worldPacket << uint32(SignatureCount);
    _worldPacket << SizedString::BitsSize<7>(NeighborhoodName);
    _worldPacket.FlushBits();
    _worldPacket << SizedString::Data(NeighborhoodName);
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
    _worldPacket << CharterGuid;
    _worldPacket << SignerGuid;
    return &_worldPacket;
}

WorldPacket const* NeighborhoodCharterOpenConfirmationUIResponse::Write()
{
    _worldPacket << uint32(Result);
    _worldPacket << CharterGuid;
    _worldPacket << CharterOwnerGuid;
    _worldPacket << SizedString::BitsSize<7>(NeighborhoodName);
    _worldPacket.FlushBits();
    _worldPacket << SizedString::Data(NeighborhoodName);
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
    _worldPacket << PlotAreaTriggerGuid;
    return &_worldPacket;
}

WorldPacket const* NeighborhoodPlayerLeavePlot::Write()
{
    return &_worldPacket;
}

WorldPacket const* NeighborhoodEvictPlayerResponse::Write()
{
    _worldPacket << PlayerGuid;
    return &_worldPacket;
}

WorldPacket const* NeighborhoodUpdateNameResponse::Write()
{
    // IDA 12.0 verified (0x5C0003): single uint8
    _worldPacket << uint8(Result);
    return &_worldPacket;
}

WorldPacket const* NeighborhoodUpdateNameNotification::Write()
{
    // IDA 12.0 verified (0x5C0004): uint8(nameLen) + bytes[nameLen]
    uint8 nameLen = NewName.empty() ? 0 : static_cast<uint8>(NewName.size() + 1);
    _worldPacket << uint8(nameLen);
    if (nameLen > 0)
        _worldPacket.append(reinterpret_cast<uint8 const*>(NewName.c_str()), nameLen);
    return &_worldPacket;
}

WorldPacket const* NeighborhoodAddSecondaryOwnerResponse::Write()
{
    // IDA 12.0 verified (0x5C0006): PackedGUID + uint8 Result
    _worldPacket << PlayerGuid;
    _worldPacket << uint8(Result);
    return &_worldPacket;
}

WorldPacket const* NeighborhoodRemoveSecondaryOwnerResponse::Write()
{
    // IDA 12.0 verified (0x5C0007): PackedGUID + uint8 Result
    _worldPacket << PlayerGuid;
    _worldPacket << uint8(Result);
    return &_worldPacket;
}

WorldPacket const* NeighborhoodBuyHouseResponse::Write()
{
    // IDA 12.0 verified (0x5C0008): JamCurrentHouseInfo + uint8 Result
    Housing::WriteJamCurrentHouseInfo(_worldPacket, HouseInfo);
    _worldPacket << uint8(Result);
    return &_worldPacket;
}

WorldPacket const* NeighborhoodMoveHouseResponse::Write()
{
    // IDA 12.0 verified (0x5C0009): JamCurrentHouseInfo + PackedGUID + uint8 Result
    Housing::WriteJamCurrentHouseInfo(_worldPacket, HouseInfo);
    _worldPacket << MoveTransactionGuid;
    _worldPacket << uint8(Result);
    return &_worldPacket;
}

WorldPacket const* NeighborhoodOpenCornerstoneUIResponse::Write()
{
    // Wire format verified against retail 12.0.1 build 65940 packet captures (Alliance + Horde)
    // IDA deserializer sub_7FF6F6E3E200: uint32→+32, GUID→+40, GUID→+56, uint64→+72, uint8→+80, GUID→+128
    // Fixed fields
    _worldPacket << uint32(PlotIndex);          // Echoed from CMSG (NOT a result code)
    _worldPacket << PlotOwnerGuid;              // →+40: Player GUID when owned, Empty when unclaimed
    _worldPacket << NeighborhoodGuid;           // →+56: Housing GUID when owned, Empty when unclaimed
    _worldPacket << uint64(Cost);               // →+72: Purchase price
    _worldPacket << uint8(PurchaseStatus);      // →+80: 73=purchasable, 0=not. Client checks ==73
    _worldPacket << CornerstoneGuid;            // →+128: Cornerstone game object

    // Bit-packed section: 1 bool + 8-bit nameLen + 6 bools = 15 bits = 2 bytes
    // Retail uses NUL-terminated CString: NameLen includes the NUL byte
    _worldPacket << Bits<1>(IsPlotOwned);
    _worldPacket << SizedCString::BitsSize<8>(NeighborhoodName);
    _worldPacket << OptionalInit(AlternatePrice);
    _worldPacket << Bits<1>(CanPurchase);
    _worldPacket.WriteBit(false);               // HasOptionalStruct — not yet implemented
    _worldPacket << Bits<1>(HasResidents);
    _worldPacket << OptionalInit(StatusValue);
    _worldPacket << Bits<1>(IsInitiative);
    _worldPacket.FlushBits();

    // Variable-length data (order matches client deserialization)
    // Optional struct data would go here if HasOptionalStruct was set
    _worldPacket << SizedCString::Data(NeighborhoodName);

    if (AlternatePrice)
        _worldPacket << uint64(*AlternatePrice);

    if (StatusValue)
        _worldPacket << uint32(*StatusValue);

    return &_worldPacket;
}

WorldPacket const* NeighborhoodInviteResidentResponse::Write()
{
    // IDA 12.0 verified (0x5C000B): uint8 Result + PackedGUID
    _worldPacket << uint8(Result);
    _worldPacket << InviteeGuid;
    return &_worldPacket;
}

WorldPacket const* NeighborhoodCancelInvitationResponse::Write()
{
    // IDA 12.0 verified (0x5C000C): uint8 Result + PackedGUID
    _worldPacket << uint8(Result);
    _worldPacket << InviteeGuid;
    return &_worldPacket;
}

WorldPacket const* NeighborhoodDeclineInvitationResponse::Write()
{
    // IDA 12.0 verified (0x5C000D): uint8 Result + PackedGUID
    _worldPacket << uint8(Result);
    _worldPacket << NeighborhoodGuid;
    return &_worldPacket;
}

WorldPacket const* NeighborhoodPlayerGetInviteResponse::Write()
{
    // IDA 12.0 verified (0x5C000E): uint8 Result + JamNeighborhoodRosterEntry(48 bytes)
    _worldPacket << uint8(Result);
    Housing::WriteJamNeighborhoodRosterEntry(_worldPacket, Entry);
    return &_worldPacket;
}

WorldPacket const* NeighborhoodGetInvitesResponse::Write()
{
    // IDA 12.0 verified (0x5C000F): uint8 Result + uint32 Count + RosterEntry[Count]
    _worldPacket << uint8(Result);
    _worldPacket << uint32(Invites.size());
    for (auto const& invite : Invites)
        Housing::WriteJamNeighborhoodRosterEntry(_worldPacket, invite);
    return &_worldPacket;
}

WorldPacket const* NeighborhoodInviteNotification::Write()
{
    // IDA 12.0 verified (0x5C0010): single PackedGUID
    _worldPacket << NeighborhoodGuid;
    return &_worldPacket;
}

WorldPacket const* NeighborhoodOfferOwnershipResponse::Write()
{
    // IDA 12.0 verified (0x5C0011): single uint8 Result
    _worldPacket << uint8(Result);
    return &_worldPacket;
}

WorldPacket const* NeighborhoodGetRosterResponse::Write()
{
    // Wire format verified against retail sniff + IDA client handler analysis
    // Structure: Result(1) + CountA(4) + ArrayB{CountB(4) + GroupEntry{...}} + Flags(1) + ArrayA{...}

    // Step 1: Result (uint8, NOT uint32)
    _worldPacket << uint8(Result);

    // Step 2: Count A — number of entries in the flat player-GUID array at the end
    _worldPacket << uint32(Members.size());

    // Step 3: Array B — "groups" (always 1 group = the neighborhood)
    _worldPacket << uint32(1); // Count B = 1 group

    // Step 3b: Single group entry (sub_7FF6A8B3A570)
    // These GUIDs populate the HousingNeighborhoodState singleton (sub_7FF6F69ECCD0):
    //   offset 352 = NeighborhoodGUID, offset 292 = ownerType (computed from OwnerGUID)
    _worldPacket << GroupNeighborhoodGuid;  // PackedGUID — Neighborhood GUID
    _worldPacket << GroupOwnerGuid;         // PackedGUID — Neighborhood owner GUID
    _worldPacket << uint64(0);             // Value 1 (timestamp or flags, 0 in sniff)
    _worldPacket << uint64(0);             // Value 2 (timestamp or flags, 0 in sniff)

    // Sub-array count within this group = number of residents
    _worldPacket << uint32(Members.size());

    // Neighborhood name length (read before sub-entries, string data written after).
    // Client stores at singleton offset 296 via sub_7FF6F97E62B0.
    // When length <= 1, client treats as empty string and reads no bytes.
    uint8 nameLen = NeighborhoodName.empty() ? 0 : static_cast<uint8>(NeighborhoodName.size() + 1); // +1 for null terminator
    _worldPacket << uint8(nameLen);

    // Group flags (bit 7 unused for now)
    _worldPacket << uint8(0);

    // Step 3b-viii: Sub-entries — per-resident data (sub_7FF6A8B3A420)
    for (auto const& member : Members)
    {
        _worldPacket << member.HouseGuid;        // PackedGUID — house GUID
        _worldPacket << member.PlayerGuid;       // PackedGUID — player GUID
        _worldPacket << member.BnetAccountGuid;  // PackedGUID — bnet account GUID (usually empty)
        _worldPacket << uint8(member.PlotIndex); // Plot index
        _worldPacket << uint32(member.JoinTime); // Join timestamp
        _worldPacket << uint8(0);                // Entry flags (bit 7 = has optional uint64)
    }

    // Step 3b-ix: Neighborhood name string (nameLen bytes including null terminator)
    if (nameLen > 1)
        _worldPacket.append(reinterpret_cast<uint8 const*>(NeighborhoodName.c_str()), nameLen);

    // Step 4: Main flags byte (bit 7 = has optional trailing GUID)
    _worldPacket << uint8(0);

    // Step 5: Array A — flat player GUID list with 2 status bytes each (sub_7FF6A8B3A780)
    for (auto const& member : Members)
    {
        _worldPacket << member.PlayerGuid; // PackedGUID
        _worldPacket << uint8(0);          // Status field 1
        // Status field 2: bit 7 (0x80) = online flag, checked by client UI for roster display
        _worldPacket << uint8(member.IsOnline ? 0x80 : 0x00);
    }

    // Step 6: Optional trailing GUID (skipped since MainFlags.bit7 = 0)

    return &_worldPacket;
}

WorldPacket const* NeighborhoodRosterResidentUpdate::Write()
{
    _worldPacket << uint32(Residents.size());
    for (auto const& resident : Residents)
    {
        _worldPacket << resident.PlayerGuid;
        _worldPacket << uint16(resident.StatusFlags);
    }
    return &_worldPacket;
}

WorldPacket const* NeighborhoodInviteNameLookupResult::Write()
{
    // IDA 12.0 verified (0x5C0014): uint8 Result + PackedGUID
    _worldPacket << uint8(Result);
    _worldPacket << PlayerGuid;
    return &_worldPacket;
}

WorldPacket const* NeighborhoodEvictPlotResponse::Write()
{
    // IDA 12.0 verified (0x5C0015): uint8 Result + PackedGUID
    _worldPacket << uint8(Result);
    _worldPacket << NeighborhoodGuid;
    return &_worldPacket;
}

WorldPacket const* NeighborhoodEvictPlotNotice::Write()
{
    // IDA 12.0 verified (0x5C0016): uint32 + PackedGUID + PackedGUID
    _worldPacket << uint32(PlotId);
    _worldPacket << NeighborhoodGuid;
    _worldPacket << PlotGuid;
    return &_worldPacket;
}

// --- Initiative System ---

void GetAvailableInitiativeRequest::Read()
{
    _worldPacket >> NeighborhoodGuid;
}

void GetInitiativeActivityLogRequest::Read()
{
    _worldPacket >> NeighborhoodGuid;
}

} // namespace WorldPackets::Neighborhood
