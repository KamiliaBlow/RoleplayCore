#pragma once

#include "GameTime.h"
#include "ItemBonusMgr.h"
#include "RoleplayPhaseMgr.h"
#include "RpsItemMetadata.h"
#include <chrono>
#include <deque>
#include <mutex>

// Only immutable IDs cross MAP/WORLD. This queue owns neither Lua references nor Units.
namespace RpsCombatBridge
{
    using Clock = std::chrono::steady_clock;
    struct Cast
    {
        ObjectGuid PlayerGuid;
        ObjectGuid TargetGuid;
        uint32 SpellId;
        uint32 MapId;
        uint32 InstanceId;
        Clock::time_point Created;
    };
    inline std::mutex Mutex;
    inline std::deque<Cast> Casts;

    inline bool SharesContext(Player* player, Unit* target)
    {
        if (!player || !target || !player->IsInWorld() || !target->IsInWorld()
            || player->GetMap() != target->GetMap() || !player->CanShareRoleplayContext(target)
            || !player->CanSeeOrDetect(target)
            || !player->IsWithinDistInMap(target, 40.0f) || !player->IsWithinLOSInMap(target))
            return false;
        return true;
    }

    // "ok" | "zero_id" | "no_spellinfo" | "effect:<index>:<type>"
    inline std::string DiceSpellStatus(uint32 id)
    {
        if (!id)
            return "zero_id";
        SpellInfo const* info = sSpellMgr->GetSpellInfo(id, DIFFICULTY_NONE);
        if (!info)
            return "no_spellinfo";
        for (SpellEffectInfo const& effect : info->GetEffects())
            if (effect.Effect != SPELL_EFFECT_NONE && effect.Effect != SPELL_EFFECT_DUMMY)
                return "effect:" + std::to_string(effect.EffectIndex) + ":" + std::to_string(uint32(effect.Effect));
        return "ok";
    }
}

namespace LuaGlobalFunctions
{
    // Public template facts only; previewing a vendor item never reads an owned instance.
    int RpsGetItemTemplate(Eluna* E)
    {
        ItemTemplate const* item = sObjectMgr->GetItemTemplate(E->CHECKVAL<uint32>(1));
        if (!item)
            return 0;
        E->Push(item->GetClass());
        E->Push(item->GetSubClass());
        E->Push(item->GetInventoryType());
        E->Push(item->GetQuality());
        E->Push(item->GetMaxStackSize());
        E->Push(item->HasFlag(ITEM_FLAGS_CU_NO_VENDOR_VALUE));
        return 6;
    }

    // Exact item GUIDs remain strings on the AIO boundary, never Lua doubles.
    int RpsFindOwnedItem(Eluna* E)
    {
        Player* player = E->CHECKOBJ<Player>(1);
        std::string text = E->CHECKVAL<std::string>(2);
        ObjectGuid guid = text.size() <= 128 ? ObjectGuid::FromString(text) : ObjectGuid::Empty;
        Item* item = guid.IsItem() ? player->GetItemByGuid(guid) : nullptr;
        E->Push(item && item->GetOwnerGUID() == player->GetGUID() ? item : nullptr);
        return 1;
    }

    // Single, nonstackable ITEM_FLAGS_CU_RP_ONLY carriers: native stats are zeroed by the core.
    int RpsIsRoleplayItem(Eluna* E)
    {
        ItemTemplate const* itemTemplate = sObjectMgr->GetItemTemplate(E->CHECKVAL<uint32>(1));
        E->Push(itemTemplate && itemTemplate->IsRoleplayOnly() && itemTemplate->GetMaxStackSize() == 1);
        return 1;
    }

    // RP grade = native item quality. Quality 1..5 maps to the reserved ItemBonus lists 60001..60005
    // (hotfix package rps-item-enchants); the item's other bonus lists are preserved.
    constexpr int32 RpsGradeBonusListFirst = 60001;
    constexpr uint32 RpsGradeMaxQuality = 5;

    int RpsGetItemQuality(Eluna* E)
    {
        E->Push(E->CHECKOBJ<Item>(1)->GetQuality());
        return 1;
    }

    int RpsSetItemGrade(Eluna* E)
    {
        Player* player = E->CHECKOBJ<Player>(1);
        Item* item = E->CHECKOBJ<Item>(2);
        uint32 quality = E->CHECKVAL<uint32>(3);
        int32 gradeList = RpsGradeBonusListFirst + int32(quality) - 1;
        if (item->GetOwnerGUID() != player->GetGUID() || quality > RpsGradeMaxQuality
            || (quality && ItemBonusMgr::GetItemBonuses(gradeList).empty()))
        {
            E->Push(false);
            return 1;
        }

        std::vector<int32> bonusLists;
        for (int32 bonusList : item->GetBonusListIDs())
            if (bonusList < RpsGradeBonusListFirst || bonusList >= RpsGradeBonusListFirst + int32(RpsGradeMaxQuality))
                bonusLists.push_back(bonusList);
        if (quality)
            bonusLists.push_back(gradeList);

        bool equipped = item->IsEquipped();
        if (equipped)
            player->_ApplyItemMods(item, item->GetSlot(), false);
        item->ClearBonuses();
        item->SetBonuses(std::move(bonusLists));
        if (equipped)
            player->_ApplyItemMods(item, item->GetSlot(), true);
        item->SetState(ITEM_CHANGED, player);
        E->Push(true);
        return 1;
    }

    // RP slot 0 = the native permanent enchant (its name shows in the stock tooltip); RP gems 1..3 use
    // the otherwise unused PROP slots, so native sockets and real WoW gems are never overwritten.
    constexpr EnchantmentSlot RpsEnchantSlots[] = { PERM_ENCHANTMENT_SLOT, PROP_ENCHANTMENT_SLOT_0, PROP_ENCHANTMENT_SLOT_1, PROP_ENCHANTMENT_SLOT_2 };

    int RpsGetItemEnchants(Eluna* E)
    {
        Item* item = E->CHECKOBJ<Item>(1);
        for (EnchantmentSlot slot : RpsEnchantSlots)
            E->Push(item->GetEnchantmentId(slot));
        return int(std::size(RpsEnchantSlots));
    }

    int RpsSetItemEnchant(Eluna* E)
    {
        Player* player = E->CHECKOBJ<Player>(1);
        Item* item = E->CHECKOBJ<Item>(2);
        uint32 rpSlot = E->CHECKVAL<uint32>(3);
        uint32 enchantId = E->CHECKVAL<uint32>(4);
        if (item->GetOwnerGUID() != player->GetGUID() || rpSlot >= std::size(RpsEnchantSlots)
            || (enchantId && !sSpellItemEnchantmentStore.LookupEntry(enchantId)))
        {
            E->Push(false);
            return 1;
        }
        if (SpellItemEnchantmentEntry const* enchant = sSpellItemEnchantmentStore.LookupEntry(enchantId))
            for (uint8 effect = 0; effect < MAX_ITEM_ENCHANTMENT_EFFECTS; ++effect)
                if ((enchant->Effect[effect] == ITEM_ENCHANTMENT_TYPE_EQUIP_SPELL
                    || enchant->Effect[effect] == ITEM_ENCHANTMENT_TYPE_USE_SPELL
                    || enchant->Effect[effect] == ITEM_ENCHANTMENT_TYPE_COMBAT_SPELL)
                    && !sSpellMgr->GetSpellInfo(enchant->EffectArg[effect], DIFFICULTY_NONE))
                {
                    E->Push(false);
                    return 1;
                }
        EnchantmentSlot slot = RpsEnchantSlots[rpSlot];
        player->ApplyEnchantment(item, slot, false);
        item->SetEnchantment(slot, enchantId, 0, 0);
        player->ApplyEnchantment(item, slot, true);
        E->Push(true);
        return 1;
    }

    void PushRpsModifiers(Eluna* E, RpsItemMetadata::ModifierMap const& modifiers)
    {
        lua_createtable(E->L, 0, modifiers.size());
        for (auto const& [key, value] : modifiers)
        {
            E->Push(value);
            lua_setfield(E->L, -2, key.c_str());
        }
    }

    int RpsGetItemModifierDefinitions(Eluna* E)
    {
        auto definitions = RpsItemMetadata::ModifierDefinitions();
        lua_createtable(E->L, definitions.size(), 0);
        int index = 0;
        for (auto const& definition : definitions)
        {
            lua_createtable(E->L, 0, 3);
            E->Push(std::string(definition.Key)); lua_setfield(E->L, -2, "key");
            E->Push(std::string(definition.Label)); lua_setfield(E->L, -2, "label");
            E->Push(definition.Limit); lua_setfield(E->L, -2, "limit");
            lua_rawseti(E->L, -2, ++index);
        }
        return 1;
    }

    int RpsParseItemMetadata(Eluna* E)
    {
        std::string error;
        auto metadata = RpsItemMetadata::Parse(E->CHECKVAL<std::string>(1), &error);
        if (!metadata)
        {
            lua_pushnil(E->L);
            E->Push(error);
            return 2;
        }
        lua_createtable(E->L, 0, 6);
        E->Push(metadata->Version); lua_setfield(E->L, -2, "version");
        E->Push(metadata->Revision); lua_setfield(E->L, -2, "revision");
        E->Push(metadata->Name); lua_setfield(E->L, -2, "name");
        E->Push(metadata->Description); lua_setfield(E->L, -2, "description");
        PushRpsModifiers(E, metadata->Modifiers);
        lua_setfield(E->L, -2, "modifiers");
        if (metadata->Properties)
        {
            PushRpsModifiers(E, *metadata->Properties);
            lua_setfield(E->L, -2, "properties");
        }
        lua_createtable(E->L, 0, metadata->Runes.size());
        for (auto const& [socket, modifiers] : metadata->Runes)
        {
            PushRpsModifiers(E, modifiers);
            lua_setfield(E->L, -2, socket.c_str());
        }
        lua_setfield(E->L, -2, "runes");
        E->Push(metadata->Serialize());
        return 2;
    }

    int RpsIsDiceSpell(Eluna* E)
    {
        E->Push(RpsCombatBridge::DiceSpellStatus(E->CHECKVAL<uint32>(1)) == "ok");
        return 1;
    }

    int RpsDiceSpellStatus(Eluna* E)
    {
        std::string reason = RpsCombatBridge::DiceSpellStatus(E->CHECKVAL<uint32>(1));
        E->Push(reason == "ok");
        E->Push(reason);
        return 2;
    }

    // Server scripts only. Same SQL authority as existing Execute, with a commit callback.
    // A successful callback confirms the transaction, not affected rows of arbitrary UPDATE.
    int CommitRpsAsync(Eluna* E, bool saveXp)
    {
        Player* player = saveXp ? E->CHECKOBJ<Player>(1) : nullptr;
        std::string pool = saveXp ? "characters" : E->CHECKVAL<std::string>(1);
        luaL_checktype(E->L, 2, LUA_TTABLE);
        luaL_checktype(E->L, 3, LUA_TFUNCTION);
        std::vector<std::string> statements;
        std::size_t bytes = 0;
        for (int i = 1; ; ++i)
        {
            lua_rawgeti(E->L, 2, i);
            if (lua_isnil(E->L, -1)) { lua_pop(E->L, 1); break; }
            std::string statement = E->CHECKVAL<std::string>(-1);
            lua_pop(E->L, 1);
            bytes += statement.size();
            if (i > 64 || bytes > 262144 || statement.empty())
                return luaL_error(E->L, "RPS transaction limit exceeded");
            statements.push_back(std::move(statement));
        }
        if (statements.empty() || (pool != "characters" && pool != "world"))
            return luaL_error(E->L, "Invalid RPS transaction pool or statements");
        // Capture only values. XP and its receipt share a CharacterDatabase commit;
        // no Player userdata is retained while the transaction is running.
        if (player)
            statements.insert(statements.begin(), Trinity::StringFormat(
                "UPDATE characters SET level={}, xp={} WHERE guid={}",
                player->GetLevel(), player->GetXP(), player->GetGUID().GetCounter()));
        lua_pushvalue(E->L, 3);
        int ref = luaL_ref(E->L, LUA_REGISTRYINDEX);
        auto enqueue = [&](auto& database)
        {
            auto transaction = database.BeginTransaction();
            for (std::string const& statement : statements)
                transaction->Append(statement.c_str());
            auto callback = database.AsyncCommitTransaction(transaction);
            callback.AfterComplete([E, ref](bool success)
            {
                lua_rawgeti(E->L, LUA_REGISTRYINDEX, ref);
                E->Push(success);
                E->ExecuteCall(1, 0);
                luaL_unref(E->L, LUA_REGISTRYINDEX, ref);
            });
            E->GetTransactionProcessor().AddCallback(std::move(callback));
        };
        if (pool == "characters") enqueue(CharacterDatabase); else enqueue(WorldDatabase);
        return 0;
    }

    int RpsCommitAsync(Eluna* E) { return CommitRpsAsync(E, false); }
    int RpsSaveXpAsync(Eluna* E) { return CommitRpsAsync(E, true); }

    int RpsXpGrantContext(Eluna* E)
    {
        Player* player = E->CHECKOBJ<Player>(1);
        uint8 first = E->CHECKVAL<uint8>(2);
        uint8 last = E->CHECKVAL<uint8>(3);
        if (!first || first >= last || last > MAX_LEVEL)
            return luaL_error(E->L, "Invalid XP level range");
        E->Push(sWorld->getIntConfig(CONFIG_MAX_PLAYER_LEVEL));
        E->Push(player->GetsRecruitAFriendBonus(true) || player->HasPlayerFlag(PLAYER_FLAGS_NO_XP_GAIN));
        lua_newtable(E->L);
        for (uint8 level = first; level < last; ++level)
        {
            E->Push(sObjectMgr->GetXPForLevel(level));
            lua_rawseti(E->L, -2, level);
        }
        return 3;
    }

    int RpsQueueDiceCast(Eluna* E)
    {
        Player* player = E->CHECKOBJ<Player>(1);
        Spell* spell = E->CHECKOBJ<Spell>(2);
        if (spell->GetCaster() != player) { E->Push(false); E->Push("caster_mismatch"); return 2; }
        std::lock_guard lock(RpsCombatBridge::Mutex);
        auto now = RpsCombatBridge::Clock::now();
        std::erase_if(RpsCombatBridge::Casts, [now](auto const& cast) { return now - cast.Created > std::chrono::seconds(5); });
        if (RpsCombatBridge::Casts.size() >= 1024) { E->Push(false); E->Push("queue_full"); return 2; }
        if (std::ranges::any_of(RpsCombatBridge::Casts, [player](auto const& cast) { return cast.PlayerGuid == player->GetGUID(); }))
        { E->Push(false); E->Push("cast_pending"); return 2; }
        RpsCombatBridge::Casts.push_back({player->GetGUID(), player->GetTarget(), spell->GetSpellInfo()->Id,
            player->GetMapId(), player->GetInstanceId(), now});
        E->Push(true);
        return 1;
    }

    int RpsPollDiceCasts(Eluna* E)
    {
        std::deque<RpsCombatBridge::Cast> casts;
        { std::lock_guard lock(RpsCombatBridge::Mutex); casts.swap(RpsCombatBridge::Casts); }
        lua_newtable(E->L);
        int index = 0;
        for (auto const& cast : casts)
        {
            Player* player = ObjectAccessor::FindPlayer(cast.PlayerGuid);
            if (!player || !player->IsInWorld())
                continue;
            char const* rejected = nullptr;
            if (player->GetMapId() != cast.MapId || player->GetInstanceId() != cast.InstanceId)
                rejected = "map_changed";
            else if (player->GetTarget() != cast.TargetGuid)
                rejected = "target_changed";
            else if (RpsCombatBridge::Clock::now() - cast.Created > std::chrono::seconds(5))
                rejected = "expired";
            lua_createtable(E->L, 0, 3);
            E->Push(player); lua_setfield(E->L, -2, "player");
            E->Push(cast.SpellId); lua_setfield(E->L, -2, "spellId");
            if (rejected) { E->Push(rejected); lua_setfield(E->L, -2, "rejected"); }
            lua_rawseti(E->L, -2, ++index);
        }
        return 1;
    }

    int RpsTargetContext(Eluna* E)
    {
        Player* player = E->CHECKOBJ<Player>(1);
        Unit* target = E->CHECKOBJ<Unit>(2);
        bool edit = E->CHECKVAL<bool>(3, false);
        bool allowed = RpsCombatBridge::SharesContext(player, target);
        if (allowed && edit)
        {
            uint64 phase = sRoleplayPhaseMgr.GetPlayerPhaseId(player->GetGUID().GetCounter(), player->GetMapId());
            bool admin = player->GetSession()->GetSecurity() >= SEC_GAMEMASTER;
            bool editor = phase && sRoleplayPhaseMgr.CanEdit(phase, player->GetGUID().GetCounter(),
                player->GetSession()->GetAccountId(), false, player->GetSession()->GetSecurity() > SEC_PLAYER);
            allowed = admin || editor;
        }
        E->Push(allowed);
        return 1;
    }

    int RpsRuntimeEpoch(Eluna* E)
    {
        static std::string const epoch = std::to_string(GameTime::GetStartTime()) + "-"
            + std::to_string(RpsCombatBridge::Clock::now().time_since_epoch().count());
        E->Push(epoch);
        return 1;
    }
}
