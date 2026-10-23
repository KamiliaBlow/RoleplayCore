#include "AccountMgr.h"
#include "BattlenetAccountMgr.h"
#include "Config.h"
#include "Creature.h"
#include "DatabaseEnv.h"
#include "GameObject.h"
#include "Log.h"
#include "MapManager.h"
#include "MotionMaster.h"
#include "MovementPackets.h"
#include "MoveSpline.h"
#include "ObjectMgr.h"
#include "PhasingHandler.h"
#include "Player.h"
#include "Transport.h"
#include "World.h"
#include "Unit.h"
#include "Log.h"

#include "RolePlay.h"
#include "RoleplayPhaseMgr.h"
#include "DB2Stores.h"
#include "TransmogMgr.h"
#include <boost/algorithm/string.hpp>
#include <boost/algorithm/string/predicate.hpp>
#include <boost/tokenizer.hpp>
#include <G3D/Quat.h>
#include "QueryPackets.h"
#include "CreatureGroups.h"
#include "SpellInfo.h"
#include "TemporarySummon.h"
#include "WorldSession.h"
#include <CharacterCache.h>
#include "ObjectAccessor.h"
#include <cctype>
#include <sstream>


#pragma region Roleplay_MANAGER
Roleplay::Roleplay()
{
}

Roleplay::~Roleplay()
{
}

Roleplay* Roleplay::instance()
{
    static Roleplay instance;
    return &instance;
}

void Roleplay::LoadAllTables()
{
    uint32 oldMSTime = getMSTime();
    sRoleplayPhaseMgr.Load();
    LoadCreatureExtras();
    LoadCreatureTemplateExtras();
    LoadCustomNpcs();

    TC_LOG_INFO("server.loading", ">> Loaded Roleplay tables in {} ms", GetMSTimeDiffToNow(oldMSTime));
}

#pragma region CREATURE
void Roleplay::LoadCreatureExtras()
{
    // clear current storage
    _creatureExtraStore.clear();

    // guid, scale, id_creator_bnet, id_creator_player, id_modifier_bnet, id_modifier_player,
    // UNIX_TIMESTAMP(created), UNIX_TIMESTAMP(modified), phaseMask, displayLock, displayId,
    // nativeDisplayId, genderLock, gender, swim, gravity, fly
    RoleplayDatabasePreparedStatement* stmt = RoleplayDatabase.GetPreparedStatement(Roleplay_SEL_CREATUREEXTRA);
    PreparedQueryResult result = RoleplayDatabase.Query(stmt);

    if (!result)
        return;

    do
    {
        Field* fields = result->Fetch();
        CreatureExtraData data;
        uint64 guid = fields[0].GetUInt64();
        data.scale = fields[1].GetFloat();
        data.creatorBnetAccId = fields[2].GetUInt32();
        data.creatorPlayerId = fields[3].GetUInt64();
        data.modifierBnetAccId = fields[4].GetUInt32();
        data.modifierPlayerId = fields[5].GetUInt64();
        data.created = fields[6].GetUInt64();
        data.modified = fields[7].GetUInt64();
        data.phaseMask = fields[8].GetUInt32();
        data.displayLock = fields[9].GetBool();
        data.displayId = fields[10].GetUInt32();
        data.nativeDisplayId = fields[11].GetUInt32();
        data.genderLock = fields[12].GetBool();
        data.gender = fields[13].GetUInt8();
        data.swim = fields[14].GetBool();
        data.gravity = fields[15].GetBool();
        data.fly = fields[16].GetBool();

        _creatureExtraStore[guid] = data;
    } while (result->NextRow());
}

void Roleplay::LoadCreatureTemplateExtras()
{
    // clear current storage
    _creatureTemplateExtraStore.clear();

    // id_entry, disabled
    RoleplayDatabasePreparedStatement* stmt = RoleplayDatabase.GetPreparedStatement(Roleplay_SEL_CREATUREEXTRA_TEMPLATE);
    PreparedQueryResult result = RoleplayDatabase.Query(stmt);

    if (!result)
        return;

    do
    {
        Field* fields = result->Fetch();
        CreatureTemplateExtraData data;
        uint32 entry = fields[0].GetUInt32();
        data.disabled = fields[1].GetBool();

        _creatureTemplateExtraStore[entry] = data;
    } while (result->NextRow());
}

void Roleplay::CreatureSetEmote(Creature* creature, uint32 emoteId)
{
    uint32 spawnId = creature->GetSpawnId();
    auto addonData = &(sObjectMgr->_creatureAddonStore[spawnId]);
    addonData->emote = emoteId;
    creature->SetEmoteState(Emote(emoteId));
}

void Roleplay::CreatureSetMount(Creature* creature, uint32 mountId)
{
    uint32 spawnId = creature->GetSpawnId();
    auto addonData = &(sObjectMgr->_creatureAddonStore[spawnId]);
    addonData->mount = mountId;

    if (mountId)
        creature->Mount(mountId);
    else
        creature->Dismount();
}

void Roleplay::CreatureSetAuraToggle(Creature* creature, uint32 auraId, bool toggle)
{
    uint32 spawnId = creature->GetSpawnId();
    auto addonData = &(sObjectMgr->_creatureAddonStore[spawnId]);

    auto it = addonData->auras.begin();
    for (; it != addonData->auras.end(); it++)
    {
        if (*it == auraId && toggle) // refresh already existing aura
        {
            creature->AddAura(auraId, creature);
            return;
        }

        if (*it == auraId && !toggle) // we found auraId we want to remove
            break;
    }

    if (toggle)
    {
        creature->AddAura(auraId, creature);
        addonData->auras.push_back(auraId);
    }
    else if (it != addonData->auras.end())
    {
        creature->RemoveAura(auraId);
        addonData->auras.erase(it);
    }
}

void Roleplay::CreatureSetGravity(Creature* creature, bool toggle)
{
    _creatureExtraStore[creature->GetSpawnId()].gravity = toggle;
    creature->SetDisableGravity(!toggle);

    if (toggle)
    {
        if (!creature->IsInWater() || !creature->CanSwim())
            creature->GetMotionMaster()->MoveFall();
    }
}

void Roleplay::CreatureSetSwim(Creature* creature, bool toggle)
{
    _creatureExtraStore[creature->GetSpawnId()].swim = toggle;
    creature->SetSwim(toggle && creature->IsInWater());

    if (toggle)
    {
        if (!creature->HasUnitMovementFlag(MOVEMENTFLAG_SWIMMING))
            creature->SetUnitMovementFlags(MOVEMENTFLAG_SWIMMING);

        creature->SetSwim(creature->IsInWater() || CreatureCanFly(creature));
    }
    else
    {
        if (creature->HasUnitMovementFlag(MOVEMENTFLAG_SWIMMING))
            creature->RemoveUnitMovementFlag(MOVEMENTFLAG_SWIMMING);

        if (!CreatureCanFly(creature))
        {
            creature->SetSwim(false);

            if (creature->IsInWater())
                creature->GetMotionMaster()->MoveFall();
        }
    }
}

void Roleplay::CreatureSetFly(Creature* creature, bool toggle)
{
    _creatureExtraStore[creature->GetSpawnId()].fly = toggle;

    if (!creature->IsInWater())
    {
        creature->SetSwim(toggle);
    }

    // Just to be sure, send animation update, because commands such as .npc move will cancel it
    WorldPackets::Movement::MoveSplineSetFlag packet(toggle ? SMSG_MOVE_SPLINE_START_SWIM : SMSG_MOVE_SPLINE_STOP_SWIM);
    packet.MoverGUID = creature->GetGUID();
    creature->SendMessageToSet(packet.Write(), true);
}

void Roleplay::CreatureSetAnimKitId(Creature* creature, uint16 animKitId)
{
    uint32 spawnId = creature->GetSpawnId();
    auto addonData = &(sObjectMgr->_creatureAddonStore[spawnId]);
    addonData->aiAnimKit = animKitId;

    creature->SetAIAnimKitId(animKitId);
}

void Roleplay::CreatureSetModel(Creature* creature, uint32 displayId) {
    creature->SetDisplayId(displayId);

    _creatureExtraStore[creature->GetSpawnId()].displayLock = true;
    _creatureExtraStore[creature->GetSpawnId()].displayId = displayId;
    _creatureExtraStore[creature->GetSpawnId()].nativeDisplayId = displayId;
}

bool Roleplay::CreatureCanSwim(Creature const* creature)
{
    return _creatureExtraStore[creature->GetSpawnId()].swim;
}

bool Roleplay::CreatureCanWalk(Creature const* creature)
{
    // Todo: Check this. Based off Creature::UpdateMovementFlags since InhabitType seems to no longer exist.
    return !creature->IsAquatic();
    // return (creature->GetCreatureTemplate()->InhabitType & INHABIT_GROUND) != 0;
}

bool Roleplay::CreatureCanFly(Creature const* creature)
{
    auto it = _creatureExtraStore.find(creature->GetSpawnId());
    if (it == _creatureExtraStore.end())
    {
        // Todo: Check this. Based off Creature::UpdateMovementFlags since InhabitType seems to no longer exist.
        _creatureExtraStore[creature->GetSpawnId()].fly = creature->CanFly();
    }

    return _creatureExtraStore[creature->GetSpawnId()].fly;
}

void Roleplay::SetCreatureTemplateExtraDisabledFlag(uint32 entryId, bool disabled)
{
    auto it = _creatureTemplateExtraStore.find(entryId);
    if (it == _creatureTemplateExtraStore.end())
        return;

    _creatureTemplateExtraStore[entryId].disabled = disabled;

    // DB update
    RoleplayDatabasePreparedStatement* stmt = RoleplayDatabase.GetPreparedStatement(Roleplay_UPD_CREATUREEXTRA_TEMPLATE);
    stmt->setBool(0, disabled);
    stmt->setUInt32(1, entryId);
    RoleplayDatabase.Execute(stmt);
}

void Roleplay::SaveCreature(Creature* creature)
{
    creature->SaveToDB();

    // Save extra attached data if it exists
    auto it = _creatureExtraStore.find(creature->GetSpawnId());

    if (it != _creatureExtraStore.end())
    {
        int index = 0;
        CreatureExtraData data = it->second;
        RoleplayDatabasePreparedStatement* stmt = RoleplayDatabase.GetPreparedStatement(Roleplay_REP_CREATUREEXTRA);
        stmt->setUInt64(index++, creature->GetSpawnId());
        stmt->setFloat(index++, data.scale);
        stmt->setUInt32(index++, data.creatorBnetAccId);
        stmt->setUInt64(index++, data.creatorPlayerId);
        stmt->setUInt32(index++, data.modifierBnetAccId);
        stmt->setUInt64(index++, data.modifierPlayerId);
        stmt->setUInt64(index++, data.created);
        stmt->setUInt64(index++, data.modified);
        stmt->setUInt32(index++, data.phaseMask);
        stmt->setBool(index++, data.displayLock);
        stmt->setUInt32(index++, data.displayId);
        stmt->setUInt32(index++, data.nativeDisplayId);
        stmt->setBool(index++, data.genderLock);
        stmt->setUInt8(index++, data.gender);
        stmt->setBool(index++, data.swim);
        stmt->setBool(index++, data.gravity);
        stmt->setBool(index++, data.fly);

        RoleplayDatabase.Execute(stmt);
    }
}

void Roleplay::CreatureSetModifyHistory(Creature* creature, Player* modifier)
{
    if (!creature || !modifier)
        return;

    CreatureExtraData data = _creatureExtraStore[creature->GetSpawnId()];
    data.modifierBnetAccId = modifier->GetSession()->GetBattlenetAccountId();
    data.modifierPlayerId = modifier->GetGUID().GetCounter();
    data.modified = time(NULL);
    _creatureExtraStore[creature->GetSpawnId()] = data;
}

void Roleplay::CreatureMove(Creature* creature, float x, float y, float z, float o)
{
    if (!creature)
        return;

    // if (CreatureData const* data = sObjectMgr->GetCreatureData(creature->GetSpawnId()))
    // {
    //     const_cast<CreatureData*>(data)->posX = x;
    //     const_cast<CreatureData*>(data)->posY = y;
    //     const_cast<CreatureData*>(data)->posZ = z;
    //     const_cast<CreatureData*>(data)->orientation = o;
    // }
    // TODO: Check if this works
    creature->Relocate(x, y, z, o);

    //! If hovering, always increase our server-side Z position
    //! Client automatically projects correct position based on Z coord sent in monster move
    //! and UNIT_FIELD_HOVERHEIGHT sent in object updates
    if (creature->HasUnitMovementFlag(MOVEMENTFLAG_HOVER))
        z += creature->GetHoverOffset();
    creature->Relocate(x, y, z, o);
    //creature->GetMotionMaster()->Initialize();

    //if (creature->IsAlive())                            // dead creature will reset movement generator at respawn
    //{
    //    creature->setDeathState(JUST_DIED);
    //    creature->Respawn();
    //}

    WorldDatabasePreparedStatement* stmt = WorldDatabase.GetPreparedStatement(WORLD_UPD_CREATURE_POSITION);

    stmt->setFloat(0, x);
    stmt->setFloat(1, y);
    stmt->setFloat(2, z);
    stmt->setFloat(3, o);
    stmt->setUInt64(4, creature->GetSpawnId());

    WorldDatabase.Execute(stmt);

    TeleportLocation target{ .Location = { creature->GetMapId(), creature->GetPosition() } };

    creature->SendTeleportPacket(target);
}

void Roleplay::CreatureTurn(Creature* creature, float o)
{
    CreatureMove(creature, creature->GetPositionX(), creature->GetPositionY(), creature->GetPositionZ(), o);
}

void Roleplay::CreatureScale(Creature* creature, float scale)
{
    if (!creature)
        return;

    float maxScale = sConfigMgr->GetFloatDefault("Roleplay.Creature.MaxScale", 15.0f);
    float minScale = sConfigMgr->GetFloatDefault("Roleplay.Creature.MinScale", 0.0f);

    if (scale < minScale)
        scale = minScale;
    if (scale > maxScale)
        scale = maxScale;

    creature->SetObjectScale(scale);
    _creatureExtraStore[creature->GetSpawnId()].scale = scale;
}

void Roleplay::CreatureDelete(Creature* creature)
{
    creature->CombatStop();
    creature->DeleteFromDB(creature->GetSpawnId());
    // TODO: This should already happen in DeleteFromDB, check this.
    creature->AddObjectToRemoveList();
    // Remove spawn from custom npc spawns
    for (auto it : _customNpcStore)
    {
        if (it.second.templateId == creature->GetEntry()) {
            std::vector<uint64> spawns = it.second.spawns;
            spawns.erase(std::remove(spawns.begin(), spawns.end(), creature->GetSpawnId()), spawns.end());
            it.second.spawns = spawns;
            _customNpcStore[it.first] = it.second;
            break;
        }
    }
}

Creature* Roleplay::CreatureCreate(Player* creator, CreatureTemplate const* creatureTemplate)
{
    uint32 entryId = creatureTemplate->Entry;
    Map* map = creator->GetMap();

    if (Transport* trans = dynamic_cast<Transport*>(creator->GetTransport()))
    {
        ObjectGuid::LowType guid = sObjectMgr->GenerateCreatureSpawnId();
        CreatureData& data = sObjectMgr->NewOrExistCreatureData(guid);
        data.spawnId = guid;
        data.spawnGroupData = sObjectMgr->GetDefaultSpawnGroup();
        data.id = entryId;
        data.spawnPoint.Relocate(creator->GetTransOffsetX(), creator->GetTransOffsetY(), creator->GetTransOffsetZ(), creator->GetTransOffsetO());
        if (Creature* creature = trans->CreateNPCPassenger(guid, &data))
        {
            creature->SaveToDB(trans->GetGOInfo()->GetSpawnMap(), { map->GetDifficultyID() });
            sObjectMgr->AddCreatureToGrid(&data);
            return creature;
        }
    }

    Creature* creature = Creature::CreateCreature(entryId, map, creator->GetPosition(), InitialPhaseShift(creator));
    if (!creature)
        return creature;

    creature->SaveToDB(map->GetId(), { map->GetDifficultyID() });

    ObjectGuid::LowType db_guid = creature->GetSpawnId();

    sRoleplay->CreatureScale(creature, creature->GetObjectScale());
    sRoleplay->CreatureSetFly(creature, creature->CanFly());

    // To call _LoadGoods(); _LoadQuests(); CreateTrainerSpells()
    // current "creature" variable is deleted and created fresh new, otherwise old values might trigger asserts or cause undefined behavior
    creature->CleanupsBeforeDelete();
    delete creature;

    creature = Creature::CreateCreatureFromDB(db_guid, map, true, true);
    if (!creature)
        return creature;

    // Creation history and straight update
    CreatureExtraData data;
    data.scale = creatureTemplate->scale;
    data.creatorBnetAccId = creator->GetSession()->GetBattlenetAccountId();
    data.creatorPlayerId = creator->GetGUID().GetCounter();
    data.modifierBnetAccId = creator->GetSession()->GetBattlenetAccountId();
    data.modifierPlayerId = creator->GetGUID().GetCounter();
    data.created = time(NULL);
    data.modified = time(NULL);
    _creatureExtraStore[creature->GetSpawnId()] = data;

    int index = 0;
    RoleplayDatabasePreparedStatement* stmt = RoleplayDatabase.GetPreparedStatement(Roleplay_REP_CREATUREEXTRA);
    stmt->setUInt64(index++, creature->GetSpawnId());
    stmt->setFloat(index++, data.scale);
    stmt->setUInt32(index++, data.creatorBnetAccId);
    stmt->setUInt64(index++, data.creatorPlayerId);
    stmt->setUInt32(index++, data.modifierBnetAccId);
    stmt->setUInt64(index++, data.modifierPlayerId);
    stmt->setUInt64(index++, data.created);
    stmt->setUInt64(index++, data.modified);
    stmt->setUInt64(index++, data.phaseMask);
    stmt->setBool(index++, data.displayLock);
    stmt->setUInt32(index++, data.displayId);
    stmt->setUInt32(index++, data.nativeDisplayId);
    stmt->setBool(index++, data.genderLock);
    stmt->setUInt8(index++, data.gender);
    stmt->setBool(index++, data.swim);
    stmt->setBool(index++, data.gravity);
    stmt->setBool(index++, data.fly);

    RoleplayDatabase.Execute(stmt);

    sObjectMgr->AddCreatureToGrid(sObjectMgr->GetCreatureData(db_guid));
    return creature;
}

void Roleplay::CreatureRefresh(Creature* creature)
{
    if (!creature)
        return;

    Map* map = creature->GetMap();
    map->GetObjectsStore().Remove<Creature>(creature);
    creature->DestroyForNearbyPlayers();

    auto newGuidLow = map->GenerateLowGuid<HighGuid::Creature>();
    auto newObjectGuid = ObjectGuid::Create<HighGuid::Creature>(map->GetId(), creature->GetEntry(), newGuidLow);

    map->GetObjectsStore().Insert<Creature>(creature);
}

CreatureExtraData const* Roleplay::GetCreatureExtraData(uint64 guid)
{
    auto it = _creatureExtraStore.find(guid);

    if (it != _creatureExtraStore.end())
    {
        return &it->second;
    }
    else
    {
        return nullptr;
    }
}

CreatureTemplateExtraData const* Roleplay::GetCreatureTemplateExtraData(uint32 entry)
{
    auto it = _creatureTemplateExtraStore.find(entry);

    if (it != _creatureTemplateExtraStore.end())
    {
        return &it->second;
    }
    else
    {
        return nullptr;
    }
}

Creature* Roleplay::GetAnyCreature(ObjectGuid::LowType lowguid)
{
    auto data = sObjectMgr->GetCreatureData(lowguid);
    if (!data)
    {
        TC_LOG_DEBUG("roleplay", "RoleplayMgr::GetAnyCreature failed to find creatureData for GUID: " SZFMTD, lowguid);
        return nullptr;
    }

    auto objectGuid = ObjectGuid::Create<HighGuid::Creature>(data->mapId, data->id, lowguid);
    Map* map = sMapMgr->FindMap(data->mapId, 0);

    if (!map)
    {
        TC_LOG_DEBUG("roleplay", "RoleplayMgr::GetAnyCreature failed to find map %u for GUID: " SZFMTD, data->mapId, lowguid);
        return nullptr;
    }

    Creature* creature = map->GetCreature(objectGuid);

    // guid is DB guid of creature
    if (!creature)
    {
        auto bounds = map->GetCreatureBySpawnIdStore().equal_range(lowguid);
        if (bounds.first == bounds.second)
        {
            TC_LOG_DEBUG("roleplay", "RoleplayMgr::GetAnyCreature failed to find creature in spawnidstore on map %u for GUID: " SZFMTD, data->mapId, lowguid);
            return nullptr;
        }

        return bounds.first->second;
    }

    return creature;
}

Creature* Roleplay::GetAnyCreature(Map* map, ObjectGuid::LowType lowguid, uint32 entry)
{
    auto objectGuid = ObjectGuid::Create<HighGuid::Creature>(map->GetId(), entry, lowguid);

    Creature* creature = map->GetCreature(objectGuid);

    // guid is DB guid of creature
    if (!creature)
    {
        auto bounds = map->GetCreatureBySpawnIdStore().equal_range(lowguid);
        if (bounds.first == bounds.second)
            return nullptr;

        return bounds.first->second;
    }

    return creature;
}

Unit* Roleplay::GetAnyUnit(ObjectGuid::LowType guidLow)
{
    Creature* creature = GetAnyCreature(guidLow);
    if (creature) {
        return creature;
    }

    Player* player = ObjectAccessor::FindPlayerByLowGUID(guidLow);
    if (player) {
        return player;
    }

    return nullptr;
}

void Roleplay::SetCreatureSelectionForPlayer(ObjectGuid::LowType playerId, ObjectGuid::LowType creatureId)
{
    _playerExtraDataStore[playerId].selectedCreatureGuid = creatureId;
}

ObjectGuid::LowType Roleplay::GetSelectedCreatureGuidFromPlayer(ObjectGuid::LowType playerId)
{
    return _playerExtraDataStore[playerId].selectedCreatureGuid;
}
#pragma endregion

#pragma region MISC

std::string Roleplay::GetMapName(uint32 mapId)
{
    const MapEntry* map = sMapStore.LookupEntry(mapId);

    if (map)
        return map->MapName[sWorld->GetDefaultDbcLocale()];
    else
        return "Unknown";
}

std::string Roleplay::GetChatLinkKey(std::string const& src, std::string type)
{
    if (src.empty())
        return "";

    std::string typePart = "|" + type + ":";
    std::string key = "";
    std::size_t pos = src.find(typePart); // find start pos of "|Hkeytype:" fragment first

    // Check for plain number first
    std::string plainNumber = src;
    boost::trim(plainNumber); // trim spaces
    plainNumber = plainNumber.substr(0, plainNumber.find(' ')); // get first token in case src had multiple ones
    if (isNumeric(plainNumber.c_str()))
        return plainNumber;

    // Do ChatLink check
    if (pos != std::string::npos)
    {
        auto it = src.begin();
        std::advance(it, pos + typePart.length());

        // if key part iteration ends without encountering ':' or '|',
        // then link was malformed and we return empty string later on
        for (; it != src.end(); it++)
        {
            char c = *it;

            if (c == ':' || c == '|')
                return key;

            key += c;
        }
    }

    return "";
}

std::string Roleplay::ToDateTimeString(time_t t)
{
    tm aTm;
    localtime_r(&t, &aTm);
    //       YYYY   year
    //       MM     month (2 digits 01-12)
    //       DD     day (2 digits 01-31)
    //       HH     hour (2 digits 00-23)
    //       MM     minutes (2 digits 00-59)
    //       SS     seconds (2 digits 00-59)
    char buf[20];
    snprintf(buf, 20, "%04d-%02d-%02d %02d:%02d:%02d", aTm.tm_year + 1900, aTm.tm_mon + 1, aTm.tm_mday, aTm.tm_hour, aTm.tm_min, aTm.tm_sec);
    return std::string(buf);
}

std::string Roleplay::ToDateString(time_t t)
{
    tm aTm;
    localtime_r(&t, &aTm);
    //       YYYY   year
    //       MM     month (2 digits 01-12)
    //       DD     day (2 digits 01-31)
    //       HH     hour (2 digits 00-23)
    //       MM     minutes (2 digits 00-59)
    //       SS     seconds (2 digits 00-59)
    char buf[14];
    snprintf(buf, 14, "%04d-%02d-%02d", aTm.tm_year + 1900, aTm.tm_mon + 1, aTm.tm_mday);
    return std::string(buf);
}

#pragma endregion

#pragma region npcappearance
void Roleplay::LoadCustomNpcSpawn(uint32 templateId, ObjectGuid::LowType spawn)
{
    for (auto it : _customNpcStore)
    {
        if (it.second.templateId == templateId) {
            TC_LOG_DEBUG("roleplay", "ROLEPLAY: Identified custom npc key '%s' for entry id '%u', adding spawn '%lu'", it.second.key, templateId, spawn);
            it.second.spawns.push_back(spawn);
            _customNpcStore[it.first] = it.second;
            break;
        }
    }
}

void Roleplay::LoadCustomNpcs()
{
    // clear current storage
    _customNpcStore.clear();

    uint32 oldMSTime = getMSTime();
    QueryResult result = RoleplayDatabase.Query("SELECT `Key`, Entry FROM custom_npcs");
    if (!result)
    {
        TC_LOG_INFO("server.loading", ">> Loaded 0 custom ncps. DB table `custom_npcs` is empty!");
        return;
    }
    uint32 count = 0;
    do
    {
        Field* fields = result->Fetch();
        uint32 entry = fields[1].GetUInt32();

        CustomNpcData npcData;
        npcData.templateId = entry;
        npcData.key = fields[0].GetString();
        _customNpcStore[npcData.key] = npcData;

        ++count;
    } while (result->NextRow());

    if (RoleplayDatabase.Query("SHOW TABLES LIKE 'custom_npc_owners'"))
    {
        if (QueryResult ownerResult = RoleplayDatabase.Query("SELECT `Key`, owner_bnet_account_id, owner_alias FROM custom_npc_owners"))
        {
            do
            {
                Field* fields = ownerResult->Fetch();
                std::string key = fields[0].GetString();
                auto itr = _customNpcStore.find(key);
                if (itr == _customNpcStore.end())
                    continue;

                itr->second.ownerBnetAccountId = fields[1].GetUInt32();
                itr->second.ownerAlias = fields[2].GetString();
            } while (ownerResult->NextRow());
        }
    }

    TC_LOG_INFO("server.loading", ">> Loaded {} custom npcs in {} ms", count, GetMSTimeDiffToNow(oldMSTime));
}

bool Roleplay::PopulateOutfitFromPlayer(Player* player, CreatureOutfit& co)
{
    if (!player)
        return false;

    co.race = player->GetRace();
    co.Class = player->GetClass();
    co.npcsoundsid = 0;
    co.SpellVisualKitID = 0;

    auto* maleModel = sDB2Manager.GetChrModel(co.race, GENDER_MALE);
    auto* femaleModel = sDB2Manager.GetChrModel(co.race, GENDER_FEMALE);
    if (!maleModel || !femaleModel)
        return false;

    co.gender = player->GetGender();
    switch (co.gender)
    {
    case GENDER_FEMALE: co.displayId = femaleModel->DisplayID; break;
    case GENDER_MALE:   co.displayId = maleModel->DisplayID; break;
    default: return false;
    }

    co.Customizations.clear();
    for (auto&& customization : player->m_playerData->Customizations)
        co.Customizations.push_back(customization);

    for (EquipmentSlots slot : CreatureOutfit::item_slots)
        co.outfitdisplays[slot] = GetPlayerVisibleSlotDisplayId(player, slot);

    co.guild = player->GetGuildId();
    return true;
}

bool Roleplay::PopulateOutfitFromCreature(Creature* creature, CreatureOutfit& co)
{
    if (!creature)
        return false;

    std::shared_ptr<CreatureOutfit> const& sourceOutfit = creature->GetOutfit();
    if (!sourceOutfit)
        return false;

    co.race = sourceOutfit->race;
    co.Class = sourceOutfit->Class;
    co.gender = sourceOutfit->gender;
    co.displayId = sourceOutfit->displayId;
    co.npcsoundsid = sourceOutfit->npcsoundsid;
    co.SpellVisualKitID = sourceOutfit->SpellVisualKitID;
    co.Customizations = sourceOutfit->Customizations;
    co.guild = sourceOutfit->guild;

    for (EquipmentSlots slot : CreatureOutfit::item_slots)
        co.outfitdisplays[slot] = sourceOutfit->outfitdisplays[slot];

    return true;
}

int32 Roleplay::GetPlayerVisibleSlotDisplayId(Player const* player, EquipmentSlots slot) const
{
    if (!player)
        return 0;

    if (slot < player->m_playerData->VisibleItems.size())
    {
        UF::VisibleItem const& visible = player->m_playerData->VisibleItems[slot];
        if (visible.ItemModifiedAppearanceID)
        {
            if (ItemModifiedAppearanceEntry const* itemModifiedAppearance = sItemModifiedAppearanceStore.LookupEntry(visible.ItemModifiedAppearanceID))
                if (ItemAppearanceEntry const* itemAppearance = sItemAppearanceStore.LookupEntry(itemModifiedAppearance->ItemAppearanceID))
                    return itemAppearance->ItemDisplayInfoID;
        }
    }

    if (Item const* item = player->GetItemByPos(INVENTORY_SLOT_BAG_0, slot))
    {
        int32 displayId = item->GetDisplayId(player);
        if (displayId)
            return displayId;

        for (ItemModifiedAppearanceEntry const* appearanceMod : sItemModifiedAppearanceStore)
        {
            if (uint32(appearanceMod->ItemID) == item->GetEntry() && appearanceMod->OrderIndex == 0)
            {
                if (ItemAppearanceEntry const* itemAppearance = sItemAppearanceStore.LookupEntry(appearanceMod->ItemAppearanceID))
                    return itemAppearance->ItemDisplayInfoID;
                break;
            }
        }
    }

    return 0;
}

bool Roleplay::PopulateEquipmentItemFromPlayer(Player const* player, EquipmentSlots visibleSlot, EquipmentItem& item) const
{
    item.ItemId = 0;
    item.AppearanceModId = 0;
    item.ItemVisual = 0;

    if (!player)
        return false;

    if (visibleSlot < player->m_playerData->VisibleItems.size())
    {
        UF::VisibleItem const& visible = player->m_playerData->VisibleItems[visibleSlot];
        if (visible.ItemModifiedAppearanceID)
        {
            if (ItemModifiedAppearanceEntry const* modifiedAppearance = sItemModifiedAppearanceStore.LookupEntry(visible.ItemModifiedAppearanceID))
            {
                item.ItemId = uint32(modifiedAppearance->ItemID);
                item.AppearanceModId = uint16(modifiedAppearance->ItemAppearanceModifierID);
                return item.ItemId != 0;
            }
        }
    }

    if (Item const* equipped = player->GetItemByPos(INVENTORY_SLOT_BAG_0, visibleSlot))
    {
        item.ItemId = equipped->GetEntry();
        return item.ItemId != 0;
    }

    return false;
}

void Roleplay::PopulateEquipmentFromUnit(Unit const* unit, EquipmentInfo& equip) const
{
    if (!unit)
        return;

    if (Player const* player = unit->ToPlayer())
    {
        PopulateEquipmentItemFromPlayer(player, EQUIPMENT_SLOT_MAINHAND, equip.Items[0]);
        PopulateEquipmentItemFromPlayer(player, EQUIPMENT_SLOT_OFFHAND, equip.Items[1]);
        PopulateEquipmentItemFromPlayer(player, EQUIPMENT_SLOT_RANGED, equip.Items[2]);
        return;
    }

    for (uint8 slot = 0; slot < MAX_EQUIPMENT_ITEMS; ++slot)
    {
        equip.Items[slot].ItemId = unit->GetVirtualItemId(slot);
        equip.Items[slot].AppearanceModId = unit->GetVirtualItemAppearanceMod(slot);
        equip.Items[slot].ItemVisual = 0;
    }
}

uint32 Roleplay::AllocateNextOutfitId()
{
    uint32 outfitId = sConfigMgr->GetInt64Default("Roleplay.CustomNpc.OutfitIdStart", 200001) + sObjectMgr->_creatureOutfitStore.size();
    if (!sObjectMgr->_creatureOutfitStore.empty())
    {
        using pairtype = std::pair<uint32, std::shared_ptr<CreatureOutfit>>;
        outfitId = std::max_element(sObjectMgr->_creatureOutfitStore.begin(), sObjectMgr->_creatureOutfitStore.end(),
            [](pairtype a, pairtype b) { return a.second->id < b.second->id; })->second->id + 1;
    }
    return outfitId;
}

uint32 Roleplay::AllocateNextCustomNpcTemplateId()
{
    uint32 npcCreatureTemplateId = sConfigMgr->GetInt64Default("Roleplay.CustomNpc.CreatureTemplateIdStart", 400000);
    if (!sRoleplay->GetCustomNpcContainer().empty())
    {
        CustomNpcDataContainer const& store = sRoleplay->GetCustomNpcContainer();
        using pairtype = std::pair<std::string, CustomNpcData>;
        npcCreatureTemplateId = std::max_element(store.begin(), store.end(),
            [](pairtype a, pairtype b) { return a.second.templateId < b.second.templateId; })->second.templateId + 1;
    }
    return npcCreatureTemplateId;
}

bool Roleplay::LoadOutfitRowFromDb(uint32 outfitId)
{
    QueryResult result = WorldDatabase.PQuery(
        "SELECT entry, npcsoundsid, race, class, gender, spellvisualkitid, customizations, "
        "head, head_appearance, shoulders, shoulders_appearance, body, body_appearance, chest, chest_appearance, waist, waist_appearance, "
        "legs, legs_appearance, feet, feet_appearance, wrists, wrists_appearance, hands, hands_appearance, tabard, tabard_appearance, back, back_appearance, "
        "guildid FROM creature_template_outfits WHERE entry = {}", outfitId);

    if (!result)
        return false;

    std::shared_ptr<CreatureOutfit> co;
    std::string error;
    if (!LoadCreatureOutfitFromFields(result->Fetch(), co, error))
    {
        TC_LOG_ERROR("roleplay", "ROLEPLAY: LoadOutfitRowFromDb({}) failed: {}", outfitId, error);
        return false;
    }

    sObjectMgr->_creatureOutfitStore[co->id] = std::move(co);
    return true;
}

void Roleplay::InitializeDefaultCustomNpcTemplate(CreatureTemplate& creatureTemplate, std::string const& key, uint32 entry)
{
    creatureTemplate.Entry = entry;

    for (uint8 i = 0; i < MAX_KILL_CREDIT; ++i)
        creatureTemplate.KillCredit[i] = 0;

    creatureTemplate.Name = key;
    creatureTemplate.RequiredExpansion = EXPANSION_CLASSIC;
    creatureTemplate.VignetteID = 0;
    creatureTemplate.faction = 35;
    creatureTemplate.npcflag = 0;
    creatureTemplate.speed_walk = 1.0f;
    creatureTemplate.speed_run = 1.14286f;
    creatureTemplate.scale = 1.0f;
    creatureTemplate.Classification = CreatureClassifications::Normal;
    creatureTemplate.dmgschool = 0;
    creatureTemplate.BaseAttackTime = 0;
    creatureTemplate.RangeAttackTime = 0;
    creatureTemplate.BaseVariance = 1;
    creatureTemplate.RangeVariance = 1;
    creatureTemplate.unit_class = UnitClass::UNIT_CLASS_WARRIOR;
    creatureTemplate.unit_flags = 0;
    creatureTemplate.unit_flags2 = 0;
    creatureTemplate.unit_flags3 = 0;
    creatureTemplate.family = CREATURE_FAMILY_NONE;
    creatureTemplate.trainer_class = 0;
    creatureTemplate.type = CreatureType::CREATURE_TYPE_HUMANOID;

    for (uint8 i = SPELL_SCHOOL_HOLY; i < MAX_SPELL_SCHOOL; ++i)
        creatureTemplate.resistance[i] = 0;

    for (uint8 i = 0; i < MAX_CREATURE_SPELLS; ++i)
        creatureTemplate.spells[i] = 0;

    creatureTemplate.VehicleId = 0;
    creatureTemplate.AIName = "";
    creatureTemplate.MovementType = 0;
    creatureTemplate.ModExperience = 1.0f;
    creatureTemplate.RacialLeader = false;
    creatureTemplate.movementId = 100;
    creatureTemplate.WidgetSetID = 0;
    creatureTemplate.WidgetSetUnitConditionID = 0;
    creatureTemplate.RegenHealth = 1;
    creatureTemplate.CreatureImmunitiesId = 0;
    creatureTemplate.flags_extra = 0;
    creatureTemplate.ScriptID = sObjectMgr->GetScriptId("");
}

bool Roleplay::PopulateDefaultOutfit(CreatureOutfit& co, Races race, Gender gender)
{
    co.race = race;
    co.Class = CLASS_WARRIOR;
    co.npcsoundsid = 0;
    co.SpellVisualKitID = 0;
    co.guild = 0;
    co.Customizations.clear();

    ChrModelEntry const* maleModel = sDB2Manager.GetChrModel(race, GENDER_MALE);
    ChrModelEntry const* femaleModel = sDB2Manager.GetChrModel(race, GENDER_FEMALE);
    if (!maleModel || !femaleModel)
        return false;

    co.gender = gender;
    switch (co.gender)
    {
    case GENDER_FEMALE: co.displayId = femaleModel->DisplayID; break;
    case GENDER_MALE:   co.displayId = maleModel->DisplayID; break;
    default: return false;
    }

    for (EquipmentSlots slot : CreatureOutfit::item_slots)
        co.outfitdisplays[slot] = 0;

    return true;
}

void Roleplay::InitializeDefaultCustomNpcAddon(uint32 templateId)
{
    CreatureAddon addon;
    addon.PathId = 0;
    addon.mount = 0;
    addon.standState = UNIT_STAND_STATE_STAND;
    addon.animTier = 0;
    addon.visFlags = 0;
    addon.sheathState = SHEATH_STATE_UNARMED;
    addon.pvpFlags = 0;
    addon.emote = 0;
    addon.aiAnimKit = 0;
    addon.movementAnimKit = 0;
    addon.meleeAnimKit = 0;
    addon.visibilityDistanceType = VisibilityDistanceType::Normal;
    sObjectMgr->_creatureTemplateAddonStore[templateId] = addon;
    SaveNpcCreatureTemplateAddonToDb(templateId, addon);
}

void Roleplay::ApplyCreatureAddonState(Creature* creature, CreatureAddon const& addon)
{
    if (!creature)
        return;

    creature->SetStandState(UnitStandStateType(addon.standState));
    creature->ReplaceAllVisFlags(UnitVisFlags(addon.visFlags));
    creature->SetAnimTier(AnimTier(addon.animTier), false);
    creature->SetSheath(SheathState(addon.sheathState));
    creature->ReplaceAllPvpFlags(UnitPVPStateFlags(addon.pvpFlags));

    if (addon.emote)
        creature->SetEmoteState(Emote(addon.emote));
    else
        creature->SetEmoteState(EMOTE_ONESHOT_NONE);

    if (addon.mount)
        creature->Mount(addon.mount);
    else
        creature->Dismount();

    creature->SetAIAnimKitId(addon.aiAnimKit);
    creature->SetMovementAnimKitId(addon.movementAnimKit);
    creature->SetMeleeAnimKitId(addon.meleeAnimKit);
}

int32 Roleplay::GetItemDisplayIdFromTemplate(ItemTemplate const* item, uint32 appearanceModId) const
{
    if (!item)
        return 0;

    if (ItemModifiedAppearanceEntry const* modifiedAppearance = TransmogMgr::GetItemModifiedAppearance(item->GetId(), appearanceModId))
        if (ItemAppearanceEntry const* itemAppearance = sItemAppearanceStore.LookupEntry(modifiedAppearance->ItemAppearanceID))
            return itemAppearance->ItemDisplayInfoID;

    return 0;
}

bool Roleplay::CreateBlankCustomNpc(Player* owner, std::string const& key, std::string const& displayName)
{
    if (!owner)
    {
        TC_LOG_ERROR("roleplay", "ROLEPLAY: Custom NPC '{}' blank creation failed: owner is null.", key);
        return false;
    }

    TC_LOG_INFO("roleplay", "ROLEPLAY: Creating blank custom NPC '{}' for owner '{}'.", key, owner->GetName());

    std::shared_ptr<CreatureOutfit> co(new CreatureOutfit());
    uint32 outfitId = AllocateNextOutfitId();
    co->id = outfitId;

    if (!PopulateDefaultOutfit(*co, RACE_HUMAN, GENDER_MALE))
    {
        TC_LOG_ERROR("roleplay", "ROLEPLAY: Custom NPC '{}' blank creation failed: unable to populate default outfit.", key);
        return false;
    }

    sObjectMgr->_creatureOutfitStore[outfitId] = std::move(co);

    CreatureTemplate creatureTemplate;
    uint32 npcCreatureTemplateId = AllocateNextCustomNpcTemplateId();
    InitializeDefaultCustomNpcTemplate(creatureTemplate, displayName, npcCreatureTemplateId);
    creatureTemplate.Models.push_back(CreatureModel(outfitId, 1.0f, 1.0f));

    sObjectMgr->CheckCreatureTemplate(&creatureTemplate);
    SaveNpcCreatureTemplateToDb(creatureTemplate);
    sObjectMgr->_creatureTemplateStore[creatureTemplate.Entry] = std::move(creatureTemplate);

    if (sWorld->getBoolConfig(CONFIG_CACHE_DATA_QUERIES))
        sObjectMgr->_creatureTemplateStore[npcCreatureTemplateId].InitializeQueryData();

    EquipmentInfo equipmentInfo{};
    sObjectMgr->_equipmentInfoStore[npcCreatureTemplateId][1] = equipmentInfo;

    CustomNpcData npcData;
    npcData.key = key;
    npcData.templateId = npcCreatureTemplateId;
    PopulateCustomNpcOwner(npcData, owner);
    _customNpcStore[key] = npcData;

    SaveNpcOutfitToDb(npcCreatureTemplateId, 1);
    SaveCustomNpcDataToDb(npcData);
    SaveCustomNpcOwnerToDb(npcData);
    SaveNpcEquipmentInfoToDb(creatureTemplate.Entry, 1);
    InitializeDefaultCustomNpcAddon(creatureTemplate.Entry);

    TC_LOG_INFO("roleplay", "ROLEPLAY: Blank custom NPC '{}' created successfully (entry={}, outfit={}).",
        key, npcCreatureTemplateId, outfitId);
    return true;
}

void Roleplay::PopulateCustomNpcOwner(CustomNpcData& npcData, Player* owner) const
{
    if (!owner || !owner->GetSession())
        return;

    npcData.ownerBnetAccountId = owner->GetSession()->GetBattlenetAccountId();
    if (!npcData.ownerBnetAccountId)
        return;

    std::string alias;
    if (Battlenet::AccountMgr::GetName(npcData.ownerBnetAccountId, alias))
        npcData.ownerAlias = alias;
    else
        npcData.ownerAlias = owner->GetSession()->GetAccountName();
}

std::string Roleplay::GenerateCustomNpcKey(Player const* owner) const
{
    if (!owner || !owner->GetSession())
        return "";

    uint32 bnetAccountId = owner->GetSession()->GetBattlenetAccountId();
    if (!bnetAccountId)
        return "";

    std::string prefix = std::to_string(bnetAccountId) + "-";
    uint32 nextNumber = 1;

    for (auto const& pair : _customNpcStore)
    {
        CustomNpcData const& npc = pair.second;
        if (npc.ownerBnetAccountId && npc.ownerBnetAccountId != bnetAccountId)
            continue;

        if (!boost::starts_with(npc.key, prefix))
            continue;

        std::string suffix = npc.key.substr(prefix.size());
        if (suffix.empty() || !std::all_of(suffix.begin(), suffix.end(), [](char ch) { return std::isdigit(static_cast<unsigned char>(ch)); }))
            continue;

        uint32 number = uint32(std::stoul(suffix));
        nextNumber = std::max(nextNumber, number + 1);
    }

    std::string key;
    do
    {
        key = prefix + std::to_string(nextNumber++);
    } while (CustomNpcNameExists(key));

    return key;
}

bool Roleplay::CreateCustomNpcFromPlayer(Player* player, std::string const& key, std::string const& displayName, Player* owner)
{
    if (!player)
    {
        TC_LOG_ERROR("roleplay", "ROLEPLAY: Custom NPC '{}' creation failed: player is null.", key);
        return false;
    }

    TC_LOG_INFO("roleplay", "ROLEPLAY: Creating custom NPC '{}' from player '{}' ({})",
        key, player->GetName(), player->GetGUID().ToString());

    std::shared_ptr<CreatureOutfit> co(new CreatureOutfit());
    uint32 outfitId = AllocateNextOutfitId();
    co->id = outfitId;

    if (!PopulateOutfitFromPlayer(player, *co))
    {
        TC_LOG_ERROR("roleplay", "ROLEPLAY: Custom NPC '{}' creation failed: unable to populate outfit from player.", key);
        return false;
    }

    TC_LOG_INFO("roleplay", "ROLEPLAY: Custom NPC '{}' allocated outfit {} (race={}, gender={}, display={}).",
        key, outfitId, uint32(co->race), uint32(co->gender), co->displayId);

    sObjectMgr->_creatureOutfitStore[outfitId] = std::move(co);

    CreatureTemplate creatureTemplate;
    uint32 npcCreatureTemplateId = AllocateNextCustomNpcTemplateId();
    InitializeDefaultCustomNpcTemplate(creatureTemplate, displayName, npcCreatureTemplateId);
    TC_LOG_INFO("roleplay", "ROLEPLAY: Custom NPC '{}' allocated creature template {}.", key, npcCreatureTemplateId);

    creatureTemplate.Models.push_back(CreatureModel(outfitId, 1.0f, 1.0f));

    sObjectMgr->CheckCreatureTemplate(&creatureTemplate);
    SaveNpcCreatureTemplateToDb(creatureTemplate);
    sObjectMgr->_creatureTemplateStore[creatureTemplate.Entry] = std::move(creatureTemplate);

    EquipmentInfo equipmentInfo;
    PopulateEquipmentFromUnit(player, equipmentInfo);
    sObjectMgr->_equipmentInfoStore[creatureTemplate.Entry][1] = equipmentInfo;

    CustomNpcData npcData;
    npcData.key = key;
    npcData.templateId = npcCreatureTemplateId;
    PopulateCustomNpcOwner(npcData, owner ? owner : player);
    _customNpcStore[key] = npcData;

    SaveNpcOutfitToDb(npcCreatureTemplateId, 1);
    SaveCustomNpcDataToDb(npcData);
    SaveCustomNpcOwnerToDb(npcData);
    SaveNpcEquipmentInfoToDb(creatureTemplate.Entry, 1);
    InitializeDefaultCustomNpcAddon(creatureTemplate.Entry);

    TC_LOG_INFO("roleplay", "ROLEPLAY: Custom NPC '{}' created successfully (entry={}, outfit={}).",
        key, npcCreatureTemplateId, outfitId);
    return true;
}

bool Roleplay::CreateCustomNpcFromCreature(Creature* creature, std::string const& key, std::string const& displayName, Player* owner)
{
    if (!creature)
    {
        TC_LOG_ERROR("roleplay", "ROLEPLAY: Custom NPC '{}' creation failed: creature is null.", key);
        return false;
    }

    if (!creature->GetOutfit())
    {
        TC_LOG_ERROR("roleplay", "ROLEPLAY: Custom NPC '{}' creation failed: creature {} has no outfit.", key, creature->GetGUID().ToString());
        return false;
    }

    TC_LOG_INFO("roleplay", "ROLEPLAY: Creating custom NPC '{}' from creature '{}' ({})",
        key, creature->GetName(), creature->GetGUID().ToString());

    std::shared_ptr<CreatureOutfit> co(new CreatureOutfit());
    uint32 outfitId = AllocateNextOutfitId();
    co->id = outfitId;

    if (!PopulateOutfitFromCreature(creature, *co))
    {
        TC_LOG_ERROR("roleplay", "ROLEPLAY: Custom NPC '{}' creation failed: unable to populate outfit from creature.", key);
        return false;
    }

    sObjectMgr->_creatureOutfitStore[outfitId] = std::move(co);

    CreatureTemplate creatureTemplate;
    uint32 npcCreatureTemplateId = AllocateNextCustomNpcTemplateId();
    InitializeDefaultCustomNpcTemplate(creatureTemplate, displayName, npcCreatureTemplateId);
    creatureTemplate.Models.push_back(CreatureModel(outfitId, creature->GetObjectScale(), 1.0f));

    sObjectMgr->CheckCreatureTemplate(&creatureTemplate);
    SaveNpcCreatureTemplateToDb(creatureTemplate);
    sObjectMgr->_creatureTemplateStore[creatureTemplate.Entry] = std::move(creatureTemplate);

    EquipmentInfo equipmentInfo;
    PopulateEquipmentFromUnit(creature, equipmentInfo);
    sObjectMgr->_equipmentInfoStore[creatureTemplate.Entry][1] = equipmentInfo;

    CustomNpcData npcData;
    npcData.key = key;
    npcData.templateId = npcCreatureTemplateId;
    PopulateCustomNpcOwner(npcData, owner);
    _customNpcStore[key] = npcData;

    SaveNpcOutfitToDb(npcCreatureTemplateId, 1);
    SaveCustomNpcDataToDb(npcData);
    SaveCustomNpcOwnerToDb(npcData);
    SaveNpcEquipmentInfoToDb(creatureTemplate.Entry, 1);
    InitializeDefaultCustomNpcAddon(creatureTemplate.Entry);

    TC_LOG_INFO("roleplay", "ROLEPLAY: Custom NPC '{}' created from creature (entry={}, outfit={}).", key, npcCreatureTemplateId, outfitId);
    return true;
}

bool Roleplay::ImportCustomNpcFromEntry(uint32 entry, std::string const& key)
{
    if (CustomNpcNameExists(key))
    {
        TC_LOG_ERROR("roleplay", "ROLEPLAY: Import failed: custom NPC key '{}' already exists.", key);
        return false;
    }

    CreatureTemplate const* cTemplate = sObjectMgr->GetCreatureTemplate(entry);
    if (!cTemplate)
    {
        TC_LOG_ERROR("roleplay", "ROLEPLAY: Import failed: creature template {} not found.", entry);
        return false;
    }

    bool hasOutfit = false;
    for (CreatureModel const& model : cTemplate->Models)
    {
        if (CreatureOutfit::IsFake(model.CreatureDisplayID))
        {
            hasOutfit = true;
            break;
        }
    }

    if (!hasOutfit)
    {
        TC_LOG_ERROR("roleplay", "ROLEPLAY: Import failed: entry {} has no fake outfit model.", entry);
        return false;
    }

    for (auto const& pair : _customNpcStore)
    {
        if (pair.second.templateId == entry)
        {
            TC_LOG_ERROR("roleplay", "ROLEPLAY: Import failed: entry {} is already registered as custom NPC '{}'.", entry, pair.first);
            return false;
        }
    }

    CustomNpcData npcData;
    npcData.key = key;
    npcData.templateId = entry;
    _customNpcStore[key] = npcData;
    SaveCustomNpcDataToDb(npcData);

    TC_LOG_INFO("roleplay", "ROLEPLAY: Imported entry {} as custom NPC '{}'.", entry, key);
    return true;
}

bool Roleplay::ApplyCustomNpcToCreature(std::string const& key, Creature* creature, uint8 variationId)
{
    if (!creature || !CustomNpcNameExists(key))
        return false;

    if (variationId < 1)
        variationId = 1;

    uint32 templateId = _customNpcStore[key].templateId;
    CreatureTemplate const& cTemplate = sObjectMgr->_creatureTemplateStore[templateId];
    if (cTemplate.Models.empty())
    {
        TC_LOG_ERROR("roleplay", "ROLEPLAY: Apply custom NPC '{}' failed: template {} has no models.", key, templateId);
        return false;
    }

    if (variationId > cTemplate.Models.size())
    {
        TC_LOG_ERROR("roleplay", "ROLEPLAY: Apply custom NPC '{}' failed: variation {} is outside model count {}.",
            key, uint32(variationId), cTemplate.Models.size());
        return false;
    }

    ApplyCustomNpcStateToCreature(creature, templateId, variationId);

    if (ObjectGuid::LowType spawnId = creature->GetSpawnId())
        LoadCustomNpcSpawn(templateId, spawnId);

    TC_LOG_INFO("roleplay", "ROLEPLAY: Applied custom NPC '{}' variation {} to creature {}.", key, variationId, creature->GetSpawnId());
    return true;
}

bool Roleplay::CanEditCustomNpc(std::string const& key, Player* player, bool allowAdmin) const
{
    if (allowAdmin)
        return true;

    auto itr = _customNpcStore.find(key);
    if (itr == _customNpcStore.end())
        return false;

    if (!itr->second.ownerBnetAccountId)
        return true;

    return player && player->GetSession() && itr->second.ownerBnetAccountId == player->GetSession()->GetBattlenetAccountId();
}

void Roleplay::EnsureCustomNpcOwner(std::string const& key, Player* owner)
{
    auto itr = _customNpcStore.find(key);
    if (itr == _customNpcStore.end() || itr->second.ownerBnetAccountId || !owner)
        return;

    PopulateCustomNpcOwner(itr->second, owner);
    SaveCustomNpcOwnerToDb(itr->second);
}

std::string Roleplay::GetCustomNpcOwnerDisplay(std::string const& key) const
{
    auto itr = _customNpcStore.find(key);
    if (itr == _customNpcStore.end() || !itr->second.ownerBnetAccountId)
        return "unowned";

    if (!itr->second.ownerAlias.empty())
        return itr->second.ownerAlias;

    return std::to_string(itr->second.ownerBnetAccountId);
}

void Roleplay::CopyCustomNpcArmorFromPlayer(std::string const& key, uint8 variationId, Player* player)
{
    if (!player)
        return;

    uint32 templateId = _customNpcStore[key].templateId;
    EnsureNpcOutfitExists(templateId, variationId);
    CreatureTemplate& cTemplate = sObjectMgr->_creatureTemplateStore[templateId];
    if (variationId < 1 || variationId > cTemplate.Models.size())
        return;

    uint32 outfitId = cTemplate.Models[variationId - 1].CreatureDisplayID;
    std::shared_ptr<CreatureOutfit> co = sObjectMgr->GetOutfit(outfitId);
    if (!co)
        return;

    for (EquipmentSlots slot : CreatureOutfit::item_slots)
        co->outfitdisplays[slot] = GetPlayerVisibleSlotDisplayId(player, slot);

    sObjectMgr->_creatureOutfitStore[outfitId] = std::move(co);
    SaveNpcOutfitToDb(templateId, variationId);
    ReloadSpawnedCustomNpcs(key, variationId);
}

void Roleplay::CopyCustomNpcArmorSlotFromPlayer(std::string const& key, uint8 variationId, EquipmentSlots slot, Player* player)
{
    if (!player)
        return;

    uint32 templateId = _customNpcStore[key].templateId;
    EnsureNpcOutfitExists(templateId, variationId);
    CreatureTemplate& cTemplate = sObjectMgr->_creatureTemplateStore[templateId];
    if (variationId < 1 || variationId > cTemplate.Models.size())
        return;

    uint32 outfitId = cTemplate.Models[variationId - 1].CreatureDisplayID;
    std::shared_ptr<CreatureOutfit> co = sObjectMgr->GetOutfit(outfitId);
    if (!co)
        return;

    co->outfitdisplays[slot] = GetPlayerVisibleSlotDisplayId(player, slot);

    sObjectMgr->_creatureOutfitStore[outfitId] = std::move(co);
    SaveNpcOutfitToDb(templateId, variationId);
    ReloadSpawnedCustomNpcs(key, variationId);
}

bool Roleplay::CopyCustomNpcWeaponFromPlayer(std::string const& key, uint8 variationId, uint8 equipmentSlot, Player* player)
{
    if (!player || equipmentSlot >= MAX_EQUIPMENT_ITEMS)
        return false;

    EquipmentSlots visibleSlot = EQUIPMENT_SLOT_MAINHAND;
    if (equipmentSlot == 1)
        visibleSlot = EQUIPMENT_SLOT_OFFHAND;
    else if (equipmentSlot == 2)
        visibleSlot = EQUIPMENT_SLOT_RANGED;

    EquipmentItem item;
    if (!PopulateEquipmentItemFromPlayer(player, visibleSlot, item))
        return false;

    uint32 templateId = _customNpcStore[key].templateId;
    EnsureEquipmentInfoExists(templateId, variationId);
    sObjectMgr->_equipmentInfoStore[templateId][variationId].Items[equipmentSlot] = item;
    SaveNpcEquipmentInfoToDb(templateId, variationId);
    ReloadSpawnedCustomNpcs(key, variationId);
    return true;
}

void Roleplay::SetCustomNpcOutfitEquipmentSlot(std::string const& key, uint8 variationId, EquipmentSlots slot, int32 displayId)
{
    uint32 templateId = _customNpcStore[key].templateId;
    EnsureNpcOutfitExists(templateId, variationId);
    CreatureTemplate& cTemplate = sObjectMgr->_creatureTemplateStore[templateId];
    if (variationId < 1 || variationId > cTemplate.Models.size())
    {
        TC_LOG_ERROR("roleplay", "ROLEPLAY: Setting equipment slot failed for custom NPC '{}': variation {} is outside model count {}.",
            key, uint32(variationId), cTemplate.Models.size());
        return;
    }

    uint32 outfitId = cTemplate.Models[variationId - 1].CreatureDisplayID;
    TC_LOG_DEBUG("roleplay", "ROLEPLAY: Setting equipmentslot '%u' for custom npc '%s' with outfitId '%u' to '%u'", slot, key, outfitId, displayId);
    std::shared_ptr<CreatureOutfit> co = sObjectMgr->GetOutfit(outfitId);
    if (!co)
    {
        TC_LOG_ERROR("roleplay", "ROLEPLAY: Setting equipment slot failed for custom NPC '{}': outfit {} not found.", key, outfitId);
        return;
    }

    co->outfitdisplays[slot] = displayId;
    sObjectMgr->_creatureOutfitStore[outfitId] = std::move(co);
    SaveNpcOutfitToDb(templateId, variationId);
    ReloadSpawnedCustomNpcs(key, variationId);
}

bool Roleplay::SetCustomNpcOutfitRace(std::string const& key, uint8 variationId, Races race, std::string& error)
{
    CreatureOutfit workingCopy;
    if (!TryGetCustomNpcOutfitCopy(key, variationId, workingCopy, error))
        return false;

    auto* maleModel = sDB2Manager.GetChrModel(race, GENDER_MALE);
    auto* femaleModel = sDB2Manager.GetChrModel(race, GENDER_FEMALE);
    if (!maleModel || !femaleModel)
    {
        error = Trinity::StringFormat("ChrModel missing for race {}", uint32(race));
        TC_LOG_ERROR("roleplay", "ROLEPLAY: Setting race failed for custom NPC '{}': ChrModel missing for race {}.", key, uint32(race));
        return false;
    }

    if (!sChrRacesStore.LookupEntry(race))
    {
        error = Trinity::StringFormat("invalid race {}", uint32(race));
        return false;
    }

    workingCopy.race = race;
    switch (workingCopy.gender)
    {
    case GENDER_FEMALE: workingCopy.displayId = femaleModel->DisplayID; break;
    case GENDER_MALE:   workingCopy.displayId = maleModel->DisplayID; break;
    default:
        error = "invalid gender on outfit";
        return false;
    }

    NormalizeOutfitCustomizations(workingCopy);
    return ValidateAndSaveCustomNpcOutfit(key, variationId, workingCopy, error);
}

bool Roleplay::SetCustomNpcOutfitGender(std::string const& key, uint8 variationId, Gender gender, std::string& error)
{
    CreatureOutfit workingCopy;
    if (!TryGetCustomNpcOutfitCopy(key, variationId, workingCopy, error))
        return false;

    auto* maleModel = sDB2Manager.GetChrModel(workingCopy.race, GENDER_MALE);
    auto* femaleModel = sDB2Manager.GetChrModel(workingCopy.race, GENDER_FEMALE);
    if (!maleModel || !femaleModel)
    {
        error = Trinity::StringFormat("ChrModel missing for race {}", uint32(workingCopy.race));
        TC_LOG_ERROR("roleplay", "ROLEPLAY: Setting gender failed for custom NPC '{}': ChrModel missing for race {}.", key, uint32(workingCopy.race));
        return false;
    }

    workingCopy.gender = gender;
    switch (workingCopy.gender)
    {
    case GENDER_FEMALE: workingCopy.displayId = femaleModel->DisplayID; break;
    case GENDER_MALE:   workingCopy.displayId = maleModel->DisplayID; break;
    default:
        error = "invalid gender";
        return false;
    }

    NormalizeOutfitCustomizations(workingCopy);
    return ValidateAndSaveCustomNpcOutfit(key, variationId, workingCopy, error);
}

void Roleplay::SetCustomNpcLeftHand(std::string const& key, uint8 variationId, int32 itemId, int32 appearanceModId)
{
    uint32 templateId = _customNpcStore[key].templateId;
    SetNpcLeftHand(templateId, variationId, itemId, appearanceModId);
    ReloadSpawnedCustomNpcs(key, variationId);
}

void Roleplay::SetNpcLeftHand(uint32 templateId, uint8 variationId, int32 itemId, int32 appearanceModId)
{
    EnsureEquipmentInfoExists(templateId, variationId);
    EquipmentInfo _equipmentInfo = sObjectMgr->_equipmentInfoStore[templateId][variationId];
    _equipmentInfo.Items[1].ItemId = itemId;
    _equipmentInfo.Items[1].AppearanceModId = appearanceModId;
    sObjectMgr->_equipmentInfoStore[templateId][variationId] = _equipmentInfo;
    SaveNpcEquipmentInfoToDb(templateId, variationId);
}

void Roleplay::SetCustomNpcRightHand(std::string const& key, uint8 variationId, int32 itemId, int32 appearanceModId)
{
    uint32 templateId = _customNpcStore[key].templateId;
    SetNpcRightHand(templateId, variationId, itemId, appearanceModId);
    ReloadSpawnedCustomNpcs(key, variationId);
}

void Roleplay::SetNpcRightHand(uint32 templateId, uint8 variationId, int32 itemId, int32 appearanceModId)
{
    EnsureEquipmentInfoExists(templateId, variationId);
    EquipmentInfo _equipmentInfo = sObjectMgr->_equipmentInfoStore[templateId][variationId];
    _equipmentInfo.Items[0].ItemId = itemId;
    _equipmentInfo.Items[0].AppearanceModId = appearanceModId;
    sObjectMgr->_equipmentInfoStore[templateId][variationId] = _equipmentInfo;
    SaveNpcEquipmentInfoToDb(templateId, variationId);
}

void Roleplay::SetCustomNpcRanged(std::string const& key, uint8 variationId, int32 itemId, int32 appearanceModId)
{
    uint32 templateId = _customNpcStore[key].templateId;
    SetNpcRanged(templateId, variationId, itemId, appearanceModId);
    ReloadSpawnedCustomNpcs(key, variationId);
}


void Roleplay::SetNpcRanged(uint32 templateId, uint8 variationId, int32 itemId, int32 appearanceModId)
{
    EnsureEquipmentInfoExists(templateId, variationId);
    EquipmentInfo _equipmentInfo = sObjectMgr->_equipmentInfoStore[templateId][variationId];
    _equipmentInfo.Items[2].ItemId = itemId;
    _equipmentInfo.Items[2].AppearanceModId = appearanceModId;
    sObjectMgr->_equipmentInfoStore[templateId][variationId] = _equipmentInfo;
    SaveNpcEquipmentInfoToDb(templateId, variationId);
}

void Roleplay::SetCustomNpcDisplayId(std::string const& key, uint8 variationId, uint32 displayId)
{
    uint32 templateId = _customNpcStore[key].templateId;
    EnsureNpcModelExists(templateId, variationId);
    TC_LOG_DEBUG("roleplay", "ROLEPLAY: Setting model display id for custom npc '%s' variation '%u' to '%u'", key.c_str(), variationId, displayId);
    CreatureTemplate& cTemplate = sObjectMgr->_creatureTemplateStore[templateId];
    CreatureModel model = cTemplate.Models[variationId - 1];
    model.CreatureDisplayID = displayId;
    cTemplate.Models[variationId - 1] = model;
    SaveNpcModelInfo(model, templateId, variationId);
    ReloadSpawnedCustomNpcs(key, variationId);
}

void Roleplay::SetCustomNpcModelScale(std::string const& key, uint8 variationId, float displayScale)
{
    uint32 templateId = _customNpcStore[key].templateId;
    EnsureNpcModelExists(templateId, variationId);
    TC_LOG_DEBUG("roleplay", "ROLEPLAY: Setting model scale for custom npc '%s' variation '%u' to '%f'", key.c_str(), variationId, displayScale);
    CreatureTemplate& cTemplate = sObjectMgr->_creatureTemplateStore[templateId];
    CreatureModel model = cTemplate.Models[variationId - 1];
    model.DisplayScale = displayScale;
    cTemplate.Models[variationId - 1] = model;
    SaveNpcModelInfo(model, templateId, variationId);
    ReloadSpawnedCustomNpcs(key, variationId);
}

void Roleplay::SetCustomNpcGuild(std::string const& key, uint8 variationId, uint64 guild)
{
    uint32 templateId = _customNpcStore[key].templateId;
    EnsureNpcOutfitExists(templateId, variationId);
    CreatureTemplate& cTemplate = sObjectMgr->_creatureTemplateStore[templateId];
    if (variationId < 1 || variationId > cTemplate.Models.size())
    {
        TC_LOG_ERROR("roleplay", "ROLEPLAY: Setting guild failed for custom NPC '{}': variation {} is outside model count {}.",
            key, uint32(variationId), cTemplate.Models.size());
        return;
    }

    uint32 outfitId = cTemplate.Models[variationId - 1].CreatureDisplayID;
    TC_LOG_DEBUG("roleplay", "ROLEPLAY: Setting guild for custom npc '%s' with outfitId '%u'", key.c_str(), outfitId);

    std::shared_ptr<CreatureOutfit> co = sObjectMgr->GetOutfit(outfitId);
    if (!co)
    {
        TC_LOG_ERROR("roleplay", "ROLEPLAY: Setting guild failed for custom NPC '{}': outfit {} not found.", key, outfitId);
        return;
    }

    co->guild = guild;
    sObjectMgr->_creatureOutfitStore[outfitId] = std::move(co);
    SaveNpcOutfitToDb(templateId, variationId);
    ReloadSpawnedCustomNpcs(key, variationId);
}

void Roleplay::SetCustomNpcTameable(std::string const& key, bool tameable)
{
    uint32 templateId = _customNpcStore[key].templateId;
    CreatureTemplate& cTemplate = sObjectMgr->_creatureTemplateStore[templateId];
    cTemplate.type = tameable ? 1 : 0;
    cTemplate.family = tameable ? CREATURE_FAMILY_GORILLA : CREATURE_FAMILY_NONE;

    SaveNpcCreatureTemplateToDb(cTemplate);
    ReloadSpawnedCustomNpcs(key);
}

void Roleplay::SetCustomNpcName(std::string const& key, std::string const& displayName)
{
    uint32 templateId = _customNpcStore[key].templateId;
    CreatureTemplate& cTemplate = sObjectMgr->_creatureTemplateStore[templateId];
    cTemplate.Name = displayName;

    SaveNpcCreatureTemplateToDb(cTemplate);
    ReloadSpawnedCustomNpcs(key);
}

void Roleplay::SetCustomNpcSubName(std::string const& key, std::string const& subName)
{
    uint32 templateId = _customNpcStore[key].templateId;
    CreatureTemplate& cTemplate = sObjectMgr->_creatureTemplateStore[templateId];
    cTemplate.SubName = subName;

    SaveNpcCreatureTemplateToDb(cTemplate);
    ReloadSpawnedCustomNpcs(key);
}

void Roleplay::SetCustomNpcCustomizations(std::string const& key, uint8 variationId, Player* player)
{
    uint32 templateId = _customNpcStore[key].templateId;
    EnsureNpcOutfitExists(templateId, variationId);
    CreatureTemplate& cTemplate = sObjectMgr->_creatureTemplateStore[templateId];
    if (variationId < 1 || variationId > cTemplate.Models.size())
    {
        TC_LOG_ERROR("roleplay", "ROLEPLAY: Setting customizations failed for custom NPC '{}': variation {} is outside model count {}.",
            key, uint32(variationId), cTemplate.Models.size());
        return;
    }

    uint32 outfitId = cTemplate.Models[variationId - 1].CreatureDisplayID;
    std::shared_ptr<CreatureOutfit> co = sObjectMgr->GetOutfit(outfitId);
    if (!co)
    {
        TC_LOG_ERROR("roleplay", "ROLEPLAY: Setting customizations failed for custom NPC '{}': outfit {} not found.", key, outfitId);
        return;
    }

    std::vector<UF::ChrCustomizationChoice> customizations;
    for (auto&& customization : player->m_playerData->Customizations)
    {
        customizations.push_back(customization);
    }
    co->Customizations = customizations;
    sObjectMgr->_creatureOutfitStore[outfitId] = std::move(co);
    SaveNpcOutfitToDb(templateId, variationId);
    ReloadSpawnedCustomNpcs(key, variationId);
}

bool Roleplay::SetCustomNpcCustomizationOption(std::string const& key, uint8 variationId, uint32 optionId, uint32 choiceId, std::string& error)
{
    CreatureOutfit workingCopy;
    if (!TryGetCustomNpcOutfitCopy(key, variationId, workingCopy, error))
        return false;

    auto options = BuildCustomizationOptionList(Races(workingCopy.race), Gender(workingCopy.gender));
    auto optionItr = std::find_if(options.begin(), options.end(), [optionId](ChrCustomizationOptionEntry const* option)
    {
        return option->ID == optionId;
    });
    if (optionItr == options.end())
    {
        error = "option id is not valid for current race/gender";
        return false;
    }

    auto choices = BuildCustomizationChoiceListForOutfit(optionId, workingCopy);
    if (!std::any_of(choices.begin(), choices.end(), [choiceId](ChrCustomizationChoiceEntry const* choice)
    {
        return choice->ID == choiceId;
    }))
    {
        error = "choice id is not valid for current outfit prerequisites; use .cnpc face choices";
        return false;
    }

    ReplaceCustomization(workingCopy.Customizations, optionId, choiceId);
    SortCustomizationsByOptions(workingCopy.Customizations, options);
    return ValidateAndSaveCustomNpcOutfit(key, variationId, workingCopy, error);
}

void Roleplay::CopyCustomNpcCustomizationOptionFromPlayer(std::string const& key, uint8 variationId, uint32 optionId, Player* player)
{
    if (!player)
        return;

    for (UF::ChrCustomizationChoice const& customization : player->m_playerData->Customizations)
    {
        if (customization.ChrCustomizationOptionID == optionId)
        {
            std::string error;
            SetCustomNpcCustomizationOption(key, variationId, optionId, customization.ChrCustomizationChoiceID, error);
            return;
        }
    }
}

void Roleplay::SetCustomNpcStandState(std::string const& key, uint8 standState)
{
    uint32 templateId = _customNpcStore[key].templateId;
    CreatureAddon& addon = sObjectMgr->_creatureTemplateAddonStore[templateId];
    addon.standState = standState;
    SaveNpcCreatureTemplateAddonToDb(templateId, addon);
    ReloadSpawnedCustomNpcs(key);
}

void Roleplay::RemoveCustomNpcVariation(std::string const& key, uint8 variationId)
{
    uint32 templateId = _customNpcStore[key].templateId;
    CreatureTemplate& cTemplate = sObjectMgr->_creatureTemplateStore[templateId];
    uint8 currentModel = variationId;
    while (currentModel != cTemplate.Models.size()) {
        // Shift models lower
        cTemplate.Models[currentModel - 1] = cTemplate.Models[currentModel];
        SaveNpcModelInfo(cTemplate.Models[currentModel - 1], templateId, currentModel);
        currentModel++;
    }

    WorldDatabasePreparedStatement* stmt = WorldDatabase.GetPreparedStatement(WORLD_DEL_CREATURE_TEMPLATE_MODEL);
    stmt->setUInt32(0, templateId);
    stmt->setUInt8(1, currentModel - 1);
    WorldDatabase.Execute(stmt);

    cTemplate.Models.pop_back();
    ReloadSpawnedCustomNpcs(key);
}

void Roleplay::SaveNpcOutfitToDb(uint32 templateId, uint8 variationId)
{
    TC_LOG_DEBUG("roleplay", "ROLEPLAY: Saving variation '%u' for templateid '%u' to DB...", variationId, templateId);
    CreatureTemplate& cTemplate = sObjectMgr->_creatureTemplateStore[templateId];
    if (variationId < 1 || variationId > cTemplate.Models.size())
    {
        TC_LOG_ERROR("roleplay", "ROLEPLAY: Saving outfit failed for template {}: variation {} is outside model count {}.",
            templateId, uint32(variationId), cTemplate.Models.size());
        return;
    }

    uint32 outfitId = cTemplate.Models[variationId - 1].CreatureDisplayID;
    std::shared_ptr<CreatureOutfit> co = sObjectMgr->GetOutfit(outfitId);
    if (!co)
    {
        TC_LOG_ERROR("roleplay", "ROLEPLAY: Saving outfit failed for template {} variation {}: outfit {} not found.",
            templateId, uint32(variationId), outfitId);
        return;
    }

    std::string customizations_iss;
    for (auto&& customization : co->Customizations)
    {
        if (customizations_iss.size()) {
            customizations_iss.append(" ");
        }
        customizations_iss.append(std::to_string(customization.ChrCustomizationOptionID) + " " + std::to_string(customization.ChrCustomizationChoiceID));
    }

    WorldDatabaseTransaction trans = WorldDatabase.BeginTransaction();
    // "REPLACE INTO creature_template_outfits (entry, race, class, gender, customizations, head, shoulders, body, chest, waist, legs, feet, wrists, hands, back, tabard, guildid)
    //  VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
    WorldDatabasePreparedStatement* stmt = WorldDatabase.GetPreparedStatement(WORLD_REP_DRESSNPC_OUTFIT);
    int index = 0;
    stmt->setUInt32(index++, outfitId);
    stmt->setUInt8(index++, co->race);
    stmt->setUInt8(index++, co->Class);
    stmt->setUInt8(index++, co->gender);
    stmt->setString(index++, customizations_iss);
    for (EquipmentSlots slot : CreatureOutfit::item_slots)
    {
        stmt->setInt32(index++, -int32(co->outfitdisplays[slot]));
        stmt->setUInt32(index++, 0);
    }
    stmt->setUInt64(index++, co->guild);
    trans->Append(stmt);

    // "REPLACE INTO creature_template_model (CreatureId, Idx, CreatureDisplayId, DisplayScale, Probability) VALUES (?, ?, ?, ?, 1)"
    stmt = WorldDatabase.GetPreparedStatement(WORLD_REP_CREATURE_TEMPLATE_MODEL);
    stmt->setUInt32(0, templateId);
    stmt->setUInt8(1, variationId - 1);
    stmt->setUInt32(2, outfitId);
    stmt->setFloat(3, cTemplate.Models[variationId - 1].DisplayScale);
    trans->Append(stmt);
    WorldDatabase.CommitTransaction(trans);
}

void Roleplay::SaveNpcModelInfo(CreatureModel model, uint32 creatureTemplateId, uint8 variationId)
{
    // "REPLACE INTO creature_template_model (CreatureId, Idx, CreatureDisplayId, DisplayScale, Probability) VALUES (?, ?, ?, ?, 1)"
    WorldDatabasePreparedStatement* stmt = WorldDatabase.GetPreparedStatement(WORLD_REP_CREATURE_TEMPLATE_MODEL);
    stmt->setUInt32(0, creatureTemplateId);
    stmt->setUInt8(1, variationId - 1);
    stmt->setUInt32(2, model.CreatureDisplayID);
    stmt->setFloat(3, model.DisplayScale);
    WorldDatabase.Execute(stmt);
}

void Roleplay::SaveCustomNpcDataToDb(CustomNpcData outfitData)
{
    TC_LOG_DEBUG("roleplay", "ROLEPLAY: Saving custom npc data for key '%s' to DB...", outfitData.key);
    // "REPLACE INTO custom_npcs (Key, Entry) VALUES (?, ?)"
    RoleplayDatabasePreparedStatement* stmt = RoleplayDatabase.GetPreparedStatement(Roleplay_REP_CUSTOMNPCDATA);
    int index = 0;
    stmt->setString(index++, outfitData.key);
    stmt->setUInt32(index++, outfitData.templateId);
    RoleplayDatabase.Execute(stmt);
}

void Roleplay::SaveCustomNpcOwnerToDb(CustomNpcData const& npcData)
{
    if (!npcData.ownerBnetAccountId)
        return;

    RoleplayDatabasePreparedStatement* stmt = RoleplayDatabase.GetPreparedStatement(Roleplay_REP_CUSTOMNPCOWNER);
    int index = 0;
    stmt->setString(index++, npcData.key);
    stmt->setUInt32(index++, npcData.ownerBnetAccountId);
    stmt->setString(index++, npcData.ownerAlias);
    RoleplayDatabase.Execute(stmt);
}

void Roleplay::RefreshCreatureTemplateClientCache(CreatureTemplate& cTemplate)
{
    cTemplate.InitializeQueryData();

    SessionMap const& smap = sWorld->GetAllSessions();
    for (SessionMap::const_iterator iter = smap.begin(); iter != smap.end(); ++iter)
    {
        WorldSession* session = iter->second;
        if (!session)
            continue;

        Player* player = session->GetPlayer();
        if (!player)
            continue;

        TC_LOG_DEBUG("roleplay", "ROLEPLAY: Sending query packet for creatureTemplate '%s' to '%s'.", cTemplate.Name.c_str(), player->GetName().c_str());
        if (sWorld->getBoolConfig(CONFIG_CACHE_DATA_QUERIES))
        {
            uint32 localeIndex = static_cast<uint32>(session->GetSessionDbLocaleIndex());
            if (localeIndex < TOTAL_LOCALES)
                session->SendPacket(&cTemplate.QueryData[localeIndex]);
        }
        else
        {
            WorldPacket response = cTemplate.BuildQueryData(session->GetSessionDbLocaleIndex(), DIFFICULTY_NONE);
            session->SendPacket(&response);
        }
    }
}

void Roleplay::ReloadCreatureLocaleFromDb(uint32 entry)
{
    sObjectMgr->_creatureLocaleStore.erase(entry);

    QueryResult result = WorldDatabase.PQuery(
        "SELECT locale, Name, NameAlt, Title, TitleAlt FROM creature_template_locale WHERE entry = {}", entry);
    if (!result)
        return;

    do
    {
        Field* fields = result->Fetch();
        LocaleConstant locale = GetLocaleByName(fields[0].GetStringView());
        if (!IsValidLocale(locale) || locale == LOCALE_enUS)
            continue;

        CreatureLocale& data = sObjectMgr->_creatureLocaleStore[entry];
        ObjectMgr::AddLocaleString(fields[1].GetStringView(), locale, data.Name);
        ObjectMgr::AddLocaleString(fields[2].GetStringView(), locale, data.NameAlt);
        ObjectMgr::AddLocaleString(fields[3].GetStringView(), locale, data.Title);
        ObjectMgr::AddLocaleString(fields[4].GetStringView(), locale, data.TitleAlt);
    } while (result->NextRow());
}

void Roleplay::SaveNpcCreatureTemplateToDb(CreatureTemplate& cTemplate)
{
    TC_LOG_DEBUG("roleplay", "ROLEPLAY: Saving creature template id '%u' to DB...", cTemplate.Entry);
    // "REPLACE INTO creature_template (entry, name, subname, HealthScalingExpansion, RequiredExpansion, faction, unit_class, type, type_flags2, movementId, CreatureDifficultyID) VALUES (?, ?, ?, 8, 0, 35, 1, 7, 2, 100, 204488)"
    WorldDatabasePreparedStatement* stmt = WorldDatabase.GetPreparedStatement(WORLD_REP_CREATURE_TEMPLATE);
    int index = 0;
    stmt->setUInt32(index++, cTemplate.Entry);
    stmt->setString(index++, cTemplate.Name);
    stmt->setString(index++, cTemplate.SubName);
    WorldDatabase.Execute(stmt);

    RefreshCreatureTemplateClientCache(cTemplate);
}

void Roleplay::SaveNpcEquipmentInfoToDb(uint32 templateId, uint8 variationId)
{
    TC_LOG_DEBUG("roleplay", "ROLEPLAY: Saving equipment info '%u' for creature template id '%u' to DB...", variationId, templateId);
    // "REPLACE INTO creature_equip_template (CreatureId, ID, ItemID1, AppearanceModID1, ItemVisual1, ItemID2, AppearanceModID2, ItemVisual2, ItemID3, AppearanceModID3, ItemVisual3) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)"

    EquipmentInfo equipInfo = sObjectMgr->_equipmentInfoStore[templateId][variationId];

    WorldDatabasePreparedStatement* stmt = WorldDatabase.GetPreparedStatement(WORLD_REP_CREATURE_EQUIP_TEMPLATE);
    int index = 0;
    stmt->setUInt32(index++, templateId);
    stmt->setUInt8(index++, variationId);
    for (uint8 equipmentInfoSlot = 0; equipmentInfoSlot < MAX_EQUIPMENT_ITEMS; equipmentInfoSlot++) {
        stmt->setUInt32(index++, equipInfo.Items[equipmentInfoSlot].ItemId);
        stmt->setUInt16(index++, equipInfo.Items[equipmentInfoSlot].AppearanceModId);
        stmt->setUInt16(index++, equipInfo.Items[equipmentInfoSlot].ItemVisual);
    }
    WorldDatabase.Execute(stmt);
}

void Roleplay::SaveNpcCreatureTemplateAddonToDb(uint32 templateId, CreatureAddon cAddon)
{
    TC_LOG_DEBUG("roleplay", "ROLEPLAY: Saving creature addon template for creature template id '%u' to DB...", templateId);
    WorldDatabasePreparedStatement* stmt = WorldDatabase.GetPreparedStatement(WORLD_REP_CREATURE_TEMPLATE_ADDON);

    int index = 0;
    stmt->setUInt32(index++, templateId);
    stmt->setUInt32(index++, cAddon.PathId);
    stmt->setUInt32(index++, cAddon.mount);
    stmt->setUInt8(index++, cAddon.standState);
    stmt->setUInt8(index++, cAddon.animTier);
    stmt->setUInt8(index++, cAddon.visFlags);
    stmt->setUInt8(index++, cAddon.sheathState);
    stmt->setUInt8(index++, cAddon.pvpFlags);
    stmt->setUInt32(index++, cAddon.emote);
    stmt->setUInt16(index++, cAddon.aiAnimKit);
    stmt->setUInt16(index++, cAddon.movementAnimKit);
    stmt->setUInt16(index++, cAddon.meleeAnimKit);
    stmt->setUInt8(index++, uint8(cAddon.visibilityDistanceType));
    if (!cAddon.auras.empty())
    {
        std::string auraList;
        for (uint32 aura : cAddon.auras)
        {
            if (auraList.empty())
                auraList = std::to_string(aura);
            else
                auraList += " " + std::to_string(aura);
        }
        stmt->setString(index, auraList);
    }
    else
    {
        stmt->setNull(index);
    }
    WorldDatabase.Execute(stmt);
}

void Roleplay::DeleteCustomNpc(std::string const& key)
{
    CustomNpcData data = _customNpcStore[key];
    TC_LOG_DEBUG("roleplay", "ROLEPLAY: Deleting custom npc '%s' with entry '%u'", key.c_str(), data.templateId);
    // Remove spawns
    for (auto spawn : data.spawns) {
        TC_LOG_DEBUG("roleplay", "ROLEPLAY: Deleting spawn " UI64FMTD, spawn);
        Creature* creature = GetAnyCreature(spawn);
        if (creature) {
            CreatureDelete(creature);
        }
        else {
            Creature::DeleteFromDB(spawn);
        }
    }
    // Cleanup database
    WorldDatabaseTransaction trans = WorldDatabase.BeginTransaction();

    WorldDatabasePreparedStatement* stmt = WorldDatabase.GetPreparedStatement(WORLD_DEL_ALL_CREATURE_TEMPLATE_MODEL);
    stmt->setUInt32(0, data.templateId);
    trans->Append(stmt);

    stmt = WorldDatabase.GetPreparedStatement(WORLD_DEL_CREATURE_EQUIP_TEMPLATE);
    stmt->setUInt32(0, data.templateId);
    trans->Append(stmt);

    stmt = WorldDatabase.GetPreparedStatement(WORLD_DEL_CREATURE_TEMPLATE_ADDON);
    stmt->setUInt32(0, data.templateId);
    trans->Append(stmt);

    stmt = WorldDatabase.GetPreparedStatement(WORLD_DEL_CREATURE_TEMPLATE);
    stmt->setUInt32(0, data.templateId);
    trans->Append(stmt);

    for (uint8 modelId = 0; modelId < sObjectMgr->_creatureTemplateStore[data.templateId].Models.size(); modelId++)
    {
        uint32 outfitId = sObjectMgr->_creatureTemplateStore[data.templateId].Models[modelId].CreatureDisplayID;
        stmt = WorldDatabase.GetPreparedStatement(WORLD_DEL_DRESSNPC_OUTFIT);
        stmt->setUInt32(0, outfitId);
        trans->Append(stmt);
    }

    WorldDatabase.CommitTransaction(trans);

    RoleplayDatabasePreparedStatement* fStmt = RoleplayDatabase.GetPreparedStatement(Roleplay_DEL_CUSTOMNPC);
    fStmt->setString(0, key);
    RoleplayDatabase.Execute(fStmt);

    fStmt = RoleplayDatabase.GetPreparedStatement(Roleplay_DEL_CUSTOMNPCOWNER);
    fStmt->setString(0, key);
    RoleplayDatabase.Execute(fStmt);

    _customNpcStore.erase(key);
    for (uint8 modelId = 0; modelId < sObjectMgr->_creatureTemplateStore[data.templateId].Models.size(); modelId++)
    {
        uint32 outfitId = sObjectMgr->_creatureTemplateStore[data.templateId].Models[modelId].CreatureDisplayID;
        sObjectMgr->_creatureOutfitStore.erase(outfitId);
    }
    sObjectMgr->_equipmentInfoStore.erase(data.templateId);
    sObjectMgr->_creatureTemplateAddonStore.erase(data.templateId);
    sObjectMgr->_creatureTemplateStore.erase(data.templateId);
}

void Roleplay::EnsureNpcOutfitExists(uint32 templateId, uint8 variationId, float displayScale)
{
    if (variationId < 1)
        variationId = 1;

    CreatureTemplate& cTemplate = sObjectMgr->_creatureTemplateStore[templateId];
    uint32 modelsSize = cTemplate.Models.size();

    std::vector<uint8> toCreate;

    // Ensure the variation exists
    if (modelsSize < variationId) {
        for (uint8 i = modelsSize; i < variationId; i++) {
            toCreate.push_back(i);
        }
    }   // Ensure model is an outfit
    else if (!CreatureOutfit::IsFake(cTemplate.Models[variationId - 1].CreatureDisplayID)) {
        toCreate.push_back(variationId - 1);
    }

    if (!toCreate.empty())
    {
        if (sObjectMgr->_creatureOutfitStore.empty())
        {
            TC_LOG_ERROR("roleplay", "ROLEPLAY: Cannot create outfit variation {} for template {}: outfit store is empty.",
                uint32(variationId), templateId);
            return;
        }

        using pairtype = std::pair<uint32, std::shared_ptr<CreatureOutfit>>;
        uint32 maxOutfitId = std::max_element(sObjectMgr->_creatureOutfitStore.begin(), sObjectMgr->_creatureOutfitStore.end(),
            [](pairtype a, pairtype b) { return a.second->id < b.second->id; })->second->id;
        std::shared_ptr<CreatureOutfit> lastOutfit;
        bool setOutfit = false;
        for (uint8 i = 1; i <= modelsSize; i++) {
            uint32 displayId = cTemplate.Models[modelsSize - i].CreatureDisplayID;
            if (CreatureOutfit::IsFake(displayId)) {
                lastOutfit = sObjectMgr->GetOutfit(displayId);
                if (lastOutfit)
                {
                    setOutfit = true;
                    break;
                }
            }
        }
        if (!setOutfit) {
            TC_LOG_DEBUG("roleplay", "ROLEPLAY: Custom NPC template '%u' has no outfits, selecting first option in store for variation '%u'.", templateId, variationId);
            // Custom NPC has only used displayids, in this case we'll just take the first creatureoutfit available.
            lastOutfit = sObjectMgr->_creatureOutfitStore.begin()->second;
        }

        if (!lastOutfit)
        {
            TC_LOG_ERROR("roleplay", "ROLEPLAY: Cannot create outfit variation {} for template {}: source outfit not found.",
                uint32(variationId), templateId);
            return;
        }

        uint8 created = 0;
        for (uint8 variation : toCreate)
        {

            std::shared_ptr<CreatureOutfit> outfit(new CreatureOutfit());
            uint32 outfitId = maxOutfitId + ++created;
            TC_LOG_DEBUG("roleplay", "ROLEPLAY: Adding outfit '%u' to create variation '%u' for template '%u'.", outfitId, variation, templateId);
            outfit->id = outfitId;
            outfit->npcsoundsid = 0;
            outfit->race = lastOutfit->race;
            outfit->Class = lastOutfit->Class;
            outfit->gender = lastOutfit->gender;
            outfit->displayId = lastOutfit->displayId;
            outfit->SpellVisualKitID = 0;
            outfit->Customizations = lastOutfit->Customizations;
            for (EquipmentSlots slot : CreatureOutfit::item_slots)
            {
                outfit->outfitdisplays[slot] = lastOutfit->outfitdisplays[slot];
            }
            outfit->guild = lastOutfit->guild;
            outfit->id = outfitId;
            sObjectMgr->_creatureOutfitStore[outfitId] = std::move(outfit);
            if (variation >= modelsSize) {
                cTemplate.Models.push_back(CreatureModel(outfitId, displayScale, 1));
            }
            else {
                cTemplate.Models[variation] = CreatureModel(outfitId, displayScale, 1);
            }
        }
    }


    TC_LOG_DEBUG("roleplay", "ROLEPLAY: Model variation '%u' for template '%u' ensured.", variationId, templateId);
}

void Roleplay::EnsureEquipmentInfoExists(uint32 templateId, uint8 variationId)
{
    if (!sObjectMgr->_equipmentInfoStore[templateId].count(variationId)) {
        TC_LOG_DEBUG("roleplay", "ROLEPLAY: Adding equipment variation '%u' for template '%u'.", variationId, templateId);
        EquipmentInfo equipInfo;
        for (uint8 equipmentInfoSlot = 0; equipmentInfoSlot < MAX_EQUIPMENT_ITEMS; equipmentInfoSlot++) {
            equipInfo.Items[equipmentInfoSlot].ItemId = 0;
            equipInfo.Items[equipmentInfoSlot].AppearanceModId = 0;
            equipInfo.Items[equipmentInfoSlot].ItemVisual = 0;
        }
        sObjectMgr->_equipmentInfoStore[templateId][variationId] = equipInfo;
    }
    TC_LOG_DEBUG("roleplay", "ROLEPLAY: Equipment variation '%u' for template '%u' ensured.", variationId, templateId);
}

void Roleplay::EnsureNpcModelExists(uint32 templateId, uint8 variationId)
{
    if (variationId < 1)
        variationId = 1;

    CreatureTemplate& cTemplate = sObjectMgr->_creatureTemplateStore[templateId];
    uint32 modelsSize = cTemplate.Models.size();
    if (!modelsSize)
    {
        TC_LOG_WARN("roleplay", "ROLEPLAY: Template {} has no models, adding placeholder model for variation {}.", templateId, uint32(variationId));
        cTemplate.Models.push_back(CreatureModel(0, 1.0f, 1.0f));
        modelsSize = cTemplate.Models.size();
    }

    if (modelsSize < variationId) {
        for (uint8 i = modelsSize; i < variationId; i++) {
            uint32 prevDisplayId = cTemplate.Models[modelsSize - 1].CreatureDisplayID;
            TC_LOG_DEBUG("roleplay", "ROLEPLAY: Creating model variation '%u' for template '%u' using displayid: '%u'.", i, templateId, prevDisplayId);
            cTemplate.Models.push_back(CreatureModel(prevDisplayId, 1, 1));
        }
    }
    TC_LOG_DEBUG("roleplay", "ROLEPLAY: Model variation '%u' for template '%u' ensured.", variationId, templateId);
}

uint8 Roleplay::GetModelVariationCountForNpc(std::string const& key) const {
    auto npcItr = _customNpcStore.find(key);
    if (npcItr == _customNpcStore.end())
        return 0;

    auto templateItr = sObjectMgr->_creatureTemplateStore.find(npcItr->second.templateId);
    if (templateItr == sObjectMgr->_creatureTemplateStore.end())
        return 0;

    return templateItr->second.Models.size();
}
uint8 Roleplay::GetEquipmentVariationCountForNpc(std::string const& key) const {
    auto npcItr = _customNpcStore.find(key);
    if (npcItr == _customNpcStore.end())
        return 0;

    return GetEquipmentVariationCountForNpc(npcItr->second.templateId);
}
uint8 Roleplay::GetEquipmentVariationCountForNpc(uint32 templateId) const {
    auto itr = sObjectMgr->_equipmentInfoStore.find(templateId);
    if (itr == sObjectMgr->_equipmentInfoStore.end())
        return 0;

    return itr->second.size();
}

void Roleplay::StoreMarkerLocationForPlayer(Player* player, const WorldLocation* marker)
{
    _playerExtraDataStore[player->GetGUID().GetCounter()].markerLocation = *marker;
}

#pragma endregion
