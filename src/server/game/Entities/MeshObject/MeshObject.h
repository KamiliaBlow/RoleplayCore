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

    // Pure virtual overrides
    ObjectGuid GetCreatorGUID() const override { return ObjectGuid::Empty; }
    ObjectGuid GetOwnerGUID() const override { return ObjectGuid::Empty; }
    uint32 GetFaction() const override { return 0; }
    std::string GetNameForLocaleIdx(LocaleConstant /*locale*/) const override { return "MeshObject"; }

    // Factory
    // pos: local-space position (stored in FMirroredPositionData_C for client rendering)
    // worldPos: if non-null, used for server-side grid placement (for child pieces attached to a parent).
    //           If null, pos is used for both grid placement and local-space data.
    static MeshObject* CreateMeshObject(Map* map, Position const& pos,
        QuaternionData const& rotation, float scale,
        int32 fileDataID, bool isWMO,
        ObjectGuid attachParent = ObjectGuid::Empty, uint8 attachFlags = 0,
        Position const* worldPos = nullptr);

    // Accessors
    int32 GetFileDataID() const { return m_meshObjectData->FileDataID; }
    ObjectGuid const& GetAttachParentGUID() const { return _attachParentGUID; }
    QuaternionData const& GetLocalRotation() const { return _rotationLocalSpace; }
    Position const& GetLocalPosition() const { return _positionLocalSpace; }
    float GetLocalScale() const { return _scaleLocalSpace; }
    uint8 GetAttachmentFlags() const { return _attachmentFlags; }
    bool IsExteriorRoot() const { return _isExteriorRoot; }
    int32 GetExteriorComponentHookID() const { return _exteriorComponentHookID; }
    int32 GetExteriorComponentID() const { return _exteriorComponentID; }
    void UpdateExteriorComponentID(int32 id);

    // Movement block Room/Decor data accessors (used by BaseEntity::BuildMovementUpdate)
    ObjectGuid const& GetRoomHouseGUID() const { return _roomHouseGUID; }
    ObjectGuid const& GetDecorRoomEntityGUID() const { return _decorRoomEntityGUID; }

    // Housing fixture
    // isRoot: true for root pieces (no parent attachment), false for child pieces.
    // Root pieces get Tag_HouseExteriorRoot (225), children get Tag_HouseExteriorPiece (224).
    void InitHousingFixtureData(ObjectGuid houseGuid, ObjectGuid fixtureGuid,
        ObjectGuid parentFixtureGuid, int32 exteriorComponentID,
        int32 houseExteriorWmoDataID, uint8 exteriorComponentType = 9,
        uint8 houseSize = 2, int32 exteriorComponentHookID = -1, bool isRoot = false);
    ObjectGuid const& GetFixtureGuid() const { return _fixtureGuid; }

    // Housing decor (adds FHousingDecor_C entity fragment for placed decor items)
    // Sniff-verified: retail decor is ALWAYS MeshObject (never GO). TargetGameObjectGUID=empty.
    // roomEntityGuid: the Housing/18 room entity this decor is attached to.
    void InitHousingDecorData(ObjectGuid decorGuid, ObjectGuid houseGuid, uint8 flags,
        ObjectGuid roomEntityGuid = ObjectGuid::Empty, uint8 sourceType = 0, std::string sourceValue = {});

    // Housing room entity (adds FHousingRoom_C + Tag_HousingRoom entity fragments)
    // Creates a room data entity that the client uses to identify the plot's room type.
    void InitHousingRoomData(ObjectGuid houseGuid, int32 houseRoomID, int32 flags, int32 floorIndex);

    // Add a mesh object GUID to the room's MeshObjects dynamic array.
    // Must call InitHousingRoomData() first.
    void AddRoomMeshObject(ObjectGuid meshObjectGuid);

    // Add a door/hookpoint entry to the room's Doors dynamic array.
    // Must call InitHousingRoomData() first. The client's HousingRoomSystem
    // reads these entries to create fixture-hookpoint links.
    void AddRoomDoor(int32 roomComponentID, Position const& offset, uint8 roomComponentType, ObjectGuid attachedRoomGuid);

    // Housing room component mesh (adds FHousingRoomComponentMesh_C fragment, sets IsRoom + Geobox)
    // The client uses the Geobox for its OutsidePlotBounds collision check.
    void InitHousingRoomComponentData(ObjectGuid roomGuid,
        int32 roomComponentOptionID, int32 roomComponentID,
        uint8 roomComponentType, int32 field24,
        int32 houseThemeID, int32 roomComponentTextureID,
        int32 roomComponentTypeParam,
        float geoboxMinX, float geoboxMinY, float geoboxMinZ,
        float geoboxMaxX, float geoboxMaxY, float geoboxMaxZ);

    // Update fields
    UF::UpdateField<UF::MeshObjectData, int32(WowCS::EntityFragment::FMeshObjectData_C), TYPEID_MESH_OBJECT> m_meshObjectData;
    UF::UpdateField<UF::MirroredPositionData, int32(WowCS::EntityFragment::FMirroredPositionData_C), 0> m_mirroredPositionData;

protected:
    void BuildValuesCreate(UF::UpdateFieldFlag flags, ByteBuffer& data, Player const* target) const override;
    void BuildValuesUpdate(UF::UpdateFieldFlag flags, ByteBuffer& data, Player const* target) const override;
    void ClearValuesChangesMask() override;

private:
    bool Create(Map* map, Position const& pos, QuaternionData const& rotation,
        float scale, int32 fileDataID, bool isWMO,
        ObjectGuid attachParent, uint8 attachFlags,
        Position const* worldPos);

    // Movement block data (serialized in BaseEntity::BuildCreateUpdateBlockMovement)
    ObjectGuid _attachParentGUID;
    Position _positionLocalSpace;   // local-space offset from parent (for movement block MeshObject section)
    QuaternionData _rotationLocalSpace;
    float _scaleLocalSpace = 1.0f;
    uint8 _attachmentFlags = 0;

    // Movement block Room/Decor data (bits 54/55 in CreateObjectBits)
    // Room: HouseGUID written when m_updateFlag.Room is set (room entities)
    // Decor: RoomEntityGUID written when m_updateFlag.Decor is set (decor entities)
    ObjectGuid _roomHouseGUID;
    ObjectGuid _decorRoomEntityGUID;
    bool _isExteriorRoot = false;
    int32 _exteriorComponentHookID = -1;
    int32 _exteriorComponentID = 0;
    ObjectGuid _fixtureGuid;
};

#endif // TRINITYCORE_MESHOBJECT_H
