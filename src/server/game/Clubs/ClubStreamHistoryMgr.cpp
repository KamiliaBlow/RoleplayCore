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

#include "ClubStreamHistoryMgr.h"
#include "Common.h"
#include "DatabaseEnv.h"
#include "GameTime.h"
#include "Log.h"
#include "Timer.h"
#include "World.h"
#include <algorithm>
#include <cctype>
#include <chrono>

namespace
{
// Packs a (clubId, streamId) pair into the single key used by the live subscription/focus sets. Stream
// ids are the small synthetic values ClubService advertises (1 = Guild, 2 = Officer), so the low 16 bits
// are ample; the club id keeps the rest.
constexpr uint64 PackStreamKey(uint64 clubId, uint64 streamId)
{
    return (clubId << 16) | (streamId & 0xFFFF);
}

// MessageId.epoch is expressed in microseconds since the unix epoch. ClubService has always minted it
// that way and the client renders those timestamps correctly today, so every time value this store hands
// back to the protocol - message epochs and both ViewMarker fields - uses the same unit.
uint64 NowMicroseconds()
{
    return uint64(std::chrono::duration_cast<std::chrono::microseconds>(GameTime::GetSystemTime().time_since_epoch()).count());
}
}

ClubStreamHistoryMgr* ClubStreamHistoryMgr::instance()
{
    static ClubStreamHistoryMgr instance;
    return &instance;
}

bool ClubStreamHistoryMgr::IsEnabled() const
{
    return GetMaxMessagesPerStream() != 0;
}

uint32 ClubStreamHistoryMgr::GetMaxMessagesPerStream() const
{
    return sWorld->getIntConfig(CONFIG_CLUB_STREAM_HISTORY_MAX_MESSAGES);
}

void ClubStreamHistoryMgr::Load()
{
    uint32 oldMSTime = getMSTime();

    _streams.clear();

    if (!IsEnabled())
    {
        TC_LOG_INFO("server.loading", ">> Club stream history is disabled (Club.StreamHistory.MaxMessages = 0).");
        // The read state is still loaded: disabling scrollback must not silently discard unread markers.
        LoadViewMarkers();
        LoadMentions();
        return;
    }

    // Age based retention. Applied before the load so aged out rows never enter the cache.
    if (uint32 maxDays = sWorld->getIntConfig(CONFIG_CLUB_STREAM_HISTORY_MAX_DAYS))
    {
        time_t oldest = GameTime::GetGameTime() - time_t(maxDays) * DAY;
        CharacterDatabase.DirectPExecute("DELETE FROM club_message WHERE createdTime < {}", uint64(std::max<time_t>(oldest, 0)));
        CharacterDatabase.DirectPExecute("DELETE FROM club_member_mention WHERE createdTime < {}", uint64(std::max<time_t>(oldest, 0)));
    }

    //                                                     0       1         2      3         4                5           6        7
    QueryResult result = CharacterDatabase.Query("SELECT clubId, streamId, epoch, position, authorAccountId, authorGuid, content, createdTime "
        "FROM club_message ORDER BY clubId, streamId, epoch, position");

    uint32 count = 0;
    if (result)
    {
        do
        {
            Field* fields = result->Fetch();

            ClubStreamMessage message;
            message.ClubId          = fields[0].GetUInt64();
            message.StreamId        = fields[1].GetUInt64();
            message.Epoch           = fields[2].GetUInt64();
            message.Position        = fields[3].GetUInt64();
            message.AuthorAccountId = fields[4].GetUInt32();
            message.AuthorGuid      = ObjectGuid::Create<HighGuid::Player>(fields[5].GetUInt64());
            message.Content         = fields[6].GetString();
            message.CreatedTime     = time_t(fields[7].GetUInt64());

            Stream& stream = _streams[StreamKey{ message.ClubId, message.StreamId }];
            stream.NextPosition = std::max(stream.NextPosition, message.Position + 1);
            stream.Messages.push_back(std::move(message));
            ++count;
        }
        while (result->NextRow());
    }

    // Count based retention. The rows are ordered ascending, so the front of each deque is the oldest.
    for (auto& [key, stream] : _streams)
        TrimStream(key.ClubId, key.StreamId, stream);

    TC_LOG_INFO("server.loading", ">> Loaded {} club stream messages across {} streams in {} ms", count, _streams.size(), GetMSTimeDiffToNow(oldMSTime));

    LoadViewMarkers();
    LoadMentions();
}

void ClubStreamHistoryMgr::LoadViewMarkers()
{
    _streamViewTimes.clear();
    _mentionViewTimes.clear();

    if (QueryResult result = CharacterDatabase.Query("SELECT clubId, streamId, memberGuid, lastViewTime FROM club_stream_view_marker"))
    {
        do
        {
            Field* fields = result->Fetch();
            _streamViewTimes[ViewMarkerKey{ fields[0].GetUInt64(), fields[1].GetUInt64(), fields[2].GetUInt64() }] = fields[3].GetUInt64();
        }
        while (result->NextRow());
    }

    if (QueryResult result = CharacterDatabase.Query("SELECT memberGuid, lastViewTime FROM club_mention_view_marker"))
    {
        do
        {
            Field* fields = result->Fetch();
            _mentionViewTimes[fields[0].GetUInt64()] = fields[1].GetUInt64();
        }
        while (result->NextRow());
    }

    TC_LOG_INFO("server.loading", ">> Loaded {} club stream view markers and {} mention view markers",
        _streamViewTimes.size(), _mentionViewTimes.size());
}

void ClubStreamHistoryMgr::LoadMentions()
{
    _mentionsByMember.clear();

    QueryResult result = CharacterDatabase.Query("SELECT clubId, streamId, memberGuid, epoch, position, authorGuid, authorAccountId "
        "FROM club_member_mention ORDER BY epoch, position");
    if (!result)
        return;

    uint32 count = 0;
    do
    {
        Field* fields = result->Fetch();

        ClubMemberMention mention;
        mention.ClubId          = fields[0].GetUInt64();
        mention.StreamId        = fields[1].GetUInt64();
        mention.MemberGuid      = ObjectGuid::Create<HighGuid::Player>(fields[2].GetUInt64());
        mention.Epoch           = fields[3].GetUInt64();
        mention.Position        = fields[4].GetUInt64();
        mention.AuthorGuid      = ObjectGuid::Create<HighGuid::Player>(fields[5].GetUInt64());
        mention.AuthorAccountId = fields[6].GetUInt32();

        _mentionsByMember[mention.MemberGuid.GetCounter()].push_back(std::move(mention));
        ++count;
    }
    while (result->NextRow());

    TC_LOG_INFO("server.loading", ">> Loaded {} club stream mentions for {} members", count, _mentionsByMember.size());
}

void ClubStreamHistoryMgr::TrimStream(uint64 clubId, uint64 streamId, Stream& stream)
{
    uint32 maxMessages = GetMaxMessagesPerStream();
    if (!maxMessages || stream.Messages.size() <= maxMessages)
        return;

    while (stream.Messages.size() > maxMessages)
        stream.Messages.pop_front();

    // One bounded delete instead of one per dropped row: everything strictly older than the new front.
    ClubStreamMessage const& oldest = stream.Messages.front();

    CharacterDatabasePreparedStatement* stmt = CharacterDatabase.GetPreparedStatement(CHAR_DEL_CLUB_MESSAGE_TRIM);
    stmt->setUInt64(0, clubId);
    stmt->setUInt64(1, streamId);
    stmt->setUInt64(2, oldest.Epoch);
    stmt->setUInt64(3, oldest.Epoch);
    stmt->setUInt64(4, oldest.Position);
    CharacterDatabase.Execute(stmt);
}

ClubStreamHistoryMgr::Stream const* ClubStreamHistoryMgr::FindStream(uint64 clubId, uint64 streamId) const
{
    auto itr = _streams.find(StreamKey{ clubId, streamId });
    return itr != _streams.end() ? &itr->second : nullptr;
}

ClubStreamMessage const* ClubStreamHistoryMgr::AddMessage(uint64 clubId, uint64 streamId, ObjectGuid author, uint32 authorAccountId,
    std::string_view content, std::vector<ObjectGuid> const& mentioned)
{
    if (!IsEnabled() || content.empty())
        return nullptr;

    Stream& stream = _streams[StreamKey{ clubId, streamId }];

    ClubStreamMessage message;
    message.ClubId          = clubId;
    message.StreamId        = streamId;
    message.Epoch           = NowMicroseconds();
    message.Position        = stream.NextPosition++;
    message.AuthorAccountId = authorAccountId;
    message.AuthorGuid      = author;
    message.Content.assign(content);
    message.CreatedTime     = GameTime::GetGameTime();

    CharacterDatabaseTransaction trans = CharacterDatabase.BeginTransaction();

    CharacterDatabasePreparedStatement* stmt = CharacterDatabase.GetPreparedStatement(CHAR_INS_CLUB_MESSAGE);
    stmt->setUInt64(0, message.ClubId);
    stmt->setUInt64(1, message.StreamId);
    stmt->setUInt64(2, message.Epoch);
    stmt->setUInt64(3, message.Position);
    stmt->setUInt32(4, message.AuthorAccountId);
    stmt->setUInt64(5, message.AuthorGuid.GetCounter());
    stmt->setString(6, message.Content);
    stmt->setUInt64(7, uint64(message.CreatedTime));
    trans->Append(stmt);

    for (ObjectGuid const& target : mentioned)
    {
        if (target == author)
            continue;

        ClubMemberMention mention;
        mention.ClubId          = clubId;
        mention.StreamId        = streamId;
        mention.MemberGuid      = target;
        mention.Epoch           = message.Epoch;
        mention.Position        = message.Position;
        mention.AuthorGuid      = author;
        mention.AuthorAccountId = authorAccountId;

        CharacterDatabasePreparedStatement* mentionStmt = CharacterDatabase.GetPreparedStatement(CHAR_INS_CLUB_MEMBER_MENTION);
        mentionStmt->setUInt64(0, mention.ClubId);
        mentionStmt->setUInt64(1, mention.StreamId);
        mentionStmt->setUInt64(2, mention.MemberGuid.GetCounter());
        mentionStmt->setUInt64(3, mention.Epoch);
        mentionStmt->setUInt64(4, mention.Position);
        mentionStmt->setUInt64(5, mention.AuthorGuid.GetCounter());
        mentionStmt->setUInt32(6, mention.AuthorAccountId);
        mentionStmt->setUInt64(7, uint64(message.CreatedTime));
        trans->Append(mentionStmt);

        _mentionsByMember[target.GetCounter()].push_back(std::move(mention));
    }

    CharacterDatabase.CommitTransaction(trans);

    stream.Messages.push_back(std::move(message));
    TrimStream(clubId, streamId, stream);

    // The author has by definition read their own message, and so does anyone actively looking at the
    // stream, so their unread marker moves with it instead of lighting up the moment they type.
    ClubStreamMessage const& stored = stream.Messages.back();

    AdvanceStreamViewTime(clubId, streamId, author, stored.Epoch);

    return &stored;
}

ClubStreamMessage const* ClubStreamHistoryMgr::GetLatestMessage(uint64 clubId, uint64 streamId) const
{
    Stream const* stream = FindStream(clubId, streamId);
    if (!stream || stream->Messages.empty())
        return nullptr;

    return &stream->Messages.back();
}

uint64 ClubStreamHistoryMgr::GetLastMessageTime(uint64 clubId, uint64 streamId) const
{
    ClubStreamMessage const* latest = GetLatestMessage(clubId, streamId);
    return latest ? latest->Epoch : 0;
}

ClubStreamMessage const* ClubStreamHistoryMgr::GetMessage(uint64 clubId, uint64 streamId, uint64 epoch, uint64 position) const
{
    Stream const* stream = FindStream(clubId, streamId);
    if (!stream)
        return nullptr;

    for (ClubStreamMessage const& message : stream->Messages)
        if (message.Epoch == epoch && message.Position == position)
            return &message;

    return nullptr;
}

std::vector<ClubStreamMessage const*> ClubStreamHistoryMgr::GetHistory(uint64 clubId, uint64 streamId, uint64 fetchFrom, uint64 fetchUntil,
    uint32 maxEvents, bool ascending) const
{
    std::vector<ClubStreamMessage const*> page;

    Stream const* stream = FindStream(clubId, streamId);
    if (!stream || stream->Messages.empty() || !maxEvents)
        return page;

    // GetEventOptions gives two bounds and a direction. Reading them as an ordered pair (fetch_from is the
    // start of the traversal, fetch_until its end) and as a plain time window disagree only in which of the
    // two is the lower bound, so they are normalised here and the pair behaves identically either way.
    uint64 lowerBound = std::min(fetchFrom, fetchUntil);
    uint64 upperBound = std::max(fetchFrom, fetchUntil);

    page.reserve(std::min<size_t>(maxEvents, stream->Messages.size()));

    if (ascending)
    {
        for (auto itr = stream->Messages.begin(); itr != stream->Messages.end() && page.size() < maxEvents; ++itr)
            if (itr->Epoch >= lowerBound && itr->Epoch <= upperBound)
                page.push_back(&*itr);
    }
    else
    {
        // Newest first, which is what the client pages backwards through.
        for (auto itr = stream->Messages.rbegin(); itr != stream->Messages.rend() && page.size() < maxEvents; ++itr)
            if (itr->Epoch >= lowerBound && itr->Epoch <= upperBound)
                page.push_back(&*itr);
    }

    return page;
}

uint64 ClubStreamHistoryMgr::GetStreamViewTime(uint64 clubId, uint64 streamId, ObjectGuid member) const
{
    auto itr = _streamViewTimes.find(ViewMarkerKey{ clubId, streamId, member.GetCounter() });
    return itr != _streamViewTimes.end() ? itr->second : 0;
}

void ClubStreamHistoryMgr::AdvanceStreamViewTime(uint64 clubId, uint64 streamId, ObjectGuid member, uint64 viewTime)
{
    if (member.IsEmpty())
        return;

    if (!viewTime)
        viewTime = NowMicroseconds();

    uint64& stored = _streamViewTimes[ViewMarkerKey{ clubId, streamId, member.GetCounter() }];

    // A view marker only ever moves forward; a late arriving request must not resurrect unread state.
    if (stored >= viewTime)
        return;

    stored = viewTime;

    CharacterDatabasePreparedStatement* stmt = CharacterDatabase.GetPreparedStatement(CHAR_REP_CLUB_STREAM_VIEW_MARKER);
    stmt->setUInt64(0, clubId);
    stmt->setUInt64(1, streamId);
    stmt->setUInt64(2, member.GetCounter());
    stmt->setUInt64(3, stored);
    CharacterDatabase.Execute(stmt);
}

void ClubStreamHistoryMgr::SetStreamSubscribed(uint64 clubId, ObjectGuid member, std::vector<uint64> const& streamIds, bool subscribed)
{
    if (member.IsEmpty())
        return;

    std::unordered_set<uint64>& streams = _subscribedStreams[member.GetCounter()];
    for (uint64 streamId : streamIds)
    {
        if (subscribed)
            streams.insert(PackStreamKey(clubId, streamId));
        else
            streams.erase(PackStreamKey(clubId, streamId));
    }

    if (streams.empty())
        _subscribedStreams.erase(member.GetCounter());
}

bool ClubStreamHistoryMgr::IsStreamSubscribed(uint64 clubId, uint64 streamId, ObjectGuid member) const
{
    auto itr = _subscribedStreams.find(member.GetCounter());
    return itr != _subscribedStreams.end() && itr->second.count(PackStreamKey(clubId, streamId)) != 0;
}

void ClubStreamHistoryMgr::SetStreamFocus(uint64 clubId, uint64 streamId, ObjectGuid member, bool focused)
{
    if (member.IsEmpty())
        return;

    if (focused)
    {
        _focusedStreams[member.GetCounter()].insert(PackStreamKey(clubId, streamId));
        return;
    }

    auto itr = _focusedStreams.find(member.GetCounter());
    if (itr == _focusedStreams.end())
        return;

    itr->second.erase(PackStreamKey(clubId, streamId));
    if (itr->second.empty())
        _focusedStreams.erase(itr);
}

bool ClubStreamHistoryMgr::IsStreamFocused(uint64 clubId, uint64 streamId, ObjectGuid member) const
{
    auto itr = _focusedStreams.find(member.GetCounter());
    return itr != _focusedStreams.end() && itr->second.count(PackStreamKey(clubId, streamId)) != 0;
}

void ClubStreamHistoryMgr::ClearSessionState(ObjectGuid member)
{
    _subscribedStreams.erase(member.GetCounter());
    _focusedStreams.erase(member.GetCounter());
}

std::vector<ClubMemberMention const*> ClubStreamHistoryMgr::GetMentions(ObjectGuid member, uint64 fetchFrom, uint64 fetchUntil,
    uint32 maxEvents, bool ascending) const
{
    std::vector<ClubMemberMention const*> page;

    auto itr = _mentionsByMember.find(member.GetCounter());
    if (itr == _mentionsByMember.end() || !maxEvents)
        return page;

    uint64 lowerBound = std::min(fetchFrom, fetchUntil);
    uint64 upperBound = std::max(fetchFrom, fetchUntil);

    std::vector<ClubMemberMention> const& mentions = itr->second;
    page.reserve(std::min<size_t>(maxEvents, mentions.size()));

    if (ascending)
    {
        for (auto mentionItr = mentions.begin(); mentionItr != mentions.end() && page.size() < maxEvents; ++mentionItr)
            if (mentionItr->Epoch >= lowerBound && mentionItr->Epoch <= upperBound)
                page.push_back(&*mentionItr);
    }
    else
    {
        for (auto mentionItr = mentions.rbegin(); mentionItr != mentions.rend() && page.size() < maxEvents; ++mentionItr)
            if (mentionItr->Epoch >= lowerBound && mentionItr->Epoch <= upperBound)
                page.push_back(&*mentionItr);
    }

    return page;
}

uint64 ClubStreamHistoryMgr::GetLastMentionTime(ObjectGuid member) const
{
    auto itr = _mentionsByMember.find(member.GetCounter());
    if (itr == _mentionsByMember.end() || itr->second.empty())
        return 0;

    // Mentions are appended in send order and loaded ordered by (epoch, position).
    return itr->second.back().Epoch;
}

uint64 ClubStreamHistoryMgr::GetMentionViewTime(ObjectGuid member) const
{
    auto itr = _mentionViewTimes.find(member.GetCounter());
    return itr != _mentionViewTimes.end() ? itr->second : 0;
}

void ClubStreamHistoryMgr::AdvanceMentionViewTime(ObjectGuid member, uint64 viewTime)
{
    if (member.IsEmpty())
        return;

    if (!viewTime)
        viewTime = NowMicroseconds();

    uint64& stored = _mentionViewTimes[member.GetCounter()];
    if (stored >= viewTime)
        return;

    stored = viewTime;

    CharacterDatabasePreparedStatement* stmt = CharacterDatabase.GetPreparedStatement(CHAR_REP_CLUB_MENTION_VIEW_MARKER);
    stmt->setUInt64(0, member.GetCounter());
    stmt->setUInt64(1, stored);
    CharacterDatabase.Execute(stmt);
}

void ClubStreamHistoryMgr::RemoveMentions(ObjectGuid member, std::vector<std::pair<uint64, uint64>> const& messageIds)
{
    auto itr = _mentionsByMember.find(member.GetCounter());
    if (itr == _mentionsByMember.end())
        return;

    CharacterDatabaseTransaction trans = CharacterDatabase.BeginTransaction();

    for (auto const& [epoch, position] : messageIds)
    {
        auto removed = std::remove_if(itr->second.begin(), itr->second.end(), [&](ClubMemberMention const& mention)
        {
            return mention.Epoch == epoch && mention.Position == position;
        });

        if (removed == itr->second.end())
            continue;

        itr->second.erase(removed, itr->second.end());

        CharacterDatabasePreparedStatement* stmt = CharacterDatabase.GetPreparedStatement(CHAR_DEL_CLUB_MEMBER_MENTION);
        stmt->setUInt64(0, member.GetCounter());
        stmt->setUInt64(1, epoch);
        stmt->setUInt64(2, position);
        trans->Append(stmt);
    }

    CharacterDatabase.CommitTransaction(trans);

    if (itr->second.empty())
        _mentionsByMember.erase(itr);
}

std::vector<ObjectGuid> ClubStreamHistoryMgr::ExtractMentions(std::string_view content,
    std::unordered_map<std::string, ObjectGuid> const& membersByLowercaseName)
{
    std::vector<ObjectGuid> mentioned;

    if (membersByLowercaseName.empty())
        return mentioned;

    for (size_t pos = content.find('@'); pos != std::string_view::npos; pos = content.find('@', pos + 1))
    {
        size_t nameStart = pos + 1;
        size_t nameEnd = nameStart;

        // Character names are a single token; anything that is not a letter ends it. Non-ASCII bytes are
        // kept because localised realms allow accented names, which arrive here as UTF-8 continuation bytes.
        while (nameEnd < content.size())
        {
            unsigned char c = static_cast<unsigned char>(content[nameEnd]);
            if (c < 0x80 && !std::isalpha(c))
                break;

            ++nameEnd;
        }

        if (nameEnd == nameStart)
            continue;

        std::string name(content.substr(nameStart, nameEnd - nameStart));
        std::transform(name.begin(), name.end(), name.begin(), [](unsigned char c) { return char(std::tolower(c)); });

        auto itr = membersByLowercaseName.find(name);
        if (itr == membersByLowercaseName.end())
            continue;

        if (std::find(mentioned.begin(), mentioned.end(), itr->second) == mentioned.end())
            mentioned.push_back(itr->second);
    }

    return mentioned;
}
