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

#include "HousingBlueprintMgr.h"
#include "Account.h"
#include "ByteBuffer.h"
#include "CryptoRandom.h"
#include "DatabaseEnv.h"
#include "DB2Stores.h"
#include "GameTime.h"
#include "Housing.h"
#include "HousingMgr.h"
#include "Log.h"
#include "Neighborhood.h"
#include "NeighborhoodMgr.h"
#include "Player.h"
#include "RealmList.h"
#include "Timer.h"
#include "Util.h"
#include "WorldSession.h"
#include <algorithm>
#include <cmath>
#include <map>

namespace
{
constexpr uint8 BLUEPRINT_CONTENT_VERSION = 1;

struct Frame
{
    float X = 0.0f;
    float Y = 0.0f;
    float Z = 0.0f;
    float Facing = 0.0f;
};

struct Quat
{
    float X = 0.0f;
    float Y = 0.0f;
    float Z = 0.0f;
    float W = 1.0f;
};

Quat Multiply(Quat const& a, Quat const& b)
{
    return {
        a.W * b.X + a.X * b.W + a.Y * b.Z - a.Z * b.Y,
        a.W * b.Y - a.X * b.Z + a.Y * b.W + a.Z * b.X,
        a.W * b.Z + a.X * b.Y - a.Y * b.X + a.Z * b.W,
        a.W * b.W - a.X * b.X - a.Y * b.Y - a.Z * b.Z
    };
}

Quat Yaw(float angle)
{
    return { 0.0f, 0.0f, std::sin(angle * 0.5f), std::cos(angle * 0.5f) };
}

// World <-> frame-local transforms. The rotation convention matches HouseInteriorMap/HousingMap, which derive a decor's
// room-local position the same way.
void ToLocal(Frame const& frame, float x, float y, float z, Quat const& worldRot, HousingBlueprintDecor& decor)
{
    float const dx = x - frame.X;
    float const dy = y - frame.Y;
    float const c = std::cos(frame.Facing);
    float const s = std::sin(frame.Facing);
    decor.PosX = c * dx + s * dy;
    decor.PosY = -s * dx + c * dy;
    decor.PosZ = z - frame.Z;
    Quat const local = Multiply(Yaw(-frame.Facing), worldRot);
    decor.RotX = local.X;
    decor.RotY = local.Y;
    decor.RotZ = local.Z;
    decor.RotW = local.W;
}

void ToWorld(Frame const& frame, HousingBlueprintDecor const& decor, float& x, float& y, float& z, Quat& worldRot)
{
    float const c = std::cos(frame.Facing);
    float const s = std::sin(frame.Facing);
    x = frame.X + c * decor.PosX - s * decor.PosY;
    y = frame.Y + s * decor.PosX + c * decor.PosY;
    z = frame.Z + decor.PosZ;
    worldRot = Multiply(Yaw(frame.Facing), { decor.RotX, decor.RotY, decor.RotZ, decor.RotW });
}

Frame InteriorRoomFrame(int32 gridX, int32 gridY, int32 floorIndex, uint32 orientation)
{
    // Same origin and room placement HouseInteriorMap uses for every house interior.
    Frame frame{ -1000.0f, -1000.0f, 0.1f, 0.0f };
    if (NeighborhoodMapData const* nmData = sHousingMgr.GetNeighborhoodMapDataForWorldMap(HOUSE_INTERIOR_MAP_ID))
    {
        frame.X = nmData->Origin[0];
        frame.Y = nmData->Origin[1];
        frame.Z = nmData->Origin[2];
    }
    frame.X += static_cast<float>(gridX);
    frame.Y += static_cast<float>(gridY);
    frame.Z += static_cast<float>(floorIndex) * HOUSE_INTERIOR_FLOOR_HEIGHT;
    frame.Facing = static_cast<float>(orientation) * float(M_PI / 2.0);
    return frame;
}

// The plot a house stands on, as HousingMap places it (HousingMgr::GetDefaultHousePosition).
Frame PlotFrame(Housing const& housing)
{
    Frame frame;
    Neighborhood* neighborhood = sNeighborhoodMgr.GetNeighborhood(housing.GetNeighborhoodGuid());
    if (!neighborhood)
        return frame;

    for (NeighborhoodPlotData const* plot : sHousingMgr.GetPlotsForMap(neighborhood->GetNeighborhoodMapID()))
    {
        if (!plot || plot->PlotIndex != int32(housing.GetPlotIndex()))
            continue;

        Position const spot = sHousingMgr.GetDefaultHousePosition(*plot);
        frame.X = spot.GetPositionX();
        frame.Y = spot.GetPositionY();
        frame.Z = spot.GetPositionZ();
        frame.Facing = spot.GetOrientation();
        break;
    }
    return frame;
}

bool CoversInterior(HousingBlueprintType type)
{
    return type == HousingBlueprintType::House || type == HousingBlueprintType::Interior || type == HousingBlueprintType::Room;
}

bool CoversExterior(HousingBlueprintType type)
{
    return type == HousingBlueprintType::House || type == HousingBlueprintType::Exterior;
}

uint32 FixtureComponentId(HousingBlueprintFixture const& fixture)
{
    // Core fixtures are stored as {componentId, 0}, hook fixtures as {hookId, componentId}.
    return fixture.OptionId ? fixture.OptionId : fixture.FixturePointId;
}

uint32 CatalogCount(Housing const& housing, uint32 decorEntryId)
{
    for (Housing::CatalogEntry const* entry : housing.GetCatalogEntries())
        if (entry && entry->DecorEntryId == decorEntryId)
            return entry->Count;
    return 0;
}

struct PooledDecor
{
    ObjectGuid Guid;
    uint8 SourceType = DECOR_SOURCE_STANDARD;
    std::string SourceValue;
};

using DecorPool = std::unordered_map<uint32 /*decorEntryId*/, std::vector<PooledDecor>>;

void PlaceBlueprintDecor(Player* player, Housing* housing, HousingBlueprintDecor const& decor, ObjectGuid roomGuid, Frame const& frame,
    DecorPool& pool, HousingBlueprintApplyResult& result)
{
    float x, y, z;
    Quat rot;
    ToWorld(frame, decor, x, y, z, rot);

    // Reuse an instance this import took out of the house before minting a new one, so the account's decor storage keeps
    // one entry per owned item.
    bool fromPool = false;
    PooledDecor pooled;
    std::vector<PooledDecor>& instances = pool[decor.DecorEntryId];
    if (!instances.empty())
    {
        pooled = std::move(instances.back());
        instances.pop_back();
        fromPool = true;
    }

    uint32 const storageBefore = CatalogCount(*housing, decor.DecorEntryId);
    uint32 placedOfType = 0;
    for (auto const& [guid, placed] : housing->GetPlacedDecorMap())
        if (placed.DecorEntryId == decor.DecorEntryId)
            ++placedOfType;

    ObjectGuid const decorGuid = fromPool ? pooled.Guid : housing->GenerateDecorGuid(decor.DecorEntryId);
    HousingResult placeResult = housing->PlaceDecorWithGuid(decorGuid, decor.DecorEntryId, x, y, z, rot.X, rot.Y, rot.Z, rot.W, roomGuid,
        decor.Scale);
    if (placeResult != HOUSING_RESULT_SUCCESS)
    {
        if (fromPool)
            instances.push_back(std::move(pooled));
        ++result.SkippedDecor;
        TC_LOG_DEBUG("housing", "HousingBlueprintMgr: decor {} not placed ({})", decor.DecorEntryId, uint32(placeResult));
        return;
    }

    // A new instance came out of storage: drop the storage entry Housing::PopulateCatalogStorageEntries created for it.
    if (!fromPool && housing->IsStoragePopulated() && player->GetSession() && storageBefore > placedOfType)
    {
        uint64 const uniqueId = player->GetGUID().GetCounter() * 100000 + decor.DecorEntryId * 100 + (storageBefore - placedOfType - 1);
        player->GetSession()->GetBattlenetAccount().RemoveHousingDecorStorageEntry(ObjectGuid::Create<HighGuid::Housing>(
            /*subType*/ 1, /*arg1*/ sRealmList->GetCurrentRealmId().Realm, /*arg2*/ decor.DecorEntryId, uniqueId));
    }

    if (std::any_of(decor.DyeSlots.begin(), decor.DyeSlots.end(), [](uint32 dye) { return dye != 0; }))
        housing->CommitDecorDyes(decorGuid, decor.DyeSlots);

    ++result.PlacedDecor;
}

void ReturnPoolToStorage(Player* player, DecorPool const& pool)
{
    if (!player->GetSession())
        return;

    Battlenet::Account& account = player->GetSession()->GetBattlenetAccount();
    for (auto const& [entryId, instances] : pool)
        for (PooledDecor const& instance : instances)
            account.SetHousingDecorStorageEntry(instance.Guid, ObjectGuid::Empty, instance.SourceType, instance.SourceValue);
}

void TakeDecorOut(Housing* housing, bool exterior, DecorPool& pool, HousingBlueprintApplyResult& result)
{
    std::vector<ObjectGuid> toRemove;
    for (auto const& [guid, decor] : housing->GetPlacedDecorMap())
        if (Housing::IsExteriorDecorPlacement(decor.RoomGuid) == exterior)
            toRemove.push_back(guid);

    for (ObjectGuid const& guid : toRemove)
    {
        Housing::PlacedDecor const* decor = housing->GetPlacedDecor(guid);
        if (!decor)
            continue;

        PooledDecor pooled{ guid, decor->SourceType, decor->SourceValue };
        uint32 const entryId = decor->DecorEntryId;
        if (housing->RemoveDecor(guid) != HOUSING_RESULT_SUCCESS)
            continue;

        pool[entryId].push_back(std::move(pooled));
        result.RemovedDecor.push_back(guid);
    }
}
}

HousingBlueprintMgr& HousingBlueprintMgr::Instance()
{
    static HousingBlueprintMgr instance;
    return instance;
}

void HousingBlueprintMgr::LoadFromDB()
{
    uint32 oldMSTime = getMSTime();

    _blueprints.clear();
    _byUuid.clear();
    _nextId = 1;

    //          0   1     2              3             4     5     6      7           8
    // SELECT id, uuid, bnetAccountId, exporterGuid, name, type, flags, createTime, content FROM account_housing_blueprint
    PreparedQueryResult result = CharacterDatabase.Query(CharacterDatabase.GetPreparedStatement(CHAR_SEL_ACCOUNT_HOUSING_BLUEPRINTS));
    if (!result)
    {
        TC_LOG_INFO("server.loading", ">> Loaded 0 housing blueprints. DB table `account_housing_blueprint` is empty.");
        return;
    }

    do
    {
        Field* fields = result->Fetch();
        HousingBlueprint blueprint;
        blueprint.Id = fields[0].GetUInt64();
        blueprint.Uuid = NormalizeUuid(fields[1].GetStringView());
        blueprint.BnetAccountId = fields[2].GetUInt32();
        blueprint.ExporterGuid = fields[3].GetUInt64();
        blueprint.Name = fields[4].GetString();
        blueprint.Type = HousingBlueprintType(fields[5].GetUInt8());
        blueprint.Flags = fields[6].GetUInt8();
        blueprint.CreateTime = fields[7].GetInt64();

        _nextId = std::max(_nextId, blueprint.Id + 1);

        if (blueprint.Uuid.empty() || blueprint.Type == HousingBlueprintType::None || blueprint.Type > HousingBlueprintType::Exterior
            || !Deserialize(fields[8].GetBinary(), blueprint.Content))
        {
            TC_LOG_ERROR("sql.sql", "Housing blueprint {} (uuid '{}') is malformed, skipped.", blueprint.Id, fields[1].GetStringView());
            continue;
        }

        _byUuid[blueprint.Uuid] = blueprint.Id;
        _blueprints.emplace(blueprint.Id, std::move(blueprint));
    } while (result->NextRow());

    TC_LOG_INFO("server.loading", ">> Loaded {} housing blueprints in {} ms", _blueprints.size(), GetMSTimeDiffToNow(oldMSTime));
}

HousingBlueprint const* HousingBlueprintMgr::GetByUuid(std::string_view uuid) const
{
    auto itr = _byUuid.find(NormalizeUuid(uuid));
    return itr != _byUuid.end() ? Get(itr->second) : nullptr;
}

HousingBlueprint const* HousingBlueprintMgr::Get(uint64 id) const
{
    auto itr = _blueprints.find(id);
    return itr != _blueprints.end() ? &itr->second : nullptr;
}

std::vector<HousingBlueprint const*> HousingBlueprintMgr::GetCollection(uint32 bnetAccountId) const
{
    std::vector<HousingBlueprint const*> collection;
    for (auto const& [id, blueprint] : _blueprints)
        if (blueprint.BnetAccountId == bnetAccountId)
            collection.push_back(&blueprint);

    std::sort(collection.begin(), collection.end(), [](HousingBlueprint const* a, HousingBlueprint const* b)
    {
        return a->CreateTime != b->CreateTime ? a->CreateTime < b->CreateTime : a->Id < b->Id;
    });
    return collection;
}

uint32 HousingBlueprintMgr::GetPlayerMadeCount(uint32 bnetAccountId) const
{
    return uint32(std::count_if(_blueprints.begin(), _blueprints.end(), [bnetAccountId](auto const& pair)
    {
        return pair.second.BnetAccountId == bnetAccountId && !pair.second.IsAutomaticBackup();
    }));
}

HousingBlueprint const* HousingBlueprintMgr::Create(uint32 bnetAccountId, ObjectGuid::LowType exporterGuid, std::string name,
    HousingBlueprintType type, uint8 flags, HousingBlueprintContent content)
{
    HousingBlueprint blueprint;
    blueprint.Id = _nextId++;
    do
        blueprint.Uuid = GenerateUuid();
    while (_byUuid.contains(blueprint.Uuid));
    blueprint.BnetAccountId = bnetAccountId;
    blueprint.ExporterGuid = exporterGuid;
    blueprint.Name = std::move(name);
    blueprint.Type = type;
    blueprint.Flags = flags;
    blueprint.CreateTime = GameTime::GetGameTime();
    blueprint.Content = std::move(content);

    CharacterDatabasePreparedStatement* stmt = CharacterDatabase.GetPreparedStatement(CHAR_INS_ACCOUNT_HOUSING_BLUEPRINT);
    stmt->setUInt64(0, blueprint.Id);
    stmt->setString(1, blueprint.Uuid);
    stmt->setUInt32(2, blueprint.BnetAccountId);
    stmt->setUInt64(3, blueprint.ExporterGuid);
    stmt->setString(4, blueprint.Name);
    stmt->setUInt8(5, uint8(blueprint.Type));
    stmt->setUInt8(6, blueprint.Flags);
    stmt->setInt64(7, blueprint.CreateTime);
    stmt->setBinary(8, Serialize(blueprint.Content));
    CharacterDatabase.Execute(stmt);

    uint64 const id = blueprint.Id;
    _byUuid[blueprint.Uuid] = id;
    _blueprints.emplace(id, std::move(blueprint));

    if (flags & HOUSING_BLUEPRINT_FLAG_AUTOMATIC_BACKUP)
        PruneBackups(bnetAccountId);

    return Get(id);
}

void HousingBlueprintMgr::PruneBackups(uint32 bnetAccountId)
{
    std::vector<HousingBlueprint const*> backups;
    for (HousingBlueprint const* blueprint : GetCollection(bnetAccountId))
        if (blueprint->IsAutomaticBackup())
            backups.push_back(blueprint);

    // GetCollection sorts oldest first.
    while (backups.size() > HOUSING_BLUEPRINTS_MAX_BACKUPS_PER_BNET_ACCOUNT)
    {
        Delete(bnetAccountId, backups.front()->Id);
        backups.erase(backups.begin());
    }
}

HousingResult HousingBlueprintMgr::Rename(uint32 bnetAccountId, uint64 blueprintId, std::string const& name)
{
    auto itr = _blueprints.find(blueprintId);
    if (itr == _blueprints.end() || itr->second.BnetAccountId != bnetAccountId)
        return HOUSING_RESULT_BLUEPRINT_NOT_FOUND;

    if (!IsValidName(name))
        return HOUSING_RESULT_BLUEPRINT_NAME_INVALID;

    itr->second.Name = name;

    CharacterDatabasePreparedStatement* stmt = CharacterDatabase.GetPreparedStatement(CHAR_UPD_ACCOUNT_HOUSING_BLUEPRINT_NAME);
    stmt->setString(0, name);
    stmt->setUInt64(1, blueprintId);
    CharacterDatabase.Execute(stmt);
    return HOUSING_RESULT_SUCCESS;
}

HousingResult HousingBlueprintMgr::Delete(uint32 bnetAccountId, uint64 blueprintId)
{
    auto itr = _blueprints.find(blueprintId);
    if (itr == _blueprints.end() || itr->second.BnetAccountId != bnetAccountId)
        return HOUSING_RESULT_BLUEPRINT_NOT_FOUND;

    _byUuid.erase(itr->second.Uuid);
    _blueprints.erase(itr);

    CharacterDatabasePreparedStatement* stmt = CharacterDatabase.GetPreparedStatement(CHAR_DEL_ACCOUNT_HOUSING_BLUEPRINT);
    stmt->setUInt64(0, blueprintId);
    CharacterDatabase.Execute(stmt);
    return HOUSING_RESULT_SUCCESS;
}

bool HousingBlueprintMgr::IsValidName(std::string const& name)
{
    // Constants.HousingConsts.BlueprintNameMinCharacters / BlueprintNameMaxCharacters, counted in characters.
    std::wstring wname;
    if (!Utf8toWStr(name, wname))
        return false;

    if (wname.size() < HOUSING_BLUEPRINT_NAME_MIN_CHARACTERS || wname.size() > HOUSING_BLUEPRINT_NAME_MAX_CHARACTERS)
        return false;

    return std::none_of(wname.begin(), wname.end(), [](wchar_t c) { return c < 0x20 || c == 0x7F || c == L'|'; });
}

std::string HousingBlueprintMgr::NormalizeUuid(std::string_view uuid)
{
    // The client writes the 16 UUID bytes as 8-4-4-4-12 hex digits.
    if (uuid.size() != 36)
        return {};

    std::string normalized(uuid);
    for (size_t i = 0; i < normalized.size(); ++i)
    {
        char& c = normalized[i];
        if (i == 8 || i == 13 || i == 18 || i == 23)
        {
            if (c != '-')
                return {};
            continue;
        }
        if (c >= 'A' && c <= 'F')
            c = char(c - 'A' + 'a');
        else if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f')))
            return {};
    }
    return normalized;
}

std::string HousingBlueprintMgr::GenerateUuid()
{
    std::array<uint8, 16> bytes = Trinity::Crypto::GetRandomBytes<16>();
    bytes[6] = uint8((bytes[6] & 0x0F) | 0x40);    // version 4
    bytes[8] = uint8((bytes[8] & 0x3F) | 0x80);    // RFC 4122 variant

    static constexpr char HexDigits[] = "0123456789abcdef";
    std::string uuid;
    uuid.reserve(36);
    for (size_t i = 0; i < bytes.size(); ++i)
    {
        if (i == 4 || i == 6 || i == 8 || i == 10)
            uuid.push_back('-');
        uuid.push_back(HexDigits[bytes[i] >> 4]);
        uuid.push_back(HexDigits[bytes[i] & 0xF]);
    }
    return uuid;
}

std::vector<uint8> HousingBlueprintMgr::Serialize(HousingBlueprintContent const& content)
{
    ByteBuffer buffer;
    buffer << uint8(BLUEPRINT_CONTENT_VERSION);
    buffer << uint32(content.HouseType);
    buffer << uint8(content.HouseSize);
    buffer << uint8(content.HasHousePosition ? 1 : 0);
    buffer << float(content.HousePosX) << float(content.HousePosY) << float(content.HousePosZ) << float(content.HouseFacing);

    buffer << uint32(content.Fixtures.size());
    for (HousingBlueprintFixture const& fixture : content.Fixtures)
        buffer << uint32(fixture.FixturePointId) << uint32(fixture.OptionId);

    buffer << uint32(content.Rooms.size());
    for (HousingBlueprintRoom const& room : content.Rooms)
    {
        buffer << uint32(room.RoomEntryId) << int32(room.GridX) << int32(room.GridY) << int32(room.FloorIndex);
        buffer << uint32(room.Orientation) << uint8(room.Mirrored ? 1 : 0);
        buffer << uint32(room.ThemeId) << uint32(room.WallThemeId) << uint32(room.FloorThemeId) << uint32(room.CeilingThemeId);
        buffer << uint32(room.WallTextureId) << uint32(room.FloorTextureId) << uint32(room.CeilingTextureId);
        buffer << int32(room.ColorOverride);
        buffer << uint32(room.DoorTypeId) << uint8(room.DoorSlot) << uint32(room.CeilingTypeId) << uint8(room.CeilingSlot);
    }

    buffer << uint32(content.Decor.size());
    for (HousingBlueprintDecor const& decor : content.Decor)
    {
        buffer << uint32(decor.DecorEntryId) << int32(decor.RoomIndex);
        buffer << float(decor.PosX) << float(decor.PosY) << float(decor.PosZ);
        buffer << float(decor.RotX) << float(decor.RotY) << float(decor.RotZ) << float(decor.RotW);
        buffer << float(decor.Scale);
        for (uint32 dye : decor.DyeSlots)
            buffer << uint32(dye);
    }

    return std::vector<uint8>(buffer.data(), buffer.data() + buffer.size());
}

bool HousingBlueprintMgr::Deserialize(std::vector<uint8> const& data, HousingBlueprintContent& content)
{
    try
    {
        ByteBuffer buffer{ std::vector<uint8>(data) };

        if (buffer.read<uint8>() != BLUEPRINT_CONTENT_VERSION)
            return false;

        content.HouseType = buffer.read<uint32>();
        content.HouseSize = buffer.read<uint8>();
        content.HasHousePosition = buffer.read<uint8>() != 0;
        content.HousePosX = buffer.read<float>();
        content.HousePosY = buffer.read<float>();
        content.HousePosZ = buffer.read<float>();
        content.HouseFacing = buffer.read<float>();

        content.Fixtures.resize(buffer.read<uint32>());
        for (HousingBlueprintFixture& fixture : content.Fixtures)
        {
            fixture.FixturePointId = buffer.read<uint32>();
            fixture.OptionId = buffer.read<uint32>();
        }

        content.Rooms.resize(buffer.read<uint32>());
        for (HousingBlueprintRoom& room : content.Rooms)
        {
            room.RoomEntryId = buffer.read<uint32>();
            room.GridX = buffer.read<int32>();
            room.GridY = buffer.read<int32>();
            room.FloorIndex = buffer.read<int32>();
            room.Orientation = buffer.read<uint32>();
            room.Mirrored = buffer.read<uint8>() != 0;
            room.ThemeId = buffer.read<uint32>();
            room.WallThemeId = buffer.read<uint32>();
            room.FloorThemeId = buffer.read<uint32>();
            room.CeilingThemeId = buffer.read<uint32>();
            room.WallTextureId = buffer.read<uint32>();
            room.FloorTextureId = buffer.read<uint32>();
            room.CeilingTextureId = buffer.read<uint32>();
            room.ColorOverride = buffer.read<int32>();
            room.DoorTypeId = buffer.read<uint32>();
            room.DoorSlot = buffer.read<uint8>();
            room.CeilingTypeId = buffer.read<uint32>();
            room.CeilingSlot = buffer.read<uint8>();
        }

        content.Decor.resize(buffer.read<uint32>());
        for (HousingBlueprintDecor& decor : content.Decor)
        {
            decor.DecorEntryId = buffer.read<uint32>();
            decor.RoomIndex = buffer.read<int32>();
            decor.PosX = buffer.read<float>();
            decor.PosY = buffer.read<float>();
            decor.PosZ = buffer.read<float>();
            decor.RotX = buffer.read<float>();
            decor.RotY = buffer.read<float>();
            decor.RotZ = buffer.read<float>();
            decor.RotW = buffer.read<float>();
            decor.Scale = buffer.read<float>();
            for (uint32& dye : decor.DyeSlots)
                dye = buffer.read<uint32>();

            if (decor.RoomIndex != HousingBlueprintDecor::EXTERIOR && (decor.RoomIndex < 0 || decor.RoomIndex >= int32(content.Rooms.size())))
                return false;
        }

        return buffer.rpos() == buffer.size();
    }
    catch (ByteBufferException const&)
    {
        return false;
    }
}

HousingResult HousingBlueprintMgr::Snapshot(Housing const& housing, HousingBlueprintType type, ObjectGuid roomGuid, HousingBlueprintContent& content)
{
    content = {};

    if (housing.GetHouseGuid().IsEmpty())
        return HOUSING_RESULT_HOUSE_NOT_FOUND;

    std::unordered_map<ObjectGuid, int32> roomIndex;
    auto addRoom = [&](Housing::Room const& room)
    {
        roomIndex[room.Guid] = int32(content.Rooms.size());
        HousingBlueprintRoom& out = content.Rooms.emplace_back();
        out.RoomEntryId = room.RoomEntryId;
        out.GridX = room.GridX;
        out.GridY = room.GridY;
        out.FloorIndex = room.FloorIndex;
        out.Orientation = room.Orientation;
        out.Mirrored = room.Mirrored;
        out.ThemeId = room.ThemeId;
        out.WallThemeId = room.WallThemeId;
        out.FloorThemeId = room.FloorThemeId;
        out.CeilingThemeId = room.CeilingThemeId;
        out.WallTextureId = room.WallTextureId;
        out.FloorTextureId = room.FloorTextureId;
        out.CeilingTextureId = room.CeilingTextureId;
        out.ColorOverride = room.ColorOverride;
        out.DoorTypeId = room.DoorTypeId;
        out.DoorSlot = room.DoorSlot;
        out.CeilingTypeId = room.CeilingTypeId;
        out.CeilingSlot = room.CeilingSlot;
    };

    switch (type)
    {
        case HousingBlueprintType::House:
        case HousingBlueprintType::Interior:
        {
            std::vector<Housing::Room const*> rooms = housing.GetRooms();
            std::sort(rooms.begin(), rooms.end(), [](Housing::Room const* a, Housing::Room const* b) { return a->SlotIndex < b->SlotIndex; });
            for (Housing::Room const* room : rooms)
                addRoom(*room);
            break;
        }
        case HousingBlueprintType::Room:
        {
            Housing::Room const* room = housing.GetRoom(roomGuid);
            if (!room)
                return HOUSING_RESULT_ROOM_NOT_FOUND;
            HouseRoomData const* roomData = sHousingMgr.GetHouseRoomData(room->RoomEntryId);
            if (!roomData || roomData->IsBaseRoom())
                return HOUSING_RESULT_BLUEPRINT_TYPE_INVALID;
            addRoom(*room);
            break;
        }
        case HousingBlueprintType::Exterior:
            break;
        default:
            return HOUSING_RESULT_BLUEPRINT_TYPE_INVALID;
    }

    if (CoversExterior(type))
    {
        content.HouseType = housing.GetHouseType();
        content.HouseSize = housing.GetHouseSize();

        bool hasCore = false;
        for (Housing::Fixture const* fixture : housing.GetFixtures())
        {
            content.Fixtures.push_back({ fixture->FixturePointId, fixture->OptionId });
            hasCore = hasCore || fixture->OptionId == 0;
        }
        if (!hasCore)
            if (uint32 coreComponent = housing.GetCoreExteriorComponentID())
                content.Fixtures.push_back({ coreComponent, 0 });

        if (housing.HasCustomPosition())
        {
            Frame const plot = PlotFrame(housing);
            Position const pos = housing.GetHousePosition();
            HousingBlueprintDecor local;
            ToLocal(plot, pos.GetPositionX(), pos.GetPositionY(), pos.GetPositionZ(), Yaw(pos.GetOrientation()), local);
            content.HasHousePosition = true;
            content.HousePosX = local.PosX;
            content.HousePosY = local.PosY;
            content.HousePosZ = local.PosZ;
            content.HouseFacing = pos.GetOrientation() - plot.Facing;
        }
    }

    Frame const plotFrame = PlotFrame(housing);
    for (auto const& [guid, decor] : housing.GetPlacedDecorMap())
    {
        bool const exterior = Housing::IsExteriorDecorPlacement(decor.RoomGuid);
        HousingBlueprintDecor out;
        out.DecorEntryId = decor.DecorEntryId;
        out.Scale = decor.Scale;
        out.DyeSlots = decor.DyeSlots;
        Quat const rot{ decor.RotationX, decor.RotationY, decor.RotationZ, decor.RotationW };

        if (exterior)
        {
            if (!CoversExterior(type))
                continue;
            out.RoomIndex = HousingBlueprintDecor::EXTERIOR;
            ToLocal(plotFrame, decor.PosX, decor.PosY, decor.PosZ, rot, out);
        }
        else
        {
            auto itr = roomIndex.find(decor.RoomGuid);
            if (itr == roomIndex.end())
                continue;
            HousingBlueprintRoom const& room = content.Rooms[itr->second];
            out.RoomIndex = itr->second;
            ToLocal(InteriorRoomFrame(room.GridX, room.GridY, room.FloorIndex, room.Orientation), decor.PosX, decor.PosY, decor.PosZ, rot, out);
        }

        content.Decor.push_back(out);
    }

    return HOUSING_RESULT_SUCCESS;
}

void HousingBlueprintMgr::Evaluate(HousingBlueprint const& blueprint, Housing const* target, Housing const* storageSource,
    HousingBlueprintEvaluation& evaluation)
{
    evaluation = {};
    HousingBlueprintContent const& content = blueprint.Content;
    bool const replaces = blueprint.Type != HousingBlueprintType::Room;

    std::map<uint32, uint32> decorTotals;
    std::map<uint32, uint32> dyeTotals;
    uint32 interiorDecorCost = 0;
    uint32 exteriorDecorCost = 0;
    for (HousingBlueprintDecor const& decor : content.Decor)
    {
        ++decorTotals[decor.DecorEntryId];
        for (uint32 dye : decor.DyeSlots)
            if (dye)
                ++dyeTotals[dye];

        uint32 const cost = sHousingMgr.GetDecorWeightCost(decor.DecorEntryId);
        if (decor.RoomIndex == HousingBlueprintDecor::EXTERIOR)
            exteriorDecorCost += cost;
        else
            interiorDecorCost += cost;
    }

    WorldPackets::Housing::JamBlueprintContentLists& totals = evaluation.Totals;
    for (auto const& [entryId, count] : decorTotals)
    {
        totals.Decor.emplace_back(entryId, count);

        if (!sHousingMgr.GetHouseDecorData(entryId))
        {
            evaluation.Invalid.Decor.emplace_back(entryId, count);
            continue;
        }

        uint32 available = storageSource ? CatalogCount(*storageSource, entryId) : 0;
        if (target && replaces)
        {
            // Decor the import takes out of the house counts as owned.
            for (auto const& [guid, placed] : target->GetPlacedDecorMap())
            {
                bool const exterior = Housing::IsExteriorDecorPlacement(placed.RoomGuid);
                if (placed.DecorEntryId == entryId && (exterior ? CoversExterior(blueprint.Type) : CoversInterior(blueprint.Type)))
                    ++available;
            }
        }

        if (count > available)
        {
            evaluation.Missing.Decor.emplace_back(entryId, count - available);
            evaluation.UnmetRequirementFlags |= HOUSING_BLUEPRINT_UNMET_MISSING_DECOR;
        }
    }

    for (auto const& [dyeId, count] : dyeTotals)
    {
        totals.Dyes.emplace_back(dyeId, count);
        if (!sDyeColorStore.LookupEntry(dyeId))
            evaluation.Invalid.Dyes.emplace_back(dyeId, count);
    }

    uint32 roomCost = 0;
    for (HousingBlueprintRoom const& room : content.Rooms)
    {
        totals.Rooms.push_back(room.RoomEntryId);
        if (!sHousingMgr.GetHouseRoomData(room.RoomEntryId))
        {
            evaluation.Missing.Rooms.push_back(room.RoomEntryId);
            evaluation.UnmetRequirementFlags |= HOUSING_BLUEPRINT_UNMET_MISSING_ROOM;
            continue;
        }
        roomCost += sHousingMgr.GetRoomWeightCost(room.RoomEntryId);
    }

    for (HousingBlueprintFixture const& fixture : content.Fixtures)
    {
        uint32 const componentId = FixtureComponentId(fixture);
        totals.Fixtures.push_back(componentId);
        if (!sExteriorComponentStore.LookupEntry(componentId))
        {
            evaluation.Missing.Fixtures.push_back(componentId);
            evaluation.UnmetRequirementFlags |= HOUSING_BLUEPRINT_UNMET_MISSING_FIXTURE;
        }
    }

    if (CoversExterior(blueprint.Type) && content.HouseType)
    {
        HouseExteriorWmoData const* wmoData = sHousingMgr.GetHouseExteriorWmoData(content.HouseType);
        if (!wmoData)
            evaluation.UnmetRequirementFlags |= HOUSING_BLUEPRINT_UNMET_MISSING_FIXTURE;
        else if (target)
        {
            // HouseExteriorWMOData.Flags limit a house type to Horde and/or Alliance neighborhoods.
            Neighborhood* neighborhood = sNeighborhoodMgr.GetNeighborhood(target->GetNeighborhoodGuid());
            int32 const faction = neighborhood ? neighborhood->GetFactionRestriction() : NEIGHBORHOOD_FACTION_NONE;
            uint32 const factionFlags = uint32(wmoData->Flags) & (HOUSE_EXTERIOR_WMO_FLAG_ALLOWED_IN_HORDE_NEIGHBORHOODS | HOUSE_EXTERIOR_WMO_FLAG_ALLOWED_IN_ALLIANCE_NEIGHBORHOODS);
            if (factionFlags)
            {
                bool const allowed = (faction == NEIGHBORHOOD_FACTION_HORDE && (factionFlags & HOUSE_EXTERIOR_WMO_FLAG_ALLOWED_IN_HORDE_NEIGHBORHOODS))
                    || (faction == NEIGHBORHOOD_FACTION_ALLIANCE && (factionFlags & HOUSE_EXTERIOR_WMO_FLAG_ALLOWED_IN_ALLIANCE_NEIGHBORHOODS))
                    || faction == NEIGHBORHOOD_FACTION_NONE;
                if (!allowed)
                    evaluation.UnmetRequirementFlags |= HOUSING_BLUEPRINT_UNMET_MISMATCHED_EXTERIOR_FACTION;
            }
        }
    }

    // Budgets. Room blueprints add to what the house already spends, every other type replaces it (the client shows
    // "available" for rooms and "max" otherwise).
    auto addBudget = [&](std::vector<WorldPackets::Housing::JamBlueprintBudgetEntry>& budgets, HousingBudgetType budgetType, uint32 cost,
        uint32 max, uint32 current)
    {
        WorldPackets::Housing::JamBlueprintBudgetEntry& entry = budgets.emplace_back();
        entry.BudgetType = uint8(budgetType);
        entry.Cost = int32(cost);
        if (!target)
            return;

        entry.Max = int32(max);
        entry.Current = int32(current);
        uint32 const available = replaces ? max : (max > current ? max - current : 0);
        if (cost > available)
            evaluation.UnmetRequirementFlags |= HOUSING_BLUEPRINT_UNMET_INSUFFICIENT_BUDGET;
    };

    if (CoversInterior(blueprint.Type))
    {
        addBudget(evaluation.InteriorBudgets, HousingBudgetType::RoomPlacement, roomCost,
            target ? target->GetMaxRoomBudget() : 0, target ? target->GetRoomWeightUsed() : 0);
        addBudget(evaluation.InteriorBudgets, HousingBudgetType::DecorPlacement, interiorDecorCost,
            target ? target->GetMaxInteriorDecorBudget() : 0, target ? target->GetInteriorDecorWeightUsed() : 0);
    }

    if (CoversExterior(blueprint.Type))
        addBudget(evaluation.ExteriorBudgets, HousingBudgetType::DecorPlacement, exteriorDecorCost,
            target ? target->GetMaxExteriorDecorBudget() : 0, target ? target->GetExteriorDecorWeightUsed() : 0);
}

HousingResult HousingBlueprintMgr::ApplyLayout(Player* player, Housing* housing, HousingBlueprint const& blueprint, HousingBlueprintApplyResult& result)
{
    result = {};

    if (!CoversExterior(blueprint.Type) && blueprint.Type != HousingBlueprintType::Interior)
        return HOUSING_RESULT_BLUEPRINT_TYPE_INVALID;

    HousingBlueprintEvaluation evaluation;
    Evaluate(blueprint, housing, housing, evaluation);
    if (evaluation.IsBlocked())
        return HOUSING_RESULT_BLUEPRINT_REQUIREMENTS_UNMET;

    // What the import replaces goes into the account's automatic backups first.
    {
        HousingBlueprintContent backup;
        if (Snapshot(*housing, blueprint.Type, ObjectGuid::Empty, backup) == HOUSING_RESULT_SUCCESS)
        {
            std::string name = housing->GetHouseName();
            if (!IsValidName(name))
                name = blueprint.Name;
            Create(player->GetSession()->GetBattlenetAccountId(), player->GetGUID().GetCounter(), std::move(name), blueprint.Type,
                HOUSING_BLUEPRINT_FLAG_AUTOMATIC_BACKUP, std::move(backup));
        }
    }

    HousingBlueprintContent const& content = blueprint.Content;
    DecorPool pool;
    std::vector<ObjectGuid> roomGuids(content.Rooms.size());

    if (CoversInterior(blueprint.Type))
    {
        TakeDecorOut(housing, /*exterior*/ false, pool, result);
        housing->RemoveAllNonBaseRooms();

        ObjectGuid const baseRoomGuid = housing->GetBaseRoomGuid();
        uint32 slot = housing->GetNextRoomSlotIndex();
        for (size_t i = 0; i < content.Rooms.size(); ++i)
        {
            HousingBlueprintRoom const& room = content.Rooms[i];
            HouseRoomData const* roomData = sHousingMgr.GetHouseRoomData(room.RoomEntryId);
            if (!roomData)
                continue;

            ObjectGuid roomGuid;
            if (roomData->IsBaseRoom())
                roomGuid = baseRoomGuid;
            else if (housing->PlaceRoom(room.RoomEntryId, slot++, room.Orientation, room.Mirrored, &roomGuid, room.GridX, room.GridY,
                room.FloorIndex) != HOUSING_RESULT_SUCCESS)
                continue;

            if (roomGuid.IsEmpty())
                continue;

            Housing::Room appearance;
            appearance.ThemeId = room.ThemeId;
            appearance.WallThemeId = room.WallThemeId;
            appearance.FloorThemeId = room.FloorThemeId;
            appearance.CeilingThemeId = room.CeilingThemeId;
            appearance.WallTextureId = room.WallTextureId;
            appearance.FloorTextureId = room.FloorTextureId;
            appearance.CeilingTextureId = room.CeilingTextureId;
            appearance.ColorOverride = room.ColorOverride;
            appearance.DoorTypeId = room.DoorTypeId;
            appearance.DoorSlot = room.DoorSlot;
            appearance.CeilingTypeId = room.CeilingTypeId;
            appearance.CeilingSlot = room.CeilingSlot;
            housing->SetRoomAppearance(roomGuid, appearance);
            roomGuids[i] = roomGuid;
        }

        result.InteriorChanged = true;
    }

    Frame const plotFrame = PlotFrame(*housing);
    if (CoversExterior(blueprint.Type))
    {
        TakeDecorOut(housing, /*exterior*/ true, pool, result);

        if (content.HouseType && sHousingMgr.GetHouseExteriorWmoData(content.HouseType) && content.HouseType != housing->GetHouseType())
            housing->SetHouseType(content.HouseType);
        if (content.HouseSize >= HOUSING_FIXTURE_SIZE_ANY && content.HouseSize <= HOUSING_FIXTURE_SIZE_LARGE && content.HouseSize != housing->GetHouseSize())
            housing->SetHouseSize(content.HouseSize);

        std::vector<Housing::Fixture> fixtures;
        for (HousingBlueprintFixture const& fixture : content.Fixtures)
            if (sExteriorComponentStore.LookupEntry(FixtureComponentId(fixture)))
                fixtures.push_back({ fixture.FixturePointId, fixture.OptionId });
        housing->ReplaceFixtures(fixtures);

        if (content.HasHousePosition)
        {
            HousingBlueprintDecor local;
            local.PosX = content.HousePosX;
            local.PosY = content.HousePosY;
            local.PosZ = content.HousePosZ;
            float x, y, z;
            Quat rot;
            ToWorld(plotFrame, local, x, y, z, rot);
            housing->SetHousePosition(x, y, z, content.HouseFacing + plotFrame.Facing);
        }

        result.ExteriorChanged = true;
    }

    for (HousingBlueprintDecor const& decor : content.Decor)
    {
        if (decor.RoomIndex == HousingBlueprintDecor::EXTERIOR)
        {
            if (!CoversExterior(blueprint.Type))
                continue;
            ObjectGuid const exteriorRoomGuid = ObjectGuid::Create<HighGuid::Housing>(/*subType*/ 2, /*arg1*/ 0,
                /*arg2*/ sHousingMgr.GetBaseRoomEntryId(), /*counter*/ ObjectGuid::LowType(housing->GetPlotIndex()) + 1);
            PlaceBlueprintDecor(player, housing, decor, exteriorRoomGuid, plotFrame, pool, result);
            continue;
        }

        ObjectGuid const roomGuid = roomGuids[decor.RoomIndex];
        Housing::Room const* room = roomGuid.IsEmpty() ? nullptr : housing->GetRoom(roomGuid);
        if (!room)
        {
            ++result.SkippedDecor;
            continue;
        }
        PlaceBlueprintDecor(player, housing, decor, roomGuid, InteriorRoomFrame(room->GridX, room->GridY, room->FloorIndex, room->Orientation),
            pool, result);
    }

    ReturnPoolToStorage(player, pool);
    housing->RecalculateBudgets();
    housing->SyncUpdateFields();

    TC_LOG_INFO("housing", "HousingBlueprintMgr::ApplyLayout: player {} imported blueprint {} ({}) type {} into house {}: {} decor placed, {} skipped, {} removed",
        player->GetGUID().ToString(), blueprint.Id, blueprint.Uuid, uint32(blueprint.Type), housing->GetHouseGuid().ToString(),
        result.PlacedDecor, result.SkippedDecor, uint32(result.RemovedDecor.size()));
    return HOUSING_RESULT_SUCCESS;
}

void HousingBlueprintMgr::ApplyRoomDecor(Player* player, Housing* housing, HousingBlueprint const& blueprint, ObjectGuid roomGuid,
    HousingBlueprintApplyResult& result)
{
    Housing::Room const* room = housing->GetRoom(roomGuid);
    if (!room || blueprint.Content.Rooms.empty())
        return;

    HousingBlueprintRoom const& source = blueprint.Content.Rooms.front();
    Housing::Room appearance;
    appearance.ThemeId = source.ThemeId;
    appearance.WallThemeId = source.WallThemeId;
    appearance.FloorThemeId = source.FloorThemeId;
    appearance.CeilingThemeId = source.CeilingThemeId;
    appearance.WallTextureId = source.WallTextureId;
    appearance.FloorTextureId = source.FloorTextureId;
    appearance.CeilingTextureId = source.CeilingTextureId;
    appearance.ColorOverride = source.ColorOverride;
    appearance.DoorTypeId = source.DoorTypeId;
    appearance.DoorSlot = source.DoorSlot;
    appearance.CeilingTypeId = source.CeilingTypeId;
    appearance.CeilingSlot = source.CeilingSlot;
    housing->SetRoomAppearance(roomGuid, appearance);

    Frame const frame = InteriorRoomFrame(room->GridX, room->GridY, room->FloorIndex, room->Orientation);
    DecorPool pool;
    for (HousingBlueprintDecor const& decor : blueprint.Content.Decor)
        if (decor.RoomIndex == 0)
            PlaceBlueprintDecor(player, housing, decor, roomGuid, frame, pool, result);

    housing->RecalculateBudgets();
    housing->SyncUpdateFields();
    result.InteriorChanged = true;
}
