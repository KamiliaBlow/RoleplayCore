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

#ifndef TRINITYCORE_CLUB_STREAM_HISTORY_MGR_H
#define TRINITYCORE_CLUB_STREAM_HISTORY_MGR_H

#include "Define.h"
#include "ObjectGuid.h"
#include <deque>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <vector>

// A single persisted club stream message.
//
// Epoch/Position together are bgs.protocol.MessageId: Epoch is the send time in MICROSECONDS since the
// unix epoch (the unit ClubService has always used for MessageId.epoch and ContentChain.edit_time, and
// the unit the client already renders correctly), Position is a per-stream monotonic counter that makes
// the identifier unique when two messages land inside the same microsecond and gives the stream a total
// order independent of clock jitter.
struct ClubStreamMessage
{
    uint64 ClubId           = 0;
    uint64 StreamId         = 0;
    uint64 Epoch            = 0;    // MessageId.epoch    - microseconds since unix epoch
    uint64 Position         = 0;    // MessageId.position - monotonic per (club, stream)
    uint32 AuthorAccountId  = 0;
    ObjectGuid AuthorGuid;
    std::string Content;
    time_t CreatedTime      = 0;    // unix seconds, used only for age based retention
};

// A resolved @name mention of one member inside one message.
struct ClubMemberMention
{
    uint64 ClubId    = 0;
    uint64 StreamId  = 0;
    ObjectGuid MemberGuid;          // the mentioned member
    uint64 Epoch     = 0;           // identifies the message, and doubles as the mention's TimeSeriesId
    uint64 Position  = 0;
    ObjectGuid AuthorGuid;
    uint32 AuthorAccountId = 0;
};

// Backing store for club stream scrollback and per-member read state.
//
// Persisted in the CHARACTER database: the only clubs that exist today are guilds, guild id == club id,
// and every guild table (guild, guild_member, guild_bank_item, club_finder_posting, ...) lives in the
// character schema. View markers are keyed by character GUID, which is also a character-schema key, and
// the auth schema holds no guild data at all and is shared across realms - a guild id there would be
// ambiguous. Account scoped storage only becomes correct once non-guild communities exist.
//
// Messages are kept in memory (a bounded deque per stream) so the RPC handlers, which have to fill their
// response synchronously, never issue a blocking query. The database is the durable mirror of that cache.
class TC_GAME_API ClubStreamHistoryMgr
{
public:
    static ClubStreamHistoryMgr* instance();

    // Loads (and prunes) the persisted history and read state. Called once from World::SetInitialWorldSettings.
    void Load();

    bool IsEnabled() const;
    uint32 GetMaxMessagesPerStream() const;

    // Stores a message and returns it, or nullptr when history is disabled. mentioned may be empty.
    ClubStreamMessage const* AddMessage(uint64 clubId, uint64 streamId, ObjectGuid author, uint32 authorAccountId,
        std::string_view content, std::vector<ObjectGuid> const& mentioned);

    // The most recently stored message of a stream, or nullptr when the stream has no history.
    ClubStreamMessage const* GetLatestMessage(uint64 clubId, uint64 streamId) const;

    // Epoch of the newest message in a stream (0 when empty). This is ViewMarker.last_message_time.
    uint64 GetLastMessageTime(uint64 clubId, uint64 streamId) const;

    // A page of history. fetchFrom/fetchUntil are inclusive bounds on ClubStreamMessage::Epoch and may be
    // 0 / UINT64_MAX to mean "unbounded"; if they arrive inverted they are swapped, so the call behaves the
    // same whether the client means them as a time window or as the start/end of a directional traversal.
    // maxEvents caps the page; the returned vector is ordered newest first unless ascending is set.
    std::vector<ClubStreamMessage const*> GetHistory(uint64 clubId, uint64 streamId, uint64 fetchFrom, uint64 fetchUntil,
        uint32 maxEvents, bool ascending) const;

    // Per (club, stream, member) read marker. Times are microseconds, matching MessageId.epoch.
    uint64 GetStreamViewTime(uint64 clubId, uint64 streamId, ObjectGuid member) const;
    void AdvanceStreamViewTime(uint64 clubId, uint64 streamId, ObjectGuid member, uint64 viewTime);

    // Which streams a member currently has subscribed / focused. Live session state, not persisted:
    // a focused stream is one the member is actively reading, so messages arriving in it are marked read.
    void SetStreamSubscribed(uint64 clubId, ObjectGuid member, std::vector<uint64> const& streamIds, bool subscribed);
    bool IsStreamSubscribed(uint64 clubId, uint64 streamId, ObjectGuid member) const;
    void SetStreamFocus(uint64 clubId, uint64 streamId, ObjectGuid member, bool focused);
    bool IsStreamFocused(uint64 clubId, uint64 streamId, ObjectGuid member) const;
    void ClearSessionState(ObjectGuid member);

    // Mentions. The mention view time is a single per-member value because the client's
    // AdvanceStreamMentionViewTime request carries no club or stream id at all.
    std::vector<ClubMemberMention const*> GetMentions(ObjectGuid member, uint64 fetchFrom, uint64 fetchUntil,
        uint32 maxEvents, bool ascending) const;
    uint64 GetLastMentionTime(ObjectGuid member) const;
    uint64 GetMentionViewTime(ObjectGuid member) const;
    void AdvanceMentionViewTime(ObjectGuid member, uint64 viewTime);
    void RemoveMentions(ObjectGuid member, std::vector<std::pair<uint64, uint64>> const& messageIds);

    // Finds the message a mention points at, so GetStreamMentions can honour fetch_messages.
    ClubStreamMessage const* GetMessage(uint64 clubId, uint64 streamId, uint64 epoch, uint64 position) const;

    // Extracts the guild members named with @Name in a message body. Matching is case insensitive and a
    // name only counts when it is a whole token, so "@Foobar" never resolves to the member "Foo".
    static std::vector<ObjectGuid> ExtractMentions(std::string_view content,
        std::unordered_map<std::string, ObjectGuid> const& membersByLowercaseName);

private:
    ClubStreamHistoryMgr() = default;
    ~ClubStreamHistoryMgr() = default;
    ClubStreamHistoryMgr(ClubStreamHistoryMgr const&) = delete;
    ClubStreamHistoryMgr& operator=(ClubStreamHistoryMgr const&) = delete;

    struct StreamKey
    {
        uint64 ClubId;
        uint64 StreamId;

        bool operator==(StreamKey const& right) const { return ClubId == right.ClubId && StreamId == right.StreamId; }
    };

    struct StreamKeyHash
    {
        std::size_t operator()(StreamKey const& key) const
        {
            return std::hash<uint64>()(key.ClubId) ^ (std::hash<uint64>()(key.StreamId) << 1);
        }
    };

    struct Stream
    {
        std::deque<ClubStreamMessage> Messages;
        uint64 NextPosition = 1;
    };

    struct ViewMarkerKey
    {
        uint64 ClubId;
        uint64 StreamId;
        uint64 MemberGuid;

        bool operator==(ViewMarkerKey const& right) const
        {
            return ClubId == right.ClubId && StreamId == right.StreamId && MemberGuid == right.MemberGuid;
        }
    };

    struct ViewMarkerKeyHash
    {
        std::size_t operator()(ViewMarkerKey const& key) const
        {
            return std::hash<uint64>()(key.ClubId) ^ (std::hash<uint64>()(key.StreamId) << 1) ^ (std::hash<uint64>()(key.MemberGuid) << 2);
        }
    };

    Stream const* FindStream(uint64 clubId, uint64 streamId) const;

    // Drops everything above the retention cap from the cache and from the database.
    void TrimStream(uint64 clubId, uint64 streamId, Stream& stream);

    void LoadViewMarkers();
    void LoadMentions();

    std::unordered_map<StreamKey, Stream, StreamKeyHash> _streams;
    std::unordered_map<ViewMarkerKey, uint64, ViewMarkerKeyHash> _streamViewTimes;
    std::unordered_map<uint64, std::vector<ClubMemberMention>> _mentionsByMember;    // member guid counter -> mentions
    std::unordered_map<uint64, uint64> _mentionViewTimes;                            // member guid counter -> view time

    // Live session state, keyed by member guid counter.
    std::unordered_map<uint64, std::unordered_set<uint64>> _subscribedStreams;       // packed (clubId, streamId)
    std::unordered_map<uint64, std::unordered_set<uint64>> _focusedStreams;
};

#define sClubStreamHistoryMgr ClubStreamHistoryMgr::instance()

#endif // TRINITYCORE_CLUB_STREAM_HISTORY_MGR_H
