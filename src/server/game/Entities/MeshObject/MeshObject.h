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

#ifndef TRINITYCORE_MESHOBJECT_H
#define TRINITYCORE_MESHOBJECT_H

#include "Object.h"
#include "GridObject.h"
#include "MapObject.h"

class TC_GAME_API MeshObject final : public WorldObject, public GridObject<MeshObject>, public MapObject
{
public:
    MeshObject();
    ~MeshObject();

    void AddToWorld() override;
    void RemoveFromWorld() override;
    void Update(uint32 diff) override;

    ObjectGuid GetCreatorGUID() const override { return ObjectGuid::Empty; }
    ObjectGuid GetOwnerGUID() const override { return ObjectGuid::Empty; }
    uint32 GetFaction() const override { return 0; }
    std::string GetNameForLocaleIdx(LocaleConstant /*locale*/) const override { return "MeshObject"; }

    // pos: local-space; worldPos: grid placement (null = use pos)
    static MeshObject* CreateMeshObject(Map* map, Position const& pos,
        QuaternionData const& rotation, float scale,
        int32 fileDataID, bool isWMO,
        ObjectGuid attachParent = ObjectGuid::Empty, uint8 attachFlags = 0,
        Position const* worldPos = nullptr);

    int32 GetFileDataID() const { return m_meshObjectData->FileDataID; }
    ObjectGuid const& GetAttachParentGUID() const { return _attachParentGUID; }
    void SetAttachParentGUID(ObjectGuid attachParent);
    QuaternionData const& GetLocalRotation() const { return _rotationLocalSpace; }
    Position const& GetLocalPosition() const { return _positionLocalSpace; }
    float GetLocalScale() const { return _scaleLocalSpace; }
    void UpdateLocalScale(float scale);
    // local-space, relative to the attach parent
    void UpdateLocalTransform(Position const& pos, QuaternionData const& rotation, float scale);
    uint8 GetAttachmentFlags() const { return _attachmentFlags; }
    bool IsExteriorRoot() const { return _isExteriorRoot; }
    int32 GetExteriorComponentHookID() const { return _exteriorComponentHookID; }
    int32 GetExteriorComponentID() const { return _exteriorComponentID; }
    void UpdateExteriorComponentID(int32 id);

    // Room/Decor data serialized in the movement block (BaseEntity::BuildMovementUpdate)
    ObjectGuid const& GetRoomHouseGUID() const { return _roomHouseGUID; }
    ObjectGuid const& GetDecorRoomEntityGUID() const { return _decorRoomEntityGUID; }

    // isRoot only marks the base for server-side lookups; the root tag belongs to the root entity
    void InitHousingFixtureData(ObjectGuid houseGuid, ObjectGuid fixtureGuid,
        ObjectGuid parentFixtureGuid, int32 exteriorComponentID,
        int32 houseExteriorWmoDataID, uint8 exteriorComponentType = 9,
        uint8 houseSize = 2, int32 exteriorComponentHookID = -1, bool isRoot = false);
    ObjectGuid const& GetFixtureGuid() const { return _fixtureGuid; }

    void InitHousingDecorData(ObjectGuid decorGuid, ObjectGuid houseGuid, uint8 flags,
        ObjectGuid roomEntityGuid = ObjectGuid::Empty, uint8 sourceType = 0, std::string sourceValue = {});

    // Housing room data entity identifying the plot's room type to the client.
    void InitHousingRoomData(ObjectGuid houseGuid, int32 houseRoomID, int32 flags, int32 floorIndex);

    // Requires InitHousingRoomData() first.
    void AddRoomMeshObject(ObjectGuid meshObjectGuid);

    // Requires InitHousingRoomData() first.
    void AddRoomDoor(int32 roomComponentID, Position const& offset, uint8 roomComponentType, ObjectGuid attachedRoomGuid);

    // sets IsRoom + Geobox for the client's OutsidePlotBounds check
    void InitHousingRoomComponentData(ObjectGuid roomGuid,
        int32 roomComponentOptionID, int32 roomComponentID,
        uint8 roomComponentType, int32 field24, uint8 field20,
        int32 houseThemeID, int32 roomComponentTextureID,
        int32 roomComponentTypeParam,
        float geoboxMinX, float geoboxMinY, float geoboxMinZ,
        float geoboxMaxX, float geoboxMaxY, float geoboxMaxZ);

    // client expects an UPDATE_OBJECT, not destroy+create
    void UpdateRoomComponentVisuals(int32 roomComponentOptionID, int32 houseThemeID,
        int32 roomComponentTextureID, int32 roomComponentTypeParam = -1);

    int32 GetRoomComponentID() const;
    int32 GetRoomComponentOptionID() const;
    int32 GetHouseThemeID() const;
    int32 GetRoomComponentTextureID() const;

    UF::UpdateField<UF::MeshObjectData, int32(WowCS::EntityFragment::FMeshObjectData_C), TYPEID_MESH_OBJECT> m_meshObjectData;
    UF::UpdateField<UF::MirroredPositionData, int32(WowCS::EntityFragment::FMirroredPositionData_C), 0> m_mirroredPositionData;

    void BuildCreateUpdateBlockForPlayer(UpdateData* data, Player* target) const override;

protected:
    void BuildValuesCreate(UF::UpdateFieldFlag flags, ByteBuffer& data, Player const* target) const override;
    void BuildValuesUpdate(UF::UpdateFieldFlag flags, ByteBuffer& data, Player const* target) const override;
    void ClearValuesChangesMask() override;

private:
    bool Create(Map* map, Position const& pos, QuaternionData const& rotation,
        float scale, int32 fileDataID, bool isWMO,
        ObjectGuid attachParent, uint8 attachFlags,
        Position const* worldPos);

    // Movement block data (serialized by BaseEntity::BuildMovementUpdate)
    ObjectGuid _attachParentGUID;
    Position _positionLocalSpace;   // local-space offset from parent
    QuaternionData _rotationLocalSpace;
    float _scaleLocalSpace = 1.0f;
    uint8 _attachmentFlags = 0;

    // written when the corresponding CreateObjectBits are set (not part of the FHousing* fragments)
    ObjectGuid _roomHouseGUID;
    ObjectGuid _decorRoomEntityGUID;
    bool _isExteriorRoot = false;
    int32 _exteriorComponentHookID = -1;
    int32 _exteriorComponentID = 0;
    ObjectGuid _fixtureGuid;
};

#endif // TRINITYCORE_MESHOBJECT_H
