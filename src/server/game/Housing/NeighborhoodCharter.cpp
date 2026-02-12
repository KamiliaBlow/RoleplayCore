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

#include "NeighborhoodCharter.h"
#include "DatabaseEnv.h"
#include "GameTime.h"
#include "Log.h"
#include <algorithm>

NeighborhoodCharter::NeighborhoodCharter(uint64 id, ObjectGuid creatorGuid)
    : _id(id), _creatorGuid(creatorGuid), _createTime(static_cast<uint32>(GameTime::GetGameTime()))
{
}

bool NeighborhoodCharter::LoadFromDB(PreparedQueryResult charter, PreparedQueryResult signatures)
{
    if (!charter)
        return false;

    Field* fields = charter->Fetch();

    //          0    1            2       3                4             5        6
    // SELECT id, creatorGuid, name, neighborhoodMapID, factionFlags, isGuild, createTime
    //        FROM neighborhood_charters WHERE id = ?

    _id                 = fields[0].GetUInt64();
    _creatorGuid        = ObjectGuid::Create<HighGuid::Player>(fields[1].GetUInt64());
    _name               = fields[2].GetString();
    _neighborhoodMapID  = fields[3].GetUInt32();
    _factionFlags       = fields[4].GetUInt32();
    _isGuild            = fields[5].GetBool();
    _createTime         = fields[6].GetUInt32();

    TC_LOG_DEBUG("housing", "NeighborhoodCharter::LoadFromDB: Loaded charter {} '{}' by {}",
        _id, _name, _creatorGuid.ToString());

    // Load signatures
    _signatures.clear();

    if (signatures)
    {
        do
        {
            Field* sigFields = signatures->Fetch();

            //          0
            // SELECT signerGuid FROM neighborhood_charter_signatures WHERE charterId = ?

            ObjectGuid signerGuid = ObjectGuid::Create<HighGuid::Player>(sigFields[0].GetUInt64());
            _signatures.push_back(signerGuid);
        } while (signatures->NextRow());
    }

    TC_LOG_DEBUG("housing", "NeighborhoodCharter::LoadFromDB: Loaded {} signatures for charter {}",
        _signatures.size(), _id);

    return true;
}

void NeighborhoodCharter::SaveToDB(CharacterDatabaseTransaction trans)
{
    // Replace the charter row
    CharacterDatabasePreparedStatement* stmt = CharacterDatabase.GetPreparedStatement(CHAR_REP_NEIGHBORHOOD_CHARTER);
    uint8 index = 0;
    stmt->setUInt64(index++, _id);
    stmt->setUInt64(index++, _creatorGuid.GetCounter());
    stmt->setString(index++, _name);
    stmt->setUInt32(index++, _neighborhoodMapID);
    stmt->setUInt32(index++, _factionFlags);
    stmt->setBool(index++, _isGuild);
    stmt->setUInt32(index++, _createTime);
    trans->Append(stmt);

    // Delete all existing signatures and re-insert
    stmt = CharacterDatabase.GetPreparedStatement(CHAR_DEL_NEIGHBORHOOD_CHARTER_SIGNATURES);
    stmt->setUInt64(0, _id);
    trans->Append(stmt);

    for (ObjectGuid const& signerGuid : _signatures)
    {
        stmt = CharacterDatabase.GetPreparedStatement(CHAR_INS_NEIGHBORHOOD_CHARTER_SIGNATURE);
        index = 0;
        stmt->setUInt64(index++, _id);
        stmt->setUInt64(index++, signerGuid.GetCounter());
        stmt->setUInt32(index++, _createTime);
        trans->Append(stmt);
    }

    TC_LOG_DEBUG("housing", "NeighborhoodCharter::SaveToDB: Saved charter {} '{}' with {} signatures",
        _id, _name, _signatures.size());
}

/*static*/ void NeighborhoodCharter::DeleteFromDB(uint64 id, CharacterDatabaseTransaction trans)
{
    // Delete signatures first (foreign key dependency)
    CharacterDatabasePreparedStatement* stmt = CharacterDatabase.GetPreparedStatement(CHAR_DEL_NEIGHBORHOOD_CHARTER_SIGNATURES);
    stmt->setUInt64(0, id);
    trans->Append(stmt);

    // Delete the charter
    stmt = CharacterDatabase.GetPreparedStatement(CHAR_DEL_NEIGHBORHOOD_CHARTER);
    stmt->setUInt64(0, id);
    trans->Append(stmt);

    TC_LOG_DEBUG("housing", "NeighborhoodCharter::DeleteFromDB: Deleted charter {}", id);
}

bool NeighborhoodCharter::AddSignature(ObjectGuid signerGuid)
{
    // Cannot sign your own charter
    if (signerGuid == _creatorGuid)
    {
        TC_LOG_DEBUG("housing", "NeighborhoodCharter::AddSignature: Player {} cannot sign their own charter {}",
            signerGuid.ToString(), _id);
        return false;
    }

    // Cannot sign twice
    if (HasSigned(signerGuid))
    {
        TC_LOG_DEBUG("housing", "NeighborhoodCharter::AddSignature: Player {} has already signed charter {}",
            signerGuid.ToString(), _id);
        return false;
    }

    _signatures.push_back(signerGuid);

    // Persist the new signature immediately
    CharacterDatabaseTransaction trans = CharacterDatabase.BeginTransaction();
    CharacterDatabasePreparedStatement* stmt = CharacterDatabase.GetPreparedStatement(CHAR_INS_NEIGHBORHOOD_CHARTER_SIGNATURE);
    uint8 index = 0;
    stmt->setUInt64(index++, _id);
    stmt->setUInt64(index++, signerGuid.GetCounter());
    stmt->setUInt32(index++, static_cast<uint32>(GameTime::GetGameTime()));
    trans->Append(stmt);
    CharacterDatabase.CommitTransaction(trans);

    TC_LOG_DEBUG("housing", "NeighborhoodCharter::AddSignature: Player {} signed charter {} ({}/{} signatures)",
        signerGuid.ToString(), _id, _signatures.size(), MIN_CHARTER_SIGNATURES);

    return true;
}

bool NeighborhoodCharter::HasSigned(ObjectGuid signerGuid) const
{
    return std::find(_signatures.begin(), _signatures.end(), signerGuid) != _signatures.end();
}
