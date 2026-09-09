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

#ifndef TRINITYCORE_GUILDRENAMEMGR_H
#define TRINITYCORE_GUILDRENAMEMGR_H

#include "Define.h"
#include "ObjectGuid.h"
#include <ctime>
#include <string>
#include <string_view>
#include <unordered_map>

class Guild;
class WorldSession;

// GuildErrorType (client Enum.GuildErrorType, Blizzard_APIDocumentationGenerated/GuildConstantsDocumentation.lua).
// Only the values used by the rename flow are named here.
enum class GuildRenameError : int32
{
    Success             = 0,
    UnknownError        = 1,
    NameInvalid         = 6,
    NameAlreadyExists   = 7,
    NoPermission        = 8,    // client spells it "NoPermisson"
    NotInGuild          = 9,
    NotEnoughMoney      = 26,
    TooMuchMoney        = 31,
    InCooldown          = 49,   // client Enum.GuildErrorType.InCooldown
    ReservationExpired  = 50    // client Enum.GuildErrorType.ReservationExpired
};

// Manages the 12.1 guild-rename cluster: name validation, the gold charge taken from the guild bank,
// the post-rename cooldown, and the time-boxed refund of the last paid rename. Per-guild state is kept
// in memory and persisted to the `guild_rename` characters-DB table.
class TC_GAME_API GuildRenameMgr
{
    GuildRenameMgr();
    ~GuildRenameMgr();

public:
    GuildRenameMgr(GuildRenameMgr const&) = delete;
    GuildRenameMgr& operator=(GuildRenameMgr const&) = delete;

    static GuildRenameMgr* instance();

    // Loads config values and per-guild rename history. Call once at world start (after guilds load).
    void Load();

    // Client requests: each reads the player's guild off the session, validates, mutates and replies.
    void HandleStatusRequest(WorldSession* session);
    void HandleNameCheck(WorldSession* session, std::string const& desiredName);
    void HandleRenameRequest(WorldSession* session, std::string const& desiredName);
    void HandleRefundRequest(WorldSession* session);

    // Pure name validation (length / charter rules / reserved list / uniqueness). Public for reuse/tests.
    GuildRenameError ValidateName(Guild const* guild, std::string_view name) const;

    bool IsEnabled() const { return _enabled; }
    uint64 GetRenameCost() const { return _cost; }

private:
    struct RenameRecord
    {
        std::string PreviousName;
        uint64 CostPaid = 0;
        time_t RenameTime = 0;      // unix time of the last paid rename
        bool Refunded = false;
    };

    RenameRecord const* GetRecord(ObjectGuid::LowType guildId) const;
    void SaveRecord(ObjectGuid::LowType guildId, RenameRecord const& record);
    void DeleteRecord(ObjectGuid::LowType guildId);

    void SendStatus(WorldSession* session, Guild* guild, GuildRenameError result);

    bool _enabled = true;
    uint64 _cost = 0;               // copper, taken from the guild bank
    uint32 _cooldown = 0;           // seconds between paid renames
    uint32 _refundWindow = 0;       // seconds a paid rename can be refunded within
    std::unordered_map<ObjectGuid::LowType, RenameRecord> _records;
};

#define sGuildRenameMgr GuildRenameMgr::instance()

#endif // TRINITYCORE_GUILDRENAMEMGR_H