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

#ifndef TRINITYCORE_AUTHENTICATION_PACKETS_H
#define TRINITYCORE_AUTHENTICATION_PACKETS_H

#include "Packet.h"
#include "Define.h"
#include "Optional.h"
#include "PacketUtilities.h"
#include <array>

struct CharacterTemplate;
struct RaceClassAvailability;

namespace WorldPackets
{
    namespace Auth
    {
        template <typename Derived>
        class EarlyProcessClientPacket : public ClientPacket
        {
            explicit EarlyProcessClientPacket(OpcodeClient opcode, WorldPacket&& packet) : ClientPacket(opcode, std::move(packet)) { }

        public:
            bool ReadNoThrow() try
            {
                static_cast<Derived*>(this)->Read();
                return true;
            }
            catch (ByteBufferException const& /*ex*/)
            {
                return false;
            }

            friend Derived;
        };

        class Ping final : public EarlyProcessClientPacket<Ping>
        {
        public:
            explicit Ping(WorldPacket&& packet) : EarlyProcessClientPacket(CMSG_PING, std::move(packet)) { }

            uint32 Serial = 0;
            uint32 Latency = 0;

        private:
            friend EarlyProcessClientPacket;
            void Read() override;
        };

        class Pong final : public ServerPacket
        {
        public:
            explicit Pong(uint32 serial) : ServerPacket(SMSG_PONG, 4), Serial(serial) { }

            WorldPacket const* Write() override;

            uint32 Serial = 0;
        };

        class AuthChallenge final : public ServerPacket
        {
        public:
            explicit AuthChallenge() : ServerPacket(SMSG_AUTH_CHALLENGE, 16 + 4 * 8 + 1) { }

            WorldPacket const* Write() override;

            std::array<uint8, 32> Challenge = { };
            std::array<uint32, 8> DosChallenge = { };
            uint8 DosZeroBits = 0;
        };

        class AuthSession final : public EarlyProcessClientPacket<AuthSession>
        {
        public:
            static constexpr uint32 DigestLength = 24;

            explicit AuthSession(WorldPacket&& packet) : EarlyProcessClientPacket(CMSG_AUTH_SESSION, std::move(packet)) { }

            uint32 RegionID = 0;
            uint32 BattlegroupID = 0;
            uint32 RealmID = 0;
            std::array<uint8, 32> LocalChallenge = { };
            std::array<uint8, DigestLength> Digest = { };
            uint64 DosResponse = 0;
            std::string RealmJoinTicket;
            bool UseIPv6 = false;

        private:
            friend EarlyProcessClientPacket;
            void Read() override;
        };

        struct AuthWaitInfo
        {
            uint32 WaitCount = 0; ///< position of the account in the login queue
            uint32 WaitTime = 0; ///< Wait time in login queue in minutes, if sent queued and this value is 0 client displays "unknown time"
            uint8 AllowedFactionGroupForCharacterCreate = 0;
            bool HasFCM = false; ///< true if the account has a forced character migration pending. @todo implement
            bool CanCreateOnlyIfExisting = false; ///< Can create characters on realm only if player has other existing characters there
        };

        struct VirtualRealmNameInfo
        {
            VirtualRealmNameInfo() : IsLocal(false), IsInternalRealm(false) { }
            VirtualRealmNameInfo(bool isHomeRealm, bool isInternalRealm, std::string const& realmNameActual, std::string const& realmNameNormalized) :
                IsLocal(isHomeRealm), IsInternalRealm(isInternalRealm), RealmNameActual(realmNameActual), RealmNameNormalized(realmNameNormalized) { }

            bool IsLocal;                    ///< true if the realm is the same as the account's home realm
            bool IsInternalRealm;            ///< @todo research
            std::string RealmNameActual;     ///< the name of the realm
            std::string RealmNameNormalized; ///< the name of the realm without spaces
        };

        struct VirtualRealmInfo
        {
            VirtualRealmInfo() : RealmAddress(0) { }
            VirtualRealmInfo(uint32 realmAddress, bool isHomeRealm, bool isInternalRealm, std::string const& realmNameActual, std::string const& realmNameNormalized) :
                RealmAddress(realmAddress), RealmNameInfo(isHomeRealm, isInternalRealm, realmNameActual, realmNameNormalized) { }

            uint32 RealmAddress;             ///< the virtual address of this realm, constructed as RealmHandle::Region << 24 | RealmHandle::Battlegroup << 16 | RealmHandle::Index
            VirtualRealmNameInfo RealmNameInfo;
        };

        struct GameTime
        {
            uint32 BillingType = 0;
            uint32 MinutesRemaining = 0;
            uint32 RealBillingType = 0;
            bool IsInIGR = false;
            bool IsPaidForByIGR = false;
            bool IsCAISEnabled = false;
        };

        struct BaseBuildKey
        {
            std::array<uint8, 16> BuildKey = { };
            std::array<uint8, 16> ConfigKey = { };
        };

        struct AuthSuccessInfo
        {
            uint8 ActiveExpansionLevel = 0; ///< the current server expansion, the possible values are in @ref Expansions
            uint8 AccountExpansionLevel = 0; ///< the current expansion of this account, the possible values are in @ref Expansions
            uint32 TimeRested = 0; ///< affects the return value of the GetBillingTimeRested() client API call, it is the number of seconds you have left until the experience points and loot you receive from creatures and quests is reduced. It is only used in the Asia region in retail, it's not implemented in TC and will probably never be.

            uint32 VirtualRealmAddress = 0; ///< a special identifier made from the Index, BattleGroup and Region.
            uint32 TimeSecondsUntilPCKick = 0; ///< @todo research
            uint32 CurrencyID = 0; ///< this is probably used for the ingame shop. @todo implement
            Timestamp<> Time;

            GameTime GameTimeInfo;

            std::vector<VirtualRealmInfo> VirtualRealms;     ///< list of realms connected to this one (inclusive) @todo implement
            std::vector<CharacterTemplate const*> Templates; ///< list of pre-made character templates.

            std::vector<RaceClassAvailability> const* AvailableClasses = nullptr; ///< the minimum AccountExpansion required to select race/class combinations

            bool IsExpansionTrial = false;
            bool ForceCharacterTemplate = false; ///< forces the client to always use a character template when creating a new character. @see Templates. @todo implement
            Optional<uint16> NumPlayersHorde; ///< number of horde players in this realm. @todo implement
            Optional<uint16> NumPlayersAlliance; ///< number of alliance players in this realm. @todo implement
            Optional<Timestamp<>> ExpansionTrialExpiration; ///< expansion trial expiration unix timestamp
            Optional<BaseBuildKey> CurrentBuild;
        };

        class AuthResponse final : public ServerPacket
        {
        public:
            explicit AuthResponse() : ServerPacket(SMSG_AUTH_RESPONSE, 132) { }

            WorldPacket const* Write() override;

            Optional<AuthSuccessInfo> SuccessInfo; ///< contains the packet data in case that it has account information (It is never set when WaitInfo is set), otherwise its contents are undefined.
            Optional<AuthWaitInfo> WaitInfo; ///< contains the queue wait information in case the account is in the login queue.
            uint32 Result = 0; ///< the result of the authentication process, possible values are @ref BattlenetRpcErrorCode
        };

        class WaitQueueUpdate final : public ServerPacket
        {
        public:
            explicit WaitQueueUpdate() : ServerPacket(SMSG_WAIT_QUEUE_UPDATE, 4 + 4 + 1) { }

            WorldPacket const* Write() override;

            AuthWaitInfo WaitInfo;
        };

        class WaitQueueFinish final : public ServerPacket
        {
        public:
            explicit WaitQueueFinish() : ServerPacket(SMSG_WAIT_QUEUE_FINISH, 0) { }

            WorldPacket const* Write() override { return &_worldPacket; }
        };

        enum class ConnectToSerial : uint32
        {
            None            = 0,
            Realm           = 14,
            WorldAttempt1   = 17,
            WorldAttempt2   = 35,
            WorldAttempt3   = 53,
            WorldAttempt4   = 71,
            WorldAttempt5   = 89
        };

        class TC_GAME_API ConnectTo final : public ServerPacket
        {
        public:
            static bool InitializeEncryption();
            static void ShutdownEncryption();

            enum AddressType : uint8
            {
                None = 0,
                IPv4 = 1,
                IPv6 = 2,
                NamedSocket = 3 // not supported by windows client
            };

            struct SocketAddress
            {
                AddressType Type = None;
                union
                {
                    std::array<uint8, 4> V4;
                    std::array<uint8, 16> V6;
                    std::array<char, 128> Name;
                } Address = { };
            };

            struct ConnectPayload
            {
                SocketAddress Where;
                uint16 Port = 0;
                std::array<uint8, 256> Signature = { };
            };

            explicit ConnectTo() : ServerPacket(SMSG_CONNECT_TO, 256 + 1 + 16 + 2 + 4 + 1 + 8) { }

            WorldPacket const* Write() override;

            uint64 Key = 0;
            ConnectToSerial Serial = ConnectToSerial::None;
            ConnectPayload Payload;
            uint8 Con = 0;
        };

        class AuthContinuedSession final : public EarlyProcessClientPacket<AuthContinuedSession>
        {
        public:
            static constexpr uint32 DigestLength = 24;

            explicit AuthContinuedSession(WorldPacket&& packet) : EarlyProcessClientPacket(CMSG_AUTH_CONTINUED_SESSION, std::move(packet)) { }

            uint64 DosResponse = 0;
            uint64 Key = 0;
            std::array<uint8, 32> LocalChallenge = { };
            std::array<uint8, DigestLength> Digest = { };

        private:
            friend EarlyProcessClientPacket;
            void Read() override;
        };

        class ResumeComms final : public ServerPacket
        {
        public:
            explicit ResumeComms(ConnectionType connection) : ServerPacket(SMSG_RESUME_COMMS, 0, connection) { }

            WorldPacket const* Write() override { return &_worldPacket; }
        };

        class ConnectToFailed final : public EarlyProcessClientPacket<ConnectToFailed>
        {
        public:
            explicit ConnectToFailed(WorldPacket&& packet) : EarlyProcessClientPacket(CMSG_CONNECT_TO_FAILED, std::move(packet)) { }

            ConnectToSerial Serial = ConnectToSerial::None;
            uint8 Con = 0;

        private:
            friend EarlyProcessClientPacket;
            void Read() override;
        };

        class TC_GAME_API EnterEncryptedMode final : public ServerPacket
        {
        public:
            static bool InitializeEncryption();
            static void ShutdownEncryption();

            explicit EnterEncryptedMode(std::array<uint8, 32> const& encryptionKey, bool enabled) : ServerPacket(SMSG_ENTER_ENCRYPTED_MODE, 4 + 256 + 1),
                EncryptionKey(encryptionKey), Enabled(enabled)
            {
            }

            WorldPacket const* Write() override;

            int32 RegionGroup = 0;
            std::array<uint8, 32> const& EncryptionKey;
            bool Enabled = false;
        };

        class QueuedMessagesEnd final : public ClientPacket
        {
        public:
            explicit QueuedMessagesEnd(WorldPacket&& packet) : ClientPacket(CMSG_QUEUED_MESSAGES_END, std::move(packet)) { }

            void Read() override;

            uint32 Timestamp = 0;
        };

        ByteBuffer& operator<<(ByteBuffer& data, VirtualRealmInfo const& realmInfo);
        ByteBuffer& operator<<(ByteBuffer& data, VirtualRealmNameInfo const& realmInfo);
    }
}

#endif // TRINITYCORE_AUTHENTICATION_PACKETS_H
