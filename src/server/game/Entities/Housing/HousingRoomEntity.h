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

#ifndef TRINITYCORE_HOUSING_ROOM_ENTITY_H
#define TRINITYCORE_HOUSING_ROOM_ENTITY_H

#include "Object.h"
#include "GridObject.h"
#include "MapObject.h"

// Room entity matching retail Housing/2 GUID (subType=2, arg2=HouseRoomID).
// objectType=18. Fragments: FHousingRoom_C + FMirroredPositionData_C + Tag_HousingRoom.
// Inherits from WorldObject so it can be AddToMap'd and sent via the visibility
// system in the SAME UPDATE_OBJECT as room MeshObjects — matching retail behavior.
// Has Stationary position in the movement block (sniff-verified: COB 0x81 0x00 0x00).
class TC_GAME_API HousingRoomEntity final : public WorldObject, public GridObject<HousingRoomEntity>, public MapObject
{
public:
    explicit HousingRoomEntity();

    void AddToWorld() override;
    void RemoveFromWorld() override;

    bool Create(ObjectGuid guid, Map* map, Position const& pos);

    // Pure virtual overrides from WorldObject
    ObjectGuid GetCreatorGUID() const override { return ObjectGuid::Empty; }
    ObjectGuid GetOwnerGUID() const override { return ObjectGuid::Empty; }
    uint32 GetFaction() const override { return 0; }

    // Override to use entity fragment serialization (like BaseEntity) instead of
    // Object's BuildValuesCreate path which expects CGObject fields.
    void BuildCreateUpdateBlockForPlayer(UpdateData* data, Player* target) const override;
    void BuildValuesCreate(UF::UpdateFieldFlag flags, ByteBuffer& data, Player const* target) const override;
    void BuildValuesUpdate(UF::UpdateFieldFlag flags, ByteBuffer& data, Player const* target) const override;
    std::string GetNameForLocaleIdx(LocaleConstant locale) const override;
    std::string GetDebugInfo() const override;

    // Room data setters
    void SetHouseGUID(ObjectGuid houseGuid);
    void SetHouseRoomID(int32 roomId);
    void SetFlags(int32 flags);
    void SetFloorIndex(int32 floorIndex);
    void AddMeshObject(ObjectGuid meshObjectGuid);
    void ReplaceMeshObjects(std::vector<ObjectGuid> const& newGuids);
    void AddDoor(int32 roomComponentID, Position const& offset, uint8 connectionType, ObjectGuid attachedRoomGuid = ObjectGuid::Empty);
    bool UpdateDoorConnection(int32 roomComponentID, ObjectGuid attachedRoomGuid);

    // Mirrored position data setters
    void SetMirroredPosition(Position const& pos, QuaternionData const& rot, float scale,
        ObjectGuid attachParent = ObjectGuid::Empty, uint8 attachFlags = 3);

    UF::UpdateField<UF::HousingRoomData, int32(WowCS::EntityFragment::FHousingRoom_C), 0> m_housingRoomData;
    UF::UpdateField<UF::MirroredPositionData, int32(WowCS::EntityFragment::FMirroredPositionData_C), 0> m_mirroredPositionData;

protected:
    UF::UpdateFieldFlag GetUpdateFieldFlagsFor(Player const* target) const override;
    void ClearValuesChangesMask() override;
    bool AddToObjectUpdate() override;
    void RemoveFromObjectUpdate() override;
};

#endif // TRINITYCORE_HOUSING_ROOM_ENTITY_H
