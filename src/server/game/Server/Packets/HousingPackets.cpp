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

// ============================================================
// Housing namespace (Decor, Fixture, Room, Services, Misc)
// ============================================================
namespace WorldPackets::Housing
{

// --- House Exterior System ---

void HouseExteriorCommitPosition::Read()
{
    // Retail 12.1.0.69933 (all 9 captured, 33 bytes): PackedGUID House + PackedGUID BNetAccount + float X, Y, Z
    // + float Facing, the position and yaw relative to the plot room (e.g. -13.07, -9.51, 0.02, 0.742). The old
    // read (bit HasPosition first) took the GUID's mask byte for HasPosition=false and ignored every move.
    _worldPacket >> HouseGuid;
    _worldPacket >> AccountGuid;
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

// Retired 2026-05-12: HousingDecorDeleteFromStorageById::Read (fake CMSG 0x30000A).

void HousingDecorRequestStorage::Read()
{
    _worldPacket >> HouseGuid;

}

void HousingDecorRedeemDeferredDecor::Read()
{
    _worldPacket >> DeferredDecorID;
    _worldPacket >> RedemptionToken;

}

// Retired 2026-05-11: HousingDecorStartPlacingNewDecor + HousingDecorCatalogCreateSearcher
// Read() bodies deleted (see HousingPackets.h retirement markers).

// Retired 2026-05-12: HousingDecorUpdateDyeSlot::Read (fake CMSG 0x300008, dup of SET_DYE_SLOTS).
// Retired 2026-05-11: HousingDecorStartPlacingFromSource Read() body deleted.
// Retired 2026-05-12: HousingDecorCleanupModeToggle::Read (fake CMSG 0x30000C).

// Retired 2026-05-11: HousingDecorBatchOperation + HousingDecorPlacementPreview Read() bodies deleted.

// --- Fixture System ---

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

// Retired 2026-05-12: HousingFixtureCreateBasicHouse::Read (fake CMSG 0x310001).
// Retired 2026-05-12: HousingFixtureDeleteHouse::Read (fake CMSG 0x310002, use RELINQUISH_HOUSE).

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
    _worldPacket >> SourceRoomGuid;
    _worldPacket >> TargetDoorComponentID;
    // 12.1.0.69587 sender 0x7FF7CD4F6320: guid, u32, u32, bit - no floor index (the server derives the floor from the
    // source room and door).
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
    _worldPacket >> BoundedSize<uint32>(OptionIDs); // NOT Size<> — an unbounded count here is a world-thread bad_alloc
    _worldPacket >> HouseThemeID;
    for (uint32& optionID : OptionIDs)
        _worldPacket >> optionID;

}

void HousingRoomApplyComponentMaterials::Read()
{
    // IDA-verified wire order (sub_7FF75C1AC240, opcode 0x320006):
    //   PackedGUID + uint32 Count + uint32 ColorOverride + uint32 TextureID
    //   + uint8 ComponentSlot + uint32[Count] OptionIDs.
    // The byte sits BEFORE the array, not after — earlier guess parsed it as a
    // trailing Bits<1> which misaligned OptionIDs[0] one byte forward.
    _worldPacket >> RoomGuid;
    _worldPacket >> BoundedSize<uint32>(OptionIDs); // NOT Size<> — an unbounded count here is a world-thread bad_alloc
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

// --- Housing Services System ---

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

// Removed 2026-04-24: HousingSvcsSetTutorialState / HousingSvcsCompleteTutorialStep
// Read() — no matching C_Housing Lua API in 12.0.5.

// Retired 2026-05-12: HousingDecorConfirmPreviewPlacement::Read (fake CMSG 0x300011).

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

// Retired 2026-05-12 (batch 2): Read() bodies for 8 fake SVCS CMSGs deleted —
// dual IDA + sniff cross-check confirmed no client senders in build 67186.

// --- Housing Misc ---
// HousingGetCurrentHouseInfo::Read() and HousingHouseStatus::Read() are empty (inline in header)

// Retired 2026-05-11: HousingRequestEditorAvailability Read() deleted (Lua API is sync).

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

// Retired 2026-05-12: HousingSystemGetHouseInfoAlt / HousingSystemHouseSnapshot /
// HousingSystemExportHouse / HousingSystemUpdateHouseInfo Read() bodies deleted —
// IDA verification confirms no client senders in build 67186.

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
    // 12.0.7 (build 68275): wire is a single 6-bit-length-prefixed player name (invite by name,
    // not GUID). RE feedback 0x40019b.
    _worldPacket >> SizedString::BitsSize<6>(PlayerName);
    _worldPacket >> SizedString::Data(PlayerName);

}

void GuildGetOthersOwnedHouses::Read()
{
    _worldPacket >> PlayerGuid;

}

// --- SMSG Packets ---

WorldPacket const* QueryNeighborhoodNameResponse::Write()
{
    _worldPacket << NeighborhoodGuid;
    _worldPacket << uint8(Result ? 128 : 0);
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

// ============================================================
// House Exterior SMSG Responses (0x50xxxx)
// ============================================================

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

// ============================================================
// House Interior SMSG (0x2Fxxxx)
// ============================================================

// Removed 2026-04-24: HouseInteriorEnterHouse / HouseInteriorLeaveHouseResponse —
// these SMSGs no longer exist in 12.0.5. House entry/leave is communicated via the
// PlayerHouseInfoComponentData.CurrentHouse UpdateField (IDA-verified).

// ============================================================
// Housing Decor SMSG Responses (0x51xxxx)
// ============================================================

WorldPacket const* LastCatalogFetchResponse::Write()
{
    // Sniff-verified: 8-byte payload = uint64 Unix timestamp (build 66337)
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
    // IDA case 5308417: PackedGUID + uint32 + PackedGUID + uint8(Result) + uint8(bit7=SuccessFlag)
    _worldPacket << PlayerGuid;
    _worldPacket << uint32(Field_09);
    _worldPacket << DecorGuid;
    _worldPacket << uint8(Result);
    _worldPacket << uint8(Field_26 ? 0x80 : 0x00);

    return &_worldPacket;
}

WorldPacket const* HousingDecorPlaceResponse::Write()
{
    // Wire format: PackedGUID PlayerGuid + uint32 Field_09 + PackedGUID DecorGuid + uint8 Result
    _worldPacket << PlayerGuid;
    _worldPacket << uint32(Field_09);
    _worldPacket << DecorGuid;
    _worldPacket << uint8(Result);

    return &_worldPacket;
}

WorldPacket const* HousingDecorRemoveResponse::Write()
{
    // Wire format: PackedGUID DecorGUID + PackedGUID UnkGUID + uint32 Field_13 + uint8 Result
    _worldPacket << DecorGuid;
    _worldPacket << UnkGUID;
    _worldPacket << uint32(Field_13);
    _worldPacket << uint8(Result);

    return &_worldPacket;
}

WorldPacket const* HousingDecorLockResponse::Write()
{
    // IDA case 5308420: PackedGUID + PackedGUID + uint32 + uint8(Result) + uint8(bit7=Locked, bit6=Field_17)
    _worldPacket << DecorGuid;
    _worldPacket << PlayerGuid;
    _worldPacket << uint32(Field_16);
    _worldPacket << uint8(Result);
    uint8 flags = 0;
    if (Locked) flags |= 0x80;
    if (Field_17) flags |= 0x40;
    _worldPacket << uint8(flags);

    return &_worldPacket;
}

WorldPacket const* HousingDecorDeleteFromStorageResponse::Write()
{
    // IDA case 5308421: uint8(Result) only — client reads nothing else
    _worldPacket << uint8(Result);

    return &_worldPacket;
}

WorldPacket const* HousingDecorRequestStorageResponse::Write()
{
    // Retail-verified wire format: PackedGUID(Empty) + uint8(ResultCode) + uint8(Flags=0x80)
    // All 3 retail sniff instances show identical 4 bytes: 00 00 00 80
    // Decor data delivered via FHousingStorage_C fragment on Account entity, not inline.
    _worldPacket << BNetAccountGuid;
    _worldPacket << uint8(ResultCode);
    _worldPacket << uint8(Flags);

    return &_worldPacket;
}

WorldPacket const* HousingDecorAddToHouseChestResponse::Write()
{
    // IDA case 5308423: uint8(bit7=success) + uint32(count) + PackedGUID[count]
    _worldPacket << uint8(Success ? 0x80 : 0x00);
    _worldPacket << uint32(DecorGuids.size());
    for (ObjectGuid const& guid : DecorGuids)
        _worldPacket << guid;

    return &_worldPacket;
}

WorldPacket const* HousingDecorSystemSetDyeSlotsResponse::Write()
{
    // Wire format: PackedGUID DecorGUID + uint8 Result
    _worldPacket << DecorGuid;
    _worldPacket << uint8(Result);

    return &_worldPacket;
}

WorldPacket const* HousingRedeemDeferredDecorResponse::Write()
{
    // Sniff-verified wire format (17 bytes): PackedGUID DecorGuid + uint8 Status + uint32 SequenceIndex
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

// Retired 2026-05-11: 4 speculative Decor*Response Write() bodies deleted (see HousingPackets.h).

// ============================================================
// Housing Fixture SMSG Responses (0x52xxxx)
// ============================================================

WorldPacket const* HousingFixtureSetEditModeResponse::Write()
{
    // Sniff-verified (build 66337): PackedGUID(HouseGuid, always empty) + PackedGUID(EditorPlayerGuid) + uint8(Result)
    _worldPacket << HouseGuid;
    _worldPacket << EditorPlayerGuid;
    _worldPacket << uint8(Result);

    return &_worldPacket;
}

WorldPacket const* HousingFixtureCreateBasicHouseResponse::Write()
{
    // IDA case 5373953: uint8(Result) only
    _worldPacket << uint8(Result);

    return &_worldPacket;
}

// Retired 2026-05-12: HousingFixtureDeleteHouseResponse::Write — orphaned after FIXTURE_DELETE_HOUSE CMSG retirement.

WorldPacket const* HousingFixtureSetHouseSizeResponse::Write()
{
    _worldPacket << uint8(Result);
    _worldPacket << uint8(Size);

    return &_worldPacket;
}

WorldPacket const* HousingFixtureSetHouseTypeResponse::Write()
{
    // IDA case 5373956: uint8(Result) + uint32(HouseExteriorTypeID) + uint8(ExtraField)
    _worldPacket << uint8(Result);
    _worldPacket << uint32(HouseExteriorTypeID);
    _worldPacket << uint8(ExtraField);

    return &_worldPacket;
}

WorldPacket const* HousingFixtureSetCoreFixtureResponse::Write()
{
    // IDA case 5373957: uint8(Result) only
    _worldPacket << uint8(Result);

    return &_worldPacket;
}

WorldPacket const* HousingFixtureCreateFixtureResponse::Write()
{
    // IDA case 5373958: PackedGUID + uint8(Result)
    _worldPacket << FixtureGuid;
    _worldPacket << uint8(Result);

    return &_worldPacket;
}

WorldPacket const* HousingFixtureDeleteFixtureResponse::Write()
{
    // IDA case 5373959: PackedGUID + uint8(Result)
    _worldPacket << FixtureGuid;
    _worldPacket << uint8(Result);

    return &_worldPacket;
}

// ============================================================
// Housing Room SMSG Responses (0x53xxxx)
// ============================================================

WorldPacket const* HousingRoomSetLayoutEditModeResponse::Write()
{
    // Sniff-verified (build 66838): PackedGUID(PlayerGuid) + uint8(Result) + uint8(bit7=Active)
    _worldPacket << PlayerGuid;
    _worldPacket << uint8(Result);
    _worldPacket << uint8(Active ? 0x80 : 0x00);

    return &_worldPacket;
}

WorldPacket const* HousingRoomAddResponse::Write()
{
    // Sniff-verified (build 66838): PackedGUID(PlayerGuid) + uint8(Result)
    _worldPacket << PlayerGuid;
    _worldPacket << uint8(Result);

    return &_worldPacket;
}

WorldPacket const* HousingRoomRemoveResponse::Write()
{
    // IDA case 5439490: PackedGUID + PackedGUID + uint8(Result)
    _worldPacket << RoomGuid;
    _worldPacket << PlayerGuid;
    _worldPacket << uint8(Result);

    return &_worldPacket;
}

WorldPacket const* HousingRoomUpdateResponse::Write()
{
    // IDA case 5439491: PackedGUID + uint8(Result)
    _worldPacket << RoomGuid;
    _worldPacket << uint8(Result);

    return &_worldPacket;
}

WorldPacket const* HousingRoomSetComponentThemeResponse::Write()
{
    // Sniff-verified: PackedGUID + uint32(arrayCount) + uint32(ThemeSetID) + uint8(Result) + uint32[arrayCount]
    _worldPacket << RoomGuid;
    _worldPacket << uint32(OptionIDs.size());
    _worldPacket << uint32(ThemeSetID);
    _worldPacket << uint8(Result);
    for (uint32 optId : OptionIDs)
        _worldPacket << uint32(optId);

    return &_worldPacket;
}

WorldPacket const* HousingRoomApplyComponentMaterialsResponse::Write()
{
    // Sniff-verified: PackedGUID + uint32(arrayCount) + uint32(TextureID) + uint8(Result) + uint32[arrayCount]
    // NOTE: ColorOverride is NOT echoed in response — only TextureID
    _worldPacket << RoomGuid;
    _worldPacket << uint32(OptionIDs.size());
    _worldPacket << uint32(RoomComponentTextureID);
    _worldPacket << uint8(Result);
    for (uint32 optId : OptionIDs)
        _worldPacket << uint32(optId);

    return &_worldPacket;
}

WorldPacket const* HousingRoomSetDoorTypeResponse::Write()
{
    // IDA case 5439494: PackedGUID + uint32(ComponentID) + uint8(DoorType) + uint8(Result)
    _worldPacket << RoomGuid;
    _worldPacket << uint32(ComponentID);
    _worldPacket << uint8(DoorType);
    _worldPacket << uint8(Result);

    return &_worldPacket;
}

WorldPacket const* HousingRoomSetCeilingTypeResponse::Write()
{
    // IDA case 5439495: PackedGUID + uint32(ComponentID) + uint8(CeilingType) + uint8(Result)
    _worldPacket << RoomGuid;
    _worldPacket << uint32(ComponentID);
    _worldPacket << uint8(CeilingType);
    _worldPacket << uint8(Result);

    return &_worldPacket;
}

// ============================================================
// Housing Services SMSG Responses (0x54xxxx)
// ============================================================

// Forward declarations for static helpers (defined after operator<< for HouseInfo)
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
    // IDA case 5505025: PackedGUID + uint8(flag) + uint8(nameLen) + String(nameLen)
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
    // IDA case 5505027: JamCliHouseFinderNeighborhood_base + uint8(trailing)
    WriteJamCliHouseFinderNeighborhoodBase(_worldPacket, Neighborhood);
    _worldPacket << uint8(TrailingResult);

    return &_worldPacket;
}

WorldPacket const* HousingSvcsNeighborhoodReservePlotResponse::Write()
{
    // Sniff-verified wire format: single uint8 Result (1 byte total)
    _worldPacket << uint8(Result);

    return &_worldPacket;
}

// Retired 2026-05-12 (batch 2): HousingSvcsClearPlotReservationResponse::Write — orphaned.

WorldPacket const* HousingSvcsRelinquishHouseResponse::Write()
{
    // IDA case 5505031: uint8(Result) + PackedGUID + PackedGUID
    _worldPacket << uint8(Result);
    _worldPacket << HouseGuid;
    _worldPacket << NeighborhoodGuid;

    return &_worldPacket;
}

WorldPacket const* HousingSvcsCancelRelinquishHouseResponse::Write()
{
    // IDA case 5505032: uint32 + PackedGUID + uint8
    _worldPacket << uint32(Field1);
    _worldPacket << HouseGuid;
    _worldPacket << uint8(Result);

    return &_worldPacket;
}

ByteBuffer& operator<<(ByteBuffer& data, HouseInfo const& houseInfo)
{
    // IDA (0x5C0008/0x5C0009): PackedGUID + PackedGUID + PackedGUID + uint8 + uint32
    //   + uint8(flags: bit 7 = HasMoveOutTime) [+ uint64 MoveOutTime]
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

// Helper: Write a JamCliHouse entry to the packet (IDA: Deserialize_ResidentArray, stride 80).
// Wire: PackedGUID(House) + PackedGUID(Owner) + PackedGUID(Neighborhood) + uint8 + uint32 + uint8(bit7=hasOpt) [+ uint64]
// Retail sniff confirms: GUID#1=HouseGUID, GUID#2=OwnerGUID (used for name lookup), GUID#3=NeighborhoodGUID.
static void WriteJamCliHouse(WorldPacket& packet, JamCliHouse const& house)
{
    // Wire: PackedGUID(House) + PackedGUID(Owner) + PackedGUID(Neighborhood)
    //     + uint8(PlotID) + uint32(HouseSettingFlags)
    //     + uint8(bit7 = HasReservationTime) [+ uint64(ReservationTime) if flag set]
    // Checked against a retail 12.1.0.69933 capture (PlotID 27 / HouseSettingFlags 1023). The byte used to carry
    // the house level and the uint32 the plot index, so every house reported plot 1 with settings = its plot.
    packet << house.HouseGUID;
    packet << house.OwnerGUID;
    packet << house.NeighborhoodGUID;
    packet << uint8(house.PlotIndex);
    packet << uint32(house.HouseSettingFlags);
    packet << uint8(house.HasOptionalField ? 0x80 : 0x00);
    if (house.HasOptionalField)
        packet << uint64(house.OptionalValue);

}

// Helper: Write JamCliHouseFinderNeighborhood BASE format.
// 12.1.0.69587 reader 0x7FF7CD4F70C0: PackedGUID + PackedGUID + uint64 + uint64 + uint32(housesCount)
//       + JamCliHouse[count] + uint8(nameLen, counts the NUL) + uint8(bit7=boolFlag) + String(nameLen)
// The houses come BEFORE the name length and flag; writing them after (the 12.0.7 order) put the house list's
// first bytes into the name length on every house-finder response with a house in it.
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

// Helper: Write JamCliHouseFinderNeighborhood FULL format (IDA: sub_7FF724C3F2C0, stride 136).
// Wire: base + uint64(ExtraField) + uint8(ExtraFlags)
static void WriteJamCliHouseFinderNeighborhood(WorldPacket& packet, JamCliHouseFinderNeighborhood const& entry)
{
    WriteJamCliHouseFinderNeighborhoodBase(packet, entry);
    packet << uint64(entry.ExtraField);
    packet << uint8(entry.ExtraFlags);
}

WorldPacket const* HousingSvcsGetPlayerHousesInfoResponse::Write()
{
    // IDA case 5505035: uint32(count) + uint8(result) + JamCliHouse[count]
    _worldPacket << uint32(Houses.size());
    _worldPacket << uint8(Result);
    for (auto const& house : Houses)
        WriteJamCliHouse(_worldPacket, house);

    return &_worldPacket;
}

WorldPacket const* HousingSvcsPlayerViewHousesResponse::Write()
{
    // IDA case 5505036: uint32(count) + uint8(result) + JamCliHouse[count]
    _worldPacket << uint32(Houses.size());
    _worldPacket << uint8(Result);
    for (auto const& house : Houses)
        WriteJamCliHouse(_worldPacket, house);

    return &_worldPacket;
}

WorldPacket const* HousingSvcsChangeHouseCosmeticOwner::Write()
{
    // IDA case 5505040: uint8(result) + PackedGUID + PackedGUID
    _worldPacket << uint8(Result);
    _worldPacket << HouseGuid;
    _worldPacket << NewOwnerGuid;

    return &_worldPacket;
}

WorldPacket const* HousingSvcsUpdateHousesLevelFavor::Write()
{
    // 12.0.7 (build 68275) LIST form (dispatcher 0x7FF7291F1920), RE feedback 0x540011:
    //   u8 Result + u32 ChangeAmount + u32 Reason + u32 count
    //   + count x { 3x PackedGUID, int64 NewFavorTotal, u8 Field3, u32 Reserved, u8(bit7 Flag) }
    // The 12.0.5 "flat record" was a 1-element list whose count was mislabeled Field2(=1);
    // for a single house this emits identical bytes to the old sniff-validated capture.
    _worldPacket << uint8(Result);
    _worldPacket << uint32(ChangeAmount);
    _worldPacket << uint32(Reason);
    _worldPacket << uint32(Houses.size());
    for (HouseLevelFavor const& house : Houses)
    {
        _worldPacket << house.BnetAccount;
        _worldPacket << house.NeighborhoodGUID;
        _worldPacket << house.HouseGUID;
        _worldPacket << int32(house.HouseLevel);            // @48 (was low dword of int64 NewFavorTotal)
        _worldPacket << int32(house.FavorValue);            // @52 (was high dword)
        _worldPacket << uint8(house.UpdateSource);          // u8 @57 (low byte of in-mem uint32)
        _worldPacket << uint32(house.SourceDataDecorID);    // u32 @60 (sourceData.decorID)
        _worldPacket << uint8(house.IsAdditive ? 0x80 : 0x00); // trailing byte, bit7 @56
    }

    return &_worldPacket;
}

WorldPacket const* HousingSvcsGuildAddHouseNotification::Write()
{
    // IDA case 5505042: JamCliHouse
    WriteJamCliHouse(_worldPacket, House);

    return &_worldPacket;
}

WorldPacket const* HousingSvcsGuildRemoveHouseNotification::Write()
{
    // IDA case 5505043: JamCliHouse
    WriteJamCliHouse(_worldPacket, House);

    return &_worldPacket;
}

// Retired 2026-05-12 (batch 2): HousingSvcsGuildAppendNeighborhoodNotification::Write — orphaned.

WorldPacket const* HousingSvcsGuildRenameNeighborhoodNotification::Write()
{
    // IDA case 5505045: uint8(nameLen) + String(nameLen) — NO GUID
    uint8 nameLen = static_cast<uint8>(std::min<size_t>(NewName.size() + 1, 255));
    _worldPacket << uint8(nameLen);
    if (nameLen > 0)
        _worldPacket.append(NewName.c_str(), nameLen);

    return &_worldPacket;
}

WorldPacket const* HousingSvcsGuildGetHousingInfoResponse::Write()
{
    // IDA case 5505046: uint32(count) + JamCliHouseFinderNeighborhood_base[count]
    _worldPacket << uint32(Neighborhoods.size());
    for (auto const& entry : Neighborhoods)
        WriteJamCliHouseFinderNeighborhoodBase(_worldPacket, entry);

    return &_worldPacket;
}

WorldPacket const* HousingSvcsAcceptNeighborhoodOwnershipResponse::Write()
{
    // IDA case 5505047: uint8 only (error check, shows ERR_HOUSING_RESULT_GENERIC_FAILURE if non-zero)
    _worldPacket << uint8(Result);

    return &_worldPacket;
}

WorldPacket const* HousingSvcsRejectNeighborhoodOwnershipResponse::Write()
{
    // IDA case 5505048: uint8 only
    _worldPacket << uint8(Result);

    return &_worldPacket;
}

WorldPacket const* HousingSvcsNeighborhoodOwnershipTransferredResponse::Write()
{
    // IDA case 5505049: bit-packed blob via ai_Decode_ClientOpcodeData
    // Client reads first byte: top 6 bits = blobSize, bottom 2 bits cached
    // Then reads blobSize raw bytes into a 49-byte struct (3×16-byte raw GUIDs + 1 byte)
    if (Result == 0)
    {
        uint8 blobSize = 49; // 3×16-byte raw ObjectGuids + 1 byte
        _worldPacket << uint8((blobSize << 2) | (Result & 0x03));
        _worldPacket.append(OwnerGUID.GetRawValue().data(), 16);
        _worldPacket.append(HouseGUID.GetRawValue().data(), 16);
        _worldPacket.append(AccountGUID.GetRawValue().data(), 16);
        _worldPacket << uint8(HouseLevel);
    }
    else
    {
        _worldPacket << uint8((Result & 0x03));
    }

    return &_worldPacket;
}

WorldPacket const* HousingSvcsGetPotentialHouseOwnersResponse::Write()
{
    // 12.0.5 sniff-verified (1451-byte packet, 41 entries):
    //   uint32      Count
    //   Per entry:
    //     PackedGUID PlayerGuid
    //     uint32     Field1
    //     uint8      AccessLevel
    //     uint8      lenByte1   = nameLen >> 1   (high 7 bits of length)
    //     uint8      lenByte2   = (nameLen & 1) << 7   (low bit at bit 7, rest unused)
    //     char[nameLen] name    (NO null terminator on the wire)
    //
    // Verified against sniff entries:
    //   "Anondk-AltarofStorms"     (20 chars) → lenByte1=0x0A, lenByte2=0x00
    //   "Dahuntermon-AltarofStorms" (25 chars) → lenByte1=0x0C, lenByte2=0x80
    //
    // The previous +1 (include NUL) shifted the encoding: for a 20-char name we
    // emitted lenByte1=0x0A, lenByte2=0x80 (encoding 21) and appended a trailing
    // zero byte. The client then read 21 chars but the next entry's GUID mask
    // landed inside the name buffer — corrupted ownership/access display.
    _worldPacket << uint32(PotentialOwners.size());
    for (auto const& owner : PotentialOwners)
    {
        _worldPacket << owner.PlayerGuid;
        _worldPacket << uint32(owner.ClassID);
        _worldPacket << uint8(owner.Error);
        uint32 nameLen = static_cast<uint32>(owner.CharacterName.size());
        uint8 lenByte1 = static_cast<uint8>(nameLen >> 1);
        uint8 lenByte2 = static_cast<uint8>((nameLen & 1) << 7);
        _worldPacket << uint8(lenByte1);
        _worldPacket << uint8(lenByte2);
        if (nameLen > 0)
            _worldPacket.append(owner.CharacterName.c_str(), nameLen);
    }

    return &_worldPacket;
}

WorldPacket const* HousingSvcsUpdateHouseSettingsResponse::Write()
{
    // uint8(Result) + JamCliHouse. Retail 12.1.0.69933 (33 bytes) after the three GUIDs: 1B FF 03 00 00 00 =
    // PlotID 27, HouseSettingFlags 1023, no reservation time.
    JamCliHouse house = House;
    house.HouseSettingFlags = SettingsFlags;
    _worldPacket << uint8(Result);
    WriteJamCliHouse(_worldPacket, house);

    return &_worldPacket;
}

WorldPacket const* HousingSvcsGetHouseFinderInfoResponse::Write()
{
    // IDA-verified wire (build 67186, sub_7FF75C1EA710 case 0x54001C):
    //   Bits<1>(Result) + FlushBits + uint32(count) + ParseHouseFinderNeighborhood[count]
    // The leading bit is the success/failure flag; old TC missed it entirely. The
    // count is a raw uint32 (helper is misnamed CompressedUInt32 but reads 4 bytes).
    _worldPacket.WriteBit(Result != 0);
    _worldPacket.FlushBits();
    _worldPacket << uint32(Entries.size());
    for (auto const& entry : Entries)
        WriteJamCliHouseFinderNeighborhood(_worldPacket, entry);

    return &_worldPacket;
}

WorldPacket const* HousingSvcsGetHouseFinderNeighborhoodResponse::Write()
{
    // IDA-verified wire (case 5505053): Bits<1>(Result) + FlushBits + ParseHouseFinderNeighborhood (single, 136 bytes)
    // Old TC wrote uint8(Result); the leading byte is actually a single bit. Sending
    // Result=non-zero as 0x01 left bit 7 = 0, so the client always read "no error"
    // regardless of actual Result. Same fix pattern as the charter responses.
    _worldPacket.WriteBit(Result != 0);
    _worldPacket.FlushBits();
    WriteJamCliHouseFinderNeighborhood(_worldPacket, Neighborhood);


    return &_worldPacket;
}

WorldPacket const* HousingSvcsGetBnetFriendNeighborhoodsResponse::Write()
{
    // IDA case 5505054: uint8(Result) + uint32(count) + JamCliHouseFinderNeighborhood[count]
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
    // IDA case 5505057: uint8 only
    _worldPacket << uint8(Result);

    return &_worldPacket;
}

// ============================================================
// Housing General SMSG Responses (0x55xxxx)
// ============================================================

// Helper: Write InviteEntry payload (IDA Housing_ParseInviteEntry, sub_7FF75C1ACB90).
// Wire: uint64 + PackedGUID + PackedGUID + uint64.
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

// Retired 2026-05-11: HousingSystemHouseSnapshotResponse Write() deleted (no C_HouseSnapshot in retail).

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
    // u8 HousingResult, 0 = success (reader: dispatcher 0x7FF7CD536260 case 0x590007)
    _worldPacket << uint8(Result);

    return &_worldPacket;
}

// Retired 2026-05-11: HousingEditorAvailabilityResponse Write() deleted (Lua API is sync).

// Retired 2026-05-12: HousingUpdateHouseInfo::Write — orphaned after UPDATE_HOUSE_INFO CMSG retirement.
// SMSG 0x550004 is real per IDA, but the only emit-site was a handler with no client sender.

// Retired 2026-05-11: SMSG_HOUSING_SET_HOUSE_NAME_RESPONSE class deleted (was using fake
// opcode 0xF1000008 + had 0 emit-sites). IDA-derived real opcode is 0x550005 with wire:
//   uint8 Result + uint64 NameLen + char[NameLen] Name
// Recreate if/when the set-house-name response gets wired into a handler.

// ============================================================
// Account/Licensing SMSG (0x42xxxx / 0x5Fxxxx)
// ============================================================

// Shared writer for all five SMSG_ACCOUNT_*_COLLECTION_UPDATE opcodes.
// IDA-verified wire (build 67186, sub_7FF75C0C76F0):
//   uint8  byte                        (top bit = IsIncrementalUpdate)
//   uint32 IDs.size()
//   uint32 StateFlags.size()
//   uint32 IDs[IDs.size()]
//   Bits<1> StateFlags[StateFlags.size()]   (8 per byte, bit 7 first)
//
// Bit and byte streams are byte-aligned in this opcode — no FlushBits
// is needed because the integer reads happen before the bit loop.
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

// ============================================================
// Decor Licensing/Refund SMSG Responses (0x42xxxx)
// ============================================================

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
    uint32 count = 0;
    _worldPacket >> count;

    // Sane limit — retail client UI limits to the refund window (2h) worth of decor
    if (count > 500)
        count = 500;

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
    // IDA-verified wire (build 67186, sub_7FF75C0EEE00 + sub_7FF75C198A60).
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
    // IDA-verified wire (build 67186, sub_7FF75C0EEF70). NO leading Result byte.
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
    // 12.1.0.69587: the client takes the whole body as one blob (Handler_SMSG_INITIATIVE_TASK_COMPLETE 0x7FF7CD3E0FD0) and
    // hands it to a hook that is unset in the retail image; retail sends a single u32, the task (203 in the one captured
    // frame, gulfofmemorydelve 69497).
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
    // IDA-verified wire (build 67186, sub_7FF75C0EF0C0): uint32 + ObjectGuid + ObjectGuid.
    _worldPacket << uint32(Result);
    _worldPacket << SourceGuid;
    _worldPacket << TargetGuid;

    return &_worldPacket;
}

WorldPacket const* InitiativeRewardAvailable::Write()
{
    // IDA-verified wire (build 67186, sub_7FF75C0EF180): uint32(count) + ObjectGuid[count].
    _worldPacket << uint32(RewardGuids.size());
    for (ObjectGuid const& guid : RewardGuids)
        _worldPacket << guid;

    return &_worldPacket;
}

// Retired 2026-05-11: InitiativeUpdateStatus / InitiativePointsUpdate / InitiativeMilestoneUpdate
// / InitiativeChestResult Write() bodies deleted (speculative 0xF1000018..0xF100001C opcodes).
// See HousingPackets.h for the wire shapes (preserved as comments for future restoration).

WorldPacket const* HousingPhotoSharingAuthorizationResult::Write()
{
    // IDA-verified wire (build 67186, sub_7FF75C0F0160):
    //   uint8 Result, uint8 (length << 1), char[length]
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
    // IDA-verified wire, unchanged 67186 -> 68275 (68275 deserializer sub_7FF7290B9C90):
    //   PackedGUID Guid (the clerk creature)
    //   uint8 Flags  — bit 0x80 = Field0, bit 0x40 = OpenForBusiness
    // The client reads the flags with a whole-byte ReadUInt8, so a plain uint8 is byte-exact.
    _worldPacket << Guid;
    uint8 flags = 0;
    if (Field0) flags |= 0x80;
    if (OpenForBusiness) flags |= 0x40;
    _worldPacket << uint8(flags);

    return &_worldPacket;
}

WorldPacket const* GuildOthersOwnedHousesResult::Write()
{
    // IDA-verified wire (build 67186, dispatcher case 5111879):
    //   uint8 Result + PackedGUID GuildGuid + uint32 count + HouseInfoStruct[count]
    _worldPacket << uint8(Result);
    _worldPacket << GuildGuid;
    _worldPacket << uint32(Houses.size());
    for (auto const& house : Houses)
        WriteJamCliHouse(_worldPacket, house);

    return &_worldPacket;
}

WorldPacket const* HousingSvcsNeighborhoodUpdateNameNotification::Write()
{
    // IDA-verified wire (build 67186, sub_7FF75C1EA710 case 0x540023):
    //   PackedGUID NeighborhoodGuid + Bits<8>(nameLen) + char[nameLen] NewName
    // The size prefix is read by sub_7FF75C0A9650 as 8 packed bits, not 7.
    _worldPacket << NeighborhoodGuid;
    _worldPacket << SizedString::BitsSize<8>(NewName);
    _worldPacket.FlushBits();
    _worldPacket << SizedString::Data(NewName);

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

// Retired 2026-05-12: NeighborhoodCharterSignResponsePacket::Read (fake CMSG 0x370002).
// Retired 2026-05-12: NeighborhoodCharterRemoveSignature::Read (fake CMSG 0x370005).

// --- Neighborhood Management System ---

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
    // IDA-verified wire (build 67186, sub_7FF75C177630): 2 PackedGUIDs.
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

// ============================================================
// Neighborhood Charter SMSG Responses (0x5Bxxxx)
// ============================================================

WorldPacket const* NeighborhoodCharterUpdateResponse::Write()
{
    // 12.0.7 (build 68275): leading field is a full uint8(Result) status code (the client
    // reads a whole byte and tests != 0), NOT a single bit. RE feedback 0x5b0000.
    //   uint8 Result + ObjectGuid CharterGuid + uint32 MapID + uint32 SignatureCount
    //   + uint32 SignersCount + uint32 Unknown + ObjectGuid[SignersCount]
    //   + uint8(NameLen) + StringData
    _worldPacket << uint8(Result);
    _worldPacket << CharterGuid;
    _worldPacket << uint32(MapID);
    _worldPacket << uint32(SignatureCount);
    _worldPacket << uint32(Signers.size());
    _worldPacket << uint32(Unknown);
    for (ObjectGuid const& signer : Signers)
        _worldPacket << signer;
    // M5: charter name is length-prefixed size+1 with a trailing NUL
    // (retail rec 14982 = 09 'Colombia' 00), matching the Roster/HouseFinder
    // writers. The old uint8(size)+WriteString dropped the terminator.
    {
        uint8 charterNameLen = static_cast<uint8>(std::min<size_t>(NeighborhoodName.size() + 1, 255));
        _worldPacket << uint8(charterNameLen);
        _worldPacket.append(NeighborhoodName.c_str(), charterNameLen);
    }

    return &_worldPacket;
}

WorldPacket const* NeighborhoodCharterOpenUIResponse::Write()
{
    // 12.0.7 (build 68275): identical shape to 0x5B0000; leading field is a full uint8(Result),
    // not a bit. RE feedback 0x5b0001.
    _worldPacket << uint8(Result);
    _worldPacket << CharterGuid;
    _worldPacket << uint32(MapID);
    _worldPacket << uint32(SignatureCount);
    _worldPacket << uint32(Signers.size());
    _worldPacket << uint32(Unknown);
    for (ObjectGuid const& signer : Signers)
        _worldPacket << signer;
    // M5: charter name is length-prefixed size+1 with a trailing NUL
    // (retail rec 14982 = 09 'Colombia' 00), matching the Roster/HouseFinder
    // writers. The old uint8(size)+WriteString dropped the terminator.
    {
        uint8 charterNameLen = static_cast<uint8>(std::min<size_t>(NeighborhoodName.size() + 1, 255));
        _worldPacket << uint8(charterNameLen);
        _worldPacket.append(NeighborhoodName.c_str(), charterNameLen);
    }

    return &_worldPacket;
}

WorldPacket const* NeighborhoodCharterSignRequest::Write()
{
    // IDA 0x5B0002: uint8 + PackedGUID + uint32 + uint32 + uint8(nameLen) + string
    _worldPacket << uint8(Result);
    _worldPacket << CharterGuid;
    _worldPacket << uint32(MapID);
    _worldPacket << uint32(Unknown);
    // M5: charter name is length-prefixed size+1 with a trailing NUL
    // (retail rec 14982 = 09 'Colombia' 00), matching the Roster/HouseFinder
    // writers. The old uint8(size)+WriteString dropped the terminator.
    {
        uint8 charterNameLen = static_cast<uint8>(std::min<size_t>(NeighborhoodName.size() + 1, 255));
        _worldPacket << uint8(charterNameLen);
        _worldPacket.append(NeighborhoodName.c_str(), charterNameLen);
    }

    return &_worldPacket;
}

WorldPacket const* NeighborhoodCharterAddSignatureResponse::Write()
{
    // IDA 0x5B0003: uint8 + PackedGUID only
    _worldPacket << uint8(Result);
    _worldPacket << CharterGuid;

    return &_worldPacket;
}

WorldPacket const* NeighborhoodCharterOpenConfirmationUIResponse::Write()
{
    // IDA 0x5B0004: uint8 + uint32 + uint32 + uint8(nameLen) + string
    _worldPacket << uint8(Result);
    _worldPacket << uint32(Field1);
    _worldPacket << uint32(Field2);
    // M5: charter name is length-prefixed size+1 with a trailing NUL
    // (retail rec 14982 = 09 'Colombia' 00), matching the Roster/HouseFinder
    // writers. The old uint8(size)+WriteString dropped the terminator.
    {
        uint8 charterNameLen = static_cast<uint8>(std::min<size_t>(NeighborhoodName.size() + 1, 255));
        _worldPacket << uint8(charterNameLen);
        _worldPacket.append(NeighborhoodName.c_str(), charterNameLen);
    }

    return &_worldPacket;
}

WorldPacket const* NeighborhoodCharterSignatureRemovedNotification::Write()
{
    // IDA 0x5B0005: PackedGUID only
    _worldPacket << CharterGuid;

    return &_worldPacket;
}

// ============================================================
// Neighborhood Management SMSG Responses (0x5Cxxxx)
// ============================================================

// NeighborhoodPlayerEnterPlot / NeighborhoodPlayerLeavePlot Write() removed in 12.0.5.
// Plot occupancy is now communicated via PlayerHouseInfoComponentData.CurrentHouse.

WorldPacket const* NeighborhoodEvictPlayerResponse::Write()
{
    // UNVERIFIED — needs live sniff. 12.0.7 client consumes this body as opaque bytes[rest]
    // without decoding fields, so the internal layout cannot be confirmed offline (RE 0x5c0000).
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
    // Wire format verified against retail 12.0.1 build 65940 packet captures (Alliance + Horde)
    // IDA deserializer sub_7FF6F6E3E200: uint32→+32, GUID→+40, GUID→+56, uint64→+72, uint8→+80, GUID→+128
    // Fixed fields
    _worldPacket << uint32(PlotIndex);          // Echoed from CMSG (NOT a result code)
    _worldPacket << PlotOwnerGuid;              // →+40: Player GUID when owned, Empty when unclaimed
    _worldPacket << NeighborhoodGuid;           // →+56: Housing GUID when owned, Empty when unclaimed
    _worldPacket << uint64(Cost);               // →+72: Purchase price
    _worldPacket << uint8(PurchaseStatus);      // →+80: 73=purchasable, 0=not. Client checks ==73
    _worldPacket << CornerstoneGuid;            // →+128: Cornerstone game object

    // Bit-packed section: 1 bool + 8-bit nameLen + 6 bools = 15 bits = 2 bytes.
    // Per IDA Housing_ParseCornerstoneHouseInfo, bit B.bit4 gates an embedded
    // Housing_ParseHouseInfoStruct that the cornerstone Lua reads as "the player
    // already has a current house" — used to flip the cornerstone button from Buy
    // to Move.
    bool const hasExistingHouse = ExistingHouse.has_value();
    _worldPacket << Bits<1>(IsPlotOwned);
    _worldPacket << SizedCString::BitsSize<8>(NeighborhoodName);
    _worldPacket << OptionalInit(AlternatePrice);
    _worldPacket << Bits<1>(CanPurchase);
    _worldPacket.WriteBit(hasExistingHouse);    // B.bit4 — HasOptionalStruct
    _worldPacket << Bits<1>(HasResidents);
    _worldPacket << OptionalInit(StatusValue);
    _worldPacket << Bits<1>(IsInitiative);
    _worldPacket.FlushBits();

    // Variable-length data, 12.1.0.69587 reader 0x7FF7CD51B190: name, then the optional price, then the optional
    // embedded HouseInfo, then the optional status value. (The 12.0.x decode order put the house before the name;
    // on 12.1 that shifted every byte of the response whenever the player already owned a house.)
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
    // 12.0.7 (build 68275): leading uint8(Result) read via ClientOpcode_helper_318EF90,
    // THEN the GUID (the invited player's guid client-side). RE feedback 0x5c000a.
    _worldPacket << uint8(Result);
    _worldPacket << NeighborhoodGuid;

    return &_worldPacket;
}

WorldPacket const* NeighborhoodPlayerGetInviteResponse::Write()
{
    // IDA-verified wire (build 67186, 0x5C000B): uint8 Result + InviteEntry(48 bytes).
    _worldPacket << uint8(Result);
    Housing::WriteInviteEntry(_worldPacket, Entry);

    return &_worldPacket;
}

WorldPacket const* NeighborhoodGetInvitesResponse::Write()
{
    // IDA-verified wire (build 67186, 0x5C000C): uint8 Result + uint32 Count + InviteEntry[Count].
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
    // 12.1.0.69587 dispatcher 0x7FF7CD51B490 case 0x60000F (UPDATE_BULLETIN_BOARD_ROSTER):
    //   u8 Result ;
    //   u32 count, count x neighborhood (reader 0x7FF7CD4F71D0) { guid, guid, u64, u64, u32 houses, houses x JamCliHouse,
    //                                                           u8 nameLen (counts the NUL), u8 flags, name } ;
    //   u32 count, count x NeighborhoodRosterMemberUpdateInfo { guid, u8 residentType, bit isOnline } ;
    //   bit HasGuid, [guid]
    // The houses give the client each resident's plot (NeighborhoodRosterMemberInfo.plotID), the second list their
    // type and online state.
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
    // 12.1.0.69587 dispatcher 0x7FF7CD51B490 case 0x600010: u32 count, count x { guid, u8 residentType, bit isOnline }.
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
    // 12.0.7 (build 68275): leading uint8(Result) via ClientOpcode_helper_318EF90, then the GUID.
    // RE feedback 0x5c0011.
    _worldPacket << uint8(Result);
    _worldPacket << PlayerGuid;

    return &_worldPacket;
}

WorldPacket const* NeighborhoodEvictPlotResponse::Write()
{
    // 12.0.7 (build 68275): leading uint8(Result) via ClientOpcode_helper_318EF90, then the GUID
    // (client treats it as the evicted plot/house guid). RE feedback 0x5c0012.
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

// --- Initiative System ---

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
