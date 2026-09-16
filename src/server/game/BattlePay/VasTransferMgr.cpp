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

#include "VasTransferMgr.h"
#include "Config.h"
#include "DatabaseEnv.h"
#include "Log.h"
#include "ObjectAccessor.h"
#include "Player.h"
#include "RealmList.h"
#include "StringConvert.h"
#include "StringFormat.h"
#include "Util.h"
#include "World.h"
#include <array>

namespace
{
    // Tables keyed directly by the character guid. Column name varies by table; taken from the
    // Player::DeleteFromDB WHERE clauses so the set matches what a delete would remove.
    struct CharTable { char const* Table; char const* Column; };
    constexpr std::array<CharTable, 47> kCharGuidTables =
    { {
        { "character_account_data",                     "guid" },
        { "character_achievement",                      "guid" },
        { "character_achievement_progress",             "guid" },
        { "character_action",                           "guid" },
        { "character_arena_stats",                      "guid" },
        { "character_aura",                             "guid" },
        { "character_aura_effect",                      "guid" },
        { "character_aura_stored_location",             "Guid" },
        { "character_bank_tab_settings",                "characterGuid" },
        { "character_battleground_data",                "guid" },
        { "character_battleground_random",              "guid" },
        { "character_cuf_profiles",                     "guid" },
        { "character_currency",                         "CharacterGuid" },
        { "character_customizations",                   "guid" },
        { "character_declinedname",                     "guid" },
        { "character_equipmentsets",                    "guid" },
        { "character_favorite_auctions",                "guid" },
        { "character_fishingsteps",                     "guid" },
        { "character_garrison",                         "guid" },
        { "character_garrison_blueprints",              "guid" },
        { "character_garrison_buildings",               "guid" },
        { "character_gifts",                            "guid" },
        { "character_glyphs",                           "guid" },
        { "character_homebind",                         "guid" },
        { "character_instance_lock",                    "guid" },
        { "character_player_data_element",              "characterGuid" },
        { "character_player_data_flag",                 "characterGuid" },
        { "character_pvp_talent",                       "guid" },
        { "character_queststatus",                      "guid" },
        { "character_queststatus_daily",                "guid" },
        { "character_queststatus_monthly",              "guid" },
        { "character_queststatus_objectives",           "guid" },
        { "character_queststatus_objectives_criteria",  "guid" },
        { "character_queststatus_objectives_criteria_progress", "guid" },
        { "character_queststatus_rewarded",             "guid" },
        { "character_queststatus_seasonal",             "guid" },
        { "character_queststatus_weekly",               "guid" },
        { "character_reputation",                       "guid" },
        { "character_select_screen_equipment_cache",    "guid" },
        { "character_skills",                           "guid" },
        { "character_spell",                            "guid" },
        { "character_spell_charges",                    "guid" },
        { "character_spell_cooldown",                   "guid" },
        { "character_spell_favorite",                   "guid" },
        { "character_stats",                            "guid" },
        { "character_talent",                           "guid" },
        { "character_inventory",                        "guid" },
    } };

    // item_instance child tables keyed by item guid (itemGuid), moved with the character's items.
    constexpr std::array<char const*, 9> kItemChildTables =
    { {
        "item_instance_gems",
        "item_instance_modifiers",
        "item_instance_transmog",
        "item_instance_artifact",
        "item_instance_artifact_powers",
        "item_instance_azerite",
        "item_instance_azerite_empowered",
        "item_instance_azerite_milestone_power",
        "item_instance_azerite_unlocked_essence",
    } };
}

VasTransferMgr* VasTransferMgr::instance()
{
    static VasTransferMgr instance;
    return &instance;
}

void VasTransferMgr::LoadConfig()
{
    _realmDatabases.clear();

    // "1:tc_characters,2:playerbot_characters,3:integ_characters"
    std::string const raw = std::string(sConfigMgr->GetStringDefault("VAS.TransferRealmDatabases", ""));
    for (std::string_view pair : Trinity::Tokenize(raw, ',', false))
    {
        std::vector<std::string_view> const kv = Trinity::Tokenize(pair, ':', false);
        if (kv.size() != 2)
            continue;
        Optional<uint32> const realmId = Trinity::StringTo<uint32>(kv[0]);
        if (!realmId || kv[1].empty())
            continue;
        _realmDatabases[*realmId] = std::string(kv[1]);
    }

    _enabled = !_realmDatabases.empty();
    if (_enabled)
        TC_LOG_INFO("server.loading", "VAS character transfer: {} realm character databases mapped.", _realmDatabases.size());
}

std::string VasTransferMgr::GetRealmDatabase(uint32 realmId) const
{
    auto it = _realmDatabases.find(realmId);
    return it != _realmDatabases.end() ? it->second : std::string();
}

char const* VasTransferMgr::ResultString(TransferResult result)
{
    switch (result)
    {
        case TRANSFER_OK:                 return "ok";
        case TRANSFER_ERR_NO_TARGET:      return "no characters database mapped for the target realm";
        case TRANSFER_ERR_NO_SOURCE:      return "no characters database mapped for this realm";
        case TRANSFER_ERR_CHAR_NOT_FOUND: return "no such character on this realm";
        case TRANSFER_ERR_IN_WORLD:       return "the character is online (transfer only from character select)";
        case TRANSFER_ERR_NAME_TAKEN:     return "a character with that name already exists on the target realm";
        case TRANSFER_ERR_GUID_COLLISION: return "the character or one of its items already exists on the target realm";
        case TRANSFER_ERR_DB:             return "a database error occurred; nothing was moved";
        default:                          return "unknown";
    }
}

VasTransferMgr::TransferResult VasTransferMgr::TransferCharacter(ObjectGuid::LowType charGuid, uint32 targetRealmId,
    std::string* outName, std::string* outTargetDb, bool validateOnly)
{
    uint32 const currentRealmId = sRealmList->GetCurrentRealmId().Realm;

    std::string const sourceDb = GetRealmDatabase(currentRealmId);
    if (sourceDb.empty())
        return TRANSFER_ERR_NO_SOURCE;

    std::string const targetDb = GetRealmDatabase(targetRealmId);
    if (targetDb.empty() || targetDb == sourceDb || targetRealmId == currentRealmId)
        return TRANSFER_ERR_NO_TARGET;

    if (outTargetDb)
        *outTargetDb = targetDb;

    // The character must exist on this realm and be offline. Only the numeric guid is ever inlined into the
    // cross-db SQL below; the character name is compared entirely inside SQL (a join on the two characters
    // tables), so no string value is ever concatenated into a query.
    if (Player* online = ObjectAccessor::FindConnectedPlayer(ObjectGuid::Create<HighGuid::Player>(charGuid)))
    {
        (void)online;
        return TRANSFER_ERR_IN_WORLD;
    }

    if (QueryResult nameRes = CharacterDatabase.Query(Trinity::StringFormat(
        "SELECT name FROM `{}`.characters WHERE guid = {}", sourceDb, charGuid).c_str()))
    {
        if (outName)
            *outName = (*nameRes)[0].GetString();
    }
    else
        return TRANSFER_ERR_CHAR_NOT_FOUND;

    // Name is the only per-realm unique key: reject if the target already has one. Compared via a join so the
    // name never leaves SQL.
    if (CharacterDatabase.Query(Trinity::StringFormat(
        "SELECT 1 FROM `{}`.characters t JOIN `{}`.characters s ON t.name = s.name WHERE s.guid = {} LIMIT 1",
        targetDb, sourceDb, charGuid).c_str()))
        return TRANSFER_ERR_NAME_TAKEN;

    // Guids are kept, not remapped, so refuse rather than corrupt if the character guid or any of its item
    // guids already exist on the target.
    if (CharacterDatabase.Query(Trinity::StringFormat(
        "SELECT 1 FROM `{}`.characters WHERE guid = {} LIMIT 1", targetDb, charGuid).c_str()))
        return TRANSFER_ERR_GUID_COLLISION;

    if (CharacterDatabase.Query(Trinity::StringFormat(
        "SELECT 1 FROM `{}`.item_instance t JOIN `{}`.item_instance s ON t.guid = s.guid "
        "WHERE s.owner_guid = {} LIMIT 1", targetDb, sourceDb, charGuid).c_str()))
        return TRANSFER_ERR_GUID_COLLISION;

    // Everything the transfer can be rejected for has been checked. A validation-only request stops here with
    // success - nothing is moved.
    if (validateOnly)
        return TRANSFER_OK;

    // Move the rows in one transaction, then remove them from the source. INSERT ... SELECT keeps every guid,
    // so all foreign references stay valid without a remap.
    CharacterDatabaseTransaction trans = CharacterDatabase.BeginTransaction();

    auto copyDel = [&](std::string const& copySql, std::string const& delSql)
    {
        trans->Append(copySql.c_str());
        trans->Append(delSql.c_str());
    };

    // The characters row first (so the target row exists before anything references it).
    copyDel(
        Trinity::StringFormat("INSERT INTO `{}`.characters SELECT * FROM `{}`.characters WHERE guid = {}", targetDb, sourceDb, charGuid),
        Trinity::StringFormat("DELETE FROM `{}`.characters WHERE guid = {}", sourceDb, charGuid));

    for (CharTable const& t : kCharGuidTables)
        copyDel(
            Trinity::StringFormat("INSERT INTO `{}`.{} SELECT * FROM `{}`.{} WHERE {} = {}", targetDb, t.Table, sourceDb, t.Table, t.Column, charGuid),
            Trinity::StringFormat("DELETE FROM `{}`.{} WHERE {} = {}", sourceDb, t.Table, t.Column, charGuid));

    // The item_instance children (keyed by item guid) BEFORE item_instance is deleted from the source, using
    // the source's item_instance to resolve the owner's item guids.
    std::string const ownedItems = Trinity::StringFormat("(SELECT guid FROM `{}`.item_instance WHERE owner_guid = {})", sourceDb, charGuid);
    for (char const* child : kItemChildTables)
    {
        trans->Append(Trinity::StringFormat("INSERT INTO `{}`.{} SELECT * FROM `{}`.{} WHERE itemGuid IN {}", targetDb, child, sourceDb, child, ownedItems).c_str());
        trans->Append(Trinity::StringFormat("DELETE FROM `{}`.{} WHERE itemGuid IN {}", sourceDb, child, ownedItems).c_str());
    }

    // Then item_instance itself.
    copyDel(
        Trinity::StringFormat("INSERT INTO `{}`.item_instance SELECT * FROM `{}`.item_instance WHERE owner_guid = {}", targetDb, sourceDb, charGuid),
        Trinity::StringFormat("DELETE FROM `{}`.item_instance WHERE owner_guid = {}", sourceDb, charGuid));

    // Mail and mail items (mail keyed by receiver; mail_items by the moved mail ids).
    std::string const ownedMail = Trinity::StringFormat("(SELECT id FROM `{}`.mail WHERE receiver = {})", sourceDb, charGuid);
    trans->Append(Trinity::StringFormat("INSERT INTO `{}`.mail_items SELECT * FROM `{}`.mail_items WHERE mail_id IN {}", targetDb, sourceDb, ownedMail).c_str());
    trans->Append(Trinity::StringFormat("DELETE FROM `{}`.mail_items WHERE mail_id IN {}", sourceDb, ownedMail).c_str());
    copyDel(
        Trinity::StringFormat("INSERT INTO `{}`.mail SELECT * FROM `{}`.mail WHERE receiver = {}", targetDb, sourceDb, charGuid),
        Trinity::StringFormat("DELETE FROM `{}`.mail WHERE receiver = {}", sourceDb, charGuid));

    // Synchronous commit so the outcome is known before we answer: DirectCommitTransaction blocks until the
    // whole transaction is applied or rolled back, then a single confirming query tells us the character now
    // lives on the target. A rollback (e.g. a schema divergence between the two realms' character DBs) leaves
    // the source untouched - the character is never left half-moved.
    CharacterDatabase.DirectCommitTransaction(trans);

    if (!CharacterDatabase.Query(Trinity::StringFormat(
        "SELECT 1 FROM `{}`.characters WHERE guid = {} LIMIT 1", targetDb, charGuid).c_str()))
    {
        TC_LOG_ERROR("entities.player.character", "VAS transfer: character {} did NOT land on `{}` (transaction "
            "rolled back - likely a schema mismatch between the source and target character databases); the "
            "character remains on `{}`.", charGuid, targetDb, sourceDb);
        return TRANSFER_ERR_DB;
    }

    TC_LOG_INFO("entities.player.character", "VAS transfer: character {} moved from `{}` to `{}` (realm {}).",
        charGuid, sourceDb, targetDb, targetRealmId);
    return TRANSFER_OK;
}
