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

enum HousingResult : uint32
{
    HOUSING_RESULT_SUCCESS                              = 0,
    HOUSING_RESULT_INTERNAL_ERROR                       = 1,
    HOUSING_RESULT_NO_HOUSE                             = 2,
    HOUSING_RESULT_HOUSE_EXISTS                         = 3,
    HOUSING_RESULT_INVALID_HOUSE                        = 4,
    HOUSING_RESULT_INVALID_NEIGHBORHOOD                 = 5,
    HOUSING_RESULT_NOT_IN_NEIGHBORHOOD                  = 6,
    HOUSING_RESULT_NEIGHBORHOOD_FULL                    = 7,
    HOUSING_RESULT_NO_PLOT                              = 8,
    HOUSING_RESULT_PLOT_OCCUPIED                        = 9,
    HOUSING_RESULT_INVALID_PLOT                         = 10,
    HOUSING_RESULT_NOT_OWNER                            = 11,
    HOUSING_RESULT_NOT_MANAGER                          = 12,
    HOUSING_RESULT_INVALID_DECOR                        = 13,
    HOUSING_RESULT_DECOR_LIMIT_REACHED                  = 14,
    HOUSING_RESULT_INVALID_ROOM                         = 15,
    HOUSING_RESULT_ROOM_LIMIT_REACHED                   = 16,
    HOUSING_RESULT_INVALID_FIXTURE                      = 17,
    HOUSING_RESULT_INVALID_THEME                        = 18,
    HOUSING_RESULT_INVALID_WALLPAPER                    = 19,
    HOUSING_RESULT_NOT_ENOUGH_CURRENCY                  = 20,
    HOUSING_RESULT_NOT_ENOUGH_FAVOR                     = 21,
    HOUSING_RESULT_LEVEL_TOO_LOW                        = 22,
    HOUSING_RESULT_CONDITION_FAILED                     = 23,
    HOUSING_RESULT_CATALOG_ENTRY_NOT_FOUND              = 24,
    HOUSING_RESULT_INVALID_POSITION                     = 25,
    HOUSING_RESULT_ALREADY_INVITED                      = 26,
    HOUSING_RESULT_INVITE_DECLINED                      = 27,
    HOUSING_RESULT_FACTION_MISMATCH                     = 28,
    HOUSING_RESULT_CHARTER_ALREADY_EXISTS               = 29,
    HOUSING_RESULT_CHARTER_NOT_FOUND                    = 30,
    HOUSING_RESULT_NOT_ENOUGH_SIGNATURES                = 31,
    HOUSING_RESULT_PLAYER_NOT_FOUND                     = 32,
    HOUSING_RESULT_ALREADY_IN_NEIGHBORHOOD              = 33,
    HOUSING_RESULT_CANNOT_EVICT_OWNER                   = 34,
    HOUSING_RESULT_TRANSFER_PENDING                     = 35,
    HOUSING_RESULT_HOUSING_DISABLED                     = 36,
    HOUSING_RESULT_INVALID_NAME                         = 37,
    HOUSING_RESULT_NAME_ALREADY_USED                    = 38,
    HOUSING_RESULT_GUILD_REQUIREMENT_NOT_MET            = 39,
    HOUSING_RESULT_INVALID_DYE                          = 40,
    HOUSING_RESULT_INVALID_DOOR_TYPE                    = 41,
    HOUSING_RESULT_INVALID_CEILING_TYPE                 = 42,
    HOUSING_RESULT_BASE_ROOM_CANNOT_BE_REMOVED          = 43,
    HOUSING_RESULT_ROOM_HAS_DECOR                       = 44,
    HOUSING_RESULT_INVALID_SLOT                         = 45,
    HOUSING_RESULT_ROOM_CONNECTION_INVALID               = 46
};

enum HousingEditorMode : uint8
{
    HOUSING_EDITOR_MODE_NONE        = 0,
    HOUSING_EDITOR_MODE_DECOR       = 1,
    HOUSING_EDITOR_MODE_FIXTURE     = 2,
    HOUSING_EDITOR_MODE_ROOM        = 3
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

static constexpr uint32 MAX_HOUSING_DECOR_PER_ROOM      = 50;
static constexpr uint32 MAX_HOUSING_ROOMS_PER_HOUSE     = 20;
static constexpr uint32 MAX_HOUSING_FIXTURES_PER_HOUSE  = 10;
static constexpr uint32 MAX_HOUSING_DYE_SLOTS           = 3;
static constexpr uint32 MAX_NEIGHBORHOOD_PLOTS          = 16;
static constexpr uint32 MAX_NEIGHBORHOOD_MANAGERS       = 5;
static constexpr uint32 MIN_CHARTER_SIGNATURES          = 4;
static constexpr uint8  INVALID_PLOT_INDEX              = 255;
static constexpr uint32 HOUSING_MAX_NAME_LENGTH         = 64;

#endif // TRINITYCORE_HOUSING_DEFINES_H
