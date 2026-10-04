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

#include "StringConvert.h"
#include "DB2Stores.h"
#include "DatabaseEnv.h"
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
#include <algorithm>

namespace
{
    // NeighborhoodMap.db2 FactionRestriction flag bits
    constexpr int32 NEIGHBORHOOD_MAP_FLAG_ALLIANCE = 0x1;
    constexpr int32 NEIGHBORHOOD_MAP_FLAG_HORDE = 0x2;
    constexpr int32 NEIGHBORHOOD_MAP_FLAG_SYSTEM_GENERATED = 0x4;

    // Housing GUID subtype used for neighborhood guids
    constexpr uint32 HOUSING_GUID_SUBTYPE_NEIGHBORHOOD = 4;
}

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
    // Periodic check; spawns new public instances when existing ones fill up.
    constexpr uint32 EXPANSION_CHECK_INTERVAL = 60 * IN_MILLISECONDS;

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

        // Rebuild the GUID exactly as GenerateNeighborhoodGuid minted it.
        ObjectGuid neighborhoodGuid = MakeNeighborhoodGuid(fields[2].GetUInt32(), fields[3].GetUInt64() != 0, guidLow);

        auto neighborhood = std::make_unique<Neighborhood>(neighborhoodGuid);

        CharacterDatabasePreparedStatement* memberStmt = CharacterDatabase.GetPreparedStatement(CHAR_SEL_NEIGHBORHOOD_MEMBERS);
        memberStmt->setUInt64(0, guidLow);
        PreparedQueryResult memberResult = CharacterDatabase.Query(memberStmt);

        CharacterDatabasePreparedStatement* inviteStmt = CharacterDatabase.GetPreparedStatement(CHAR_SEL_NEIGHBORHOOD_INVITES);
        inviteStmt->setUInt64(0, guidLow);
        PreparedQueryResult inviteResult = CharacterDatabase.Query(inviteStmt);

        // Per-member render state so occupied plots render while owners are offline.
        CharacterDatabasePreparedStatement* fixStmt = CharacterDatabase.GetPreparedStatement(CHAR_SEL_NEIGHBORHOOD_MEMBER_FIXTURES);
        fixStmt->setUInt64(0, guidLow);
        PreparedQueryResult fixtureResult = CharacterDatabase.Query(fixStmt);

        CharacterDatabasePreparedStatement* decorStmt = CharacterDatabase.GetPreparedStatement(CHAR_SEL_NEIGHBORHOOD_MEMBER_DECOR);
        decorStmt->setUInt64(0, guidLow);
        PreparedQueryResult decorResult = CharacterDatabase.Query(decorStmt);

        CharacterDatabasePreparedStatement* roomStmt = CharacterDatabase.GetPreparedStatement(CHAR_SEL_NEIGHBORHOOD_MEMBER_ROOMS);
        roomStmt->setUInt64(0, guidLow);
        PreparedQueryResult roomResult = CharacterDatabase.Query(roomStmt);

        CharacterDatabasePreparedStatement* neighborhoodStmt = CharacterDatabase.GetPreparedStatement(CHAR_SEL_NEIGHBORHOOD);
        neighborhoodStmt->setUInt64(0, guidLow);
        PreparedQueryResult neighborhoodResult = CharacterDatabase.Query(neighborhoodStmt);

        if (!neighborhood->LoadFromDB(neighborhoodResult, memberResult, inviteResult, fixtureResult, decorResult, roomResult))
        {
            TC_LOG_ERROR("housing", "NeighborhoodMgr::LoadFromDB: Failed to load neighborhood guid {}. Skipping.", guidLow);
            continue;
        }

        // One charter neighborhood per character; a guild neighborhood belongs to its guild (GetNeighborhoodByGuildId).
        if (!neighborhood->GetGuildId())
            _ownerToNeighborhood[neighborhood->GetOwnerGuid()] = neighborhoodGuid;
        _neighborhoods[neighborhoodGuid] = std::move(neighborhood);
        _neighborhoodsByCounter[guidLow] = _neighborhoods[neighborhoodGuid].get();
        ++count;

    } while (result->NextRow());

    TC_LOG_INFO("server.loading", ">> Loaded {} neighborhoods in {} ms", count, GetMSTimeDiffToNow(oldMSTime));
}

Neighborhood* NeighborhoodMgr::CreateNeighborhood(ObjectGuid ownerGuid, std::string const& name, uint32 neighborhoodMapID, int32 factionRestriction, bool isPublic /*= false*/, uint32 guildId /*= 0*/)
{
    // One charter neighborhood per character; a guild neighborhood belongs to its guild, one per guild.
    if (guildId ? GetNeighborhoodByGuildId(guildId) != nullptr : _ownerToNeighborhood.contains(ownerGuid))
    {
        return nullptr;
    }

    // System neighborhoods are owned by a HighGuid::Housing placeholder and keep a NeighborhoodNameGen name.
    ObjectGuid neighborhoodGuid = GenerateNeighborhoodGuid(neighborhoodMapID, ownerGuid.IsPlayer());

    auto neighborhood = std::make_unique<Neighborhood>(neighborhoodGuid);

    uint32 createTime = static_cast<uint32>(GameTime::GetGameTime());

    CharacterDatabaseTransaction trans = CharacterDatabase.BeginTransaction();

    CharacterDatabasePreparedStatement* stmt = CharacterDatabase.GetPreparedStatement(CHAR_INS_NEIGHBORHOOD);
    uint8 index = 0;
    stmt->setUInt64(index++, neighborhoodGuid.GetCounter());
    stmt->setString(index++, name);
    stmt->setUInt32(index++, neighborhoodMapID);
    stmt->setUInt64(index++, ownerGuid.GetCounter());
    stmt->setInt32(index++, factionRestriction);
    stmt->setBool(index++, isPublic);
    stmt->setUInt32(index++, createTime);
    stmt->setUInt32(index++, guildId);
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

    // Load from DB to populate all internal structures
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
    if (!guildId)
        _ownerToNeighborhood[ownerGuid] = neighborhoodGuid;
    _neighborhoods[neighborhoodGuid] = std::move(neighborhood);
    _neighborhoodsByCounter[neighborhoodGuid.GetCounter()] = result;

    return result;
}

Neighborhood* NeighborhoodMgr::CreateGuildNeighborhood(ObjectGuid ownerGuid, std::string const& name, uint32 neighborhoodMapID, uint32 factionID, uint32 guildId)
{
    int32 factionRestriction = NEIGHBORHOOD_FACTION_NONE;
    if (factionID == HORDE)
        factionRestriction = NEIGHBORHOOD_FACTION_HORDE;
    else if (factionID == ALLIANCE)
        factionRestriction = NEIGHBORHOOD_FACTION_ALLIANCE;

    // Persist the guild link at creation so GetNeighborhoodByGuildId works across restarts.
    Neighborhood* neighborhood = CreateNeighborhood(ownerGuid, name, neighborhoodMapID, factionRestriction, /*isPublic*/ false, guildId);
    if (neighborhood)
        neighborhood->SetGuildId(guildId);
    return neighborhood;
}

void NeighborhoodMgr::DeleteNeighborhood(ObjectGuid neighborhoodGuid)
{
    auto it = _neighborhoods.find(neighborhoodGuid);
    if (it == _neighborhoods.end())
    {
        return;
    }

    ObjectGuid ownerGuid = it->second->GetOwnerGuid();

    // Delete from DB
    CharacterDatabaseTransaction trans = CharacterDatabase.BeginTransaction();
    Neighborhood::DeleteFromDB(neighborhoodGuid.GetCounter(), trans);
    CharacterDatabase.CommitTransaction(trans);

    // Remove from maps (a guild neighborhood was never registered under its founder)
    if (auto ownerItr = _ownerToNeighborhood.find(ownerGuid); ownerItr != _ownerToNeighborhood.end() && ownerItr->second == neighborhoodGuid)
        _ownerToNeighborhood.erase(ownerItr);
    _neighborhoodsByCounter.erase(it->first.GetCounter());
    _neighborhoods.erase(it);

}

bool NeighborhoodMgr::SetNeighborhoodOwner(Neighborhood& neighborhood, ObjectGuid newOwnerGuid)
{
    // Guild neighborhoods belong to their guild; a charter transfer does not apply.
    if (neighborhood.GetGuildId())
        return false;

    ObjectGuid oldOwnerGuid = neighborhood.GetOwnerGuid();
    if (newOwnerGuid.IsEmpty() || newOwnerGuid == oldOwnerGuid)
        return false;

    // One charter neighborhood per character.
    if (Neighborhood* existing = GetNeighborhoodByOwner(newOwnerGuid))
        if (existing != &neighborhood)
            return false;

    if (!neighborhood.ReassignOwner(newOwnerGuid))
        return false;

    // Re-key the ownership registry.
    _ownerToNeighborhood.erase(oldOwnerGuid);
    _ownerToNeighborhood[newOwnerGuid] = neighborhood.GetGuid();
    return true;
}

Neighborhood* NeighborhoodMgr::FindNeighborhoodByName(std::string_view name)
{
    for (auto const& [guid, neighborhood] : _neighborhoods)
        if (StringEqualI(neighborhood->GetName(), name))
            return neighborhood.get();

    return nullptr;
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

    // Client may send a bulletin board GO GUID; fall back to the player's current housing map.
    if (player)
    {
        if (HousingMap* housingMap = dynamic_cast<HousingMap*>(player->GetMap()))
        {
            if (Neighborhood* neighborhood = housingMap->GetNeighborhood())
            {
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
        int32 flags = data.Flags;
        bool isAlliance = (flags & NEIGHBORHOOD_MAP_FLAG_ALLIANCE) != 0;
        bool isHorde = (flags & NEIGHBORHOOD_MAP_FLAG_HORDE) != 0;
        bool canSystemGenerate = (flags & NEIGHBORHOOD_MAP_FLAG_SYSTEM_GENERATED) != 0;

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
        char const* factionName = (teamId == ALLIANCE) ? "Alliance" : (teamId == HORDE) ? "Horde" : "unknown";
        uint32 wantBit = (teamId == ALLIANCE) ? NEIGHBORHOOD_MAP_FLAG_ALLIANCE : NEIGHBORHOOD_MAP_FLAG_HORDE;
        // No matching map exists: full housing lockout for the faction.
        TC_LOG_ERROR("housing",
            "FindOrCreatePublicNeighborhood: HOUSING LOCKOUT for {} — NeighborhoodMap has no system-generatable "
            "row (Flags bit 0x4) carrying the {} flag (0x{:X}). Players of this faction cannot enter housing. "
            "Check NeighborhoodMap.db2 in the extracted client data: "
            "Alliance = ID 1 / MapID 2735 / FactionRestriction 5 (0x1|0x4), "
            "Horde = ID 2 / MapID 2736 / FactionRestriction 6 (0x2|0x4).",
            factionName, factionName, wantBit);
        return nullptr;
    }

    Neighborhood* found = FindPublicNeighborhoodForMap(targetMapId);
    if (found)
        return found;

    // Startup creation should have handled this; force-run as a fallback and retry.
    TC_LOG_WARN("housing", "FindOrCreatePublicNeighborhood: No public neighborhood for map {}, running EnsurePublicNeighborhoods", targetMapId);
    EnsurePublicNeighborhoods();

    return FindPublicNeighborhoodForMap(targetMapId);
}

Neighborhood* NeighborhoodMgr::GetNeighborhoodByCounter(uint64 counter) const
{
    auto itr = _neighborhoodsByCounter.find(counter);
    return itr != _neighborhoodsByCounter.end() ? itr->second : nullptr;
}

Neighborhood* NeighborhoodMgr::FindPublicNeighborhoodForMap(uint32 neighborhoodMapId) const
{
    // Least-loaded public neighborhood on this map, to spread players across instances.
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
    // Align factionRestriction with the map's flags; must run before EnsurePublicNeighborhoods.
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

        int32 mapFlags = it->second.Flags;
        bool mapIsAlliance = (mapFlags & NEIGHBORHOOD_MAP_FLAG_ALLIANCE) != 0;
        bool mapIsHorde = (mapFlags & NEIGHBORHOOD_MAP_FLAG_HORDE) != 0;

        int32 correctFaction = NEIGHBORHOOD_FACTION_NONE;
        if (mapIsAlliance && !mapIsHorde)
            correctFaction = NEIGHBORHOOD_FACTION_ALLIANCE;
        else if (mapIsHorde && !mapIsAlliance)
            correctFaction = NEIGHBORHOOD_FACTION_HORDE;

        if (correctFaction == NEIGHBORHOOD_FACTION_NONE)
            continue; // ambiguous or no faction

        if (nb->GetFactionRestriction() == correctFaction)
            continue;

        TC_LOG_INFO("server.loading", ">> Fixing neighborhood '{}' (guid={}) factionRestriction: {} -> {} (based on NeighborhoodMap {} flags)",
            nb->GetName(), guid.ToString(), nb->GetFactionRestriction(), correctFaction, mapId);

        CharacterDatabase.DirectExecute(
            Trinity::StringFormat("UPDATE neighborhoods SET factionRestriction = {} WHERE guid = {}",
                correctFaction, guid.GetCounter()).c_str());

        nb->SetFactionRestriction(correctFaction);
        ++fixedCount;
    }

    if (fixedCount > 0)
        TC_LOG_INFO("server.loading", ">> Fixed factionRestriction for {} neighborhood(s)", fixedCount);
}

void NeighborhoodMgr::EnsurePublicNeighborhoods()
{
    // Guarantee at least one public neighborhood per faction.
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
        int32 flags = data.Flags;
        bool isAlliance = (flags & NEIGHBORHOOD_MAP_FLAG_ALLIANCE) != 0;
        bool isHorde = (flags & NEIGHBORHOOD_MAP_FLAG_HORDE) != 0;
        bool canSystemGenerate = (flags & NEIGHBORHOOD_MAP_FLAG_SYSTEM_GENERATED) != 0;

        if (!canSystemGenerate)
            continue;

        if (!hasAlliancePublic && isAlliance)
        {
            ObjectGuid systemOwner = ObjectGuid::Create<HighGuid::Housing>(HOUSING_GUID_SUBTYPE_NEIGHBORHOOD, sRealmList->GetCurrentRealmId().Realm, /*arg2*/ 0, uint64(0));
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
            ObjectGuid systemOwner = ObjectGuid::Create<HighGuid::Housing>(HOUSING_GUID_SUBTYPE_NEIGHBORHOOD, sRealmList->GetCurrentRealmId().Realm, /*arg2*/ 1, uint64(0));
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
    {
        TC_LOG_INFO("server.loading", ">> Public neighborhoods verified for both factions");
        return;
    }

    // A faction without a public neighborhood is a hard data error, not a warning.
    if (!hasAlliancePublic)
        TC_LOG_ERROR("server.loading",
            ">> HOUSING LOCKOUT: no public Alliance neighborhood exists and none could be created. "
            "NeighborhoodMap has no system-generatable map with the Alliance flag (0x1|0x4). "
            "Check NeighborhoodMap.db2 in the extracted client data: "
            "ID 1 must be MapID 2735 with FactionRestriction 5 (0x1 Alliance | 0x4 SystemGenerate).");
    if (!hasHordePublic)
        TC_LOG_ERROR("server.loading",
            ">> HOUSING LOCKOUT: no public Horde neighborhood exists and none could be created. "
            "NeighborhoodMap has no system-generatable map with the Horde flag (0x2|0x4). "
            "Check NeighborhoodMap.db2 in the extracted client data: "
            "ID 2 must be MapID 2736 with FactionRestriction 6 (0x2 Horde | 0x4 SystemGenerate).");
}

void NeighborhoodMgr::MigrateWrongFactionResidents()
{
    // Move members stuck in wrong-faction public neighborhoods (legacy data) into the correct one.
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
            continue; // already in correct faction's neighborhood

        bool alreadyInCorrect = membershipSet.contains({m.PlayerGuidLow, correctNbLow});

        // Delete old wrong-faction membership
        CharacterDatabasePreparedStatement* delStmt = CharacterDatabase.GetPreparedStatement(CHAR_DEL_NEIGHBORHOOD_MEMBER);
        delStmt->setUInt64(0, m.NbGuidLow);
        delStmt->setUInt64(1, m.PlayerGuidLow);
        CharacterDatabase.DirectExecute(delStmt);

        if (!alreadyInCorrect)
        {
            // Player has no membership in the correct neighborhood yet — create one
            std::set<uint8>& usedPlots = (correctNbLow == allianceNbLow) ? usedPlotsInAlliance : usedPlotsInHorde;
            uint8 newPlotIndex = m.PlotIndex;

            // character_housing may already point at the correct neighborhood.
            QueryResult housingResult = CharacterDatabase.Query(
                Trinity::StringFormat("SELECT plotIndex FROM character_housing WHERE guid = {} AND neighborhoodGuid = {}",
                    m.PlayerGuidLow, correctNbLow).c_str());
            if (housingResult)
                newPlotIndex = housingResult->Fetch()[0].GetUInt8();

            if (newPlotIndex != INVALID_PLOT_INDEX && usedPlots.contains(newPlotIndex))
            {
                newPlotIndex = INVALID_PLOT_INDEX;
                for (uint8 i = 0; i < MAX_NEIGHBORHOOD_PLOTS; ++i)
                {
                    if (!usedPlots.contains(i))
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
    // Public names are "ID1-ID2-ID3" NeighborhoodNameGen entry tokens; regenerate invalid ones.
    uint32 regenerated = 0;
    for (auto& [guid, neighborhood] : _neighborhoods)
    {
        if (!neighborhood->IsPublic())
            continue;

        std::string const& name = neighborhood->GetName();
        bool needsRegeneration = false;

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
                if (t.empty() || !std::all_of(t.begin(), t.end(), [](char c) { return c >= '0' && c <= '9'; }))
                {
                    needsRegeneration = true;
                    break;
                }

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
    // Spawn a new public neighborhood once a faction's public supply drops to
    // HOUSING_EXPANSION_FREE_PLOTS free plots or fewer, so walk-in buyers always have somewhere
    // to settle without being accepted into a guild or private neighborhood.
    std::unordered_map<int32, std::vector<Neighborhood*>> factionNeighborhoods;
    for (auto const& [guid, neighborhood] : _neighborhoods)
    {
        if (neighborhood->IsPublic())
            factionNeighborhoods[neighborhood->GetFactionRestriction()].push_back(neighborhood.get());
    }

    for (auto& [faction, neighborhoods] : factionNeighborhoods)
    {
        if (faction == NEIGHBORHOOD_FACTION_NONE)
            continue;

        uint32 freePlots = 0;
        for (Neighborhood* neighborhood : neighborhoods)
        {
            // Plots or membership, whichever is the tighter bottleneck.
            uint32 usage = std::max(neighborhood->GetOccupiedPlotCount(), neighborhood->GetMemberCount());
            freePlots += usage < MAX_NEIGHBORHOOD_PLOTS ? MAX_NEIGHBORHOOD_PLOTS - usage : 0;
        }

        if (freePlots > HOUSING_EXPANSION_FREE_PLOTS)
            continue;

        uint32 targetMapId = 0;
        for (auto const& [id, data] : sHousingMgr.GetAllNeighborhoodMapData())
        {
            int32 flags = data.Flags;
            bool isAlliance = (flags & NEIGHBORHOOD_MAP_FLAG_ALLIANCE) != 0;
            bool isHorde = (flags & NEIGHBORHOOD_MAP_FLAG_HORDE) != 0;
            bool canSystemGenerate = (flags & NEIGHBORHOOD_MAP_FLAG_SYSTEM_GENERATED) != 0;

            if (!canSystemGenerate)
                continue;

            if ((faction == NEIGHBORHOOD_FACTION_ALLIANCE && isAlliance) ||
                (faction == NEIGHBORHOOD_FACTION_HORDE && isHorde))
            {
                targetMapId = id;
                break;
            }
        }

        if (targetMapId == 0)
            continue;

        std::string name = sHousingMgr.GenerateNeighborhoodName(targetMapId);
        // Unique arg2 per instance to pass the one-neighborhood-per-owner check.
        ObjectGuid systemOwner = ObjectGuid::Create<HighGuid::Housing>(HOUSING_GUID_SUBTYPE_NEIGHBORHOOD, sRealmList->GetCurrentRealmId().Realm,
            static_cast<uint32>(neighborhoods.size()), uint64(0));

        Neighborhood* spawned = CreateNeighborhood(systemOwner, name, targetMapId, faction, /*isPublic*/ true);
        if (spawned)
        {
            TC_LOG_INFO("housing", "CheckAndExpandNeighborhoods: spawned public neighborhood '{}' for faction {} "
                "(free plots had dropped to {})", name, faction, freePlots);
            // Keep the just-spawned neighborhood in the loop so repeated checks also scale.
            neighborhoods.push_back(spawned);
        }
    }
}

ObjectGuid NeighborhoodMgr::MakeNeighborhoodGuid(uint32 neighborhoodMapID, bool hasCustomName, uint64 counter)
{
    return ObjectGuid::Create<HighGuid::Housing>(HOUSING_GUID_SUBTYPE_NEIGHBORHOOD, neighborhoodMapID, hasCustomName ? 1 : 0, counter);
}

ObjectGuid NeighborhoodMgr::GenerateNeighborhoodGuid(uint32 neighborhoodMapID, bool hasCustomName)
{
    if (_nextGuid >= 0xFFFFFFFFFFFFFFFE)
    {
        TC_LOG_ERROR("housing", "Neighborhood guid overflow! Cannot continue, shutting down server.");
        World::StopNow(ERROR_EXIT_CODE);
    }

    uint64 counter = _nextGuid++;
    // arg1 must be the NeighborhoodMap.db2 record id; a realm id there makes the client's lookup miss (House Finder spins forever).
    return MakeNeighborhoodGuid(neighborhoodMapID, hasCustomName, counter);
}
