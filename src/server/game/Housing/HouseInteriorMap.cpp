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

#include "HouseInteriorMap.h"
#include "Account.h"
#include "BattlePetMgr.h"
#include "Creature.h"
#include "DB2Stores.h"
#include "DBCEnums.h"
#include "GameObject.h"
#include "Housing.h"
#include "HousingDecorEntity.h"
#include "HousingDefines.h"
#include "HousingMgr.h"
#include "HousingPackets.h"
#include "HousingPlayerHouseEntity.h"
#include "HousingRoomEntity.h"
#include "Log.h"
#include "MeshObject.h"
#include "Neighborhood.h"
#include "NeighborhoodMgr.h"
#include "ObjectAccessor.h"
#include "ObjectMgr.h"
#include "PhasingHandler.h"
#include "Player.h"
#include "TemporarySummon.h"
#include "World.h"
#include "WorldSession.h"
#include <algorithm>

namespace
{
    // Retail interior exit door GO flags (0x40000, sniffed).
    constexpr GameObjectFlags INTERIOR_DOOR_GO_FLAGS = GameObjectFlags(0x40000);
}

HouseInteriorMap::HouseInteriorMap(uint32 id, time_t expiry, uint32 instanceId, ObjectGuid const& owner)
    : Map(id, expiry, instanceId, DIFFICULTY_NORMAL),
      _owner(owner),
      _loadingPlayer(nullptr),
      _sourceNeighborhoodMapId(0),
      _sourcePlotIndex(0),
      _roomsSpawned(false)
{
    HouseInteriorMap::InitVisibilityDistance();

    // Look up interior origin from NeighborhoodMap DB2 data for this world map
    if (NeighborhoodMapData const* nmData = sHousingMgr.GetNeighborhoodMapDataForWorldMap(id))
    {
        _originX = nmData->Origin[0];
        _originY = nmData->Origin[1];
        _originZ = nmData->Origin[2];
    }
}

void HouseInteriorMap::InitVisibilityDistance()
{
    // Maximum visibility so all room entities and decor are CREATEd immediately on entry.
    m_VisibleDistance = MAX_VISIBILITY_DISTANCE;
    m_VisibilityNotifyPeriod = sWorld->getIntConfig(CONFIG_VISIBILITY_NOTIFY_PERIOD_INSTANCE);
}

void HouseInteriorMap::LoadGridObjects(NGridType* grid)
{
    Map::LoadGridObjects(grid);
    // Room WMO geometry spawns when the owner enters; no static spawns on the interior map template.
}

Housing* HouseInteriorMap::GetOwnerHousing()
{
    // Houses belong to the account: any character of the owner's account carries this house.
    if (_loadingPlayer)
        if (Housing* housing = _loadingPlayer->GetHousingByOwner(_owner))
            return housing;

    for (MapReference const& ref : GetPlayers())
        if (Housing* housing = ref.GetSource()->GetHousingByOwner(_owner))
            return housing;

    if (Player* owner = ObjectAccessor::FindConnectedPlayer(_owner))
        return owner->GetHousingByOwner(_owner);

    return nullptr;
}

bool HouseInteriorMap::IsHouseOwnerAccount(Player const* player) const
{
    return player->GetGUID() == _owner || player->GetHousingByOwner(_owner) != nullptr;
}

void HouseInteriorMap::SpawnRoomMeshObjects(Housing* housing, int32 factionRestriction)
{
    if (!housing)
        return;
    SpawnRoomMeshObjectsFromList(housing->GetRooms(), factionRestriction, housing->GetHouseGuid());
}

Position HouseInteriorMap::GetRoomWorldPosition(Housing::Room const& room) const
{
    // GridX/GridY are yard offsets; world Z = FloorIndex * 12 yards.
    static constexpr float FLOOR_HEIGHT_Y = 12.0f;
    return Position(_originX + static_cast<float>(room.GridX), _originY + static_cast<float>(room.GridY),
        _originZ + static_cast<float>(room.FloorIndex) * FLOOR_HEIGHT_Y, static_cast<float>(room.Orientation) * float(M_PI / 2.0));
}

std::unordered_map<uint32, HouseInteriorMap::DoorwayState> HouseInteriorMap::GetDoorwayStates(
    std::vector<Housing::Room const*> const& rooms, Housing::Room const& room)
{
    std::unordered_map<uint32, DoorwayState> states;
    for (Housing::RoomDoor const& door : Housing::GetRoomDoors(room))
    {
        if (door.IsVertical())
            continue; // stairwell floor/ceiling: a link, not a doorway

        uint32 otherComponentId = 0;
        Housing::Room const* other = Housing::FindRoomAtDoor(rooms, room, door, &otherComponentId);
        if (!other)
            continue;

        DoorwayState& state = states[door.ComponentId];
        state.AttachedRoom = other->Guid;
        state.Owner = Housing::OwnsDoorway(room, *other);
        state.Variant = Housing::GetDoorwayVariant(room, door.ComponentId, *other, otherComponentId);
    }
    return states;
}

int32 HouseInteriorMap::GetComponentThemeID(Housing::Room const& room, RoomComponentData const& comp, int32 factionThemeID)
{
    // The slot's own theme first, then the per-surface theme older rows carry, the legacy single ThemeId, the faction.
    auto itr = room.ComponentThemes.find(comp.ID);
    if (itr != room.ComponentThemes.end() && itr->second)
        return static_cast<int32>(itr->second);

    uint32 perSurfaceTheme = 0;
    switch (comp.Type)
    {
        case HOUSING_ROOM_COMPONENT_WALL:
        case HOUSING_ROOM_COMPONENT_DOORWAY_WALL:
            perSurfaceTheme = room.WallThemeId;
            break;
        case HOUSING_ROOM_COMPONENT_FLOOR:
            perSurfaceTheme = room.FloorThemeId;
            break;
        case HOUSING_ROOM_COMPONENT_CEILING:
            perSurfaceTheme = room.CeilingThemeId;
            break;
        default:
            break;
    }
    return perSurfaceTheme ? static_cast<int32>(perSurfaceTheme)
        : (room.ThemeId != 0 ? static_cast<int32>(room.ThemeId) : factionThemeID);
}

int32 HouseInteriorMap::GetComponentHouseThemeID(Housing::Room const& room, RoomComponentData const& comp, RoomComponentOptionEntry const* option)
{
    // HouseThemeID carries the chosen sub-theme (retail: 10 "Bel'ameth (neutral)" on an option of base theme 4).
    int32 chosenTheme = GetComponentThemeID(room, comp, 0);
    if (chosenTheme > 0 && (chosenTheme == static_cast<int32>(option->HouseThemeID)
        || sHousingMgr.GetBaseThemeID(chosenTheme) == static_cast<int32>(option->HouseThemeID)))
        return chosenTheme;
    return sHousingMgr.GetDefaultSubThemeID(option->HouseThemeID);
}

std::vector<RoomComponentOptionEntry const*> HouseInteriorMap::SelectComponentOptions(Housing::Room const& room,
    RoomComponentData const& comp, int32 factionThemeID, DoorwayState const* doorway)
{
    int32 rawTheme = GetComponentThemeID(room, comp, factionThemeID);
    // Option rows only exist for base themes; resolve stored sub-themes to the parent base theme or the style is lost.
    int32 lookupTheme = sHousingMgr.GetBaseThemeID(rawTheme);
    if (lookupTheme <= 0)
        lookupTheme = rawTheme;
    // Theme 0 means "any theme" to FindAllRoomComponentOptions, so only the last fallback below may pass it
    std::vector<RoomComponentOptionEntry const*> allOptions;
    if (lookupTheme > 0)
        allOptions = sHousingMgr.FindAllRoomComponentOptions(comp.MeshStyleFilterID, lookupTheme);
    if (allOptions.empty() && factionThemeID > 0)
        allOptions = sHousingMgr.FindAllRoomComponentOptions(comp.MeshStyleFilterID, factionThemeID);
    if (allOptions.empty() && factionThemeID != 2)
        allOptions = sHousingMgr.FindAllRoomComponentOptions(comp.MeshStyleFilterID, 2);
    if (allOptions.empty() && factionThemeID != 1)
        allOptions = sHousingMgr.FindAllRoomComponentOptions(comp.MeshStyleFilterID, 1);
    // Some shapes ship options in a single theme only (the round rooms' filters 65-68).
    if (allOptions.empty())
        allOptions = sHousingMgr.FindAllRoomComponentOptions(comp.MeshStyleFilterID, 0);

    std::sort(allOptions.begin(), allOptions.end(), [](RoomComponentOptionEntry const* a, RoomComponentOptionEntry const* b) { return a->ID < b->ID; });

    // One slot, one look: a connected door gets the connection variant's DoorwayWall (+ Doorway pieces on the owning
    // side); anything else gets a single Cosmetic piece of the chosen variant.
    std::vector<RoomComponentOptionEntry const*> selected;
    if (doorway)
    {
        for (RoomComponentOptionEntry const* option : allOptions)
        {
            if (option->RoomComponentID != doorway->Variant)
                continue;
            if (option->Type == HOUSING_ROOM_COMPONENT_OPTION_DOORWAY_WALL
                || (doorway->Owner && option->Type == HOUSING_ROOM_COMPONENT_OPTION_DOORWAY))
                selected.push_back(option);
        }
        return selected;
    }

    int32 variant = 0;
    if (comp.Type == HOUSING_ROOM_COMPONENT_CEILING && room.CeilingTypeId == comp.ID)
        variant = room.CeilingSlot;
    else if (comp.Type == HOUSING_ROOM_COMPONENT_STAIRS)
        variant = 1; // sniffed stairwell: stairs piece 432 (variant 1)

    // Every Cosmetic piece of that variant that has its own model; fall back to variant 0, then a single piece.
    auto collect = [&](int32 wantedVariant)
    {
        for (RoomComponentOptionEntry const* option : allOptions)
            if (option->Type == HOUSING_ROOM_COMPONENT_OPTION_COSMETIC && option->ModelFileDataID > 0
                && option->RoomComponentID == wantedVariant)
                selected.push_back(option);
    };
    collect(variant);
    if (selected.empty() && variant != 0)
        collect(0);
    if (selected.size() > 1 && comp.MeshStyleFilterID == 0)
        selected.resize(1); // unfiltered style: the options are alternatives, not pieces
    if (selected.empty())
    {
        RoomComponentOptionEntry const* fallback = nullptr;
        for (RoomComponentOptionEntry const* option : allOptions)
            if (option->Type == HOUSING_ROOM_COMPONENT_OPTION_COSMETIC && (!fallback || (fallback->ModelFileDataID <= 0 && option->ModelFileDataID > 0)))
                fallback = option;
        if (fallback)
            selected.push_back(fallback);
    }
    return selected;
}

MeshObject* HouseInteriorMap::CreateRoomComponentMesh(Housing::Room const& room, RoomComponentData const& comp,
    RoomComponentOptionEntry const* option, Position const& roomPos)
{
    int32 compFileDataID = option->ModelFileDataID > 0 ? option->ModelFileDataID : comp.ModelFileDataID;
    if (compFileDataID <= 0)
        return nullptr; // No model for this option

    // Component position/rotation: local to room entity
    Position compPos(comp.OffsetPos[0], comp.OffsetPos[1], comp.OffsetPos[2], 0.0f);
    QuaternionData compRot;
    // DB2 OffsetRot is in DEGREES; Z is negated.
    static constexpr float DEG_TO_RAD = static_cast<float>(M_PI / 180.0);
    float rx = comp.OffsetRot[0] * DEG_TO_RAD;
    float ry = comp.OffsetRot[1] * DEG_TO_RAD;
    float rz = -comp.OffsetRot[2] * DEG_TO_RAD;
    float cx = std::cos(rx / 2.0f), sx = std::sin(rx / 2.0f);
    float cy = std::cos(ry / 2.0f), sy = std::sin(ry / 2.0f);
    float cz = std::cos(rz / 2.0f), sz = std::sin(rz / 2.0f);
    compRot.x = sx * cy * cz - cx * sy * sz;
    compRot.y = cx * sy * cz + sx * cy * sz;
    compRot.z = cx * cy * sz - sx * sy * cz;
    compRot.w = cx * cy * cz + sx * sy * sz;

    // RoomWmoData → Geobox bounds
    float geoMinX = -35.0f, geoMinY = -30.0f, geoMinZ = -1.01f;
    float geoMaxX =  35.0f, geoMaxY =  30.0f, geoMaxZ = 125.01f;
    HouseRoomData const* roomData = sHousingMgr.GetHouseRoomData(room.RoomEntryId);
    if (RoomWmoDataEntry const* wmoData = roomData && roomData->RoomWmoDataID ? sRoomWmoDataStore.LookupEntry(roomData->RoomWmoDataID) : nullptr)
    {
        geoMinX = wmoData->BoundingBoxMinX;
        geoMinY = wmoData->BoundingBoxMinY;
        geoMinZ = wmoData->BoundingBoxMinZ;
        geoMaxX = wmoData->BoundingBoxMaxX;
        geoMaxY = wmoData->BoundingBoxMaxY;
        geoMaxZ = wmoData->BoundingBoxMaxZ;
    }

    int32 roomComponentOptionID = static_cast<int32>(option->ID);
    int32 houseThemeID = GetComponentHouseThemeID(room, comp, option);

    // Material: the slot's own, else the per-surface one older rows carry
    int32 roomComponentTextureID = 0;
    uint32 storedTexture = 0;
    auto textureItr = room.ComponentTextures.find(comp.ID);
    if (textureItr != room.ComponentTextures.end())
        storedTexture = textureItr->second;
    else
    {
        switch (comp.Type)
        {
            case HOUSING_ROOM_COMPONENT_WALL:
            case HOUSING_ROOM_COMPONENT_DOORWAY_WALL:
                storedTexture = room.WallTextureId;
                break;
            case HOUSING_ROOM_COMPONENT_FLOOR:
                storedTexture = room.FloorTextureId;
                break;
            case HOUSING_ROOM_COMPONENT_CEILING:
                storedTexture = room.CeilingTextureId;
                break;
            default:
                break;
        }
    }
    if (storedTexture != 0)
        roomComponentTextureID = static_cast<int32>(storedTexture);
    else
    {
        roomComponentTextureID = sHousingMgr.GetTextureIdForComponentOption(roomComponentOptionID);
        if (roomComponentTextureID == 0)
            roomComponentTextureID = sHousingMgr.GetTextureIdForComponentType(comp.Type);
        if (roomComponentTextureID == 0)
        {
            switch (comp.Type)
            {
                case 1: roomComponentTextureID = 24; break;
                case 2: roomComponentTextureID = 40; break;
                case 3: roomComponentTextureID = 54; break;
                default: break;
            }
        }
    }

    MeshObject* componentMesh = MeshObject::CreateMeshObject(this, compPos, compRot, 1.0f,
        compFileDataID, /*isWMO*/ true, room.Guid, /*attachFlags*/ 3, &roomPos);
    if (!componentMesh)
    {
        TC_LOG_ERROR("housing", "HouseInteriorMap::CreateRoomComponentMesh: CreateMeshObject failed for component "
            "(compID={}, option={}, fileDataID={}, roomEntry={})", comp.ID, option->ID, compFileDataID, room.RoomEntryId);
        return nullptr;
    }

    PhasingHandler::InitDbPhaseShift(componentMesh->GetPhaseShift(), PHASE_USE_FLAGS_ALWAYS_VISIBLE, 0, 0);
    // Sniff: Field_20 = option Type, RoomComponentTypeParam = option variant (739 → 2, 366 → 1, 329 → 0).
    componentMesh->InitHousingRoomComponentData(room.Guid,
        roomComponentOptionID, static_cast<int32>(comp.ID),
        comp.Type, static_cast<int32>(option->SubType), static_cast<uint8>(option->Type),
        houseThemeID, roomComponentTextureID,
        /*roomComponentTypeParam*/ option->RoomComponentID,
        geoMinX, geoMinY, geoMinZ,
        geoMaxX, geoMaxY, geoMaxZ);
    return componentMesh;
}

void HouseInteriorMap::SpawnRoomMeshObjectsFromList(std::vector<Housing::Room const*> const& rooms, int32 factionRestriction, ObjectGuid houseGuid)
{
    if (rooms.empty())
    {
        TC_LOG_ERROR("housing", "HouseInteriorMap::SpawnRoomMeshObjects: No rooms to spawn for owner {} "
            "(new house — rooms will appear when placed via editor)",
            _owner.ToString());
        return;
    }

    int32 factionThemeID = sHousingMgr.GetFactionDefaultThemeID(factionRestriction);
    uint32 totalMeshes = 0;

    TC_LOG_ERROR("housing", "HouseInteriorMap::SpawnRoomMeshObjects: Starting spawn for {} rooms "
        "(owner={}, factionThemeID={}, houseGuid={})",
        uint32(rooms.size()), _owner.ToString(), factionThemeID, houseGuid.ToString());

    // Upper floors first: the client prices a stairwell once only when the upper half is already known.
    std::vector<Housing::Room const*> spawnOrder(rooms.begin(), rooms.end());
    std::stable_sort(spawnOrder.begin(), spawnOrder.end(), [](Housing::Room const* a, Housing::Room const* b)
    {
        return a->FloorIndex != b->FloorIndex ? a->FloorIndex > b->FloorIndex : a->SlotIndex < b->SlotIndex;
    });

    for (Housing::Room const* room : spawnOrder)
    {
        // Skip rooms that already have entities on the map (incremental spawn for room add).
        if (_roomMeshObjects.contains(room->Guid))
            continue;

        HouseRoomData const* roomData = sHousingMgr.GetHouseRoomData(room->RoomEntryId);
        if (!roomData)
        {
            TC_LOG_ERROR("housing", "HouseInteriorMap::SpawnRoomMeshObjects: Unknown room entry {} "
                "for room {} (owner {})",
                room->RoomEntryId, room->Guid.ToString(), _owner.ToString());
            continue;
        }

        int32 roomWmoDataID = roomData->RoomWmoDataID;
        std::vector<RoomComponentData> const* components = sHousingMgr.GetRoomComponents(roomWmoDataID);
        if (!components || components->empty())
        {
            TC_LOG_ERROR("housing", "HouseInteriorMap::SpawnRoomMeshObjects: No components for "
                "room '{}' (entry={}, roomWmoDataID={})",
                roomData->Name, room->RoomEntryId, roomWmoDataID);
            continue;
        }

        Position roomPos = GetRoomWorldPosition(*room);
        QuaternionData roomRot = QuaternionData::fromEulerAnglesZYX(roomPos.GetOrientation(), 0.0f, 0.0f);

        LoadGrid(roomPos.GetPositionX(), roomPos.GetPositionY());

        // Lock the grid so it never unloads while the interior is active
        GridCoord roomGrid = Trinity::ComputeGridCoord(roomPos.GetPositionX(), roomPos.GetPositionY());
        GridMarkNoUnload(roomGrid.x_coord, roomGrid.y_coord);

        // --- Phase 1: HousingRoomEntity ---
        // Must reach the client BEFORE its component MeshObjects (they AttachParentGUID to it); the CREATE carries both.
        HousingRoomEntity* housingRoom = new HousingRoomEntity();
        PhasingHandler::InitDbPhaseShift(housingRoom->GetPhaseShift(), PHASE_USE_FLAGS_ALWAYS_VISIBLE, 0, 0);
        housingRoom->SetHouseGUID(houseGuid);
        housingRoom->SetHouseRoomID(room->RoomEntryId);
        // HouseRoom flags minus UNLOCKED_BY_DEFAULT
        housingRoom->SetFlags(roomData->Flags & ~HOUSING_ROOM_FLAG_UNLOCKED_BY_DEFAULT);
        housingRoom->SetFloorIndex(room->FloorIndex);
        housingRoom->SetMirroredPosition(roomPos, roomRot, 1.0f);

        // --- Phase 2: component MeshObjects (not on the map yet) ---
        std::unordered_map<uint32, DoorwayState> doorways = GetDoorwayStates(rooms, *room);
        std::vector<MeshObject*> componentMeshes;

        for (RoomComponentData const& comp : *components)
        {
            if (IsComponentHidden(rooms, *room, comp))
                continue;

            auto doorway = doorways.find(comp.ID);
            for (RoomComponentOptionEntry const* option : SelectComponentOptions(*room, comp, factionThemeID,
                doorway != doorways.end() ? &doorway->second : nullptr))
                if (MeshObject* componentMesh = CreateRoomComponentMesh(*room, comp, option, roomPos))
                    componentMeshes.push_back(componentMesh);
        }

        // --- Phase 3: door list ---
        // One entry per horizontal connectable wall, connected or not.
        std::vector<Housing::RoomDoor> doors = Housing::GetRoomDoors(*room);
        for (Housing::RoomDoor const& door : doors)
        {
            Housing::Room const* attached = Housing::FindRoomAtDoor(rooms, *room, door);
            housingRoom->AddDoor(static_cast<int32>(door.ComponentId), door.Local, door.ComponentType,
                attached ? attached->Guid : ObjectGuid::Empty);
        }

        for (MeshObject* componentMesh : componentMeshes)
            housingRoom->AddMeshObject(componentMesh->GetGUID());

        if (!housingRoom->Create(room->Guid, this, roomPos))
        {
            TC_LOG_ERROR("housing", "HouseInteriorMap: Failed to add HousingRoomEntity to map (roomEntry={})",
                room->RoomEntryId);
            delete housingRoom;
            for (MeshObject* componentMesh : componentMeshes)
                delete componentMesh;
            continue;
        }
        _roomEntities.push_back(housingRoom);

        // --- Phase 4: Add all component MeshObjects to map ---
        std::vector<ObjectGuid>& roomMeshes = _roomMeshObjects[room->Guid];
        bool meshFailed = false;
        for (MeshObject* componentMesh : componentMeshes)
        {
            if (AddToMap(componentMesh))
            {
                roomMeshes.push_back(componentMesh->GetGUID());
                ++totalMeshes;
            }
            else
            {
                TC_LOG_ERROR("housing", "HouseInteriorMap::SpawnRoomMeshObjects: "
                    "AddToMap failed for component (roomEntry={})",
                    room->RoomEntryId);
                delete componentMesh;
                meshFailed = true;
            }
        }
        if (meshFailed)
            housingRoom->ReplaceMeshObjects(roomMeshes);

        TC_LOG_ERROR("housing", "HouseInteriorMap::SpawnRoomMeshObjects: Room '{}' (entry={}, slot={}) guid={} "
            "spawned {} component MeshObjects, {} doors ({} connected) at ({:.1f},{:.1f},{:.1f}) orientation={}",
            roomData->Name, room->RoomEntryId, room->SlotIndex, room->Guid.ToString(),
            uint32(componentMeshes.size()), uint32(doors.size()), uint32(doorways.size()),
            roomPos.GetPositionX(), roomPos.GetPositionY(), roomPos.GetPositionZ(), room->Orientation);
    }

    TC_LOG_ERROR("housing", "HouseInteriorMap::SpawnRoomMeshObjects: Spawned {} total MeshObjects for {} rooms "
        "(owner={}, map={}, instanceId={}, faction={})",
        totalMeshes, uint32(rooms.size()), _owner.ToString(), GetId(), GetInstanceId(),
        factionRestriction == NEIGHBORHOOD_FACTION_ALLIANCE ? "Alliance" : "Horde");
}

HousingRoomEntity* HouseInteriorMap::FindRoomEntity(ObjectGuid roomGuid) const
{
    for (HousingRoomEntity* re : _roomEntities)
        if (re && re->IsInWorld() && re->GetGUID() == roomGuid)
            return re;
    return nullptr;
}

bool HouseInteriorMap::IsComponentHidden(std::vector<Housing::Room const*> const& rooms, Housing::Room const& room, RoomComponentData const& comp)
{
    // The lower stairwell half has no ceiling, the upper one no floor and no stairs (open shaft between them).
    HouseRoomData const* roomData = sHousingMgr.GetHouseRoomData(room.RoomEntryId);
    if (!roomData || !roomData->HasStairs())
        return false;

    for (Housing::Room const* other : rooms)
    {
        if (other == &room || other->GridX != room.GridX || other->GridY != room.GridY)
            continue;
        HouseRoomData const* otherData = sHousingMgr.GetHouseRoomData(other->RoomEntryId);
        if (!otherData || !otherData->HasStairs())
            continue;
        if (other->FloorIndex == room.FloorIndex + 1 && comp.Type == HOUSING_ROOM_COMPONENT_CEILING)
            return true;
        if (other->FloorIndex == room.FloorIndex - 1
            && (comp.Type == HOUSING_ROOM_COMPONENT_FLOOR || comp.Type == HOUSING_ROOM_COMPONENT_STAIRS))
            return true;
    }
    return false;
}

void HouseInteriorMap::ReplaceSlotMeshes(Housing::Room const& room, RoomComponentData const& comp,
    std::vector<RoomComponentOptionEntry const*> const& wanted, Position const& roomPos, std::vector<ObjectGuid>& roomMeshes)
{
    // Retail answers a restyle with DESTROY of the slot's pieces and CREATE of new ones (new GUIDs).
    std::vector<ObjectGuid> current;
    for (ObjectGuid const& meshGuid : roomMeshes)
        if (MeshObject* mesh = GetMeshObject(meshGuid))
            if (mesh->GetRoomComponentID() == static_cast<int32>(comp.ID))
                current.push_back(meshGuid);

    for (ObjectGuid const& meshGuid : current)
    {
        if (MeshObject* mesh = GetMeshObject(meshGuid))
        {
            mesh->DestroyForNearbyPlayers();
            RemoveFromMap(mesh, true);
        }
        std::erase(roomMeshes, meshGuid);
    }

    for (RoomComponentOptionEntry const* option : wanted)
    {
        MeshObject* mesh = CreateRoomComponentMesh(room, comp, option, roomPos);
        if (!mesh)
            continue;
        if (AddToMap(mesh))
            roomMeshes.push_back(mesh->GetGUID());
        else
            delete mesh;
    }
}

void HouseInteriorMap::RebuildRoomComponents(std::vector<Housing::Room const*> const& rooms, Housing::Room const& room,
    int32 factionRestriction, std::vector<uint32> const& componentIds)
{
    auto meshItr = _roomMeshObjects.find(room.Guid);
    HousingRoomEntity* roomEntity = FindRoomEntity(room.Guid);
    HouseRoomData const* roomData = sHousingMgr.GetHouseRoomData(room.RoomEntryId);
    std::vector<RoomComponentData> const* components = roomData ? sHousingMgr.GetRoomComponents(roomData->RoomWmoDataID) : nullptr;
    if (meshItr == _roomMeshObjects.end() || !roomEntity || !components)
        return;

    int32 factionThemeID = sHousingMgr.GetFactionDefaultThemeID(factionRestriction);
    std::unordered_map<uint32, DoorwayState> doorways = GetDoorwayStates(rooms, room);
    Position roomPos = GetRoomWorldPosition(room);
    uint32 rebuilt = 0;

    for (RoomComponentData const& comp : *components)
    {
        if (std::find(componentIds.begin(), componentIds.end(), comp.ID) == componentIds.end())
            continue;
        if (IsComponentHidden(rooms, room, comp))
            continue;

        auto doorway = doorways.find(comp.ID);
        std::vector<RoomComponentOptionEntry const*> wanted = SelectComponentOptions(room, comp, factionThemeID,
            doorway != doorways.end() ? &doorway->second : nullptr);

        // A slot that already looks like this is left alone (retail skips it in an "apply to all").
        std::vector<std::pair<int32, int32>> currentLook, wantedLook;
        for (ObjectGuid const& meshGuid : meshItr->second)
            if (MeshObject* mesh = GetMeshObject(meshGuid))
                if (mesh->GetRoomComponentID() == static_cast<int32>(comp.ID))
                    currentLook.emplace_back(mesh->GetRoomComponentOptionID(), mesh->GetHouseThemeID());
        for (RoomComponentOptionEntry const* option : wanted)
            wantedLook.emplace_back(static_cast<int32>(option->ID), GetComponentHouseThemeID(room, comp, option));
        std::sort(currentLook.begin(), currentLook.end());
        std::sort(wantedLook.begin(), wantedLook.end());
        if (currentLook == wantedLook)
            continue;

        ReplaceSlotMeshes(room, comp, wanted, roomPos, meshItr->second);
        ++rebuilt;
    }

    if (rebuilt)
        roomEntity->ReplaceMeshObjects(meshItr->second);

}

void HouseInteriorMap::RefreshRoomDoors(std::vector<Housing::Room const*> const& rooms, int32 factionRestriction)
{
    // Rebuild only the door slots whose look changed, the way retail answers layout edits.
    int32 factionThemeID = sHousingMgr.GetFactionDefaultThemeID(factionRestriction);

    for (Housing::Room const* room : rooms)
    {
        auto meshItr = _roomMeshObjects.find(room->Guid);
        HousingRoomEntity* roomEntity = FindRoomEntity(room->Guid);
        if (meshItr == _roomMeshObjects.end() || !roomEntity)
            continue;

        HouseRoomData const* roomData = sHousingMgr.GetHouseRoomData(room->RoomEntryId);
        std::vector<RoomComponentData> const* components = roomData ? sHousingMgr.GetRoomComponents(roomData->RoomWmoDataID) : nullptr;
        if (!components)
            continue;

        std::unordered_map<uint32, DoorwayState> doorways = GetDoorwayStates(rooms, *room);
        Position roomPos = GetRoomWorldPosition(*room);
        bool meshesChanged = false;

        for (Housing::RoomDoor const& door : Housing::GetRoomDoors(*room))
        {
            auto compItr = std::find_if(components->begin(), components->end(),
                [&](RoomComponentData const& c) { return c.ID == door.ComponentId; });
            if (compItr == components->end())
                continue;

            Housing::Room const* attached = Housing::FindRoomAtDoor(rooms, *room, door);
            roomEntity->UpdateDoorConnection(static_cast<int32>(door.ComponentId), attached ? attached->Guid : ObjectGuid::Empty);
            if (door.IsVertical())
                continue; // the stairwell link has no doorway pieces

            auto doorway = doorways.find(door.ComponentId);

            std::vector<RoomComponentOptionEntry const*> wanted = SelectComponentOptions(*room, *compItr, factionThemeID,
                doorway != doorways.end() ? &doorway->second : nullptr);

            std::vector<int32> currentOptions;
            for (ObjectGuid const& meshGuid : meshItr->second)
                if (MeshObject* mesh = GetMeshObject(meshGuid))
                    if (mesh->GetRoomComponentID() == static_cast<int32>(door.ComponentId))
                        currentOptions.push_back(mesh->GetRoomComponentOptionID());

            std::vector<int32> wantedOptions;
            for (RoomComponentOptionEntry const* option : wanted)
                wantedOptions.push_back(static_cast<int32>(option->ID));
            std::sort(currentOptions.begin(), currentOptions.end());
            std::sort(wantedOptions.begin(), wantedOptions.end());
            if (currentOptions == wantedOptions)
                continue;

            ReplaceSlotMeshes(*room, *compItr, wanted, roomPos, meshItr->second);
            meshesChanged = true;
        }

        if (meshesChanged)
            roomEntity->ReplaceMeshObjects(meshItr->second);
    }
}

void HouseInteriorMap::UpdateRoomPlacement(Housing::Room const& room)
{
    for (HousingRoomEntity* re : _roomEntities)
    {
        if (!re || !re->IsInWorld() || re->GetGUID() != room.Guid)
            continue;

        // One VALUES update of the room's FMirroredPositionData_C; its meshes and decor follow on the client.
        Position roomPos = GetRoomWorldPosition(room);
        re->Relocate(roomPos);
        re->SetMirroredPosition(roomPos, QuaternionData::fromEulerAnglesZYX(roomPos.GetOrientation(), 0.0f, 0.0f), 1.0f);
        break;
    }
}

void HouseInteriorMap::RespawnRoomVisuals(Housing::Room const& room, int32 factionRestriction, ObjectGuid houseGuid,
    Housing* housing)
{
    if (auto meshItr = _roomMeshObjects.find(room.Guid); meshItr != _roomMeshObjects.end())
    {
        for (ObjectGuid const& meshGuid : meshItr->second)
            if (MeshObject* mesh = GetMeshObject(meshGuid))
            {
                mesh->DestroyForNearbyPlayers();
                RemoveFromMap(mesh, true);
            }
        _roomMeshObjects.erase(meshItr);
    }

    for (auto it = _roomEntities.begin(); it != _roomEntities.end(); ++it)
    {
        if (HousingRoomEntity* entity = *it; entity && entity->IsInWorld() && entity->GetGUID() == room.Guid)
        {
            entity->DestroyForNearbyPlayers();
            RemoveFromMap(entity, true);
            _roomEntities.erase(it);
            break;
        }
    }

    SpawnRoomMeshObjectsFromList({ &room }, factionRestriction, houseGuid);

    // Decor bakes its world transform at spawn just like room meshes; re-arrive it with the room.
    if (housing)
    {
        std::vector<Housing::PlacedDecor> roomDecor;
        for (auto const& [decorGuid, decor] : housing->GetPlacedDecorMap())
            if (decor.RoomGuid == room.Guid)
            {
                if (_decorGuidToObjGuid.contains(decorGuid))
                    DespawnDecorItem(decorGuid);
                roomDecor.push_back(decor);
            }

        SpawnInteriorDecorFromList(roomDecor, houseGuid);
    }
}

bool HouseInteriorMap::IsInsideAnyRoom(Position const& pos, std::vector<Housing::Room const*> const& rooms) const
{
    constexpr float TOLERANCE = 0.5f;
    for (Housing::Room const* room : rooms)
    {
        HouseRoomData const* roomData = sHousingMgr.GetHouseRoomData(room->RoomEntryId);
        RoomWmoDataEntry const* bounds = roomData && roomData->RoomWmoDataID ? sRoomWmoDataStore.LookupEntry(roomData->RoomWmoDataID) : nullptr;
        if (!bounds)
            continue;

        Position const local = HousingWorldToRoomLocal(GetRoomWorldPosition(*room), pos);
        if (local.GetPositionX() >= bounds->BoundingBoxMinX - TOLERANCE && local.GetPositionX() <= bounds->BoundingBoxMaxX + TOLERANCE
            && local.GetPositionY() >= bounds->BoundingBoxMinY - TOLERANCE && local.GetPositionY() <= bounds->BoundingBoxMaxY + TOLERANCE
            && local.GetPositionZ() >= bounds->BoundingBoxMinZ - TOLERANCE && local.GetPositionZ() <= bounds->BoundingBoxMaxZ + TOLERANCE)
            return true;
    }
    return false;
}

void HouseInteriorMap::DespawnAllRoomMeshObjects()
{
    // Immediate removal: deferred removal crashes the client on overlapping CREATE/DESTROY for the same GUIDs.
    for (auto& [roomGuid, meshGuids] : _roomMeshObjects)
    {
        for (ObjectGuid const& meshGuid : meshGuids)
        {
            if (MeshObject* mesh = GetMeshObject(meshGuid))
                RemoveFromMap(mesh, true);
        }
    }

    // HousingRoomEntities must be removed before SpawnRoomMeshObjects recreates them with the same GUIDs.
    for (HousingRoomEntity* roomEntity : _roomEntities)
    {
        if (roomEntity && roomEntity->IsInWorld())
            RemoveFromMap(roomEntity, true);
    }
    _roomEntities.clear();

    // Placed decor must go with the rooms, or stale tracking makes every respawn silently skip all decor.
    for (auto const& [decorGuid, objGuid] : _decorGuidToObjGuid)
    {
        if (objGuid.IsGameObject())
        {
            if (GameObject* go = GetGameObject(objGuid))
                RemoveFromMap(go, true);
        }
        else if (MeshObject* mesh = GetMeshObject(objGuid))
            RemoveFromMap(mesh, true);
    }
    _decorGuidToObjGuid.clear();

    _roomMeshObjects.clear();
    _roomsSpawned = false;
}

void HouseInteriorMap::DespawnRoomEntities(ObjectGuid roomGuid)
{
    // Remove this room's MeshObjects
    auto itr = _roomMeshObjects.find(roomGuid);
    if (itr != _roomMeshObjects.end())
    {
        for (ObjectGuid const& meshGuid : itr->second)
        {
            if (MeshObject* mesh = GetMeshObject(meshGuid))
                RemoveFromMap(mesh, true);
        }
        _roomMeshObjects.erase(itr);
    }

    // Remove this room's HousingRoomEntity
    for (auto it = _roomEntities.begin(); it != _roomEntities.end(); ++it)
    {
        HousingRoomEntity* entity = *it;
        if (entity && entity->IsInWorld() && entity->GetGUID() == roomGuid)
        {
            RemoveFromMap(entity, true);
            _roomEntities.erase(it);
            break;
        }
    }
}

void HouseInteriorMap::UpdateRoomComponentTextures(ObjectGuid roomGuid, Housing::Room const& /*room*/,
    std::vector<uint32> const* componentIDs, int32 textureID)
{
    // Material/texture-only change: update existing MeshObjects in-place (no model change).
    auto itr = _roomMeshObjects.find(roomGuid);
    if (itr == _roomMeshObjects.end())
        return;

    for (ObjectGuid const& meshGuid : itr->second)
    {
        MeshObject* mesh = GetMeshObject(meshGuid);
        if (!mesh)
            continue;
        int32 compID = mesh->GetRoomComponentID();
        if (compID == 0)
            continue;

        bool match = !componentIDs || componentIDs->empty();
        if (!match)
            for (uint32 cid : *componentIDs)
                if (static_cast<int32>(cid) == compID) { match = true; break; }
        if (!match)
            continue;

        mesh->UpdateRoomComponentVisuals(
            mesh->GetRoomComponentOptionID(),
            mesh->GetHouseThemeID(),
            textureID);
    }
}

void HouseInteriorMap::SpawnInteriorDecor(Housing* housing)
{
    if (!housing)
        return;
    std::vector<Housing::PlacedDecor> tmp;
    tmp.reserve(housing->GetPlacedDecorMap().size());
    for (auto const& [_, decor] : housing->GetPlacedDecorMap())
        tmp.push_back(decor);
    SpawnInteriorDecorFromList(tmp, housing->GetHouseGuid());
}

void HouseInteriorMap::SpawnInteriorDecorFromList(std::vector<Housing::PlacedDecor> const& placedDecor, ObjectGuid houseGuid)
{
    uint32 spawnCount = 0;
    uint32 exteriorSkipped = 0;
    uint32 totalDecor = uint32(placedDecor.size());

    TC_LOG_ERROR("housing", "HouseInteriorMap::SpawnInteriorDecor: Starting — totalDecor={} "
        "_roomMeshObjects entries={} owner={}",
        totalDecor, uint32(_roomMeshObjects.size()), _owner.ToString());

    // Log all room mesh object entries for cross-reference
    for (auto const& [roomGuid, meshGuids] : _roomMeshObjects)
    {
        TC_LOG_ERROR("housing", "  _roomMeshObjects[{}] = {} entries (first={})",
            roomGuid.ToString(), uint32(meshGuids.size()),
            meshGuids.empty() ? "EMPTY" : meshGuids[0].ToString());
    }

    // Spawn order: an item snapped onto another decor needs its parent's object to exist first.
    std::vector<Housing::PlacedDecor const*> spawnQueue;
    {
        std::unordered_set<ObjectGuid> batchGuids;
        for (Housing::PlacedDecor const& decor : placedDecor)
            batchGuids.insert(decor.Guid);

        std::unordered_set<ObjectGuid> spawned;
        std::vector<Housing::PlacedDecor const*> pending;
        for (Housing::PlacedDecor const& decor : placedDecor)
            pending.push_back(&decor);

        while (!pending.empty())
        {
            bool progressed = false;
            for (auto pendingItr = pending.begin(); pendingItr != pending.end();)
            {
                Housing::PlacedDecor const& decor = **pendingItr;
                bool const parentReady = decor.ParentDecorGuid.IsEmpty()
                    || !batchGuids.contains(decor.ParentDecorGuid)
                    || spawned.contains(decor.ParentDecorGuid);
                if (!parentReady)
                {
                    ++pendingItr;
                    continue;
                }

                spawnQueue.push_back(&decor);
                spawned.insert(decor.Guid);
                pendingItr = pending.erase(pendingItr);
                progressed = true;
            }
            if (!progressed)
            {
                // Broken chains (missing parent rows): spawn anyway; the anchor falls back to the room.
                for (Housing::PlacedDecor const* decor : pending)
                    spawnQueue.push_back(decor);
                break;
            }
        }
    }

    for (Housing::PlacedDecor const* decorPtr : spawnQueue)
    {
        Housing::PlacedDecor const& decor = *decorPtr;
        HouseDecorData const* decorData = sHousingMgr.GetHouseDecorData(decor.DecorEntryId);
        if (!decorData)
        {
            TC_LOG_ERROR("housing", "HouseInteriorMap::SpawnInteriorDecor: No HouseDecorData for entry {} (decorGuid={})",
                decor.DecorEntryId, decor.Guid.ToString());
            continue;
        }

        // Only spawn decor placed inside a room (interior). Exterior decor has empty RoomGuid.
        if (decor.RoomGuid.IsEmpty())
        {
            ++exteriorSkipped;
            continue;
        }

        // Skip decor already spawned (e.g., placed during this session via SpawnSingleInteriorDecor)
        if (_decorGuidToObjGuid.contains(decor.Guid))
            continue;

        TC_LOG_ERROR("housing", "  SpawnInteriorDecor: decor entry={} roomGuid={} pos=({:.1f},{:.1f},{:.1f})",
            decor.DecorEntryId, decor.RoomGuid.ToString(), decor.PosX, decor.PosY, decor.PosZ);

        ObjectGuid roomEntityGuid = decor.RoomGuid;
        Position roomWorldPos;

        // Resolve the room's world position from the spawned room entity; a miss is skipped rather than defaulting to the origin.
        bool roomResolved = false;
        if (!roomEntityGuid.IsEmpty())
        {
            for (HousingRoomEntity const* re : _roomEntities)
            {
                if (re && re->GetGUID() == roomEntityGuid)
                {
                    roomWorldPos = Position(re->GetPositionX(), re->GetPositionY(), re->GetPositionZ(), re->GetOrientation());
                    roomResolved = true;
                    break;
                }
            }

            if (!roomResolved)
            {
                TC_LOG_ERROR("housing", "HouseInteriorMap::SpawnInteriorDecor: room entity {} not spawned for decor entry {} - skipping",
                    roomEntityGuid.ToString(), decor.DecorEntryId);
                continue;
            }
        }

        // decor.Pos is a WORLD position in interior-map space; PositionLocalSpace is derived by subtracting the room origin.
        float worldX = decor.PosX;
        float worldY = decor.PosY;
        float worldZ = decor.PosZ;
        LoadGrid(worldX, worldY);

        QuaternionData rot(decor.RotationX, decor.RotationY, decor.RotationZ, decor.RotationW);

        Position worldPos(worldX, worldY, worldZ);
        Position localPos = roomEntityGuid.IsEmpty() ? worldPos : HousingWorldToRoomLocal(roomWorldPos, worldPos);
        // Mirrored rotation must be in the room frame (worldRot = roomRot ⊗ localRot on the client).
        QuaternionData localRot = roomEntityGuid.IsEmpty() ? rot : HousingWorldRotationToRoomLocal(roomWorldPos.GetOrientation(), rot);
        float decorScale = decor.Scale > 0.01f ? decor.Scale : 1.0f;
        uint8 attachFlags = roomEntityGuid.IsEmpty() ? uint8(0) : uint8(3);

        // Parenting lives in the position data: an item snapped onto another decor attaches to that
        // decor's own client object with a parent-relative offset, so editor drags carry it along.
        ObjectGuid attachObjectGuid = roomEntityGuid;
        if (!decor.ParentDecorGuid.IsEmpty())
            if (auto parentObjItr = _decorGuidToObjGuid.find(decor.ParentDecorGuid); parentObjItr != _decorGuidToObjGuid.end())
            {
                QuaternionData parentLocalRot;
                Position parentWorldPos;
                bool parentIsGo = false;
                bool parentResolved = false;
                if (MeshObject* parentMesh = GetMeshObject(parentObjItr->second))
                {
                    parentLocalRot = parentMesh->GetLocalRotation();
                    parentWorldPos = parentMesh->GetPosition();
                    parentResolved = true;
                }
                else if (GameObject* parentGo = GetGameObject(parentObjItr->second))
                {
                    parentLocalRot = parentGo->GetLocalRotation();
                    parentWorldPos = parentGo->GetPosition();
                    parentIsGo = true;
                    parentResolved = true;
                }

                if (parentResolved)
                {
                    // GO decor stores its rotation world-frame, mesh decor in its attach frame (the
                    // room); and a mesh/GO Position orientation is always 0, so the anchor's world
                    // yaw must be set explicitly or the child's offset lands rotated away.
                    float parentWorldYaw = 2.0f * std::atan2(parentLocalRot.z, parentLocalRot.w);
                    if (!parentIsGo)
                        parentWorldYaw += roomWorldPos.GetOrientation();
                    parentWorldPos.SetOrientation(parentWorldYaw);
                    attachObjectGuid = parentObjItr->second;
                    localPos = HousingWorldToRoomLocal(parentWorldPos, worldPos);
                    localRot = HousingWorldRotationToRoomLocal(parentWorldYaw, rot);
                    _decorAttachParentObj[decor.Guid] = attachObjectGuid;
                }
            }

        // Functional decor branch (real GameObject — chair, chest, mailbox, etc.)
        bool spawnedAsGo = false;
        if (decorData->GameObjectID > 0)
        {
            uint32 goEntry = static_cast<uint32>(decorData->GameObjectID);
            if (GameObjectTemplate const* goTemplate = sObjectMgr->GetGameObjectTemplate(goEntry))
            {
                float orientation = 2.0f * std::atan2(rot.z, rot.w);
                Position goWorldPos(worldX, worldY, worldZ, orientation);

                GameObject* go = GameObject::CreateGameObject(goEntry, this, goWorldPos, rot,
                    255, GO_STATE_READY, 0);
                if (go)
                {
                    PhasingHandler::InitDbPhaseShift(go->GetPhaseShift(), PHASE_USE_FLAGS_ALWAYS_VISIBLE, 0, 0);
                    go->SetObjectScale(decorScale);
                    // Template default flags — keep CHAIR/CHEST/MAILBOX interactive behavior.
                    go->InitHousingDecorData(decor.Guid, houseGuid, decor.Locked ? 1 : 0,
                        roomEntityGuid, decor.SourceType, decor.SourceValue);
                    go->SetHousingDecorDyeSlots(decor.DyeSlots);
                    RestoreDecorPetBinding(go, decor.Guid, decor.PetGuid, decor.PetFlag);
                    go->InitHousingDecorMirroredPosition(localPos, localRot, decorScale, attachObjectGuid, attachFlags);

                    if (AddToMap(go))
                    {
                        _decorGuidToObjGuid[decor.Guid] = go->GetGUID();
                        ++spawnCount;
                        spawnedAsGo = true;
                    }
                    else
                    {
                        delete go;
                    }
                }
            }
        }

        if (spawnedAsGo)
            continue;

        // Visual-only branch (MeshObject)
        int32 fileDataID = decorData->ModelFileDataID;
        if (fileDataID <= 0 && decorData->GameObjectID > 0)
        {
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
            continue;
        }

        // HouseDecor.ModelType 2 = WMO (interior walls, pillars, doorways); sent as an M2 the client crashes loading it.
        MeshObject* mesh = MeshObject::CreateMeshObject(this, localPos, localRot, decorScale,
            fileDataID, /*isWMO*/ decorData->ModelType == HOUSE_DECOR_MODEL_TYPE_WMO, attachObjectGuid, attachFlags, &worldPos);

        if (!mesh)
        {
            TC_LOG_ERROR("housing", "HouseInteriorMap::SpawnInteriorDecor: Failed to create MeshObject for decor {} (fileDataID={})",
                decor.Guid.ToString(), fileDataID);
            continue;
        }

        PhasingHandler::InitDbPhaseShift(mesh->GetPhaseShift(), PHASE_USE_FLAGS_ALWAYS_VISIBLE, 0, 0);
        mesh->InitHousingDecorData(decor.Guid, houseGuid, decor.Locked ? 1 : 0, roomEntityGuid, decor.SourceType, decor.SourceValue);
        mesh->SetHousingDecorDyeSlots(decor.DyeSlots);
        RestoreDecorPetBinding(mesh, decor.Guid, decor.PetGuid, decor.PetFlag);

        if (AddToMap(mesh))
        {
            _decorGuidToObjGuid[decor.Guid] = mesh->GetGUID();
            ++spawnCount;
        }
        else
        {
            delete mesh;
            TC_LOG_ERROR("housing", "HouseInteriorMap::SpawnInteriorDecor: AddToMap failed for MeshObject decor {}", decor.Guid.ToString());
        }
    }

    TC_LOG_ERROR("housing", "HouseInteriorMap::SpawnInteriorDecor: Spawned {} decor entities for owner {} "
        "(total={}, exteriorSkipped={})",
        spawnCount, _owner.ToString(), totalDecor, exteriorSkipped);
}

void HouseInteriorMap::SpawnSingleInteriorDecor(Housing::PlacedDecor const& decor, ObjectGuid houseGuid)
{
    // Truly exterior decor (no room association) is not spawned on the interior map.
    if (decor.RoomGuid.IsEmpty())
        return;

    if (_decorGuidToObjGuid.contains(decor.Guid))
        return;

    HouseDecorData const* decorData = sHousingMgr.GetHouseDecorData(decor.DecorEntryId);
    if (!decorData)
        return;

    // Decor attaches to the HousingRoomEntity (Housing/2 GUID), not a MeshObject.
    ObjectGuid roomEntityGuid = decor.RoomGuid;
    Position roomWorldPos;

    // Legacy rows without a RoomGuid: auto-assign to the first non-base room, else the first room.
    Housing* ownerHousing = GetOwnerHousing();
    if (roomEntityGuid.IsEmpty() && ownerHousing)
    {
        for (Housing::Room const* rm : ownerHousing->GetRooms())
        {
            HouseRoomData const* rd = sHousingMgr.GetHouseRoomData(rm->RoomEntryId);
            if (rd && !rd->IsBaseRoom())
            {
                roomEntityGuid = rm->Guid;
                break;
            }
        }
        if (roomEntityGuid.IsEmpty())
        {
            for (Housing::Room const* rm : ownerHousing->GetRooms())
            {
                roomEntityGuid = rm->Guid;
                break;
            }
        }
    }

    // Take the room's world position from the spawned room entity (authoritative).
    if (!roomEntityGuid.IsEmpty())
    {
        bool roomResolved = false;
        for (HousingRoomEntity const* re : _roomEntities)
        {
            if (re && re->GetGUID() == roomEntityGuid)
            {
                roomWorldPos = Position(re->GetPositionX(), re->GetPositionY(), re->GetPositionZ(), re->GetOrientation());
                roomResolved = true;
                break;
            }
        }

        if (!roomResolved)
        {
            TC_LOG_ERROR("housing", "HouseInteriorMap::SpawnSingleInteriorDecor: room entity {} not spawned for decor {} - skipping",
                roomEntityGuid.ToString(), decor.Guid.ToString());
            return;
        }
    }

    // decor.Pos is a WORLD position in interior-map space - see SpawnInteriorDecorFromList.
    float worldX = decor.PosX, worldY = decor.PosY, worldZ = decor.PosZ;
    LoadGrid(worldX, worldY);

    QuaternionData rot(decor.RotationX, decor.RotationY, decor.RotationZ, decor.RotationW);

    Position worldPos(worldX, worldY, worldZ);
    Position localPos = roomEntityGuid.IsEmpty() ? worldPos : HousingWorldToRoomLocal(roomWorldPos, worldPos);
    // Mirrored rotation must be in the room frame (worldRot = roomRot ⊗ localRot on the client).
    QuaternionData localRot = roomEntityGuid.IsEmpty() ? rot : HousingWorldRotationToRoomLocal(roomWorldPos.GetOrientation(), rot);
    float decorScale = decor.Scale > 0.01f ? decor.Scale : 1.0f;
    uint8 attachFlags = roomEntityGuid.IsEmpty() ? uint8(0) : uint8(3);

    // Snapped decor anchors to the parent decor's client object (see SpawnInteriorDecorFromList).
    ObjectGuid attachObjectGuid = roomEntityGuid;
    if (!decor.ParentDecorGuid.IsEmpty())
        if (auto parentObjItr = _decorGuidToObjGuid.find(decor.ParentDecorGuid); parentObjItr != _decorGuidToObjGuid.end())
        {
            QuaternionData parentLocalRot;
            Position parentWorldPos;
            bool parentIsGo = false;
            bool parentResolved = false;
            if (MeshObject* parentMesh = GetMeshObject(parentObjItr->second))
            {
                parentLocalRot = parentMesh->GetLocalRotation();
                parentWorldPos = parentMesh->GetPosition();
                parentResolved = true;
            }
            else if (GameObject* parentGo = GetGameObject(parentObjItr->second))
            {
                parentLocalRot = parentGo->GetLocalRotation();
                parentWorldPos = parentGo->GetPosition();
                parentIsGo = true;
                parentResolved = true;
            }

            if (parentResolved)
            {
                // GO decor stores its rotation world-frame, mesh decor in its attach frame (the
                // room); and a mesh/GO Position orientation is always 0, so the anchor's world
                // yaw must be set explicitly or the child's offset lands rotated away.
                float parentWorldYaw = 2.0f * std::atan2(parentLocalRot.z, parentLocalRot.w);
                if (!parentIsGo)
                    parentWorldYaw += roomWorldPos.GetOrientation();
                parentWorldPos.SetOrientation(parentWorldYaw);
                attachObjectGuid = parentObjItr->second;
                localPos = HousingWorldToRoomLocal(parentWorldPos, worldPos);
                localRot = HousingWorldRotationToRoomLocal(parentWorldYaw, rot);
                _decorAttachParentObj[decor.Guid] = attachObjectGuid;
            }
        }

    // Functional decor: spawn a real GameObject so it stays interactive.
    if (decorData->GameObjectID > 0)
    {
        uint32 goEntry = static_cast<uint32>(decorData->GameObjectID);
        if (GameObjectTemplate const* goTemplate = sObjectMgr->GetGameObjectTemplate(goEntry))
        {
            float orientation = 2.0f * std::atan2(rot.z, rot.w);
            Position goWorldPos(worldX, worldY, worldZ, orientation);

            GameObject* go = GameObject::CreateGameObject(goEntry, this, goWorldPos, rot,
                255, GO_STATE_READY, 0);
            if (go)
            {
                PhasingHandler::InitDbPhaseShift(go->GetPhaseShift(), PHASE_USE_FLAGS_ALWAYS_VISIBLE, 0, 0);
                go->SetObjectScale(decorScale);
                // Template default flags, like the login spawn.
                go->InitHousingDecorData(decor.Guid, houseGuid, decor.Locked ? 1 : 0,
                    roomEntityGuid, decor.SourceType, decor.SourceValue);
                go->SetHousingDecorDyeSlots(decor.DyeSlots);
                RestoreDecorPetBinding(go, decor.Guid, decor.PetGuid, decor.PetFlag);
                go->InitHousingDecorMirroredPosition(localPos, localRot, decorScale, attachObjectGuid, attachFlags);

                if (AddToMap(go))
                {
                    _decorGuidToObjGuid[decor.Guid] = go->GetGUID();
                    return;
                }
                delete go;
            }
        }
    }

    // Visual-only (MeshObject) path.
    int32 fileDataID = decorData->ModelFileDataID;
    if (fileDataID <= 0 && decorData->GameObjectID > 0)
    {
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
        return;

    // HouseDecor.ModelType 2 = WMO (interior walls, pillars, doorways); sent as an M2 the client crashes loading it.
    MeshObject* mesh = MeshObject::CreateMeshObject(this, localPos, localRot, decorScale,
        fileDataID, /*isWMO*/ decorData->ModelType == HOUSE_DECOR_MODEL_TYPE_WMO, attachObjectGuid, attachFlags, &worldPos);

    if (!mesh)
        return;

    PhasingHandler::InitDbPhaseShift(mesh->GetPhaseShift(), PHASE_USE_FLAGS_ALWAYS_VISIBLE, 0, 0);
    mesh->InitHousingDecorData(decor.Guid, houseGuid, decor.Locked ? 1 : 0, roomEntityGuid, decor.SourceType, decor.SourceValue);
    mesh->SetHousingDecorDyeSlots(decor.DyeSlots);
    RestoreDecorPetBinding(mesh, decor.Guid, decor.PetGuid, decor.PetFlag);

    if (AddToMap(mesh))
    {
        _decorGuidToObjGuid[decor.Guid] = mesh->GetGUID();
    }
    else
    {
        delete mesh;
    }
}

void HouseInteriorMap::UpdateDecorDyes(ObjectGuid decorGuid, std::array<uint32, MAX_HOUSING_DYE_SLOTS> const& dyeSlots)
{
    auto itr = _decorGuidToObjGuid.find(decorGuid);
    if (itr == _decorGuidToObjGuid.end())
        return;

    if (itr->second.IsGameObject())
    {
        if (GameObject* go = GetGameObject(itr->second))
            go->SetHousingDecorDyeSlots(dyeSlots);
    }
    else if (MeshObject* mesh = GetMeshObject(itr->second))
        mesh->SetHousingDecorDyeSlots(dyeSlots);
}

void HouseInteriorMap::UpdateDecorPet(ObjectGuid decorGuid, ObjectGuid battlePetGuid, uint32 creatureId,
    std::string const& petName, uint8 petBehavior)
{
    auto itr = _decorGuidToObjGuid.find(decorGuid);
    if (itr == _decorGuidToObjGuid.end())
        return;

    if (itr->second.IsGameObject())
    {
        if (GameObject* go = GetGameObject(itr->second))
            ApplyDecorPetBinding(go, decorGuid, battlePetGuid, creatureId, petName, petBehavior);
    }
    else if (MeshObject* mesh = GetMeshObject(itr->second))
        ApplyDecorPetBinding(mesh, decorGuid, battlePetGuid, creatureId, petName, petBehavior);
}

void HouseInteriorMap::ApplyDecorPetBinding(WorldObject* obj, ObjectGuid decorGuid, ObjectGuid battlePetGuid,
    uint32 creatureId, std::string const& petName, uint8 petBehavior)
{
    if (!obj)
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
            Position petPos = obj->GetPosition();
            float const escapeDistance = 1.5f;
            petPos.Relocate(petPos.GetPositionX() + std::cos(petPos.GetOrientation()) * escapeDistance,
                petPos.GetPositionY() + std::sin(petPos.GetOrientation()) * escapeDistance,
                petPos.GetPositionZ());

            if (TempSummon* summon = SummonCreature(creatureId, petPos, nullptr, Milliseconds(0), obj))
            {
                summon->SetReactState(REACT_PASSIVE);
                summon->SetImmuneToAll(true);
                summon->SetControlled(true, UNIT_STATE_ROOT);
                spawnedPetGuid = summon->GetGUID();
                _decorGuidToPetSummon[decorGuid] = spawnedPetGuid;
            }
        }
    }

    obj->SetHousingDecorPet(battlePetGuid, creatureId, petName, petBehavior, spawnedPetGuid);
}

void HouseInteriorMap::RestoreDecorPetBinding(WorldObject* obj, ObjectGuid decorGuid, ObjectGuid petGuid, uint8 petBehavior)
{
    if (!obj || petGuid.IsEmpty())
        return;

    // PetInfo needs CreatureID/name from the owner's battle pet journal (only the journal GUID is persisted).
    if (Player* owner = ObjectAccessor::FindConnectedPlayer(_owner))
        if (BattlePets::BattlePetMgr* petMgr = owner->GetSession()->GetBattlePetMgr())
            if (BattlePets::BattlePet const* pet = petMgr->GetPet(petGuid))
                ApplyDecorPetBinding(obj, decorGuid, petGuid, pet->PacketInfo.CreatureID, pet->PacketInfo.Name, petBehavior);
}

Position HouseInteriorMap::GetEntryPosition() const
{
    // The entry anchors to the placed front-door decor: wherever the owner moved it, entering
    // players appear there. Without it (never placed), the entry hall origin.
    if (Housing* ownerHousing = GetOwnerHousing())
        return ownerHousing->GetInteriorEntryPosition();

    return Position(_originX, _originY, _originZ, 0.0f);
}

Housing* HouseInteriorMap::GetOwnerHousing() const
{
    return const_cast<HouseInteriorMap*>(this)->GetOwnerHousing();
}

void HouseInteriorMap::UpdateDecorAttachment(Housing::PlacedDecor const& decor)
{
    auto objItr = _decorGuidToObjGuid.find(decor.Guid);
    if (objItr == _decorGuidToObjGuid.end())
        return;

    ObjectGuid attachObjectGuid = decor.RoomGuid;
    if (!decor.ParentDecorGuid.IsEmpty())
        if (auto parentItr = _decorGuidToObjGuid.find(decor.ParentDecorGuid); parentItr != _decorGuidToObjGuid.end())
            attachObjectGuid = parentItr->second;

    if (attachObjectGuid == decor.RoomGuid)
        _decorAttachParentObj.erase(decor.Guid);
    else
        _decorAttachParentObj[decor.Guid] = attachObjectGuid;

    if (attachObjectGuid == decor.RoomGuid && attachObjectGuid.IsEmpty())
        return;

    if (GameObject* go = GetGameObject(objItr->second))
        go->SetHousingDecorAttachParent(attachObjectGuid);
    else if (MeshObject* mesh = GetMeshObject(objItr->second))
        mesh->SetAttachParentGUID(attachObjectGuid);
}

void HouseInteriorMap::UpdateDecorPosition(ObjectGuid decorGuid, Position const& pos, QuaternionData const& rot, float scale /*= 1.0f*/)
{
    auto itr = _decorGuidToObjGuid.find(decorGuid);
    if (itr == _decorGuidToObjGuid.end())
        return;

    // The client renders decor from its attach-relative transform (FMirroredPositionData_C): room-relative
    // normally, parent-decor-relative for snapped stacks.
    // GO decor stores its rotation world-frame, mesh decor in its attach frame (its own room —
    // a stacked child's attach GUID names the parent object, so the room is taken from the parent).
    auto anchorFor = [this](ObjectGuid decorGuid, Position& anchorPos, float& anchorLocalYaw,
        bool& anchorIsGo, ObjectGuid& anchorRoomGuid) -> bool
    {
        auto attachItr = _decorAttachParentObj.find(decorGuid);
        if (attachItr == _decorAttachParentObj.end())
            return false;

        QuaternionData localRot;
        if (MeshObject* parentMesh = GetMeshObject(attachItr->second))
        {
            anchorPos = parentMesh->GetPosition();
            localRot = parentMesh->GetLocalRotation();
            anchorRoomGuid = parentMesh->GetAttachParentGUID();
        }
        else if (GameObject* parentGo = GetGameObject(attachItr->second))
        {
            anchorPos = parentGo->GetPosition();
            localRot = parentGo->GetLocalRotation();
            anchorIsGo = true;
        }
        else
            return false;

        anchorLocalYaw = 2.0f * std::atan2(localRot.z, localRot.w);
        return true;
    };

    auto toLocal = [this, &anchorFor, decorGuid](ObjectGuid roomGuid, Position const& worldPos)
    {
        float roomYaw = 0.0f;
        for (HousingRoomEntity const* re : _roomEntities)
            if (re && re->GetGUID() == roomGuid)
                roomYaw = re->GetOrientation();

        Position anchorPos;
        float anchorLocalYaw = 0.0f;
        bool anchorIsGo = false;
        ObjectGuid anchorRoomGuid;
        if (anchorFor(decorGuid, anchorPos, anchorLocalYaw, anchorIsGo, anchorRoomGuid))
        {
            float anchorRoomYaw = 0.0f;
            for (HousingRoomEntity const* re : _roomEntities)
                if (re && re->GetGUID() == anchorRoomGuid)
                    anchorRoomYaw = re->GetOrientation();

            // Same rule as the spawn path: the anchor carries the parent's world yaw.
            anchorPos.SetOrientation(anchorIsGo ? anchorLocalYaw : anchorRoomYaw + anchorLocalYaw);
            return HousingWorldToRoomLocal(anchorPos, worldPos);
        }
        for (HousingRoomEntity const* re : _roomEntities)
            if (re && re->GetGUID() == roomGuid)
                return HousingWorldToRoomLocal(re->GetPosition(), worldPos);
        return worldPos;
    };
    auto toLocalRot = [this, &anchorFor, decorGuid](ObjectGuid roomGuid, QuaternionData const& worldRot)
    {
        // A stack never crosses rooms: the parent's stored rotation is room-frame, so its world yaw
        // is its own yaw plus the child's room orientation.
        float roomYaw = 0.0f;
        for (HousingRoomEntity const* re : _roomEntities)
            if (re && re->GetGUID() == roomGuid)
                roomYaw = re->GetOrientation();

        Position anchorPos;
        float anchorLocalYaw = 0.0f;
        bool anchorIsGo = false;
        ObjectGuid anchorRoomGuid;
        if (anchorFor(decorGuid, anchorPos, anchorLocalYaw, anchorIsGo, anchorRoomGuid))
        {
            float anchorRoomYaw = 0.0f;
            for (HousingRoomEntity const* re : _roomEntities)
                if (re && re->GetGUID() == anchorRoomGuid)
                    anchorRoomYaw = re->GetOrientation();
            return HousingWorldRotationToRoomLocal(anchorIsGo ? anchorLocalYaw : anchorRoomYaw + anchorLocalYaw, worldRot);
        }
        return HousingWorldRotationToRoomLocal(roomYaw, worldRot);
    };

    ObjectGuid objGuid = itr->second;
    if (objGuid.IsGameObject())
    {
        if (GameObject* go = GetGameObject(objGuid))
        {
            go->Relocate(pos);
            go->SetLocalRotation(rot.x, rot.y, rot.z, rot.w);
            if (std::abs(go->GetObjectScale() - scale) > 0.001f)
                go->SetObjectScale(scale);
            go->UpdateHousingDecorMirroredTransform(toLocal(go->GetHousingDecorAttachParent(), pos),
                toLocalRot(go->GetHousingDecorAttachParent(), rot), scale);
        }
    }
    else if (MeshObject* mesh = GetMeshObject(objGuid))
    {
        mesh->Relocate(pos);
        mesh->UpdateLocalTransform(toLocal(mesh->GetAttachParentGUID(), pos), toLocalRot(mesh->GetAttachParentGUID(), rot), scale);
    }
}

void HouseInteriorMap::DespawnDecorItem(ObjectGuid decorGuid)
{
    // Remove the bound companion creature first, then the decor itself.
    auto petItr = _decorGuidToPetSummon.find(decorGuid);
    if (petItr != _decorGuidToPetSummon.end())
    {
        if (Creature* pet = GetCreature(petItr->second))
            pet->DespawnOrUnsummon();
        _decorGuidToPetSummon.erase(petItr);
    }

    auto itr = _decorGuidToObjGuid.find(decorGuid);
    if (itr == _decorGuidToObjGuid.end())
    {
        return;
    }

    ObjectGuid objGuid = itr->second;
    if (objGuid.IsGameObject())
    {
        if (GameObject* go = GetGameObject(objGuid))
            go->AddObjectToRemoveList();
    }
    else if (MeshObject* mesh = GetMeshObject(objGuid))
        mesh->AddObjectToRemoveList();

    _decorGuidToObjGuid.erase(itr);
    _decorAttachParentObj.erase(decorGuid);
}

bool HouseInteriorMap::AddPlayerToMap(Player* player, bool initPlayer /*= true*/)
{
    TC_LOG_ERROR("housing", "HouseInteriorMap::AddPlayerToMap: ENTER player={} owner={} isOwner={} "
        "_roomsSpawned={} map={} instanceId={} this={}",
        player->GetGUID().ToString(), _owner.ToString(),
        IsHouseOwnerAccount(player), _roomsSpawned,
        GetId(), GetInstanceId(), (void*)this);

    if (IsHouseOwnerAccount(player))
        _loadingPlayer = player;

    // === PRE-SPAWN: populate all housing entities BEFORE Map::AddPlayerToMap so the initial
    // UPDATE_OBJECT carries full housing context (visits to offline owners spawn from PlotInfo).
    Housing* preloadHousing = IsHouseOwnerAccount(player) ? player->GetHousingByOwner(_owner) : player->GetHousing();
    if (!preloadHousing && player->GetGUID() == _owner)
        preloadHousing = player->GetHousing();
    bool visitingOfflineOwner = !_roomsSpawned && !IsHouseOwnerAccount(player);
    if (visitingOfflineOwner)
    {
        for (Neighborhood* nbh : sNeighborhoodMgr.GetNeighborhoodsForPlayer(_owner))
        {
            Neighborhood::PlotInfo const* ownerPlot = nullptr;
            for (Neighborhood::PlotInfo const& plot : nbh->GetPlots())
            {
                if (plot.OwnerGuid == _owner && plot.IsOccupied())
                {
                    ownerPlot = &plot;
                    break;
                }
            }
            if (!ownerPlot)
                continue;

            int32 faction = nbh->GetFactionRestriction();
            std::vector<Housing::Room const*> roomPtrs;
            roomPtrs.reserve(ownerPlot->Rooms.size());
            for (Housing::Room const& room : ownerPlot->Rooms)
                roomPtrs.push_back(&room);
            SpawnRoomMeshObjectsFromList(roomPtrs, faction, ownerPlot->HouseGuid);
            SpawnInteriorDecorFromList(ownerPlot->Decor, ownerPlot->HouseGuid);
            _roomsSpawned = true;

            break;
        }
    }

    bool const ownerPreSpawn = preloadHousing && IsHouseOwnerAccount(player);
    if (ownerPreSpawn)
    {
        // Rebuild stale rooms from an earlier visit, but only while nobody is standing in them
        // (a later rebuild would destroy entities the initial UPDATE_OBJECT just delivered).
        if (_roomsSpawned && !HavePlayers())
            DespawnAllRoomMeshObjects();

        // Clear exterior fixture edit mode that persists across map transfer
        if (preloadHousing->GetEditorMode() != HOUSING_EDITOR_MODE_NONE)
        {
            preloadHousing->SetEditorMode(HOUSING_EDITOR_MODE_NONE);
            player->RemoveUnitFlag(UNIT_FLAG_PACIFIED);
            player->RemoveUnitFlag2(UNIT_FLAG2_NO_ACTIONS);
            player->ReplaceAllSilencedSchoolMask(SpellSchoolMask(0));
        }

        preloadHousing->SetInInterior(true);

        // Spawn rooms + decor onto the map (before player enters)
        if (!_roomsSpawned)
        {
            int32 faction = preloadHousing->GetNeighborhoodFaction();
            SpawnRoomMeshObjects(preloadHousing, faction);
            SpawnInteriorDecor(preloadHousing);
            _roomsSpawned = true;
        }

        // Populate the Account entity so the initial UPDATE_OBJECT includes full housing context.
        preloadHousing->PopulateCatalogStorageEntries();
        preloadHousing->SyncUpdateFields();

        // The player stays where the transfer put them: the entry hall at the interior origin.

        TC_LOG_ERROR("housing", "HouseInteriorMap::AddPlayerToMap: PRE-SPAWNED rooms+decor+storage "
            "(%u rooms, %u decor) before Map::AddPlayerToMap",
            uint32(_roomMeshObjects.size()), uint32(_decorGuidToObjGuid.size()));
    }

    bool result = Map::AddPlayerToMap(player, initPlayer);

    if (IsHouseOwnerAccount(player))
        _loadingPlayer = nullptr;

    TC_LOG_ERROR("housing", "HouseInteriorMap::AddPlayerToMap: Map::AddPlayerToMap returned {} for player={}",
        result, player->GetGUID().ToString());

    if (result)
    {
        Housing* housing = player->GetHousing();
        TC_LOG_ERROR("housing", "HouseInteriorMap::AddPlayerToMap: housing={} for player={}",
            housing ? "VALID" : "NULL", player->GetGUID().ToString());

        if (housing)
        {
            TC_LOG_ERROR("housing", "HouseInteriorMap::AddPlayerToMap: houseGuid={} rooms={} decor={} "
                "houseGuid.IsEmpty={}",
                housing->GetHouseGuid().ToString(),
                uint32(housing->GetRooms().size()),
                uint32(housing->GetPlacedDecorMap().size()),
                housing->GetHouseGuid().IsEmpty());

            housing->SetInInterior(true);

            // Spawn room meshes on first entry. The owner's rooms were already (re)built in the
            // pre-spawn above and delivered with the initial UPDATE_OBJECT.
            if (IsHouseOwnerAccount(player) && !ownerPreSpawn)
            {
                // Force a fresh spawn on login (stale fragment formats); safe only while the arriving
                // owner has received no entities yet — leave the rooms standing when others are present.
                bool spawnRooms = true;
                if (_roomsSpawned)
                {
                    bool otherPlayersPresent = std::any_of(GetPlayers().begin(), GetPlayers().end(),
                        [player](MapReference const& ref) { return ref.GetSource() != player; });

                    if (otherPlayersPresent)
                        spawnRooms = false;
                    else
                        DespawnAllRoomMeshObjects();
                }

                if (spawnRooms)
                {
                    TC_LOG_ERROR("housing", "HouseInteriorMap::AddPlayerToMap: === SPAWNING ROOMS ===");

                    for (Housing::Room const* room : housing->GetRooms())
                    {
                        TC_LOG_ERROR("housing", "  Room: guid={} entryId={} slot={} grid=({},{}) orientation={} mirrored={}",
                            room->Guid.ToString(), room->RoomEntryId, room->SlotIndex,
                            room->GridX, room->GridY, room->Orientation, room->Mirrored);
                    }

                    int32 faction = housing->GetNeighborhoodFaction();
                    SpawnRoomMeshObjects(housing, faction);
                    _roomsSpawned = true;
                }
            }

            // Always spawn interior decor (handles both first entry and re-entry).
            // SpawnInteriorDecor skips already-spawned decor via _decorGuidToObjGuid check.
            SpawnInteriorDecor(housing);

            TC_LOG_ERROR("housing", "HouseInteriorMap::AddPlayerToMap: === SPAWN COMPLETE === "
                "roomMeshObjects entries={} decorGuidToObj entries={}",
                uint32(_roomMeshObjects.size()), uint32(_decorGuidToObjGuid.size()));

            // Defer housing context packets by 500ms so the client can process the initial UPDATE_OBJECT first
            // (mirrors the exterior map's deferred ENTER_PLOT pattern).
            {
                ObjectGuid playerGuid = player->GetGUID();
                ObjectGuid houseGuid = housing->GetHouseGuid();
                ObjectGuid neighborhoodGuid = housing->GetNeighborhoodGuid();
                ObjectGuid accountGuid = player->GetSession()->GetBattlenetAccountGUID();
                uint8 plotIndex = housing->GetPlotIndex();
                uint32 settingsFlags = housing->GetSettingsFlags();

                player->m_Events.AddEventAtOffset([this, playerGuid, houseGuid, neighborhoodGuid, accountGuid, plotIndex, settingsFlags]()
                {
                    Player* p = ObjectAccessor::FindPlayer(playerGuid);
                    if (!p || !p->IsInWorld())
                        return;

                    // The entering player's OWN housing - null for a houseless visitor, who still needs the context below.
                    Housing* housing = p->GetHousing();
                    bool const ownerEntering = IsHouseOwnerAccount(p);

                    // Drive the tutorial forward for the OWNER only (the credit completes their quest).
                    if (ownerEntering)
                        GrantHousingTutorialProgress(p);

                    // PostTutorialAuras (slots 8, 9, 50) + account storage: bound to the entering player's own housing.
                    if (housing)
                    {
                        SendHousingPostTutorialAuras(p);

                        {
                            housing->PopulateCatalogStorageEntries();
                            housing->SyncUpdateFields();

                            WorldSession* session = p->GetSession();
                            UpdateData storageUpdate(p->GetMapId());
                            WorldPacket storagePacket;

                            // Both entities as CREATE, like retail (plot enter + storage request).
                            session->BuildHousingAccountEntitiesUpdate(&storageUpdate, p, /*accountAsCreate=*/true);

                            // Decor and room CREATEs come from the map visibility system; double-sending corrupts the client's entity state.

                            storageUpdate.BuildPacket(&storagePacket);
                            p->SendDirectMessage(&storagePacket);

                            session->GetBattlenetAccount().ClearUpdateMask(true);
                            session->GetHousingPlayerHouseEntity().ClearUpdateMask(true);
                        }
                    }

                    {
                        WorldPackets::Housing::InitiativeServiceStatus initStatus;
                        initStatus.ServiceEnabled = true;
                        p->SendDirectMessage(initStatus.Write());
                    }

                    // No plot AreaTrigger, plot-enter auras or CurrentHouse inside the house.

                    // Starter door: houses from before the door grant get one placed into the entry
                    // hall once. It is normal placed decor from here on — the owner can move or
                    // withdraw it freely, and the interior entry point (GetEntryPosition) anchors
                    // to its live position.
                    {
                        Housing* ownerHousing = p->GetHousingByOwner(_owner);
                        if (!ownerHousing)
                            if (Player* ownerPlayer = ObjectAccessor::FindPlayer(_owner))
                                ownerHousing = ownerPlayer->GetHousingByOwner(_owner);

                        if (!ownerHousing)
                            TC_LOG_WARN("housing", "StarterDoor: owner housing unavailable (owner offline?) for {}",
                                playerGuid.ToString());
                        else if (Housing::PlacedDecor const* placedDoor = ownerHousing->EnsureStarterDoorPlaced())
                        {
                            // Spawn the just-placed door for this session (decor rows spawn on next load).
                            SpawnSingleInteriorDecor(*placedDoor, ownerHousing->GetHouseGuid());
                            TC_LOG_INFO("housing", "StarterDoor: placed starter door decor {} into entry hall for {}",
                                placedDoor->DecorEntryId, playerGuid.ToString());
                        }
                    }

                    TC_LOG_ERROR("housing", "HouseInteriorMap deferred: Complete — "
                        "HouseInfo+Status+Perms+Auras+Account+Initiative+PlotAT+ENTER_PLOT+Door for {}",
                        playerGuid.ToString());
                }, Milliseconds(500));
            }
        }
        else
        {
            TC_LOG_ERROR("housing", "HouseInteriorMap::AddPlayerToMap: NO HOUSING for player {} — "
                "cannot spawn rooms/decor", player->GetGUID().ToString());
        }

        // Toggle WS[30906]=1 for EVERYONE entering (a houseless visitor needs it too), synchronously.
        player->SendUpdateWorldState(WORLDSTATE_HOUSING_INTERIOR, 1);

        TC_LOG_ERROR("housing", "HouseInteriorMap: Player {} entered house interior (owner={}, map={}, instanceId={})",
            player->GetGUID().ToString(), _owner.ToString(), GetId(), GetInstanceId());
    }
    else
    {
        TC_LOG_ERROR("housing", "HouseInteriorMap::AddPlayerToMap: FAILED for player={}",
            player->GetGUID().ToString());
    }

    return result;
}

void HouseInteriorMap::RemovePlayerFromMap(Player* player, bool remove)
{
    Housing* housing = player->GetHousing();
    if (housing)
    {
        housing->SetInInterior(false);

        // Leaving must not carry an editor out of the house (stun/no-gravity aura would stick to the player).
        if (IsHouseOwnerAccount(player) && housing->GetEditorMode() != HOUSING_EDITOR_MODE_NONE)
        {
            housing->SetEditorMode(HOUSING_EDITOR_MODE_NONE);
            player->RemoveUnitFlag(UNIT_FLAG_PACIFIED);
            player->RemoveUnitFlag2(UNIT_FLAG2_NO_ACTIONS);
            player->ReplaceAllSilencedSchoolMask(SpellSchoolMask(0));
        }
    }

    // Toggle WS[30906]=0 to signal the client that the player left the house interior.
    player->SendUpdateWorldState(WORLDSTATE_HOUSING_INTERIOR, 0);

    TC_LOG_ERROR("housing", "HouseInteriorMap::RemovePlayerFromMap: Player {} leaving interior "
        "(owner={}, map={}, instanceId={}, _roomsSpawned={}, roomMeshEntries={}, decorEntries={}, this={})",
        player->GetGUID().ToString(), _owner.ToString(), GetId(), GetInstanceId(),
        _roomsSpawned, uint32(_roomMeshObjects.size()), uint32(_decorGuidToObjGuid.size()), (void*)this);

    Map::RemovePlayerFromMap(player, remove);
}

void HouseInteriorMap::GrantHousingTutorialProgress(Player* player)
{
    if (!player)
        return;

    if (player->GetQuestRewardStatus(QUEST_HOUSING_TUTORIAL_COMPLETE))
        return; // tutorial already finished - editor is unlocked

    Quest const* quest = sObjectMgr->GetQuestTemplate(QUEST_HOUSING_TUTORIAL_COMPLETE);
    if (!quest)
    {
        TC_LOG_ERROR("housing", "GrantHousingTutorialProgress: quest {} missing from quest_template - the housing "
            "editor can never unlock", QUEST_HOUSING_TUTORIAL_COMPLETE);
        return;
    }

    // Retail grants this by script: AUTO_ACCEPT, no NPC offers it, so put it in the log.
    if (player->GetQuestStatus(QUEST_HOUSING_TUTORIAL_COMPLETE) == QUEST_STATUS_NONE)
    {
        if (!player->CanTakeQuest(quest, false) || !player->CanAddQuest(quest, false))
        {
            TC_LOG_ERROR("housing", "GrantHousingTutorialProgress: player {} cannot take quest {} (log full or "
                "requirements unmet) - housing editor stays locked",
                player->GetGUID().ToString(), QUEST_HOUSING_TUTORIAL_COMPLETE);
            return;
        }

        player->AddQuestAndCheckCompletion(quest, nullptr);
    }

    // Completing objective: credited the moment the player stands in the house.
    player->KilledMonsterCredit(NPC_HOUSING_TUTORIAL_HOUSE_ENTERED_CREDIT);

    // AUTO_COMPLETE quests are submitted by the client with the PLAYER as quest giver; close the loop here.
    if (player->GetQuestStatus(QUEST_HOUSING_TUTORIAL_COMPLETE) == QUEST_STATUS_COMPLETE
        && quest->HasFlag(QUEST_FLAGS_AUTO_COMPLETE))
    {
        player->RewardQuest(quest, LootItemType::Item, 0, player, false);
    }
}
