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

// The client never sees share codes on the wire: it builds them from {2, type, 16 uuid bytes} itself.

#ifndef TRINITYCORE_HOUSING_BLUEPRINT_PACKETS_H
#define TRINITYCORE_HOUSING_BLUEPRINT_PACKETS_H

#include "ObjectGuid.h"
#include "Packet.h"
#include <string>
#include <vector>

namespace WorldPackets::Housing
{
    // ===================== CMSG =====================

    // ExportBlueprint sends an empty guid; ExportRoomBlueprint sends type Room + the room.
    class HousingBlueprintExport final : public ClientPacket
    {
    public:
        explicit HousingBlueprintExport(WorldPacket&& packet) : ClientPacket(CMSG_HOUSING_BLUEPRINT_EXPORT, std::move(packet)) { }
        void Read() override;

        std::string Name;
        uint8 BlueprintType = 0;
        ObjectGuid RoomGuid;
    };

    class HousingBlueprintRequestCollection final : public ClientPacket
    {
    public:
        explicit HousingBlueprintRequestCollection(WorldPacket&& packet) : ClientPacket(CMSG_HOUSING_BLUEPRINT_REQUEST_COLLECTION, std::move(packet)) { }
        void Read() override { }
    };

    class HousingBlueprintRename final : public ClientPacket
    {
    public:
        explicit HousingBlueprintRename(WorldPacket&& packet) : ClientPacket(CMSG_HOUSING_BLUEPRINT_RENAME, std::move(packet)) { }
        void Read() override;

        uint64 BlueprintID = 0;
        std::string Name;
    };

    class HousingBlueprintDelete final : public ClientPacket
    {
    public:
        explicit HousingBlueprintDelete(WorldPacket&& packet) : ClientPacket(CMSG_HOUSING_BLUEPRINT_DELETE, std::move(packet)) { }
        void Read() override;

        uint64 BlueprintID = 0;
    };

    // House import: Flag=1, no guid; room import: type Room + the picked door's RoomComponent id.
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

    // Guid is the house the contents are evaluated against (RequestBlueprintContentsForContext), empty for none.
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

    class HousingBlueprintCollection final : public ServerPacket
    {
    public:
        HousingBlueprintCollection() : ServerPacket(SMSG_HOUSING_BLUEPRINT_COLLECTION) { }
        WorldPacket const* Write() override;

        uint8 Result = 0;
        std::vector<JamHousingBlueprint> Blueprints;
    };

    // -> HOUSING_BLUEPRINT_RENAME_SUCCESS(id, name) / _FAILURE(id, result).
    class HousingBlueprintRenameResponse final : public ServerPacket
    {
    public:
        HousingBlueprintRenameResponse() : ServerPacket(SMSG_HOUSING_BLUEPRINT_RENAME_RESPONSE) { }
        WorldPacket const* Write() override;

        uint8 Result = 0;
        uint64 BlueprintID = 0;
        std::string Name;
    };

    // -> HOUSING_BLUEPRINT_DELETE_SUCCESS(id) / _FAILURE(id, result).
    class HousingBlueprintDeleteResponse final : public ServerPacket
    {
    public:
        HousingBlueprintDeleteResponse() : ServerPacket(SMSG_HOUSING_BLUEPRINT_DELETE_RESPONSE) { }
        WorldPacket const* Write() override;

        uint8 Result = 0;
        uint64 BlueprintID = 0;
    };

    // The client does not use the u32.
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

    // Decor/Dyes are {id, count} pairs; Rooms/Fixtures one id each.
    struct JamBlueprintContentLists
    {
        std::vector<std::pair<uint32, uint32>> Decor;
        std::vector<std::pair<uint32, uint32>> Dyes;
        std::vector<uint32> Rooms;
        std::vector<uint32> Fixtures;
    };

    // The client drops Max/Current when they are not positive (no target house).
    struct JamBlueprintBudgetEntry
    {
        uint8 BudgetType = 0;       // HousingBudgetType
        int32 Max = 0;
        int32 Current = 0;
        int32 Cost = 0;
    };

    // Client builds totals from Contents, numMissing from Missing and invalid entries from Invalid.
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

    struct JamHouseBudgetEntry
    {
        uint32 BudgetType = 0;
        int32 Max = 0;
        int32 Current = 0;
        int32 Cost = 0;
    };

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
