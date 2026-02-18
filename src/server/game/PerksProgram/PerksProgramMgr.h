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

#ifndef TRINITYCORE_PERKS_PROGRAM_MGR_H
#define TRINITYCORE_PERKS_PROGRAM_MGR_H

#include "Define.h"
#include "DatabaseEnvFwd.h"
#include "Duration.h"
#include "PerksProgramPackets.h"
#include <map>
#include <vector>
#include <unordered_map>
#include <unordered_set>

class Player;
struct PerksVendorItemTemplate;
struct PerksVendorCategoryTemplate;

namespace PerksProgram
{
    enum PerksProgramResultCode
    {
        PERKS_PROGRAM_SUCCESS                    = 0,
        PERKS_PROGRAM_ERR_NOT_ENOUGH_CURRENCY    = 1,
        PERKS_PROGRAM_ERR_ITEM_NOT_FOUND         = 2,
        PERKS_PROGRAM_ERR_ITEM_ALREADY_OWNED     = 3,
        PERKS_PROGRAM_ERR_ITEM_EXPIRED           = 4,
        PERKS_PROGRAM_ERR_ITEM_DISABLED          = 5,
        PERKS_PROGRAM_ERR_ITEM_NOT_REFUNDABLE    = 6,
        PERKS_PROGRAM_ERR_REFUND_TIME_EXPIRED    = 7,
        PERKS_PROGRAM_ERR_UNKNOWN                = 8
    };

    struct PlayerPerksData
    {
        int32 CurrencyAmount = 0;
        std::unordered_set<int32> PurchasedItems;
        std::unordered_map<int32, int64> PurchaseTimes;
        int32 FrozenVendorItemID = 0;
    };

    class PerksProgramMgr
    {
    public:
        static PerksProgramMgr* instance();

        void LoadVendorItems();
        void LoadVendorCategories();
        void LoadPlayerData(Player* player);

        void SavePlayerData(Player* player);

        PerksVendorItemTemplate const* GetVendorItemTemplate(int32 vendorItemID) const;
        PerksVendorCategoryTemplate const* GetVendorCategoryTemplate(int32 categoryID) const;

        std::vector<int32> GetAvailableVendorItemIDs() const;
        std::vector<int32> GetAvailableCategoryIDs() const;

        WorldPackets::PerksProgram::PerksVendorItemInfo BuildVendorItemInfo(PerksVendorItemTemplate const* templ, Player const* player) const;
        WorldPackets::PerksProgram::PerksVendorCategory BuildVendorCategoryInfo(PerksVendorCategoryTemplate const* templ) const;

        int32 GetCurrencyAmount(Player const* player) const;
        void ModifyCurrency(Player* player, int32 amount, bool sendUpdate = true);

        bool CanPurchaseItem(Player const* player, int32 vendorItemID) const;
        bool PurchaseItem(Player* player, int32 vendorItemID);
        bool CanRefundItem(Player const* player, int32 vendorItemID) const;
        bool RefundItem(Player* player, int32 vendorItemID);

        void SetFrozenItem(Player* player, int32 vendorItemID);
        int32 GetFrozenItem(Player const* player) const;

        void SendOpenPerksProgram(Player* player);
        void SendClosePerksProgram(Player* player);
        void SendCurrencyUpdate(Player* player, int32 oldValue, int32 newValue);
        void SendPurchaseResult(Player* player, int32 vendorItemID, int32 result, Optional<std::string> errorDesc = {});
        void SendRefundResult(Player* player, int32 vendorItemID, int32 result);
        void SendActivityUpdate(Player* player);

        void AddMonthlyCurrency(Player* player, int32 amount);

    private:
        PerksProgramMgr() = default;
        ~PerksProgramMgr() = default;

        std::unordered_map<int32, PerksVendorItemTemplate> _vendorItemTemplates;
        std::unordered_map<int32, PerksVendorCategoryTemplate> _vendorCategoryTemplates;
    };
}

#define sPerksProgramMgr PerksProgram::PerksProgramMgr::instance()

struct PerksVendorItemTemplate
{
    int32 ID = 0;
    int32 CategoryID = 0;
    std::string Name;
    std::string Description;
    std::string IconTexture;
    int32 Price = 0;
    int32 OriginalPrice = 0;
    uint32 AvailableUntil = 0;
    bool DoesNotExpire = false;
    bool Disabled = false;

    int32 ItemID = 0;
    int32 MountID = 0;
    int32 SpeciesID = 0;
    int32 TransmogSetID = 0;
    int32 ItemModifiedAppearanceID = 0;
    int32 TransmogIllusionID = 0;
    int32 ToyID = 0;
    int32 WarbandSceneID = 0;

    uint8 Quality = 0;
    std::string InvType;
    std::string MountTypeName;

    std::vector<WorldPackets::PerksProgram::PerksVendorSubItem> SubItems;
    Optional<WorldPackets::PerksProgram::PerksVendorItemUIGroup> UIGroupInfo;

    Optional<int32> OverrideModelSceneID;
    Optional<int32> CreatureDisplayInfoID;
    Optional<int32> MainHandItemModifiedAppearanceID;
    Optional<int32> OffHandItemModifiedAppearanceID;
};

struct PerksVendorCategoryTemplate
{
    int32 ID = 0;
    std::string DisplayName;
    int32 DefaultUIModelSceneID = 0;
};

#endif
