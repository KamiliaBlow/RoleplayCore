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

    // Resolve a neighborhood GUID that may be in client format (GO GUID of bulletin board)
    // Falls back to the player's current HousingMap neighborhood when direct lookup fails
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

    // Expansion
    void CheckAndExpandNeighborhoods();

    // Charter support.
    // neighborhoodMapID becomes arg1 of the GUID: the 12.0.7 client slices that 16-bit field straight out of
    // the high qword and uses it as the record ID into NeighborhoodMap.db2 (both
    // C_Housing.DoesFactionMatchNeighborhood @ RVA 0xF7C1B0 and C_Housing.GetUIMapIDForNeighborhood @ 0xF80C90
    // do `shr rax,0x20; movzx edx,ax` and look up the store at data RVA 0x486F5C0). It is NOT a realm id.
    ObjectGuid GenerateNeighborhoodGuid(uint32 neighborhoodMapID);

    // Startup guarantee
    void VerifyNeighborhoodFactions();
    void EnsurePublicNeighborhoods();

    // Migrate members in wrong-faction public neighborhoods (legacy data fix)
    void MigrateWrongFactionResidents();

    // Regenerate names for public neighborhoods using base DB2 entry IDs
    void RegenerateNeighborhoodNames();

private:
    NeighborhoodMgr() = default;

    std::unordered_map<ObjectGuid, std::unique_ptr<Neighborhood>> _neighborhoods;
    // Counter -> neighborhood. _neighborhoods is keyed by the FULL ObjectGuid, so now that arg1 varies per
    // neighborhood map, nothing may rebuild a lookup GUID from a bare persisted counter - the persisted tables
    // store only GetCounter(). Every such site resolves through GetNeighborhoodByCounter instead.
    std::unordered_map<uint64, Neighborhood*> _neighborhoodsByCounter;
    std::unordered_map<ObjectGuid, ObjectGuid> _ownerToNeighborhood; // owner guid -> neighborhood guid
    uint64 _nextGuid = 1;
    uint32 _expansionCheckTimer = 0;
};

#define sNeighborhoodMgr NeighborhoodMgr::Instance()

#endif // TRINITYCORE_NEIGHBORHOOD_MGR_H
