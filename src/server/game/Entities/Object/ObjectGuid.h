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

#ifndef TRINITYCORE_OBJECT_GUID_H
#define TRINITYCORE_OBJECT_GUID_H

#include "Define.h"
#include "EnumFlag.h"
#include "StringFormatFwd.h"
#include "advstd.h"
#include <array>
#include <functional>
#include <list>
#include <set>
#include <span>
#include <string>
#include <type_traits>
#include <unordered_set>
#include <vector>

enum TypeID
{
    TYPEID_OBJECT                 = 0,
    TYPEID_ITEM                   = 1,
    TYPEID_CONTAINER              = 2,
    TYPEID_AZERITE_EMPOWERED_ITEM = 3,
    TYPEID_AZERITE_ITEM           = 4,
    TYPEID_UNIT                   = 5,
    TYPEID_PLAYER                 = 6,
    TYPEID_ACTIVE_PLAYER          = 7,
    TYPEID_GAMEOBJECT             = 8,
    TYPEID_DYNAMICOBJECT          = 9,
    TYPEID_CORPSE                 = 10,
    TYPEID_AREATRIGGER            = 11,
    TYPEID_SCENEOBJECT            = 12,
    TYPEID_CONVERSATION           = 13
};

#define NUM_CLIENT_OBJECT_TYPES             14

enum TypeMask
{
    TYPEMASK_OBJECT                 = 0x0001,
    TYPEMASK_ITEM                   = 0x0002,
    TYPEMASK_CONTAINER              = 0x0004,
    TYPEMASK_AZERITE_EMPOWERED_ITEM = 0x0008,
    TYPEMASK_AZERITE_ITEM           = 0x0010,
    TYPEMASK_UNIT                   = 0x0020,
    TYPEMASK_PLAYER                 = 0x0040,
    TYPEMASK_ACTIVE_PLAYER          = 0x0080,
    TYPEMASK_GAMEOBJECT             = 0x0100,
    TYPEMASK_DYNAMICOBJECT          = 0x0200,
    TYPEMASK_CORPSE                 = 0x0400,
    TYPEMASK_AREATRIGGER            = 0x0800,
    TYPEMASK_SCENEOBJECT            = 0x1000,
    TYPEMASK_CONVERSATION           = 0x2000,

    TYPEMASK_SEER                   = TYPEMASK_UNIT | TYPEMASK_PLAYER | TYPEMASK_DYNAMICOBJECT,
    TYPEMASK_WORLDOBJECT            = TYPEMASK_UNIT | TYPEMASK_GAMEOBJECT | TYPEMASK_DYNAMICOBJECT | TYPEMASK_CORPSE | TYPEMASK_AREATRIGGER | TYPEMASK_SCENEOBJECT | TYPEMASK_CONVERSATION
};

enum class HighGuid
{
    Null             = 0,
    Uniq             = 1,
    Player           = 2,
    Item             = 3,
    WorldTransaction = 4,
    StaticDoor       = 5,   //NYI
    Transport        = 6,
    Conversation     = 7,
    Creature         = 8,
    Vehicle          = 9,
    Pet              = 10,
    GameObject       = 11,
    DynamicObject    = 12,
    AreaTrigger      = 13,
    Corpse           = 14,
    LootObject       = 15,
    SceneObject      = 16,
    Scenario         = 17,
    AIGroup          = 18,
    DynamicDoor      = 19,
    ClientActor      = 20,  //NYI
    Vignette         = 21,
    CallForHelp      = 22,
    AIResource       = 23,
    AILock           = 24,
    AILockTicket     = 25,
    ChatChannel      = 26,
    Party            = 27,
    Guild            = 28,
    WowAccount       = 29,
    BNetAccount      = 30,
    GMTask           = 31,
    MobileSession    = 32,  //NYI
    RaidGroup        = 33,
    Spell            = 34,
    Mail             = 35,
    WebObj           = 36,  //NYI
    LFGObject        = 37,  //NYI
    LFGList          = 38,  //NYI
    UserRouter       = 39,
    PVPQueueGroup    = 40,
    UserClient       = 41,
    PetBattle        = 42,  //NYI
    UniqUserClient   = 43,
    BattlePet        = 44,
    CommerceObj      = 45,
    ClientSession    = 46,
    Cast             = 47,
    ClientConnection = 48,
    ClubFinder       = 49,
    ToolsClient      = 50,
    WorldLayer       = 51,
    ArenaTeam        = 52,
    LMMParty         = 53,
    LMMLobby         = 54,

    Count,
};

enum class ObjectGuidSequenceSource
{
    None    = 0x0,
    Global  = 0x1,
    Realm   = 0x2,
    Map     = 0x4
};

DEFINE_ENUM_FLAG(ObjectGuidSequenceSource);

enum class ObjectGuidFormatType
{
    Null,
    Uniq,
    Player,
    Item,
    WorldObject,
    Transport,
    ClientActor,
    ChatChannel,
    Global,
    Guild,
    MobileSession,
    WebObj,
    LFGObject,
    LFGList,
    Client,
    ClubFinder,
    ToolsClient,
    WorldLayer,
    LMMLobby,
};

template<HighGuid high>
struct ObjectGuidTraits
{
    static constexpr EnumFlag<ObjectGuidSequenceSource> SequenceSource = ObjectGuidSequenceSource::None;
    using Format = std::integral_constant<ObjectGuidFormatType, ObjectGuidFormatType::Null>;
};

#define MAKE_GUID_TRAIT(high, sequence, format) \
    template<> struct ObjectGuidTraits<high> \
    { \
        static constexpr EnumFlag<ObjectGuidSequenceSource> SequenceSource = sequence; \
        using Format = std::integral_constant<ObjectGuidFormatType, format>; \
    }

MAKE_GUID_TRAIT(HighGuid::Null, ObjectGuidSequenceSource::None, ObjectGuidFormatType::Null);
MAKE_GUID_TRAIT(HighGuid::Uniq, ObjectGuidSequenceSource::None, ObjectGuidFormatType::Uniq);
MAKE_GUID_TRAIT(HighGuid::Player, ObjectGuidSequenceSource::Realm, ObjectGuidFormatType::Player);
MAKE_GUID_TRAIT(HighGuid::Item, ObjectGuidSequenceSource::Realm, ObjectGuidFormatType::Item);
MAKE_GUID_TRAIT(HighGuid::WorldTransaction, ObjectGuidSequenceSource::Map, ObjectGuidFormatType::WorldObject);
MAKE_GUID_TRAIT(HighGuid::StaticDoor, ObjectGuidSequenceSource::Global | ObjectGuidSequenceSource::Map, ObjectGuidFormatType::Transport);
MAKE_GUID_TRAIT(HighGuid::Transport, ObjectGuidSequenceSource::Global | ObjectGuidSequenceSource::Map, ObjectGuidFormatType::Transport);
MAKE_GUID_TRAIT(HighGuid::Conversation, ObjectGuidSequenceSource::Map, ObjectGuidFormatType::WorldObject);
MAKE_GUID_TRAIT(HighGuid::Creature, ObjectGuidSequenceSource::Map, ObjectGuidFormatType::WorldObject);
MAKE_GUID_TRAIT(HighGuid::Vehicle, ObjectGuidSequenceSource::Map, ObjectGuidFormatType::WorldObject);
MAKE_GUID_TRAIT(HighGuid::Pet, ObjectGuidSequenceSource::Map, ObjectGuidFormatType::WorldObject);
MAKE_GUID_TRAIT(HighGuid::GameObject, ObjectGuidSequenceSource::Map, ObjectGuidFormatType::WorldObject);
MAKE_GUID_TRAIT(HighGuid::DynamicObject, ObjectGuidSequenceSource::Map, ObjectGuidFormatType::WorldObject);
MAKE_GUID_TRAIT(HighGuid::AreaTrigger, ObjectGuidSequenceSource::Map, ObjectGuidFormatType::WorldObject);
MAKE_GUID_TRAIT(HighGuid::Corpse, ObjectGuidSequenceSource::Map, ObjectGuidFormatType::WorldObject);
MAKE_GUID_TRAIT(HighGuid::LootObject, ObjectGuidSequenceSource::Map, ObjectGuidFormatType::WorldObject);
MAKE_GUID_TRAIT(HighGuid::SceneObject, ObjectGuidSequenceSource::Map, ObjectGuidFormatType::WorldObject);
MAKE_GUID_TRAIT(HighGuid::Scenario, ObjectGuidSequenceSource::Map, ObjectGuidFormatType::WorldObject);
MAKE_GUID_TRAIT(HighGuid::AIGroup, ObjectGuidSequenceSource::Map, ObjectGuidFormatType::WorldObject);
MAKE_GUID_TRAIT(HighGuid::DynamicDoor, ObjectGuidSequenceSource::Map, ObjectGuidFormatType::WorldObject);
MAKE_GUID_TRAIT(HighGuid::ClientActor, ObjectGuidSequenceSource::None, ObjectGuidFormatType::ClientActor);
MAKE_GUID_TRAIT(HighGuid::Vignette, ObjectGuidSequenceSource::Map, ObjectGuidFormatType::WorldObject);
MAKE_GUID_TRAIT(HighGuid::CallForHelp, ObjectGuidSequenceSource::Map, ObjectGuidFormatType::WorldObject);
MAKE_GUID_TRAIT(HighGuid::AIResource, ObjectGuidSequenceSource::Map, ObjectGuidFormatType::WorldObject);
MAKE_GUID_TRAIT(HighGuid::AILock, ObjectGuidSequenceSource::Map, ObjectGuidFormatType::WorldObject);
MAKE_GUID_TRAIT(HighGuid::AILockTicket, ObjectGuidSequenceSource::Map, ObjectGuidFormatType::WorldObject);
MAKE_GUID_TRAIT(HighGuid::ChatChannel, ObjectGuidSequenceSource::Realm, ObjectGuidFormatType::ChatChannel);
MAKE_GUID_TRAIT(HighGuid::Party, ObjectGuidSequenceSource::Global, ObjectGuidFormatType::Global);
MAKE_GUID_TRAIT(HighGuid::Guild, ObjectGuidSequenceSource::Realm, ObjectGuidFormatType::Guild);
MAKE_GUID_TRAIT(HighGuid::WowAccount, ObjectGuidSequenceSource::Global, ObjectGuidFormatType::Global);
MAKE_GUID_TRAIT(HighGuid::BNetAccount, ObjectGuidSequenceSource::Global, ObjectGuidFormatType::Global);
MAKE_GUID_TRAIT(HighGuid::GMTask, ObjectGuidSequenceSource::Global, ObjectGuidFormatType::Global);
MAKE_GUID_TRAIT(HighGuid::MobileSession, ObjectGuidSequenceSource::Realm, ObjectGuidFormatType::MobileSession);
MAKE_GUID_TRAIT(HighGuid::RaidGroup, ObjectGuidSequenceSource::Global, ObjectGuidFormatType::Global);
MAKE_GUID_TRAIT(HighGuid::Spell, ObjectGuidSequenceSource::Global, ObjectGuidFormatType::Global);
MAKE_GUID_TRAIT(HighGuid::Mail, ObjectGuidSequenceSource::Global, ObjectGuidFormatType::Global);
MAKE_GUID_TRAIT(HighGuid::WebObj, ObjectGuidSequenceSource::Realm, ObjectGuidFormatType::WebObj);
MAKE_GUID_TRAIT(HighGuid::LFGObject, ObjectGuidSequenceSource::Global, ObjectGuidFormatType::LFGObject);
MAKE_GUID_TRAIT(HighGuid::LFGList, ObjectGuidSequenceSource::Global, ObjectGuidFormatType::LFGList);
MAKE_GUID_TRAIT(HighGuid::UserRouter, ObjectGuidSequenceSource::Global, ObjectGuidFormatType::Global);
MAKE_GUID_TRAIT(HighGuid::PVPQueueGroup, ObjectGuidSequenceSource::Global, ObjectGuidFormatType::Global);
MAKE_GUID_TRAIT(HighGuid::UserClient, ObjectGuidSequenceSource::Global, ObjectGuidFormatType::Global);
MAKE_GUID_TRAIT(HighGuid::PetBattle, ObjectGuidSequenceSource::Realm, ObjectGuidFormatType::Client);
MAKE_GUID_TRAIT(HighGuid::UniqUserClient, ObjectGuidSequenceSource::Realm, ObjectGuidFormatType::Client);
MAKE_GUID_TRAIT(HighGuid::BattlePet, ObjectGuidSequenceSource::Global, ObjectGuidFormatType::Global);
MAKE_GUID_TRAIT(HighGuid::CommerceObj, ObjectGuidSequenceSource::Global, ObjectGuidFormatType::Global);
MAKE_GUID_TRAIT(HighGuid::ClientSession, ObjectGuidSequenceSource::Realm, ObjectGuidFormatType::Client);
MAKE_GUID_TRAIT(HighGuid::Cast, ObjectGuidSequenceSource::Map, ObjectGuidFormatType::WorldObject);
MAKE_GUID_TRAIT(HighGuid::ClientConnection, ObjectGuidSequenceSource::Realm, ObjectGuidFormatType::Client);
MAKE_GUID_TRAIT(HighGuid::ClubFinder, ObjectGuidSequenceSource::Global, ObjectGuidFormatType::ClubFinder);
MAKE_GUID_TRAIT(HighGuid::ToolsClient, ObjectGuidSequenceSource::Realm, ObjectGuidFormatType::ToolsClient);
MAKE_GUID_TRAIT(HighGuid::WorldLayer, ObjectGuidSequenceSource::Global, ObjectGuidFormatType::WorldLayer);
MAKE_GUID_TRAIT(HighGuid::ArenaTeam, ObjectGuidSequenceSource::Realm, ObjectGuidFormatType::Guild);
MAKE_GUID_TRAIT(HighGuid::LMMParty, ObjectGuidSequenceSource::Realm, ObjectGuidFormatType::Client);
MAKE_GUID_TRAIT(HighGuid::LMMLobby, ObjectGuidSequenceSource::Realm, ObjectGuidFormatType::LMMLobby);

class ByteBuffer;
class ObjectGuid;

class TC_GAME_API ObjectGuidFactory
{
public:
    static constexpr ObjectGuid CreateNull();
    static constexpr ObjectGuid CreateUniq(uint64 id);
    static ObjectGuid CreatePlayer(uint32 realmId, uint64 dbId);
    static ObjectGuid CreateItem(uint32 realmId, uint64 dbId);
    static ObjectGuid CreateWorldObject(HighGuid type, uint8 subType, uint32 realmId, uint16 mapId, uint32 serverId, uint32 entry, uint64 counter);
    static ObjectGuid CreateTransport(HighGuid type, uint32 counter);
    static ObjectGuid CreateClientActor(uint16 ownerType, uint16 ownerId, uint32 counter);
    static ObjectGuid CreateChatChannel(uint32 realmId, bool builtIn, bool trade, uint16 zoneId, uint8 factionGroupMask, uint64 counter);
    static ObjectGuid CreateGlobal(HighGuid type, uint64 dbIdHigh, uint64 dbId);
    static ObjectGuid CreateGuild(HighGuid type, uint32 realmId, uint64 dbId);
    static ObjectGuid CreateMobileSession(uint32 realmId, uint16 arg1, uint64 counter);
    static ObjectGuid CreateWebObj(uint32 realmId, uint8 arg1, uint8 arg2, uint64 counter);
    static ObjectGuid CreateLFGObject(uint8 arg1, uint8 arg2, uint8 arg3, uint8 arg4, bool arg5, uint8 arg6, uint64 counter);
    static ObjectGuid CreateLFGList(uint8 arg1, uint64 counter);
    static ObjectGuid CreateClient(HighGuid type, uint32 realmId, uint32 arg1, uint64 counter);
    static ObjectGuid CreateClubFinder(uint32 realmId, uint8 type, uint32 clubFinderId, uint64 dbId);
    static ObjectGuid CreateToolsClient(uint16 mapId, uint32 serverId, uint64 counter);
    static ObjectGuid CreateWorldLayer(uint32 arg1, uint16 arg2, uint8 arg3, uint32 arg4);
    static ObjectGuid CreateLMMLobby(uint32 realmId, uint32 arg2, uint8 arg3, uint8 arg4, uint64 counter);
};

class TC_GAME_API ObjectGuid
{
    friend class ObjectGuidFactory;
    friend TC_GAME_API ByteBuffer& operator<<(ByteBuffer& buf, ObjectGuid const& guid);
    friend TC_GAME_API ByteBuffer& operator>>(ByteBuffer& buf, ObjectGuid& guid);

    public:
        static ObjectGuid const Empty;
        static ObjectGuid const ToStringFailed;
        static ObjectGuid const FromStringFailed;
        static ObjectGuid const TradeItem;

        static constexpr std::size_t BytesSize = 16;

        using LowType = uint64;

        constexpr ObjectGuid() = default;

        uint64 GetRawValue(std::size_t i) const { return _data[i]; }
        std::array<uint8, 16> GetRawValue() const;
        void SetRawValue(std::span<uint8 const> rawBytes);
        void SetRawValue(uint64 high, uint64 low) { _data[0] = low; _data[1] = high; }
        void Clear() { _data = { }; }

        HighGuid GetHigh() const { return HighGuid((_data[1] >> 58) & 0x3F); }
        uint32 GetRealmId() const { return uint32((_data[1] >> 42) & 0xFFFF); }
        uint32 GetMapId() const { return uint32((_data[1] >> 29) & 0x1FFF); }
        uint32 GetEntry() const { return uint32((_data[1] >> 6) & 0x7FFFFF); }
        uint32 GetSubType() const { return uint32(_data[1] & 0x3F); }
        LowType GetCounter() const
        {
            switch (GetHigh())
            {
                case HighGuid::Transport:
                    return (_data[1] >> 38) & UI64LIT(0xFFFFF);
                default:
                    break;
            }
            return _data[0] & UI64LIT(0x000000FFFFFFFFFF);
        }

        static LowType GetMaxCounter(HighGuid high)
        {
            switch (high)
            {
                case HighGuid::Transport:
                    return UI64LIT(0xFFFFF);
                default:
                    break;
            }
            return UI64LIT(0xFFFFFFFFFF);
        }

        LowType GetMaxCounter() const { return GetMaxCounter(GetHigh()); }

        bool IsEmpty()             const { return _data[0] == 0 && _data[1] == 0; }
        bool IsCreature()          const { return GetHigh() == HighGuid::Creature; }
        bool IsPet()               const { return GetHigh() == HighGuid::Pet; }
        bool IsVehicle()           const { return GetHigh() == HighGuid::Vehicle; }
        bool IsCreatureOrPet()     const { return IsCreature() || IsPet(); }
        bool IsCreatureOrVehicle() const { return IsCreature() || IsVehicle(); }
        bool IsAnyTypeCreature()   const { return IsCreature() || IsPet() || IsVehicle(); }
        bool IsPlayer()            const { return !IsEmpty() && GetHigh() == HighGuid::Player; }
        bool IsUnit()              const { return IsAnyTypeCreature() || IsPlayer(); }
        bool IsItem()              const { return GetHigh() == HighGuid::Item; }
        bool IsGameObject()        const { return GetHigh() == HighGuid::GameObject; }
        bool IsDynamicObject()     const { return GetHigh() == HighGuid::DynamicObject; }
        bool IsCorpse()            const { return GetHigh() == HighGuid::Corpse; }
        bool IsAreaTrigger()       const { return GetHigh() == HighGuid::AreaTrigger; }
        bool IsMOTransport()       const { return GetHigh() == HighGuid::Transport; }
        bool IsAnyTypeGameObject() const { return IsGameObject() || IsMOTransport(); }
        bool IsParty()             const { return GetHigh() == HighGuid::Party; }
        bool IsGuild()             const { return GetHigh() == HighGuid::Guild; }
        bool IsSceneObject()       const { return GetHigh() == HighGuid::SceneObject; }
        bool IsConversation()      const { return GetHigh() == HighGuid::Conversation; }
        bool IsCast()              const { return GetHigh() == HighGuid::Cast; }

// Need for ELUNA
        static TypeID GetTypeId(HighGuid high)
        {
            switch (high)
            {
            case HighGuid::Item:         return TYPEID_ITEM;
                //case HighGuid::Container:    return TYPEID_CONTAINER; HighGuid::Container == HighGuid::Item currently
            case HighGuid::Creature:         return TYPEID_UNIT;
            case HighGuid::Pet:          return TYPEID_UNIT;
            case HighGuid::Player:       return TYPEID_PLAYER;
            case HighGuid::GameObject:   return TYPEID_GAMEOBJECT;
            case HighGuid::DynamicObject: return TYPEID_DYNAMICOBJECT;
            case HighGuid::Corpse:       return TYPEID_CORPSE;
            case HighGuid::Transport: return TYPEID_GAMEOBJECT;
            case HighGuid::Vehicle:      return TYPEID_UNIT;
                // unknown
            case HighGuid::Party:
            default:                    return TYPEID_OBJECT;
            }
        }

        TypeID GetTypeId() const { return GetTypeId(GetHigh()); }
// Need for ELUNA

        bool operator!() const { return IsEmpty(); }
        bool operator==(ObjectGuid const& right) const = default;
        std::strong_ordering operator<=>(ObjectGuid const& right) const
        {
            if (std::strong_ordering cmp = _data[1] <=> right._data[1]; advstd::is_neq(cmp))
                return cmp;
            if (std::strong_ordering cmp = _data[0] <=> right._data[0]; advstd::is_neq(cmp))
                return cmp;
            return std::strong_ordering::equal;
        }

        static std::string_view GetTypeName(HighGuid high);
        std::string_view GetTypeName() const { return !IsEmpty() ? GetTypeName(GetHigh()) : "None"; }
        std::string ToString() const;
        std::string ToHexString() const;
        static ObjectGuid FromString(std::string_view guidString);
        std::size_t GetHash() const;

        template <HighGuid type, std::enable_if_t<ObjectGuidTraits<type>::Format::value == ObjectGuidFormatType::Null, int32> = 0> static constexpr ObjectGuid Create() { return ObjectGuidFactory::CreateNull(); }
        template <HighGuid type, std::enable_if_t<ObjectGuidTraits<type>::Format::value == ObjectGuidFormatType::Uniq, int32> = 0> static constexpr ObjectGuid Create(LowType id) { return ObjectGuidFactory::CreateUniq(id); }
        template <HighGuid type, std::enable_if_t<ObjectGuidTraits<type>::Format::value == ObjectGuidFormatType::Player, int32> = 0> static ObjectGuid Create(LowType dbId) { return ObjectGuidFactory::CreatePlayer(0, dbId); }
        template <HighGuid type, std::enable_if_t<ObjectGuidTraits<type>::Format::value == ObjectGuidFormatType::Item, int32> = 0> static ObjectGuid Create(LowType dbId) { return ObjectGuidFactory::CreateItem(0, dbId); }
        template <HighGuid type, std::enable_if_t<ObjectGuidTraits<type>::Format::value == ObjectGuidFormatType::WorldObject, int32> = 0> static ObjectGuid Create(uint16 mapId, uint32 entry, LowType counter) { return ObjectGuidFactory::CreateWorldObject(type, 0, 0, mapId, 0, entry, counter); }
        template <HighGuid type, std::enable_if_t<ObjectGuidTraits<type>::Format::value == ObjectGuidFormatType::WorldObject, int32> = 0> static ObjectGuid Create(uint8 subType, uint16 mapId, uint32 entry, LowType counter) { return ObjectGuidFactory::CreateWorldObject(type, subType, 0, mapId, 0, entry, counter); }
        template <HighGuid type, std::enable_if_t<ObjectGuidTraits<type>::Format::value == ObjectGuidFormatType::Transport, int32> = 0> static ObjectGuid Create(uint32 counter) { return ObjectGuidFactory::CreateTransport(type, counter); }
        template <HighGuid type, std::enable_if_t<ObjectGuidTraits<type>::Format::value == ObjectGuidFormatType::ClientActor, int32> = 0> static ObjectGuid Create(uint16 ownerType, uint16 ownerId, uint32 counter) { return ObjectGuidFactory::CreateClientActor(ownerType, ownerId, counter); }
        template <HighGuid type, std::enable_if_t<ObjectGuidTraits<type>::Format::value == ObjectGuidFormatType::ChatChannel, int32> = 0> static ObjectGuid Create(bool builtIn, bool trade, uint16 zoneId, uint8 factionGroupMask, LowType counter) { return ObjectGuidFactory::CreateChatChannel(0, builtIn, trade, zoneId, factionGroupMask, counter); }
        template <HighGuid type, std::enable_if_t<ObjectGuidTraits<type>::Format::value == ObjectGuidFormatType::Global, int32> = 0> static ObjectGuid Create(LowType dbId) { return ObjectGuidFactory::CreateGlobal(type, UI64LIT(0), dbId); }
        template <HighGuid type, std::enable_if_t<ObjectGuidTraits<type>::Format::value == ObjectGuidFormatType::Guild, int32> = 0> static ObjectGuid Create(LowType dbId) { return ObjectGuidFactory::CreateGuild(type, 0, dbId); }
        template <HighGuid type, std::enable_if_t<ObjectGuidTraits<type>::Format::value == ObjectGuidFormatType::MobileSession, int32> = 0> static ObjectGuid Create(uint16 arg1, LowType counter) { return ObjectGuidFactory::CreateMobileSession(0, arg1, counter); }
        template <HighGuid type, std::enable_if_t<ObjectGuidTraits<type>::Format::value == ObjectGuidFormatType::WebObj, int32> = 0> static ObjectGuid Create(uint8 arg1, uint8 arg2, LowType counter) { return ObjectGuidFactory::CreateWebObj(0, arg1, arg2, counter); }
        template <HighGuid type, std::enable_if_t<ObjectGuidTraits<type>::Format::value == ObjectGuidFormatType::LFGObject, int32> = 0> static ObjectGuid Create(uint8 arg1, uint8 arg2, uint8 arg3, uint8 arg4, bool arg5, uint8 arg6, LowType counter) { return ObjectGuidFactory::CreateLFGObject(arg1, arg2, arg3, arg4, arg5, arg6, counter); }
        template <HighGuid type, std::enable_if_t<ObjectGuidTraits<type>::Format::value == ObjectGuidFormatType::LFGList, int32> = 0> static ObjectGuid Create(uint8 arg1, LowType counter) { return ObjectGuidFactory::CreateLFGList(arg1, counter); }
        template <HighGuid type, std::enable_if_t<ObjectGuidTraits<type>::Format::value == ObjectGuidFormatType::Client, int32> = 0> static ObjectGuid Create(uint32 arg1, LowType counter) { return ObjectGuidFactory::CreateClient(type, 0, arg1, counter); }
        template <HighGuid type, std::enable_if_t<ObjectGuidTraits<type>::Format::value == ObjectGuidFormatType::ClubFinder, int32> = 0> static ObjectGuid Create(uint8 clubType, uint32 clubFinderId, LowType dbId) { return ObjectGuidFactory::CreateClubFinder(0, clubType, clubFinderId, dbId); }
        template <HighGuid type, std::enable_if_t<ObjectGuidTraits<type>::Format::value == ObjectGuidFormatType::ToolsClient, int32> = 0> static ObjectGuid Create(uint16 mapId, uint32 serverId, LowType counter) { return ObjectGuidFactory::CreateToolsClient(mapId, serverId, counter); }
        template <HighGuid type, std::enable_if_t<ObjectGuidTraits<type>::Format::value == ObjectGuidFormatType::WorldLayer, int32> = 0> static ObjectGuid Create(uint32 arg1, uint16 arg2, uint8 arg3, uint32 arg4) { return ObjectGuidFactory::CreateWorldLayer(arg1, arg2, arg3, arg4); }
        template <HighGuid type, std::enable_if_t<ObjectGuidTraits<type>::Format::value == ObjectGuidFormatType::LMMLobby, int32> = 0> static ObjectGuid Create(uint32 arg2, uint8 arg3, uint8 arg4, LowType counter) { return ObjectGuidFactory::CreateLMMLobby(0, arg2, arg3, arg4, counter); }

    protected:
        constexpr ObjectGuid(uint64 high, uint64 low) : _data({{ low, high }})
        {
        }

        std::array<uint64, 2> _data = { };
};

// Some Shared defines
using GuidSet = std::set<ObjectGuid>;
using GuidList = std::list<ObjectGuid>;
using GuidVector = std::vector<ObjectGuid>;
using GuidUnorderedSet = std::unordered_set<ObjectGuid>;

TC_GAME_API ByteBuffer& operator<<(ByteBuffer& buf, ObjectGuid const& guid);
TC_GAME_API ByteBuffer& operator>>(ByteBuffer& buf, ObjectGuid&       guid);

template<>
struct std::hash<ObjectGuid>
{
    size_t operator()(ObjectGuid const& key) const noexcept
    {
        return key.GetHash();
    }
};

template <>
struct fmt::formatter<ObjectGuid, char, void> : Trinity::NoArgFormatterBase
{
    template <typename FormatContext>
    auto format(ObjectGuid const& guid, FormatContext& ctx) const -> decltype(ctx.out());
};

namespace Trinity
{
    namespace Legacy
    {
        enum class TypeID
        {
            Object          = 0,
            Item            = 1,
            Container       = 2,
            Unit            = 3,
            Player          = 4,
            GameObject      = 5,
            DynamicObject   = 6,
            Corpse          = 7,
            AreaTrigger     = 8,
            SceneObject     = 9,
            Conversation    = 10,
            Max
        };

        constexpr inline ::TypeID ConvertLegacyTypeID(TypeID legacyTypeID)
        {
            switch (legacyTypeID)
            {
                case TypeID::Object:
                    return TYPEID_OBJECT;
                case TypeID::Item:
                    return TYPEID_ITEM;
                case TypeID::Container:
                    return TYPEID_CONTAINER;
                case TypeID::Unit:
                    return TYPEID_UNIT;
                case TypeID::Player:
                    return TYPEID_PLAYER;
                case TypeID::GameObject:
                    return TYPEID_GAMEOBJECT;
                case TypeID::DynamicObject:
                    return TYPEID_DYNAMICOBJECT;
                case TypeID::Corpse:
                    return TYPEID_CORPSE;
                case TypeID::AreaTrigger:
                    return TYPEID_AREATRIGGER;
                case TypeID::SceneObject:
                    return TYPEID_SCENEOBJECT;
                case TypeID::Conversation:
                    return TYPEID_CONVERSATION;
                default:
                    return TYPEID_OBJECT;
            }
        }

        constexpr inline TypeMask ConvertLegacyTypeMask(uint32 legacyTypeMask)
        {
            uint32 typeMask = 0;
            for (TypeID i = TypeID::Object; i < TypeID::Max; i = TypeID(uint32(i) + 1))
                if (legacyTypeMask & (1 << uint32(i)))
                    typeMask |= 1u << ConvertLegacyTypeID(i);

            return TypeMask(typeMask);
        }
    }
}

#endif // TRINITYCORE_OBJECT_GUID_H
