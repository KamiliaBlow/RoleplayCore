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

#include "ClubFinderPackets.h"
#include "PacketOperators.h"

#include <algorithm>

namespace WorldPackets::ClubFinder
{
void ClubFinderPost::Read()
{
    _worldPacket.ResetBitPos();

    _worldPacket >> SizedString::BitsSize<7>(Name);
    _worldPacket >> SizedString::BitsSize<12>(Description);
    _worldPacket >> Bits<3>(Type);
    _worldPacket >> Bits<1>(CrossFaction);

    // The first byte-aligned read flushes the remaining bits of the block, matching the
    // client's explicit FlushBits.
    _worldPacket >> ClubId;
    _worldPacket >> RecruitingSpecs;
    _worldPacket >> RecruitmentFlags;
    _worldPacket >> ItemLevelRequirement;
    _worldPacket >> AvatarId;
    _worldPacket >> SizedString::Data(Name);
    _worldPacket >> SizedString::Data(Description);
}

WorldPacket const* ClubFinderResponsePostRecruitmentMessage::Write()
{
    _worldPacket << ClubFinderGUID;
    _worldPacket << Bits<3>(Result);  // 0 = created, 1 = updated
    _worldPacket << Bits<3>(Unused);  // always 1 on retail
    _worldPacket.FlushBits();

    return &_worldPacket;
}

void ClubFinderRequestSubscribedClubPostingIds::Read()
{
    _worldPacket >> Size<uint32>(ClubIds);
    for (uint64& clubId : ClubIds)
        _worldPacket >> clubId;
}

WorldPacket const* ClubFinderGetClubPostingIdsResponse::Write()
{
    // u32 count, then one { club id u64, guild id u64 } record per subscribed club.
    _worldPacket << Size<uint32>(PostingIds);
    for (ClubPostingClubIDMap const& postingId : PostingIds)
    {
        _worldPacket << postingId.ClubID;
        _worldPacket << postingId.GuildID;
    }

    return &_worldPacket;
}

// The Bits<24> byte count is present only for the string-valued forms, and the client emits
// strlen + 1, so the count always equals the number of bytes that follow.
ByteBuffer& operator>>(ByteBuffer& data, ClubFinderPostingFilter& filter)
{
    data >> Bits<3>(filter.Type);
    data.ResetBitPos();

    data >> Bits<3>(filter.ValueType);

    uint32 byteCount = 0;
    if (filter.ValueType == 5 || filter.ValueType == 6)
        byteCount = data.ReadBits(24);

    data.ResetBitPos();

    switch (filter.ValueType)
    {
        case 1:
        case 2:
            data >> filter.UintValue;
            break;
        case 3:
        case 4:
            data >> filter.Uint64Value;
            break;
        case 5:
        case 6:
            if (byteCount)
            {
                filter.StringValue.resize(byteCount);
                data.read(reinterpret_cast<uint8*>(filter.StringValue.data()), byteCount);
                // The client counts the terminator; drop it so the value is a plain string.
                if (!filter.StringValue.empty() && filter.StringValue.back() == '\0')
                    filter.StringValue.pop_back();
            }
            break;
        default:
            break;
    }

    return data;
}

void ClubFinderRequestClubsData::Read()
{
    uint32 filterCount = 0;

    _worldPacket >> Size<uint32>(ClubPostingIDs);
    _worldPacket >> filterCount;
    for (uint32& clubPostingId : ClubPostingIDs)
        _worldPacket >> clubPostingId;

    _worldPacket >> Bits<3>(Type);
    _worldPacket >> Bits<1>(LinkedLookup);
    _worldPacket.ResetBitPos();

    // Cap before resize: an uncapped crafted count would std::bad_alloc the world thread.
    filterCount = std::min<uint32>(filterCount, _worldPacket.size());
    Filters.resize(filterCount);
    for (ClubFinderPostingFilter& filter : Filters)
        _worldPacket >> filter;
}

// The 12.1 wire keeps the 12.0 field order; only the packet envelope moved (see
// ClubFinderLookupClubPostingsList::Write).
ByteBuffer& operator<<(ByteBuffer& data, ClubFinderClubCacheData const& posting)
{
    // One bit block per record: 7 + 12 + 6 = 25 bits, flushed to four whole bytes.
    data << SizedString::BitsSize<7>(posting.ClubName);
    data << SizedString::BitsSize<12>(posting.Comment);
    data << SizedString::BitsSize<6>(posting.GuildLeader);
    data.FlushBits();

    data << posting.ClubFinderGUID;
    data << posting.NumActiveMembers;
    data << posting.RecruitingSpecs;
    data << posting.RecruitmentFlags;
    data << posting.MinIlvl;
    data << posting.TabardInfo;
    data << posting.LastPosterGUID;
    data << posting.ClubID;
    data << posting.LastUpdatedTime;

    data << SizedString::Data(posting.ClubName);
    data << SizedString::Data(posting.Comment);
    data << SizedString::Data(posting.GuildLeader);
    return data;
}

WorldPacket const* ClubFinderReturnRecruitingClubs::Write()
{
    _worldPacket << Size<uint32>(ClubPostingIDs);
    for (uint32 clubPostingId : ClubPostingIDs)
        _worldPacket << clubPostingId;

    _worldPacket << Bits<3>(Type);
    _worldPacket.FlushBits();

    return &_worldPacket;
}

void ClubFinderRequestClubsList::Read()
{
    uint32 const searchStringLength = _worldPacket.ReadBits(9);
    _worldPacket >> Bits<3>(Type);
    _worldPacket >> Bits<1>(CrossFaction);
    _worldPacket.ResetBitPos();

    uint32 filterCount = 0;
    _worldPacket >> filterCount;
    _worldPacket >> ApplicantSettings;

    if (searchStringLength)
    {
        SearchString.resize(searchStringLength);
        _worldPacket.read(reinterpret_cast<uint8*>(SearchString.data()), searchStringLength);
    }

    // Cap before resize: an uncapped crafted count would std::bad_alloc the world thread.
    filterCount = std::min<uint32>(filterCount, _worldPacket.size());
    Filters.resize(filterCount);
    for (ClubFinderPostingFilter& filter : Filters)
        _worldPacket >> filter;
}

WorldPacket const* ClubFinderLookupClubPostingsList::Write()
{
    // u32 count, then all records, then the single envelope byte LAST (type in its top 3
    // bits). Writing the envelope right after the count (the 12.0 shape) eats the first byte
    // of record #1's bit block and the whole packet misparses.
    _worldPacket << Size<uint32>(Postings);
    for (ClubCacheData const& posting : Postings)
        _worldPacket << posting;

    _worldPacket << Bits<3>(Type);
    _worldPacket << Bits<1>(LinkedLookup);
    _worldPacket.FlushBits();

    return &_worldPacket;
}

void ClubFinderRequestMembershipToClub::Read()
{
    _worldPacket >> ClubFinderGUID;
    _worldPacket >> RecruitingSpecs;
    _worldPacket >> SizedString::BitsSize<10>(Comment);
    _worldPacket.ResetBitPos();
    _worldPacket >> SizedString::Data(Comment);
}

void ClubFinderGetApplicantsList::Read()
{
    _worldPacket >> Bits<3>(Type);
    _worldPacket.ResetBitPos();
}

void ClubFinderRequestPendingClubsList::Read()
{
    _worldPacket >> Bits<3>(Type);
    _worldPacket.ResetBitPos();
}

void ClubFinderRespondToApplicant::Read()
{
    _worldPacket >> ClubFinderGUID;
    _worldPacket >> PlayerGUID;
    _worldPacket >> Bits<1>(ShouldAccept);
    _worldPacket >> Bits<3>(Type);
    // ForceAccept is read to keep the bit stream aligned but deliberately not honoured: an
    // accept always routes through the applicant's own consent step.
    _worldPacket >> Bits<1>(ForceAccept);
    _worldPacket.ResetBitPos();
}

void ClubFinderApplicationResponse::Read()
{
    _worldPacket >> ClubFinderGUID;
    _worldPacket >> Bits<3>(UpdateType);
    _worldPacket >> Bits<3>(Type);
    _worldPacket.ResetBitPos();
}

WorldPacket const* ClubFinderApplicationList::Write()
{
    _worldPacket << Size<uint32>(Applications);

    for (PendingApplication const& application : Applications)
    {
        _worldPacket << application.ClubFinderGUID;
        _worldPacket << application.PlayerGUID;
        _worldPacket << application.Closed;
        _worldPacket << application.LastUpdatedTime;
        _worldPacket << uint8(application.ApplicationStatus << 4);
    }

    // Envelope byte LAST; writing it first (the 12.0 shape) misparses every record.
    _worldPacket << uint8(Type << 5);

    return &_worldPacket;
}

WorldPacket const* ClubFinderApplicantsList::Write()
{
    _worldPacket << ClubFinderGUID;
    _worldPacket << Size<uint32>(Applicants);

    for (Applicant const& applicant : Applicants)
    {
        _worldPacket << applicant.ClubFinderGUID;
        _worldPacket << applicant.PlayerGUID;
        _worldPacket << applicant.Closed;
        _worldPacket << uint8(0xFF);
        _worldPacket << uint8(0xFF);
        _worldPacket << applicant.Level;
        _worldPacket << uint32(0xFFFFFFFF); // item level: not provided, the client resolves it itself
        _worldPacket << applicant.RecruitingSpecs;
        _worldPacket << applicant.LastUpdatedTime;
        _worldPacket << uint8(0xFF);

        // Length block: nameLen << 2 | messageLen >> 8, messageLen & 0xFF, then
        // (request status << 4) | (closed << 3). The name is always empty on the wire.
        uint32 const messageLength = std::min<uint32>(applicant.Message.size(), 0x3FF);
        _worldPacket << uint8(messageLength >> 8);
        _worldPacket << uint8(messageLength & 0xFF);
        _worldPacket << uint8((applicant.RequestStatus << 4) | (applicant.Closed << 3));

        // ByteBuffer::append asserts on a zero count, and a comment may legitimately be empty.
        if (messageLength)
            _worldPacket.append(applicant.Message.c_str(), messageLength);
    }

    _worldPacket << uint8(Type << 5);

    return &_worldPacket;
}

void ClubFinderWhisperApplicantRequest::Read()
{
    _worldPacket >> ClubFinderGUID;
    _worldPacket >> PlayerGUID;
}

WorldPacket const* ClubFinderPlayerGuidLookupData::Write()
{
    _worldPacket << Guid;

    return &_worldPacket;
}

WorldPacket const* ClubFinderPlayerGuidLookupResult::Write()
{
    _worldPacket << Guid;
    _worldPacket << uint8(Found ? 0x80 : 0x00);

    return &_worldPacket;
}

WorldPacket const* ClubFinderWhisperApplicantResponse::Write()
{
    _worldPacket << ClubFinderGUID;
    _worldPacket << PlayerGUID;

    return &_worldPacket;
}

WorldPacket const* ClubFinderErrorMessage::Write()
{
    _worldPacket << Bits<3>(Type);
    _worldPacket << Bits<4>(Error);
    _worldPacket.FlushBits();

    return &_worldPacket;
}
}
