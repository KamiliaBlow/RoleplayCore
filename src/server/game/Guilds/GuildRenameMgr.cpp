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

#include "GuildRenameMgr.h"
#include "Common.h"
#include "Config.h"
#include "DatabaseEnv.h"
#include "GameTime.h"
#include "Guild.h"
#include "GuildMgr.h"
#include "GuildPackets.h"
#include "Log.h"
#include "ObjectMgr.h"
#include "Player.h"
#include "World.h"
#include "WorldSession.h"

GuildRenameMgr::GuildRenameMgr() = default;
GuildRenameMgr::~GuildRenameMgr() = default;

GuildRenameMgr* GuildRenameMgr::instance()
{
    static GuildRenameMgr instance;
    return &instance;
}

void GuildRenameMgr::Load()
{
    _enabled = sConfigMgr->GetBoolDefault("Guild.Rename.Enabled", true);
    _cost = static_cast<uint64>(sConfigMgr->GetInt64Default("Guild.Rename.Cost", 1000000));      // default 100g, taken from guild bank
    _cooldown = sConfigMgr->GetIntDefault("Guild.Rename.Cooldown", 0);                           // seconds; 0 = no cooldown
    _refundWindow = sConfigMgr->GetIntDefault("Guild.Rename.RefundWindow", 7 * DAY);             // seconds refund stays available

    _records.clear();

    if (PreparedQueryResult result = CharacterDatabase.Query(CharacterDatabase.GetPreparedStatement(CHAR_SEL_GUILD_RENAME)))
    {
        do
        {
            Field* fields = result->Fetch();
            ObjectGuid::LowType guildId = fields[0].GetUInt64();
            RenameRecord& record = _records[guildId];
            record.PreviousName = fields[1].GetString();
            record.CostPaid = fields[2].GetUInt64();
            record.RenameTime = static_cast<time_t>(fields[3].GetUInt64());
            record.Refunded = fields[4].GetBool();
        } while (result->NextRow());
    }

    TC_LOG_INFO("server.loading", ">> Loaded {} guild rename record(s). Guild rename {}.",
        _records.size(), _enabled ? "enabled" : "disabled");
}

GuildRenameMgr::RenameRecord const* GuildRenameMgr::GetRecord(ObjectGuid::LowType guildId) const
{
    auto itr = _records.find(guildId);
    return itr != _records.end() ? &itr->second : nullptr;
}

void GuildRenameMgr::SaveRecord(ObjectGuid::LowType guildId, RenameRecord const& record)
{
    _records[guildId] = record;

    CharacterDatabasePreparedStatement* stmt = CharacterDatabase.GetPreparedStatement(CHAR_REP_GUILD_RENAME);
    stmt->setUInt64(0, guildId);
    stmt->setString(1, record.PreviousName);
    stmt->setUInt64(2, record.CostPaid);
    stmt->setUInt64(3, static_cast<uint64>(record.RenameTime));
    stmt->setBool(4, record.Refunded);
    CharacterDatabase.Execute(stmt);
}

void GuildRenameMgr::DeleteRecord(ObjectGuid::LowType guildId)
{
    _records.erase(guildId);

    CharacterDatabasePreparedStatement* stmt = CharacterDatabase.GetPreparedStatement(CHAR_DEL_GUILD_RENAME);
    stmt->setUInt64(0, guildId);
    CharacterDatabase.Execute(stmt);
}

GuildRenameError GuildRenameMgr::ValidateName(Guild const* guild, std::string_view name) const
{
    if (name.empty() || name.length() > 24 || !ObjectMgr::IsValidCharterName(name) || sObjectMgr->IsReservedName(name))
        return GuildRenameError::NameInvalid;

    // Uniqueness: any existing guild with this name blocks it - including this guild itself,
    // since a rename to the current name is a no-op the client should not be allowed to buy.
    (void)guild;
    if (sGuildMgr->GetGuildByName(name))
        return GuildRenameError::NameAlreadyExists;

    return GuildRenameError::Success;
}

void GuildRenameMgr::SendStatus(WorldSession* session, Guild* guild, GuildRenameError result)
{
    Player* player = session->GetPlayer();
    time_t now = GameTime::GetGameTime();

    WorldPackets::Guild::GuildRenameStatusUpdate status;
    status.IsNameChangeEnabled = _enabled && guild != nullptr;
    status.IsPlayerGuildMaster = guild && player && guild->GetLeaderGUID() == player->GetGUID();
    status.RenamePrice = _cost;
    status.CurrentGuildMoney = guild ? guild->GetBankMoney() : 0;
    status.Result = int32(result);

    if (guild)
    {
        if (RenameRecord const* record = GetRecord(guild->GetId()))
        {
            status.OldGuildName = record->PreviousName;

            if (_cooldown && now < record->RenameTime + time_t(_cooldown))
                status.NextRenameTime = record->RenameTime + time_t(_cooldown);

            if (!record->Refunded && (!_refundWindow || now < record->RenameTime + time_t(_refundWindow)))
            {
                status.RefundEligibleEndTime = _refundWindow ? record->RenameTime + time_t(_refundWindow) : 0;
                status.RefundAmount = record->CostPaid;
            }
        }
    }

    session->SendPacket(status.Write());
}

void GuildRenameMgr::HandleStatusRequest(WorldSession* session)
{
    Guild* guild = session->GetPlayer() ? session->GetPlayer()->GetGuild() : nullptr;
    SendStatus(session, guild, guild ? GuildRenameError::Success : GuildRenameError::NotInGuild);
}

void GuildRenameMgr::HandleNameCheck(WorldSession* session, std::string const& desiredName)
{
    Guild* guild = session->GetPlayer() ? session->GetPlayer()->GetGuild() : nullptr;
    GuildRenameError status = ValidateName(guild, desiredName);

    WorldPackets::Guild::GuildRenameNameCheckResult result;
    result.DesiredName = desiredName;
    result.Status = int32(status);
    session->SendPacket(result.Write());
}

void GuildRenameMgr::HandleRenameRequest(WorldSession* session, std::string const& desiredName)
{
    Player* player = session->GetPlayer();
    Guild* guild = player ? player->GetGuild() : nullptr;

    auto reply = [&](GuildRenameError status, std::string const& name)
    {
        WorldPackets::Guild::GuildRenameRequestedResult result;
        result.NewName = name;
        result.Status = int32(status);
        session->SendPacket(result.Write());
    };

    if (!guild)
        return reply(GuildRenameError::NotInGuild, desiredName);

    if (!_enabled)
        return reply(GuildRenameError::UnknownError, desiredName);

    if (guild->GetLeaderGUID() != player->GetGUID())
        return reply(GuildRenameError::NoPermission, desiredName);

    time_t now = GameTime::GetGameTime();
    if (RenameRecord const* record = GetRecord(guild->GetId()))
        if (_cooldown && now < record->RenameTime + time_t(_cooldown))
            return reply(GuildRenameError::InCooldown, desiredName);

    if (GuildRenameError nameStatus = ValidateName(guild, desiredName); nameStatus != GuildRenameError::Success)
        return reply(nameStatus, desiredName);

    if (guild->GetBankMoney() < _cost)
        return reply(GuildRenameError::NotEnoughMoney, desiredName);

    std::string previousName(guild->GetName());

    // Take the fee from the guild bank first; a failed name application below refunds it.
    if (_cost && !guild->ModifyBankMoney(_cost, false))
        return reply(GuildRenameError::NotEnoughMoney, desiredName);

    if (!guild->SetName(desiredName))
    {
        if (_cost)
            guild->ModifyBankMoney(_cost, true);
        return reply(GuildRenameError::NameInvalid, desiredName);
    }

    RenameRecord record;
    record.PreviousName = previousName;
    record.CostPaid = _cost;
    record.RenameTime = now;
    record.Refunded = false;
    SaveRecord(guild->GetId(), record);

    TC_LOG_INFO("guild", "Guild {} renamed '{}' -> '{}' by {} (cost {} copper)",
        guild->GetId(), previousName, desiredName, session->GetPlayerInfo(), _cost);

    reply(GuildRenameError::Success, desiredName);
    SendStatus(session, guild, GuildRenameError::Success);
}

void GuildRenameMgr::HandleRefundRequest(WorldSession* session)
{
    Player* player = session->GetPlayer();
    Guild* guild = player ? player->GetGuild() : nullptr;

    auto reply = [&](GuildRenameError status, std::string const& name)
    {
        WorldPackets::Guild::GuildRenameRefundResult result;
        result.GuildName = name;
        result.Status = int32(status);
        session->SendPacket(result.Write());
    };

    if (!guild)
        return reply(GuildRenameError::NotInGuild, "");

    if (guild->GetLeaderGUID() != player->GetGUID())
        return reply(GuildRenameError::NoPermission, std::string(guild->GetName()));

    RenameRecord const* record = GetRecord(guild->GetId());
    if (!record || record->Refunded)
        return reply(GuildRenameError::ReservationExpired, std::string(guild->GetName()));

    time_t now = GameTime::GetGameTime();
    if (_refundWindow && now > record->RenameTime + time_t(_refundWindow))
        return reply(GuildRenameError::ReservationExpired, std::string(guild->GetName()));

    std::string restoreName = record->PreviousName;
    uint64 refundAmount = record->CostPaid;

    // The old name may have been taken by another guild in the meantime.
    if (sGuildMgr->GetGuildByName(restoreName))
        return reply(GuildRenameError::NameAlreadyExists, std::string(guild->GetName()));

    if (!guild->SetName(restoreName))
        return reply(GuildRenameError::UnknownError, std::string(guild->GetName()));

    if (refundAmount)
        guild->ModifyBankMoney(refundAmount, true);

    RenameRecord updated = *record;
    updated.Refunded = true;
    SaveRecord(guild->GetId(), updated);

    TC_LOG_INFO("guild", "Guild {} rename refunded, restored name '{}' by {} (refund {} copper)",
        guild->GetId(), restoreName, session->GetPlayerInfo(), refundAmount);

    reply(GuildRenameError::Success, std::string(guild->GetName()));
    SendStatus(session, guild, GuildRenameError::Success);
}