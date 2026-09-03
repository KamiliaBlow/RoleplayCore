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
 * You should have received a copy of the GNU General Public License along with
 * this program. If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef TRINITYCORE_CLUB_FINDER_PACKETS_H
#define TRINITYCORE_CLUB_FINDER_PACKETS_H

#include "ObjectGuid.h"
#include "Packet.h"
#include "PacketUtilities.h"

namespace WorldPackets
{
    namespace ClubFinder
    {
        // Produced by Lua C_ClubFinder.PostClub(clubId, itemLevelRequirement, name, description,
        // avatarId, specs, type, crossFaction). The client clamps Name to 96 characters and
        // Description to 2048 before sending; both string bodies are written raw at the end with no
        // length prefix and no terminator, their lengths living in the leading bit block.
        class ClubFinderPost final : public ClientPacket
        {
        public:
            explicit ClubFinderPost(WorldPacket&& packet) : ClientPacket(CMSG_CLUB_FINDER_POST, std::move(packet)) { }

            void Read() override;

            uint64 ClubId                = 0;
            uint64 RecruitingSpecs       = 0;
            uint32 RecruitmentFlags      = 0;   // bit-index-per-value ClubFinderSettingFlags mask,
                                                // locale packed as (locale + 1) in bits 21-25
            uint32 ItemLevelRequirement  = 0;
            uint32 AvatarId              = 0;
            uint8 Type                   = 0;   // ClubFinderRequestType, 3 bits on the wire
            bool CrossFaction            = false;
            std::string Name;
            std::string Description;
        };

        // A PackedGuid followed by one bit byte carrying two 3-bit fields. The client's handler
        // branches on the FIRST field only: anything other than 0 or 1 makes it raise
        // ERR_CLUB_FINDER_ERROR_POST_CLUB and drop the message. On 0/1 it refreshes the posting
        // cache and fires CLUB_FINDER_POST_UPDATED, which is what closes the posting dialog.
        // So the field is a post result code, not a request type.
        //
        // The second field is parsed and then never read by the handler - it reaches neither Lua nor
        // manager state. Its value does not affect client behaviour.
        class ClubFinderResponsePostRecruitmentMessage final : public ServerPacket
        {
        public:
            explicit ClubFinderResponsePostRecruitmentMessage() : ServerPacket(SMSG_CLUB_FINDER_RESPONSE_POST_RECRUITMENT_MESSAGE, 18) { }

            WorldPacket const* Write() override;

            ObjectGuid ClubFinderGUID;
            uint8 Result = 0;   // 3 bits; 0 and 1 are success, >= 2 makes the client report a failure
            uint8 Unused = 0;   // 3 bits; parsed by the client and discarded
        };

        // ClubFinderPostingFilter, shared by REQUEST_CLUBS_DATA and REQUEST_CLUBS_LIST. Wire is
        // Bits<3> Type, flush, Bits<3> ValueType, a Bits<24> byte count for the string forms only,
        // flush, then the value itself:
        //   1 = focus flags (Dungeons/Raids/PvP/RP/Social)   2 = guild size (Small/Medium/Large)
        //   3 = player average item level                    4 = player level (capture-verified:
        //                                                      arrives as e.g. 15 alongside ilvl; NOT
        //                                                      a role mask - see ApplySearchFilters)
        //   5 = specialization bitmask (uint64)              6 = locale flags
        struct ClubFinderPostingFilter
        {
            uint8 Type        = 0;   // 3 bits
            uint8 ValueType   = 0;   // 3 bits; 1/2 uint32, 3/4 uint64, 5/6 sized bytes
            uint32 UintValue  = 0;
            uint64 Uint64Value = 0;
            std::string StringValue;
        };

        ByteBuffer& operator>>(ByteBuffer& data, ClubFinderPostingFilter& filter);

        class ClubFinderRequestSubscribedClubPostingIds final : public ClientPacket
        {
        public:
            explicit ClubFinderRequestSubscribedClubPostingIds(WorldPacket&& packet) : ClientPacket(CMSG_CLUB_FINDER_REQUEST_SUBSCRIBED_CLUB_POSTING_IDS, std::move(packet)) { }

            void Read() override;

            std::vector<uint64> ClubIds;
        };

        // A stride-16 vector of { uint64, uint32, uint32 }, which is exactly the client's own
        // ClubFinderClubPostingClubIDMap reflection type.
        class ClubFinderGetClubPostingIdsResponse final : public ServerPacket
        {
        public:
            explicit ClubFinderGetClubPostingIdsResponse() : ServerPacket(SMSG_CLUB_FINDER_GET_CLUB_POSTING_IDS_RESPONSE, 4) { }

            WorldPacket const* Write() override;

            struct ClubPostingClubIDMap
            {
                uint64 ClubID              = 0;
                uint32 ClubPostingID       = 0;
                uint32 PostingDisplayFlags = 0;
            };

            std::vector<ClubPostingClubIDMap> PostingIds;
        };

        // Two counts, then the posting ids, then the bit block, then any filters.
        class ClubFinderRequestClubsData final : public ClientPacket
        {
        public:
            explicit ClubFinderRequestClubsData(WorldPacket&& packet) : ClientPacket(CMSG_CLUB_FINDER_REQUEST_CLUBS_DATA, std::move(packet)) { }

            void Read() override;

            std::vector<uint32> ClubPostingIDs;
            std::vector<ClubFinderPostingFilter> Filters;
            uint8 Type         = 0;   // 3 bits, ClubFinderRequestType
            // Set when the client is resolving a single linked club rather than paging a browse. The
            // response must echo it: the client only hands the record to the invitation frame, via
            // CLUB_FINDER_LINKED_CLUB_RETURNED, when it comes back set.
            bool LinkedLookup  = false;
        };

        // One cached posting record, shared by the browse response and the record lookup response.
        // Wire layout reverse-engineered from the 12.1.0.69404 client's packet parser (found at
        // img+0x751950 in a live-process dump): a 25-bit string-size block - name(7) + comment(12)
        // + leader(6) - then member count, finder GUID, recruitment flags, ilvl, tabard, poster
        // GUID, last-updated time, then the three string bodies. RecruitingSpecs and ClubID are
        // NOT on the 12.1 wire (dropped by the client; the club id lives in the finder GUID's low
        // qword). They stay on the struct for server-side use only.
        struct ClubFinderClubCacheData
        {
            std::string ClubName;
            std::string Comment;
            std::string GuildLeader;
            std::string RealmName;      // not on the 12.1 wire; kept for server-side use
            ObjectGuid ClubFinderGUID;
            ObjectGuid LastPosterGUID;
            uint64 RecruitingSpecs  = 0; // not on the 12.1 wire; kept for search filtering
            uint64 ClubID           = 0; // not on the 12.1 wire; lives in ClubFinderGUID's low qword
            int64 LastUpdatedTime   = 0;
            uint32 NumActiveMembers = 0;
            uint32 TabardInfo       = 0;
            int32 RecruitmentFlags  = 0;
            int32 MinIlvl           = 0;
            bool CrossFaction       = false; // not on the 12.1 wire; kept for search filtering
        };

        ByteBuffer& operator<<(ByteBuffer& data, ClubFinderClubCacheData const& posting);

        // The answer to a search: the matching posting ids (client-proven layout - the client's
        // handler reads count then u32 ids and then requests the full records itself).
        class ClubFinderReturnRecruitingClubs final : public ServerPacket
        {
        public:
            explicit ClubFinderReturnRecruitingClubs() : ServerPacket(SMSG_RETURN_RECRUITING_CLUBS, 5) { }

            WorldPacket const* Write() override;

            std::vector<uint32> ClubPostingIDs;
            uint8 Type = 0;   // 3 bits, ClubFinderRequestType
        };

        // Produced by Lua C_ClubFinder.RequestClubsList(guildListRequested, searchString, specIDs).
        // The search string is capped at 400 characters by the client's own buffer and carries no
        // terminator on the wire.
        class ClubFinderRequestClubsList final : public ClientPacket
        {
        public:
            explicit ClubFinderRequestClubsList(WorldPacket&& packet) : ClientPacket(CMSG_CLUB_FINDER_REQUEST_CLUBS_LIST, std::move(packet)) { }

            void Read() override;

            std::string SearchString;
            std::vector<ClubFinderPostingFilter> Filters;
            uint32 ApplicantSettings = 0;   // ClubFinderSettingFlags bit-index mask
            uint8 Type               = 0;   // 3 bits; 1 = Guild, 2 = Community
            bool CrossFaction        = false;
        };

        // The browse response. Envelope is uint32 Count plus one bit byte (Bits<3> request type,
        // Bits<1> flag), then Count records of { bit block (7+12+6 string sizes), GUIDs and ints,
        // string bodies }.
        class ClubFinderLookupClubPostingsList final : public ServerPacket
        {
        public:
            explicit ClubFinderLookupClubPostingsList() : ServerPacket(SMSG_CLUB_FINDER_LOOKUP_CLUB_POSTINGS_LIST, 5) { }

            WorldPacket const* Write() override;

            using ClubCacheData = ClubFinderClubCacheData;

            std::vector<ClubCacheData> Postings;
            // The type MUST echo the request: the handler only fires the pending page callback (and so
            // CLUB_FINDER_CLUB_LIST_RETURNED) for callbacks whose type matches this field.
            uint8 Type        = 0;
            bool LinkedLookup = false;   // echo of the request's flag; false for browse and paging
        };

        // Lua RequestMembershipToClub(clubFinderGUID, comment, specIDs). Comment buffer is char[513]
        // client-side, so it is clamped to 512 here even though Bits<10> would permit 1023.
        class ClubFinderRequestMembershipToClub final : public ClientPacket
        {
        public:
            explicit ClubFinderRequestMembershipToClub(WorldPacket&& packet) : ClientPacket(CMSG_CLUB_FINDER_REQUEST_MEMBERSHIP_TO_CLUB, std::move(packet)) { }

            void Read() override;

            ObjectGuid ClubFinderGUID;
            uint64 RecruitingSpecs = 0;
            std::string Comment;
        };

        // Body is a single byte. Notably it carries no club GUID, so the club has to be inferred from
        // the sender's own guild.
        class ClubFinderGetApplicantsList final : public ClientPacket
        {
        public:
            explicit ClubFinderGetApplicantsList(WorldPacket&& packet) : ClientPacket(CMSG_CLUB_FINDER_GET_APPLICANTS_LIST, std::move(packet)) { }

            void Read() override;

            uint8 Type = 0;
        };

        class ClubFinderRequestPendingClubsList final : public ClientPacket
        {
        public:
            explicit ClubFinderRequestPendingClubsList(WorldPacket&& packet) : ClientPacket(CMSG_CLUB_FINDER_REQUEST_PENDING_CLUBS_LIST, std::move(packet)) { }

            void Read() override;

            uint8 Type = 0;
        };

        // The Lua binding also takes playerName and reported, but neither reaches the wire.
        class ClubFinderRespondToApplicant final : public ClientPacket
        {
        public:
            explicit ClubFinderRespondToApplicant(WorldPacket&& packet) : ClientPacket(CMSG_CLUB_FINDER_RESPOND_TO_APPLICANT, std::move(packet)) { }

            void Read() override;

            ObjectGuid ClubFinderGUID;
            ObjectGuid PlayerGUID;
            uint8 Type        = 0;
            bool ShouldAccept = false;
            bool ForceAccept  = false;
        };

        class ClubFinderApplicationResponse final : public ClientPacket
        {
        public:
            explicit ClubFinderApplicationResponse(WorldPacket&& packet) : ClientPacket(CMSG_CLUB_FINDER_APPLICATION_RESPONSE, std::move(packet)) { }

            void Read() override;

            ObjectGuid ClubFinderGUID;
            uint8 UpdateType = 0;   // ClubFinderApplicationUpdateType
            uint8 Type       = 0;   // ClubFinderRequestType
        };

        // Record type ClubFinderPendingApplicationData / ClubFinderUpdateApplicationData - both client
        // readers are instruction-for-instruction identical, so the two opcodes share this body.
        class ClubFinderApplicationList final : public ServerPacket
        {
        public:
            explicit ClubFinderApplicationList(OpcodeServer opcode) : ServerPacket(opcode, 5) { }

            WorldPacket const* Write() override;

            struct PendingApplication
            {
                ObjectGuid ClubFinderGUID;
                ObjectGuid PlayerGUID;
                int64 LastUpdatedTime = 0;
                uint32 Closed         = 0;
                uint8 ApplicationStatus = 0;   // 4 bits, PlayerClubRequestStatus
            };

            std::vector<PendingApplication> Applications;
            uint8 Type = 0;   // 3 bits, ClubFinderRequestType
        };

        // An officer asks permission to whisper an applicant. Both directions carry the same pair of
        // PackedGuids; the client opens a whisper to the applicant when the response arrives.
        class ClubFinderWhisperApplicantRequest final : public ClientPacket
        {
        public:
            explicit ClubFinderWhisperApplicantRequest(WorldPacket&& packet) : ClientPacket(CMSG_CLUB_FINDER_WHISPER_APPLICANT_REQUEST, std::move(packet)) { }

            void Read() override;

            ObjectGuid ClubFinderGUID;
            ObjectGuid PlayerGUID;
        };

        class ClubFinderWhisperApplicantResponse final : public ServerPacket
        {
        public:
            explicit ClubFinderWhisperApplicantResponse() : ServerPacket(SMSG_CLUB_FINDER_WHISPER_APPLICANT_RESPONSE, 34) { }

            WorldPacket const* Write() override;

            ObjectGuid ClubFinderGUID;
            ObjectGuid PlayerGUID;
        };

        // A 3-bit request type and a 4-bit error field. The client's handler switches on the 4-bit
        // field, mapping each value 1:1 onto an ERR_CLUB_FINDER_* global string, and uses the 3-bit
        // field as the ClubFinderRequestType of the list re-request it issues for the recoverable
        // cases.
        class ClubFinderErrorMessage final : public ServerPacket
        {
        public:
            explicit ClubFinderErrorMessage() : ServerPacket(SMSG_CLUB_FINDER_ERROR_MESSAGE, 1) { }

            WorldPacket const* Write() override;

            uint8 Type  = 0;   // 3 bits, ClubFinderRequestType
            uint8 Error = 0;   // 4 bits, ClubFinderErrorType
        };
    }
}

#endif // TRINITYCORE_CLUB_FINDER_PACKETS_H
