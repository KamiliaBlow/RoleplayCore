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

#ifndef TRINITYCORE_NEIGHBORHOOD_MGR_H
#define TRINITYCORE_NEIGHBORHOOD_MGR_H

#include "Define.h"
#include "ObjectGuid.h"
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

class Neighborhood;

class TC_GAME_API NeighborhoodMgr
{
public:
    static NeighborhoodMgr& Instance();

    NeighborhoodMgr(NeighborhoodMgr const&) = delete;
    NeighborhoodMgr(NeighborhoodMgr&&) = delete;
    NeighborhoodMgr& operator=(NeighborhoodMgr const&) = delete;
    NeighborhoodMgr& operator=(NeighborhoodMgr&&) = delete;

    void Initialize();
    void LoadFromDB();

    // Neighborhood lifecycle
    Neighborhood* CreateNeighborhood(ObjectGuid ownerGuid, std::string const& name, uint32 neighborhoodMapID, int32 factionRestriction, bool isPublic = false);
    Neighborhood* CreateGuildNeighborhood(ObjectGuid ownerGuid, std::string const& name, uint32 neighborhoodMapID, uint32 factionID);
    void DeleteNeighborhood(ObjectGuid neighborhoodGuid);
    Neighborhood* GetNeighborhood(ObjectGuid neighborhoodGuid);
    Neighborhood const* GetNeighborhood(ObjectGuid neighborhoodGuid) const;

    // Queries
    Neighborhood* GetNeighborhoodByOwner(ObjectGuid ownerGuid);
    std::vector<Neighborhood*> GetPublicNeighborhoods() const;
    std::vector<Neighborhood*> GetNeighborhoodsForPlayer(ObjectGuid playerGuid) const;
    std::string GetNeighborhoodName(ObjectGuid neighborhoodGuid) const;
    Neighborhood* FindNeighborhoodWithPendingInvite(ObjectGuid playerGuid);

    // Tutorial support
    Neighborhood* FindOrCreateTutorialNeighborhood(ObjectGuid playerGuid, uint32 teamId);

    // Expansion
    void CheckAndExpandNeighborhoods();

    // Charter support
    ObjectGuid GenerateNeighborhoodGuid();

    // Startup guarantee
    void EnsurePublicNeighborhoods();

private:
    NeighborhoodMgr() = default;

    std::unordered_map<ObjectGuid, std::unique_ptr<Neighborhood>> _neighborhoods;
    std::unordered_map<ObjectGuid, ObjectGuid> _ownerToNeighborhood; // owner guid -> neighborhood guid
    uint64 _nextGuid = 1;
};

#define sNeighborhoodMgr NeighborhoodMgr::Instance()

#endif // TRINITYCORE_NEIGHBORHOOD_MGR_H
