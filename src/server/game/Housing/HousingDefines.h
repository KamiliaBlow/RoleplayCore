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

// HousingResult enum - 90 values (0-89), verified against client binary
enum HousingResult : uint32
{
    HOUSING_RESULT_SUCCESS                                  = 0,
    HOUSING_RESULT_ACTION_LOCKED_BY_COMBAT                  = 1,
    HOUSING_RESULT_BOUNDS_FAILURE_CHILDREN                   = 2,
    HOUSING_RESULT_BOUNDS_FAILURE_PLOT                       = 3,
    HOUSING_RESULT_BOUNDS_FAILURE_ROOM                       = 4,
    HOUSING_RESULT_CANNOT_AFFORD                             = 5,
    HOUSING_RESULT_CHARTER_COMPLETE                          = 6,
    HOUSING_RESULT_COLLISION_INVALID                          = 7,
    HOUSING_RESULT_DB_ERROR                                  = 8,
    HOUSING_RESULT_DECOR_CANNOT_BE_REDEEMED                  = 9,
    HOUSING_RESULT_DECOR_ITEM_NOT_DESTROYABLE                = 10,
    HOUSING_RESULT_DECOR_NOT_FOUND                           = 11,
    HOUSING_RESULT_DECOR_NOT_FOUND_IN_STORAGE                = 12,
    HOUSING_RESULT_DUPLICATE_CHARTER_SIGNATURE                = 13,
    HOUSING_RESULT_FILTER_REJECTED                           = 14,
    HOUSING_RESULT_FIXTURE_CANT_DELETE_DOOR                   = 15,
    HOUSING_RESULT_FIXTURE_HOOK_EMPTY                        = 16,
    HOUSING_RESULT_FIXTURE_HOOK_OCCUPIED                     = 17,
    HOUSING_RESULT_FIXTURE_HOUSE_TYPE_MISMATCH               = 18,
    HOUSING_RESULT_FIXTURE_NOT_FOUND                         = 19,
    HOUSING_RESULT_FIXTURE_SIZE_MISMATCH                     = 20,
    HOUSING_RESULT_FIXTURE_TYPE_MISMATCH                     = 21,
    HOUSING_RESULT_GENERIC_FAILURE                           = 22,
    HOUSING_RESULT_GUILD_MORE_ACCOUNTS_NEEDED                = 23,
    HOUSING_RESULT_GUILD_MORE_ACTIVE_PLAYERS_NEEDED          = 24,
    HOUSING_RESULT_GUILD_NOT_LOADED                          = 25,
    HOUSING_RESULT_HOUSE_EDIT_LOCK_FAILED                    = 26,
    HOUSING_RESULT_HOUSE_EXTERIOR_ALREADY_THAT_SIZE          = 27,
    HOUSING_RESULT_HOUSE_EXTERIOR_ALREADY_THAT_TYPE          = 28,
    HOUSING_RESULT_HOUSE_EXTERIOR_ROOT_NOT_FOUND             = 29,
    HOUSING_RESULT_HOUSE_EXTERIOR_TYPE_NEIGHBORHOOD_MISMATCH = 30,
    HOUSING_RESULT_HOUSE_EXTERIOR_TYPE_NOT_FOUND             = 31,
    HOUSING_RESULT_HOUSE_EXTERIOR_TYPE_SIZE_MISMATCH         = 32,
    HOUSING_RESULT_HOUSE_EXTERIOR_SIZE_NOT_AVAILABLE          = 33,
    HOUSING_RESULT_HOOK_NOT_CHILD_OF_FIXTURE                 = 34,
    HOUSING_RESULT_HOUSE_NOT_FOUND                           = 35,
    HOUSING_RESULT_INCORRECT_FACTION                         = 36,
    HOUSING_RESULT_INVALID_DECOR_ITEM                        = 37,
    HOUSING_RESULT_INVALID_DISTANCE                          = 38,
    HOUSING_RESULT_INVALID_GUILD                             = 39,
    HOUSING_RESULT_INVALID_HOUSE                             = 40,
    HOUSING_RESULT_INVALID_INSTANCE                          = 41,
    HOUSING_RESULT_INVALID_INTERACTION                       = 42,
    HOUSING_RESULT_INVALID_MAP                               = 43,
    HOUSING_RESULT_INVALID_NEIGHBORHOOD_NAME                 = 44,
    HOUSING_RESULT_INVALID_ROOM_LAYOUT                       = 45,
    HOUSING_RESULT_LOCKED_BY_OTHER_PLAYER                    = 46,
    HOUSING_RESULT_LOCK_OPERATION_FAILED                     = 47,
    HOUSING_RESULT_MAX_DECOR_REACHED                         = 48,
    HOUSING_RESULT_MAX_PREVIEW_DECOR_REACHED                 = 49,
    HOUSING_RESULT_MISSING_CORE_FIXTURE                      = 50,
    HOUSING_RESULT_MISSING_DYE                               = 51,
    HOUSING_RESULT_MISSING_EXPANSION_ACCESS                  = 52,
    HOUSING_RESULT_MISSING_FACTION_MAP                       = 53,
    HOUSING_RESULT_MISSING_PRIVATE_NEIGHBORHOOD_INVITE       = 54,
    HOUSING_RESULT_MORE_HOUSE_SLOTS_NEEDED                   = 55,
    HOUSING_RESULT_MORE_SIGNATURES_NEEDED                    = 56,
    HOUSING_RESULT_NEIGHBORHOOD_NOT_FOUND                    = 57,
    HOUSING_RESULT_NO_NEIGHBORHOOD_OWNERSHIP_REQUESTS        = 58,
    HOUSING_RESULT_NOT_IN_DECOR_EDIT_MODE                    = 59,
    HOUSING_RESULT_NOT_IN_FIXTURE_EDIT_MODE                  = 60,
    HOUSING_RESULT_NOT_IN_LAYOUT_EDIT_MODE                   = 61,
    HOUSING_RESULT_NOT_INSIDE_HOUSE                          = 62,
    HOUSING_RESULT_NOT_ON_OWNED_PLOT                         = 63,
    HOUSING_RESULT_OPERATION_ABORTED                         = 64,
    HOUSING_RESULT_OWNER_NOT_IN_GUILD                        = 65,
    HOUSING_RESULT_PERMISSION_DENIED                         = 66,
    HOUSING_RESULT_PLACEMENT_TARGET_INVALID                  = 67,
    HOUSING_RESULT_PLAYER_NOT_FOUND                          = 68,
    HOUSING_RESULT_PLAYER_NOT_IN_INSTANCE                    = 69,
    HOUSING_RESULT_PLOT_NOT_FOUND                            = 70,
    HOUSING_RESULT_PLOT_NOT_VACANT                           = 71,
    HOUSING_RESULT_PLOT_RESERVATION_COOLDOWN                 = 72,
    HOUSING_RESULT_PLOT_RESERVED                             = 73,
    HOUSING_RESULT_ROOM_NOT_FOUND                            = 74,
    HOUSING_RESULT_ROOM_UPDATE_FAILED                        = 75,
    HOUSING_RESULT_RPC_FAILURE                               = 76,
    HOUSING_RESULT_SERVICE_NOT_AVAILABLE                     = 77,
    HOUSING_RESULT_STATIC_DATA_NOT_FOUND                     = 78,
    HOUSING_RESULT_TIMEOUT_LIMIT                             = 79,
    HOUSING_RESULT_TIMERUNNING_NOT_ALLOWED                   = 80,
    HOUSING_RESULT_TOKEN_REQUIRED                            = 81,
    HOUSING_RESULT_TOO_MANY_REQUESTS                         = 82,
    HOUSING_RESULT_TRANSACTION_FAILURE                       = 83,
    HOUSING_RESULT_UNCOLLECTED_EXTERIOR_FIXTURE              = 84,
    HOUSING_RESULT_UNCOLLECTED_HOUSE_TYPE                    = 85,
    HOUSING_RESULT_UNCOLLECTED_ROOM                          = 86,
    HOUSING_RESULT_UNCOLLECTED_ROOM_MATERIAL                 = 87,
    HOUSING_RESULT_UNCOLLECTED_ROOM_THEME                    = 88,
    HOUSING_RESULT_UNLOCK_OPERATION_FAILED                   = 89
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
    NEIGHBORHOOD_INVITE_TOO_MANY_REQUESTS       = 10
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
    HOUSE_SETTING_PLOT_ACCESS_PARTY         = 0x200
};

// HousingDecorPlacementFlags enum - 5 values (bitmask)
enum HousingDecorPlacementFlags : int32
{
    DECOR_PLACEMENT_FLOOR       = 0x01,
    DECOR_PLACEMENT_WALL        = 0x02,
    DECOR_PLACEMENT_CEILING     = 0x04,
    DECOR_PLACEMENT_OUTDOOR     = 0x08,
    DECOR_PLACEMENT_STACKABLE   = 0x10
};

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
