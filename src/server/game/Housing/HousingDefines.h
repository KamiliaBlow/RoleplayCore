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

#ifndef TRINITYCORE_HOUSING_DEFINES_H
#define TRINITYCORE_HOUSING_DEFINES_H

#include "Define.h"
#include "Duration.h"

// Enum.HousingResult client values, sent as uint8
enum HousingResult : uint8
{
    HOUSING_RESULT_SUCCESS                                   = 0,
    HOUSING_RESULT_ACCOUNT_BANNED                            = 1,
    HOUSING_RESULT_ACTION_LOCKED_BY_COMBAT                   = 2,
    HOUSING_RESULT_BLUEPRINT_CODE_INVALID                    = 3,
    HOUSING_RESULT_BLUEPRINT_DYE_FAILED                      = 4,
    HOUSING_RESULT_BLUEPRINT_GENERIC_EXPORT_ERROR            = 5,
    HOUSING_RESULT_BLUEPRINT_GENERIC_IMPORT_ERROR            = 6,
    HOUSING_RESULT_BLUEPRINT_LOCATION_INVALID                = 7,
    HOUSING_RESULT_BLUEPRINT_NAME_INVALID                    = 8,
    HOUSING_RESULT_BLUEPRINT_NOT_FOUND                       = 9,
    HOUSING_RESULT_BLUEPRINT_REQUIREMENTS_UNMET              = 10,
    HOUSING_RESULT_BLUEPRINT_ROOM_PLACEMENT_REQUIRED         = 11,
    HOUSING_RESULT_BLUEPRINT_TYPE_INVALID                    = 12,
    HOUSING_RESULT_BLUEPRINT_TYPE_LOCATION_INVALID           = 13,
    HOUSING_RESULT_BLUEPRINT_STORAGE_LIMIT                   = 14,
    HOUSING_RESULT_BLUEPRINT_VERSION_INVALID                 = 15,
    HOUSING_RESULT_BOUNDS_FAILURE_CHILDREN                   = 16,
    HOUSING_RESULT_BOUNDS_FAILURE_PLOT                       = 17,
    HOUSING_RESULT_BOUNDS_FAILURE_ROOM                       = 18,
    HOUSING_RESULT_BOUND_TO_STARTING_AREA                    = 19,
    HOUSING_RESULT_CANNOT_AFFORD                             = 20,
    HOUSING_RESULT_CHARTER_COMPLETE                          = 21,
    HOUSING_RESULT_COLLISION_INVALID                         = 22,
    HOUSING_RESULT_DB_ERROR                                  = 23,
    HOUSING_RESULT_DECOR_CANNOT_BE_REDEEMED                  = 24,
    HOUSING_RESULT_DECOR_ITEM_NOT_DESTROYABLE                = 25,
    HOUSING_RESULT_DECOR_NOT_FOUND                           = 26,
    HOUSING_RESULT_DECOR_NOT_FOUND_IN_STORAGE                = 27,
    HOUSING_RESULT_DUPLICATE_CHARTER_SIGNATURE               = 28,
    HOUSING_RESULT_FILTER_REJECTED                           = 29,
    HOUSING_RESULT_FIXTURE_CANT_DELETE_DOOR                  = 30,
    HOUSING_RESULT_FIXTURE_HOOK_EMPTY                        = 31,
    HOUSING_RESULT_FIXTURE_HOOK_OCCUPIED                     = 32,
    HOUSING_RESULT_FIXTURE_HOUSE_TYPE_MISMATCH               = 33,
    HOUSING_RESULT_FIXTURE_NOT_FOUND                         = 34,
    HOUSING_RESULT_FIXTURE_SIZE_MISMATCH                     = 35,
    HOUSING_RESULT_FIXTURE_TYPE_MISMATCH                     = 36,
    HOUSING_RESULT_GENERIC_FAILURE                           = 37,
    HOUSING_RESULT_GUILD_MORE_ACCOUNTS_NEEDED                = 38,
    HOUSING_RESULT_GUILD_MORE_ACTIVE_PLAYERS_NEEDED          = 39,
    HOUSING_RESULT_GUILD_NOT_LOADED                          = 40,
    HOUSING_RESULT_HOUSE_EDIT_LOCK_FAILED                    = 41,
    HOUSING_RESULT_HOUSE_EXTERIOR_ALREADY_THAT_SIZE          = 42,
    HOUSING_RESULT_HOUSE_EXTERIOR_ALREADY_THAT_TYPE          = 43,
    HOUSING_RESULT_HOUSE_EXTERIOR_ROOT_NOT_FOUND             = 44,
    HOUSING_RESULT_HOUSE_EXTERIOR_TYPE_NEIGHBORHOOD_MISMATCH = 45,
    HOUSING_RESULT_HOUSE_EXTERIOR_TYPE_NOT_FOUND             = 46,
    HOUSING_RESULT_HOUSE_EXTERIOR_TYPE_SIZE_MISMATCH         = 47,
    HOUSING_RESULT_HOUSE_EXTERIOR_SIZE_NOT_AVAILABLE         = 48,
    HOUSING_RESULT_HOOK_NOT_CHILD_OF_FIXTURE                 = 49,
    HOUSING_RESULT_HOUSE_NOT_FOUND                           = 50,
    HOUSING_RESULT_INCORRECT_FACTION                         = 51,
    HOUSING_RESULT_INVALID_DECOR_ITEM                        = 52,
    HOUSING_RESULT_INVALID_DISTANCE                          = 53,
    HOUSING_RESULT_INVALID_EXTERIOR_DOCUMENT                 = 54,
    HOUSING_RESULT_INVALID_GUILD                             = 55,
    HOUSING_RESULT_INVALID_HOUSE                             = 56,
    HOUSING_RESULT_INVALID_INSTANCE                          = 57,
    HOUSING_RESULT_INVALID_INTERACTION                       = 58,
    HOUSING_RESULT_INVALID_INTERIOR_DOCUMENT                 = 59,
    HOUSING_RESULT_INVALID_LIGHT_OVERLAP                     = 60,
    HOUSING_RESULT_INVALID_MAP                               = 61,
    HOUSING_RESULT_INVALID_NEIGHBORHOOD_NAME                 = 62,
    HOUSING_RESULT_INVALID_ROOM_LAYOUT                       = 63,
    HOUSING_RESULT_INSUFFICIENT_ROOM_BUDGET                  = 64,
    HOUSING_RESULT_LOCKED_BY_OTHER_PLAYER                    = 65,
    HOUSING_RESULT_LOCK_OPERATION_FAILED                     = 66,
    HOUSING_RESULT_MAX_PLACED_DECOR_REACHED                  = 67,
    HOUSING_RESULT_MAX_PET_DECOR_REACHED                     = 68,
    HOUSING_RESULT_MAX_PREVIEW_DECOR_REACHED                 = 69,
    HOUSING_RESULT_MAX_STORAGE_DECOR_REACHED                 = 70,
    HOUSING_RESULT_MISSING_CORE_FIXTURE                      = 71,
    HOUSING_RESULT_MISSING_DYE                               = 72,
    HOUSING_RESULT_MISSING_EXPANSION_ACCESS                  = 73,
    HOUSING_RESULT_MISSING_FACTION_MAP                       = 74,
    HOUSING_RESULT_MISSING_PRIVATE_NEIGHBORHOOD_INVITE       = 75,
    HOUSING_RESULT_MORE_HOUSE_SLOTS_NEEDED                   = 76,
    HOUSING_RESULT_MORE_SIGNATURES_NEEDED                    = 77,
    HOUSING_RESULT_NEIGHBORHOOD_NOT_FOUND                    = 78,
    HOUSING_RESULT_NO_NEIGHBORHOOD_OWNERSHIP_REQUESTS        = 79,
    HOUSING_RESULT_NOT_IN_DECOR_EDIT_MODE                    = 80,
    HOUSING_RESULT_NOT_IN_FIXTURE_EDIT_MODE                  = 81,
    HOUSING_RESULT_NOT_IN_LAYOUT_EDIT_MODE                   = 82,
    HOUSING_RESULT_NOT_INSIDE_HOUSE                          = 83,
    HOUSING_RESULT_NOT_ON_OWNED_PLOT                         = 84,
    HOUSING_RESULT_OPERATION_ABORTED                         = 85,
    HOUSING_RESULT_OWNER_NOT_IN_GUILD                        = 86,
    HOUSING_RESULT_PERMISSION_DENIED                         = 87,
    HOUSING_RESULT_PLACEMENT_TARGET_INVALID                  = 88,
    HOUSING_RESULT_PLAYER_NOT_FOUND                          = 89,
    HOUSING_RESULT_PLAYER_NOT_IN_INSTANCE                    = 90,
    HOUSING_RESULT_PLOT_NOT_FOUND                            = 91,
    HOUSING_RESULT_PLOT_NOT_VACANT                           = 92,
    HOUSING_RESULT_PLOT_RESERVATION_COOLDOWN                 = 93,
    HOUSING_RESULT_PLOT_RESERVED                             = 94,
    HOUSING_RESULT_ROOM_NOT_FOUND                            = 95,
    HOUSING_RESULT_ROOM_PLACEMENT_OUT_OF_BOUNDS              = 96,
    HOUSING_RESULT_ROOM_UPDATE_FAILED                        = 97,
    HOUSING_RESULT_RPC_FAILURE                               = 98,
    HOUSING_RESULT_SERVICE_NOT_AVAILABLE                     = 99,
    HOUSING_RESULT_STATIC_DATA_NOT_FOUND                     = 100,
    HOUSING_RESULT_TIMEOUT_LIMIT                             = 101,
    HOUSING_RESULT_TIMERUNNING_NOT_ALLOWED                   = 102,
    HOUSING_RESULT_TOKEN_REQUIRED                            = 103,
    HOUSING_RESULT_TOO_MANY_REQUESTS                         = 104,
    HOUSING_RESULT_TRANSACTION_FAILURE                       = 105,
    HOUSING_RESULT_UNCOLLECTED_EXTERIOR_FIXTURE              = 106,
    HOUSING_RESULT_UNCOLLECTED_HOUSE_TYPE                    = 107,
    HOUSING_RESULT_UNCOLLECTED_ROOM                          = 108,
    HOUSING_RESULT_UNCOLLECTED_ROOM_MATERIAL                 = 109,
    HOUSING_RESULT_UNCOLLECTED_ROOM_THEME                    = 110,
    HOUSING_RESULT_UNLOCK_OPERATION_FAILED                   = 111
};

// HouseEditorMode
enum HousingEditorMode : uint8
{
    HOUSING_EDITOR_MODE_NONE                    = 0,
    HOUSING_EDITOR_MODE_BASIC_DECOR             = 1,
    HOUSING_EDITOR_MODE_EXPERT_DECOR            = 2,
    HOUSING_EDITOR_MODE_LAYOUT                  = 3,
    HOUSING_EDITOR_MODE_CUSTOMIZE               = 4,
    HOUSING_EDITOR_MODE_CLEANUP                 = 5,
    HOUSING_EDITOR_MODE_EXTERIOR_CUSTOMIZATION  = 6
};

// HouseEditingContext
enum HouseEditingContext : uint8
{
    HOUSE_EDITING_CONTEXT_NONE      = 0,
    HOUSE_EDITING_CONTEXT_DECOR     = 1,
    HOUSE_EDITING_CONTEXT_ROOM      = 2,
    HOUSE_EDITING_CONTEXT_FIXTURE   = 3
};

// HousingFixtureSize
enum HousingFixtureSize : uint8
{
    HOUSING_FIXTURE_SIZE_NONE       = 0,
    HOUSING_FIXTURE_SIZE_ANY        = 1,
    HOUSING_FIXTURE_SIZE_SMALL      = 2,
    HOUSING_FIXTURE_SIZE_MEDIUM     = 3,
    HOUSING_FIXTURE_SIZE_LARGE      = 4
};

// HousingFixtureType (sparse values)
enum HousingFixtureType : uint8
{
    HOUSING_FIXTURE_TYPE_NONE           = 0,
    HOUSING_FIXTURE_TYPE_BASE           = 9,
    HOUSING_FIXTURE_TYPE_ROOF           = 10,
    HOUSING_FIXTURE_TYPE_DOOR           = 11,
    HOUSING_FIXTURE_TYPE_WINDOW         = 12,
    HOUSING_FIXTURE_TYPE_ROOF_DETAIL    = 13,
    HOUSING_FIXTURE_TYPE_ROOF_WINDOW    = 14,
    HOUSING_FIXTURE_TYPE_TOWER          = 15,
    HOUSING_FIXTURE_TYPE_CHIMNEY        = 16
};

// HousingRoomComponentType
enum HousingRoomComponentType : uint8
{
    HOUSING_ROOM_COMPONENT_NONE         = 0,
    HOUSING_ROOM_COMPONENT_WALL         = 1,
    HOUSING_ROOM_COMPONENT_FLOOR        = 2,
    HOUSING_ROOM_COMPONENT_CEILING      = 3,
    HOUSING_ROOM_COMPONENT_STAIRS       = 4,
    HOUSING_ROOM_COMPONENT_PILLAR       = 5,
    HOUSING_ROOM_COMPONENT_DOORWAY_WALL = 6,
    HOUSING_ROOM_COMPONENT_DOORWAY      = 7
};

// HousingRoomComponentDoorType
enum HousingRoomComponentDoorType : uint8
{
    HOUSING_ROOM_DOOR_TYPE_NONE         = 0,
    HOUSING_ROOM_DOOR_TYPE_DOORWAY      = 1,
    HOUSING_ROOM_DOOR_TYPE_THRESHOLD    = 2
};

// HousingRoomComponentCeilingType
enum HousingRoomComponentCeilingType : uint8
{
    HOUSING_ROOM_CEILING_TYPE_FLAT      = 0,
    HOUSING_ROOM_CEILING_TYPE_VAULTED   = 1
};

// HousingRoomComponentStairType
enum HousingRoomComponentStairType : uint8
{
    HOUSING_ROOM_STAIR_TYPE_NONE            = 0,
    HOUSING_ROOM_STAIR_TYPE_START_TO_END    = 1,
    HOUSING_ROOM_STAIR_TYPE_START_TO_MIDDLE = 2,
    HOUSING_ROOM_STAIR_TYPE_MIDDLE_TO_MIDDLE = 3,
    HOUSING_ROOM_STAIR_TYPE_MIDDLE_TO_END   = 4
};

// HousingRoomComponentOptionType
enum HousingRoomComponentOptionType : uint8
{
    HOUSING_ROOM_COMPONENT_OPTION_COSMETIC      = 0,
    HOUSING_ROOM_COMPONENT_OPTION_DOORWAY_WALL  = 1,
    HOUSING_ROOM_COMPONENT_OPTION_DOORWAY       = 2
};

// How a decor item was acquired (SourceValue carries the spell ID / item GUID as a string)
enum DecorSourceType : uint8
{
    DECOR_SOURCE_STANDARD       = 0, // Default / starter decor / placed
    DECOR_SOURCE_SPELL          = 5, // Acquired via spell cast (SourceValue = spell ID string)
    DECOR_SOURCE_ITEM           = 6, // Acquired via item use (SourceValue = item GUID string)
    DECOR_SOURCE_DEFERRED       = 3, // Redeemed from deferred reward queue
};

// DecorStoragePersistedData.PlacementStatus: the client sums the interior budget from 1 records, exterior from 2.
enum HousingDecorPlacementStatus : uint8
{
    HOUSING_DECOR_IN_STORAGE    = 0,
    HOUSING_DECOR_PLACED_HOUSE  = 1,
    HOUSING_DECOR_PLACED_PLOT   = 2,
};

// HousingCatalogEntryType
enum HousingCatalogEntryType : uint8
{
    HOUSING_CATALOG_ENTRY_INVALID   = 0,
    HOUSING_CATALOG_ENTRY_DECOR     = 1,
    HOUSING_CATALOG_ENTRY_ROOM      = 2
};

// HousingCatalogEntrySize
enum HousingCatalogEntrySize : uint8
{
    HOUSING_CATALOG_SIZE_NONE       = 0,
    HOUSING_CATALOG_SIZE_TINY       = 65,
    HOUSING_CATALOG_SIZE_SMALL      = 66,
    HOUSING_CATALOG_SIZE_MEDIUM     = 67,
    HOUSING_CATALOG_SIZE_LARGE      = 68,
    HOUSING_CATALOG_SIZE_HUGE       = 69
};

// HousingDecorTheme
enum HousingDecorTheme : uint8
{
    HOUSING_DECOR_THEME_NONE        = 0,
    HOUSING_DECOR_THEME_FOLK        = 1,
    HOUSING_DECOR_THEME_RUGGED      = 2,
    HOUSING_DECOR_THEME_GENERIC     = 3,
    HOUSING_DECOR_THEME_NIGHT_ELF   = 4,
    HOUSING_DECOR_THEME_BLOOD_ELF   = 5
};

// RoomConnectionType
enum RoomConnectionType : uint8
{
    ROOM_CONNECTION_NONE    = 0,
    ROOM_CONNECTION_ALL     = 1
};

// HousingRoomFlags (bitmask)
enum HousingRoomFlags : uint32
{
    HOUSING_ROOM_FLAG_NONE                  = 0x00,
    HOUSING_ROOM_FLAG_BASE_ROOM             = 0x01,
    HOUSING_ROOM_FLAG_HAS_STAIRS            = 0x02,
    HOUSING_ROOM_FLAG_UNLOCKED_BY_DEFAULT   = 0x04,
    HOUSING_ROOM_FLAG_HAS_CUSTOM_GEOMETRY   = 0x08
};

// HousingLayoutRestriction
enum HousingLayoutRestriction : uint8
{
    HOUSING_LAYOUT_RESTRICTION_NONE                 = 0,
    HOUSING_LAYOUT_RESTRICTION_ROOM_NOT_FOUND       = 1,
    HOUSING_LAYOUT_RESTRICTION_NOT_INSIDE_HOUSE     = 2,
    HOUSING_LAYOUT_RESTRICTION_NOT_HOUSE_OWNER      = 3,
    HOUSING_LAYOUT_RESTRICTION_IS_BASE_ROOM         = 4,
    HOUSING_LAYOUT_RESTRICTION_ROOM_NOT_LEAF        = 5,
    HOUSING_LAYOUT_RESTRICTION_STAIRWELL_CONNECTION = 6,
    HOUSING_LAYOUT_RESTRICTION_LAST_ROOM            = 7,
    HOUSING_LAYOUT_RESTRICTION_UNREACHABLE_ROOM     = 8,
    HOUSING_LAYOUT_RESTRICTION_SINGLE_DOOR          = 9
};

// NeighborhoodInviteResult (verified against client binary)
enum NeighborhoodInviteResult : uint8
{
    NEIGHBORHOOD_INVITE_SUCCESS                 = 0,
    NEIGHBORHOOD_INVITE_DB_ERROR                = 1,
    NEIGHBORHOOD_INVITE_RPC_FAILURE             = 2,
    NEIGHBORHOOD_INVITE_GENERIC_FAILURE         = 3,
    NEIGHBORHOOD_INVITE_PERMISSION              = 4,
    NEIGHBORHOOD_INVITE_FACTION                 = 5,
    NEIGHBORHOOD_INVITE_PENDING_INVITATION      = 6,
    NEIGHBORHOOD_INVITE_INVITE_LIMIT            = 7,
    NEIGHBORHOOD_INVITE_NOT_ENOUGH_PLOTS        = 8,
    NEIGHBORHOOD_INVITE_NOT_FOUND               = 9,
    NEIGHBORHOOD_INVITE_TOO_MANY_REQUESTS       = 10
};

// Enum.BulkRefundResult
enum BulkRefundResult : uint8
{
    BULK_REFUND_RESULT_SUCCESS                  = 0,
    BULK_REFUND_RESULT_FAILED                   = 1,
    BULK_REFUND_RESULT_INVALID_REQUEST          = 2,
    BULK_REFUND_RESULT_REFUND_WINDOW_EXPIRED    = 3,
    BULK_REFUND_RESULT_SYSTEM_DISABLED          = 4,
    BULK_REFUND_RESULT_TIMEOUT                  = 5
};

// HouseOwnerError (JamPotentialCosmeticHouseOwner.Error: the house settings owner list greys the character out)
enum HouseOwnerError : uint8
{
    HOUSE_OWNER_ERROR_NONE                  = 0,
    HOUSE_OWNER_ERROR_FACTION               = 1,
    HOUSE_OWNER_ERROR_GUILD                 = 2,
    HOUSE_OWNER_ERROR_GENERIC_PERMISSION    = 3
};

// CreateNeighborhoodErrorType
enum CreateNeighborhoodErrorType : uint8
{
    CREATE_NEIGHBORHOOD_ERROR_NONE              = 0,
    CREATE_NEIGHBORHOOD_ERROR_PROFANITY         = 1,
    CREATE_NEIGHBORHOOD_ERROR_UNDERSIZED_GUILD  = 2,
    CREATE_NEIGHBORHOOD_ERROR_OVERSIZED_GUILD   = 3
};

// NeighborhoodMemberRole
enum NeighborhoodMemberRole : uint8
{
    NEIGHBORHOOD_ROLE_RESIDENT  = 0,
    NEIGHBORHOOD_ROLE_MANAGER   = 1,
    NEIGHBORHOOD_ROLE_OWNER     = 2
};

// NeighborhoodFactionRestriction
enum NeighborhoodFactionRestriction : int32
{
    NEIGHBORHOOD_FACTION_NONE       = 0,
    NEIGHBORHOOD_FACTION_HORDE      = 1,
    NEIGHBORHOOD_FACTION_ALLIANCE   = 2
};

// FrameTutorialAccount bit 38 "HousingModesUnlocked" in the account data: word 1, bit 6 -> "0 64".
constexpr char const* HOUSING_MODES_UNLOCKED_CVAR = "0 64";

// Rewarding all four quests (in the chain's own order) ends the housing tutorial.
constexpr uint32 HOUSING_TUTORIAL_QUEST_CHAIN[] = { 93057, 91863, 91968, 91969 };

// HouseSettingFlags (bitmask): bits 0-4 HouseAccess (interior), bits 5-9 PlotAccess (exterior)
enum HouseSettingFlags : uint32
{
    HOUSE_SETTING_NONE                      = 0x000,
    HOUSE_SETTING_HOUSE_ACCESS_ANYONE       = 0x001,
    HOUSE_SETTING_HOUSE_ACCESS_NEIGHBORS    = 0x002,
    HOUSE_SETTING_HOUSE_ACCESS_GUILD        = 0x004,
    HOUSE_SETTING_HOUSE_ACCESS_FRIENDS      = 0x008,
    HOUSE_SETTING_HOUSE_ACCESS_PARTY        = 0x010,
    HOUSE_SETTING_PLOT_ACCESS_ANYONE        = 0x020,
    HOUSE_SETTING_PLOT_ACCESS_NEIGHBORS     = 0x040,
    HOUSE_SETTING_PLOT_ACCESS_GUILD         = 0x080,
    HOUSE_SETTING_PLOT_ACCESS_FRIENDS       = 0x100,
    HOUSE_SETTING_PLOT_ACCESS_PARTY         = 0x200,
    // Who may export this house as a blueprint.
    HOUSE_SETTING_BLUEPRINT_EXPORT_ANYONE   = 0x400,
    HOUSE_SETTING_BLUEPRINT_EXPORT_NEIGHBORS = 0x800,
    HOUSE_SETTING_BLUEPRINT_EXPORT_GUILD    = 0x1000,
    HOUSE_SETTING_BLUEPRINT_EXPORT_FRIENDS  = 0x2000,
    HOUSE_SETTING_BLUEPRINT_EXPORT_PARTY    = 0x4000
};

// Fresh-house default: plot AND house open to everyone ("no bits = nobody", so both must be set).
constexpr uint32 HOUSE_SETTING_DEFAULT    = HOUSE_SETTING_PLOT_ACCESS_ANYONE | HOUSE_SETTING_HOUSE_ACCESS_ANYONE;
constexpr uint32 HOUSE_SETTING_VALID_MASK = 0x7FFF; // bits 0-14

// HousingDecorPlacementFlags enum - 5 values (bitmask)
enum HousingDecorPlacementFlags : int32
{
    DECOR_PLACEMENT_FLOOR       = 0x01,
    DECOR_PLACEMENT_WALL        = 0x02,
    DECOR_PLACEMENT_CEILING     = 0x04,
    DECOR_PLACEMENT_OUTDOOR     = 0x08,
    DECOR_PLACEMENT_STACKABLE   = 0x10
};

// SMSG_HOUSING_GET_PLAYER_PERMISSIONS_RESPONSE flags: 0xFE own house, 0x10 plain visitor, 0x0C blueprint export/import grants.
constexpr uint8 HOUSING_PERMISSIONS_OWNER   = 0xFE;
constexpr uint8 HOUSING_PERMISSIONS_VISITOR = 0x10;
constexpr uint8 HOUSING_PERMISSIONS_BLUEPRINT = 0x0C;

// TrinityString entry for the plot-eviction warning (sent as CHAT_MSG_RAID_BOSS_WHISPER).
constexpr uint32 HOUSING_STRING_PLOT_ACCESS_DENIED = 304665;

// Plot eviction sequence: warning spell + aura, then the whisper, then the teleport when the aura expires.
constexpr uint32 SPELL_HOUSING_PLOT_EVICT_WARNING = 1245416;
constexpr Milliseconds HOUSING_PLOT_EVICT_DELAY = 5s;

// HouseDecor.db2 Flags bit carried by licensed (shop / promotional) decor.
constexpr int32 HOUSE_DECOR_FLAG_LICENSED = 0x400;

// HousingRoomSize
enum HousingRoomSize : int8
{
    ROOM_SIZE_SMALL     = 0,
    ROOM_SIZE_MEDIUM    = 1,
    ROOM_SIZE_LARGE     = 2
};

// HousingPlotSize
enum HousingPlotSize : int32
{
    PLOT_SIZE_SMALL     = 0,
    PLOT_SIZE_MEDIUM    = 1,
    PLOT_SIZE_LARGE     = 2
};

// HousingInitiativeType
enum HousingInitiativeType : int32
{
    INITIATIVE_TYPE_GATHERING       = 0,
    INITIATIVE_TYPE_CRAFTING        = 1,
    INITIATIVE_TYPE_COMBAT          = 2,
    INITIATIVE_TYPE_EXPLORATION     = 3
};

// HousingFixtureFlags (bitmask)
enum HousingFixtureFlags : uint32
{
    HOUSING_FIXTURE_FLAG_NONE               = 0x00,
    HOUSING_FIXTURE_FLAG_IS_DEFAULT         = 0x01,
    HOUSING_FIXTURE_FLAG_UNLOCKED_BY_DEFAULT = 0x02
};

// HousingRoomComponentFlags (bitmask)
enum HousingRoomComponentFlags : uint32
{
    HOUSING_ROOM_COMPONENT_FLAG_NONE                    = 0x00,
    HOUSING_ROOM_COMPONENT_FLAG_HIDDEN_IN_LAYOUT_MODE   = 0x01
};

// HousingDecorPlacementRestriction (bitmask) - server-sent placement-failure reasons.
enum HousingDecorPlacementRestriction : uint32
{
    HOUSING_DECOR_PLACEMENT_RESTRICTION_TOO_FAR_AWAY          = 0x01,
    HOUSING_DECOR_PLACEMENT_RESTRICTION_OUTSIDE_ROOM_BOUNDS   = 0x02,
    HOUSING_DECOR_PLACEMENT_RESTRICTION_OUTSIDE_PLOT_BOUNDS   = 0x04,
    HOUSING_DECOR_PLACEMENT_RESTRICTION_CHILD_OUTSIDE_BOUNDS  = 0x08,
    HOUSING_DECOR_PLACEMENT_RESTRICTION_INVALID_TARGET        = 0x10,
    HOUSING_DECOR_PLACEMENT_RESTRICTION_INVALID_COLLISION     = 0x20,
    HOUSING_DECOR_PLACEMENT_RESTRICTION_INVALID_LIGHT_OVERLAP = 0x40
};

// HousingRoomComponentOptionFlags (bitmask)
enum HousingRoomComponentOptionFlags : uint32
{
    HOUSING_ROOM_COMPONENT_OPTION_FLAG_NONE         = 0x00,
    HOUSING_ROOM_COMPONENT_OPTION_FLAG_IS_DEFAULT   = 0x01
};

// HousingRoomComponentTextureFlags (bitmask)
enum HousingRoomComponentTextureFlags : uint32
{
    HOUSING_ROOM_COMPONENT_TEXTURE_FLAG_NONE                    = 0x00,
    HOUSING_ROOM_COMPONENT_TEXTURE_FLAG_UNLOCKED_BY_DEFAULT     = 0x01
};

// NeighborhoodFlags (bitmask)
enum NeighborhoodFlags : uint32
{
    NEIGHBORHOOD_FLAG_NONE              = 0x00,
    NEIGHBORHOOD_FLAG_POOL_PARENT       = 0x01,
    NEIGHBORHOOD_FLAG_OPEN_TO_PUBLIC    = 0x02
};

// HouseExteriorWMODataFlags (bitmask)
enum HouseExteriorWMODataFlags : uint32
{
    HOUSE_EXTERIOR_WMO_FLAG_NONE                            = 0x00,
    HOUSE_EXTERIOR_WMO_FLAG_UNLOCKED_BY_DEFAULT             = 0x01,
    HOUSE_EXTERIOR_WMO_FLAG_ALLOWED_IN_HORDE_NEIGHBORHOODS  = 0x02,
    HOUSE_EXTERIOR_WMO_FLAG_ALLOWED_IN_ALLIANCE_NEIGHBORHOODS = 0x04
};

// HousingDecorModelType
enum HousingDecorModelType : uint8
{
    HOUSING_DECOR_MODEL_TYPE_NONE   = 0,
    HOUSING_DECOR_MODEL_TYPE_M2     = 1,
    HOUSING_DECOR_MODEL_TYPE_WMO    = 2
};

// NeighborhoodInitiativeUpdateStatus - sent via SMSG_INITIATIVE_UPDATE_STATUS
enum NeighborhoodInitiativeUpdateStatus : uint8
{
    NI_UPDATE_STATUS_STARTED                = 0,
    NI_UPDATE_STATUS_MILESTONE_COMPLETED    = 1,
    NI_UPDATE_STATUS_COMPLETED              = 2,
    NI_UPDATE_STATUS_FAILED                 = 3
};

// NeighborhoodInitiativeChestResult - sent via SMSG_INITIATIVE_CHEST_RESULT
enum NeighborhoodInitiativeChestResult : uint32
{
    NI_CHEST_SUCCESS                = 0,
    NI_CHEST_UNSPECIFIED_FAILURE    = 1,
    NI_CHEST_NO_HOUSE_FOUND        = 2,
    NI_CHEST_NO_REWARDS             = 3,
    NI_CHEST_THROTTLED              = 4,
    NI_CHEST_SERVICE_DISABLED       = 5
};

// NeighborhoodInitiativeTaskType - from InitiativeTask DB2 TaskType field
enum NeighborhoodInitiativeTaskType : int32
{
    NI_TASK_TYPE_SINGLE                 = 0,
    NI_TASK_TYPE_REPEATABLE_FINITE      = 1,
    NI_TASK_TYPE_REPEATABLE_INFINITE    = 2
};

// NeighborhoodInitiativeCompletionState - per-task completion state
enum NeighborhoodInitiativeCompletionState : uint8
{
    NI_COMPLETION_NOT_COMPLETED         = 0,
    NI_COMPLETION_PLAYER_COMPLETED      = 1,
    NI_COMPLETION_SYSTEM_ABANDONED      = 2
};

// NeighborhoodInitiativeFlags - from NeighborhoodInitiative DB2 Flags field
enum NeighborhoodInitiativeFlags : uint32
{
    NI_FLAG_DISABLED    = 0x1,
    NI_FLAG_NO_ABANDON  = 0x2,
    NI_FLAG_NO_REPEAT   = 0x4
};

// InitiativeMilestoneFlags - from InitiativeMilestone DB2 Flags field
enum InitiativeMilestoneFlags : int32
{
    INITIATIVE_MILESTONE_FLAG_FINAL = 0x1
};

// InitiativeRewardFlags - from InitiativeReward DB2 Flags field
enum InitiativeRewardFlags : int32
{
    INITIATIVE_REWARD_FLAG_PERMANENT_WORLD_STATE = 0x1
};

// NeighborhoodInitiativeNeighborhoodType enum
enum NeighborhoodInitiativeNeighborhoodType : uint8
{
    NI_NEIGHBORHOOD_TYPE_SINGLETON  = 0,
    NI_NEIGHBORHOOD_TYPE_POOL       = 1
};

// HousingFavorUpdateSource
enum HousingFavorUpdateSource : uint8
{
    HOUSING_FAVOR_SOURCE_UNKNOWN            = 0,
    HOUSING_FAVOR_SOURCE_DECOR_COLLECTION   = 1,
    HOUSING_FAVOR_SOURCE_DEFERRED_REWARDS   = 2,
    HOUSING_FAVOR_SOURCE_RETROACTIVE_DECOR  = 3,
    HOUSING_FAVOR_SOURCE_NEW_HOUSE_DECOR    = 4,
    HOUSING_FAVOR_SOURCE_INITIATIVE_TASK    = 5,
    HOUSING_FAVOR_SOURCE_INITIATIVE_CHEST   = 6,
    HOUSING_FAVOR_SOURCE_QUEST              = 7
};

// HousingFavorUpdateType
enum HousingFavorUpdateType : uint8
{
    HOUSING_FAVOR_UPDATE_NONE           = 0,
    HOUSING_FAVOR_UPDATE_INITIATIVE_ADD = 1,
    HOUSING_FAVOR_UPDATE_SET            = 2
};

// HousingPlotOwnerType
enum HousingPlotOwnerType : uint8
{
    HOUSING_PLOT_OWNER_NONE     = 0,
    HOUSING_PLOT_OWNER_STRANGER = 1,
    HOUSING_PLOT_OWNER_FRIEND   = 2,
    HOUSING_PLOT_OWNER_SELF     = 3
};

// HousingTeleportReason
enum HousingTeleportReason : uint8
{
    HOUSING_TELEPORT_NONE                   = 0,
    HOUSING_TELEPORT_CHEAT                  = 1,
    HOUSING_TELEPORT_UNSPECIFIED_SPELLCAST  = 2,
    HOUSING_TELEPORT_BOOTED                 = 3,
    HOUSING_TELEPORT_HOMESTONE              = 4,
    HOUSING_TELEPORT_VISIT                  = 5,
    HOUSING_TELEPORT_FRIEND                 = 6,
    HOUSING_TELEPORT_GUILD_MEMBER           = 7,
    HOUSING_TELEPORT_PARTY_MEMBER           = 8,
    HOUSING_TELEPORT_EXITING_HOUSE          = 9,
    HOUSING_TELEPORT_PORTAL                 = 10,
    HOUSING_TELEPORT_TUTORIAL               = 11
};

// HousingThrottleType
enum HousingThrottleType : uint8
{
    HOUSING_THROTTLE_GENERAL    = 0,
    HOUSING_THROTTLE_DECORATION = 1
};

// HousingThemeFlags (bitmask)
enum HousingThemeFlags : uint32
{
    HOUSING_THEME_FLAG_NONE                     = 0x00,
    HOUSING_THEME_FLAG_UNLOCKED_BY_DEFAULT      = 0x01,
    HOUSING_THEME_FLAG_SHOW_IN_STYLE_SELECTOR   = 0x02
};

// NeighborhoodMapFlags (bitmask)
enum NeighborhoodMapFlags : uint32
{
    NEIGHBORHOOD_MAP_FLAG_NONE                  = 0x00,
    NEIGHBORHOOD_MAP_FLAG_ALLIANCE_PURCHASABLE  = 0x01,
    NEIGHBORHOOD_MAP_FLAG_HORDE_PURCHASABLE     = 0x02,
    NEIGHBORHOOD_MAP_FLAG_CAN_SYSTEM_GENERATE   = 0x04
};

// NeighborhoodOwnerType
enum NeighborhoodOwnerType : uint8
{
    NEIGHBORHOOD_OWNER_NONE     = 0,
    NEIGHBORHOOD_OWNER_GUILD    = 1,
    NEIGHBORHOOD_OWNER_CHARTER  = 2
};

// NeighborhoodType
enum NeighborhoodType : uint8
{
    NEIGHBORHOOD_TYPE_OPEN      = 0,
    NEIGHBORHOOD_TYPE_PRIVATE   = 1,
    NEIGHBORHOOD_TYPE_PUBLIC    = 2
};

// PurchaseHouseDisabledReason
enum PurchaseHouseDisabledReason : uint8
{
    PURCHASE_HOUSE_DISABLED_NONE                = 0,
    PURCHASE_HOUSE_DISABLED_WRONG_FACTION       = 1,
    PURCHASE_HOUSE_DISABLED_WRONG_GUILD         = 2,
    PURCHASE_HOUSE_DISABLED_NOT_INVITED         = 3,
    PURCHASE_HOUSE_DISABLED_NO_EXPANSION        = 4,
    PURCHASE_HOUSE_DISABLED_RESERVED            = 5,
    PURCHASE_HOUSE_DISABLED_GUILD_LOCKOUT       = 6,
    PURCHASE_HOUSE_DISABLED_CHARTER_LOCKOUT     = 7,
    PURCHASE_HOUSE_DISABLED_MAX_HOUSES          = 8,
    PURCHASE_HOUSE_DISABLED_NO_GAME_TIME        = 9
};

// ReservationFlags (bitmask)
enum ReservationFlags : uint32
{
    RESERVATION_FLAG_NONE       = 0x00,
    RESERVATION_FLAG_RELINQUISH = 0x01,
    RESERVATION_FLAG_CANCELED   = 0x02,
    RESERVATION_FLAG_PLOTLESS   = 0x04
};

// RetroactiveDecorRewardFlags (bitmask)
enum RetroactiveDecorRewardFlags : uint32
{
    RETROACTIVE_DECOR_REWARD_FLAG_NONE                  = 0x00,
    RETROACTIVE_DECOR_REWARD_FLAG_ALL_CRITERIA_REQUIRED = 0x01
};

// InvalidPlotScreenshotReason
enum InvalidPlotScreenshotReason : uint8
{
    INVALID_PLOT_SCREENSHOT_NONE                = 0,
    INVALID_PLOT_SCREENSHOT_OUT_OF_BOUNDS       = 1,
    INVALID_PLOT_SCREENSHOT_FACING              = 2,
    INVALID_PLOT_SCREENSHOT_NO_NEIGHBORHOOD     = 3,
    INVALID_PLOT_SCREENSHOT_NO_ACTIVE_PLAYER    = 4
};

// HouseFinderSuggestionReason (bitmask)
enum HouseFinderSuggestionReason : uint8
{
    HOUSE_FINDER_SUGGESTION_NONE            = 0x00,
    HOUSE_FINDER_SUGGESTION_OWNER           = 0x01,
    HOUSE_FINDER_SUGGESTION_CHARTER_INVITE  = 0x02,
    HOUSE_FINDER_SUGGESTION_GUILD           = 0x04,
    HOUSE_FINDER_SUGGESTION_BNET_FRIENDS    = 0x08,
    HOUSE_FINDER_SUGGESTION_PARTY_SYNC      = 0x10,
    HOUSE_FINDER_SUGGESTION_RANDOM          = 0x20,
    HOUSE_FINDER_SUGGESTION_HOME_OWNER      = 0x40,
    HOUSE_FINDER_SUGGESTION_RELINQUISHED    = 0x80
};

// CornerstonePurchaseMode
enum CornerstonePurchaseMode : uint8
{
    CORNERSTONE_PURCHASE_BASIC  = 0,
    CORNERSTONE_PURCHASE_IMPORT = 1,
    CORNERSTONE_PURCHASE_MOVE   = 2
};

// HouseLevelRewardType
enum HouseLevelRewardType : uint8
{
    HOUSE_LEVEL_REWARD_VALUE    = 0,
    HOUSE_LEVEL_REWARD_OBJECT   = 1
};

// HouseVisitType
enum HouseVisitType : uint8
{
    HOUSE_VISIT_UNKNOWN = 0,
    HOUSE_VISIT_FRIEND  = 1,
    HOUSE_VISIT_GUILD   = 2,
    HOUSE_VISIT_PARTY   = 3
};

// HousingItemToastType
enum HousingItemToastType : uint8
{
    HOUSING_ITEM_TOAST_ROOM          = 0,
    HOUSING_ITEM_TOAST_FIXTURE       = 1,
    HOUSING_ITEM_TOAST_CUSTOMIZATION = 2,
    HOUSING_ITEM_TOAST_DECOR         = 3,
    HOUSING_ITEM_TOAST_HOUSE         = 4
};

// HousingRoomComponentFloorType
enum HousingRoomComponentFloorType : uint8
{
    HOUSING_ROOM_COMPONENT_FLOOR_TYPE_FLOOR = 0
};

// HousingDecorType
enum HousingDecorType : uint8
{
    HOUSING_DECOR_TYPE_NONE     = 0,
    HOUSING_DECOR_TYPE_FLOOR    = 1,
    HOUSING_DECOR_TYPE_WALL     = 2,
    HOUSING_DECOR_TYPE_CEILING  = 3,
    HOUSING_DECOR_TYPE_FLOORING = 4
};

// HouseLevelRewardValueType
enum HouseLevelRewardValueType : uint8
{
    HOUSE_LEVEL_REWARD_EXTERIOR_DECOR   = 0,
    HOUSE_LEVEL_REWARD_INTERIOR_DECOR   = 1,
    HOUSE_LEVEL_REWARD_ROOMS            = 2,
    HOUSE_LEVEL_REWARD_FIXTURES         = 3
};

// Constants
// Spatial-validation half-extent for decor positions, measured from the placement anchor.
static constexpr float HOUSING_MAX_DECOR_LOCAL_EXTENT  = 1024.0f;
// DecorCategory.db2 id 4 "Lighting" (12.0.7 lets Lighting decor be placed outdoors).
static constexpr uint32 HOUSING_DECOR_CATEGORY_LIGHTING = 4;
// 12.0.7 "two lights cannot overlap" rule; not datamineable, documented default separation in yards.
static constexpr float HOUSING_LIGHT_OVERLAP_RADIUS = 3.0f;

// Half-extents for CMSG_HOUSE_EXTERIOR_SET_HOUSE_POSITION: nudge within the plot, no relocation.
static constexpr float HOUSING_MAX_HOUSE_PLOT_OFFSET_XY = 45.0f;
static constexpr float HOUSING_MAX_HOUSE_PLOT_OFFSET_Z  = 50.0f;
// Decoration throttle: at most BURST place/move/remove ops per WINDOW_MS.
static constexpr uint32 HOUSING_DECOR_THROTTLE_WINDOW_MS = 10000;
static constexpr uint32 HOUSING_DECOR_THROTTLE_BURST     = 40;
static constexpr uint32 MAX_HOUSING_DECOR_PER_ROOM      = 50;
static constexpr uint32 MAX_HOUSING_ROOMS_PER_HOUSE     = 20;
static constexpr uint32 MAX_HOUSING_DYE_SLOTS           = 3;
static constexpr uint32 MAX_NEIGHBORHOOD_PLOTS          = 55;
static constexpr uint32 HOUSING_EXPANSION_FREE_PLOTS    = 5;    // spawn a new public neighborhood when free plots drop to this
static constexpr uint32 MAX_NEIGHBORHOOD_MANAGERS       = 5;
static constexpr uint32 MAX_PENDING_INVITES             = 20;
static constexpr uint8  INVALID_PLOT_INDEX              = 255;
// Retail spawns the same "Cornerstone" GO on every plot; identified by the GO's FJamHousingCornerstone_C fragment.
static constexpr uint32 HOUSING_CORNERSTONE_GAMEOBJECT_ENTRY = 457142;
static constexpr uint32 HOUSING_MAX_NAME_LENGTH         = 64;
static constexpr uint64 HOUSE_MOVE_COST_COPPER          = 500ULL * 10000ULL;       // 500g move cost
static constexpr uint32 MAX_HOUSE_LEVEL                 = 12;   // HouseLevelData.db2 levels 1-12

// Starter favor granted on house purchase.
static constexpr uint64 HOUSE_PURCHASE_STARTER_FAVOR    = 910;

// Initiative progress is reported on a 0..1000 point scale (PlayerInitiativeInfo.ProgressRequired).
static constexpr float INITIATIVE_PROGRESS_REQUIRED     = 1000.0f;

// InitiativeMilestone.RequiredContributionAmount is a PERCENTAGE (25/50/75/100), not a 0..1 fraction.
static constexpr float INITIATIVE_MILESTONE_SCALE       = 100.0f;

// Floating world-text for neighborhood contribution credit (reproduced from the retail capture; enUS only).
constexpr char const HOUSING_WORLD_TEXT_NEIGHBORLY[] = "|cnYELLOW_FONT_COLOR:+Neighborly|r";

// Quest 91863 objective 17 ("Acquire a house") kill credit, granted on successful purchase.
static constexpr uint32 NPC_KILL_CREDIT_BUY_HOME        = 248858;

// Spell applied during housing decor edit mode ("phased-out" visual; aura slot 51, Flags=NoCaster, ActiveFlags=15)
static constexpr uint32 SPELL_HOUSING_EDIT_MODE_AURA    = 1263303;

// "[DNT] Decorating - Disable All the Things - Room Editor": room layout mode (stun + no gravity, pacify, immunity).
static constexpr uint32 SPELL_HOUSING_ROOM_EDIT_MODE_AURA = 1263316;

// "Leave House" (effect 343): cast by the interior door after it opens; takes the player out to the plot
static constexpr uint32 SPELL_HOUSING_LEAVE_HOUSE       = 1234193;
// 10 s casts ending in SPELL_EFFECT_TELEPORT_UNITS to a plot
static constexpr uint32 SPELL_HOUSING_TELEPORT_HOME     = 1233637; // CMSG_HOUSING_SVCS_TELEPORT_TO_PLOT to one's own plot
static constexpr uint32 SPELL_HOUSING_VISIT_HOUSE       = 1265142; // house finder "Visit" (after the plot reservation) and other plots

// Spell applied when player enters their own housing plot (aura slots 50/55, Flags=NoCaster, ActiveFlags=1-2)
static constexpr uint32 SPELL_HOUSING_PLOT_ENTER        = 1239847;

// Second spell applied when player enters their own housing plot (aura slot 56, Flags=NoCaster, ActiveFlags=1)
static constexpr uint32 SPELL_HOUSING_PLOT_PRESENCE     = 469226;

// Third spell applied on first plot enter - replaces the slot 9 aura (Flags=NoCaster|Scalable, has PointsCount)
static constexpr uint32 SPELL_HOUSING_PLOT_ENTER_2      = 1266699;

// Neighborhood map-entry auras, applied right after the big SMSG_UPDATE_OBJECT at neighborhood-map entry.
static constexpr uint32 SPELL_HOUSING_MAP_ENTRY_FIXUP      = 1272741;  // "Housing Fixup Aura"
static constexpr uint32 SPELL_HOUSING_MAP_ENTRY_REACT      = 1263578;  // "Player Action React (DNT)"
static constexpr uint32 SPELL_HOUSING_MAP_ENTRY_ENDEAVOR   = 1276064;  // "[DNT] Endeavor Cover Aura"
static constexpr uint32 SPELL_HOUSING_MAP_ENTRY_NEIGHBOR   = 1227147;  // "In Your Neighborhood"
// Cast on the player after a house is bought or moved to a new plot.
static constexpr uint32 SPELL_HOUSING_HOUSE_ACQUIRED       = 1253572;
// SpellXSpellVisualID baked into spell 1227147's AuraDataInfo.Visual on retail.
static constexpr uint32 VISUAL_HOUSING_MAP_ENTRY_NEIGHBOR  = 503683;

// Completes the housing tutorial; turning it in unlocks all editor modes.
static constexpr uint32 QUEST_HOUSING_TUTORIAL_COMPLETE = 94455; // "Home at Last"

// "Create a Neighborhood" - charter founding quest (charter neighborhoods require 10 signatures on retail).
static constexpr uint32 QUEST_CREATE_A_NEIGHBORHOOD = 89450;
// Neighborhood Charter - provided by quest 89450; re-obtainable from stewards on retail.
static constexpr uint32 ITEM_NEIGHBORHOOD_CHARTER = 239098;

// Post-tutorial auras, applied when QUEST_HOUSING_TUTORIAL_COMPLETE completes (not in DB2; sent as manual SMSG_AURA_UPDATE).
static constexpr uint32 SPELL_HOUSING_TUTORIAL_DONE_1   = 1285428; // slot 8
static constexpr uint32 SPELL_HOUSING_TUTORIAL_DONE_2   = 1285424; // slot 9
// Slot 50: same spell as SPELL_HOUSING_PLOT_ENTER_2 but applied at slot 50, not slot 9.
static constexpr uint32 SPELL_HOUSING_TUTORIAL_DONE_3   = 1266699;

// WorldState IDs - continuous counters sent throughout the housing session.
static constexpr uint32 WORLDSTATE_HOUSING_COUNTER_1    = 13436;
static constexpr uint32 WORLDSTATE_HOUSING_COUNTER_2    = 13437;
static constexpr uint32 WORLDSTATE_HOUSING_COUNTER_3    = 13438;

// WS[30906]: toggled 1 when inside a house interior (MapID=2783), 0 when leaving.
static constexpr uint32 WORLDSTATE_HOUSING_INTERIOR     = 30906;

// Interval and increment for housing WorldState counter updates
static constexpr uint32 HOUSING_WORLDSTATE_INTERVAL_MS  = 5000;  // counters 1-3 every ~5 s (+1333)
static constexpr uint32 HOUSING_WORLDSTATE_INCREMENT    = 1333;
static constexpr uint32 HOUSING_WORLDSTATE_INCREMENT_2  = 7233;

// Cosmetic phases removed on plot enter and restored on leave.
static constexpr uint32 HOUSING_COSMETIC_PHASES[] =
{
    25571, 26216, 27429, 27442, 27489, 27695,
    28304, 28312, 28313, 28314, 28315, 28316,
    28320, 28339, 28370, 28748
};

static constexpr uint32 HOUSING_COSMETIC_PHASE_COUNT = sizeof(HOUSING_COSMETIC_PHASES) / sizeof(HOUSING_COSMETIC_PHASES[0]);

// Delay in milliseconds before cosmetic phase shifts take effect on plot enter/leave
static constexpr uint32 HOUSING_COSMETIC_PHASE_DELAY_MS = 10000;

// Room grid spacing for interior maps (~24 yards between room centers)
static constexpr float HOUSING_ROOM_GRID_SPACING = 24.0f;

// HouseExteriorWmoDataID for the Horde theme
static constexpr int32 HORDE_HOUSE_EXTERIOR_WMO_DATA_ID = 87;

// Max players allowed on a housing map (exterior neighborhood + interior combined)
static constexpr uint32 MAX_HOUSING_MAP_PLAYERS = 40;

// Housing warning flags - reasons why housing features may be restricted
enum HousingWarningFlag : uint32
{
    HOUSING_WARNING_NONE                    = 0x00,
    HOUSING_WARNING_EXPANSION_REQUIRED      = 0x01, // Player needs The War Within expansion
    HOUSING_WARNING_LEVEL_TOO_LOW           = 0x02, // Player below minimum housing level
    HOUSING_WARNING_FACTION_RESTRICTED      = 0x04, // Faction-specific restriction
    HOUSING_WARNING_SERVICE_DISABLED        = 0x08, // Housing service disabled via CVar
};

// Minimum player level to access housing features
static constexpr uint32 HOUSING_MIN_PLAYER_LEVEL = 10;

// Required expansion for housing access (The War Within = 10)
static constexpr uint32 HOUSING_REQUIRED_EXPANSION = 10;

// Kill credit completing QUEST_HOUSING_TUTORIAL_COMPLETE, granted the moment the player first stands inside their house.
static constexpr uint32 NPC_HOUSING_TUTORIAL_HOUSE_ENTERED_CREDIT = 257763;

// House interior instance map (MAP_HOUSE_INTERIOR = 7)
static constexpr uint32 HOUSE_INTERIOR_MAP_ID = 2783;

// Vertical spacing between stacked interior rooms; must match HouseInteriorMap's room positioning.
static constexpr float HOUSE_INTERIOR_FLOOR_HEIGHT = 12.0f;

// Interior front-door GameObjects, picked by faction in HouseInteriorMap (not reachable from ExteriorComponent).
static constexpr uint8 HOUSE_DECOR_MODEL_TYPE_WMO = 2; // HouseDecor.ModelType: interior walls, pillars, doorways

static constexpr uint32 STARTER_DOOR_DECOR_ID_ALLIANCE = 9144;   // Founders' Front Door (GO 575017)
static constexpr uint32 STARTER_DOOR_DECOR_ID_HORDE    = 10952;  // Thornwind Front Door (GO 587318)

// Both starter doors are granted to every house; the placed one anchors the interior entry point.
inline bool IsStarterDoorDecor(uint32 decorEntryId)
{
    return decorEntryId == STARTER_DOOR_DECOR_ID_ALLIANCE || decorEntryId == STARTER_DOOR_DECOR_ID_HORDE;
}
static constexpr uint32 INTERIOR_DOOR_GO_ALLIANCE = 575017; // displayId 113554
static constexpr uint32 INTERIOR_DOOR_GO_HORDE    = 587318;

// Housing blueprints: enum values are the client's; limits are the Constants.HousingConsts values.

enum class HousingBlueprintType : uint8
{
    None     = 0,
    House    = 1,
    Room     = 2,
    Interior = 3,
    Exterior = 4,
};

enum HousingBlueprintFlag : uint8
{
    HOUSING_BLUEPRINT_FLAG_NONE             = 0x0,
    HOUSING_BLUEPRINT_FLAG_AUTOMATIC_BACKUP = 0x1,  // client: isAutoSave = flags & 1
};

enum class HousingBlueprintContentType : uint8
{
    None      = 0,
    HouseType = 1,
    Room      = 2,
    Decor     = 3,
    Dye       = 4,
    Fixture   = 5,
    Other     = 6,
};

enum HousingBlueprintUnmetRequirementFlags : uint32
{
    HOUSING_BLUEPRINT_UNMET_NONE                        = 0x00,
    HOUSING_BLUEPRINT_UNMET_INSUFFICIENT_BUDGET         = 0x01,
    HOUSING_BLUEPRINT_UNMET_MISSING_ROOM                = 0x02,
    HOUSING_BLUEPRINT_UNMET_MISSING_FIXTURE             = 0x04,
    HOUSING_BLUEPRINT_UNMET_MISSING_DECOR               = 0x08,
    HOUSING_BLUEPRINT_UNMET_MISSING_DYE                 = 0x10,
    HOUSING_BLUEPRINT_UNMET_MISMATCHED_EXTERIOR_FACTION = 0x20,
    HOUSING_BLUEPRINT_UNMET_HOUSE_TYPE_LOCKED           = 0x40,
    HOUSING_BLUEPRINT_UNMET_HOUSE_SIZE_LOCKED           = 0x80,
    // The client derives blockingRequirementFlags as unmet & 0xE7: missing decor/dyes do not stop an import.
    HOUSING_BLUEPRINT_UNMET_BLOCKING_MASK               = 0xE7,
};

enum class HousingBudgetType : uint8
{
    RoomPlacement  = 0,
    DecorPlacement = 1,
    PetDecor       = 2,
};

static constexpr uint32 HOUSING_BLUEPRINTS_MAX_PER_BNET_ACCOUNT         = 50;
static constexpr uint32 HOUSING_BLUEPRINTS_MAX_BACKUPS_PER_BNET_ACCOUNT = 10;
static constexpr uint32 HOUSING_BLUEPRINT_NAME_MIN_CHARACTERS           = 3;
static constexpr uint32 HOUSING_BLUEPRINT_NAME_MAX_CHARACTERS           = 50;

// The post-tutorial auras are re-sent whenever the player enters either housing map.
class Player;
TC_GAME_API void SendHousingPostTutorialAuras(Player* player);

#endif // TRINITYCORE_HOUSING_DEFINES_H
