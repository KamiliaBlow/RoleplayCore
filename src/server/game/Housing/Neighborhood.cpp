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
#include "BattlenetAccountMgr.h"
#include "DatabaseEnv.h"
#include "GameTime.h"
#include "HousingMgr.h"
#include "HousingNeighborhoodMirrorEntity.h"
#include "HousingPackets.h"
#include "Log.h"
#include "Map.h"
#include "MapManager.h"
#include "ObjectAccessor.h"
#include "Player.h"
#include "RealmList.h"
#include "WorldSession.h"
#include <algorithm>

namespace
{
    constexpr uint32 RESERVATION_EXPIRY_SECONDS = 5 * MINUTE;
    constexpr uint32 OWNERSHIP_TRANSFER_TIMEOUT = 5 * MINUTE;
}

Neighborhood::Neighborhood(ObjectGuid guid) : _guid(guid)
{
}

bool Neighborhood::LoadFromDB(PreparedQueryResult neighborhood, PreparedQueryResult members, PreparedQueryResult invites,
    PreparedQueryResult memberFixtures /*= nullptr*/, PreparedQueryResult memberDecor /*= nullptr*/,
    PreparedQueryResult memberRooms /*= nullptr*/)
{
    if (!neighborhood)
        return false;

    Field* fields = neighborhood->Fetch();

    //          0     1       2            3                    4         5          6          7
    // SELECT guid, name, neighborhoodMapID, ownerGuid, factionRestriction, isPublic, createTime, guildId
    //        FROM neighborhoods WHERE guid = ?
    _name               = fields[1].GetString();
    _neighborhoodMapID  = fields[2].GetUInt32();
    {
        uint64 ownerCounter = fields[3].GetUInt64();
        _ownerGuid = ownerCounter ? ObjectGuid::Create<HighGuid::Player>(ownerCounter) : ObjectGuid::Empty;
    }
    _factionRestriction = fields[4].GetInt32();
    _isPublic           = fields[5].GetBool();
    _createTime         = fields[6].GetUInt32();
    _guildId            = fields[7].GetUInt32(); // 0 = not a guild neighborhood

    // Load members
    if (members)
    {
        do
        {
            Field* memberFields = members->Fetch();

            //          0              1     2          3          4           5          6              7         8           9            10           11    12    13    14
            // SELECT nm.playerGuid, nm.role, nm.joinTime, nm.plotIndex, ch.houseId, c.account, ch.houseLevel, ch.favor, ch.houseName, ch.houseType, ch.settingsFlags, ch.posX, ch.posY, ch.posZ, ch.facing
            // FROM neighborhood_members nm LEFT JOIN character_housing ch ON nm.playerGuid = ch.guid
            //   LEFT JOIN characters c ON nm.playerGuid = c.guid
            // WHERE nm.neighborhoodGuid = ?

            Member member;
            member.PlayerGuid   = ObjectGuid::Create<HighGuid::Player>(memberFields[0].GetUInt64());
            member.Role         = memberFields[1].GetUInt8();
            member.JoinTime     = memberFields[2].GetUInt32();
            member.PlotIndex    = memberFields[3].GetUInt8();

            _members.push_back(member);

            // Build plot info from members that have plots assigned
            if (member.PlotIndex != INVALID_PLOT_INDEX && member.PlotIndex < MAX_NEIGHBORHOOD_PLOTS)
            {
                PlotInfo& plot = _plots[member.PlotIndex];
                plot.PlotIndex = member.PlotIndex;
                plot.OwnerGuid = member.PlayerGuid;

                // Client requires a non-zero BNet account GUID for plot IsInsidePlot() validation.
                uint32 gameAccountId = memberFields[5].GetUInt32();
                if (gameAccountId != 0)
                {
                    uint32 bnetAccountId = Battlenet::AccountMgr::GetIdByGameAccount(gameAccountId);
                    if (bnetAccountId != 0)
                    {
                        plot.OwnerBnetGuid = ObjectGuid::Create<HighGuid::BNetAccount>(bnetAccountId);
                        // Must match HousingPlayerHouseEntity's GUID, which uses the battlenetAccountId
                        plot.HouseGuid = Housing::MakeHouseGuid(_neighborhoodMapID, bnetAccountId);
                    }
                }

                // NULL ch.* columns (no character_housing row yet) keep the defaults.
                if (!memberFields[6].IsNull())
                    plot.HouseLevel = std::max<uint8>(1, memberFields[6].GetUInt8());
                if (!memberFields[7].IsNull())
                    plot.HouseFavor = memberFields[7].GetUInt64();
                if (!memberFields[8].IsNull())
                    plot.HouseName = memberFields[8].GetString();
                if (!memberFields[9].IsNull())
                    plot.HouseType = memberFields[9].GetUInt32();
                if (!memberFields[10].IsNull())
                    plot.HouseSettingsFlags = memberFields[10].GetUInt32();
                // All-zero coordinates mean "no custom position" (Housing::LoadFromDB does the same)
                if (!memberFields[11].IsNull())
                {
                    Position housePos(memberFields[11].GetFloat(), memberFields[12].GetFloat(), memberFields[13].GetFloat(), memberFields[14].GetFloat());
                    if (housePos.GetPositionX() != 0.0f || housePos.GetPositionY() != 0.0f || housePos.GetPositionZ() != 0.0f)
                        plot.HousePosition = housePos;
                }
            }
        } while (members->NextRow());
    }

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

    // Fixture overrides for occupied plots whose owners are offline.
    if (memberFixtures)
    {
        do
        {
            Field* f = memberFixtures->Fetch();
            //   0           1                 2
            // ownerGuid, fixturePointId, fixtureOptionId
            ObjectGuid ownerGuid = ObjectGuid::Create<HighGuid::Player>(f[0].GetUInt64());
            PlotInfo* plot = GetPlotByOwner(ownerGuid);
            if (!plot)
                continue;
            plot->Fixtures[f[1].GetUInt32()] = f[2].GetUInt32();
        } while (memberFixtures->NextRow());
    }

    // Interior room layout per owner, spawned for visitors entering their house.
    // Loaded before decor so decor RoomGuids can resolve against the owner's room instances.
    if (memberRooms)
    {
        do
        {
            Field* r = memberRooms->Fetch();
            //   0         1       2              3           4      5      6             7            8         9          10             11              12               13              14          15        16             17           18            19             20            21          22
            // ownerGuid, id, houseRoomId, slotIndex, gridX, gridY, floorIndex, orientation, mirrored, themeId, wallTextureId, floorTextureId, ceilingTextureId, colorOverride, doorTypeId, doorSlot, ceilingTypeId, ceilingSlot, wallThemeId, floorThemeId, ceilingThemeId, doorTypes, componentStyles
            ObjectGuid ownerGuid = ObjectGuid::Create<HighGuid::Player>(r[0].GetUInt64());
            PlotInfo* plot = GetPlotByOwner(ownerGuid);
            if (!plot)
                continue;

            Housing::Room room;
            room.Guid             = ObjectGuidFactory::CreateHousing(/*subType*/ 2, /*realmId*/ 0, /*arg2*/ 0, r[1].GetUInt64());
            room.RoomEntryId      = r[2].GetUInt32();
            room.SlotIndex        = r[3].GetUInt32();
            room.GridX            = r[4].GetInt32();
            room.GridY            = r[5].GetInt32();
            room.FloorIndex       = r[6].GetInt32();
            room.Orientation      = r[7].GetUInt8();
            room.Mirrored         = r[8].GetUInt8() != 0;
            room.ThemeId          = r[9].GetUInt32();
            room.WallTextureId    = r[10].GetUInt32();
            room.FloorTextureId   = r[11].GetUInt32();
            room.CeilingTextureId = r[12].GetUInt32();
            room.ColorOverride    = r[13].GetInt32();
            room.DoorTypeId       = r[14].GetUInt32();
            room.DoorSlot         = r[15].GetUInt8();
            room.CeilingTypeId    = r[16].GetUInt32();
            room.CeilingSlot      = r[17].GetUInt8();
            room.WallThemeId      = r[18].GetUInt32();
            room.FloorThemeId     = r[19].GetUInt32();
            room.CeilingThemeId   = r[20].GetUInt32();
            Housing::LoadDoorTypes(room, r[21].GetString());
            Housing::LoadComponentStyles(room, r[22].GetString());
            plot->Rooms.push_back(std::move(room));
        } while (memberRooms->NextRow());
    }

    // Placed decor for occupied plots (exterior spawns at preload; interior serves visitors).
    if (memberDecor)
    {
        do
        {
            Field* d = memberDecor->Fetch();
            //   0      1            2            3     4     5     6     7     8     9       10     11        12        13       14        15      16            17           18
            // id, ownerGuid, houseDecorId, posX, posY, posZ, rotX, rotY, rotZ, rotW, scale, dyeSlot0, dyeSlot1, dyeSlot2, roomGuid, locked, placementTime, sourceType, sourceValue
            ObjectGuid ownerGuid = ObjectGuid::Create<HighGuid::Player>(d[1].GetUInt64());
            PlotInfo* plot = GetPlotByOwner(ownerGuid);
            if (!plot)
                continue;

            Housing::PlacedDecor decor;
            // realmId must match Housing::LoadFromDB; the client keys decor GUIDs by it.
            decor.Guid          = ObjectGuidFactory::CreateHousing(/*subType*/ 1, /*realmId*/ sRealmList->GetCurrentRealmId().Realm, d[2].GetUInt32(), d[0].GetUInt64());
            decor.DecorEntryId  = d[2].GetUInt32();
            decor.PosX          = d[3].GetFloat();
            decor.PosY          = d[4].GetFloat();
            decor.PosZ          = d[5].GetFloat();
            decor.RotationX     = d[6].GetFloat();
            decor.RotationY     = d[7].GetFloat();
            decor.RotationZ     = d[8].GetFloat();
            decor.RotationW     = d[9].GetFloat();
            decor.Scale         = d[10].GetFloat();
            decor.DyeSlots[0]   = d[11].GetUInt32();
            decor.DyeSlots[1]   = d[12].GetUInt32();
            decor.DyeSlots[2]   = d[13].GetUInt32();
            // Same resolution as Housing::LoadFromDB: a counter matching no room instance
            // (exterior decor stores the plot base-room identity) means exterior — leave empty.
            if (uint64 roomCounter = d[14].GetUInt64())
                for (Housing::Room const& room : plot->Rooms)
                    if (room.Guid.GetCounter() == roomCounter)
                    {
                        decor.RoomGuid = room.Guid;
                        break;
                    }
            decor.Locked        = d[15].GetUInt8() != 0;
            decor.PlacementTime = static_cast<time_t>(d[16].GetUInt64());
            decor.SourceType    = d[17].GetUInt8();
            decor.SourceValue   = d[18].GetString();
            plot->Decor.push_back(std::move(decor));
        } while (memberDecor->NextRow());
    }

    return true;
}

void Neighborhood::SaveToDB(CharacterDatabaseTransaction trans)
{
    CharacterDatabasePreparedStatement* stmt = CharacterDatabase.GetPreparedStatement(CHAR_REP_NEIGHBORHOOD);
    uint8 index = 0;
    stmt->setUInt64(index++, _guid.GetCounter());
    stmt->setString(index++, _name);
    stmt->setUInt32(index++, _neighborhoodMapID);
    stmt->setUInt64(index++, _ownerGuid.GetCounter());
    stmt->setInt32(index++, _factionRestriction);
    stmt->setBool(index++, _isPublic);
    stmt->setUInt32(index++, _createTime);
    stmt->setUInt32(index++, _guildId);
    trans->Append(stmt);

    // Replace members
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
}

void Neighborhood::DeleteFromDB(ObjectGuid::LowType guid, CharacterDatabaseTransaction trans)
{
    CharacterDatabasePreparedStatement* stmt = CharacterDatabase.GetPreparedStatement(CHAR_DEL_NEIGHBORHOOD_INVITES);
    stmt->setUInt64(0, guid);
    trans->Append(stmt);

    stmt = CharacterDatabase.GetPreparedStatement(CHAR_DEL_NEIGHBORHOOD_MEMBERS);
    stmt->setUInt64(0, guid);
    trans->Append(stmt);

    stmt = CharacterDatabase.GetPreparedStatement(CHAR_DEL_NEIGHBORHOOD);
    stmt->setUInt64(0, guid);
    trans->Append(stmt);
}

void Neighborhood::SetName(std::string const& name)
{
    _name = name;

    CharacterDatabasePreparedStatement* stmt = CharacterDatabase.GetPreparedStatement(CHAR_UPD_NEIGHBORHOOD_NAME);
    stmt->setString(0, _name);
    stmt->setUInt64(1, _guid.GetCounter());
    CharacterDatabase.Execute(stmt);

}

void Neighborhood::SetPublic(bool isPublic)
{
    _isPublic = isPublic;

    CharacterDatabasePreparedStatement* stmt = CharacterDatabase.GetPreparedStatement(CHAR_UPD_NEIGHBORHOOD_PUBLIC);
    stmt->setBool(0, _isPublic);
    stmt->setUInt64(1, _guid.GetCounter());
    CharacterDatabase.Execute(stmt);
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
        return HOUSING_RESULT_NEIGHBORHOOD_NOT_FOUND;
    }

    if (targetMember->Role == NEIGHBORHOOD_ROLE_OWNER)
    {
        return HOUSING_RESULT_PERMISSION_DENIED;
    }

    if (targetMember->Role == NEIGHBORHOOD_ROLE_MANAGER)
    {
        return HOUSING_RESULT_SUCCESS;
    }

    if (managerCount >= MAX_NEIGHBORHOOD_MANAGERS)
    {
        // HousingResult has no "too many managers" value.
        return HOUSING_RESULT_PERMISSION_DENIED;
    }

    targetMember->Role = NEIGHBORHOOD_ROLE_MANAGER;

    CharacterDatabasePreparedStatement* stmt = CharacterDatabase.GetPreparedStatement(CHAR_UPD_NEIGHBORHOOD_MEMBER_ROLE);
    stmt->setUInt8(0, NEIGHBORHOOD_ROLE_MANAGER);
    stmt->setUInt64(1, _guid.GetCounter());
    stmt->setUInt64(2, playerGuid.GetCounter());
    CharacterDatabase.Execute(stmt);

    // Roster broadcast and mirror refresh happen in the handler layer.
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
                return HOUSING_RESULT_PERMISSION_DENIED;
            }

            if (member.Role != NEIGHBORHOOD_ROLE_MANAGER)
            {
                return HOUSING_RESULT_PERMISSION_DENIED;
            }

            member.Role = NEIGHBORHOOD_ROLE_RESIDENT;

            CharacterDatabasePreparedStatement* stmt = CharacterDatabase.GetPreparedStatement(CHAR_UPD_NEIGHBORHOOD_MEMBER_ROLE);
            stmt->setUInt8(0, NEIGHBORHOOD_ROLE_RESIDENT);
            stmt->setUInt64(1, _guid.GetCounter());
            stmt->setUInt64(2, playerGuid.GetCounter());
            CharacterDatabase.Execute(stmt);

            // Roster broadcast and mirror refresh happen in the handler layer.
            return HOUSING_RESULT_SUCCESS;
        }
    }

    return HOUSING_RESULT_NEIGHBORHOOD_NOT_FOUND;
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
        return HOUSING_RESULT_PERMISSION_DENIED;
    }

    // Check invite limit
    if (_pendingInvites.size() >= MAX_PENDING_INVITES)
    {
        return HOUSING_RESULT_TOO_MANY_REQUESTS;
    }

    // Check if neighborhood has available plots (rough check: members + pending >= totalPlots)
    if (GetOccupiedPlotCount() + _pendingInvites.size() >= MAX_NEIGHBORHOOD_PLOTS)
    {
        return HOUSING_RESULT_PLOT_NOT_VACANT;
    }

    // Check invitee is not already a member
    for (Member const& member : _members)
    {
        if (member.PlayerGuid == inviteeGuid)
        {
            return HOUSING_RESULT_GENERIC_FAILURE;
        }
    }

    // Check invitee does not already have a pending invite
    for (PendingInvite const& invite : _pendingInvites)
    {
        if (invite.InviteeGuid == inviteeGuid)
        {
            return HOUSING_RESULT_GENERIC_FAILURE;
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
                return HOUSING_RESULT_INCORRECT_FACTION;
            }
        }
    }

    // Online invitees with the auto-decline flag are rejected without creating an invite.
    if (Player* invitee = ObjectAccessor::FindPlayer(inviteeGuid))
    {
        if (invitee->HasPlayerFlagEx(PLAYER_FLAGS_EX_AUTO_DECLINE_NEIGHBORHOOD))
        {
            return HOUSING_RESULT_FILTER_REJECTED;
        }
    }

    // Create the pending invite
    PendingInvite invite;
    invite.InviteeGuid  = inviteeGuid;
    invite.InviterGuid  = inviterGuid;
    invite.InviteTime   = static_cast<uint32>(GameTime::GetGameTime());
    _pendingInvites.push_back(invite);

    CharacterDatabaseTransaction trans = CharacterDatabase.BeginTransaction();
    CharacterDatabasePreparedStatement* stmt = CharacterDatabase.GetPreparedStatement(CHAR_INS_NEIGHBORHOOD_INVITE);
    uint8 index = 0;
    stmt->setUInt64(index++, _guid.GetCounter());
    stmt->setUInt64(index++, inviteeGuid.GetCounter());
    stmt->setUInt64(index++, inviterGuid.GetCounter());
    stmt->setUInt32(index++, invite.InviteTime);
    trans->Append(stmt);
    CharacterDatabase.CommitTransaction(trans);

    return HOUSING_RESULT_SUCCESS;
}

HousingResult Neighborhood::CancelInvitation(ObjectGuid inviteeGuid)
{
    auto it = std::find_if(_pendingInvites.begin(), _pendingInvites.end(),
        [&inviteeGuid](PendingInvite const& invite) { return invite.InviteeGuid == inviteeGuid; });

    if (it == _pendingInvites.end())
    {
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

    return HOUSING_RESULT_SUCCESS;
}

HousingResult Neighborhood::AcceptInvitation(ObjectGuid playerGuid)
{
    auto it = std::find_if(_pendingInvites.begin(), _pendingInvites.end(),
        [&playerGuid](PendingInvite const& invite) { return invite.InviteeGuid == playerGuid; });

    if (it == _pendingInvites.end())
    {
        return HOUSING_RESULT_PLAYER_NOT_FOUND;
    }

    // Check neighborhood not full
    if (_members.size() >= MAX_NEIGHBORHOOD_PLOTS)
    {
        return HOUSING_RESULT_PLOT_NOT_VACANT;
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

    // A new resident: every online member's bulletin board gets the whole roster again.
    BroadcastRoster();

    return HOUSING_RESULT_SUCCESS;
}

HousingResult Neighborhood::AddResident(ObjectGuid playerGuid)
{
    // Already a member?
    if (IsMember(playerGuid))
        return HOUSING_RESULT_SUCCESS;

    // Check neighborhood not full
    if (_members.size() >= MAX_NEIGHBORHOOD_PLOTS)
    {
        return HOUSING_RESULT_PLOT_NOT_VACANT;
    }

    Member newMember;
    newMember.PlayerGuid    = playerGuid;
    newMember.Role          = NEIGHBORHOOD_ROLE_RESIDENT;
    newMember.JoinTime      = static_cast<uint32>(GameTime::GetGameTime());
    newMember.PlotIndex     = INVALID_PLOT_INDEX;
    _members.push_back(newMember);

    // Persist to DB
    CharacterDatabasePreparedStatement* stmt = CharacterDatabase.GetPreparedStatement(CHAR_INS_NEIGHBORHOOD_MEMBER);
    uint8 index = 0;
    stmt->setUInt64(index++, _guid.GetCounter());
    stmt->setUInt64(index++, newMember.PlayerGuid.GetCounter());
    stmt->setUInt8(index++, newMember.Role);
    stmt->setUInt32(index++, newMember.JoinTime);
    stmt->setUInt8(index++, newMember.PlotIndex);
    CharacterDatabase.Execute(stmt);

    // Clear any pending invite for this player now that they've joined
    auto inviteIt = std::find_if(_pendingInvites.begin(), _pendingInvites.end(),
        [&playerGuid](PendingInvite const& invite) { return invite.InviteeGuid == playerGuid; });
    if (inviteIt != _pendingInvites.end())
    {
        CharacterDatabasePreparedStatement* delStmt = CharacterDatabase.GetPreparedStatement(CHAR_DEL_NEIGHBORHOOD_INVITE);
        delStmt->setUInt64(0, _guid.GetCounter());
        delStmt->setUInt64(1, playerGuid.GetCounter());
        CharacterDatabase.Execute(delStmt);
        _pendingInvites.erase(inviteIt);
    }

    // A new resident: every online member's bulletin board gets the whole roster again.
    BroadcastRoster();

    return HOUSING_RESULT_SUCCESS;
}

HousingResult Neighborhood::DeclineInvitation(ObjectGuid playerGuid)
{
    auto it = std::find_if(_pendingInvites.begin(), _pendingInvites.end(),
        [&playerGuid](PendingInvite const& invite) { return invite.InviteeGuid == playerGuid; });

    if (it == _pendingInvites.end())
    {
        return HOUSING_RESULT_PLAYER_NOT_FOUND;
    }

    _pendingInvites.erase(it);

    CharacterDatabaseTransaction trans = CharacterDatabase.BeginTransaction();
    CharacterDatabasePreparedStatement* stmt = CharacterDatabase.GetPreparedStatement(CHAR_DEL_NEIGHBORHOOD_INVITE);
    stmt->setUInt64(0, _guid.GetCounter());
    stmt->setUInt64(1, playerGuid.GetCounter());
    trans->Append(stmt);
    CharacterDatabase.CommitTransaction(trans);

    // A declined invite is not an error.
    return HOUSING_RESULT_SUCCESS;
}

HousingResult Neighborhood::EvictPlayer(ObjectGuid playerGuid)
{
    auto it = std::find_if(_members.begin(), _members.end(),
        [&playerGuid](Member const& member) { return member.PlayerGuid == playerGuid; });

    if (it == _members.end())
    {
        return HOUSING_RESULT_NEIGHBORHOOD_NOT_FOUND;
    }

    if (it->Role == NEIGHBORHOOD_ROLE_OWNER)
    {
        return HOUSING_RESULT_PERMISSION_DENIED;
    }

    // Clear any plot assignment
    if (it->PlotIndex != INVALID_PLOT_INDEX && it->PlotIndex < MAX_NEIGHBORHOOD_PLOTS)
        _plots[it->PlotIndex] = PlotInfo{};

    _members.erase(it);

    // Remove from DB
    CharacterDatabaseTransaction trans = CharacterDatabase.BeginTransaction();
    CharacterDatabasePreparedStatement* stmt = CharacterDatabase.GetPreparedStatement(CHAR_DEL_NEIGHBORHOOD_MEMBER);
    stmt->setUInt64(0, _guid.GetCounter());
    stmt->setUInt64(1, playerGuid.GetCounter());
    trans->Append(stmt);
    CharacterDatabase.CommitTransaction(trans);

    // The evicted player is gone from the roster: every remaining member gets it again.
    BroadcastRoster();

    return HOUSING_RESULT_SUCCESS;
}

bool Neighborhood::ReleasePlot(ObjectGuid ownerGuid)
{
    bool freed = false;

    // Free every plot recorded as owned by this player (normally one).
    for (uint8 i = 0; i < MAX_NEIGHBORHOOD_PLOTS; ++i)
    {
        if (_plots[i].IsOccupied() && _plots[i].OwnerGuid == ownerGuid)
        {
            _plots[i] = PlotInfo{};
            freed = true;
        }
    }

    // Clear the member's plot assignment (memory + DB) so they can buy again.
    auto it = std::find_if(_members.begin(), _members.end(),
        [&ownerGuid](Member const& member) { return member.PlayerGuid == ownerGuid; });
    if (it != _members.end() && it->PlotIndex != INVALID_PLOT_INDEX)
    {
        it->PlotIndex = INVALID_PLOT_INDEX;

        CharacterDatabasePreparedStatement* stmt = CharacterDatabase.GetPreparedStatement(CHAR_UPD_NEIGHBORHOOD_MEMBER_PLOT);
        stmt->setUInt8(0, INVALID_PLOT_INDEX);
        stmt->setUInt64(1, _guid.GetCounter());
        stmt->setUInt64(2, ownerGuid.GetCounter());
        CharacterDatabase.Execute(stmt);
        freed = true;
    }

    return freed;
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
        return HOUSING_RESULT_NEIGHBORHOOD_NOT_FOUND;
    }

    if (!oldOwner)
    {
        return HOUSING_RESULT_RPC_FAILURE;
    }

    // Promote new owner, demote old owner to manager
    newOwner->Role = NEIGHBORHOOD_ROLE_OWNER;
    oldOwner->Role = NEIGHBORHOOD_ROLE_MANAGER;
    ObjectGuid previousOwnerGuid = _ownerGuid;
    _ownerGuid = newOwnerGuid;

    // Persist the ownership change to the database
    CharacterDatabaseTransaction trans = CharacterDatabase.BeginTransaction();

    // Update the neighborhood's ownerGuid
    CharacterDatabasePreparedStatement* stmt = CharacterDatabase.GetPreparedStatement(CHAR_UPD_NEIGHBORHOOD_OWNER);
    stmt->setUInt64(0, newOwnerGuid.GetCounter());
    stmt->setUInt64(1, _guid.GetCounter());
    trans->Append(stmt);

    // Update the old owner's role to manager
    stmt = CharacterDatabase.GetPreparedStatement(CHAR_UPD_NEIGHBORHOOD_MEMBER_ROLE);
    stmt->setUInt8(0, NEIGHBORHOOD_ROLE_MANAGER);
    stmt->setUInt64(1, _guid.GetCounter());
    stmt->setUInt64(2, previousOwnerGuid.GetCounter());
    trans->Append(stmt);

    // Update the new owner's role to owner
    stmt = CharacterDatabase.GetPreparedStatement(CHAR_UPD_NEIGHBORHOOD_MEMBER_ROLE);
    stmt->setUInt8(0, NEIGHBORHOOD_ROLE_OWNER);
    stmt->setUInt64(1, _guid.GetCounter());
    stmt->setUInt64(2, newOwnerGuid.GetCounter());
    trans->Append(stmt);

    CharacterDatabase.CommitTransaction(trans);

    // Both resident types changed.
    BroadcastMemberStatus(previousOwnerGuid);
    BroadcastMemberStatus(newOwnerGuid);

    return HOUSING_RESULT_SUCCESS;
}

HousingResult Neighborhood::OfferOwnership(ObjectGuid targetGuid)
{
    if (_pendingTransfer.has_value())
    {
        return HOUSING_RESULT_GENERIC_FAILURE;
    }

    if (!IsMember(targetGuid))
    {
        return HOUSING_RESULT_NEIGHBORHOOD_NOT_FOUND;
    }

    PendingOwnershipTransfer transfer;
    transfer.TargetGuid = targetGuid;
    transfer.OfferTime = static_cast<uint32>(GameTime::GetGameTime());
    _pendingTransfer = transfer;

    return HOUSING_RESULT_SUCCESS;
}

HousingResult Neighborhood::AcceptOwnershipTransfer(ObjectGuid acceptorGuid)
{
    if (!_pendingTransfer.has_value())
    {
        return HOUSING_RESULT_NO_NEIGHBORHOOD_OWNERSHIP_REQUESTS;
    }

    if (_pendingTransfer->TargetGuid != acceptorGuid)
    {
        return HOUSING_RESULT_PERMISSION_DENIED;
    }

    uint32 now = static_cast<uint32>(GameTime::GetGameTime());
    if (now - _pendingTransfer->OfferTime > OWNERSHIP_TRANSFER_TIMEOUT)
    {
        _pendingTransfer.reset();
        return HOUSING_RESULT_TIMEOUT_LIMIT;
    }

    _pendingTransfer.reset();
    return TransferOwnership(acceptorGuid);
}

HousingResult Neighborhood::RejectOwnershipTransfer(ObjectGuid rejectorGuid)
{
    if (!_pendingTransfer.has_value())
    {
        return HOUSING_RESULT_NO_NEIGHBORHOOD_OWNERSHIP_REQUESTS;
    }

    if (_pendingTransfer->TargetGuid != rejectorGuid)
    {
        return HOUSING_RESULT_PERMISSION_DENIED;
    }

    _pendingTransfer.reset();

    return HOUSING_RESULT_SUCCESS;
}

HousingResult Neighborhood::PurchasePlot(ObjectGuid playerGuid, uint8 plotIndex)
{
    if (plotIndex >= MAX_NEIGHBORHOOD_PLOTS)
    {
        return HOUSING_RESULT_PLOT_NOT_FOUND;
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
        return HOUSING_RESULT_NEIGHBORHOOD_NOT_FOUND;
    }

    // Check if player already has a plot
    if (buyer->PlotIndex != INVALID_PLOT_INDEX)
    {
        return HOUSING_RESULT_PLOT_NOT_VACANT;
    }

    // Check if plot is already occupied
    if (_plots[plotIndex].IsOccupied())
    {
        return HOUSING_RESULT_PLOT_NOT_VACANT;
    }

    // Assign the plot
    buyer->PlotIndex = plotIndex;

    _plots[plotIndex].PlotIndex   = plotIndex;
    _plots[plotIndex].OwnerGuid   = playerGuid;

    // Persist the plot assignment to DB
    CharacterDatabasePreparedStatement* stmt = CharacterDatabase.GetPreparedStatement(CHAR_UPD_NEIGHBORHOOD_MEMBER_PLOT);
    stmt->setUInt8(0, plotIndex);
    stmt->setUInt64(1, _guid.GetCounter());
    stmt->setUInt64(2, playerGuid.GetCounter());
    CharacterDatabase.Execute(stmt);

    TC_LOG_DEBUG("housing", "Player {} purchased plot {} in neighborhood '{}'",
        playerGuid.ToString(), plotIndex, _name);

    return HOUSING_RESULT_SUCCESS;
}

void Neighborhood::UpdatePlotHouseInfo(uint8 plotIndex, ObjectGuid houseGuid, ObjectGuid ownerBnetGuid)
{
    if (plotIndex >= MAX_NEIGHBORHOOD_PLOTS || !_plots[plotIndex].IsOccupied())
    {
        return;
    }

    _plots[plotIndex].HouseGuid = houseGuid;
    _plots[plotIndex].OwnerBnetGuid = ownerBnetGuid;
}

void Neighborhood::UpdatePlotHousePosition(ObjectGuid ownerGuid, Optional<Position> const& housePosition)
{
    if (PlotInfo* plot = GetPlotByOwner(ownerGuid))
        plot->HousePosition = housePosition;
}

void Neighborhood::UpdatePlotHouseType(ObjectGuid ownerGuid, uint32 houseType)
{
    if (PlotInfo* plot = GetPlotByOwner(ownerGuid))
        plot->HouseType = houseType;
}

void Neighborhood::UpdatePlotSettingsFlags(ObjectGuid ownerGuid, uint32 settingsFlags)
{
    if (PlotInfo* plot = GetPlotByOwner(ownerGuid))
        plot->HouseSettingsFlags = settingsFlags;
}

HousingResult Neighborhood::MoveHouse(ObjectGuid sourcePlotOwner, uint8 newPlotIndex)
{
    if (newPlotIndex >= MAX_NEIGHBORHOOD_PLOTS)
    {
        return HOUSING_RESULT_PLOT_NOT_FOUND;
    }

    // Check destination is not occupied
    if (_plots[newPlotIndex].IsOccupied())
    {
        return HOUSING_RESULT_PLOT_NOT_VACANT;
    }

    // Find the source plot by owner (still needs linear scan by OwnerGuid)
    PlotInfo* sourcePlot = GetPlotByOwner(sourcePlotOwner);
    if (!sourcePlot)
    {
        return HOUSING_RESULT_PLOT_NOT_FOUND;
    }

    uint8 oldPlotIndex = static_cast<uint8>(sourcePlot - _plots.data());

    // Move plot data: copy to new slot, clear old slot
    _plots[newPlotIndex] = _plots[oldPlotIndex];
    _plots[newPlotIndex].PlotIndex = newPlotIndex;
    _plots[oldPlotIndex] = PlotInfo{};

    // Update the member's plot index as well
    for (Member& member : _members)
    {
        if (member.PlayerGuid == sourcePlotOwner)
        {
            member.PlotIndex = newPlotIndex;
            break;
        }
    }

    // Persist the plot move to DB
    CharacterDatabasePreparedStatement* stmt = CharacterDatabase.GetPreparedStatement(CHAR_UPD_NEIGHBORHOOD_MEMBER_PLOT);
    stmt->setUInt8(0, newPlotIndex);
    stmt->setUInt64(1, _guid.GetCounter());
    stmt->setUInt64(2, sourcePlotOwner.GetCounter());
    CharacterDatabase.Execute(stmt);

    return HOUSING_RESULT_SUCCESS;
}

uint32 Neighborhood::GetOccupiedPlotCount() const
{
    uint32 count = 0;
    for (auto const& plot : _plots)
        if (plot.IsOccupied())
            ++count;
    return count;
}

Neighborhood::PlotInfo* Neighborhood::GetPlotByOwner(ObjectGuid ownerGuid)
{
    for (PlotInfo& plot : _plots)
        if (plot.IsOccupied() && plot.OwnerGuid == ownerGuid)
            return &plot;
    return nullptr;
}

bool Neighborhood::TransferPlot(ObjectGuid oldOwnerGuid, ObjectGuid newOwnerGuid, CharacterDatabaseTransaction trans)
{
    auto findMember = [this](ObjectGuid guid)
    {
        return std::find_if(_members.begin(), _members.end(), [guid](Member const& member) { return member.PlayerGuid == guid; });
    };

    auto oldItr = findMember(oldOwnerGuid);
    if (oldItr == _members.end() || oldItr->PlotIndex == INVALID_PLOT_INDEX)
        return false;

    auto newItr = findMember(newOwnerGuid);
    if (newItr != _members.end() && newItr->PlotIndex != INVALID_PLOT_INDEX)
        return false;

    uint8 const plotIndex = oldItr->PlotIndex;
    ObjectGuid const houseGuid = oldItr->HouseGuid;
    uint32 const joinTime = oldItr->JoinTime;
    bool const oldIsResident = oldItr->Role == NEIGHBORHOOD_ROLE_RESIDENT;

    if (oldIsResident && newItr == _members.end())
    {
        oldItr->PlayerGuid = newOwnerGuid;

        CharacterDatabasePreparedStatement* stmt = CharacterDatabase.GetPreparedStatement(CHAR_UPD_NEIGHBORHOOD_MEMBER_PLAYER);
        stmt->setUInt64(0, newOwnerGuid.GetCounter());
        stmt->setUInt64(1, _guid.GetCounter());
        stmt->setUInt64(2, oldOwnerGuid.GetCounter());
        trans->Append(stmt);
    }
    else
    {
        if (newItr != _members.end())
        {
            newItr->PlotIndex = plotIndex;
            newItr->HouseGuid = houseGuid;

            CharacterDatabasePreparedStatement* stmt = CharacterDatabase.GetPreparedStatement(CHAR_UPD_NEIGHBORHOOD_MEMBER_PLOT);
            stmt->setUInt8(0, plotIndex);
            stmt->setUInt64(1, _guid.GetCounter());
            stmt->setUInt64(2, newOwnerGuid.GetCounter());
            trans->Append(stmt);
        }
        else
        {
            Member newMember;
            newMember.PlayerGuid = newOwnerGuid;
            newMember.HouseGuid = houseGuid;
            newMember.Role = NEIGHBORHOOD_ROLE_RESIDENT;
            newMember.JoinTime = joinTime;
            newMember.PlotIndex = plotIndex;

            CharacterDatabasePreparedStatement* stmt = CharacterDatabase.GetPreparedStatement(CHAR_INS_NEIGHBORHOOD_MEMBER);
            stmt->setUInt64(0, _guid.GetCounter());
            stmt->setUInt64(1, newOwnerGuid.GetCounter());
            stmt->setUInt8(2, newMember.Role);
            stmt->setUInt32(3, newMember.JoinTime);
            stmt->setUInt8(4, newMember.PlotIndex);
            trans->Append(stmt);

            _members.push_back(newMember);
        }

        // push_back may have moved the members
        oldItr = findMember(oldOwnerGuid);
        if (oldIsResident)
        {
            CharacterDatabasePreparedStatement* stmt = CharacterDatabase.GetPreparedStatement(CHAR_DEL_NEIGHBORHOOD_MEMBER);
            stmt->setUInt64(0, _guid.GetCounter());
            stmt->setUInt64(1, oldOwnerGuid.GetCounter());
            trans->Append(stmt);
            _members.erase(oldItr);
        }
        else
        {
            oldItr->PlotIndex = INVALID_PLOT_INDEX;
            oldItr->HouseGuid.Clear();

            CharacterDatabasePreparedStatement* stmt = CharacterDatabase.GetPreparedStatement(CHAR_UPD_NEIGHBORHOOD_MEMBER_PLOT);
            stmt->setUInt8(0, INVALID_PLOT_INDEX);
            stmt->setUInt64(1, _guid.GetCounter());
            stmt->setUInt64(2, oldOwnerGuid.GetCounter());
            trans->Append(stmt);
        }
    }

    _plots[plotIndex].OwnerGuid = newOwnerGuid;

    return true;
}

void Neighborhood::SetPlotAreaTriggerGuid(uint8 plotIndex, ObjectGuid atGuid)
{
    if (plotIndex >= MAX_NEIGHBORHOOD_PLOTS)
        return;

    _plots[plotIndex].PlotGuid = atGuid;
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

void Neighborhood::BroadcastPacket(WorldPacket const* packet, ObjectGuid excludeGuid /*= ObjectGuid::Empty*/) const
{
    for (auto const& member : _members)
    {
        if (member.PlayerGuid == excludeGuid)
            continue;
        if (Player* player = ObjectAccessor::FindPlayer(member.PlayerGuid))
            player->SendDirectMessage(packet);
    }
}

void Neighborhood::BuildRosterResponse(WorldPackets::Neighborhood::NeighborhoodGetRosterResponse& response) const
{
    response.Result = static_cast<uint8>(HOUSING_RESULT_SUCCESS);
    response.GroupNeighborhoodGuid = GetGuid();
    response.GroupOwnerGuid = GetClientOwnerGuid();
    response.NeighborhoodName = GetName();
    response.Members.reserve(_members.size());
    for (Member const& member : _members)
    {
        WorldPackets::Neighborhood::NeighborhoodGetRosterResponse::RosterMemberData& data = response.Members.emplace_back();
        data.PlayerGuid = member.PlayerGuid;
        data.PlotIndex = member.PlotIndex;
        data.JoinTime = member.JoinTime;
        data.ResidentType = member.Role;
        data.IsOnline = ObjectAccessor::FindPlayer(member.PlayerGuid) != nullptr;
        // PlotInfo mirrors character_housing, so offline residents' houses are listed too.
        if (member.PlotIndex != INVALID_PLOT_INDEX)
        {
            if (PlotInfo const* plotInfo = GetPlotInfo(member.PlotIndex))
            {
                data.HouseGuid = plotInfo->HouseGuid;
                data.HouseLevel = plotInfo->HouseLevel;
                data.HouseSettingFlags = plotInfo->HouseSettingsFlags;
            }
        }
    }
}

void Neighborhood::BroadcastRoster(ObjectGuid excludeGuid /*= ObjectGuid::Empty*/) const
{
    WorldPackets::Neighborhood::NeighborhoodGetRosterResponse response;
    BuildRosterResponse(response);
    BroadcastPacket(response.Write(), excludeGuid);
}

void Neighborhood::BroadcastMemberStatus(ObjectGuid playerGuid, bool isOnline) const
{
    Member const* member = GetMember(playerGuid);
    if (!member)
        return;

    WorldPackets::Neighborhood::NeighborhoodRosterResidentUpdate update;
    update.Residents.push_back({ playerGuid, member->Role, isOnline });
    BroadcastPacket(update.Write());
}

void Neighborhood::BroadcastMemberStatus(ObjectGuid playerGuid) const
{
    BroadcastMemberStatus(playerGuid, ObjectAccessor::FindPlayer(playerGuid) != nullptr);
}

void Neighborhood::RebuildMirrorDataFor(Player* player) const
{
    if (!player || !player->GetSession())
        return;

    // FNeighborhoodMirrorData_C belongs on the Housing/4 entity, not the BNetAccount entity.
    HousingNeighborhoodMirrorEntity& mirrorEntity = player->GetSession()->GetHousingNeighborhoodMirrorEntity();

    // Name + Owner
    mirrorEntity.SetName(_name);
    mirrorEntity.SetOwnerGUID(GetClientOwnerGuid());

    // Houses must be dense: Houses[i] = plot i, or the client misidentifies occupied plots.
    mirrorEntity.ClearHouses();
    for (auto const& plot : _plots)
    {
        if (plot.IsOccupied() && !plot.HouseGuid.IsEmpty())
            mirrorEntity.AddHouse(plot.HouseGuid, plot.OwnerGuid);
        else
            mirrorEntity.AddHouse(ObjectGuid::Empty, ObjectGuid::Empty);
    }

    // Managers
    mirrorEntity.ClearManagers();
    for (auto const& m : _members)
    {
        if (m.Role == NEIGHBORHOOD_ROLE_MANAGER || m.Role == NEIGHBORHOOD_ROLE_OWNER)
        {
            ObjectGuid bnetGuid;
            if (Player* mgr = ObjectAccessor::FindPlayer(m.PlayerGuid))
                bnetGuid = mgr->GetSession()->GetBattlenetAccountGUID();
            mirrorEntity.AddManager(bnetGuid, m.PlayerGuid);
        }
    }
}

void Neighborhood::RefreshMirrorDataForPlayer(Player* player) const
{
    if (!player || !player->GetSession())
        return;

    RebuildMirrorDataFor(player);

    // Setters only flip dirty bits; re-send wholesale so the client refreshes its map icons.
    player->GetSession()->GetHousingNeighborhoodMirrorEntity().SendCreateToPlayer(player);
}

void Neighborhood::RefreshMirrorDataForOnlineMembers() const
{
    // Refreshes every current viewer of the map instance, member or not.
    uint32 const worldMapId = sHousingMgr.GetWorldMapIdByNeighborhoodMapId(GetNeighborhoodMapID());
    if (!worldMapId)
        return;

    if (Map* map = sMapMgr->FindMap(worldMapId, static_cast<uint32>(GetGuid().GetCounter())))
        for (MapReference const& ref : map->GetPlayers())
            if (Player* player = ref.GetSource())
                RefreshMirrorDataForPlayer(player);
}

bool Neighborhood::ReservePlot(ObjectGuid playerGuid, uint8 plotIndex)
{
    if (plotIndex >= MAX_NEIGHBORHOOD_PLOTS)
        return false;

    if (_plots[plotIndex].IsOccupied())
        return false;

    // Sweep expired reservations first so a stale one never blocks a new player
    uint32 now = static_cast<uint32>(GameTime::GetGameTime());
    for (auto it = _plotReservations.begin(); it != _plotReservations.end(); )
    {
        if (now >= it->second.ReserveTime + RESERVATION_EXPIRY_SECONDS)
        {
            it = _plotReservations.erase(it);
        }
        else
        {
            ++it;
        }
    }

    for (auto const& [guid, res] : _plotReservations)
    {
        if (res.PlotIndex == plotIndex && guid != playerGuid)
            return false;
    }

    PlotReservation& reservation = _plotReservations[playerGuid];
    reservation.PlotIndex = plotIndex;
    reservation.ReserveTime = now;

    return true;
}

bool Neighborhood::ClearReservation(ObjectGuid playerGuid)
{
    auto it = _plotReservations.find(playerGuid);
    if (it == _plotReservations.end())
        return false;

    _plotReservations.erase(it);
    return true;
}

bool Neighborhood::HasReservation(ObjectGuid playerGuid) const
{
    return _plotReservations.find(playerGuid) != _plotReservations.end();
}

uint8 Neighborhood::GetReservedPlot(ObjectGuid playerGuid) const
{
    auto it = _plotReservations.find(playerGuid);
    if (it != _plotReservations.end())
        return it->second.PlotIndex;
    return INVALID_PLOT_INDEX;
}

ObjectGuid Neighborhood::GetPlotReserverOther(uint8 plotIndex, ObjectGuid viewerGuid)
{
    if (plotIndex >= MAX_NEIGHBORHOOD_PLOTS)
        return ObjectGuid::Empty;

    uint32 now = static_cast<uint32>(GameTime::GetGameTime());

    // Sweep expired reservations so stale holds don't paint plots as reserved.
    for (auto it = _plotReservations.begin(); it != _plotReservations.end(); )
    {
        if (now >= it->second.ReserveTime + RESERVATION_EXPIRY_SECONDS)
            it = _plotReservations.erase(it);
        else
            ++it;
    }

    for (auto const& [guid, res] : _plotReservations)
        if (res.PlotIndex == plotIndex && guid != viewerGuid)
            return guid;

    return ObjectGuid::Empty;
}
