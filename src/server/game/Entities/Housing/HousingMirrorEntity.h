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

#ifndef TRINITYCORE_HOUSING_MIRROR_ENTITY_H
#define TRINITYCORE_HOUSING_MIRROR_ENTITY_H

#include "BaseEntity.h"
#include "Position.h"
#include "QuaternionData.h"

class Map;

// Position-carrier entity paired with housing MeshObjects / player houses.
class TC_GAME_API HousingMirrorEntity final : public BaseEntity
{
public:
    HousingMirrorEntity(Map* map, ObjectGuid guid);
    ~HousingMirrorEntity();

    // which Tag_HouseExterior* fragments InitPositionData attaches
    enum class Tagging : uint8
    {
        None,
        Piece,
        PieceAndRoot,
    };

    // position/rotation/scale are local-space to attachParent
    void InitPositionData(ObjectGuid attachParent,
        Position const& position, QuaternionData const& rotation,
        float scale, uint8 attachmentFlags, Tagging tagging);

    void ClearUpdateMask(bool remove) override;
    std::string GetNameForLocaleIdx(LocaleConstant locale) const override;
    void BuildUpdate(UpdateDataMapType& data_map) override;
    std::string GetDebugInfo() const override;

    UF::UpdateField<UF::MirroredPositionData, int32(WowCS::EntityFragment::FMirroredPositionData_C), 0> m_mirroredPositionData;

protected:
    UF::UpdateFieldFlag GetUpdateFieldFlagsFor(Player const* target) const override;
    bool AddToObjectUpdate() override;
    void RemoveFromObjectUpdate() override;
};

#endif // TRINITYCORE_HOUSING_MIRROR_ENTITY_H
