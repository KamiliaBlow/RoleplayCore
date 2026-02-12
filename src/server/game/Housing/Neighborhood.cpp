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

#include "Neighborhood.h"
#include "DatabaseEnv.h"
#include "GameTime.h"
#include "Log.h"
#include "ObjectAccessor.h"
#include "Player.h"
#include <algorithm>

Neighborhood::Neighborhood(ObjectGuid guid) : _guid(guid)
{
}

bool Neighborhood::LoadFromDB(PreparedQueryResult neighborhood, PreparedQueryResult members, PreparedQueryResult invites)
{
    if (!neighborhood)
        return false;

    Field* fields = neighborhood->Fetch();

    //          0     1       2            3                    4         5
    // SELECT guid, name, neighborhoodMapID, ownerGuid, factionRestriction, isPublic,
    //          6
    //        createTime FROM neighborhoods WHERE guid = ?

    // _guid is already set in constructor
    _name               = fields[1].GetString();
    _neighborhoodMapID  = fields[2].GetUInt32();
    _ownerGuid          = ObjectGuid::Create<HighGuid::Player>(fields[3].GetUInt64());
    _factionRestriction = fields[4].GetInt32();
    _isPublic           = fields[5].GetBool();
    _createTime         = fields[6].GetUInt32();

    TC_LOG_DEBUG("housing", "Neighborhood::LoadFromDB: Loaded neighborhood '{}' (guid: {}), owner: {}, mapId: {}, members: loading...",
        _name, _guid.ToString(), _ownerGuid.ToString(), _neighborhoodMapID);

    // Load members
    if (members)
    {
        do
        {
            Field* memberFields = members->Fetch();

            //          0          1     2
            // SELECT playerGuid, role, joinTime,
            //          3
            //        plotIndex FROM neighborhood_members WHERE neighborhoodGuid = ?

            Member member;
            member.PlayerGuid   = ObjectGuid::Create<HighGuid::Player>(memberFields[0].GetUInt64());
            member.Role         = memberFields[1].GetUInt8();
            member.JoinTime     = memberFields[2].GetUInt32();
            member.PlotIndex    = memberFields[3].GetUInt8();

            _members.push_back(member);

            // Build plot info from members that have plots assigned
            if (member.PlotIndex != INVALID_PLOT_INDEX)
            {
                PlotInfo plot;
                plot.PlotIndex  = member.PlotIndex;
                plot.OwnerGuid  = member.PlayerGuid;
                // HouseGuid and OwnerBnetGuid are resolved later when needed
                _plots.push_back(plot);
            }
        } while (members->NextRow());
    }

    TC_LOG_DEBUG("housing", "Neighborhood::LoadFromDB: Loaded {} members for neighborhood '{}'",
        _members.size(), _name);

    // Load pending invites
    if (invites)
    {
        do
        {
            Field* inviteFields = invites->Fetch();

            //          0           1          2
            // SELECT inviteeGuid, inviterGuid, inviteTime
            //        FROM neighborhood_invites WHERE neighborhoodGuid = ?

            PendingInvite invite;
            invite.InviteeGuid  = ObjectGuid::Create<HighGuid::Player>(inviteFields[0].GetUInt64());
            invite.InviterGuid  = ObjectGuid::Create<HighGuid::Player>(inviteFields[1].GetUInt64());
            invite.InviteTime   = inviteFields[2].GetUInt32();

            _pendingInvites.push_back(invite);
        } while (invites->NextRow());
    }

    TC_LOG_DEBUG("housing", "Neighborhood::LoadFromDB: Loaded {} pending invites for neighborhood '{}'",
        _pendingInvites.size(), _name);

    return true;
}

void Neighborhood::SaveToDB(CharacterDatabaseTransaction trans)
{
    // Update the main neighborhood row
    CharacterDatabasePreparedStatement* stmt = CharacterDatabase.GetPreparedStatement(CHAR_REP_NEIGHBORHOOD);
    uint8 index = 0;
    stmt->setUInt64(index++, _guid.GetCounter());
    stmt->setString(index++, _name);
    stmt->setUInt32(index++, _neighborhoodMapID);
    stmt->setUInt64(index++, _ownerGuid.GetCounter());
    stmt->setInt32(index++, _factionRestriction);
    stmt->setBool(index++, _isPublic);
    stmt->setUInt32(index++, _createTime);
    trans->Append(stmt);

    // Delete all members and re-insert
    stmt = CharacterDatabase.GetPreparedStatement(CHAR_DEL_NEIGHBORHOOD_MEMBERS);
    stmt->setUInt64(0, _guid.GetCounter());
    trans->Append(stmt);

    for (Member const& member : _members)
    {
        stmt = CharacterDatabase.GetPreparedStatement(CHAR_INS_NEIGHBORHOOD_MEMBER);
        index = 0;
        stmt->setUInt64(index++, _guid.GetCounter());
        stmt->setUInt64(index++, member.PlayerGuid.GetCounter());
        stmt->setUInt8(index++, member.Role);
        stmt->setUInt32(index++, member.JoinTime);
        stmt->setUInt8(index++, member.PlotIndex);
        trans->Append(stmt);
    }

    // Delete all invites and re-insert
    stmt = CharacterDatabase.GetPreparedStatement(CHAR_DEL_NEIGHBORHOOD_INVITES);
    stmt->setUInt64(0, _guid.GetCounter());
    trans->Append(stmt);

    for (PendingInvite const& invite : _pendingInvites)
    {
        stmt = CharacterDatabase.GetPreparedStatement(CHAR_INS_NEIGHBORHOOD_INVITE);
        index = 0;
        stmt->setUInt64(index++, _guid.GetCounter());
        stmt->setUInt64(index++, invite.InviteeGuid.GetCounter());
        stmt->setUInt64(index++, invite.InviterGuid.GetCounter());
        stmt->setUInt32(index++, invite.InviteTime);
        trans->Append(stmt);
    }

    TC_LOG_DEBUG("housing", "Neighborhood::SaveToDB: Saved neighborhood '{}' with {} members and {} invites",
        _name, _members.size(), _pendingInvites.size());
}

/*static*/ void Neighborhood::DeleteFromDB(ObjectGuid::LowType guid, CharacterDatabaseTransaction trans)
{
    CharacterDatabasePreparedStatement* stmt = CharacterDatabase.GetPreparedStatement(CHAR_DEL_NEIGHBORHOOD_INVITES_BY_GUID);
    stmt->setUInt64(0, guid);
    trans->Append(stmt);

    stmt = CharacterDatabase.GetPreparedStatement(CHAR_DEL_NEIGHBORHOOD_MEMBERS_BY_GUID);
    stmt->setUInt64(0, guid);
    trans->Append(stmt);

    stmt = CharacterDatabase.GetPreparedStatement(CHAR_DEL_NEIGHBORHOOD);
    stmt->setUInt64(0, guid);
    trans->Append(stmt);

    TC_LOG_DEBUG("housing", "Neighborhood::DeleteFromDB: Deleted neighborhood guid {}", guid);
}

void Neighborhood::SetName(std::string const& name)
{
    _name = name;

    TC_LOG_DEBUG("housing", "Neighborhood::SetName: Neighborhood {} renamed to '{}'",
        _guid.ToString(), _name);
}

void Neighborhood::SetPublic(bool isPublic)
{
    _isPublic = isPublic;

    TC_LOG_DEBUG("housing", "Neighborhood::SetPublic: Neighborhood '{}' public status set to {}",
        _name, _isPublic);
}

HousingResult Neighborhood::AddManager(ObjectGuid playerGuid)
{
    // Count current managers
    uint32 managerCount = 0;
    Member* targetMember = nullptr;

    for (Member& member : _members)
    {
        if (member.Role == NEIGHBORHOOD_ROLE_MANAGER)
            ++managerCount;

        if (member.PlayerGuid == playerGuid)
            targetMember = &member;
    }

    if (!targetMember)
    {
        TC_LOG_DEBUG("housing", "Neighborhood::AddManager: Player {} is not a member of neighborhood '{}'",
            playerGuid.ToString(), _name);
        return HOUSING_RESULT_NOT_IN_NEIGHBORHOOD;
    }

    if (targetMember->Role == NEIGHBORHOOD_ROLE_OWNER)
    {
        TC_LOG_DEBUG("housing", "Neighborhood::AddManager: Player {} is already owner of neighborhood '{}'",
            playerGuid.ToString(), _name);
        return HOUSING_RESULT_NOT_MANAGER;
    }

    if (targetMember->Role == NEIGHBORHOOD_ROLE_MANAGER)
    {
        TC_LOG_DEBUG("housing", "Neighborhood::AddManager: Player {} is already a manager in neighborhood '{}'",
            playerGuid.ToString(), _name);
        return HOUSING_RESULT_SUCCESS;
    }

    if (managerCount >= MAX_NEIGHBORHOOD_MANAGERS)
    {
        TC_LOG_DEBUG("housing", "Neighborhood::AddManager: Neighborhood '{}' has reached max managers ({})",
            _name, MAX_NEIGHBORHOOD_MANAGERS);
        return HOUSING_RESULT_NEIGHBORHOOD_FULL;
    }

    targetMember->Role = NEIGHBORHOOD_ROLE_MANAGER;

    TC_LOG_DEBUG("housing", "Neighborhood::AddManager: Player {} promoted to manager in neighborhood '{}'",
        playerGuid.ToString(), _name);

    return HOUSING_RESULT_SUCCESS;
}

HousingResult Neighborhood::RemoveManager(ObjectGuid playerGuid)
{
    for (Member& member : _members)
    {
        if (member.PlayerGuid == playerGuid)
        {
            if (member.Role == NEIGHBORHOOD_ROLE_OWNER)
            {
                TC_LOG_DEBUG("housing", "Neighborhood::RemoveManager: Cannot demote owner {} in neighborhood '{}'",
                    playerGuid.ToString(), _name);
                return HOUSING_RESULT_CANNOT_EVICT_OWNER;
            }

            if (member.Role != NEIGHBORHOOD_ROLE_MANAGER)
            {
                TC_LOG_DEBUG("housing", "Neighborhood::RemoveManager: Player {} is not a manager in neighborhood '{}'",
                    playerGuid.ToString(), _name);
                return HOUSING_RESULT_NOT_MANAGER;
            }

            member.Role = NEIGHBORHOOD_ROLE_RESIDENT;

            TC_LOG_DEBUG("housing", "Neighborhood::RemoveManager: Player {} demoted to resident in neighborhood '{}'",
                playerGuid.ToString(), _name);

            return HOUSING_RESULT_SUCCESS;
        }
    }

    TC_LOG_DEBUG("housing", "Neighborhood::RemoveManager: Player {} is not a member of neighborhood '{}'",
        playerGuid.ToString(), _name);
    return HOUSING_RESULT_NOT_IN_NEIGHBORHOOD;
}

HousingResult Neighborhood::InviteResident(ObjectGuid inviterGuid, ObjectGuid inviteeGuid)
{
    // Check inviter is owner or manager
    bool inviterHasPermission = false;
    for (Member const& member : _members)
    {
        if (member.PlayerGuid == inviterGuid)
        {
            if (member.Role == NEIGHBORHOOD_ROLE_OWNER || member.Role == NEIGHBORHOOD_ROLE_MANAGER)
                inviterHasPermission = true;
            break;
        }
    }

    if (!inviterHasPermission)
    {
        TC_LOG_DEBUG("housing", "Neighborhood::InviteResident: Inviter {} lacks permission in neighborhood '{}'",
            inviterGuid.ToString(), _name);
        return HOUSING_RESULT_NOT_MANAGER;
    }

    // Check invitee is not already a member
    for (Member const& member : _members)
    {
        if (member.PlayerGuid == inviteeGuid)
        {
            TC_LOG_DEBUG("housing", "Neighborhood::InviteResident: Player {} is already a member of neighborhood '{}'",
                inviteeGuid.ToString(), _name);
            return HOUSING_RESULT_ALREADY_IN_NEIGHBORHOOD;
        }
    }

    // Check invitee does not already have a pending invite
    for (PendingInvite const& invite : _pendingInvites)
    {
        if (invite.InviteeGuid == inviteeGuid)
        {
            TC_LOG_DEBUG("housing", "Neighborhood::InviteResident: Player {} already has a pending invite to neighborhood '{}'",
                inviteeGuid.ToString(), _name);
            return HOUSING_RESULT_ALREADY_INVITED;
        }
    }

    // Check faction restriction
    if (_factionRestriction != NEIGHBORHOOD_FACTION_NONE)
    {
        Player* invitee = ObjectAccessor::FindPlayer(inviteeGuid);
        if (invitee)
        {
            uint32 team = invitee->GetTeam();
            if ((_factionRestriction == NEIGHBORHOOD_FACTION_HORDE && team != HORDE) ||
                (_factionRestriction == NEIGHBORHOOD_FACTION_ALLIANCE && team != ALLIANCE))
            {
                TC_LOG_DEBUG("housing", "Neighborhood::InviteResident: Player {} faction mismatch for neighborhood '{}'",
                    inviteeGuid.ToString(), _name);
                return HOUSING_RESULT_FACTION_MISMATCH;
            }
        }
    }

    // Create the pending invite
    PendingInvite invite;
    invite.InviteeGuid  = inviteeGuid;
    invite.InviterGuid  = inviterGuid;
    invite.InviteTime   = static_cast<uint32>(GameTime::GetGameTime());
    _pendingInvites.push_back(invite);

    // Persist to DB immediately
    CharacterDatabaseTransaction trans = CharacterDatabase.BeginTransaction();
    CharacterDatabasePreparedStatement* stmt = CharacterDatabase.GetPreparedStatement(CHAR_INS_NEIGHBORHOOD_INVITE);
    uint8 index = 0;
    stmt->setUInt64(index++, _guid.GetCounter());
    stmt->setUInt64(index++, inviteeGuid.GetCounter());
    stmt->setUInt64(index++, inviterGuid.GetCounter());
    stmt->setUInt32(index++, invite.InviteTime);
    trans->Append(stmt);
    CharacterDatabase.CommitTransaction(trans);

    TC_LOG_DEBUG("housing", "Neighborhood::InviteResident: Player {} invited to neighborhood '{}' by {}",
        inviteeGuid.ToString(), _name, inviterGuid.ToString());

    return HOUSING_RESULT_SUCCESS;
}

HousingResult Neighborhood::CancelInvitation(ObjectGuid inviteeGuid)
{
    auto it = std::find_if(_pendingInvites.begin(), _pendingInvites.end(),
        [&inviteeGuid](PendingInvite const& invite) { return invite.InviteeGuid == inviteeGuid; });

    if (it == _pendingInvites.end())
    {
        TC_LOG_DEBUG("housing", "Neighborhood::CancelInvitation: No pending invite for {} in neighborhood '{}'",
            inviteeGuid.ToString(), _name);
        return HOUSING_RESULT_PLAYER_NOT_FOUND;
    }

    _pendingInvites.erase(it);

    // Remove from DB
    CharacterDatabaseTransaction trans = CharacterDatabase.BeginTransaction();
    CharacterDatabasePreparedStatement* stmt = CharacterDatabase.GetPreparedStatement(CHAR_DEL_NEIGHBORHOOD_INVITE);
    stmt->setUInt64(0, _guid.GetCounter());
    stmt->setUInt64(1, inviteeGuid.GetCounter());
    trans->Append(stmt);
    CharacterDatabase.CommitTransaction(trans);

    TC_LOG_DEBUG("housing", "Neighborhood::CancelInvitation: Invitation for {} cancelled in neighborhood '{}'",
        inviteeGuid.ToString(), _name);

    return HOUSING_RESULT_SUCCESS;
}

HousingResult Neighborhood::AcceptInvitation(ObjectGuid playerGuid)
{
    auto it = std::find_if(_pendingInvites.begin(), _pendingInvites.end(),
        [&playerGuid](PendingInvite const& invite) { return invite.InviteeGuid == playerGuid; });

    if (it == _pendingInvites.end())
    {
        TC_LOG_DEBUG("housing", "Neighborhood::AcceptInvitation: No pending invite for {} in neighborhood '{}'",
            playerGuid.ToString(), _name);
        return HOUSING_RESULT_PLAYER_NOT_FOUND;
    }

    // Check neighborhood not full
    if (_members.size() >= MAX_NEIGHBORHOOD_PLOTS)
    {
        TC_LOG_DEBUG("housing", "Neighborhood::AcceptInvitation: Neighborhood '{}' is full ({} members)",
            _name, _members.size());
        return HOUSING_RESULT_NEIGHBORHOOD_FULL;
    }

    // Add as resident
    Member newMember;
    newMember.PlayerGuid    = playerGuid;
    newMember.Role          = NEIGHBORHOOD_ROLE_RESIDENT;
    newMember.JoinTime      = static_cast<uint32>(GameTime::GetGameTime());
    newMember.PlotIndex     = INVALID_PLOT_INDEX;
    _members.push_back(newMember);

    // Remove the invite
    _pendingInvites.erase(it);

    // Persist both changes to DB
    CharacterDatabaseTransaction trans = CharacterDatabase.BeginTransaction();

    CharacterDatabasePreparedStatement* stmt = CharacterDatabase.GetPreparedStatement(CHAR_INS_NEIGHBORHOOD_MEMBER);
    uint8 index = 0;
    stmt->setUInt64(index++, _guid.GetCounter());
    stmt->setUInt64(index++, newMember.PlayerGuid.GetCounter());
    stmt->setUInt8(index++, newMember.Role);
    stmt->setUInt32(index++, newMember.JoinTime);
    stmt->setUInt8(index++, newMember.PlotIndex);
    trans->Append(stmt);

    stmt = CharacterDatabase.GetPreparedStatement(CHAR_DEL_NEIGHBORHOOD_INVITE);
    stmt->setUInt64(0, _guid.GetCounter());
    stmt->setUInt64(1, playerGuid.GetCounter());
    trans->Append(stmt);

    CharacterDatabase.CommitTransaction(trans);

    TC_LOG_DEBUG("housing", "Neighborhood::AcceptInvitation: Player {} joined neighborhood '{}' as resident",
        playerGuid.ToString(), _name);

    return HOUSING_RESULT_SUCCESS;
}

HousingResult Neighborhood::DeclineInvitation(ObjectGuid playerGuid)
{
    auto it = std::find_if(_pendingInvites.begin(), _pendingInvites.end(),
        [&playerGuid](PendingInvite const& invite) { return invite.InviteeGuid == playerGuid; });

    if (it == _pendingInvites.end())
    {
        TC_LOG_DEBUG("housing", "Neighborhood::DeclineInvitation: No pending invite for {} in neighborhood '{}'",
            playerGuid.ToString(), _name);
        return HOUSING_RESULT_PLAYER_NOT_FOUND;
    }

    _pendingInvites.erase(it);

    // Remove from DB
    CharacterDatabaseTransaction trans = CharacterDatabase.BeginTransaction();
    CharacterDatabasePreparedStatement* stmt = CharacterDatabase.GetPreparedStatement(CHAR_DEL_NEIGHBORHOOD_INVITE);
    stmt->setUInt64(0, _guid.GetCounter());
    stmt->setUInt64(1, playerGuid.GetCounter());
    trans->Append(stmt);
    CharacterDatabase.CommitTransaction(trans);

    TC_LOG_DEBUG("housing", "Neighborhood::DeclineInvitation: Player {} declined invite to neighborhood '{}'",
        playerGuid.ToString(), _name);

    return HOUSING_RESULT_INVITE_DECLINED;
}

HousingResult Neighborhood::EvictPlayer(ObjectGuid playerGuid)
{
    auto it = std::find_if(_members.begin(), _members.end(),
        [&playerGuid](Member const& member) { return member.PlayerGuid == playerGuid; });

    if (it == _members.end())
    {
        TC_LOG_DEBUG("housing", "Neighborhood::EvictPlayer: Player {} is not in neighborhood '{}'",
            playerGuid.ToString(), _name);
        return HOUSING_RESULT_NOT_IN_NEIGHBORHOOD;
    }

    if (it->Role == NEIGHBORHOOD_ROLE_OWNER)
    {
        TC_LOG_DEBUG("housing", "Neighborhood::EvictPlayer: Cannot evict owner {} from neighborhood '{}'",
            playerGuid.ToString(), _name);
        return HOUSING_RESULT_CANNOT_EVICT_OWNER;
    }

    // Remove any plot assignment
    if (it->PlotIndex != INVALID_PLOT_INDEX)
    {
        auto plotIt = std::find_if(_plots.begin(), _plots.end(),
            [plotIndex = it->PlotIndex](PlotInfo const& plot) { return plot.PlotIndex == plotIndex; });
        if (plotIt != _plots.end())
            _plots.erase(plotIt);
    }

    _members.erase(it);

    // Remove from DB
    CharacterDatabaseTransaction trans = CharacterDatabase.BeginTransaction();
    CharacterDatabasePreparedStatement* stmt = CharacterDatabase.GetPreparedStatement(CHAR_DEL_NEIGHBORHOOD_MEMBER);
    stmt->setUInt64(0, _guid.GetCounter());
    stmt->setUInt64(1, playerGuid.GetCounter());
    trans->Append(stmt);
    CharacterDatabase.CommitTransaction(trans);

    TC_LOG_DEBUG("housing", "Neighborhood::EvictPlayer: Player {} evicted from neighborhood '{}'",
        playerGuid.ToString(), _name);

    return HOUSING_RESULT_SUCCESS;
}

HousingResult Neighborhood::TransferOwnership(ObjectGuid newOwnerGuid)
{
    Member* oldOwner = nullptr;
    Member* newOwner = nullptr;

    for (Member& member : _members)
    {
        if (member.PlayerGuid == _ownerGuid)
            oldOwner = &member;
        if (member.PlayerGuid == newOwnerGuid)
            newOwner = &member;
    }

    if (!newOwner)
    {
        TC_LOG_DEBUG("housing", "Neighborhood::TransferOwnership: New owner {} is not a member of neighborhood '{}'",
            newOwnerGuid.ToString(), _name);
        return HOUSING_RESULT_NOT_IN_NEIGHBORHOOD;
    }

    if (!oldOwner)
    {
        TC_LOG_DEBUG("housing", "Neighborhood::TransferOwnership: Current owner {} not found in member list of neighborhood '{}'",
            _ownerGuid.ToString(), _name);
        return HOUSING_RESULT_INTERNAL_ERROR;
    }

    // Promote new owner, demote old owner to manager
    newOwner->Role = NEIGHBORHOOD_ROLE_OWNER;
    oldOwner->Role = NEIGHBORHOOD_ROLE_MANAGER;
    _ownerGuid = newOwnerGuid;

    TC_LOG_DEBUG("housing", "Neighborhood::TransferOwnership: Ownership of neighborhood '{}' transferred from {} to {}",
        _name, oldOwner->PlayerGuid.ToString(), newOwnerGuid.ToString());

    return HOUSING_RESULT_SUCCESS;
}

HousingResult Neighborhood::PurchasePlot(ObjectGuid playerGuid, uint8 plotIndex)
{
    if (plotIndex >= MAX_NEIGHBORHOOD_PLOTS)
    {
        TC_LOG_DEBUG("housing", "Neighborhood::PurchasePlot: Invalid plot index {} in neighborhood '{}'",
            plotIndex, _name);
        return HOUSING_RESULT_INVALID_PLOT;
    }

    // Check if player is a member
    Member* buyer = nullptr;
    for (Member& member : _members)
    {
        if (member.PlayerGuid == playerGuid)
        {
            buyer = &member;
            break;
        }
    }

    if (!buyer)
    {
        TC_LOG_DEBUG("housing", "Neighborhood::PurchasePlot: Player {} is not a member of neighborhood '{}'",
            playerGuid.ToString(), _name);
        return HOUSING_RESULT_NOT_IN_NEIGHBORHOOD;
    }

    // Check if player already has a plot
    if (buyer->PlotIndex != INVALID_PLOT_INDEX)
    {
        TC_LOG_DEBUG("housing", "Neighborhood::PurchasePlot: Player {} already owns plot {} in neighborhood '{}'",
            playerGuid.ToString(), buyer->PlotIndex, _name);
        return HOUSING_RESULT_PLOT_OCCUPIED;
    }

    // Check if plot is already occupied
    for (PlotInfo const& plot : _plots)
    {
        if (plot.PlotIndex == plotIndex)
        {
            TC_LOG_DEBUG("housing", "Neighborhood::PurchasePlot: Plot {} is already occupied in neighborhood '{}'",
                plotIndex, _name);
            return HOUSING_RESULT_PLOT_OCCUPIED;
        }
    }

    // Assign the plot
    buyer->PlotIndex = plotIndex;

    PlotInfo newPlot;
    newPlot.PlotIndex   = plotIndex;
    newPlot.OwnerGuid   = playerGuid;
    _plots.push_back(newPlot);

    TC_LOG_DEBUG("housing", "Neighborhood::PurchasePlot: Player {} purchased plot {} in neighborhood '{}'",
        playerGuid.ToString(), plotIndex, _name);

    return HOUSING_RESULT_SUCCESS;
}

HousingResult Neighborhood::MoveHouse(ObjectGuid sourcePlotOwner, uint8 newPlotIndex)
{
    if (newPlotIndex >= MAX_NEIGHBORHOOD_PLOTS)
    {
        TC_LOG_DEBUG("housing", "Neighborhood::MoveHouse: Invalid target plot index {} in neighborhood '{}'",
            newPlotIndex, _name);
        return HOUSING_RESULT_INVALID_PLOT;
    }

    // Check destination is not occupied
    for (PlotInfo const& plot : _plots)
    {
        if (plot.PlotIndex == newPlotIndex)
        {
            TC_LOG_DEBUG("housing", "Neighborhood::MoveHouse: Target plot {} is occupied in neighborhood '{}'",
                newPlotIndex, _name);
            return HOUSING_RESULT_PLOT_OCCUPIED;
        }
    }

    // Find the source plot
    PlotInfo* sourcePlot = nullptr;
    for (PlotInfo& plot : _plots)
    {
        if (plot.OwnerGuid == sourcePlotOwner)
        {
            sourcePlot = &plot;
            break;
        }
    }

    if (!sourcePlot)
    {
        TC_LOG_DEBUG("housing", "Neighborhood::MoveHouse: Player {} has no plot in neighborhood '{}'",
            sourcePlotOwner.ToString(), _name);
        return HOUSING_RESULT_NO_PLOT;
    }

    uint8 oldPlotIndex = sourcePlot->PlotIndex;
    sourcePlot->PlotIndex = newPlotIndex;

    // Update the member's plot index as well
    for (Member& member : _members)
    {
        if (member.PlayerGuid == sourcePlotOwner)
        {
            member.PlotIndex = newPlotIndex;
            break;
        }
    }

    TC_LOG_DEBUG("housing", "Neighborhood::MoveHouse: Player {} moved house from plot {} to plot {} in neighborhood '{}'",
        sourcePlotOwner.ToString(), oldPlotIndex, newPlotIndex, _name);

    return HOUSING_RESULT_SUCCESS;
}

Neighborhood::PlotInfo const* Neighborhood::GetPlotInfo(uint8 plotIndex) const
{
    for (PlotInfo const& plot : _plots)
        if (plot.PlotIndex == plotIndex)
            return &plot;

    return nullptr;
}

Neighborhood::Member const* Neighborhood::GetMember(ObjectGuid playerGuid) const
{
    for (Member const& member : _members)
        if (member.PlayerGuid == playerGuid)
            return &member;

    return nullptr;
}

bool Neighborhood::IsMember(ObjectGuid playerGuid) const
{
    return GetMember(playerGuid) != nullptr;
}

bool Neighborhood::IsManager(ObjectGuid playerGuid) const
{
    Member const* member = GetMember(playerGuid);
    return member && (member->Role == NEIGHBORHOOD_ROLE_MANAGER || member->Role == NEIGHBORHOOD_ROLE_OWNER);
}

bool Neighborhood::IsOwner(ObjectGuid playerGuid) const
{
    return _ownerGuid == playerGuid;
}

bool Neighborhood::HasPendingInvite(ObjectGuid playerGuid) const
{
    return std::any_of(_pendingInvites.begin(), _pendingInvites.end(),
        [&playerGuid](PendingInvite const& invite) { return invite.InviteeGuid == playerGuid; });
}
