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

// HousingResult enum - 12.1.0.69587 client values (Enum.HousingResult, 112 values). 12.1 inserted AccountBanned and the
// Blueprint* results near the top, shifting every later value; the client's blueprint system itself returns 3, 7, 8,
// 11, 12, 13, 87, 95 and 99 for CodeInvalid, LocationInvalid, NameInvalid, RoomPlacementRequired, TypeInvalid,
// TypeLocationInvalid, PermissionDenied, RoomNotFound and ServiceNotAvailable.
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

// HouseEditorMode enum - 7 values
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

// HouseEditingContext enum - 4 values
enum HouseEditingContext : uint8
{
    HOUSE_EDITING_CONTEXT_NONE      = 0,
    HOUSE_EDITING_CONTEXT_DECOR     = 1,
    HOUSE_EDITING_CONTEXT_ROOM      = 2,
    HOUSE_EDITING_CONTEXT_FIXTURE   = 3
};

// HousingFixtureSize enum - 5 values
enum HousingFixtureSize : uint8
{
    HOUSING_FIXTURE_SIZE_NONE       = 0,
    HOUSING_FIXTURE_SIZE_ANY        = 1,
    HOUSING_FIXTURE_SIZE_SMALL      = 2,
    HOUSING_FIXTURE_SIZE_MEDIUM     = 3,
    HOUSING_FIXTURE_SIZE_LARGE      = 4
};

// HousingFixtureType enum - 9 values (sparse)
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

// HousingRoomComponentType enum - 8 values
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

// HousingRoomComponentDoorType enum - 3 values
enum HousingRoomComponentDoorType : uint8
{
    HOUSING_ROOM_DOOR_TYPE_NONE         = 0,
    HOUSING_ROOM_DOOR_TYPE_DOORWAY      = 1,
    HOUSING_ROOM_DOOR_TYPE_THRESHOLD    = 2
};

// HousingRoomComponentCeilingType enum - 2 values
enum HousingRoomComponentCeilingType : uint8
{
    HOUSING_ROOM_CEILING_TYPE_FLAT      = 0,
    HOUSING_ROOM_CEILING_TYPE_VAULTED   = 1
};

// HousingRoomComponentStairType enum - 5 values
enum HousingRoomComponentStairType : uint8
{
    HOUSING_ROOM_STAIR_TYPE_NONE            = 0,
    HOUSING_ROOM_STAIR_TYPE_START_TO_END    = 1,
    HOUSING_ROOM_STAIR_TYPE_START_TO_MIDDLE = 2,
    HOUSING_ROOM_STAIR_TYPE_MIDDLE_TO_MIDDLE = 3,
    HOUSING_ROOM_STAIR_TYPE_MIDDLE_TO_END   = 4
};

// HousingRoomComponentOptionType enum - 3 values
enum HousingRoomComponentOptionType : uint8
{
    HOUSING_ROOM_COMPONENT_OPTION_COSMETIC      = 0,
    HOUSING_ROOM_COMPONENT_OPTION_DOORWAY_WALL  = 1,
    HOUSING_ROOM_COMPONENT_OPTION_DOORWAY       = 2
};

// DecorSourceType — identifies how a decor item was acquired.
// IDA-verified: client reads SourceType as uint8 and SourceValue as SizedCString from DecorStoragePersistedData.
// Retail sniff examples:
//   SourceType=5, SourceValue="1250393"            → spell-acquired (spell ID as string)
//   SourceType=6, SourceValue="3713-0-40000009CD1F16CB" → item-acquired (item GUID as string)
enum DecorSourceType : uint8
{
    DECOR_SOURCE_STANDARD       = 0, // Default / starter decor / placed
    DECOR_SOURCE_SPELL          = 5, // Acquired via spell cast (SourceValue = spell ID string)
    DECOR_SOURCE_ITEM           = 6, // Acquired via item use (SourceValue = item GUID string)
    DECOR_SOURCE_DEFERRED       = 3, // Redeemed from deferred reward queue
};

// HousingCatalogEntryType enum - 3 values
enum HousingCatalogEntryType : uint8
{
    HOUSING_CATALOG_ENTRY_INVALID   = 0,
    HOUSING_CATALOG_ENTRY_DECOR     = 1,
    HOUSING_CATALOG_ENTRY_ROOM      = 2
};

// HousingCatalogEntrySize enum - 6 values
enum HousingCatalogEntrySize : uint8
{
    HOUSING_CATALOG_SIZE_NONE       = 0,
    HOUSING_CATALOG_SIZE_TINY       = 65,
    HOUSING_CATALOG_SIZE_SMALL      = 66,
    HOUSING_CATALOG_SIZE_MEDIUM     = 67,
    HOUSING_CATALOG_SIZE_LARGE      = 68,
    HOUSING_CATALOG_SIZE_HUGE       = 69
};

// HousingDecorTheme enum - 6 values
enum HousingDecorTheme : uint8
{
    HOUSING_DECOR_THEME_NONE        = 0,
    HOUSING_DECOR_THEME_FOLK        = 1,
    HOUSING_DECOR_THEME_RUGGED      = 2,
    HOUSING_DECOR_THEME_GENERIC     = 3,
    HOUSING_DECOR_THEME_NIGHT_ELF   = 4,
    HOUSING_DECOR_THEME_BLOOD_ELF   = 5
};

// RoomConnectionType enum - 2 values
enum RoomConnectionType : uint8
{
    ROOM_CONNECTION_NONE    = 0,
    ROOM_CONNECTION_ALL     = 1
};

// HousingRoomFlags enum - 5 values (bitmask)
enum HousingRoomFlags : uint32
{
    HOUSING_ROOM_FLAG_NONE                  = 0x00,
    HOUSING_ROOM_FLAG_BASE_ROOM             = 0x01,
    HOUSING_ROOM_FLAG_HAS_STAIRS            = 0x02,
    HOUSING_ROOM_FLAG_UNLOCKED_BY_DEFAULT   = 0x04,
    HOUSING_ROOM_FLAG_HAS_CUSTOM_GEOMETRY   = 0x08
};

// HousingLayoutRestriction enum - 10 values
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

// NeighborhoodInviteResult enum - 11 values (0-10), verified against client binary
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

// Enum.BulkRefundResult (12.1.0.69587, BattlepayConstantsDocumentation.lua)
enum BulkRefundResult : uint8
{
    BULK_REFUND_RESULT_SUCCESS                  = 0,
    BULK_REFUND_RESULT_FAILED                   = 1,
    BULK_REFUND_RESULT_INVALID_REQUEST          = 2,
    BULK_REFUND_RESULT_REFUND_WINDOW_EXPIRED    = 3,
    BULK_REFUND_RESULT_SYSTEM_DISABLED          = 4,
    BULK_REFUND_RESULT_TIMEOUT                  = 5
};

// HouseOwnerError enum - 4 values
enum HouseOwnerError : uint8
{
    HOUSE_OWNER_ERROR_NONE                  = 0,
    HOUSE_OWNER_ERROR_FACTION               = 1,
    HOUSE_OWNER_ERROR_GUILD                 = 2,
    HOUSE_OWNER_ERROR_GENERIC_PERMISSION    = 3
};

// CreateNeighborhoodErrorType enum - 4 values
enum CreateNeighborhoodErrorType : uint8
{
    CREATE_NEIGHBORHOOD_ERROR_NONE              = 0,
    CREATE_NEIGHBORHOOD_ERROR_PROFANITY         = 1,
    CREATE_NEIGHBORHOOD_ERROR_UNDERSIZED_GUILD  = 2,
    CREATE_NEIGHBORHOOD_ERROR_OVERSIZED_GUILD   = 3
};

// NeighborhoodMemberRole enum - 3 values
enum NeighborhoodMemberRole : uint8
{
    NEIGHBORHOOD_ROLE_RESIDENT  = 0,
    NEIGHBORHOOD_ROLE_MANAGER   = 1,
    NEIGHBORHOOD_ROLE_OWNER     = 2
};

// NeighborhoodFactionRestriction enum - 3 values
enum NeighborhoodFactionRestriction : int32
{
    NEIGHBORHOOD_FACTION_NONE       = 0,
    NEIGHBORHOOD_FACTION_HORDE      = 1,
    NEIGHBORHOOD_FACTION_ALLIANCE   = 2
};

// closedInfoFramesAccountWide is the client's FrameTutorialAccount bitfield, kept in the
// GLOBAL_CONFIG_CACHE account data as two space-separated uint32 words (48 bits; word 0 = bits 0-31,
// word 1 = bits 32-47). The housing editor keeps its expert/cleanup/layout/customize modes locked until
// bit 38 "HousingModesUnlocked" is set: 38 - 32 = 6 within word 1, so 1 << 6 = 64 -> "0 64".
//
// Set ONLY that bit. Writing the whole field (the old "4294967295 4294967295") also marked every other
// FrameTutorialAccount step as already seen and, together with housingTutorialsEnabled=0, told the client
// the housing tutorial was already finished - so a first-time buyer was dropped straight into the House
// Finder instead of being walked through it. Unlocking the editor modes was the only thing that change
// was ever meant to do.
constexpr char const* HOUSING_MODES_UNLOCKED_CVAR = "0 64";

// HouseSettingFlags enum - 11 values (bitmask), verified against client binary
// Two groups: HouseAccess (bits 0-4) for interior, PlotAccess (bits 5-9) for exterior
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
    // 12.1 (Enum.HouseSettingFlags registration 0x7FF7CE1216E0): who may export this house as a blueprint.
    HOUSE_SETTING_BLUEPRINT_EXPORT_ANYONE   = 0x400,
    HOUSE_SETTING_BLUEPRINT_EXPORT_NEIGHBORS = 0x800,
    HOUSE_SETTING_BLUEPRINT_EXPORT_GUILD    = 0x1000,
    HOUSE_SETTING_BLUEPRINT_EXPORT_FRIENDS  = 0x2000,
    HOUSE_SETTING_BLUEPRINT_EXPORT_PARTY    = 0x4000
};

constexpr uint32 HOUSE_SETTING_DEFAULT    = HOUSE_SETTING_PLOT_ACCESS_ANYONE; // 0x020 — sniff-verified default
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

// SMSG_HOUSING_GET_PLAYER_PERMISSIONS_RESPONSE flags, retail 12.1.0.69933: 0xFE in all 5 responses for the player's
// own house; 0x10 in 8 of the 16 for other houses (the rest 0x18 or 0, rules not yet known).
constexpr uint8 HOUSING_PERMISSIONS_OWNER   = 0xFE;
constexpr uint8 HOUSING_PERMISSIONS_VISITOR = 0x10;

// HouseDecor.db2 Flags bit carried by licensed (shop / promotional) decor. Retail 12.1.0.69933 lists only such
// entries in SMSG_GET_ALL_LICENSED_DECOR_QUANTITIES_RESPONSE (all 8 captured have Flags 0x503 or 0x403);
// ordinary decor (Flags 0x3) never appears there.
constexpr int32 HOUSE_DECOR_FLAG_LICENSED = 0x400;

// HousingRoomSize enum - 3 values
enum HousingRoomSize : int8
{
    ROOM_SIZE_SMALL     = 0,
    ROOM_SIZE_MEDIUM    = 1,
    ROOM_SIZE_LARGE     = 2
};

// HousingPlotSize enum - 3 values
enum HousingPlotSize : int32
{
    PLOT_SIZE_SMALL     = 0,
    PLOT_SIZE_MEDIUM    = 1,
    PLOT_SIZE_LARGE     = 2
};

// HousingInitiativeType enum - 4 values
enum HousingInitiativeType : int32
{
    INITIATIVE_TYPE_GATHERING       = 0,
    INITIATIVE_TYPE_CRAFTING        = 1,
    INITIATIVE_TYPE_COMBAT          = 2,
    INITIATIVE_TYPE_EXPLORATION     = 3
};

// HousingFixtureFlags enum - 3 values (bitmask)
enum HousingFixtureFlags : uint32
{
    HOUSING_FIXTURE_FLAG_NONE               = 0x00,
    HOUSING_FIXTURE_FLAG_IS_DEFAULT         = 0x01,
    HOUSING_FIXTURE_FLAG_UNLOCKED_BY_DEFAULT = 0x02
};

// HousingRoomComponentFlags enum - 2 values (bitmask)
enum HousingRoomComponentFlags : uint32
{
    HOUSING_ROOM_COMPONENT_FLAG_NONE                    = 0x00,
    HOUSING_ROOM_COMPONENT_FLAG_HIDDEN_IN_LAYOUT_MODE   = 0x01
};

// HousingDecorPlacementRestriction enum - 7 values (bitmask) - 12.0.7 (68275) client-verified,
// server-sent placement-failure reasons. HOUSING_ENUMS_68275.md.
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

// HousingRoomComponentOptionFlags enum - 2 values (bitmask)
enum HousingRoomComponentOptionFlags : uint32
{
    HOUSING_ROOM_COMPONENT_OPTION_FLAG_NONE         = 0x00,
    HOUSING_ROOM_COMPONENT_OPTION_FLAG_IS_DEFAULT   = 0x01
};

// HousingRoomComponentTextureFlags enum - 2 values (bitmask)
enum HousingRoomComponentTextureFlags : uint32
{
    HOUSING_ROOM_COMPONENT_TEXTURE_FLAG_NONE                    = 0x00,
    HOUSING_ROOM_COMPONENT_TEXTURE_FLAG_UNLOCKED_BY_DEFAULT     = 0x01
};

// NeighborhoodFlags enum - 3 values (bitmask)
enum NeighborhoodFlags : uint32
{
    NEIGHBORHOOD_FLAG_NONE              = 0x00,
    NEIGHBORHOOD_FLAG_POOL_PARENT       = 0x01,
    NEIGHBORHOOD_FLAG_OPEN_TO_PUBLIC    = 0x02
};

// HouseExteriorWMODataFlags enum - 4 values (bitmask)
enum HouseExteriorWMODataFlags : uint32
{
    HOUSE_EXTERIOR_WMO_FLAG_NONE                            = 0x00,
    HOUSE_EXTERIOR_WMO_FLAG_UNLOCKED_BY_DEFAULT             = 0x01,
    HOUSE_EXTERIOR_WMO_FLAG_ALLOWED_IN_HORDE_NEIGHBORHOODS  = 0x02,
    HOUSE_EXTERIOR_WMO_FLAG_ALLOWED_IN_ALLIANCE_NEIGHBORHOODS = 0x04
};

// ============================================================================
// New enums from client binary analysis (previously missing)
// ============================================================================

// HousingDecorModelType enum - 3 values
enum HousingDecorModelType : uint8
{
    HOUSING_DECOR_MODEL_TYPE_NONE   = 0,
    HOUSING_DECOR_MODEL_TYPE_M2     = 1,
    HOUSING_DECOR_MODEL_TYPE_WMO    = 2
};

// NeighborhoodInitiativeUpdateStatus enum — sent via SMSG_INITIATIVE_UPDATE_STATUS
enum NeighborhoodInitiativeUpdateStatus : uint8
{
    NI_UPDATE_STATUS_STARTED                = 0,
    NI_UPDATE_STATUS_MILESTONE_COMPLETED    = 1,
    NI_UPDATE_STATUS_COMPLETED              = 2,
    NI_UPDATE_STATUS_FAILED                 = 3
};

// NeighborhoodInitiativeChestResult enum — sent via SMSG_INITIATIVE_CHEST_RESULT
enum NeighborhoodInitiativeChestResult : uint32
{
    NI_CHEST_SUCCESS                = 0,
    NI_CHEST_UNSPECIFIED_FAILURE    = 1,
    NI_CHEST_NO_HOUSE_FOUND        = 2,
    NI_CHEST_NO_REWARDS             = 3,
    NI_CHEST_THROTTLED              = 4,
    NI_CHEST_SERVICE_DISABLED       = 5
};

// NeighborhoodInitiativeTaskType enum — from InitiativeTask DB2 TaskType field
enum NeighborhoodInitiativeTaskType : int32
{
    NI_TASK_TYPE_SINGLE                 = 0,
    NI_TASK_TYPE_REPEATABLE_FINITE      = 1,
    NI_TASK_TYPE_REPEATABLE_INFINITE    = 2
};

// NeighborhoodInitiativeCompletionState enum — per-task completion state
enum NeighborhoodInitiativeCompletionState : uint8
{
    NI_COMPLETION_NOT_COMPLETED         = 0,
    NI_COMPLETION_PLAYER_COMPLETED      = 1,
    NI_COMPLETION_SYSTEM_ABANDONED      = 2
};

// NeighborhoodInitiativeFlags enum — from NeighborhoodInitiative DB2 Flags field
enum NeighborhoodInitiativeFlags : uint32
{
    NI_FLAG_DISABLED    = 0x1,
    NI_FLAG_NO_ABANDON  = 0x2,
    NI_FLAG_NO_REPEAT   = 0x4
};

// InitiativeMilestoneFlags enum — from InitiativeMilestone DB2 Flags field
enum InitiativeMilestoneFlags : int32
{
    INITIATIVE_MILESTONE_FLAG_FINAL = 0x1
};

// InitiativeRewardFlags enum — from InitiativeReward DB2 Flags field
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

// HousingFavorUpdateSource enum - 8 values
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

// HousingFavorUpdateType enum - 3 values
enum HousingFavorUpdateType : uint8
{
    HOUSING_FAVOR_UPDATE_NONE           = 0,
    HOUSING_FAVOR_UPDATE_INITIATIVE_ADD = 1,
    HOUSING_FAVOR_UPDATE_SET            = 2
};

// HousingPlotOwnerType enum - 4 values
enum HousingPlotOwnerType : uint8
{
    HOUSING_PLOT_OWNER_NONE     = 0,
    HOUSING_PLOT_OWNER_STRANGER = 1,
    HOUSING_PLOT_OWNER_FRIEND   = 2,
    HOUSING_PLOT_OWNER_SELF     = 3
};

// HousingTeleportReason enum - 12 values
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

// HousingThrottleType enum - 2 values
enum HousingThrottleType : uint8
{
    HOUSING_THROTTLE_GENERAL    = 0,
    HOUSING_THROTTLE_DECORATION = 1
};

// HousingThemeFlags enum - 3 values (bitmask)
enum HousingThemeFlags : uint32
{
    HOUSING_THEME_FLAG_NONE                     = 0x00,
    HOUSING_THEME_FLAG_UNLOCKED_BY_DEFAULT      = 0x01,
    HOUSING_THEME_FLAG_SHOW_IN_STYLE_SELECTOR   = 0x02
};

// NeighborhoodMapFlags enum - 4 values (bitmask)
enum NeighborhoodMapFlags : uint32
{
    NEIGHBORHOOD_MAP_FLAG_NONE                  = 0x00,
    NEIGHBORHOOD_MAP_FLAG_ALLIANCE_PURCHASABLE  = 0x01,
    NEIGHBORHOOD_MAP_FLAG_HORDE_PURCHASABLE     = 0x02,
    NEIGHBORHOOD_MAP_FLAG_CAN_SYSTEM_GENERATE   = 0x04
};

// NeighborhoodOwnerType enum - 3 values
enum NeighborhoodOwnerType : uint8
{
    NEIGHBORHOOD_OWNER_NONE     = 0,
    NEIGHBORHOOD_OWNER_GUILD    = 1,
    NEIGHBORHOOD_OWNER_CHARTER  = 2
};

// NeighborhoodType enum - 3 values
enum NeighborhoodType : uint8
{
    NEIGHBORHOOD_TYPE_OPEN      = 0,
    NEIGHBORHOOD_TYPE_PRIVATE   = 1,
    NEIGHBORHOOD_TYPE_PUBLIC    = 2
};

// PurchaseHouseDisabledReason enum - 10 values
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

// ReservationFlags enum - 4 values (bitmask)
enum ReservationFlags : uint32
{
    RESERVATION_FLAG_NONE       = 0x00,
    RESERVATION_FLAG_RELINQUISH = 0x01,
    RESERVATION_FLAG_CANCELED   = 0x02,
    RESERVATION_FLAG_PLOTLESS   = 0x04
};

// RetroactiveDecorRewardFlags enum - 2 values (bitmask)
enum RetroactiveDecorRewardFlags : uint32
{
    RETROACTIVE_DECOR_REWARD_FLAG_NONE                  = 0x00,
    RETROACTIVE_DECOR_REWARD_FLAG_ALL_CRITERIA_REQUIRED = 0x01
};

// InvalidPlotScreenshotReason enum - 5 values
enum InvalidPlotScreenshotReason : uint8
{
    INVALID_PLOT_SCREENSHOT_NONE                = 0,
    INVALID_PLOT_SCREENSHOT_OUT_OF_BOUNDS       = 1,
    INVALID_PLOT_SCREENSHOT_FACING              = 2,
    INVALID_PLOT_SCREENSHOT_NO_NEIGHBORHOOD     = 3,
    INVALID_PLOT_SCREENSHOT_NO_ACTIVE_PLAYER    = 4
};

// HouseFinderSuggestionReason enum - 7 values (bitmask)
enum HouseFinderSuggestionReason : uint32
{
    HOUSE_FINDER_SUGGESTION_NONE            = 0x00,
    HOUSE_FINDER_SUGGESTION_OWNER           = 0x01,
    HOUSE_FINDER_SUGGESTION_CHARTER_INVITE  = 0x02,
    HOUSE_FINDER_SUGGESTION_GUILD           = 0x04,
    HOUSE_FINDER_SUGGESTION_BNET_FRIENDS    = 0x08,
    HOUSE_FINDER_SUGGESTION_PARTY_SYNC      = 0x10,
    HOUSE_FINDER_SUGGESTION_RANDOM          = 0x20
};

// CornerstonePurchaseMode enum - 3 values
enum CornerstonePurchaseMode : uint8
{
    CORNERSTONE_PURCHASE_BASIC  = 0,
    CORNERSTONE_PURCHASE_IMPORT = 1,
    CORNERSTONE_PURCHASE_MOVE   = 2
};

// HouseLevelRewardType enum - 2 values
enum HouseLevelRewardType : uint8
{
    HOUSE_LEVEL_REWARD_VALUE    = 0,
    HOUSE_LEVEL_REWARD_OBJECT   = 1
};

// HouseVisitType enum - 4 values
enum HouseVisitType : uint8
{
    HOUSE_VISIT_UNKNOWN = 0,
    HOUSE_VISIT_FRIEND  = 1,
    HOUSE_VISIT_GUILD   = 2,
    HOUSE_VISIT_PARTY   = 3
};

// HousingItemToastType enum - 5 values
enum HousingItemToastType : uint8
{
    HOUSING_ITEM_TOAST_ROOM          = 0,
    HOUSING_ITEM_TOAST_FIXTURE       = 1,
    HOUSING_ITEM_TOAST_CUSTOMIZATION = 2,
    HOUSING_ITEM_TOAST_DECOR         = 3,
    HOUSING_ITEM_TOAST_HOUSE         = 4
};

// HousingRoomComponentFloorType enum - 1 value
enum HousingRoomComponentFloorType : uint8
{
    HOUSING_ROOM_COMPONENT_FLOOR_TYPE_FLOOR = 0
};

// HousingDecorType enum - 5 values
enum HousingDecorType : uint8
{
    HOUSING_DECOR_TYPE_NONE     = 0,
    HOUSING_DECOR_TYPE_FLOOR    = 1,
    HOUSING_DECOR_TYPE_WALL     = 2,
    HOUSING_DECOR_TYPE_CEILING  = 3,
    HOUSING_DECOR_TYPE_FLOORING = 4
};

// HouseLevelRewardValueType enum - 4 values
enum HouseLevelRewardValueType : uint8
{
    HOUSE_LEVEL_REWARD_EXTERIOR_DECOR   = 0,
    HOUSE_LEVEL_REWARD_INTERIOR_DECOR   = 1,
    HOUSE_LEVEL_REWARD_ROOMS            = 2,
    HOUSE_LEVEL_REWARD_FIXTURES         = 3
};

// Constants
// M1/A4 spatial-validation bound. Decor positions are world coordinates (what the client sends and what
// character_housing_decor stores), so they are measured from Housing::GetDecorPlacementAnchor: the interior
// origin for interior decor, the owner's position for plot decor. A plot or interior is a few tens of yards
// across; this half-extent never rejects a legitimate placement but still refuses arbitrary-coordinate spam.
static constexpr float HOUSING_MAX_DECOR_LOCAL_EXTENT  = 1024.0f;
// #16 Outdoor Lighting (12.0.7): DecorCategory.db2 id 4 "Lighting" (subcategories
// 16-21: Large/Wall/Ceiling/Small/Misc Lights). 12.0.7 lets Lighting decor be
// placed outdoors on the plot; the placement path classifies a decor as Lighting
// through DecorXDecorSubcategory -> DecorSubcategory.DecorCategoryID.
static constexpr uint32 HOUSING_DECOR_CATEGORY_LIGHTING = 4;
// A4 / RETAIL PARITY-OUTDOOR-LIGHT-RADIUS: 12.0.7 rule "two lights cannot overlap".
// The exact light-to-light overlap radius is NOT datamineable from DB2 or any
// capture we hold (CAPTURE-BLOCKED). This is a documented default minimum
// separation between two exterior lights, in local decor space (yards) — replace
// with the sniffed value once an outdoor-light placement capture exists.
static constexpr float HOUSING_LIGHT_OVERLAP_RADIUS = 3.0f;

// H-05 bound for CMSG_HOUSE_EXTERIOR_SET_HOUSE_POSITION. A player may nudge the
// house around its own plot; they may not relocate it. The plot's placement
// volume is the RoomWmoData geobox SpawnRoomForPlot uses (~+/-35 x +/-30 yards),
// so these half-extents are deliberately a little wider than that - generous
// enough never to reject a legitimate reposition, tight enough that the house
// cannot be parked on a neighbour's plot or flung off the map. Before this the
// handler validated std::isfinite() and nothing else, and the value was
// persisted, so any finite coordinate survived a restart.
static constexpr float HOUSING_MAX_HOUSE_PLOT_OFFSET_XY = 45.0f;
static constexpr float HOUSING_MAX_HOUSE_PLOT_OFFSET_Z  = 50.0f;
// m3/A6 decoration throttle: at most BURST place/move/remove ops per WINDOW_MS.
// Generous enough for rapid legitimate redecorating, tight enough to cap the
// AddToMap + synchronous-DB-write amplification a scripted client can drive.
static constexpr uint32 HOUSING_DECOR_THROTTLE_WINDOW_MS = 10000;
static constexpr uint32 HOUSING_DECOR_THROTTLE_BURST     = 40;
static constexpr uint32 MAX_HOUSING_DECOR_PER_ROOM      = 50;
static constexpr uint32 MAX_HOUSING_ROOMS_PER_HOUSE     = 20;
static constexpr uint32 MAX_HOUSING_DYE_SLOTS           = 3;
static constexpr uint32 MAX_NEIGHBORHOOD_PLOTS          = 55;
static constexpr uint32 MAX_NEIGHBORHOOD_MANAGERS       = 5;
static constexpr uint32 MAX_PENDING_INVITES             = 20;
static constexpr uint32 MIN_CHARTER_SIGNATURES          = 4;
static constexpr uint8  INVALID_PLOT_INDEX              = 255;
static constexpr uint32 HOUSING_MAX_NAME_LENGTH         = 64;
static constexpr uint64 HOUSE_MOVE_COST_COPPER          = 500ULL * 10000ULL;       // 500g move cost
static constexpr uint32 MAX_HOUSE_LEVEL                 = 20;

// Starter favor granted on house purchase (sniff: ChangeAmount=910, NewFavorTotal=910 in the
// post-purchase HousingSvcsUpdateHousesLevelFavor pair).
static constexpr uint64 HOUSE_PURCHASE_STARTER_FAVOR    = 910;

// Neighborhood initiative ("Endeavor" in the client UI) progress is reported to the client on a
// 0..1000 point scale — sniff-verified: PlayerInitiativeInfo.ProgressRequired == 1000.
// InitiativeTask.ProgressContributionAmount (12.0.7 DB2 values 10/25/50/75/100/150/300) is how
// many of those points ONE completion of that task is worth.
static constexpr float INITIATIVE_PROGRESS_REQUIRED     = 1000.0f;

// InitiativeMilestone.RequiredContributionAmount is a PERCENTAGE of initiative completion, not a
// 0..1 fraction: the 12.0.7 DB2 holds exactly 25/50/75/100 and only the 100.0 rows carry
// INITIATIVE_MILESTONE_FLAG_FINAL. ActiveInitiative::Progress is a 0..1 fraction, so it has to be
// scaled by this before being compared against a milestone threshold.
static constexpr float INITIATIVE_MILESTONE_SCALE       = 100.0f;

// Floating world-text shown when a player earns neighborhood contribution credit. Reproduced
// byte-for-byte from the retail build-68275 housing capture (housing12.0.7.pkt, four
// SMSG_DISPLAY_WORLD_TEXT records, each immediately followed by the SMSG_CRITERIA_UPDATE batch for
// the deed that earned it): null anchor guid, Arg1 = Arg2 = 0, and this exact 34-byte string. The
// colour token is resolved client-side; the "+Neighborly" wording matches the CriteriaTree strings
// "Neighborly deeds performed" / "Good Neighbor Points". Only the enUS sample exists, so this is not
// localized — retail presumably sends the client's locale here.
constexpr char const HOUSING_WORLD_TEXT_NEIGHBORLY[] = "|cnYELLOW_FONT_COLOR:+Neighborly|r";

// Quest 91863 objective 17 ("Acquire a house") kill credit, granted on successful purchase.
static constexpr uint32 NPC_KILL_CREDIT_BUY_HOME        = 248858;

// Spell applied during housing decor edit mode (creates "phased-out" visual effect)
// Sniff: aura slot 51, Flags=NoCaster, ActiveFlags=15, CastLevel=36
static constexpr uint32 SPELL_HOUSING_EDIT_MODE_AURA    = 1263303;

// "[DNT] Decorating - Disable All the Things - Room Editor": room layout mode (stun + no gravity, pacify, immunity)
// Sniff: aura slot 116, Flags=NoCaster, ActiveFlags=15, cast on CMSG_HOUSING_ROOM_SET_LAYOUT_EDIT_MODE
static constexpr uint32 SPELL_HOUSING_ROOM_EDIT_MODE_AURA = 1263316;

// "Leave House" (effect 343): cast by the interior door after it opens; takes the player out to the plot
static constexpr uint32 SPELL_HOUSING_LEAVE_HOUSE       = 1234193;
// 10 s casts that end in SPELL_EFFECT_TELEPORT_UNITS to a plot (12.1.0.69933 sniff 19-48-29)
static constexpr uint32 SPELL_HOUSING_TELEPORT_HOME     = 1233637; // CMSG_HOUSING_SVCS_TELEPORT_TO_PLOT to one's own plot
static constexpr uint32 SPELL_HOUSING_VISIT_HOUSE       = 1265142; // house finder "Visit" (after the plot reservation) and other plots

// Spell applied when player enters their own housing plot
// Sniff: aura slot 50/55, Flags=NoCaster, ActiveFlags=1-2, CastLevel=36
static constexpr uint32 SPELL_HOUSING_PLOT_ENTER        = 1239847;

// Second spell applied when player enters their own housing plot
// Sniff: aura slot 56, Flags=NoCaster, ActiveFlags=1, CastLevel=36
static constexpr uint32 SPELL_HOUSING_PLOT_PRESENCE     = 469226;

// Third spell applied on first plot enter — replaces slot 9 aura
// Sniff: aura slot 9, Flags=NoCaster|Scalable(9), ActiveFlags=1, CastLevel=36, has PointsCount
static constexpr uint32 SPELL_HOUSING_PLOT_ENTER_2      = 1266699;

// Neighborhood map-entry auras — 4 housing-specific auras applied immediately
// after the big SMSG_UPDATE_OBJECT batch at neighborhood-map entry. Decoded
// from dump_12.0.1.66838_2026-04-15_09-35-59.pkt idx 9985-10000 (and
// cross-checked against the 2026-04-10 capture at idx 15673-15690).
// Slot/Flags/ActiveFlags/Applications/SpellVisual each sniff-verified.
static constexpr uint32 SPELL_HOUSING_MAP_ENTRY_FIXUP      = 1272741;  // "Housing Fixup Aura"
static constexpr uint32 SPELL_HOUSING_MAP_ENTRY_REACT      = 1263578;  // "Player Action React (DNT)"
static constexpr uint32 SPELL_HOUSING_MAP_ENTRY_ENDEAVOR   = 1276064;  // "[DNT] Endeavor Cover Aura"
static constexpr uint32 SPELL_HOUSING_MAP_ENTRY_NEIGHBOR   = 1227147;  // "In Your Neighborhood"
// Cast on the player after a house is bought or moved to a new plot (retail 12.1.0.69933, right after
// SMSG_NEIGHBORHOOD_MOVE_HOUSE_RESPONSE). Triggers 1248306 (kill credit 248858), 1253658 (quest 92486),
// 1253555 and 1260705 (aura 430: scene 3819, the move-in cutscene).
static constexpr uint32 SPELL_HOUSING_HOUSE_ACQUIRED       = 1253572;
// SpellXSpellVisualID baked into spell 1227147's AuraDataInfo.Visual on retail.
static constexpr uint32 VISUAL_HOUSING_MAP_ENTRY_NEIGHBOR  = 503683;

// Quest that completes the housing tutorial. Once turned in, the player is granted the
// post-tutorial aura set and all editor modes (expert/cleanup/layout/customize) unlock.
static constexpr uint32 QUEST_HOUSING_TUTORIAL_COMPLETE = 94455; // "Home at Last"

// Post-tutorial auras — applied when QUEST_HOUSING_TUTORIAL_COMPLETE is completed.
// Sniff-verified: quest reward removes old tutorial auras (slots 8,9,50) and replaces them
// with these three new ones. These don't exist in DB2, so we send manual SMSG_AURA_UPDATE.
// Slot 8: Flags=NoCaster, ActiveFlags=1, CastLevel=36
static constexpr uint32 SPELL_HOUSING_TUTORIAL_DONE_1   = 1285428;
// Slot 9: Flags=NoCaster, ActiveFlags=1, CastLevel=36
static constexpr uint32 SPELL_HOUSING_TUTORIAL_DONE_2   = 1285424;
// Slot 50: Flags=NoCaster|Scalable, ActiveFlags=1, CastLevel=36, Points=[1]
// Note: Same spell ID as SPELL_HOUSING_PLOT_ENTER_2 but applied at slot 50 (not slot 9)
static constexpr uint32 SPELL_HOUSING_TUTORIAL_DONE_3   = 1266699;

// WorldState IDs — continuous counters sent throughout the entire housing session.
// Sniff-verified: 5 counters total, sent as individual SMSG_UPDATE_WORLD_STATE packets.
// Counters 1-3 increment by ~1333 every ~300ms.
// Counters 4-5 increment by ~7233 every ~300ms.
static constexpr uint32 WORLDSTATE_HOUSING_COUNTER_1    = 13436;
static constexpr uint32 WORLDSTATE_HOUSING_COUNTER_2    = 13437;
static constexpr uint32 WORLDSTATE_HOUSING_COUNTER_3    = 13438;
static constexpr uint32 WORLDSTATE_HOUSING_COUNTER_4    = 16035;
static constexpr uint32 WORLDSTATE_HOUSING_COUNTER_5    = 16711;

// WS[30906]: Toggled 1 when inside a house interior (MapID=2783), 0 when leaving.
static constexpr uint32 WORLDSTATE_HOUSING_INTERIOR     = 30906;

// Interval and increment for housing WorldState counter updates
static constexpr uint32 HOUSING_WORLDSTATE_INTERVAL_MS  = 300;
static constexpr uint32 HOUSING_WORLDSTATE_INCREMENT    = 1333;
static constexpr uint32 HOUSING_WORLDSTATE_INCREMENT_2  = 7233;

// Cosmetic phases removed when a player enters their own housing plot and
// restored when they leave. Sniff-verified: 16 phases with ~10s delay.
static constexpr uint32 HOUSING_COSMETIC_PHASES[] =
{
    25571, 26216, 27429, 27442, 27489, 27695,
    28304, 28312, 28313, 28314, 28315, 28316,
    28320, 28339, 28370, 28748
};

static constexpr uint32 HOUSING_COSMETIC_PHASE_COUNT = sizeof(HOUSING_COSMETIC_PHASES) / sizeof(HOUSING_COSMETIC_PHASES[0]);

// Delay in milliseconds before cosmetic phase shifts take effect on plot enter/leave
static constexpr uint32 HOUSING_COSMETIC_PHASE_DELAY_MS = 10000;

// Room grid spacing for interior maps (sniff-verified: ~24 yards between room centers)
static constexpr float HOUSING_ROOM_GRID_SPACING = 24.0f;

// ------------------------------------------------------------------
// Horde House Interior Mesh Data (from retail sniff, HouseExteriorWmoDataID=87)
// ------------------------------------------------------------------
// Attachment hierarchy:
//   Root: House GO (spawned at plot position, e.g. entry 582075)
//     └── Building shell WMO (FileDataID 6322976, attached to house GO)
//           ├── Interior room WMOs (6426xxx, attached to building shell)
//           └── Exterior fixture M2s (attached to building shell)
//
// The client uses the house GO as the root anchor. MeshObjects carry
// FHousingFixture_C fragment data (ExteriorComponentID, HouseExteriorWmoDataID)
// that the client uses to resolve which art assets to render.

// Main building shell WMO (approx bounding box: ±35x30x126)
static constexpr int32 HORDE_HOUSE_BUILDING_SHELL_FDI = 6322976;

// Interior room WMOs — each approximately 24x24 unit rooms arranged on a 24-unit grid
// Vertical floor height: 7.0 units between stacked rooms
static constexpr int32 HORDE_HOUSE_INTERIOR_ROOM_FDIS[] = {
    6426613,    // Main room / wall section (also used as corner)
    6426431,    // Small room variant
    6426641,    // Small room variant
    6426647,    // Small room variant
    6426665,    // Large room corner
    6426605,    // Room ceiling
    6426671,    // Room wall with door opening
    6426452,    // Small room with specific configuration
    6426672     // Room section variant
};
static constexpr uint32 HORDE_HOUSE_INTERIOR_ROOM_COUNT = sizeof(HORDE_HOUSE_INTERIOR_ROOM_FDIS) / sizeof(HORDE_HOUSE_INTERIOR_ROOM_FDIS[0]);
static constexpr float  HORDE_HOUSE_FLOOR_HEIGHT = 7.0f;  // Vertical spacing between stacked rooms

// HouseExteriorWmoDataID for Horde theme (from sniff)
static constexpr int32 HORDE_HOUSE_EXTERIOR_WMO_DATA_ID = 87;

// Max players allowed on a housing map (exterior neighborhood + interior combined)
static constexpr uint32 MAX_HOUSING_MAP_PLAYERS = 40;

// Housing warning flags — reasons why housing features may be restricted
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

// Kill credit that completes QUEST_HOUSING_TUTORIAL_COMPLETE. Packet-attested in the retail
// Horde starter capture: creature "[DNT] Kill Credit: Housing - Tutorial - 01 - House Entered"
// is credited the moment the player first stands inside their house, and the quest - which is
// AUTO_ACCEPT|AUTO_COMPLETE and has no quest-giver NPC on either end - then auto-submits.
static constexpr uint32 NPC_HOUSING_TUTORIAL_HOUSE_ENTERED_CREDIT = 257763;

// House interior instance map (MAP_HOUSE_INTERIOR = 7)
static constexpr uint32 HOUSE_INTERIOR_MAP_ID = 2783;

// Vertical spacing between stacked interior rooms. Must match the value HouseInteriorMap
// actually positions rooms with (roomZ = origin + FloorIndex * this) or anything deriving a
// room origin from FloorIndex lands on the wrong floor.
static constexpr float HOUSE_INTERIOR_FLOOR_HEIGHT = 12.0f;

// Interior front-door GameObjects, picked by faction in HouseInteriorMap. Unlike the exterior
// doors these are NOT reachable from ExteriorComponent (Type 11), so HousingMgr has to bind the
// go_housing_door script to them explicitly - without it the door inside the house is inert.
// HouseDecor.ModelType
static constexpr uint8 HOUSE_DECOR_MODEL_TYPE_WMO = 2; // interior walls, pillars, doorways

static constexpr uint32 INTERIOR_DOOR_GO_ALLIANCE = 575017; // displayId 113554
static constexpr uint32 INTERIOR_DOOR_GO_HORDE    = 587318;

// ============================================================================
// Housing blueprints (12.1.0.69587). Enum values are the client's (HousingBlueprintConstantsDocumentation.lua,
// PlayerHousingConstantsDocumentation.lua); limits are the Constants.HousingConsts values the client registers
// (0x7FF7CE1254A0).
// ============================================================================

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
    // The client derives blockingRequirementFlags as unmet & 0xE7: missing decor and dyes do not stop an import, the
    // missing pieces are left out.
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

// Pet beds (12.1): decor items with their own placement budget. Caps come from client
// config globals housingMaxPetBedsInterior@0x127F60 / housingMaxPetBedsExterior@0x127FD0
// [BIN symbols]. Default caps are placeholders until a value capture/DB confirms. [INF]
static constexpr uint32 HOUSING_MAX_PET_BEDS_INTERIOR = 6;
static constexpr uint32 HOUSING_MAX_PET_BEDS_EXTERIOR = 6;

// 12.1 raised the displayed house level cap to 12 (patch notes). MAX_HOUSE_LEVEL above is
// already 20 (headroom); levels 11-12 are HouseLevelData.db2 rows + larger budgets +
// large-exterior unlock — a DATA change, not a code cap. [data]
static constexpr uint32 HOUSING_DISPLAY_LEVEL_CAP_12_1 = 12;

// The three post-tutorial auras (slots 8, 9, 50) are re-sent whenever the player enters either
// housing map, so the sequence lives in one place instead of being carried by both map classes.
class Player;
TC_GAME_API void SendHousingPostTutorialAuras(Player* player);


#endif // TRINITYCORE_HOUSING_DEFINES_H
