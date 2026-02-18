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

#ifndef TRINITYCORE_PERKS_PROGRAM_PACKETS_H
#define TRINITYCORE_PERKS_PROGRAM_PACKETS_H

#include "Packet.h"
#include "PerksProgramPacketsCommon.h"
#include "ItemPacketsCommon.h"
#include "Optional.h"
#include <vector>

namespace WorldPackets
{
    namespace PerksProgram
    {
        struct PerksVendorCategory
        {
            int32 ID = 0;
            std::string DisplayName;
            int32 DefaultUIModelSceneID = 0;
        };

        struct PerksVendorSubItem
        {
            std::string Name;
            int32 ItemID = 0;
            int32 ItemModifiedAppearanceID = 0;
            std::string InvType;
            uint8 Quality = 0;
        };

        struct PerksVendorItemUIGroup
        {
            int32 ID = 0;
            std::string Name;
            int32 Priority = 0;
        };

        struct PerksVendorItemInfo
        {
            int32 PerksVendorItemID = 0;
            int32 PerksVendorCategoryID = 0;
            std::string Name;
            std::string Description;
            int32 Price = 0;
            Optional<int32> OriginalPrice;
            bool ShowSaleBanner = false;
            Timestamp<> TimeRemaining;
            bool DoesNotExpire = false;
            bool Purchased = false;
            bool Refundable = false;
            bool IsPurchasePending = false;
            bool SubItemsLoaded = false;
            int32 ItemID = 0;
            std::string IconTexture;
            int32 MountID = 0;
            std::string MountTypeName;
            int32 SpeciesID = 0;
            int32 TransmogSetID = 0;
            int32 ItemModifiedAppearanceID = 0;
            std::string InvType;
            uint8 Quality = 0;
            std::vector<PerksVendorSubItem> SubItems;
            Optional<PerksVendorItemUIGroup> UIGroupInfo;
        };

        struct PerksProgramItemDisplayInfo
        {
            Optional<int32> OverrideModelSceneID;
            Optional<int32> CreatureDisplayInfoID;
            Optional<int32> MainHandItemModifiedAppearanceID;
            Optional<int32> OffHandItemModifiedAppearanceID;
        };

        struct PerksProgramPendingChestReward
        {
            int32 RewardTypeID = 0;
            Optional<int32> PerksVendorItemID;
            int32 RewardAmount = 0;
            Optional<std::string> MonthRewarded;
            int32 ActivityMonthID = 0;
            int32 ThresholdOrderIndex = 0;
        };

        struct PerksProgramActivityThreshold
        {
            int32 ThresholdOrderIndex = 0;
            int32 CurrencyAmount = 0;
            Timestamp<> WhenComplete;
            bool IsComplete = false;
        };

        struct PerksProgramActivityInfo
        {
            int32 ActivityMonthID = 0;
            Timestamp<> MonthStartTime;
            Timestamp<> MonthEndTime;
            int32 CurrencyEarned = 0;
            int32 MaxCurrency = 0;
            std::vector<PerksProgramActivityThreshold> Thresholds;
        };

        ByteBuffer& operator<<(ByteBuffer& data, PerksVendorCategory const& category);
        ByteBuffer& operator<<(ByteBuffer& data, PerksVendorSubItem const& subItem);
        ByteBuffer& operator<<(ByteBuffer& data, PerksVendorItemUIGroup const& uiGroup);
        ByteBuffer& operator<<(ByteBuffer& data, PerksVendorItemInfo const& itemInfo);
        ByteBuffer& operator<<(ByteBuffer& data, PerksProgramItemDisplayInfo const& displayInfo);
        ByteBuffer& operator<<(ByteBuffer& data, PerksProgramPendingChestReward const& reward);
        ByteBuffer& operator<<(ByteBuffer& data, PerksProgramActivityThreshold const& threshold);
        ByteBuffer& operator<<(ByteBuffer& data, PerksProgramActivityInfo const& activityInfo);

        class PerksProgramOpen final : public ServerPacket
        {
        public:
            PerksProgramOpen() : ServerPacket(SMSG_PERKS_PROGRAM_VENDOR_UPDATE, 50) { }

            WorldPacket const* Write() override;

            int32 CurrencyAmount = 0;
            std::vector<int32> VendorItemIDs;
            std::vector<PerksVendorCategory> Categories;
            std::vector<PerksVendorItemInfo> Items;
        };

        class PerksProgramClose final : public ServerPacket
        {
        public:
            PerksProgramClose() : ServerPacket(SMSG_PERKS_PROGRAM_DISABLED, 0) { }

            WorldPacket const* Write() override;
        };

        class PerksProgramStatusRequest final : public ClientPacket
        {
        public:
            explicit PerksProgramStatusRequest(WorldPacket&& packet) : ClientPacket(CMSG_PERKS_PROGRAM_STATUS_REQUEST, std::move(packet)) { }

            void Read() override;
        };

        class PerksProgramItemsRefreshed final : public ClientPacket
        {
        public:
            explicit PerksProgramItemsRefreshed(WorldPacket&& packet) : ClientPacket(CMSG_PERKS_PROGRAM_ITEMS_REFRESHED, std::move(packet)) { }

            void Read() override;
        };

        class PerksProgramRequestPurchase final : public ClientPacket
        {
        public:
            explicit PerksProgramRequestPurchase(WorldPacket&& packet) : ClientPacket(CMSG_PERKS_PROGRAM_REQUEST_PURCHASE, std::move(packet)) { }

            void Read() override;

            int32 VendorItemID = 0;
        };

        class PerksProgramRequestCartCheckout final : public ClientPacket
        {
        public:
            explicit PerksProgramRequestCartCheckout(WorldPacket&& packet) : ClientPacket(CMSG_PERKS_PROGRAM_REQUEST_CART_CHECKOUT, std::move(packet)) { }

            void Read() override;

            std::vector<int32> VendorItemIDs;
        };

        class PerksProgramRequestRefund final : public ClientPacket
        {
        public:
            explicit PerksProgramRequestRefund(WorldPacket&& packet) : ClientPacket(CMSG_PERKS_PROGRAM_REQUEST_REFUND, std::move(packet)) { }

            void Read() override;

            int32 VendorItemID = 0;
        };

        class PerksProgramSetFrozenVendorItem final : public ClientPacket
        {
        public:
            explicit PerksProgramSetFrozenVendorItem(WorldPacket&& packet) : ClientPacket(CMSG_PERKS_PROGRAM_SET_FROZEN_VENDOR_ITEM, std::move(packet)) { }

            void Read() override;

            int32 VendorItemID = 0;
        };

        class PerksProgramRequestPendingRewards final : public ClientPacket
        {
        public:
            explicit PerksProgramRequestPendingRewards(WorldPacket&& packet) : ClientPacket(CMSG_PERKS_PROGRAM_REQUEST_PENDING_REWARDS, std::move(packet)) { }

            void Read() override;
        };

        class PerksProgramGetRecentPurchases final : public ClientPacket
        {
        public:
            explicit PerksProgramGetRecentPurchases(WorldPacket&& packet) : ClientPacket(CMSG_PERKS_PROGRAM_GET_RECENT_PURCHASES, std::move(packet)) { }

            void Read() override;
        };

        class PerksProgramResult final : public ServerPacket
        {
        public:
            PerksProgramResult() : ServerPacket(SMSG_PERKS_PROGRAM_RESULT, 8) { }

            WorldPacket const* Write() override;

            int32 VendorItemID = 0;
            int32 Result = 0;
            Optional<std::string> ErrorDescription;
        };

        class PerksProgramPurchaseSuccess final : public ServerPacket
        {
        public:
            PerksProgramPurchaseSuccess() : ServerPacket(SMSG_PERKS_PROGRAM_VENDOR_UPDATE, 20) { }

            WorldPacket const* Write() override;

            int32 VendorItemID = 0;
            int32 NewCurrencyAmount = 0;
            PerksVendorItemInfo ItemInfo;
        };

        class PerksProgramRefundSuccess final : public ServerPacket
        {
        public:
            PerksProgramRefundSuccess() : ServerPacket(SMSG_PERKS_PROGRAM_VENDOR_UPDATE, 20) { }

            WorldPacket const* Write() override;

            int32 VendorItemID = 0;
            int32 NewCurrencyAmount = 0;
            PerksVendorItemInfo ItemInfo;
        };

        class PerksProgramCurrencyRefresh final : public ServerPacket
        {
        public:
            PerksProgramCurrencyRefresh() : ServerPacket(SMSG_PERKS_PROGRAM_VENDOR_UPDATE, 8) { }

            WorldPacket const* Write() override;

            int32 OldValue = 0;
            int32 NewValue = 0;
        };

        class PerksProgramActivityUpdate final : public ServerPacket
        {
        public:
            PerksProgramActivityUpdate() : ServerPacket(SMSG_PERKS_PROGRAM_ACTIVITY_UPDATE, 50) { }

            WorldPacket const* Write() override;

            PerksProgramActivityInfo ActivityInfo;
        };

        class PerksProgramActivityComplete final : public ServerPacket
        {
        public:
            PerksProgramActivityComplete() : ServerPacket(SMSG_PERKS_PROGRAM_ACTIVITY_COMPLETE, 8) { }

            WorldPacket const* Write() override;

            int32 ActivityMonthID = 0;
            int32 ThresholdOrderIndex = 0;
        };

        class PerksProgramPendingRewards final : public ServerPacket
        {
        public:
            PerksProgramPendingRewards() : ServerPacket(SMSG_PERKS_PROGRAM_VENDOR_UPDATE, 20) { }

            WorldPacket const* Write() override;

            std::vector<PerksProgramPendingChestReward> Rewards;
        };
    }
}

#endif
