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

// Lightweight position-carrier entity paired with a housing MeshObject or
// HousingPlayerHouse. Retail emits these as HighGuid::Entity (57) with
// objectType=18 and a single FMirroredPositionData_C fragment (plus tags).
// Their role in the client is to place the plot/house in map-space spatial
// indices used by the world-map icon picker and other rendering paths.
//
// Sniff-verified at dump_12.0.1.66838_2026-04-15_09-35-59 idx 9984:
//   - Fragments: FMirroredPositionData_C [+ Tag_HouseExteriorPiece / Root]
//   - movement block: 3 CreateObjectBits flag bytes + uint32 PauseTimes=0
//   - valuesSize: 44 when AttachParent is HighGuid::Housing (short packed GUID)
//   - No CGObject, no FMeshObjectData_C — pure mirror
class TC_GAME_API HousingMirrorEntity final : public BaseEntity
{
public:
    HousingMirrorEntity(Map* map, ObjectGuid guid);
    ~HousingMirrorEntity();

    // Tag set this mirror carries — controls which entity-fragment Tag_*
    // entries are added when InitPositionData is called.
    //   None         — Group B per-piece mesh anchor (no tags)
    //   Piece        — Group A non-root piece (Tag_HouseExteriorPiece only)
    //   PieceAndRoot — Group A root piece (Tag_HouseExteriorPiece +
    //                  Tag_HouseExteriorRoot)
    enum class Tagging : uint8
    {
        None,
        Piece,
        PieceAndRoot,
    };

    // Populate the mirror's position fragment.
    //   attachParent: the housing entity this mirror is attached to
    //                 (room identity for Group A, MeshObject for Group B)
    //   position/rotation/scale: local-space to attachParent
    //   attachmentFlags: usually 3 (sniff-typical)
    //   tagging: which Tag_HouseExterior* fragments to attach (see enum above)
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

private:
    Map* _map;
};

#endif // TRINITYCORE_HOUSING_MIRROR_ENTITY_H
