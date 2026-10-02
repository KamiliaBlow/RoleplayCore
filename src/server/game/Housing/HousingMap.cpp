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
#include "AreaTrigger.h"
#include "Creature.h"
#include "DB2Stores.h"
#include "DB2Structure.h"
#include "EventProcessor.h"
#include "GameObject.h"
#include "GridDefines.h"
#include "Housing.h"
#include "HousingDefines.h"
#include "HousingMgr.h"
#include "HousingMirrorEntity.h"
#include "HousingNeighborhoodMirrorEntity.h"
#include "HousingPackets.h"
#include "HousingPlayerHouseEntity.h"
#include "HousingRoomEntity.h"
#include "Log.h"
#include "MeshObject.h"
#include "Neighborhood.h"
#include "NeighborhoodMgr.h"
#include "ObjectAccessor.h"
#include "ObjectGuid.h"
#include "ObjectMgr.h"
#include "PhasingHandler.h"
#include "Player.h"
#include "SocialMgr.h"
#include "Spell.h"
#include "SpellAuraDefines.h"
#include "SpellPackets.h"
#include "TemporarySummon.h"
#include "UpdateData.h"
#include "World.h"
#include "WorldSession.h"
#include "WorldStatePackets.h"
#include <algorithm>
#include <cmath>
#include <set>

namespace
{
    // DB2 stores the hook yaw clockwise; the client expects its negation.
    QuaternionData GetHookLocalRotation(ExteriorComponentHookEntry const* hook)
    {
        static constexpr float DEG_TO_RAD = static_cast<float>(M_PI / 180.0);
        float const rx = hook->Rotation[0] * DEG_TO_RAD;
        float const ry = hook->Rotation[1] * DEG_TO_RAD;
        float const rz = -hook->Rotation[2] * DEG_TO_RAD;
        float const cx = std::cos(rx / 2.0f), sx = std::sin(rx / 2.0f);
        float const cy = std::cos(ry / 2.0f), sy = std::sin(ry / 2.0f);
        float const cz = std::cos(rz / 2.0f), sz = std::sin(rz / 2.0f);
        QuaternionData rot;
        rot.x = sx * cy * cz - cx * sy * sz;
        rot.y = cx * sy * cz + sx * cy * sz;
        rot.z = cx * cy * sz - sx * sy * cz;
        rot.w = cx * cy * cz + sx * sy * sz;
        return rot;
    }

    // Recurring event that sends the housing WorldState counters every ~5 s, +1333 per tick.
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
                return true;

            player->SendUpdateWorldState(WORLDSTATE_HOUSING_COUNTER_1, _counter1);
            player->SendUpdateWorldState(WORLDSTATE_HOUSING_COUNTER_2, _counter2);
            player->SendUpdateWorldState(WORLDSTATE_HOUSING_COUNTER_3, _counter3);
            // Counters 4/5 are not sent by retail.
            _counter1 += HOUSING_WORLDSTATE_INCREMENT;
            _counter2 += HOUSING_WORLDSTATE_INCREMENT;
            _counter3 += HOUSING_WORLDSTATE_INCREMENT;
            _counter4 += HOUSING_WORLDSTATE_INCREMENT_2;
            _counter5 += HOUSING_WORLDSTATE_INCREMENT_2;

            player->m_Events.AddEventAtOffset(
                new HousingWorldStateCounterEvent(_playerGuid,
                    _counter1, _counter2, _counter3, _counter4, _counter5),
                Milliseconds(HOUSING_WORLDSTATE_INTERVAL_MS));

            return true;
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
    // Keep the map loaded forever: Map::CanUnload() returns false when m_unloadTimer == 0.
    m_unloadTimer = 0;
    HousingMap::InitVisibilityDistance();

    // The client's "Airlock" system needs InstanceType==8 or every decor placement fails with OutsidePlotBounds.
    if (GetEntry()->InstanceType != MAP_HOUSE_NEIGHBORHOOD)
    {
        TC_LOG_ERROR("housing", "CRITICAL: HousingMap {} '{}' has InstanceType={}, expected {} (MAP_HOUSE_NEIGHBORHOOD). "
            "Client will NOT allow decor placement — OutsidePlotBounds will always fire!",
            id, GetEntry()->MapName[sWorld->GetDefaultDbcLocale()],
            GetEntry()->InstanceType, MAP_HOUSE_NEIGHBORHOOD);
    }
}

HousingMap::~HousingMap()
{
    _playerHousings.clear();
}

void HousingMap::InitVisibilityDistance()
{
    // Retail streams by bounded grid distance; persistent infra entities are marked far-visible at spawn time.
    m_VisibleDistance = 200.0f;
    m_VisibilityNotifyPeriod = sWorld->getIntConfig(CONFIG_VISIBILITY_NOTIFY_PERIOD_INSTANCE);
}

void HousingMap::LoadGridObjects(NGridType* grid)
{
    Map::LoadGridObjects(grid);
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
    std::vector<NeighborhoodPlotData const*> const& plots = sHousingMgr.GetPlotsForMap(neighborhoodMapId);

    if (plots.empty())
    {
        TC_LOG_ERROR("housing", "HousingMap::SpawnPlotGameObjects: NO plots found for neighborhoodMapId={} (neighborhood='{}') - check DB2 NeighborhoodPlot data",
            neighborhoodMapId, _neighborhood->GetName());
        return;
    }

    for (NeighborhoodPlotData const* plot : plots)
    {
        float x = plot->CornerstonePosition[0];
        float y = plot->CornerstonePosition[1];
        float z = plot->CornerstonePosition[2];

        LoadGrid(x, y);

        // Retail spawns the same "Cornerstone" GO on every plot; GOState: 0 = owned, 1 = ForSale sign.
        Neighborhood::PlotInfo const* plotInfo = _neighborhood->GetPlotInfo(static_cast<uint8>(plot->PlotIndex));
        uint32 const goEntry = HOUSING_CORNERSTONE_GAMEOBJECT_ENTRY;
        uint32 const anchorEntry = static_cast<uint32>(plot->CornerstoneGameObjectID);
        bool isOwned = plotInfo && !plotInfo->OwnerGuid.IsEmpty();

        if (!anchorEntry)
        {
            TC_LOG_ERROR("housing", "HousingMap::SpawnPlotGameObjects: Plot {} has CornerstoneGameObjectID=0 - no position anchor, skipping",
                plot->PlotIndex);
            continue;
        }

        // Cornerstone transform: prefer the GameObjects.db2 anchor row (NeighborhoodPlot rows are unreliable); fall back to CornerstoneRotation+PI.
        float rotZ;
        QuaternionData rot;
        if (GameObjectsEntry const* goData = sGameObjectsStore.LookupEntry(anchorEntry))
        {
            x = goData->Pos.X;
            y = goData->Pos.Y;
            z = goData->Pos.Z;
            LoadGrid(x, y);
            rot = QuaternionData(goData->Rot[0], goData->Rot[1], goData->Rot[2], goData->Rot[3]);
            float unusedY = 0.0f, unusedX = 0.0f;
            rot.toEulerAnglesZYX(rotZ, unusedY, unusedX);
        }
        else
        {
            rotZ = plot->CornerstoneRotation[2] + float(M_PI);
            rot = QuaternionData::fromEulerAnglesZYX(rotZ, plot->CornerstoneRotation[1], plot->CornerstoneRotation[0]);
        }

        GOState plotState = isOwned ? GO_STATE_ACTIVE : GO_STATE_READY;

        Position pos(x, y, z, rotZ);
        GameObject* go = GameObject::CreateGameObject(goEntry, this, pos, rot, 255, plotState);
        if (!go)
        {
            TC_LOG_ERROR("housing", "HousingMap::SpawnPlotGameObjects: Failed to create GO entry {} at ({}, {}, {}) for plot {} in neighborhood '{}'",
                goEntry, x, y, z, plot->PlotIndex, _neighborhood->GetName());
            continue;
        }

        // All Cornerstone GOs carry GO_FLAG_NODESPAWN.
        go->SetFlag(GO_FLAG_NODESPAWN);

        // Mark universally visible so area-based player phases cannot hide them.
        PhasingHandler::InitDbPhaseShift(go->GetPhaseShift(), PHASE_USE_FLAGS_ALWAYS_VISIBLE, 0, 0);

        // The FJamHousingCornerstone_C fragment drives the "For Sale"/owned UI.
        go->InitHousingCornerstoneData(plot->Cost, static_cast<int32>(plot->PlotIndex));

        if (!AddToMap(go))
        {
            delete go;
            TC_LOG_ERROR("housing", "HousingMap::SpawnPlotGameObjects: Failed to add GO entry {} to map for plot {} in neighborhood '{}'",
                goEntry, plot->PlotIndex, _neighborhood->GetName());
            continue;
        }

        // Keep cornerstone GOs streamed to every player (house finder, OPEN_CORNERSTONE_UI, world-map icon).
        go->setActive(true);
        go->SetFarVisible(true);

        _plotGameObjects[static_cast<uint8>(plot->PlotIndex)] = go->GetGUID();

        if (isOwned)
        {
            SpawnPlotAreaTrigger(plot);
            SetPlotGroundCleared(plot, true);
        }
    }

    // The per-plot WorldState is a BINARY occupancy flag shipped inside SMSG_INIT_WORLD_STATES;
    // owner identity is carried by the plot AT and NeighborhoodMirrorData.Houses.
    for (NeighborhoodPlotData const* plot : plots)
    {
        uint8 plotIdx = static_cast<uint8>(plot->PlotIndex);
        uint32 wsId = plot->WorldState;
        if (!wsId)
            continue;
        Neighborhood::PlotInfo const* pi = _neighborhood->GetPlotInfo(plotIdx);
        bool occupied = pi && pi->IsOccupied() && !pi->HouseGuid.IsEmpty();
        SetWorldStateValue(wsId, occupied ? 1 : 0, /*hidden*/ false);
    }

    // Spawn house structure GOs for owned plots
    for (NeighborhoodPlotData const* plot : plots)
    {
        uint8 plotIdx = static_cast<uint8>(plot->PlotIndex);
        Neighborhood::PlotInfo const* plotInfo = _neighborhood->GetPlotInfo(plotIdx);
        if (!plotInfo || plotInfo->OwnerGuid.IsEmpty())
            continue;

        // Spawn data comes from the DB regardless of owner online state; the live Housing* only adds runtime-computed fields.
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

            // Mirrored fixture overrides so visitors see each neighbour's customised roof/doors/windows.
            for (auto const& [pointId, optionId] : plotInfo->Fixtures)
                fixtureOverrides[pointId] = optionId;

            // Root component selections (OptionId==0), the offline-owner counterpart of Housing::GetRootComponentOverrides.
            for (auto const& [pointId, optionId] : plotInfo->Fixtures)
            {
                if (optionId != 0)
                    continue;
                ExteriorComponentEntry const* comp = sExteriorComponentStore.LookupEntry(pointId);
                if (!comp)
                    continue;
                if (comp->Type != HOUSING_FIXTURE_TYPE_BASE && comp->Type != HOUSING_FIXTURE_TYPE_ROOF)
                    continue;
                if (comp->HouseExteriorWmoDataID != plotInfo->HouseType)
                    continue;
                rootOverrides[comp->Type] = pointId;
            }

            // Core fixture: an override with OptionId==0, else DB2 IsDefault, else the first root.
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

        FixtureOverrideMap const* overridesPtr = fixtureOverrides.empty() ? nullptr : &fixtureOverrides;
        RootOverrideMap const* rootOvrPtr = rootOverrides.empty() ? nullptr : &rootOverrides;

        GameObject* houseGo = nullptr;
        if (housing && housing->HasCustomPosition())
        {
            Position customPos = housing->GetHousePosition();
            houseGo = SpawnHouseForPlot(plotIdx, &customPos, exteriorComponentID, houseExteriorWmoDataID, overridesPtr, rootOvrPtr);
        }
        else if (!housing && plotInfo->HousePosition)
        {
            // Owner offline: the moved house position mirrored from character_housing
            Position customPos = *plotInfo->HousePosition;
            houseGo = SpawnHouseForPlot(plotIdx, &customPos, exteriorComponentID, houseExteriorWmoDataID, overridesPtr, rootOvrPtr);
        }
        else
        {
            houseGo = SpawnHouseForPlot(plotIdx, nullptr, exteriorComponentID, houseExteriorWmoDataID, overridesPtr, rootOvrPtr);
        }
        if (!houseGo)
            TC_LOG_ERROR("housing", "HousingMap::SpawnPlotGameObjects: FAILED to spawn house for plot {} owned by {}",
                plotIdx, plotInfo->OwnerGuid.ToString());

        // Spawn placed decor — live Housing path preferred; otherwise iterate PlotInfo.Decor from the DB.
        if (housing)
        {
            SpawnAllDecorForPlot(plotIdx, housing);
        }
        else
        {
            for (Housing::PlacedDecor const& decor : plotInfo->Decor)
            {
                if (!decor.RoomGuid.IsEmpty())
                    continue; // exterior-only at preload
                SpawnDecorItem(plotIdx, decor, plotInfo->HouseGuid);
            }
            _decorSpawnedPlots.insert(plotIdx);
        }
    }
}

void HousingMap::LockPlotGrids()
{
    if (!_neighborhood)
        return;

    uint32 neighborhoodMapId = _neighborhood->GetNeighborhoodMapID();
    std::vector<NeighborhoodPlotData const*> const& plots = sHousingMgr.GetPlotsForMap(neighborhoodMapId);
    std::set<std::pair<uint32, uint32>> lockedGrids;

    for (NeighborhoodPlotData const* plot : plots)
    {
        GridCoord cornerstoneGrid = Trinity::ComputeGridCoord(plot->CornerstonePosition[0], plot->CornerstonePosition[1]);
        if (lockedGrids.insert({ cornerstoneGrid.x_coord, cornerstoneGrid.y_coord }).second)
            GridMarkNoUnload(cornerstoneGrid.x_coord, cornerstoneGrid.y_coord);

        // The house position may sit in a different grid
        GridCoord houseGrid = Trinity::ComputeGridCoord(plot->HousePosition[0], plot->HousePosition[1]);
        if (lockedGrids.insert({ houseGrid.x_coord, houseGrid.y_coord }).second)
            GridMarkNoUnload(houseGrid.x_coord, houseGrid.y_coord);
    }
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

// Spawn plot AreaTrigger (entry 37358) at the plot room: required for the edit menu and the plot boundary decal.
// Owned plots only - no AT exists for an unsold plot (the unsold marker is drawn client-side from the plot worldstate).
AreaTrigger* HousingMap::SpawnPlotAreaTrigger(NeighborhoodPlotData const* plot)
{
    uint8 plotIndex = static_cast<uint8>(plot->PlotIndex);
    if (AreaTrigger* existing = GetPlotAreaTrigger(plotIndex))
        return existing;

    float hx = plot->HousePosition[0];
    float hy = plot->HousePosition[1];
    float hz = plot->HousePosition[2];

    // Compute facing toward the cornerstone
    float hFacing = plot->HouseRotation[2];
    if (plot->HouseRotation[0] == 0.0f && plot->HouseRotation[1] == 0.0f && plot->HouseRotation[2] == 0.0f)
        hFacing = std::atan2(plot->CornerstonePosition[1] - hy, plot->CornerstonePosition[0] - hx);

    // The AT sits 31.5 yards above the plot GameObject (centre of its 94-yard box).
    static constexpr float PLOT_AT_HEIGHT_OFFSET = 31.5f;
    Position roomFrame;
    if (GetPlotRoomFrame(plotIndex, roomFrame))
    {
        hx = roomFrame.GetPositionX();
        hy = roomFrame.GetPositionY();
        hz = roomFrame.GetPositionZ() + PLOT_AT_HEIGHT_OFFSET;
        hFacing = roomFrame.GetOrientation();
    }

    LoadGrid(hx, hy);

    Position atPos(hx, hy, hz, hFacing);
    // Create with addToMap=false so the housing visual fields are set before the CREATE goes out.
    AreaTrigger* plotAt = AreaTrigger::CreateStaticAreaTrigger({ .Id = 37358, .IsCustom = false }, this, atPos, -1, false);
    if (!plotAt)
    {
        TC_LOG_ERROR("housing", "HousingMap::SpawnPlotAreaTrigger: Failed to create plot AT (entry 37358) for plot {} at ({:.1f},{:.1f},{:.1f})",
            plot->PlotIndex, hx, hy, hz);
        return nullptr;
    }

    PhasingHandler::InitDbPhaseShift(plotAt->GetPhaseShift(), PHASE_USE_FLAGS_ALWAYS_VISIBLE, 0, 0);

    // No per-AT housing fragment: plot ownership travels via PlayerHouseInfoComponentData.CurrentHouse.
    plotAt->InitHousingPlotVisuals();

    if (!AddToMap(plotAt))
    {
        TC_LOG_ERROR("housing", "HousingMap::SpawnPlotAreaTrigger: AddToMap failed for plot AT (entry 37358) plot {} at ({:.1f},{:.1f},{:.1f})",
            plot->PlotIndex, hx, hy, hz);
        delete plotAt;
        return nullptr;
    }

    // Keep plot ATs streamed regardless of player distance (decor placement and the ownership UI need them).
    plotAt->setActive(true);
    plotAt->SetFarVisible(true);

    _plotAreaTriggers[plotIndex] = plotAt->GetGUID();

    // Push the create to every current viewer; a bystander would otherwise keep the "for sale" pin until a relog.
    uint32 atReceivers = 0;
    for (MapReference const& ref : GetPlayers())
    {
        Player* viewer = ref.GetSource();
        if (!viewer || viewer->HaveAtClient(plotAt))
            continue;

        ++atReceivers;
        UpdateData atUpdate(GetId());
        plotAt->BuildCreateUpdateBlockForPlayer(&atUpdate, viewer);
        viewer->m_clientGUIDs.insert(plotAt->GetGUID());
        WorldPacket packet;
        atUpdate.BuildPacket(&packet);
        viewer->SendDirectMessage(&packet);
    }

    TC_LOG_ERROR("housing", "[PinSync] plot {} area trigger create -> {} viewers", plotIndex, atReceivers);

    return plotAt;
}

void HousingMap::DespawnPlotAreaTrigger(uint8 plotIndex)
{
    auto itr = _plotAreaTriggers.find(plotIndex);
    if (itr == _plotAreaTriggers.end())
        return;

    if (AreaTrigger* plotAt = GetAreaTrigger(itr->second))
        plotAt->Remove();

    _plotAreaTriggers.erase(itr);
}

void HousingMap::SetPlotGroundCleared(NeighborhoodPlotData const* plot, bool cleared)
{
    GameObjectsEntry const* plotGo = sGameObjectsStore.LookupEntry(plot->PlotGameObjectID);
    if (!plotGo)
        return;

    uint8 const plotIndex = static_cast<uint8>(plot->PlotIndex);
    auto [itr, inserted] = _plotGroundSpawns.try_emplace(plotIndex);
    if (inserted)
    {
        // Plot bounds = the plot room geobox, 70x60 around the plot GameObject.
        static constexpr float PLOT_HALF_X = 35.0f;
        static constexpr float PLOT_HALF_Y = 30.0f;
        float yaw = 0.0f, unusedY = 0.0f, unusedX = 0.0f;
        QuaternionData(plotGo->Rot[0], plotGo->Rot[1], plotGo->Rot[2], plotGo->Rot[3]).toEulerAnglesZYX(yaw, unusedY, unusedX);
        float const c = std::cos(yaw);
        float const s = std::sin(yaw);
        if (GridObjectGuidsMap const* mapSpawns = sObjectMgr->GetMapObjectGuids(GetId(), GetDifficultyID()))
        {
            for (auto const& [gridId, gridSpawns] : *mapSpawns)
            {
                for (ObjectGuid::LowType spawnId : gridSpawns.gameobjects)
                {
                    GameObjectData const* data = sObjectMgr->GetGameObjectData(spawnId);
                    if (!data || data->id == static_cast<uint32>(plot->CornerstoneGameObjectID))
                        continue;
                    float const dx = data->spawnPoint.GetPositionX() - plotGo->Pos.X;
                    float const dy = data->spawnPoint.GetPositionY() - plotGo->Pos.Y;
                    if (std::fabs(dx * c + dy * s) <= PLOT_HALF_X && std::fabs(dy * c - dx * s) <= PLOT_HALF_Y)
                        itr->second.push_back(spawnId);
                }
            }
        }
    }

    for (ObjectGuid::LowType spawnId : itr->second)
    {
        if (cleared)
        {
            _suppressedPlotSpawns.insert(spawnId);
            // Retail sends SMSG_GAME_OBJECT_DESPAWN (fade out) for each of them before the destroy.
            auto range = GetGameObjectBySpawnIdStore().equal_range(spawnId);
            for (auto goItr = range.first; goItr != range.second; ++goItr)
                goItr->second->SendGameObjectDespawn();
            DespawnAll(SPAWN_TYPE_GAMEOBJECT, spawnId);
        }
        else if (_suppressedPlotSpawns.erase(spawnId))
        {
            GameObjectData const* data = sObjectMgr->GetGameObjectData(spawnId);
            if (!data || !IsGridLoaded(data->spawnPoint))
                continue; // spawns with its grid

            // Door GOs are not plot ground clutter: respawning them resurrected an invisible scripted go_housing_door.
            if (uint32 const doorScriptId = sObjectMgr->GetScriptId("go_housing_door", false))
                if (GameObjectTemplate const* goInfo = sObjectMgr->GetGameObjectTemplate(data->id))
                    if (goInfo->ScriptId == doorScriptId)
                        continue;

            GameObject* go = new GameObject();
            if (!go->LoadFromDB(spawnId, this, true))
                delete go;
        }
    }
}

bool HousingMap::GetPlotRoomFrame(uint8 plotIndex, Position& frame) const
{
    if (!_neighborhood)
        return false;

    for (NeighborhoodPlotData const* plot : sHousingMgr.GetPlotsForMap(_neighborhood->GetNeighborhoodMapID()))
    {
        if (static_cast<uint8>(plot->PlotIndex) != plotIndex)
            continue;
        GameObjectsEntry const* plotGo = sGameObjectsStore.LookupEntry(plot->PlotGameObjectID);
        if (!plotGo)
            return false;
        float goYaw = 0.0f, unusedY = 0.0f, unusedX = 0.0f;
        QuaternionData(plotGo->Rot[0], plotGo->Rot[1], plotGo->Rot[2], plotGo->Rot[3]).toEulerAnglesZYX(goYaw, unusedY, unusedX);
        frame.Relocate(plotGo->Pos.X, plotGo->Pos.Y, plotGo->Pos.Z, Position::NormalizeOrientation(goYaw + float(M_PI)));
        return true;
    }
    return false;
}

bool HousingMap::IsSpawnSuppressed(SpawnObjectType type, ObjectGuid::LowType spawnId) const
{
    return type == SPAWN_TYPE_GAMEOBJECT && _suppressedPlotSpawns.contains(spawnId);
}

void HousingMap::SetPlotOwnershipState(uint8 plotIndex, bool owned)
{
    if (!_neighborhood)
        return;

    // GOState 0 = owned cornerstone, GOState 1 = ForSale sign
    GOState newState = owned ? GO_STATE_ACTIVE : GO_STATE_READY;

    auto itr = _plotGameObjects.find(plotIndex);
    if (itr != _plotGameObjects.end())
    {
        if (GameObject* go = GetGameObject(itr->second))
            go->SetGoState(newState);
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

    // Plot ownership is communicated via PlayerHouseInfoComponentData.CurrentHouse on each Player.
    uint32 neighborhoodMapId = _neighborhood->GetNeighborhoodMapID();
    std::vector<NeighborhoodPlotData const*> const& plots = sHousingMgr.GetPlotsForMap(neighborhoodMapId);

    for (NeighborhoodPlotData const* plotData : plots)
    {
        if (plotData->PlotIndex != static_cast<int32>(plotIndex))
            continue;

        // The plot AreaTrigger exists exactly while the plot is owned (see SpawnPlotAreaTrigger).
        if (owned)
            SpawnPlotAreaTrigger(plotData);
        else
            DespawnPlotAreaTrigger(plotIndex);

        SetPlotGroundCleared(plotData, owned);

        uint32 wsId = plotData->WorldState;
        if (wsId)
        {
            SetWorldStateValue(wsId, owned ? 1 : 0, /*hidden*/ false);

            // The broadcast is filtered through the worldstate template's AreaIds; push the flip to every player explicitly.
            WorldPackets::WorldState::UpdateWorldState plotOccupancy;
            plotOccupancy.VariableID = wsId;
            plotOccupancy.Value = owned ? 1 : 0;
            plotOccupancy.Hidden = false;
            plotOccupancy.Write();
            uint32 plotStateReceivers = 0;
            for (MapReference const& ref : GetPlayers())
                if (Player* viewer = ref.GetSource())
                {
                    viewer->SendDirectMessage(plotOccupancy.GetRawPacket());
                    ++plotStateReceivers;
                }

            TC_LOG_ERROR("housing", "[PinSync] plot {} worldstate {} -> {} broadcast to {} players",
                plotIndex, wsId, owned ? 1 : 0, plotStateReceivers);
        }

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
    // Resolve by the persisted counter: the GUID cannot be rebuilt here (arg1 is the NeighborhoodMapID).
    _neighborhood = sNeighborhoodMgr.GetNeighborhoodByCounter(static_cast<uint64>(_neighborhoodId));

    if (!_neighborhood)
        TC_LOG_ERROR("housing", "HousingMap::LoadNeighborhoodData: Failed to load neighborhood {} for map {} instanceId {}",
            _neighborhoodId, GetId(), GetInstanceId());
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
        player->SendTransferAborted(GetId(), TRANSFER_ABORT_HOUSING_MAX_PLAYERS_IN_HOUSE);
        return false;
    }

    // Do NOT auto-add the player as a neighborhood member: membership is granted on plot purchase or invite.

    // Track the player's housing: exact GUID match first, then fall back to checking all housings (legacy data).
    Housing* housing = player->GetHousingForNeighborhood(_neighborhood->GetGuid());
    if (!housing)
    {
        for (Housing const* h : player->GetAllHousings())
        {
            // The plot must be this house's own: an occupied plot of the same number may belong to someone else.
            Neighborhood::PlotInfo const* plotInfo = h ? _neighborhood->GetPlotInfo(h->GetPlotIndex()) : nullptr;
            if (plotInfo && plotInfo->OwnerGuid == h->GetOwnerGuid())
            {
                housing = const_cast<Housing*>(h);
                housing->SetNeighborhoodGuid(_neighborhood->GetGuid());
                break;
            }
        }
    }

    if (housing)
    {
        // Keyed by the buying character; another character of the account may be here.
        AddPlayerHousing(housing->GetOwnerGuid(), housing);

        uint8 plotIdx = housing->GetPlotIndex();
        ObjectGuid ownerBnetGuid = player->GetSession() ? player->GetSession()->GetBattlenetAccountGUID() : ObjectGuid::Empty;
        if (!housing->GetHouseGuid().IsEmpty())
            _neighborhood->UpdatePlotHouseInfo(plotIdx, housing->GetHouseGuid(), ownerBnetGuid);

        // Point PlayerMirrorHouse.MapID at this map or the client rejects edit mode.
        player->UpdateHousingMapId(housing->GetHouseGuid(), static_cast<int32>(GetId()));

        // Spawn the house if not already present (offline -> online transition)
        bool alreadySpawned = _houseGameObjects.find(plotIdx) != _houseGameObjects.end();

        if (!alreadySpawned)
        {
            int32 exteriorComponentID = static_cast<int32>(housing->GetCoreExteriorComponentID());
            int32 houseExteriorWmoDataID = static_cast<int32>(housing->GetHouseType());
            if (!exteriorComponentID || !houseExteriorWmoDataID)
            {
                TC_LOG_ERROR("housing", "HousingMap::AddPlayerToMap: Plot {} has invalid data: ExteriorComponentID={}, WmoDataID={} — skipping spawn",
                    plotIdx, exteriorComponentID, houseExteriorWmoDataID);
            }
            else
            {
                auto fixtureOverrides = housing->GetFixtureOverrideMap();
                FixtureOverrideMap const* overridesPtr = fixtureOverrides.empty() ? nullptr : &fixtureOverrides;
                auto rootOverrides = housing->GetRootComponentOverrides();
                RootOverrideMap const* rootOvrPtr = &rootOverrides;

                if (housing->HasCustomPosition())
                {
                    Position customPos = housing->GetHousePosition();
                    SpawnHouseForPlot(plotIdx, &customPos, exteriorComponentID, houseExteriorWmoDataID, overridesPtr, rootOvrPtr);
                }
                else
                    SpawnHouseForPlot(plotIdx, nullptr, exteriorComponentID, houseExteriorWmoDataID, overridesPtr, rootOvrPtr);
            }
        }
        else
        {
            // House GO was spawned during map init; apply fixture data and spawn the MeshObjects if not yet done.
            auto meshItr = _meshObjects.find(plotIdx);
            bool hasMeshObjects = meshItr != _meshObjects.end() && !meshItr->second.empty();
            if (!hasMeshObjects && !housing->GetHouseGuid().IsEmpty())
            {
                if (GameObject* houseGo = GetHouseGameObject(plotIdx))
                {
                    // Do NOT call InitHousingFixtureData on the GO — attaching it to a GO crashes the client.
                    Position pos = houseGo->GetPosition();
                    QuaternionData rot = houseGo->GetLocalRotation();
                    int32 faction = _neighborhood->GetFactionRestriction();

                    int32 lateExtCompID = static_cast<int32>(housing->GetCoreExteriorComponentID());
                    int32 lateWmoDataID = static_cast<int32>(housing->GetHouseType());

                    auto lateFixtureOvr = housing->GetFixtureOverrideMap();
                    FixtureOverrideMap const* lateFixturePtr = lateFixtureOvr.empty() ? nullptr : &lateFixtureOvr;
                    auto lateRootOvr = housing->GetRootComponentOverrides();
                    RootOverrideMap const* lateRootPtr = &lateRootOvr;

                    SpawnFullHouseMeshObjects(plotIdx, pos, rot,
                        housing->GetHouseGuid(), lateExtCompID, lateWmoDataID, faction,
                        lateFixturePtr, lateRootPtr);

                    if (_roomEntities.find(plotIdx) == _roomEntities.end())
                        SpawnRoomForPlot(plotIdx, pos, rot, housing->GetHouseGuid());
                }
            }
        }

        SpawnAllDecorForPlot(plotIdx, housing);

        // The session HousingPlayerHouseEntity keeps EntityGUID=Empty; the client's icon picker
        // classifies own vs friend vs stranger by comparing BnetAccount against the local BnetGuid.
    }
    else
    {
        TC_LOG_ERROR("housing", "HousingMap::AddPlayerToMap: Player {} has NO housing in this neighborhood (no house will spawn)",
            player->GetGUID().ToString());
    }

    // Point the session's Housing/4 mirror at THIS neighborhood before Map::AddPlayerToMap builds the self bundle.
    if (WorldSession* session = player->GetSession())
    {
        HousingNeighborhoodMirrorEntity& mirrorEntity = session->GetHousingNeighborhoodMirrorEntity();
        if (mirrorEntity.GetGUID() != _neighborhood->GetGuid())
            mirrorEntity.ResetGuid(_neighborhood->GetGuid());
        _neighborhood->RebuildMirrorDataFor(player);
        if (housing)
            housing->SyncUpdateFields();
    }

    if (!Map::AddPlayerToMap(player, initPlayer))
        return false;

    // Force immediate visibility update so MeshObjects get their CREATE now, not on the next tick.
    player->UpdateVisibilityForPlayer();

    // ENTER_PLOT must be sent after the AT's UPDATE_OBJECT is flushed; the deferred event gives it time.
    // SetPlayerCurrentPlot here keeps the AT script's alreadyOnPlot guard from double-sending.
    if (housing)
    {
        uint8 plotIndex = housing->GetPlotIndex();
        SetPlayerCurrentPlot(player->GetGUID(), plotIndex);

        ObjectGuid playerGuid = player->GetGUID();
        ObjectGuid houseGuid = housing->GetHouseGuid();
        ObjectGuid neighborhoodGuid = housing->GetNeighborhoodGuid();
        player->m_Events.AddEventAtOffset([playerGuid, plotIndex, houseGuid, neighborhoodGuid]()
        {
            Player* p = ObjectAccessor::FindPlayer(playerGuid);
            if (!p || !p->IsInWorld())
                return;

            HousingMap* hMap = dynamic_cast<HousingMap*>(p->GetMap());
            if (!hMap)
                return;

            AreaTrigger* plotAt = hMap->GetPlotAreaTrigger(plotIndex);
            if (!plotAt)
            {
                TC_LOG_ERROR("housing", "HousingMap deferred ENTER_PLOT: No AT for plot {} player {}",
                    plotIndex, playerGuid.ToString());
                return;
            }

            // Arriving is not being on the plot: leaving the house puts the owner at TeleportPosition, outside the AT.
            bool onPlot = plotAt->GetInsideUnits().contains(playerGuid);
            if (!onPlot && hMap->GetPlayerCurrentPlot(playerGuid) == static_cast<int8>(plotIndex))
                hMap->ClearPlayerCurrentPlot(playerGuid);

            // Retail sends a VALUES update on the AT (or a CREATE when the client does not hold it) with ENTER_PLOT.
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

            // Re-CREATE all fixture MeshObjects for this plot; entities sent during map load are never re-processed otherwise.
            {
                auto const& meshMap = hMap->GetPlotMeshObjects();
                auto meshItr = meshMap.find(plotIndex);
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
                }
            }

            if (Housing* housing = p->GetHousing())
            {
                housing->PopulateCatalogStorageEntries();
                housing->SyncUpdateFields();

                // Write the player's HouseGuid to CurrentHouse in the same bundle (the AT's OnUnitEnter never fires at login).
                if (onPlot)
                    p->SetCurrentHouse(housing->GetHouseGuid());

                // Same reason: the AT script's HouseStatus/Permissions push never runs at login.
                if (onPlot)
                {
                    WorldPackets::Housing::HousingHouseStatusResponse statusResponse;
                    statusResponse.HouseGuid = housing->GetHouseGuid();
                    statusResponse.AccountGuid = p->GetSession()->GetBattlenetAccountGUID();
                    statusResponse.OwnerPlayerGuid = p->GetGUID();
                    statusResponse.Status = 0;
                    statusResponse.EditModeFlags = housing->GetEditModeStatusFlags();
                    p->SendDirectMessage(statusResponse.Write());

                    WorldPackets::Housing::HousingGetPlayerPermissionsResponse permResponse;
                    permResponse.HouseGuid = housing->GetHouseGuid();
                    permResponse.ResultCode = 0;
                    permResponse.PermissionFlags = HOUSING_PERMISSIONS_OWNER;
                    p->SendDirectMessage(permResponse.Write());
                }

                WorldSession* session = p->GetSession();

                // Replay the retail response sequence of the client's auto-sent CMSG_HOUSING_DECOR_REQUEST_STORAGE.
                WorldPackets::Housing::HousingDecorRequestStorageResponse storageAck;
                storageAck.ResultCode = static_cast<uint8>(HOUSING_RESULT_SUCCESS);
                p->SendDirectMessage(storageAck.Write());

                UpdateData storageUpdate(p->GetMapId());
                WorldPacket storagePacket;

                // Account as full CREATE (retail re-CREATEs the BNetAccount entity in its storage response)
                session->BuildHousingAccountEntitiesUpdate(&storageUpdate, p, /*accountAsCreate=*/true);

                // Bundle ALL decor MeshObject CREATEs so the client correlates them with FHousingStorage_C in one pass.
                for (auto const& [decorGuid, meshObjGuid] : hMap->GetDecorGuidMap())
                {
                    MeshObject* meshObj = hMap->GetMeshObject(meshObjGuid);
                    if (!meshObj || !meshObj->IsInWorld())
                        continue;

                    meshObj->BuildCreateUpdateBlockForPlayer(&storageUpdate, p);
                    p->m_clientGUIDs.insert(meshObjGuid);
                }

                storageUpdate.BuildPacket(&storagePacket);
                p->SendDirectMessage(&storagePacket);

                session->GetBattlenetAccount().ClearUpdateMask(true);
                session->GetHousingPlayerHouseEntity().ClearUpdateMask(true);

                // Simulate the edit-mode ON -> OFF flag transition that unblocks cornerstone and door interactivity at login.
                {
                    // "ON" push
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

                    // "OFF" push
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
        }, Milliseconds(500));
    }

    // Retail emits the post-tutorial aura trio and the map-entry aura burst synchronously at map entry.
    SendHousingPostTutorialAuras(player);
    SendNeighborhoodMapEntryAuras(player);

    // Start the periodic WorldState counter timer; the exact seed doesn't matter as long as the increment pattern is right.
    {
        uint32 baseSeed = getMSTime();
        player->m_Events.AddEventAtOffset(
            new HousingWorldStateCounterEvent(player->GetGUID(),
                baseSeed, baseSeed / 3, baseSeed + 55758738,
                baseSeed * 2, baseSeed + 123456789),
            Milliseconds(HOUSING_WORLDSTATE_INTERVAL_MS));
    }

    SendPerPlayerPlotWorldStates(player);

    return true;
}

void HousingMap::RemovePlayerFromMap(Player* player, bool remove)
{
    // Remove plot auras first; the house is registered under its buyer, possibly another character of the account.
    Housing const* leavingHousing = _neighborhood ? player->GetHousingForNeighborhood(_neighborhood->GetGuid()) : nullptr;
    if (Housing const* housing = leavingHousing ? GetHousingForPlayer(leavingHousing->GetOwnerGuid()) : nullptr)
        SendPlotLeaveAuraRemoval(player);

    // Cleared for EVERY player leaving: the map never unloads, so a visitor's stale entry would live forever.
    ClearPlayerCurrentPlot(player->GetGUID());

    if (leavingHousing)
        RemovePlayerHousing(leavingHousing->GetOwnerGuid());

    Map::RemovePlayerFromMap(player, remove);
}

void SendHousingPostTutorialAuras(Player* player)
{
    // After QUEST_HOUSING_TUTORIAL_COMPLETE, three persistent auras at slots 8, 9, 50 (not in DB2; manual SMSG_AURA_UPDATE).
    if (!player->GetQuestRewardStatus(QUEST_HOUSING_TUTORIAL_COMPLETE))
        return;

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
        auraInfo.AuraData->Flags = AFLAG_SELF_CAST;
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
        auraInfo.AuraData->Flags = AFLAG_SELF_CAST;
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
        auraInfo.AuraData->Flags = AFLAG_SELF_CAST | AFLAG_SCALABLE;
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

}

void HousingMap::SendNeighborhoodMapEntryAuras(Player* player)
{
    // Map-entry aura burst: one AURA_UPDATE + SPELL_START + SPELL_GO triple per entry.
    // CastLevel uses the player's level; pre-existing character/ambient auras are left to the core aura resync.

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
        { SPELL_HOUSING_MAP_ENTRY_FIXUP,    20,  AFLAG_SELF_CAST,                1, 0                                        },
        { SPELL_HOUSING_MAP_ENTRY_REACT,    22,  AFLAG_SELF_CAST,                1, 0                                        },
        { SPELL_HOUSING_MAP_ENTRY_ENDEAVOR, 53,  AFLAG_SELF_CAST,                1, 0                                        },
        { SPELL_HOUSING_MAP_ENTRY_NEIGHBOR, 121, uint16(AFLAG_SELF_CAST | AFLAG_POSITIVE), 3, VISUAL_HOUSING_MAP_ENTRY_NEIGHBOR },
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

}

void HousingMap::SendPlotEnterSpellPackets(Player* player, uint8 plotIndex)
{
    // Plot AT overlap spells (not map-entry), invoked from at_housing_plot's OnUnitEnter.
    // Manual packets are required - these DNT spell IDs don't exist in DB2.

    // 1. Spell 1239847 — plot enter tracking aura (slot 55, NOT slot 50 which is the tutorial aura)
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
        auraInfo.AuraData->Flags = AFLAG_SELF_CAST;
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

    // 2. Set HasPlayers on the plot AreaTrigger, between the first and second spell sets.
    if (AreaTrigger* plotAt = GetPlotAreaTrigger(plotIndex))
        plotAt->SetAreaTriggerFlag(AreaTriggerFieldFlags::HasPlayers);

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
        auraInfo.AuraData->Flags = AFLAG_SELF_CAST;
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
        auraInfo.AuraData->Flags = AFLAG_SELF_CAST | AFLAG_SCALABLE;
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

}

void HousingMap::SendPlotLeaveAuraRemoval(Player* player)
{
    // Remove the plot enter/presence auras (empty AuraData = HasAura=false)
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
}

HousingPlotOwnerType HousingMap::GetPlotOwnerTypeForPlayer(Player const* player, uint8 plotIndex) const
{
    if (!_neighborhood || !player)
        return HOUSING_PLOT_OWNER_NONE;

    Neighborhood::PlotInfo const* plotInfo = _neighborhood->GetPlotInfo(plotIndex);
    if (!plotInfo || plotInfo->OwnerGuid.IsEmpty())
        return HOUSING_PLOT_OWNER_NONE;

    if (plotInfo->OwnerGuid == player->GetGUID())
        return HOUSING_PLOT_OWNER_SELF;

    // Alt on the same BNet account (same person, different character)
    if (!plotInfo->OwnerBnetGuid.IsEmpty() && player->GetSession())
    {
        if (plotInfo->OwnerBnetGuid == player->GetSession()->GetBattlenetAccountGUID())
            return HOUSING_PLOT_OWNER_SELF;
    }

    if (PlayerSocial* social = player->GetSocial())
    {
        if (social->HasFriend(plotInfo->OwnerGuid))
            return HOUSING_PLOT_OWNER_FRIEND;
    }

    return HOUSING_PLOT_OWNER_STRANGER;
}

void HousingMap::SendPerPlayerPlotWorldStates(Player* player)
{
    // Blizzlike no-op: per-plot occupancy travels in SMSG_INIT_WORLD_STATES; retained as an extension hook.
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
}

void HousingMap::RemovePlayerHousing(ObjectGuid playerGuid)
{
    _playerHousings.erase(playerGuid);
}

// House Structure GO Management

GameObject* HousingMap::SpawnHouseForPlot(uint8 plotIndex, Position const* customPos,
    int32 exteriorComponentID, int32 houseExteriorWmoDataID,
    FixtureOverrideMap const* fixtureOverrides /*= nullptr*/,
    RootOverrideMap const* rootOverrides /*= nullptr*/)
{
    if (!_neighborhood)
        return nullptr;

    uint32 neighborhoodMapId = _neighborhood->GetNeighborhoodMapID();
    std::vector<NeighborhoodPlotData const*> const& plots = sHousingMgr.GetPlotsForMap(neighborhoodMapId);

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

    // Ground-clamp a Z to the walkable surface (DB2 Z and dragging-client Z can sit below ground).
    auto groundClamp = [this, plotIndex](Position& p)
    {
        LoadGrid(p.GetPositionX(), p.GetPositionY());
        PhaseShift tempPhase;
        PhasingHandler::InitDbPhaseShift(tempPhase, PHASE_USE_FLAGS_ALWAYS_VISIBLE, 0, 0);
        float groundZ = GetHeight(tempPhase, p.GetPositionX(), p.GetPositionY(), p.GetPositionZ() + 50.0f, true, 100.0f);
        if (groundZ > INVALID_HEIGHT && groundZ > p.GetPositionZ() - 5.0f)
        {
            p.Relocate(p.GetPositionX(), p.GetPositionY(), groundZ, p.GetOrientation());
        }
    };

    // Client-chosen positions keep their height verbatim (snapping sinks houses on slopes); only the DB2 default spot is clamped.
    Position plotPos = sHousingMgr.GetDefaultHousePosition(*targetPlot);
    Position housePosition = plotPos;
    if (customPos)
        housePosition = *customPos;
    else
        groundClamp(housePosition);

    LoadGrid(plotPos.GetPositionX(), plotPos.GetPositionY());

    float x = housePosition.GetPositionX();
    float y = housePosition.GetPositionY();
    float z = housePosition.GetPositionZ();
    float facing = housePosition.GetOrientation();

    // Pure yaw: DB2 HouseRotation X/Y are always 0 and the computed facing is a yaw.
    QuaternionData rot = QuaternionData::fromEulerAnglesZYX(facing, 0.0f, 0.0f);

    // The platform WMO (GO entry 574432) is a static spawn; a dynamic second platform would render on top.

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

    // Spawn house MeshObjects from the DB2 ExteriorComponent tree: base and door pieces are roots, others attach with local-space transforms.
    if (plotInfo && !plotInfo->HouseGuid.IsEmpty())
    {
        int32 faction = _neighborhood->GetFactionRestriction();
        TC_LOG_ERROR("housing", "HousingMap::SpawnHouseForPlot: Spawning MeshObjects for plot {} — "
            "HouseGuid={} faction={} ({})",
            plotIndex, plotInfo->HouseGuid.ToString(), faction,
            faction == NEIGHBORHOOD_FACTION_ALLIANCE ? "Alliance" : "Horde");

        // Room entity + Geobox mesh first — the house meshes attach to it and the client validates against the Geobox.
        SpawnRoomForPlot(plotIndex, plotPos, QuaternionData::fromEulerAnglesZYX(plotPos.GetOrientation(), 0.0f, 0.0f),
            plotInfo->HouseGuid);

        // Exterior root Entity first: the base and roof meshes attach to it.
        SpawnOrMoveHouseRootEntity(plotIndex, pos,
            MakeHouseMirrorGuid(plotIndex, static_cast<uint32>(plotInfo->OwnerBnetGuid.GetCounter())));

        SpawnFullHouseMeshObjects(plotIndex, pos, rot, plotInfo->HouseGuid,
            exteriorComponentID, houseExteriorWmoDataID, faction, fixtureOverrides, rootOverrides);

        // Group B Entity mirrors: one per visible exterior fixture MeshObject, AttachParent = the piece's own MeshObject.
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
                    // 9=Base, 10=Roof, 11=Door, 12=Window — other types don't get mirrors.
                    if (compType < 9 || compType > 12)
                        continue;

                    ObjectGuid mirrorGuid = MakeHouseMeshMirrorGuid(plotIndex, bnetId, pieceIndex);
                    auto mirror = std::make_unique<HousingMirrorEntity>(this, mirrorGuid);
                    mirror->InitPositionData(meshGuid,
                        localPos, identity, /*scale*/ 1.0f, /*attachmentFlags*/ 3,
                        HousingMirrorEntity::Tagging::None);
                    mirrors.push_back(std::move(mirror));
                    ++pieceIndex;
                }
            }

            if (mirrors.empty())
            {
                TC_LOG_WARN("housing", "HousingMap::SpawnHouseForPlot: no fixture MeshObjects found for plot {}; "
                    "Group B mirrors skipped (client spatial anchors off house meshes will be missing)", plotIndex);
            }
        }
    }

    // Door GO spawning happens inside SpawnExtCompTree (Type=11 with GameObjectID > 0).
    return GetHouseGameObject(plotIndex);
}

void HousingMap::SpawnRoomForPlot(uint8 plotIndex, Position const& housePos,
    QuaternionData const& houseRot, ObjectGuid houseGuid)
{
    // The client needs a Room entity with a Geobox-carrying MeshObject to define the plot boundary.
    // Geobox bounds are identical for all factions: (-35,-30,-1.01)->(35,30,125.01).

    int32 const houseRoomId = static_cast<int32>(sHousingMgr.GetBaseRoomEntryId());
    static constexpr int32 ROOM_FLAGS        = 1;     // BASE_ROOM
    static constexpr int32 ROOM_COMPONENT_ID = 196;   // sniff-verified: same for alliance+horde

    // The plot room depends only on the plot: keep an existing one (see DespawnHouseForPlot).
    if (GetRoomIdentityEntity(plotIndex))
        return;

    // 1. HouseRoom -> RoomWmoDataID
    HouseRoomEntry const* houseRoomEntry = sHouseRoomStore.LookupEntry(houseRoomId);
    int32 roomWmoDataID = houseRoomEntry ? houseRoomEntry->RoomWmoDataID : 0;

    // 2. RoomWmoData -> Geobox bounds (sniff fallback: (-35,-30,-1.01)->(35,30,125.01))
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

    // 3. RoomComponent -> ModelFileDataID, Type (sniff fallback: FileDataID=6322976, Type=2)
    int32 fileDataID = 6322976;
    uint8 roomComponentType = 2;
    RoomComponentEntry const* compEntry = sRoomComponentStore.LookupEntry(ROOM_COMPONENT_ID);
    if (compEntry)
    {
        if (compEntry->ModelFileDataID > 0)
            fileDataID = compEntry->ModelFileDataID;
        roomComponentType = compEntry->Type;
    }

    // 4. RoomComponentOption: cosmetic only, does not affect the Geobox/bounds check.
    int32 roomComponentOptionID = 0;
    int32 houseThemeID = 0;
    int32 roomComponentTextureID = 0;
    int32 field24 = 0;

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
    // Alliance defaults when no DB2 entry matches
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

    // Retail architecture: a Housing/2 identity entity plus a component MeshObject attached to it that carries the Geobox.
    // The identity is the single authoritative room — the mesh must not also carry FHousingRoom_C.

    // 1. Housing/2 identity entity.
    ObjectGuid roomIdentityGuid = ObjectGuid::Create<HighGuid::Housing>(
        /*subType*/ 2,
        /*arg1*/ 0,
        /*arg2*/ static_cast<uint32>(houseRoomId),
        /*counter*/ static_cast<ObjectGuid::LowType>(plotIndex + 1));

    HousingRoomEntity* roomIdentity = new HousingRoomEntity();
    if (!roomIdentity->Create(roomIdentityGuid, this, housePos))
    {
        TC_LOG_ERROR("housing", "HousingMap::SpawnRoomForPlot: Failed to create Housing/2 identity room for plot {}", plotIndex);
        delete roomIdentity;
        return;
    }

    roomIdentity->SetHouseGUID(houseGuid);
    roomIdentity->SetHouseRoomID(houseRoomId);
    roomIdentity->SetFlags(ROOM_FLAGS);
    roomIdentity->SetFloorIndex(0);

    // With AttachParent empty, PositionLocalSpace must be the room's WORLD position or all attached decor lands at the origin.
    roomIdentity->SetMirroredPosition(housePos, houseRot,
        /*scale*/ 1.0f, ObjectGuid::Empty, /*attachFlags*/ 3);

    // 1b. Doors on the identity entity (read by the client's HousingRoomSystem).
    if (std::vector<RoomDoorInfo> const* doors = sHousingMgr.GetRoomDoors(roomWmoDataID))
    {
        for (RoomDoorInfo const& door : *doors)
        {
            Position doorOffset(door.OffsetPos[0], door.OffsetPos[1], door.OffsetPos[2], 0.0f);
            roomIdentity->AddDoor(static_cast<int32>(door.RoomComponentID), doorOffset,
                /*connectionType*/ static_cast<uint8>(7) /*HOUSING_ROOM_COMPONENT_DOORWAY*/,
                /*attachedRoomGuid*/ ObjectGuid::Empty);
        }
    }
    else
    {
        TC_LOG_WARN("housing", "HousingMap::SpawnRoomForPlot: plot={} no door data for roomWmoDataID={} — fixture hookpoints will show 'None'",
            plotIndex, roomWmoDataID);
    }

    // 2. Component MeshObject — attaches to the identity and carries the Geobox.
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

    // 3. Link: add the component GUID to the identity's MeshObjects array.
    roomIdentity->AddMeshObject(componentMesh->GetGUID());

    // 4. Add the component to the map (the identity was AddToMap'd via its Create()).
    if (!AddToMap(componentMesh))
    {
        TC_LOG_ERROR("housing", "HousingMap::SpawnRoomForPlot: Failed to add room component mesh to map for plot {}", plotIndex);
        roomIdentity->AddObjectToRemoveList();
        delete componentMesh;
        return;
    }

    _roomIdentityGuids[plotIndex] = roomIdentityGuid;
    _roomComponentMeshes[plotIndex] = componentMesh->GetGUID();
    // Legacy _roomEntities tracking points at the identity GUID.
    _roomEntities[plotIndex] = roomIdentityGuid;

    TC_LOG_ERROR("housing", "HousingMap::SpawnRoomForPlot: plot={} identity={} component={} "
        "at ({:.1f},{:.1f},{:.1f}) geobox=({:.2f},{:.2f},{:.2f})->({:.2f},{:.2f},{:.2f})",
        plotIndex, roomIdentityGuid.ToString(), componentMesh->GetGUID().ToString(),
        housePos.GetPositionX(), housePos.GetPositionY(), housePos.GetPositionZ(),
        geoMinX, geoMinY, geoMinZ, geoMaxX, geoMaxY, geoMaxZ);
}

void HousingMap::DespawnRoomForPlot(uint8 plotIndex)
{
    // The exterior root is attached to the room.
    DespawnHouseRootEntity(plotIndex);

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

    // Legacy _roomEntities tracking; entity already removed above.
    _roomEntities.erase(plotIndex);
}

MeshObject* HousingMap::SpawnHouseMeshObject(uint8 plotIndex, int32 fileDataID, bool isWMO,
    Position const& pos, QuaternionData const& rot, float scale,
    ObjectGuid houseGuid, int32 exteriorComponentID, int32 houseExteriorWmoDataID,
    uint8 exteriorComponentType /*= 9*/, uint8 houseSize /*= 2*/, int32 exteriorComponentHookID /*= -1*/,
    ObjectGuid attachParent /*= ObjectGuid::Empty*/, uint8 attachFlags /*= 0*/,
    Position const* worldPos /*= nullptr*/)
{
    // worldPos is the parent's world position, used for grid placement of child pieces.
    if (worldPos)
        LoadGrid(worldPos->GetPositionX(), worldPos->GetPositionY());
    else
        LoadGrid(pos.GetPositionX(), pos.GetPositionY());

    // HousingFixtureData.Size is the piece's own ExteriorComponent.Size, not the house size.
    if (ExteriorComponentEntry const* component = sExteriorComponentStore.LookupEntry(exteriorComponentID))
        houseSize = component->Size;

    MeshObject* mesh = MeshObject::CreateMeshObject(this, pos, rot, scale, fileDataID, isWMO,
        attachParent, attachFlags, worldPos);
    if (!mesh)
    {
        TC_LOG_ERROR("housing", "HousingMap::SpawnHouseMeshObject: CreateMeshObject failed for plot {} fileDataID {}",
            plotIndex, fileDataID);
        return nullptr;
    }

    // Set up all entity fragments BEFORE AddToMap; the base piece gets Tag_HouseExteriorRoot (225), others Tag_HouseExteriorPiece (224).
    bool isRoot = (exteriorComponentType == 9) && (attachParent.IsEmpty() || attachParent.GetHigh() == HighGuid::Housing
        || attachParent.GetHigh() == HighGuid::Entity);

    // Guid = the MeshObject's own GUID, AttachParentGUID = the parent MeshObject's GUID (not Housing GUIDs), or hookpoint linking fails.
    ObjectGuid fixtureGuid = mesh->GetGUID(); // self-reference: the MeshObject's own GUID

    ObjectGuid parentFixtureGuid;
    if (!attachParent.IsEmpty())
    {
        if (MeshObject* parentMesh = GetMeshObject(attachParent))
            parentFixtureGuid = parentMesh->GetGUID(); // parent's MeshObject GUID
        else if (attachParent.GetHigh() == HighGuid::Entity)
            parentFixtureGuid = attachParent; // retail: a root's fixture parent is the exterior root Entity
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

    return mesh;
}

void HousingMap::SpawnFullHouseMeshObjects(uint8 plotIndex, Position const& housePos,
    QuaternionData const& houseRot, ObjectGuid houseGuid,
    int32 exteriorComponentID, int32 houseExteriorWmoDataID,
    int32 factionRestriction /*= NEIGHBORHOOD_FACTION_ALLIANCE*/,
    FixtureOverrideMap const* fixtureOverrides /*= nullptr*/,
    RootOverrideMap const* rootOverrides /*= nullptr*/)
{
    // Build the house from the DB2 ExteriorComponent tree. Root selection per type:
    // 1. rootOverrides, 2. the core component, 3. DB2 default (Flags & 0x1 IsDefault).
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

            // Every root attaches to the house's exterior-root entity; the base root takes that role, others ride at (0,0,0).
            std::vector<std::pair<uint8, uint32>> selectedRoots; // type, component
            for (auto const& [type, compIDs] : rootsByType)
            {
                uint32 selectedCompID = 0;

                // 1. The player's root override for this type; ignore a different-size (stale) override or the rebuild locks to the old size.
                if (rootOverrides)
                {
                    auto ovrItr = rootOverrides->find(type);
                    if (ovrItr != rootOverrides->end())
                    {
                        ExteriorComponentEntry const* ovrComp = sExteriorComponentStore.LookupEntry(ovrItr->second);
                        if (ovrComp && ovrComp->Size == houseSize)
                            selectedCompID = ovrItr->second;
                    }
                }

                // 2. For the core type, use the player's selected coreExtCompID
                if (!selectedCompID && type == coreComp->Type)
                    selectedCompID = coreExtCompID;

                // 3. Fall back to DB2 default for this type + wmoDataID at the house's size
                if (!selectedCompID)
                {
                    uint32 defaultID = sHousingMgr.GetDefaultFixtureForType(type, wmoDataID, houseSize);
                    if (defaultID)
                        selectedCompID = defaultID;
                }

                // 4. Last resort: first available in the list
                if (!selectedCompID && !compIDs.empty())
                    selectedCompID = compIDs[0];

                if (selectedCompID)
                    selectedRoots.emplace_back(type, selectedCompID);
            }

            // Base first: the other roots attach to it.
            std::stable_sort(selectedRoots.begin(), selectedRoots.end(), [](auto const& a, auto const& b)
            {
                return (a.first == HOUSING_FIXTURE_TYPE_BASE) > (b.first == HOUSING_FIXTURE_TYPE_BASE);
            });

            HousingRoomEntity const* roomId = GetRoomIdentityEntity(plotIndex);
            HousingRoomEntity const* rootEntity = GetHouseRootEntity(plotIndex);
            QuaternionData identity;
            identity.x = identity.y = identity.z = 0.0f;
            identity.w = 1.0f;
            ObjectGuid baseRootGuid;
            for (auto const& [type, selectedCompID] : selectedRoots)
            {
                std::vector<ObjectGuid>& plotMeshes = _meshObjects[plotIndex];
                size_t const firstNew = plotMeshes.size();
                // Retail: every root (base, roof) sits at local 0 on the exterior root Entity.
                if (rootEntity)
                    totalSpawned += SpawnExtCompTree(plotIndex, selectedCompID,
                        Position(0.0f, 0.0f, 0.0f, 0.0f), identity,
                        houseGuid, houseExteriorWmoDataID,
                        rootEntity->GetGUID(), &housePos, 0, fixtureOverrides);
                else if (!baseRootGuid.IsEmpty())
                    totalSpawned += SpawnExtCompTree(plotIndex, selectedCompID,
                        Position(0.0f, 0.0f, 0.0f, 0.0f), identity,
                        houseGuid, houseExteriorWmoDataID,
                        baseRootGuid, &housePos, 0, fixtureOverrides);
                else if (roomId)
                    totalSpawned += SpawnExtCompTree(plotIndex, selectedCompID,
                        HousingWorldToRoomLocal(roomId->GetPosition(), housePos),
                        QuaternionData::fromEulerAnglesZYX(housePos.GetOrientation() - roomId->GetOrientation(), 0.0f, 0.0f),
                        houseGuid, houseExteriorWmoDataID,
                        roomId->GetGUID(), &housePos, 0, fixtureOverrides);
                else
                    totalSpawned += SpawnExtCompTree(plotIndex, selectedCompID,
                        housePos, houseRot,
                        houseGuid, houseExteriorWmoDataID,
                        ObjectGuid::Empty, nullptr, 0, fixtureOverrides);

                // SpawnExtCompTree registers the root before its children.
                if (baseRootGuid.IsEmpty() && plotMeshes.size() > firstNew)
                    baseRootGuid = plotMeshes[firstNew];
            }
        }

        if (totalSpawned > 0)
        {
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
    // Root 0: base (comp 141) with door child (comp 1380, hook 2505); root 1: roof (comp 1503) with chimney + window children.

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

    // Child of base: Door mesh (ExteriorComponentID 1380, type 11, hookID 2505); local space relative to base.
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
}

void HousingMap::SpawnHordeHouseMeshObjects(uint8 plotIndex, Position const& housePos,
    QuaternionData const& houseRot, ObjectGuid houseGuid,
    int32 /*exteriorComponentID*/, int32 /*houseExteriorWmoDataID*/)
{
    // === HORDE EXTERIOR (HouseExteriorWmoDataID=87) ===
    // Root 0: main structure (comp 3811) with door/wall/roof children; root 1: base (comp 1003).

    int32 hordeWmoDataID = HORDE_HOUSE_EXTERIOR_WMO_DATA_ID; // 87

    // Root piece 0: Main structure
    MeshObject* rootPiece = SpawnHouseMeshObject(plotIndex, 7118906, /*isWMO*/ true,
        housePos, houseRot, 1.0f,
        houseGuid, 3811, hordeWmoDataID,
        /*exteriorComponentType*/ 10, /*houseSize*/ 2, /*hookID*/ -1,
        ObjectGuid::Empty, /*attachFlags*/ 0);

    // Root piece 1: Base structure
    SpawnHouseMeshObject(plotIndex, 6648685, /*isWMO*/ true,
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

    // The hookIDOverride must propagate at all depths: initial house spawn iterates hooks at depth >= 1.
    int32 effectiveHookID = (hookIDOverride > 0) ? hookIDOverride : -1;

    MeshObject* mesh = SpawnHouseMeshObject(plotIndex, comp->ModelFileDataID, /*isWMO*/ true,
        pos, rot, 1.0f,
        houseGuid, static_cast<int32>(extCompID), houseExteriorWmoDataID,
        comp->Type, comp->Size, effectiveHookID,
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

    // Cumulative world position: parent's world position composed with the local hook offset, rotated by the parent's cumulative yaw.
    Position thisMeshWorldPos;
    if (!worldPos)
    {
        thisMeshWorldPos = pos;
    }
    else if (depth == 0)
    {
        thisMeshWorldPos = *worldPos;
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
        // Z-axis rotation contribution of the hook quaternion for the cumulative yaw.
        float thisZRot = std::atan2(
            2.0f * (rot.w * rot.z + rot.x * rot.y),
            1.0f - 2.0f * (rot.y * rot.y + rot.z * rot.z));
        thisMeshWorldPos.Relocate(wx, wy, wz, parentFacing + thisZRot);
    }

    // Door component (Type=11): spawn a separate interactive GO at the door mesh's world position.
    if (comp->Type == 11 && comp->GameObjectID > 0)
    {
        uint32 doorGoEntry = static_cast<uint32>(comp->GameObjectID);
        GameObjectTemplate const* doorTemplate = sObjectMgr->GetGameObjectTemplate(doorGoEntry);
        if (doorTemplate)
        {
            float doorWorldX = thisMeshWorldPos.GetPositionX();
            float doorWorldY = thisMeshWorldPos.GetPositionY();
            float doorWorldZ = thisMeshWorldPos.GetPositionZ();
            float doorFacing = thisMeshWorldPos.GetOrientation();
            float cosFacing = std::cos(doorFacing);
            float sinFacing = std::sin(doorFacing);

            // The door GO attaches at ExteriorComponent.EntryOffset in door-local space (the ExitPoint is where players are put outside).
            doorWorldX += comp->Position[0] * cosFacing - comp->Position[1] * sinFacing;
            doorWorldY += comp->Position[0] * sinFacing + comp->Position[1] * cosFacing;
            doorWorldZ += comp->Position[2];

            Position doorPos(doorWorldX, doorWorldY, doorWorldZ, doorFacing);
            QuaternionData doorRot = QuaternionData::fromEulerAnglesZYX(doorFacing, 0.0f, 0.0f);

            // Drop the previously-tracked door GO first, or it leaks as a phantom clickable box.
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
                    // Door interaction lives entirely in go_housing_door::OnGossipHello.
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

    // Children receive this mesh's cumulative world position as their parent transform.
    Position const* childWorldPos = &thisMeshWorldPos;

    auto const* hooks = sHousingMgr.GetHooksOnComponent(extCompID);

    // Spawn child components at hooks from the player's fixture overrides.
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

            // Hook position/rotation are the local-space coordinates where the child mesh attaches.
            Position hookPos(hook->Position[0], hook->Position[1], hook->Position[2], 0.0f);
            QuaternionData const hookRot = GetHookLocalRotation(hook);

            count += SpawnExtCompTree(plotIndex, childComp->ID,
                hookPos, hookRot,
                houseGuid, houseExteriorWmoDataID,
                meshGuid, childWorldPos, depth + 1, fixtureOverrides,
                static_cast<int32>(hook->ID));
        }
    }

    // NOTE: ExteriorComponent.ParentComponentID links are color/dye variants, not structural children.

    return count;
}

void HousingMap::SendPlotMeshObjectsToPlayers(uint8 plotIndex)
{
    auto meshItr = _meshObjects.find(plotIndex);
    if (meshItr == _meshObjects.end() || meshItr->second.empty())
    {
        TC_LOG_ERROR("housing", "HousingMap::SendPlotMeshObjectsToPlayers: plot {} has no MeshObjects to send", plotIndex);
        return;
    }

    for (MapReference const& ref : GetPlayers())
    {
        Player* p = ref.GetSource();
        if (!p || !p->IsInWorld())
            continue;

        UpdateData updateData(GetId());
        uint32 created = 0;
        for (ObjectGuid const& meshGuid : meshItr->second)
        {
            MeshObject* meshObj = GetMeshObject(meshGuid);
            if (!meshObj || !meshObj->IsInWorld())
                continue;

            // Re-CREATEing a GUID the client already holds crashes it - refresh instead.
            if (p->HaveAtClient(meshObj))
            {
                meshObj->BuildValuesUpdateBlockForPlayer(&updateData, p);
                continue;
            }

            meshObj->BuildCreateUpdateBlockForPlayer(&updateData, p);
            p->m_clientGUIDs.insert(meshGuid);
            ++created;
        }

        if (!updateData.HasData())
            continue;

        WorldPacket packet;
        updateData.BuildPacket(&packet);
        p->SendDirectMessage(&packet);

    }
}

void HousingMap::SendPlotGeometryEntitiesToPlayer(uint8 plotIndex, Player* player)
{
    if (!player || !player->IsInWorld())
        return;

    // The client validates placement/moves against these geometry entities, which grid visibility never delivers.
    UpdateData updateData(GetId());
    uint32 created = 0, updated = 0;

    auto pushEntity = [&](BaseEntity* entity)
    {
        if (!entity)
            return;
        // No IsInWorld check — Group B mirrors are never map-added and only reach the client via hand-built bundles.
        if (player->HaveAtClient(entity))
        {
            entity->BuildValuesUpdateBlockForPlayer(&updateData, player);
            ++updated;
        }
        else
        {
            entity->BuildCreateUpdateBlockForPlayer(&updateData, player);
            player->m_clientGUIDs.insert(entity->GetGUID());
            ++created;
        }
    };

    if (HousingRoomEntity* room = GetRoomIdentityEntity(plotIndex))
        pushEntity(room);

    if (auto meshItr = _roomComponentMeshes.find(plotIndex); meshItr != _roomComponentMeshes.end())
        if (MeshObject* geobox = GetMeshObject(meshItr->second))
            pushEntity(geobox);

    if (HousingRoomEntity* root = GetHouseRootEntity(plotIndex))
        pushEntity(root);

    for (HousingMirrorEntity* mirror : GetHouseMeshMirrors(plotIndex))
        pushEntity(mirror);

    if (!updateData.HasData())
        return;

    WorldPacket packet;
    updateData.BuildPacket(&packet);
    player->SendDirectMessage(&packet);

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

    _meshObjects.erase(itr);
}

void HousingMap::SendPlotGeometryEntitiesToMap(uint8 plotIndex)
{
    // Push the geometry to every viewer; HaveAtClient gating keeps it safe for players who already hold it.
    for (MapReference const& ref : GetPlayers())
        if (Player* viewer = ref.GetSource())
            SendPlotGeometryEntitiesToPlayer(plotIndex, viewer);
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

}

MeshObject* HousingMap::SpawnFixtureAtHook(uint8 plotIndex, uint32 hookID, uint32 componentID,
    ObjectGuid houseGuid, int32 houseExteriorWmoDataID, Player* target, ObjectGuid parentHint)
{
    ExteriorComponentHookEntry const* hookEntry = sExteriorComponentHookStore.LookupEntry(hookID);
    if (!hookEntry)
    {
        TC_LOG_ERROR("housing", "HousingMap::SpawnFixtureAtHook: Hook {} not found in DB2", hookID);
        return nullptr;
    }

    // The client's parent hint wins: facade variants re-key the parent mesh's ExteriorComponentID.
    MeshObject* parentMesh = nullptr;
    auto meshItr = _meshObjects.find(plotIndex);
    if (meshItr != _meshObjects.end())
    {
        if (!parentHint.IsEmpty())
        {
            if (std::find(meshItr->second.begin(), meshItr->second.end(), parentHint) != meshItr->second.end())
                parentMesh = GetMeshObject(parentHint);
        }

        if (!parentMesh)
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
    }

    if (!parentMesh)
    {
        TC_LOG_ERROR("housing", "HousingMap::SpawnFixtureAtHook: Parent mesh for hook {} (parent comp {}, client hint {}) not found on plot {}",
            hookID, hookEntry->ExteriorComponentID, parentHint.ToString(), plotIndex);
        return nullptr;
    }

    // Hook position/rotation are local-space offsets relative to the parent
    Position hookPos(hookEntry->Position[0], hookEntry->Position[1], hookEntry->Position[2], 0.0f);
    QuaternionData const hookRot = GetHookLocalRotation(hookEntry);

    // Use the parent's world position for grid placement
    Position parentWorldPos(parentMesh->GetPositionX(), parentMesh->GetPositionY(),
        parentMesh->GetPositionZ(), parentMesh->GetOrientation());

    // Pass hookID as the override so the top-level mesh gets the actual hook point, not the component's native one.
    uint32 spawned = SpawnExtCompTree(plotIndex, componentID,
        hookPos, hookRot,
        houseGuid, houseExteriorWmoDataID,
        parentMesh->GetGUID(), &parentWorldPos, /*depth*/ 1, nullptr,
        static_cast<int32>(hookID));

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
    // The plot room stays: it belongs to the plot, not the house, and the house root and yard decor hang off it.
    DespawnAllMeshObjectsForPlot(plotIndex);

    // The exterior root Entity stays (SpawnOrMoveHouseRootEntity moves it); the mesh mirrors go with the meshes.
    _houseMeshMirrorEntities.erase(plotIndex);

    auto itr = _houseGameObjects.find(plotIndex);
    if (itr == _houseGameObjects.end())
        return;

    if (GameObject* go = GetGameObject(itr->second))
        go->AddObjectToRemoveList();

    _houseGameObjects.erase(itr);
}

HousingRoomEntity* HousingMap::GetHouseRootEntity(uint8 plotIndex) const
{
    auto itr = _houseRootEntityGuids.find(plotIndex);
    if (itr == _houseRootEntityGuids.end())
        return nullptr;
    return const_cast<HousingMap*>(this)->GetObjectsStore().Find<HousingRoomEntity>(itr->second);
}

ObjectGuid HousingMap::GetHouseMirrorGuid(uint8 plotIndex) const
{
    if (HousingRoomEntity const* root = GetHouseRootEntity(plotIndex))
        return root->GetGUID();
    return ObjectGuid::Empty;
}

void HousingMap::SpawnOrMoveHouseRootEntity(uint8 plotIndex, Position const& housePos, ObjectGuid rootGuid)
{
    HousingRoomEntity const* room = GetRoomIdentityEntity(plotIndex);
    if (!room)
    {
        TC_LOG_ERROR("housing", "HousingMap::SpawnOrMoveHouseRootEntity: plot {} has no room identity", plotIndex);
        return;
    }

    // House offset inside the plot, relative to the plot room (retail PositionLocalSpace/RotationLocalSpace).
    Position const localPos = HousingWorldToRoomLocal(room->GetPosition(), housePos);
    QuaternionData const localRot = QuaternionData::fromEulerAnglesZYX(housePos.GetOrientation() - room->GetOrientation(), 0.0f, 0.0f);

    HousingRoomEntity* root = GetHouseRootEntity(plotIndex);
    if (root && root->GetGUID() != rootGuid)
    {
        DespawnHouseRootEntity(plotIndex); // new owner
        root = nullptr;
    }

    if (!root)
    {
        root = new HousingRoomEntity(/*exteriorRoot*/ true);
        if (!root->Create(rootGuid, this, housePos))
        {
            TC_LOG_ERROR("housing", "HousingMap::SpawnOrMoveHouseRootEntity: failed to create root {} for plot {}",
                rootGuid.ToString(), plotIndex);
            delete root;
            return;
        }
        _houseRootEntityGuids[plotIndex] = rootGuid;
    }

    // An existing root just gets a VALUES update, as retail sends after CMSG_HOUSE_EXTERIOR_SET_HOUSE_POSITION.
    root->Relocate(housePos);
    root->SetMirroredPosition(localPos, localRot, 1.0f, room->GetGUID(), /*attachFlags*/ 3);
}

void HousingMap::DespawnHouseRootEntity(uint8 plotIndex)
{
    auto itr = _houseRootEntityGuids.find(plotIndex);
    if (itr == _houseRootEntityGuids.end())
        return;
    if (HousingRoomEntity* root = GetObjectsStore().Find<HousingRoomEntity>(itr->second))
        root->AddObjectToRemoveList();
    _houseRootEntityGuids.erase(itr);
}

ObjectGuid HousingMap::MakeHouseMirrorGuid(uint8 plotIndex, uint32 bnetAccountId, uint8 pieceIndex /*= 0*/) const
{
    // Deterministic convention: HighGuid::Entity, synthetic entry 37361, counter packs (bnetAccountId, plotIndex, pieceIndex).
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
    // Returns the first (root-piece) Group B mirror for legacy callers.
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
    // Distinct synthetic entry from Group A (37361); counter packs (bnetId, plot, piece).
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

    _houseGameObjects.erase(itr);
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

// Decor Management

bool HousingMap::SpawnDecorItem(uint8 plotIndex, Housing::PlacedDecor const& decor, ObjectGuid houseGuid)
{
    HouseDecorData const* decorData = sHousingMgr.GetHouseDecorData(decor.DecorEntryId);
    if (!decorData)
    {
        TC_LOG_ERROR("housing", "HousingMap::SpawnDecorItem: No HouseDecorData for entry {} (decorGuid={})",
            decor.DecorEntryId, decor.Guid.ToString());
        return false;
    }

    // Look up the room entity we attach to.
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

    // World → room-local.
    Position worldPos(worldX, worldY, worldZ);
    Position localPos = roomEntityGuid.IsEmpty() ? worldPos : HousingWorldToRoomLocal(roomWorldPos, worldPos);
    // The mirrored rotation must be in the room frame (worldRot = roomRot ⊗ localRot on the client).
    QuaternionData const localRot = roomEntityGuid.IsEmpty() ? rot : HousingWorldRotationToRoomLocal(roomWorldPos.GetOrientation(), rot);
    float decorScale = decor.Scale > 0.01f ? decor.Scale : 1.0f;
    uint8 attachFlags = roomEntityGuid.IsEmpty() ? uint8(0) : uint8(3);

    // ---------- Functional decor branch (real GameObject) ----------
    // Spawn as a real GameObject so the client treats it as interactive; requires a matching gameobject_template.
    if (decorData->GameObjectID > 0)
    {
        uint32 goEntry = static_cast<uint32>(decorData->GameObjectID);
        if (GameObjectTemplate const* goTemplate = sObjectMgr->GetGameObjectTemplate(goEntry))
        {
            // Orientation from the quaternion: the packed quat drives rendering, the yaw the stationary direction.
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

                // Keep template default flags — chairs need CHAIR, chests CHEST, mailboxes MAILBOX behavior.

                go->InitHousingDecorData(decor.Guid, houseGuid, decor.Locked ? 1 : 0,
                    roomEntityGuid, decor.SourceType, decor.SourceValue);
                go->SetHousingDecorDyeSlots(decor.DyeSlots);
                go->InitHousingDecorMirroredPosition(localPos, localRot, decorScale, roomEntityGuid, attachFlags);

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

                return true;
            }
        }
    }

    // ---------- Visual-only branch (MeshObject) ----------
    int32 fileDataID = decorData->ModelFileDataID;
    if (fileDataID <= 0 && decorData->GameObjectID > 0)
    {
        // Fallback: derive FileDataID from the GO template displayInfo.
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

    MeshObject* mesh = MeshObject::CreateMeshObject(this, localPos, localRot, decorScale,
        fileDataID, /*isWMO*/ decorData->ModelType == HOUSE_DECOR_MODEL_TYPE_WMO, roomEntityGuid, attachFlags, &worldPos);

    if (!mesh)
    {
        TC_LOG_ERROR("housing", "HousingMap::SpawnDecorItem: Failed to create decor MeshObject fileDataID={} for decor {}",
            fileDataID, decor.Guid.ToString());
        return false;
    }

    PhasingHandler::InitDbPhaseShift(mesh->GetPhaseShift(), PHASE_USE_FLAGS_ALWAYS_VISIBLE, 0, 0);
    mesh->InitHousingDecorData(decor.Guid, houseGuid, decor.Locked ? 1 : 0, roomEntityGuid, decor.SourceType, decor.SourceValue);
    mesh->SetHousingDecorDyeSlots(decor.DyeSlots);

    if (!AddToMap(mesh))
    {
        TC_LOG_ERROR("housing", "HousingMap::SpawnDecorItem: Failed to add decor MeshObject to map for decor {}", decor.Guid.ToString());
        delete mesh;
        return false;
    }

    _decorGameObjects[plotIndex].push_back(mesh->GetGUID());
    _decorGuidToGoGuid[decor.Guid] = mesh->GetGUID();
    _decorGuidToPlotIndex[decor.Guid] = plotIndex;

    return true;
}

void HousingMap::DespawnDecorItem(uint8 plotIndex, ObjectGuid decorGuid)
{
    // Remove the bound companion creature first, then the decor itself.
    auto petItr = _decorGuidToPetSummon.find(decorGuid);
    if (petItr != _decorGuidToPetSummon.end())
    {
        if (Creature* pet = GetCreature(petItr->second))
            pet->DespawnOrUnsummon();
        _decorGuidToPetSummon.erase(petItr);
    }

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

void HousingMap::UpdateDecorDyes(ObjectGuid decorGuid, std::array<uint32, MAX_HOUSING_DYE_SLOTS> const& dyeSlots)
{
    auto itr = _decorGuidToGoGuid.find(decorGuid);
    if (itr == _decorGuidToGoGuid.end())
        return;

    if (itr->second.IsGameObject())
    {
        if (GameObject* go = GetGameObject(itr->second))
            go->SetHousingDecorDyeSlots(dyeSlots);
    }
    else if (MeshObject* mesh = GetMeshObject(itr->second))
        mesh->SetHousingDecorDyeSlots(dyeSlots);
}

void HousingMap::UpdateDecorPet(ObjectGuid decorGuid, ObjectGuid battlePetGuid, uint32 creatureId,
    std::string const& petName, uint8 petBehavior)
{
    auto itr = _decorGuidToGoGuid.find(decorGuid);
    if (itr == _decorGuidToGoGuid.end())
        return;

    WorldObject* decorObj = nullptr;
    if (itr->second.IsGameObject())
        decorObj = GetGameObject(itr->second);
    else
        decorObj = GetMeshObject(itr->second);
    if (!decorObj)
        return;

    // Drop the previous companion for this decor first (bind-over-bind and unbind both pass here).
    auto summonItr = _decorGuidToPetSummon.find(decorGuid);
    if (summonItr != _decorGuidToPetSummon.end())
    {
        if (Creature* oldPet = GetCreature(summonItr->second))
            oldPet->DespawnOrUnsummon();
        _decorGuidToPetSummon.erase(summonItr);
    }

    ObjectGuid spawnedPetGuid;
    if (!battlePetGuid.IsEmpty() && creatureId != 0)
    {
        // Spawn the companion creature beside the decor (passive until pet AI is tuned).
        if (CreatureTemplate const* creatureTemplate = sObjectMgr->GetCreatureTemplate(creatureId))
        {
            Position petPos = decorObj->GetPosition();
            float const escapeDistance = 1.5f;
            petPos.Relocate(petPos.GetPositionX() + std::cos(petPos.GetOrientation()) * escapeDistance,
                petPos.GetPositionY() + std::sin(petPos.GetOrientation()) * escapeDistance,
                petPos.GetPositionZ());

            if (TempSummon* summon = SummonCreature(creatureId, petPos, nullptr, Milliseconds(0), decorObj))
            {
                summon->SetReactState(REACT_PASSIVE);
                summon->SetImmuneToAll(true);
                summon->SetControlled(true, UNIT_STATE_ROOT);
                spawnedPetGuid = summon->GetGUID();
                _decorGuidToPetSummon[decorGuid] = spawnedPetGuid;
            }
        }
    }

    decorObj->SetHousingDecorPet(battlePetGuid, creatureId, petName, petBehavior, spawnedPetGuid);
}

void HousingMap::UpdateDecorPosition(uint8 plotIndex, ObjectGuid decorGuid, Position const& pos, QuaternionData const& rot, float scale /*= 1.0f*/)
{
    auto itr = _decorGuidToGoGuid.find(decorGuid);
    if (itr == _decorGuidToGoGuid.end())
        return;

    // Move the room-relative transform the client renders from (FMirroredPositionData_C) as well.
    HousingRoomEntity* roomId = GetRoomIdentityEntity(plotIndex);
    Position localPos = roomId ? HousingWorldToRoomLocal(roomId->GetPosition(), pos) : pos;
    QuaternionData const localRot = roomId ? HousingWorldRotationToRoomLocal(roomId->GetOrientation(), rot) : rot;

    ObjectGuid objGuid = itr->second;
    if (objGuid.IsGameObject())
    {
        if (GameObject* go = GetGameObject(objGuid))
        {
            go->Relocate(pos);
            go->SetLocalRotation(rot.x, rot.y, rot.z, rot.w);
            if (std::abs(go->GetObjectScale() - scale) > 0.001f)
                go->SetObjectScale(scale);
            go->UpdateHousingDecorMirroredTransform(localPos, localRot, scale);
        }
    }
    else if (MeshObject* mesh = GetMeshObject(objGuid))
    {
        mesh->Relocate(pos);
        mesh->UpdateLocalTransform(localPos, localRot, scale);
    }
}
