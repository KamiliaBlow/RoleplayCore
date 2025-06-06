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

#ifndef TRINITYCORE_BATTLE_PET_PACKETS_H
#define TRINITYCORE_BATTLE_PET_PACKETS_H

#include "Packet.h"
#include "PacketUtilities.h"
#include "ObjectGuid.h"
#include "Optional.h"
#include "UnitDefines.h"
#include <memory>

static uint8 const PARTICIPANTS_COUNT = 1;

namespace WorldPackets
{
    namespace BattlePet
    {
        struct BattlePetOwnerInfo
        {
            ObjectGuid Guid;
            uint32 PlayerVirtualRealm = 0;
            uint32 PlayerNativeRealm = 0;
        };

        struct BattlePet
        {
            ObjectGuid Guid;
            uint32 Species = 0;
            uint32 CreatureID = 0;
            uint32 DisplayID = 0;
            uint16 Breed = 0;
            uint16 Level = 0;
            uint16 Exp = 0;
            uint16 Flags = 0;
            uint32 Power = 0;
            uint32 Health = 0;
            uint32 MaxHealth = 0;
            uint32 Speed = 0;
            uint8 Quality = 0;
            Optional<BattlePetOwnerInfo> OwnerInfo;
            std::string Name;
            bool NoRename = false;
        };

        struct BattlePetSlot
        {
            BattlePet Pet;
            uint32 CollarID = 0;
            uint8 Index = 0;
            bool Locked = true;
        };

        class BattlePetJournal final : public ServerPacket
        {
        public:
            BattlePetJournal() : ServerPacket(SMSG_BATTLE_PET_JOURNAL) { }

            WorldPacket const* Write() override;

            uint16 Trap = 0;
            bool HasJournalLock = false;
            std::vector<std::reference_wrapper<BattlePetSlot>> Slots;
            std::vector<std::reference_wrapper<BattlePet>> Pets;
        };

        class BattlePetJournalLockAcquired final : public ServerPacket
        {
        public:
            BattlePetJournalLockAcquired() : ServerPacket(SMSG_BATTLE_PET_JOURNAL_LOCK_ACQUIRED, 0) { }

            WorldPacket const* Write() override { return &_worldPacket; }
        };

        class BattlePetJournalLockDenied final : public ServerPacket
        {
        public:
            BattlePetJournalLockDenied() : ServerPacket(SMSG_BATTLE_PET_JOURNAL_LOCK_DENIED, 0) { }

            WorldPacket const* Write() override { return &_worldPacket; }
        };

        class BattlePetRequestJournal final : public ClientPacket
        {
        public:
            explicit BattlePetRequestJournal(WorldPacket&& packet) : ClientPacket(CMSG_BATTLE_PET_REQUEST_JOURNAL, std::move(packet)) { }

            void Read() override { }
        };

        class BattlePetRequestJournalLock final : public ClientPacket
        {
        public:
            explicit BattlePetRequestJournalLock(WorldPacket&& packet) : ClientPacket(CMSG_BATTLE_PET_REQUEST_JOURNAL_LOCK, std::move(packet)) { }

            void Read() override { }
        };

        class BattlePetUpdates final : public ServerPacket
        {
        public:
            BattlePetUpdates() : ServerPacket(SMSG_BATTLE_PET_UPDATES) { }

            WorldPacket const* Write() override;

            std::vector<std::reference_wrapper<BattlePet>> Pets;
            bool PetAdded = false;
        };

        class PetBattleSlotUpdates final : public ServerPacket
        {
        public:
            PetBattleSlotUpdates() : ServerPacket(SMSG_PET_BATTLE_SLOT_UPDATES) { }

            WorldPacket const* Write() override;

            std::vector<BattlePetSlot> Slots;
            bool AutoSlotted = false;
            bool NewSlot = false;
        };

        class BattlePetSetBattleSlot final : public ClientPacket
        {
        public:
            explicit BattlePetSetBattleSlot(WorldPacket&& packet) : ClientPacket(CMSG_BATTLE_PET_SET_BATTLE_SLOT, std::move(packet)) { }

            void Read() override;

            ObjectGuid PetGuid;
            uint8 Slot = 0;
        };

        class BattlePetModifyName final : public ClientPacket
        {
        public:
            explicit BattlePetModifyName(WorldPacket&& packet) : ClientPacket(CMSG_BATTLE_PET_MODIFY_NAME, std::move(packet)) { }

            void Read() override;

            ObjectGuid PetGuid;
            std::string Name;
            std::unique_ptr<DeclinedName> DeclinedNames;
        };

        class QueryBattlePetName final : public ClientPacket
        {
        public:
            explicit QueryBattlePetName(WorldPacket&& packet) : ClientPacket(CMSG_QUERY_BATTLE_PET_NAME, std::move(packet)) { }

            void Read() override;

            ObjectGuid BattlePetID;
            ObjectGuid UnitGUID;
        };

        class QueryBattlePetNameResponse final : public ServerPacket
        {
        public:
            QueryBattlePetNameResponse() : ServerPacket(SMSG_QUERY_BATTLE_PET_NAME_RESPONSE) { }

            WorldPacket const* Write() override;

            ObjectGuid BattlePetID;
            int32 CreatureID = 0;
            WorldPackets::Timestamp<> Timestamp;
            bool Allow = false;

            bool HasDeclined = false;
            DeclinedName DeclinedNames;
            std::string Name;
        };

        class BattlePetDeletePet final : public ClientPacket
        {
        public:
            explicit BattlePetDeletePet(WorldPacket&& packet) : ClientPacket(CMSG_BATTLE_PET_DELETE_PET, std::move(packet)) { }

            void Read() override;

            ObjectGuid PetGuid;
        };

        class BattlePetSetFlags final : public ClientPacket
        {
        public:
            explicit BattlePetSetFlags(WorldPacket&& packet) : ClientPacket(CMSG_BATTLE_PET_SET_FLAGS, std::move(packet)) { }

            void Read() override;

            ObjectGuid PetGuid;
            uint16 Flags = 0;
            uint8 ControlType = 0;
        };

        class BattlePetClearFanfare final : public ClientPacket
        {
        public:
            explicit BattlePetClearFanfare(WorldPacket&& packet) : ClientPacket(CMSG_BATTLE_PET_CLEAR_FANFARE, std::move(packet)) { }

            void Read() override;

            ObjectGuid PetGuid;
        };

        class CageBattlePet final : public ClientPacket
        {
        public:
            explicit CageBattlePet(WorldPacket&& packet) : ClientPacket(CMSG_CAGE_BATTLE_PET, std::move(packet)) { }

            void Read() override;

            ObjectGuid PetGuid;
        };

        class BattlePetDeleted final : public ServerPacket
        {
        public:
            BattlePetDeleted() : ServerPacket(SMSG_BATTLE_PET_DELETED, 16) { }

            WorldPacket const* Write() override;

            ObjectGuid PetGuid;
        };

        class BattlePetError final : public ServerPacket
        {
        public:
            BattlePetError() : ServerPacket(SMSG_BATTLE_PET_ERROR, 5) { }

            WorldPacket const* Write() override;

            uint8 Result = 0;
            int32 CreatureID = 0;
        };

        class BattlePetSummon final : public ClientPacket
        {
        public:
            explicit BattlePetSummon(WorldPacket&& packet) : ClientPacket(CMSG_BATTLE_PET_SUMMON, std::move(packet)) { }

            void Read() override;

            ObjectGuid PetGuid;
        };

        class BattlePetUpdateNotify final : public ClientPacket
        {
        public:
            explicit BattlePetUpdateNotify(WorldPacket&& packet) : ClientPacket(CMSG_BATTLE_PET_UPDATE_NOTIFY, std::move(packet)) { }

            void Read() override;

            ObjectGuid PetGuid;
        };

        class BattlePetCageDateError final : public ServerPacket
        {
        public:
            BattlePetCageDateError(int32 secondsUntilCanCage) : ServerPacket(SMSG_BATTLE_PET_CAGE_DATE_ERROR, 4), SecondsUntilCanCage(secondsUntilCanCage) { }

            WorldPacket const* Write() override;

            int32 SecondsUntilCanCage = 0;
        };

        class BattlePetTrapLevel final : public ServerPacket
        {
        public:
            BattlePetTrapLevel(int16 trapLevel) : ServerPacket(SMSG_BATTLE_PET_TRAP_LEVEL, 2), TrapLevel(trapLevel) { }

            WorldPacket const* Write() override;

            int16 TrapLevel = 0;
        };

        struct PetBattleLocation
        {
            int32 LocationResult = 0;
            TaggedPosition<Position::XYZO> BattleOrigin;
            TaggedPosition<Position::XYZ> PlayerPositions[PARTICIPANTS_COUNT] = {};
        };

        class FinalizeLocation final : public ServerPacket
        {
        public:
            FinalizeLocation() : ServerPacket(SMSG_PET_BATTLE_FINALIZE_LOCATION, 4 + 12 + 4 + 12 * 2) { }

            WorldPacket const* Write() override;

            PetBattleLocation Location;
        };
    }
}

#endif // TRINITYCORE_BATTLE_PET_PACKETS_H
