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
class Player;

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
    void Update(uint32 diff);

    // Neighborhood lifecycle
    Neighborhood* CreateNeighborhood(ObjectGuid ownerGuid, std::string const& name, uint32 neighborhoodMapID, int32 factionRestriction, bool isPublic = false, uint32 guildId = 0);
    Neighborhood* CreateGuildNeighborhood(ObjectGuid ownerGuid, std::string const& name, uint32 neighborhoodMapID, uint32 factionID, uint32 guildId);
    void DeleteNeighborhood(ObjectGuid neighborhoodGuid);
    Neighborhood* GetNeighborhood(ObjectGuid neighborhoodGuid);
    Neighborhood const* GetNeighborhood(ObjectGuid neighborhoodGuid) const;

    // Accepts client-format GUIDs (bulletin board GO); falls back to the player's current map neighborhood.
    Neighborhood* ResolveNeighborhood(ObjectGuid guid, Player* player);

    // Queries
    Neighborhood* GetNeighborhoodByOwner(ObjectGuid ownerGuid);
    Neighborhood* GetNeighborhoodByGuildId(uint32 guildId);
    std::vector<Neighborhood*> GetAllNeighborhoods() const;
    std::vector<Neighborhood*> GetPublicNeighborhoods() const;
    std::vector<Neighborhood*> GetNeighborhoodsForPlayer(ObjectGuid playerGuid) const;
    std::vector<Neighborhood*> GetNeighborhoodsByBnetAccount(ObjectGuid bnetAccountGuid) const;
    std::string GetNeighborhoodName(ObjectGuid neighborhoodGuid) const;
    Neighborhood* FindNeighborhoodWithPendingInvite(ObjectGuid playerGuid);

    // Find or create a public neighborhood for a faction (no membership changes)
    Neighborhood* FindOrCreatePublicNeighborhood(uint32 teamId);

    // Find a public neighborhood on the given map (for visitors, no membership change)
    Neighborhood* FindPublicNeighborhoodForMap(uint32 neighborhoodMapId) const;

    // Resolve by the counter that is persisted in the DB (neighborhoods.guid and every FK to it).
    Neighborhood* GetNeighborhoodByCounter(uint64 counter) const;
    Neighborhood* FindNeighborhoodByName(std::string_view name);
    /// Reassigns a charter neighborhood to another character (one-neighborhood-per-character
    /// invariant enforced; guild neighborhoods are rejected). False when the transfer is invalid.
    bool SetNeighborhoodOwner(Neighborhood& neighborhood, ObjectGuid newOwnerGuid);

    // Expansion
    void CheckAndExpandNeighborhoods();

    // neighborhoodMapID becomes arg1 of the GUID: the client's NeighborhoodMap.db2 record ID (not a realm id).
    ObjectGuid GenerateNeighborhoodGuid(uint32 neighborhoodMapID, bool hasCustomName);

    // arg2 of the GUID: 0 = client formats a system NeighborhoodNameGen name, else the sent name is shown.
    static ObjectGuid MakeNeighborhoodGuid(uint32 neighborhoodMapID, bool hasCustomName, uint64 counter);

    // Startup guarantee
    void VerifyNeighborhoodFactions();
    void EnsurePublicNeighborhoods();

    // Move members stuck in wrong-faction public neighborhoods (legacy data fix)
    void MigrateWrongFactionResidents();

    // Regenerate names for public neighborhoods using base DB2 entry IDs
    void RegenerateNeighborhoodNames();

private:
    NeighborhoodMgr() = default;

    std::unordered_map<ObjectGuid, std::unique_ptr<Neighborhood>> _neighborhoods;
    // Persisted tables store only GetCounter(), so DB-driven lookups resolve through this map.
    std::unordered_map<uint64, Neighborhood*> _neighborhoodsByCounter;
    std::unordered_map<ObjectGuid, ObjectGuid> _ownerToNeighborhood; // owner guid -> neighborhood guid
    uint64 _nextGuid = 1;
    uint32 _expansionCheckTimer = 0;
};

#define sNeighborhoodMgr NeighborhoodMgr::Instance()

#endif // TRINITYCORE_NEIGHBORHOOD_MGR_H
