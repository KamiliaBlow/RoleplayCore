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

#include "BaseEntity.h"
#include "Position.h"

class Map;

// Room entity matching retail Housing/18 GUID (subType=2, arg2=18).
// ObjectType 18. Fragments: FHousingRoom_C + FMirroredPositionData_C + Tag_HousingRoom.
// NOT a MeshObject — no CGObject, no FMeshObjectData_C, no Tag_MeshObject.
// Has stationary position in the movement block.
class TC_GAME_API HousingRoomEntity final : public BaseEntity
{
public:
    explicit HousingRoomEntity(Map* map, ObjectGuid guid, Position const& pos);

    void ClearUpdateMask(bool remove) override;
    std::string GetNameForLocaleIdx(LocaleConstant locale) const override;
    void BuildUpdate(UpdateDataMapType& data_map) override;
    void BuildCreateUpdateBlockForPlayer(UpdateData* data, Player* target) const override;
    std::string GetDebugInfo() const override;

    // Room data setters
    void SetHouseGUID(ObjectGuid houseGuid);
    void SetHouseRoomID(int32 roomId);
    void SetFlags(int32 flags);
    void SetFloorIndex(int32 floorIndex);
    void AddMeshObject(ObjectGuid meshObjectGuid);

    // Mirrored position data setters
    void SetMirroredPosition(Position const& pos, QuaternionData const& rot, float scale,
        ObjectGuid attachParent = ObjectGuid::Empty, uint8 attachFlags = 3);

    Position const& GetStationaryPosition() const { return _stationaryPosition; }
    ObjectGuid const& GetHouseGUID() const { return _houseGuid; }

    Map* GetMap() const { return _map; }

    UF::UpdateField<UF::HousingRoomData, int32(WowCS::EntityFragment::FHousingRoom_C), 0> m_housingRoomData;
    UF::UpdateField<UF::MirroredPositionData, int32(WowCS::EntityFragment::FMirroredPositionData_C), 0> m_mirroredPositionData;

protected:
    UF::UpdateFieldFlag GetUpdateFieldFlagsFor(Player const* target) const override;
    bool AddToObjectUpdate() override;
    void RemoveFromObjectUpdate() override;

private:
    Map* _map;
    Position _stationaryPosition;
    ObjectGuid _houseGuid;
};

#endif // TRINITYCORE_HOUSING_ROOM_ENTITY_H
