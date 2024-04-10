#include "AccountMgr.h"
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
#include <boost/algorithm/string.hpp>
#include <boost/algorithm/string/predicate.hpp>
#include <boost/tokenizer.hpp>
#include <G3D/Quat.h>
#include <QueryPackets.h>
#include <CreatureGroups.h>


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

void Roleplay::CreatureSetBytes1(Creature* creature)
{
    uint32 spawnId = creature->GetSpawnId();
    auto addonData = &(sObjectMgr->_creatureAddonStore[spawnId]);

    creature->SetStandState(UnitStandStateType(addonData->standState));
    creature->ReplaceAllVisFlags(UnitVisFlags(addonData->visFlags));
    creature->SetAnimTier(AnimTier(addonData->animTier), false);
}

void Roleplay::CreatureSetBytes2(Creature* creature)
{
    uint32 spawnId = creature->GetSpawnId();
    auto addonData = &(sObjectMgr->_creatureAddonStore[spawnId]);

    creature->SetSheath(SheathState(addonData->sheathState));
    creature->ReplaceAllPvpFlags(UnitPVPStateFlags(addonData->pvpFlags));
    creature->ReplaceAllPetFlags(UNIT_PET_FLAG_NONE);
    creature->SetShapeshiftForm(FORM_NONE);
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

    creature->SendTeleportPacket(creature->GetPosition());
}

void Roleplay::CreatureTurn(Creature* creature, float o)
{
    CreatureMove(creature, creature->GetPositionX(), creature->GetPositionY(), creature->GetPositionZ(), o);
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
            creature->SaveToDB(trans->GetGOInfo()->moTransport.SpawnMap, { map->GetDifficultyID() });
            sObjectMgr->AddCreatureToGrid(&data);
            return creature;
        }
    }

    Creature* creature = Creature::CreateCreature(entryId, map, creator->GetPosition());
    if (!creature)
        return creature;

    PhasingHandler::InheritPhaseShift(creature, creator);
    creature->SaveToDB(map->GetId(), { map->GetDifficultyID() });

    ObjectGuid::LowType db_guid = creature->GetSpawnId();

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

    // ObjectGuid::LowType guidLow = creature->GetSpawnId();
    Map* map = creature->GetMap();
    map->GetObjectsStore().Remove<Creature>(creature->GetGUID());
    creature->DestroyForNearbyPlayers();

    auto newGuidLow = map->GenerateLowGuid<HighGuid::Creature>();
    auto newObjectGuid = ObjectGuid::Create<HighGuid::Creature>(map->GetId(), creature->GetEntry(), newGuidLow);

    // TODO: Test this
    creature->SetGuid(newObjectGuid);
    // No longer works
    //creature->SetGuidValue(OBJECT_FIELD_GUID, newObjectGuid);
    //creature->SetPackGUID(newObjectGuid);
    map->GetObjectsStore().Insert(newObjectGuid, creature);
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
        return nullptr;

    auto objectGuid = ObjectGuid::Create<HighGuid::Creature>(data->mapId, data->id, lowguid);
    Map* map = sMapMgr->FindMap(data->mapId, 0);

    if (!map)
        return nullptr;

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
    QueryResult result = RoleplayDatabase.Query("SELECT `Key`, Entry from custom_npcs");
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

    TC_LOG_INFO("server.loading", ">> Loaded {} custom npcs in {} ms", count, GetMSTimeDiffToNow(oldMSTime));
}

void Roleplay::CreateCustomNpcFromPlayer(Player* player, std::string const& key)
{
    std::shared_ptr<CreatureOutfit> co(new CreatureOutfit());

    uint32 outfitId = sConfigMgr->GetInt64Default("Roleplay.CustomNpc.OutfitIdStart", 200001) + sObjectMgr->_creatureOutfitStore.size();
    if (!sObjectMgr->_creatureOutfitStore.empty()) {
        using pairtype = std::pair<uint32, std::shared_ptr<CreatureOutfit>>;
        outfitId = std::max_element(sObjectMgr->_creatureOutfitStore.begin(), sObjectMgr->_creatureOutfitStore.end(),
            [](pairtype a, pairtype b) { return a.second->id < b.second->id; })->second->id + 1;
    }

    co->id = outfitId;
    co->npcsoundsid = 0;
    co->race = player->GetRace();
    co->Class = player->GetClass();

    auto* maleModel = sDB2Manager.GetChrModel(co->race, GENDER_MALE);
    auto* femaleModel = sDB2Manager.GetChrModel(co->race, GENDER_FEMALE);

    co->gender = player->GetGender();
    switch (co->gender)
    {
    case GENDER_FEMALE: co->displayId = femaleModel->DisplayID; break;
    case GENDER_MALE:   co->displayId = maleModel->DisplayID; break;
    }

    co->SpellVisualKitID = 0;
    for (auto&& customization : player->m_playerData->Customizations)
    {
        co->Customizations.push_back(customization);
    }

    for (EquipmentSlots slot : CreatureOutfit::item_slots)
    {
        if (Item* pItem = player->GetItemByPos(INVENTORY_SLOT_BAG_0, slot))
        {
            int32 displayId = pItem->GetDisplayId(player);
            if (displayId == 0) {
                // weird case with items that don't have appearancemod of 0, just pick first appearance in the list
                for (ItemModifiedAppearanceEntry const* appearanceMod : sItemModifiedAppearanceStore)
                {
                    if ((uint32)appearanceMod->ItemID == pItem->GetEntry() && appearanceMod->OrderIndex == 0) {
                        displayId = sDB2Manager.GetItemDisplayId(pItem->GetEntry(), appearanceMod->ItemAppearanceModifierID);
                        break;
                    }
                }
            }
            co->outfitdisplays[slot] = displayId;
        }
        else {
            co->outfitdisplays[slot] = 0;
        }
    }
    co->guild = player->GetGuildId();


    sObjectMgr->_creatureOutfitStore[outfitId] = std::move(co);

    CreatureTemplate creatureTemplate;

    uint32 npcCreatureTemplateId = sConfigMgr->GetInt64Default("Freedom.CustomNpc.CreatureTemplateIdStart", 400000);
    if (!_customNpcStore.empty()) {
        using pairtype = std::pair<std::string, CustomNpcData>;
        npcCreatureTemplateId = std::max_element(_customNpcStore.begin(), _customNpcStore.end(),
            [](pairtype a, pairtype b) { return a.second.templateId < b.second.templateId; })->second.templateId + 1;
    }
    creatureTemplate.Entry = npcCreatureTemplateId;

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
    creatureTemplate.flags_extra = 0;
    creatureTemplate.ScriptID = sObjectMgr->GetScriptId("");

    creatureTemplate.Models.push_back(CreatureModel(outfitId, 1.0f, 1.0f));

    if (sWorld->getBoolConfig(CONFIG_CACHE_DATA_QUERIES)) {
        for (uint8 loc = LOCALE_enUS; loc < TOTAL_LOCALES; ++loc)
            creatureTemplate.QueryData[loc] = creatureTemplate.BuildQueryData(static_cast<LocaleConstant>(loc), DIFFICULTY_NONE);
    }

    sObjectMgr->CheckCreatureTemplate(&creatureTemplate);

    sObjectMgr->_creatureTemplateStore[creatureTemplate.Entry] = creatureTemplate;

    EquipmentInfo _equipmentInfo;
    for (uint8 equipmentInfoSlot = 0; equipmentInfoSlot < MAX_EQUIPMENT_ITEMS; equipmentInfoSlot++) {
        _equipmentInfo.Items[equipmentInfoSlot].ItemId = 0;
        _equipmentInfo.Items[equipmentInfoSlot].AppearanceModId = 0;
        _equipmentInfo.Items[equipmentInfoSlot].ItemVisual = 0;
    }
    sObjectMgr->_equipmentInfoStore[creatureTemplate.Entry][1] = _equipmentInfo;

    CustomNpcData npcData;
    npcData.key = key;
    npcData.templateId = npcCreatureTemplateId;

    _customNpcStore[key] = npcData;
    SaveNpcOutfitToDb(npcCreatureTemplateId, 1);
    SaveCustomNpcDataToDb(npcData);
    SaveNpcCreatureTemplateToDb(creatureTemplate);
    SaveNpcEquipmentInfoToDb(creatureTemplate.Entry, 1);
}

void Roleplay::SetCustomNpcOutfitEquipmentSlot(std::string const& key, uint8 variationId, EquipmentSlots slot, int32 displayId)
{
    uint32 templateId = _customNpcStore[key].templateId;
    EnsureNpcOutfitExists(templateId, variationId);
    uint32 outfitId = sObjectMgr->_creatureTemplateStore[templateId].Models[variationId - 1].CreatureDisplayID;
    TC_LOG_DEBUG("roleplay", "ROLEPLAY: Setting equipmentslot '%u' for custom npc '%s' with outfitId '%u' to '%u'", slot, key, outfitId, displayId);
    std::shared_ptr<CreatureOutfit> co = sObjectMgr->_creatureOutfitStore[outfitId];
    co->outfitdisplays[slot] = displayId;
    sObjectMgr->_creatureOutfitStore[outfitId] = std::move(co);
    SaveNpcOutfitToDb(templateId, variationId);
    ReloadSpawnedCustomNpcs(key);
}

void Roleplay::SetCustomNpcOutfitRace(std::string const& key, uint8 variationId, Races race)
{
    uint32 templateId = _customNpcStore[key].templateId;
    EnsureNpcOutfitExists(templateId, variationId);
    uint32 outfitId = sObjectMgr->_creatureTemplateStore[templateId].Models[variationId - 1].CreatureDisplayID;
    TC_LOG_DEBUG("roleplay", "ROLEPLAY: Setting race for custom npc '%s' with outfitId '%u'", key, outfitId);
    std::shared_ptr<CreatureOutfit> co = sObjectMgr->_creatureOutfitStore[outfitId];
    co->race = race;

    auto* maleModel = sDB2Manager.GetChrModel(co->race, GENDER_MALE);
    auto* femaleModel = sDB2Manager.GetChrModel(co->race, GENDER_FEMALE);
    switch (co->gender)
    {
    case GENDER_FEMALE: co->displayId = femaleModel->DisplayID; break;
    case GENDER_MALE:   co->displayId = maleModel->DisplayID; break;
    }
    sObjectMgr->_creatureOutfitStore[outfitId] = std::move(co);
    SaveNpcOutfitToDb(templateId, variationId);
    ReloadSpawnedCustomNpcs(key);
}

void Roleplay::SetCustomNpcOutfitGender(std::string const& key, uint8 variationId, Gender gender)
{
    uint32 templateId = _customNpcStore[key].templateId;
    EnsureNpcOutfitExists(templateId, variationId);
    uint32 outfitId = sObjectMgr->_creatureTemplateStore[templateId].Models[variationId - 1].CreatureDisplayID;
    TC_LOG_DEBUG("roleplay", "ROLEPLAY: Setting gender for custom npc '%s' with outfitId '%u'", key, outfitId);
    std::shared_ptr<CreatureOutfit> co = sObjectMgr->_creatureOutfitStore[outfitId];

    co->gender = gender;
    auto* maleModel = sDB2Manager.GetChrModel(co->race, GENDER_MALE);
    auto* femaleModel = sDB2Manager.GetChrModel(co->race, GENDER_FEMALE);
    switch (co->gender)
    {
    case GENDER_FEMALE: co->displayId = femaleModel->DisplayID; break;
    case GENDER_MALE:   co->displayId = maleModel->DisplayID; break;
    }
    sObjectMgr->_creatureOutfitStore[outfitId] = std::move(co);
    SaveNpcOutfitToDb(templateId, variationId);
    ReloadSpawnedCustomNpcs(key);
}

void Roleplay::SetCustomNpcLeftHand(std::string const& key, uint8 variationId, int32 itemId, int32 appearanceModId)
{
    uint32 templateId = _customNpcStore[key].templateId;
    EnsureEquipmentInfoExists(templateId, variationId);
    EquipmentInfo _equipmentInfo = sObjectMgr->_equipmentInfoStore[templateId][variationId];
    _equipmentInfo.Items[1].ItemId = itemId;
    _equipmentInfo.Items[1].AppearanceModId = appearanceModId;
    sObjectMgr->_equipmentInfoStore[templateId][variationId] = _equipmentInfo;
    SaveNpcEquipmentInfoToDb(templateId, variationId);
    ReloadSpawnedCustomNpcs(key);
}

void Roleplay::SetCustomNpcRightHand(std::string const& key, uint8 variationId, int32 itemId, int32 appearanceModId)
{
    uint32 templateId = _customNpcStore[key].templateId;
    EnsureEquipmentInfoExists(templateId, variationId);
    EquipmentInfo _equipmentInfo = sObjectMgr->_equipmentInfoStore[templateId][variationId];
    _equipmentInfo.Items[0].ItemId = itemId;
    _equipmentInfo.Items[0].AppearanceModId = appearanceModId;
    sObjectMgr->_equipmentInfoStore[templateId][variationId] = _equipmentInfo;
    SaveNpcEquipmentInfoToDb(templateId, variationId);
    ReloadSpawnedCustomNpcs(key);
}

void Roleplay::SetCustomNpcRanged(std::string const& key, uint8 variationId, int32 itemId, int32 appearanceModId)
{
    uint32 templateId = _customNpcStore[key].templateId;
    EnsureEquipmentInfoExists(templateId, variationId);
    EquipmentInfo _equipmentInfo = sObjectMgr->_equipmentInfoStore[templateId][variationId];
    _equipmentInfo.Items[2].ItemId = itemId;
    _equipmentInfo.Items[2].AppearanceModId = appearanceModId;
    sObjectMgr->_equipmentInfoStore[templateId][variationId] = _equipmentInfo;
    SaveNpcEquipmentInfoToDb(templateId, variationId);
    ReloadSpawnedCustomNpcs(key);
}

void Roleplay::SetCustomNpcDisplayId(std::string const& key, uint8 variationId, uint32 displayId)
{
    uint32 templateId = _customNpcStore[key].templateId;
    EnsureNpcModelExists(templateId, variationId);
    TC_LOG_DEBUG("roleplay", "ROLEPLAY: Setting model display id for custom npc '%s' variation '%u' to '%u'", key.c_str(), variationId, displayId);
    CreatureTemplate cTemplate = sObjectMgr->_creatureTemplateStore[templateId];
    CreatureModel model = cTemplate.Models[variationId - 1];
    model.CreatureDisplayID = displayId;
    cTemplate.Models[variationId - 1] = model;
    sObjectMgr->_creatureTemplateStore[templateId] = cTemplate;
    SaveNpcModelInfo(model, templateId, variationId);
    ReloadSpawnedCustomNpcs(key);
}

void Roleplay::SetCustomNpcModelScale(std::string const& key, uint8 variationId, float displayScale)
{
    uint32 templateId = _customNpcStore[key].templateId;
    EnsureNpcModelExists(templateId, variationId);
    TC_LOG_DEBUG("roleplay", "ROLEPLAY: Setting model scale for custom npc '%s' variation '%u' to '%f'", key.c_str(), variationId, displayScale);
    CreatureTemplate cTemplate = sObjectMgr->_creatureTemplateStore[templateId];
    CreatureModel model = cTemplate.Models[variationId - 1];
    model.DisplayScale = displayScale;
    cTemplate.Models[variationId - 1] = model;
    sObjectMgr->_creatureTemplateStore[templateId] = cTemplate;
    SaveNpcModelInfo(model, templateId, variationId);
    ReloadSpawnedCustomNpcs(key);
}

void Roleplay::SetCustomNpcGuild(std::string const& key, uint8 variationId, uint64 guild)
{
    uint32 templateId = _customNpcStore[key].templateId;
    EnsureNpcOutfitExists(templateId, variationId);
    uint32 outfitId = sObjectMgr->_creatureTemplateStore[templateId].Models[variationId - 1].CreatureDisplayID;
    TC_LOG_DEBUG("roleplay", "ROLEPLAY: Setting guild for custom npc '%s' with outfitId '%u'", key.c_str(), outfitId);

    std::shared_ptr<CreatureOutfit> co = sObjectMgr->_creatureOutfitStore[outfitId];
    co->guild = guild;
    sObjectMgr->_creatureOutfitStore[outfitId] = std::move(co);
    SaveNpcOutfitToDb(templateId, variationId);
    ReloadSpawnedCustomNpcs(key);
}

void Roleplay::SetCustomNpcTameable(std::string const& key, bool tameable)
{
    uint32 templateId = _customNpcStore[key].templateId;
    CreatureTemplate cTemplate = sObjectMgr->_creatureTemplateStore[templateId];
    cTemplate.type = tameable ? 1 : 0;
    cTemplate.family = tameable ? CREATURE_FAMILY_GORILLA : CREATURE_FAMILY_NONE;

    sObjectMgr->_creatureTemplateStore[cTemplate.Entry] = cTemplate;
    SaveNpcCreatureTemplateToDb(cTemplate);
    ReloadSpawnedCustomNpcs(key);
}

void Roleplay::SetCustomNpcName(std::string const& key, std::string const& displayName)
{
    uint32 templateId = _customNpcStore[key].templateId;
    CreatureTemplate cTemplate = sObjectMgr->_creatureTemplateStore[templateId];
    cTemplate.Name = displayName;

    sObjectMgr->_creatureTemplateStore[cTemplate.Entry] = cTemplate;
    SaveNpcCreatureTemplateToDb(cTemplate);
    ReloadSpawnedCustomNpcs(key);
}

void Roleplay::SetCustomNpcSubName(std::string const& key, std::string const& subName)
{
    uint32 templateId = _customNpcStore[key].templateId;
    CreatureTemplate cTemplate = sObjectMgr->_creatureTemplateStore[templateId];
    cTemplate.SubName = subName;

    sObjectMgr->_creatureTemplateStore[cTemplate.Entry] = cTemplate;
    SaveNpcCreatureTemplateToDb(cTemplate);
    ReloadSpawnedCustomNpcs(key);
}

void Roleplay::SetCustomNpcCustomizations(std::string const& key, uint8 variationId, Player* player)
{
    uint32 templateId = _customNpcStore[key].templateId;
    EnsureNpcOutfitExists(templateId, variationId);
    uint32 outfitId = sObjectMgr->_creatureTemplateStore[templateId].Models[variationId - 1].CreatureDisplayID;
    std::shared_ptr<CreatureOutfit> co = sObjectMgr->_creatureOutfitStore[outfitId];

    std::vector<UF::ChrCustomizationChoice> customizations;
    for (auto&& customization : player->m_playerData->Customizations)
    {
        customizations.push_back(customization);
    }
    co->Customizations = customizations;
    sObjectMgr->_creatureOutfitStore[outfitId] = std::move(co);
    SaveNpcOutfitToDb(templateId, variationId);
    ReloadSpawnedCustomNpcs(key);
}

void Roleplay::RemoveCustomNpcVariation(std::string const& key, uint8 variationId)
{
    uint32 templateId = _customNpcStore[key].templateId;
    CreatureTemplate cTemplate = sObjectMgr->_creatureTemplateStore[templateId];
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
    sObjectMgr->_creatureTemplateStore[templateId] = cTemplate;
    ReloadSpawnedCustomNpcs(key);
}

void Roleplay::SaveNpcOutfitToDb(uint32 templateId, uint8 variationId)
{
    TC_LOG_DEBUG("roleplay", "ROLEPLAY: Saving variation '%u' for templateid '%u' to DB...", variationId, templateId);
    CreatureTemplate cTemplate = sObjectMgr->_creatureTemplateStore[templateId];
    uint32 outfitId = cTemplate.Models[variationId - 1].CreatureDisplayID;
    std::shared_ptr<CreatureOutfit> co = sObjectMgr->_creatureOutfitStore[outfitId];
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
        stmt->setInt32(index++, -((int)co->outfitdisplays[slot]));
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

void Roleplay::SaveNpcCreatureTemplateToDb(CreatureTemplate cTemplate)
{
    TC_LOG_DEBUG("roleplay", "ROLEPLAY: Saving creature template id '%u' to DB...", cTemplate.Entry);
    // "REPLACE INTO creature_template (entry, name, subname, HealthScalingExpansion, RequiredExpansion, faction, unit_class, type, type_flags2, movementId, CreatureDifficultyID) VALUES (?, ?, ?, 8, 0, 35, 1, 7, 2, 100, 204488)"
    WorldDatabasePreparedStatement* stmt = WorldDatabase.GetPreparedStatement(WORLD_REP_CREATURE_TEMPLATE);
    int index = 0;
    stmt->setUInt32(index++, cTemplate.Entry);
    stmt->setString(index++, cTemplate.Name);
    stmt->setString(index++, cTemplate.SubName);
    WorldDatabase.Execute(stmt);

    // Redo cached creatureTemplates
    cTemplate.InitializeQueryData();

    // Resend creature template query packet for npc to all players. Best update we can do atm.
    SessionMap const& smap = sWorld->GetAllSessions();
    for (SessionMap::const_iterator iter = smap.begin(); iter != smap.end(); ++iter)
    {
        TC_LOG_DEBUG("roleplay", "ROLEPLAY: Sending query packet for creatureTemplate '%s' to '%s'.", cTemplate.Name.c_str(), iter->second->GetPlayer()->GetName().c_str());
        if (sWorld->getBoolConfig(CONFIG_CACHE_DATA_QUERIES)) {
            iter->second->SendPacket(&cTemplate.QueryData[static_cast<uint32>(iter->second->GetSessionDbLocaleIndex())]);
        }
        else
        {
            WorldPacket response = cTemplate.BuildQueryData(iter->second->GetSessionDbLocaleIndex(), DIFFICULTY_NONE);
            iter->second->SendPacket(&response);
        }
    }
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

void Roleplay::ReloadSpawnedCustomNpcs(std::string const& key)
{
    TC_LOG_DEBUG("roleplay", "ROLEPLAY: Reloading custom npc '%s'", key);
    CustomNpcData data = _customNpcStore[key];
    TC_LOG_DEBUG("roleplay", "ROLEPLAY: Iterated spawns has " SZFMTD " elements in it.", data.spawns.size());
    for (auto spawn : data.spawns) {
        Creature* creature = GetAnyCreature(spawn);
        if (!creature) {
            TC_LOG_DEBUG("roleplay", "ROLEPLAY: Unable to find creature id '%lu'", spawn);
        }
        else {
            TC_LOG_DEBUG("roleplay", "ROLEPLAY: Reloading creature id '%lu'", spawn);
            uint8 modelId = urand(0u, sObjectMgr->_creatureTemplateStore[data.templateId].Models.size() - 1);
            CreatureModel model = sObjectMgr->_creatureTemplateStore[data.templateId].Models[modelId];
            creature->SetDisplayId(model.CreatureDisplayID, model.DisplayScale);
            creature->SetName(sObjectMgr->_creatureTemplateStore[data.templateId].Name);
            creature->LoadEquipment(urand(1u, sObjectMgr->_equipmentInfoStore[data.templateId].size()));
            TC_LOG_DEBUG("roleplay", "ROLEPLAY: Reloaded creature id '%lu'", spawn);
        }
    }
}


void Roleplay::DeleteCustomNpc(std::string const& key)
{
    CustomNpcData data = _customNpcStore[key];
    TC_LOG_DEBUG("roleplay", "ROLEPLAY: Deleting custom npc '%s' with entry '%u'", key, data.templateId);
    // Remove spawns
    for (auto spawn : data.spawns) {
        TC_LOG_DEBUG("roleplay", "ROLEPLAY: Deleting spawn " UI64FMTD, spawn);
        Creature* creature = GetAnyCreature(spawn);
        CreatureDelete(creature);
    }
    // Cleanup database
    WorldDatabaseTransaction trans = WorldDatabase.BeginTransaction();

    WorldDatabasePreparedStatement* stmt = WorldDatabase.GetPreparedStatement(WORLD_DEL_ALL_CREATURE_TEMPLATE_MODEL);
    stmt->setUInt32(0, data.templateId);
    trans->Append(stmt);

    stmt = WorldDatabase.GetPreparedStatement(WORLD_DEL_CREATURE_EQUIP_TEMPLATE);
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

    _customNpcStore.erase(key);
    for (uint8 modelId = 0; modelId < sObjectMgr->_creatureTemplateStore[data.templateId].Models.size(); modelId++)
    {
        uint32 outfitId = sObjectMgr->_creatureTemplateStore[data.templateId].Models[modelId].CreatureDisplayID;
        sObjectMgr->_creatureOutfitStore.erase(outfitId);
    }
    sObjectMgr->_equipmentInfoStore.erase(data.templateId);
    sObjectMgr->_creatureTemplateStore.erase(data.templateId);
}

void Roleplay::EnsureNpcOutfitExists(uint32 templateId, uint8 variationId, float displayScale)
{
    CreatureTemplate cTemplate = sObjectMgr->_creatureTemplateStore[templateId];
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
        using pairtype = std::pair<uint32, std::shared_ptr<CreatureOutfit>>;
        uint32 maxOutfitId = std::max_element(sObjectMgr->_creatureOutfitStore.begin(), sObjectMgr->_creatureOutfitStore.end(),
            [](pairtype a, pairtype b) { return a.second->id < b.second->id; })->second->id;
        std::shared_ptr<CreatureOutfit> lastOutfit;
        bool setOutfit = false;
        for (uint8 i = 1; i <= modelsSize; i++) {
            uint32 displayId = cTemplate.Models[modelsSize - i].CreatureDisplayID;
            if (CreatureOutfit::IsFake(displayId)) {
                lastOutfit = sObjectMgr->_creatureOutfitStore[displayId];
                setOutfit = true;
                break;
            }
        }
        if (!setOutfit) {
            TC_LOG_DEBUG("roleplay", "ROLEPLAY: Custom NPC template '%u' has no outfits, selecting first option in store for variation '%u'.", templateId, variationId);
            // Custom NPC has only used displayids, in this case we'll just take the first creatureoutfit available.
            lastOutfit = sObjectMgr->_creatureOutfitStore.begin()->second;
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
            sObjectMgr->_creatureTemplateStore[templateId] = cTemplate;
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
    CreatureTemplate cTemplate = sObjectMgr->_creatureTemplateStore[templateId];
    uint32 modelsSize = cTemplate.Models.size();
    if (modelsSize < variationId) {
        for (uint8 i = modelsSize; i < variationId; i++) {
            uint32 prevDisplayId = cTemplate.Models[modelsSize - 1].CreatureDisplayID;
            TC_LOG_DEBUG("roleplay", "ROLEPLAY: Creating model variation '%u' for template '%u' using displayid: '%u'.", i, templateId, prevDisplayId);
            cTemplate.Models.push_back(CreatureModel(prevDisplayId, 1, 1));
            sObjectMgr->_creatureTemplateStore[templateId] = cTemplate;
        }
    }
    TC_LOG_DEBUG("roleplay", "ROLEPLAY: Model variation '%u' for template '%u' ensured.", variationId, templateId);
}

uint8 Roleplay::GetModelVariationCountForNpc(std::string const& key) {
    return sObjectMgr->_creatureTemplateStore[_customNpcStore[key].templateId].Models.size();
}
uint8 Roleplay::GetEquipmentVariationCountForNpc(std::string const& key) {
    return sObjectMgr->_equipmentInfoStore[_customNpcStore[key].templateId].size();
}

#pragma endregion
