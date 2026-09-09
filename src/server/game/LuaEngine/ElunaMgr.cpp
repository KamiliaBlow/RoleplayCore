/*
* Copyright (C) 2010 - 2025 Eluna Lua Engine <https://elunaluaengine.github.io/>
* This program is free software licensed under GPL version 3
* Please see the included DOCS/LICENSE.md for more information
*/

#include "ElunaMgr.h"
#include "LuaEngine.h"

ElunaMgr::ElunaMgr()
{
}

ElunaMgr* ElunaMgr::instance()
{
    static ElunaMgr instance;
    return &instance;
}

ElunaMgr::~ElunaMgr()
{
}

void ElunaMgr::Create(Map* map, ElunaInfo const& info)
{
    if (!info.IsValid())
        return;

    // Construct the state before taking the lock: OpenLua/RunScripts can run
    // all lua scripts for the new state and must not stall hook dispatch on
    // every other thread.
    auto eluna = std::make_unique<Eluna>(map);

    std::unique_lock<std::shared_mutex> lock(_elunaMapLock);

    // If already exists, do nothing (the freshly built state is discarded)
    if (_elunaMap.find(info.key) != _elunaMap.end())
        return;

    _elunaMap.emplace(info.key, std::move(eluna));
}

Eluna* ElunaMgr::Get(ElunaInfoKey key) const
{
    std::shared_lock<std::shared_mutex> lock(_elunaMapLock);

    auto it = _elunaMap.find(key);
    if (it != _elunaMap.end())
        return it->second.get();

    return nullptr;
}

Eluna* ElunaMgr::Get(ElunaInfo const& info) const
{
    return Get(info.key);
}

void ElunaMgr::Destroy(ElunaInfoKey key)
{
    std::unique_ptr<Eluna> eluna;
    {
        std::unique_lock<std::shared_mutex> lock(_elunaMapLock);

        auto it = _elunaMap.find(key);
        if (it == _elunaMap.end())
            return;

        eluna = std::move(it->second);
        _elunaMap.erase(it);
    }

    // Destroy the state outside the lock: ~Eluna runs OnLuaStateClose hooks,
    // and lua code can reach sElunaMgr->Get through core dispatch points,
    // which would self-deadlock on a non-recursive shared_mutex.
}

void ElunaMgr::Destroy(ElunaInfo const& info)
{
    Destroy(info.key);
}

ElunaInfo::~ElunaInfo()
{
    if (IsValid() && sElunaMgr)
        sElunaMgr->Destroy(key);
}

bool ElunaInfo::IsValid() const
{
    return key.IsValid();
}

bool ElunaInfo::IsGlobal() const
{
    return key.IsGlobal();
}

uint32 ElunaInfo::GetMapId() const
{
    return key.GetMapId();
}

uint32 ElunaInfo::GetInstanceId() const
{
    return key.GetInstanceId();
}

Eluna* ElunaInfo::GetEluna() const
{
    if (IsValid() && sElunaMgr)
        return sElunaMgr->Get(key);

    return nullptr;
}
