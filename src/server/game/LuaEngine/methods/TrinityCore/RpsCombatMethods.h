#pragma once

#include "GameTime.h"
#include "RoleplayPhaseMgr.h"
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
    int RpsCommitAsync(Eluna* E)
    {
        std::string pool = E->CHECKVAL<std::string>(1);
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
