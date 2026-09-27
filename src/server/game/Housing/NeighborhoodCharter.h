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

#ifndef TRINITYCORE_NEIGHBORHOOD_CHARTER_H
#define TRINITYCORE_NEIGHBORHOOD_CHARTER_H

#include "Define.h"
#include "DatabaseEnvFwd.h"
#include "HousingDefines.h"
#include "ObjectGuid.h"
#include <string>
#include <vector>

class TC_GAME_API NeighborhoodCharter
{
public:
    explicit NeighborhoodCharter(uint64 id, ObjectGuid creatorGuid);

    uint64 GetId() const { return _id; }
    ObjectGuid GetCreatorGuid() const { return _creatorGuid; }
    std::string const& GetName() const { return _name; }
    uint32 GetNeighborhoodMapID() const { return _neighborhoodMapID; }
    uint32 GetFactionFlags() const { return _factionFlags; }
    bool IsGuildCharter() const { return _isGuild; }

    void SetName(std::string const& name) { _name = name; }
    void SetNeighborhoodMapID(uint32 mapId) { _neighborhoodMapID = mapId; }
    void SetFactionFlags(uint32 flags) { _factionFlags = flags; }
    void SetIsGuild(bool isGuild) { _isGuild = isGuild; }

    bool AddSignature(ObjectGuid signerGuid);
    bool HasSigned(ObjectGuid signerGuid) const;
    uint32 GetSignatureCount() const { return static_cast<uint32>(_signatures.size()); }
    bool HasEnoughSignatures() const { return GetSignatureCount() >= MIN_CHARTER_SIGNATURES; }
    std::vector<ObjectGuid> const& GetSignatures() const { return _signatures; }

    void SaveToDB(CharacterDatabaseTransaction trans);
    bool LoadFromDB(PreparedQueryResult charter, PreparedQueryResult signatures);
    static void DeleteFromDB(uint64 id, CharacterDatabaseTransaction trans);

private:
    uint64 _id = 0;
    ObjectGuid _creatorGuid;
    std::string _name;
    uint32 _neighborhoodMapID = 0;
    uint32 _factionFlags = 0;
    bool _isGuild = false;
    uint32 _createTime = 0;
    std::vector<ObjectGuid> _signatures;
};

#endif // TRINITYCORE_NEIGHBORHOOD_CHARTER_H
