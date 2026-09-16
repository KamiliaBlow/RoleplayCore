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

#ifndef TRINITYCORE_TOKEN_PACKETS_H
#define TRINITYCORE_TOKEN_PACKETS_H

#include "Packet.h"
#include "ObjectGuid.h"
#include "PacketUtilities.h"

namespace WorldPackets
{
    namespace Token
    {
        class CommerceTokenGetLog final : public ClientPacket
        {
        public:
            explicit CommerceTokenGetLog(WorldPacket&& packet) : ClientPacket(CMSG_COMMERCE_TOKEN_GET_LOG, std::move(packet)) { }

            void Read() override;

            uint32 ClientToken   = 0;
        };

        class CommerceTokenGetLogResponse final : public ServerPacket
        {
        public:
            explicit CommerceTokenGetLogResponse() : ServerPacket(SMSG_COMMERCE_TOKEN_GET_LOG_RESPONSE, 12) { }

            WorldPacket const* Write() override;

            struct AuctionableTokenInfo
            {
                uint64 Id           = 0;
                Timestamp<> LastUpdate;
                int32 Status        = 0;
                uint64 Price        = 0;
                uint32 DurationLeft = 0;
            };

            uint32 ClientToken      = 0;
            uint32 Result           = 0;
            std::vector<AuctionableTokenInfo> AuctionableTokens;
        };

        class CommerceTokenGetCount final : public ClientPacket
        {
        public:
            explicit CommerceTokenGetCount(WorldPacket&& packet) : ClientPacket(CMSG_COMMERCE_TOKEN_GET_COUNT, std::move(packet)) {}

            void Read() override;

            uint32 ClientToken = 0;
        };

        // The client reader (12.0.7 68275, sub_7FF7290AC730) takes two parallel uint64 id lists,
        // both counts first and then both payloads. An all-zero 16 byte body - both lists empty -
        // is what retail sends to an account holding no tokens.
        class CommerceTokenGetCountResponse final : public ServerPacket
        {
        public:
            explicit CommerceTokenGetCountResponse() : ServerPacket(SMSG_COMMERCE_TOKEN_GET_COUNT_RESPONSE, 16) {}

            WorldPacket const* Write() override;

            uint32 ClientToken = 0;
            uint32 Result = 0;
            std::vector<uint64> AuctionableTokenIDs;
            std::vector<uint64> ConsumableTokenIDs;
        };

        // Unsolicited push carrying the same two lists, sent when the account's holdings change.
        class CommerceTokenUpdate final : public ServerPacket
        {
        public:
            explicit CommerceTokenUpdate() : ServerPacket(SMSG_COMMERCE_TOKEN_UPDATE, 8) {}

            WorldPacket const* Write() override;

            std::vector<uint64> AuctionableTokenIDs;
            std::vector<uint64> ConsumableTokenIDs;
        };

        class ConsumableTokenCanVeteranBuy final : public ClientPacket
        {
        public:
            explicit ConsumableTokenCanVeteranBuy(WorldPacket&& packet) : ClientPacket(CMSG_CONSUMABLE_TOKEN_CAN_VETERAN_BUY, std::move(packet)) {}

            void Read() override;

            uint32 ClientToken = 0;
        };

        class ConsumableTokenCanVeteranBuyResponse final : public ServerPacket
        {
        public:
            explicit ConsumableTokenCanVeteranBuyResponse() : ServerPacket(SMSG_CONSUMABLE_TOKEN_CAN_VETERAN_BUY_RESPONSE, 16) {}

            WorldPacket const* Write() override;

            uint32 ClientToken = 0;
            uint32 Result = 0;
            uint64 RemainingGoldAmount = 0;
        };

        // Body is a single bit. Retail answers this request in none of the nine 12.0.7 captures, so
        // no response is fabricated here - see WOW_TOKEN_RE_68275.md.
        class CanRedeemTokenForBalance final : public ClientPacket
        {
        public:
            explicit CanRedeemTokenForBalance(WorldPacket&& packet) : ClientPacket(CMSG_CAN_REDEEM_TOKEN_FOR_BALANCE, std::move(packet)) {}

            void Read() override;

            bool Refresh = false;
        };

        // The strict 1:1 answer to CMSG_BATTLE_PAY_OPEN_CHECKOUT: the leading u32 is the request's
        // ClientToken echoed verbatim (proven in all 8 captures). This build has no CMSG_GENERATE_SSO_TOKEN
        // opcode; the token is only ever produced in response to a checkout. See COMMERCE_AUDIT C-09.
        class GenerateSsoTokenResponse final : public ServerPacket
        {
        public:
            explicit GenerateSsoTokenResponse() : ServerPacket(SMSG_GENERATE_SSO_TOKEN_RESPONSE, 26) {}

            WorldPacket const* Write() override;

            uint32 ClientToken = 0;
            uint32 Result = 0;
            Timestamp<> Issued;
            Timestamp<> Expires;
            std::string Token;
        };

        class CommerceTokenGetMarketPrice final : public ClientPacket
        {
        public:
            explicit CommerceTokenGetMarketPrice(WorldPacket&& packet) : ClientPacket(CMSG_COMMERCE_TOKEN_GET_MARKET_PRICE, std::move(packet)) { }

            void Read() override;

            uint32 ClientToken = 0;
        };

        class CommerceTokenGetMarketPriceResponse final : public ServerPacket
        {
        public:
            explicit CommerceTokenGetMarketPriceResponse() : ServerPacket(SMSG_COMMERCE_TOKEN_GET_MARKET_PRICE_RESPONSE, 20) { }

            WorldPacket const* Write() override;

            uint32 ClientToken              = 0;
            int32 Result                    = 0;
            uint64 Price                    = 0;
            uint32 ExpectedSecondsUntilSold = 0;
        };

        // ---- Token TRADE legs (sell / buy / redeem) ------------------------------------------------
        // Wire layouts derived from the 12.0.7 (68275) client (de)serializers, see WOW_TOKEN_RE_68275.md.
        // The trailing single bit and PackedGuid follow the client's own reader for each opcode. Field
        // semantics that the dossier could not name (the Unk* members) are carried but not acted upon.

        // CMSG_AUCTIONABLE_TOKEN_SELL - client serializer 0x7FF72907BCD0: u64 TokenID, u64 Price, u32 ClientToken.
        class AuctionableTokenSell final : public ClientPacket
        {
        public:
            explicit AuctionableTokenSell(WorldPacket&& packet) : ClientPacket(CMSG_AUCTIONABLE_TOKEN_SELL, std::move(packet)) {}

            void Read() override;

            uint64 TokenID = 0;
            uint64 Price = 0;
            uint32 ClientToken = 0;
        };

        // CMSG_AUCTIONABLE_TOKEN_SELL_AT_MARKET_PRICE - client serializer 0x7FF72907BDC0: PackedGuid, u32, u32, u64, bit.
        // The confirm click that follows SMSG_AUCTIONABLE_TOKEN_SELL_CONFIRM_REQUIRED.
        class AuctionableTokenSellAtMarketPrice final : public ClientPacket
        {
        public:
            explicit AuctionableTokenSellAtMarketPrice(WorldPacket&& packet) : ClientPacket(CMSG_AUCTIONABLE_TOKEN_SELL_AT_MARKET_PRICE, std::move(packet)) {}

            void Read() override;

            ObjectGuid Guid;
            uint32 ClientToken = 0;
            uint32 Unk = 0;
            uint64 UnkPrice = 0;
            bool Confirmed = false;
        };

        // Empty body - matches the payload-less Lua event TOKEN_SELL_CONFIRM_REQUIRED.
        class AuctionableTokenSellConfirmRequired final : public ServerPacket
        {
        public:
            explicit AuctionableTokenSellConfirmRequired() : ServerPacket(SMSG_AUCTIONABLE_TOKEN_SELL_CONFIRM_REQUIRED, 0) {}

            WorldPacket const* Write() override { return &_worldPacket; }
        };

        class AuctionableTokenSellAtMarketPriceResponse final : public ServerPacket
        {
        public:
            explicit AuctionableTokenSellAtMarketPriceResponse() : ServerPacket(SMSG_AUCTIONABLE_TOKEN_SELL_AT_MARKET_PRICE_RESPONSE, 8) {}

            WorldPacket const* Write() override;

            uint32 ClientToken = 0;
            uint32 Result = 0;
        };

        // Unsolicited push to the seller when their listed token is bought - payload-less Lua event TOKEN_AUCTION_SOLD.
        class AuctionableTokenAuctionSold final : public ServerPacket
        {
        public:
            explicit AuctionableTokenAuctionSold() : ServerPacket(SMSG_AUCTIONABLE_TOKEN_AUCTION_SOLD, 0) {}

            WorldPacket const* Write() override { return &_worldPacket; }
        };

        // CMSG_CONSUMABLE_TOKEN_BUY - client serializer 0x7FF72907BEF0: u32 ClientToken, PackedGuid, u64.
        class ConsumableTokenBuy final : public ClientPacket
        {
        public:
            explicit ConsumableTokenBuy(WorldPacket&& packet) : ClientPacket(CMSG_CONSUMABLE_TOKEN_BUY, std::move(packet)) {}

            void Read() override;

            uint32 ClientToken = 0;
            ObjectGuid Guid;
            uint64 Unk = 0;
        };

        // CMSG_CONSUMABLE_TOKEN_BUY_AT_MARKET_PRICE - client serializer 0x7FF72907BFC0: u64, u32, u32, bit.
        class ConsumableTokenBuyAtMarketPrice final : public ClientPacket
        {
        public:
            explicit ConsumableTokenBuyAtMarketPrice(WorldPacket&& packet) : ClientPacket(CMSG_CONSUMABLE_TOKEN_BUY_AT_MARKET_PRICE, std::move(packet)) {}

            void Read() override;

            uint64 Unk = 0;
            uint32 ClientToken = 0;
            uint32 Unk2 = 0;
            bool Confirmed = false;
        };

        // Empty body - matches the payload-less Lua event TOKEN_BUY_CONFIRM_REQUIRED.
        class ConsumableTokenBuyChoiceRequired final : public ServerPacket
        {
        public:
            explicit ConsumableTokenBuyChoiceRequired() : ServerPacket(SMSG_CONSUMABLE_TOKEN_BUY_CHOICE_REQUIRED, 0) {}

            WorldPacket const* Write() override { return &_worldPacket; }
        };

        class ConsumableTokenBuyAtMarketPriceResponse final : public ServerPacket
        {
        public:
            explicit ConsumableTokenBuyAtMarketPriceResponse() : ServerPacket(SMSG_CONSUMABLE_TOKEN_BUY_AT_MARKET_PRICE_RESPONSE, 8) {}

            WorldPacket const* Write() override;

            uint32 ClientToken = 0;
            uint32 Result = 0;
        };

        // CMSG_CONSUMABLE_TOKEN_REDEEM - client serializer 0x7FF72907C0E0: u32 RedeemType, u64 TokenID, u32 ClientToken.
        class ConsumableTokenRedeem final : public ClientPacket
        {
        public:
            explicit ConsumableTokenRedeem(WorldPacket&& packet) : ClientPacket(CMSG_CONSUMABLE_TOKEN_REDEEM, std::move(packet)) {}

            void Read() override;

            uint32 RedeemType = 0;
            uint64 TokenID = 0;
            uint32 ClientToken = 0;
        };

        // CMSG_CONSUMABLE_TOKEN_REDEEM_CONFIRMATION - client serializer 0x7FF72907C1C0: u32, u64, PackedGuid, u32, bit.
        class ConsumableTokenRedeemConfirmation final : public ClientPacket
        {
        public:
            explicit ConsumableTokenRedeemConfirmation(WorldPacket&& packet) : ClientPacket(CMSG_CONSUMABLE_TOKEN_REDEEM_CONFIRMATION, std::move(packet)) {}

            void Read() override;

            uint32 RedeemType = 0;
            uint64 TokenID = 0;
            ObjectGuid Guid;
            uint32 ClientToken = 0;
            bool Confirmed = false;
        };

        // SMSG_CONSUMABLE_TOKEN_REDEEM_CONFIRM_REQUIRED - reader 0x7FF7290ACCF0: u32, u32, u32, u64, u64, u32, u8.
        // Fields 1-3 are ClientToken / ChoiceType / Result; the trailing values feed the confirm dialog's
        // displayed amounts (their exact meaning is not named in the dossier - see WOW_TOKEN_RE_68275.md).
        class ConsumableTokenRedeemConfirmRequired final : public ServerPacket
        {
        public:
            explicit ConsumableTokenRedeemConfirmRequired() : ServerPacket(SMSG_CONSUMABLE_TOKEN_REDEEM_CONFIRM_REQUIRED, 29) {}

            WorldPacket const* Write() override;

            uint32 ClientToken = 0;
            uint32 ChoiceType = 0;
            uint32 Result = 0;
            uint64 Amount = 0;
            uint64 Amount2 = 0;
            uint32 Seconds = 0;
            uint8 Flags = 0;
        };

        class ConsumableTokenRedeemResponse final : public ServerPacket
        {
        public:
            explicit ConsumableTokenRedeemResponse() : ServerPacket(SMSG_CONSUMABLE_TOKEN_REDEEM_RESPONSE, 12) {}

            WorldPacket const* Write() override;

            uint32 ClientToken = 0;
            uint32 Result = 0;
            uint32 ChoiceType = 0;
        };
    }
}

#endif // TRINITYCORE_TOKEN_PACKETS_H
