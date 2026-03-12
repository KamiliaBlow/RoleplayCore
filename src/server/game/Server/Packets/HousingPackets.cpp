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
    _worldPacket >> HouseGuid;
    _worldPacket >> PlotGuid;
    _worldPacket >> PositionX;
    _worldPacket >> PositionY;
    _worldPacket >> PositionZ;
    _worldPacket >> Facing;

    TC_LOG_DEBUG("network.opcode", "CMSG_HOUSE_EXTERIOR_COMMIT_POSITION HouseGuid: {} PlotGuid: {} Pos: ({}, {}, {}) Facing: {:.2f}",
        HouseGuid.ToString(), PlotGuid.ToString(), PositionX, PositionY, PositionZ, Facing);
}

// --- Decor System ---

void HousingDecorSetEditMode::Read()
{
    _worldPacket >> Bits<1>(Active);

    TC_LOG_DEBUG("network.opcode", "CMSG_HOUSING_DECOR_SET_EDIT_MODE Active: {}", Active);
}

void HousingDecorPlace::Read()
{
    _worldPacket >> DecorGuid;
    _worldPacket >> Position;
    _worldPacket >> Rotation;
    _worldPacket >> Scale;
    _worldPacket >> AttachParentGuid;
    _worldPacket >> RoomGuid;
    _worldPacket >> Field_61;
    _worldPacket >> Field_62;
    _worldPacket >> Field_63;

    TC_LOG_INFO("network.opcode", "CMSG_HOUSING_DECOR_PLACE DecorGuid: {} Pos: ({}, {}, {}) Rot: ({}, {}, {}) Scale: {} AttachParent: {} RoomGuid: {} Field_61: {} Field_62: {} Field_63: {}",
        DecorGuid.ToString(), Position.Pos.GetPositionX(), Position.Pos.GetPositionY(), Position.Pos.GetPositionZ(),
        Rotation.Pos.GetPositionX(), Rotation.Pos.GetPositionY(), Rotation.Pos.GetPositionZ(), Scale,
        AttachParentGuid.ToString(), RoomGuid.ToString(), Field_61, Field_62, Field_63);
}

void HousingDecorMove::Read()
{
    _worldPacket >> DecorGuid;
    _worldPacket >> Position;
    _worldPacket >> Rotation;
    _worldPacket >> Scale;
    _worldPacket >> AttachParentGuid;
    _worldPacket >> RoomGuid;
    _worldPacket >> Field_70;
    _worldPacket >> Field_80;
    _worldPacket >> Field_85;
    _worldPacket >> Field_86;
    _worldPacket >> Bits<1>(IsBasicMove);

    TC_LOG_DEBUG("network.opcode", "CMSG_HOUSING_DECOR_MOVE DecorGuid: {} Pos: ({}, {}, {}) Rot: ({}, {}, {}) Scale: {} IsBasicMove: {}",
        DecorGuid.ToString(), Position.Pos.GetPositionX(), Position.Pos.GetPositionY(), Position.Pos.GetPositionZ(),
        Rotation.Pos.GetPositionX(), Rotation.Pos.GetPositionY(), Rotation.Pos.GetPositionZ(), Scale, IsBasicMove);
}

void HousingDecorRemove::Read()
{
    _worldPacket >> DecorGuid;

    TC_LOG_DEBUG("network.opcode", "CMSG_HOUSING_DECOR_REMOVE DecorGuid: {}", DecorGuid.ToString());
}

void HousingDecorLock::Read()
{
    _worldPacket >> DecorGuid;
    _worldPacket >> Bits<1>(Locked);

    TC_LOG_DEBUG("network.opcode", "CMSG_HOUSING_DECOR_LOCK DecorGuid: {} Locked: {}", DecorGuid.ToString(), Locked);
}

void HousingDecorSetDyeSlots::Read()
{
    _worldPacket >> DecorGuid;
    for (int32& dyeColor : DyeColorID)
        _worldPacket >> dyeColor;

    TC_LOG_DEBUG("network.opcode", "CMSG_HOUSING_DECOR_SET_DYE_SLOTS DecorGuid: {} DyeColors: [{}, {}, {}]",
        DecorGuid.ToString(), DyeColorID[0], DyeColorID[1], DyeColorID[2]);
}

void HousingDecorDeleteFromStorage::Read()
{
    uint32 count = 0;
    _worldPacket >> Bits<32>(count);
    DecorGuids.resize(count);
    for (ObjectGuid& guid : DecorGuids)
        _worldPacket >> guid;

    TC_LOG_DEBUG("network.opcode", "CMSG_HOUSING_DECOR_DELETE_FROM_STORAGE Count: {}", count);
}

void HousingDecorDeleteFromStorageById::Read()
{
    _worldPacket >> DecorRecID;

    TC_LOG_DEBUG("network.opcode", "CMSG_HOUSING_DECOR_DELETE_FROM_STORAGE_BY_ID DecorRecID: {}", DecorRecID);
}

void HousingDecorRequestStorage::Read()
{
    _worldPacket >> HouseGuid;

    TC_LOG_DEBUG("network.opcode", "CMSG_HOUSING_DECOR_REQUEST_STORAGE HouseGuid: {}", HouseGuid.ToString());
}

void HousingDecorRedeemDeferredDecor::Read()
{
    _worldPacket >> DeferredDecorID;
    _worldPacket >> RedemptionToken;

    TC_LOG_INFO("network.opcode", "CMSG_HOUSING_DECOR_REDEEM_DEFERRED DeferredDecorID: {} RedemptionToken: {} (pktSize={})", DeferredDecorID, RedemptionToken, _worldPacket.size());
}

void HousingDecorStartPlacingNewDecor::Read()
{
    _worldPacket >> CatalogEntryID;
    _worldPacket >> Field_4;

    TC_LOG_DEBUG("network.opcode", "CMSG_HOUSING_DECOR_START_PLACING_NEW_DECOR CatalogEntryID: {} Field_4: {}", CatalogEntryID, Field_4);
}

void HousingDecorCatalogCreateSearcher::Read()
{
    _worldPacket >> Owner;

    TC_LOG_DEBUG("network.opcode", "CMSG_HOUSING_DECOR_CATALOG_CREATE_SEARCHER Owner: {}", Owner.ToString());
}

void HousingDecorUpdateDyeSlot::Read()
{
    _worldPacket >> DecorGuid;
    _worldPacket >> SlotIndex;
    _worldPacket >> DyeColorID;

    TC_LOG_DEBUG("network.opcode", "CMSG_HOUSING_DECOR_UPDATE_DYE_SLOT DecorGuid: {} SlotIndex: {} DyeColorID: {}",
        DecorGuid.ToString(), SlotIndex, DyeColorID);
}

void HousingDecorStartPlacingFromSource::Read()
{
    _worldPacket >> SourceType;
    _worldPacket >> SourceID;

    TC_LOG_DEBUG("network.opcode", "CMSG_HOUSING_DECOR_START_PLACING_FROM_SOURCE SourceType: {} SourceID: {}",
        SourceType, SourceID);
}

void HousingDecorCleanupModeToggle::Read()
{
    _worldPacket >> Bits<1>(Enabled);

    TC_LOG_DEBUG("network.opcode", "CMSG_HOUSING_DECOR_CLEANUP_MODE_TOGGLE Enabled: {}", Enabled);
}

void HousingDecorBatchOperation::Read()
{
    _worldPacket >> OperationType;
    uint32 count = 0;
    _worldPacket >> Bits<32>(count);
    DecorGuids.resize(count);
    for (ObjectGuid& guid : DecorGuids)
        _worldPacket >> guid;

    TC_LOG_DEBUG("network.opcode", "CMSG_HOUSING_DECOR_BATCH_OPERATION OperationType: {} Count: {}",
        OperationType, count);
}

void HousingDecorPlacementPreview::Read()
{
    _worldPacket >> DecorGuid;
    _worldPacket >> PreviewPosition;
    _worldPacket >> PreviewRotation;
    _worldPacket >> Scale;

    TC_LOG_DEBUG("network.opcode", "CMSG_HOUSING_DECOR_PLACEMENT_PREVIEW DecorGuid: {} Pos: ({}, {}, {}) Scale: {}",
        DecorGuid.ToString(), PreviewPosition.Pos.GetPositionX(), PreviewPosition.Pos.GetPositionY(),
        PreviewPosition.Pos.GetPositionZ(), Scale);
}

// --- Fixture System ---

void HousingFixtureSetEditMode::Read()
{
    _worldPacket >> Bits<1>(Active);

    TC_LOG_DEBUG("network.opcode", "CMSG_HOUSING_FIXTURE_SET_EDIT_MODE Active: {}", Active);
}

void HousingFixtureSetCoreFixture::Read()
{
    _worldPacket >> FixtureGuid;
    _worldPacket >> ExteriorComponentID;

    TC_LOG_DEBUG("network.opcode", "CMSG_HOUSING_FIXTURE_SET_CORE_FIXTURE FixtureGuid: {} ExteriorComponentID: {}", FixtureGuid.ToString(), ExteriorComponentID);
}

void HousingFixtureCreateFixture::Read()
{
    _worldPacket >> AttachParentGuid;
    _worldPacket >> RoomGuid;
    _worldPacket >> ExteriorComponentType;
    _worldPacket >> ExteriorComponentHookID;

    TC_LOG_DEBUG("network.opcode", "CMSG_HOUSING_FIXTURE_CREATE AttachParentGuid: {} RoomGuid: {} ExteriorComponentType: {} HookID: {}",
        AttachParentGuid.ToString(), RoomGuid.ToString(), ExteriorComponentType, ExteriorComponentHookID);
}

void HousingFixtureDeleteFixture::Read()
{
    _worldPacket >> FixtureGuid;
    _worldPacket >> RoomGuid;
    _worldPacket >> ExteriorComponentID;

    TC_LOG_DEBUG("network.opcode", "CMSG_HOUSING_FIXTURE_DELETE FixtureGuid: {} RoomGuid: {} ExteriorComponentID: {}", FixtureGuid.ToString(), RoomGuid.ToString(), ExteriorComponentID);
}

void HousingFixtureSetHouseSize::Read()
{
    _worldPacket >> HouseGuid;
    _worldPacket >> Size;

    TC_LOG_DEBUG("network.opcode", "CMSG_HOUSING_FIXTURE_SET_HOUSE_SIZE HouseGuid: {} Size: {}", HouseGuid.ToString(), Size);
}

void HousingFixtureSetHouseType::Read()
{
    _worldPacket >> HouseGuid;
    _worldPacket >> HouseExteriorWmoDataID;

    TC_LOG_DEBUG("network.opcode", "CMSG_HOUSING_FIXTURE_SET_HOUSE_TYPE HouseGuid: {} HouseExteriorWmoDataID: {}", HouseGuid.ToString(), HouseExteriorWmoDataID);
}

void HousingFixtureCreateBasicHouse::Read()
{
    _worldPacket >> PlotGuid;
    _worldPacket >> HouseStyleID;

    TC_LOG_DEBUG("network.opcode", "CMSG_HOUSING_FIXTURE_CREATE_BASIC_HOUSE PlotGuid: {} HouseStyleID: {}",
        PlotGuid.ToString(), HouseStyleID);
}

void HousingFixtureDeleteHouse::Read()
{
    _worldPacket >> HouseGuid;

    TC_LOG_DEBUG("network.opcode", "CMSG_HOUSING_FIXTURE_DELETE_HOUSE HouseGuid: {}", HouseGuid.ToString());
}

void HouseExteriorLock::Read()
{
    _worldPacket >> HouseGuid;
    _worldPacket >> PlotGuid;
    _worldPacket >> NeighborhoodGuid;
    _worldPacket >> Bits<1>(Locked);

    TC_LOG_DEBUG("network.opcode", "CMSG_HOUSE_EXTERIOR_LOCK HouseGuid: {} PlotGuid: {} NeighborhoodGuid: {} Locked: {}",
        HouseGuid.ToString(), PlotGuid.ToString(), NeighborhoodGuid.ToString(), Locked);
}

// --- Room System ---

void HousingRoomSetLayoutEditMode::Read()
{
    _worldPacket >> Bits<1>(Active);

    TC_LOG_DEBUG("network.opcode", "CMSG_HOUSING_ROOM_SET_LAYOUT_EDIT_MODE Active: {}", Active);
}

void HousingRoomAdd::Read()
{
    _worldPacket >> HouseGuid;
    _worldPacket >> HouseRoomID;
    _worldPacket >> Flags;
    _worldPacket >> FloorIndex;
    _worldPacket >> Bits<1>(AutoFurnish);

    TC_LOG_DEBUG("network.opcode", "CMSG_HOUSING_ROOM_ADD HouseGuid: {} HouseRoomID: {} Flags: {} FloorIndex: {} AutoFurnish: {}",
        HouseGuid.ToString(), HouseRoomID, Flags, FloorIndex, AutoFurnish);
}

void HousingRoomRemove::Read()
{
    _worldPacket >> RoomGuid;

    TC_LOG_DEBUG("network.opcode", "CMSG_HOUSING_ROOM_REMOVE RoomGuid: {}", RoomGuid.ToString());
}

void HousingRoomRotate::Read()
{
    _worldPacket >> RoomGuid;
    _worldPacket >> Bits<1>(Clockwise);

    TC_LOG_DEBUG("network.opcode", "CMSG_HOUSING_ROOM_ROTATE RoomGuid: {} Clockwise: {}", RoomGuid.ToString(), Clockwise);
}

void HousingRoomMoveRoom::Read()
{
    _worldPacket >> RoomGuid;
    _worldPacket >> TargetSlotIndex;
    _worldPacket >> TargetGuid;
    _worldPacket >> FloorIndex;

    TC_LOG_DEBUG("network.opcode", "CMSG_HOUSING_ROOM_MOVE RoomGuid: {} TargetSlotIndex: {} TargetGuid: {} FloorIndex: {}",
        RoomGuid.ToString(), TargetSlotIndex, TargetGuid.ToString(), FloorIndex);
}

void HousingRoomSetComponentTheme::Read()
{
    _worldPacket >> RoomGuid;
    _worldPacket >> Size<uint32>(RoomComponentIDs);
    _worldPacket >> HouseThemeID;
    for (uint32& componentID : RoomComponentIDs)
        _worldPacket >> componentID;

    TC_LOG_DEBUG("network.opcode", "CMSG_HOUSING_ROOM_SET_COMPONENT_THEME RoomGuid: {} HouseThemeID: {} ComponentCount: {}",
        RoomGuid.ToString(), HouseThemeID, RoomComponentIDs.size());
}

void HousingRoomApplyComponentMaterials::Read()
{
    _worldPacket >> RoomGuid;
    _worldPacket >> Size<uint32>(RoomComponentIDs);
    _worldPacket >> RoomComponentTextureID;
    _worldPacket >> RoomComponentTypeParam;
    for (uint32& componentID : RoomComponentIDs)
        _worldPacket >> componentID;

    TC_LOG_DEBUG("network.opcode", "CMSG_HOUSING_ROOM_APPLY_COMPONENT_MATERIALS RoomGuid: {} TextureID: {} TypeParam: {} ComponentCount: {}",
        RoomGuid.ToString(), RoomComponentTextureID, RoomComponentTypeParam, RoomComponentIDs.size());
}

void HousingRoomSetDoorType::Read()
{
    _worldPacket >> RoomGuid;
    _worldPacket >> RoomComponentID;
    _worldPacket >> RoomComponentType;

    TC_LOG_DEBUG("network.opcode", "CMSG_HOUSING_ROOM_SET_DOOR_TYPE RoomGuid: {} ComponentID: {} Type: {}", RoomGuid.ToString(), RoomComponentID, RoomComponentType);
}

void HousingRoomSetCeilingType::Read()
{
    _worldPacket >> RoomGuid;
    _worldPacket >> RoomComponentID;
    _worldPacket >> RoomComponentType;

    TC_LOG_DEBUG("network.opcode", "CMSG_HOUSING_ROOM_SET_CEILING_TYPE RoomGuid: {} ComponentID: {} Type: {}", RoomGuid.ToString(), RoomComponentID, RoomComponentType);
}

// --- Housing Services System ---

void HousingSvcsGuildCreateNeighborhood::Read()
{
    _worldPacket >> NeighborhoodTypeID;
    _worldPacket >> SizedString::BitsSize<7>(NeighborhoodName);

    _worldPacket >> Flags;
    _worldPacket >> SizedString::Data(NeighborhoodName);

    TC_LOG_DEBUG("network.opcode", "CMSG_HOUSING_SVCS_GUILD_CREATE_NEIGHBORHOOD TypeID: {} Flags: {} Name: '{}'",
        NeighborhoodTypeID, Flags, NeighborhoodName);
}

void HousingSvcsNeighborhoodReservePlot::Read()
{
    _worldPacket >> NeighborhoodGuid;
    _worldPacket >> PlotIndex;
    _worldPacket >> Bits<1>(Reserve);

    TC_LOG_DEBUG("network.opcode", "CMSG_HOUSING_SVCS_NEIGHBORHOOD_RESERVE_PLOT NeighborhoodGuid: {} PlotIndex: {} Reserve: {}",
        NeighborhoodGuid.ToString(), PlotIndex, Reserve);
}

void HousingSvcsRelinquishHouse::Read()
{
    _worldPacket >> HouseGuid;

    TC_LOG_DEBUG("network.opcode", "CMSG_HOUSING_SVCS_RELINQUISH_HOUSE HouseGuid: {}", HouseGuid.ToString());
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

    TC_LOG_DEBUG("network.opcode", "CMSG_HOUSING_SVCS_UPDATE_HOUSE_SETTINGS HouseGuid: {} HasPlotSettings: {} HasVisitorPermission: {}",
        HouseGuid.ToString(), PlotSettingsID.has_value(), VisitorPermissionGuid.has_value());
}

void HousingSvcsPlayerViewHousesByPlayer::Read()
{
    _worldPacket >> PlayerGuid;

    TC_LOG_DEBUG("network.opcode", "CMSG_HOUSING_SVCS_PLAYER_VIEW_HOUSES_BY_PLAYER PlayerGuid: {}", PlayerGuid.ToString());
}

void HousingSvcsPlayerViewHousesByBnetAccount::Read()
{
    _worldPacket >> BnetAccountGuid;

    TC_LOG_DEBUG("network.opcode", "CMSG_HOUSING_SVCS_PLAYER_VIEW_HOUSES_BY_BNET BnetAccountGuid: {}", BnetAccountGuid.ToString());
}

void HousingSvcsTeleportToPlot::Read()
{
    _worldPacket >> NeighborhoodGuid;
    _worldPacket >> OwnerGuid;
    _worldPacket >> PlotIndex;
    _worldPacket >> TeleportType;

    TC_LOG_DEBUG("network.opcode", "CMSG_HOUSING_SVCS_TELEPORT_TO_PLOT NeighborhoodGuid: {} OwnerGuid: {} PlotIndex: {} TeleportType: {}",
        NeighborhoodGuid.ToString(), OwnerGuid.ToString(), PlotIndex, TeleportType);
}

void HousingSvcsSetTutorialState::Read()
{
    _worldPacket >> TutorialFlags;
    TC_LOG_DEBUG("network.opcode", "CMSG_HOUSING_SVCS_SET_TUTORIAL_STATE TutorialFlags: {}", TutorialFlags);
}

void HousingSvcsCompleteTutorialStep::Read()
{
    _worldPacket >> StepIndex;
    TC_LOG_DEBUG("network.opcode", "CMSG_HOUSING_SVCS_COMPLETE_TUTORIAL_STEP StepIndex: {}", StepIndex);
}

void HousingDecorConfirmPreviewPlacement::Read()
{
    _worldPacket >> DecorGuid;
    TC_LOG_DEBUG("network.opcode", "CMSG_HOUSING_DECOR_CONFIRM_PREVIEW_PLACEMENT DecorGuid: {}", DecorGuid.ToString());
}

void HousingSvcsAcceptNeighborhoodOwnership::Read()
{
    _worldPacket >> NeighborhoodGuid;

    TC_LOG_DEBUG("network.opcode", "CMSG_HOUSING_SVCS_ACCEPT_NEIGHBORHOOD_OWNERSHIP NeighborhoodGuid: {}", NeighborhoodGuid.ToString());
}

void HousingSvcsRejectNeighborhoodOwnership::Read()
{
    _worldPacket >> NeighborhoodGuid;

    TC_LOG_DEBUG("network.opcode", "CMSG_HOUSING_SVCS_REJECT_NEIGHBORHOOD_OWNERSHIP NeighborhoodGuid: {}", NeighborhoodGuid.ToString());
}

void HousingSvcsGetHouseFinderNeighborhood::Read()
{
    _worldPacket >> NeighborhoodGuid;

    TC_LOG_DEBUG("network.opcode", "CMSG_HOUSING_SVCS_GET_HOUSE_FINDER_NEIGHBORHOOD NeighborhoodGuid: {}", NeighborhoodGuid.ToString());
}

void HousingSvcsGetBnetFriendNeighborhoods::Read()
{
    _worldPacket >> BnetAccountGuid;

    TC_LOG_DEBUG("network.opcode", "CMSG_HOUSING_SVCS_GET_BNET_FRIEND_NEIGHBORHOODS BnetAccountGuid: {}", BnetAccountGuid.ToString());
}

void HousingSvcsClearPlotReservation::Read()
{
    _worldPacket >> NeighborhoodGuid;

    TC_LOG_DEBUG("network.opcode", "CMSG_HOUSING_SVCS_CLEAR_PLOT_RESERVATION NeighborhoodGuid: {}", NeighborhoodGuid.ToString());
}

void HousingSvcsGetPlayerHousesInfoAlt::Read()
{
    _worldPacket >> PlayerGuid;

    TC_LOG_DEBUG("network.opcode", "CMSG_HOUSING_SVCS_GET_PLAYER_HOUSES_INFO_ALT PlayerGuid: {}", PlayerGuid.ToString());
}

void HousingSvcsGetRosterData::Read()
{
    _worldPacket >> NeighborhoodGuid;

    TC_LOG_DEBUG("network.opcode", "CMSG_HOUSING_SVCS_GET_ROSTER_DATA NeighborhoodGuid: {}", NeighborhoodGuid.ToString());
}

void HousingSvcsChangeHouseCosmeticOwnerRequest::Read()
{
    _worldPacket >> HouseGuid;
    _worldPacket >> NewOwnerGuid;

    TC_LOG_DEBUG("network.opcode", "CMSG_HOUSING_SVCS_CHANGE_HOUSE_COSMETIC_OWNER HouseGuid: {} NewOwnerGuid: {}",
        HouseGuid.ToString(), NewOwnerGuid.ToString());
}

void HousingSvcsQueryHouseLevelFavor::Read()
{
    _worldPacket >> HouseGuid;

    TC_LOG_DEBUG("network.opcode", "CMSG_HOUSING_SVCS_QUERY_HOUSE_LEVEL_FAVOR HouseGuid: {}", HouseGuid.ToString());
}

void HousingSvcsGuildAddHouse::Read()
{
    _worldPacket >> HouseGuid;

    TC_LOG_DEBUG("network.opcode", "CMSG_HOUSING_SVCS_GUILD_ADD_HOUSE HouseGuid: {}", HouseGuid.ToString());
}

void HousingSvcsGuildAppendNeighborhood::Read()
{
    _worldPacket >> NeighborhoodGuid;

    TC_LOG_DEBUG("network.opcode", "CMSG_HOUSING_SVCS_GUILD_APPEND_NEIGHBORHOOD NeighborhoodGuid: {}", NeighborhoodGuid.ToString());
}

void HousingSvcsGuildRenameNeighborhood::Read()
{
    _worldPacket >> NeighborhoodGuid;
    _worldPacket >> SizedString::BitsSize<7>(NewName);
    _worldPacket >> SizedString::Data(NewName);

    TC_LOG_DEBUG("network.opcode", "CMSG_HOUSING_SVCS_GUILD_RENAME_NEIGHBORHOOD NeighborhoodGuid: {} NewName: '{}'",
        NeighborhoodGuid.ToString(), NewName);
}

void HousingSvcsGuildGetHousingInfo::Read()
{
    _worldPacket >> GuildGuid;

    TC_LOG_DEBUG("network.opcode", "CMSG_HOUSING_SVCS_GUILD_GET_HOUSING_INFO GuildGuid: {}", GuildGuid.ToString());
}

// --- Housing Misc ---
// HousingGetCurrentHouseInfo::Read() and HousingHouseStatus::Read() are empty (inline in header)

void HousingRequestEditorAvailability::Read()
{
    _worldPacket >> Field_0;
    _worldPacket >> HouseGuid;

    TC_LOG_DEBUG("network.opcode", "CMSG_HOUSING_REQUEST_EDITOR_AVAILABILITY Field_0: {} HouseGuid: {}", Field_0, HouseGuid.ToString());
}

void HousingGetPlayerPermissions::Read()
{
    _worldPacket >> OptionalInit(HouseGuid);

    if (HouseGuid)
        _worldPacket >> *HouseGuid;

    TC_LOG_DEBUG("network.opcode", "CMSG_HOUSING_GET_PLAYER_PERMISSIONS HasHouseGuid: {}", HouseGuid.has_value());
}

void HousingSvcsGetPotentialHouseOwners::Read()
{
    _worldPacket >> NeighborhoodGuid;

    TC_LOG_DEBUG("network.opcode", "CMSG_HOUSING_SVCS_GET_POTENTIAL_HOUSE_OWNERS NeighborhoodGuid: {}", NeighborhoodGuid.ToString());
}

void HousingSystemGetHouseInfoAlt::Read()
{
    _worldPacket >> HouseGuid;

    TC_LOG_DEBUG("network.opcode", "CMSG_HOUSING_SYSTEM_GET_HOUSE_INFO_ALT HouseGuid: {}", HouseGuid.ToString());
}

void HousingSystemHouseSnapshot::Read()
{
    _worldPacket >> HouseGuid;
    _worldPacket >> SnapshotType;

    TC_LOG_DEBUG("network.opcode", "CMSG_HOUSING_SYSTEM_HOUSE_SNAPSHOT HouseGuid: {} SnapshotType: {}",
        HouseGuid.ToString(), SnapshotType);
}

void HousingSystemExportHouse::Read()
{
    _worldPacket >> HouseGuid;

    TC_LOG_DEBUG("network.opcode", "CMSG_HOUSING_SYSTEM_EXPORT_HOUSE HouseGuid: {}", HouseGuid.ToString());
}

void HousingSystemUpdateHouseInfo::Read()
{
    _worldPacket >> HouseGuid;
    _worldPacket >> InfoType;
    _worldPacket >> SizedString::BitsSize<8>(HouseName);
    _worldPacket >> SizedString::BitsSize<10>(HouseDescription);
    _worldPacket >> SizedString::Data(HouseName);
    _worldPacket >> SizedString::Data(HouseDescription);

    TC_LOG_DEBUG("network.opcode", "CMSG_HOUSING_SYSTEM_UPDATE_HOUSE_INFO HouseGuid: {} InfoType: {} Name: '{}' Desc: '{}'",
        HouseGuid.ToString(), InfoType, HouseName, HouseDescription);
}

// --- Other Housing CMSG ---

void DeclineNeighborhoodInvites::Read()
{
    _worldPacket >> Bits<1>(Allow);

    TC_LOG_DEBUG("network.opcode", "CMSG_DECLINE_NEIGHBORHOOD_INVITES Allow: {}", Allow);
}

void QueryNeighborhoodInfo::Read()
{
    _worldPacket >> NeighborhoodGuid;

    TC_LOG_DEBUG("network.opcode", "CMSG_QUERY_NEIGHBORHOOD_INFO NeighborhoodGuid: {}", NeighborhoodGuid.ToString());
}

void InvitePlayerToNeighborhood::Read()
{
    _worldPacket >> PlayerGuid;
    _worldPacket >> NeighborhoodGuid;

    TC_LOG_DEBUG("network.opcode", "CMSG_INVITE_PLAYER_TO_NEIGHBORHOOD PlayerGuid: {} NeighborhoodGuid: {}",
        PlayerGuid.ToString(), NeighborhoodGuid.ToString());
}

void GuildGetOthersOwnedHouses::Read()
{
    _worldPacket >> PlayerGuid;

    TC_LOG_DEBUG("network.opcode", "CMSG_GUILD_GET_OTHERS_OWNED_HOUSES PlayerGuid: {}", PlayerGuid.ToString());
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

    TC_LOG_DEBUG("network.opcode", "SMSG_QUERY_NEIGHBORHOOD_NAME_RESPONSE NeighborhoodGuid: {} Result: {} Name: '{}'",
        NeighborhoodGuid.ToString(), Result, NeighborhoodName);

    return &_worldPacket;
}

WorldPacket const* InvalidateNeighborhoodName::Write()
{
    _worldPacket << NeighborhoodGuid;

    TC_LOG_DEBUG("network.opcode", "SMSG_INVALIDATE_NEIGHBORHOOD_NAME NeighborhoodGuid: {}", NeighborhoodGuid.ToString());

    return &_worldPacket;
}

// ============================================================
// House Exterior SMSG Responses (0x50xxxx)
// ============================================================

WorldPacket const* HouseExteriorLockResponse::Write()
{
    _worldPacket << uint8(Result);
    _worldPacket << HouseGuid;
    _worldPacket.WriteBit(Locked);
    _worldPacket.FlushBits();

    TC_LOG_DEBUG("network.opcode", "SMSG_HOUSE_EXTERIOR_LOCK_RESPONSE Result: {} HouseGuid: {} Locked: {}", Result, HouseGuid.ToString(), Locked);

    return &_worldPacket;
}

WorldPacket const* HouseExteriorSetHousePositionResponse::Write()
{
    _worldPacket << uint8(Result);
    _worldPacket << HouseGuid;

    TC_LOG_DEBUG("network.opcode", "SMSG_HOUSE_EXTERIOR_SET_HOUSE_POSITION_RESPONSE Result: {} HouseGuid: {}", Result, HouseGuid.ToString());

    return &_worldPacket;
}

// ============================================================
// House Interior SMSG (0x2Fxxxx)
// ============================================================

WorldPacket const* HouseInteriorEnterHouse::Write()
{
    _worldPacket << HouseGuid;

    TC_LOG_DEBUG("network.opcode", "SMSG_HOUSE_INTERIOR_ENTER_HOUSE HouseGuid: {}", HouseGuid.ToString());

    return &_worldPacket;
}

WorldPacket const* HouseInteriorLeaveHouseResponse::Write()
{
    _worldPacket << uint8(TeleportReason);

    TC_LOG_DEBUG("network.opcode", "SMSG_HOUSE_INTERIOR_LEAVE_HOUSE_RESPONSE TeleportReason: {}", TeleportReason);

    return &_worldPacket;
}

// ============================================================
// Housing Decor SMSG Responses (0x51xxxx)
// ============================================================

WorldPacket const* HousingDecorSetEditModeResponse::Write()
{
    _worldPacket << HouseGuid;
    _worldPacket << BNetAccountGuid;
    _worldPacket << uint32(AllowedEditor.size());
    _worldPacket << uint8(Result);

    for (ObjectGuid const& guid : AllowedEditor)
        _worldPacket << guid;

    TC_LOG_DEBUG("network.opcode", "SMSG_HOUSING_DECOR_SET_EDIT_MODE_RESPONSE Result: {} HouseGuid: {} AllowedEditors: {}",
        Result, HouseGuid.ToString(), uint32(AllowedEditor.size()));

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

    TC_LOG_DEBUG("network.opcode", "SMSG_HOUSING_DECOR_MOVE_RESPONSE Result: {} PlayerGuid: {} DecorGuid: {}",
        Result, PlayerGuid.ToString(), DecorGuid.ToString());

    return &_worldPacket;
}

WorldPacket const* HousingDecorPlaceResponse::Write()
{
    // Wire format: PackedGUID PlayerGuid + uint32 Field_09 + PackedGUID DecorGuid + uint8 Result
    _worldPacket << PlayerGuid;
    _worldPacket << uint32(Field_09);
    _worldPacket << DecorGuid;
    _worldPacket << uint8(Result);

    TC_LOG_INFO("network.opcode", "SMSG_HOUSING_DECOR_PLACE_RESPONSE PlayerGuid: {} Result: {} DecorGuid: {} Field_09: {}",
        PlayerGuid.ToString(), Result, DecorGuid.ToString(), Field_09);

    return &_worldPacket;
}

WorldPacket const* HousingDecorRemoveResponse::Write()
{
    // Wire format: PackedGUID DecorGUID + PackedGUID UnkGUID + uint32 Field_13 + uint8 Result
    _worldPacket << DecorGuid;
    _worldPacket << UnkGUID;
    _worldPacket << uint32(Field_13);
    _worldPacket << uint8(Result);

    TC_LOG_INFO("network.opcode", "SMSG_HOUSING_DECOR_REMOVE_RESPONSE DecorGuid: {} Result: {}",
        DecorGuid.ToString(), Result);

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

    TC_LOG_INFO("network.opcode", "SMSG_HOUSING_DECOR_LOCK_RESPONSE DecorGuid: {} PlayerGuid: {} Result: {} Locked: {}",
        DecorGuid.ToString(), PlayerGuid.ToString(), Result, Locked);

    return &_worldPacket;
}

WorldPacket const* HousingDecorDeleteFromStorageResponse::Write()
{
    // IDA case 5308421: uint8(Result) only — client reads nothing else
    _worldPacket << uint8(Result);

    TC_LOG_DEBUG("network.opcode", "SMSG_HOUSING_DECOR_DELETE_FROM_STORAGE_RESPONSE Result: {}", Result);

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

    TC_LOG_DEBUG("network.opcode", "SMSG_HOUSING_DECOR_REQUEST_STORAGE_RESPONSE ResultCode: {} Flags: 0x{:02X} BNetAccountGuid: {}",
        ResultCode, Flags, BNetAccountGuid.ToString());

    return &_worldPacket;
}

WorldPacket const* HousingDecorAddToHouseChestResponse::Write()
{
    // IDA case 5308423: uint8(bit7=success) + uint32(count) + PackedGUID[count]
    _worldPacket << uint8(Success ? 0x80 : 0x00);
    _worldPacket << uint32(DecorGuids.size());
    for (ObjectGuid const& guid : DecorGuids)
        _worldPacket << guid;

    TC_LOG_DEBUG("network.opcode", "SMSG_HOUSING_DECOR_ADD_TO_HOUSE_CHEST_RESPONSE Success: {} DecorCount: {}",
        Success, uint32(DecorGuids.size()));

    return &_worldPacket;
}

WorldPacket const* HousingDecorSystemSetDyeSlotsResponse::Write()
{
    // Wire format: PackedGUID DecorGUID + uint8 Result
    _worldPacket << DecorGuid;
    _worldPacket << uint8(Result);

    TC_LOG_DEBUG("network.opcode", "SMSG_HOUSING_DECOR_SET_DYE_SLOTS_RESPONSE Result: {} DecorGuid: {}", Result, DecorGuid.ToString());

    return &_worldPacket;
}

WorldPacket const* HousingRedeemDeferredDecorResponse::Write()
{
    // Sniff-verified wire format (17 bytes): PackedGUID DecorGuid + uint8 Status + uint32 SequenceIndex
    _worldPacket << DecorGuid;
    _worldPacket << uint8(Result);
    _worldPacket << uint32(SequenceIndex);

    TC_LOG_INFO("network.opcode", "SMSG_HOUSING_REDEEM_DEFERRED_DECOR_RESPONSE DecorGuid: {} Result: {} SequenceIndex: {}",
        DecorGuid.ToString(), Result, SequenceIndex);

    return &_worldPacket;
}

WorldPacket const* HousingFirstTimeDecorAcquisition::Write()
{
    _worldPacket << uint32(DecorEntryID);

    TC_LOG_DEBUG("network.opcode", "SMSG_HOUSING_FIRST_TIME_DECOR_ACQUISITION DecorEntryID: {}", DecorEntryID);

    return &_worldPacket;
}

WorldPacket const* HousingDecorStartPlacingNewDecorResponse::Write()
{
    _worldPacket << DecorGuid;
    _worldPacket << uint8(Result);
    _worldPacket << Field_13;

    TC_LOG_DEBUG("network.opcode", "SMSG_HOUSING_DECOR_START_PLACING_NEW_DECOR_RESPONSE DecorGuid: {} Result: {} Field_13: {}",
        DecorGuid.ToString(), Result, Field_13);

    return &_worldPacket;
}

WorldPacket const* HousingDecorCatalogCreateSearcherResponse::Write()
{
    _worldPacket << Owner;
    _worldPacket << uint8(Result);

    TC_LOG_DEBUG("network.opcode", "SMSG_HOUSING_DECOR_CATALOG_CREATE_SEARCHER_RESPONSE Owner: {} Result: {}",
        Owner.ToString(), Result);

    return &_worldPacket;
}

WorldPacket const* HousingDecorBatchOperationResponse::Write()
{
    _worldPacket << uint8(Result);
    _worldPacket << uint32(ProcessedCount);

    TC_LOG_DEBUG("network.opcode", "SMSG_HOUSING_DECOR_BATCH_OPERATION_RESPONSE Result: {} ProcessedCount: {}",
        Result, ProcessedCount);

    return &_worldPacket;
}

WorldPacket const* HousingDecorPlacementPreviewResponse::Write()
{
    _worldPacket << uint8(Result);
    _worldPacket << uint8(RestrictionFlags);

    TC_LOG_DEBUG("network.opcode", "SMSG_HOUSING_DECOR_PLACEMENT_PREVIEW_RESPONSE Result: {} RestrictionFlags: {}",
        Result, RestrictionFlags);

    return &_worldPacket;
}

// ============================================================
// Housing Fixture SMSG Responses (0x52xxxx)
// ============================================================

WorldPacket const* HousingFixtureSetEditModeResponse::Write()
{
    // IDA case 5373952: PackedGUID + PackedGUID + uint8(Result)
    _worldPacket << HouseGuid;
    _worldPacket << FixtureGuid;
    _worldPacket << uint8(Result);

    TC_LOG_DEBUG("network.opcode", "SMSG_HOUSING_FIXTURE_SET_EDIT_MODE_RESPONSE HouseGuid: {} FixtureGuid: {} Result: {}",
        HouseGuid.ToString(), FixtureGuid.ToString(), Result);

    return &_worldPacket;
}

WorldPacket const* HousingFixtureCreateBasicHouseResponse::Write()
{
    // IDA case 5373953: uint8(Result) only
    _worldPacket << uint8(Result);

    TC_LOG_DEBUG("network.opcode", "SMSG_HOUSING_FIXTURE_CREATE_BASIC_HOUSE_RESPONSE Result: {}", Result);

    return &_worldPacket;
}

WorldPacket const* HousingFixtureDeleteHouseResponse::Write()
{
    // IDA case 5373954: uint8(Result) only
    _worldPacket << uint8(Result);

    TC_LOG_DEBUG("network.opcode", "SMSG_HOUSING_FIXTURE_DELETE_HOUSE_RESPONSE Result: {}", Result);

    return &_worldPacket;
}

WorldPacket const* HousingFixtureSetHouseSizeResponse::Write()
{
    _worldPacket << uint8(Result);
    _worldPacket << uint8(Size);

    TC_LOG_DEBUG("network.opcode", "SMSG_HOUSING_FIXTURE_SET_HOUSE_SIZE_RESPONSE Result: {} Size: {}", Result, Size);

    return &_worldPacket;
}

WorldPacket const* HousingFixtureSetHouseTypeResponse::Write()
{
    // IDA case 5373956: uint8(Result) + uint32(HouseExteriorTypeID) + uint8(ExtraField)
    _worldPacket << uint8(Result);
    _worldPacket << uint32(HouseExteriorTypeID);
    _worldPacket << uint8(ExtraField);

    TC_LOG_DEBUG("network.opcode", "SMSG_HOUSING_FIXTURE_SET_HOUSE_TYPE_RESPONSE Result: {} HouseExteriorTypeID: {} ExtraField: {}",
        Result, HouseExteriorTypeID, ExtraField);

    return &_worldPacket;
}

WorldPacket const* HousingFixtureSetCoreFixtureResponse::Write()
{
    // IDA case 5373957: uint8(Result) only
    _worldPacket << uint8(Result);

    TC_LOG_DEBUG("network.opcode", "SMSG_HOUSING_FIXTURE_SET_CORE_FIXTURE_RESPONSE Result: {}", Result);

    return &_worldPacket;
}

WorldPacket const* HousingFixtureCreateFixtureResponse::Write()
{
    // IDA case 5373958: PackedGUID + uint8(Result)
    _worldPacket << FixtureGuid;
    _worldPacket << uint8(Result);

    TC_LOG_DEBUG("network.opcode", "SMSG_HOUSING_FIXTURE_CREATE_FIXTURE_RESPONSE FixtureGuid: {} Result: {}", FixtureGuid.ToString(), Result);

    return &_worldPacket;
}

WorldPacket const* HousingFixtureDeleteFixtureResponse::Write()
{
    // IDA case 5373959: PackedGUID + uint8(Result)
    _worldPacket << FixtureGuid;
    _worldPacket << uint8(Result);

    TC_LOG_DEBUG("network.opcode", "SMSG_HOUSING_FIXTURE_DELETE_FIXTURE_RESPONSE FixtureGuid: {} Result: {}", FixtureGuid.ToString(), Result);

    return &_worldPacket;
}

// ============================================================
// Housing Room SMSG Responses (0x53xxxx)
// ============================================================

WorldPacket const* HousingRoomSetLayoutEditModeResponse::Write()
{
    // IDA case 5439488: PackedGUID + uint8(Result) + uint8(bit7=Active)
    _worldPacket << RoomGuid;
    _worldPacket << uint8(Result);
    _worldPacket << uint8(Active ? 0x80 : 0x00);

    TC_LOG_DEBUG("network.opcode", "SMSG_HOUSING_ROOM_SET_LAYOUT_EDIT_MODE_RESPONSE RoomGuid: {} Result: {} Active: {}",
        RoomGuid.ToString(), Result, Active);

    return &_worldPacket;
}

WorldPacket const* HousingRoomAddResponse::Write()
{
    // IDA case 5439489: PackedGUID + uint8(Result)
    _worldPacket << RoomGuid;
    _worldPacket << uint8(Result);

    TC_LOG_DEBUG("network.opcode", "SMSG_HOUSING_ROOM_ADD_RESPONSE RoomGuid: {} Result: {}", RoomGuid.ToString(), Result);

    return &_worldPacket;
}

WorldPacket const* HousingRoomRemoveResponse::Write()
{
    // IDA case 5439490: PackedGUID + PackedGUID + uint8(Result)
    _worldPacket << RoomGuid;
    _worldPacket << SecondGuid;
    _worldPacket << uint8(Result);

    TC_LOG_DEBUG("network.opcode", "SMSG_HOUSING_ROOM_REMOVE_RESPONSE RoomGuid: {} SecondGuid: {} Result: {}",
        RoomGuid.ToString(), SecondGuid.ToString(), Result);

    return &_worldPacket;
}

WorldPacket const* HousingRoomUpdateResponse::Write()
{
    // IDA case 5439491: PackedGUID + uint8(Result)
    _worldPacket << RoomGuid;
    _worldPacket << uint8(Result);

    TC_LOG_DEBUG("network.opcode", "SMSG_HOUSING_ROOM_UPDATE_RESPONSE RoomGuid: {} Result: {}", RoomGuid.ToString(), Result);

    return &_worldPacket;
}

WorldPacket const* HousingRoomSetComponentThemeResponse::Write()
{
    // IDA case 5439492: PackedGUID + uint32(arrayCount) + uint32(ThemeSetID) + uint8(Result) + uint32[arrayCount]
    _worldPacket << RoomGuid;
    _worldPacket << uint32(ComponentIDs.size());
    _worldPacket << uint32(ThemeSetID);
    _worldPacket << uint8(Result);
    for (uint32 compId : ComponentIDs)
        _worldPacket << uint32(compId);

    TC_LOG_DEBUG("network.opcode", "SMSG_HOUSING_ROOM_SET_COMPONENT_THEME_RESPONSE RoomGuid: {} ThemeSetID: {} Result: {} ComponentCount: {}",
        RoomGuid.ToString(), ThemeSetID, Result, uint32(ComponentIDs.size()));

    return &_worldPacket;
}

WorldPacket const* HousingRoomApplyComponentMaterialsResponse::Write()
{
    // IDA case 5439493: PackedGUID + uint32(arrayCount) + uint32(TextureRecordID) + uint8(Result) + uint32[arrayCount]
    _worldPacket << RoomGuid;
    _worldPacket << uint32(ComponentIDs.size());
    _worldPacket << uint32(RoomComponentTextureRecordID);
    _worldPacket << uint8(Result);
    for (uint32 compId : ComponentIDs)
        _worldPacket << uint32(compId);

    TC_LOG_DEBUG("network.opcode", "SMSG_HOUSING_ROOM_APPLY_COMPONENT_MATERIALS_RESPONSE RoomGuid: {} TextureRecordID: {} Result: {} ComponentCount: {}",
        RoomGuid.ToString(), RoomComponentTextureRecordID, Result, uint32(ComponentIDs.size()));

    return &_worldPacket;
}

WorldPacket const* HousingRoomSetDoorTypeResponse::Write()
{
    // IDA case 5439494: PackedGUID + uint32(ComponentID) + uint8(DoorType) + uint8(Result)
    _worldPacket << RoomGuid;
    _worldPacket << uint32(ComponentID);
    _worldPacket << uint8(DoorType);
    _worldPacket << uint8(Result);

    TC_LOG_DEBUG("network.opcode", "SMSG_HOUSING_ROOM_SET_DOOR_TYPE_RESPONSE RoomGuid: {} ComponentID: {} DoorType: {} Result: {}",
        RoomGuid.ToString(), ComponentID, DoorType, Result);

    return &_worldPacket;
}

WorldPacket const* HousingRoomSetCeilingTypeResponse::Write()
{
    // IDA case 5439495: PackedGUID + uint32(ComponentID) + uint8(CeilingType) + uint8(Result)
    _worldPacket << RoomGuid;
    _worldPacket << uint32(ComponentID);
    _worldPacket << uint8(CeilingType);
    _worldPacket << uint8(Result);

    TC_LOG_DEBUG("network.opcode", "SMSG_HOUSING_ROOM_SET_CEILING_TYPE_RESPONSE RoomGuid: {} ComponentID: {} CeilingType: {} Result: {}",
        RoomGuid.ToString(), ComponentID, CeilingType, Result);

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

    TC_LOG_DEBUG("network.opcode", "SMSG_HOUSING_SVCS_NOTIFY_PERMISSIONS_FAILURE FailureType: {} ErrorCode: {}", FailureType, ErrorCode);

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

    TC_LOG_DEBUG("network.opcode", "SMSG_HOUSING_SVCS_GUILD_CREATE_NEIGHBORHOOD_NOTIFICATION NeighborhoodGuid: {} Flag: {} Name: '{}'",
        NeighborhoodGuid.ToString(), Flag, Name);

    return &_worldPacket;
}

WorldPacket const* HousingSvcsCreateNeighborhoodResponse::Write()
{
    // IDA case 5505026: JamCliHouseFinderNeighborhood_base + uint8(trailing)
    WriteJamCliHouseFinderNeighborhoodBase(_worldPacket, Neighborhood);
    _worldPacket << uint8(TrailingResult);

    TC_LOG_DEBUG("network.opcode", "SMSG_HOUSING_SVCS_CREATE_NEIGHBORHOOD_RESPONSE NeighborhoodGuid: {} TrailingResult: {}",
        Neighborhood.NeighborhoodGUID.ToString(), TrailingResult);

    return &_worldPacket;
}

WorldPacket const* HousingSvcsCreateCharterNeighborhoodResponse::Write()
{
    // IDA case 5505027: JamCliHouseFinderNeighborhood_base + uint8(trailing)
    WriteJamCliHouseFinderNeighborhoodBase(_worldPacket, Neighborhood);
    _worldPacket << uint8(TrailingResult);

    TC_LOG_DEBUG("network.opcode", "SMSG_HOUSING_SVCS_CREATE_CHARTER_NEIGHBORHOOD_RESPONSE NeighborhoodGuid: {} TrailingResult: {}",
        Neighborhood.NeighborhoodGUID.ToString(), TrailingResult);

    return &_worldPacket;
}

WorldPacket const* HousingSvcsNeighborhoodReservePlotResponse::Write()
{
    // Sniff-verified wire format: single uint8 Result (1 byte total)
    _worldPacket << uint8(Result);

    TC_LOG_DEBUG("network.opcode", "SMSG_HOUSING_SVCS_NEIGHBORHOOD_RESERVE_PLOT_RESPONSE Result: {}", Result);

    return &_worldPacket;
}

WorldPacket const* HousingSvcsClearPlotReservationResponse::Write()
{
    // IDA case 5505029: uint8 only (shared case with 0x54000E, 0x54000F)
    _worldPacket << uint8(Result);

    TC_LOG_DEBUG("network.opcode", "SMSG_HOUSING_SVCS_CLEAR_PLOT_RESERVATION_RESPONSE Result: {}", Result);

    return &_worldPacket;
}

WorldPacket const* HousingSvcsHouseExpirationNotification::Write()
{
    // IDA case 5505030: uint8 + uint64 + uint32
    _worldPacket << uint8(Type);
    _worldPacket << uint64(Timestamp);
    _worldPacket << uint32(Duration);

    TC_LOG_DEBUG("network.opcode", "SMSG_HOUSING_SVCS_HOUSE_EXPIRATION_NOTIFICATION Type: {} Timestamp: {} Duration: {}",
        Type, Timestamp, Duration);

    return &_worldPacket;
}

WorldPacket const* HousingSvcsRelinquishHouseResponse::Write()
{
    // IDA case 5505031: uint8(Result) + PackedGUID + PackedGUID
    _worldPacket << uint8(Result);
    _worldPacket << HouseGuid;
    _worldPacket << NeighborhoodGuid;

    TC_LOG_DEBUG("network.opcode", "SMSG_HOUSING_SVCS_RELINQUISH_HOUSE_RESPONSE Result: {} HouseGuid: {} NeighborhoodGuid: {}",
        Result, HouseGuid.ToString(), NeighborhoodGuid.ToString());

    return &_worldPacket;
}

WorldPacket const* HousingSvcsCancelRelinquishHouseResponse::Write()
{
    // IDA case 5505032: uint32 + PackedGUID + uint8
    _worldPacket << uint32(Field1);
    _worldPacket << HouseGuid;
    _worldPacket << uint8(Result);

    TC_LOG_DEBUG("network.opcode", "SMSG_HOUSING_SVCS_CANCEL_RELINQUISH_HOUSE_RESPONSE Field1: {} HouseGuid: {} Result: {}",
        Field1, HouseGuid.ToString(), Result);

    return &_worldPacket;
}

// Helper: Write a JamHousingSearchResult entry to the packet
// IDA: Deserialize_JamHousingSearchResult (0x7FF724C7D4A0), stride 96
// Wire: PackedGUID + uint64 + uint64 + uint8 + PackedGUID + uint8(nameLen) + String(nameLen)
static void WriteJamHousingSearchResult(WorldPacket& packet, JamHousingSearchResult const& result)
{
    packet << result.PrimaryGUID;
    packet << uint64(result.SortKey);
    packet << uint64(result.SortData);
    packet << uint8(result.StatusType);
    packet << result.SecondaryGUID;
    uint8 nameLen = static_cast<uint8>(std::min<size_t>(result.Name.size() + 1, 255));
    packet << uint8(nameLen);
    if (nameLen > 0)
        packet.append(result.Name.c_str(), nameLen);
}

WorldPacket const* HousingSvcsSearchNeighborhoodsResponse::Write()
{
    // IDA case 5505033: uint32(count) + uint8(flags) + JamHousingSearchResult[count]
    _worldPacket << uint32(Results.size());
    _worldPacket << uint8(Flags);
    for (auto const& result : Results)
        WriteJamHousingSearchResult(_worldPacket, result);

    TC_LOG_DEBUG("network.opcode", "SMSG_HOUSING_SVCS_SEARCH_NEIGHBORHOODS_RESPONSE Count: {} Flags: {}",
        Results.size(), Flags);

    return &_worldPacket;
}

WorldPacket const* HousingSvcsGetNeighborhoodDetailsResponse::Write()
{
    // IDA case 5505034 (sub_7FF724C7D700):
    // Counts/metadata first, then arrays:
    // uint32(count1) + uint32(count2) + PackedGUID + uint64 + uint32(count3)
    // + uint32[count3] + JamCliHouse[count1] + JamCliHouse[count2]
    _worldPacket << uint32(PrimaryHouses.size());
    _worldPacket << uint32(SecondaryHouses.size());
    _worldPacket << NeighborhoodGUID;
    _worldPacket << uint64(Field1);
    _worldPacket << uint32(ExtraIds.size());
    for (uint32 id : ExtraIds)
        _worldPacket << uint32(id);
    for (auto const& house : PrimaryHouses)
        WriteJamCliHouse(_worldPacket, house);
    for (auto const& house : SecondaryHouses)
        WriteJamCliHouse(_worldPacket, house);

    TC_LOG_DEBUG("network.opcode", "SMSG_HOUSING_SVCS_GET_NEIGHBORHOOD_DETAILS_RESPONSE Primary: {} Secondary: {} ExtraIds: {} Neighborhood: {}",
        PrimaryHouses.size(), SecondaryHouses.size(), ExtraIds.size(), NeighborhoodGUID.ToString());

    return &_worldPacket;
}

WorldPacket const* HousingSvcsGetNeighborhoodHousesResponse::Write()
{
    // IDA case 5505037: uint32(count) + uint8(result) + JamCliHouse[count]
    _worldPacket << uint32(Houses.size());
    _worldPacket << uint8(Result);
    for (auto const& house : Houses)
        WriteJamCliHouse(_worldPacket, house);

    TC_LOG_DEBUG("network.opcode", "SMSG_HOUSING_SVCS_GET_NEIGHBORHOOD_HOUSES_RESPONSE Count: {} Result: {}",
        Houses.size(), Result);

    return &_worldPacket;
}

WorldPacket const* HousingSvcsMoveHouseResponse::Write()
{
    // IDA case 5505038: uint8 only (shared with 5505029/5505039)
    _worldPacket << uint8(Result);

    TC_LOG_DEBUG("network.opcode", "SMSG_HOUSING_SVCS_MOVE_HOUSE_RESPONSE Result: {}", Result);

    return &_worldPacket;
}

WorldPacket const* HousingSvcsSwapPlotsResponse::Write()
{
    // IDA case 5505039: uint8 only (shared with 5505029/5505038)
    _worldPacket << uint8(Result);

    TC_LOG_DEBUG("network.opcode", "SMSG_HOUSING_SVCS_SWAP_PLOTS_RESPONSE Result: {}", Result);

    return &_worldPacket;
}

ByteBuffer& operator<<(ByteBuffer& data, HouseInfo const& houseInfo)
{
    // IDA (0x5C0008/0x5C0009): PackedGUID + PackedGUID + PackedGUID + uint8 + uint32
    //   + uint8(bit7=HasMoveOutTime) [+ uint64(MoveOutTime)]
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
    packet << house.HouseGUID;
    packet << house.OwnerGUID;
    packet << house.NeighborhoodGUID;
    packet << uint8(house.HouseLevel);
    packet << uint32(house.PlotIndex);
    packet << uint8(house.HasOptionalField ? 0x80 : 0x00);
    if (house.HasOptionalField)
        packet << uint64(house.OptionalValue);
}

// Helper: Write JamCliHouseFinderNeighborhood BASE format (IDA: sub_7FF724C3F040, stride 120).
// Wire: PackedGUID + PackedGUID + uint64 + uint64 + uint32(housesCount)
//       + uint8(nameLen) + uint8(bit7=boolFlag) + JamCliHouse[count] + String(nameLen)
static void WriteJamCliHouseFinderNeighborhoodBase(WorldPacket& packet, JamCliHouseFinderNeighborhood const& entry)
{
    packet << entry.NeighborhoodGUID;
    packet << entry.OwnerGUID;
    packet << uint64(entry.Field1);
    packet << uint64(entry.Field2);
    packet << uint32(entry.Houses.size());
    uint8 nameLen = static_cast<uint8>(std::min<size_t>(entry.Name.size() + 1, 255));
    packet << uint8(nameLen);
    packet << uint8(entry.BoolFlag ? 0x80 : 0x00);
    for (auto const& house : entry.Houses)
        WriteJamCliHouse(packet, house);
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

    TC_LOG_DEBUG("network.opcode", "SMSG_HOUSING_SVCS_GET_PLAYER_HOUSES_INFO_RESPONSE HouseCount: {} Result: {}", Houses.size(), Result);

    return &_worldPacket;
}

WorldPacket const* HousingSvcsPlayerViewHousesResponse::Write()
{
    // IDA case 5505036: uint32(count) + uint8(result) + JamCliHouse[count]
    _worldPacket << uint32(Houses.size());
    _worldPacket << uint8(Result);
    for (auto const& house : Houses)
        WriteJamCliHouse(_worldPacket, house);

    TC_LOG_DEBUG("network.opcode", "SMSG_HOUSING_SVCS_PLAYER_VIEW_HOUSES_RESPONSE HouseCount: {} Result: {}", Houses.size(), Result);

    return &_worldPacket;
}

WorldPacket const* HousingSvcsChangeHouseCosmeticOwner::Write()
{
    // IDA case 5505040: uint8(result) + PackedGUID + PackedGUID
    _worldPacket << uint8(Result);
    _worldPacket << HouseGuid;
    _worldPacket << NewOwnerGuid;

    TC_LOG_DEBUG("network.opcode", "SMSG_HOUSING_SVCS_CHANGE_HOUSE_COSMETIC_OWNER Result: {} HouseGuid: {} NewOwnerGuid: {}",
        Result, HouseGuid.ToString(), NewOwnerGuid.ToString());

    return &_worldPacket;
}

WorldPacket const* AccountHousingRoomAdded::Write()
{
    _worldPacket << RoomGuid;
    return &_worldPacket;
}

WorldPacket const* AccountHousingFixtureAdded::Write()
{
    _worldPacket << FixtureGuid;
    _worldPacket.WriteBits(Name.size(), 7);
    _worldPacket.WriteBit(false); // HasTextureComposition
    _worldPacket.FlushBits();
    _worldPacket.WriteString(Name);
    return &_worldPacket;
}

WorldPacket const* AccountHousingThemeAdded::Write()
{
    _worldPacket << ThemeGuid;
    _worldPacket.WriteBits(Name.size(), 7);
    _worldPacket.WriteBit(false); // HasTextureComposition
    _worldPacket.FlushBits();
    _worldPacket.WriteString(Name);
    return &_worldPacket;
}

WorldPacket const* AccountHousingRoomComponentTextureAdded::Write()
{
    _worldPacket << TextureGuid;
    _worldPacket.WriteBits(Name.size(), 7);
    _worldPacket.WriteBit(false); // HasTextureComposition
    _worldPacket.FlushBits();
    _worldPacket.WriteString(Name);
    return &_worldPacket;
}

WorldPacket const* HousingSvcsUpdateHousesLevelFavor::Write()
{
    // IDA case 5505041: uint8 + uint32 + uint32 + uint32(count) + Entry[count]{3×GUID + uint32 + uint32 + uint8 + uint8(bit7)}
    _worldPacket << uint8(Type);
    _worldPacket << uint32(Field1);
    _worldPacket << uint32(Field2);
    _worldPacket << uint32(Entries.size());
    for (auto const& entry : Entries)
    {
        _worldPacket << entry.OwnerGUID;
        _worldPacket << entry.HouseGUID;
        _worldPacket << entry.NeighborhoodGUID;
        _worldPacket << uint32(entry.FavorAmount);
        _worldPacket << uint32(entry.Level);
        _worldPacket << uint8(entry.Flags);
        _worldPacket << uint8(entry.HasOptional ? 0x80 : 0x00);
    }

    TC_LOG_DEBUG("network.opcode", "SMSG_HOUSING_SVCS_UPDATE_HOUSES_LEVEL_FAVOR Type: {} EntryCount: {}", Type, Entries.size());

    return &_worldPacket;
}

WorldPacket const* HousingSvcsGuildAddHouseNotification::Write()
{
    // IDA case 5505042: JamCliHouse
    WriteJamCliHouse(_worldPacket, House);

    TC_LOG_DEBUG("network.opcode", "SMSG_HOUSING_SVCS_GUILD_ADD_HOUSE_NOTIFICATION HouseGuid: {}", House.HouseGUID.ToString());

    return &_worldPacket;
}

WorldPacket const* HousingSvcsGuildRemoveHouseNotification::Write()
{
    // IDA case 5505043: JamCliHouse
    WriteJamCliHouse(_worldPacket, House);

    TC_LOG_DEBUG("network.opcode", "SMSG_HOUSING_SVCS_GUILD_REMOVE_HOUSE_NOTIFICATION HouseGuid: {}", House.HouseGUID.ToString());

    return &_worldPacket;
}

WorldPacket const* HousingSvcsGuildAppendNeighborhoodNotification::Write()
{
    // IDA case 5505044: JamCliHouseFinderNeighborhood_base
    WriteJamCliHouseFinderNeighborhoodBase(_worldPacket, Neighborhood);

    TC_LOG_DEBUG("network.opcode", "SMSG_HOUSING_SVCS_GUILD_APPEND_NEIGHBORHOOD_NOTIFICATION NeighborhoodGuid: {}", Neighborhood.NeighborhoodGUID.ToString());

    return &_worldPacket;
}

WorldPacket const* HousingSvcsGuildRenameNeighborhoodNotification::Write()
{
    // IDA case 5505045: uint8(nameLen) + String(nameLen) — NO GUID
    uint8 nameLen = static_cast<uint8>(std::min<size_t>(NewName.size() + 1, 255));
    _worldPacket << uint8(nameLen);
    if (nameLen > 0)
        _worldPacket.append(NewName.c_str(), nameLen);

    TC_LOG_DEBUG("network.opcode", "SMSG_HOUSING_SVCS_GUILD_RENAME_NEIGHBORHOOD_NOTIFICATION NewName: '{}'", NewName);

    return &_worldPacket;
}

WorldPacket const* HousingSvcsGuildGetHousingInfoResponse::Write()
{
    // IDA case 5505046: uint32(count) + JamCliHouseFinderNeighborhood_base[count]
    _worldPacket << uint32(Neighborhoods.size());
    for (auto const& entry : Neighborhoods)
        WriteJamCliHouseFinderNeighborhoodBase(_worldPacket, entry);

    TC_LOG_DEBUG("network.opcode", "SMSG_HOUSING_SVCS_GUILD_GET_HOUSING_INFO_RESPONSE NeighborhoodCount: {}", Neighborhoods.size());

    return &_worldPacket;
}

WorldPacket const* HousingSvcsAcceptNeighborhoodOwnershipResponse::Write()
{
    // IDA case 5505047: uint8 only (error check, shows ERR_HOUSING_RESULT_GENERIC_FAILURE if non-zero)
    _worldPacket << uint8(Result);

    TC_LOG_DEBUG("network.opcode", "SMSG_HOUSING_SVCS_ACCEPT_NEIGHBORHOOD_OWNERSHIP_RESPONSE Result: {}", Result);

    return &_worldPacket;
}

WorldPacket const* HousingSvcsRejectNeighborhoodOwnershipResponse::Write()
{
    // IDA case 5505048: uint8 only
    _worldPacket << uint8(Result);

    TC_LOG_DEBUG("network.opcode", "SMSG_HOUSING_SVCS_REJECT_NEIGHBORHOOD_OWNERSHIP_RESPONSE Result: {}", Result);

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

    TC_LOG_DEBUG("network.opcode", "SMSG_HOUSING_SVCS_NEIGHBORHOOD_OWNERSHIP_TRANSFERRED Result: {} Owner: {} House: {}",
        Result, OwnerGUID.ToString(), HouseGUID.ToString());

    return &_worldPacket;
}

WorldPacket const* HousingSvcsGetPotentialHouseOwnersResponse::Write()
{
    // IDA case 5505050 (sub_7FF724C7DA70): NO Result byte
    // uint32(count) + Entry[count]{PackedGUID + uint32 + uint8 + uint8 + uint8(bit7→nameLen) + String(nameLen)}
    // Name length encoding: nameLen = 2 * second_uint8 | (third_uint8 >> 7)
    // So we encode: second_uint8 = nameLen >> 1, third_uint8 = (nameLen & 1) << 7
    _worldPacket << uint32(PotentialOwners.size());
    for (auto const& owner : PotentialOwners)
    {
        _worldPacket << owner.PlayerGuid;
        _worldPacket << uint32(owner.Field1);
        _worldPacket << uint8(owner.AccessLevel);
        uint32 nameLen = static_cast<uint32>(owner.PlayerName.size() + 1); // include null terminator
        uint8 lenByte1 = static_cast<uint8>(nameLen >> 1);
        uint8 lenByte2 = static_cast<uint8>((nameLen & 1) << 7);
        _worldPacket << uint8(lenByte1);
        _worldPacket << uint8(lenByte2);
        _worldPacket.append(owner.PlayerName.c_str(), nameLen);
    }

    TC_LOG_DEBUG("network.opcode", "SMSG_HOUSING_SVCS_GET_POTENTIAL_HOUSE_OWNERS_RESPONSE OwnerCount: {}", PotentialOwners.size());

    return &_worldPacket;
}

WorldPacket const* HousingSvcsUpdateHouseSettingsResponse::Write()
{
    // IDA case 5505051: uint8(Result) + JamCliHouse
    _worldPacket << uint8(Result);
    WriteJamCliHouse(_worldPacket, House);

    TC_LOG_DEBUG("network.opcode", "SMSG_HOUSING_SVCS_UPDATE_HOUSE_SETTINGS_RESPONSE Result: {} HouseGuid: {}",
        Result, House.HouseGUID.ToString());

    return &_worldPacket;
}

WorldPacket const* HousingSvcsGetHouseFinderInfoResponse::Write()
{
    // IDA case 5505052: uint8(Result) + uint32(count) + JamCliHouseFinderNeighborhood[count]
    _worldPacket << uint8(Result);
    _worldPacket << uint32(Entries.size());
    for (auto const& entry : Entries)
        WriteJamCliHouseFinderNeighborhood(_worldPacket, entry);

    TC_LOG_INFO("housing", "SMSG_HOUSING_SVCS_GET_HOUSE_FINDER_INFO_RESPONSE Result: {} EntryCount: {} PacketSize: {}",
        Result, Entries.size(), _worldPacket.size());
    for (size_t i = 0; i < Entries.size(); ++i)
    {
        auto const& e = Entries[i];
        TC_LOG_INFO("housing", "  LIST_ENTRY[{}]: nbGuid={} houses={} Field1=0x{:016X} Field2={} ExtraFlags=0x{:02X}",
            i, e.NeighborhoodGUID.ToString(), e.Houses.size(), e.Field1, e.Field2, e.ExtraFlags);
    }

    return &_worldPacket;
}

WorldPacket const* HousingSvcsGetHouseFinderNeighborhoodResponse::Write()
{
    // IDA case 5505053: uint8(Result) + ONE JamCliHouseFinderNeighborhood
    _worldPacket << uint8(Result);
    WriteJamCliHouseFinderNeighborhood(_worldPacket, Neighborhood);

    TC_LOG_INFO("housing", "SMSG_HOUSING_SVCS_GET_HOUSE_FINDER_NEIGHBORHOOD_RESPONSE Result: {} Houses: {} PacketSize: {}",
        Result, Neighborhood.Houses.size(), _worldPacket.size());

    // Hex dump of the first 256 bytes for wire format verification
    std::string hexDump;
    size_t dumpLen = std::min<size_t>(_worldPacket.size(), 256);
    for (size_t i = 0; i < dumpLen; ++i)
    {
        char buf[4];
        snprintf(buf, sizeof(buf), "%02X ", _worldPacket[i]);
        hexDump += buf;
        if ((i + 1) % 32 == 0) hexDump += "\n    ";
    }
    TC_LOG_INFO("housing", "  PACKET_HEX (first {} bytes):\n    {}", dumpLen, hexDump);

    return &_worldPacket;
}

WorldPacket const* HousingSvcsGetBnetFriendNeighborhoodsResponse::Write()
{
    // IDA case 5505054: uint8(Result) + uint32(count) + JamCliHouseFinderNeighborhood[count]
    _worldPacket << uint8(Result);
    _worldPacket << uint32(Entries.size());
    for (auto const& entry : Entries)
        WriteJamCliHouseFinderNeighborhood(_worldPacket, entry);

    TC_LOG_DEBUG("network.opcode", "SMSG_HOUSING_SVCS_GET_BNET_FRIEND_NEIGHBORHOODS_RESPONSE Result: {} EntryCount: {}", Result, Entries.size());

    return &_worldPacket;
}

WorldPacket const* HousingSvcsHouseFinderForceRefresh::Write()
{
    TC_LOG_DEBUG("network.opcode", "SMSG_HOUSING_SVCS_HOUSE_FINDER_FORCE_REFRESH (no data)");

    return &_worldPacket;
}

WorldPacket const* HousingSvcRequestPlayerReloadData::Write()
{
    TC_LOG_DEBUG("network.opcode", "SMSG_HOUSING_SVC_REQUEST_PLAYER_RELOAD_DATA (no data)");

    return &_worldPacket;
}

WorldPacket const* HousingSvcsDeleteAllNeighborhoodInvitesResponse::Write()
{
    // IDA case 5505057: uint8 only
    _worldPacket << uint8(Result);

    TC_LOG_DEBUG("network.opcode", "SMSG_HOUSING_SVCS_DELETE_ALL_NEIGHBORHOOD_INVITES_RESPONSE Result: {}", Result);

    return &_worldPacket;
}

WorldPacket const* HousingSvcsSetNeighborhoodSettingsResponse::Write()
{
    // IDA case 5505058: PackedGUID + uint8
    _worldPacket << NeighborhoodGuid;
    _worldPacket << uint8(Result);

    TC_LOG_DEBUG("network.opcode", "SMSG_HOUSING_SVCS_SET_NEIGHBORHOOD_SETTINGS_RESPONSE Neighborhood: {} Result: {}",
        NeighborhoodGuid.ToString(), Result);

    return &_worldPacket;
}

// ============================================================
// Housing General SMSG Responses (0x55xxxx)
// ============================================================

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
    // IDA 0x550000: PackedGUID×4 + uint8(Status) + uint8(FlagByte: bit7/bit6/bit5)
    _worldPacket << HouseGuid;
    _worldPacket << AccountGuid;
    _worldPacket << OwnerPlayerGuid;
    _worldPacket << NeighborhoodGuid;
    _worldPacket << uint8(Status);
    _worldPacket << uint8(FlagByte);

    TC_LOG_DEBUG("network.opcode", "SMSG_HOUSING_HOUSE_STATUS_RESPONSE HouseGuid: {} AccountGuid: {} OwnerPlayerGuid: {} NeighborhoodGuid: {} Status: {} FlagByte: 0x{:02X}",
        HouseGuid.ToString(), AccountGuid.ToString(), OwnerPlayerGuid.ToString(), NeighborhoodGuid.ToString(), Status, FlagByte);

    return &_worldPacket;
}

WorldPacket const* HousingGetCurrentHouseInfoResponse::Write()
{
    _worldPacket << House;
    _worldPacket << uint8(Result);

    TC_LOG_DEBUG("network.opcode", "SMSG_HOUSING_GET_CURRENT_HOUSE_INFO_RESPONSE Result: {} HouseGuid: {}",
        Result, House.HouseGuid.ToString());

    return &_worldPacket;
}

WorldPacket const* HousingExportHouseResponse::Write()
{
    // IDA 0x550003: PackedGUID + uint8(Result) + uint8(bit7=HasExportString)
    //   [+ 24bit-BE(strLen) + string(strLen)] + uint32(blobSize) + blob(blobSize)
    _worldPacket << HouseGuid;
    _worldPacket << uint8(Result);
    _worldPacket << uint8(HasExportString ? 0x80 : 0x00);
    if (HasExportString)
    {
        uint32 strLen = static_cast<uint32>(ExportString.size());
        _worldPacket << uint8((strLen >> 16) & 0xFF);
        _worldPacket << uint8((strLen >> 8) & 0xFF);
        _worldPacket << uint8(strLen & 0xFF);
        _worldPacket.append(ExportString.data(), ExportString.size());
    }
    _worldPacket << uint32(ExportBlob.size());
    if (!ExportBlob.empty())
        _worldPacket.append(ExportBlob.data(), ExportBlob.size());

    TC_LOG_DEBUG("network.opcode", "SMSG_HOUSING_EXPORT_HOUSE_RESPONSE HouseGuid: {} Result: {} HasExportString: {} StringLen: {} BlobLen: {}",
        HouseGuid.ToString(), Result, HasExportString, ExportString.size(), ExportBlob.size());

    return &_worldPacket;
}

WorldPacket const* HousingSystemHouseSnapshotResponse::Write()
{
    _worldPacket << uint8(Result);

    TC_LOG_DEBUG("network.opcode", "SMSG_HOUSING_SYSTEM_HOUSE_SNAPSHOT_RESPONSE Result: {}", Result);

    return &_worldPacket;
}

WorldPacket const* HousingGetPlayerPermissionsResponse::Write()
{
    _worldPacket << HouseGuid;
    _worldPacket << uint8(ResultCode);
    _worldPacket << uint8(PermissionFlags);

    TC_LOG_DEBUG("network.opcode", "SMSG_HOUSING_GET_PLAYER_PERMISSIONS_RESPONSE HouseGuid: {} ResultCode: {} PermissionFlags: {}",
        HouseGuid.ToString(), ResultCode, PermissionFlags);

    return &_worldPacket;
}

WorldPacket const* HousingResetKioskModeResponse::Write()
{
    _worldPacket << uint8(Result);

    TC_LOG_DEBUG("network.opcode", "SMSG_HOUSING_RESET_KIOSK_MODE_RESPONSE Result: {}", Result);

    return &_worldPacket;
}

WorldPacket const* HousingEditorAvailabilityResponse::Write()
{
    _worldPacket << HouseGuid;
    _worldPacket << uint8(Result);
    _worldPacket << Field_09;

    TC_LOG_DEBUG("network.opcode", "SMSG_HOUSING_EDITOR_AVAILABILITY_RESPONSE HouseGuid: {} Result: {} Field_09: {}",
        HouseGuid.ToString(), Result, Field_09);

    return &_worldPacket;
}

WorldPacket const* HousingUpdateHouseInfo::Write()
{
    // IDA 0x550004: 3×24bit-BE(strLen) + 3×uint32 + uint8 + 3×string(strLen)
    auto writeBE24 = [&](uint32 len) {
        _worldPacket << uint8((len >> 16) & 0xFF);
        _worldPacket << uint8((len >> 8) & 0xFF);
        _worldPacket << uint8(len & 0xFF);
    };

    writeBE24(static_cast<uint32>(HouseName.size()));
    writeBE24(static_cast<uint32>(HouseDescription.size()));
    writeBE24(static_cast<uint32>(HouseExtra.size()));

    _worldPacket << uint32(Field1);
    _worldPacket << uint32(Field2);
    _worldPacket << uint32(Field3);
    _worldPacket << uint8(Result);

    _worldPacket.append(HouseName.data(), HouseName.size());
    _worldPacket.append(HouseDescription.data(), HouseDescription.size());
    _worldPacket.append(HouseExtra.data(), HouseExtra.size());

    TC_LOG_DEBUG("network.opcode", "SMSG_HOUSING_UPDATE_HOUSE_INFO Result: {} Name: '{}' Desc: '{}' Extra: '{}' F1: {} F2: {} F3: {}",
        Result, HouseName, HouseDescription, HouseExtra, Field1, Field2, Field3);

    return &_worldPacket;
}

WorldPacket const* HousingSetHouseNameResponse::Write()
{
    // IDA 0x550005: uint8(Result) + 24bit-BE(nameLen) + string(Name)
    _worldPacket << uint8(Result);

    uint32 nameLen = static_cast<uint32>(Name.size());
    _worldPacket << uint8((nameLen >> 16) & 0xFF);
    _worldPacket << uint8((nameLen >> 8) & 0xFF);
    _worldPacket << uint8(nameLen & 0xFF);
    _worldPacket.append(Name.data(), Name.size());

    TC_LOG_DEBUG("network.opcode", "SMSG_HOUSING_SET_HOUSE_NAME_RESPONSE Result: {} Name: '{}'",
        Result, Name);

    return &_worldPacket;
}

// ============================================================
// Account/Licensing SMSG (0x42xxxx / 0x5Fxxxx)
// ============================================================

WorldPacket const* AccountExteriorFixtureCollectionUpdate::Write()
{
    _worldPacket << uint32(FixtureID);

    TC_LOG_DEBUG("network.opcode", "SMSG_ACCOUNT_EXTERIOR_FIXTURE_COLLECTION_UPDATE FixtureID: {}", FixtureID);

    return &_worldPacket;
}

WorldPacket const* AccountHouseTypeCollectionUpdate::Write()
{
    _worldPacket << uint32(HouseTypeID);

    TC_LOG_DEBUG("network.opcode", "SMSG_ACCOUNT_HOUSE_TYPE_COLLECTION_UPDATE HouseTypeID: {}", HouseTypeID);

    return &_worldPacket;
}

WorldPacket const* AccountRoomCollectionUpdate::Write()
{
    _worldPacket << uint32(RoomID);

    TC_LOG_DEBUG("network.opcode", "SMSG_ACCOUNT_ROOM_COLLECTION_UPDATE RoomID: {}", RoomID);

    return &_worldPacket;
}

WorldPacket const* AccountRoomThemeCollectionUpdate::Write()
{
    _worldPacket << uint32(ThemeID);

    TC_LOG_DEBUG("network.opcode", "SMSG_ACCOUNT_ROOM_THEME_COLLECTION_UPDATE ThemeID: {}", ThemeID);

    return &_worldPacket;
}

WorldPacket const* AccountRoomMaterialCollectionUpdate::Write()
{
    _worldPacket << uint32(MaterialID);

    TC_LOG_DEBUG("network.opcode", "SMSG_ACCOUNT_ROOM_MATERIAL_COLLECTION_UPDATE MaterialID: {}", MaterialID);

    return &_worldPacket;
}

WorldPacket const* InvalidateNeighborhood::Write()
{
    _worldPacket << NeighborhoodGuid;

    TC_LOG_DEBUG("network.opcode", "SMSG_INVALIDATE_NEIGHBORHOOD NeighborhoodGuid: {}", NeighborhoodGuid.ToString());

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

    TC_LOG_DEBUG("network.opcode", "SMSG_GET_DECOR_REFUND_LIST_RESPONSE DecorCount: {}", Decors.size());
    for (size_t i = 0; i < Decors.size(); ++i)
        TC_LOG_DEBUG("network.opcode", "  Decor[{}]: ID={} RefundPrice={} ExpiryTime={} Flags={}",
            i, Decors[i].DecorID, Decors[i].RefundPrice, Decors[i].ExpiryTime, Decors[i].Flags);

    return &_worldPacket;
}

WorldPacket const* GetAllLicensedDecorQuantitiesResponse::Write()
{
    _worldPacket << uint32(Quantities.size());
    for (auto const& qty : Quantities)
    {
        _worldPacket << uint32(qty.DecorID);
        _worldPacket << uint32(qty.Quantity);
    }

    TC_LOG_DEBUG("network.opcode", "SMSG_GET_ALL_LICENSED_DECOR_QUANTITIES_RESPONSE QuantityCount: {}", Quantities.size());
    for (size_t i = 0; i < Quantities.size(); ++i)
        TC_LOG_DEBUG("network.opcode", "  Quantity[{}]: DecorID={} Quantity={}", i, Quantities[i].DecorID, Quantities[i].Quantity);

    return &_worldPacket;
}

WorldPacket const* LicensedDecorQuantitiesUpdate::Write()
{
    _worldPacket << uint32(Quantities.size());
    for (auto const& qty : Quantities)
    {
        _worldPacket << uint32(qty.DecorID);
        _worldPacket << uint32(qty.Quantity);
    }

    TC_LOG_DEBUG("network.opcode", "SMSG_LICENSED_DECOR_QUANTITIES_UPDATE QuantityCount: {}", Quantities.size());
    for (size_t i = 0; i < Quantities.size(); ++i)
        TC_LOG_DEBUG("network.opcode", "  Quantity[{}]: DecorID={} Quantity={}", i, Quantities[i].DecorID, Quantities[i].Quantity);

    return &_worldPacket;
}

// ============================================================
// Initiative System SMSG Responses (0x4203xx)
// ============================================================

WorldPacket const* InitiativeServiceStatus::Write()
{
    _worldPacket.WriteBit(ServiceEnabled);
    _worldPacket.FlushBits();

    TC_LOG_DEBUG("network.opcode", "SMSG_INITIATIVE_SERVICE_STATUS ServiceEnabled: {}", ServiceEnabled);

    return &_worldPacket;
}

WorldPacket const* GetPlayerInitiativeInfoResult::Write()
{
    _worldPacket << NeighborhoodGUID;
    _worldPacket.WriteBit(HasError);
    _worldPacket.WriteBit(HasInitiativeData);
    _worldPacket.FlushBits();

    if (HasInitiativeData)
    {
        _worldPacket << int64(RemainingDuration);
        _worldPacket << int32(CurrentInitiativeID);
        _worldPacket << int32(CurrentMilestoneID);
        _worldPacket << int32(CurrentCycleID);
        _worldPacket << float(ProgressRequired);
        _worldPacket << float(CurrentProgress);
        _worldPacket << float(PlayerTotalContribution);
    }

    _worldPacket << uint32(Tasks.size());
    for (auto const& task : Tasks)
    {
        _worldPacket << uint32(task.TaskID);
        _worldPacket << uint32(task.Progress);
        _worldPacket << uint32(task.Status);
    }

    TC_LOG_DEBUG("network.opcode", "SMSG_GET_PLAYER_INITIATIVE_INFO_RESULT NH={} Error={} HasData={} InitID={} CycleID={} Progress={:.1f}/{:.0f} Tasks={}",
        NeighborhoodGUID.ToString(), HasError, HasInitiativeData,
        CurrentInitiativeID, CurrentCycleID, CurrentProgress, ProgressRequired, Tasks.size());

    return &_worldPacket;
}

WorldPacket const* GetInitiativeActivityLogResult::Write()
{
    _worldPacket << uint8(Result);
    _worldPacket << uint32(CompletedTasks.size());
    for (auto const& entry : CompletedTasks)
    {
        _worldPacket << uint32(entry.InitiativeID);
        _worldPacket << uint32(entry.TaskID);
        _worldPacket << uint32(entry.CycleID);
        _worldPacket << uint64(entry.CompletionTime);
        _worldPacket << entry.PlayerGuid;
        _worldPacket << uint32(entry.ContributionAmount);
        _worldPacket << uint32(entry.Unknown1);
        _worldPacket << uint64(entry.ExtraData);
    }

    TC_LOG_DEBUG("network.opcode", "SMSG_GET_INITIATIVE_ACTIVITY_LOG_RESULT Result: {} CompletedTaskCount: {}", Result, CompletedTasks.size());
    for (size_t i = 0; i < CompletedTasks.size(); ++i)
        TC_LOG_DEBUG("network.opcode", "  CompletedTask[{}]: InitiativeID={} TaskID={} CycleID={} Player={}",
            i, CompletedTasks[i].InitiativeID, CompletedTasks[i].TaskID, CompletedTasks[i].CycleID, CompletedTasks[i].PlayerGuid.ToString());

    return &_worldPacket;
}

WorldPacket const* InitiativeTaskComplete::Write()
{
    _worldPacket << uint32(InitiativeID);
    _worldPacket << uint32(TaskID);

    TC_LOG_DEBUG("network.opcode", "SMSG_INITIATIVE_TASK_COMPLETE InitiativeID: {} TaskID: {}", InitiativeID, TaskID);

    return &_worldPacket;
}

WorldPacket const* InitiativeComplete::Write()
{
    _worldPacket << uint32(InitiativeID);

    TC_LOG_DEBUG("network.opcode", "SMSG_INITIATIVE_COMPLETE InitiativeID: {}", InitiativeID);

    return &_worldPacket;
}

WorldPacket const* ClearInitiativeTaskCriteriaProgress::Write()
{
    _worldPacket << uint32(InitiativeID);
    _worldPacket << uint32(TaskID);

    TC_LOG_DEBUG("network.opcode", "SMSG_CLEAR_INITIATIVE_TASK_CRITERIA_PROGRESS InitiativeID: {} TaskID: {}", InitiativeID, TaskID);

    return &_worldPacket;
}

WorldPacket const* GetInitiativeRewardsResult::Write()
{
    _worldPacket << uint8(Result);

    TC_LOG_DEBUG("network.opcode", "SMSG_GET_INITIATIVE_REWARDS_RESULT Result: {}", Result);

    return &_worldPacket;
}

WorldPacket const* InitiativeRewardAvailable::Write()
{
    _worldPacket << uint32(InitiativeID);
    _worldPacket << uint32(MilestoneIndex);

    TC_LOG_DEBUG("network.opcode", "SMSG_INITIATIVE_REWARD_AVAILABLE InitiativeID: {} MilestoneIndex: {}", InitiativeID, MilestoneIndex);

    return &_worldPacket;
}

WorldPacket const* InitiativeUpdateStatus::Write()
{
    _worldPacket << uint8(Status);
    return &_worldPacket;
}

WorldPacket const* InitiativePointsUpdate::Write()
{
    _worldPacket << uint32(CurrentPoints);
    _worldPacket << uint32(MaxPoints);
    return &_worldPacket;
}

WorldPacket const* InitiativeMilestoneUpdate::Write()
{
    _worldPacket << uint8(MilestoneIndex);
    _worldPacket << uint8(Reached);
    _worldPacket << uint8(Flags);
    return &_worldPacket;
}

WorldPacket const* InitiativeChestResult::Write()
{
    _worldPacket << uint32(Result);
    return &_worldPacket;
}

WorldPacket const* InitiativeTrackedUpdated::Write()
{
    _worldPacket << NeighborhoodGUID;
    return &_worldPacket;
}

WorldPacket const* HousingPhotoSharingAuthorizationResult::Write()
{
    _worldPacket << uint8(Result);

    TC_LOG_DEBUG("network.opcode", "SMSG_HOUSING_PHOTO_SHARING_AUTHORIZATION_RESULT Result: {}", Result);

    return &_worldPacket;
}

WorldPacket const* HousingPhotoSharingAuthorizationClearedResult::Write()
{
    _worldPacket << uint8(Result);

    TC_LOG_DEBUG("network.opcode", "SMSG_HOUSING_PHOTO_SHARING_AUTHORIZATION_CLEARED_RESULT Result: {}", Result);

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

    TC_LOG_DEBUG("network.opcode", "CMSG_NEIGHBORHOOD_CHARTER_CREATE MapID: {} FactionFlags: {} Name: '{}'", NeighborhoodMapID, FactionFlags, Name);
}

void NeighborhoodCharterEdit::Read()
{
    _worldPacket >> NeighborhoodMapID;
    _worldPacket >> FactionFlags;
    _worldPacket >> SizedString::BitsSize<7>(Name);

    _worldPacket >> SizedString::Data(Name);

    TC_LOG_DEBUG("network.opcode", "CMSG_NEIGHBORHOOD_CHARTER_EDIT MapID: {} FactionFlags: {} Name: '{}'", NeighborhoodMapID, FactionFlags, Name);
}

void NeighborhoodCharterAddSignature::Read()
{
    _worldPacket >> CharterGuid;

    TC_LOG_DEBUG("network.opcode", "CMSG_NEIGHBORHOOD_CHARTER_ADD_SIGNATURE CharterGuid: {}", CharterGuid.ToString());
}

void NeighborhoodCharterSendSignatureRequest::Read()
{
    _worldPacket >> TargetPlayerGuid;

    TC_LOG_DEBUG("network.opcode", "CMSG_NEIGHBORHOOD_CHARTER_SEND_SIGNATURE_REQUEST TargetPlayerGuid: {}", TargetPlayerGuid.ToString());
}

void NeighborhoodCharterSignResponsePacket::Read()
{
    _worldPacket >> CharterGuid;

    TC_LOG_DEBUG("network.opcode", "CMSG_NEIGHBORHOOD_CHARTER_SIGN_RESPONSE CharterGuid: {}", CharterGuid.ToString());
}

void NeighborhoodCharterRemoveSignature::Read()
{
    _worldPacket >> CharterGuid;
    _worldPacket >> SignerGuid;

    TC_LOG_DEBUG("network.opcode", "CMSG_NEIGHBORHOOD_CHARTER_REMOVE_SIGNATURE CharterGuid: {} SignerGuid: {}",
        CharterGuid.ToString(), SignerGuid.ToString());
}

// --- Neighborhood Management System ---

void NeighborhoodUpdateName::Read()
{
    _worldPacket >> SizedString::BitsSize<7>(NewName);

    _worldPacket >> SizedString::Data(NewName);

    TC_LOG_DEBUG("network.opcode", "CMSG_NEIGHBORHOOD_UPDATE_NAME NewName: '{}'", NewName);
}

void NeighborhoodSetPublicFlag::Read()
{
    _worldPacket >> NeighborhoodGuid;
    _worldPacket >> Bits<1>(IsPublic);

    TC_LOG_DEBUG("network.opcode", "CMSG_NEIGHBORHOOD_SET_PUBLIC_FLAG NeighborhoodGuid: {} IsPublic: {}", NeighborhoodGuid.ToString(), IsPublic);
}

void NeighborhoodAddSecondaryOwner::Read()
{
    _worldPacket >> PlayerGuid;

    TC_LOG_DEBUG("network.opcode", "CMSG_NEIGHBORHOOD_ADD_SECONDARY_OWNER PlayerGuid: {}", PlayerGuid.ToString());
}

void NeighborhoodRemoveSecondaryOwner::Read()
{
    _worldPacket >> PlayerGuid;

    TC_LOG_DEBUG("network.opcode", "CMSG_NEIGHBORHOOD_REMOVE_SECONDARY_OWNER PlayerGuid: {}", PlayerGuid.ToString());
}

void NeighborhoodInviteResident::Read()
{
    _worldPacket >> PlayerGuid;

    TC_LOG_DEBUG("network.opcode", "CMSG_NEIGHBORHOOD_INVITE_RESIDENT PlayerGuid: {}", PlayerGuid.ToString());
}

void NeighborhoodCancelInvitation::Read()
{
    _worldPacket >> InviteeGuid;

    TC_LOG_DEBUG("network.opcode", "CMSG_NEIGHBORHOOD_CANCEL_INVITATION InviteeGuid: {}", InviteeGuid.ToString());
}

void NeighborhoodPlayerDeclineInvite::Read()
{
    _worldPacket >> NeighborhoodGuid;

    TC_LOG_DEBUG("network.opcode", "CMSG_NEIGHBORHOOD_PLAYER_DECLINE_INVITE NeighborhoodGuid: {}", NeighborhoodGuid.ToString());
}

void NeighborhoodBuyHouse::Read()
{
    // Sniff 12.0.1 (23 bytes): uint32(HouseStyleID) + PackedGUID(CornerstoneGuid) + uint16(Padding)
    _worldPacket >> HouseStyleID;
    _worldPacket >> CornerstoneGuid;
    _worldPacket >> Padding;

    TC_LOG_DEBUG("network.opcode", "CMSG_NEIGHBORHOOD_BUY_HOUSE HouseStyleID: {} CornerstoneGuid: {} Padding: {}",
        HouseStyleID, CornerstoneGuid.ToString(), Padding);
}

void NeighborhoodMoveHouse::Read()
{
    _worldPacket >> NeighborhoodGuid;
    _worldPacket >> PlotGuid;

    TC_LOG_DEBUG("network.opcode", "CMSG_NEIGHBORHOOD_MOVE_HOUSE NeighborhoodGuid: {} PlotGuid: {}", NeighborhoodGuid.ToString(), PlotGuid.ToString());
}

void NeighborhoodOpenCornerstoneUI::Read()
{
    _worldPacket >> PlotIndex;
    _worldPacket >> NeighborhoodGuid;

    TC_LOG_DEBUG("network.opcode", "CMSG_NEIGHBORHOOD_OPEN_CORNERSTONE_UI PlotIndex: {} NeighborhoodGuid: {}", PlotIndex, NeighborhoodGuid.ToString());
}

void NeighborhoodOfferOwnership::Read()
{
    _worldPacket >> NewOwnerGuid;

    TC_LOG_DEBUG("network.opcode", "CMSG_NEIGHBORHOOD_OFFER_OWNERSHIP NewOwnerGuid: {}", NewOwnerGuid.ToString());
}

void NeighborhoodGetRoster::Read()
{
    _worldPacket >> NeighborhoodGuid;

    TC_LOG_DEBUG("network.opcode", "CMSG_NEIGHBORHOOD_GET_ROSTER NeighborhoodGuid: {}", NeighborhoodGuid.ToString());
}

void NeighborhoodEvictPlot::Read()
{
    _worldPacket >> PlotIndex;
    _worldPacket >> NeighborhoodGuid;

    TC_LOG_DEBUG("network.opcode", "CMSG_NEIGHBORHOOD_EVICT_PLOT PlotIndex: {} NeighborhoodGuid: {}", PlotIndex, NeighborhoodGuid.ToString());
}

void NeighborhoodCancelInvitationAlt::Read()
{
    _worldPacket >> InviteeGuid;

    TC_LOG_DEBUG("network.opcode", "CMSG_NEIGHBORHOOD_CANCEL_INVITATION_ALT InviteeGuid: {}", InviteeGuid.ToString());
}

void NeighborhoodInviteNotificationAck::Read()
{
    _worldPacket >> NeighborhoodGuid;

    TC_LOG_DEBUG("network.opcode", "CMSG_NEIGHBORHOOD_INVITE_NOTIFICATION_ACK NeighborhoodGuid: {}", NeighborhoodGuid.ToString());
}

void NeighborhoodOfferOwnershipResponsePacket::Read()
{
    _worldPacket >> NeighborhoodGuid;
    _worldPacket >> Bits<1>(Accept);

    TC_LOG_DEBUG("network.opcode", "CMSG_NEIGHBORHOOD_OFFER_OWNERSHIP_RESPONSE NeighborhoodGuid: {} Accept: {}",
        NeighborhoodGuid.ToString(), Accept);
}

// ============================================================
// Neighborhood Charter SMSG Responses (0x5Bxxxx)
// ============================================================

WorldPacket const* NeighborhoodCharterUpdateResponse::Write()
{
    // IDA 0x5B0000: uint8 + PackedGUID + uint32 + uint32 + uint32(arraySize) + uint32 + PackedGUID[arraySize] + uint8(nameLen) + string
    _worldPacket << uint8(Result);
    _worldPacket << CharterGuid;
    _worldPacket << uint32(MapID);
    _worldPacket << uint32(SignatureCount);
    _worldPacket << uint32(Signers.size());
    _worldPacket << uint32(Unknown);
    for (ObjectGuid const& signer : Signers)
        _worldPacket << signer;
    _worldPacket << uint8(NeighborhoodName.size());
    _worldPacket.WriteString(NeighborhoodName);

    TC_LOG_DEBUG("network.opcode", "SMSG_NEIGHBORHOOD_CHARTER_UPDATE_RESPONSE Result: {} CharterGuid: {} MapID: {} SigCount: {} Signers: {} Name: '{}'",
        Result, CharterGuid.ToString(), MapID, SignatureCount, Signers.size(), NeighborhoodName);

    return &_worldPacket;
}

WorldPacket const* NeighborhoodCharterOpenUIResponse::Write()
{
    // IDA 0x5B0001: identical wire format to 0x5B0000
    _worldPacket << uint8(Result);
    _worldPacket << CharterGuid;
    _worldPacket << uint32(MapID);
    _worldPacket << uint32(SignatureCount);
    _worldPacket << uint32(Signers.size());
    _worldPacket << uint32(Unknown);
    for (ObjectGuid const& signer : Signers)
        _worldPacket << signer;
    _worldPacket << uint8(NeighborhoodName.size());
    _worldPacket.WriteString(NeighborhoodName);

    TC_LOG_DEBUG("network.opcode", "SMSG_NEIGHBORHOOD_CHARTER_OPEN_UI_RESPONSE Result: {} CharterGuid: {} MapID: {} SigCount: {} Signers: {} Name: '{}'",
        Result, CharterGuid.ToString(), MapID, SignatureCount, Signers.size(), NeighborhoodName);

    return &_worldPacket;
}

WorldPacket const* NeighborhoodCharterSignRequest::Write()
{
    // IDA 0x5B0002: uint8 + PackedGUID + uint32 + uint32 + uint8(nameLen) + string
    _worldPacket << uint8(Result);
    _worldPacket << CharterGuid;
    _worldPacket << uint32(MapID);
    _worldPacket << uint32(Unknown);
    _worldPacket << uint8(NeighborhoodName.size());
    _worldPacket.WriteString(NeighborhoodName);

    TC_LOG_DEBUG("network.opcode", "SMSG_NEIGHBORHOOD_CHARTER_SIGN_REQUEST Result: {} CharterGuid: {} MapID: {} Name: '{}'",
        Result, CharterGuid.ToString(), MapID, NeighborhoodName);

    return &_worldPacket;
}

WorldPacket const* NeighborhoodCharterAddSignatureResponse::Write()
{
    // IDA 0x5B0003: uint8 + PackedGUID only
    _worldPacket << uint8(Result);
    _worldPacket << CharterGuid;

    TC_LOG_DEBUG("network.opcode", "SMSG_NEIGHBORHOOD_CHARTER_ADD_SIGNATURE_RESPONSE Result: {} CharterGuid: {}",
        Result, CharterGuid.ToString());

    return &_worldPacket;
}

WorldPacket const* NeighborhoodCharterOpenConfirmationUIResponse::Write()
{
    // IDA 0x5B0004: uint8 + uint32 + uint32 + uint8(nameLen) + string
    _worldPacket << uint8(Result);
    _worldPacket << uint32(Field1);
    _worldPacket << uint32(Field2);
    _worldPacket << uint8(NeighborhoodName.size());
    _worldPacket.WriteString(NeighborhoodName);

    TC_LOG_DEBUG("network.opcode", "SMSG_NEIGHBORHOOD_CHARTER_OPEN_CONFIRMATION_UI_RESPONSE Result: {} Field1: {} Field2: {} Name: '{}'",
        Result, Field1, Field2, NeighborhoodName);

    return &_worldPacket;
}

WorldPacket const* NeighborhoodCharterSignatureRemovedNotification::Write()
{
    // IDA 0x5B0005: PackedGUID only
    _worldPacket << CharterGuid;

    TC_LOG_DEBUG("network.opcode", "SMSG_NEIGHBORHOOD_CHARTER_SIGNATURE_REMOVED CharterGuid: {}",
        CharterGuid.ToString());

    return &_worldPacket;
}

// ============================================================
// Neighborhood Management SMSG Responses (0x5Cxxxx)
// ============================================================

WorldPacket const* NeighborhoodPlayerEnterPlot::Write()
{
    _worldPacket << PlotAreaTriggerGuid;

    TC_LOG_DEBUG("network.opcode", "SMSG_NEIGHBORHOOD_PLAYER_ENTER_PLOT PlotAreaTriggerGuid: {}", PlotAreaTriggerGuid.ToString());

    return &_worldPacket;
}

WorldPacket const* NeighborhoodPlayerLeavePlot::Write()
{
    TC_LOG_DEBUG("network.opcode", "SMSG_NEIGHBORHOOD_PLAYER_LEAVE_PLOT (no data)");

    return &_worldPacket;
}

WorldPacket const* NeighborhoodEvictPlayerResponse::Write()
{
    _worldPacket << PlayerGuid;

    TC_LOG_DEBUG("network.opcode", "SMSG_NEIGHBORHOOD_EVICT_PLAYER_RESPONSE PlayerGuid: {}", PlayerGuid.ToString());

    return &_worldPacket;
}

WorldPacket const* NeighborhoodUpdateNameResponse::Write()
{
    _worldPacket << uint8(Result);

    TC_LOG_DEBUG("network.opcode", "SMSG_NEIGHBORHOOD_UPDATE_NAME_RESPONSE Result: {}", Result);

    return &_worldPacket;
}

WorldPacket const* NeighborhoodUpdateNameNotification::Write()
{
    uint8 nameLen = NewName.empty() ? 0 : static_cast<uint8>(NewName.size() + 1);
    _worldPacket << uint8(nameLen);
    if (nameLen > 0)
        _worldPacket.append(reinterpret_cast<uint8 const*>(NewName.c_str()), nameLen);

    TC_LOG_DEBUG("network.opcode", "SMSG_NEIGHBORHOOD_UPDATE_NAME_NOTIFICATION NewName: '{}' NameLen: {}", NewName, nameLen);

    return &_worldPacket;
}

WorldPacket const* NeighborhoodAddSecondaryOwnerResponse::Write()
{
    _worldPacket << PlayerGuid;
    _worldPacket << uint8(Result);

    TC_LOG_DEBUG("network.opcode", "SMSG_NEIGHBORHOOD_ADD_SECONDARY_OWNER_RESPONSE PlayerGuid: {} Result: {}",
        PlayerGuid.ToString(), Result);

    return &_worldPacket;
}

WorldPacket const* NeighborhoodRemoveSecondaryOwnerResponse::Write()
{
    _worldPacket << PlayerGuid;
    _worldPacket << uint8(Result);

    TC_LOG_DEBUG("network.opcode", "SMSG_NEIGHBORHOOD_REMOVE_SECONDARY_OWNER_RESPONSE PlayerGuid: {} Result: {}",
        PlayerGuid.ToString(), Result);

    return &_worldPacket;
}

WorldPacket const* NeighborhoodBuyHouseResponse::Write()
{
    _worldPacket << House;
    _worldPacket << uint8(Result);

    TC_LOG_DEBUG("network.opcode", "SMSG_NEIGHBORHOOD_BUY_HOUSE_RESPONSE Result: {} HouseGuid: {}", Result, House.HouseGuid.ToString());

    return &_worldPacket;
}

WorldPacket const* NeighborhoodMoveHouseResponse::Write()
{
    _worldPacket << House;
    _worldPacket << MoveTransactionGuid;
    _worldPacket << uint8(Result);

    TC_LOG_DEBUG("network.opcode", "SMSG_NEIGHBORHOOD_MOVE_HOUSE_RESPONSE Result: {} MoveTransactionGuid: {}",
        Result, MoveTransactionGuid.ToString());

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

    TC_LOG_DEBUG("network.opcode", "SMSG_NEIGHBORHOOD_OPEN_CORNERSTONE_UI_RESPONSE PlotIndex: {} Cost: {} PurchaseStatus: {} IsPlotOwned: {} CanPurchase: {} Name: '{}'",
        PlotIndex, Cost, PurchaseStatus, IsPlotOwned, CanPurchase, NeighborhoodName);

    return &_worldPacket;
}

WorldPacket const* NeighborhoodInviteResidentResponse::Write()
{
    _worldPacket << uint8(Result);
    _worldPacket << InviteeGuid;

    TC_LOG_DEBUG("network.opcode", "SMSG_NEIGHBORHOOD_INVITE_RESIDENT_RESPONSE Result: {} InviteeGuid: {}", Result, InviteeGuid.ToString());

    return &_worldPacket;
}

WorldPacket const* NeighborhoodCancelInvitationResponse::Write()
{
    _worldPacket << uint8(Result);
    _worldPacket << InviteeGuid;

    TC_LOG_DEBUG("network.opcode", "SMSG_NEIGHBORHOOD_CANCEL_INVITATION_RESPONSE Result: {} InviteeGuid: {}", Result, InviteeGuid.ToString());

    return &_worldPacket;
}

WorldPacket const* NeighborhoodDeclineInvitationResponse::Write()
{
    _worldPacket << uint8(Result);
    _worldPacket << NeighborhoodGuid;

    TC_LOG_DEBUG("network.opcode", "SMSG_NEIGHBORHOOD_DECLINE_INVITATION_RESPONSE Result: {} NeighborhoodGuid: {}",
        Result, NeighborhoodGuid.ToString());

    return &_worldPacket;
}

WorldPacket const* NeighborhoodPlayerGetInviteResponse::Write()
{
    _worldPacket << uint8(Result);
    Housing::WriteJamNeighborhoodRosterEntry(_worldPacket, Entry);

    TC_LOG_DEBUG("network.opcode", "SMSG_NEIGHBORHOOD_PLAYER_GET_INVITE_RESPONSE Result: {} PlayerGuid: {}",
        Result, Entry.PlayerGuid.ToString());

    return &_worldPacket;
}

WorldPacket const* NeighborhoodGetInvitesResponse::Write()
{
    _worldPacket << uint8(Result);
    _worldPacket << uint32(Invites.size());
    for (auto const& invite : Invites)
        Housing::WriteJamNeighborhoodRosterEntry(_worldPacket, invite);

    TC_LOG_DEBUG("network.opcode", "SMSG_NEIGHBORHOOD_GET_INVITES_RESPONSE Result: {} InviteCount: {}", Result, Invites.size());

    return &_worldPacket;
}

WorldPacket const* NeighborhoodInviteNotification::Write()
{
    _worldPacket << NeighborhoodGuid;

    TC_LOG_DEBUG("network.opcode", "SMSG_NEIGHBORHOOD_INVITE_NOTIFICATION NeighborhoodGuid: {}", NeighborhoodGuid.ToString());

    return &_worldPacket;
}

WorldPacket const* NeighborhoodOfferOwnershipResponse::Write()
{
    _worldPacket << uint8(Result);

    TC_LOG_DEBUG("network.opcode", "SMSG_NEIGHBORHOOD_OFFER_OWNERSHIP_RESPONSE Result: {}", Result);

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

    TC_LOG_DEBUG("network.opcode", "SMSG_NEIGHBORHOOD_GET_ROSTER_RESPONSE Result: {} MemberCount: {} NeighborhoodGuid: {} Name: '{}'",
        Result, Members.size(), GroupNeighborhoodGuid.ToString(), NeighborhoodName);
    for (size_t i = 0; i < Members.size(); ++i)
        TC_LOG_DEBUG("network.opcode", "  Member[{}]: PlayerGuid={} HouseGuid={} PlotIndex={} Online={}",
            i, Members[i].PlayerGuid.ToString(), Members[i].HouseGuid.ToString(), Members[i].PlotIndex, Members[i].IsOnline);

    return &_worldPacket;
}

WorldPacket const* NeighborhoodRosterResidentUpdate::Write()
{
    _worldPacket << uint32(Residents.size());
    for (auto const& resident : Residents)
    {
        _worldPacket << resident.PlayerGuid;
        _worldPacket << uint8(resident.UpdateType);
        // IDA: client deserializer does v6 >> 7 — only bit 7 is kept as bool
        _worldPacket << uint8(resident.IsPrivileged ? 0x80 : 0x00);
    }

    TC_LOG_DEBUG("network.opcode", "SMSG_NEIGHBORHOOD_ROSTER_RESIDENT_UPDATE ResidentCount: {}", Residents.size());

    return &_worldPacket;
}

WorldPacket const* NeighborhoodInviteNameLookupResult::Write()
{
    _worldPacket << uint8(Result);
    _worldPacket << PlayerGuid;

    TC_LOG_DEBUG("network.opcode", "SMSG_NEIGHBORHOOD_INVITE_NAME_LOOKUP_RESULT Result: {} PlayerGuid: {}", Result, PlayerGuid.ToString());

    return &_worldPacket;
}

WorldPacket const* NeighborhoodEvictPlotResponse::Write()
{
    _worldPacket << uint8(Result);
    _worldPacket << NeighborhoodGuid;

    TC_LOG_DEBUG("network.opcode", "SMSG_NEIGHBORHOOD_EVICT_PLOT_RESPONSE Result: {} NeighborhoodGuid: {}", Result, NeighborhoodGuid.ToString());

    return &_worldPacket;
}

WorldPacket const* NeighborhoodEvictPlotNotice::Write()
{
    _worldPacket << uint32(PlotId);
    _worldPacket << NeighborhoodGuid;
    _worldPacket << PlotGuid;

    TC_LOG_DEBUG("network.opcode", "SMSG_NEIGHBORHOOD_EVICT_PLOT_NOTICE PlotId: {} NeighborhoodGuid: {} PlotGuid: {}",
        PlotId, NeighborhoodGuid.ToString(), PlotGuid.ToString());

    return &_worldPacket;
}

// --- Initiative System ---

void GetAvailableInitiativeRequest::Read()
{
    _worldPacket >> NeighborhoodGuid;

    TC_LOG_DEBUG("network.opcode", "CMSG_GET_AVAILABLE_INITIATIVE_REQUEST NeighborhoodGuid: {}", NeighborhoodGuid.ToString());
}

void GetInitiativeActivityLogRequest::Read()
{
    _worldPacket >> NeighborhoodGuid;

    TC_LOG_DEBUG("network.opcode", "CMSG_GET_INITIATIVE_ACTIVITY_LOG_REQUEST NeighborhoodGuid: {}", NeighborhoodGuid.ToString());
}

void InitiativeUpdateActiveNeighborhood::Read()
{
    _worldPacket >> NeighborhoodGuid;

    TC_LOG_DEBUG("network.opcode", "CMSG_INITIATIVE_UPDATE_ACTIVE_NEIGHBORHOOD NeighborhoodGuid: {}", NeighborhoodGuid.ToString());
}

void InitiativeAcceptMilestoneRequest::Read()
{
    _worldPacket >> NeighborhoodGuid;
    _worldPacket >> InitiativeID;
    _worldPacket >> MilestoneIndex;

    TC_LOG_DEBUG("network.opcode", "CMSG_INITIATIVE_ACCEPT_MILESTONE_REQUEST NeighborhoodGuid: {} InitiativeID: {} MilestoneIndex: {}",
        NeighborhoodGuid.ToString(), InitiativeID, MilestoneIndex);
}

void InitiativeReportProgress::Read()
{
    _worldPacket >> NeighborhoodGuid;

    TC_LOG_DEBUG("network.opcode", "CMSG_INITIATIVE_REPORT_PROGRESS NeighborhoodGuid: {}",
        NeighborhoodGuid.ToString());
}

void GetInitiativeClaimRewardRequest::Read()
{
    _worldPacket >> NeighborhoodGuid;
    _worldPacket >> InitiativeID;
    _worldPacket >> MilestoneIndex;

    TC_LOG_DEBUG("network.opcode", "CMSG_GET_INITIATIVE_CLAIM_REWARD_REQUEST NeighborhoodGuid: {} InitiativeID: {} MilestoneIndex: {}",
        NeighborhoodGuid.ToString(), InitiativeID, MilestoneIndex);
}

void GetInitiativeLeaderboardRequest::Read()
{
    _worldPacket >> NeighborhoodGuid;
    _worldPacket >> InitiativeID;

    TC_LOG_DEBUG("network.opcode", "CMSG_GET_INITIATIVE_LEADERBOARD_REQUEST NeighborhoodGuid: {} InitiativeID: {}",
        NeighborhoodGuid.ToString(), InitiativeID);
}

void GetInitiativeOpenChestRequest::Read()
{
    _worldPacket >> NeighborhoodGuid;
    _worldPacket >> InitiativeID;

    TC_LOG_DEBUG("network.opcode", "CMSG_GET_INITIATIVE_OPEN_CHEST_REQUEST NeighborhoodGuid: {} InitiativeID: {}",
        NeighborhoodGuid.ToString(), InitiativeID);
}

void GetInitiativeTaskAcceptRequest::Read()
{
    _worldPacket >> NeighborhoodGuid;
    _worldPacket >> TaskID;

    TC_LOG_DEBUG("network.opcode", "CMSG_GET_INITIATIVE_TASK_ACCEPT_REQUEST NeighborhoodGuid: {} TaskID: {}",
        NeighborhoodGuid.ToString(), TaskID);
}

void GetInitiativeTaskAbandonRequest::Read()
{
    _worldPacket >> NeighborhoodGuid;
    _worldPacket >> TaskID;

    TC_LOG_DEBUG("network.opcode", "CMSG_GET_INITIATIVE_TASK_ABANDON_REQUEST NeighborhoodGuid: {} TaskID: {}",
        NeighborhoodGuid.ToString(), TaskID);
}

void GetInitiativeTaskProgressRequest::Read()
{
    _worldPacket >> NeighborhoodGuid;
    _worldPacket >> TaskID;

    TC_LOG_DEBUG("network.opcode", "CMSG_GET_INITIATIVE_TASK_PROGRESS_REQUEST NeighborhoodGuid: {} TaskID: {}",
        NeighborhoodGuid.ToString(), TaskID);
}

} // namespace WorldPackets::Neighborhood
