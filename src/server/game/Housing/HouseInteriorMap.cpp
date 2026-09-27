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
#include "AreaTrigger.h"
#include "HousingPlayerHouseEntity.h"
#include "DB2Stores.h"
#include "DBCEnums.h"
#include "GameObjectData.h"
#include "Housing.h"
#include "HousingDefines.h"
#include "HousingRoomEntity.h"
#include "HousingDecorEntity.h"
#include "HousingMgr.h"
#include "Neighborhood.h"
#include "NeighborhoodMgr.h"
#include "HousingPackets.h"
#include "Log.h"
#include "MeshObject.h"
#include "ObjectAccessor.h"
#include "ObjectGridLoader.h"
#include "ObjectMgr.h"
#include "PhasingHandler.h"
#include "Player.h"
#include "Spell.h"
#include "SpellAuraDefines.h"
#include "SpellPackets.h"
#include "RealmList.h"
#include "World.h"
#include "WorldSession.h"
#include <algorithm>

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
        TC_LOG_INFO("housing", "HouseInteriorMap::CTOR: Using DB2 origin ({:.1f}, {:.1f}, {:.1f}) from "
            "NeighborhoodMap {} for map {}",
            _originX, _originY, _originZ, nmData->ID, id);
    }
    else
    {
        TC_LOG_INFO("housing", "HouseInteriorMap::CTOR: No NeighborhoodMap DB2 entry for map {} — "
            "using default origin ({:.1f}, {:.1f}, {:.1f})",
            id, _originX, _originY, _originZ);
    }

    TC_LOG_ERROR("housing", "HouseInteriorMap::CTOR: Created interior map {} instanceId {} for owner {} "
        "(this={}, _roomsSpawned={}, InstanceType={}, Instanceable={}, IsNeighborhood={})",
        id, instanceId, owner.ToString(), (void*)this, _roomsSpawned,
        GetEntry()->InstanceType, GetEntry()->Instanceable(), GetEntry()->IsNeighborhood());
}

void HouseInteriorMap::InitVisibilityDistance()
{
    // House interiors are small single-room spaces. Use maximum visibility so
    // all room entities, decor, and furniture are CREATEd immediately on entry.
    m_VisibleDistance = MAX_VISIBILITY_DISTANCE;
    m_VisibilityNotifyPeriod = sWorld->getIntConfig(CONFIG_VISIBILITY_NOTIFY_PERIOD_INSTANCE);
}

void HouseInteriorMap::LoadGridObjects(NGridType* grid)
{
    Map::LoadGridObjects(grid);

    // Room WMO geometry is spawned when the owner enters via AddPlayerToMap.
    // No static spawns exist on the interior map template.
}

Housing* HouseInteriorMap::GetOwnerHousing()
{
    if (_loadingPlayer)
        return _loadingPlayer->GetHousing();

    if (Player* owner = ObjectAccessor::FindConnectedPlayer(_owner))
        return owner->GetHousing();

    return nullptr;
}

void HouseInteriorMap::SpawnRoomMeshObjects(Housing* housing, int32 factionRestriction)
{
    if (!housing)
        return;
    SpawnRoomMeshObjectsFromList(housing->GetRooms(), factionRestriction, housing->GetHouseGuid());
}

Position HouseInteriorMap::GetRoomWorldPosition(Housing::Room const& room) const
{
    // GridX/GridY = yard offsets. FloorIndex = floor NUMBER (0=ground, 1=floor2, …).
    // Sniff-verified: retail HousingRoomEntity.FloorIndex is a small integer
    // used by the client's floor selector UI, while world Z is computed as
    // FloorIndex × 12 yards (confirmed by positions at Z=0.1, 12.1, 24.1).
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
    // RoomComponentOption rows only exist for base themes (1-5). The stored
    // per-surface themes are usually sub-themes (e.g. 11=Bel'ameth Folk,
    // 20=Folk Light) — resolve them to the parent base theme or the lookup
    // falls through to the faction default and the user's style is lost.
    int32 lookupTheme = sHousingMgr.GetBaseThemeID(rawTheme);
    if (lookupTheme <= 0)
        lookupTheme = rawTheme;
    std::vector<RoomComponentOptionEntry const*> allOptions = sHousingMgr.FindAllRoomComponentOptions(comp.MeshStyleFilterID, lookupTheme);
    if (allOptions.empty())
        allOptions = sHousingMgr.FindAllRoomComponentOptions(comp.MeshStyleFilterID, factionThemeID);
    if (allOptions.empty() && factionThemeID != 2)
        allOptions = sHousingMgr.FindAllRoomComponentOptions(comp.MeshStyleFilterID, 2);
    if (allOptions.empty() && factionThemeID != 1)
        allOptions = sHousingMgr.FindAllRoomComponentOptions(comp.MeshStyleFilterID, 1);

    std::sort(allOptions.begin(), allOptions.end(), [](RoomComponentOptionEntry const* a, RoomComponentOptionEntry const* b) { return a->ID < b->ID; });

    // One slot, one look (retail 12.1.0.69933). RoomComponentOption.RoomComponentID is the variant of a slot
    // (doorway style, ceiling shape, stair model):
    //   - a connected door: the side owning the doorway gets the DoorwayWall + Doorway pieces of the connection's
    //     variant, the other side only the DoorwayWall of that variant - often nothing, the doorway fills the gap;
    //   - anything else: a single Cosmetic piece of the chosen variant.
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

    // Every Cosmetic piece of that variant that has its own model: a small square room's corners are 325 + 562 in
    // retail, while a stair floor's model-less 729 is left out next to 437. Fall back to variant 0; failing that a
    // single piece (a model-less one takes the component's own model, see CreateRoomComponentMesh).
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
    // DB2 OffsetRot is in DEGREES — convert to radians. Z is negated (sniff-verified).
    static constexpr float DEG_TO_RAD = static_cast<float>(M_PI / 180.0);
    float rx = comp.OffsetRot[0] * DEG_TO_RAD;
    float ry = comp.OffsetRot[1] * DEG_TO_RAD;
    float rz = -comp.OffsetRot[2] * DEG_TO_RAD; // negated (sniff-verified)
    float cx = std::cos(rx / 2.0f), sx = std::sin(rx / 2.0f);
    float cy = std::cos(ry / 2.0f), sy = std::sin(ry / 2.0f);
    float cz = std::cos(rz / 2.0f), sz = std::sin(rz / 2.0f);
    compRot.x = sx * cy * cz - cx * sy * sz;
    compRot.y = cx * sy * cz + sx * cy * sz;
    compRot.z = cx * cy * sz - sx * sy * cz;
    compRot.w = cx * cy * cz + sx * sy * sz;

    // RoomWmoData → Geobox bounds (bounding box for OutsidePlotBounds check)
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

    // Upper floors first, as retail sends a new stairwell (12.1.0.69933 sniff: the upper half's CREATE precedes the
    // lower half's). Every room reaches the client in its own packet here, and the client prices the stairwell once
    // only when the lower half, linking up through its ceiling, finds the upper one already known - built bottom-up
    // the room budget showed it twice until the editor was reopened.
    std::vector<Housing::Room const*> spawnOrder(rooms.begin(), rooms.end());
    std::stable_sort(spawnOrder.begin(), spawnOrder.end(), [](Housing::Room const* a, Housing::Room const* b)
    {
        return a->FloorIndex != b->FloorIndex ? a->FloorIndex > b->FloorIndex : a->SlotIndex < b->SlotIndex;
    });

    for (Housing::Room const* room : spawnOrder)
    {
        // Skip rooms that already have entities on the map (incremental spawn for room add).
        if (_roomMeshObjects.count(room->Guid) > 0)
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
        // It must reach the client BEFORE its component MeshObjects, which use AttachParentGUID = room guid (the
        // client resolves it on create and crashes on a missing parent, NULL+0x20). Its CREATE goes out as soon as it
        // is added to the map, so the mesh list and the doors are filled in first: retail's CREATE already carries
        // both, and the client prices a stairwell from the door links it sees on create - with an empty door list
        // both halves were charged until the editor was reopened.
        HousingRoomEntity* housingRoom = new HousingRoomEntity();
        PhasingHandler::InitDbPhaseShift(housingRoom->GetPhaseShift(), PHASE_USE_FLAGS_ALWAYS_VISIBLE, 0, 0);
        housingRoom->SetHouseGUID(houseGuid);
        housingRoom->SetHouseRoomID(room->RoomEntryId);
        // Sniff: entry hall 1, stairwell halves 2 (HouseRoom flags BASE_ROOM / HAS_STAIRS)
        housingRoom->SetFlags(roomData->Flags & (HOUSING_ROOM_FLAG_BASE_ROOM | HOUSING_ROOM_FLAG_HAS_STAIRS));
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
        // One entry per horizontal connectable wall, connected or not: the client renders a
        // fixture handle on doors with an empty AttachedRoomGUID and hides it on connected ones.
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
    // Both stairwell halves are the same HouseRoom stacked at one XY (12.1.0.69933 sniff): the lower half has no
    // ceiling, the upper one no floor and no stairs, so the stairs reach the landing through an open shaft.
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

    TC_LOG_DEBUG("housing", "HouseInteriorMap::RebuildRoomComponents: room {} rebuilt {} of {} requested slots",
        room.Guid.ToString(), rebuilt, uint32(componentIds.size()));
}

void HouseInteriorMap::RefreshRoomDoors(std::vector<Housing::Room const*> const& rooms, int32 factionRestriction)
{
    // Called after the layout changed (room added, removed, turned, door style picked): rebuild only the door
    // slots whose look changed and re-point the door list, the way retail answers these edits.
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

        // Retail turns a room in place: one VALUES update of its FMirroredPositionData_C. Its meshes and decor
        // hang off it and follow on the client.
        Position roomPos = GetRoomWorldPosition(room);
        re->Relocate(roomPos);
        re->SetMirroredPosition(roomPos, QuaternionData::fromEulerAnglesZYX(roomPos.GetOrientation(), 0.0f, 0.0f), 1.0f);
        break;
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
    uint32 despawnCount = 0;

    // Use immediate removal (RemoveFromMap) instead of deferred (AddObjectToRemoveList).
    // Deferred removal causes client crashes when new entities are created in the same
    // update cycle — the client sees overlapping CREATE/DESTROY for the same GUIDs.
    for (auto& [roomGuid, meshGuids] : _roomMeshObjects)
    {
        for (ObjectGuid const& meshGuid : meshGuids)
        {
            if (MeshObject* mesh = GetMeshObject(meshGuid))
            {
                RemoveFromMap(mesh, true);
                ++despawnCount;
            }
        }
    }

    // Also despawn HousingRoomEntities — they must be removed before
    // SpawnRoomMeshObjects recreates them with the same GUIDs.
    for (HousingRoomEntity* roomEntity : _roomEntities)
    {
        if (roomEntity && roomEntity->IsInWorld())
        {
            RemoveFromMap(roomEntity, true);
            ++despawnCount;
        }
    }
    _roomEntities.clear();

    // Placed decor must go with the rooms. Its visuals hang off the room entities we just
    // destroyed, and _decorGuidToObjGuid is what SpawnInteriorDecor consults to decide a decor
    // item is "already spawned". Leaving it populated made every subsequent respawn skip all
    // decor silently ("Spawned 0 decor entities (total=3, exteriorSkipped=0)"), so a house whose
    // rooms were respawned came back completely empty.
    uint32 decorDespawned = 0;
    for (auto const& [decorGuid, objGuid] : _decorGuidToObjGuid)
    {
        if (objGuid.IsGameObject())
        {
            if (GameObject* go = GetGameObject(objGuid))
            {
                RemoveFromMap(go, true);
                ++decorDespawned;
            }
        }
        else if (MeshObject* mesh = GetMeshObject(objGuid))
        {
            RemoveFromMap(mesh, true);
            ++decorDespawned;
        }
    }
    _decorGuidToObjGuid.clear();
    despawnCount += decorDespawned;

    _roomMeshObjects.clear();
    _roomsSpawned = false;

    TC_LOG_DEBUG("housing", "HouseInteriorMap::DespawnAllRoomMeshObjects: Despawned {} objects incl. {} decor (owner={})",
        despawnCount, decorDespawned, _owner.ToString());
}

void HouseInteriorMap::DespawnRoomEntities(ObjectGuid roomGuid)
{
    uint32 despawnCount = 0;

    // Remove this room's MeshObjects
    auto itr = _roomMeshObjects.find(roomGuid);
    if (itr != _roomMeshObjects.end())
    {
        for (ObjectGuid const& meshGuid : itr->second)
        {
            if (MeshObject* mesh = GetMeshObject(meshGuid))
            {
                RemoveFromMap(mesh, true);
                ++despawnCount;
            }
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
            ++despawnCount;
            break;
        }
    }

    TC_LOG_DEBUG("housing", "HouseInteriorMap::DespawnRoomEntities: Removed {} entities for room {} (owner={})",
        despawnCount, roomGuid.ToString(), _owner.ToString());
}

void HouseInteriorMap::UpdateRoomComponentTextures(ObjectGuid roomGuid, Housing::Room const& /*room*/,
    std::vector<uint32> const* componentIDs, int32 textureID)
{
    // Material/texture-only change: update existing MeshObjects in-place via UPDATE_OBJECT.
    // No model change needed — only the RoomComponentTextureID field changes.
    auto itr = _roomMeshObjects.find(roomGuid);
    if (itr == _roomMeshObjects.end())
        return;

    uint32 matchCount = 0;
    for (ObjectGuid const& meshGuid : itr->second)
    {
        MeshObject* mesh = GetMeshObject(meshGuid);
        if (!mesh) continue;
        int32 compID = mesh->GetRoomComponentID();
        if (compID == 0) continue;

        bool match = !componentIDs || componentIDs->empty();
        if (!match)
            for (uint32 cid : *componentIDs)
                if (static_cast<int32>(cid) == compID) { match = true; break; }
        if (!match) continue;

        mesh->UpdateRoomComponentVisuals(
            mesh->GetRoomComponentOptionID(),
            mesh->GetHouseThemeID(),
            textureID);
        ++matchCount;
    }
    TC_LOG_DEBUG("housing", "UpdateRoomComponentTextures: room={} textureID={} matched={}", roomGuid.ToString(), textureID, matchCount);
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

    for (Housing::PlacedDecor const& decor : placedDecor)
    {
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
        if (_decorGuidToObjGuid.count(decor.Guid))
            continue;

        TC_LOG_ERROR("housing", "  SpawnInteriorDecor: decor entry={} roomGuid={} pos=({:.1f},{:.1f},{:.1f})",
            decor.DecorEntryId, decor.RoomGuid.ToString(), decor.PosX, decor.PosY, decor.PosZ);

        ObjectGuid roomEntityGuid = decor.RoomGuid;
        Position roomWorldPos;

        // Resolve the room's world position from the room entity we just spawned. The old
        // lookup went through GetOwnerHousing()->GetRooms(), which returns nothing on some
        // passes (owner not loaded), and a miss silently left roomWorldPos at (0,0,0) - which
        // is why the same decor item computed a different position on consecutive respawns.
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

        // decor.Pos is a WORLD position in interior-map space - it is stored verbatim from the
        // client's placement packet, and the client sends world coords (rows placed live read
        // e.g. -980.4/-1005.2/0.07 for a room sitting at -985/-1000/0.1). PositionLocalSpace is
        // therefore derived by subtracting the room origin, NOT the other way round.
        float worldX = decor.PosX;
        float worldY = decor.PosY;
        float worldZ = decor.PosZ;
        LoadGrid(worldX, worldY);

        QuaternionData rot(decor.RotationX, decor.RotationY, decor.RotationZ, decor.RotationW);

        Position worldPos(worldX, worldY, worldZ);
        Position localPos = roomEntityGuid.IsEmpty() ? worldPos : HousingWorldToRoomLocal(roomWorldPos, worldPos);
        float decorScale = decor.Scale > 0.01f ? decor.Scale : 1.0f;
        uint8 attachFlags = roomEntityGuid.IsEmpty() ? uint8(0) : uint8(3);

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
                    go->InitHousingDecorMirroredPosition(localPos, rot, decorScale, roomEntityGuid, attachFlags);

                    if (AddToMap(go))
                    {
                        _decorGuidToObjGuid[decor.Guid] = go->GetGUID();
                        ++spawnCount;
                        spawnedAsGo = true;
                        TC_LOG_INFO("housing", "HouseInteriorMap::SpawnInteriorDecor: Spawned functional-decor "
                            "GameObject entry={} goEntry={} goType={} goGuid={} at world({:.1f},{:.1f},{:.1f}) room={}",
                            decor.DecorEntryId, goEntry, uint32(goTemplate->type), go->GetGUID().ToString(),
                            worldX, worldY, worldZ, roomEntityGuid.ToString());
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
            TC_LOG_DEBUG("housing", "HouseInteriorMap::SpawnInteriorDecor: Cannot derive FileDataID for decor entry {} "
                "(GameObjectID={}, ModelFileDataID={}), skipping",
                decor.DecorEntryId, decorData->GameObjectID, decorData->ModelFileDataID);
            continue;
        }

        // HouseDecor.ModelType 2 = WMO (interior walls, pillars, doorways); sent as an M2 the client crashes loading it.
        MeshObject* mesh = MeshObject::CreateMeshObject(this, localPos, rot, decorScale,
            fileDataID, /*isWMO*/ decorData->ModelType == HOUSE_DECOR_MODEL_TYPE_WMO, roomEntityGuid, attachFlags, &worldPos);

        if (!mesh)
        {
            TC_LOG_ERROR("housing", "HouseInteriorMap::SpawnInteriorDecor: Failed to create MeshObject for decor {} (fileDataID={})",
                decor.Guid.ToString(), fileDataID);
            continue;
        }

        PhasingHandler::InitDbPhaseShift(mesh->GetPhaseShift(), PHASE_USE_FLAGS_ALWAYS_VISIBLE, 0, 0);
        mesh->InitHousingDecorData(decor.Guid, houseGuid, decor.Locked ? 1 : 0, roomEntityGuid, decor.SourceType, decor.SourceValue);

        if (AddToMap(mesh))
        {
            _decorGuidToObjGuid[decor.Guid] = mesh->GetGUID();
            ++spawnCount;
            TC_LOG_INFO("housing", "HouseInteriorMap::SpawnInteriorDecor: Spawned decor MeshObject fileDataID={} "
                "at world({:.1f},{:.1f},{:.1f}) local({:.1f},{:.1f},{:.1f}) room={}",
                fileDataID, worldX, worldY, worldZ, localPos.GetPositionX(), localPos.GetPositionY(), localPos.GetPositionZ(), roomEntityGuid.ToString());
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
    // If RoomGuid is empty, the decor was placed without room association.
    // This can happen when placed via the interior editor before room entities existed.
    // Skip truly exterior decor, but allow interior-placed decor through.
    if (decor.RoomGuid.IsEmpty())
    {
        TC_LOG_DEBUG("housing", "HouseInteriorMap::SpawnSingleInteriorDecor: Decor {} has empty RoomGuid, skipping",
            decor.Guid.ToString());
        return;
    }

    // Already spawned?
    if (_decorGuidToObjGuid.count(decor.Guid))
        return;

    HouseDecorData const* decorData = sHousingMgr.GetHouseDecorData(decor.DecorEntryId);
    if (!decorData)
        return;

    // Decor attaches to the HousingRoomEntity (Housing/2 GUID), not a MeshObject.
    ObjectGuid roomEntityGuid = decor.RoomGuid;
    Position roomWorldPos;

    // If decor has no RoomGuid (placed before room entity system), auto-assign
    // to the first non-base room. Without a valid parent, decor is not selectable.
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

    // Take the room's world position from the spawned room entity (authoritative, and unlike
    // the GetRooms() walk it cannot silently miss and leave roomWorldPos at the origin).
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
    float decorScale = decor.Scale > 0.01f ? decor.Scale : 1.0f;
    uint8 attachFlags = roomEntityGuid.IsEmpty() ? uint8(0) : uint8(3);

    // Functional decor (chair, chest, mailbox, etc.): spawn real GameObject so it stays interactive.
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
                go->ReplaceAllFlags(GameObjectFlags(0x40000));
                go->InitHousingDecorData(decor.Guid, houseGuid, decor.Locked ? 1 : 0,
                    roomEntityGuid, decor.SourceType, decor.SourceValue);
                go->InitHousingDecorMirroredPosition(localPos, rot, decorScale, roomEntityGuid, attachFlags);

                if (AddToMap(go))
                {
                    _decorGuidToObjGuid[decor.Guid] = go->GetGUID();
                    TC_LOG_INFO("housing", "HouseInteriorMap::SpawnSingleInteriorDecor: Spawned functional-decor "
                        "GameObject entry={} goEntry={} goType={} goGuid={} at world({:.1f},{:.1f},{:.1f}) room={}",
                        decor.DecorEntryId, goEntry, uint32(goTemplate->type), go->GetGUID().ToString(),
                        worldX, worldY, worldZ, roomEntityGuid.ToString());
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
    MeshObject* mesh = MeshObject::CreateMeshObject(this, localPos, rot, decorScale,
        fileDataID, /*isWMO*/ decorData->ModelType == HOUSE_DECOR_MODEL_TYPE_WMO, roomEntityGuid, attachFlags, &worldPos);

    if (!mesh)
        return;

    PhasingHandler::InitDbPhaseShift(mesh->GetPhaseShift(), PHASE_USE_FLAGS_ALWAYS_VISIBLE, 0, 0);
    mesh->InitHousingDecorData(decor.Guid, houseGuid, decor.Locked ? 1 : 0, roomEntityGuid, decor.SourceType, decor.SourceValue);

    if (AddToMap(mesh))
    {
        _decorGuidToObjGuid[decor.Guid] = mesh->GetGUID();
        TC_LOG_INFO("housing", "HouseInteriorMap::SpawnSingleInteriorDecor: Spawned decor MeshObject fileDataID={} "
            "at world({:.1f},{:.1f},{:.1f}) room={}",
            fileDataID, worldX, worldY, worldZ, roomEntityGuid.ToString());
    }
    else
    {
        delete mesh;
    }
}

void HouseInteriorMap::UpdateDecorPosition(ObjectGuid decorGuid, Position const& pos, QuaternionData const& rot, float scale /*= 1.0f*/)
{
    auto itr = _decorGuidToObjGuid.find(decorGuid);
    if (itr == _decorGuidToObjGuid.end())
        return;

    // The client renders decor from its room-relative transform (FMirroredPositionData_C), so that has to move
    // too - relocating only the server-side world position left every re-sent CREATE (re-entry, relog) at the
    // spawn-time spot.
    auto toLocal = [this](ObjectGuid roomGuid, Position const& worldPos)
    {
        for (HousingRoomEntity const* re : _roomEntities)
            if (re && re->GetGUID() == roomGuid)
                return HousingWorldToRoomLocal(re->GetPosition(), worldPos);
        return worldPos;
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
            go->UpdateHousingDecorMirroredTransform(toLocal(go->GetHousingDecorAttachParent(), pos), rot, scale);
            TC_LOG_DEBUG("housing", "HouseInteriorMap::UpdateDecorPosition: Moved decor GameObject {} to ({:.1f},{:.1f},{:.1f}) scale={:.2f}",
                decorGuid.ToString(), pos.GetPositionX(), pos.GetPositionY(), pos.GetPositionZ(), scale);
        }
    }
    else if (MeshObject* mesh = GetMeshObject(objGuid))
    {
        mesh->Relocate(pos);
        mesh->UpdateLocalTransform(toLocal(mesh->GetAttachParentGUID(), pos), rot, scale);
        TC_LOG_DEBUG("housing", "HouseInteriorMap::UpdateDecorPosition: Moved decor MeshObject {} to ({:.1f},{:.1f},{:.1f}) scale={:.2f}",
            decorGuid.ToString(), pos.GetPositionX(), pos.GetPositionY(), pos.GetPositionZ(), scale);
    }
}

void HouseInteriorMap::DespawnDecorItem(ObjectGuid decorGuid)
{
    auto itr = _decorGuidToObjGuid.find(decorGuid);
    if (itr == _decorGuidToObjGuid.end())
    {
        TC_LOG_DEBUG("housing", "HouseInteriorMap::DespawnDecorItem: No tracked visual for decorGuid={}", decorGuid.ToString());
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

    TC_LOG_DEBUG("housing", "HouseInteriorMap::DespawnDecorItem: Despawned visual for decorGuid={}", decorGuid.ToString());
}

bool HouseInteriorMap::AddPlayerToMap(Player* player, bool initPlayer /*= true*/)
{
    TC_LOG_ERROR("housing", "HouseInteriorMap::AddPlayerToMap: ENTER player={} owner={} isOwner={} "
        "_roomsSpawned={} map={} instanceId={} this={}",
        player->GetGUID().ToString(), _owner.ToString(),
        player->GetGUID() == _owner, _roomsSpawned,
        GetId(), GetInstanceId(), (void*)this);

    if (player->GetGUID() == _owner)
        _loadingPlayer = player;

    // === PRE-SPAWN: Populate ALL housing entities BEFORE Map::AddPlayerToMap ===
    // Retail sends ONE ~210KB UPDATE_OBJECT with all entity data (rooms, decor,
    // account storage, budgets) on interior map transfer. If we spawn after
    // AddPlayerToMap, the initial UPDATE_OBJECT has empty housing data and the
    // client never gets proper housing context for the editor UI.
    //
    // Visiting an offline-owner house: `preloadHousing` is null and the player
    // isn't the map's owner. Spawn rooms/decor from PlotInfo (mirrored from
    // the DB) so visitors see the owner's actual layout without needing the
    // owner online.
    Housing* preloadHousing = player->GetHousing();
    bool visitingOfflineOwner = !_roomsSpawned && player->GetGUID() != _owner;
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

            TC_LOG_INFO("housing", "HouseInteriorMap::AddPlayerToMap: VISITOR pre-spawn for offline owner {} — {} rooms, {} decor",
                _owner.ToString(), uint32(ownerPlot->Rooms.size()), uint32(ownerPlot->Decor.size()));
            break;
        }
    }

    bool const ownerPreSpawn = preloadHousing && player->GetGUID() == _owner;
    if (ownerPreSpawn)
    {
        // Rebuild rooms left over from an earlier visit (they may carry stale fragment formats),
        // but only while nobody is standing in them. The owner is not on the map yet, so no
        // DESTROY reaches them; rebuilding after Map::AddPlayerToMap instead would destroy the
        // entities the initial UPDATE_OBJECT just delivered and leave the editor pointing at
        // removed room entities (client crash on opening the house editor).
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
            int32 faction = (player->GetTeamId() == TEAM_ALLIANCE)
                ? NEIGHBORHOOD_FACTION_ALLIANCE : NEIGHBORHOOD_FACTION_HORDE;
            SpawnRoomMeshObjects(preloadHousing, faction);
            SpawnInteriorDecor(preloadHousing);
            _roomsSpawned = true;
        }

        // Populate Account entity with FHousingStorage_C + budget data
        // so the initial UPDATE_OBJECT includes full housing context
        preloadHousing->PopulateCatalogStorageEntries();
        preloadHousing->SyncUpdateFields();

        // The player stays where the transfer put them: the entry hall at the interior origin, facing the
        // house (retail SMSG_NEW_WORLD -1000/-1000/0.1, no teleport afterwards).

        // The interior plot AreaTrigger is created in the DEFERRED callback,
        // NOT here. If we AddToMap now, the visibility system includes it in
        // the initial UPDATE_OBJECT, and the client fires HOUSE_PLOT_ENTERED
        // before Status+Permissions arrive. Retail sends the AT in a separate
        // UPDATE_OBJECT (#11752) AFTER the main entity data.

        TC_LOG_ERROR("housing", "HouseInteriorMap::AddPlayerToMap: PRE-SPAWNED rooms+decor+storage "
            "(%u rooms, %u decor) before Map::AddPlayerToMap (AT deferred)",
            uint32(_roomMeshObjects.size()), uint32(_decorGuidToObjGuid.size()));
    }

    bool result = Map::AddPlayerToMap(player, initPlayer);

    if (player->GetGUID() == _owner)
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
            if (player->GetGUID() == _owner && !ownerPreSpawn)
            {
                // Always force a fresh spawn on login — old entities from a previous binary/session
                // may have stale fragment formats (e.g., root MeshObjects with FHousingRoom_C that
                // no longer exist in the current code). DespawnAll is safe here because the player
                // hasn't received any entities yet (no DESTROY goes to the client).
                //
                // H-18: that justification holds for the owner arriving, who has received
                // nothing yet - but not for anyone already standing in the interior.
                // _roomsSpawned can be set by a visitor who pre-spawned the rooms through
                // the offline-owner path, and that visitor HAS received the MeshObjects.
                // Despawning here sends them a DESTROY and the house dissolves around them
                // until the respawn lands. The stale-fragment problem the rebuild exists for
                // is only possible for entities spawned by a PREVIOUS process; if someone
                // else is standing in the map, this binary spawned what they are looking at
                // and it is already current-format. So: rebuild when alone, leave the rooms
                // standing when not.
                bool spawnRooms = true;
                if (_roomsSpawned)
                {
                    bool otherPlayersPresent = false;
                    for (MapReference const& ref : GetPlayers())
                    {
                        if (ref.GetSource() != player)
                        {
                            otherPlayersPresent = true;
                            break;
                        }
                    }

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

                    int32 faction = (player->GetTeamId() == TEAM_ALLIANCE)
                        ? NEIGHBORHOOD_FACTION_ALLIANCE : NEIGHBORHOOD_FACTION_HORDE;
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

            // Toggle WS[30906]=1 to signal the client that the player is inside a house interior.
            // Sent synchronously so the client knows it's an interior before deferred packets.
            player->SendUpdateWorldState(WORLDSTATE_HOUSING_INTERIOR, 1);

            // Defer ALL housing context packets by 500ms. The client needs time to
            // process the initial UPDATE_OBJECT (entities, room MeshObjects) before
            // housing response packets can be processed. This mirrors the exterior
            // map's deferred ENTER_PLOT pattern (HousingMap.cpp).
            // Without the delay, the housing system TLS may not be ready and the
            // client silently drops the Status/Permissions packets.
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

                    Housing* housing = p->GetHousing();
                    if (!housing)
                        return;

                    // ENTER_PLOT is sent AFTER the AT CREATE in step 8 below.
                    // The sequence is: AT CREATE → ENTER_PLOT → re-send Status+Perms.
                    // ENTER_PLOT fires HOUSE_PLOT_ENTERED (FrameScript event 1073) which
                    // loads Blizzard_HousingControls. The handler resets editor state, so
                    // Status+Perms are re-sent afterward to re-establish context.

                    // Steps 1-3 (HouseInfo, Status, Permissions) moved to AFTER
                    // ENTER_PLOT below. ENTER_PLOT resets editor state, so sending
                    // Status+Permissions before it is wasteful. The exterior AT handler
                    // (at_housing_plot.cpp) also sends Status+Permissions AFTER ENTER_PLOT.

                    // 0) Drive the housing tutorial forward. The editor UI stays locked until
                    // QUEST_HOUSING_TUTORIAL_COMPLETE ("Home at Last") is REWARDED, and that quest
                    // is AUTO_ACCEPT|AUTO_COMPLETE with no quest-giver NPC on either end - retail
                    // grants it by script and completes it with a kill credit the moment the player
                    // first stands inside their house (both packet-attested in the starter captures).
                    // Without this the chain is unenterable: no NPC ever offers it, so the player
                    // reaches the end of the visible questline and the editor never unlocks.
                    GrantHousingTutorialProgress(p);

                    // 1) PostTutorialAuras (slots 8, 9, 50)
                    SendHousingPostTutorialAuras(p);

                    // 5) Account CREATE + HousingPlayerHouseEntity + decor
                    {
                        housing->PopulateCatalogStorageEntries();
                        housing->SyncUpdateFields();

                        WorldSession* session = p->GetSession();
                        UpdateData storageUpdate(p->GetMapId());
                        WorldPacket storagePacket;

                        session->BuildHousingAccountEntitiesUpdate(&storageUpdate, p);

                        // Decor and HousingRoomEntity CREATEs are sent by the map visibility
                        // system (AddToMap in SpawnRoomMeshObjects/SpawnInteriorDecor).
                        // Do NOT send manual CREATEs here — double-sending corrupts the
                        // client's entity state and makes decor unselectable after relog.

                        storageUpdate.BuildPacket(&storagePacket);
                        p->SendDirectMessage(&storagePacket);

                        session->GetBattlenetAccount().ClearUpdateMask(true);
                        session->GetHousingPlayerHouseEntity().ClearUpdateMask(true);

                        TC_LOG_ERROR("housing", "HouseInteriorMap deferred: Sent Account+budget for {}",
                            playerGuid.ToString());
                    }

                    // Map-level Housing/3 entity (objectType=18) is now included in
                    // the initial UPDATE_OBJECT via Player::BuildCreateUpdateBlockForPlayer.
                    // It must be in the initial batch for the client's type-18 render init.

                    // 7) InitiativeServiceStatus
                    {
                        WorldPackets::Housing::InitiativeServiceStatus initStatus;
                        initStatus.ServiceEnabled = true;
                        p->SendDirectMessage(initStatus.Write());
                    }

                    // No plot AreaTrigger, plot-enter auras or CurrentHouse inside the house: the 12.1.0.69933 interior
                    // sniffs have none of them (CurrentHouse stays empty, the only type-11-free entities are rooms, house,
                    // account and decor). The interior AT we used to add kept the client's editor camera tied to a plot
                    // that no longer existed once the player walked out of the house.

                    // 10) Spawn the interior exit door — blizzlike decor entity + GO hierarchy.
                    // Retail sniff: A HousingDecorEntity (Object Type 18, Housing/56 GUID) with
                    // FHousingDecor_C fragment carries TargetGameObjectGUID pointing to the door GO.
                    // The GO (type GOOBER) attaches to the decor entity via TransportGUID with
                    // PositionLocalSpace=(0,0,0) and AttachmentFlags=7.
                    // Alliance entry=575017 (displayId=113554), Horde entry=587318.
                    {
                        TC_LOG_INFO("housing", "InteriorDoor: Starting blizzlike door spawn for player {} (map owner={})",
                            playerGuid.ToString(), _owner.ToString());

                        // The interior map is instanced per-HOUSE, not per-visitor.
                        // Guests can enter the owner's house — p->GetHousing() is the VISITOR's house,
                        // not the one being visited. Always resolve the OWNER's housing for door context.
                        Housing* ownerHousing = nullptr;
                        if (Player* ownerPlayer = ObjectAccessor::FindPlayer(_owner))
                            ownerHousing = ownerPlayer->GetHousing();

                        if (!ownerHousing)
                        {
                            // Owner may be offline when a guest enters — fall back to a minimal
                            // SummonGameObject so the guest isn't locked in. Blizzlike decor entity
                            // hierarchy requires owner housing context (entry hall GUID, houseGuid).
                            TC_LOG_WARN("housing", "InteriorDoor: owner housing unavailable (owner offline?) — "
                                "fallback to SummonGameObject for player {}", playerGuid.ToString());
                            float fbX = _originX - 2.52f;
                            float fbY = _originY;
                            float fbZ = _originZ + 0.02f;
                            if (GameObject* doorGo = p->SummonGameObject(INTERIOR_DOOR_GO_ALLIANCE,
                                Position(fbX, fbY, fbZ, 0.0f), QuaternionData(0, 0, 0, 1), 0s))
                            {
                                doorGo->ReplaceAllFlags(GameObjectFlags(0x40000));
                                TC_LOG_INFO("housing", "InteriorDoor: Fallback door guid={}",
                                    doorGo->GetGUID().ToString());
                            }
                            return;
                        }

                        uint32 doorGoEntry = INTERIOR_DOOR_GO_ALLIANCE; // Alliance default
                        Neighborhood* nbh = sNeighborhoodMgr.GetNeighborhood(ownerHousing->GetNeighborhoodGuid());
                        int32 faction = nbh ? nbh->GetFactionRestriction() : NEIGHBORHOOD_FACTION_ALLIANCE;
                        if (faction == NEIGHBORHOOD_FACTION_HORDE)
                            doorGoEntry = INTERIOR_DOOR_GO_HORDE;

                        TC_LOG_INFO("housing", "InteriorDoor: faction={} doorGoEntry={}",
                            faction == NEIGHBORHOOD_FACTION_HORDE ? "Horde" : "Alliance", doorGoEntry);

                        // Sniff-verified position: (-2.521, 0.006, 0.020) relative to entry hall room entity
                        float doorLocalX = -2.52f;
                        float doorLocalY = 0.006f;
                        float doorLocalZ = 0.02f;
                        float doorWorldX = _originX + doorLocalX;
                        float doorWorldY = _originY + doorLocalY;
                        float doorWorldZ = _originZ + doorLocalZ;

                        TC_LOG_INFO("housing", "InteriorDoor: origin=({:.2f},{:.2f},{:.2f}) doorWorld=({:.2f},{:.2f},{:.2f})",
                            _originX, _originY, _originZ, doorWorldX, doorWorldY, doorWorldZ);

                        // Find the entry hall room entity GUID (slot 0) from the OWNER's housing
                        ObjectGuid entryHallGuid = ObjectGuid::Empty;
                        for (auto const* rm : ownerHousing->GetRooms())
                        {
                            TC_LOG_DEBUG("housing", "InteriorDoor: examining room slot={} entryId={} guid={}",
                                rm->SlotIndex, rm->RoomEntryId, rm->Guid.ToString());
                            if (rm->SlotIndex == 0)
                            {
                                entryHallGuid = rm->Guid;
                                break;
                            }
                        }

                        if (entryHallGuid.IsEmpty())
                        {
                            TC_LOG_ERROR("housing", "InteriorDoor: entry hall room (slot 0) NOT FOUND — "
                                "falling back to SummonGameObject");
                            // Fallback to the old simple approach so door still works
                            if (GameObject* doorGo = p->SummonGameObject(doorGoEntry,
                                Position(doorWorldX, doorWorldY, doorWorldZ, 0.0f),
                                QuaternionData(0, 0, 0, 1), 0s))
                            {
                                doorGo->ReplaceAllFlags(GameObjectFlags(0x40000));
                                TC_LOG_INFO("housing", "InteriorDoor: Fallback SummonGameObject succeeded guid={}",
                                    doorGo->GetGUID().ToString());
                            }
                            return;
                        }

                        // The OWNER's house GUID — not the visiting player's
                        ObjectGuid interiorHouseGuid = ownerHousing->GetHouseGuid();
                        TC_LOG_INFO("housing", "InteriorDoor: entryHallGuid={} houseGuid={}",
                            entryHallGuid.ToString(), interiorHouseGuid.ToString());

                        // Create the decor entity (Object Type 18, Housing/56 subType=1)
                        ObjectGuid decorGuid = ObjectGuidFactory::CreateHousing(1, 0,
                            doorGoEntry, GetInstanceId() + 900000);

                        TC_LOG_INFO("housing", "InteriorDoor: generated decorGuid={}", decorGuid.ToString());

                        // GO via SummonGameObject has PrivateObjectOwner=player, so it is
                        // despawned when the player leaves the map. The decor entity persists
                        // on the (per-player) interior map across leave+reenter. Split the two:
                        //   - if decor entity exists, skip re-creating it (duplicate-GUID insert
                        //     would hit MapStoredObjectsUnorderedMap's assertion)
                        //   - always (re)summon the interactive GO so the door button is there
                        //     on re-entry too
                        Position doorWorldPos(doorWorldX, doorWorldY, doorWorldZ, 0.0f);
                        bool decorAlreadyPresent = GetObjectsStore().Find<HousingDecorEntity>(decorGuid) != nullptr;

                        if (!decorAlreadyPresent)
                        {
                            HousingDecorEntity* decorEntity = new HousingDecorEntity();

                            if (!decorEntity->Create(decorGuid, this, doorWorldPos))
                            {
                                TC_LOG_ERROR("housing", "InteriorDoor: decorEntity Create FAILED — falling back to SummonGameObject");
                                delete decorEntity;
                                if (GameObject* doorGo = p->SummonGameObject(doorGoEntry,
                                    doorWorldPos, QuaternionData(0, 0, 0, 1), 0s))
                                {
                                    doorGo->ReplaceAllFlags(GameObjectFlags(0x40000));
                                    TC_LOG_INFO("housing", "InteriorDoor: Fallback SummonGameObject succeeded guid={}",
                                        doorGo->GetGUID().ToString());
                                }
                                return;
                            }

                            TC_LOG_INFO("housing", "InteriorDoor: decorEntity created OK guid={}", decorGuid.ToString());

                            decorEntity->SetDecorGUID(decorGuid);
                            decorEntity->SetAttachParentGUID(entryHallGuid);
                            decorEntity->SetFlags(0);
                            decorEntity->SetPersistedData(interiorHouseGuid);

                            ObjectGuid goGuid = ObjectGuid::Create<HighGuid::GameObject>(
                                GetId(), doorGoEntry, GetInstanceId() + 900000);
                            decorEntity->SetTargetGameObjectGUID(goGuid);

                            Position localPos(doorLocalX, doorLocalY, doorLocalZ);
                            decorEntity->SetMirroredPosition(localPos, QuaternionData(0, 0, 0, 1),
                                1.0f, entryHallGuid, 3);

                            PhasingHandler::InitDbPhaseShift(decorEntity->GetPhaseShift(),
                                PHASE_USE_FLAGS_ALWAYS_VISIBLE, 0, 0);

                            if (!AddToMap(decorEntity))
                            {
                                TC_LOG_ERROR("housing", "InteriorDoor: decorEntity AddToMap FAILED — falling back to SummonGameObject");
                                delete decorEntity;
                                if (GameObject* doorGo = p->SummonGameObject(doorGoEntry,
                                    doorWorldPos, QuaternionData(0, 0, 0, 1), 0s))
                                {
                                    doorGo->ReplaceAllFlags(GameObjectFlags(0x40000));
                                    TC_LOG_INFO("housing", "InteriorDoor: Fallback SummonGameObject succeeded guid={}",
                                        doorGo->GetGUID().ToString());
                                }
                                return;
                            }

                            TC_LOG_INFO("housing", "InteriorDoor: decorEntity AddToMap OK");
                        }
                        else
                        {
                            TC_LOG_INFO("housing", "InteriorDoor: decor entity already present on map — re-summoning GO only");
                        }

                        // Spawn the interactive GO via SummonGameObject — this path is proven
                        // to trigger visibility updates to the existing player. Manual
                        // CreateGameObject+AddToMap misses SetSpawnedByDefault(false) +
                        // SetRespawnTime(0) and resulted in the GO being invisible client-side.
                        // This always runs (even on re-entry with existing decor) because the
                        // GO is PrivateObjectOwner-tied to the player and despawns on leave.
                        GameObject* doorGo = p->SummonGameObject(doorGoEntry,
                            doorWorldPos, QuaternionData(0, 0, 0, 1), 0s);
                        if (!doorGo)
                        {
                            TC_LOG_ERROR("housing", "InteriorDoor: SummonGameObject FAILED for entry={}",
                                doorGoEntry);
                            return;
                        }

                        doorGo->ReplaceAllFlags(GameObjectFlags(0x40000));

                        TC_LOG_INFO("housing", "InteriorDoor: SUCCESS — decorReused={} decorEntity={} doorGO={} entry={} "
                            "at ({:.2f},{:.2f},{:.2f}) entryHall={} house={}",
                            decorAlreadyPresent, decorGuid.ToString(), doorGo->GetGUID().ToString(),
                            doorGoEntry, doorWorldX, doorWorldY, doorWorldZ,
                            entryHallGuid.ToString(), interiorHouseGuid.ToString());
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

        // Leaving by any path (hearthstone, teleport, logout) must not carry an editor out of the house:
        // the layout editor's stun/no-gravity aura and the editing context would stick to the player.
        if (player->GetGUID() == _owner && housing->GetEditorMode() != HOUSING_EDITOR_MODE_NONE)
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

    // Retail grants this by script, not through a quest chain: it is AUTO_ACCEPT and no NPC offers
    // it, so unless we put it in the log the player can never obtain it.
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
        TC_LOG_INFO("housing", "GrantHousingTutorialProgress: granted quest {} to player {}",
            QUEST_HOUSING_TUTORIAL_COMPLETE, player->GetGUID().ToString());
    }

    // Completing objective: retail credits this creature the moment the player stands in the house.
    player->KilledMonsterCredit(NPC_HOUSING_TUTORIAL_HOUSE_ENTERED_CREDIT);

    // AUTO_COMPLETE quests are submitted by the client with the PLAYER as the quest giver. Our editor
    // gate reads GetQuestRewardStatus, so close the loop here rather than depending on that round trip.
    if (player->GetQuestStatus(QUEST_HOUSING_TUTORIAL_COMPLETE) == QUEST_STATUS_COMPLETE
        && quest->HasFlag(QUEST_FLAGS_AUTO_COMPLETE))
    {
        player->RewardQuest(quest, LootItemType::Item, 0, player, false);
        TC_LOG_INFO("housing", "GrantHousingTutorialProgress: auto-completed quest {} for player {} - housing editor unlocked",
            QUEST_HOUSING_TUTORIAL_COMPLETE, player->GetGUID().ToString());
    }
    else
        TC_LOG_INFO("housing", "GrantHousingTutorialProgress: quest {} status for player {} is {} after credit {}",
            QUEST_HOUSING_TUTORIAL_COMPLETE, player->GetGUID().ToString(),
            uint32(player->GetQuestStatus(QUEST_HOUSING_TUTORIAL_COMPLETE)),
            NPC_HOUSING_TUTORIAL_HOUSE_ENTERED_CREDIT);
}
