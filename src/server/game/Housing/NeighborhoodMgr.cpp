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

#include "NeighborhoodMgr.h"
#include "DatabaseEnv.h"
#include "GameTime.h"
#include "HousingDefines.h"
#include "HousingMgr.h"
#include "Log.h"
#include "Neighborhood.h"
#include "SharedDefines.h"
#include "Timer.h"
#include "World.h"

NeighborhoodMgr& NeighborhoodMgr::Instance()
{
    static NeighborhoodMgr instance;
    return instance;
}

void NeighborhoodMgr::Initialize()
{
    TC_LOG_INFO("server.loading", "Initializing NeighborhoodMgr...");
    LoadFromDB();
    EnsurePublicNeighborhoods();
}

void NeighborhoodMgr::LoadFromDB()
{
    uint32 oldMSTime = getMSTime();

    _neighborhoods.clear();
    _ownerToNeighborhood.clear();

    //                                                     0     1       2                3          4                    5         6
    QueryResult result = CharacterDatabase.Query("SELECT guid, name, neighborhoodMapID, ownerGuid, factionRestriction, isPublic, createTime FROM neighborhoods ORDER BY guid ASC");

    if (!result)
    {
        TC_LOG_INFO("server.loading", ">> Loaded 0 neighborhoods. DB table `neighborhoods` is empty.");
        return;
    }

    uint32 count = 0;

    do
    {
        Field* fields = result->Fetch();

        uint64 guidLow = fields[0].GetUInt64();

        // Track highest guid for generation
        if (guidLow >= _nextGuid)
            _nextGuid = guidLow + 1;

        ObjectGuid neighborhoodGuid = ObjectGuid::Create<HighGuid::Housing>(/*subType*/ 4, /*arg1*/ 0, /*arg2*/ 0, guidLow);

        auto neighborhood = std::make_unique<Neighborhood>(neighborhoodGuid);

        // Load members for this neighborhood
        CharacterDatabasePreparedStatement* memberStmt = CharacterDatabase.GetPreparedStatement(CHAR_SEL_NEIGHBORHOOD_MEMBERS);
        memberStmt->setUInt64(0, guidLow);
        PreparedQueryResult memberResult = CharacterDatabase.Query(memberStmt);

        // Load invites for this neighborhood
        CharacterDatabasePreparedStatement* inviteStmt = CharacterDatabase.GetPreparedStatement(CHAR_SEL_NEIGHBORHOOD_INVITES);
        inviteStmt->setUInt64(0, guidLow);
        PreparedQueryResult inviteResult = CharacterDatabase.Query(inviteStmt);

        // Wrap the neighborhood row as a PreparedQueryResult by passing the raw result
        // LoadFromDB expects PreparedQueryResult for the first param but we have the raw fields;
        // so we call LoadFromDB with the data directly already parsed from fields above.
        // Since LoadFromDB needs the PreparedQueryResult format, we use a query per neighborhood.
        CharacterDatabasePreparedStatement* neighborhoodStmt = CharacterDatabase.GetPreparedStatement(CHAR_SEL_NEIGHBORHOOD);
        neighborhoodStmt->setUInt64(0, guidLow);
        PreparedQueryResult neighborhoodResult = CharacterDatabase.Query(neighborhoodStmt);

        if (!neighborhood->LoadFromDB(neighborhoodResult, memberResult, inviteResult))
        {
            TC_LOG_ERROR("housing", "NeighborhoodMgr::LoadFromDB: Failed to load neighborhood guid {}. Skipping.", guidLow);
            continue;
        }

        ObjectGuid ownerGuid = neighborhood->GetOwnerGuid();
        _ownerToNeighborhood[ownerGuid] = neighborhoodGuid;
        _neighborhoods[neighborhoodGuid] = std::move(neighborhood);
        ++count;

    } while (result->NextRow());

    TC_LOG_INFO("server.loading", ">> Loaded {} neighborhoods in {} ms", count, GetMSTimeDiffToNow(oldMSTime));
}

Neighborhood* NeighborhoodMgr::CreateNeighborhood(ObjectGuid ownerGuid, std::string const& name, uint32 neighborhoodMapID, int32 factionRestriction, bool isPublic /*= false*/)
{
    // Check if owner already has a neighborhood
    if (_ownerToNeighborhood.find(ownerGuid) != _ownerToNeighborhood.end())
    {
        TC_LOG_DEBUG("housing", "NeighborhoodMgr::CreateNeighborhood: Owner {} already has a neighborhood",
            ownerGuid.ToString());
        return nullptr;
    }

    ObjectGuid neighborhoodGuid = GenerateNeighborhoodGuid();

    auto neighborhood = std::make_unique<Neighborhood>(neighborhoodGuid);

    // Set up initial state via the member data structures
    // We need to persist and then reload to go through LoadFromDB, or set internal state directly.
    // For creation, we persist first and then load.

    uint32 createTime = static_cast<uint32>(GameTime::GetGameTime());

    CharacterDatabaseTransaction trans = CharacterDatabase.BeginTransaction();

    // Insert the neighborhood row
    CharacterDatabasePreparedStatement* stmt = CharacterDatabase.GetPreparedStatement(CHAR_INS_NEIGHBORHOOD);
    uint8 index = 0;
    stmt->setUInt64(index++, neighborhoodGuid.GetCounter());
    stmt->setString(index++, name);
    stmt->setUInt32(index++, neighborhoodMapID);
    stmt->setUInt64(index++, ownerGuid.GetCounter());
    stmt->setInt32(index++, factionRestriction);
    stmt->setBool(index++, isPublic);
    stmt->setUInt32(index++, createTime);
    trans->Append(stmt);

    // Insert the owner as a member with OWNER role
    stmt = CharacterDatabase.GetPreparedStatement(CHAR_INS_NEIGHBORHOOD_MEMBER);
    index = 0;
    stmt->setUInt64(index++, neighborhoodGuid.GetCounter());
    stmt->setUInt64(index++, ownerGuid.GetCounter());
    stmt->setUInt8(index++, NEIGHBORHOOD_ROLE_OWNER);
    stmt->setUInt32(index++, createTime);
    stmt->setUInt8(index++, INVALID_PLOT_INDEX);
    trans->Append(stmt);

    CharacterDatabase.DirectCommitTransaction(trans);

    // Now load from DB to populate all internal structures properly
    CharacterDatabasePreparedStatement* selStmt = CharacterDatabase.GetPreparedStatement(CHAR_SEL_NEIGHBORHOOD);
    selStmt->setUInt64(0, neighborhoodGuid.GetCounter());
    PreparedQueryResult neighborhoodResult = CharacterDatabase.Query(selStmt);

    selStmt = CharacterDatabase.GetPreparedStatement(CHAR_SEL_NEIGHBORHOOD_MEMBERS);
    selStmt->setUInt64(0, neighborhoodGuid.GetCounter());
    PreparedQueryResult memberResult = CharacterDatabase.Query(selStmt);

    selStmt = CharacterDatabase.GetPreparedStatement(CHAR_SEL_NEIGHBORHOOD_INVITES);
    selStmt->setUInt64(0, neighborhoodGuid.GetCounter());
    PreparedQueryResult inviteResult = CharacterDatabase.Query(selStmt);

    if (!neighborhood->LoadFromDB(neighborhoodResult, memberResult, inviteResult))
    {
        TC_LOG_ERROR("housing", "NeighborhoodMgr::CreateNeighborhood: Failed to load newly created neighborhood '{}'", name);
        return nullptr;
    }

    Neighborhood* result = neighborhood.get();
    _ownerToNeighborhood[ownerGuid] = neighborhoodGuid;
    _neighborhoods[neighborhoodGuid] = std::move(neighborhood);

    TC_LOG_DEBUG("housing", "NeighborhoodMgr::CreateNeighborhood: Created neighborhood '{}' (guid: {}) for owner {}",
        name, neighborhoodGuid.ToString(), ownerGuid.ToString());

    return result;
}

Neighborhood* NeighborhoodMgr::CreateGuildNeighborhood(ObjectGuid ownerGuid, std::string const& name, uint32 neighborhoodMapID, uint32 factionID)
{
    int32 factionRestriction = NEIGHBORHOOD_FACTION_NONE;
    if (factionID == HORDE)
        factionRestriction = NEIGHBORHOOD_FACTION_HORDE;
    else if (factionID == ALLIANCE)
        factionRestriction = NEIGHBORHOOD_FACTION_ALLIANCE;

    return CreateNeighborhood(ownerGuid, name, neighborhoodMapID, factionRestriction);
}

void NeighborhoodMgr::DeleteNeighborhood(ObjectGuid neighborhoodGuid)
{
    auto it = _neighborhoods.find(neighborhoodGuid);
    if (it == _neighborhoods.end())
    {
        TC_LOG_DEBUG("housing", "NeighborhoodMgr::DeleteNeighborhood: Neighborhood {} not found",
            neighborhoodGuid.ToString());
        return;
    }

    ObjectGuid ownerGuid = it->second->GetOwnerGuid();

    // Delete from DB
    CharacterDatabaseTransaction trans = CharacterDatabase.BeginTransaction();
    Neighborhood::DeleteFromDB(neighborhoodGuid.GetCounter(), trans);
    CharacterDatabase.CommitTransaction(trans);

    // Remove from maps
    _ownerToNeighborhood.erase(ownerGuid);
    _neighborhoods.erase(it);

    TC_LOG_DEBUG("housing", "NeighborhoodMgr::DeleteNeighborhood: Deleted neighborhood {}",
        neighborhoodGuid.ToString());
}

Neighborhood* NeighborhoodMgr::GetNeighborhood(ObjectGuid neighborhoodGuid)
{
    auto it = _neighborhoods.find(neighborhoodGuid);
    if (it != _neighborhoods.end())
        return it->second.get();

    return nullptr;
}

Neighborhood const* NeighborhoodMgr::GetNeighborhood(ObjectGuid neighborhoodGuid) const
{
    auto it = _neighborhoods.find(neighborhoodGuid);
    if (it != _neighborhoods.end())
        return it->second.get();

    return nullptr;
}

Neighborhood* NeighborhoodMgr::GetNeighborhoodByOwner(ObjectGuid ownerGuid)
{
    auto it = _ownerToNeighborhood.find(ownerGuid);
    if (it != _ownerToNeighborhood.end())
        return GetNeighborhood(it->second);

    return nullptr;
}

std::vector<Neighborhood*> NeighborhoodMgr::GetPublicNeighborhoods() const
{
    std::vector<Neighborhood*> result;
    for (auto const& [guid, neighborhood] : _neighborhoods)
    {
        if (neighborhood->IsPublic())
            result.push_back(neighborhood.get());
    }
    return result;
}

std::vector<Neighborhood*> NeighborhoodMgr::GetNeighborhoodsForPlayer(ObjectGuid playerGuid) const
{
    std::vector<Neighborhood*> result;
    for (auto const& [guid, neighborhood] : _neighborhoods)
    {
        if (neighborhood->IsMember(playerGuid))
            result.push_back(neighborhood.get());
    }
    return result;
}

std::string NeighborhoodMgr::GetNeighborhoodName(ObjectGuid neighborhoodGuid) const
{
    Neighborhood const* neighborhood = GetNeighborhood(neighborhoodGuid);
    if (neighborhood)
        return neighborhood->GetName();

    return "";
}

Neighborhood* NeighborhoodMgr::FindNeighborhoodWithPendingInvite(ObjectGuid playerGuid)
{
    for (auto const& [guid, neighborhood] : _neighborhoods)
    {
        if (neighborhood->HasPendingInvite(playerGuid))
            return neighborhood.get();
    }
    return nullptr;
}

Neighborhood* NeighborhoodMgr::FindOrCreateTutorialNeighborhood(ObjectGuid playerGuid, uint32 teamId)
{
    // Check if player already belongs to a neighborhood
    std::vector<Neighborhood*> existing = GetNeighborhoodsForPlayer(playerGuid);
    if (!existing.empty())
    {
        TC_LOG_DEBUG("housing", "FindOrCreateTutorialNeighborhood: Player {} already in neighborhood '{}'",
            playerGuid.ToString(), existing[0]->GetName());
        return existing[0];
    }

    // Determine the correct NeighborhoodMapID for the player's faction
    // NeighborhoodMap flags: bit 0 = Alliance, bit 1 = Horde, bit 2 = CanSystemGenerate
    uint32 targetMapId = 0;
    int32 factionRestriction = NEIGHBORHOOD_FACTION_NONE;

    for (auto const& [id, data] : sHousingMgr.GetAllNeighborhoodMapData())
    {
        int32 flags = data.UiMapID; // UiMapID stores DB2 FactionRestriction/Flags field
        bool isAlliance = (flags & 0x1) != 0;
        bool isHorde = (flags & 0x2) != 0;
        bool canSystemGenerate = (flags & 0x4) != 0;

        if (!canSystemGenerate)
            continue;

        if (teamId == ALLIANCE && isAlliance)
        {
            targetMapId = id;
            factionRestriction = NEIGHBORHOOD_FACTION_ALLIANCE;
            break;
        }
        else if (teamId == HORDE && isHorde)
        {
            targetMapId = id;
            factionRestriction = NEIGHBORHOOD_FACTION_HORDE;
            break;
        }
    }

    if (targetMapId == 0)
    {
        TC_LOG_ERROR("housing", "FindOrCreateTutorialNeighborhood: No system-generatable NeighborhoodMap found for team {}",
            teamId);
        return nullptr;
    }

    // Look for an existing public neighborhood on the target map with available plots
    for (auto const& [guid, neighborhood] : _neighborhoods)
    {
        if (neighborhood->GetNeighborhoodMapID() == targetMapId && neighborhood->IsPublic())
        {
            // Ensure the player is a member (they may not be if they didn't create this neighborhood)
            neighborhood->AddResident(playerGuid);

            TC_LOG_DEBUG("housing", "FindOrCreateTutorialNeighborhood: Found existing public neighborhood '{}' for player {}",
                neighborhood->GetName(), playerGuid.ToString());
            return neighborhood.get();
        }
    }

    // Create a new public, system-generated neighborhood with the player as owner
    std::string name = sHousingMgr.GenerateNeighborhoodName(targetMapId);
    Neighborhood* neighborhood = CreateNeighborhood(playerGuid, name, targetMapId, factionRestriction, /*isPublic*/ true);
    if (neighborhood)
    {
        TC_LOG_INFO("housing", "FindOrCreateTutorialNeighborhood: Created tutorial neighborhood '{}' (map {}) for player {}",
            name, targetMapId, playerGuid.ToString());
    }

    return neighborhood;
}

void NeighborhoodMgr::EnsurePublicNeighborhoods()
{
    // Ensure at least one public neighborhood exists per faction
    // This guarantees players always have a neighborhood to enter via the tutorial flow

    bool hasAlliancePublic = false;
    bool hasHordePublic = false;

    for (auto const& [guid, neighborhood] : _neighborhoods)
    {
        if (!neighborhood->IsPublic())
            continue;

        int32 faction = neighborhood->GetFactionRestriction();
        if (faction == NEIGHBORHOOD_FACTION_ALLIANCE)
            hasAlliancePublic = true;
        else if (faction == NEIGHBORHOOD_FACTION_HORDE)
            hasHordePublic = true;
    }

    // Find system-generatable NeighborhoodMap entries for missing factions
    for (auto const& [id, data] : sHousingMgr.GetAllNeighborhoodMapData())
    {
        int32 flags = data.UiMapID; // Stores faction/flags bitmask
        bool isAlliance = (flags & 0x1) != 0;
        bool isHorde = (flags & 0x2) != 0;
        bool canSystemGenerate = (flags & 0x4) != 0;

        if (!canSystemGenerate)
            continue;

        if (!hasAlliancePublic && isAlliance)
        {
            // Create a system-owned Alliance neighborhood (owner guid = empty)
            // Use a sentinel owner guid so the neighborhood has a valid owner
            ObjectGuid systemOwner = ObjectGuid::Create<HighGuid::Housing>(/*subType*/ 4, /*arg1*/ 0, /*arg2*/ 0, uint64(0));
            std::string allianceName = sHousingMgr.GenerateNeighborhoodName(id);
            Neighborhood* neighborhood = CreateNeighborhood(systemOwner, allianceName, id, NEIGHBORHOOD_FACTION_ALLIANCE, /*isPublic*/ true);
            if (neighborhood)
            {
                hasAlliancePublic = true;
                TC_LOG_INFO("server.loading", ">> Created default public Alliance neighborhood '{}' (map {})", allianceName, id);
            }
        }

        if (!hasHordePublic && isHorde)
        {
            ObjectGuid systemOwner = ObjectGuid::Create<HighGuid::Housing>(/*subType*/ 4, /*arg1*/ 0, /*arg2*/ 1, uint64(0));
            std::string hordeName = sHousingMgr.GenerateNeighborhoodName(id);
            Neighborhood* neighborhood = CreateNeighborhood(systemOwner, hordeName, id, NEIGHBORHOOD_FACTION_HORDE, /*isPublic*/ true);
            if (neighborhood)
            {
                hasHordePublic = true;
                TC_LOG_INFO("server.loading", ">> Created default public Horde neighborhood '{}' (map {})", hordeName, id);
            }
        }
    }

    if (hasAlliancePublic && hasHordePublic)
        TC_LOG_INFO("server.loading", ">> Public neighborhoods verified for both factions");
    else if (!hasAlliancePublic || !hasHordePublic)
        TC_LOG_WARN("server.loading", ">> Missing public neighborhood for {} — no system-generatable NeighborhoodMap found",
            !hasAlliancePublic ? "Alliance" : "Horde");
}

void NeighborhoodMgr::CheckAndExpandNeighborhoods()
{
    // For each faction, check if all public neighborhoods are at or above 50% occupation
    // If so, create a new one to accommodate future players

    // Group public neighborhoods by faction
    std::unordered_map<int32, std::vector<Neighborhood*>> factionNeighborhoods;
    for (auto const& [guid, neighborhood] : _neighborhoods)
    {
        if (neighborhood->IsPublic())
            factionNeighborhoods[neighborhood->GetFactionRestriction()].push_back(neighborhood.get());
    }

    // Check each faction
    for (auto const& [faction, neighborhoods] : factionNeighborhoods)
    {
        if (faction == NEIGHBORHOOD_FACTION_NONE)
            continue;

        bool hasCapacity = false;
        for (Neighborhood* neighborhood : neighborhoods)
        {
            // Count occupied plots
            uint32 occupiedPlots = 0;
            for (Neighborhood::PlotInfo const& plot : neighborhood->GetPlots())
            {
                if (!plot.OwnerGuid.IsEmpty())
                    ++occupiedPlots;
            }

            // If any neighborhood is below 50% occupation, there's still capacity
            if (occupiedPlots < MAX_NEIGHBORHOOD_PLOTS / 2)
            {
                hasCapacity = true;
                break;
            }
        }

        if (hasCapacity)
            continue;

        // All public neighborhoods for this faction are at or above 50% — create a new one
        // Find the correct NeighborhoodMapID for this faction
        uint32 targetMapId = 0;
        for (auto const& [id, data] : sHousingMgr.GetAllNeighborhoodMapData())
        {
            int32 flags = data.UiMapID;
            bool isAlliance = (flags & 0x1) != 0;
            bool isHorde = (flags & 0x2) != 0;
            bool canSystemGenerate = (flags & 0x4) != 0;

            if (!canSystemGenerate)
                continue;

            if (faction == NEIGHBORHOOD_FACTION_ALLIANCE && isAlliance)
                { targetMapId = id; break; }
            else if (faction == NEIGHBORHOOD_FACTION_HORDE && isHorde)
                { targetMapId = id; break; }
        }

        if (targetMapId == 0)
            continue;

        std::string name = sHousingMgr.GenerateNeighborhoodName(targetMapId);
        ObjectGuid systemOwner = ObjectGuid::Create<HighGuid::Housing>(/*subType*/ 4, /*arg1*/ 0, /*arg2*/ 0, uint64(0));

        // Use a unique system owner per neighborhood to avoid the "owner already has a neighborhood" check
        // Offset by the number of existing neighborhoods for this faction
        systemOwner = ObjectGuid::Create<HighGuid::Housing>(/*subType*/ 4, /*arg1*/ 0,
            /*arg2*/ static_cast<uint32>(neighborhoods.size()), uint64(0));

        Neighborhood* newNeighborhood = CreateNeighborhood(systemOwner, name, targetMapId, faction, /*isPublic*/ true);
        if (newNeighborhood)
        {
            TC_LOG_INFO("housing", "CheckAndExpandNeighborhoods: Created new {} neighborhood '{}' (all existing at 50%+ capacity)",
                faction == NEIGHBORHOOD_FACTION_ALLIANCE ? "Alliance" : "Horde", name);
        }
    }
}

ObjectGuid NeighborhoodMgr::GenerateNeighborhoodGuid()
{
    if (_nextGuid >= 0xFFFFFFFFFFFFFFFE)
    {
        TC_LOG_ERROR("housing", "Neighborhood guid overflow! Cannot continue, shutting down server.");
        World::StopNow(ERROR_EXIT_CODE);
    }

    uint64 counter = _nextGuid++;
    return ObjectGuid::Create<HighGuid::Housing>(/*subType*/ 4, /*arg1*/ 0, /*arg2*/ 0, counter);
}
