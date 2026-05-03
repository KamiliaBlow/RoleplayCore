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

#include "HousingMap.h"
#include "Account.h"
#include "HousingMirrorEntity.h"
#include "HousingNeighborhoodMirrorEntity.h"
#include "HousingPlayerHouseEntity.h"
#include "HousingRoomEntity.h"
#include <algorithm>
#include <atomic>
#include <cmath>
#include <limits>
#include <set>
#include "AreaTrigger.h"
#include "EventProcessor.h"
#include "DB2Stores.h"
#include "DB2Structure.h"
#include "GameObject.h"
#include "GridDefines.h"
#include "Housing.h"
#include "HousingDefines.h"
#include "HousingMgr.h"
#include "HousingPackets.h"
#include "Log.h"
#include "MeshObject.h"
#include "Neighborhood.h"
#include "NeighborhoodMgr.h"
#include "ObjectAccessor.h"
#include "ObjectGridLoader.h"
#include "ObjectGuid.h"
#include "ObjectMgr.h"
#include "PhasingHandler.h"
#include "Player.h"
#include "QueryPackets.h"
#include "RealmList.h"
#include "SocialMgr.h"
#include "Spell.h"
#include "SpellAuraDefines.h"
#include "SpellPackets.h"
#include "UpdateData.h"
#include "World.h"
#include "WorldSession.h"
#include "WorldStateMgr.h"

namespace
{
    std::string HexDumpPacket(WorldPacket const* packet, size_t maxBytes = 128)
    {
        if (!packet || packet->size() == 0)
            return "(empty)";
        size_t len = std::min(packet->size(), maxBytes);
        std::string result;
        result.reserve(len * 3 + 32);
        uint8 const* raw = packet->data();
        for (size_t i = 0; i < len; ++i)
        {
            if (i > 0 && i % 32 == 0)
                result += "\n  ";
            else if (i > 0)
                result += ' ';
            result += fmt::format("{:02X}", raw[i]);
        }
        if (len < packet->size())
            result += fmt::format(" ...({} more)", packet->size() - len);
        return result;
    }

    // Recurring event that sends housing WorldState counters every ~300ms.
    // Sniff-verified: 5 continuous counters throughout the entire housing map session.
    // Counters 1-3 (13436/13437/13438) increment by ~1333 each tick.
    // Counters 4-5 (16035/16711) increment by ~7233 each tick.
    class HousingWorldStateCounterEvent : public BasicEvent
    {
    public:
        HousingWorldStateCounterEvent(ObjectGuid playerGuid,
            uint32 counter1, uint32 counter2, uint32 counter3,
            uint32 counter4, uint32 counter5)
            : _playerGuid(playerGuid)
            , _counter1(counter1), _counter2(counter2), _counter3(counter3)
            , _counter4(counter4), _counter5(counter5) { }

        bool Execute(uint64 /*e_time*/, uint32 /*p_time*/) override
        {
            Player* player = ObjectAccessor::FindPlayer(_playerGuid);
            if (!player || !player->IsInWorld())
                return true; // delete event — player gone

            // Send all five counter WorldState updates
            player->SendUpdateWorldState(WORLDSTATE_HOUSING_COUNTER_1, _counter1);
            player->SendUpdateWorldState(WORLDSTATE_HOUSING_COUNTER_2, _counter2);
            player->SendUpdateWorldState(WORLDSTATE_HOUSING_COUNTER_3, _counter3);
            player->SendUpdateWorldState(WORLDSTATE_HOUSING_COUNTER_4, _counter4);
            player->SendUpdateWorldState(WORLDSTATE_HOUSING_COUNTER_5, _counter5);

            // Increment for next tick (different rates per sniff)
            _counter1 += HOUSING_WORLDSTATE_INCREMENT;
            _counter2 += HOUSING_WORLDSTATE_INCREMENT;
            _counter3 += HOUSING_WORLDSTATE_INCREMENT;
            _counter4 += HOUSING_WORLDSTATE_INCREMENT_2;
            _counter5 += HOUSING_WORLDSTATE_INCREMENT_2;

            // Re-schedule self for next tick
            player->m_Events.AddEventAtOffset(
                new HousingWorldStateCounterEvent(_playerGuid,
                    _counter1, _counter2, _counter3, _counter4, _counter5),
                Milliseconds(HOUSING_WORLDSTATE_INTERVAL_MS));

            return true; // delete this instance (new one scheduled)
        }

    private:
        ObjectGuid _playerGuid;
        uint32 _counter1;
        uint32 _counter2;
        uint32 _counter3;
        uint32 _counter4;
        uint32 _counter5;
    };
}

HousingMap::HousingMap(uint32 id, time_t expiry, uint32 instanceId, Difficulty spawnMode, uint32 neighborhoodId)
    : Map(id, expiry, instanceId, spawnMode), _neighborhoodId(neighborhoodId), _neighborhood(nullptr)
{
    // Prevent the map from being unloaded — housing maps are persistent
    // Map::CanUnload() returns false when m_unloadTimer == 0
    m_unloadTimer = 0;
    HousingMap::InitVisibilityDistance();

    // Verify InstanceType is MAP_HOUSE_NEIGHBORHOOD (8).
    // The client's "Airlock" system sets field_32=2 ONLY when InstanceType==8.
    // Without this, IsInsidePlot() always returns false → OutsidePlotBounds on ALL decor placement.
    if (GetEntry()->InstanceType != MAP_HOUSE_NEIGHBORHOOD)
    {
        TC_LOG_ERROR("housing", "CRITICAL: HousingMap {} '{}' has InstanceType={}, expected {} (MAP_HOUSE_NEIGHBORHOOD). "
            "Client will NOT allow decor placement — OutsidePlotBounds will always fire!",
            id, GetEntry()->MapName[sWorld->GetDefaultDbcLocale()],
            GetEntry()->InstanceType, MAP_HOUSE_NEIGHBORHOOD);
    }
    else
    {
        TC_LOG_DEBUG("housing", "HousingMap::ctor: mapId={} neighborhoodId={} instanceId={} "
            "InstanceType={} (MAP_HOUSE_NEIGHBORHOOD) — OK",
            id, neighborhoodId, instanceId, GetEntry()->InstanceType);
    }
}

HousingMap::~HousingMap()
{
    _playerHousings.clear();
}

void HousingMap::InitVisibilityDistance()
{
    // Audit 2026-04-21 measured ~20× over-emission vs retail when this map used
    // MAX_VISIBILITY_DISTANCE (533y) — every player received CREATE_OBJECT for
    // every decor / mesh / fixture on the map regardless of where they stood.
    // Retail uses a bounded distance and relies on the client's entity registry
    // already holding the persistent infra entities (plot ATs, cornerstone GOs,
    // room identities) at any distance. We mirror that: the persistent ones are
    // marked setActive(true) + SetFarVisible(true) at spawn time (see
    // SpawnPlotGameObjects + HousingRoomEntity::Create), so their CREATEs always
    // reach the player; everything else (decor, fixtures, component meshes)
    // streams via grid visibility once the player is within 200y. 200y is wide
    // enough that adjacent plots remain visible while keeping per-player update
    // traffic bounded.
    m_VisibleDistance = 200.0f;
    m_VisibilityNotifyPeriod = sWorld->getIntConfig(CONFIG_VISIBILITY_NOTIFY_PERIOD_INSTANCE);
}

void HousingMap::LoadGridObjects(NGridType* grid, Cell const& cell)
{
    Map::LoadGridObjects(grid, cell);
}

void HousingMap::SpawnPlotGameObjects()
{
    if (!_neighborhood)
    {
        TC_LOG_ERROR("housing", "HousingMap::SpawnPlotGameObjects: _neighborhood is NULL for map {} instanceId {} neighborhoodId {}",
            GetId(), GetInstanceId(), _neighborhoodId);
        return;
    }

    uint32 neighborhoodMapId = _neighborhood->GetNeighborhoodMapID();
    std::vector<NeighborhoodPlotData const*> plots = sHousingMgr.GetPlotsForMap(neighborhoodMapId);

    TC_LOG_INFO("housing", "HousingMap::SpawnPlotGameObjects: map={} instanceId={} neighborhoodMapId={} plotCount={}",
        GetId(), GetInstanceId(), neighborhoodMapId, uint32(plots.size()));

    if (plots.empty())
    {
        TC_LOG_ERROR("housing", "HousingMap::SpawnPlotGameObjects: NO plots found for neighborhoodMapId={} (neighborhood='{}') - check DB2 NeighborhoodPlot data",
            neighborhoodMapId, _neighborhood->GetName());
        return;
    }

    uint32 goCount = 0;
    uint32 noEntryCount = 0;

    TC_LOG_DEBUG("housing", "HousingMap::SpawnPlotGameObjects: DB2 PlotIndex→GOEntry mapping for {} plots on map {}:",
        uint32(plots.size()), neighborhoodMapId);
    for (NeighborhoodPlotData const* plot : plots)
    {
        TC_LOG_DEBUG("housing", "  DB2 ID={} PlotIndex={} CornerstoneGOEntry={} Cost={} WorldState={} HousePos=({:.1f},{:.1f},{:.1f})",
            plot->ID, plot->PlotIndex, plot->CornerstoneGameObjectID, plot->Cost, plot->WorldState,
            plot->HousePosition[0], plot->HousePosition[1], plot->HousePosition[2]);
    }

    for (NeighborhoodPlotData const* plot : plots)
    {
        float x = plot->CornerstonePosition[0];
        float y = plot->CornerstonePosition[1];
        float z = plot->CornerstonePosition[2];

        // Ensure the grid at this position is loaded so we can add GOs
        LoadGrid(x, y);

        // Retail uses a UNIQUE CornerstoneGameObjectID per plot so the server can
        // identify which plot a player interacted with.  All share type=48, displayId=110660.
        // Ownership state via GOState: 0 (ACTIVE) = Owned/Claimed, 1 (READY) = ForSale sign.
        Neighborhood::PlotInfo const* plotInfo = _neighborhood->GetPlotInfo(static_cast<uint8>(plot->PlotIndex));
        uint32 goEntry = static_cast<uint32>(plot->CornerstoneGameObjectID);
        bool isOwned = plotInfo && !plotInfo->OwnerGuid.IsEmpty();

        TC_LOG_DEBUG("housing", "HousingMap::SpawnPlotGameObjects: Plot {} at ({:.1f}, {:.1f}, {:.1f}) -> goEntry={} (Cornerstone={}, owned={})",
            plot->PlotIndex, x, y, z, goEntry, plot->CornerstoneGameObjectID,
            isOwned ? "yes" : "no");

        if (!goEntry)
        {
            TC_LOG_ERROR("housing", "HousingMap::SpawnPlotGameObjects: Plot {} has CornerstoneGameObjectID=0 - skipping",
                plot->PlotIndex);
            ++noEntryCount;
            continue;
        }

        // Build rotation from the stored euler angles
        float rotZ = plot->CornerstoneRotation[2];
        QuaternionData rot = QuaternionData::fromEulerAnglesZYX(rotZ, plot->CornerstoneRotation[1], plot->CornerstoneRotation[0]);

        // GOState 0 (ACTIVE) = Owned/Claimed cornerstone, GOState 1 (READY) = ForSale sign
        GOState plotState = isOwned ? GO_STATE_ACTIVE : GO_STATE_READY;

        Position pos(x, y, z, rotZ);
        GameObject* go = GameObject::CreateGameObject(goEntry, this, pos, rot, 255, plotState);
        if (!go)
        {
            TC_LOG_ERROR("housing", "HousingMap::SpawnPlotGameObjects: Failed to create GO entry {} at ({}, {}, {}) for plot {} in neighborhood '{}'",
                goEntry, x, y, z, plot->PlotIndex, _neighborhood->GetName());
            continue;
        }

        // Retail sniff: all Cornerstone GOs have Flags=32 (GO_FLAG_NODESPAWN)
        go->SetFlag(GO_FLAG_NODESPAWN);

        // Housing objects are dynamically spawned (no DB spawn record), so they have no
        // phase_area association. Explicitly mark them as universally visible so they're
        // seen by players regardless of what phases the player has from area-based phasing.
        PhasingHandler::InitDbPhaseShift(go->GetPhaseShift(), PHASE_USE_FLAGS_ALWAYS_VISIBLE, 0, 0);

        // Populate the FJamHousingCornerstone_C entity fragment so the client
        // knows this is a Cornerstone and can render the "For Sale" / owned UI
        go->InitHousingCornerstoneData(plot->Cost, static_cast<int32>(plot->PlotIndex));

        if (!AddToMap(go))
        {
            delete go;
            TC_LOG_ERROR("housing", "HousingMap::SpawnPlotGameObjects: Failed to add GO entry {} to map for plot {} in neighborhood '{}'",
                goEntry, plot->PlotIndex, _neighborhood->GetName());
            continue;
        }

        // Always keep cornerstone GOs streamed to every player on the map regardless of
        // distance — the house finder UI, OPEN_CORNERSTONE_UI, and the world-map icon
        // resolver all rely on the GO being in the entity registry. Without this, dropping
        // the map's visibility distance below MAX would hide cornerstones at far plots
        // and break the finder/buy flow when players are anywhere except next to the GO.
        go->setActive(true);
        go->SetFarVisible(true);

        // Track the plot GO for later swap (purchase/eviction)
        _plotGameObjects[static_cast<uint8>(plot->PlotIndex)] = go->GetGUID();

        TC_LOG_DEBUG("housing", "HousingMap::SpawnPlotGameObjects: Plot {} GO entry={} displayId={} type={} name='{}' guid={}",
            plot->PlotIndex, goEntry, go->GetGOInfo()->displayId, go->GetGOInfo()->type,
            go->GetGOInfo()->name, go->GetGUID().ToString());

        ++goCount;

        // Spawn plot AreaTrigger (entry 37358) at the house position (plot center).
        // Sniff-verified: Box shape 35x30x94, DecalPropertiesId=621 (plot boundary visual),
        // SpellForVisuals=1282351, FHousingPlotAreaTrigger_C entity fragment with owner data.
        // The AT is required for the client to show the edit menu and plot boundary decal.
        {
            float hx = plot->HousePosition[0];
            float hy = plot->HousePosition[1];
            float hz = plot->HousePosition[2];

            // Compute facing toward cornerstone (DB2 HouseRotation is (0,0,0) for all plots)
            float hFacing = plot->HouseRotation[2];
            if (plot->HouseRotation[0] == 0.0f && plot->HouseRotation[1] == 0.0f && plot->HouseRotation[2] == 0.0f)
                hFacing = std::atan2(plot->CornerstonePosition[1] - hy, plot->CornerstonePosition[0] - hx);

            LoadGrid(hx, hy);

            Position atPos(hx, hy, hz, hFacing);
            // Create with addToMap=false so we can set up ALL housing data (entity
            // fragment, SpellForVisuals, SpellXSpellVisualID) BEFORE the CREATE_OBJECT
            // packet is sent. The client needs FHousingPlotAreaTrigger_C and
            // DecalPropertiesId=621 in the initial create to render the plot border decal.
            AreaTrigger* plotAt = AreaTrigger::CreateStaticAreaTrigger({ .Id = 37358, .IsCustom = false }, this, atPos, -1, false);
            if (plotAt)
            {
                PhasingHandler::InitDbPhaseShift(plotAt->GetPhaseShift(), PHASE_USE_FLAGS_ALWAYS_VISIBLE, 0, 0);

                TC_LOG_INFO("housing", "  PlotAT[{}] spawn (plotInfo={} hasOwner={})",
                    plot->PlotIndex,
                    plotInfo ? "present" : "null",
                    plotInfo ? !plotInfo->OwnerGuid.IsEmpty() : false);

                // 12.0.5: no per-AT housing fragment. Only set the AT's own visual fields
                // (SpellForVisuals, PeriodModifier, ExtraScaleCurve). Plot ownership is
                // now propagated via PlayerHouseInfoComponentData.CurrentHouse on the Player.
                plotAt->InitHousingPlotVisuals();

                if (!AddToMap(plotAt))
                {
                    TC_LOG_ERROR("housing", "HousingMap::SpawnPlotGameObjects: AddToMap failed for plot AT (entry 37358) plot {} at ({:.1f},{:.1f},{:.1f})",
                        plot->PlotIndex, hx, hy, hz);
                    delete plotAt;
                    plotAt = nullptr;
                }

                if (plotAt)
                {
                    // Always keep plot ATs streamed regardless of player distance. The
                    // ENTER_PLOT / IsInsidePlot path looks up the AT in the client's
                    // entity registry; if the AT is not streamed, IsInsidePlot returns
                    // false and decor placement / plot ownership UI breaks. With the
                    // active flag + far visibility, the AT is in the registry for every
                    // player on the map, so we can safely drop the map's visibility
                    // distance below MAX without re-introducing the lookup-fail bug.
                    plotAt->setActive(true);
                    plotAt->SetFarVisible(true);

                    _plotAreaTriggers[static_cast<uint8>(plot->PlotIndex)] = plotAt->GetGUID();

                    std::string ownerDesc = (plotInfo && !plotInfo->OwnerGuid.IsEmpty())
                        ? plotInfo->OwnerGuid.ToString() : std::string("none");
                    TC_LOG_DEBUG("housing", "HousingMap::SpawnPlotGameObjects: Plot {} AT entry=37358 guid={} at ({:.1f},{:.1f},{:.1f}) owner={} DecalPropertiesID=621",
                        plot->PlotIndex, plotAt->GetGUID().ToString(), hx, hy, hz, ownerDesc);
                }
            }
            else
            {
                TC_LOG_ERROR("housing", "HousingMap::SpawnPlotGameObjects: Failed to create plot AT (entry 37358) for plot {} at ({:.1f},{:.1f},{:.1f})",
                    plot->PlotIndex, hx, hy, hz);
            }
        }
    }

    // Blizzlike: the per-plot WorldState from NeighborhoodPlot.db2 is a BINARY occupancy flag
    // that retail ships inside SMSG_INIT_WORLD_STATES (confirmed via sniff dump_12.0.1.66838_2026-04-15:
    //   - interior map 2783: all 55 plot worldstates = 0 (nothing yet visible)
    //   - exterior map 2735: occupied plots = 1, empty plots = 0
    // The VALUE is not the owner-type enum — it's purely "is a house here?". Owner identity is
    // carried in the plot AT (FHousingPlotAreaTrigger_C) and NeighborhoodMirrorData.Houses,
    // both of which are populated elsewhere; the client correlates plot icon -> AT -> owner
    // when you hover on the map.
    //
    // Setting the values through Map::SetWorldStateValue() at spawn time lands them in
    // _worldStateValues, so the next SMSG_INIT_WORLD_STATES already carries them. This
    // replaces the per-player SMSG_UPDATE_WORLD_STATE spam in SendPerPlayerPlotWorldStates()
    // with the correct channel.
    uint32 occupiedWs = 0;
    uint32 emptyWs = 0;
    for (NeighborhoodPlotData const* plot : plots)
    {
        uint8 plotIdx = static_cast<uint8>(plot->PlotIndex);
        // Prefer the DB2 WorldState ID when present; otherwise synthesise one
        // (DB2 extraction currently has this column zero for all plots).
        uint32 wsId = plot->WorldState != 0 ? plot->WorldState
                        : MakeHousingPlotWorldStateId(neighborhoodMapId, plotIdx);
        Neighborhood::PlotInfo const* pi = _neighborhood->GetPlotInfo(plotIdx);
        bool occupied = pi && pi->IsOccupied() && !pi->HouseGuid.IsEmpty();
        SetWorldStateValue(wsId, occupied ? 1 : 0, /*hidden*/ false);
        TC_LOG_INFO("housing", "  PlotWS[{}] WorldState={}{} value={} (owner={} house={})",
            plot->PlotIndex, wsId, plot->WorldState == 0 ? " [synth]" : "",
            occupied ? 1 : 0,
            pi ? pi->OwnerGuid.ToString() : "n/a",
            pi ? pi->HouseGuid.ToString() : "n/a");
        if (occupied) ++occupiedWs; else ++emptyWs;
    }

    TC_LOG_INFO("housing", "HousingMap::SpawnPlotGameObjects: Spawned {} GOs, {} plots occupied / {} empty "
        "(worldstate binary) for {} plots in neighborhood '{}' (noEntry={})",
        goCount, occupiedWs, emptyWs, uint32(plots.size()), _neighborhood->GetName(), noEntryCount);

    // Spawn house structure GOs for owned plots
    uint32 houseCount = 0;
    uint32 houseSuccessCount = 0;
    for (NeighborhoodPlotData const* plot : plots)
    {
        uint8 plotIdx = static_cast<uint8>(plot->PlotIndex);
        Neighborhood::PlotInfo const* plotInfo = _neighborhood->GetPlotInfo(plotIdx);
        if (!plotInfo || plotInfo->OwnerGuid.IsEmpty())
            continue;

        TC_LOG_DEBUG("housing", "HousingMap::SpawnPlotGameObjects: Plot {} is owned by {} - attempting house spawn (HousePos: {:.1f}, {:.1f}, {:.1f})",
            plotIdx, plotInfo->OwnerGuid.ToString(),
            plot->HousePosition[0], plot->HousePosition[1], plot->HousePosition[2]);

        // Spawn data comes from the DB via Neighborhood::LoadFromDB regardless of
        // whether the plot's owner is currently online. The live `Housing*` is
        // only used for fields that Housing computes at runtime (custom position,
        // root-type overrides derived from fixture selection). When null, we fall
        // back to PlotInfo fields mirrored from the DB.
        Housing* housing = GetHousingForPlayer(plotInfo->OwnerGuid);

        int32 exteriorComponentID = 0;
        int32 houseExteriorWmoDataID = 0;
        FixtureOverrideMap fixtureOverrides;
        RootOverrideMap rootOverrides;

        if (housing)
        {
            exteriorComponentID = static_cast<int32>(housing->GetCoreExteriorComponentID());
            houseExteriorWmoDataID = static_cast<int32>(housing->GetHouseType());
            fixtureOverrides = housing->GetFixtureOverrideMap();
            rootOverrides = housing->GetRootComponentOverrides();
        }
        else if (plotInfo->HouseType != 0)
        {
            houseExteriorWmoDataID = static_cast<int32>(plotInfo->HouseType);

            // Apply the mirrored fixture overrides from PlotInfo.Fixtures. This
            // mirrors character_housing_fixtures at Neighborhood::LoadFromDB so
            // visitors see each neighbour's customised roof/doors/windows, not
            // the raw default.
            for (auto const& [pointId, optionId] : plotInfo->Fixtures)
                fixtureOverrides[pointId] = optionId;

            // Pick the core fixture for the house. Prefer an override with OptionId==0
            // (player-selected base, same rule Housing::GetCoreExteriorComponentID uses);
            // fall back to DB2 IsDefault, then the first root entry.
            for (auto const& [pointId, optionId] : plotInfo->Fixtures)
            {
                if (optionId != 0)
                    continue;
                ExteriorComponentEntry const* comp = sExteriorComponentStore.LookupEntry(pointId);
                if (comp && comp->ParentComponentID == 0 && comp->HouseExteriorWmoDataID == static_cast<int32>(plotInfo->HouseType))
                {
                    exteriorComponentID = static_cast<int32>(pointId);
                    break;
                }
            }
            if (!exteriorComponentID)
            {
                auto const* roots = sHousingMgr.GetRootComponentsForWmoData(plotInfo->HouseType);
                if (roots)
                {
                    uint32 fallbackComp = 0;
                    for (uint32 compID : *roots)
                    {
                        ExteriorComponentEntry const* comp = sExteriorComponentStore.LookupEntry(compID);
                        if (!comp)
                            continue;
                        if (!fallbackComp)
                            fallbackComp = compID;
                        if (comp->Flags & 0x1) // IsDefault
                        {
                            fallbackComp = compID;
                            break;
                        }
                    }
                    exteriorComponentID = static_cast<int32>(fallbackComp);
                }
            }

            TC_LOG_INFO("housing", "HousingMap::SpawnPlotGameObjects: Plot {} owner {} (offline) — using mirrored HouseType={} ExtComp={} fixtures={} decor={} from PlotInfo",
                plotIdx, plotInfo->OwnerGuid.ToString(), houseExteriorWmoDataID, exteriorComponentID,
                uint32(plotInfo->Fixtures.size()), uint32(plotInfo->Decor.size()));
        }
        else
        {
            TC_LOG_ERROR("housing", "HousingMap::SpawnPlotGameObjects: Plot {} owned by {} but PlotInfo.HouseType=0 — cannot spawn house",
                plotIdx, plotInfo->OwnerGuid.ToString());
            continue;
        }

        if (!exteriorComponentID || !houseExteriorWmoDataID)
        {
            TC_LOG_ERROR("housing", "HousingMap::SpawnPlotGameObjects: Plot {} has invalid data: ExteriorComponentID={}, WmoDataID={}",
                plotIdx, exteriorComponentID, houseExteriorWmoDataID);
            continue;
        }
        TC_LOG_DEBUG("housing", "HousingMap::SpawnPlotGameObjects: Plot {} using ExteriorComponentID={}, WmoDataID={}",
            plotIdx, exteriorComponentID, houseExteriorWmoDataID);

        FixtureOverrideMap const* overridesPtr = fixtureOverrides.empty() ? nullptr : &fixtureOverrides;
        RootOverrideMap const* rootOvrPtr = rootOverrides.empty() ? nullptr : &rootOverrides;

        GameObject* houseGo = nullptr;
        if (housing && housing->HasCustomPosition())
        {
            Position customPos = housing->GetHousePosition();
            houseGo = SpawnHouseForPlot(plotIdx, &customPos, exteriorComponentID, houseExteriorWmoDataID, overridesPtr, rootOvrPtr);
        }
        else
        {
            houseGo = SpawnHouseForPlot(plotIdx, nullptr, exteriorComponentID, houseExteriorWmoDataID, overridesPtr, rootOvrPtr);
        }
        ++houseCount;
        if (houseGo)
            ++houseSuccessCount;
        else
            TC_LOG_ERROR("housing", "HousingMap::SpawnPlotGameObjects: FAILED to spawn house for plot {} owned by {}",
                plotIdx, plotInfo->OwnerGuid.ToString());

        // Spawn placed decor — live Housing path preferred (may include items
        // placed but not yet saved). Otherwise iterate PlotInfo.Decor from DB.
        if (housing)
        {
            SpawnAllDecorForPlot(plotIdx, housing);
        }
        else
        {
            uint32 spawnedDecor = 0;
            for (Housing::PlacedDecor const& decor : plotInfo->Decor)
            {
                if (!decor.RoomGuid.IsEmpty())
                    continue; // exterior-only at preload
                if (SpawnDecorItem(plotIdx, decor, plotInfo->HouseGuid))
                    ++spawnedDecor;
            }
            _decorSpawnedPlots.insert(plotIdx);
            if (spawnedDecor || !plotInfo->Decor.empty())
                TC_LOG_INFO("housing", "HousingMap::SpawnPlotGameObjects: Plot {} (offline owner) — spawned {} exterior decor from PlotInfo ({} total decor entries cached)",
                    plotIdx, spawnedDecor, uint32(plotInfo->Decor.size()));
        }
    }

    TC_LOG_DEBUG("housing", "HousingMap::SpawnPlotGameObjects: House spawn results: {}/{} successful for neighborhood '{}'",
        houseSuccessCount, houseCount, _neighborhood->GetName());
}

void HousingMap::LockPlotGrids()
{
    if (!_neighborhood)
        return;

    uint32 neighborhoodMapId = _neighborhood->GetNeighborhoodMapID();
    std::vector<NeighborhoodPlotData const*> plots = sHousingMgr.GetPlotsForMap(neighborhoodMapId);
    std::set<std::pair<uint32, uint32>> lockedGrids;

    for (NeighborhoodPlotData const* plot : plots)
    {
        // Lock grid for cornerstone position
        GridCoord cornerstoneGrid = Trinity::ComputeGridCoord(plot->CornerstonePosition[0], plot->CornerstonePosition[1]);
        if (lockedGrids.insert({ cornerstoneGrid.x_coord, cornerstoneGrid.y_coord }).second)
            GridMarkNoUnload(cornerstoneGrid.x_coord, cornerstoneGrid.y_coord);

        // Lock grid for house position (may be a different grid)
        GridCoord houseGrid = Trinity::ComputeGridCoord(plot->HousePosition[0], plot->HousePosition[1]);
        if (lockedGrids.insert({ houseGrid.x_coord, houseGrid.y_coord }).second)
            GridMarkNoUnload(houseGrid.x_coord, houseGrid.y_coord);
    }

    TC_LOG_DEBUG("housing", "HousingMap::LockPlotGrids: Locked {} grids for {} plots in neighborhood '{}'",
        lockedGrids.size(), plots.size(), _neighborhood->GetName());
}

AreaTrigger* HousingMap::GetPlotAreaTrigger(uint8 plotIndex)
{
    auto itr = _plotAreaTriggers.find(plotIndex);
    if (itr == _plotAreaTriggers.end())
        return nullptr;

    return GetAreaTrigger(itr->second);
}

int8 HousingMap::GetPlotIndexForAreaTrigger(ObjectGuid atGuid) const
{
    for (auto const& [plotIdx, guid] : _plotAreaTriggers)
        if (guid == atGuid)
            return static_cast<int8>(plotIdx);
    return -1;
}

GameObject* HousingMap::GetPlotGameObject(uint8 plotIndex)
{
    auto itr = _plotGameObjects.find(plotIndex);
    if (itr == _plotGameObjects.end())
        return nullptr;

    return GetGameObject(itr->second);
}

void HousingMap::SetPlotOwnershipState(uint8 plotIndex, bool owned)
{
    if (!_neighborhood)
        return;

    // Toggle GOState on the existing Cornerstone GO.
    // GOState 0 (ACTIVE) = Owned/Claimed cornerstone, GOState 1 (READY) = ForSale sign
    GOState newState = owned ? GO_STATE_ACTIVE : GO_STATE_READY;

    auto itr = _plotGameObjects.find(plotIndex);
    if (itr != _plotGameObjects.end())
    {
        if (GameObject* go = GetGameObject(itr->second))
        {
            go->SetGoState(newState);

            TC_LOG_DEBUG("housing", "HousingMap::SetPlotOwnershipState: Plot {} GOState -> {} ({}) in neighborhood '{}'",
                plotIndex, uint32(newState), owned ? "owned" : "for-sale", _neighborhood->GetName());
        }
        else
        {
            TC_LOG_ERROR("housing", "HousingMap::SetPlotOwnershipState: Plot {} GO guid {} not found on map in neighborhood '{}'",
                plotIndex, itr->second.ToString(), _neighborhood->GetName());
        }
    }
    else
    {
        TC_LOG_ERROR("housing", "HousingMap::SetPlotOwnershipState: Plot {} has no tracked GO in neighborhood '{}'",
            plotIndex, _neighborhood->GetName());
    }

    // 12.0.5: per-AT ownership fragment removed. Plot ownership is communicated
    // via PlayerHouseInfoComponentData.CurrentHouse on each Player — updated by
    // the enter/leave-plot code paths (see HousingMap::OnPlayerEnterPlotArea).

    // Blizzlike: the worldstate value is a per-player OWNER TYPE enum, not a binary
    // flag. Update the map-scoped default (0/1) so late joiners see reasonable INIT
    // state, then send a personalized UPDATE to every player already on the map so
    // the CURRENT state (owner relationship) renders the right icon.
    uint32 neighborhoodMapId = _neighborhood->GetNeighborhoodMapID();
    std::vector<NeighborhoodPlotData const*> plots = sHousingMgr.GetPlotsForMap(neighborhoodMapId);

    for (NeighborhoodPlotData const* plotData : plots)
    {
        if (plotData->PlotIndex != static_cast<int32>(plotIndex))
            continue;

        // Prefer the DB2 WorldState ID; synthesise one when the extraction has it zero.
        uint32 wsId = plotData->WorldState != 0 ? plotData->WorldState
                        : MakeHousingPlotWorldStateId(neighborhoodMapId, plotIndex);

        // Blizzlike: the per-plot WorldState is a BINARY occupancy flag — 0 empty,
        // 1 occupied. `Map::SetWorldStateValue` stores the value (so future joins
        // get it in INIT_WORLD_STATES) AND broadcasts `SMSG_UPDATE_WORLD_STATE` to
        // every player currently on the map. No per-player enum override.
        SetWorldStateValue(wsId, owned ? 1 : 0, /*hidden*/ false);

        TC_LOG_DEBUG("housing", "SetPlotOwnershipState: ws={}{} value={} plot={} {} neighborhoodMap={}",
            wsId, plotData->WorldState == 0 ? " [synth]" : "",
            owned ? 1 : 0, plotIndex, owned ? "occupied" : "empty", neighborhoodMapId);
        break;
    }
}

Housing* HousingMap::GetHousingForPlayer(ObjectGuid playerGuid) const
{
    auto itr = _playerHousings.find(playerGuid);
    if (itr != _playerHousings.end())
        return itr->second;

    return nullptr;
}

void HousingMap::LoadNeighborhoodData()
{
    ObjectGuid neighborhoodGuid = ObjectGuid::Create<HighGuid::Housing>(/*subType*/ 4, /*arg1*/ sRealmList->GetCurrentRealmId().Realm, /*arg2*/ 0, static_cast<uint64>(_neighborhoodId));
    _neighborhood = sNeighborhoodMgr.GetNeighborhood(neighborhoodGuid);

    if (!_neighborhood)
        TC_LOG_ERROR("housing", "HousingMap::LoadNeighborhoodData: Failed to load neighborhood {} for map {} instanceId {}",
            _neighborhoodId, GetId(), GetInstanceId());
    else
        TC_LOG_DEBUG("housing", "HousingMap::LoadNeighborhoodData: Loaded neighborhood '{}' (id: {}) for map {} instanceId {}",
            _neighborhood->GetName(), _neighborhoodId, GetId(), GetInstanceId());
}

bool HousingMap::AddPlayerToMap(Player* player, bool initPlayer /*= true*/)
{
    if (!_neighborhood)
    {
        TC_LOG_ERROR("housing", "HousingMap::AddPlayerToMap: No neighborhood loaded for map {} instanceId {}",
            GetId(), GetInstanceId());
        return false;
    }

    // Enforce max players on housing map
    if (GetPlayersCountExceptGMs() >= MAX_HOUSING_MAP_PLAYERS)
    {
        TC_LOG_DEBUG("housing", "HousingMap::AddPlayerToMap: Map {} full ({} players), rejecting player {}",
            GetId(), GetPlayersCountExceptGMs(), player->GetGUID().ToString());
        player->SendTransferAborted(GetId(), TRANSFER_ABORT_HOUSING_MAX_PLAYERS_IN_HOUSE);
        return false;
    }

    // Do NOT auto-add the player as a neighborhood member here.
    // Membership is granted when the player buys a plot or is invited.
    // Auto-adding causes the client to resolve neighborhoodOwnerType as
    // Self instead of None, which prevents the "For Sale" Cornerstone UI.

    // Track player housing if they own a house in this neighborhood.
    // First try exact GUID match, then fall back to checking all housings
    // (handles legacy data where neighborhood GUID counter was from client's DB2 ID
    // instead of the server's canonical counter).
    TC_LOG_DEBUG("housing", "HousingMap::AddPlayerToMap: Looking up housing for player {} in neighborhood '{}' (guid={})",
        player->GetGUID().ToString(), _neighborhood->GetName(), _neighborhood->GetGuid().ToString());

    Housing* housing = player->GetHousingForNeighborhood(_neighborhood->GetGuid());
    if (!housing)
    {
        TC_LOG_DEBUG("housing", "HousingMap::AddPlayerToMap: No housing found via GetHousingForNeighborhood. Checking fallback (allHousings count: {})",
            uint32(player->GetAllHousings().size()));

        // Fallback: check if any of the player's housings has a plot in this neighborhood
        for (Housing const* h : player->GetAllHousings())
        {
            TC_LOG_DEBUG("housing", "HousingMap::AddPlayerToMap: Fallback check: housing plotIndex={} neighborhoodGuid={} houseGuid={}",
                h ? h->GetPlotIndex() : 255,
                h ? h->GetNeighborhoodGuid().ToString() : "null",
                h ? h->GetHouseGuid().ToString() : "null");

            if (h && _neighborhood->GetPlotInfo(h->GetPlotIndex()))
            {
                housing = const_cast<Housing*>(h);
                TC_LOG_DEBUG("housing", "HousingMap::AddPlayerToMap: Fixed neighborhood GUID mismatch for player {} (stored={}, canonical={})",
                    player->GetGUID().ToString(), h->GetNeighborhoodGuid().ToString(), _neighborhood->GetGuid().ToString());
                // Fix the stored GUID so future lookups work
                housing->SetNeighborhoodGuid(_neighborhood->GetGuid());
                break;
            }
        }
    }

    if (housing)
    {
        TC_LOG_DEBUG("housing", "HousingMap::AddPlayerToMap: Player {} has housing: plotIndex={} houseType={} houseGuid={}",
            player->GetGUID().ToString(), housing->GetPlotIndex(), housing->GetHouseType(), housing->GetHouseGuid().ToString());

        AddPlayerHousing(player->GetGUID(), housing);

        // Ensure the neighborhood PlotInfo has the HouseGuid (may be missing after server restart
        // since LoadFromDB only populates it if character_housing row exists)
        uint8 plotIdx = housing->GetPlotIndex();
        ObjectGuid ownerBnetGuid = player->GetSession() ? player->GetSession()->GetBattlenetAccountGUID() : ObjectGuid::Empty;
        if (!housing->GetHouseGuid().IsEmpty())
        {
            _neighborhood->UpdatePlotHouseInfo(plotIdx, housing->GetHouseGuid(), ownerBnetGuid);

            // 12.0.5: per-AT ownership fragment removed. Ownership is propagated via
            // PlayerHouseInfoComponentData.CurrentHouse on the Player at plot entry.
        }

        // Update PlayerMirrorHouse.MapID so the client knows this house is on the current map.
        // Without this, MapID stays at 0 (set during login) and the client rejects
        // edit mode with HOUSING_RESULT_ACTION_LOCKED_BY_COMBAT (error 1 = first non-success code).
        player->UpdateHousingMapId(housing->GetHouseGuid(), static_cast<int32>(GetId()));

        // Spawn house GO if not already present (handles offline → online transition)
        bool alreadySpawned = _houseGameObjects.find(plotIdx) != _houseGameObjects.end();
        TC_LOG_DEBUG("housing", "HousingMap::AddPlayerToMap: House GO for plot {}: alreadySpawned={} _houseGameObjects.size={}",
            plotIdx, alreadySpawned, uint32(_houseGameObjects.size()));

        if (!alreadySpawned)
        {
            // Read exterior data from Housing — no hardcoded fallbacks
            int32 exteriorComponentID = static_cast<int32>(housing->GetCoreExteriorComponentID());
            int32 houseExteriorWmoDataID = static_cast<int32>(housing->GetHouseType());
            if (!exteriorComponentID || !houseExteriorWmoDataID)
            {
                TC_LOG_ERROR("housing", "HousingMap::AddPlayerToMap: Plot {} has invalid data: ExteriorComponentID={}, WmoDataID={} — skipping spawn",
                    plotIdx, exteriorComponentID, houseExteriorWmoDataID);
            }
            else
            {
            TC_LOG_DEBUG("housing", "HousingMap::AddPlayerToMap: Plot {} spawning house with ExteriorComponentID={}, WmoDataID={}",
                plotIdx, exteriorComponentID, houseExteriorWmoDataID);

            auto fixtureOverrides = housing->GetFixtureOverrideMap();
            FixtureOverrideMap const* overridesPtr = fixtureOverrides.empty() ? nullptr : &fixtureOverrides;
            auto rootOverrides = housing->GetRootComponentOverrides();
            RootOverrideMap const* rootOvrPtr = &rootOverrides;

            GameObject* go = nullptr;
            if (housing->HasCustomPosition())
            {
                Position customPos = housing->GetHousePosition();
                go = SpawnHouseForPlot(plotIdx, &customPos, exteriorComponentID, houseExteriorWmoDataID, overridesPtr, rootOvrPtr);
            }
            else
                go = SpawnHouseForPlot(plotIdx, nullptr, exteriorComponentID, houseExteriorWmoDataID, overridesPtr, rootOvrPtr);

            TC_LOG_DEBUG("housing", "HousingMap::AddPlayerToMap: SpawnHouseForPlot result for plot {}: {}",
                plotIdx, go ? "spawned" : "FAILED");
            } // else (valid exteriorComponentID && houseExteriorWmoDataID)
        }
        else
        {
            // House GO was spawned during map init (before HouseGuid was available).
            // Apply fixture data and spawn MeshObject if not yet done.
            bool hasMeshObjects = _meshObjects.find(plotIdx) != _meshObjects.end() && !_meshObjects[plotIdx].empty();
            if (!hasMeshObjects && !housing->GetHouseGuid().IsEmpty())
            {
                // Get the house GO position for the MeshObject
                if (GameObject* houseGo = GetHouseGameObject(plotIdx))
                {
                    // NOTE: Do NOT call InitHousingFixtureData on the GO.
                    // In retail, only MeshObjects carry FHousingFixture_C — attaching it to a GO
                    // causes a client crash at +0x64 (GO entity factory doesn't allocate a housing
                    // fixture component, so the GUID resolver returns null and dereferences it).

                    Position pos = houseGo->GetPosition();
                    QuaternionData rot = houseGo->GetLocalRotation();
                    int32 faction = _neighborhood ? _neighborhood->GetFactionRestriction()
                        : NEIGHBORHOOD_FACTION_ALLIANCE;

                    int32 lateExtCompID = static_cast<int32>(housing->GetCoreExteriorComponentID());
                    int32 lateWmoDataID = static_cast<int32>(housing->GetHouseType());

                    auto lateFixtureOvr = housing->GetFixtureOverrideMap();
                    FixtureOverrideMap const* lateFixturePtr = lateFixtureOvr.empty() ? nullptr : &lateFixtureOvr;
                    auto lateRootOvr = housing->GetRootComponentOverrides();
                    RootOverrideMap const* lateRootPtr = &lateRootOvr;

                    SpawnFullHouseMeshObjects(plotIdx, pos, rot,
                        housing->GetHouseGuid(), lateExtCompID, lateWmoDataID, faction,
                        lateFixturePtr, lateRootPtr);

                    // Also spawn room entity + Geobox if not already present
                    if (_roomEntities.find(plotIdx) == _roomEntities.end())
                        SpawnRoomForPlot(plotIdx, pos, rot, housing->GetHouseGuid());

                    TC_LOG_DEBUG("housing", "HousingMap::AddPlayerToMap: Late-spawned MeshObjects for plot {} (house GO {} already existed)",
                        plotIdx, houseGo->GetGUID().ToString());
                }
            }
        }

        // Spawn decor GOs if not already spawned for this plot
        SpawnAllDecorForPlot(plotIdx, housing);

        // The session HousingPlayerHouseEntity keeps EntityGUID=Empty as set
        // in Player::LoadFromDB. Sniff-verified (dump_12.0.1.66838_2026-04-15
        // _09-35-59 idx 9984): among 47 retail Housing/3 CREATE blocks, the
        // one with EntityGUID=Empty is the owner's own-plot block. Proxy
        // Housing/3 blocks for other plots use non-empty mirror-pattern GUIDs
        // (most of which dangle — the client doesn't need them to resolve).
        // The icon picker at sub_7FF624BB1880 classifies own vs friend vs
        // stranger by comparing FHousingPlayerHouse_C.BnetAccount against the
        // player's local BnetGuid, not via EntityGUID resolution.
    }
    else
    {
        TC_LOG_ERROR("housing", "HousingMap::AddPlayerToMap: Player {} has NO housing in this neighborhood (no house will spawn)",
            player->GetGUID().ToString());
    }

    if (!Map::AddPlayerToMap(player, initPlayer))
        return false;

    // Force immediate visibility update so all MeshObjects (house pieces, decor) get
    // CREATE_OBJECT sent to the player NOW, not deferred to the next map tick.
    // Map::AddPlayerToMap calls UpdateObjectVisibility(false) which only sets
    // NOTIFY_VISIBILITY_CHANGED — the actual grid visit is deferred until the next
    // relocation processing tick. Without forcing here, the client won't have MeshObject
    // entities when entering edit mode, causing an empty Placed Decor list.
    player->UpdateVisibilityForPlayer();

    // === DIAGNOSTIC: Report plot GO state when player enters ===
    {
        TC_LOG_DEBUG("housing", "=== HOUSING DIAGNOSTIC for player {} entering map {} ===", player->GetGUID().ToString(), GetId());
        TC_LOG_DEBUG("housing", "  Player position: ({:.1f}, {:.1f}, {:.1f})", player->GetPositionX(), player->GetPositionY(), player->GetPositionZ());
        TC_LOG_DEBUG("housing", "  _plotGameObjects.size={} Map InstanceType={} (expected {} for MAP_HOUSE_NEIGHBORHOOD)",
            uint32(_plotGameObjects.size()), GetEntry()->InstanceType, MAP_HOUSE_NEIGHBORHOOD);

        uint32 shown = 0;
        for (auto const& [plotIdx, goGuid] : _plotGameObjects)
        {
            if (shown >= 3) break;
            if (GameObject* go = GetGameObject(goGuid))
            {
                float dist = player->GetDistance(go);
                TC_LOG_DEBUG("housing", "  Plot[{}] GO: guid={} entry={} pos=({:.1f},{:.1f},{:.1f}) dist={:.1f}yd",
                    plotIdx, goGuid.ToString(), go->GetEntry(),
                    go->GetPositionX(), go->GetPositionY(), go->GetPositionZ(), dist);
            }
            ++shown;
        }
        TC_LOG_DEBUG("housing", "=== END HOUSING DIAGNOSTIC ===");
    }

    // BLIZZLIKE: no unprompted housing SMSG emissions at login.
    //
    // Previously we emitted SMSG_HOUSING_GET_CURRENT_HOUSE_INFO_RESPONSE,
    // SMSG_HOUSING_CATALOG_STATE_SYNC, SMSG_HOUSING_SVCS_GET_PLAYER_HOUSES_INFO_
    // RESPONSE, SMSG_HOUSING_SVCS_UPDATE_HOUSES_LEVEL_FAVOR, and
    // SMSG_INITIATIVE_SERVICE_STATUS here as "wake-ups" to prime the client.
    // Retail 66838 sniff analysis across 3 independent captures
    // (floorplan_editor_rotation 2026-04-10, wall_floor_ceiling_customize
    // 2026-04-12, interrior_exterrior_advanced_editor 2026-04-15) shows
    // ZERO unprompted housing-specific SMSGs in the post-LVW window. The
    // client receives all housing state via the Player CREATE bundle's
    // UpdateField data and asks for anything else via CMSGs. The existing
    // reactive handlers (HandleHousingGetCurrentHouseInfo, HandleHousingSvcs
    // GetPlayerHousesInfo, HandleHousingHouseStatus, etc.) will respond
    // when queried.
    //
    // CATALOG_STATE_SYNC at login has been removed too — retail emits it at
    // ~LVW+988 in the advanced_editor sniff, well after login, as a
    // time-delayed server push. Not at map-entry time. If the client
    // actively needs it before the delayed push, CMSG_HOUSING_DECOR_REQUEST_
    // STORAGE also triggers catalog dispatch via HandleHousingDecorRequestStorage.

    // ENTER_PLOT must be sent AFTER SMSG_UPDATE_OBJECT creates the AT on the client.
    // UPDATE_OBJECT is flushed after AddPlayerToMap returns, so sending ENTER_PLOT
    // here synchronously would reference an AT GUID the client doesn't know yet.
    // Solution: schedule a deferred event that sends ENTER_PLOT after a short delay,
    // giving the UPDATE_OBJECT time to flush to the client first.
    // The at_housing_plot AT overlap script also sends ENTER_PLOT when the player
    // physically enters the box, but on login the AT overlap check may not fire
    // on the first tick (player already inside the AT when it was created).
    // SetPlayerCurrentPlot is called here so the AT script's alreadyOnPlot guard
    // prevents duplicate ENTER_PLOT sends if both paths fire.
    if (housing)
    {
        uint8 plotIndex = housing->GetPlotIndex();
        SetPlayerCurrentPlot(player->GetGUID(), plotIndex);

        ObjectGuid playerGuid = player->GetGUID();
        ObjectGuid houseGuid = housing->GetHouseGuid();
        ObjectGuid neighborhoodGuid = housing->GetNeighborhoodGuid();
        uint8 deferredPlotIndex = plotIndex;
        player->m_Events.AddEventAtOffset([playerGuid, deferredPlotIndex, houseGuid, neighborhoodGuid]()
        {
            Player* p = ObjectAccessor::FindPlayer(playerGuid);
            if (!p || !p->IsInWorld())
                return;

            HousingMap* hMap = dynamic_cast<HousingMap*>(p->GetMap());
            if (!hMap)
                return;

            AreaTrigger* plotAt = hMap->GetPlotAreaTrigger(deferredPlotIndex);
            if (!plotAt)
            {
                TC_LOG_ERROR("housing", "HousingMap deferred ENTER_PLOT: No AT for plot {} player {}",
                    deferredPlotIndex, playerGuid.ToString());
                return;
            }

            // Retail pattern: send VALUES update on AT (with FHousingPlotAreaTrigger_C data)
            // at the same timestamp as ENTER_PLOT, ensuring the client entity table has fresh data.
            {
                UpdateData atUpdate(p->GetMapId());
                if (p->HaveAtClient(plotAt))
                    plotAt->BuildValuesUpdateBlockForPlayer(&atUpdate, p);
                else
                {
                    plotAt->BuildCreateUpdateBlockForPlayer(&atUpdate, p);
                    p->m_clientGUIDs.insert(plotAt->GetGUID());
                }
                if (atUpdate.HasData())
                {
                    WorldPacket atPacket;
                    atUpdate.BuildPacket(&atPacket);
                    p->SendDirectMessage(&atPacket);
                }
            }

            // REMOVED proactive SMSG_NEIGHBORHOOD_PLAYER_ENTER_PLOT (0x5C0000)
            // and SMSG_HOUSING_FIXTURE_CREATE_BASIC_HOUSE_RESPONSE (0x520001).
            // Sniff set-diff of 3 retail login captures shows retail never
            // emits either opcode at login / during the deferred-map-entry
            // window; both are strictly reactive to specific user actions
            // (AT overlap entry, fixture-editor click). Keeping the
            // proactive sends here put the client's housing-state machine
            // into pre-initialised state that suppressed the world-map
            // icon-picker refresh. The at_housing_plot AT script still
            // emits PLAYER_ENTER_PLOT (and HouseStatus+Permissions) on
            // actual plot overlap, which matches retail.
            TC_LOG_DEBUG("housing", "HousingMap deferred ENTER_PLOT: proactive PLAYER_ENTER_PLOT + FIXTURE_CREATE_BASIC_HOUSE suppressed for player {}",
                playerGuid.ToString());

            // Re-CREATE ALL fixture MeshObjects for this plot AFTER the rebuild.
            // The rebuild (triggered by CREATE_BASIC_HOUSE_RESPONSE above) sets the
            // fixture manager's house GUID. The CREATE callback then compares each
            // entity's HouseGUID against it — if they match, it registers the entity
            // at its hook point. Without this re-CREATE, entities that were already
            // sent during the initial map load are never re-processed.
            // (Same pattern as the decor fix: re-CREATE after the system is ready.)
            {
                auto const& meshMap = hMap->GetPlotMeshObjects();
                auto meshItr = meshMap.find(deferredPlotIndex);
                if (meshItr != meshMap.end())
                {
                    UpdateData fixtureUpdate(p->GetMapId());
                    uint32 fixtureCreateCount = 0;

                    for (ObjectGuid const& meshGuid : meshItr->second)
                    {
                        MeshObject* meshObj = hMap->GetMeshObject(meshGuid);
                        if (meshObj && meshObj->IsInWorld() && meshObj->m_housingFixtureData.has_value())
                        {
                            meshObj->BuildCreateUpdateBlockForPlayer(&fixtureUpdate, p);
                            p->m_clientGUIDs.insert(meshGuid);
                            ++fixtureCreateCount;
                        }
                    }

                    if (fixtureCreateCount > 0)
                    {
                        WorldPacket fixturePacket;
                        fixtureUpdate.BuildPacket(&fixturePacket);
                        p->SendDirectMessage(&fixturePacket);
                    }

                    TC_LOG_DEBUG("housing", "HousingMap deferred ENTER_PLOT: Re-CREATE {} fixture MeshObjects for plot {} (post-rebuild)",
                        fixtureCreateCount, deferredPlotIndex);
                }
            }

            // Proactively populate FHousingStorage_C (decor list) and budget fields.
            // At login, PopulateCatalogStorageEntries() is NOT called to avoid crashes when
            // storage data appears in the initial Account entity CREATE.
            //
            // CRITICAL: Must send Account as CREATE (not VALUES_UPDATE). The initial login
            // Account CREATE had an empty FHousingStorage_C.Decor MapUpdateField.
            // A VALUES_UPDATE for a MapUpdateField that was empty at CREATE time does NOT
            // properly convey new entries to the client — the client never processes them.
            // Sending a second CREATE with all current data works because the client handles
            // re-CREATE for an existing entity gracefully (replaces the old data).
            //
            // Also bundle ALL decor MeshObject CREATEs in the SAME UPDATE_OBJECT packet.
            // The client correlates MeshObject FHousingDecor_C.DecorGUID with Account
            // FHousingStorage_C entries to build the Placed Decor list. If they arrive in
            // separate packets, the client may not retroactively associate them.
            if (Housing* housing = p->GetHousing())
            {
                housing->PopulateCatalogStorageEntries();
                housing->SyncUpdateFields();

                // 12.0.5: write the player's own HouseGuid to PlayerHouseInfoComponent.CurrentHouse
                // so the client's HOUSE_PLOT_ENTERED field-change callback fires from the same
                // UPDATE_OBJECT bundle. Without this the AT enter event is the only path that
                // sets it, but at login the player is already inside the AT box so OnUnitEnter
                // doesn't trigger; the editor menu then never arms.
                p->SetCurrentHouse(housing->GetHouseGuid());

                // Push HouseStatus + Permissions so the client's permissions cache is populated
                // for the player's own plot. Same reason as SetCurrentHouse above — at login
                // OnUnitEnter doesn't fire, so the AT script's proactive push at
                // at_housing_plot.cpp never runs and the permissions window won't open until
                // the player walks out of the AT and back in.
                {
                    WorldPackets::Housing::HousingHouseStatusResponse statusResponse;
                    statusResponse.HouseGuid = housing->GetHouseGuid();
                    statusResponse.AccountGuid = p->GetSession()->GetBattlenetAccountGUID();
                    statusResponse.OwnerPlayerGuid = p->GetGUID();
                    statusResponse.NeighborhoodGuid = housing->GetNeighborhoodGuid();
                    statusResponse.Status = 0;
                    statusResponse.PermissionFlags = 0xE0;
                    p->SendDirectMessage(statusResponse.Write());

                    WorldPackets::Housing::HousingGetPlayerPermissionsResponse permResponse;
                    permResponse.HouseGuid = housing->GetHouseGuid();
                    permResponse.ResultCode = 0;
                    permResponse.PermissionFlags = 0xE0;
                    p->SendDirectMessage(permResponse.Write());

                    TC_LOG_DEBUG("housing", "HousingMap deferred ENTER_PLOT: pushed HouseStatus+Permissions for owner {} flags=0xE0",
                        p->GetGUID().ToString());
                }

                WorldSession* session = p->GetSession();

                // Mimic the retail client's auto-sent CMSG_HOUSING_DECOR_REQUEST_STORAGE
                // response sequence. Horde sniff (dump_12.0.1.65940_2026-02-19_10-51-32,
                // pkt 8942 @ 17:53:30.294): client auto-emits the CMSG ~9s after login
                // once it has processed the housing UPDATE_OBJECT bundle. Retail server
                // responds with (1) 4-byte STORAGE_RSP ack, (2) Account + Housing/3 +
                // decor meshes in ONE UPDATE_OBJECT, (3) PLAYER_HOUSES_INFO_RESPONSE.
                //
                // Empirically on our server the client does NOT auto-send the CMSG
                // (the user's "click dashboard" workaround is what triggers this path
                // via HandleHousingDecorRequestStorage). Emitting the full sequence
                // server-side at the 500ms defer replays the same transport the client
                // expects, without requiring the CMSG round-trip.
                WorldPackets::Housing::HousingDecorRequestStorageResponse storageAck;
                storageAck.ResultCode = static_cast<uint8>(HOUSING_RESULT_SUCCESS);
                p->SendDirectMessage(storageAck.Write());

                UpdateData storageUpdate(p->GetMapId());
                WorldPacket storagePacket;

                // Account entity as CREATE (includes full FHousingStorage_C with Decor map)
                session->GetBattlenetAccount().BuildCreateUpdateBlockForPlayer(&storageUpdate, p);
                p->m_clientGUIDs.insert(session->GetBattlenetAccount().GetGUID());

                // HousingPlayerHouseEntity (budgets)
                if (p->HaveAtClient(&session->GetHousingPlayerHouseEntity()))
                    session->GetHousingPlayerHouseEntity().BuildValuesUpdateBlockForPlayer(&storageUpdate, p);
                else
                {
                    session->GetHousingPlayerHouseEntity().BuildCreateUpdateBlockForPlayer(&storageUpdate, p);
                    p->m_clientGUIDs.insert(session->GetHousingPlayerHouseEntity().GetGUID());
                }

                // Bundle ALL decor MeshObject CREATEs so the client can correlate
                // FHousingDecor_C.DecorGUID with FHousingStorage_C entries in one pass.
                uint32 meshCreateCount = 0;
                for (auto const& [decorGuid, meshObjGuid] : hMap->GetDecorGuidMap())
                {
                    MeshObject* meshObj = hMap->GetMeshObject(meshObjGuid);
                    if (!meshObj || !meshObj->IsInWorld())
                        continue;

                    meshObj->BuildCreateUpdateBlockForPlayer(&storageUpdate, p);
                    p->m_clientGUIDs.insert(meshObjGuid);
                    ++meshCreateCount;
                }

                storageUpdate.BuildPacket(&storagePacket);
                p->SendDirectMessage(&storagePacket);

                session->GetBattlenetAccount().ClearUpdateMask(true);
                session->GetHousingPlayerHouseEntity().ClearUpdateMask(true);

                // Removed second PlayerHousesInfoResponse emission. The original
                // rationale ("matching retail step 3 of CMSG_HOUSING_DECOR_REQUEST_STORAGE
                // sequence") turns out to have been based on sniff analysis of our own
                // TC server output, not real retail. Retail's pristine 66838 login
                // sniffs (verified across 3 independent captures) show zero unprompted
                // PlayerHousesInfo emissions. Per user's blizzlike guardrail this
                // speculative re-emission is dropped.

                TC_LOG_DEBUG("housing", "HousingMap deferred ENTER_PLOT: Sent STORAGE_RSP ack + Account CREATE + {} decor MeshObject CREATEs for player {}",
                    meshCreateCount, playerGuid.ToString());

                // BLIZZLIKE: the 500 ms defer no longer emits housing
                // response SMSGs. Retail 66838 sniff analysis across 3
                // independent login captures shows ZERO unprompted housing
                // emissions in the post-LVW window. Previous iterations
                // emitted QueryNeighborhoodNameResponse + mirror VALUES_UPDATE
                // + QueryPlayerNamesResponse as a speculative "roster-replay
                // wake-up". All removed per user's blizzlike guardrail. The
                // CMSG handlers emit the correct reactive responses when the
                // client queries. The mirror state was populated synchronously
                // in Player::LoadFromDB and rides in the Player CREATE bundle.
                (void)session;

                // Simulate the edit-mode ON → OFF transition on the Player
                // entity (without actually entering edit mode). The user's
                // manual toggle is the only known way to unblock cornerstone
                // and door interactivity at login, and the single observable
                // effect of that toggle on the Player is:
                //   ON:  set UNIT_FLAG_PACIFIED / UNIT_FLAG2_NO_ACTIONS /
                //        silenced-school → push VALUES_UPDATE
                //   OFF: clear all three → push VALUES_UPDATE
                // Replicating the pair at plot-enter pushes the same flag
                // transition the client apparently needs to wire up housing
                // GO clicks.
                {
                    // "ON" push: set the edit-mode-ish flags and flush.
                    p->SetUnitFlag(UNIT_FLAG_PACIFIED);
                    p->SetUnitFlag2(UNIT_FLAG2_NO_ACTIONS);
                    p->ReplaceAllSilencedSchoolMask(SPELL_SCHOOL_MASK_ALL);
                    p->BuildUpdateChangesMask();
                    {
                        UpdateData onUpdate(p->GetMapId());
                        WorldPacket onPacket;
                        p->BuildValuesUpdateBlockForPlayer(&onUpdate, p);
                        if (onUpdate.HasData())
                        {
                            onUpdate.BuildPacket(&onPacket);
                            p->SendDirectMessage(&onPacket);
                        }
                        p->ClearUpdateMask(false);
                    }

                    // "OFF" push: clear them and flush again.
                    p->RemoveUnitFlag(UNIT_FLAG_PACIFIED);
                    p->RemoveUnitFlag2(UNIT_FLAG2_NO_ACTIONS);
                    p->ReplaceAllSilencedSchoolMask(SpellSchoolMask(0));
                    p->BuildUpdateChangesMask();
                    {
                        UpdateData offUpdate(p->GetMapId());
                        WorldPacket offPacket;
                        p->BuildValuesUpdateBlockForPlayer(&offUpdate, p);
                        if (offUpdate.HasData())
                        {
                            offUpdate.BuildPacket(&offPacket);
                            p->SendDirectMessage(&offPacket);
                        }
                        p->ClearUpdateMask(false);
                    }
                }
            }

            // Post-tutorial + neighborhood-map-entry auras are now emitted
            // synchronously at the end of AddPlayerToMap (see below), immediately
            // after the initial UPDATE_OBJECT bundle flushes. They must not ride
            // on the 500 ms defer, which caused the 2 min "update gap" observed
            // by the user where the map/icons settled only after the delayed
            // burst.
            // NOTE: SendPlotEnterSpellPackets is emitted by the plot AT's OnUnitEnter
            // hook (at_housing_plot.cpp) when the player physically overlaps the plot
            // AreaTrigger — the correct blizzlike trigger per the sniff (spells
            // "In Plot"/1239847 and "Visiting Neighbor"/469226 are plot-overlap, not
            // map-entry auras). The previous deferred emission here fired it on map
            // entry regardless of whether the player's spawn position was inside a
            // plot AT, which is wrong. Removed; AT hook remains the sole caller.

            // Diagnostic: print AT position vs player position for OutsidePlotBounds debugging
            float dist2d = p->GetExactDist2d(plotAt);
            float dist3d = p->GetExactDist(plotAt);
            bool inBox = p->IsWithinBox(*plotAt, 35.0f, 30.0f, 47.0f);  // half-extents from SQL ShapeData

            TC_LOG_DEBUG("housing", "HousingMap deferred ENTER_PLOT: player {} plot {} AT {}\n"
                "  AT pos: ({:.1f}, {:.1f}, {:.1f}, facing={:.3f})\n"
                "  Player pos: ({:.1f}, {:.1f}, {:.1f})\n"
                "  Dist2D={:.1f} Dist3D={:.1f} InBox={} HasPlayers={}",
                playerGuid.ToString(), deferredPlotIndex, plotAt->GetGUID().ToString(),
                plotAt->GetPositionX(), plotAt->GetPositionY(), plotAt->GetPositionZ(), plotAt->GetOrientation(),
                p->GetPositionX(), p->GetPositionY(), p->GetPositionZ(),
                dist2d, dist3d, inBox,
                plotAt->HasAreaTriggerFlag(AreaTriggerFieldFlags::HasPlayers));
        }, Milliseconds(500));
    }

    // Retail sniff dump_12.0.1.66838_2026-04-15_09-35-59 emits the post-tutorial
    // aura trio and the 4-aura neighborhood-map-entry burst immediately after
    // the big UPDATE_OBJECT at map entry, not after a multi-second delay.
    // Emitting them synchronously here (instead of from the 500 ms deferred
    // ENTER_PLOT callback) removes the ~2 min settle the user was seeing and
    // fires the spell triples for visitors too (the deferred block was gated
    // on the player having a house).
    SendPostTutorialAuras(player);
    SendNeighborhoodMapEntryAuras(player);

    // Start the periodic housing WorldState counter timer.
    // Sniff-verified: 5 counters sent as individual SMSG_UPDATE_WORLD_STATE packets.
    // Counters 1-3 increment by ~1333, counters 4-5 by ~7233, every ~300ms.
    // Seed with getMSTime()-based values (retail uses opaque server-tick values;
    // the exact seed doesn't matter as long as the increment pattern is correct).
    {
        uint32 baseSeed = getMSTime();
        player->m_Events.AddEventAtOffset(
            new HousingWorldStateCounterEvent(player->GetGUID(),
                baseSeed, baseSeed / 3, baseSeed + 55758738,
                baseSeed * 2, baseSeed + 123456789),
            Milliseconds(HOUSING_WORLDSTATE_INTERVAL_MS));
        TC_LOG_DEBUG("housing", "HousingMap::AddPlayerToMap: Started WorldState counter timer (5 counters) for player {}",
            player->GetGUID().ToString());
    }

    // Send personalized per-plot WorldState values for this specific player.
    // The init world states (sent during Map::AddPlayerToMap) use map-global defaults
    // (STRANGER for occupied, NONE for unoccupied). This corrects them to SELF/FRIEND
    // based on the player's relationship to each plot owner.
    SendPerPlayerPlotWorldStates(player);

    // Comprehensive summary of all packets sent during map entry (for sniff comparison)
    TC_LOG_DEBUG("housing", "=== AddPlayerToMap COMPLETE for player {} ===\n"
        "  Map: {} InstanceType={} NeighborhoodId={}\n"
        "  HasHouse: {} PlotIndex: {}\n"
        "  Packets sent: CURRENT_HOUSE_INFO, 3xAURA+3xSTART+3xGO, "
        "deferred ENTER_PLOT (500ms), WorldState timer started, PerPlayerPlotWorldStates\n"
        "  Player pos: ({:.1f}, {:.1f}, {:.1f})",
        player->GetGUID().ToString(),
        GetId(), GetEntry()->InstanceType, _neighborhoodId,
        housing ? "yes" : "no",
        housing ? housing->GetPlotIndex() : 255,
        player->GetPositionX(), player->GetPositionY(), player->GetPositionZ());

    return true;
}

void HousingMap::RemovePlayerFromMap(Player* player, bool remove)
{
    // Remove plot auras before removing housing data.
    if (Housing const* housing = GetHousingForPlayer(player->GetGUID()))
    {
        // Remove all plot enter/presence auras (manual packets — spells not in DB2)
        SendPlotLeaveAuraRemoval(player);

        // Clear plot tracking
        ClearPlayerCurrentPlot(player->GetGUID());
    }

    RemovePlayerHousing(player->GetGUID());

    TC_LOG_DEBUG("housing", "HousingMap::RemovePlayerFromMap: Player {} leaving housing map {} instanceId {}",
        player->GetGUID().ToString(), GetId(), GetInstanceId());

    Map::RemovePlayerFromMap(player, remove);
}

void HousingMap::SendPostTutorialAuras(Player* player)
{
    // Sniff-verified: After quest 94455 "Home at Last" completion, three "post-tutorial" auras
    // are applied at slots 8, 9, 50. These replace old tutorial-phase auras.
    // These persist for the rest of the session. Since they don't exist in DB2, we send
    // manual SMSG_AURA_UPDATE packets each time the player enters the housing map.
    // Slot 8: spell 1285428 (NoCaster, ActiveFlags=1)
    // Slot 9: spell 1285424 (NoCaster, ActiveFlags=1) — will be overwritten by plot enter aura
    // Slot 50: spell 1266699 (NoCaster|Scalable, ActiveFlags=1, Points=1) — overwritten by plot enter

    // Spell 1285428 at slot 8
    {
        ObjectGuid castId = ObjectGuid::Create<HighGuid::Cast>(
            SPELL_CAST_SOURCE_NORMAL, player->GetMapId(), SPELL_HOUSING_TUTORIAL_DONE_1,
            player->GetMap()->GenerateLowGuid<HighGuid::Cast>());

        WorldPackets::Spells::AuraUpdate auraUpdate;
        auraUpdate.UpdateAll = false;
        auraUpdate.UnitGUID = player->GetGUID();

        WorldPackets::Spells::AuraInfo auraInfo;
        auraInfo.Slot = 8;
        auraInfo.AuraData.emplace();
        auraInfo.AuraData->CastID = castId;
        auraInfo.AuraData->SpellID = SPELL_HOUSING_TUTORIAL_DONE_1;
        auraInfo.AuraData->Flags = AFLAG_NOCASTER;
        auraInfo.AuraData->ActiveFlags = 1;
        auraInfo.AuraData->CastLevel = 36;
        auraInfo.AuraData->Applications = 0;
        auraUpdate.Auras.push_back(std::move(auraInfo));
        player->SendDirectMessage(auraUpdate.Write());

        WorldPackets::Spells::SpellStart spellStart;
        spellStart.Cast.CasterGUID = player->GetGUID();
        spellStart.Cast.CasterUnit = player->GetGUID();
        spellStart.Cast.CastID = castId;
        spellStart.Cast.SpellID = SPELL_HOUSING_TUTORIAL_DONE_1;
        spellStart.Cast.CastFlags = CAST_FLAG_PENDING | CAST_FLAG_HAS_TRAJECTORY | CAST_FLAG_UNKNOWN_3 | CAST_FLAG_UNKNOWN_4;  // 15
        spellStart.Cast.CastTime = 0;
        player->SendDirectMessage(spellStart.Write());

        WorldPackets::Spells::SpellGo spellGo;
        spellGo.Cast.CasterGUID = player->GetGUID();
        spellGo.Cast.CasterUnit = player->GetGUID();
        spellGo.Cast.CastID = castId;
        spellGo.Cast.SpellID = SPELL_HOUSING_TUTORIAL_DONE_1;
        spellGo.Cast.CastFlags = CAST_FLAG_PENDING | CAST_FLAG_UNKNOWN_3 | CAST_FLAG_UNKNOWN_4 | CAST_FLAG_UNKNOWN_9 | CAST_FLAG_UNKNOWN_10;  // 781
        spellGo.Cast.CastFlagsEx = 16;
        spellGo.Cast.CastFlagsEx2 = 4;
        spellGo.Cast.CastTime = getMSTime();
        spellGo.Cast.Target.Flags = TARGET_FLAG_UNIT;
        spellGo.Cast.HitTargets.push_back(player->GetGUID());
        spellGo.Cast.HitStatus.emplace_back(uint8(0));
        spellGo.LogData.Initialize(player);
        player->SendDirectMessage(spellGo.Write());
    }

    // Spell 1285424 at slot 9
    {
        ObjectGuid castId = ObjectGuid::Create<HighGuid::Cast>(
            SPELL_CAST_SOURCE_NORMAL, player->GetMapId(), SPELL_HOUSING_TUTORIAL_DONE_2,
            player->GetMap()->GenerateLowGuid<HighGuid::Cast>());

        WorldPackets::Spells::AuraUpdate auraUpdate;
        auraUpdate.UpdateAll = false;
        auraUpdate.UnitGUID = player->GetGUID();

        WorldPackets::Spells::AuraInfo auraInfo;
        auraInfo.Slot = 9;
        auraInfo.AuraData.emplace();
        auraInfo.AuraData->CastID = castId;
        auraInfo.AuraData->SpellID = SPELL_HOUSING_TUTORIAL_DONE_2;
        auraInfo.AuraData->Flags = AFLAG_NOCASTER;
        auraInfo.AuraData->ActiveFlags = 1;
        auraInfo.AuraData->CastLevel = 36;
        auraInfo.AuraData->Applications = 0;
        auraUpdate.Auras.push_back(std::move(auraInfo));
        player->SendDirectMessage(auraUpdate.Write());

        WorldPackets::Spells::SpellStart spellStart;
        spellStart.Cast.CasterGUID = player->GetGUID();
        spellStart.Cast.CasterUnit = player->GetGUID();
        spellStart.Cast.CastID = castId;
        spellStart.Cast.SpellID = SPELL_HOUSING_TUTORIAL_DONE_2;
        spellStart.Cast.CastFlags = CAST_FLAG_PENDING | CAST_FLAG_HAS_TRAJECTORY | CAST_FLAG_UNKNOWN_3 | CAST_FLAG_UNKNOWN_4;
        spellStart.Cast.CastTime = 0;
        player->SendDirectMessage(spellStart.Write());

        WorldPackets::Spells::SpellGo spellGo;
        spellGo.Cast.CasterGUID = player->GetGUID();
        spellGo.Cast.CasterUnit = player->GetGUID();
        spellGo.Cast.CastID = castId;
        spellGo.Cast.SpellID = SPELL_HOUSING_TUTORIAL_DONE_2;
        spellGo.Cast.CastFlags = CAST_FLAG_PENDING | CAST_FLAG_UNKNOWN_3 | CAST_FLAG_UNKNOWN_4 | CAST_FLAG_UNKNOWN_9 | CAST_FLAG_UNKNOWN_10;
        spellGo.Cast.CastFlagsEx = 16;
        spellGo.Cast.CastFlagsEx2 = 4;
        spellGo.Cast.CastTime = getMSTime();
        spellGo.Cast.Target.Flags = TARGET_FLAG_UNIT;
        spellGo.Cast.HitTargets.push_back(player->GetGUID());
        spellGo.Cast.HitStatus.emplace_back(uint8(0));
        spellGo.LogData.Initialize(player);
        player->SendDirectMessage(spellGo.Write());
    }

    // Spell 1266699 at slot 50 (same ID as SPELL_HOUSING_PLOT_ENTER_2, different slot + Points)
    {
        ObjectGuid castId = ObjectGuid::Create<HighGuid::Cast>(
            SPELL_CAST_SOURCE_NORMAL, player->GetMapId(), SPELL_HOUSING_TUTORIAL_DONE_3,
            player->GetMap()->GenerateLowGuid<HighGuid::Cast>());

        WorldPackets::Spells::AuraUpdate auraUpdate;
        auraUpdate.UpdateAll = false;
        auraUpdate.UnitGUID = player->GetGUID();

        WorldPackets::Spells::AuraInfo auraInfo;
        auraInfo.Slot = 50;
        auraInfo.AuraData.emplace();
        auraInfo.AuraData->CastID = castId;
        auraInfo.AuraData->SpellID = SPELL_HOUSING_TUTORIAL_DONE_3;
        auraInfo.AuraData->Flags = AFLAG_NOCASTER | AFLAG_SCALABLE;
        auraInfo.AuraData->ActiveFlags = 1;
        auraInfo.AuraData->CastLevel = 36;
        auraInfo.AuraData->Applications = 0;
        auraInfo.AuraData->Points.push_back(1.0f);
        auraUpdate.Auras.push_back(std::move(auraInfo));
        player->SendDirectMessage(auraUpdate.Write());

        WorldPackets::Spells::SpellStart spellStart;
        spellStart.Cast.CasterGUID = player->GetGUID();
        spellStart.Cast.CasterUnit = player->GetGUID();
        spellStart.Cast.CastID = castId;
        spellStart.Cast.SpellID = SPELL_HOUSING_TUTORIAL_DONE_3;
        spellStart.Cast.CastFlags = CAST_FLAG_PENDING | CAST_FLAG_HAS_TRAJECTORY | CAST_FLAG_UNKNOWN_3 | CAST_FLAG_UNKNOWN_4;
        spellStart.Cast.CastTime = 0;
        player->SendDirectMessage(spellStart.Write());

        WorldPackets::Spells::SpellGo spellGo;
        spellGo.Cast.CasterGUID = player->GetGUID();
        spellGo.Cast.CasterUnit = player->GetGUID();
        spellGo.Cast.CastID = castId;
        spellGo.Cast.SpellID = SPELL_HOUSING_TUTORIAL_DONE_3;
        spellGo.Cast.CastFlags = CAST_FLAG_PENDING | CAST_FLAG_UNKNOWN_3 | CAST_FLAG_UNKNOWN_4 | CAST_FLAG_UNKNOWN_9 | CAST_FLAG_UNKNOWN_10;
        spellGo.Cast.CastFlagsEx = 16;
        spellGo.Cast.CastFlagsEx2 = 4;
        spellGo.Cast.CastTime = getMSTime();
        spellGo.Cast.Target.Flags = TARGET_FLAG_UNIT;
        spellGo.Cast.HitTargets.push_back(player->GetGUID());
        spellGo.Cast.HitStatus.emplace_back(uint8(0));
        spellGo.LogData.Initialize(player);
        player->SendDirectMessage(spellGo.Write());
    }

    TC_LOG_DEBUG("housing", "SendPostTutorialAuras: Sent 3 post-tutorial aura sequences "
        "(1285428@s8, 1285424@s9, 1266699@s50) for player {}",
        player->GetGUID().ToString());
}

void HousingMap::SendNeighborhoodMapEntryAuras(Player* player)
{
    // Blizzlike map-entry aura burst — decoded from
    // dump_12.0.1.66838_2026-04-15_09-35-59.pkt idx 9988/9994/9997/10000
    // and cross-checked against dump_12.0.1.66838_2026-04-10_08-45-23.pkt
    // idx 15676/15682/15685/15688. Each entry is one AURA_UPDATE +
    // SPELL_START + SPELL_GO triple. All four fields (Slot, Flags,
    // ActiveFlags, Visual.SpellXSpellVisualID) are taken straight from the
    // sniff. CastLevel 84 on retail; we use player->getLevel() as a bonus
    // since the decoded value reflects whatever char captured the sniff.
    //
    // 431539 (Morning Star) and 1266699 (Sound Squisher) appeared in the
    // same retail burst but are character/ambient auras pre-existing before
    // map entry (first seen at idx 5762/5786, 127× + 8× before map entry).
    // Core TC aura re-sync on map change already handles those; we emit
    // only the four truly-new housing-specific auras here.

    if (!player)
        return;

    struct MapEntryAura
    {
        uint32 SpellID;
        uint16 Slot;
        uint16 Flags;
        uint32 ActiveFlags;
        uint32 VisualSpellXSpellVisualID;
    };
    constexpr std::array<MapEntryAura, 4> kAuras = {{
        { SPELL_HOUSING_MAP_ENTRY_FIXUP,    20,  AFLAG_NOCASTER,                1, 0                                        },
        { SPELL_HOUSING_MAP_ENTRY_REACT,    22,  AFLAG_NOCASTER,                1, 0                                        },
        { SPELL_HOUSING_MAP_ENTRY_ENDEAVOR, 53,  AFLAG_NOCASTER,                1, 0                                        },
        { SPELL_HOUSING_MAP_ENTRY_NEIGHBOR, 121, uint16(AFLAG_NOCASTER | AFLAG_POSITIVE), 3, VISUAL_HOUSING_MAP_ENTRY_NEIGHBOR },
    }};

    uint16 const castLevel = static_cast<uint16>(player->GetLevel());

    for (MapEntryAura const& a : kAuras)
    {
        ObjectGuid castId = ObjectGuid::Create<HighGuid::Cast>(
            SPELL_CAST_SOURCE_NORMAL, player->GetMapId(), a.SpellID,
            player->GetMap()->GenerateLowGuid<HighGuid::Cast>());

        WorldPackets::Spells::AuraUpdate auraUpdate;
        auraUpdate.UpdateAll = false;
        auraUpdate.UnitGUID = player->GetGUID();

        WorldPackets::Spells::AuraInfo auraInfo;
        auraInfo.Slot = a.Slot;
        auraInfo.AuraData.emplace();
        auraInfo.AuraData->CastID = castId;
        auraInfo.AuraData->SpellID = a.SpellID;
        auraInfo.AuraData->Visual.SpellXSpellVisualID = a.VisualSpellXSpellVisualID;
        auraInfo.AuraData->Flags = a.Flags;
        auraInfo.AuraData->ActiveFlags = a.ActiveFlags;
        auraInfo.AuraData->CastLevel = castLevel;
        auraInfo.AuraData->Applications = 0;
        auraUpdate.Auras.push_back(std::move(auraInfo));
        player->SendDirectMessage(auraUpdate.Write());

        WorldPackets::Spells::SpellStart spellStart;
        spellStart.Cast.CasterGUID = player->GetGUID();
        spellStart.Cast.CasterUnit = player->GetGUID();
        spellStart.Cast.CastID = castId;
        spellStart.Cast.SpellID = a.SpellID;
        spellStart.Cast.Visual.SpellXSpellVisualID = a.VisualSpellXSpellVisualID;
        spellStart.Cast.CastFlags = CAST_FLAG_HAS_TRAJECTORY | CAST_FLAG_UNKNOWN_3 | CAST_FLAG_UNKNOWN_4 | CAST_FLAG_VISUAL_CHAIN;
        spellStart.Cast.CastTime = 0;
        player->SendDirectMessage(spellStart.Write());

        WorldPackets::Spells::SpellGo spellGo;
        spellGo.Cast.CasterGUID = player->GetGUID();
        spellGo.Cast.CasterUnit = player->GetGUID();
        spellGo.Cast.CastID = castId;
        spellGo.Cast.SpellID = a.SpellID;
        spellGo.Cast.Visual.SpellXSpellVisualID = a.VisualSpellXSpellVisualID;
        spellGo.Cast.CastFlags = CAST_FLAG_UNKNOWN_3 | CAST_FLAG_UNKNOWN_4 | CAST_FLAG_UNKNOWN_9 | CAST_FLAG_UNKNOWN_10 | CAST_FLAG_VISUAL_CHAIN;
        spellGo.Cast.CastFlagsEx = 16;
        spellGo.Cast.CastFlagsEx2 = 4;
        spellGo.Cast.CastTime = getMSTime();
        spellGo.Cast.Target.Flags = TARGET_FLAG_UNIT;
        spellGo.Cast.HitTargets.push_back(player->GetGUID());
        spellGo.Cast.HitStatus.emplace_back(uint8(0));
        spellGo.LogData.Initialize(player);
        player->SendDirectMessage(spellGo.Write());
    }

    TC_LOG_DEBUG("housing", "SendNeighborhoodMapEntryAuras: Sent 4 map-entry aura triples "
        "(1272741@s20, 1263578@s22, 1276064@s53, 1227147@s121 vis={}) for player {}",
        VISUAL_HOUSING_MAP_ENTRY_NEIGHBOR, player->GetGUID().ToString());
}

void HousingMap::SendPlotEnterSpellPackets(Player* player, uint8 plotIndex)
{
    // PLOT AT OVERLAP event — NOT map-entry. Wowhead:
    //   1239847 = "[DNT] In Plot"           → applied when the player's unit
    //                                         is inside a plot's AreaTrigger box
    //   469226  = "[DNT] Visiting Neighbor" → applied when that plot is owned
    //                                         by someone else
    //   1266699 = "[DNT] Sound Squisher"    → audio mixer aura
    //
    // Invoked only from at_housing_plot.cpp's OnUnitEnter hook. The earlier
    // 500 ms-deferred invocation inside HousingMap::AddPlayerToMap was moved
    // out because it fired unconditionally on map entry regardless of whether
    // the player was actually inside a plot AT — the retail triggers are
    // strictly AT overlap, not map transition.
    //
    // Manual packets are required because these spell IDs don't exist in DB2
    // (CastSpell() fails silently for DNT spells).

    TC_LOG_DEBUG("housing", "SendPlotEnterSpellPackets: BEGIN for player {} plot {} map {}",
        player->GetGUID().ToString(), plotIndex, GetId());

    // 1. Spell 1239847 — plot enter tracking aura (slot 55)
    // Sniff-verified: retail sends to slot 55, ActiveFlags=1 (NOT slot 50 which is tutorial aura)
    {
        ObjectGuid castId = ObjectGuid::Create<HighGuid::Cast>(
            SPELL_CAST_SOURCE_NORMAL, player->GetMapId(), SPELL_HOUSING_PLOT_ENTER,
            player->GetMap()->GenerateLowGuid<HighGuid::Cast>());

        WorldPackets::Spells::AuraUpdate auraUpdate;
        auraUpdate.UpdateAll = false;
        auraUpdate.UnitGUID = player->GetGUID();

        WorldPackets::Spells::AuraInfo auraInfo;
        auraInfo.Slot = 55;
        auraInfo.AuraData.emplace();
        auraInfo.AuraData->CastID = castId;
        auraInfo.AuraData->SpellID = SPELL_HOUSING_PLOT_ENTER;
        auraInfo.AuraData->Flags = AFLAG_NOCASTER;
        auraInfo.AuraData->ActiveFlags = 1;
        auraInfo.AuraData->CastLevel = 36;
        auraInfo.AuraData->Applications = 0;
        auraUpdate.Auras.push_back(std::move(auraInfo));
        player->SendDirectMessage(auraUpdate.Write());

        WorldPackets::Spells::SpellStart spellStart;
        spellStart.Cast.CasterGUID = player->GetGUID();
        spellStart.Cast.CasterUnit = player->GetGUID();
        spellStart.Cast.CastID = castId;
        spellStart.Cast.SpellID = SPELL_HOUSING_PLOT_ENTER;
        spellStart.Cast.CastFlags = CAST_FLAG_HAS_TRAJECTORY | CAST_FLAG_UNKNOWN_3 | CAST_FLAG_UNKNOWN_4 | CAST_FLAG_VISUAL_CHAIN;  // 524302 = 0x8000E
        spellStart.Cast.CastTime = 0;
        player->SendDirectMessage(spellStart.Write());

        WorldPackets::Spells::SpellGo spellGo;
        spellGo.Cast.CasterGUID = player->GetGUID();
        spellGo.Cast.CasterUnit = player->GetGUID();
        spellGo.Cast.CastID = castId;
        spellGo.Cast.SpellID = SPELL_HOUSING_PLOT_ENTER;
        spellGo.Cast.CastFlags = CAST_FLAG_UNKNOWN_3 | CAST_FLAG_UNKNOWN_4 | CAST_FLAG_UNKNOWN_9 | CAST_FLAG_UNKNOWN_10 | CAST_FLAG_VISUAL_CHAIN;  // 525068 = 0x8030C
        spellGo.Cast.CastFlagsEx = 16;
        spellGo.Cast.CastFlagsEx2 = 4;
        spellGo.Cast.CastTime = getMSTime();
        spellGo.Cast.Target.Flags = TARGET_FLAG_UNIT;
        spellGo.Cast.HitTargets.push_back(player->GetGUID());
        spellGo.Cast.HitStatus.emplace_back(uint8(0));
        spellGo.LogData.Initialize(player);
        player->SendDirectMessage(spellGo.Write());
    }

    // 2. Set HasPlayers flag (Flags=1024) on the plot AreaTrigger.
    // Sniff-verified: this UPDATE_OBJECT goes out between the first spell set (1239847)
    // and the second (469226). It tells the client that players are inside this AT.
    if (AreaTrigger* plotAt = GetPlotAreaTrigger(plotIndex))
    {
        plotAt->SetAreaTriggerFlag(AreaTriggerFieldFlags::HasPlayers);
        TC_LOG_DEBUG("housing", "SendPlotEnterSpellPackets: Set HasPlayers on AT {} for player {} plot {}",
            plotAt->GetGUID().ToString(), player->GetGUID().ToString(), plotIndex);
    }

    // 3. Spell 469226 — plot presence aura (slot 56)
    {
        ObjectGuid castId2 = ObjectGuid::Create<HighGuid::Cast>(
            SPELL_CAST_SOURCE_NORMAL, player->GetMapId(), SPELL_HOUSING_PLOT_PRESENCE,
            player->GetMap()->GenerateLowGuid<HighGuid::Cast>());

        WorldPackets::Spells::AuraUpdate auraUpdate;
        auraUpdate.UpdateAll = false;
        auraUpdate.UnitGUID = player->GetGUID();

        WorldPackets::Spells::AuraInfo auraInfo;
        auraInfo.Slot = 56;
        auraInfo.AuraData.emplace();
        auraInfo.AuraData->CastID = castId2;
        auraInfo.AuraData->SpellID = SPELL_HOUSING_PLOT_PRESENCE;
        auraInfo.AuraData->Flags = AFLAG_NOCASTER;
        auraInfo.AuraData->ActiveFlags = 1;
        auraInfo.AuraData->CastLevel = 36;
        auraInfo.AuraData->Applications = 0;
        auraUpdate.Auras.push_back(std::move(auraInfo));
        player->SendDirectMessage(auraUpdate.Write());

        WorldPackets::Spells::SpellStart spellStart;
        spellStart.Cast.CasterGUID = player->GetGUID();
        spellStart.Cast.CasterUnit = player->GetGUID();
        spellStart.Cast.CastID = castId2;
        spellStart.Cast.SpellID = SPELL_HOUSING_PLOT_PRESENCE;
        spellStart.Cast.CastFlags = CAST_FLAG_PENDING | CAST_FLAG_HAS_TRAJECTORY | CAST_FLAG_UNKNOWN_4 | CAST_FLAG_UNKNOWN_24 | CAST_FLAG_UNKNOWN_30;  // 0x2080000B
        spellStart.Cast.CastFlagsEx = 0x2000200;
        spellStart.Cast.CastTime = 0;
        player->SendDirectMessage(spellStart.Write());

        WorldPackets::Spells::SpellGo spellGo;
        spellGo.Cast.CasterGUID = player->GetGUID();
        spellGo.Cast.CasterUnit = player->GetGUID();
        spellGo.Cast.CastID = castId2;
        spellGo.Cast.SpellID = SPELL_HOUSING_PLOT_PRESENCE;
        spellGo.Cast.CastFlags = CAST_FLAG_PENDING | CAST_FLAG_UNKNOWN_4 | CAST_FLAG_UNKNOWN_9 | CAST_FLAG_UNKNOWN_10 | CAST_FLAG_UNKNOWN_24 | CAST_FLAG_UNKNOWN_30;  // 0x20800309
        spellGo.Cast.CastFlagsEx = 0x2000210;
        spellGo.Cast.CastFlagsEx2 = 4;
        spellGo.Cast.CastTime = getMSTime();
        spellGo.Cast.Target.Flags = TARGET_FLAG_UNIT;
        spellGo.Cast.HitTargets.push_back(player->GetGUID());
        spellGo.Cast.HitStatus.emplace_back(uint8(0));
        spellGo.LogData.Initialize(player);
        player->SendDirectMessage(spellGo.Write());
    }

    // 4. Spell 1266699 — slot 9 replacement (preceded by slot 9 removal)
    // CastFlags: START=15, GO=781, GoEx=16, GoEx2=4
    {
        // Remove existing slot 9 aura
        WorldPackets::Spells::AuraUpdate auraRemove;
        auraRemove.UpdateAll = false;
        auraRemove.UnitGUID = player->GetGUID();
        WorldPackets::Spells::AuraInfo removeInfo;
        removeInfo.Slot = 9;
        auraRemove.Auras.push_back(std::move(removeInfo));
        player->SendDirectMessage(auraRemove.Write());

        ObjectGuid castId3 = ObjectGuid::Create<HighGuid::Cast>(
            SPELL_CAST_SOURCE_NORMAL, player->GetMapId(), SPELL_HOUSING_PLOT_ENTER_2,
            player->GetMap()->GenerateLowGuid<HighGuid::Cast>());

        // Apply 1266699 at slot 9 (Flags=NoCaster|Scalable=9, PointsCount=1, Points[0]=1)
        WorldPackets::Spells::AuraUpdate auraUpdate;
        auraUpdate.UpdateAll = false;
        auraUpdate.UnitGUID = player->GetGUID();
        WorldPackets::Spells::AuraInfo auraInfo;
        auraInfo.Slot = 9;
        auraInfo.AuraData.emplace();
        auraInfo.AuraData->CastID = castId3;
        auraInfo.AuraData->SpellID = SPELL_HOUSING_PLOT_ENTER_2;
        auraInfo.AuraData->Flags = AFLAG_NOCASTER | AFLAG_SCALABLE;
        auraInfo.AuraData->ActiveFlags = 1;
        auraInfo.AuraData->CastLevel = 36;
        auraInfo.AuraData->Applications = 0;
        auraInfo.AuraData->Points.push_back(1.0f);
        auraUpdate.Auras.push_back(std::move(auraInfo));
        player->SendDirectMessage(auraUpdate.Write());

        WorldPackets::Spells::SpellStart spellStart;
        spellStart.Cast.CasterGUID = player->GetGUID();
        spellStart.Cast.CasterUnit = player->GetGUID();
        spellStart.Cast.CastID = castId3;
        spellStart.Cast.SpellID = SPELL_HOUSING_PLOT_ENTER_2;
        spellStart.Cast.CastFlags = CAST_FLAG_PENDING | CAST_FLAG_HAS_TRAJECTORY | CAST_FLAG_UNKNOWN_3 | CAST_FLAG_UNKNOWN_4;  // 15
        spellStart.Cast.CastTime = 0;
        player->SendDirectMessage(spellStart.Write());

        WorldPackets::Spells::SpellGo spellGo;
        spellGo.Cast.CasterGUID = player->GetGUID();
        spellGo.Cast.CasterUnit = player->GetGUID();
        spellGo.Cast.CastID = castId3;
        spellGo.Cast.SpellID = SPELL_HOUSING_PLOT_ENTER_2;
        spellGo.Cast.CastFlags = CAST_FLAG_PENDING | CAST_FLAG_UNKNOWN_3 | CAST_FLAG_UNKNOWN_4 | CAST_FLAG_UNKNOWN_9 | CAST_FLAG_UNKNOWN_10;  // 781
        spellGo.Cast.CastFlagsEx = 16;
        spellGo.Cast.CastFlagsEx2 = 4;
        spellGo.Cast.CastTime = getMSTime();
        spellGo.Cast.Target.Flags = TARGET_FLAG_UNIT;
        spellGo.Cast.HitTargets.push_back(player->GetGUID());
        spellGo.Cast.HitStatus.emplace_back(uint8(0));
        spellGo.LogData.Initialize(player);
        player->SendDirectMessage(spellGo.Write());
    }

    TC_LOG_DEBUG("housing", "SendPlotEnterSpellPackets: END — sent 3 spell sequences "
        "(1239847@s50, 469226@s56, 1266699@s9) + AT HasPlayers flag for player {} plot {}",
        player->GetGUID().ToString(), plotIndex);
}

void HousingMap::SendPlotLeaveAuraRemoval(Player* player)
{
    // Remove all plot enter/presence auras (slots 50, 56, 9)
    // Send aura removal packets (empty AuraData = HasAura=False)
    for (uint8 slot : { uint8(50), uint8(56), uint8(9) })
    {
        WorldPackets::Spells::AuraUpdate auraUpdate;
        auraUpdate.UpdateAll = false;
        auraUpdate.UnitGUID = player->GetGUID();

        WorldPackets::Spells::AuraInfo auraInfo;
        auraInfo.Slot = slot;
        auraUpdate.Auras.push_back(std::move(auraInfo));

        player->SendDirectMessage(auraUpdate.Write());
    }
    TC_LOG_DEBUG("housing", "HousingMap::SendPlotLeaveAuraRemoval: Removed auras (slots 50, 56, 9) for player {}",
        player->GetGUID().ToString());
}

HousingPlotOwnerType HousingMap::GetPlotOwnerTypeForPlayer(Player const* player, uint8 plotIndex) const
{
    if (!_neighborhood || !player)
        return HOUSING_PLOT_OWNER_NONE;

    Neighborhood::PlotInfo const* plotInfo = _neighborhood->GetPlotInfo(plotIndex);
    if (!plotInfo || plotInfo->OwnerGuid.IsEmpty())
        return HOUSING_PLOT_OWNER_NONE;

    // Check if the player owns this plot
    if (plotInfo->OwnerGuid == player->GetGUID())
        return HOUSING_PLOT_OWNER_SELF;

    // Check if it's an alt on the same BNet account (same person, different character)
    if (!plotInfo->OwnerBnetGuid.IsEmpty() && player->GetSession())
    {
        if (plotInfo->OwnerBnetGuid == player->GetSession()->GetBattlenetAccountGUID())
            return HOUSING_PLOT_OWNER_SELF;
    }

    // Check character-level friendship
    if (PlayerSocial* social = player->GetSocial())
    {
        if (social->HasFriend(plotInfo->OwnerGuid))
            return HOUSING_PLOT_OWNER_FRIEND;
    }

    return HOUSING_PLOT_OWNER_STRANGER;
}

void HousingMap::SendPerPlayerPlotWorldStates(Player* player)
{
    // Blizzlike no-op: the per-plot occupancy worldstate is carried inside
    // SMSG_INIT_WORLD_STATES (set by SpawnPlotGameObjects via SetWorldStateValue),
    // and the regular-map icon + hover info come from the JamCliHouse[] array
    // in SMSG_HOUSING_SVCS_GET_HOUSE_FINDER_NEIGHBORHOOD_RESPONSE plus the
    // Housing/4 NeighborhoodMirrorData.Houses entries. No per-player
    // SMSG_UPDATE_WORLD_STATE spam — retail doesn't do it, and sending enum
    // values here was the regression that broke the icons.
    //
    // Retained as an extension hook for genuine per-player overrides.
    (void)player;
}

void HousingMap::AddPlayerHousing(ObjectGuid playerGuid, Housing* housing)
{
    if (!housing)
    {
        TC_LOG_ERROR("housing", "HousingMap::AddPlayerHousing: Attempted to add null housing for player {} on map {} instanceId {}",
            playerGuid.ToString(), GetId(), GetInstanceId());
        return;
    }

    _playerHousings[playerGuid] = housing;

    TC_LOG_DEBUG("housing", "HousingMap::AddPlayerHousing: Added housing for player {} on map {} instanceId {} (total: {})",
        playerGuid.ToString(), GetId(), GetInstanceId(), static_cast<uint32>(_playerHousings.size()));
}

void HousingMap::RemovePlayerHousing(ObjectGuid playerGuid)
{
    auto itr = _playerHousings.find(playerGuid);
    if (itr != _playerHousings.end())
    {
        _playerHousings.erase(itr);

        TC_LOG_DEBUG("housing", "HousingMap::RemovePlayerHousing: Removed housing for player {} on map {} instanceId {} (remaining: {})",
            playerGuid.ToString(), GetId(), GetInstanceId(), static_cast<uint32>(_playerHousings.size()));
    }
    else
    {
        TC_LOG_DEBUG("housing", "HousingMap::RemovePlayerHousing: No housing found for player {} on map {} instanceId {}",
            playerGuid.ToString(), GetId(), GetInstanceId());
    }
}

// ============================================================
// House Structure GO Management
// ============================================================

GameObject* HousingMap::SpawnHouseForPlot(uint8 plotIndex, Position const* customPos,
    int32 exteriorComponentID, int32 houseExteriorWmoDataID,
    FixtureOverrideMap const* fixtureOverrides /*= nullptr*/,
    RootOverrideMap const* rootOverrides /*= nullptr*/)
{
    if (!_neighborhood)
        return nullptr;

    uint32 neighborhoodMapId = _neighborhood->GetNeighborhoodMapID();
    std::vector<NeighborhoodPlotData const*> plots = sHousingMgr.GetPlotsForMap(neighborhoodMapId);

    NeighborhoodPlotData const* targetPlot = nullptr;
    for (NeighborhoodPlotData const* plot : plots)
    {
        if (static_cast<uint8>(plot->PlotIndex) == plotIndex)
        {
            targetPlot = plot;
            break;
        }
    }

    if (!targetPlot)
    {
        TC_LOG_ERROR("housing", "HousingMap::SpawnHouseForPlot: No plot data for plotIndex {} in neighborhood '{}'",
            plotIndex, _neighborhood->GetName());
        return nullptr;
    }

    // Determine position: use customPos (persisted player position) or DB2 defaults
    float x, y, z, facing;
    if (customPos)
    {
        x = customPos->GetPositionX();
        y = customPos->GetPositionY();
        z = customPos->GetPositionZ();
        facing = customPos->GetOrientation();
    }
    else
    {
        x = targetPlot->HousePosition[0];
        y = targetPlot->HousePosition[1];
        z = targetPlot->HousePosition[2];

        // DB2 HouseRotation is (0,0,0) for all plots — compute facing so the
        // entrance points toward the cornerstone (the plot's interaction point).
        float hRotX = targetPlot->HouseRotation[0];
        float hRotY = targetPlot->HouseRotation[1];
        float hRotZ = targetPlot->HouseRotation[2];
        if (hRotX == 0.0f && hRotY == 0.0f && hRotZ == 0.0f)
        {
            float dx = targetPlot->CornerstonePosition[0] - x;
            float dy = targetPlot->CornerstonePosition[1] - y;
            facing = std::atan2(dy, dx);
        }
        else
            facing = hRotZ;
    }

    LoadGrid(x, y);

    // Ground-clamp the house Z to the platform surface. The static platform WMO spawns
    // (GO entry 574432) are loaded from the gameobject table by LoadGrid and registered
    // in the DynamicMapTree. GetGameObjectFloor finds the platform top surface, which is
    // the correct elevation for the house. DB2 HousePosition.Z alone places the house
    // slightly below the platform surface.
    {
        PhaseShift tempPhase;
        PhasingHandler::InitDbPhaseShift(tempPhase, PHASE_USE_FLAGS_ALWAYS_VISIBLE, 0, 0);
        float groundZ = GetHeight(tempPhase, x, y, z + 50.0f, true, 100.0f);
        if (groundZ > INVALID_HEIGHT && groundZ > z - 5.0f)
        {
            TC_LOG_DEBUG("housing", "HousingMap::SpawnHouseForPlot: plot {} ground-clamped Z from {:.2f} to {:.2f}",
                plotIndex, z, groundZ);
            z = groundZ;
        }
        else
        {
            TC_LOG_DEBUG("housing", "HousingMap::SpawnHouseForPlot: plot {} using DB2 Z={:.2f} (no ground-clamp, height={:.2f})",
                plotIndex, z, groundZ);
        }
    }

    // Build rotation quaternion from the facing angle (yaw only for housing plots,
    // since DB2 HouseRotation X/Y are always 0 and the computed facing is a pure yaw).
    QuaternionData rot = QuaternionData::fromEulerAnglesZYX(facing, 0.0f, 0.0f);

    // Platform WMO (GO entry 574432) is loaded from the static gameobject table (sniff data).
    // Do NOT dynamically spawn a second platform — it renders visibly on top of the static
    // one and the static spawn already provides DynamicMapTree collision for ground-clamping.

    Position pos(x, y, z, facing);
    Neighborhood::PlotInfo const* plotInfo = _neighborhood->GetPlotInfo(plotIndex);

    TC_LOG_ERROR("housing", "HousingMap::SpawnHouseForPlot: plot={} pos=({:.2f}, {:.2f}, {:.2f}) facing={:.3f} "
        "rot=({:.3f}, {:.3f}, {:.3f}, {:.3f}) hasPlotInfo={} hasHouseGuid={} extCompID={} wmoDataID={}",
        plotIndex, x, y, z, facing, rot.x, rot.y, rot.z, rot.w,
        plotInfo != nullptr, plotInfo && !plotInfo->HouseGuid.IsEmpty(),
        exteriorComponentID, houseExteriorWmoDataID);

    if (!plotInfo)
    {
        TC_LOG_ERROR("housing", "HousingMap::SpawnHouseForPlot: plotInfo is NULL for plot {} — "
            "skipping MeshObject spawn (IsOccupied check failed?)", plotIndex);
    }
    else if (plotInfo->HouseGuid.IsEmpty())
    {
        TC_LOG_ERROR("housing", "HousingMap::SpawnHouseForPlot: HouseGuid is EMPTY for plot {} — "
            "skipping MeshObject spawn (UpdatePlotHouseInfo not called?)", plotIndex);
    }

    // Spawn all house MeshObjects (sniff-verified: 10 structural pieces for alliance, different for horde)
    // Pieces have a parent-child hierarchy: base piece (0) and door piece (1) are roots,
    // other pieces attach to them with local-space positions/rotations.
    if (plotInfo && !plotInfo->HouseGuid.IsEmpty())
    {
        int32 faction = _neighborhood ? _neighborhood->GetFactionRestriction()
            : NEIGHBORHOOD_FACTION_ALLIANCE;
        TC_LOG_ERROR("housing", "HousingMap::SpawnHouseForPlot: Spawning MeshObjects for plot {} — "
            "HouseGuid={} faction={} ({})",
            plotIndex, plotInfo->HouseGuid.ToString(), faction,
            faction == NEIGHBORHOOD_FACTION_ALLIANCE ? "Alliance" : "Horde");

        SpawnFullHouseMeshObjects(plotIndex, pos, rot, plotInfo->HouseGuid,
            exteriorComponentID, houseExteriorWmoDataID, faction, fixtureOverrides, rootOverrides);

        // Spawn room entity + component mesh with Geobox for this plot.
        // The client uses the MeshObject Geobox to validate decor placement bounds.
        // Without this, ALL placement attempts fail with OutsidePlotBounds.
        SpawnRoomForPlot(plotIndex, pos, rot, plotInfo->HouseGuid);

        // Group A house-exterior Entity mirrors — one per visible exterior
        // fixture MeshObject (Base/Roof/Door/Window — ExteriorComponent Type
        // 9/10/11/12). All four share AttachParent = Housing/2 room identity
        // (sniff idx 9984 in dump_12.0.1.66838_2026-04-15_09-35-59 decoded
        // every Group A mirror's AttachParent as `01 c1 XX 12 40 dc` —
        // subType=2 HousingRoom, arg2=18 HouseRoomID). The Type-9 root piece
        // (pieceIndex 0) carries Tag_HouseExteriorPiece + Tag_HouseExteriorRoot
        // and its GUID is the canonical "house mirror GUID" referenced by
        // FHousingPlayerHouse_C.EntityGUID; the others carry Piece only.
        //
        // The HousingPlayerHouse has no position itself; the room entity
        // carries the plot's world position so chaining via AttachParent
        // resolves to real coordinates for the world-map icon picker.
        // SpawnRoomForPlot was just called above and registered the room
        // identity in _roomIdentityGuids[plotIndex].
        {
            ObjectGuid roomParentGuid = GetRoomIdentityGuid(plotIndex);
            if (roomParentGuid.IsEmpty())
            {
                roomParentGuid = plotInfo->HouseGuid;
                TC_LOG_ERROR("housing", "HousingMap::SpawnHouseForPlot: no Housing/2 identity for plot {}; "
                    "mirror AttachParent falls back to HouseGuid (icon may not render)", plotIndex);
            }

            std::vector<std::unique_ptr<HousingMirrorEntity>>& mirrors = _houseMirrorEntities[plotIndex];
            mirrors.clear();
            uint32 const bnetId = static_cast<uint32>(plotInfo->OwnerBnetGuid.GetCounter());
            uint8 pieceIndex = 0;
            QuaternionData identity;
            identity.x = identity.y = identity.z = 0.0f;
            identity.w = 1.0f;
            // All Group A mirrors share the room identity as AttachParent and
            // use (0,0,0) local pos — the room is positioned at the plot
            // centre, so the chain resolves there for every piece. Without a
            // fresh sniff parse showing exact non-root offsets, sharing the
            // root's pos preserves the icon-resolution behaviour we already
            // have for index 0; the additional pieces are pure registry
            // entries used by the client for spatial categorisation.
            Position const localPos(0.0f, 0.0f, 0.0f, 0.0f);

            auto meshItr = _meshObjects.find(plotIndex);
            if (meshItr != _meshObjects.end())
            {
                for (ObjectGuid const& meshGuid : meshItr->second)
                {
                    MeshObject* mesh = GetMeshObject(meshGuid);
                    if (!mesh || !mesh->m_housingFixtureData.has_value())
                        continue;
                    UF::HousingFixtureData const& fd = *mesh->m_housingFixtureData;
                    uint8 const compType = uint8(fd.ExteriorComponentType);
                    if (compType < 9 || compType > 12)
                        continue;

                    ObjectGuid mirrorGuid = MakeHouseMirrorGuid(plotIndex, bnetId, pieceIndex);
                    auto mirror = std::make_unique<HousingMirrorEntity>(this, mirrorGuid);
                    HousingMirrorEntity::Tagging const tagging = (compType == 9)
                        ? HousingMirrorEntity::Tagging::PieceAndRoot
                        : HousingMirrorEntity::Tagging::Piece;
                    mirror->InitPositionData(roomParentGuid,
                        localPos, identity, /*scale*/ 1.0f, /*attachmentFlags*/ 3,
                        tagging);
                    TC_LOG_DEBUG("housing", "HousingMap::SpawnHouseForPlot: spawned Group A mirror[{}] {} "
                        "for plot {} (attach={} [room], type={}, tag={})",
                        pieceIndex, mirrorGuid.ToString(), plotIndex, roomParentGuid.ToString(),
                        compType, compType == 9 ? "PieceAndRoot" : "Piece");
                    mirrors.push_back(std::move(mirror));
                    ++pieceIndex;
                }
            }

            if (mirrors.empty())
            {
                TC_LOG_ERROR("housing", "HousingMap::SpawnHouseForPlot: no fixture MeshObjects found for plot {}; "
                    "Group A mirrors skipped — FHousingPlayerHouse_C.EntityGUID will dangle and the "
                    "world-map icon will not resolve", plotIndex);
            }
            else
            {
                TC_LOG_DEBUG("housing", "HousingMap::SpawnHouseForPlot: emitted {} Group A mirrors for plot {} "
                    "(plotWorldPos=({:.2f},{:.2f},{:.2f}))",
                    mirrors.size(), plotIndex,
                    pos.GetPositionX(), pos.GetPositionY(), pos.GetPositionZ());
            }
        }

        // Group B Entity mirrors — FMirroredPositionData_C only (no tags),
        // one per visible exterior fixture MeshObject (Base/Roof/Door/Window —
        // ExteriorComponent Type 9/10/11/12). AttachParent = the piece's own
        // MeshObject. Sniff idx 9984 shows 4 Group B mirrors per plot, one
        // anchored to each fixture mesh. Without these per-piece anchors the
        // client lacks spatial hooks for door-hover detection and expert-mode
        // placement preview off non-root meshes.
        {
            std::vector<std::unique_ptr<HousingMirrorEntity>>& mirrors = _houseMeshMirrorEntities[plotIndex];
            mirrors.clear();
            uint32 const bnetId = static_cast<uint32>(plotInfo->OwnerBnetGuid.GetCounter());
            uint8 pieceIndex = 0;
            QuaternionData identity;
            identity.x = identity.y = identity.z = 0.0f;
            identity.w = 1.0f;
            Position const localPos(0.0f, 0.0f, 0.0f, 0.0f);

            auto meshItr = _meshObjects.find(plotIndex);
            if (meshItr != _meshObjects.end())
            {
                for (ObjectGuid const& meshGuid : meshItr->second)
                {
                    MeshObject* mesh = GetMeshObject(meshGuid);
                    if (!mesh || !mesh->m_housingFixtureData.has_value())
                        continue;
                    UF::HousingFixtureData const& fd = *mesh->m_housingFixtureData;
                    uint8 const compType = uint8(fd.ExteriorComponentType);
                    // Pair Group B mirrors with the visible fixture types only:
                    // 9=Base, 10=Roof, 11=Door, 12=Window. Other component types
                    // (decorative subpieces, hooks) don't get retail-side mirrors.
                    if (compType < 9 || compType > 12)
                        continue;

                    ObjectGuid mirrorGuid = MakeHouseMeshMirrorGuid(plotIndex, bnetId, pieceIndex);
                    auto mirror = std::make_unique<HousingMirrorEntity>(this, mirrorGuid);
                    mirror->InitPositionData(meshGuid,
                        localPos, identity, /*scale*/ 1.0f, /*attachmentFlags*/ 3,
                        HousingMirrorEntity::Tagging::None);
                    TC_LOG_DEBUG("housing", "HousingMap::SpawnHouseForPlot: spawned Group B mirror[{}] {} "
                        "for plot {} (attach={} [mesh type={}])",
                        pieceIndex, mirrorGuid.ToString(), plotIndex, meshGuid.ToString(), compType);
                    mirrors.push_back(std::move(mirror));
                    ++pieceIndex;
                }
            }

            if (mirrors.empty())
            {
                TC_LOG_WARN("housing", "HousingMap::SpawnHouseForPlot: no fixture MeshObjects found for plot {}; "
                    "Group B mirrors skipped (client spatial anchors off house meshes will be missing)", plotIndex);
            }
            else
            {
                TC_LOG_DEBUG("housing", "HousingMap::SpawnHouseForPlot: emitted {} Group B mirrors for plot {}",
                    mirrors.size(), plotIndex);
            }
        }
    }

    // Door GO spawning is now handled blizzlike inside SpawnExtCompTree:
    // When a door-type MeshObject (ExteriorComponentType=11) with GameObjectID > 0 is
    // spawned as part of the fixture tree, SpawnExtCompTree automatically creates the
    // interactive GO at the door mesh's world position. This removes the dependency on
    // fixtureOverrides for door spawning — the door GO spawns whenever its MeshObject
    // spawns, regardless of whether the player has used the fixture editor.
    //
    // The door GO is stored in _houseGameObjects[plotIndex] by SpawnExtCompTree.
    // Return it here for callers that need the pointer.
    //
    // The missing `return` here was causing C4715 warnings at every build AND a
    // crash in HandleNeighborhoodBuyHouse (2026-04-23 14:23:30) — callers got
    // whatever garbage sat in RAX, which sometimes looked like a non-null GameObject*
    // pointer. `houseGo->GetGUID().ToString()` then dereferenced bad memory and
    // SIGSEGV'd inside ObjectGuid::GetHigh via the fmt formatter.
    return GetHouseGameObject(plotIndex);
}

void HousingMap::SpawnRoomForPlot(uint8 plotIndex, Position const& housePos,
    QuaternionData const& houseRot, ObjectGuid houseGuid)
{
    // The retail client requires a Room entity with an attached MeshObject that has a Geobox
    // (axis-aligned bounding box) to define the plot's placement boundary. Without these,
    // the client reports "OutsidePlotBounds" for ALL decor placement attempts.
    //
    // Sniff-verified structure (12.0.1 retail):
    // 1. Room entity:      FHousingRoom_C fragment with HouseGUID, HouseRoomID=18, Flags=1
    // 2. Room MeshObject:  FHousingRoomComponentMesh_C + Geobox attached to room at local (0,0,0)
    //    Geobox bounds are identical for ALL factions/themes: (-35,-30,-1.01)→(35,30,125.01)
    //
    // In retail, the Room entity is a lightweight entity (type 18) without CGObject/FMeshObjectData_C.
    // Our implementation uses MeshObjects for both, adding room data as extra entity fragments.
    // The client processes fragments independently, so the extra fragments are harmless.

    int32 HOUSE_ROOM_ID = static_cast<int32>(sHousingMgr.GetBaseRoomEntryId());
    static constexpr int32 ROOM_FLAGS    = 1;     // BASE_ROOM
    static constexpr int32 ROOM_COMPONENT_ID = 196; // Sniff-verified: same for alliance+horde

    // Clean up any existing room entities for this plot
    DespawnRoomForPlot(plotIndex);

    // --- DB2 lookups for room component data ---

    // 1. HouseRoom → RoomWmoDataID
    HouseRoomEntry const* houseRoomEntry = sHouseRoomStore.LookupEntry(HOUSE_ROOM_ID);
    int32 roomWmoDataID = houseRoomEntry ? houseRoomEntry->RoomWmoDataID : 0;

    // 2. RoomWmoData → Geobox bounds (bounding box for OutsidePlotBounds check)
    //    Sniff fallback: (-35,-30,-1.01)→(35,30,125.01)
    float geoMinX = -35.0f, geoMinY = -30.0f, geoMinZ = -1.01f;
    float geoMaxX =  35.0f, geoMaxY =  30.0f, geoMaxZ = 125.01f;
    RoomWmoDataEntry const* wmoData = roomWmoDataID ? sRoomWmoDataStore.LookupEntry(roomWmoDataID) : nullptr;
    if (wmoData)
    {
        geoMinX = wmoData->BoundingBoxMinX;
        geoMinY = wmoData->BoundingBoxMinY;
        geoMinZ = wmoData->BoundingBoxMinZ;
        geoMaxX = wmoData->BoundingBoxMaxX;
        geoMaxY = wmoData->BoundingBoxMaxY;
        geoMaxZ = wmoData->BoundingBoxMaxZ;
    }
    else
    {
        TC_LOG_WARN("housing", "HousingMap::SpawnRoomForPlot: No RoomWmoData for roomWmoDataID={} "
            "(plot {}), using fallback geobox (-35,-30,-1.01)->(35,30,125.01)",
            roomWmoDataID, plotIndex);
    }

    // 3. RoomComponent → ModelFileDataID, Type
    //    Sniff fallback: FileDataID=6322976, Type=2
    int32 fileDataID = 6322976;
    uint8 roomComponentType = 2;
    RoomComponentEntry const* compEntry = sRoomComponentStore.LookupEntry(ROOM_COMPONENT_ID);
    if (compEntry)
    {
        if (compEntry->ModelFileDataID > 0)
            fileDataID = compEntry->ModelFileDataID;
        roomComponentType = compEntry->Type;
    }

    // 4. RoomComponentOption → theme-specific cosmetic data (varies by faction/house style)
    //    Alliance sniff: optionID=874, themeID=1 (Folk), field24=0, textureID=3
    //    Horde sniff:    optionID=420, themeID=2 (Rugged), field24=0, textureID=40
    //    These are cosmetic only (don't affect Geobox/bounds check).
    int32 roomComponentOptionID = 0;
    int32 houseThemeID = 0;
    int32 roomComponentTextureID = 0;
    int32 field24 = 0;

    // Use faction-aware theme lookup via MeshStyleFilterID
    int32 factionThemeID = _neighborhood
        ? sHousingMgr.GetFactionDefaultThemeID(_neighborhood->GetFactionRestriction())
        : 1; // Folk (Alliance default)
    int32 compMeshStyleFilterID = compEntry ? compEntry->MeshStyleFilterID : 48;
    RoomComponentOptionEntry const* optEntry = sHousingMgr.FindRoomComponentOption(compMeshStyleFilterID, factionThemeID);
    if (optEntry)
    {
        roomComponentOptionID = static_cast<int32>(optEntry->ID);
        houseThemeID = optEntry->HouseThemeID;
        field24 = static_cast<int32>(optEntry->SubType);
    }
    // If no DB2 entry found, use sniff-verified alliance defaults
    if (roomComponentOptionID == 0)
    {
        roomComponentOptionID = 874;
        houseThemeID = 1;
        field24 = 0;
        roomComponentTextureID = 3;
    }

    TC_LOG_ERROR("housing", "HousingMap::SpawnRoomForPlot: plot={} DB2 lookup: "
        "roomWmoDataID={} geobox=({:.2f},{:.2f},{:.2f})→({:.2f},{:.2f},{:.2f}) "
        "fileDataID={} compType={} optionID={} themeID={} field24={} textureID={}",
        plotIndex, roomWmoDataID,
        geoMinX, geoMinY, geoMinZ, geoMaxX, geoMaxY, geoMaxZ,
        fileDataID, roomComponentType,
        roomComponentOptionID, houseThemeID, field24, roomComponentTextureID);

    // --- Create entities (retail-matching blizzlike architecture) ---
    //
    // Retail idx 9984 (dump_12.0.1.66838_2026-04-15_09-35-59) architecture:
    //   - Housing/2 identity entity (HighGuid::Housing subType=2, objType=18)
    //     fragments: [FHousingRoom_C, FMirroredPositionData_C, Tag_HousingRoom]
    //     carries HouseGUID, HouseRoomID=18, Flags, FloorIndex, Doors, MeshObjects
    //   - Component MeshObject (HighGuid::MeshObject, objType=14)
    //     fragments: [CGObject, FMeshObjectData_C, FHousingRoomComponentMesh_C,
    //                 FMirroredPositionData_C, Tag_MeshObject]
    //     MovementUpdate.TransportGuid = Housing/2 identity GUID
    //     carries the Geobox (used by client for OutsidePlotBounds check)
    //
    // The previous dual-MeshObject architecture (a roomEntity MeshObject with
    // FHousingRoom_C AND a componentMesh with FHousingRoomComponentMesh_C) put
    // two entities at the plot position both claiming HouseRoomID=18. Client
    // room registry accepted only the first, dropping the other's Geobox chain
    // → OutsidePlotBounds + ownership categorization breakage (audit 2026-04-22).

    // 1. Housing/2 identity entity — the single authoritative room for this plot.
    ObjectGuid roomIdentityGuid = ObjectGuid::Create<HighGuid::Housing>(
        /*subType*/ 2,
        /*arg1*/ 0,
        /*arg2*/ static_cast<uint32>(HOUSE_ROOM_ID),
        /*counter*/ static_cast<ObjectGuid::LowType>(plotIndex + 1));

    HousingRoomEntity* roomIdentity = new HousingRoomEntity();
    if (!roomIdentity->Create(roomIdentityGuid, this, housePos))
    {
        TC_LOG_ERROR("housing", "HousingMap::SpawnRoomForPlot: Failed to create Housing/2 identity room for plot {}", plotIndex);
        delete roomIdentity;
        return;
    }

    roomIdentity->SetHouseGUID(houseGuid);
    roomIdentity->SetHouseRoomID(HOUSE_ROOM_ID);
    roomIdentity->SetFlags(ROOM_FLAGS);
    roomIdentity->SetFloorIndex(0);

    // CRITICAL (audit 2026-04-22 byte-for-byte retail comparison, idx 9984):
    // When AttachParent is Empty, retail sets PositionLocalSpace = the room's
    // WORLD position (not zero) and RotationLocalSpace = the house's facing
    // quaternion. The client uses this as the root of the attach chain — any
    // child entity (decor, mirrors) resolves its world pos as
    //   worldPos = room.PositionLocalSpace + rotate(child.localPos, room.rot)
    // If we set room.PositionLocalSpace=(0,0,0), every attached decor ends up
    // near world origin (invisible) and the client's OutsidePlotBounds check
    // fails for every placement because the player's world position is far
    // from the "plot center" the client derives from the room chain.
    roomIdentity->SetMirroredPosition(housePos, houseRot,
        /*scale*/ 1.0f, ObjectGuid::Empty, /*attachFlags*/ 3);

    // 1b. Doors on the identity entity (not on the component mesh).
    // Client's HousingRoomSystem DoorConnection handler reads this array.
    if (std::vector<RoomDoorInfo> const* doors = sHousingMgr.GetRoomDoors(roomWmoDataID))
    {
        for (RoomDoorInfo const& door : *doors)
        {
            Position doorOffset(door.OffsetPos[0], door.OffsetPos[1], door.OffsetPos[2], 0.0f);
            roomIdentity->AddDoor(static_cast<int32>(door.RoomComponentID), doorOffset,
                /*connectionType*/ static_cast<uint8>(7) /*HOUSING_ROOM_COMPONENT_DOORWAY*/,
                /*attachedRoomGuid*/ ObjectGuid::Empty);
        }
        TC_LOG_DEBUG("housing", "HousingMap::SpawnRoomForPlot: plot={} added {} door entries to Housing/2 identity from roomWmoDataID={}",
            plotIndex, uint32(doors->size()), roomWmoDataID);
    }
    else
    {
        TC_LOG_WARN("housing", "HousingMap::SpawnRoomForPlot: plot={} no door data for roomWmoDataID={} — fixture hookpoints will show 'None'",
            plotIndex, roomWmoDataID);
    }

    // 2. Component MeshObject — attaches to Housing/2 identity. Carries the
    // Geobox via FHousingRoomComponentMesh_C. Does NOT carry FHousingRoom_C
    // (that's the identity's job). Retail-verified: component mesh fragment
    // list is [CGObject, FMeshObjectData_C, FHousingRoomComponentMesh_C,
    // FMirroredPositionData_C, Tag_MeshObject] — no FHousingRoom_C.
    Position componentPos(0.0f, 0.0f, 0.0f, 0.0f);
    QuaternionData componentRot;
    componentRot.x = 0.0f;
    componentRot.y = 0.0f;
    componentRot.z = 0.0f;
    componentRot.w = 1.0f;

    MeshObject* componentMesh = MeshObject::CreateMeshObject(this, componentPos, componentRot, 1.0f,
        fileDataID, /*isWMO*/ true,
        /*attachParent*/ roomIdentityGuid, /*attachFlags*/ 3, &housePos);

    if (!componentMesh)
    {
        TC_LOG_ERROR("housing", "HousingMap::SpawnRoomForPlot: Failed to create room component mesh for plot {}", plotIndex);
        roomIdentity->AddObjectToRemoveList();
        return;
    }

    PhasingHandler::InitDbPhaseShift(componentMesh->GetPhaseShift(), PHASE_USE_FLAGS_ALWAYS_VISIBLE, 0, 0);
    componentMesh->InitHousingRoomComponentData(roomIdentityGuid,
        roomComponentOptionID, ROOM_COMPONENT_ID,
        roomComponentType, field24, /*field20*/ 0,
        houseThemeID, roomComponentTextureID,
        /*roomComponentTypeParam*/ 0,
        geoMinX, geoMinY, geoMinZ,
        geoMaxX, geoMaxY, geoMaxZ);

    // 3. Link: add component GUID to identity's MeshObjects array.
    roomIdentity->AddMeshObject(componentMesh->GetGUID());

    // 4. Add component to map. Identity was already AddToMap'd via its Create().
    if (!AddToMap(componentMesh))
    {
        TC_LOG_ERROR("housing", "HousingMap::SpawnRoomForPlot: Failed to add room component mesh to map for plot {}", plotIndex);
        roomIdentity->AddObjectToRemoveList();
        delete componentMesh;
        return;
    }

    _roomIdentityGuids[plotIndex] = roomIdentityGuid;
    _roomComponentMeshes[plotIndex] = componentMesh->GetGUID();
    // _roomEntities[plotIndex] is intentionally NOT populated — the MeshObject
    // "room entity" no longer exists in the unified architecture. Callers that
    // used _roomEntities should now prefer _roomIdentityGuids.
    _roomEntities[plotIndex] = roomIdentityGuid; // keep the map populated with the identity so legacy callers still see something sensible

    TC_LOG_ERROR("housing", "HousingMap::SpawnRoomForPlot: plot={} identity={} component={} "
        "at ({:.1f},{:.1f},{:.1f}) geobox=({:.2f},{:.2f},{:.2f})->({:.2f},{:.2f},{:.2f})",
        plotIndex, roomIdentityGuid.ToString(), componentMesh->GetGUID().ToString(),
        housePos.GetPositionX(), housePos.GetPositionY(), housePos.GetPositionZ(),
        geoMinX, geoMinY, geoMinZ, geoMaxX, geoMaxY, geoMaxZ);
}

void HousingMap::DespawnRoomForPlot(uint8 plotIndex)
{
    // Component mesh first (it attaches to the identity).
    auto compItr = _roomComponentMeshes.find(plotIndex);
    if (compItr != _roomComponentMeshes.end())
    {
        if (MeshObject* mesh = GetMeshObject(compItr->second))
            mesh->AddObjectToRemoveList();
        _roomComponentMeshes.erase(compItr);
    }

    // Housing/2 identity entity.
    auto identItr = _roomIdentityGuids.find(plotIndex);
    if (identItr != _roomIdentityGuids.end())
    {
        if (HousingRoomEntity* room = GetObjectsStore().Find<HousingRoomEntity>(identItr->second))
            room->AddObjectToRemoveList();
        _roomIdentityGuids.erase(identItr);
    }

    // Legacy _roomEntities tracking — now points at the same identity GUID,
    // entity already removed above; just clear the map.
    _roomEntities.erase(plotIndex);
}

MeshObject* HousingMap::SpawnHouseMeshObject(uint8 plotIndex, int32 fileDataID, bool isWMO,
    Position const& pos, QuaternionData const& rot, float scale,
    ObjectGuid houseGuid, int32 exteriorComponentID, int32 houseExteriorWmoDataID,
    uint8 exteriorComponentType /*= 9*/, uint8 houseSize /*= 2*/, int32 exteriorComponentHookID /*= -1*/,
    ObjectGuid attachParent /*= ObjectGuid::Empty*/, uint8 attachFlags /*= 0*/,
    Position const* worldPos /*= nullptr*/)
{
    // For child pieces, worldPos contains the parent's world position for grid placement.
    // Use it for LoadGrid so the grid cell near the house is loaded (not the local-space origin).
    if (worldPos)
        LoadGrid(worldPos->GetPositionX(), worldPos->GetPositionY());
    else
        LoadGrid(pos.GetPositionX(), pos.GetPositionY());

    MeshObject* mesh = MeshObject::CreateMeshObject(this, pos, rot, scale, fileDataID, isWMO,
        attachParent, attachFlags, worldPos);
    if (!mesh)
    {
        TC_LOG_ERROR("housing", "HousingMap::SpawnHouseMeshObject: CreateMeshObject failed for plot {} fileDataID {}",
            plotIndex, fileDataID);
        return nullptr;
    }

    // Set up all entity fragments BEFORE AddToMap (create packet is sent during AddToMap)
    // The base piece (componentType=9, no parent) gets Tag_HouseExteriorRoot (225).
    // All other pieces (roof, door, chimney, windows) get Tag_HouseExteriorPiece (224).
    // The client uses Tag_HouseExteriorRoot to identify the fixture GUID for edit mode.
    bool isRoot = (exteriorComponentType == 9) && attachParent.IsEmpty();

    // Generate a unique fixture GUID per fixture. The client uses FHousingFixture_C::Guid
    // to identify individual fixtures — if all fixtures share the same GUID (houseGuid),
    // Retail sniff-verified: the Guid field (field 5) = the MeshObject's own GUID,
    // and AttachParentGUID (field 4) = the parent MeshObject's GUID (NOT Housing GUIDs).
    // The client's fixture manager searches the frame tree using AttachParentGUID as the
    // key (sub_7FF72697BE70). Frames are indexed by the parent MeshObject entity's GUID.
    // Using Housing GUIDs here causes a key mismatch → fixture-hookpoint linking fails.
    ObjectGuid fixtureGuid = mesh->GetGUID(); // self-reference: the MeshObject's own GUID

    ObjectGuid parentFixtureGuid;
    if (!attachParent.IsEmpty())
    {
        if (MeshObject* parentMesh = GetMeshObject(attachParent))
            parentFixtureGuid = parentMesh->GetGUID(); // parent's MeshObject GUID
    }

    mesh->InitHousingFixtureData(houseGuid, fixtureGuid, parentFixtureGuid,
        exteriorComponentID, houseExteriorWmoDataID,
        exteriorComponentType, houseSize, exteriorComponentHookID, isRoot);

    // Now add to map — this triggers the create packet with all fragments included
    if (!AddToMap(mesh))
    {
        TC_LOG_ERROR("housing", "HousingMap::SpawnHouseMeshObject: AddToMap failed for plot {} fileDataID {}",
            plotIndex, fileDataID);
        delete mesh;
        return nullptr;
    }

    _meshObjects[plotIndex].push_back(mesh->GetGUID());

    TC_LOG_DEBUG("housing", "HousingMap::SpawnHouseMeshObject: plot={} guid={} fileDataID={} isWMO={} "
        "localPos=({:.1f}, {:.1f}, {:.1f}) gridPos=({:.1f}, {:.1f}, {:.1f}) exteriorComponentID={} wmoDataID={}",
        plotIndex, mesh->GetGUID().ToString(), fileDataID, isWMO,
        pos.GetPositionX(), pos.GetPositionY(), pos.GetPositionZ(),
        mesh->GetPositionX(), mesh->GetPositionY(), mesh->GetPositionZ(),
        exteriorComponentID, houseExteriorWmoDataID);

    return mesh;
}

void HousingMap::SpawnFullHouseMeshObjects(uint8 plotIndex, Position const& housePos,
    QuaternionData const& houseRot, ObjectGuid houseGuid,
    int32 exteriorComponentID, int32 houseExteriorWmoDataID,
    int32 factionRestriction /*= NEIGHBORHOOD_FACTION_ALLIANCE*/,
    FixtureOverrideMap const* fixtureOverrides /*= nullptr*/,
    RootOverrideMap const* rootOverrides /*= nullptr*/)
{
    // === DATA-DRIVEN EXTERIOR SPAWNING ===
    // Build the house from DB2 ExteriorComponent tree.
    // A house consists of multiple independent root components (Base type=9, Roof type=10, etc.)
    // all sharing the same HouseExteriorWmoDataID. Each root is spawned independently at the
    // house position, and each has its own hook children (doors on base, chimney/windows on roof).
    //
    // Root selection per type:
    //   1. Check rootOverrides (player's explicit choice for that type)
    //   2. Use coreExtCompID for the core type
    //   3. Fall back to DB2 default (Flags & 0x1 IsDefault)

    uint32 coreExtCompID = static_cast<uint32>(exteriorComponentID);
    ExteriorComponentEntry const* coreComp = sExteriorComponentStore.LookupEntry(coreExtCompID);

    if (coreComp && coreComp->ModelFileDataID > 0 && coreComp->HouseExteriorWmoDataID > 0)
    {
        uint32 wmoDataID = coreComp->HouseExteriorWmoDataID;
        auto const* rootComps = sHousingMgr.GetRootComponentsForWmoData(wmoDataID);
        uint32 totalSpawned = 0;

        if (rootComps)
        {
            // Group roots by type, filtering to match the house's Size
            uint8 houseSize = coreComp->Size;
            std::unordered_map<uint8 /*type*/, std::vector<uint32>> rootsByType;
            for (uint32 rootID : *rootComps)
            {
                ExteriorComponentEntry const* rc = sExteriorComponentStore.LookupEntry(rootID);
                if (rc && rc->ModelFileDataID > 0 && rc->Size == houseSize)
                    rootsByType[rc->Type].push_back(rootID);
            }

            for (auto const& [type, compIDs] : rootsByType)
            {
                uint32 selectedCompID = 0;

                // 1. Check player's root overrides for this type
                if (rootOverrides)
                {
                    auto ovrItr = rootOverrides->find(type);
                    if (ovrItr != rootOverrides->end())
                        selectedCompID = ovrItr->second;
                    else
                    {
                        // Type not in fixtures DB → not unlocked yet, skip it
                        TC_LOG_DEBUG("housing", "SpawnFullHouseMeshObjects: Skipping root type={} — not in fixtures",
                            type);
                        continue;
                    }
                }

                // 2. For the core type, use the player's selected coreExtCompID
                if (!selectedCompID && type == coreComp->Type)
                    selectedCompID = coreExtCompID;

                // 3. Fall back to DB2 default for this type + wmoDataID
                if (!selectedCompID)
                {
                    uint32 defaultID = sHousingMgr.GetDefaultFixtureForType(type, wmoDataID);
                    if (defaultID)
                        selectedCompID = defaultID;
                }

                // 4. Last resort: first available in the list
                if (!selectedCompID && !compIDs.empty())
                    selectedCompID = compIDs[0];

                if (selectedCompID)
                {
                    ExteriorComponentEntry const* selComp = sExteriorComponentStore.LookupEntry(selectedCompID);
                    TC_LOG_INFO("housing", "SpawnFullHouseMeshObjects: Spawning root type={} comp={} '{}' "
                        "(wmoDataID={}, size={}, ModelFDID={})",
                        type, selectedCompID,
                        selComp && selComp->Name[DEFAULT_LOCALE] ? selComp->Name[DEFAULT_LOCALE] : "",
                        wmoDataID, houseSize, selComp ? selComp->ModelFileDataID : 0);

                    totalSpawned += SpawnExtCompTree(plotIndex, selectedCompID,
                        housePos, houseRot,
                        houseGuid, houseExteriorWmoDataID,
                        ObjectGuid::Empty, nullptr, 0, fixtureOverrides);
                }
            }
        }

        if (totalSpawned > 0)
        {
            TC_LOG_INFO("housing", "HousingMap::SpawnFullHouseMeshObjects: Data-driven spawn "
                "for plot {} wmoDataID {} coreComp {} — {} total MeshObjects (faction={})",
                plotIndex, wmoDataID, coreExtCompID, totalSpawned,
                factionRestriction == NEIGHBORHOOD_FACTION_ALLIANCE ? "Alliance" : "Horde");
            return;
        }

        TC_LOG_ERROR("housing", "HousingMap::SpawnFullHouseMeshObjects: Data-driven spawn "
            "yielded 0 meshes for plot {} wmoDataID {} coreComp {} — no DB2 data available",
            plotIndex, wmoDataID, coreExtCompID);
        return;
    }
    else if (!coreComp)
    {
        TC_LOG_ERROR("housing", "HousingMap::SpawnFullHouseMeshObjects: ExteriorComponent {} not found "
            "— cannot spawn house for plot {}", exteriorComponentID, plotIndex);
    }

    // === HARDCODED FALLBACK ===
    if (factionRestriction == NEIGHBORHOOD_FACTION_HORDE)
    {
        SpawnHordeHouseMeshObjects(plotIndex, housePos, houseRot, houseGuid,
            exteriorComponentID, houseExteriorWmoDataID);
        return;
    }

    // === ALLIANCE EXTERIOR (Stucco Small) ===
    // Two root pieces (base + roof) at the house position, children with local-space offsets.
    //
    // Structure:
    //   Root 0 (base, 6648736) - ExteriorComponentID 141, type 9 (Base)
    //     └── Child: Door (7450804), ExteriorComponentID 1380, type 11, hookID 2505
    //   Root 1 (roof, 7420602) - ExteriorComponentID 1503, type 10 (Roof)
    //     ├── Child: Chimney (7118952), hookID 14931
    //     ├── Child: Window back-left (7450830), hookID 17202
    //     └── Child: Window back-right (7450830), hookID 14929

    // Spawn root piece 0: Base structure (uses the passed exteriorComponentID, default 141 = Stucco Base)
    MeshObject* basePiece = SpawnHouseMeshObject(plotIndex, 6648736, /*isWMO*/ true,
        housePos, houseRot, 1.0f,
        houseGuid, exteriorComponentID, houseExteriorWmoDataID,
        /*exteriorComponentType*/ 9, /*houseSize*/ 2, /*hookID*/ -1,
        ObjectGuid::Empty, /*attachFlags*/ 0);

    // Spawn root piece 1: Roof (retail sniff: ExteriorComponentID 1503, type 10, fileDataID 7420602)
    MeshObject* roofPiece = SpawnHouseMeshObject(plotIndex, 7420602, /*isWMO*/ true,
        housePos, houseRot, 1.0f,
        houseGuid, 1503, houseExteriorWmoDataID,
        /*exteriorComponentType*/ 10, /*houseSize*/ 2, /*hookID*/ -1,
        ObjectGuid::Empty, /*attachFlags*/ 0);

    // Child of base: Door mesh (retail sniff: ExteriorComponentID 1380, type 11, hookID 2505)
    // Position and rotation from retail sniff — local space relative to base.
    if (basePiece)
    {
        ObjectGuid baseGuid = basePiece->GetGUID();

        SpawnHouseMeshObject(plotIndex, 7450804, /*isWMO*/ true,
            Position(9.2805f, -3.4555f, -0.5611f, 0.0f),
            QuaternionData(0.0f, 0.0f, 0.0f, 1.0f), 1.0f,
            houseGuid, 1380, houseExteriorWmoDataID,
            /*exteriorComponentType*/ 11, /*houseSize*/ 1, /*hookID*/ 2505,
            baseGuid, /*attachFlags*/ 3, &housePos);
    }

    // Children of roof piece — chimney and windows (local-space positions/rotations)
    if (roofPiece)
    {
        ObjectGuid roofGuid = roofPiece->GetGUID();

        // Chimney (back-left)
        SpawnHouseMeshObject(plotIndex, 7118952, /*isWMO*/ true,
            Position(-3.6472f, -5.6444f, 12.3556f, 0.0f),
            QuaternionData(0.0f, 0.0f, -0.7071066f, 0.70710695f), 1.0f,
            houseGuid, 1452, houseExteriorWmoDataID,
            /*exteriorComponentType*/ 16, /*houseSize*/ 2, /*hookID*/ 14931,
            roofGuid, /*attachFlags*/ 3, &housePos);

        // Window back-left
        SpawnHouseMeshObject(plotIndex, 7450830, /*isWMO*/ true,
            Position(-3.025f, -0.0222f, 11.35f, 0.0f),
            QuaternionData(0.0f, 0.0f, -1.0f, 0.0f), 1.0f,
            houseGuid, 1448, houseExteriorWmoDataID,
            /*exteriorComponentType*/ 14, /*houseSize*/ 2, /*hookID*/ 17202,
            roofGuid, /*attachFlags*/ 3, &housePos);

        // Window back-right
        SpawnHouseMeshObject(plotIndex, 7450830, /*isWMO*/ true,
            Position(3.0305f, -0.0222f, 11.35f, 0.0f),
            QuaternionData(0.0f, 0.0f, 0.0f, 1.0f), 1.0f,
            houseGuid, 1448, houseExteriorWmoDataID,
            /*exteriorComponentType*/ 14, /*houseSize*/ 2, /*hookID*/ 14929,
            roofGuid, /*attachFlags*/ 3, &housePos);
    }

    uint32 meshCount = 0;
    auto meshItr = _meshObjects.find(plotIndex);
    if (meshItr != _meshObjects.end())
        meshCount = static_cast<uint32>(meshItr->second.size());

    TC_LOG_DEBUG("housing", "HousingMap::SpawnFullHouseMeshObjects: Spawned {} alliance MeshObjects for plot {} in neighborhood '{}'",
        meshCount, plotIndex, _neighborhood ? _neighborhood->GetName() : "?");
}

void HousingMap::SpawnHordeHouseMeshObjects(uint8 plotIndex, Position const& housePos,
    QuaternionData const& houseRot, ObjectGuid houseGuid,
    int32 /*exteriorComponentID*/, int32 /*houseExteriorWmoDataID*/)
{
    // === HORDE EXTERIOR ===
    // Sniff-verified: Horde starter house with HouseExteriorWmoDataID=87.
    // Two root pieces at the house position, children with local-space offsets.
    //
    // Parent-child hierarchy:
    //   Root 0 (main structure, 7118906) - ExteriorComponentID 3811, type 10
    //     ├── Child: Door/entrance (7118912), hookID 17245, extCompID 976
    //     ├── Child: Wall element (7460531), hookID -1, extCompID 2476
    //     ├── Child: Wall variant (7118901), hookID -1, extCompID 1011
    //     ├── Child: Roof piece A (7462686), hookID 17294, extCompID 2445
    //     ├── Child: Structure detail (7118918), hookID 17286, extCompID 980
    //     └── Child: Roof piece B (7462686), hookID 17285, extCompID 2445
    //   Root 1 (base, 6648685) - ExteriorComponentID 1003, type 9

    int32 hordeWmoDataID = HORDE_HOUSE_EXTERIOR_WMO_DATA_ID; // 87

    // Root piece 0: Main structure
    MeshObject* rootPiece = SpawnHouseMeshObject(plotIndex, 7118906, /*isWMO*/ true,
        housePos, houseRot, 1.0f,
        houseGuid, 3811, hordeWmoDataID,
        /*exteriorComponentType*/ 10, /*houseSize*/ 2, /*hookID*/ -1,
        ObjectGuid::Empty, /*attachFlags*/ 0);

    // Root piece 1: Base structure
    MeshObject* basePiece = SpawnHouseMeshObject(plotIndex, 6648685, /*isWMO*/ true,
        housePos, houseRot, 1.0f,
        houseGuid, 1003, hordeWmoDataID,
        /*exteriorComponentType*/ 9, /*houseSize*/ 2, /*hookID*/ -1,
        ObjectGuid::Empty, /*attachFlags*/ 0);

    // Children of root piece 0
    if (rootPiece)
    {
        ObjectGuid rootGuid = rootPiece->GetGUID();

        // Door/entrance
        SpawnHouseMeshObject(plotIndex, 7118912, /*isWMO*/ true,
            Position(14.2722f, -8.6194f, 0.0f, 0.0f),
            QuaternionData(0.0f, 0.0f, -0.2873478f, 0.9578263f), 1.0f,
            houseGuid, 976, hordeWmoDataID,
            /*exteriorComponentType*/ 11, /*houseSize*/ 2, /*hookID*/ 17245,
            rootGuid, /*attachFlags*/ 3, &housePos);

        // Wall element
        SpawnHouseMeshObject(plotIndex, 7460531, /*isWMO*/ true,
            Position(0.0f, 0.0f, 0.0f, 0.0f),
            QuaternionData(0.0f, 0.0f, 0.0f, 1.0f), 1.0f,
            houseGuid, 2476, hordeWmoDataID,
            /*exteriorComponentType*/ 12, /*houseSize*/ 2, /*hookID*/ -1,
            rootGuid, /*attachFlags*/ 3, &housePos);

        // Wall variant
        SpawnHouseMeshObject(plotIndex, 7118901, /*isWMO*/ true,
            Position(0.0f, 0.0f, 0.0f, 0.0f),
            QuaternionData(0.0f, 0.0f, 0.0f, 1.0f), 1.0f,
            houseGuid, 1011, hordeWmoDataID,
            /*exteriorComponentType*/ 12, /*houseSize*/ 2, /*hookID*/ -1,
            rootGuid, /*attachFlags*/ 3, &housePos);

        // Roof piece A (right side)
        SpawnHouseMeshObject(plotIndex, 7462686, /*isWMO*/ true,
            Position(6.2889f, -4.4556f, 0.0833f, 0.0f),
            QuaternionData(0.0f, 0.0f, 0.95782566f, 0.28735f), 1.0f,
            houseGuid, 2445, hordeWmoDataID,
            /*exteriorComponentType*/ 13, /*houseSize*/ 2, /*hookID*/ 17294,
            rootGuid, /*attachFlags*/ 3, &housePos);

        // Structure detail
        SpawnHouseMeshObject(plotIndex, 7118918, /*isWMO*/ true,
            Position(-0.1389f, 8.6806f, 5.4139f, 0.0f),
            QuaternionData(0.0f, 0.0f, 0.7071066f, 0.70710695f), 1.0f,
            houseGuid, 980, hordeWmoDataID,
            /*exteriorComponentType*/ 12, /*houseSize*/ 2, /*hookID*/ 17286,
            rootGuid, /*attachFlags*/ 3, &housePos);

        // Roof piece B (left side)
        SpawnHouseMeshObject(plotIndex, 7462686, /*isWMO*/ true,
            Position(-7.0611f, -3.7361f, 0.0833f, 0.0f),
            QuaternionData(0.0f, 0.0f, 0.2873478f, 0.9578263f), 1.0f,
            houseGuid, 2445, hordeWmoDataID,
            /*exteriorComponentType*/ 13, /*houseSize*/ 2, /*hookID*/ 17285,
            rootGuid, /*attachFlags*/ 3, &housePos);
    }

    uint32 meshCount = 0;
    auto meshItr = _meshObjects.find(plotIndex);
    if (meshItr != _meshObjects.end())
        meshCount = static_cast<uint32>(meshItr->second.size());

    TC_LOG_DEBUG("housing", "HousingMap::SpawnHordeHouseMeshObjects: Spawned {} MeshObjects for plot {} in neighborhood '{}' "
        "(root={} base={})",
        meshCount, plotIndex, _neighborhood ? _neighborhood->GetName() : "?",
        rootPiece ? "OK" : "FAIL", basePiece ? "OK" : "FAIL");
}

uint32 HousingMap::SpawnExtCompTree(uint8 plotIndex, uint32 extCompID,
    Position const& pos, QuaternionData const& rot,
    ObjectGuid houseGuid, int32 houseExteriorWmoDataID,
    ObjectGuid parentGuid, Position const* worldPos, int32 depth /*= 0*/,
    FixtureOverrideMap const* fixtureOverrides /*= nullptr*/,
    int32 hookIDOverride /*= -1*/)
{
    if (depth > 10) // safety limit against infinite recursion
        return 0;

    ExteriorComponentEntry const* comp = sExteriorComponentStore.LookupEntry(extCompID);
    if (!comp)
    {
        TC_LOG_ERROR("housing", "HousingMap::SpawnExtCompTree: ExteriorComponent {} not found", extCompID);
        return 0;
    }

    if (comp->ModelFileDataID <= 0)
    {
        TC_LOG_WARN("housing", "HousingMap::SpawnExtCompTree: ExteriorComponent {} has no ModelFileDataID", extCompID);
        return 0;
    }

    // Determine attach flags: root pieces (no parent) use 0, children use 3
    uint8 attachFlags = parentGuid.IsEmpty() ? 0 : 3;

    // The hookIDOverride tells the client which hook point this component occupies.
    // This must propagate at ALL depths (not just depth=0) because during initial house spawn
    // via SpawnFullHouseMeshObjects, hooks are iterated at depth >= 1 with valid hookIDOverride.
    int32 effectiveHookID = (hookIDOverride > 0) ? hookIDOverride : -1;

    MeshObject* mesh = SpawnHouseMeshObject(plotIndex, comp->ModelFileDataID, /*isWMO*/ true,
        pos, rot, 1.0f,
        houseGuid, static_cast<int32>(extCompID), houseExteriorWmoDataID,
        comp->Type, /*houseSize*/ 2, effectiveHookID,
        parentGuid, attachFlags, worldPos);

    if (!mesh)
    {
        TC_LOG_ERROR("housing", "HousingMap::SpawnExtCompTree: Failed to spawn mesh for comp {} "
            "(ModelFileDataID={}) at depth {}",
            extCompID, comp->ModelFileDataID, depth);
        return 0;
    }

    uint32 count = 1;
    ObjectGuid meshGuid = mesh->GetGUID();

    // Compute THIS mesh's cumulative world position by composing the immediate
    // parent's world position with our local-space hook offset, rotated by the
    // parent's cumulative orientation. At depth 0 we are the root: our local
    // pos IS the world position (passed by SpawnFullHouseMeshObjects).
    //
    // This cumulative position is required so that a grand-child's door GO
    // (e.g. house-root → DoorwayWall → door) lands on the visible mesh rather
    // than offset by the wall's distance from the root.
    Position thisMeshWorldPos;
    if (depth == 0 || !worldPos)
    {
        thisMeshWorldPos = pos;
    }
    else
    {
        Position const& parentWorldPos = *worldPos;
        float parentFacing = parentWorldPos.GetOrientation();
        float cf = std::cos(parentFacing);
        float sf = std::sin(parentFacing);
        float wx = parentWorldPos.GetPositionX() + pos.GetPositionX() * cf - pos.GetPositionY() * sf;
        float wy = parentWorldPos.GetPositionY() + pos.GetPositionX() * sf + pos.GetPositionY() * cf;
        float wz = parentWorldPos.GetPositionZ() + pos.GetPositionZ();
        // Extract the Z-axis rotation contribution of the hook quaternion so
        // that grand-children of this mesh apply the correct cumulative yaw.
        // yaw = atan2(2*(w*z + x*y), 1 - 2*(y² + z²))
        float thisZRot = std::atan2(
            2.0f * (rot.w * rot.z + rot.x * rot.y),
            1.0f - 2.0f * (rot.y * rot.y + rot.z * rot.z));
        thisMeshWorldPos.Relocate(wx, wy, wz, parentFacing + thisZRot);
    }

    // --- Blizzlike: If this is a door component (Type=11), spawn the interactive GO ---
    // Retail sniff: door MeshObjects have FHousingFixture_C.GameObjectGUID set to the
    // door GO entry. A separate GameObject (type GOOBER) is spawned at the door mesh's
    // world position for player click interaction. The GO's PositionLocalSpace=(0,0,0)
    // relative to a parent entity at the door's world position.
    if (comp->Type == 11 && comp->GameObjectID > 0)
    {
        uint32 doorGoEntry = static_cast<uint32>(comp->GameObjectID);
        GameObjectTemplate const* doorTemplate = sObjectMgr->GetGameObjectTemplate(doorGoEntry);
        if (doorTemplate)
        {
            // thisMeshWorldPos already carries the door mesh's cumulative world
            // position through every parent hook transform. Earlier revisions
            // computed this as `houseRoot + localPos`, which lost the offset of
            // any intermediate parent (e.g. a DoorwayWall hosting the door) and
            // left the clickable GO a few yards away from the visible mesh.
            float doorWorldX = thisMeshWorldPos.GetPositionX();
            float doorWorldY = thisMeshWorldPos.GetPositionY();
            float doorWorldZ = thisMeshWorldPos.GetPositionZ();
            float doorFacing = thisMeshWorldPos.GetOrientation();
            float cosFacing = std::cos(doorFacing);
            float sinFacing = std::sin(doorFacing);

            // Apply the ExteriorComponentExitPoint offset in DOOR-local space
            // (rotated by the cumulative facing): X/Y push the clickable box in
            // front of the mesh (e.g. "step in front of the door"), Z lifts it
            // onto the porch instead of the mesh base.
            ExteriorComponentExitPointEntry const* exitPt = sHousingMgr.GetExitPoint(extCompID);
            if (exitPt)
            {
                doorWorldX += exitPt->Position[0] * cosFacing - exitPt->Position[1] * sinFacing;
                doorWorldY += exitPt->Position[0] * sinFacing + exitPt->Position[1] * cosFacing;
                doorWorldZ += exitPt->Position[2];
            }

            Position doorPos(doorWorldX, doorWorldY, doorWorldZ, doorFacing);
            QuaternionData doorRot(0, 0, 0, 1);

            // Remove any previously-tracked door GO for this plot before we store a
            // new one — otherwise the old GUID reference is lost and the game object
            // leaks on the map (visible as a phantom clickable box at the old door
            // location after the entrance is moved).
            if (auto existingItr = _houseGameObjects.find(plotIndex); existingItr != _houseGameObjects.end())
            {
                if (GameObject* oldGo = GetGameObject(existingItr->second))
                    oldGo->AddObjectToRemoveList();
                _houseGameObjects.erase(existingItr);
            }

            GameObject* doorGo = GameObject::CreateGameObject(doorGoEntry, this, doorPos, doorRot, 255, GO_STATE_READY);
            if (doorGo)
            {
                doorGo->SetFlag(GO_FLAG_NODESPAWN);
                PhasingHandler::InitDbPhaseShift(doorGo->GetPhaseShift(), PHASE_USE_FLAGS_ALWAYS_VISIBLE, 0, 0);

                if (AddToMap(doorGo))
                {
                    _houseGameObjects[plotIndex] = doorGo->GetGUID();
                    TC_LOG_INFO("housing", "SpawnExtCompTree: Door GO spawned blizzlike — entry={} guid={} "
                        "at ({:.1f},{:.1f},{:.1f}) for comp={} (hook local: {:.1f},{:.1f},{:.1f}) exitPt=({:.1f},{:.1f},{:.1f}) plot={}",
                        doorGoEntry, doorGo->GetGUID().ToString(),
                        doorWorldX, doorWorldY, doorWorldZ,
                        extCompID,
                        pos.GetPositionX(), pos.GetPositionY(), pos.GetPositionZ(),
                        exitPt ? exitPt->Position[0] : 0.0f,
                        exitPt ? exitPt->Position[1] : 0.0f,
                        exitPt ? exitPt->Position[2] : 0.0f,
                        plotIndex);
                }
                else
                {
                    TC_LOG_ERROR("housing", "SpawnExtCompTree: Door GO AddToMap FAILED for comp={} plot={}", extCompID, plotIndex);
                    delete doorGo;
                }
            }
            else
            {
                TC_LOG_ERROR("housing", "SpawnExtCompTree: CreateGameObject FAILED for door entry={} comp={}", doorGoEntry, extCompID);
            }
        }
        else
        {
            TC_LOG_ERROR("housing", "SpawnExtCompTree: Door GO template {} NOT FOUND for comp={}", doorGoEntry, extCompID);
        }
    }

    // Children receive THIS mesh's cumulative world position as their parent
    // transform so their local hook offsets compose correctly (was previously
    // always the root's position, which broke grand-children like doors hosted
    // by DoorwayWalls).
    Position const* childWorldPos = &thisMeshWorldPos;

    // Recurse into hooks on this component
    auto const* hooks = sHousingMgr.GetHooksOnComponent(extCompID);
    TC_LOG_INFO("housing", "SpawnExtCompTree: comp={} has {} hooks, fixtureOverrides={}",
        extCompID, hooks ? uint32(hooks->size()) : 0, fixtureOverrides != nullptr);

    // Spawn child components at hooks from player fixture overrides.
    // Door meshes with GameObjectID > 0 automatically spawn their interactive GO above.
    if (hooks)
    {
        for (ExteriorComponentHookEntry const* hook : *hooks)
        {
            if (!hook)
                continue;

            // Only spawn hook children that the player has explicitly selected
            ExteriorComponentEntry const* childComp = nullptr;
            if (fixtureOverrides)
            {
                auto overrideItr = fixtureOverrides->find(hook->ID);
                if (overrideItr != fixtureOverrides->end())
                    childComp = sExteriorComponentStore.LookupEntry(overrideItr->second);
            }
            if (!childComp)
                continue;

            TC_LOG_INFO("housing", "SpawnExtCompTree: parent={} hook={} (type={}) → child comp {} '{}' (ParentComp={}, ModelFDID={})",
                extCompID, hook->ID, hook->ExteriorComponentTypeID, childComp->ID,
                childComp->Name[DEFAULT_LOCALE] ? childComp->Name[DEFAULT_LOCALE] : "",
                childComp->ParentComponentID, childComp->ModelFileDataID);

            // Hook position/rotation are the local-space coordinates where the child
            // mesh attaches on the parent. Use hook position directly as the child's
            // PositionLocalSpace — the client handles the attachment via AttachParentGUID.
            Position hookPos(hook->Position[0], hook->Position[1], hook->Position[2], 0.0f);
            QuaternionData hookRot;
            // Hook rotation is in degrees — convert to quaternion (XYZ extrinsic Euler)
            static constexpr float DEG_TO_RAD = static_cast<float>(M_PI / 180.0);
            float rx = hook->Rotation[0] * DEG_TO_RAD;
            float ry = hook->Rotation[1] * DEG_TO_RAD;
            float rz = hook->Rotation[2] * DEG_TO_RAD;
            float cx = std::cos(rx / 2.0f), sx = std::sin(rx / 2.0f);
            float cy = std::cos(ry / 2.0f), sy = std::sin(ry / 2.0f);
            float cz = std::cos(rz / 2.0f), sz = std::sin(rz / 2.0f);
            hookRot.x = sx * cy * cz - cx * sy * sz;
            hookRot.y = cx * sy * cz + sx * cy * sz;
            hookRot.z = cx * cy * sz - sx * sy * cz;
            hookRot.w = cx * cy * cz + sx * sy * sz;

            count += SpawnExtCompTree(plotIndex, childComp->ID,
                hookPos, hookRot,
                houseGuid, houseExteriorWmoDataID,
                meshGuid, childWorldPos, depth + 1, fixtureOverrides,
                static_cast<int32>(hook->ID));
        }
    }

    // NOTE: ExteriorComponent.ParentComponentID links are color/dye variants of the same shape,
    // NOT structural children. They share the same mesh with a different dye (Field_011).
    // These are NOT spawned as additional meshes — the player's fixture selection determines
    // which variant is used, handled via GetRootComponentOverrides() / fixture overrides.

    return count;
}

void HousingMap::DespawnAllMeshObjectsForPlot(uint8 plotIndex)
{
    auto itr = _meshObjects.find(plotIndex);
    if (itr == _meshObjects.end())
        return;

    for (ObjectGuid const& guid : itr->second)
    {
        if (MeshObject* mesh = GetMeshObject(guid))
            mesh->AddObjectToRemoveList();
    }

    TC_LOG_DEBUG("housing", "HousingMap::DespawnAllMeshObjectsForPlot: Despawned {} MeshObject(s) for plot {}",
        itr->second.size(), plotIndex);
    _meshObjects.erase(itr);
}

MeshObject* HousingMap::FindMeshObjectByHookID(uint8 plotIndex, int32 hookID)
{
    auto itr = _meshObjects.find(plotIndex);
    if (itr == _meshObjects.end())
        return nullptr;

    for (ObjectGuid const& guid : itr->second)
    {
        if (MeshObject* mesh = GetMeshObject(guid))
        {
            if (mesh->GetExteriorComponentHookID() == hookID)
                return mesh;
        }
    }
    return nullptr;
}

void HousingMap::DespawnSingleMeshObject(uint8 plotIndex, ObjectGuid meshGuid)
{
    auto itr = _meshObjects.find(plotIndex);
    if (itr == _meshObjects.end())
        return;

    // Also remove any children attached to this mesh (recursive)
    std::vector<ObjectGuid> toRemove;
    toRemove.push_back(meshGuid);

    // Find children (meshes whose AttachParentGUID == meshGuid)
    for (ObjectGuid const& guid : itr->second)
    {
        if (MeshObject* mesh = GetMeshObject(guid))
        {
            if (mesh->GetAttachParentGUID() == meshGuid)
                toRemove.push_back(guid);
        }
    }

    for (ObjectGuid const& guid : toRemove)
    {
        if (MeshObject* mesh = GetMeshObject(guid))
            mesh->AddObjectToRemoveList();

        auto& vec = itr->second;
        vec.erase(std::remove(vec.begin(), vec.end(), guid), vec.end());
    }

    TC_LOG_DEBUG("housing", "HousingMap::DespawnSingleMeshObject: Removed {} mesh(es) for plot {} (root {})",
        toRemove.size(), plotIndex, meshGuid.ToString());
}

MeshObject* HousingMap::SpawnFixtureAtHook(uint8 plotIndex, uint32 hookID, uint32 componentID,
    ObjectGuid houseGuid, int32 houseExteriorWmoDataID, Player* target)
{
    ExteriorComponentHookEntry const* hookEntry = sExteriorComponentHookStore.LookupEntry(hookID);
    if (!hookEntry)
    {
        TC_LOG_ERROR("housing", "HousingMap::SpawnFixtureAtHook: Hook {} not found in DB2", hookID);
        return nullptr;
    }

    // Find the parent mesh that owns this hook (the hook's ExteriorComponentID is the parent)
    MeshObject* parentMesh = nullptr;
    auto meshItr = _meshObjects.find(plotIndex);
    if (meshItr != _meshObjects.end())
    {
        for (ObjectGuid const& guid : meshItr->second)
        {
            if (MeshObject* mesh = GetMeshObject(guid))
            {
                if (mesh->GetExteriorComponentID() == static_cast<int32>(hookEntry->ExteriorComponentID))
                {
                    parentMesh = mesh;
                    break;
                }
            }
        }
    }

    if (!parentMesh)
    {
        TC_LOG_ERROR("housing", "HousingMap::SpawnFixtureAtHook: Parent mesh for hook {} (parent comp {}) not found on plot {}",
            hookID, hookEntry->ExteriorComponentID, plotIndex);
        return nullptr;
    }

    // Hook position/rotation are local-space offsets relative to the parent
    Position hookPos(hookEntry->Position[0], hookEntry->Position[1], hookEntry->Position[2], 0.0f);
    QuaternionData hookRot;
    static constexpr float DEG_TO_RAD = static_cast<float>(M_PI / 180.0);
    float rx = hookEntry->Rotation[0] * DEG_TO_RAD;
    float ry = hookEntry->Rotation[1] * DEG_TO_RAD;
    float rz = hookEntry->Rotation[2] * DEG_TO_RAD;
    float cx = std::cos(rx / 2.0f), sx = std::sin(rx / 2.0f);
    float cy = std::cos(ry / 2.0f), sy = std::sin(ry / 2.0f);
    float cz = std::cos(rz / 2.0f), sz = std::sin(rz / 2.0f);
    hookRot.x = sx * cy * cz - cx * sy * sz;
    hookRot.y = cx * sy * cz + sx * cy * sz;
    hookRot.z = cx * cy * sz - sx * sy * cz;
    hookRot.w = cx * cy * cz + sx * sy * sz;

    // Use the parent's world position for grid placement
    Position parentWorldPos(parentMesh->GetPositionX(), parentMesh->GetPositionY(),
        parentMesh->GetPositionZ(), parentMesh->GetOrientation());

    // Spawn the component tree at this hook (may have sub-hooks/children).
    // Pass hookID as the override so the top-level mesh gets ExteriorComponentHookID = hookID
    // (the actual hook point where we're installing it, not the component's native HookID from DB2).
    uint32 spawned = SpawnExtCompTree(plotIndex, componentID,
        hookPos, hookRot,
        houseGuid, houseExteriorWmoDataID,
        parentMesh->GetGUID(), &parentWorldPos, 0, nullptr,
        static_cast<int32>(hookID));

    TC_LOG_DEBUG("housing", "HousingMap::SpawnFixtureAtHook: Spawned {} mesh(es) for hook {} component {} on plot {}",
        spawned, hookID, componentID, plotIndex);

    // Send CREATE to the requesting player for the newly spawned meshes
    if (target && spawned > 0 && meshItr != _meshObjects.end())
    {
        UpdateData updateData(GetId());
        // The new meshes are at the end of the vector
        size_t totalMeshes = meshItr->second.size();
        for (size_t i = totalMeshes - spawned; i < totalMeshes; ++i)
        {
            ObjectGuid const& guid = meshItr->second[i];
            if (MeshObject* mesh = GetMeshObject(guid))
            {
                mesh->BuildCreateUpdateBlockForPlayer(&updateData, target);
                target->m_clientGUIDs.insert(guid);
            }
        }
        WorldPacket updatePacket;
        updateData.BuildPacket(&updatePacket);
        target->SendDirectMessage(&updatePacket);
    }

    // Return the first (root) mesh at the hook
    return FindMeshObjectByHookID(plotIndex, static_cast<int32>(hookID));
}

void HousingMap::DespawnHouseForPlot(uint8 plotIndex)
{
    // Despawn room entities, MeshObjects, then the GO
    DespawnRoomForPlot(plotIndex);
    DespawnAllMeshObjectsForPlot(plotIndex);

    // Despawn the Entity mirrors paired with the exterior root.
    _houseMirrorEntities.erase(plotIndex);
    _houseMeshMirrorEntities.erase(plotIndex);

    auto itr = _houseGameObjects.find(plotIndex);
    if (itr == _houseGameObjects.end())
        return;

    if (GameObject* go = GetGameObject(itr->second))
        go->AddObjectToRemoveList();

    TC_LOG_DEBUG("housing", "HousingMap::DespawnHouseForPlot: Despawned house GO for plot {}", plotIndex);
    _houseGameObjects.erase(itr);
}

HousingMirrorEntity* HousingMap::GetHouseMirror(uint8 plotIndex) const
{
    auto itr = _houseMirrorEntities.find(plotIndex);
    if (itr == _houseMirrorEntities.end() || itr->second.empty())
        return nullptr;
    // Returns the Type-9 (Base) root mirror — the canonical mirror referenced
    // by FHousingPlayerHouse_C.EntityGUID. Per-piece access via GetHouseMirrors.
    return itr->second.front().get();
}

ObjectGuid HousingMap::GetHouseMirrorGuid(uint8 plotIndex) const
{
    if (HousingMirrorEntity* m = GetHouseMirror(plotIndex))
        return m->GetGUID();
    return ObjectGuid::Empty;
}

std::vector<HousingMirrorEntity*> HousingMap::GetHouseMirrors(uint8 plotIndex) const
{
    std::vector<HousingMirrorEntity*> result;
    auto itr = _houseMirrorEntities.find(plotIndex);
    if (itr == _houseMirrorEntities.end())
        return result;
    result.reserve(itr->second.size());
    for (auto const& mirror : itr->second)
        result.push_back(mirror.get());
    return result;
}

ObjectGuid HousingMap::MakeHouseMirrorGuid(uint8 plotIndex, uint32 bnetAccountId, uint8 pieceIndex /*= 0*/) const
{
    // Deterministic convention: HighGuid::Entity, mapId=GetId() (neighborhood
    // world map), entry=37361 (synthetic entry for housing mirrors — picked
    // outside the range of creature/gameobject entries in use). Counter packs
    // (bnetAccountId, plotIndex, pieceIndex) so each per-piece Group A mirror
    // has a unique GUID and the mapping stays deterministic across runs.
    // pieceIndex 0 is the Type-9 root mirror (canonical "house mirror GUID"
    // referenced by FHousingPlayerHouse_C.EntityGUID); 1+ are Roof/Door/Window
    // pieces. Same derivation is used by proxy emission for neighbour plots
    // without needing the mirror object to exist.
    constexpr uint32 HOUSING_MIRROR_ENTRY = 37361;
    uint64 counter = (static_cast<uint64>(bnetAccountId) << 16)
                   | (static_cast<uint64>(plotIndex)     << 8)
                   |  static_cast<uint64>(pieceIndex);
    return ObjectGuid::Create<HighGuid::Entity>(GetId(), HOUSING_MIRROR_ENTRY, counter);
}

HousingMirrorEntity* HousingMap::GetHouseMeshMirror(uint8 plotIndex) const
{
    auto itr = _houseMeshMirrorEntities.find(plotIndex);
    if (itr == _houseMeshMirrorEntities.end() || itr->second.empty())
        return nullptr;
    // Returns the first (root-piece) Group B mirror for legacy callers that
    // only need any anchor; per-piece access goes via _houseMeshMirrorEntities.
    return itr->second.front().get();
}

ObjectGuid HousingMap::GetHouseMeshMirrorGuid(uint8 plotIndex) const
{
    if (HousingMirrorEntity* m = GetHouseMeshMirror(plotIndex))
        return m->GetGUID();
    return ObjectGuid::Empty;
}

std::vector<HousingMirrorEntity*> HousingMap::GetHouseMeshMirrors(uint8 plotIndex) const
{
    std::vector<HousingMirrorEntity*> result;
    auto itr = _houseMeshMirrorEntities.find(plotIndex);
    if (itr == _houseMeshMirrorEntities.end())
        return result;
    result.reserve(itr->second.size());
    for (auto const& mirror : itr->second)
        result.push_back(mirror.get());
    return result;
}

ObjectGuid HousingMap::MakeHouseMeshMirrorGuid(uint8 plotIndex, uint32 bnetAccountId, uint8 pieceIndex /*= 0*/) const
{
    // Distinct synthetic entry from Group A (37361) so Group A/B guids never collide.
    // Counter packs the (bnet, plot, piece) triple so each per-piece Group B
    // mirror has a unique GUID across the realm: (bnetId << 16) | (plot << 8) | piece.
    constexpr uint32 HOUSING_MESH_MIRROR_ENTRY = 37362;
    uint64 counter = (static_cast<uint64>(bnetAccountId) << 16)
                   | (static_cast<uint64>(plotIndex)     << 8)
                   |  static_cast<uint64>(pieceIndex);
    return ObjectGuid::Create<HighGuid::Entity>(GetId(), HOUSING_MESH_MIRROR_ENTRY, counter);
}

HousingRoomEntity* HousingMap::GetRoomIdentityEntity(uint8 plotIndex) const
{
    auto itr = _roomIdentityGuids.find(plotIndex);
    if (itr == _roomIdentityGuids.end())
        return nullptr;
    return const_cast<HousingMap*>(this)->GetObjectsStore().Find<HousingRoomEntity>(itr->second);
}

ObjectGuid HousingMap::GetRoomIdentityGuid(uint8 plotIndex) const
{
    auto itr = _roomIdentityGuids.find(plotIndex);
    return itr != _roomIdentityGuids.end() ? itr->second : ObjectGuid::Empty;
}

void HousingMap::DespawnDoorGO(uint8 plotIndex)
{
    auto itr = _houseGameObjects.find(plotIndex);
    if (itr == _houseGameObjects.end())
        return;

    if (GameObject* go = GetGameObject(itr->second))
        go->AddObjectToRemoveList();

    TC_LOG_DEBUG("housing", "HousingMap::DespawnDoorGO: Removed door GO {} for plot {}",
        itr->second.ToString(), plotIndex);
    _houseGameObjects.erase(itr);
}

void HousingMap::RespawnDoorGOAtHook(uint8 plotIndex, uint32 hookID, uint32 doorComponentID, Housing const* housing, Player* player /*= nullptr*/)
{
    // Remove old door GO
    DespawnDoorGO(plotIndex);

    // Resolve the door GO entry from the component's GameObjectID
    ExteriorComponentEntry const* doorComp = sExteriorComponentStore.LookupEntry(doorComponentID);
    if (!doorComp || doorComp->GameObjectID <= 0)
    {
        TC_LOG_ERROR("housing", "HousingMap::RespawnDoorGOAtHook: No GameObjectID for door comp {} at hook {}", doorComponentID, hookID);
        return;
    }
    uint32 doorEntry = static_cast<uint32>(doorComp->GameObjectID);

    GameObjectTemplate const* doorTemplate = sObjectMgr->GetGameObjectTemplate(doorEntry);
    if (!doorTemplate)
    {
        TC_LOG_ERROR("housing", "HousingMap::RespawnDoorGOAtHook: GO template {} not found for door comp {}", doorEntry, doorComponentID);
        return;
    }

    // Get hook position (local offset from house center)
    ExteriorComponentHookEntry const* hookEntry = sExteriorComponentHookStore.LookupEntry(hookID);
    if (!hookEntry)
    {
        TC_LOG_ERROR("housing", "HousingMap::RespawnDoorGOAtHook: Hook {} not found in DB2", hookID);
        return;
    }

    // Get house position for world-space transform.
    // Use the room entity's position (already spawned at the correct world-space coordinates)
    // since DB2 HousePosition Z doesn't match the actual terrain height.
    Position housePos;
    if (housing->HasCustomPosition())
    {
        housePos = housing->GetHousePosition();
    }
    else
    {
        // Try the Housing/2 identity room first — it's spawned at the correct house position.
        if (HousingRoomEntity* roomId = GetRoomIdentityEntity(plotIndex))
            housePos = roomId->GetPosition();

        // Fallback: resolve facing from DB2 (needed for the rotation transform)
        if (housePos.GetOrientation() == 0.0f && _neighborhood)
        {
            auto plots = sHousingMgr.GetPlotsForMap(_neighborhood->GetNeighborhoodMapID());
            for (NeighborhoodPlotData const* pd : plots)
            {
                if (pd && pd->PlotIndex == static_cast<int32>(plotIndex))
                {
                    float hFacing = pd->HouseRotation[2];
                    if (pd->HouseRotation[0] == 0.0f && pd->HouseRotation[1] == 0.0f && pd->HouseRotation[2] == 0.0f)
                        hFacing = std::atan2(pd->CornerstonePosition[1] - pd->HousePosition[1],
                                            pd->CornerstonePosition[0] - pd->HousePosition[0]);
                    housePos.SetOrientation(hFacing);
                    break;
                }
            }
        }
    }

    // Place the door GO at the door fixture MeshObject's position.
    // The MeshObject is already positioned correctly by the WMO attachment system.
    Position doorPos;
    if (MeshObject* doorMesh = FindMeshObjectByHookID(plotIndex, static_cast<int32>(hookID)))
    {
        doorPos = doorMesh->GetPosition();
    }
    else
    {
        // Fallback: compute from hook offset + house position
        float doorLocalX = hookEntry->Position[0];
        float doorLocalY = hookEntry->Position[1];
        float doorLocalZ = hookEntry->Position[2];
        float facing = housePos.GetOrientation();
        float cosFacing = std::cos(facing);
        float sinFacing = std::sin(facing);
        doorPos = Position(
            housePos.GetPositionX() + doorLocalX * cosFacing - doorLocalY * sinFacing,
            housePos.GetPositionY() + doorLocalX * sinFacing + doorLocalY * cosFacing,
            housePos.GetPositionZ() + doorLocalZ,
            housePos.GetOrientation());
    }

    // Apply the ExteriorComponentExitPoint Z offset to match SpawnExtCompTree —
    // without it the final GO after a fixture move lands ~2 yards below the
    // initial spawn and sinks into the ground / out of the player's view.
    if (ExteriorComponentExitPointEntry const* exitPt = sHousingMgr.GetExitPoint(doorComponentID))
        doorPos.m_positionZ += exitPt->Position[2];

    QuaternionData rot = QuaternionData::fromEulerAnglesZYX(doorPos.GetOrientation(), 0.0f, 0.0f);
    GameObject* doorGo = GameObject::CreateGameObject(doorEntry, this, doorPos, rot, 255, GO_STATE_READY);
    if (!doorGo)
    {
        TC_LOG_ERROR("housing", "HousingMap::RespawnDoorGOAtHook: CreateGameObject failed for entry {} at hook {}", doorEntry, hookID);
        return;
    }

    doorGo->SetFlag(GO_FLAG_NODESPAWN);
    PhasingHandler::InitDbPhaseShift(doorGo->GetPhaseShift(), PHASE_USE_FLAGS_ALWAYS_VISIBLE, 0, 0);

    if (AddToMap(doorGo))
    {
        _houseGameObjects[plotIndex] = doorGo->GetGUID();

        // Force-send CREATE to the player so the GO is immediately visible
        if (player)
        {
            UpdateData goUpdate(player->GetMapId());
            doorGo->BuildCreateUpdateBlockForPlayer(&goUpdate, player);
            player->m_clientGUIDs.insert(doorGo->GetGUID());
            if (goUpdate.HasData())
            {
                WorldPacket goPacket;
                goUpdate.BuildPacket(&goPacket);
                player->SendDirectMessage(&goPacket);
            }
        }

        TC_LOG_INFO("housing", "HousingMap::RespawnDoorGOAtHook: Door GO {} spawned at hook {} ({:.1f},{:.1f},{:.1f}) for plot {}",
            doorGo->GetGUID().ToString(), hookID, doorPos.GetPositionX(), doorPos.GetPositionY(), doorPos.GetPositionZ(), plotIndex);
    }
    else
    {
        TC_LOG_ERROR("housing", "HousingMap::RespawnDoorGOAtHook: AddToMap failed for door GO at hook {}", hookID);
        delete doorGo;
    }
}

GameObject* HousingMap::GetHouseGameObject(uint8 plotIndex)
{
    auto itr = _houseGameObjects.find(plotIndex);
    if (itr == _houseGameObjects.end())
        return nullptr;

    return GetGameObject(itr->second);
}

int8 HousingMap::GetPlotIndexForHouseGO(ObjectGuid goGuid) const
{
    for (auto const& [plotIndex, guid] : _houseGameObjects)
    {
        if (guid == goGuid)
            return static_cast<int8>(plotIndex);
    }
    return -1;
}

// ============================================================
// Decor Management
// ============================================================
// Functional decor (HouseDecorData.GameObjectID > 0 AND gameobject_template
// exists) spawns as a real interactive GameObject with FHousingDecor_C +
// FMirroredPositionData_C fragments, retaining normal GO behavior (sit on
// chairs, open chests, mail UI on mailboxes, etc.). Retail sniff-verified
// fragment set: [CGObject, FHousingDecor_C, FMirroredPositionData_C, Tag_GameObject].
//
// Visual-only decor (ModelFileDataID-only) spawns as a MeshObject with
// fragments [CGObject, FMeshObjectData_C, FHousingDecor_C, FMirroredPositionData_C,
// Tag_MeshObject] — also retail-verified.

bool HousingMap::SpawnDecorItem(uint8 plotIndex, Housing::PlacedDecor const& decor, ObjectGuid houseGuid)
{
    HouseDecorData const* decorData = sHousingMgr.GetHouseDecorData(decor.DecorEntryId);
    if (!decorData)
    {
        TC_LOG_ERROR("housing", "HousingMap::SpawnDecorItem: No HouseDecorData for entry {} (decorGuid={})",
            decor.DecorEntryId, decor.Guid.ToString());
        return false;
    }

    // Look up the room entity we attach to (the Housing/sub2 plot room).
    ObjectGuid roomEntityGuid = ObjectGuid::Empty;
    Position roomWorldPos;
    if (HousingRoomEntity* roomId = GetRoomIdentityEntity(plotIndex))
    {
        roomEntityGuid = roomId->GetGUID();
        roomWorldPos = roomId->GetPosition();
    }

    float worldX = decor.PosX;
    float worldY = decor.PosY;
    float worldZ = decor.PosZ;
    LoadGrid(worldX, worldY);

    QuaternionData rot(decor.RotationX, decor.RotationY, decor.RotationZ, decor.RotationW);

    // World → room-local (inverse room rotation). Matches MeshObject decor path.
    float localX = worldX;
    float localY = worldY;
    float localZ = worldZ;
    if (!roomEntityGuid.IsEmpty())
    {
        float dx = worldX - roomWorldPos.GetPositionX();
        float dy = worldY - roomWorldPos.GetPositionY();
        float roomFacing = roomWorldPos.GetOrientation();
        float cosF = std::cos(roomFacing);
        float sinF = std::sin(roomFacing);
        localX =  cosF * dx + sinF * dy;
        localY = -sinF * dx + cosF * dy;
        localZ = worldZ - roomWorldPos.GetPositionZ();
    }

    Position localPos(localX, localY, localZ);
    Position worldPos(worldX, worldY, worldZ);
    float decorScale = decor.Scale > 0.01f ? decor.Scale : 1.0f;
    uint8 attachFlags = roomEntityGuid.IsEmpty() ? uint8(0) : uint8(3);

    // ---------- Functional decor branch (real GameObject) ----------
    // Retail: chairs, chests, mailboxes, chandeliers, fireplaces, etc. spawn as
    // real GameObjects so the client treats them as interactive (sittable/
    // openable/usable). We require both GameObjectID in DB2 AND a matching
    // gameobject_template to fall into this path; missing templates fall back
    // to visual-only MeshObject.
    if (decorData->GameObjectID > 0)
    {
        uint32 goEntry = static_cast<uint32>(decorData->GameObjectID);
        if (GameObjectTemplate const* goTemplate = sObjectMgr->GetGameObjectTemplate(goEntry))
        {
            // Derive orientation from the quaternion for GO world rotation.
            // GameObject::SetLocalRotation uses the packed quat for rendering;
            // the orientation-from-quat is used for stationary direction.
            float orientation = 2.0f * std::atan2(rot.z, rot.w);
            Position goWorldPos(worldX, worldY, worldZ, orientation);

            GameObject* go = GameObject::CreateGameObject(goEntry, this, goWorldPos, rot,
                255 /*animProgress*/, GO_STATE_READY, 0 /*artKit*/);
            if (!go)
            {
                TC_LOG_ERROR("housing", "HousingMap::SpawnDecorItem: CreateGameObject failed for decor entry={} "
                    "goEntry={} at ({:.1f},{:.1f},{:.1f}) — falling back to MeshObject",
                    decor.DecorEntryId, goEntry, worldX, worldY, worldZ);
            }
            else
            {
                PhasingHandler::InitDbPhaseShift(go->GetPhaseShift(), PHASE_USE_FLAGS_ALWAYS_VISIBLE, 0, 0);
                go->SetObjectScale(decorScale);

                // Keep template default flags — chairs need CHAIR type behavior, chests need
                // CHEST interaction, mailboxes need MAILBOX UI. Don't override with door-style
                // flags; the GO's normal on-use handler is what we want.

                // Retail wire fragments: FHousingDecor_C + FMirroredPositionData_C.
                // Order matches the sniff-verified fragment list.
                go->InitHousingDecorData(decor.Guid, houseGuid, decor.Locked ? 1 : 0,
                    roomEntityGuid, decor.SourceType, decor.SourceValue);
                go->InitHousingDecorMirroredPosition(localPos, rot, decorScale, roomEntityGuid, attachFlags);

                if (!AddToMap(go))
                {
                    TC_LOG_ERROR("housing", "HousingMap::SpawnDecorItem: AddToMap failed for GO decor "
                        "entry={} goEntry={} decorGuid={}",
                        decor.DecorEntryId, goEntry, decor.Guid.ToString());
                    delete go;
                    return false;
                }

                _decorGameObjects[plotIndex].push_back(go->GetGUID());
                _decorGuidToGoGuid[decor.Guid] = go->GetGUID();
                _decorGuidToPlotIndex[decor.Guid] = plotIndex;

                TC_LOG_INFO("housing", "HousingMap::SpawnDecorItem: Spawned functional-decor GameObject "
                    "entry={} goEntry={} goType={} goGuid={} decorGuid={} "
                    "at world({:.1f},{:.1f},{:.1f}) local({:.1f},{:.1f},{:.1f}) scale={:.2f} "
                    "room={} plot={}",
                    decor.DecorEntryId, goEntry, uint32(goTemplate->type), go->GetGUID().ToString(),
                    decor.Guid.ToString(), worldX, worldY, worldZ, localX, localY, localZ,
                    decorScale, roomEntityGuid.ToString(), plotIndex);
                return true;
            }
        }
        else
        {
            TC_LOG_DEBUG("housing", "HousingMap::SpawnDecorItem: GameObjectID={} referenced by decor entry={} "
                "is not in gameobject_template — falling back to MeshObject (visual-only)",
                goEntry, decor.DecorEntryId);
        }
    }

    // ---------- Visual-only branch (MeshObject) ----------
    int32 fileDataID = decorData->ModelFileDataID;
    if (fileDataID <= 0 && decorData->GameObjectID > 0)
    {
        // Fallback: derive FileDataID from the GO template displayInfo when the
        // DB2 entry's ModelFileDataID is 0 but GameObjectID points at a valid template.
        if (GameObjectTemplate const* goTemplate = sObjectMgr->GetGameObjectTemplate(
                static_cast<uint32>(decorData->GameObjectID)))
        {
            if (GameObjectDisplayInfoEntry const* displayInfo =
                    sGameObjectDisplayInfoStore.LookupEntry(goTemplate->displayId))
            {
                if (displayInfo->FileDataID > 0)
                    fileDataID = displayInfo->FileDataID;
            }
        }
    }

    if (fileDataID <= 0)
    {
        TC_LOG_ERROR("housing", "HousingMap::SpawnDecorItem: Cannot derive FileDataID for decor entry {} "
            "(GameObjectID={}, ModelFileDataID={}), skipping",
            decor.DecorEntryId, decorData->GameObjectID, decorData->ModelFileDataID);
        return false;
    }

    MeshObject* mesh = MeshObject::CreateMeshObject(this, localPos, rot, decorScale,
        fileDataID, /*isWMO*/ false, roomEntityGuid, attachFlags, &worldPos);

    if (!mesh)
    {
        TC_LOG_ERROR("housing", "HousingMap::SpawnDecorItem: Failed to create decor MeshObject fileDataID={} for decor {}",
            fileDataID, decor.Guid.ToString());
        return false;
    }

    PhasingHandler::InitDbPhaseShift(mesh->GetPhaseShift(), PHASE_USE_FLAGS_ALWAYS_VISIBLE, 0, 0);
    mesh->InitHousingDecorData(decor.Guid, houseGuid, decor.Locked ? 1 : 0, roomEntityGuid, decor.SourceType, decor.SourceValue);

    if (!AddToMap(mesh))
    {
        TC_LOG_ERROR("housing", "HousingMap::SpawnDecorItem: Failed to add decor MeshObject to map for decor {}", decor.Guid.ToString());
        delete mesh;
        return false;
    }

    _decorGameObjects[plotIndex].push_back(mesh->GetGUID());
    _decorGuidToGoGuid[decor.Guid] = mesh->GetGUID();
    _decorGuidToPlotIndex[decor.Guid] = plotIndex;

    TC_LOG_INFO("housing", "HousingMap::SpawnDecorItem: Spawned decor MeshObject fileDataID={} meshGuid={} decorGuid={} "
        "at world({:.1f},{:.1f},{:.1f}) local({:.1f},{:.1f},{:.1f}) scale={:.2f} room={} plot={}",
        fileDataID, mesh->GetGUID().ToString(), decor.Guid.ToString(),
        worldX, worldY, worldZ, localX, localY, localZ, decorScale,
        roomEntityGuid.ToString(), plotIndex);
    return true;
}

void HousingMap::DespawnDecorItem(uint8 plotIndex, ObjectGuid decorGuid)
{
    auto itr = _decorGuidToGoGuid.find(decorGuid);
    if (itr == _decorGuidToGoGuid.end())
        return;

    ObjectGuid objGuid = itr->second;
    // Decor may be either a functional-decor GameObject or a visual-only MeshObject.
    if (objGuid.IsGameObject())
    {
        if (GameObject* go = GetGameObject(objGuid))
            go->AddObjectToRemoveList();
    }
    else if (MeshObject* mesh = GetMeshObject(objGuid))
        mesh->AddObjectToRemoveList();

    auto& plotDecor = _decorGameObjects[plotIndex];
    plotDecor.erase(std::remove(plotDecor.begin(), plotDecor.end(), objGuid), plotDecor.end());
    _decorGuidToGoGuid.erase(itr);
    _decorGuidToPlotIndex.erase(decorGuid);

    TC_LOG_DEBUG("housing", "HousingMap::DespawnDecorItem: Despawned decor {} for decorGuid={} plot={}",
        objGuid.ToString(), decorGuid.ToString(), plotIndex);
}

void HousingMap::DespawnAllDecorForPlot(uint8 plotIndex)
{
    auto itr = _decorGameObjects.find(plotIndex);
    if (itr == _decorGameObjects.end())
        return;

    for (ObjectGuid const& objGuid : itr->second)
    {
        if (objGuid.IsGameObject())
        {
            if (GameObject* go = GetGameObject(objGuid))
                go->AddObjectToRemoveList();
        }
        else if (MeshObject* mesh = GetMeshObject(objGuid))
            mesh->AddObjectToRemoveList();
    }

    // Clean up all tracking for this plot's decor
    std::vector<ObjectGuid> decorGuidsToRemove;
    for (auto const& [decorGuid, pIdx] : _decorGuidToPlotIndex)
    {
        if (pIdx == plotIndex)
            decorGuidsToRemove.push_back(decorGuid);
    }
    for (ObjectGuid const& decorGuid : decorGuidsToRemove)
    {
        _decorGuidToGoGuid.erase(decorGuid);
        _decorGuidToPlotIndex.erase(decorGuid);
    }

    itr->second.clear();
    _decorSpawnedPlots.erase(plotIndex);

    TC_LOG_DEBUG("housing", "HousingMap::DespawnAllDecorForPlot: Despawned all decor MeshObjects for plot {}", plotIndex);
}

void HousingMap::SpawnAllDecorForPlot(uint8 plotIndex, Housing const* housing)
{
    if (!housing)
        return;

    if (_decorSpawnedPlots.count(plotIndex))
    {
        TC_LOG_ERROR("housing", "HousingMap::SpawnAllDecorForPlot: Plot {} already in _decorSpawnedPlots — skipping respawn "
            "(decorGuidMap.size={} decorGOs[{}].size={})",
            plotIndex, uint32(_decorGuidToGoGuid.size()),
            plotIndex, _decorGameObjects.count(plotIndex) ? uint32(_decorGameObjects[plotIndex].size()) : 0);
        return; // Already spawned
    }

    ObjectGuid houseGuid = housing->GetHouseGuid();
    uint32 spawnCount = 0;
    uint32 exteriorCount = 0;
    uint32 failCount = 0;
    for (auto const& [decorGuid, decor] : housing->GetPlacedDecorMap())
    {
        // Skip interior decor — those are spawned by HouseInteriorMap::SpawnInteriorDecor
        if (!decor.RoomGuid.IsEmpty())
            continue;

        ++exteriorCount;
        if (SpawnDecorItem(plotIndex, decor, houseGuid))
            ++spawnCount;
        else
            ++failCount;
    }

    _decorSpawnedPlots.insert(plotIndex);

    TC_LOG_ERROR("housing", "HousingMap::SpawnAllDecorForPlot: Spawned {}/{} exterior decor for plot {} "
        "(failed={}, neighborhood='{}')",
        spawnCount, exteriorCount, plotIndex, failCount,
        _neighborhood ? _neighborhood->GetName() : "?");
}

void HousingMap::UpdateDecorPosition(uint8 plotIndex, ObjectGuid decorGuid, Position const& pos, QuaternionData const& rot, float scale /*= 1.0f*/)
{
    auto itr = _decorGuidToGoGuid.find(decorGuid);
    if (itr == _decorGuidToGoGuid.end())
        return;

    ObjectGuid objGuid = itr->second;
    if (objGuid.IsGameObject())
    {
        if (GameObject* go = GetGameObject(objGuid))
        {
            go->Relocate(pos);
            go->SetLocalRotation(rot.x, rot.y, rot.z, rot.w);
            if (std::abs(go->GetObjectScale() - scale) > 0.001f)
                go->SetObjectScale(scale);
            TC_LOG_DEBUG("housing", "HousingMap::UpdateDecorPosition: Moved decor GameObject {} to ({:.1f}, {:.1f}, {:.1f}) scale={:.2f} for plot {}",
                decorGuid.ToString(), pos.GetPositionX(), pos.GetPositionY(), pos.GetPositionZ(), scale, plotIndex);
        }
    }
    else if (MeshObject* mesh = GetMeshObject(objGuid))
    {
        mesh->Relocate(pos);
        if (std::abs(mesh->GetLocalScale() - scale) > 0.001f)
            mesh->UpdateLocalScale(scale);
        TC_LOG_DEBUG("housing", "HousingMap::UpdateDecorPosition: Moved decor MeshObject {} to ({:.1f}, {:.1f}, {:.1f}) scale={:.2f} for plot {}",
            decorGuid.ToString(), pos.GetPositionX(), pos.GetPositionY(), pos.GetPositionZ(), scale, plotIndex);
    }
}
