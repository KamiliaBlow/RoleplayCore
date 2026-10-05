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
#include <array>
#include <string>
#include <vector>

namespace WorldPackets::Housing
{
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

    struct InviteEntry
    {
        uint64 Timestamp = 0;
        ObjectGuid PlayerGuid;
        ObjectGuid HouseGuid;
        uint64 ExtraData = 0;
    };

    struct RosterEntry
    {
        ObjectGuid PlayerGuid;
        uint8 ResidentType = 0;
        bool IsPrivileged = false;
    };

    struct JamCliHouse
    {
        ObjectGuid HouseGUID;
        ObjectGuid OwnerGUID;
        ObjectGuid NeighborhoodGUID;
        uint32 PlotIndex = 0;
        uint32 HouseSettingFlags = 0;
        bool HasOptionalField = false;
        uint64 OptionalValue = 0;        // ReservationTime, only when HasOptionalField
    };

    struct JamCliHouseFinderNeighborhood
    {
        ObjectGuid NeighborhoodGUID;
        ObjectGuid OwnerGUID;
        std::string Name;
        uint64 Field1 = 0;               // packed plot counts: high dword total, low dword available
        uint64 Field2 = 0;
        bool BoolFlag = false;
        std::vector<JamCliHouse> Houses;
        uint64 ExtraField = 0;
        uint8 ExtraFlags = 0;

        void SetPlotCounts(uint32 available, uint32 total)
        {
            Field1 = (static_cast<uint64>(total) << 32) | static_cast<uint64>(available);
        }
    };

    class HouseExteriorCommitPosition final : public ClientPacket
    {
    public:
        explicit HouseExteriorCommitPosition(WorldPacket&& packet) : ClientPacket(CMSG_HOUSE_EXTERIOR_SET_HOUSE_POSITION, std::move(packet)) { }

        void Read() override;

        ObjectGuid HouseGuid;
        ObjectGuid AccountGuid;
        float PositionX = 0.0f;      // relative to the plot room
        float PositionY = 0.0f;
        float PositionZ = 0.0f;
        float Facing = 0.0f;
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

    // No Enter/LeaveHouse SMSGs in 12.0.5: the client tracks house entry via PlayerHouseInfoComponentData.CurrentHouse.
    class HouseInteriorLeaveHouse final : public ClientPacket
    {
    public:
        explicit HouseInteriorLeaveHouse(WorldPacket&& packet) : ClientPacket(CMSG_HOUSE_INTERIOR_LEAVE_HOUSE, std::move(packet)) { }

        void Read() override { }
    };

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
        std::array<float, 4> Quaternion = { };
        float Scale = 1.0f;
        ObjectGuid AttachParentGuid;
        ObjectGuid RoomGuid;
        ObjectGuid AnchorMeshObjectGuid;
        uint32 AttachPoint = 0;
    };

    class HousingDecorMove final : public ClientPacket
    {
    public:
        explicit HousingDecorMove(WorldPacket&& packet) : ClientPacket(CMSG_HOUSING_DECOR_MOVE, std::move(packet)) { }

        void Read() override;

        ObjectGuid DecorGuid;
        TaggedPosition<Position::XYZ> Position;
        TaggedPosition<Position::XYZ> Rotation;
        std::array<float, 4> Quaternion = { };
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
        bool Field_49 = false;
    };

    // Empty PetGuid clears the binding.
    class HousingDecorSetPet final : public ClientPacket
    {
    public:
        explicit HousingDecorSetPet(WorldPacket&& packet) : ClientPacket(CMSG_HOUSING_DECOR_SET_PET, std::move(packet)) { }

        void Read() override;

        ObjectGuid DecorGuid;
        ObjectGuid PetGuid;
        uint8 Flag = 0;
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
        uint64 Timestamp = 0;
    };

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

        // Flags semantics unconfirmed.
        ObjectGuid FixtureGuid;
        uint32 ExteriorComponentID = 0;
        uint8 Flags = 0;
    };

    class HousingFixtureCreateFixture final : public ClientPacket
    {
    public:
        explicit HousingFixtureCreateFixture(WorldPacket&& packet) : ClientPacket(CMSG_HOUSING_FIXTURE_CREATE_FIXTURE, std::move(packet)) { }

        void Read() override;

        ObjectGuid AttachParentGuid;         // Housing/3 exterior root entity
        ObjectGuid HookEntityGuid;           // Housing/4 hook point entity on the house
        uint32 ExteriorComponentHookID = 0;  // DB2 ExteriorComponentHook row ID
        uint32 ExteriorComponentID = 0;      // DB2 ExteriorComponent row ID
        uint8 Flags = 0;
    };

    class HousingFixtureDeleteFixture final : public ClientPacket
    {
    public:
        explicit HousingFixtureDeleteFixture(WorldPacket&& packet) : ClientPacket(CMSG_HOUSING_FIXTURE_DELETE_FIXTURE, std::move(packet)) { }

        void Read() override;

        ObjectGuid FixtureGuid;
        ObjectGuid RoomGuid;
        uint32 ExteriorComponentID = 0;
        uint8 Flags = 0;
    };

    class HousingFixtureSetHouseSize final : public ClientPacket
    {
    public:
        explicit HousingFixtureSetHouseSize(WorldPacket&& packet) : ClientPacket(CMSG_HOUSING_FIXTURE_SET_HOUSE_SIZE, std::move(packet)) { }

        void Read() override;

        ObjectGuid HouseGuid;
        uint8 Size = 0;
        uint8 Flags = 0;
    };

    class HousingFixtureSetHouseType final : public ClientPacket
    {
    public:
        explicit HousingFixtureSetHouseType(WorldPacket&& packet) : ClientPacket(CMSG_HOUSING_FIXTURE_SET_HOUSE_TYPE, std::move(packet)) { }

        void Read() override;

        ObjectGuid HouseGuid;
        uint32 HouseExteriorWmoDataID = 0;
        uint8 Flags = 0;
    };

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

        // SourceRoomGuid is a Housing subType 2 room guid (the room whose door we connect to), not a house guid.
        ObjectGuid SourceRoomGuid;
        uint32 TargetDoorComponentID = 0;   // RoomComponent.ID of the door being connected to
        uint32 HouseRoomID = 0;             // HouseRoom.ID of the room template to add
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
        std::vector<uint32> OptionIDs; // RoomComponentOption IDs (not RoomComponent IDs)
    };

    class HousingRoomApplyComponentMaterials final : public ClientPacket
    {
    public:
        explicit HousingRoomApplyComponentMaterials(WorldPacket&& packet) : ClientPacket(CMSG_HOUSING_ROOM_APPLY_COMPONENT_MATERIALS, std::move(packet)) { }

        void Read() override;

        ObjectGuid RoomGuid;
        int32 ColorOverride = -1;
        uint32 RoomComponentTextureID = 0;
        uint8 ComponentSlot = 0;        // which wall/face the materials apply to
        std::vector<uint32> OptionIDs;
    };

    class HousingRoomSetDoorType final : public ClientPacket
    {
    public:
        explicit HousingRoomSetDoorType(WorldPacket&& packet) : ClientPacket(CMSG_HOUSING_ROOM_SET_DOOR_TYPE, std::move(packet)) { }

        void Read() override;

        ObjectGuid RoomGuid;
        uint32 ThemeOptionID = 0; // RoomComponentOption ID
        uint8 DoorType = 0;
    };

    class HousingRoomSetCeilingType final : public ClientPacket
    {
    public:
        explicit HousingRoomSetCeilingType(WorldPacket&& packet) : ClientPacket(CMSG_HOUSING_ROOM_SET_CEILING_TYPE, std::move(packet)) { }

        void Read() override;

        ObjectGuid RoomGuid;
        uint32 ThemeOptionID = 0; // RoomComponentOption ID
        uint8 CeilingType = 0;
    };

    class HousingSvcsGuildCreateNeighborhood final : public ClientPacket
    {
    public:
        explicit HousingSvcsGuildCreateNeighborhood(WorldPacket&& packet) : ClientPacket(CMSG_HOUSING_SVCS_GUILD_CREATE_NEIGHBORHOOD, std::move(packet)) { }

        void Read() override;

        uint32 NeighborhoodTypeID = 0;
        uint32 SecondaryID = 0;
        std::string NeighborhoodName;
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
        Optional<ObjectGuid> NewOwnerGuid; // C_Housing.SaveHouseSettings(playerGUID, ...): only sent when the owner changes
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

        // The house the settings screen is opened for; empty after it was relinquished.
        ObjectGuid HouseGuid;
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

    // Hide a neighborhood from this player's house-finder suggestions.
    class HousingSvcsHouseFinderIgnoreNeighborhood final : public ClientPacket
    {
    public:
        explicit HousingSvcsHouseFinderIgnoreNeighborhood(WorldPacket&& packet) : ClientPacket(CMSG_HOUSING_SVCS_HOUSE_FINDER_IGNORE_NEIGHBORHOOD, std::move(packet)) { }

        void Read() override;

        ObjectGuid NeighborhoodGuid;
    };

    // Ack for the ignore action; drives the client IGNORE_NEIGHBORHOOD_RESPONSE event.
    class HousingSvcsIgnoreNeighborhoodInviteResponse final : public ServerPacket
    {
    public:
        HousingSvcsIgnoreNeighborhoodInviteResponse() : ServerPacket(SMSG_HOUSING_SVCS_IGNORE_NEIGHBORHOOD_INVITE_RESPONSE) { }

        WorldPacket const* Write() override;

        bool Success = false;
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

    // Wipe the house's placed decor; HousingHouseScope: 0 None, 1 Interior, 2 Exterior.
    class HousingResetHouse final : public ClientPacket
    {
    public:
        explicit HousingResetHouse(WorldPacket&& packet) : ClientPacket(CMSG_HOUSING_RESET_HOUSE, std::move(packet)) { }

        void Read() override;

        uint8 ResetScope = 0;
    };

    class HousingHouseStatus final : public ClientPacket
    {
    public:
        explicit HousingHouseStatus(WorldPacket&& packet) : ClientPacket(CMSG_HOUSING_HOUSE_STATUS, std::move(packet)) { }

        void Read() override { }
    };

    class HousingGetPlayerPermissions final : public ClientPacket
    {
    public:
        explicit HousingGetPlayerPermissions(WorldPacket&& packet) : ClientPacket(CMSG_HOUSING_GET_PLAYER_PERMISSIONS, std::move(packet)) { }

        void Read() override;

        Optional<ObjectGuid> HouseGuid;
    };

    class HousingPhotoSharingCompleteAuthorization final : public ClientPacket
    {
    public:
        explicit HousingPhotoSharingCompleteAuthorization(WorldPacket&& packet) : ClientPacket(CMSG_HOUSING_PHOTO_SHARING_COMPLETE_AUTHORIZATION, std::move(packet)) { }

        void Read() override;

        std::string Token; // Bits<6> length, up to 40 chars; content unconfirmed (sniffs show it empty)
    };

    class HousingPhotoSharingClearAuthorization final : public ClientPacket
    {
    public:
        explicit HousingPhotoSharingClearAuthorization(WorldPacket&& packet) : ClientPacket(CMSG_HOUSING_PHOTO_SHARING_CLEAR_AUTHORIZATION, std::move(packet)) { }

        void Read() override { }
    };

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

    // Bulk-refund placed decor within the refund window.
    class BulkRefund final : public ClientPacket
    {
    public:
        explicit BulkRefund(WorldPacket&& packet) : ClientPacket(CMSG_BULK_REFUND, std::move(packet)) { }

        void Read() override;

        std::vector<ObjectGuid> DecorGUIDs;
    };

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

        std::string PlayerName; // 6-bit-length-prefixed name, no GUIDs (invite by name)
    };

    class GuildGetOthersOwnedHouses final : public ClientPacket
    {
    public:
        explicit GuildGetOthersOwnedHouses(WorldPacket&& packet) : ClientPacket(CMSG_GUILD_GET_OTHERS_OWNED_HOUSES, std::move(packet)) { }

        void Read() override;

        ObjectGuid PlayerGuid;
    };

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

    class HouseExteriorLockResponse final : public ServerPacket
    {
    public:
        HouseExteriorLockResponse() : ServerPacket(SMSG_HOUSE_EXTERIOR_LOCK_RESPONSE) { }
        WorldPacket const* Write() override;
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

    class HousingDecorSetEditModeResponse final : public ServerPacket
    {
    public:
        HousingDecorSetEditModeResponse() : ServerPacket(SMSG_HOUSING_DECOR_SET_EDIT_MODE_RESPONSE) { }
        WorldPacket const* Write() override;

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

        // Decor data itself arrives via the FHousingStorage_C entity fragment; this is only an ack.
        ObjectGuid BNetAccountGuid;
        uint8 ResultCode = 0;
        uint8 Flags = 0x80;  // always 0x80
    };

    class HousingDecorAddToHouseChestResponse final : public ServerPacket
    {
    public:
        HousingDecorAddToHouseChestResponse() : ServerPacket(SMSG_HOUSING_DECOR_ADD_TO_HOUSE_CHEST_RESPONSE) { }
        WorldPacket const* Write() override;
        bool Success = false;
        std::vector<ObjectGuid> DecorGuids;
    };

    class HousingDecorSystemSetDyeSlotsResponse final : public ServerPacket
    {
    public:
        HousingDecorSystemSetDyeSlotsResponse() : ServerPacket(SMSG_HOUSING_DECOR_SYSTEM_SET_DYE_SLOTS_RESPONSE) { }
        WorldPacket const* Write() override;
        ObjectGuid DecorGuid;
        uint8 Result = 0;
    };

    class HousingRedeemDeferredDecorResponse final : public ServerPacket
    {
    public:
        HousingRedeemDeferredDecorResponse() : ServerPacket(SMSG_HOUSING_REDEEM_DEFERRED_DECOR_RESPONSE) { }
        WorldPacket const* Write() override;
        ObjectGuid DecorGuid;       // the new decor item's GUID (used for placement)
        uint8 Result = 0;           // 0 = success
        uint32 SequenceIndex = 0;   // echoes CMSG RedemptionToken
    };

    class HousingFirstTimeDecorAcquisition final : public ServerPacket
    {
    public:
        HousingFirstTimeDecorAcquisition() : ServerPacket(SMSG_HOUSING_FIRST_TIME_DECOR_ACQUISITION) { }
        WorldPacket const* Write() override;
        uint32 DecorEntryID = 0;
    };

    class HousingFixtureSetEditModeResponse final : public ServerPacket
    {
    public:
        HousingFixtureSetEditModeResponse() : ServerPacket(SMSG_HOUSING_FIXTURE_SET_EDIT_MODE_RESPONSE) { }
        WorldPacket const* Write() override;
        // Client compares EditorPlayerGuid against its stored reference to enter/exit edit mode.
        ObjectGuid HouseGuid;           // always empty in sniffs
        ObjectGuid EditorPlayerGuid;    // player GUID on enter, empty on exit
        uint8 Result = 0;
    };

    class HousingFixtureCreateBasicHouseResponse final : public ServerPacket
    {
    public:
        HousingFixtureCreateBasicHouseResponse() : ServerPacket(SMSG_HOUSING_FIXTURE_CREATE_BASIC_HOUSE_RESPONSE) { }
        WorldPacket const* Write() override;
        uint8 Result = 0; // client ignores any trailing data
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
        uint8 Result = 0;
        uint32 HouseExteriorTypeID = 0;
        uint8 ExtraField = 0;
    };

    class HousingFixtureSetCoreFixtureResponse final : public ServerPacket
    {
    public:
        HousingFixtureSetCoreFixtureResponse() : ServerPacket(SMSG_HOUSING_FIXTURE_SET_CORE_FIXTURE_RESPONSE) { }
        WorldPacket const* Write() override;
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

    class HousingRoomSetLayoutEditModeResponse final : public ServerPacket
    {
    public:
        HousingRoomSetLayoutEditModeResponse() : ServerPacket(SMSG_HOUSING_ROOM_SET_LAYOUT_EDIT_MODE_RESPONSE) { }
        WorldPacket const* Write() override;
        ObjectGuid PlayerGuid;
        uint8 Result = 0;
        bool Active = false;
    };

    class HousingRoomAddResponse final : public ServerPacket
    {
    public:
        HousingRoomAddResponse() : ServerPacket(SMSG_HOUSING_ROOM_ADD_RESPONSE) { }
        WorldPacket const* Write() override;
        ObjectGuid RoomGuid;
        // PlayerGuid is edit context, not the new room's GUID.
        ObjectGuid PlayerGuid;
        uint8 Result = 0;
    };

    class HousingRoomRemoveResponse final : public ServerPacket
    {
    public:
        HousingRoomRemoveResponse() : ServerPacket(SMSG_HOUSING_ROOM_REMOVE_RESPONSE) { }
        WorldPacket const* Write() override;
        ObjectGuid RoomGuid;
        ObjectGuid PlayerGuid;
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
        ObjectGuid RoomGuid;
        uint32 ThemeSetID = 0;
        uint8 Result = 0;
        std::vector<uint32> OptionIDs;
    };

    class HousingRoomApplyComponentMaterialsResponse final : public ServerPacket
    {
    public:
        HousingRoomApplyComponentMaterialsResponse() : ServerPacket(SMSG_HOUSING_ROOM_APPLY_COMPONENT_MATERIALS_RESPONSE) { }
        WorldPacket const* Write() override;
        // ColorOverride is NOT echoed in the response — only TextureID.
        ObjectGuid RoomGuid;
        uint32 RoomComponentTextureID = 0;
        uint8 Result = 0;
        std::vector<uint32> OptionIDs;
    };

    class HousingRoomSetDoorTypeResponse final : public ServerPacket
    {
    public:
        HousingRoomSetDoorTypeResponse() : ServerPacket(SMSG_HOUSING_ROOM_SET_DOOR_TYPE_RESPONSE) { }
        WorldPacket const* Write() override;
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
        ObjectGuid RoomGuid;
        uint32 ComponentID = 0;
        uint8 CeilingType = 0;
        uint8 Result = 0;
    };

    class HousingSvcsNotifyPermissionsFailure final : public ServerPacket
    {
    public:
        HousingSvcsNotifyPermissionsFailure() : ServerPacket(SMSG_HOUSING_SVCS_NOTIFY_PERMISSIONS_FAILURE) { }
        WorldPacket const* Write() override;
        uint8 FailureType = 0;  // two separate uint8 reads, not one uint16
        uint8 ErrorCode = 0;
    };

    class HousingSvcsGuildCreateNeighborhoodNotification final : public ServerPacket
    {
    public:
        HousingSvcsGuildCreateNeighborhoodNotification() : ServerPacket(SMSG_HOUSING_SVCS_GUILD_CREATE_NEIGHBORHOOD_NOTIFICATION) { }
        WorldPacket const* Write() override;
        ObjectGuid NeighborhoodGuid;
        uint8 Flag = 0;
        std::string Name;
    };

    class HousingSvcsCreateCharterNeighborhoodResponse final : public ServerPacket
    {
    public:
        HousingSvcsCreateCharterNeighborhoodResponse() : ServerPacket(SMSG_HOUSING_SVCS_CREATE_CHARTER_NEIGHBORHOOD_RESPONSE) { }
        WorldPacket const* Write() override;
        JamCliHouseFinderNeighborhood Neighborhood;
        uint8 TrailingResult = 0;
    };

    class HousingSvcsNeighborhoodReservePlotResponse final : public ServerPacket
    {
    public:
        HousingSvcsNeighborhoodReservePlotResponse() : ServerPacket(SMSG_HOUSING_SVCS_NEIGHBORHOOD_RESERVE_PLOT_RESPONSE) { }
        WorldPacket const* Write() override;
        uint8 Result = 0;
    };

    class HousingSvcsRelinquishHouseResponse final : public ServerPacket
    {
    public:
        HousingSvcsRelinquishHouseResponse() : ServerPacket(SMSG_HOUSING_SVCS_RELINQUISH_HOUSE_RESPONSE) { }
        WorldPacket const* Write() override;
        uint8 Result = 0;
        ObjectGuid HouseGuid;
        ObjectGuid NeighborhoodGuid;
    };

    class HousingSvcsCancelRelinquishHouseResponse final : public ServerPacket
    {
    public:
        HousingSvcsCancelRelinquishHouseResponse() : ServerPacket(SMSG_HOUSING_SVCS_CANCEL_RELINQUISH_HOUSE_RESPONSE) { }
        WorldPacket const* Write() override;
        uint32 Field1 = 0;
        ObjectGuid HouseGuid;
        uint8 Result = 0;
    };

    // Unused scaffolding for the unimplemented neighborhood search response.
    struct JamHousingSearchResult
    {
        ObjectGuid Guid;
        uint64 OccupiedPlots = 0;
        uint64 ReservationMask = 0;
        uint32 Flags = 0;
        ObjectGuid OwnerGUID;
        std::string NeighborhoodName;
    };

    class HousingSvcsGetPlayerHousesInfoResponse final : public ServerPacket
    {
    public:
        HousingSvcsGetPlayerHousesInfoResponse() : ServerPacket(SMSG_HOUSING_SVCS_GET_PLAYER_HOUSES_INFO_RESPONSE) { }
        WorldPacket const* Write() override;

        std::vector<JamCliHouse> Houses;
        uint8 Result = 0;
    };

    class HousingSvcsPlayerViewHousesResponse final : public ServerPacket
    {
    public:
        HousingSvcsPlayerViewHousesResponse() : ServerPacket(SMSG_HOUSING_SVCS_PLAYER_VIEW_HOUSES_RESPONSE) { }
        WorldPacket const* Write() override;

        std::vector<JamCliHouse> Houses;
        uint8 Result = 0;
    };

    class HousingSvcsChangeHouseCosmeticOwner final : public ServerPacket
    {
    public:
        HousingSvcsChangeHouseCosmeticOwner() : ServerPacket(SMSG_HOUSING_SVCS_CHANGE_HOUSE_COSMETIC_OWNER) { }
        WorldPacket const* Write() override;
        uint8 Result = 0;
        ObjectGuid HouseGuid;
        ObjectGuid NewOwnerGuid;
    };

    class HousingSvcsUpdateHousesLevelFavor final : public ServerPacket
    {
    public:
        HousingSvcsUpdateHousesLevelFavor() : ServerPacket(SMSG_HOUSING_SVCS_UPDATE_HOUSES_LEVEL_FAVOR) { }
        WorldPacket const* Write() override;

        uint8 Result = 0;
        uint32 ChangeAmount = 0;
        uint32 Reason = 0;

        struct HouseLevelFavor
        {
            ObjectGuid BnetAccount;
            ObjectGuid NeighborhoodGUID;
            ObjectGuid HouseGUID;
            int32 HouseLevel = 0;         // low dword of the int64
            int32 FavorValue = 0;         // high dword of the int64
            uint8 UpdateSource = 0;
            uint32 SourceDataDecorID = 0;
            bool IsAdditive = true;       // wire bit 7 of the trailing byte
        };
        std::vector<HouseLevelFavor> Houses;
    };

    class HousingSvcsGuildAddHouseNotification final : public ServerPacket
    {
    public:
        HousingSvcsGuildAddHouseNotification() : ServerPacket(SMSG_HOUSING_SVCS_GUILD_ADD_HOUSE_NOTIFICATION) { }
        WorldPacket const* Write() override;
        JamCliHouse House;
    };

    class HousingSvcsGuildRemoveHouseNotification final : public ServerPacket
    {
    public:
        HousingSvcsGuildRemoveHouseNotification() : ServerPacket(SMSG_HOUSING_SVCS_GUILD_REMOVE_HOUSE_NOTIFICATION) { }
        WorldPacket const* Write() override;
        JamCliHouse House;
    };

    class HousingSvcsGuildRenameNeighborhoodNotification final : public ServerPacket
    {
    public:
        HousingSvcsGuildRenameNeighborhoodNotification() : ServerPacket(SMSG_HOUSING_SVCS_GUILD_RENAME_NEIGHBORHOOD_NOTIFICATION) { }
        WorldPacket const* Write() override;
        std::string NewName; // no GUID on the wire
    };

    class HousingSvcsGuildGetHousingInfoResponse final : public ServerPacket
    {
    public:
        HousingSvcsGuildGetHousingInfoResponse() : ServerPacket(SMSG_HOUSING_SVCS_GUILD_GET_HOUSING_INFO_RESPONSE) { }
        WorldPacket const* Write() override;
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
        // First byte = blobSize << 2 | Result; the blob is 3 raw (unpacked) GUIDs + 1 byte, only on Result == 0.
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

        // Per entry the name length is split across two bytes; the name has no NUL terminator.
        struct PotentialOwnerData
        {
            ObjectGuid PlayerGuid;
            uint32 ClassID = 0;
            uint8 Error = 0;            // HousingResult code, low byte
            std::string CharacterName;
        };
        std::vector<PotentialOwnerData> PotentialOwners;
    };

    class HousingSvcsUpdateHouseSettingsResponse final : public ServerPacket
    {
    public:
        HousingSvcsUpdateHouseSettingsResponse() : ServerPacket(SMSG_HOUSING_SVCS_UPDATE_HOUSE_SETTINGS_RESPONSE) { }
        WorldPacket const* Write() override;

        uint8 Result = 0;
        JamCliHouse House;
        uint32 SettingsFlags = 0;
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

    class HousingHouseStatusResponse final : public ServerPacket
    {
    public:
        HousingHouseStatusResponse() : ServerPacket(SMSG_HOUSING_HOUSE_STATUS_RESPONSE) { }
        WorldPacket const* Write() override;

        ObjectGuid HouseGuid;
        ObjectGuid AccountGuid;
        ObjectGuid OwnerPlayerGuid;
        ObjectGuid LockedDecorGuid;
        uint8 Status = 0;
        uint8 EditModeFlags = 0;    // 0x80 decor edit, 0x40 layout edit, 0x20 fixture edit
    };

    class HousingGetCurrentHouseInfoResponse final : public ServerPacket
    {
    public:
        HousingGetCurrentHouseInfoResponse() : ServerPacket(SMSG_HOUSING_GET_CURRENT_HOUSE_INFO_RESPONSE) { }
        WorldPacket const* Write() override;

        JamCliHouse House;
        uint8 Result = 0;
    };

    class HousingGetPlayerPermissionsResponse final : public ServerPacket
    {
    public:
        HousingGetPlayerPermissionsResponse() : ServerPacket(SMSG_HOUSING_GET_PLAYER_PERMISSIONS_RESPONSE) { }
        WorldPacket const* Write() override;

        ObjectGuid HouseGuid;
        uint8 ResultCode = 0;
        uint8 PermissionFlags = 0;   // bit 7 houseEditingPermitted, bit 6 plotEntryPermitted, bit 5 houseEntryPermitted
    };

    class HousingResetKioskModeResponse final : public ServerPacket
    {
    public:
        HousingResetKioskModeResponse() : ServerPacket(SMSG_HOUSING_RESET_KIOSK_MODE_RESPONSE) { }
        WorldPacket const* Write() override;
        uint8 Result = 0;
    };

    // Drives HOUSE_RESET_COMPLETED (Result == 0) / HOUSE_RESET_FAILED client events.
    class HousingResetHouseResponse final : public ServerPacket
    {
    public:
        HousingResetHouseResponse() : ServerPacket(SMSG_HOUSING_RESET_HOUSE_RESPONSE) { }
        WorldPacket const* Write() override;
        uint32 Result = 0;   // HousingResult (0 = success)
    };

    // Shared format for the five SMSG_ACCOUNT_*_COLLECTION_UPDATE opcodes; AddSingle() covers the one-item delta.
    class AccountCollectionUpdateBase : public ServerPacket
    {
    public:
        AccountCollectionUpdateBase(OpcodeServer opcode) : ServerPacket(opcode) { }

        bool IsIncrementalUpdate = true; // top bit of the leading byte
        std::vector<uint32> IDs;
        std::vector<bool> StateFlags;

        void AddSingle(uint32 id, bool state = true)
        {
            IDs.push_back(id);
            StateFlags.push_back(state);
        }

    protected:
        void WriteCollection();
    };

    class AccountExteriorFixtureCollectionUpdate final : public AccountCollectionUpdateBase
    {
    public:
        AccountExteriorFixtureCollectionUpdate() : AccountCollectionUpdateBase(SMSG_ACCOUNT_EXTERIOR_FIXTURE_COLLECTION_UPDATE) { }
        WorldPacket const* Write() override;
    };

    class AccountHouseTypeCollectionUpdate final : public AccountCollectionUpdateBase
    {
    public:
        AccountHouseTypeCollectionUpdate() : AccountCollectionUpdateBase(SMSG_ACCOUNT_HOUSE_TYPE_COLLECTION_UPDATE) { }
        WorldPacket const* Write() override;
    };

    class AccountRoomCollectionUpdate final : public AccountCollectionUpdateBase
    {
    public:
        AccountRoomCollectionUpdate() : AccountCollectionUpdateBase(SMSG_ACCOUNT_ROOM_COLLECTION_UPDATE) { }
        WorldPacket const* Write() override;
    };

    class AccountRoomThemeCollectionUpdate final : public AccountCollectionUpdateBase
    {
    public:
        AccountRoomThemeCollectionUpdate() : AccountCollectionUpdateBase(SMSG_ACCOUNT_ROOM_THEME_COLLECTION_UPDATE) { }
        WorldPacket const* Write() override;
    };

    class AccountRoomMaterialCollectionUpdate final : public AccountCollectionUpdateBase
    {
    public:
        AccountRoomMaterialCollectionUpdate() : AccountCollectionUpdateBase(SMSG_ACCOUNT_ROOM_MATERIAL_COLLECTION_UPDATE) { }
        WorldPacket const* Write() override;
    };

    class InvalidateNeighborhood final : public ServerPacket
    {
    public:
        InvalidateNeighborhood() : ServerPacket(SMSG_INVALIDATE_NEIGHBORHOOD) { }
        WorldPacket const* Write() override;
        ObjectGuid NeighborhoodGuid;
    };

    struct JamClientRefundableDecor
    {
        uint32 DecorID = 0;
        uint64 RefundPrice = 0;
        uint64 ExpiryTime = 0;
        uint32 Flags = 0;
    };

    struct JamLicensedDecorQuantity
    {
        uint32 HouseDecorID = 0;
        uint32 PlacedQuantity = 0;
        uint32 StoredQuantity = 0;
    };

    struct JamPlayerInitiativeTaskInfo
    {
        uint32 TaskID = 0;
        uint32 Progress = 0;
    };

    // Field semantics unconfirmed.
    struct NICompletedTasksEntry
    {
        ObjectGuid PlayerGuid;
        ObjectGuid TargetGuid;
        uint32 ContributionAmount = 0;
        uint64 CompletionTime = 0;
        uint32 TaskID = 0;
    };

    class GetDecorRefundListResponse final : public ServerPacket
    {
    public:
        GetDecorRefundListResponse() : ServerPacket(SMSG_GET_DECOR_REFUND_LIST_RESPONSE) { }
        WorldPacket const* Write() override;
        std::vector<JamClientRefundableDecor> Decors;
    };

    class BulkRefundResponse final : public ServerPacket
    {
    public:
        BulkRefundResponse() : ServerPacket(SMSG_BULK_REFUND_RESPONSE) { }
        WorldPacket const* Write() override;
        uint32 Result = 0; // BulkRefundResult enum
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

        // Only (Flags >> 6) == 1 means the data block below follows; any other value ends the packet.
        ObjectGuid NeighborhoodGUID;
        uint8 Flags = 0;

        // InitiativeInfo block, only written when (Flags >> 6) == 1.
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

        // No leading Result byte — failures go via SMSG_HOUSING_SVCS_NOTIFY_PERMISSIONS_FAILURE.
        ObjectGuid NeighborhoodGuid;
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
        std::vector<uint64> CriteriaIDs;
    };

    class GetInitiativeRewardsResult final : public ServerPacket
    {
    public:
        GetInitiativeRewardsResult() : ServerPacket(SMSG_GET_INITIATIVE_REWARDS_RESULT) { }
        WorldPacket const* Write() override;
        uint32 Result = 0; // field semantics unconfirmed
        ObjectGuid SourceGuid;
        ObjectGuid TargetGuid;
    };

    class InitiativeRewardAvailable final : public ServerPacket
    {
    public:
        InitiativeRewardAvailable() : ServerPacket(SMSG_INITIATIVE_REWARD_AVAILABLE) { }
        WorldPacket const* Write() override;
        // InitiativeID and MilestoneIndex are caller-side only, not serialized.
        uint32 InitiativeID = 0;
        uint32 MilestoneIndex = 0;
        std::vector<ObjectGuid> RewardGuids;
    };

    class HousingPhotoSharingAuthorizationResult final : public ServerPacket
    {
    public:
        HousingPhotoSharingAuthorizationResult() : ServerPacket(SMSG_HOUSING_PHOTO_SHARING_AUTHORIZATION_RESULT) { }
        WorldPacket const* Write() override;

        // Second byte = Length << 1 (max 0x7F); the name is not NUL-terminated.
        uint8 Result = 0;
        std::string PartnerName;
    };

    class HousingPhotoSharingAuthorizationClearedResult final : public ServerPacket
    {
    public:
        HousingPhotoSharingAuthorizationClearedResult() : ServerPacket(SMSG_HOUSING_PHOTO_SHARING_AUTHORIZATION_CLEARED_RESULT) { }
        WorldPacket const* Write() override;
        uint8 Result = 0;
    };

    // Not a housing packet despite the name: this is the crafting-order clerk's greeting (AuctionHello twin).
    class CraftingHouseHelloResponse final : public ServerPacket
    {
    public:
        CraftingHouseHelloResponse() : ServerPacket(SMSG_CRAFTING_HOUSE_HELLO_RESPONSE) { }
        WorldPacket const* Write() override;

        ObjectGuid Guid;
        bool Field0 = false;            // bit 0x80 — unrecovered
        bool OpenForBusiness = false;   // bit 0x40 — false raises CRAFTING_HOUSE_DISABLED instead
    };

    class GuildOthersOwnedHousesResult final : public ServerPacket
    {
    public:
        GuildOthersOwnedHousesResult() : ServerPacket(SMSG_GUILD_OTHERS_OWNED_HOUSES_RESULT) { }
        WorldPacket const* Write() override;

        uint8 Result = 0;
        ObjectGuid GuildGuid;
        std::vector<JamCliHouse> Houses;
    };

    class HousingSvcsNeighborhoodUpdateNameNotification final : public ServerPacket
    {
    public:
        HousingSvcsNeighborhoodUpdateNameNotification()
            : ServerPacket(SMSG_HOUSING_SVCS_NEIGHBORHOOD_UPDATE_NAME_NOTIFICATION) { }
        WorldPacket const* Write() override;
        ObjectGuid NeighborhoodGuid;
        std::string NewName;
    };
}

namespace WorldPackets::Neighborhood
{
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

        ObjectGuid CornerstoneGuid;
        ObjectGuid HouseGuid;
    };

    class NeighborhoodMoveHouse final : public ClientPacket
    {
    public:
        explicit NeighborhoodMoveHouse(WorldPacket&& packet) : ClientPacket(CMSG_NEIGHBORHOOD_MOVE_HOUSE, std::move(packet)) { }

        void Read() override;

        // CornerstoneGuid must be a GameObject guid (the client's TryMoveHouse checks its high part).
        ObjectGuid CornerstoneGuid;
        ObjectGuid HouseGuid;
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

    // Wire identical to NeighborhoodCharterUpdateResponse.
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

    class NeighborhoodCharterAddSignatureResponse final : public ServerPacket
    {
    public:
        NeighborhoodCharterAddSignatureResponse() : ServerPacket(SMSG_NEIGHBORHOOD_CHARTER_ADD_SIGNATURE_RESPONSE) { }
        WorldPacket const* Write() override;
        uint8 Result = 0;
        ObjectGuid CharterGuid;
    };

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

    class NeighborhoodCharterSignatureRemovedNotification final : public ServerPacket
    {
    public:
        NeighborhoodCharterSignatureRemovedNotification() : ServerPacket(SMSG_NEIGHBORHOOD_CHARTER_SIGNATURE_REMOVED_NOTIFICATION) { }
        WorldPacket const* Write() override;
        ObjectGuid CharterGuid;
    };

    // No Enter/LeavePlot SMSGs in 12.0.5: plot occupancy rides on PlayerHouseInfoComponentData.CurrentHouse.

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
        uint8 Result = 0;  // single uint8
    };

    class NeighborhoodAddSecondaryOwnerResponse final : public ServerPacket
    {
    public:
        NeighborhoodAddSecondaryOwnerResponse() : ServerPacket(SMSG_NEIGHBORHOOD_ADD_SECONDARY_OWNER_RESPONSE) { }
        WorldPacket const* Write() override;
        ObjectGuid PlayerGuid;
        uint8 Result = 0;
    };

    class NeighborhoodRemoveSecondaryOwnerResponse final : public ServerPacket
    {
    public:
        NeighborhoodRemoveSecondaryOwnerResponse() : ServerPacket(SMSG_NEIGHBORHOOD_REMOVE_SECONDARY_OWNER_RESPONSE) { }
        WorldPacket const* Write() override;
        ObjectGuid PlayerGuid;
        uint8 Result = 0;
    };

    class NeighborhoodBuyHouseResponse final : public ServerPacket
    {
    public:
        NeighborhoodBuyHouseResponse() : ServerPacket(SMSG_NEIGHBORHOOD_BUY_HOUSE_RESPONSE) { }
        WorldPacket const* Write() override;
        Housing::JamCliHouse House;
        uint8 Result = 0;
    };

    class NeighborhoodMoveHouseResponse final : public ServerPacket
    {
    public:
        NeighborhoodMoveHouseResponse() : ServerPacket(SMSG_NEIGHBORHOOD_MOVE_HOUSE_RESPONSE) { }
        WorldPacket const* Write() override;
        Housing::JamCliHouse House;
        ObjectGuid MoveTransactionGuid;
        uint8 Result = 0;
    };

    class NeighborhoodOpenCornerstoneUIResponse final : public ServerPacket
    {
    public:
        NeighborhoodOpenCornerstoneUIResponse() : ServerPacket(SMSG_NEIGHBORHOOD_OPEN_CORNERSTONE_UI_RESPONSE) { }
        WorldPacket const* Write() override;

        uint32 PlotIndex = 0;               // echoed from the CMSG, not a result code
        ObjectGuid PlotOwnerGuid;           // player GUID when owned, empty when unclaimed
        ObjectGuid NeighborhoodGuid;        // housing GUID when owned, empty when unclaimed
        uint64 Cost = 0;                    // purchase price (0 if owned or free)
        uint8 PurchaseStatus = 0;           // 73 (0x49) = purchasable, 0 = not; client checks == 73
        ObjectGuid CornerstoneGuid;         // cornerstone game object GUID
        bool IsPlotOwned = false;
        bool CanPurchase = false;
        bool HasResidents = false;
        bool IsInitiative = false;
        Optional<uint64> AlternatePrice;    // alternate/discounted price
        Optional<uint32> StatusValue;       // additional status value
        std::string NeighborhoodName;       // NUL-terminated CString on the wire

        // Embedded house info for the player's CURRENT house; when set the client offers "Move" instead of "Buy".
        Optional<Housing::JamCliHouse> ExistingHouse;
    };

    class NeighborhoodInviteResidentResponse final : public ServerPacket
    {
    public:
        NeighborhoodInviteResidentResponse() : ServerPacket(SMSG_NEIGHBORHOOD_INVITE_RESIDENT_RESPONSE) { }
        WorldPacket const* Write() override;
        uint8 Result = 0;
        ObjectGuid InviteeGuid;
    };

    class NeighborhoodCancelInvitationResponse final : public ServerPacket
    {
    public:
        NeighborhoodCancelInvitationResponse() : ServerPacket(SMSG_NEIGHBORHOOD_CANCEL_INVITATION_RESPONSE) { }
        WorldPacket const* Write() override;
        uint8 Result = 0;
        ObjectGuid InviteeGuid;
    };

    class NeighborhoodDeclineInvitationResponse final : public ServerPacket
    {
    public:
        NeighborhoodDeclineInvitationResponse() : ServerPacket(SMSG_NEIGHBORHOOD_DECLINE_INVITATION_RESPONSE) { }
        WorldPacket const* Write() override;
        uint8 Result = 0;
        ObjectGuid NeighborhoodGuid;
    };

    class NeighborhoodPlayerGetInviteResponse final : public ServerPacket
    {
    public:
        NeighborhoodPlayerGetInviteResponse() : ServerPacket(SMSG_NEIGHBORHOOD_PLAYER_GET_INVITE_RESPONSE) { }
        WorldPacket const* Write() override;
        uint8 Result = 0;
        Housing::InviteEntry Entry;
    };

    class NeighborhoodGetInvitesResponse final : public ServerPacket
    {
    public:
        NeighborhoodGetInvitesResponse() : ServerPacket(SMSG_NEIGHBORHOOD_GET_INVITES_RESPONSE) { }
        WorldPacket const* Write() override;
        uint8 Result = 0;
        std::vector<Housing::InviteEntry> Invites;
    };

    class NeighborhoodInviteNotification final : public ServerPacket
    {
    public:
        NeighborhoodInviteNotification() : ServerPacket(SMSG_NEIGHBORHOOD_INVITE_NOTIFICATION) { }
        WorldPacket const* Write() override;
        ObjectGuid NeighborhoodGuid;
    };

    class NeighborhoodOfferOwnershipResponse final : public ServerPacket
    {
    public:
        NeighborhoodOfferOwnershipResponse() : ServerPacket(SMSG_NEIGHBORHOOD_OFFER_OWNERSHIP_RESPONSE) { }
        WorldPacket const* Write() override;
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
            ObjectGuid BnetAccountGuid;  // usually empty
            uint8 PlotIndex = 0xFF;      // INVALID_PLOT_INDEX
            uint32 JoinTime = 0;
            uint8 HouseLevel = 0;
            uint32 HouseSettingFlags = 0;
            uint8 ResidentType = 0;      // Enum.ResidentType: 0 = Resident, 1 = Manager, 2 = Owner
            bool IsOnline = false;
        };
        std::vector<RosterMemberData> Members;

        // Group entry fields: the client uses these to fill its HousingNeighborhoodState singleton.
        ObjectGuid GroupNeighborhoodGuid;
        ObjectGuid GroupOwnerGuid;          // used to compute neighborhoodOwnerType
        std::string NeighborhoodName;       // displayed in the cornerstone UI
    };

    class NeighborhoodRosterResidentUpdate final : public ServerPacket
    {
    public:
        // The client updates already-listed members matched by guid; joins and departures need a full roster.
        struct ResidentEntry
        {
            ObjectGuid PlayerGuid;
            uint8 ResidentType = 0;
            bool IsOnline = false;
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
        uint8 Result = 0;
        ObjectGuid PlayerGuid;
    };

    class NeighborhoodEvictPlotResponse final : public ServerPacket
    {
    public:
        NeighborhoodEvictPlotResponse() : ServerPacket(SMSG_NEIGHBORHOOD_EVICT_PLOT_RESPONSE) { }
        WorldPacket const* Write() override;
        uint8 Result = 0;
        ObjectGuid NeighborhoodGuid;
    };

    class NeighborhoodEvictPlotNotice final : public ServerPacket
    {
    public:
        NeighborhoodEvictPlotNotice() : ServerPacket(SMSG_NEIGHBORHOOD_EVICT_PLOT_NOTICE) { }
        WorldPacket const* Write() override;
        uint32 PlotId = 0;
        ObjectGuid NeighborhoodGuid;
        ObjectGuid PlotGuid;
    };

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
}

#endif // TRINITYCORE_HOUSING_PACKETS_H
