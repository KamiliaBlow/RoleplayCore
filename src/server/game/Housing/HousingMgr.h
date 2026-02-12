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

#ifndef HousingMgr_h__
#define HousingMgr_h__

#include "Define.h"
#include "HousingDefines.h"
#include "Position.h"
#include <string>
#include <unordered_map>
#include <vector>

struct HouseDecorData
{
    uint32 ID = 0;
    std::string Name;
    float BoundingBox[3] = {};
    int32 PlacementFlags = 0;
    int32 CategoryID = 0;
    uint8 Quality = 0;
    uint8 MaxDyeSlots = 0;
    int32 SpellVisualID = 0;
    int32 GameObjectDisplayInfoID = 0;
    int32 CurrencyTypeID = 0;
    int32 CurrencyCost = 0;
    float Scale = 1.0f;
    int32 SourceTypeEnum = 0;
    int32 SourceID = 0;
    int8 SourceDifficultyID = 0;
    int32 UiModelSceneID = 0;
    int32 HouseDecorCategoryID = 0;
};

struct HouseLevelData
{
    uint32 ID = 0;
    int32 MaxDecorCount = 0;
    int32 FavorRequired = 0;
};

struct HouseRoomData
{
    uint32 ID = 0;
    std::string Name;
    int8 Size = 0;
    int32 WmoID = 0;
    int32 MaxDecorCount = 0;
    int32 DoorSlots = 0;
    int32 CeilingSlots = 0;
    int32 WallSlots = 0;
};

struct HouseThemeData
{
    uint32 ID = 0;
    std::string Name;
    int32 ThemeSetID = 0;
    int32 UiModelSceneID = 0;
};

struct HouseDecorThemeSetData
{
    uint32 ID = 0;
    std::string Name;
    int32 HouseThemeID = 0;
    int32 HouseDecorCategoryID = 0;
};

struct NeighborhoodMapData
{
    uint32 ID = 0;
    float Origin[3] = {};
    int32 MapID = 0;
    float PlotSpacing = 0.0f;
    uint32 MaxPlots = 0;
    int32 UiMapID = 0;
};

struct NeighborhoodPlotData
{
    uint32 ID = 0;
    uint64 ContentTuningID = 0;
    std::string InternalName;
    float Position[3] = {};
    float Rotation[3] = {};
    float Scale[3] = {};
    float BoundingBoxMin[3] = {};
    float BoundingBoxMax[3] = {};
    int32 NeighborhoodMapID = 0;
    int32 PlotSize = 0;
    int32 PlotIndex = 0;
    int32 HouseExteriorWmoDataID = 0;
    int32 GameObjectID = 0;
    int32 AreaTriggerID = 0;
    float PlotRadius = 0.0f;
    int32 Flags = 0;
};

struct NeighborhoodNameGenData
{
    uint32 ID = 0;
    std::string Prefix;
    std::string Middle;
    std::string Suffix;
    int32 NeighborhoodMapID = 0;
};

class TC_GAME_API HousingMgr
{
public:
    HousingMgr();
    HousingMgr(HousingMgr const&) = delete;
    HousingMgr(HousingMgr&&) = delete;
    HousingMgr& operator=(HousingMgr const&) = delete;
    HousingMgr& operator=(HousingMgr&&) = delete;
    ~HousingMgr();

    static HousingMgr& Instance();

    void Initialize();

    // DB2 data accessors
    HouseDecorData const* GetHouseDecorData(uint32 id) const;
    HouseLevelData const* GetLevelData(uint32 level) const;
    HouseRoomData const* GetHouseRoomData(uint32 id) const;
    HouseThemeData const* GetHouseThemeData(uint32 id) const;
    HouseDecorThemeSetData const* GetHouseDecorThemeSetData(uint32 id) const;
    NeighborhoodMapData const* GetNeighborhoodMapData(uint32 id) const;

    // Neighborhood plot lookups
    std::vector<NeighborhoodPlotData const*> GetPlotsForMap(uint32 neighborhoodMapId) const;

    // Name generation
    std::string GenerateNeighborhoodName(uint32 neighborhoodMapId) const;

    // Level-based limits
    uint32 GetMaxDecorForLevel(uint32 level) const;

    // Validation
    HousingResult ValidateDecorPlacement(uint32 decorId, Position const& pos, uint32 houseLevel) const;

private:
    void LoadHouseDecorData();
    void LoadHouseLevelData();
    void LoadHouseRoomData();
    void LoadHouseThemeData();
    void LoadHouseDecorThemeSetData();
    void LoadNeighborhoodMapData();
    void LoadNeighborhoodPlotData();
    void LoadNeighborhoodNameGenData();

    // DB2 data stores indexed by ID
    std::unordered_map<uint32, HouseDecorData> _houseDecorStore;
    std::unordered_map<uint32, HouseLevelData> _houseLevelDataStore;
    std::unordered_map<uint32, HouseRoomData> _houseRoomStore;
    std::unordered_map<uint32, HouseThemeData> _houseThemeStore;
    std::unordered_map<uint32, HouseDecorThemeSetData> _houseDecorThemeSetStore;
    std::unordered_map<uint32, NeighborhoodMapData> _neighborhoodMapStore;
    std::unordered_map<uint32, NeighborhoodPlotData> _neighborhoodPlotStore;

    // Lookup indexes
    std::unordered_map<uint32 /*neighborhoodMapId*/, std::vector<NeighborhoodPlotData const*>> _plotsByMap;
    std::unordered_map<uint32 /*neighborhoodMapId*/, std::vector<NeighborhoodNameGenData>> _nameGenByMap;
    std::unordered_map<uint32 /*level*/, HouseLevelData const*> _levelDataByLevel;
};

#define sHousingMgr HousingMgr::Instance()

#endif // HousingMgr_h__
