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
#include "DB2Stores.h"
#include "GameTime.h"
#include "HousingDefines.h"
#include "HousingMap.h"
#include "HousingMgr.h"
#include "Log.h"
#include "Map.h"
#include "Neighborhood.h"
#include "Player.h"
#include "RealmList.h"
#include "SharedDefines.h"
#include "StringFormat.h"
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
    VerifyNeighborhoodFactions();
    EnsurePublicNeighborhoods();
    MigrateWrongFactionResidents();
    RegenerateNeighborhoodNames();
}

void NeighborhoodMgr::Update(uint32 diff)
{
    // Periodic neighborhood expansion check (every 60 seconds)
    // Ensures new instances are created even if no one is actively purchasing plots
    static constexpr uint32 EXPANSION_CHECK_INTERVAL = 60 * IN_MILLISECONDS;

    _expansionCheckTimer += diff;
    if (_expansionCheckTimer >= EXPANSION_CHECK_INTERVAL)
    {
        _expansionCheckTimer = 0;
        CheckAndExpandNeighborhoods();
    }
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

        ObjectGuid neighborhoodGuid = ObjectGuid::Create<HighGuid::Housing>(/*subType*/ 4, /*arg1*/ sRealmList->GetCurrentRealmId().Realm, /*arg2*/ 0, guidLow);

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

    // Debug dump all neighborhoods and their plot states
    for (auto const& [guid, neighborhood] : _neighborhoods)
    {
        TC_LOG_INFO("housing", "NEIGHBORHOOD_DUMP: guid={} name='{}' mapId={} faction={} public={} "
            "members={} occupiedPlots={} owner={}",
            guid.ToString(), neighborhood->GetName(), neighborhood->GetNeighborhoodMapID(),
            neighborhood->GetFactionRestriction(), neighborhood->IsPublic(),
            neighborhood->GetMemberCount(), neighborhood->GetOccupiedPlotCount(),
            neighborhood->GetOwnerGuid().ToString());

        for (auto const& member : neighborhood->GetMembers())
        {
            TC_LOG_INFO("housing", "  MEMBER: player={} role={} plotIndex={} houseGuid={}",
                member.PlayerGuid.ToString(), member.Role, member.PlotIndex, member.HouseGuid.ToString());
        }

        for (uint8 i = 0; i < MAX_NEIGHBORHOOD_PLOTS; ++i)
        {
            auto const& plot = neighborhood->GetPlots()[i];
            if (plot.IsOccupied())
            {
                TC_LOG_INFO("housing", "  PLOT[{}]: owner={} house={} bnet={}",
                    i, plot.OwnerGuid.ToString(), plot.HouseGuid.ToString(), plot.OwnerBnetGuid.ToString());
            }
        }
    }
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

Neighborhood* NeighborhoodMgr::ResolveNeighborhood(ObjectGuid guid, Player* player)
{
    // Try direct lookup first (correct Housing GUID format)
    if (Neighborhood* neighborhood = GetNeighborhood(guid))
        return neighborhood;

    // If the GUID lookup fails (e.g., client sent a bulletin board GO GUID),
    // fall back to the player's current housing map neighborhood
    if (player)
    {
        if (HousingMap* housingMap = dynamic_cast<HousingMap*>(player->GetMap()))
        {
            if (Neighborhood* neighborhood = housingMap->GetNeighborhood())
            {
                TC_LOG_DEBUG("housing", "NeighborhoodMgr::ResolveNeighborhood: Resolved client GUID {} to neighborhood '{}' via housing map fallback",
                    guid.ToString(), neighborhood->GetName());
                return neighborhood;
            }
        }
    }

    return nullptr;
}

Neighborhood* NeighborhoodMgr::GetNeighborhoodByOwner(ObjectGuid ownerGuid)
{
    auto it = _ownerToNeighborhood.find(ownerGuid);
    if (it != _ownerToNeighborhood.end())
        return GetNeighborhood(it->second);

    return nullptr;
}

Neighborhood* NeighborhoodMgr::GetNeighborhoodByGuildId(uint32 guildId)
{
    if (guildId == 0)
        return nullptr;

    for (auto const& [guid, neighborhood] : _neighborhoods)
    {
        if (neighborhood->GetGuildId() == guildId)
            return neighborhood.get();
    }
    return nullptr;
}

std::vector<Neighborhood*> NeighborhoodMgr::GetAllNeighborhoods() const
{
    std::vector<Neighborhood*> result;
    result.reserve(_neighborhoods.size());
    for (auto const& [guid, neighborhood] : _neighborhoods)
        result.push_back(neighborhood.get());
    return result;
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

std::vector<Neighborhood*> NeighborhoodMgr::GetNeighborhoodsByBnetAccount(ObjectGuid bnetAccountGuid) const
{
    std::vector<Neighborhood*> result;
    for (auto const& [guid, neighborhood] : _neighborhoods)
    {
        for (auto const& plot : neighborhood->GetPlots())
        {
            if (plot.IsOccupied() && plot.OwnerBnetGuid == bnetAccountGuid)
            {
                result.push_back(neighborhood.get());
                break; // Only add each neighborhood once
            }
        }
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

Neighborhood* NeighborhoodMgr::FindOrCreatePublicNeighborhood(uint32 teamId)
{
    // Determine the correct NeighborhoodMapID for the faction
    uint32 targetMapId = 0;

    for (auto const& [id, data] : sHousingMgr.GetAllNeighborhoodMapData())
    {
        int32 flags = data.FactionRestriction;
        bool isAlliance = (flags & 0x1) != 0;
        bool isHorde = (flags & 0x2) != 0;
        bool canSystemGenerate = (flags & 0x4) != 0;

        if (!canSystemGenerate)
            continue;

        if ((teamId == ALLIANCE && isAlliance) || (teamId == HORDE && isHorde))
        {
            targetMapId = id;
            break;
        }
    }

    if (targetMapId == 0)
    {
        TC_LOG_ERROR("housing", "FindOrCreatePublicNeighborhood: No system-generatable NeighborhoodMap found for team {}", teamId);
        return nullptr;
    }

    // Look for an existing public neighborhood — no membership changes
    Neighborhood* found = FindPublicNeighborhoodForMap(targetMapId);
    if (found)
        return found;

    // None exists yet — EnsurePublicNeighborhoods should have created them at startup.
    // Force-run it now as a fallback, then retry.
    TC_LOG_WARN("housing", "FindOrCreatePublicNeighborhood: No public neighborhood for map {}, running EnsurePublicNeighborhoods", targetMapId);
    EnsurePublicNeighborhoods();

    return FindPublicNeighborhoodForMap(targetMapId);
}

Neighborhood* NeighborhoodMgr::FindPublicNeighborhoodForMap(uint32 neighborhoodMapId) const
{
    // Return the least-loaded public neighborhood on this map.
    // When multiple instances exist (after expansion), we want to distribute
    // players evenly rather than always returning the first-created instance.
    Neighborhood* best = nullptr;
    uint32 bestOccupancy = MAX_NEIGHBORHOOD_PLOTS + 1;

    for (auto const& [guid, neighborhood] : _neighborhoods)
    {
        if (neighborhood->GetNeighborhoodMapID() == neighborhoodMapId && neighborhood->IsPublic())
        {
            uint32 occupancy = neighborhood->GetOccupiedPlotCount();
            if (occupancy < bestOccupancy)
            {
                best = neighborhood.get();
                bestOccupancy = occupancy;
            }
        }
    }
    return best;
}

void NeighborhoodMgr::VerifyNeighborhoodFactions()
{
    // Verify that each public neighborhood's factionRestriction matches its NeighborhoodMap's faction flags.
    // This fixes data inconsistencies from earlier code that may have assigned wrong faction values.
    // Must run BEFORE EnsurePublicNeighborhoods and MigrateWrongFactionResidents.

    auto const& allMaps = sHousingMgr.GetAllNeighborhoodMapData();
    uint32 fixedCount = 0;

    for (auto& [guid, nb] : _neighborhoods)
    {
        if (!nb->IsPublic())
            continue;

        uint32 mapId = nb->GetNeighborhoodMapID();
        auto it = allMaps.find(mapId);
        if (it == allMaps.end())
            continue;

        int32 mapFlags = it->second.FactionRestriction;
        bool mapIsAlliance = (mapFlags & 0x1) != 0;
        bool mapIsHorde = (mapFlags & 0x2) != 0;

        // Determine the correct faction restriction from the NeighborhoodMap flags
        int32 correctFaction = NEIGHBORHOOD_FACTION_NONE;
        if (mapIsAlliance && !mapIsHorde)
            correctFaction = NEIGHBORHOOD_FACTION_ALLIANCE;
        else if (mapIsHorde && !mapIsAlliance)
            correctFaction = NEIGHBORHOOD_FACTION_HORDE;

        if (correctFaction == NEIGHBORHOOD_FACTION_NONE)
            continue; // Ambiguous or no faction — skip

        int32 currentFaction = nb->GetFactionRestriction();
        if (currentFaction == correctFaction)
            continue; // Already correct

        TC_LOG_INFO("server.loading", ">> Fixing neighborhood '{}' (guid={}) factionRestriction: {} -> {} (based on NeighborhoodMap {} flags)",
            nb->GetName(), guid.ToString(), currentFaction, correctFaction, mapId);

        // Update in DB
        CharacterDatabase.DirectExecute(
            Trinity::StringFormat("UPDATE neighborhoods SET factionRestriction = {} WHERE guid = {}",
                correctFaction, guid.GetCounter()).c_str());

        // Update in memory
        nb->SetFactionRestriction(correctFaction);
        ++fixedCount;
    }

    if (fixedCount > 0)
        TC_LOG_INFO("server.loading", ">> Fixed factionRestriction for {} neighborhood(s)", fixedCount);
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
        int32 flags = data.FactionRestriction;
        bool isAlliance = (flags & 0x1) != 0;
        bool isHorde = (flags & 0x2) != 0;
        bool canSystemGenerate = (flags & 0x4) != 0;

        if (!canSystemGenerate)
            continue;

        if (!hasAlliancePublic && isAlliance)
        {
            // Create a system-owned Alliance neighborhood (owner guid = empty)
            // Use a sentinel owner guid so the neighborhood has a valid owner
            ObjectGuid systemOwner = ObjectGuid::Create<HighGuid::Housing>(/*subType*/ 4, /*arg1*/ sRealmList->GetCurrentRealmId().Realm, /*arg2*/ 0, uint64(0));
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
            ObjectGuid systemOwner = ObjectGuid::Create<HighGuid::Housing>(/*subType*/ 4, /*arg1*/ sRealmList->GetCurrentRealmId().Realm, /*arg2*/ 1, uint64(0));
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

void NeighborhoodMgr::MigrateWrongFactionResidents()
{
    // After EnsurePublicNeighborhoods creates missing faction neighborhoods,
    // check if any members are in the wrong faction's public neighborhood.
    // This handles legacy data from before faction restrictions were enforced:
    // e.g. Alliance characters placed in a Horde neighborhood when only one existed.

    // Find public neighborhoods by faction
    uint64 allianceNbLow = 0;
    uint64 hordeNbLow = 0;

    for (auto const& [guid, nb] : _neighborhoods)
    {
        if (!nb->IsPublic())
            continue;
        if (nb->GetFactionRestriction() == NEIGHBORHOOD_FACTION_ALLIANCE && !allianceNbLow)
            allianceNbLow = guid.GetCounter();
        else if (nb->GetFactionRestriction() == NEIGHBORHOOD_FACTION_HORDE && !hordeNbLow)
            hordeNbLow = guid.GetCounter();
    }

    if (!allianceNbLow || !hordeNbLow)
        return;

    // Query all members in faction-restricted public neighborhoods joined with their race
    QueryResult result = CharacterDatabase.Query(
        "SELECT nm.playerGuid, nm.neighborhoodGuid, nm.plotIndex, nm.role, nm.joinTime, c.race "
        "FROM neighborhood_members nm "
        "JOIN characters c ON nm.playerGuid = c.guid "
        "JOIN neighborhoods n ON nm.neighborhoodGuid = n.guid "
        "WHERE n.isPublic = 1 AND n.factionRestriction != 0");

    if (!result)
        return;

    struct MemberInfo
    {
        uint64 PlayerGuidLow;
        uint64 NbGuidLow;
        uint8 PlotIndex;
        uint8 Role;
        uint32 JoinTime;
        uint8 Race;
    };

    std::vector<MemberInfo> allMembers;
    do
    {
        Field* fields = result->Fetch();
        allMembers.push_back({
            fields[0].GetUInt64(),
            fields[1].GetUInt64(),
            fields[2].GetUInt8(),
            fields[3].GetUInt8(),
            fields[4].GetUInt32(),
            fields[5].GetUInt8()
        });
    } while (result->NextRow());

    // Build set of existing memberships for quick lookup: (playerGuid, nbGuid)
    std::set<std::pair<uint64, uint64>> membershipSet;
    for (auto const& m : allMembers)
        membershipSet.insert({m.PlayerGuidLow, m.NbGuidLow});

    // Pre-populate used plots in each target neighborhood
    std::set<uint8> usedPlotsInAlliance;
    std::set<uint8> usedPlotsInHorde;
    for (auto const& m : allMembers)
    {
        if (m.NbGuidLow == allianceNbLow && m.PlotIndex != INVALID_PLOT_INDEX)
            usedPlotsInAlliance.insert(m.PlotIndex);
        else if (m.NbGuidLow == hordeNbLow && m.PlotIndex != INVALID_PLOT_INDEX)
            usedPlotsInHorde.insert(m.PlotIndex);
    }

    uint32 migratedCount = 0;

    for (auto const& m : allMembers)
    {
        Team team = Player::TeamForRace(m.Race);
        uint64 correctNbLow = (team == ALLIANCE) ? allianceNbLow : hordeNbLow;

        if (m.NbGuidLow == correctNbLow)
            continue; // Already in correct faction's neighborhood

        bool alreadyInCorrect = membershipSet.count({m.PlayerGuidLow, correctNbLow}) > 0;

        // Delete old wrong-faction membership
        CharacterDatabasePreparedStatement* delStmt = CharacterDatabase.GetPreparedStatement(CHAR_DEL_NEIGHBORHOOD_MEMBER);
        delStmt->setUInt64(0, m.NbGuidLow);
        delStmt->setUInt64(1, m.PlayerGuidLow);
        CharacterDatabase.DirectExecute(delStmt);

        if (!alreadyInCorrect)
        {
            // Player doesn't have a membership in the correct neighborhood yet — create one
            std::set<uint8>& usedPlots = (correctNbLow == allianceNbLow) ? usedPlotsInAlliance : usedPlotsInHorde;
            uint8 newPlotIndex = m.PlotIndex;

            // Check if character_housing already points to the correct neighborhood
            // (e.g., player bought a new house there before migration ran)
            QueryResult housingResult = CharacterDatabase.Query(
                Trinity::StringFormat("SELECT plotIndex FROM character_housing WHERE guid = {} AND neighborhoodGuid = {}",
                    m.PlayerGuidLow, correctNbLow).c_str());
            if (housingResult)
                newPlotIndex = housingResult->Fetch()[0].GetUInt8();

            // Check for plot conflict in target neighborhood
            if (newPlotIndex != INVALID_PLOT_INDEX && usedPlots.count(newPlotIndex))
            {
                // Find first available plot
                newPlotIndex = INVALID_PLOT_INDEX;
                for (uint8 i = 0; i < MAX_NEIGHBORHOOD_PLOTS; ++i)
                {
                    if (!usedPlots.count(i))
                    {
                        newPlotIndex = i;
                        break;
                    }
                }
            }

            if (newPlotIndex != INVALID_PLOT_INDEX)
                usedPlots.insert(newPlotIndex);

            // Insert new membership in correct neighborhood
            CharacterDatabasePreparedStatement* insStmt = CharacterDatabase.GetPreparedStatement(CHAR_INS_NEIGHBORHOOD_MEMBER);
            insStmt->setUInt64(0, correctNbLow);
            insStmt->setUInt64(1, m.PlayerGuidLow);
            insStmt->setUInt8(2, m.Role);
            insStmt->setUInt32(3, m.JoinTime);
            insStmt->setUInt8(4, newPlotIndex);
            CharacterDatabase.DirectExecute(insStmt);

            // Update character_housing to point to correct neighborhood (only if it still references the old one)
            CharacterDatabase.DirectExecute(
                Trinity::StringFormat("UPDATE character_housing SET neighborhoodGuid = {}, plotIndex = {} WHERE guid = {} AND neighborhoodGuid = {}",
                    correctNbLow, newPlotIndex, m.PlayerGuidLow, m.NbGuidLow).c_str());

            TC_LOG_INFO("server.loading", ">> Migrated player {} from neighborhood {} to {} (plot {} -> {})",
                m.PlayerGuidLow, m.NbGuidLow, correctNbLow, m.PlotIndex, newPlotIndex);
        }
        else
        {
            TC_LOG_INFO("server.loading", ">> Removed duplicate wrong-faction membership for player {} from neighborhood {}",
                m.PlayerGuidLow, m.NbGuidLow);
        }

        ++migratedCount;
    }

    if (migratedCount > 0)
    {
        TC_LOG_INFO("server.loading", ">> Migrated {} resident(s) to correct faction neighborhoods — reloading", migratedCount);
        LoadFromDB(); // Reload to pick up the changes
    }
}

void NeighborhoodMgr::RegenerateNeighborhoodNames()
{
    // Neighborhood names are stored as "ID1-ID2-ID3" tokens referencing NeighborhoodNameGen
    // entry IDs from the base DB2. Old names (from hotfix-era data) may contain text
    // where the values are the TEXT content of overwritten entries, not actual DB2 entry IDs.
    // Validate each public neighborhood's name: parse tokens, verify each is a real entry.
    uint32 regenerated = 0;
    for (auto& [guid, neighborhood] : _neighborhoods)
    {
        if (!neighborhood->IsPublic())
            continue;

        std::string const& name = neighborhood->GetName();
        bool needsRegeneration = false;

        // Validate: name must be "ID1-ID2-ID3" where each ID is a valid NeighborhoodNameGen entry
        std::vector<std::string> tokens;
        std::string token;
        for (char c : name)
        {
            if (c == '-')
            {
                if (!token.empty())
                    tokens.push_back(token);
                token.clear();
            }
            else
                token += c;
        }
        if (!token.empty())
            tokens.push_back(token);

        if (tokens.size() != 3)
        {
            needsRegeneration = true;
        }
        else
        {
            for (std::string const& t : tokens)
            {
                // Must be purely numeric
                bool allDigits = !t.empty();
                for (char c : t)
                {
                    if (c < '0' || c > '9')
                    {
                        allDigits = false;
                        break;
                    }
                }
                if (!allDigits)
                {
                    needsRegeneration = true;
                    break;
                }

                // Must reference a valid NeighborhoodNameGen entry in the base DB2
                uint32 entryId = std::stoul(t);
                if (!sNeighborhoodNameGenStore.LookupEntry(entryId))
                {
                    needsRegeneration = true;
                    break;
                }
            }
        }

        if (!needsRegeneration)
            continue;

        std::string newName = sHousingMgr.GenerateNeighborhoodName(neighborhood->GetNeighborhoodMapID());
        if (newName == "Unnamed Neighborhood")
            continue;

        TC_LOG_INFO("server.loading", ">> Regenerating neighborhood (guid={}) name: '{}' -> '{}'",
            guid.ToString(), name, newName);
        neighborhood->SetName(newName);
        ++regenerated;
    }

    if (regenerated > 0)
        TC_LOG_INFO("server.loading", ">> Regenerated {} neighborhood name(s) using base DB2 entry IDs", regenerated);
    else
        TC_LOG_INFO("server.loading", ">> Public neighborhood names verified");
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
            uint32 occupiedPlots = neighborhood->GetOccupiedPlotCount();
            uint32 memberCount = neighborhood->GetMemberCount();

            // Check both occupied plots AND member count — either can be the bottleneck.
            // A neighborhood might have many members (invited/added) but few plots occupied,
            // or vice versa. Use the higher of the two for capacity assessment.
            uint32 usage = std::max(occupiedPlots, memberCount);

            // If any neighborhood is below 50% usage, there's still capacity
            if (usage < MAX_NEIGHBORHOOD_PLOTS / 2)
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
            int32 flags = data.FactionRestriction;
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
        // Use a unique system owner per neighborhood to avoid the "owner already has a neighborhood" check
        // Offset by the number of existing neighborhoods for this faction
        ObjectGuid systemOwner = ObjectGuid::Create<HighGuid::Housing>(/*subType*/ 4, /*arg1*/ sRealmList->GetCurrentRealmId().Realm,
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
    return ObjectGuid::Create<HighGuid::Housing>(/*subType*/ 4, /*arg1*/ sRealmList->GetCurrentRealmId().Realm, /*arg2*/ 0, counter);
}
