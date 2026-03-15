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

#ifndef TRINITYCORE_HOUSING_PACKETS_H
#define TRINITYCORE_HOUSING_PACKETS_H

#include "ObjectGuid.h"
#include "Optional.h"
#include "Packet.h"
#include "Position.h"
#include <string>
#include <vector>

namespace WorldPackets::Housing
{
    // ============================================================
    // Shared Structs
    // ============================================================

    // HouseInfo — IDA: PackedGUID + PackedGUID + PackedGUID + uint8 + uint32
    //   + uint8(bit7=HasMoveOutTime) [+ uint64(MoveOutTime)]
    struct HouseInfo
    {
        ObjectGuid HouseGuid;
        ObjectGuid OwnerGuid;
        ObjectGuid NeighborhoodGuid;
        uint8 PlotId = 0;
        uint32 AccessFlags = 0;
        bool HasMoveOutTime = false;
        uint64 MoveOutTime = 0;
    };

    // JamNeighborhoodRosterEntry — sub_7FF6F6E0A460 (48 bytes, used by 0x5C000E, 0x5C000F)
    // Wire order: uint64 + PackedGUID + PackedGUID + uint64
    struct JamNeighborhoodRosterEntry
    {
        uint64 Timestamp = 0;
        ObjectGuid PlayerGuid;
        ObjectGuid HouseGuid;
        uint64 ExtraData = 0;
    };

    // IDA-verified wire format for house/resident entries nested inside neighborhood data.
    // Deserializer: Deserialize_ResidentArray (0x7FF724C3EEF0), stride 80 bytes in memory.
    // Wire order: PackedGUID(House) + PackedGUID(Owner) + PackedGUID(Neighborhood) + uint8 + uint32 + uint8(bit7=hasOpt) [+ uint64]
    // IDA proof: offset-0 GUID compared vs house records; offset-16 GUID passed to ai_Process_PlayerContextUpdate (name lookup).
    struct JamCliHouse
    {
        ObjectGuid HouseGUID;            // wire pos 1, client offset 0: house entity GUID (compared vs HouseInfo records)
        ObjectGuid OwnerGUID;            // wire pos 2, client offset 16: owner player GUID (used for name lookup)
        ObjectGuid NeighborhoodGUID;     // wire pos 3, client offset 32: neighborhood GUID
        uint8 HouseLevel = 0;           // client offset 48
        uint32 PlotIndex = 0;           // client offset 72
        bool HasOptionalField = false;   // client offset 64: derived from bit 7 of wire uint8
        uint64 OptionalValue = 0;        // client offset 56: only present if HasOptionalField
    };

    // IDA-verified wire format for neighborhood entries in house finder responses.
    // Deserializer: sub_7FF724C3F2C0 = sub_7FF724C3F040 (base) + 2 extra fields, stride 136 bytes.
    // Base wire (sub_7FF724C3F040):
    //   PackedGUID(NeighborhoodGUID) + PackedGUID(OwnerGUID) + uint64 + uint64
    //   + uint32(HousesCount) + uint8(NameLen) + uint8(bit7=BoolFlag)
    //   + JamCliHouse[HousesCount] + String(NameLen)
    // Extra (sub_7FF724C3F2C0):
    //   + uint64(ExtraField) + uint8(ExtraFlags)
    struct JamCliHouseFinderNeighborhood
    {
        ObjectGuid NeighborhoodGUID;     // offset 0
        ObjectGuid OwnerGUID;            // offset 16
        std::string Name;                // offset 32 (pointer+len)
        uint64 Field1 = 0;              // offset 80 (e.g. available|total plots packed)
        uint64 Field2 = 0;              // offset 88 (e.g. mapID)
        bool BoolFlag = false;           // offset 72 (written as bit 7 of a uint8)
        std::vector<JamCliHouse> Houses; // offset 96 (dynamic array)
        uint64 ExtraField = 0;           // offset 120 (from sub_7FF724C3F2C0)
        uint8 ExtraFlags = 0;            // offset 128 (from sub_7FF724C3F2C0)

        void SetPlotCounts(uint32 available, uint32 total)
        {
            Field1 = (static_cast<uint64>(total) << 32) | static_cast<uint64>(available);
        }
    };

    // ============================================================
    // House Exterior System (0x2Exxxx)
    // ============================================================

    class HouseExteriorCommitPosition final : public ClientPacket
    {
    public:
        explicit HouseExteriorCommitPosition(WorldPacket&& packet) : ClientPacket(CMSG_HOUSE_EXTERIOR_SET_HOUSE_POSITION, std::move(packet)) { }

        void Read() override;

        bool HasPosition = false;
        ObjectGuid HouseGuid;
        float PositionX = 0.0f;
        float PositionY = 0.0f;
        float PositionZ = 0.0f;
        float RotationX = 0.0f;
        float RotationY = 0.0f;
        float RotationZ = 0.0f;
        float RotationW = 1.0f;
    };

    class HouseExteriorLock final : public ClientPacket
    {
    public:
        explicit HouseExteriorLock(WorldPacket&& packet) : ClientPacket(CMSG_HOUSE_EXTERIOR_LOCK, std::move(packet)) { }

        void Read() override;

        ObjectGuid HouseGuid;
        ObjectGuid PlotGuid;
        ObjectGuid NeighborhoodGuid;
        bool Locked = false;
    };

    // ============================================================
    // House Interior System (0x2Fxxxx)
    // ============================================================

    class HouseInteriorEnterHouse final : public ServerPacket
    {
    public:
        HouseInteriorEnterHouse() : ServerPacket(SMSG_HOUSE_INTERIOR_ENTER_HOUSE) { }
        WorldPacket const* Write() override;
        // IDA: CliHouseInteriorSystem sub 0 (0x2F0000)
        // Wire format TBD (needs sniff verification) — sending HouseGuid for now
        ObjectGuid HouseGuid;
    };

    class HouseInteriorLeaveHouse final : public ClientPacket
    {
    public:
        explicit HouseInteriorLeaveHouse(WorldPacket&& packet) : ClientPacket(CMSG_HOUSE_INTERIOR_LEAVE_HOUSE, std::move(packet)) { }

        void Read() override { }
    };

    class HouseInteriorLeaveHouseResponse final : public ServerPacket
    {
    public:
        HouseInteriorLeaveHouseResponse() : ServerPacket(SMSG_HOUSE_INTERIOR_LEAVE_HOUSE_RESPONSE) { }
        WorldPacket const* Write() override;

        // IDA: CliHouseInteriorSystem sub 1 (0x2F0001)
        // HousingTeleportReason: None(0), Cheat(1), UnspecifiedSpellcast(2), Booted(3),
        //   Homestone(4), Visit(5), Friend(6), GuildMember(7), PartyMember(8),
        //   ExitingHouse(9), Portal(10), Tutorial(11)
        uint8 TeleportReason = 9; // Default: ExitingHouse
    };

    // ============================================================
    // Decor System (0x30xxxx)
    // ============================================================

    class HousingDecorSetEditMode final : public ClientPacket
    {
    public:
        explicit HousingDecorSetEditMode(WorldPacket&& packet) : ClientPacket(CMSG_HOUSING_DECOR_SET_EDIT_MODE, std::move(packet)) { }

        void Read() override;

        bool Active = false;
    };

    class HousingDecorPlace final : public ClientPacket
    {
    public:
        explicit HousingDecorPlace(WorldPacket&& packet) : ClientPacket(CMSG_HOUSING_DECOR_PLACE, std::move(packet)) { }

        void Read() override;

        ObjectGuid DecorGuid;
        TaggedPosition<Position::XYZ> Position;
        TaggedPosition<Position::XYZ> Rotation;
        float Scale = 1.0f;
        ObjectGuid AttachParentGuid;
        ObjectGuid RoomGuid;
        uint8 Field_61 = 0;
        uint8 Field_62 = 0;
        int32 Field_63 = -1;
    };

    class HousingDecorMove final : public ClientPacket
    {
    public:
        explicit HousingDecorMove(WorldPacket&& packet) : ClientPacket(CMSG_HOUSING_DECOR_MOVE, std::move(packet)) { }

        void Read() override;

        ObjectGuid DecorGuid;
        TaggedPosition<Position::XYZ> Position;
        TaggedPosition<Position::XYZ> Rotation;
        float Scale = 1.0f;
        ObjectGuid AttachParentGuid;
        ObjectGuid RoomGuid;
        ObjectGuid Field_70;
        int32 Field_80 = 0;
        uint8 Field_85 = 0;
        uint8 Field_86 = 0;
        bool IsBasicMove = false;
    };

    class HousingDecorRemove final : public ClientPacket
    {
    public:
        explicit HousingDecorRemove(WorldPacket&& packet) : ClientPacket(CMSG_HOUSING_DECOR_REMOVE, std::move(packet)) { }

        void Read() override;

        ObjectGuid DecorGuid;
    };

    class HousingDecorLock final : public ClientPacket
    {
    public:
        explicit HousingDecorLock(WorldPacket&& packet) : ClientPacket(CMSG_HOUSING_DECOR_LOCK, std::move(packet)) { }

        void Read() override;

        ObjectGuid DecorGuid;
        bool Locked = false;
    };

    class HousingDecorSetDyeSlots final : public ClientPacket
    {
    public:
        explicit HousingDecorSetDyeSlots(WorldPacket&& packet) : ClientPacket(CMSG_HOUSING_DECOR_SET_DYE_SLOTS, std::move(packet)) { }

        void Read() override;

        ObjectGuid DecorGuid;
        std::array<int32, 3> DyeColorID = {};
    };

    class HousingDecorDeleteFromStorage final : public ClientPacket
    {
    public:
        explicit HousingDecorDeleteFromStorage(WorldPacket&& packet) : ClientPacket(CMSG_HOUSING_DECOR_DELETE_FROM_STORAGE, std::move(packet)) { }

        void Read() override;

        std::vector<ObjectGuid> DecorGuids;
    };

    class HousingDecorDeleteFromStorageById final : public ClientPacket
    {
    public:
        explicit HousingDecorDeleteFromStorageById(WorldPacket&& packet) : ClientPacket(CMSG_HOUSING_DECOR_DELETE_FROM_STORAGE_BY_ID, std::move(packet)) { }

        void Read() override;

        uint32 DecorRecID = 0;
    };

    class HousingDecorRequestStorage final : public ClientPacket
    {
    public:
        explicit HousingDecorRequestStorage(WorldPacket&& packet) : ClientPacket(CMSG_HOUSING_DECOR_REQUEST_STORAGE, std::move(packet)) { }

        void Read() override;

        ObjectGuid HouseGuid;
    };

    class HousingDecorRedeemDeferredDecor final : public ClientPacket
    {
    public:
        explicit HousingDecorRedeemDeferredDecor(WorldPacket&& packet) : ClientPacket(CMSG_HOUSING_DECOR_REDEEM_DEFERRED_DECOR, std::move(packet)) { }

        void Read() override;

        uint32 DeferredDecorID = 0;
        uint32 RedemptionToken = 0;
    };

    class HousingDecorStartPlacingNewDecor final : public ClientPacket
    {
    public:
        explicit HousingDecorStartPlacingNewDecor(WorldPacket&& packet) : ClientPacket(CMSG_HOUSING_DECOR_START_PLACING_NEW_DECOR, std::move(packet)) { }

        void Read() override;

        uint32 CatalogEntryID = 0;
        uint32 Field_4 = 0;
    };

    class HousingDecorCatalogCreateSearcher final : public ClientPacket
    {
    public:
        explicit HousingDecorCatalogCreateSearcher(WorldPacket&& packet) : ClientPacket(CMSG_HOUSING_DECOR_CATALOG_CREATE_SEARCHER, std::move(packet)) { }

        void Read() override;

        ObjectGuid Owner;
    };

    class GetLastCatalogFetch final : public ClientPacket
    {
    public:
        explicit GetLastCatalogFetch(WorldPacket&& packet) : ClientPacket(CMSG_GET_LAST_CATALOG_FETCH, std::move(packet)) { }
        void Read() override { }
    };

    class UpdateLastCatalogFetch final : public ClientPacket
    {
    public:
        explicit UpdateLastCatalogFetch(WorldPacket&& packet) : ClientPacket(CMSG_UPDATE_LAST_CATALOG_FETCH, std::move(packet)) { }
        void Read() override { }
    };

    class LastCatalogFetchResponse final : public ServerPacket
    {
    public:
        LastCatalogFetchResponse() : ServerPacket(SMSG_LAST_CATALOG_FETCH_RESPONSE) { }
        WorldPacket const* Write() override;
        // Sniff-verified: 8-byte payload = uint64 Unix timestamp
        uint64 Timestamp = 0;
    };

    class HousingDecorUpdateDyeSlot final : public ClientPacket
    {
    public:
        explicit HousingDecorUpdateDyeSlot(WorldPacket&& packet) : ClientPacket(CMSG_HOUSING_DECOR_UPDATE_DYE_SLOT, std::move(packet)) { }
        void Read() override;
        ObjectGuid DecorGuid;
        uint8 SlotIndex = 0;
        uint32 DyeColorID = 0;
    };

    class HousingDecorStartPlacingFromSource final : public ClientPacket
    {
    public:
        explicit HousingDecorStartPlacingFromSource(WorldPacket&& packet) : ClientPacket(CMSG_HOUSING_DECOR_START_PLACING_FROM_SOURCE, std::move(packet)) { }
        void Read() override;
        uint32 SourceType = 0;
        uint32 SourceID = 0;
    };

    class HousingDecorCleanupModeToggle final : public ClientPacket
    {
    public:
        explicit HousingDecorCleanupModeToggle(WorldPacket&& packet) : ClientPacket(CMSG_HOUSING_DECOR_CLEANUP_MODE_TOGGLE, std::move(packet)) { }
        void Read() override;
        bool Enabled = false;
    };

    class HousingDecorBatchOperation final : public ClientPacket
    {
    public:
        explicit HousingDecorBatchOperation(WorldPacket&& packet) : ClientPacket(CMSG_HOUSING_DECOR_BATCH_OPERATION, std::move(packet)) { }
        void Read() override;
        uint8 OperationType = 0;
        std::vector<ObjectGuid> DecorGuids;
    };

    class HousingDecorPlacementPreview final : public ClientPacket
    {
    public:
        explicit HousingDecorPlacementPreview(WorldPacket&& packet) : ClientPacket(CMSG_HOUSING_DECOR_PLACEMENT_PREVIEW, std::move(packet)) { }
        void Read() override;
        ObjectGuid DecorGuid;
        TaggedPosition<Position::XYZ> PreviewPosition;
        TaggedPosition<Position::XYZ> PreviewRotation;
        float Scale = 1.0f;
    };

    // ============================================================
    // Fixture System (0x31xxxx)
    // ============================================================

    class HousingFixtureSetEditMode final : public ClientPacket
    {
    public:
        explicit HousingFixtureSetEditMode(WorldPacket&& packet) : ClientPacket(CMSG_HOUSING_FIXTURE_SET_EDIT_MODE, std::move(packet)) { }

        void Read() override;

        bool Active = false;
    };

    class HousingFixtureSetCoreFixture final : public ClientPacket
    {
    public:
        explicit HousingFixtureSetCoreFixture(WorldPacket&& packet) : ClientPacket(CMSG_HOUSING_FIXTURE_SET_CORE_FIXTURE, std::move(packet)) { }

        void Read() override;

        ObjectGuid FixtureGuid;
        uint32 ExteriorComponentID = 0;
    };

    class HousingFixtureCreateFixture final : public ClientPacket
    {
    public:
        explicit HousingFixtureCreateFixture(WorldPacket&& packet) : ClientPacket(CMSG_HOUSING_FIXTURE_CREATE_FIXTURE, std::move(packet)) { }

        void Read() override;

        ObjectGuid AttachParentGuid;         // Housing/3 exterior root entity
        ObjectGuid HookEntityGuid;            // Housing/4 hook point entity on the house
        uint32 ExteriorComponentHookID = 0;   // DB2 ExteriorComponentHook row ID (which hook point)
        uint32 ExteriorComponentID = 0;       // DB2 ExteriorComponent row ID (which component to install)
    };

    class HousingFixtureDeleteFixture final : public ClientPacket
    {
    public:
        explicit HousingFixtureDeleteFixture(WorldPacket&& packet) : ClientPacket(CMSG_HOUSING_FIXTURE_DELETE_FIXTURE, std::move(packet)) { }

        void Read() override;

        ObjectGuid FixtureGuid;
        ObjectGuid RoomGuid;
        uint32 ExteriorComponentID = 0;
    };

    class HousingFixtureSetHouseSize final : public ClientPacket
    {
    public:
        explicit HousingFixtureSetHouseSize(WorldPacket&& packet) : ClientPacket(CMSG_HOUSING_FIXTURE_SET_HOUSE_SIZE, std::move(packet)) { }

        void Read() override;

        ObjectGuid HouseGuid;
        uint8 Size = 0;
    };

    class HousingFixtureSetHouseType final : public ClientPacket
    {
    public:
        explicit HousingFixtureSetHouseType(WorldPacket&& packet) : ClientPacket(CMSG_HOUSING_FIXTURE_SET_HOUSE_TYPE, std::move(packet)) { }

        void Read() override;

        ObjectGuid HouseGuid;
        uint32 HouseExteriorWmoDataID = 0;
    };

    class HousingFixtureCreateBasicHouse final : public ClientPacket
    {
    public:
        explicit HousingFixtureCreateBasicHouse(WorldPacket&& packet) : ClientPacket(CMSG_HOUSING_FIXTURE_CREATE_BASIC_HOUSE, std::move(packet)) { }
        void Read() override;
        ObjectGuid PlotGuid;
        uint32 HouseStyleID = 0;
    };

    class HousingFixtureDeleteHouse final : public ClientPacket
    {
    public:
        explicit HousingFixtureDeleteHouse(WorldPacket&& packet) : ClientPacket(CMSG_HOUSING_FIXTURE_DELETE_HOUSE, std::move(packet)) { }
        void Read() override;
        ObjectGuid HouseGuid;
    };

    // ============================================================
    // Room System (0x32xxxx)
    // ============================================================

    class HousingRoomSetLayoutEditMode final : public ClientPacket
    {
    public:
        explicit HousingRoomSetLayoutEditMode(WorldPacket&& packet) : ClientPacket(CMSG_HOUSING_ROOM_SET_LAYOUT_EDIT_MODE, std::move(packet)) { }

        void Read() override;

        bool Active = false;
    };

    class HousingRoomAdd final : public ClientPacket
    {
    public:
        explicit HousingRoomAdd(WorldPacket&& packet) : ClientPacket(CMSG_HOUSING_ROOM_ADD, std::move(packet)) { }

        void Read() override;

        ObjectGuid HouseGuid;
        uint32 HouseRoomID = 0;
        uint32 Flags = 0;
        uint32 FloorIndex = 0;
        bool AutoFurnish = false;
    };

    class HousingRoomRemove final : public ClientPacket
    {
    public:
        explicit HousingRoomRemove(WorldPacket&& packet) : ClientPacket(CMSG_HOUSING_ROOM_REMOVE, std::move(packet)) { }

        void Read() override;

        ObjectGuid RoomGuid;
    };

    class HousingRoomRotate final : public ClientPacket
    {
    public:
        explicit HousingRoomRotate(WorldPacket&& packet) : ClientPacket(CMSG_HOUSING_ROOM_ROTATE, std::move(packet)) { }

        void Read() override;

        ObjectGuid RoomGuid;
        bool Clockwise = false;
    };

    class HousingRoomMoveRoom final : public ClientPacket
    {
    public:
        explicit HousingRoomMoveRoom(WorldPacket&& packet) : ClientPacket(CMSG_HOUSING_ROOM_MOVE, std::move(packet)) { }

        void Read() override;

        ObjectGuid RoomGuid;
        uint32 TargetSlotIndex = 0;
        ObjectGuid TargetGuid;
        uint32 FloorIndex = 0;
    };

    class HousingRoomSetComponentTheme final : public ClientPacket
    {
    public:
        explicit HousingRoomSetComponentTheme(WorldPacket&& packet) : ClientPacket(CMSG_HOUSING_ROOM_SET_COMPONENT_THEME, std::move(packet)) { }

        void Read() override;

        ObjectGuid RoomGuid;
        uint32 HouseThemeID = 0;
        std::vector<uint32> RoomComponentIDs;
    };

    class HousingRoomApplyComponentMaterials final : public ClientPacket
    {
    public:
        explicit HousingRoomApplyComponentMaterials(WorldPacket&& packet) : ClientPacket(CMSG_HOUSING_ROOM_APPLY_COMPONENT_MATERIALS, std::move(packet)) { }

        void Read() override;

        ObjectGuid RoomGuid;
        uint32 RoomComponentTextureID = 0;
        uint32 RoomComponentTypeParam = 0;
        std::vector<uint32> RoomComponentIDs;
    };

    class HousingRoomSetDoorType final : public ClientPacket
    {
    public:
        explicit HousingRoomSetDoorType(WorldPacket&& packet) : ClientPacket(CMSG_HOUSING_ROOM_SET_DOOR_TYPE, std::move(packet)) { }

        void Read() override;

        ObjectGuid RoomGuid;
        uint32 RoomComponentID = 0;
        uint8 RoomComponentType = 0;
    };

    class HousingRoomSetCeilingType final : public ClientPacket
    {
    public:
        explicit HousingRoomSetCeilingType(WorldPacket&& packet) : ClientPacket(CMSG_HOUSING_ROOM_SET_CEILING_TYPE, std::move(packet)) { }

        void Read() override;

        ObjectGuid RoomGuid;
        uint32 RoomComponentID = 0;
        uint8 RoomComponentType = 0;
    };

    // ============================================================
    // Housing Services System (0x33xxxx)
    // ============================================================

    class HousingSvcsGuildCreateNeighborhood final : public ClientPacket
    {
    public:
        explicit HousingSvcsGuildCreateNeighborhood(WorldPacket&& packet) : ClientPacket(CMSG_HOUSING_SVCS_GUILD_CREATE_NEIGHBORHOOD, std::move(packet)) { }

        void Read() override;

        uint32 NeighborhoodTypeID = 0;
        std::string NeighborhoodName;
        uint8 Flags = 0;
    };

    class HousingSvcsNeighborhoodReservePlot final : public ClientPacket
    {
    public:
        explicit HousingSvcsNeighborhoodReservePlot(WorldPacket&& packet) : ClientPacket(CMSG_HOUSING_SVCS_NEIGHBORHOOD_RESERVE_PLOT, std::move(packet)) { }

        void Read() override;

        ObjectGuid NeighborhoodGuid;
        uint8 PlotIndex = 0;
        bool Reserve = false;
    };

    class HousingSvcsRelinquishHouse final : public ClientPacket
    {
    public:
        explicit HousingSvcsRelinquishHouse(WorldPacket&& packet) : ClientPacket(CMSG_HOUSING_SVCS_RELINQUISH_HOUSE, std::move(packet)) { }

        void Read() override;

        ObjectGuid HouseGuid;
    };

    class HousingSvcsUpdateHouseSettings final : public ClientPacket
    {
    public:
        explicit HousingSvcsUpdateHouseSettings(WorldPacket&& packet) : ClientPacket(CMSG_HOUSING_SVCS_UPDATE_HOUSE_SETTINGS, std::move(packet)) { }

        void Read() override;

        ObjectGuid HouseGuid;
        Optional<uint32> PlotSettingsID;
        Optional<ObjectGuid> VisitorPermissionGuid;
    };

    class HousingSvcsPlayerViewHousesByPlayer final : public ClientPacket
    {
    public:
        explicit HousingSvcsPlayerViewHousesByPlayer(WorldPacket&& packet) : ClientPacket(CMSG_HOUSING_SVCS_PLAYER_VIEW_HOUSES_BY_PLAYER, std::move(packet)) { }

        void Read() override;

        ObjectGuid PlayerGuid;
    };

    class HousingSvcsPlayerViewHousesByBnetAccount final : public ClientPacket
    {
    public:
        explicit HousingSvcsPlayerViewHousesByBnetAccount(WorldPacket&& packet) : ClientPacket(CMSG_HOUSING_SVCS_PLAYER_VIEW_HOUSES_BY_BNET_ACCOUNT, std::move(packet)) { }

        void Read() override;

        ObjectGuid BnetAccountGuid;
    };

    class HousingSvcsGetPlayerHousesInfo final : public ClientPacket
    {
    public:
        explicit HousingSvcsGetPlayerHousesInfo(WorldPacket&& packet) : ClientPacket(CMSG_HOUSING_SVCS_GET_PLAYER_HOUSES_INFO, std::move(packet)) { }

        void Read() override { }
    };

    class HousingSvcsTeleportToPlot final : public ClientPacket
    {
    public:
        explicit HousingSvcsTeleportToPlot(WorldPacket&& packet) : ClientPacket(CMSG_HOUSING_SVCS_TELEPORT_TO_PLOT, std::move(packet)) { }

        void Read() override;

        ObjectGuid NeighborhoodGuid;
        ObjectGuid OwnerGuid;
        uint32 PlotIndex = 0;
        uint8 TeleportType = 0;
    };

    class HousingSvcsStartTutorial final : public ClientPacket
    {
    public:
        explicit HousingSvcsStartTutorial(WorldPacket&& packet) : ClientPacket(CMSG_HOUSING_SVCS_START_TUTORIAL, std::move(packet)) { }

        void Read() override { }
    };

    class HousingSvcsSetTutorialState final : public ClientPacket
    {
    public:
        explicit HousingSvcsSetTutorialState(WorldPacket&& packet) : ClientPacket(CMSG_HOUSING_SVCS_SET_TUTORIAL_STATE, std::move(packet)) { }

        void Read() override;
        uint32 TutorialFlags = 0;
    };

    class HousingSvcsCompleteTutorialStep final : public ClientPacket
    {
    public:
        explicit HousingSvcsCompleteTutorialStep(WorldPacket&& packet) : ClientPacket(CMSG_HOUSING_SVCS_COMPLETE_TUTORIAL_STEP, std::move(packet)) { }

        void Read() override;
        uint32 StepIndex = 0;
    };

    class HousingSvcsSkipTutorial final : public ClientPacket
    {
    public:
        explicit HousingSvcsSkipTutorial(WorldPacket&& packet) : ClientPacket(CMSG_HOUSING_SVCS_SKIP_TUTORIAL, std::move(packet)) { }

        void Read() override { }
    };

    class HousingSvcsQueryPendingInvites final : public ClientPacket
    {
    public:
        explicit HousingSvcsQueryPendingInvites(WorldPacket&& packet) : ClientPacket(CMSG_HOUSING_SVCS_QUERY_PENDING_INVITES, std::move(packet)) { }

        void Read() override { }
    };

    class HousingDecorConfirmPreviewPlacement final : public ClientPacket
    {
    public:
        explicit HousingDecorConfirmPreviewPlacement(WorldPacket&& packet) : ClientPacket(CMSG_HOUSING_DECOR_CONFIRM_PREVIEW_PLACEMENT, std::move(packet)) { }

        void Read() override;
        ObjectGuid DecorGuid;
    };

    class HousingSvcsAcceptNeighborhoodOwnership final : public ClientPacket
    {
    public:
        explicit HousingSvcsAcceptNeighborhoodOwnership(WorldPacket&& packet) : ClientPacket(CMSG_HOUSING_SVCS_ACCEPT_NEIGHBORHOOD_OWNERSHIP, std::move(packet)) { }

        void Read() override;

        ObjectGuid NeighborhoodGuid;
    };

    class HousingSvcsRejectNeighborhoodOwnership final : public ClientPacket
    {
    public:
        explicit HousingSvcsRejectNeighborhoodOwnership(WorldPacket&& packet) : ClientPacket(CMSG_HOUSING_SVCS_REJECT_NEIGHBORHOOD_OWNERSHIP, std::move(packet)) { }

        void Read() override;

        ObjectGuid NeighborhoodGuid;
    };

    class HousingSvcsGetPotentialHouseOwners final : public ClientPacket
    {
    public:
        explicit HousingSvcsGetPotentialHouseOwners(WorldPacket&& packet) : ClientPacket(CMSG_HOUSING_SVCS_GET_POTENTIAL_HOUSE_OWNERS, std::move(packet)) { }

        void Read() override;

        ObjectGuid NeighborhoodGuid;
    };

    class HousingSvcsGetHouseFinderInfo final : public ClientPacket
    {
    public:
        explicit HousingSvcsGetHouseFinderInfo(WorldPacket&& packet) : ClientPacket(CMSG_HOUSING_SVCS_GET_HOUSE_FINDER_INFO, std::move(packet)) { }

        void Read() override { }
    };

    class HousingSvcsGetHouseFinderNeighborhood final : public ClientPacket
    {
    public:
        explicit HousingSvcsGetHouseFinderNeighborhood(WorldPacket&& packet) : ClientPacket(CMSG_HOUSING_SVCS_GET_HOUSE_FINDER_NEIGHBORHOOD, std::move(packet)) { }

        void Read() override;

        ObjectGuid NeighborhoodGuid;
    };

    class HousingSvcsGetBnetFriendNeighborhoods final : public ClientPacket
    {
    public:
        explicit HousingSvcsGetBnetFriendNeighborhoods(WorldPacket&& packet) : ClientPacket(CMSG_HOUSING_SVCS_GET_BNET_FRIEND_NEIGHBORHOODS, std::move(packet)) { }

        void Read() override;

        ObjectGuid BnetAccountGuid;
    };

    class HousingSvcsDeleteAllNeighborhoodInvites final : public ClientPacket
    {
    public:
        explicit HousingSvcsDeleteAllNeighborhoodInvites(WorldPacket&& packet) : ClientPacket(CMSG_HOUSING_SVCS_DELETE_ALL_NEIGHBORHOOD_INVITES, std::move(packet)) { }

        void Read() override { }
    };

    class HousingSvcsRequestPermissionsCheck final : public ClientPacket
    {
    public:
        explicit HousingSvcsRequestPermissionsCheck(WorldPacket&& packet) : ClientPacket(CMSG_HOUSING_SVCS_REQUEST_PERMISSIONS_CHECK, std::move(packet)) { }
        void Read() override { }
    };

    class HousingSvcsClearPlotReservation final : public ClientPacket
    {
    public:
        explicit HousingSvcsClearPlotReservation(WorldPacket&& packet) : ClientPacket(CMSG_HOUSING_SVCS_CLEAR_PLOT_RESERVATION, std::move(packet)) { }
        void Read() override;
        ObjectGuid NeighborhoodGuid;
    };

    class HousingSvcsGetPlayerHousesInfoAlt final : public ClientPacket
    {
    public:
        explicit HousingSvcsGetPlayerHousesInfoAlt(WorldPacket&& packet) : ClientPacket(CMSG_HOUSING_SVCS_GET_PLAYER_HOUSES_INFO_ALT, std::move(packet)) { }
        void Read() override;
        ObjectGuid PlayerGuid;
    };

    class HousingSvcsGetRosterData final : public ClientPacket
    {
    public:
        explicit HousingSvcsGetRosterData(WorldPacket&& packet) : ClientPacket(CMSG_HOUSING_SVCS_GET_ROSTER_DATA, std::move(packet)) { }
        void Read() override;
        ObjectGuid NeighborhoodGuid;
    };

    class HousingSvcsRosterUpdateSubscribe final : public ClientPacket
    {
    public:
        explicit HousingSvcsRosterUpdateSubscribe(WorldPacket&& packet) : ClientPacket(CMSG_HOUSING_SVCS_ROSTER_UPDATE_SUBSCRIBE, std::move(packet)) { }
        void Read() override { }
    };

    class HousingSvcsChangeHouseCosmeticOwnerRequest final : public ClientPacket
    {
    public:
        explicit HousingSvcsChangeHouseCosmeticOwnerRequest(WorldPacket&& packet) : ClientPacket(CMSG_HOUSING_SVCS_CHANGE_HOUSE_COSMETIC_OWNER, std::move(packet)) { }
        void Read() override;
        ObjectGuid HouseGuid;
        ObjectGuid NewOwnerGuid;
    };

    class HousingSvcsQueryHouseLevelFavor final : public ClientPacket
    {
    public:
        explicit HousingSvcsQueryHouseLevelFavor(WorldPacket&& packet) : ClientPacket(CMSG_HOUSING_SVCS_QUERY_HOUSE_LEVEL_FAVOR, std::move(packet)) { }
        void Read() override;
        ObjectGuid HouseGuid;
    };

    class HousingSvcsGuildAddHouse final : public ClientPacket
    {
    public:
        explicit HousingSvcsGuildAddHouse(WorldPacket&& packet) : ClientPacket(CMSG_HOUSING_SVCS_GUILD_ADD_HOUSE, std::move(packet)) { }
        void Read() override;
        ObjectGuid HouseGuid;
    };

    class HousingSvcsGuildAppendNeighborhood final : public ClientPacket
    {
    public:
        explicit HousingSvcsGuildAppendNeighborhood(WorldPacket&& packet) : ClientPacket(CMSG_HOUSING_SVCS_GUILD_APPEND_NEIGHBORHOOD, std::move(packet)) { }
        void Read() override;
        ObjectGuid NeighborhoodGuid;
    };

    class HousingSvcsGuildRenameNeighborhood final : public ClientPacket
    {
    public:
        explicit HousingSvcsGuildRenameNeighborhood(WorldPacket&& packet) : ClientPacket(CMSG_HOUSING_SVCS_GUILD_RENAME_NEIGHBORHOOD, std::move(packet)) { }
        void Read() override;
        ObjectGuid NeighborhoodGuid;
        std::string NewName;
    };

    class HousingSvcsGuildGetHousingInfo final : public ClientPacket
    {
    public:
        explicit HousingSvcsGuildGetHousingInfo(WorldPacket&& packet) : ClientPacket(CMSG_HOUSING_SVCS_GUILD_GET_HOUSING_INFO, std::move(packet)) { }
        void Read() override;
        ObjectGuid GuildGuid;
    };

    // ============================================================
    // Housing Misc (0x35xxxx)
    // ============================================================

    class HousingGetCurrentHouseInfo final : public ClientPacket
    {
    public:
        explicit HousingGetCurrentHouseInfo(WorldPacket&& packet) : ClientPacket(CMSG_HOUSING_GET_CURRENT_HOUSE_INFO, std::move(packet)) { }

        void Read() override { }
    };

    class HousingResetKioskMode final : public ClientPacket
    {
    public:
        explicit HousingResetKioskMode(WorldPacket&& packet) : ClientPacket(CMSG_HOUSING_RESET_KIOSK_MODE, std::move(packet)) { }

        void Read() override { }
    };

    class HousingHouseStatus final : public ClientPacket
    {
    public:
        explicit HousingHouseStatus(WorldPacket&& packet) : ClientPacket(CMSG_HOUSING_HOUSE_STATUS, std::move(packet)) { }

        void Read() override { }
    };

    class HousingRequestEditorAvailability final : public ClientPacket
    {
    public:
        explicit HousingRequestEditorAvailability(WorldPacket&& packet) : ClientPacket(CMSG_HOUSING_REQUEST_EDITOR_AVAILABILITY, std::move(packet)) { }

        void Read() override;

        uint8 Field_0 = 0;
        ObjectGuid HouseGuid;
    };

    class HousingGetPlayerPermissions final : public ClientPacket
    {
    public:
        explicit HousingGetPlayerPermissions(WorldPacket&& packet) : ClientPacket(CMSG_HOUSING_GET_PLAYER_PERMISSIONS, std::move(packet)) { }

        void Read() override;

        Optional<ObjectGuid> HouseGuid;
    };

    class HousingSystemHouseStatusQuery final : public ClientPacket
    {
    public:
        explicit HousingSystemHouseStatusQuery(WorldPacket&& packet) : ClientPacket(CMSG_HOUSING_SYSTEM_HOUSE_STATUS_QUERY, std::move(packet)) { }
        void Read() override { }
    };

    class HousingSystemGetHouseInfoAlt final : public ClientPacket
    {
    public:
        explicit HousingSystemGetHouseInfoAlt(WorldPacket&& packet) : ClientPacket(CMSG_HOUSING_SYSTEM_GET_HOUSE_INFO_ALT, std::move(packet)) { }
        void Read() override;
        ObjectGuid HouseGuid;
    };

    class HousingSystemHouseSnapshot final : public ClientPacket
    {
    public:
        explicit HousingSystemHouseSnapshot(WorldPacket&& packet) : ClientPacket(CMSG_HOUSING_SYSTEM_HOUSE_SNAPSHOT, std::move(packet)) { }
        void Read() override;
        ObjectGuid HouseGuid;
        uint8 SnapshotType = 0;
    };

    class HousingSystemExportHouse final : public ClientPacket
    {
    public:
        explicit HousingSystemExportHouse(WorldPacket&& packet) : ClientPacket(CMSG_HOUSING_SYSTEM_EXPORT_HOUSE, std::move(packet)) { }
        void Read() override;
        ObjectGuid HouseGuid;
    };

    class HousingSystemUpdateHouseInfo final : public ClientPacket
    {
    public:
        explicit HousingSystemUpdateHouseInfo(WorldPacket&& packet) : ClientPacket(CMSG_HOUSING_SYSTEM_UPDATE_HOUSE_INFO, std::move(packet)) { }
        void Read() override;
        ObjectGuid HouseGuid;
        uint32 InfoType = 0;
        std::string HouseName;
        std::string HouseDescription;
    };

    // ============================================================
    // Photo Sharing Authorization (0x40019x)
    // ============================================================

    class HousingPhotoSharingCompleteAuthorization final : public ClientPacket
    {
    public:
        explicit HousingPhotoSharingCompleteAuthorization(WorldPacket&& packet) : ClientPacket(CMSG_HOUSING_PHOTO_SHARING_COMPLETE_AUTHORIZATION, std::move(packet)) { }

        void Read() override { }
    };

    class HousingPhotoSharingClearAuthorization final : public ClientPacket
    {
    public:
        explicit HousingPhotoSharingClearAuthorization(WorldPacket&& packet) : ClientPacket(CMSG_HOUSING_PHOTO_SHARING_CLEAR_AUTHORIZATION, std::move(packet)) { }

        void Read() override { }
    };

    // ============================================================
    // Decor Licensing / Refund CMSG
    // ============================================================

    class GetAllLicensedDecorQuantities final : public ClientPacket
    {
    public:
        explicit GetAllLicensedDecorQuantities(WorldPacket&& packet) : ClientPacket(CMSG_GET_ALL_LICENSED_DECOR_QUANTITIES, std::move(packet)) { }

        void Read() override { }
    };

    class GetDecorRefundList final : public ClientPacket
    {
    public:
        explicit GetDecorRefundList(WorldPacket&& packet) : ClientPacket(CMSG_GET_DECOR_REFUND_LIST, std::move(packet)) { }

        void Read() override { }
    };

    // ============================================================
    // Other Housing CMSG
    // ============================================================

    class DeclineNeighborhoodInvites final : public ClientPacket
    {
    public:
        explicit DeclineNeighborhoodInvites(WorldPacket&& packet) : ClientPacket(CMSG_DECLINE_NEIGHBORHOOD_INVITES, std::move(packet)) { }

        void Read() override;

        bool Allow = false;
    };

    class QueryNeighborhoodInfo final : public ClientPacket
    {
    public:
        explicit QueryNeighborhoodInfo(WorldPacket&& packet) : ClientPacket(CMSG_QUERY_NEIGHBORHOOD_INFO, std::move(packet)) { }

        void Read() override;

        ObjectGuid NeighborhoodGuid;
    };

    class InvitePlayerToNeighborhood final : public ClientPacket
    {
    public:
        explicit InvitePlayerToNeighborhood(WorldPacket&& packet) : ClientPacket(CMSG_INVITE_PLAYER_TO_NEIGHBORHOOD, std::move(packet)) { }

        void Read() override;

        ObjectGuid PlayerGuid;
        ObjectGuid NeighborhoodGuid;
    };

    class GuildGetOthersOwnedHouses final : public ClientPacket
    {
    public:
        explicit GuildGetOthersOwnedHouses(WorldPacket&& packet) : ClientPacket(CMSG_GUILD_GET_OTHERS_OWNED_HOUSES, std::move(packet)) { }

        void Read() override;

        ObjectGuid PlayerGuid;
    };

    // ============================================================
    // SMSG Packets
    // ============================================================

    class QueryNeighborhoodNameResponse final : public ServerPacket
    {
    public:
        explicit QueryNeighborhoodNameResponse() : ServerPacket(SMSG_QUERY_NEIGHBORHOOD_NAME_RESPONSE) { }

        WorldPacket const* Write() override;

        ObjectGuid NeighborhoodGuid;
        bool Result = true;
        std::string NeighborhoodName;
    };

    class InvalidateNeighborhoodName final : public ServerPacket
    {
    public:
        explicit InvalidateNeighborhoodName() : ServerPacket(SMSG_INVALIDATE_NEIGHBORHOOD_NAME) { }

        WorldPacket const* Write() override;

        ObjectGuid NeighborhoodGuid;
    };

    // ============================================================
    // House Exterior SMSG Responses (0x50xxxx)
    // ============================================================

    class HouseExteriorLockResponse final : public ServerPacket
    {
    public:
        HouseExteriorLockResponse() : ServerPacket(SMSG_HOUSE_EXTERIOR_LOCK_RESPONSE) { }
        WorldPacket const* Write() override;
        // Sniff-verified wire format (build 66337, 19 bytes):
        //   PackedGUID(FixtureEntityGuid) + PackedGUID(EditorPlayerGuid) + uint8(Result) + Bits<1>(Active) + FlushBits
        ObjectGuid FixtureEntityGuid;   // Housing/3 fixture entity (exterior root)
        ObjectGuid EditorPlayerGuid;    // Player performing the edit
        uint8 Result = 0;
        bool Active = false;
    };

    class HouseExteriorSetHousePositionResponse final : public ServerPacket
    {
    public:
        HouseExteriorSetHousePositionResponse() : ServerPacket(SMSG_HOUSE_EXTERIOR_SET_HOUSE_POSITION_RESPONSE) { }
        WorldPacket const* Write() override;
        uint8 Result = 0;
        ObjectGuid HouseGuid;
    };

    // ============================================================
    // Housing Decor SMSG Responses (0x51xxxx)
    // ============================================================

    class HousingDecorSetEditModeResponse final : public ServerPacket
    {
    public:
        HousingDecorSetEditModeResponse() : ServerPacket(SMSG_HOUSING_DECOR_SET_EDIT_MODE_RESPONSE) { }
        WorldPacket const* Write() override;

        // Wire format (verified against working implementation):
        //   PackedGUID HouseGuid + PackedGUID BNetAccountGuid
        //   + uint32 AllowedEditor.size() + uint8 Result + [PackedGUID AllowedEditors...]
        ObjectGuid HouseGuid;
        ObjectGuid BNetAccountGuid;
        uint8 Result = 0;
        std::vector<ObjectGuid> AllowedEditor;
    };

    class HousingDecorMoveResponse final : public ServerPacket
    {
    public:
        HousingDecorMoveResponse() : ServerPacket(SMSG_HOUSING_DECOR_MOVE_RESPONSE) { }
        WorldPacket const* Write() override;
        // IDA case 5308417: PackedGUID + uint32 + PackedGUID + uint8(Result) + uint8(bit7=SuccessFlag)
        ObjectGuid PlayerGuid;
        uint32 Field_09 = 0;
        ObjectGuid DecorGuid;
        uint8 Result = 0;
        uint8 Field_26 = 0;
    };

    class HousingDecorPlaceResponse final : public ServerPacket
    {
    public:
        HousingDecorPlaceResponse() : ServerPacket(SMSG_HOUSING_DECOR_PLACE_RESPONSE) { }
        WorldPacket const* Write() override;
        // Wire format: PackedGUID PlayerGuid + uint32 Field_09 + PackedGUID DecorGuid + uint8 Result
        ObjectGuid PlayerGuid;
        uint32 Field_09 = 0;
        ObjectGuid DecorGuid;
        uint8 Result = 0;
    };

    class HousingDecorRemoveResponse final : public ServerPacket
    {
    public:
        HousingDecorRemoveResponse() : ServerPacket(SMSG_HOUSING_DECOR_REMOVE_RESPONSE) { }
        WorldPacket const* Write() override;
        // Wire format: PackedGUID DecorGUID + PackedGUID UnkGUID + uint32 Field_13 + uint8 Result
        ObjectGuid DecorGuid;
        ObjectGuid UnkGUID;
        uint32 Field_13 = 0;
        uint8 Result = 0;
    };

    class HousingDecorLockResponse final : public ServerPacket
    {
    public:
        HousingDecorLockResponse() : ServerPacket(SMSG_HOUSING_DECOR_LOCK_RESPONSE) { }
        WorldPacket const* Write() override;
        // Wire format: PackedGUID DecorGUID + PackedGUID PlayerGUID + uint32 Field_16
        //   + uint8 Result + Bits<1> Locked + Bits<1> Field_17 + FlushBits
        ObjectGuid DecorGuid;
        ObjectGuid PlayerGuid;
        uint32 Field_16 = 0;
        uint8 Result = 0;
        bool Locked = false;
        bool Field_17 = true;
    };

    class HousingDecorDeleteFromStorageResponse final : public ServerPacket
    {
    public:
        HousingDecorDeleteFromStorageResponse() : ServerPacket(SMSG_HOUSING_DECOR_DELETE_FROM_STORAGE_RESPONSE) { }
        WorldPacket const* Write() override;
        uint8 Result = 0;
        ObjectGuid DecorGuid;
    };

    class HousingDecorRequestStorageResponse final : public ServerPacket
    {
    public:
        HousingDecorRequestStorageResponse() : ServerPacket(SMSG_HOUSING_DECOR_REQUEST_STORAGE_RESPONSE) { }
        WorldPacket const* Write() override;

        // IDA-verified wire format (case 5308422):
        // PackedGUID BNetAccountGUID + uint8 ResultCode + uint8 Flags
        // Sniff-verified: Flags is ALWAYS 0x80 in all 3 retail instances (housing + garrison).
        // Actual decor data is delivered via FHousingStorage_C fragment on the Account entity,
        // not inline in this packet. This response is purely an acknowledgement.
        ObjectGuid BNetAccountGuid;
        uint8 ResultCode = 0;
        uint8 Flags = 0x80;  // Retail-verified: always 0x80
    };

    class HousingDecorAddToHouseChestResponse final : public ServerPacket
    {
    public:
        HousingDecorAddToHouseChestResponse() : ServerPacket(SMSG_HOUSING_DECOR_ADD_TO_HOUSE_CHEST_RESPONSE) { }
        WorldPacket const* Write() override;
        // IDA case 5308423: uint8(bit7=success) + uint32(count) + PackedGUID[count]
        bool Success = false;
        std::vector<ObjectGuid> DecorGuids;
    };

    class HousingDecorSystemSetDyeSlotsResponse final : public ServerPacket
    {
    public:
        HousingDecorSystemSetDyeSlotsResponse() : ServerPacket(SMSG_HOUSING_DECOR_SYSTEM_SET_DYE_SLOTS_RESPONSE) { }
        WorldPacket const* Write() override;
        // Wire format: PackedGUID DecorGUID + uint8 Result
        ObjectGuid DecorGuid;
        uint8 Result = 0;
    };

    class HousingRedeemDeferredDecorResponse final : public ServerPacket
    {
    public:
        HousingRedeemDeferredDecorResponse() : ServerPacket(SMSG_HOUSING_REDEEM_DEFERRED_DECOR_RESPONSE) { }
        WorldPacket const* Write() override;
        ObjectGuid DecorGuid;       // Sniff: PackedGUID — the new decor item's GUID (client uses for placement)
        uint8 Result = 0;           // Sniff: uint8 Status (0 = success)
        uint32 SequenceIndex = 0;   // Sniff: uint32 — echoes CMSG RedemptionToken
    };

    class HousingDecorStartPlacingNewDecorResponse final : public ServerPacket
    {
    public:
        HousingDecorStartPlacingNewDecorResponse() : ServerPacket(SMSG_HOUSING_DECOR_START_PLACING_NEW_DECOR_RESPONSE) { }
        WorldPacket const* Write() override;
        ObjectGuid DecorGuid;
        uint8 Result = 0;
        uint32 Field_13 = 0;
    };

    class HousingDecorCatalogCreateSearcherResponse final : public ServerPacket
    {
    public:
        HousingDecorCatalogCreateSearcherResponse() : ServerPacket(SMSG_HOUSING_DECOR_CATALOG_CREATE_SEARCHER_RESPONSE) { }
        WorldPacket const* Write() override;
        ObjectGuid Owner;
        uint8 Result = 0;
    };

    class HousingDecorBatchOperationResponse final : public ServerPacket
    {
    public:
        HousingDecorBatchOperationResponse() : ServerPacket(SMSG_HOUSING_DECOR_BATCH_OPERATION_RESPONSE) { }
        WorldPacket const* Write() override;
        uint8 Result = 0;
        uint32 ProcessedCount = 0;
    };

    class HousingDecorPlacementPreviewResponse final : public ServerPacket
    {
    public:
        HousingDecorPlacementPreviewResponse() : ServerPacket(SMSG_HOUSING_DECOR_PLACEMENT_PREVIEW_RESPONSE) { }
        WorldPacket const* Write() override;
        uint8 Result = 0;
        uint8 RestrictionFlags = 0;
    };

    class HousingFirstTimeDecorAcquisition final : public ServerPacket
    {
    public:
        HousingFirstTimeDecorAcquisition() : ServerPacket(SMSG_HOUSING_FIRST_TIME_DECOR_ACQUISITION) { }
        WorldPacket const* Write() override;
        uint32 DecorEntryID = 0;
    };

    // ============================================================
    // Housing Fixture SMSG Responses (0x52xxxx)
    // ============================================================

    class HousingFixtureSetEditModeResponse final : public ServerPacket
    {
    public:
        HousingFixtureSetEditModeResponse() : ServerPacket(SMSG_HOUSING_FIXTURE_SET_EDIT_MODE_RESPONSE) { }
        WorldPacket const* Write() override;
        // Sniff-verified (build 66337): PackedGUID(HouseGuid, always empty) + PackedGUID(EditorPlayerGuid) + uint8(Result)
        // Client compares EditorPlayerGuid against stored reference: match → enter, mismatch/empty → exit
        ObjectGuid HouseGuid;           // Always empty in sniff
        ObjectGuid EditorPlayerGuid;    // Player GUID on enter, empty on exit
        uint8 Result = 0;
    };

    class HousingFixtureCreateBasicHouseResponse final : public ServerPacket
    {
    public:
        HousingFixtureCreateBasicHouseResponse() : ServerPacket(SMSG_HOUSING_FIXTURE_CREATE_BASIC_HOUSE_RESPONSE) { }
        WorldPacket const* Write() override;
        // IDA case 5373953: uint8(Result) only — client ignores any trailing data
        uint8 Result = 0;
    };

    class HousingFixtureDeleteHouseResponse final : public ServerPacket
    {
    public:
        HousingFixtureDeleteHouseResponse() : ServerPacket(SMSG_HOUSING_FIXTURE_DELETE_HOUSE_RESPONSE) { }
        WorldPacket const* Write() override;
        // IDA case 5373954: uint8(Result) only
        uint8 Result = 0;
    };

    class HousingFixtureSetHouseSizeResponse final : public ServerPacket
    {
    public:
        HousingFixtureSetHouseSizeResponse() : ServerPacket(SMSG_HOUSING_FIXTURE_SET_HOUSE_SIZE_RESPONSE) { }
        WorldPacket const* Write() override;
        uint8 Result = 0;
        uint8 Size = 0;
    };

    class HousingFixtureSetHouseTypeResponse final : public ServerPacket
    {
    public:
        HousingFixtureSetHouseTypeResponse() : ServerPacket(SMSG_HOUSING_FIXTURE_SET_HOUSE_TYPE_RESPONSE) { }
        WorldPacket const* Write() override;
        // IDA case 5373956: uint8(Result) + uint32(HouseExteriorTypeID) + uint8(ExtraField)
        uint8 Result = 0;
        uint32 HouseExteriorTypeID = 0;
        uint8 ExtraField = 0;
    };

    class HousingFixtureSetCoreFixtureResponse final : public ServerPacket
    {
    public:
        HousingFixtureSetCoreFixtureResponse() : ServerPacket(SMSG_HOUSING_FIXTURE_SET_CORE_FIXTURE_RESPONSE) { }
        WorldPacket const* Write() override;
        // IDA case 5373957: uint8(Result) only
        uint8 Result = 0;
    };

    class HousingFixtureCreateFixtureResponse final : public ServerPacket
    {
    public:
        HousingFixtureCreateFixtureResponse() : ServerPacket(SMSG_HOUSING_FIXTURE_CREATE_FIXTURE_RESPONSE) { }
        WorldPacket const* Write() override;
        uint8 Result = 0;
        ObjectGuid FixtureGuid;
    };

    class HousingFixtureDeleteFixtureResponse final : public ServerPacket
    {
    public:
        HousingFixtureDeleteFixtureResponse() : ServerPacket(SMSG_HOUSING_FIXTURE_DELETE_FIXTURE_RESPONSE) { }
        WorldPacket const* Write() override;
        uint8 Result = 0;
        ObjectGuid FixtureGuid;
    };

    // ============================================================
    // Housing Room SMSG Responses (0x53xxxx)
    // ============================================================

    class HousingRoomSetLayoutEditModeResponse final : public ServerPacket
    {
    public:
        HousingRoomSetLayoutEditModeResponse() : ServerPacket(SMSG_HOUSING_ROOM_SET_LAYOUT_EDIT_MODE_RESPONSE) { }
        WorldPacket const* Write() override;
        // IDA case 5439488: PackedGUID + uint8(Result) + uint8(bit7=Active)
        ObjectGuid RoomGuid;
        uint8 Result = 0;
        bool Active = false;
    };

    class HousingRoomAddResponse final : public ServerPacket
    {
    public:
        HousingRoomAddResponse() : ServerPacket(SMSG_HOUSING_ROOM_ADD_RESPONSE) { }
        WorldPacket const* Write() override;
        uint8 Result = 0;
        ObjectGuid RoomGuid;
    };

    class HousingRoomRemoveResponse final : public ServerPacket
    {
    public:
        HousingRoomRemoveResponse() : ServerPacket(SMSG_HOUSING_ROOM_REMOVE_RESPONSE) { }
        WorldPacket const* Write() override;
        // IDA case 5439490: PackedGUID + PackedGUID + uint8(Result)
        ObjectGuid RoomGuid;
        ObjectGuid SecondGuid;
        uint8 Result = 0;
    };

    class HousingRoomUpdateResponse final : public ServerPacket
    {
    public:
        HousingRoomUpdateResponse() : ServerPacket(SMSG_HOUSING_ROOM_UPDATE_RESPONSE) { }
        WorldPacket const* Write() override;
        uint8 Result = 0;
        ObjectGuid RoomGuid;
    };

    class HousingRoomSetComponentThemeResponse final : public ServerPacket
    {
    public:
        HousingRoomSetComponentThemeResponse() : ServerPacket(SMSG_HOUSING_ROOM_SET_COMPONENT_THEME_RESPONSE) { }
        WorldPacket const* Write() override;
        // IDA case 5439492: PackedGUID + uint32(arrayCount) + uint32(ThemeSetID) + uint8(Result) + uint32[arrayCount]
        ObjectGuid RoomGuid;
        uint32 ThemeSetID = 0;
        uint8 Result = 0;
        std::vector<uint32> ComponentIDs;
    };

    class HousingRoomApplyComponentMaterialsResponse final : public ServerPacket
    {
    public:
        HousingRoomApplyComponentMaterialsResponse() : ServerPacket(SMSG_HOUSING_ROOM_APPLY_COMPONENT_MATERIALS_RESPONSE) { }
        WorldPacket const* Write() override;
        // IDA case 5439493: PackedGUID + uint32(arrayCount) + uint32(TextureRecordID) + uint8(Result) + uint32[arrayCount]
        ObjectGuid RoomGuid;
        uint32 RoomComponentTextureRecordID = 0;
        uint8 Result = 0;
        std::vector<uint32> ComponentIDs;
    };

    class HousingRoomSetDoorTypeResponse final : public ServerPacket
    {
    public:
        HousingRoomSetDoorTypeResponse() : ServerPacket(SMSG_HOUSING_ROOM_SET_DOOR_TYPE_RESPONSE) { }
        WorldPacket const* Write() override;
        // IDA case 5439494: PackedGUID + uint32(ComponentID) + uint8(DoorType) + uint8(Result)
        ObjectGuid RoomGuid;
        uint32 ComponentID = 0;
        uint8 DoorType = 0;
        uint8 Result = 0;
    };

    class HousingRoomSetCeilingTypeResponse final : public ServerPacket
    {
    public:
        HousingRoomSetCeilingTypeResponse() : ServerPacket(SMSG_HOUSING_ROOM_SET_CEILING_TYPE_RESPONSE) { }
        WorldPacket const* Write() override;
        // IDA case 5439495: PackedGUID + uint32(ComponentID) + uint8(CeilingType) + uint8(Result)
        ObjectGuid RoomGuid;
        uint32 ComponentID = 0;
        uint8 CeilingType = 0;
        uint8 Result = 0;
    };

    // ============================================================
    // Housing Services SMSG Responses (0x54xxxx)
    // ============================================================

    class HousingSvcsNotifyPermissionsFailure final : public ServerPacket
    {
    public:
        HousingSvcsNotifyPermissionsFailure() : ServerPacket(SMSG_HOUSING_SVCS_NOTIFY_PERMISSIONS_FAILURE) { }
        WorldPacket const* Write() override;
        uint8 FailureType = 0;  // IDA-verified: 2 separate uint8 reads, not 1 uint16
        uint8 ErrorCode = 0;
    };

    class HousingSvcsGuildCreateNeighborhoodNotification final : public ServerPacket
    {
    public:
        HousingSvcsGuildCreateNeighborhoodNotification() : ServerPacket(SMSG_HOUSING_SVCS_GUILD_CREATE_NEIGHBORHOOD_NOTIFICATION) { }
        WorldPacket const* Write() override;
        // IDA case 5505025: PackedGUID + uint8(flag) + uint8(nameLen) + String(nameLen)
        ObjectGuid NeighborhoodGuid;
        uint8 Flag = 0;
        std::string Name;
    };

    class HousingSvcsCreateNeighborhoodResponse final : public ServerPacket
    {
    public:
        HousingSvcsCreateNeighborhoodResponse() : ServerPacket(SMSG_HOUSING_SVCS_CREATE_NEIGHBORHOOD_RESPONSE) { }
        WorldPacket const* Write() override;
        // IDA case 5505026: JamCliHouseFinderNeighborhood_base + uint8(trailing)
        JamCliHouseFinderNeighborhood Neighborhood;
        uint8 TrailingResult = 0;
    };

    class HousingSvcsCreateCharterNeighborhoodResponse final : public ServerPacket
    {
    public:
        HousingSvcsCreateCharterNeighborhoodResponse() : ServerPacket(SMSG_HOUSING_SVCS_CREATE_CHARTER_NEIGHBORHOOD_RESPONSE) { }
        WorldPacket const* Write() override;
        // IDA case 5505027: JamCliHouseFinderNeighborhood_base + uint8(trailing)
        JamCliHouseFinderNeighborhood Neighborhood;
        uint8 TrailingResult = 0;
    };

    class HousingSvcsNeighborhoodReservePlotResponse final : public ServerPacket
    {
    public:
        HousingSvcsNeighborhoodReservePlotResponse() : ServerPacket(SMSG_HOUSING_SVCS_NEIGHBORHOOD_RESERVE_PLOT_RESPONSE) { }
        WorldPacket const* Write() override;
        // Sniff-verified wire format: single uint8 Result (1 byte total)
        uint8 Result = 0;
    };

    class HousingSvcsClearPlotReservationResponse final : public ServerPacket
    {
    public:
        HousingSvcsClearPlotReservationResponse() : ServerPacket(SMSG_HOUSING_SVCS_CLEAR_PLOT_RESERVATION_RESPONSE) { }
        WorldPacket const* Write() override;
        uint8 Result = 0;
        ObjectGuid NeighborhoodGuid;
    };

    class HousingSvcsHouseExpirationNotification final : public ServerPacket
    {
    public:
        HousingSvcsHouseExpirationNotification() : ServerPacket(SMSG_HOUSING_SVCS_HOUSE_EXPIRATION_NOTIFICATION) { }
        WorldPacket const* Write() override;
        // IDA case 5505030: uint8 + uint64 + uint32
        uint8 Type = 0;
        uint64 Timestamp = 0;
        uint32 Duration = 0;
    };

    class HousingSvcsRelinquishHouseResponse final : public ServerPacket
    {
    public:
        HousingSvcsRelinquishHouseResponse() : ServerPacket(SMSG_HOUSING_SVCS_RELINQUISH_HOUSE_RESPONSE) { }
        WorldPacket const* Write() override;
        // IDA case 5505031: uint8(Result) + PackedGUID + PackedGUID
        uint8 Result = 0;
        ObjectGuid HouseGuid;
        ObjectGuid NeighborhoodGuid;
    };

    class HousingSvcsCancelRelinquishHouseResponse final : public ServerPacket
    {
    public:
        HousingSvcsCancelRelinquishHouseResponse() : ServerPacket(SMSG_HOUSING_SVCS_CANCEL_RELINQUISH_HOUSE_RESPONSE) { }
        WorldPacket const* Write() override;
        // IDA case 5505032: uint32 + PackedGUID + uint8
        uint32 Field1 = 0;
        ObjectGuid HouseGuid;
        uint8 Result = 0;
    };

    // IDA-verified: JamHousingSearchResult entry (stride 96)
    // Deserializer: Deserialize_JamHousingSearchResult (0x7FF724C7D4A0)
    struct JamHousingSearchResult
    {
        ObjectGuid PrimaryGUID;      // +0: Neighborhood/search entity GUID
        uint64 SortKey = 0;          // +16: timestamp or sort priority
        uint64 SortData = 0;         // +24: additional sort data
        uint8 StatusType = 0;        // +32: result status type
        ObjectGuid SecondaryGUID;    // +40: related entity GUID (owner)
        std::string Name;            // +56: result name string (len at +64)
    };

    class HousingSvcsSearchNeighborhoodsResponse final : public ServerPacket
    {
    public:
        HousingSvcsSearchNeighborhoodsResponse() : ServerPacket(SMSG_HOUSING_SVCS_SEARCH_NEIGHBORHOODS_RESPONSE) { }
        WorldPacket const* Write() override;
        // IDA case 5505033: uint32(count) + uint8(flags) + JamHousingSearchResult[count]
        std::vector<JamHousingSearchResult> Results;
        uint8 Flags = 0;
    };

    class HousingSvcsGetNeighborhoodDetailsResponse final : public ServerPacket
    {
    public:
        HousingSvcsGetNeighborhoodDetailsResponse() : ServerPacket(SMSG_HOUSING_SVCS_GET_NEIGHBORHOOD_DETAILS_RESPONSE) { }
        WorldPacket const* Write() override;
        // IDA case 5505034 (sub_7FF724C7D700):
        // uint32(count1) + uint32(count2) + PackedGUID + uint64 + uint32(count3)
        // + uint32[count3] + JamCliHouse[count1] + JamCliHouse[count2]
        std::vector<JamCliHouse> PrimaryHouses;
        std::vector<JamCliHouse> SecondaryHouses;
        ObjectGuid NeighborhoodGUID;
        uint64 Field1 = 0;
        std::vector<uint32> ExtraIds;
    };

    class HousingSvcsGetPlayerHousesInfoResponse final : public ServerPacket
    {
    public:
        HousingSvcsGetPlayerHousesInfoResponse() : ServerPacket(SMSG_HOUSING_SVCS_GET_PLAYER_HOUSES_INFO_RESPONSE) { }
        WorldPacket const* Write() override;

        // IDA case 5505035: uint32(count) + uint8(result) + JamCliHouse[count]
        std::vector<JamCliHouse> Houses;
        uint8 Result = 0;
    };

    class HousingSvcsPlayerViewHousesResponse final : public ServerPacket
    {
    public:
        HousingSvcsPlayerViewHousesResponse() : ServerPacket(SMSG_HOUSING_SVCS_PLAYER_VIEW_HOUSES_RESPONSE) { }
        WorldPacket const* Write() override;

        // IDA case 5505036: uint32(count) + uint8(result) + JamCliHouse[count]
        std::vector<JamCliHouse> Houses;
        uint8 Result = 0;
    };

    class HousingSvcsGetNeighborhoodHousesResponse final : public ServerPacket
    {
    public:
        HousingSvcsGetNeighborhoodHousesResponse() : ServerPacket(SMSG_HOUSING_SVCS_GET_NEIGHBORHOOD_HOUSES_RESPONSE) { }
        WorldPacket const* Write() override;
        // IDA case 5505037: uint32(count) + uint8(result) + JamCliHouse[count]
        std::vector<JamCliHouse> Houses;
        uint8 Result = 0;
    };

    class HousingSvcsMoveHouseResponse final : public ServerPacket
    {
    public:
        HousingSvcsMoveHouseResponse() : ServerPacket(SMSG_HOUSING_SVCS_MOVE_HOUSE_RESPONSE) { }
        WorldPacket const* Write() override;
        // IDA case 5505038: uint8 only (shared with 5505029/5505039)
        uint8 Result = 0;
    };

    class HousingSvcsSwapPlotsResponse final : public ServerPacket
    {
    public:
        HousingSvcsSwapPlotsResponse() : ServerPacket(SMSG_HOUSING_SVCS_SWAP_PLOTS_RESPONSE) { }
        WorldPacket const* Write() override;
        // IDA case 5505039: uint8 only (shared with 5505029/5505038)
        uint8 Result = 0;
    };

    class HousingSvcsChangeHouseCosmeticOwner final : public ServerPacket
    {
    public:
        HousingSvcsChangeHouseCosmeticOwner() : ServerPacket(SMSG_HOUSING_SVCS_CHANGE_HOUSE_COSMETIC_OWNER) { }
        WorldPacket const* Write() override;
        // IDA case 5505040: uint8(result) + PackedGUID + PackedGUID
        uint8 Result = 0;
        ObjectGuid HouseGuid;
        ObjectGuid NewOwnerGuid;
    };

    // Account-level housing collection notifications
    class AccountHousingRoomAdded final : public ServerPacket
    {
    public:
        AccountHousingRoomAdded() : ServerPacket(SMSG_ACCOUNT_HOUSING_ROOM_ADDED) { }
        WorldPacket const* Write() override;

        ObjectGuid RoomGuid;
    };

    class AccountHousingFixtureAdded final : public ServerPacket
    {
    public:
        AccountHousingFixtureAdded() : ServerPacket(SMSG_ACCOUNT_HOUSING_FIXTURE_ADDED) { }
        WorldPacket const* Write() override;

        ObjectGuid FixtureGuid;
        std::string Name;
    };

    class AccountHousingThemeAdded final : public ServerPacket
    {
    public:
        AccountHousingThemeAdded() : ServerPacket(SMSG_ACCOUNT_HOUSING_THEME_ADDED) { }
        WorldPacket const* Write() override;

        ObjectGuid ThemeGuid;
        std::string Name;
    };

    class AccountHousingRoomComponentTextureAdded final : public ServerPacket
    {
    public:
        AccountHousingRoomComponentTextureAdded() : ServerPacket(SMSG_ACCOUNT_HOUSING_ROOM_COMPONENT_TEXTURE_ADDED) { }
        WorldPacket const* Write() override;

        ObjectGuid TextureGuid;
        std::string Name;
    };

    class HousingSvcsUpdateHousesLevelFavor final : public ServerPacket
    {
    public:
        HousingSvcsUpdateHousesLevelFavor() : ServerPacket(SMSG_HOUSING_SVCS_UPDATE_HOUSES_LEVEL_FAVOR) { }
        WorldPacket const* Write() override;

        // IDA case 5505041 (sub_7FF724C7D8D0):
        // uint8 + uint32 + uint32 + uint32(count) + Entry[count]{3×PackedGUID + uint32 + uint32 + uint8 + uint8(bit7)}
        uint8 Type = 0;
        uint32 Field1 = 0;
        uint32 Field2 = 0;

        struct LevelFavorEntry
        {
            ObjectGuid OwnerGUID;
            ObjectGuid HouseGUID;
            ObjectGuid NeighborhoodGUID;
            uint32 FavorAmount = 0;
            uint32 Level = 0;
            uint8 Flags = 0;
            bool HasOptional = false;
        };
        std::vector<LevelFavorEntry> Entries;
    };

    class HousingSvcsGuildAddHouseNotification final : public ServerPacket
    {
    public:
        HousingSvcsGuildAddHouseNotification() : ServerPacket(SMSG_HOUSING_SVCS_GUILD_ADD_HOUSE_NOTIFICATION) { }
        WorldPacket const* Write() override;
        // IDA case 5505042: JamCliHouse (Deserialize_ResidentArray)
        JamCliHouse House;
    };

    class HousingSvcsGuildRemoveHouseNotification final : public ServerPacket
    {
    public:
        HousingSvcsGuildRemoveHouseNotification() : ServerPacket(SMSG_HOUSING_SVCS_GUILD_REMOVE_HOUSE_NOTIFICATION) { }
        WorldPacket const* Write() override;
        // IDA case 5505043: JamCliHouse (Deserialize_ResidentArray)
        JamCliHouse House;
    };

    class HousingSvcsGuildAppendNeighborhoodNotification final : public ServerPacket
    {
    public:
        HousingSvcsGuildAppendNeighborhoodNotification() : ServerPacket(SMSG_HOUSING_SVCS_GUILD_APPEND_NEIGHBORHOOD_NOTIFICATION) { }
        WorldPacket const* Write() override;
        // IDA case 5505044: JamCliHouseFinderNeighborhood_base (sub_7FF724C3F040)
        JamCliHouseFinderNeighborhood Neighborhood;
    };

    class HousingSvcsGuildRenameNeighborhoodNotification final : public ServerPacket
    {
    public:
        HousingSvcsGuildRenameNeighborhoodNotification() : ServerPacket(SMSG_HOUSING_SVCS_GUILD_RENAME_NEIGHBORHOOD_NOTIFICATION) { }
        WorldPacket const* Write() override;
        // IDA case 5505045: uint8(nameLen) + String(nameLen) — NO GUID
        std::string NewName;
    };

    class HousingSvcsGuildGetHousingInfoResponse final : public ServerPacket
    {
    public:
        HousingSvcsGuildGetHousingInfoResponse() : ServerPacket(SMSG_HOUSING_SVCS_GUILD_GET_HOUSING_INFO_RESPONSE) { }
        WorldPacket const* Write() override;
        // IDA case 5505046: uint32(count) + JamCliHouseFinderNeighborhood_base[count] (sub_7FF724C3F160)
        std::vector<JamCliHouseFinderNeighborhood> Neighborhoods;
    };

    class HousingSvcsAcceptNeighborhoodOwnershipResponse final : public ServerPacket
    {
    public:
        HousingSvcsAcceptNeighborhoodOwnershipResponse() : ServerPacket(SMSG_HOUSING_SVCS_ACCEPT_NEIGHBORHOOD_OWNERSHIP_RESPONSE) { }
        WorldPacket const* Write() override;
        uint8 Result = 0;
        ObjectGuid NeighborhoodGuid;
    };

    class HousingSvcsRejectNeighborhoodOwnershipResponse final : public ServerPacket
    {
    public:
        HousingSvcsRejectNeighborhoodOwnershipResponse() : ServerPacket(SMSG_HOUSING_SVCS_REJECT_NEIGHBORHOOD_OWNERSHIP_RESPONSE) { }
        WorldPacket const* Write() override;
        uint8 Result = 0;
        ObjectGuid NeighborhoodGuid;
    };

    class HousingSvcsNeighborhoodOwnershipTransferredResponse final : public ServerPacket
    {
    public:
        HousingSvcsNeighborhoodOwnershipTransferredResponse() : ServerPacket(SMSG_HOUSING_SVCS_NEIGHBORHOOD_OWNERSHIP_TRANSFERRED_RESPONSE) { }
        WorldPacket const* Write() override;
        // IDA case 5505049: bit-packed blob encoding via ai_Decode_ClientOpcodeData
        // First byte: top 6 bits = blobSize (49 on success, 0 on failure)
        //             bottom 2 bits = result code (0=success, 1-3=error)
        // Blob: 3×16-byte raw ObjectGuids + 1 byte = 49 bytes total
        uint8 Result = 0;
        ObjectGuid OwnerGUID;
        ObjectGuid HouseGUID;
        ObjectGuid AccountGUID;
        uint8 HouseLevel = 0;
    };

    class HousingSvcsGetPotentialHouseOwnersResponse final : public ServerPacket
    {
    public:
        HousingSvcsGetPotentialHouseOwnersResponse() : ServerPacket(SMSG_HOUSING_SVCS_GET_POTENTIAL_HOUSE_OWNERS_RESPONSE) { }
        WorldPacket const* Write() override;

        // IDA case 5505050 (sub_7FF724C7DA70): NO Result byte
        // uint32(count) + Entry[count]{PackedGUID + uint32 + uint8 + uint8 + uint8(bit7→nameLen) + String(nameLen)}
        struct PotentialOwnerData
        {
            ObjectGuid PlayerGuid;
            uint32 Field1 = 0;          // offset 324 in client struct
            uint8 AccessLevel = 0;      // offset 328 in client struct
            std::string PlayerName;     // offset 16 in client struct, variable length
        };
        std::vector<PotentialOwnerData> PotentialOwners;
    };

    class HousingSvcsUpdateHouseSettingsResponse final : public ServerPacket
    {
    public:
        HousingSvcsUpdateHouseSettingsResponse() : ServerPacket(SMSG_HOUSING_SVCS_UPDATE_HOUSE_SETTINGS_RESPONSE) { }
        WorldPacket const* Write() override;
        // IDA case 5505051: uint8(Result) + JamCliHouse
        uint8 Result = 0;
        JamCliHouse House;
    };

    class HousingSvcsGetHouseFinderInfoResponse final : public ServerPacket
    {
    public:
        HousingSvcsGetHouseFinderInfoResponse() : ServerPacket(SMSG_HOUSING_SVCS_GET_HOUSE_FINDER_INFO_RESPONSE) { }
        WorldPacket const* Write() override;
        uint8 Result = 0;
        std::vector<JamCliHouseFinderNeighborhood> Entries;
    };

    class HousingSvcsGetHouseFinderNeighborhoodResponse final : public ServerPacket
    {
    public:
        HousingSvcsGetHouseFinderNeighborhoodResponse() : ServerPacket(SMSG_HOUSING_SVCS_GET_HOUSE_FINDER_NEIGHBORHOOD_RESPONSE) { }
        WorldPacket const* Write() override;
        uint8 Result = 0;
        JamCliHouseFinderNeighborhood Neighborhood; // single entry, not array
    };

    class HousingSvcsGetBnetFriendNeighborhoodsResponse final : public ServerPacket
    {
    public:
        HousingSvcsGetBnetFriendNeighborhoodsResponse() : ServerPacket(SMSG_HOUSING_SVCS_GET_BNET_FRIEND_NEIGHBORHOODS_RESPONSE) { }
        WorldPacket const* Write() override;
        uint8 Result = 0;

        // IDA: uint8(result) + uint32(count) + JamCliHouseFinderNeighborhood[count]
        std::vector<JamCliHouseFinderNeighborhood> Entries;
    };

    class HousingSvcsHouseFinderForceRefresh final : public ServerPacket
    {
    public:
        HousingSvcsHouseFinderForceRefresh() : ServerPacket(SMSG_HOUSING_SVCS_HOUSE_FINDER_FORCE_REFRESH) { }
        WorldPacket const* Write() override;
    };

    class HousingSvcRequestPlayerReloadData final : public ServerPacket
    {
    public:
        HousingSvcRequestPlayerReloadData() : ServerPacket(SMSG_HOUSING_SVC_REQUEST_PLAYER_RELOAD_DATA) { }
        WorldPacket const* Write() override;
    };

    class HousingSvcsDeleteAllNeighborhoodInvitesResponse final : public ServerPacket
    {
    public:
        HousingSvcsDeleteAllNeighborhoodInvitesResponse() : ServerPacket(SMSG_HOUSING_SVCS_DELETE_ALL_NEIGHBORHOOD_INVITES_RESPONSE) { }
        WorldPacket const* Write() override;
        uint8 Result = 0;
        ObjectGuid NeighborhoodGuid;
    };

    class HousingSvcsSetNeighborhoodSettingsResponse final : public ServerPacket
    {
    public:
        HousingSvcsSetNeighborhoodSettingsResponse() : ServerPacket(SMSG_HOUSING_SVCS_SET_NEIGHBORHOOD_SETTINGS_RESPONSE) { }
        WorldPacket const* Write() override;
        // IDA case 5505058: PackedGUID + uint8
        ObjectGuid NeighborhoodGuid;
        uint8 Result = 0;
    };

    // ============================================================
    // Housing General SMSG Responses (0x55xxxx)
    // ============================================================

    class HousingHouseStatusResponse final : public ServerPacket
    {
    public:
        HousingHouseStatusResponse() : ServerPacket(SMSG_HOUSING_HOUSE_STATUS_RESPONSE) { }
        WorldPacket const* Write() override;

        // IDA 0x550000: PackedGUID×4 + uint8(Status) + uint8(FlagByte: bit7/bit6/bit5)
        // Position 2 is BNetAccount GUID per sniff byte analysis (hi byte7=0x78)
        ObjectGuid HouseGuid;
        ObjectGuid AccountGuid;         // BNetAccount GUID (sniff-verified at position 2)
        ObjectGuid OwnerPlayerGuid;
        ObjectGuid NeighborhoodGuid;    // Actual neighborhood/housing GUID
        uint8 Status = 0;
        uint8 FlagByte = 0;     // bit7=flag1, bit6=flag2, bit5=flag3
    };

    class HousingGetCurrentHouseInfoResponse final : public ServerPacket
    {
    public:
        HousingGetCurrentHouseInfoResponse() : ServerPacket(SMSG_HOUSING_GET_CURRENT_HOUSE_INFO_RESPONSE) { }
        WorldPacket const* Write() override;

        // Wire format: HouseInfo + uint8 Result
        HouseInfo House;
        uint8 Result = 0;
    };

    class HousingExportHouseResponse final : public ServerPacket
    {
    public:
        HousingExportHouseResponse() : ServerPacket(SMSG_HOUSING_EXPORT_HOUSE_RESPONSE) { }
        WorldPacket const* Write() override;

        // IDA 0x550003: PackedGUID + uint8(Result) + uint8(bit7=HasExportString)
        //   [+ 24bit-BE(strLen) + string(strLen)] + uint32(blobSize) + blob(blobSize)
        ObjectGuid HouseGuid;
        uint8 Result = 0;
        bool HasExportString = false;
        std::string ExportString;
        std::vector<uint8> ExportBlob;
    };

    class HousingSystemHouseSnapshotResponse final : public ServerPacket
    {
    public:
        HousingSystemHouseSnapshotResponse() : ServerPacket(SMSG_HOUSING_SYSTEM_HOUSE_SNAPSHOT_RESPONSE) { }
        WorldPacket const* Write() override;
        uint8 Result = 0;
    };

    class HousingSetHouseNameResponse final : public ServerPacket
    {
    public:
        HousingSetHouseNameResponse() : ServerPacket(SMSG_HOUSING_SET_HOUSE_NAME_RESPONSE) { }
        WorldPacket const* Write() override;

        // IDA 0x550005: uint8(Result) + 24bit-BE(nameLen) + string(Name)
        uint8 Result = 0;
        std::string Name;
    };

    class HousingGetPlayerPermissionsResponse final : public ServerPacket
    {
    public:
        HousingGetPlayerPermissionsResponse() : ServerPacket(SMSG_HOUSING_GET_PLAYER_PERMISSIONS_RESPONSE) { }
        WorldPacket const* Write() override;

        // Wire format (IDA 12.0 verified, 0x550006):
        // PackedGUID + uint8 ResultCode + uint8 Permissions(bits 5,6,7)
        ObjectGuid HouseGuid;
        uint8 ResultCode = 0;
        uint8 PermissionFlags = 0;   // bit7=houseEditingPermitted, bit6=plotEntryPermitted, bit5=houseEntryPermitted
    };

    class HousingResetKioskModeResponse final : public ServerPacket
    {
    public:
        HousingResetKioskModeResponse() : ServerPacket(SMSG_HOUSING_RESET_KIOSK_MODE_RESPONSE) { }
        WorldPacket const* Write() override;
        uint8 Result = 0;  // IDA 12.0 verified (0x550007): single uint8
    };

    class HousingEditorAvailabilityResponse final : public ServerPacket
    {
    public:
        HousingEditorAvailabilityResponse() : ServerPacket(SMSG_HOUSING_EDITOR_AVAILABILITY_RESPONSE) { }
        WorldPacket const* Write() override;
        ObjectGuid HouseGuid;
        uint8 Result = 0;
        uint8 Field_09 = 0;
    };

    class HousingUpdateHouseInfo final : public ServerPacket
    {
    public:
        HousingUpdateHouseInfo() : ServerPacket(SMSG_HOUSING_UPDATE_HOUSE_INFO) { }
        WorldPacket const* Write() override;

        // IDA 0x550004: 3×24bit-BE(strLen) + 3×uint32 + uint8 + 3×string(strLen)
        std::string HouseName;
        std::string HouseDescription;
        std::string HouseExtra;         // third string (purpose TBD)
        uint32 Field1 = 0;
        uint32 Field2 = 0;
        uint32 Field3 = 0;
        uint8 Result = 0;
    };

    // ============================================================
    // Account/Licensing SMSG (0x42xxxx / 0x5Fxxxx)
    // ============================================================

    class AccountExteriorFixtureCollectionUpdate final : public ServerPacket
    {
    public:
        AccountExteriorFixtureCollectionUpdate() : ServerPacket(SMSG_ACCOUNT_EXTERIOR_FIXTURE_COLLECTION_UPDATE) { }
        WorldPacket const* Write() override;
        uint32 FixtureID = 0;
    };

    class AccountHouseTypeCollectionUpdate final : public ServerPacket
    {
    public:
        AccountHouseTypeCollectionUpdate() : ServerPacket(SMSG_ACCOUNT_HOUSE_TYPE_COLLECTION_UPDATE) { }
        WorldPacket const* Write() override;
        uint32 HouseTypeID = 0;
    };

    class AccountRoomCollectionUpdate final : public ServerPacket
    {
    public:
        AccountRoomCollectionUpdate() : ServerPacket(SMSG_ACCOUNT_ROOM_COLLECTION_UPDATE) { }
        WorldPacket const* Write() override;
        uint32 RoomID = 0;
    };

    class AccountRoomThemeCollectionUpdate final : public ServerPacket
    {
    public:
        AccountRoomThemeCollectionUpdate() : ServerPacket(SMSG_ACCOUNT_ROOM_THEME_COLLECTION_UPDATE) { }
        WorldPacket const* Write() override;
        uint32 ThemeID = 0;
    };

    class AccountRoomMaterialCollectionUpdate final : public ServerPacket
    {
    public:
        AccountRoomMaterialCollectionUpdate() : ServerPacket(SMSG_ACCOUNT_ROOM_MATERIAL_COLLECTION_UPDATE) { }
        WorldPacket const* Write() override;
        uint32 MaterialID = 0;
    };

    class InvalidateNeighborhood final : public ServerPacket
    {
    public:
        InvalidateNeighborhood() : ServerPacket(SMSG_INVALIDATE_NEIGHBORHOOD) { }
        WorldPacket const* Write() override;
        ObjectGuid NeighborhoodGuid;
    };

    // ============================================================
    // Decor Licensing/Refund JAM Structs
    // ============================================================

    struct JamClientRefundableDecor
    {
        uint32 DecorID = 0;
        uint64 RefundPrice = 0;
        uint64 ExpiryTime = 0;
        uint32 Flags = 0;
    };

    struct JamLicensedDecorQuantity
    {
        uint32 DecorID = 0;
        uint32 Quantity = 0;
    };

    // ============================================================
    // Initiative JAM Structs
    // ============================================================

    struct JamPlayerInitiativeTaskInfo
    {
        uint32 TaskID = 0;
        uint32 Progress = 0;
        uint32 Status = 0;
    };

    struct NICompletedTasksEntry
    {
        uint32 InitiativeID = 0;
        uint32 TaskID = 0;
        uint32 CycleID = 0;
        uint64 CompletionTime = 0;
        ObjectGuid PlayerGuid;
        uint32 ContributionAmount = 0;
        uint32 Unknown1 = 0;
        uint64 ExtraData = 0;
    };

    // ============================================================
    // Decor Licensing/Refund SMSG Responses (0x42xxxx)
    // ============================================================

    class GetDecorRefundListResponse final : public ServerPacket
    {
    public:
        GetDecorRefundListResponse() : ServerPacket(SMSG_GET_DECOR_REFUND_LIST_RESPONSE) { }
        WorldPacket const* Write() override;
        std::vector<JamClientRefundableDecor> Decors;
    };

    class GetAllLicensedDecorQuantitiesResponse final : public ServerPacket
    {
    public:
        GetAllLicensedDecorQuantitiesResponse() : ServerPacket(SMSG_GET_ALL_LICENSED_DECOR_QUANTITIES_RESPONSE) { }
        WorldPacket const* Write() override;
        std::vector<JamLicensedDecorQuantity> Quantities;
    };

    class LicensedDecorQuantitiesUpdate final : public ServerPacket
    {
    public:
        LicensedDecorQuantitiesUpdate() : ServerPacket(SMSG_LICENSED_DECOR_QUANTITIES_UPDATE) { }
        WorldPacket const* Write() override;
        std::vector<JamLicensedDecorQuantity> Quantities;
    };

    // ============================================================
    // Initiative System SMSG Responses (0x4203xx)
    // ============================================================

    class InitiativeServiceStatus final : public ServerPacket
    {
    public:
        InitiativeServiceStatus() : ServerPacket(SMSG_INITIATIVE_SERVICE_STATUS) { }
        WorldPacket const* Write() override;
        bool ServiceEnabled = false;
    };

    class GetPlayerInitiativeInfoResult final : public ServerPacket
    {
    public:
        GetPlayerInitiativeInfoResult() : ServerPacket(SMSG_GET_PLAYER_INITIATIVE_INFO_RESULT) { }
        WorldPacket const* Write() override;

        ObjectGuid NeighborhoodGUID;
        bool HasError = false;
        bool HasInitiativeData = false;

        // InitiativeInfo block (only written when HasInitiativeData = true)
        int64 RemainingDuration = 0;
        int32 CurrentInitiativeID = 0;
        int32 CurrentMilestoneID = -1;
        int32 CurrentCycleID = 0;
        float ProgressRequired = 0.0f;
        float CurrentProgress = 0.0f;
        float PlayerTotalContribution = 0.0f;

        std::vector<JamPlayerInitiativeTaskInfo> Tasks;
    };

    class GetInitiativeActivityLogResult final : public ServerPacket
    {
    public:
        GetInitiativeActivityLogResult() : ServerPacket(SMSG_GET_INITIATIVE_ACTIVITY_LOG_RESULT) { }
        WorldPacket const* Write() override;
        uint8 Result = 0;
        std::vector<NICompletedTasksEntry> CompletedTasks;
    };

    class InitiativeTaskComplete final : public ServerPacket
    {
    public:
        InitiativeTaskComplete() : ServerPacket(SMSG_INITIATIVE_TASK_COMPLETE) { }
        WorldPacket const* Write() override;
        uint32 InitiativeID = 0;
        uint32 TaskID = 0;
    };

    class InitiativeComplete final : public ServerPacket
    {
    public:
        InitiativeComplete() : ServerPacket(SMSG_INITIATIVE_COMPLETE) { }
        WorldPacket const* Write() override;
        uint32 InitiativeID = 0;
    };

    class ClearInitiativeTaskCriteriaProgress final : public ServerPacket
    {
    public:
        ClearInitiativeTaskCriteriaProgress() : ServerPacket(SMSG_CLEAR_INITIATIVE_TASK_CRITERIA_PROGRESS) { }
        WorldPacket const* Write() override;
        uint32 InitiativeID = 0;
        uint32 TaskID = 0;
    };

    class GetInitiativeRewardsResult final : public ServerPacket
    {
    public:
        GetInitiativeRewardsResult() : ServerPacket(SMSG_GET_INITIATIVE_REWARDS_RESULT) { }
        WorldPacket const* Write() override;
        uint8 Result = 0;
    };

    class InitiativeRewardAvailable final : public ServerPacket
    {
    public:
        InitiativeRewardAvailable() : ServerPacket(SMSG_INITIATIVE_REWARD_AVAILABLE) { }
        WorldPacket const* Write() override;
        uint32 InitiativeID = 0;
        uint32 MilestoneIndex = 0;
    };

    // IDA-verified: SMSG_INITIATIVE_UPDATE_STATUS carries 1 byte (NeighborhoodInitiativeUpdateStatus)
    // Sent when initiative state changes: Started, MilestoneCompleted, Completed, Failed
    class InitiativeUpdateStatus final : public ServerPacket
    {
    public:
        InitiativeUpdateStatus() : ServerPacket(SMSG_INITIATIVE_UPDATE_STATUS) { }
        WorldPacket const* Write() override;
        uint8 Status = 0; // NeighborhoodInitiativeUpdateStatus
    };

    // IDA-verified: SMSG_INITIATIVE_POINTS_UPDATE carries 2 uint32 (current, max)
    // Sent after progress changes to update the client's progress bar
    class InitiativePointsUpdate final : public ServerPacket
    {
    public:
        InitiativePointsUpdate() : ServerPacket(SMSG_INITIATIVE_POINTS_UPDATE) { }
        WorldPacket const* Write() override;
        uint32 CurrentPoints = 0;
        uint32 MaxPoints = 0;
    };

    // IDA-verified: SMSG_INITIATIVE_MILESTONE_UPDATE carries 3 bytes
    // Sent when milestone state changes (milestoneIndex, reached, flags)
    class InitiativeMilestoneUpdate final : public ServerPacket
    {
    public:
        InitiativeMilestoneUpdate() : ServerPacket(SMSG_INITIATIVE_MILESTONE_UPDATE) { }
        WorldPacket const* Write() override;
        uint8 MilestoneIndex = 0;
        uint8 Reached = 0;
        uint8 Flags = 0;
    };

    // IDA-verified: SMSG_INITIATIVE_CHEST_RESULT carries 1 uint32 (NeighborhoodInitiativeChestResult)
    class InitiativeChestResult final : public ServerPacket
    {
    public:
        InitiativeChestResult() : ServerPacket(SMSG_INITIATIVE_CHEST_RESULT) { }
        WorldPacket const* Write() override;
        uint32 Result = 0; // NeighborhoodInitiativeChestResult
    };

    // IDA-verified: SMSG_INITIATIVE_TRACKED_UPDATED carries a packed GUID (8 bytes)
    class InitiativeTrackedUpdated final : public ServerPacket
    {
    public:
        InitiativeTrackedUpdated() : ServerPacket(SMSG_INITIATIVE_TRACKED_UPDATED) { }
        WorldPacket const* Write() override;
        ObjectGuid NeighborhoodGUID;
    };

    // ============================================================
    // Photo Sharing SMSG Responses (0x42037x)
    // ============================================================

    class HousingPhotoSharingAuthorizationResult final : public ServerPacket
    {
    public:
        HousingPhotoSharingAuthorizationResult() : ServerPacket(SMSG_HOUSING_PHOTO_SHARING_AUTHORIZATION_RESULT) { }
        WorldPacket const* Write() override;
        uint8 Result = 0;
    };

    class HousingPhotoSharingAuthorizationClearedResult final : public ServerPacket
    {
    public:
        HousingPhotoSharingAuthorizationClearedResult() : ServerPacket(SMSG_HOUSING_PHOTO_SHARING_AUTHORIZATION_CLEARED_RESULT) { }
        WorldPacket const* Write() override;
        uint8 Result = 0;
    };
}

namespace WorldPackets::Neighborhood
{
    // ============================================================
    // Neighborhood Charter System (0x37xxxx)
    // ============================================================

    class NeighborhoodCharterOpenConfirmationUI final : public ClientPacket
    {
    public:
        explicit NeighborhoodCharterOpenConfirmationUI(WorldPacket&& packet) : ClientPacket(CMSG_NEIGHBORHOOD_CHARTER_OPEN_CONFIRMATION_UI, std::move(packet)) { }

        void Read() override { }
    };

    class NeighborhoodCharterCreate final : public ClientPacket
    {
    public:
        explicit NeighborhoodCharterCreate(WorldPacket&& packet) : ClientPacket(CMSG_NEIGHBORHOOD_CHARTER_CREATE, std::move(packet)) { }

        void Read() override;

        uint32 NeighborhoodMapID = 0;
        uint32 FactionFlags = 0;
        std::string Name;
    };

    class NeighborhoodCharterEdit final : public ClientPacket
    {
    public:
        explicit NeighborhoodCharterEdit(WorldPacket&& packet) : ClientPacket(CMSG_NEIGHBORHOOD_CHARTER_EDIT, std::move(packet)) { }

        void Read() override;

        uint32 NeighborhoodMapID = 0;
        uint32 FactionFlags = 0;
        std::string Name;
    };

    class NeighborhoodCharterFinalize final : public ClientPacket
    {
    public:
        explicit NeighborhoodCharterFinalize(WorldPacket&& packet) : ClientPacket(CMSG_NEIGHBORHOOD_CHARTER_FINALIZE, std::move(packet)) { }

        void Read() override { }
    };

    class NeighborhoodCharterAddSignature final : public ClientPacket
    {
    public:
        explicit NeighborhoodCharterAddSignature(WorldPacket&& packet) : ClientPacket(CMSG_NEIGHBORHOOD_CHARTER_ADD_SIGNATURE, std::move(packet)) { }

        void Read() override;

        ObjectGuid CharterGuid;
    };

    class NeighborhoodCharterSendSignatureRequest final : public ClientPacket
    {
    public:
        explicit NeighborhoodCharterSendSignatureRequest(WorldPacket&& packet) : ClientPacket(CMSG_NEIGHBORHOOD_CHARTER_SEND_SIGNATURE_REQUEST, std::move(packet)) { }

        void Read() override;

        ObjectGuid TargetPlayerGuid;
    };

    class NeighborhoodCharterSignResponsePacket final : public ClientPacket
    {
    public:
        explicit NeighborhoodCharterSignResponsePacket(WorldPacket&& packet) : ClientPacket(CMSG_NEIGHBORHOOD_CHARTER_SIGN_RESPONSE, std::move(packet)) { }
        void Read() override;
        ObjectGuid CharterGuid;
    };

    class NeighborhoodCharterRemoveSignature final : public ClientPacket
    {
    public:
        explicit NeighborhoodCharterRemoveSignature(WorldPacket&& packet) : ClientPacket(CMSG_NEIGHBORHOOD_CHARTER_REMOVE_SIGNATURE, std::move(packet)) { }
        void Read() override;
        ObjectGuid CharterGuid;
        ObjectGuid SignerGuid;
    };

    // ============================================================
    // Neighborhood Management System (0x38xxxx)
    // ============================================================

    class NeighborhoodUpdateName final : public ClientPacket
    {
    public:
        explicit NeighborhoodUpdateName(WorldPacket&& packet) : ClientPacket(CMSG_NEIGHBORHOOD_UPDATE_NAME, std::move(packet)) { }

        void Read() override;

        std::string NewName;
    };

    class NeighborhoodSetPublicFlag final : public ClientPacket
    {
    public:
        explicit NeighborhoodSetPublicFlag(WorldPacket&& packet) : ClientPacket(CMSG_NEIGHBORHOOD_SET_PUBLIC_FLAG, std::move(packet)) { }

        void Read() override;

        ObjectGuid NeighborhoodGuid;
        bool IsPublic = false;
    };

    class NeighborhoodAddSecondaryOwner final : public ClientPacket
    {
    public:
        explicit NeighborhoodAddSecondaryOwner(WorldPacket&& packet) : ClientPacket(CMSG_NEIGHBORHOOD_ADD_SECONDARY_OWNER, std::move(packet)) { }

        void Read() override;

        ObjectGuid PlayerGuid;
    };

    class NeighborhoodRemoveSecondaryOwner final : public ClientPacket
    {
    public:
        explicit NeighborhoodRemoveSecondaryOwner(WorldPacket&& packet) : ClientPacket(CMSG_NEIGHBORHOOD_REMOVE_SECONDARY_OWNER, std::move(packet)) { }

        void Read() override;

        ObjectGuid PlayerGuid;
    };

    class NeighborhoodInviteResident final : public ClientPacket
    {
    public:
        explicit NeighborhoodInviteResident(WorldPacket&& packet) : ClientPacket(CMSG_NEIGHBORHOOD_INVITE_RESIDENT, std::move(packet)) { }

        void Read() override;

        ObjectGuid PlayerGuid;
    };

    class NeighborhoodCancelInvitation final : public ClientPacket
    {
    public:
        explicit NeighborhoodCancelInvitation(WorldPacket&& packet) : ClientPacket(CMSG_NEIGHBORHOOD_CANCEL_INVITATION, std::move(packet)) { }

        void Read() override;

        ObjectGuid InviteeGuid;
    };

    class NeighborhoodPlayerDeclineInvite final : public ClientPacket
    {
    public:
        explicit NeighborhoodPlayerDeclineInvite(WorldPacket&& packet) : ClientPacket(CMSG_NEIGHBORHOOD_PLAYER_DECLINE_INVITE, std::move(packet)) { }

        void Read() override;

        ObjectGuid NeighborhoodGuid;
    };

    class NeighborhoodPlayerGetInvite final : public ClientPacket
    {
    public:
        explicit NeighborhoodPlayerGetInvite(WorldPacket&& packet) : ClientPacket(CMSG_NEIGHBORHOOD_PLAYER_GET_INVITE, std::move(packet)) { }

        void Read() override { }
    };

    class NeighborhoodGetInvites final : public ClientPacket
    {
    public:
        explicit NeighborhoodGetInvites(WorldPacket&& packet) : ClientPacket(CMSG_NEIGHBORHOOD_GET_INVITES, std::move(packet)) { }

        void Read() override { }
    };

    class NeighborhoodBuyHouse final : public ClientPacket
    {
    public:
        explicit NeighborhoodBuyHouse(WorldPacket&& packet) : ClientPacket(CMSG_NEIGHBORHOOD_BUY_HOUSE, std::move(packet)) { }

        void Read() override;

        // Sniff 12.0.1: uint32(HouseStyleID) + PackedGUID(CornerstoneGuid) + uint16(Padding)
        ObjectGuid CornerstoneGuid;
        uint32 HouseStyleID = 0;
        uint16 Padding = 0;
    };

    class NeighborhoodMoveHouse final : public ClientPacket
    {
    public:
        explicit NeighborhoodMoveHouse(WorldPacket&& packet) : ClientPacket(CMSG_NEIGHBORHOOD_MOVE_HOUSE, std::move(packet)) { }

        void Read() override;

        ObjectGuid NeighborhoodGuid;
        ObjectGuid PlotGuid;
    };

    class NeighborhoodOpenCornerstoneUI final : public ClientPacket
    {
    public:
        explicit NeighborhoodOpenCornerstoneUI(WorldPacket&& packet) : ClientPacket(CMSG_NEIGHBORHOOD_OPEN_CORNERSTONE_UI, std::move(packet)) { }

        void Read() override;

        ObjectGuid NeighborhoodGuid;
        uint32 PlotIndex = 0;
    };

    class NeighborhoodOfferOwnership final : public ClientPacket
    {
    public:
        explicit NeighborhoodOfferOwnership(WorldPacket&& packet) : ClientPacket(CMSG_NEIGHBORHOOD_OFFER_OWNERSHIP, std::move(packet)) { }

        void Read() override;

        ObjectGuid NewOwnerGuid;
    };

    class NeighborhoodGetRoster final : public ClientPacket
    {
    public:
        explicit NeighborhoodGetRoster(WorldPacket&& packet) : ClientPacket(CMSG_NEIGHBORHOOD_GET_ROSTER, std::move(packet)) { }

        void Read() override;

        ObjectGuid NeighborhoodGuid;
    };

    class NeighborhoodEvictPlot final : public ClientPacket
    {
    public:
        explicit NeighborhoodEvictPlot(WorldPacket&& packet) : ClientPacket(CMSG_NEIGHBORHOOD_EVICT_PLOT, std::move(packet)) { }

        void Read() override;

        ObjectGuid NeighborhoodGuid;
        uint32 PlotIndex = 0;
    };

    class NeighborhoodCancelInvitationAlt final : public ClientPacket
    {
    public:
        explicit NeighborhoodCancelInvitationAlt(WorldPacket&& packet) : ClientPacket(CMSG_NEIGHBORHOOD_CANCEL_INVITATION_ALT, std::move(packet)) { }
        void Read() override;
        ObjectGuid InviteeGuid;
    };

    class NeighborhoodInviteNotificationAck final : public ClientPacket
    {
    public:
        explicit NeighborhoodInviteNotificationAck(WorldPacket&& packet) : ClientPacket(CMSG_NEIGHBORHOOD_INVITE_NOTIFICATION_ACK, std::move(packet)) { }
        void Read() override;
        ObjectGuid NeighborhoodGuid;
    };

    class NeighborhoodOfferOwnershipResponsePacket final : public ClientPacket
    {
    public:
        explicit NeighborhoodOfferOwnershipResponsePacket(WorldPacket&& packet) : ClientPacket(CMSG_NEIGHBORHOOD_OFFER_OWNERSHIP_RESPONSE, std::move(packet)) { }
        void Read() override;
        ObjectGuid NeighborhoodGuid;
        bool Accept = false;
    };

    // ============================================================
    // Neighborhood Charter SMSG Responses (0x5Bxxxx)
    // ============================================================

    // IDA 0x5B0000: uint8(Result) + PackedGUID(CharterGuid) + uint32(MapID) + uint32(SigCount)
    //   + uint32(SignerArraySize) + uint32(Unknown) + PackedGUID[SignerArraySize] + uint8(nameLen) + string(Name)
    class NeighborhoodCharterUpdateResponse final : public ServerPacket
    {
    public:
        NeighborhoodCharterUpdateResponse() : ServerPacket(SMSG_NEIGHBORHOOD_CHARTER_UPDATE_RESPONSE) { }
        WorldPacket const* Write() override;
        uint8 Result = 0;
        ObjectGuid CharterGuid;
        uint32 MapID = 0;
        uint32 SignatureCount = 0;
        std::vector<ObjectGuid> Signers;
        uint32 Unknown = 0;
        std::string NeighborhoodName;
    };

    // IDA 0x5B0001: identical wire format to 0x5B0000
    class NeighborhoodCharterOpenUIResponse final : public ServerPacket
    {
    public:
        NeighborhoodCharterOpenUIResponse() : ServerPacket(SMSG_NEIGHBORHOOD_CHARTER_OPEN_UI_RESPONSE) { }
        WorldPacket const* Write() override;
        uint8 Result = 0;
        ObjectGuid CharterGuid;
        uint32 MapID = 0;
        uint32 SignatureCount = 0;
        std::vector<ObjectGuid> Signers;
        uint32 Unknown = 0;
        std::string NeighborhoodName;
    };

    // IDA 0x5B0002: uint8(Result) + PackedGUID(CharterGuid) + uint32(MapID) + uint32(Unknown) + uint8(nameLen) + string(Name)
    class NeighborhoodCharterSignRequest final : public ServerPacket
    {
    public:
        NeighborhoodCharterSignRequest() : ServerPacket(SMSG_NEIGHBORHOOD_CHARTER_SIGN_REQUEST) { }
        WorldPacket const* Write() override;
        uint8 Result = 0;
        ObjectGuid CharterGuid;
        uint32 MapID = 0;
        uint32 Unknown = 0;
        std::string NeighborhoodName;
    };

    // IDA 0x5B0003: uint8(Result) + PackedGUID(CharterGuid)
    class NeighborhoodCharterAddSignatureResponse final : public ServerPacket
    {
    public:
        NeighborhoodCharterAddSignatureResponse() : ServerPacket(SMSG_NEIGHBORHOOD_CHARTER_ADD_SIGNATURE_RESPONSE) { }
        WorldPacket const* Write() override;
        uint8 Result = 0;
        ObjectGuid CharterGuid;
    };

    // IDA 0x5B0004: uint8(Result) + uint32(field1) + uint32(field2) + uint8(nameLen) + string(Name)
    class NeighborhoodCharterOpenConfirmationUIResponse final : public ServerPacket
    {
    public:
        NeighborhoodCharterOpenConfirmationUIResponse() : ServerPacket(SMSG_NEIGHBORHOOD_CHARTER_OPEN_CONFIRMATION_UI_RESPONSE) { }
        WorldPacket const* Write() override;
        uint8 Result = 0;
        uint32 Field1 = 0;
        uint32 Field2 = 0;
        std::string NeighborhoodName;
    };

    // IDA 0x5B0005: PackedGUID(CharterGuid) only
    class NeighborhoodCharterSignatureRemovedNotification final : public ServerPacket
    {
    public:
        NeighborhoodCharterSignatureRemovedNotification() : ServerPacket(SMSG_NEIGHBORHOOD_CHARTER_SIGNATURE_REMOVED_NOTIFICATION) { }
        WorldPacket const* Write() override;
        ObjectGuid CharterGuid;
    };

    // ============================================================
    // Neighborhood Management SMSG Responses (0x5Cxxxx)
    // ============================================================

    class NeighborhoodPlayerEnterPlot final : public ServerPacket
    {
    public:
        NeighborhoodPlayerEnterPlot() : ServerPacket(SMSG_NEIGHBORHOOD_PLAYER_ENTER_PLOT) { }
        WorldPacket const* Write() override;
        ObjectGuid PlotAreaTriggerGuid;
    };

    class NeighborhoodPlayerLeavePlot final : public ServerPacket
    {
    public:
        NeighborhoodPlayerLeavePlot() : ServerPacket(SMSG_NEIGHBORHOOD_PLAYER_LEAVE_PLOT) { }
        WorldPacket const* Write() override;
    };

    class NeighborhoodEvictPlayerResponse final : public ServerPacket
    {
    public:
        NeighborhoodEvictPlayerResponse() : ServerPacket(SMSG_NEIGHBORHOOD_EVICT_PLAYER) { }
        WorldPacket const* Write() override;
        ObjectGuid PlayerGuid;
    };

    class NeighborhoodUpdateNameResponse final : public ServerPacket
    {
    public:
        NeighborhoodUpdateNameResponse() : ServerPacket(SMSG_NEIGHBORHOOD_UPDATE_NAME_RESPONSE) { }
        WorldPacket const* Write() override;
        uint8 Result = 0;  // IDA 12.0 verified (0x5C0003): single uint8
    };

    class NeighborhoodUpdateNameNotification final : public ServerPacket
    {
    public:
        NeighborhoodUpdateNameNotification() : ServerPacket(SMSG_NEIGHBORHOOD_UPDATE_NAME_NOTIFICATION) { }
        WorldPacket const* Write() override;
        // IDA 12.0 verified (0x5C0004): uint8(nameLen) + bytes[nameLen]
        std::string NewName;
    };

    class NeighborhoodAddSecondaryOwnerResponse final : public ServerPacket
    {
    public:
        NeighborhoodAddSecondaryOwnerResponse() : ServerPacket(SMSG_NEIGHBORHOOD_ADD_SECONDARY_OWNER_RESPONSE) { }
        WorldPacket const* Write() override;
        // IDA 12.0 verified (0x5C0006): PackedGUID + uint8 Result
        ObjectGuid PlayerGuid;
        uint8 Result = 0;
    };

    class NeighborhoodRemoveSecondaryOwnerResponse final : public ServerPacket
    {
    public:
        NeighborhoodRemoveSecondaryOwnerResponse() : ServerPacket(SMSG_NEIGHBORHOOD_REMOVE_SECONDARY_OWNER_RESPONSE) { }
        WorldPacket const* Write() override;
        // IDA 12.0 verified (0x5C0007): PackedGUID + uint8 Result
        ObjectGuid PlayerGuid;
        uint8 Result = 0;
    };

    class NeighborhoodBuyHouseResponse final : public ServerPacket
    {
    public:
        NeighborhoodBuyHouseResponse() : ServerPacket(SMSG_NEIGHBORHOOD_BUY_HOUSE_RESPONSE) { }
        WorldPacket const* Write() override;
        // Wire format: HouseInfo + uint8 Result
        Housing::HouseInfo House;
        uint8 Result = 0;
    };

    class NeighborhoodMoveHouseResponse final : public ServerPacket
    {
    public:
        NeighborhoodMoveHouseResponse() : ServerPacket(SMSG_NEIGHBORHOOD_MOVE_HOUSE_RESPONSE) { }
        WorldPacket const* Write() override;
        // Wire format: HouseInfo + PackedGUID + uint8 Result
        Housing::HouseInfo House;
        ObjectGuid MoveTransactionGuid;
        uint8 Result = 0;
    };

    class NeighborhoodOpenCornerstoneUIResponse final : public ServerPacket
    {
    public:
        NeighborhoodOpenCornerstoneUIResponse() : ServerPacket(SMSG_NEIGHBORHOOD_OPEN_CORNERSTONE_UI_RESPONSE) { }
        WorldPacket const* Write() override;

        // Wire format verified against retail 12.0.1 build 65940 packet captures
        // IDA deserializer sub_7FF6F6E3E200: uint32→+32, GUID→+40, GUID→+56, uint64→+72, uint8→+80, GUID→+128
        uint32 PlotIndex = 0;               // Echoed from CMSG (NOT a result code)
        ObjectGuid PlotOwnerGuid;           // →Buffer+40: Player GUID when owned, Empty when unclaimed
        ObjectGuid NeighborhoodGuid;        // →Buffer+56: Housing GUID when owned, Empty when unclaimed
        uint64 Cost = 0;                    // →Buffer+72: Purchase price (0 if owned or free)
        uint8 PurchaseStatus = 0;           // →Buffer+80: 73 (0x49) = purchasable, 0 = not. Client checks ==73
        ObjectGuid CornerstoneGuid;         // →Buffer+128: Cornerstone game object GUID
        bool IsPlotOwned = false;           // Whether this plot has an owner
        bool CanPurchase = false;           // Whether the player can purchase this plot
        bool HasResidents = false;          // Whether the plot has residents
        bool IsInitiative = false;          // Initiative-related flag
        Optional<uint64> AlternatePrice;    // Alternate/discounted price
        Optional<uint32> StatusValue;       // Additional status value
        std::string NeighborhoodName;       // NUL-terminated CString in wire format
    };

    class NeighborhoodInviteResidentResponse final : public ServerPacket
    {
    public:
        NeighborhoodInviteResidentResponse() : ServerPacket(SMSG_NEIGHBORHOOD_INVITE_RESIDENT_RESPONSE) { }
        WorldPacket const* Write() override;
        // IDA 12.0 verified (0x5C000B): uint8 Result + PackedGUID
        uint8 Result = 0;
        ObjectGuid InviteeGuid;
    };

    class NeighborhoodCancelInvitationResponse final : public ServerPacket
    {
    public:
        NeighborhoodCancelInvitationResponse() : ServerPacket(SMSG_NEIGHBORHOOD_CANCEL_INVITATION_RESPONSE) { }
        WorldPacket const* Write() override;
        // IDA 12.0 verified (0x5C000C): uint8 Result + PackedGUID
        uint8 Result = 0;
        ObjectGuid InviteeGuid;
    };

    class NeighborhoodDeclineInvitationResponse final : public ServerPacket
    {
    public:
        NeighborhoodDeclineInvitationResponse() : ServerPacket(SMSG_NEIGHBORHOOD_DECLINE_INVITATION_RESPONSE) { }
        WorldPacket const* Write() override;
        // IDA 12.0 verified (0x5C000D): uint8 Result + PackedGUID
        uint8 Result = 0;
        ObjectGuid NeighborhoodGuid;
    };

    class NeighborhoodPlayerGetInviteResponse final : public ServerPacket
    {
    public:
        NeighborhoodPlayerGetInviteResponse() : ServerPacket(SMSG_NEIGHBORHOOD_PLAYER_GET_INVITE_RESPONSE) { }
        WorldPacket const* Write() override;
        // IDA 12.0 verified (0x5C000E): uint8 Result + JamNeighborhoodRosterEntry(48 bytes)
        uint8 Result = 0;
        Housing::JamNeighborhoodRosterEntry Entry;
    };

    class NeighborhoodGetInvitesResponse final : public ServerPacket
    {
    public:
        NeighborhoodGetInvitesResponse() : ServerPacket(SMSG_NEIGHBORHOOD_GET_INVITES_RESPONSE) { }
        WorldPacket const* Write() override;
        // IDA 12.0 verified (0x5C000F): uint8 Result + uint32 Count + JamNeighborhoodRosterEntry[Count]
        uint8 Result = 0;
        std::vector<Housing::JamNeighborhoodRosterEntry> Invites;
    };

    class NeighborhoodInviteNotification final : public ServerPacket
    {
    public:
        NeighborhoodInviteNotification() : ServerPacket(SMSG_NEIGHBORHOOD_INVITE_NOTIFICATION) { }
        WorldPacket const* Write() override;
        // IDA 12.0 verified (0x5C0010): single PackedGUID
        ObjectGuid NeighborhoodGuid;
    };

    class NeighborhoodOfferOwnershipResponse final : public ServerPacket
    {
    public:
        NeighborhoodOfferOwnershipResponse() : ServerPacket(SMSG_NEIGHBORHOOD_OFFER_OWNERSHIP_RESPONSE) { }
        WorldPacket const* Write() override;
        // IDA 12.0 verified (0x5C0011): single uint8 Result
        uint8 Result = 0;
    };

    class NeighborhoodGetRosterResponse final : public ServerPacket
    {
    public:
        NeighborhoodGetRosterResponse() : ServerPacket(SMSG_NEIGHBORHOOD_GET_ROSTER_RESPONSE) { }
        WorldPacket const* Write() override;
        uint8 Result = 0;

        struct RosterMemberData
        {
            ObjectGuid HouseGuid;
            ObjectGuid PlayerGuid;
            ObjectGuid BnetAccountGuid;  // Usually empty
            uint8 PlotIndex = 0xFF;      // INVALID_PLOT_INDEX
            uint32 JoinTime = 0;
            uint8 ResidentType = 0;      // NeighborhoodMemberRole (0=Resident, 1=Manager, 2=Owner)
            bool IsOnline = false;       // Controls status byte 2 bit 7 in roster UI
        };
        std::vector<RosterMemberData> Members;

        // Group entry fields — the client deserializes these to populate
        // the HousingNeighborhoodState singleton that GetCornerstoneNeighborhoodInfo() reads.
        ObjectGuid GroupNeighborhoodGuid;   // Neighborhood GUID (stored at singleton offset 352)
        ObjectGuid GroupOwnerGuid;          // Neighborhood owner GUID (used to compute neighborhoodOwnerType)
        std::string NeighborhoodName;       // Displayed in Cornerstone UI (stored at singleton offset 296)
    };

    class NeighborhoodRosterResidentUpdate final : public ServerPacket
    {
    public:
        struct ResidentEntry
        {
            ObjectGuid PlayerGuid;
            uint8 UpdateType = 0;   // 0=Added, 1=RoleChanged, 2=Removed
            bool IsPrivileged = false; // IDA: client reads uint8, extracts bit7 only (>> 7), stores as bool — true for Manager/Owner
        };

        NeighborhoodRosterResidentUpdate() : ServerPacket(SMSG_NEIGHBORHOOD_ROSTER_RESIDENT_UPDATE) { }
        WorldPacket const* Write() override;
        std::vector<ResidentEntry> Residents;
    };

    class NeighborhoodInviteNameLookupResult final : public ServerPacket
    {
    public:
        NeighborhoodInviteNameLookupResult() : ServerPacket(SMSG_NEIGHBORHOOD_INVITE_NAME_LOOKUP_RESULT) { }
        WorldPacket const* Write() override;
        // IDA 12.0 verified (0x5C0014): uint8 Result + PackedGUID
        uint8 Result = 0;
        ObjectGuid PlayerGuid;
    };

    class NeighborhoodEvictPlotResponse final : public ServerPacket
    {
    public:
        NeighborhoodEvictPlotResponse() : ServerPacket(SMSG_NEIGHBORHOOD_EVICT_PLOT_RESPONSE) { }
        WorldPacket const* Write() override;
        // IDA 12.0 verified (0x5C0015): uint8 Result + PackedGUID
        uint8 Result = 0;
        ObjectGuid NeighborhoodGuid;
    };

    class NeighborhoodEvictPlotNotice final : public ServerPacket
    {
    public:
        NeighborhoodEvictPlotNotice() : ServerPacket(SMSG_NEIGHBORHOOD_EVICT_PLOT_NOTICE) { }
        WorldPacket const* Write() override;
        // IDA 12.0 verified (0x5C0016): uint32 + PackedGUID + PackedGUID
        uint32 PlotId = 0;
        ObjectGuid NeighborhoodGuid;
        ObjectGuid PlotGuid;
    };
    // --- Initiative System ---

    class NeighborhoodInitiativeServiceStatusCheck final : public ClientPacket
    {
    public:
        NeighborhoodInitiativeServiceStatusCheck(WorldPacket&& packet) : ClientPacket(CMSG_NEIGHBORHOOD_INITIATIVE_SERVICE_STATUS_CHECK, std::move(packet)) { }
        void Read() override { }
    };

    class GetAvailableInitiativeRequest final : public ClientPacket
    {
    public:
        GetAvailableInitiativeRequest(WorldPacket&& packet) : ClientPacket(CMSG_GET_AVAILABLE_INITIATIVE_REQUEST, std::move(packet)) { }
        void Read() override;
        ObjectGuid NeighborhoodGuid;
    };

    class GetInitiativeActivityLogRequest final : public ClientPacket
    {
    public:
        GetInitiativeActivityLogRequest(WorldPacket&& packet) : ClientPacket(CMSG_GET_INITIATIVE_ACTIVITY_LOG_REQUEST, std::move(packet)) { }
        void Read() override;
        ObjectGuid NeighborhoodGuid;
    };

    class InitiativeUpdateActiveNeighborhood final : public ClientPacket
    {
    public:
        InitiativeUpdateActiveNeighborhood(WorldPacket&& packet) : ClientPacket(CMSG_INITIATIVE_UPDATE_ACTIVE_NEIGHBORHOOD, std::move(packet)) { }
        void Read() override;
        ObjectGuid NeighborhoodGuid;
    };

    class InitiativeAcceptMilestoneRequest final : public ClientPacket
    {
    public:
        InitiativeAcceptMilestoneRequest(WorldPacket&& packet) : ClientPacket(CMSG_INITIATIVE_ACCEPT_MILESTONE_REQUEST, std::move(packet)) { }
        void Read() override;
        ObjectGuid NeighborhoodGuid;
        uint32 InitiativeID = 0;
        uint32 MilestoneIndex = 0;
    };

    class InitiativeReportProgress final : public ClientPacket
    {
    public:
        InitiativeReportProgress(WorldPacket&& packet) : ClientPacket(CMSG_INITIATIVE_REPORT_PROGRESS, std::move(packet)) { }
        void Read() override;
        ObjectGuid NeighborhoodGuid;
    };

    class GetInitiativeClaimRewardRequest final : public ClientPacket
    {
    public:
        GetInitiativeClaimRewardRequest(WorldPacket&& packet) : ClientPacket(CMSG_GET_INITIATIVE_CLAIM_REWARD_REQUEST, std::move(packet)) { }
        void Read() override;
        ObjectGuid NeighborhoodGuid;
        uint32 InitiativeID = 0;
        uint32 MilestoneIndex = 0;
    };

    class GetInitiativeLeaderboardRequest final : public ClientPacket
    {
    public:
        GetInitiativeLeaderboardRequest(WorldPacket&& packet) : ClientPacket(CMSG_GET_INITIATIVE_LEADERBOARD_REQUEST, std::move(packet)) { }
        void Read() override;
        ObjectGuid NeighborhoodGuid;
        uint32 InitiativeID = 0;
    };

    class GetInitiativeOpenChestRequest final : public ClientPacket
    {
    public:
        GetInitiativeOpenChestRequest(WorldPacket&& packet) : ClientPacket(CMSG_GET_INITIATIVE_OPEN_CHEST_REQUEST, std::move(packet)) { }
        void Read() override;
        ObjectGuid NeighborhoodGuid;
        uint32 InitiativeID = 0;
    };

    class GetInitiativeTaskAcceptRequest final : public ClientPacket
    {
    public:
        GetInitiativeTaskAcceptRequest(WorldPacket&& packet) : ClientPacket(CMSG_GET_INITIATIVE_TASK_ACCEPT_REQUEST, std::move(packet)) { }
        void Read() override;
        ObjectGuid NeighborhoodGuid;
        uint32 TaskID = 0;
    };

    class GetInitiativeTaskAbandonRequest final : public ClientPacket
    {
    public:
        GetInitiativeTaskAbandonRequest(WorldPacket&& packet) : ClientPacket(CMSG_GET_INITIATIVE_TASK_ABANDON_REQUEST, std::move(packet)) { }
        void Read() override;
        ObjectGuid NeighborhoodGuid;
        uint32 TaskID = 0;
    };

    class GetInitiativeTaskProgressRequest final : public ClientPacket
    {
    public:
        GetInitiativeTaskProgressRequest(WorldPacket&& packet) : ClientPacket(CMSG_GET_INITIATIVE_TASK_PROGRESS_REQUEST, std::move(packet)) { }
        void Read() override;
        ObjectGuid NeighborhoodGuid;
        uint32 TaskID = 0;
    };
}

#endif // TRINITYCORE_HOUSING_PACKETS_H
