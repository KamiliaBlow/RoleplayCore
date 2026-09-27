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

#ifndef TRINITYCORE_HOUSING_BLUEPRINT_MGR_H
#define TRINITYCORE_HOUSING_BLUEPRINT_MGR_H

#include "Define.h"
#include "HousingBlueprintPackets.h"
#include "HousingDefines.h"
#include "ObjectGuid.h"
#include <array>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

class Housing;
class Player;

// A saved layout. Decor is stored relative to its room (interior) or to the plot (exterior), so a blueprint lands the same
// way in any house.
struct HousingBlueprintRoom
{
    uint32 RoomEntryId = 0;
    int32 GridX = 0;
    int32 GridY = 0;
    int32 FloorIndex = 0;
    uint32 Orientation = 0;
    bool Mirrored = false;
    uint32 ThemeId = 0;
    uint32 WallThemeId = 0;
    uint32 FloorThemeId = 0;
    uint32 CeilingThemeId = 0;
    uint32 WallTextureId = 0;
    uint32 FloorTextureId = 0;
    uint32 CeilingTextureId = 0;
    int32 ColorOverride = -1;
    uint32 DoorTypeId = 0;
    uint8 DoorSlot = 0;
    uint32 CeilingTypeId = 0;
    uint8 CeilingSlot = 0;
};

struct HousingBlueprintDecor
{
    static constexpr int32 EXTERIOR = -1;

    uint32 DecorEntryId = 0;
    int32 RoomIndex = EXTERIOR;     // index into HousingBlueprintContent::Rooms
    float PosX = 0.0f;              // local to the room or the plot
    float PosY = 0.0f;
    float PosZ = 0.0f;
    float RotX = 0.0f;
    float RotY = 0.0f;
    float RotZ = 0.0f;
    float RotW = 1.0f;
    float Scale = 1.0f;
    std::array<uint32, MAX_HOUSING_DYE_SLOTS> DyeSlots = {};
};

struct HousingBlueprintFixture
{
    uint32 FixturePointId = 0;
    uint32 OptionId = 0;
};

struct HousingBlueprintContent
{
    uint32 HouseType = 0;
    uint8 HouseSize = 0;
    bool HasHousePosition = false;
    float HousePosX = 0.0f;         // local to the plot
    float HousePosY = 0.0f;
    float HousePosZ = 0.0f;
    float HouseFacing = 0.0f;
    std::vector<HousingBlueprintFixture> Fixtures;
    std::vector<HousingBlueprintRoom> Rooms;
    std::vector<HousingBlueprintDecor> Decor;
};

struct HousingBlueprint
{
    uint64 Id = 0;
    std::string Uuid;
    uint32 BnetAccountId = 0;
    ObjectGuid::LowType ExporterGuid = 0;
    std::string Name;
    HousingBlueprintType Type = HousingBlueprintType::None;
    uint8 Flags = HOUSING_BLUEPRINT_FLAG_NONE;
    int64 CreateTime = 0;
    HousingBlueprintContent Content;

    bool IsAutomaticBackup() const { return (Flags & HOUSING_BLUEPRINT_FLAG_AUTOMATIC_BACKUP) != 0; }
};

// What importing a blueprint into a house needs and lacks - SMSG_HOUSING_BLUEPRINT_CONTENTS.
struct HousingBlueprintEvaluation
{
    uint32 UnmetRequirementFlags = 0;
    WorldPackets::Housing::JamBlueprintContentLists Totals;
    WorldPackets::Housing::JamBlueprintContentLists Missing;
    WorldPackets::Housing::JamBlueprintContentLists Invalid;
    std::vector<WorldPackets::Housing::JamBlueprintBudgetEntry> InteriorBudgets;
    std::vector<WorldPackets::Housing::JamBlueprintBudgetEntry> ExteriorBudgets;

    bool IsBlocked() const { return (UnmetRequirementFlags & HOUSING_BLUEPRINT_UNMET_BLOCKING_MASK) != 0; }
};

// Result of applying a blueprint to a house's model: what the maps have to despawn and rebuild.
struct HousingBlueprintApplyResult
{
    std::vector<ObjectGuid> RemovedDecor;
    bool InteriorChanged = false;
    bool ExteriorChanged = false;
    uint32 PlacedDecor = 0;
    uint32 SkippedDecor = 0;
};

class TC_GAME_API HousingBlueprintMgr
{
public:
    HousingBlueprintMgr(HousingBlueprintMgr const&) = delete;
    HousingBlueprintMgr& operator=(HousingBlueprintMgr const&) = delete;

    static HousingBlueprintMgr& Instance();

    void LoadFromDB();

    HousingBlueprint const* GetByUuid(std::string_view uuid) const;
    HousingBlueprint const* Get(uint64 id) const;
    std::vector<HousingBlueprint const*> GetCollection(uint32 bnetAccountId) const;
    uint32 GetPlayerMadeCount(uint32 bnetAccountId) const;

    HousingBlueprint const* Create(uint32 bnetAccountId, ObjectGuid::LowType exporterGuid, std::string name, HousingBlueprintType type,
        uint8 flags, HousingBlueprintContent content);
    HousingResult Rename(uint32 bnetAccountId, uint64 blueprintId, std::string const& name);
    HousingResult Delete(uint32 bnetAccountId, uint64 blueprintId);

    static bool IsValidName(std::string const& name);
    static std::string NormalizeUuid(std::string_view uuid);

    // Exports what the blueprint type covers from a house: House = everything, Interior = rooms and interior decor,
    // Exterior = house type, size, position, fixtures and yard decor, Room = one room and its decor.
    static HousingResult Snapshot(Housing const& housing, HousingBlueprintType type, ObjectGuid roomGuid, HousingBlueprintContent& content);

    // Contents, costs and unmet requirements of importing into target (nullptr: no house context; ownership is then taken
    // from storageSource).
    static void Evaluate(HousingBlueprint const& blueprint, Housing const* target, Housing const* storageSource,
        HousingBlueprintEvaluation& evaluation);

    // Imports a house, interior or exterior blueprint into the model of housing. Saves the replaced part as an automatic
    // backup first.
    HousingResult ApplyLayout(Player* player, Housing* housing, HousingBlueprint const& blueprint, HousingBlueprintApplyResult& result);
    // Places a room blueprint's decor into a room that was just added for it.
    static void ApplyRoomDecor(Player* player, Housing* housing, HousingBlueprint const& blueprint, ObjectGuid roomGuid,
        HousingBlueprintApplyResult& result);

private:
    HousingBlueprintMgr() = default;

    static std::vector<uint8> Serialize(HousingBlueprintContent const& content);
    static bool Deserialize(std::vector<uint8> const& data, HousingBlueprintContent& content);
    static std::string GenerateUuid();
    void PruneBackups(uint32 bnetAccountId);

    std::unordered_map<uint64, HousingBlueprint> _blueprints;
    std::unordered_map<std::string, uint64> _byUuid;
    uint64 _nextId = 1;
};

#define sHousingBlueprintMgr HousingBlueprintMgr::Instance()

#endif // TRINITYCORE_HOUSING_BLUEPRINT_MGR_H
