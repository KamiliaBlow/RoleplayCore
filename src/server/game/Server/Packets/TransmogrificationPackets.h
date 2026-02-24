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

#ifndef TRINITYCORE_TRANSMOGRIFICATION_PACKETS_H
#define TRINITYCORE_TRANSMOGRIFICATION_PACKETS_H

#include "Packet.h"
#include "ObjectGuid.h"
#include "PacketUtilities.h"
#include "EquipmentSet.h"

namespace WorldPackets
{
    namespace Transmogrification
    {
        struct TransmogrifyItem
        {
            int32 ItemModifiedAppearanceID = 0;
            uint32 Slot = 0;
            int32 SpellItemEnchantmentID = 0;
            int32 SecondaryItemModifiedAppearanceID = 0;
        };

        class TransmogrifyItems final : public ClientPacket
        {
        public:
            enum
            {
                MAX_TRANSMOGRIFY_ITEMS = 13
            };

            explicit TransmogrifyItems(WorldPacket&& packet) : ClientPacket(CMSG_TRANSMOGRIFY_ITEMS, std::move(packet)) { }

            void Read() override;

            ObjectGuid Npc;
            Array<TransmogrifyItem, MAX_TRANSMOGRIFY_ITEMS> Items;
            bool CurrentSpecOnly = false;
        };


        struct TransmogOutfitSituationEntry
        {
            uint32 SituationID = 0;
            uint32 SpecID = 0;
            uint32 LoadoutID = 0;
            uint32 EquipmentSetID = 0;
        };

        class TransmogOutfitNew final : public ClientPacket
        {
        public:
            explicit TransmogOutfitNew(WorldPacket&& packet) : ClientPacket(CMSG_TRANSMOG_OUTFIT_NEW, std::move(packet)) { }

            void Read() override;

            EquipmentSetInfo::EquipmentSetData Set;
            bool ParseSuccess = true;
            std::string ParseError;
            std::string DiagnosticReadTrace;
            size_t PayloadSize = 0;
            std::string PayloadPreviewHex;
        };

        class TransmogOutfitUpdateInfo final : public ClientPacket
        {
        public:
            explicit TransmogOutfitUpdateInfo(WorldPacket&& packet) : ClientPacket(CMSG_TRANSMOG_OUTFIT_UPDATE_INFO, std::move(packet)) { }

            void Read() override;

            EquipmentSetInfo::EquipmentSetData Set;
            bool ParseSuccess = true;
            std::string ParseError;
            std::string DiagnosticReadTrace;
            size_t PayloadSize = 0;
            std::string PayloadPreviewHex;
        };

        class TransmogOutfitUpdateSlots final : public ClientPacket
        {
        public:
            explicit TransmogOutfitUpdateSlots(WorldPacket&& packet) : ClientPacket(CMSG_TRANSMOG_OUTFIT_UPDATE_SLOTS, std::move(packet)) { }

            void Read() override;

            EquipmentSetInfo::EquipmentSetData Set;
            bool ParseSuccess = true;
            std::string ParseError;
            std::string DiagnosticReadTrace;
            size_t PayloadSize = 0;
            std::string PayloadPreviewHex;
        };

        class TransmogOutfitUpdateSituations final : public ClientPacket
        {
        public:
            explicit TransmogOutfitUpdateSituations(WorldPacket&& packet) : ClientPacket(CMSG_TRANSMOG_OUTFIT_UPDATE_SITUATIONS, std::move(packet)) { }

            void Read() override;

            uint64 Guid = 0;
            uint32 SetID = 0;
            std::vector<TransmogOutfitSituationEntry> Situations;
            bool ParseSuccess = true;
            std::string ParseError;
            std::string DiagnosticReadTrace;
            size_t PayloadSize = 0;
            std::string PayloadPreviewHex;
        };

        class AccountTransmogUpdate final : public ServerPacket
        {
        public:
            explicit AccountTransmogUpdate() : ServerPacket(SMSG_ACCOUNT_TRANSMOG_UPDATE) { }

            WorldPacket const* Write() override;

            bool IsFullUpdate = false;
            bool IsSetFavorite = false;
            std::vector<uint32> FavoriteAppearances;
            std::vector<uint32> NewAppearances;
        };
    }
}

#endif // TRINITYCORE_TRANSMOGRIFICATION_PACKETS_H
