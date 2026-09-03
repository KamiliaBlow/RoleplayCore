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

namespace WorldPackets::ClubFinder
{
void ClubFinderPost::Read()
{
    _worldPacket.ResetBitPos();

    _worldPacket >> SizedString::BitsSize<7>(Name);
    _worldPacket >> SizedString::BitsSize<12>(Description);
    _worldPacket >> Bits<3>(Type);
    _worldPacket >> Bits<1>(CrossFaction);

    // The first byte-aligned read below flushes the remaining bits of the block for us
    // (ByteBuffer::read<T> calls ResetBitPos), matching the client's explicit FlushBits.
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
    _worldPacket << Bits<3>(Result);
    _worldPacket << Bits<3>(Unused);
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
    _worldPacket << Size<uint32>(PostingIds);
    for (ClubPostingClubIDMap const& postingId : PostingIds)
    {
        _worldPacket << postingId.ClubID;
        _worldPacket << postingId.ClubPostingID;
        _worldPacket << postingId.PostingDisplayFlags;
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

    filterCount = std::min<uint32>(filterCount, _worldPacket.size()); // cap before resize (uncapped -> std::bad_alloc -> world-thread crash)
    Filters.resize(filterCount);
    for (ClubFinderPostingFilter& filter : Filters)
        _worldPacket >> filter;
}

// Shared record body, reverse-engineered from the 12.1.0.69404 client's own parser
// (img+0x751950; field count cross-checked against the JAM reflection names: 4 u32 + 3 u64 +
// 2 packed GUIDs + 3 strings, struct slots 0x68 and 0x8c8 being the two 16-byte GUID slots).
// The field ORDER is the 12.0.1 order (finder GUID first, poster GUID before the tail u64s);
// what changed in 12.1 is the PACKET envelope, which moved to the end (see Write()).
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

    filterCount = std::min<uint32>(filterCount, _worldPacket.size()); // cap before resize (uncapped -> std::bad_alloc -> world-thread crash)
    Filters.resize(filterCount);
    for (ClubFinderPostingFilter& filter : Filters)
        _worldPacket >> filter;
}

WorldPacket const* ClubFinderLookupClubPostingsList::Write()
{
    // 12.1 envelope order (client parser at img+0x607411): count u32, then ALL records, then
    // the single envelope byte LAST (type in the top 3 bits, linked in bit 4). 12.0.1 put the
    // envelope right after the count - writing it there eats the first byte of record #1's
    // bit block and the whole packet misparses.
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
    // ForceAccept is read off the wire to keep the bit stream aligned, but it is DELIBERATELY
    // NOT HONOURED per realm policy. The client sets it to skip the applicant's own accept step and
    // force the join through; this realm always routes an accept through the normal consent path
    // (Guild::AddMember with the applicant's status guard), so the parsed value is intentionally
    // ignored by the handler rather than silently dropped as an unnamed bit.
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
    _worldPacket << Bits<3>(Type);
    _worldPacket.FlushBits();

    for (PendingApplication const& application : Applications)
    {
        _worldPacket << application.ClubFinderGUID;
        _worldPacket << application.PlayerGUID;
        _worldPacket << application.Closed;
        _worldPacket << application.LastUpdatedTime;
        _worldPacket << Bits<4>(application.ApplicationStatus);
        _worldPacket.FlushBits();
    }

    return &_worldPacket;
}

void ClubFinderWhisperApplicantRequest::Read()
{
    _worldPacket >> ClubFinderGUID;
    _worldPacket >> PlayerGUID;
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
