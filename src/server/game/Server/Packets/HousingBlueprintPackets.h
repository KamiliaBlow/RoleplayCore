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

// Housing blueprint packets, 12.1.0.69587.
//
// Client side: CliHousingBlueprintSystem (senders in vtable 0x7FF7D09ED140, one 5-slot message vtable per CMSG) and the
// family-0x54 dispatcher 0x7FF7CD50C8C0. The client never sees share codes on the wire: every message carries the
// blueprint's UUID string ("xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx") and its HousingBlueprintType, and the client turns
// {2, type, 16 uuid bytes} into the 24-character base64 share code itself (0x7FF7CD07CC60) - and back
// (0x7FF7CD07CB20) before it sends one.

#ifndef TRINITYCORE_HOUSING_BLUEPRINT_PACKETS_H
#define TRINITYCORE_HOUSING_BLUEPRINT_PACKETS_H

#include "ObjectGuid.h"
#include "Packet.h"
#include <string>
#include <vector>

namespace WorldPackets::Housing
{
    // ===================== CMSG =====================

    // 0x310000, sender 0x7FF7CD4FA390: SizedCString<6> name, u8 type, guid, name. C_HousingBlueprint.ExportBlueprint sends an
    // empty guid; ExportRoomBlueprint sends type Room and the room.
    class HousingBlueprintExport final : public ClientPacket
    {
    public:
        explicit HousingBlueprintExport(WorldPacket&& packet) : ClientPacket(CMSG_HOUSING_BLUEPRINT_EXPORT, std::move(packet)) { }
        void Read() override;

        std::string Name;
        uint8 BlueprintType = 0;
        ObjectGuid RoomGuid;
    };

    // 0x310001, sender 0x7FF7CD4FA430: empty.
    class HousingBlueprintRequestCollection final : public ClientPacket
    {
    public:
        explicit HousingBlueprintRequestCollection(WorldPacket&& packet) : ClientPacket(CMSG_HOUSING_BLUEPRINT_REQUEST_COLLECTION, std::move(packet)) { }
        void Read() override { }
    };

    // 0x310002, sender 0x7FF7CD4FA4D0: u64 id, SizedCString<6> name.
    class HousingBlueprintRename final : public ClientPacket
    {
    public:
        explicit HousingBlueprintRename(WorldPacket&& packet) : ClientPacket(CMSG_HOUSING_BLUEPRINT_RENAME, std::move(packet)) { }
        void Read() override;

        uint64 BlueprintID = 0;
        std::string Name;
    };

    // 0x310003, sender 0x7FF7CD4FA560: u64 id.
    class HousingBlueprintDelete final : public ClientPacket
    {
    public:
        explicit HousingBlueprintDelete(WorldPacket&& packet) : ClientPacket(CMSG_HOUSING_BLUEPRINT_DELETE, std::move(packet)) { }
        void Read() override;

        uint64 BlueprintID = 0;
    };

    // 0x310005, sender 0x7FF7CD4FA670: SizedCString<24> uuid, bit, u8 type, guid, u32, uuid. ImportBlueprint (house,
    // interior, exterior: 0x7FF7CD07D110) sends bit 1, no guid, 0; a room import (0x7FF7CD07D380) sends type Room, the
    // room whose door was picked and the door's RoomComponent id.
    class HousingBlueprintImport final : public ClientPacket
    {
    public:
        explicit HousingBlueprintImport(WorldPacket&& packet) : ClientPacket(CMSG_HOUSING_BLUEPRINT_IMPORT, std::move(packet)) { }
        void Read() override;

        std::string Uuid;
        bool Flag = false;
        uint8 BlueprintType = 0;
        ObjectGuid SourceRoomGuid;
        uint32 TargetDoorComponentID = 0;
    };

    // 0x310008, sender 0x7FF7CD4FA810: SizedCString<24> uuid, bit (always 1), u8 type, guid, uuid. The guid is the house the
    // contents are evaluated against (RequestBlueprintContentsForContext), empty for none.
    class HousingBlueprintRequestContents final : public ClientPacket
    {
    public:
        explicit HousingBlueprintRequestContents(WorldPacket&& packet) : ClientPacket(CMSG_HOUSING_BLUEPRINT_REQUEST_CONTENTS, std::move(packet)) { }
        void Read() override;

        std::string Uuid;
        bool Flag = false;
        uint8 BlueprintType = 0;
        ObjectGuid TargetHouseGuid;
    };

    // ===================== SMSG =====================

    // 0x540000 (case 0x540000, handler 0x7FF7CD07D880): u8 result, u8 type, SizedCString<24> uuid.
    // result != 0 -> HOUSING_BLUEPRINT_EXPORT_FAILURE(result), else HOUSING_BLUEPRINT_EXPORT_SUCCESS(shareCode).
    class HousingBlueprintExportResponse final : public ServerPacket
    {
    public:
        HousingBlueprintExportResponse() : ServerPacket(SMSG_HOUSING_BLUEPRINT_EXPORT_RESPONSE) { }
        WorldPacket const* Write() override;

        uint8 Result = 0;
        uint8 BlueprintType = 0;
        std::string Uuid;
    };

    // JamHousingBlueprint, element reader 0x7FF7CD50C3C0 (120 bytes).
    struct JamHousingBlueprint
    {
        uint64 ID = 0;
        std::string Uuid;
        std::string Name;
        uint8 Type = 0;
        int64 DateCreated = 0;
        int64 DateDeleted = 0;
        uint8 Flags = 0;            // HousingBlueprintFlag
        uint8 DeleteReason = 0;
    };

    // 0x540001 (handler 0x7FF7CD07E210): u8 result, u32 count, count x JamHousingBlueprint.
    class HousingBlueprintCollection final : public ServerPacket
    {
    public:
        HousingBlueprintCollection() : ServerPacket(SMSG_HOUSING_BLUEPRINT_COLLECTION) { }
        WorldPacket const* Write() override;

        uint8 Result = 0;
        std::vector<JamHousingBlueprint> Blueprints;
    };

    // 0x540002: u8 result, u64 id, SizedCString<6> name -> HOUSING_BLUEPRINT_RENAME_SUCCESS(id, name) / _FAILURE(id, result).
    class HousingBlueprintRenameResponse final : public ServerPacket
    {
    public:
        HousingBlueprintRenameResponse() : ServerPacket(SMSG_HOUSING_BLUEPRINT_RENAME_RESPONSE) { }
        WorldPacket const* Write() override;

        uint8 Result = 0;
        uint64 BlueprintID = 0;
        std::string Name;
    };

    // 0x540003: u8 result, u64 id -> HOUSING_BLUEPRINT_DELETE_SUCCESS(id) / _FAILURE(id, result).
    class HousingBlueprintDeleteResponse final : public ServerPacket
    {
    public:
        HousingBlueprintDeleteResponse() : ServerPacket(SMSG_HOUSING_BLUEPRINT_DELETE_RESPONSE) { }
        WorldPacket const* Write() override;

        uint8 Result = 0;
        uint64 BlueprintID = 0;
    };

    // 0x540004 (handler 0x7FF7CD07DD10): u8 result, u8 type, u32, SizedCString<24> uuid. The client does not use the u32.
    class HousingBlueprintImportResponse final : public ServerPacket
    {
    public:
        HousingBlueprintImportResponse() : ServerPacket(SMSG_HOUSING_BLUEPRINT_IMPORT_RESPONSE) { }
        WorldPacket const* Write() override;

        uint8 Result = 0;
        uint8 BlueprintType = 0;
        uint32 Unused = 0;
        std::string Uuid;
    };

    // Per content type lists of a blueprint (reader 0x7FF7CD4E94E0): decor {HouseDecor id, count}, dyes {DyeColor id, count},
    // one HouseRoom id per room, one ExteriorComponent id per fixture.
    struct JamBlueprintContentLists
    {
        std::vector<std::pair<uint32, uint32>> Decor;
        std::vector<std::pair<uint32, uint32>> Dyes;
        std::vector<uint32> Rooms;
        std::vector<uint32> Fixtures;
    };

    // HousingBlueprintBudgetEntry (reader 0x7FF7CD4E96A0): u8 type, i32 max, i32 current, i32 cost. The client drops max
    // and current when they are not positive (no target house).
    struct JamBlueprintBudgetEntry
    {
        uint8 BudgetType = 0;       // HousingBudgetType
        int32 Max = 0;
        int32 Current = 0;
        int32 Cost = 0;
    };

    // 0x540007 (reader 0x7FF7CD50C6F0, handlers 0x7FF7CD07EA40 and 0x7FF7CF0445E0): u8 result, u8 type, guid target house,
    // u32 unmet requirement flags, missing lists, invalid lists, u32 interior budgets, u32 exterior budgets, the budget
    // entries, bit has contents, [content lists], SizedCString<24> uuid. The client builds the content groups: totals from
    // the contents, numMissing from the missing lists and invalid from the invalid lists.
    class HousingBlueprintContents final : public ServerPacket
    {
    public:
        HousingBlueprintContents() : ServerPacket(SMSG_HOUSING_BLUEPRINT_CONTENTS) { }
        WorldPacket const* Write() override;

        uint8 Result = 0;
        uint8 BlueprintType = 0;
        ObjectGuid TargetHouseGuid;
        uint32 UnmetRequirementFlags = 0;
        JamBlueprintContentLists Missing;
        JamBlueprintContentLists Invalid;
        std::vector<JamBlueprintBudgetEntry> InteriorBudgets;
        std::vector<JamBlueprintBudgetEntry> ExteriorBudgets;
        Optional<JamBlueprintContentLists> Contents;
        std::string Uuid;
    };

    // JamHouseBudgetEntry (spec §2).
    struct JamHouseBudgetEntry
    {
        uint32 BudgetType = 0;
        int32 Max = 0;
        int32 Current = 0;
        int32 Cost = 0;
    };

    // SMSG_HOUSING_HOUSE_BUDGETS_UPDATE 0x620000 - JamHouseBudgets (spec §2).
    class HousingHouseBudgetsUpdate final : public ServerPacket
    {
    public:
        HousingHouseBudgetsUpdate() : ServerPacket(SMSG_HOUSING_HOUSE_BUDGETS_UPDATE) { }
        WorldPacket const* Write() override;

        ObjectGuid HouseGuid;
        std::vector<JamHouseBudgetEntry> InteriorBudgets;
        std::vector<JamHouseBudgetEntry> ExteriorBudgets;
    };
}

#endif // TRINITYCORE_HOUSING_BLUEPRINT_PACKETS_H
