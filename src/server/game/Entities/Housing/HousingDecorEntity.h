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

#ifndef TRINITYCORE_HOUSING_DECOR_ENTITY_H
#define TRINITYCORE_HOUSING_DECOR_ENTITY_H

#include "Object.h"
#include "GridObject.h"
#include "MapObject.h"

// Lightweight decor entity matching retail Object Type 18 entities with FHousingDecor_C.
// Used for interior door decor: a non-visual data entity that the clickable GO attaches to
// via TransportGUID. Retail sniff fragment list: [FHousingDecor_C, FMirroredPositionData_C].
// NO CGObject, NO FMeshObjectData_C, NO Tag_MeshObject — purely a data carrier.
//
// The GO (entry 575017/587318) attaches to this entity with PositionLocalSpace=(0,0,0)
// and AttachmentFlags=7, making the GO appear at the decor entity's world position.
class TC_GAME_API HousingDecorEntity final : public WorldObject, public GridObject<HousingDecorEntity>, public MapObject
{
public:
    explicit HousingDecorEntity();

    void AddToWorld() override;
    void RemoveFromWorld() override;

    bool Create(ObjectGuid guid, Map* map, Position const& pos);

    // Pure virtual overrides from WorldObject
    ObjectGuid GetCreatorGUID() const override { return ObjectGuid::Empty; }
    ObjectGuid GetOwnerGUID() const override { return ObjectGuid::Empty; }
    uint32 GetFaction() const override { return 0; }

    void BuildCreateUpdateBlockForPlayer(UpdateData* data, Player* target) const override;
    void BuildValuesCreate(UF::UpdateFieldFlag flags, ByteBuffer& data, Player const* target) const override;
    void BuildValuesUpdate(UF::UpdateFieldFlag flags, ByteBuffer& data, Player const* target) const override;
    std::string GetNameForLocaleIdx(LocaleConstant locale) const override;
    std::string GetDebugInfo() const override;

    // Decor data setters — call before AddToMap
    void SetDecorGUID(ObjectGuid decorGuid);
    void SetAttachParentGUID(ObjectGuid roomEntityGuid);
    void SetTargetGameObjectGUID(ObjectGuid goGuid);
    void SetFlags(uint8 flags);
    void SetPersistedData(ObjectGuid houseGuid, uint8 sourceType = 0, std::string sourceValue = {});

    // Mirrored position data setters
    void SetMirroredPosition(Position const& pos, QuaternionData const& rot, float scale,
        ObjectGuid attachParent = ObjectGuid::Empty, uint8 attachFlags = 3);

    ObjectGuid GetDecorGUID() const { return _decorGuid; }

    UF::UpdateField<UF::HousingDecorData, int32(WowCS::EntityFragment::FHousingDecor_C), 0> m_housingDecorData;
    UF::UpdateField<UF::MirroredPositionData, int32(WowCS::EntityFragment::FMirroredPositionData_C), 0> m_mirroredPositionData;

protected:
    UF::UpdateFieldFlag GetUpdateFieldFlagsFor(Player const* target) const override;
    void ClearValuesChangesMask() override;
    bool AddToObjectUpdate() override;
    void RemoveFromObjectUpdate() override;

private:
    ObjectGuid _decorGuid;
};

#endif // TRINITYCORE_HOUSING_DECOR_ENTITY_H
