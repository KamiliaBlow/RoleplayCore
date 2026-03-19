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

#include "MeshObject.h"
#include "Log.h"
#include "Map.h"
#include "PhasingHandler.h"
#include "UpdateData.h"

MeshObject::MeshObject() : WorldObject(false), MapObject()
{
    m_objectTypeId = TYPEID_MESH_OBJECT;

    m_updateFlag.Stationary = true;
    m_updateFlag.MeshObject = true;

    m_entityFragments.Add(WowCS::EntityFragment::Tag_MeshObject, false);
}

MeshObject::~MeshObject() = default;

void MeshObject::AddToWorld()
{
    if (!IsInWorld())
    {
        GetMap()->GetObjectsStore().Insert<MeshObject>(this);
        WorldObject::AddToWorld();
    }
}

void MeshObject::RemoveFromWorld()
{
    if (IsInWorld())
    {
        WorldObject::RemoveFromWorld();
        GetMap()->GetObjectsStore().Remove<MeshObject>(this);
    }
}

void MeshObject::Update(uint32 diff)
{
    WorldObject::Update(diff);
}

MeshObject* MeshObject::CreateMeshObject(Map* map, Position const& pos,
    QuaternionData const& rotation, float scale,
    int32 fileDataID, bool isWMO,
    ObjectGuid attachParent /*= ObjectGuid::Empty*/, uint8 attachFlags /*= 0*/,
    Position const* worldPos /*= nullptr*/)
{
    MeshObject* mesh = new MeshObject();
    if (!mesh->Create(map, pos, rotation, scale, fileDataID, isWMO, attachParent, attachFlags, worldPos))
    {
        delete mesh;
        return nullptr;
    }

    return mesh;
}

bool MeshObject::Create(Map* map, Position const& pos, QuaternionData const& rotation,
    float scale, int32 fileDataID, bool isWMO,
    ObjectGuid attachParent, uint8 attachFlags,
    Position const* worldPos)
{
    SetMap(map);

    // For child pieces (attached to a parent), pos contains LOCAL-SPACE coordinates.
    // Use worldPos (the parent's position) for server-side grid placement so the MeshObject
    // is in the correct grid cell and visible to players near the house.
    // The local-space offset is stored in FMirroredPositionData_C for client rendering.
    if (worldPos)
        Relocate(*worldPos);
    else
        Relocate(pos);

    if (!IsPositionValid())
    {
        TC_LOG_ERROR("entities.meshobject", "MeshObject not created. Invalid coordinates (X: {} Y: {})",
            GetPositionX(), GetPositionY());
        return false;
    }

    // Phase shift: always visible regardless of player's phase state.
    // Must use PHASE_USE_FLAGS_ALWAYS_VISIBLE (matching door GOs, cornerstones, ATs, decor GOs)
    // otherwise cosmetic phase additions on plot exit hide meshes including neighbor houses.
    PhasingHandler::InitDbPhaseShift(GetPhaseShift(), PHASE_USE_FLAGS_ALWAYS_VISIBLE, 0, 0);

    _Create(ObjectGuid::Create<HighGuid::MeshObject>(GetMapId(), 0,
        GetMap()->GenerateLowGuid<HighGuid::MeshObject>()));

    SetObjectScale(1.0f);

    // Retail sniff: MeshObject decor entities have ObjectData.EntryID set to FileDataID.
    // e.g., EntryID=7011541 matching FileDataID=7011541 on the same entity.
    // The client may use EntryID for internal entity lookups in the placed decor hash map.
    SetEntry(fileDataID);

    // Set mesh object update fields
    auto meshData = m_values.ModifyValue(&MeshObject::m_meshObjectData);
    SetUpdateFieldValue(meshData.ModifyValue(&UF::MeshObjectData::FileDataID), fileDataID);
    SetUpdateFieldValue(meshData.ModifyValue(&UF::MeshObjectData::IsWMO), isWMO);
    SetUpdateFieldValue(meshData.ModifyValue(&UF::MeshObjectData::IsRoom), false);

    // Register FMeshObjectData_C entity fragment
    m_entityFragments.Add(WowCS::EntityFragment::FMeshObjectData_C, false,
        WowCS::GetRawFragmentData(m_meshObjectData));

    // Store movement block data (used by BaseEntity::BuildCreateUpdateBlockMovement)
    _attachParentGUID = attachParent;
    _positionLocalSpace = pos;  // local-space offset (for movement block MeshObject section)
    _rotationLocalSpace = rotation;
    _scaleLocalSpace = scale;
    _attachmentFlags = attachFlags;

    // Set mirrored position data (FMirroredPositionData_C fragment)
    auto posData = m_values.ModifyValue(&MeshObject::m_mirroredPositionData)
        .ModifyValue(&UF::MirroredPositionData::PositionData);
    SetUpdateFieldValue(posData.ModifyValue(&UF::MirroredMeshObjectData::AttachParentGUID), attachParent);
    SetUpdateFieldValue(posData.ModifyValue(&UF::MirroredMeshObjectData::PositionLocalSpace),
        TaggedPosition<Position::XYZ>(pos.GetPositionX(), pos.GetPositionY(), pos.GetPositionZ()));
    SetUpdateFieldValue(posData.ModifyValue(&UF::MirroredMeshObjectData::RotationLocalSpace), rotation);
    SetUpdateFieldValue(posData.ModifyValue(&UF::MirroredMeshObjectData::ScaleLocalSpace), scale);
    SetUpdateFieldValue(posData.ModifyValue(&UF::MirroredMeshObjectData::AttachmentFlags), attachFlags);

    // Register FMirroredPositionData_C entity fragment
    m_entityFragments.Add(WowCS::EntityFragment::FMirroredPositionData_C, false,
        WowCS::GetRawFragmentData(m_mirroredPositionData));

    SetZoneScript();
    UpdatePositionData();

    // NOTE: AddToMap is NOT called here. The caller must call map->AddToMap(mesh) after
    // setting up all entity fragments (e.g. InitHousingFixtureData). The create packet
    // is sent during AddToMap, so all fragments must be registered before that point.

    TC_LOG_DEBUG("housing", "MeshObject::Create: guid={} fileDataID={} isWMO={} at ({:.1f}, {:.1f}, {:.1f}) on map {} (not yet added to map)",
        GetGUID().ToString(), fileDataID, isWMO,
        pos.GetPositionX(), pos.GetPositionY(), pos.GetPositionZ(), GetMapId());

    return true;
}

void MeshObject::InitHousingDecorData(ObjectGuid decorGuid, ObjectGuid houseGuid, uint8 flags,
    ObjectGuid roomEntityGuid /*= ObjectGuid::Empty*/, uint8 sourceType /*= 0*/, std::string sourceValue /*= {}*/)
{
    if (m_housingDecorData.has_value())
        return;

    // Sniff-verified: FHousingDecor_C entity fragment on MeshObject decor entities.
    // TargetGameObjectGUID is EMPTY (0x0) in ALL retail sniffs.
    // AttachParentGUID points to the room entity (Housing/18 base room) the decor is placed in.
    SetUpdateFieldValue(m_values.ModifyValue(&Object::m_housingDecorData, 0)
        .ModifyValue(&UF::HousingDecorData::DecorGUID), decorGuid);
    SetUpdateFieldValue(m_values.ModifyValue(&Object::m_housingDecorData, 0)
        .ModifyValue(&UF::HousingDecorData::AttachParentGUID), roomEntityGuid);
    SetUpdateFieldValue(m_values.ModifyValue(&Object::m_housingDecorData, 0)
        .ModifyValue(&UF::HousingDecorData::Flags), flags);
    SetUpdateFieldValue(m_values.ModifyValue(&Object::m_housingDecorData, 0)
        .ModifyValue(&UF::HousingDecorData::TargetGameObjectGUID), ObjectGuid::Empty);

    auto persistedRef = m_values.ModifyValue(&Object::m_housingDecorData, 0)
        .ModifyValue(&UF::HousingDecorData::PersistedData, 0);
    SetUpdateFieldValue(persistedRef.ModifyValue(&UF::DecorStoragePersistedData::HouseGUID), houseGuid);
    SetUpdateFieldValue(persistedRef.ModifyValue(&UF::DecorStoragePersistedData::SourceType), sourceType);
    SetUpdateFieldValue(persistedRef.ModifyValue(&UF::DecorStoragePersistedData::SourceValue), std::move(sourceValue));

    m_entityFragments.Add(WowCS::EntityFragment::FHousingDecor_C, IsInWorld(),
        WowCS::GetRawFragmentData(m_housingDecorData));

    // Retail sniff-verified: decor MeshObjects have exactly these fragments:
    //   [CGObject(2), FMeshObjectData_C(19), FHousingDecor_C(20),
    //    FMirroredPositionData_C(31), Tag_MeshObject(221)]
    // FHousingDecorActor_C (28) is NOT present on any retail entity.

    // Retail sniff: HasDecor movement block flag is NEVER set (always False).
    // The room entity GUID is already in the FHousingDecor_C fragment's AttachParentGUID field.
    // Do NOT set m_updateFlag.Decor here — it adds an extra movement block field
    // that the client does not expect.
    _decorRoomEntityGUID = roomEntityGuid;

    TC_LOG_DEBUG("housing", "MeshObject::InitHousingDecorData: guid={} decorGuid={} houseGuid={} flags={} roomEntity={} (FHousingDecor_C ON)",
        GetGUID().ToString(), decorGuid.ToString(), houseGuid.ToString(), flags, roomEntityGuid.ToString());
}

void MeshObject::InitHousingFixtureData(ObjectGuid houseGuid, ObjectGuid fixtureGuid,
    ObjectGuid parentFixtureGuid, int32 exteriorComponentID,
    int32 houseExteriorWmoDataID, uint8 exteriorComponentType /*= 9*/,
    uint8 houseSize /*= 2*/, int32 exteriorComponentHookID /*= -1*/, bool isRoot /*= false*/)
{
    if (m_housingFixtureData.has_value())
        return;

    // FHousingFixture_C fragment (ID 34, 96 bytes, 11 fields with HasChangesMask<11>).
    // Field order must match the client's CREATE deserializer:
    //   [0] ExteriorComponentID  (CompressedUInt32)
    //   [1] HouseExteriorWmoDataID (CompressedUInt32)
    //   [2] ExteriorComponentHookID (CompressedUInt32, defaults -1)
    //   [3] HouseGUID (PackedGUID128)
    //   [4] AttachParentGUID (PackedGUID128) — parent fixture in hierarchy
    //   [5] Guid (PackedGUID128) — unique per fixture, for client identification
    //   [6] GameObjectGUID (PackedGUID128) — always empty
    //   [7] ExteriorComponentType (uint8)
    //   [8] Field_59 (uint8)
    //   [9] Size (uint8)
    SetUpdateFieldValue(m_values.ModifyValue(&Object::m_housingFixtureData, 0)
        .ModifyValue(&UF::HousingFixtureData::ExteriorComponentID), exteriorComponentID);
    SetUpdateFieldValue(m_values.ModifyValue(&Object::m_housingFixtureData, 0)
        .ModifyValue(&UF::HousingFixtureData::HouseExteriorWmoDataID), houseExteriorWmoDataID);
    SetUpdateFieldValue(m_values.ModifyValue(&Object::m_housingFixtureData, 0)
        .ModifyValue(&UF::HousingFixtureData::ExteriorComponentHookID), exteriorComponentHookID);
    SetUpdateFieldValue(m_values.ModifyValue(&Object::m_housingFixtureData, 0)
        .ModifyValue(&UF::HousingFixtureData::HouseGUID), houseGuid);
    // AttachParentGUID: the parent fixture's unique GUID in the hierarchy.
    // Root pieces have empty parent. Child pieces point to their parent root's fixture GUID.
    // The client uses this to build the fixture tree and resolve hook point ownership.
    SetUpdateFieldValue(m_values.ModifyValue(&Object::m_housingFixtureData, 0)
        .ModifyValue(&UF::HousingFixtureData::AttachParentGUID), parentFixtureGuid);
    // Guid: unique per fixture — the client uses this to identify individual fixtures.
    // Must be a Housing-type GUID (client crashes with non-Housing GUIDs here).
    SetUpdateFieldValue(m_values.ModifyValue(&Object::m_housingFixtureData, 0)
        .ModifyValue(&UF::HousingFixtureData::Guid), fixtureGuid);
    // GameObjectGUID: sniff confirms empty for all fixture pieces
    SetUpdateFieldValue(m_values.ModifyValue(&Object::m_housingFixtureData, 0)
        .ModifyValue(&UF::HousingFixtureData::ExteriorComponentType), exteriorComponentType);
    SetUpdateFieldValue(m_values.ModifyValue(&Object::m_housingFixtureData, 0)
        .ModifyValue(&UF::HousingFixtureData::Field_59), uint8(1)); // sniff: always 1
    SetUpdateFieldValue(m_values.ModifyValue(&Object::m_housingFixtureData, 0)
        .ModifyValue(&UF::HousingFixtureData::Size), houseSize);

    m_entityFragments.Add(WowCS::EntityFragment::FHousingFixture_C, IsInWorld(),
        WowCS::GetRawFragmentData(m_housingFixtureData));

    // Cache for targeted fixture lookup and hierarchy traversal
    _exteriorComponentHookID = exteriorComponentHookID;
    _exteriorComponentID = exteriorComponentID;
    _fixtureGuid = fixtureGuid;

    // Root pieces get Tag_HouseExteriorRoot (225), child pieces get Tag_HouseExteriorPiece (224).
    // The client uses Tag_HouseExteriorRoot to identify the fixture GUID for edit mode enter/exit.
    _isExteriorRoot = isRoot;
    if (isRoot)
        m_entityFragments.Add(WowCS::EntityFragment::Tag_HouseExteriorRoot, IsInWorld());
    else
        m_entityFragments.Add(WowCS::EntityFragment::Tag_HouseExteriorPiece, IsInWorld());

    TC_LOG_DEBUG("housing", "MeshObject::InitHousingFixtureData: meshGuid={} fixtureGuid={} "
        "parentFixtureGuid={} houseGuid={} extCompID={} wmoDataID={} hookID={} type={} size={} isRoot={}",
        GetGUID().ToString(), fixtureGuid.ToString(), parentFixtureGuid.ToString(),
        houseGuid.ToString(), exteriorComponentID, houseExteriorWmoDataID,
        exteriorComponentHookID, exteriorComponentType, houseSize, isRoot);
}

void MeshObject::UpdateExteriorComponentID(int32 id)
{
    if (!m_housingFixtureData.has_value())
        return;

    SetUpdateFieldValue(m_values.ModifyValue(&Object::m_housingFixtureData, 0)
        .ModifyValue(&UF::HousingFixtureData::ExteriorComponentID), id);
    _exteriorComponentID = id;
}

void MeshObject::InitHousingRoomData(ObjectGuid houseGuid, int32 houseRoomID,
    int32 flags, int32 floorIndex)
{
    if (m_housingRoomData.has_value())
        return;

    // Populate HousingRoomData (FHousingRoom_C fragment data).
    // Sniff-verified: Room entities carry HouseGUID, HouseRoomID, Flags, FloorIndex,
    // and a MeshObjects array referencing attached room component MeshObjects.
    auto roomData = m_values.ModifyValue(&Object::m_housingRoomData, 0);
    SetUpdateFieldValue(roomData.ModifyValue(&UF::HousingRoomData::HouseGUID), houseGuid);
    SetUpdateFieldValue(roomData.ModifyValue(&UF::HousingRoomData::HouseRoomID), houseRoomID);
    SetUpdateFieldValue(roomData.ModifyValue(&UF::HousingRoomData::Flags), flags);
    SetUpdateFieldValue(roomData.ModifyValue(&UF::HousingRoomData::FloorIndex), floorIndex);

    // Register FHousingRoom_C entity fragment
    m_entityFragments.Add(WowCS::EntityFragment::FHousingRoom_C, IsInWorld(),
        WowCS::GetRawFragmentData(m_housingRoomData));

    // Register Tag_HousingRoom tag fragment
    m_entityFragments.Add(WowCS::EntityFragment::Tag_HousingRoom, IsInWorld());

    // Retail sniff: HasRoom movement block flag is NEVER set (always False).
    // The house GUID is already in the FHousingRoom_C fragment data.
    // Do NOT set m_updateFlag.Room here — it adds an extra movement block field
    // that the client does not expect.
    _roomHouseGUID = houseGuid;

    TC_LOG_DEBUG("housing", "MeshObject::InitHousingRoomData: guid={} houseGuid={} "
        "roomID={} flags={} floor={} (Room flag=ON)",
        GetGUID().ToString(), houseGuid.ToString(), houseRoomID, flags, floorIndex);
}

void MeshObject::AddRoomMeshObject(ObjectGuid meshObjectGuid)
{
    if (!m_housingRoomData.has_value())
        return;

    AddDynamicUpdateFieldValue(m_values.ModifyValue(&Object::m_housingRoomData, 0)
        .ModifyValue(&UF::HousingRoomData::MeshObjects)) = meshObjectGuid;

    TC_LOG_DEBUG("housing", "MeshObject::AddRoomMeshObject: room={} added meshObject={}",
        GetGUID().ToString(), meshObjectGuid.ToString());
}

void MeshObject::AddRoomDoor(int32 roomComponentID, Position const& offset, uint8 roomComponentType, ObjectGuid attachedRoomGuid)
{
    if (!m_housingRoomData.has_value())
        return;

    // For a fresh dynamic array entry, populate fields via the mutable reference's ModifyValue
    // which returns a setter that marks the change mask and writes the underlying value.
    auto&& doorRef = AddDynamicUpdateFieldValue(m_values.ModifyValue(&Object::m_housingRoomData, 0)
        .ModifyValue(&UF::HousingRoomData::Doors));
    doorRef.ModifyValue(&UF::HousingDoorData::RoomComponentID).SetValue(roomComponentID);
    doorRef.ModifyValue(&UF::HousingDoorData::RoomComponentOffset).SetValue(
        TaggedPosition<Position::XYZ>(offset.GetPositionX(), offset.GetPositionY(), offset.GetPositionZ()));
    doorRef.ModifyValue(&UF::HousingDoorData::RoomComponentType).SetValue(roomComponentType);
    doorRef.ModifyValue(&UF::HousingDoorData::AttachedRoomGUID).SetValue(attachedRoomGuid);

    TC_LOG_DEBUG("housing", "MeshObject::AddRoomDoor: room={} componentID={} type={} offset=({:.1f},{:.1f},{:.1f}) attachedRoom={}",
        GetGUID().ToString(), roomComponentID, roomComponentType,
        offset.GetPositionX(), offset.GetPositionY(), offset.GetPositionZ(),
        attachedRoomGuid.ToString());
}

void MeshObject::InitHousingRoomComponentData(ObjectGuid roomGuid,
    int32 roomComponentOptionID, int32 roomComponentID,
    uint8 roomComponentType, int32 field24,
    int32 houseThemeID, int32 roomComponentTextureID,
    int32 roomComponentTypeParam,
    float geoboxMinX, float geoboxMinY, float geoboxMinZ,
    float geoboxMaxX, float geoboxMaxY, float geoboxMaxZ)
{
    if (m_housingRoomComponentMeshData.has_value())
        return;

    // Set IsRoom = true on MeshObjectData so the client treats this as a room mesh
    {
        auto meshData = m_values.ModifyValue(&MeshObject::m_meshObjectData);
        SetUpdateFieldValue(meshData.ModifyValue(&UF::MeshObjectData::IsRoom), true);
    }

    // Set Geobox (axis-aligned bounding box) on MeshObjectData.
    // Sniff-verified: ALL retail room component meshes have Geobox (-35,-30,-1.01)→(35,30,125.01).
    // The client uses this box for its OutsidePlotBounds collision check.
    {
        auto meshData = m_values.ModifyValue(&MeshObject::m_meshObjectData);
        UF::AaBox geobox;
        geobox.Low = TaggedPosition<Position::XYZ>(geoboxMinX, geoboxMinY, geoboxMinZ);
        geobox.High = TaggedPosition<Position::XYZ>(geoboxMaxX, geoboxMaxY, geoboxMaxZ);
        SetUpdateFieldValue(meshData.ModifyValue(&UF::MeshObjectData::Geobox, uint32(0)), std::move(geobox));
    }

    // Populate HousingRoomComponentMeshData (FHousingRoomComponentMesh_C fragment data)
    auto compData = m_values.ModifyValue(&Object::m_housingRoomComponentMeshData, 0);
    SetUpdateFieldValue(compData.ModifyValue(&UF::HousingRoomComponentMeshData::RoomGUID), roomGuid);
    SetUpdateFieldValue(compData.ModifyValue(&UF::HousingRoomComponentMeshData::RoomComponentOptionID), roomComponentOptionID);
    SetUpdateFieldValue(compData.ModifyValue(&UF::HousingRoomComponentMeshData::RoomComponentID), roomComponentID);
    SetUpdateFieldValue(compData.ModifyValue(&UF::HousingRoomComponentMeshData::Field_20), uint8(0));
    SetUpdateFieldValue(compData.ModifyValue(&UF::HousingRoomComponentMeshData::RoomComponentType), roomComponentType);
    SetUpdateFieldValue(compData.ModifyValue(&UF::HousingRoomComponentMeshData::Field_24), field24);
    SetUpdateFieldValue(compData.ModifyValue(&UF::HousingRoomComponentMeshData::HouseThemeID), houseThemeID);
    SetUpdateFieldValue(compData.ModifyValue(&UF::HousingRoomComponentMeshData::RoomComponentTextureID), roomComponentTextureID);
    SetUpdateFieldValue(compData.ModifyValue(&UF::HousingRoomComponentMeshData::RoomComponentTypeParam), roomComponentTypeParam);

    // Register FHousingRoomComponentMesh_C entity fragment
    m_entityFragments.Add(WowCS::EntityFragment::FHousingRoomComponentMesh_C, IsInWorld(),
        WowCS::GetRawFragmentData(m_housingRoomComponentMeshData));

    TC_LOG_ERROR("housing", "MeshObject::InitHousingRoomComponentData: guid={} roomGuid={} "
        "compOptionID={} compID={} compType={} themeID={} "
        "geobox=({:.2f},{:.2f},{:.2f})→({:.2f},{:.2f},{:.2f})",
        GetGUID().ToString(), roomGuid.ToString(),
        roomComponentOptionID, roomComponentID, roomComponentType, houseThemeID,
        geoboxMinX, geoboxMinY, geoboxMinZ, geoboxMaxX, geoboxMaxY, geoboxMaxZ);
}

void MeshObject::BuildValuesCreate(UF::UpdateFieldFlag flags, ByteBuffer& data, Player const* target) const
{
    // Only ObjectData belongs to the CGObject fragment for MeshObjects.
    // m_meshObjectData is serialized by FMeshObjectData_C fragment's own SerializeCreate handler.
    // m_mirroredPositionData is serialized by FMirroredPositionData_C fragment.
    // m_housingFixtureData is serialized by FHousingFixture_C fragment.
    m_objectData->WriteCreate(flags, data, target, this);
}

void MeshObject::BuildValuesUpdate(UF::UpdateFieldFlag flags, ByteBuffer& data, Player const* target) const
{
    // GetChangedObjectTypeMask() only contains CGObject-owned fields (TYPEID_OBJECT).
    // m_meshObjectData (FMeshObjectData_C) changes are handled by its own fragment's SerializeUpdate.
    data << uint32(m_values.GetChangedObjectTypeMask());

    if (m_values.HasChanged(TYPEID_OBJECT))
        m_objectData->WriteUpdate(flags, data, target, this);
}

void MeshObject::ClearValuesChangesMask()
{
    m_values.ClearChangesMask(&MeshObject::m_meshObjectData);
    m_values.ClearChangesMask(&MeshObject::m_mirroredPositionData);
    WorldObject::ClearValuesChangesMask();
}
