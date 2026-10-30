#pragma once

namespace RpsConsumableBridge
{
    struct Use
    {
        ObjectGuid PlayerGuid, ItemGuid, TargetGuid;
        uint32 MapId, InstanceId;
        RpsCombatBridge::Clock::time_point Created;
    };
    inline std::mutex Mutex;
    inline std::deque<Use> Uses;

    inline bool CanUse(Player* player, Player* target)
    {
        return player && target && player->IsAlive() && target->IsAlive()
            && !player->IsInCombat() && !target->IsInCombat()
            && RpsCombatBridge::SharesContext(player, target) && player->IsWithinDistInMap(target, 30.0f);
    }
}

namespace LuaGlobalFunctions
{
    int RpsConsumableContext(Eluna* E)
    {
        E->Push(RpsConsumableBridge::CanUse(E->CHECKOBJ<Player>(1), E->CHECKOBJ<Player>(2)));
        return 1;
    }

    int RpsQueueConsumableUse(Eluna* E)
    {
        Player* player = E->CHECKOBJ<Player>(1);
        Item* item = E->CHECKOBJ<Item>(2);
        Unit* target = E->CHECKOBJ<Unit>(3, false);
        if (!target)
            target = player;
        bool valid = item->GetOwnerGUID() == player->GetGUID()
            && item->GetCount() == 1 && item->GetTemplate()->GetMaxStackSize() == 1
            && RpsConsumableBridge::CanUse(player, target->ToPlayer());
        if (valid)
        {
            std::lock_guard lock(RpsConsumableBridge::Mutex);
            auto now = RpsCombatBridge::Clock::now();
            std::erase_if(RpsConsumableBridge::Uses, [now](auto const& use) { return now - use.Created > std::chrono::seconds(5); });
            valid = RpsConsumableBridge::Uses.size() < 256
                && !std::ranges::any_of(RpsConsumableBridge::Uses, [player](auto const& use) { return use.PlayerGuid == player->GetGUID(); });
            if (valid)
                RpsConsumableBridge::Uses.push_back({player->GetGUID(), item->GetGUID(), target->GetGUID(), player->GetMapId(), player->GetInstanceId(), now});
        }
        E->Push(valid);
        return 1;
    }

    int RpsPollConsumableUses(Eluna* E)
    {
        std::deque<RpsConsumableBridge::Use> uses;
        { std::lock_guard lock(RpsConsumableBridge::Mutex); uses.swap(RpsConsumableBridge::Uses); }
        lua_newtable(E->L);
        int index = 0;
        for (auto const& use : uses)
        {
            Player* player = ObjectAccessor::FindPlayer(use.PlayerGuid);
            Player* target = ObjectAccessor::FindPlayer(use.TargetGuid);
            if (!player || !target || player->GetMapId() != use.MapId || player->GetInstanceId() != use.InstanceId
                || RpsCombatBridge::Clock::now() - use.Created > std::chrono::seconds(5)
                || !RpsConsumableBridge::CanUse(player, target))
                continue;
            lua_createtable(E->L, 0, 3);
            E->Push(player); lua_setfield(E->L, -2, "player");
            E->Push(target); lua_setfield(E->L, -2, "target");
            E->Push(use.ItemGuid.ToString()); lua_setfield(E->L, -2, "itemGuid");
            lua_rawseti(E->L, -2, ++index);
        }
        return 1;
    }

}
