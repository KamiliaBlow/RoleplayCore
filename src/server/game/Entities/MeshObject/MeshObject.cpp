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
#include "DB2Stores.h"
#include "Log.h"
#include "Map.h"
#include "ObjectGuid.h"
#include "PhasingHandler.h"
#include "UpdateData.h"

MeshObject::MeshObject() : WorldObject(false), MapObject()
{
    m_objectTypeId = TYPEID_MESH_OBJECT;

    // Stationary would shift client parsing of the MeshObject block
    m_updateFlag.Stationary = false;
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

    // pos is the local-space offset; grid placement uses the parent's world position
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

    // always visible so phase changes on plot exit don't hide meshes (incl. neighbour houses)
    PhasingHandler::InitDbPhaseShift(GetPhaseShift(), PHASE_USE_FLAGS_ALWAYS_VISIBLE, 0, 0);

    _Create(ObjectGuid::Create<HighGuid::MeshObject>(GetMapId(), 0,
        GetMap()->GenerateLowGuid<HighGuid::MeshObject>()));

    SetObjectScale(1.0f);

    // client looks up placed decor by EntryID
    SetEntry(fileDataID);

    auto meshData = m_values.ModifyValue(&MeshObject::m_meshObjectData);
    SetUpdateFieldValue(meshData.ModifyValue(&UF::MeshObjectData::FileDataID), fileDataID);
    SetUpdateFieldValue(meshData.ModifyValue(&UF::MeshObjectData::IsWMO), isWMO);
    SetUpdateFieldValue(meshData.ModifyValue(&UF::MeshObjectData::IsRoom), false);

    m_entityFragments.Add(WowCS::EntityFragment::FMeshObjectData_C, false,
        WowCS::GetRawFragmentData(m_meshObjectData));

    _attachParentGUID = attachParent;
    _positionLocalSpace = pos;
    _rotationLocalSpace = rotation;
    _scaleLocalSpace = scale;
    _attachmentFlags = attachFlags;

    auto posData = m_values.ModifyValue(&MeshObject::m_mirroredPositionData)
        .ModifyValue(&UF::MirroredPositionData::PositionData);
    SetUpdateFieldValue(posData.ModifyValue(&UF::MirroredMeshObjectData::AttachParentGUID), attachParent);
    SetUpdateFieldValue(posData.ModifyValue(&UF::MirroredMeshObjectData::PositionLocalSpace),
        TaggedPosition<Position::XYZ>(pos.GetPositionX(), pos.GetPositionY(), pos.GetPositionZ()));
    SetUpdateFieldValue(posData.ModifyValue(&UF::MirroredMeshObjectData::RotationLocalSpace), rotation);
    SetUpdateFieldValue(posData.ModifyValue(&UF::MirroredMeshObjectData::ScaleLocalSpace), scale);
    SetUpdateFieldValue(posData.ModifyValue(&UF::MirroredMeshObjectData::AttachmentFlags), attachFlags);

    m_entityFragments.Add(WowCS::EntityFragment::FMirroredPositionData_C, false,
        WowCS::GetRawFragmentData(m_mirroredPositionData));

    SetZoneScript();
    UpdatePositionData();

    // NOTE: caller must AddToMap after all InitHousing* calls (the create packet is sent there)

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

    auto decorData = m_values.ModifyValue(&Object::m_housingDecorData, 0);
    SetUpdateFieldValue(decorData.ModifyValue(&UF::HousingDecorData::DecorGUID), decorGuid);
    SetUpdateFieldValue(decorData.ModifyValue(&UF::HousingDecorData::AttachParentGUID), roomEntityGuid);
    SetUpdateFieldValue(decorData.ModifyValue(&UF::HousingDecorData::Flags), flags);
    SetUpdateFieldValue(decorData.ModifyValue(&UF::HousingDecorData::TargetGameObjectGUID), ObjectGuid::Empty);

    auto persistedRef = decorData.ModifyValue(&UF::HousingDecorData::PersistedData, 0);
    SetUpdateFieldValue(persistedRef.ModifyValue(&UF::DecorStoragePersistedData::HouseGUID), houseGuid);
    SetUpdateFieldValue(persistedRef.ModifyValue(&UF::DecorStoragePersistedData::SourceType), sourceType);
    SetUpdateFieldValue(persistedRef.ModifyValue(&UF::DecorStoragePersistedData::SourceValue), std::move(sourceValue));

    m_entityFragments.Add(WowCS::EntityFragment::FHousingDecor_C, IsInWorld(),
        WowCS::GetRawFragmentData(m_housingDecorData));

    // HasDecor movement flag not set; the room entity GUID lives in FHousingDecor_C
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

    auto fixtureData = m_values.ModifyValue(&Object::m_housingFixtureData, 0);
    SetUpdateFieldValue(fixtureData.ModifyValue(&UF::HousingFixtureData::ExteriorComponentID), exteriorComponentID);
    SetUpdateFieldValue(fixtureData.ModifyValue(&UF::HousingFixtureData::HouseExteriorWmoDataID), houseExteriorWmoDataID);
    SetUpdateFieldValue(fixtureData.ModifyValue(&UF::HousingFixtureData::ExteriorComponentHookID), exteriorComponentHookID);
    SetUpdateFieldValue(fixtureData.ModifyValue(&UF::HousingFixtureData::HouseGUID), houseGuid);
    // the client builds its fixture tree from this
    SetUpdateFieldValue(fixtureData.ModifyValue(&UF::HousingFixtureData::AttachParentGUID), parentFixtureGuid);
    // must be a Housing-type GUID - the client's resolver crashes on non-Housing GUIDs
    SetUpdateFieldValue(fixtureData.ModifyValue(&UF::HousingFixtureData::Guid), fixtureGuid);

    // Door components (Type 11) carry the GO entry's GUID; other fixture types stay empty.
    {
        ObjectGuid goGuid = ObjectGuid::Empty;
        if (exteriorComponentID > 0)
        {
            ExteriorComponentEntry const* extComp = sExteriorComponentStore.LookupEntry(
                static_cast<uint32>(exteriorComponentID));
            if (extComp && extComp->GameObjectID > 0)
                // reuse the MeshObject's counter in the GO GUID
                goGuid = ObjectGuid::Create<HighGuid::GameObject>(GetMap()->GetId(),
                    static_cast<uint32>(extComp->GameObjectID), GetGUID().GetCounter());
        }

        if (!goGuid.IsEmpty())
            SetUpdateFieldValue(fixtureData.ModifyValue(&UF::HousingFixtureData::GameObjectGUID), goGuid);
    }

    SetUpdateFieldValue(fixtureData.ModifyValue(&UF::HousingFixtureData::ExteriorComponentType), exteriorComponentType);
    SetUpdateFieldValue(fixtureData.ModifyValue(&UF::HousingFixtureData::Field_59), uint8(1)); // always 1
    SetUpdateFieldValue(fixtureData.ModifyValue(&UF::HousingFixtureData::Size), houseSize);

    m_entityFragments.Add(WowCS::EntityFragment::FHousingFixture_C, IsInWorld(),
        WowCS::GetRawFragmentData(m_housingFixtureData));

    _exteriorComponentHookID = exteriorComponentHookID;
    _exteriorComponentID = exteriorComponentID;
    _fixtureGuid = fixtureGuid;
    _isExteriorRoot = isRoot;

    // Tag_HouseExteriorRoot belongs to the root entity; tagging the base Root breaks house dragging
    m_entityFragments.Add(WowCS::EntityFragment::Tag_HouseExteriorPiece, IsInWorld());

    TC_LOG_DEBUG("housing", "MeshObject::InitHousingFixtureData: meshGuid={} fixtureGuid={} "
        "parentFixtureGuid={} houseGuid={} extCompID={} wmoDataID={} hookID={} type={} size={} isRoot={}",
        GetGUID().ToString(), fixtureGuid.ToString(), parentFixtureGuid.ToString(),
        houseGuid.ToString(), exteriorComponentID, houseExteriorWmoDataID,
        exteriorComponentHookID, exteriorComponentType, houseSize, isRoot);
}

void MeshObject::UpdateLocalScale(float scale)
{
    _scaleLocalSpace = scale;
    auto posData = m_values.ModifyValue(&MeshObject::m_mirroredPositionData)
        .ModifyValue(&UF::MirroredPositionData::PositionData);
    SetUpdateFieldValue(posData.ModifyValue(&UF::MirroredMeshObjectData::ScaleLocalSpace), scale);
}

void MeshObject::UpdateLocalTransform(Position const& pos, QuaternionData const& rotation, float scale)
{
    _positionLocalSpace = pos;
    _rotationLocalSpace = rotation;
    _scaleLocalSpace = scale;
    auto posData = m_values.ModifyValue(&MeshObject::m_mirroredPositionData)
        .ModifyValue(&UF::MirroredPositionData::PositionData);
    SetUpdateFieldValue(posData.ModifyValue(&UF::MirroredMeshObjectData::PositionLocalSpace),
        TaggedPosition<Position::XYZ>(pos.GetPositionX(), pos.GetPositionY(), pos.GetPositionZ()));
    SetUpdateFieldValue(posData.ModifyValue(&UF::MirroredMeshObjectData::RotationLocalSpace), rotation);
    SetUpdateFieldValue(posData.ModifyValue(&UF::MirroredMeshObjectData::ScaleLocalSpace), scale);
}

void MeshObject::SetAttachParentGUID(ObjectGuid attachParent)
{
    _attachParentGUID = attachParent;
    auto posData = m_values.ModifyValue(&MeshObject::m_mirroredPositionData)
        .ModifyValue(&UF::MirroredPositionData::PositionData);
    SetUpdateFieldValue(posData.ModifyValue(&UF::MirroredMeshObjectData::AttachParentGUID), attachParent);
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

    // this MeshObject carries FHousingRoom_C, so the client reads it via MeshObjectSystem
    SetUpdateFieldValue(m_values.ModifyValue(&MeshObject::m_meshObjectData)
        .ModifyValue(&UF::MeshObjectData::IsRoom), true);

    auto roomData = m_values.ModifyValue(&Object::m_housingRoomData, 0);
    SetUpdateFieldValue(roomData.ModifyValue(&UF::HousingRoomData::HouseGUID), houseGuid);
    SetUpdateFieldValue(roomData.ModifyValue(&UF::HousingRoomData::HouseRoomID), houseRoomID);
    SetUpdateFieldValue(roomData.ModifyValue(&UF::HousingRoomData::Flags), flags);

    m_entityFragments.Add(WowCS::EntityFragment::FHousingRoom_C, IsInWorld(),
        WowCS::GetRawFragmentData(m_housingRoomData));
    m_entityFragments.Add(WowCS::EntityFragment::Tag_HousingRoom, IsInWorld());

    // HasRoom movement flag not set; the house GUID lives in FHousingRoom_C
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
    uint8 roomComponentType, int32 field24, uint8 field20,
    int32 houseThemeID, int32 roomComponentTextureID,
    int32 roomComponentTypeParam,
    float geoboxMinX, float geoboxMinY, float geoboxMinZ,
    float geoboxMaxX, float geoboxMaxY, float geoboxMaxZ)
{
    if (m_housingRoomComponentMeshData.has_value())
        return;

    // room component MeshObjects always have IsRoom=true
    auto meshData = m_values.ModifyValue(&MeshObject::m_meshObjectData);
    SetUpdateFieldValue(meshData.ModifyValue(&UF::MeshObjectData::IsRoom), true);

    UF::AaBox geobox;
    geobox.Low = TaggedPosition<Position::XYZ>(geoboxMinX, geoboxMinY, geoboxMinZ);
    geobox.High = TaggedPosition<Position::XYZ>(geoboxMaxX, geoboxMaxY, geoboxMaxZ);
    SetUpdateFieldValue(meshData.ModifyValue(&UF::MeshObjectData::Geobox, uint32(0)), std::move(geobox));

    auto compData = m_values.ModifyValue(&Object::m_housingRoomComponentMeshData, 0);
    SetUpdateFieldValue(compData.ModifyValue(&UF::HousingRoomComponentMeshData::RoomGUID), roomGuid);
    SetUpdateFieldValue(compData.ModifyValue(&UF::HousingRoomComponentMeshData::RoomComponentOptionID), roomComponentOptionID);
    SetUpdateFieldValue(compData.ModifyValue(&UF::HousingRoomComponentMeshData::RoomComponentID), roomComponentID);
    SetUpdateFieldValue(compData.ModifyValue(&UF::HousingRoomComponentMeshData::Field_20), field20);
    SetUpdateFieldValue(compData.ModifyValue(&UF::HousingRoomComponentMeshData::RoomComponentType), roomComponentType);
    SetUpdateFieldValue(compData.ModifyValue(&UF::HousingRoomComponentMeshData::Field_24), field24);
    SetUpdateFieldValue(compData.ModifyValue(&UF::HousingRoomComponentMeshData::HouseThemeID), houseThemeID);
    SetUpdateFieldValue(compData.ModifyValue(&UF::HousingRoomComponentMeshData::RoomComponentTextureID), roomComponentTextureID);
    SetUpdateFieldValue(compData.ModifyValue(&UF::HousingRoomComponentMeshData::RoomComponentTypeParam), roomComponentTypeParam);

    m_entityFragments.Add(WowCS::EntityFragment::FHousingRoomComponentMesh_C, IsInWorld(),
        WowCS::GetRawFragmentData(m_housingRoomComponentMeshData));
}

void MeshObject::UpdateRoomComponentVisuals(int32 roomComponentOptionID, int32 houseThemeID,
    int32 roomComponentTextureID, int32 roomComponentTypeParam /*= -1*/)
{
    if (!m_housingRoomComponentMeshData.has_value())
        return;

    auto compData = m_values.ModifyValue(&Object::m_housingRoomComponentMeshData, 0);
    SetUpdateFieldValue(compData.ModifyValue(&UF::HousingRoomComponentMeshData::RoomComponentOptionID), roomComponentOptionID);
    SetUpdateFieldValue(compData.ModifyValue(&UF::HousingRoomComponentMeshData::HouseThemeID), houseThemeID);
    SetUpdateFieldValue(compData.ModifyValue(&UF::HousingRoomComponentMeshData::RoomComponentTextureID), roomComponentTextureID);
    if (roomComponentTypeParam >= 0)
        SetUpdateFieldValue(compData.ModifyValue(&UF::HousingRoomComponentMeshData::RoomComponentTypeParam), roomComponentTypeParam);

    TC_LOG_DEBUG("housing", "MeshObject::UpdateRoomComponentVisuals: guid={} optionID={} themeID={} textureID={} typeParam={}",
        GetGUID().ToString(), roomComponentOptionID, houseThemeID, roomComponentTextureID, roomComponentTypeParam);
}

int32 MeshObject::GetRoomComponentID() const
{
    if (!m_housingRoomComponentMeshData.has_value())
        return 0;
    return m_housingRoomComponentMeshData->RoomComponentID;
}

int32 MeshObject::GetRoomComponentOptionID() const
{
    if (!m_housingRoomComponentMeshData.has_value())
        return 0;
    return m_housingRoomComponentMeshData->RoomComponentOptionID;
}

int32 MeshObject::GetHouseThemeID() const
{
    if (!m_housingRoomComponentMeshData.has_value())
        return 0;
    return m_housingRoomComponentMeshData->HouseThemeID;
}

int32 MeshObject::GetRoomComponentTextureID() const
{
    if (!m_housingRoomComponentMeshData.has_value())
        return 0;
    return m_housingRoomComponentMeshData->RoomComponentTextureID;
}

void MeshObject::BuildCreateUpdateBlockForPlayer(UpdateData* data, Player* target) const
{
    if (!target)
        return;

    // the client may not handle CreateObject2 for entity-fragment types
    CreateObjectBits flags = m_updateFlag;

    ByteBuffer& buf = data->GetBuffer();
    buf << uint8(UPDATETYPE_CREATE_OBJECT);
    buf << GetGUID();
    buf << uint8(m_objectTypeId);

    BuildMovementUpdate(buf, flags, target);

    UF::UpdateFieldFlag fieldFlags = GetUpdateFieldFlagsFor(target);
    std::size_t sizePos = buf.wpos();
    buf << uint32(0);
    buf << uint8(fieldFlags);
    BuildEntityFragments(buf, m_entityFragments.GetIds());

    for (std::size_t i = 0; i < m_entityFragments.UpdateableCount; ++i)
    {
        WowCS::EntityFragment fragmentId = m_entityFragments.Updateable.Ids[i];
        if (WowCS::IsIndirectFragment(fragmentId))
            buf << uint8(1);

        WowCS::EntityFragmentInfo->SerializeCreate[static_cast<std::size_t>(fragmentId)](
            m_entityFragments.Updateable.Data[i], fieldFlags, buf, target, this);
    }

    buf.put<uint32>(sizePos, buf.wpos() - sizePos - 4);
    data->AddUpdateBlock();
}

void MeshObject::BuildValuesCreate(UF::UpdateFieldFlag flags, ByteBuffer& data, Player const* target) const
{
    // only ObjectData belongs to CGObject; other fields go via their entity fragments
    m_objectData->WriteCreate(flags, data, target, this);
}

void MeshObject::BuildValuesUpdate(UF::UpdateFieldFlag flags, ByteBuffer& data, Player const* target) const
{
    // entity fragment data is handled separately via SerializeUpdate
    data << uint32(m_values.GetChangedObjectTypeMask());

    if (m_values.HasChanged(TYPEID_OBJECT))
        m_objectData->WriteUpdate(flags, data, target, this);
}

void MeshObject::ClearValuesChangesMask()
{
    m_values.ClearChangesMask(&MeshObject::m_meshObjectData);
    m_values.ClearChangesMask(&MeshObject::m_mirroredPositionData);
    m_values.ClearChangesMask(&Object::m_housingRoomComponentMeshData);
    m_values.ClearChangesMask(&Object::m_housingDecorData);
    m_values.ClearChangesMask(&Object::m_housingFixtureData);
    WorldObject::ClearValuesChangesMask();
}
