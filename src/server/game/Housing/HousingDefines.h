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

// Official HousingResult enum (90 values)
enum HousingResult : uint32
{
    HOUSING_RESULT_SUCCESS                          = 0,
    HOUSING_RESULT_ACTION_LOCKED_BY_COMBAT          = 1,
    HOUSING_RESULT_BOUNDS_FAILURE_CHILDREN           = 2,
    HOUSING_RESULT_BOUNDS_FAILURE_PLOT               = 3,
    HOUSING_RESULT_BOUNDS_FAILURE_ROOM               = 4,
    HOUSING_RESULT_CANNOT_AFFORD                     = 5,
    HOUSING_RESULT_CHARTER_COMPLETE                  = 6,
    HOUSING_RESULT_COLLISION_INVALID                  = 7,
    HOUSING_RESULT_DB_ERROR                          = 8,
    HOUSING_RESULT_DECOR_ALREADY_LOCKED              = 9,
    HOUSING_RESULT_DECOR_ALREADY_UNLOCKED            = 10,
    HOUSING_RESULT_DECOR_BUDGET_EXCEEDED             = 11,
    HOUSING_RESULT_DECOR_COUNT_EXCEEDED              = 12,
    HOUSING_RESULT_DECOR_INVALID_GUID                = 13,
    HOUSING_RESULT_DECOR_INVALID_STATE               = 14,
    HOUSING_RESULT_DECOR_NOT_ALLOWED_INDOORS         = 15,
    HOUSING_RESULT_DECOR_NOT_ALLOWED_OUTDOORS        = 16,
    HOUSING_RESULT_DECOR_NOT_OWNED                   = 17,
    HOUSING_RESULT_DECOR_NOT_PLACED                  = 18,
    HOUSING_RESULT_DECOR_NOT_UNLOCKED                = 19,
    HOUSING_RESULT_DOOR_IN_USE                       = 20,
    HOUSING_RESULT_DOOR_NOT_FOUND                    = 21,
    HOUSING_RESULT_EDIT_MODE_ALREADY_SET             = 22,
    HOUSING_RESULT_EDIT_MODE_NOT_ALLOWED             = 23,
    HOUSING_RESULT_EDIT_MODE_NOT_SET                 = 24,
    HOUSING_RESULT_EXTERIOR_INVALID_SIZE             = 25,
    HOUSING_RESULT_EXTERIOR_NO_HOUSE                 = 26,
    HOUSING_RESULT_EXTERIOR_NOT_ALLOWED              = 27,
    HOUSING_RESULT_FIXTURE_BUDGET_EXCEEDED           = 28,
    HOUSING_RESULT_FIXTURE_HOOK_OCCUPIED             = 29,
    HOUSING_RESULT_FIXTURE_HOOK_EMPTY                = 30,
    HOUSING_RESULT_FIXTURE_INVALID_DATA              = 31,
    HOUSING_RESULT_FIXTURE_NOT_FOUND_ON_HOOK         = 32,
    HOUSING_RESULT_FIXTURE_DELETE_NOT_ALLOWED        = 33,
    HOUSING_RESULT_FIXTURE_CANT_DELETE_DOOR          = 34,
    HOUSING_RESULT_FIXTURE_NOT_ALLOWED               = 35,
    HOUSING_RESULT_GAME_OBJECT_CREATION_FAILED       = 36,
    HOUSING_RESULT_GUILD_ERROR                       = 37,
    HOUSING_RESULT_HOUSE_ALREADY_EXISTS              = 38,
    HOUSING_RESULT_HOUSE_IS_NOT_GUILD_OWNED          = 39,
    HOUSING_RESULT_HOUSE_NOT_FOUND                   = 40,
    HOUSING_RESULT_HOUSE_NOT_IN_PLOT                 = 41,
    HOUSING_RESULT_INVALID_CHARTER_NAME              = 42,
    HOUSING_RESULT_INVALID_FIXTURE_TYPE              = 43,
    HOUSING_RESULT_INVALID_FIXTURE_SIZE              = 44,
    HOUSING_RESULT_INVALID_NEIGHBORHOOD_NAME         = 45,
    HOUSING_RESULT_INVALID_PLOT                      = 46,
    HOUSING_RESULT_INVALID_ROOM                      = 47,
    HOUSING_RESULT_INVALID_ROOM_COMPONENT            = 48,
    HOUSING_RESULT_ITEM_NOT_DECOR                    = 49,
    HOUSING_RESULT_LOCK_DENIED                       = 50,
    HOUSING_RESULT_MOVE_ON_COOLDOWN                  = 51,
    HOUSING_RESULT_NEIGHBORHOOD_ALREADY_GUILD        = 52,
    HOUSING_RESULT_NEIGHBORHOOD_CHARTER_UPDATE_FAILED = 53,
    HOUSING_RESULT_NEIGHBORHOOD_FULL                 = 54,
    HOUSING_RESULT_NEIGHBORHOOD_GUILD_TOO_SMALL      = 55,
    HOUSING_RESULT_NEIGHBORHOOD_NAME_TAKEN           = 56,
    HOUSING_RESULT_NEIGHBORHOOD_NOT_FOUND            = 57,
    HOUSING_RESULT_NO_BASIC_HOUSE_IN_SLOT            = 58,
    HOUSING_RESULT_NOT_ALLOWED                       = 59,
    HOUSING_RESULT_NOT_ENOUGH_SIGNATURES             = 60,
    HOUSING_RESULT_NOT_IN_NEIGHBORHOOD               = 61,
    HOUSING_RESULT_NOT_IN_OWN_HOUSE                  = 62,
    HOUSING_RESULT_NOT_INSIDE_HOUSE                  = 63,
    HOUSING_RESULT_NOT_ON_PLOT                       = 64,
    HOUSING_RESULT_PERMISSION_DENIED                 = 65,
    HOUSING_RESULT_PLOT_ALREADY_OWNED                = 66,
    HOUSING_RESULT_PLOT_NOT_AVAILABLE                 = 67,
    HOUSING_RESULT_PLOT_NOT_OWNED                    = 68,
    HOUSING_RESULT_PLOT_RESERVATION_CONFLICT         = 69,
    HOUSING_RESULT_PLAYER_NOT_FOUND                  = 70,
    HOUSING_RESULT_REDEEM_NOT_AVAILABLE              = 71,
    HOUSING_RESULT_ROOM_BUDGET_EXCEEDED              = 72,
    HOUSING_RESULT_ROOM_COMPONENT_INVALID            = 73,
    HOUSING_RESULT_ROOM_IS_BASE_ROOM                 = 74,
    HOUSING_RESULT_ROOM_IS_LEAF                      = 75,
    HOUSING_RESULT_ROOM_LAST                         = 76,
    HOUSING_RESULT_ROOM_MAX_FLOORS                   = 77,
    HOUSING_RESULT_ROOM_NOT_FOUND                    = 78,
    HOUSING_RESULT_ROOM_NOT_LEAF                     = 79,
    HOUSING_RESULT_ROOM_STAIRWELL_CONNECTION         = 80,
    HOUSING_RESULT_ROOM_UNREACHABLE                  = 81,
    HOUSING_RESULT_RPC_ERROR                         = 82,
    HOUSING_RESULT_SETTING_UPDATE_FAILED             = 83,
    HOUSING_RESULT_STORAGE_DELETE_FAILED             = 84,
    HOUSING_RESULT_STORAGE_EMPTY                     = 85,
    HOUSING_RESULT_STORAGE_FULL                      = 86,
    HOUSING_RESULT_THROTTLE_LIMIT_EXCEEDED           = 87,
    HOUSING_RESULT_TOO_FAR_FROM_PLOT                 = 88,
    HOUSING_RESULT_UNLOCK_OPERATION_FAILED           = 89
};

// Official HouseEditorMode enum (7 values)
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

enum HouseEditingContext : uint8
{
    HOUSE_EDITING_CONTEXT_NONE      = 0,
    HOUSE_EDITING_CONTEXT_DECOR     = 1,
    HOUSE_EDITING_CONTEXT_ROOM      = 2,
    HOUSE_EDITING_CONTEXT_FIXTURE   = 3
};

enum HousingFixtureSize : uint8
{
    HOUSING_FIXTURE_SIZE_NONE       = 0,
    HOUSING_FIXTURE_SIZE_ANY        = 1,
    HOUSING_FIXTURE_SIZE_SMALL      = 2,
    HOUSING_FIXTURE_SIZE_MEDIUM     = 3,
    HOUSING_FIXTURE_SIZE_LARGE      = 4
};

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

enum HousingRoomComponentDoorType : uint8
{
    HOUSING_ROOM_DOOR_TYPE_NONE         = 0,
    HOUSING_ROOM_DOOR_TYPE_DOORWAY      = 1,
    HOUSING_ROOM_DOOR_TYPE_THRESHOLD    = 2
};

enum HousingRoomComponentCeilingType : uint8
{
    HOUSING_ROOM_CEILING_TYPE_FLAT      = 0,
    HOUSING_ROOM_CEILING_TYPE_VAULTED   = 1
};

enum HousingRoomComponentStairType : uint8
{
    HOUSING_ROOM_STAIR_TYPE_NONE            = 0,
    HOUSING_ROOM_STAIR_TYPE_START_TO_END    = 1,
    HOUSING_ROOM_STAIR_TYPE_START_TO_MIDDLE = 2,
    HOUSING_ROOM_STAIR_TYPE_MIDDLE_TO_MIDDLE = 3,
    HOUSING_ROOM_STAIR_TYPE_MIDDLE_TO_END   = 4
};

enum HousingRoomComponentOptionType : uint8
{
    HOUSING_ROOM_COMPONENT_OPTION_COSMETIC      = 0,
    HOUSING_ROOM_COMPONENT_OPTION_DOORWAY_WALL  = 1,
    HOUSING_ROOM_COMPONENT_OPTION_DOORWAY       = 2
};

enum HousingCatalogEntryType : uint8
{
    HOUSING_CATALOG_ENTRY_INVALID   = 0,
    HOUSING_CATALOG_ENTRY_DECOR     = 1,
    HOUSING_CATALOG_ENTRY_ROOM      = 2
};

enum HousingCatalogEntrySize : uint8
{
    HOUSING_CATALOG_SIZE_NONE       = 0,
    HOUSING_CATALOG_SIZE_TINY       = 65,
    HOUSING_CATALOG_SIZE_SMALL      = 66,
    HOUSING_CATALOG_SIZE_MEDIUM     = 67,
    HOUSING_CATALOG_SIZE_LARGE      = 68,
    HOUSING_CATALOG_SIZE_HUGE       = 69
};

enum HousingDecorTheme : uint8
{
    HOUSING_DECOR_THEME_NONE        = 0,
    HOUSING_DECOR_THEME_FOLK        = 1,
    HOUSING_DECOR_THEME_RUGGED      = 2,
    HOUSING_DECOR_THEME_GENERIC     = 3,
    HOUSING_DECOR_THEME_NIGHT_ELF   = 4,
    HOUSING_DECOR_THEME_BLOOD_ELF   = 5
};

enum RoomConnectionType : uint8
{
    ROOM_CONNECTION_NONE    = 0,
    ROOM_CONNECTION_ALL     = 1
};

enum HousingRoomFlags : uint32
{
    HOUSING_ROOM_FLAG_NONE                  = 0x00,
    HOUSING_ROOM_FLAG_BASE_ROOM             = 0x01,
    HOUSING_ROOM_FLAG_HAS_STAIRS            = 0x02,
    HOUSING_ROOM_FLAG_UNLOCKED_BY_DEFAULT   = 0x04,
    HOUSING_ROOM_FLAG_HAS_CUSTOM_GEOMETRY   = 0x08
};

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

enum NeighborhoodInviteResult : uint32
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
    NEIGHBORHOOD_INVITE_TOO_MANY_REQUESTS       = 10,
    NEIGHBORHOOD_INVITE_ALREADY_IN_NEIGHBORHOOD = 11
};

enum HouseOwnerError : uint8
{
    HOUSE_OWNER_ERROR_NONE                  = 0,
    HOUSE_OWNER_ERROR_FACTION               = 1,
    HOUSE_OWNER_ERROR_GUILD                 = 2,
    HOUSE_OWNER_ERROR_GENERIC_PERMISSION    = 3
};

enum CreateNeighborhoodErrorType : uint8
{
    CREATE_NEIGHBORHOOD_ERROR_NONE              = 0,
    CREATE_NEIGHBORHOOD_ERROR_PROFANITY         = 1,
    CREATE_NEIGHBORHOOD_ERROR_UNDERSIZED_GUILD  = 2,
    CREATE_NEIGHBORHOOD_ERROR_OVERSIZED_GUILD   = 3
};

enum NeighborhoodMemberRole : uint8
{
    NEIGHBORHOOD_ROLE_RESIDENT  = 0,
    NEIGHBORHOOD_ROLE_MANAGER   = 1,
    NEIGHBORHOOD_ROLE_OWNER     = 2
};

enum NeighborhoodFactionRestriction : int32
{
    NEIGHBORHOOD_FACTION_NONE       = 0,
    NEIGHBORHOOD_FACTION_HORDE      = 1,
    NEIGHBORHOOD_FACTION_ALLIANCE   = 2
};

enum HouseSettingsFlags : uint32
{
    HOUSE_SETTING_ALLOW_VISITORS            = 0x01,
    HOUSE_SETTING_ALLOW_NEIGHBORHOOD_ONLY   = 0x02,
    HOUSE_SETTING_ALLOW_FRIENDS_ONLY        = 0x04,
    HOUSE_SETTING_LOCKED                    = 0x08
};

enum HousingDecorPlacementFlags : int32
{
    DECOR_PLACEMENT_FLOOR       = 0x01,
    DECOR_PLACEMENT_WALL        = 0x02,
    DECOR_PLACEMENT_CEILING     = 0x04,
    DECOR_PLACEMENT_OUTDOOR     = 0x08,
    DECOR_PLACEMENT_STACKABLE   = 0x10
};

enum HousingRoomSize : int8
{
    ROOM_SIZE_SMALL     = 0,
    ROOM_SIZE_MEDIUM    = 1,
    ROOM_SIZE_LARGE     = 2
};

enum HousingPlotSize : int32
{
    PLOT_SIZE_SMALL     = 0,
    PLOT_SIZE_MEDIUM    = 1,
    PLOT_SIZE_LARGE     = 2
};

enum HousingInitiativeType : int32
{
    INITIATIVE_TYPE_GATHERING       = 0,
    INITIATIVE_TYPE_CRAFTING        = 1,
    INITIATIVE_TYPE_COMBAT          = 2,
    INITIATIVE_TYPE_EXPLORATION     = 3
};

enum HousingFixtureFlags : uint32
{
    HOUSING_FIXTURE_FLAG_NONE               = 0x00,
    HOUSING_FIXTURE_FLAG_IS_DEFAULT         = 0x01,
    HOUSING_FIXTURE_FLAG_UNLOCKED_BY_DEFAULT = 0x02
};

enum HousingRoomComponentFlags : uint32
{
    HOUSING_ROOM_COMPONENT_FLAG_NONE                    = 0x00,
    HOUSING_ROOM_COMPONENT_FLAG_HIDDEN_IN_LAYOUT_MODE   = 0x01
};

enum HousingRoomComponentOptionFlags : uint32
{
    HOUSING_ROOM_COMPONENT_OPTION_FLAG_NONE         = 0x00,
    HOUSING_ROOM_COMPONENT_OPTION_FLAG_IS_DEFAULT   = 0x01
};

enum HousingRoomComponentTextureFlags : uint32
{
    HOUSING_ROOM_COMPONENT_TEXTURE_FLAG_NONE                    = 0x00,
    HOUSING_ROOM_COMPONENT_TEXTURE_FLAG_UNLOCKED_BY_DEFAULT     = 0x01
};

enum NeighborhoodFlags : uint32
{
    NEIGHBORHOOD_FLAG_NONE              = 0x00,
    NEIGHBORHOOD_FLAG_POOL_PARENT       = 0x01,
    NEIGHBORHOOD_FLAG_OPEN_TO_PUBLIC    = 0x02
};

enum HouseExteriorWMODataFlags : uint32
{
    HOUSE_EXTERIOR_WMO_FLAG_NONE                            = 0x00,
    HOUSE_EXTERIOR_WMO_FLAG_UNLOCKED_BY_DEFAULT             = 0x01,
    HOUSE_EXTERIOR_WMO_FLAG_ALLOWED_IN_HORDE_NEIGHBORHOODS  = 0x02,
    HOUSE_EXTERIOR_WMO_FLAG_ALLOWED_IN_ALLIANCE_NEIGHBORHOODS = 0x04
};

// Constants
static constexpr uint32 MAX_HOUSING_DECOR_PER_ROOM      = 50;
static constexpr uint32 MAX_HOUSING_ROOMS_PER_HOUSE     = 20;
static constexpr uint32 MAX_HOUSING_FIXTURES_PER_HOUSE  = 10;
static constexpr uint32 MAX_HOUSING_DYE_SLOTS           = 3;
static constexpr uint32 MAX_NEIGHBORHOOD_PLOTS          = 55;
static constexpr uint32 MAX_NEIGHBORHOOD_MANAGERS       = 5;
static constexpr uint32 MIN_CHARTER_SIGNATURES          = 4;
static constexpr uint8  INVALID_PLOT_INDEX              = 255;
static constexpr uint32 HOUSING_MAX_NAME_LENGTH         = 64;
static constexpr uint64 HOUSE_PURCHASE_COST_COPPER      = 10000000ULL * 10000ULL;  // 10M gold in copper
static constexpr uint64 HOUSE_MOVE_COST_COPPER          = 1000ULL * 10000ULL;      // 1K gold in copper
static constexpr uint32 MAX_HOUSE_LEVEL                 = 20;

#endif // TRINITYCORE_HOUSING_DEFINES_H
