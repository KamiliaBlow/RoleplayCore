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

#include "ClubService.h"
#include "BattlenetRpcErrorCodes.h"
#include "ClientBuildInfo.h"
#include "ClubFinderMgr.h"
#include "ClubMembershipService.h"
#include "ClubStreamHistoryMgr.h"
#include "GameTime.h"
#include "Guild.h"
#include "Log.h"
#include "Player.h"
#include "SocialMgr.h"
#include "api/client/v1/club_listener.pb.h"

#include <algorithm>
#include <chrono>
#include <limits>

namespace Battlenet::Services
{

ClubService::ClubService(WorldSession* session) : BaseService(session) { }

uint32 ClubService::HandleGetClubType(club::v1::client::GetClubTypeRequest const* request, club::v1::client::GetClubTypeResponse* response,
    std::function<void(ServiceBase*, uint32, google::protobuf::Message const*)>& /*continuation*/)
{
    // We only support guilds for now.
    if (request->type().name() == "guild")
    {
        response->set_allocated_type(CreateGuildClubType().release());
        return ERROR_OK;
    }

    return ERROR_NOT_IMPLEMENTED;
}

uint32 ClubService::HandleSubscribe(club::v1::client::SubscribeRequest const* /*request*/, NoData* /*response*/,
    std::function<void(ServiceBase*, uint32, google::protobuf::Message const*)>& /*continuation*/)
{
    Player const* player = _session->GetPlayer();

    if (!player)
        return ERROR_INTERNAL;

    Guild const* guild = player->GetGuild();

    if (!guild)
        return ERROR_CLUB_NO_CLUB;

    // Subscibe the client to it's own guild club.
    club::v1::client::SubscribeNotification subscribeNotification;

    Guild::Member const* guildMember = guild->GetMember(player->GetGUID());

    if (!guildMember)
        return ERROR_CLUB_NOT_MEMBER;

    Guild::Member const* guildLeader = guild->GetMember(guild->GetLeaderGUID());

    if (!guildLeader)
        return ERROR_CLUB_NO_SUCH_MEMBER;

    subscribeNotification.set_club_id(guild->GetId());
    subscribeNotification.set_allocated_agent_id(ClubMembershipService::CreateClubMemberId(player->GetGUID()).release());

    club::v1::client::Club* guildClub = subscribeNotification.mutable_club();

    guildClub->set_id(guild->GetId());
    guildClub->set_allocated_type(CreateGuildClubType().release());
    guildClub->set_name(guild->GetName());

    // The 12.1 client treats the club description as the recruitment description: the editor
    // prefills from it and the native C_ClubFinder.PostClub reads it from the club registry
    // (its description ARGUMENT is silently dropped). Serve the posting's description so the
    // round trip works: UpdateClubState persists edits onto the posting, this feeds them back.
    if (ClubFinderPosting const* posting = sClubFinderMgr->GetPostingForClub(guild->GetId()))
        guildClub->set_description(posting->Description);

    // These are not related to normal guild functionality so we hardcode them for now.
    guildClub->set_privacy_level(club::v1::PrivacyLevel::PRIVACY_LEVEL_OPEN);
    guildClub->set_visibility_level(club::v1::VISIBILITY_LEVEL_PRIVATE);

    guildClub->set_member_count(guild->GetMembersCount());

    // Set the club leader, guild master in this case.
    club::v1::client::MemberDescription* guildLeaderDescription = guildClub->add_leader();

    guildLeaderDescription->mutable_id()->set_account_id(guildLeader->GetAccountId());
    guildLeaderDescription->mutable_id()->set_unique_id(guildLeader->GetGUID().GetCounter());

    club::v1::client::Member* subscriber = subscribeNotification.mutable_member();

    // The member sending the notification data.
    subscriber->set_allocated_id(ClubMembershipService::CreateClubMemberId(player->GetGUID()).release());

    // Community/Club default roles have slightly different values.
    // Also this is required to set the current leader/guild master symbol in the interface.
    // 1 = Owner, 4 = Member. Once communities are fully implemented these will go into a new database table.
    if (guildMember->IsRank(GuildRankId::GuildMaster))
        subscriber->add_role(AsUnderlyingType(ClubRoleIdentifier::Owner));
    else if (guild->HasAnyRankRight(guildMember->GetRankId(), GuildRankRights(GR_RIGHT_OFFCHATLISTEN | GR_RIGHT_OFFCHATSPEAK)))
        subscriber->add_role(AsUnderlyingType(ClubRoleIdentifier::Moderator));
    else
        subscriber->add_role(AsUnderlyingType(ClubRoleIdentifier::Member));

    subscriber->set_presence_level(club::v1::client::PRESENCE_LEVEL_RICH);
    subscriber->set_whisper_level(club::v1::client::WHISPER_LEVEL_OPEN);

    // Member is online and active.
    subscriber->set_active(true);

    WorldserverService<club::v1::client::ClubListener>(_session).OnSubscribe(&subscribeNotification, true, true);

    // Notify the client about the changed club state.
    club::v1::client::SubscriberStateChangedNotification subscriberStateChangedNotification;

    subscriberStateChangedNotification.set_club_id(guild->GetId());

    club::v1::client::SubscriberStateAssignment* assignment = subscriberStateChangedNotification.add_assignment();

    assignment->set_allocated_member_id(ClubMembershipService::CreateClubMemberId(player->GetGUID()).release());

    // Member is online and active.
    assignment->set_active(true);

    WorldserverService<club::v1::client::ClubListener>(_session).OnSubscriberStateChanged(&subscriberStateChangedNotification, true, true);

    return ERROR_OK;
}

uint32 ClubService::HandleGetMembers(club::v1::client::GetMembersRequest const* /*request*/, club::v1::client::GetMembersResponse* response,
    std::function<void(ServiceBase*, uint32, google::protobuf::Message const*)>& /*continuation*/)
{
    Player const* player = _session->GetPlayer();

    if (!player)
        return ERROR_INTERNAL;

    Guild const* guild = player->GetGuild();

    if (!guild)
        return ERROR_CLUB_NO_CLUB;

    response->mutable_member()->Reserve(guild->GetMembersCount());

    for (auto const& [guid, member] : guild->GetMembers())
    {
        club::v1::client::Member* clubMember = response->add_member();

        clubMember->set_allocated_id(ClubMembershipService::CreateClubMemberId(guid).release());

        // Community/Club default roles have slightly different values.
        // When communities are implemented those are going to be database fields.
        if (member.IsRank(GuildRankId::GuildMaster))
            clubMember->add_role(AsUnderlyingType(ClubRoleIdentifier::Owner));
        else if (guild->HasAnyRankRight(member.GetRankId(), GuildRankRights(GR_RIGHT_OFFCHATLISTEN | GR_RIGHT_OFFCHATSPEAK)))
            clubMember->add_role(AsUnderlyingType(ClubRoleIdentifier::Moderator));
        else
            clubMember->add_role(AsUnderlyingType(ClubRoleIdentifier::Member));

        clubMember->set_presence_level(club::v1::client::PresenceLevel::PRESENCE_LEVEL_RICH);
        clubMember->set_whisper_level(club::v1::client::WhisperLevel::WHISPER_LEVEL_OPEN);
        std::string_view publicNote = member.GetPublicNote();
        clubMember->set_note(publicNote.data(), publicNote.size());
        clubMember->set_active(member.IsOnline());
    }

    return ERROR_OK;
}

uint32 ClubService::HandleGetStreams(club::v1::client::GetStreamsRequest const* /*request*/, club::v1::client::GetStreamsResponse* response,
    std::function<void(ServiceBase*, uint32, google::protobuf::Message const*)>& /*continuation*/)
{
    Player const* player = _session->GetPlayer();

    if (!player)
        return ERROR_INTERNAL;

    Guild const* guild = player->GetGuild();

    if (!guild)
        return ERROR_CLUB_NO_CLUB;

    // General guild channel.
    club::v1::client::Stream* generalGuildChannelStream = response->add_stream();

    generalGuildChannelStream->set_club_id(guild->GetId());
    generalGuildChannelStream->set_id(AsUnderlyingType(ClubStreamType::Guild));

    v2::Attribute* generalStreamAttribute = generalGuildChannelStream->add_attribute();

    generalStreamAttribute->set_name("global_strings_tag");
    generalStreamAttribute->mutable_value()->set_string_value("COMMUNITIES_GUILD_GENERAL_CHANNEL_NAME");

    generalGuildChannelStream->set_name("Guild");

    // All roles got access to this channel.
    // Club roles are currently guild role + 1.
    // With a complete club/community system those will be handled differently.
    generalGuildChannelStream->mutable_access()->add_role(AsUnderlyingType(ClubRoleIdentifier::Owner));
    generalGuildChannelStream->mutable_access()->add_role(AsUnderlyingType(ClubRoleIdentifier::Leader));
    generalGuildChannelStream->mutable_access()->add_role(AsUnderlyingType(ClubRoleIdentifier::Moderator));
    generalGuildChannelStream->mutable_access()->add_role(AsUnderlyingType(ClubRoleIdentifier::Member));

    // No voice support.
    generalGuildChannelStream->set_voice_level(club::v1::client::StreamVoiceLevel::VOICE_LEVEL_DISABLED);

    // Officer guild channel.
    club::v1::client::Stream* officerGuildChannelStream = response->add_stream();

    officerGuildChannelStream->set_club_id(guild->GetId());
    officerGuildChannelStream->set_id(AsUnderlyingType(ClubStreamType::Officer));

    v2::Attribute* officerStreamAttribute = officerGuildChannelStream->add_attribute();

    officerStreamAttribute->set_name("global_strings_tag");
    officerStreamAttribute->mutable_value()->set_string_value("COMMUNITIES_GUILD_OFFICER_CHANNEL_NAME");

    officerGuildChannelStream->set_name("Officer");

    // All roles got access to this channel.
    // Club roles are currently guild role + 1.
    // With a complete club/community system those will be handled differently.
    officerGuildChannelStream->mutable_access()->add_role(AsUnderlyingType(ClubRoleIdentifier::Owner));
    officerGuildChannelStream->mutable_access()->add_role(AsUnderlyingType(ClubRoleIdentifier::Leader));
    officerGuildChannelStream->mutable_access()->add_role(AsUnderlyingType(ClubRoleIdentifier::Moderator));

    // No voice support.
    officerGuildChannelStream->set_voice_level(club::v1::client::StreamVoiceLevel::VOICE_LEVEL_DISABLED);

    // Enable channel view
    club::v1::client::StreamView* generalView = response->add_view();

    generalView->set_club_id(guild->GetId());
    generalView->set_stream_id(AsUnderlyingType(ClubStreamType::Guild));

    club::v1::client::StreamView* officerView = response->add_view();

    officerView->set_club_id(guild->GetId());
    officerView->set_stream_id(AsUnderlyingType(ClubStreamType::Officer));

    // Unread state: the client compares each stream's last message time against the member's last read
    // time to decide whether the stream shows an unread highlight.
    auto fillViewMarker = [guild, player](club::v1::client::StreamView* view, uint64 streamId)
    {
        view->mutable_marker()->set_last_read_time(sClubStreamHistoryMgr->GetStreamViewTime(guild->GetId(), streamId, player->GetGUID()));
        view->mutable_marker()->set_last_message_time(sClubStreamHistoryMgr->GetLastMessageTime(guild->GetId(), streamId));
    };

    fillViewMarker(generalView, AsUnderlyingType(ClubStreamType::Guild));
    fillViewMarker(officerView, AsUnderlyingType(ClubStreamType::Officer));

    return ERROR_OK;
}

uint32 ClubService::HandleSubscribeStream(club::v1::client::SubscribeStreamRequest const* request, NoData* /*response*/,
    std::function<void(ServiceBase*, uint32, google::protobuf::Message const*)>& /*continuation*/)
{
    Player const* player = _session->GetPlayer();

    if (!player)
        return ERROR_INTERNAL;

    Guild const* guild = player->GetGuild();

    if (!guild)
        return ERROR_CLUB_NO_CLUB;

    // Basic sanity check until full communities are implemented.
    // 1 - Guild, 2 - Officer chat stream.
    if (request->stream_id().empty() || (request->stream_id().Get(0) != AsUnderlyingType(ClubStreamType::Guild) && request->stream_id().Get(0) != AsUnderlyingType(ClubStreamType::Officer)))
        return ERROR_CLUB_STREAM_NO_STREAM;

    std::vector<uint64> streamIds(request->stream_id().begin(), request->stream_id().end());
    sClubStreamHistoryMgr->SetStreamSubscribed(guild->GetId(), player->GetGUID(), streamIds, true);

    return ERROR_OK;
}

uint32 ClubService::HandleUnsubscribeStream(club::v1::client::UnsubscribeStreamRequest const* request, NoData* /*response*/,
    std::function<void(ServiceBase*, uint32, google::protobuf::Message const*)>& /*continuation*/)
{
    Player const* player = _session->GetPlayer();

    if (!player)
        return ERROR_INTERNAL;

    Guild const* guild = player->GetGuild();

    if (!guild)
        return ERROR_CLUB_NO_CLUB;

    std::vector<uint64> streamIds(request->stream_id().begin(), request->stream_id().end());
    sClubStreamHistoryMgr->SetStreamSubscribed(guild->GetId(), player->GetGUID(), streamIds, false);

    // We just have to signal the client that the unsubscribe request came through.
    return ERROR_OK;
}

uint32 ClubService::HandleSetStreamFocus(club::v1::client::SetStreamFocusRequest const* request, NoData* /*response*/,
    std::function<void(ServiceBase*, uint32, google::protobuf::Message const*)>& /*continuation*/)
{
    Player const* player = _session->GetPlayer();

    if (!player)
        return ERROR_INTERNAL;

    Guild const* guild = player->GetGuild();

    if (!guild)
        return ERROR_CLUB_NOT_MEMBER;

    if (request->club_id() != guild->GetId())
        return ERROR_CLUB_NO_CLUB;

    // Basic sanity check until full communities are implemented.
    // 1 - Guild, 2 - Officer chat stream.
    if (request->stream_id() != AsUnderlyingType(ClubStreamType::Guild) && request->stream_id() != AsUnderlyingType(ClubStreamType::Officer))
        return ERROR_CLUB_STREAM_NO_STREAM;

    sClubStreamHistoryMgr->SetStreamFocus(guild->GetId(), request->stream_id(), player->GetGUID(), request->has_focus() ? request->focus() : true);

    return ERROR_OK;
}

uint32 ClubService::HandleAdvanceStreamViewTime(club::v1::client::AdvanceStreamViewTimeRequest const* request, NoData* /*response*/,
    std::function<void(ServiceBase*, uint32, google::protobuf::Message const*)>& /*continuation*/)
{
    Player const* player = _session->GetPlayer();

    if (!player)
        return ERROR_INTERNAL;

    Guild const* guild = player->GetGuild();

    if (!guild)
        return ERROR_CLUB_NOT_MEMBER;

    if (request->club_id() != guild->GetId())
        return ERROR_CLUB_NO_CLUB;

    for (uint64 streamId : request->stream_id())
    {
        // Basic sanity check until full communities are implemented.
        // 1 - Guild, 2 - Officer chat stream.
        if (streamId != AsUnderlyingType(ClubStreamType::Guild) && streamId != AsUnderlyingType(ClubStreamType::Officer))
            continue;

        sClubStreamHistoryMgr->AdvanceStreamViewTime(guild->GetId(), streamId, player->GetGUID(), 0);
    }

    return ERROR_OK;
}

uint32 ClubService::HandleCreateMessage(club::v1::client::CreateMessageRequest const* request, club::v1::client::CreateMessageResponse* response,
    std::function<void(ServiceBase*, uint32, google::protobuf::Message const*)>& continuation)
{
    // Basic sanity check until full communities are implemented.
    // 1 - Guild, 2 - Officer chat stream.
    if (request->stream_id() != AsUnderlyingType(ClubStreamType::Guild) && request->stream_id() != AsUnderlyingType(ClubStreamType::Officer))
        return ERROR_CLUB_STREAM_NO_STREAM;

    // Just some sanity checks. We do not care about the requested stream for now since we only have two.
    Player const* player = _session->GetPlayer();

    if (!player)
        return ERROR_INTERNAL;

    Guild const* guild = player->GetGuild();

    if (!guild)
        return ERROR_CLUB_NO_CLUB;

    GuildRankRights requiredRights = { };
    ChatMessageResult result = { };

    switch (ClubStreamType(request->stream_id()))
    {
        case ClubStreamType::Guild:
            requiredRights = GR_RIGHT_GCHATLISTEN;
            result = _session->HandleChatMessage(CHAT_MSG_GUILD, LANG_UNIVERSAL, request->options().content());
            break;
        case ClubStreamType::Officer:
            requiredRights = GR_RIGHT_OFFCHATLISTEN;
            result = _session->HandleChatMessage(CHAT_MSG_OFFICER, LANG_UNIVERSAL, request->options().content());
            break;
        default:
            return ERROR_CLUB_STREAM_NO_STREAM;
    }

    if (result == ChatMessageResult::Ok)
    {
        std::string_view content = request->options().content();

        // Resolve @Name mentions against the guild roster so mentioned members can be tracked.
        std::unordered_map<std::string, ObjectGuid> membersByLowercaseName;
        for (auto const& [guid, member] : guild->GetMembers())
        {
            std::string name(member.GetName());
            std::transform(name.begin(), name.end(), name.begin(), [](unsigned char c) { return char(std::tolower(c)); });
            membersByLowercaseName.try_emplace(std::move(name), guid);
        }

        std::vector<ObjectGuid> mentioned = ClubStreamHistoryMgr::ExtractMentions(content, membersByLowercaseName);

        ClubStreamMessage const* stored = sClubStreamHistoryMgr->AddMessage(guild->GetId(), request->stream_id(),
            player->GetGUID(), _session->GetAccountId(), content, mentioned);

        uint64 const epoch = stored ? stored->Epoch : uint64(std::chrono::duration_cast<std::chrono::microseconds>(GameTime::GetSystemTime().time_since_epoch()).count());
        uint64 const position = stored ? stored->Position : 0;

        FillStreamMessage(response->mutable_message(), content, epoch, position, player->GetGUID());

        club::v1::client::StreamMessageAddedNotification messageAddedNotification;
        messageAddedNotification.set_allocated_agent_id(ClubMembershipService::CreateClubMemberId(player->GetGUID()).release());
        messageAddedNotification.set_club_id(guild->GetId());
        messageAddedNotification.set_stream_id(request->stream_id());
        FillStreamMessage(messageAddedNotification.mutable_message(), content, epoch, position, player->GetGUID());

        guild->BroadcastWorker([&](Player const* receiver)
        {
            Guild::Member const* receiverMember = guild->GetMember(receiver->GetGUID());
            if (!guild->HasAnyRankRight(receiverMember->GetRankId(), requiredRights))
                return;

            if (receiver->GetSocial()->HasIgnore(player->GetGUID(), _session->GetAccountGUID()))
                return;

            // A receiver who is currently focused on (or subscribed to) the stream reads the message as
            // it arrives, so their unread marker moves with it instead of lighting up.
            if (sClubStreamHistoryMgr->IsStreamFocused(guild->GetId(), request->stream_id(), receiver->GetGUID())
                || sClubStreamHistoryMgr->IsStreamSubscribed(guild->GetId(), request->stream_id(), receiver->GetGUID()))
                sClubStreamHistoryMgr->AdvanceStreamViewTime(guild->GetId(), request->stream_id(), receiver->GetGUID(), epoch);

            WorldserverService<club::v1::client::ClubListener>(receiver->GetSession()).OnStreamMessageAdded(&messageAddedNotification, true, true);
        }, player);

        return ERROR_OK;
    }

    // If the message is empty there should never be a response to message request.
    continuation = nullptr;

    return ERROR_CLUB_STREAM_NO_SUCH_MESSAGE;
}

std::unique_ptr<club::v1::UniqueClubType> ClubService::CreateGuildClubType()
{
    std::unique_ptr<club::v1::UniqueClubType> type = std::make_unique<club::v1::UniqueClubType>();
    type->set_program(ClientBuild::Program::WoW);
    type->set_name("guild");
    return type;
}

uint32 ClubService::HandleUpdateClubSettings(club::v1::client::UpdateClubSettingsRequest const* request, NoData* /*response*/,
    std::function<void(ServiceBase*, uint32, google::protobuf::Message const*)>& /*continuation*/)
{
    Player const* player = _session->GetPlayer();

    if (!player)
        return ERROR_INTERNAL;

    Guild const* guild = player->GetGuild();

    if (!guild)
        return ERROR_CLUB_NO_CLUB;

    if (request->club_id() != guild->GetId())
        return ERROR_CLUB_NO_CLUB;

    Guild::Member const* guildMember = guild->GetMember(player->GetGUID());

    if (!guildMember)
        return ERROR_CLUB_NOT_MEMBER;

    // Club settings are club-wide: only the owner (guild master) or a moderator (officer) may change
    // them, matching the role model the other handlers use.
    if (!guildMember->IsRank(GuildRankId::GuildMaster) && !guild->HasAnyRankRight(guildMember->GetRankId(), GuildRankRights(GR_RIGHT_OFFCHATLISTEN | GR_RIGHT_OFFCHATSPEAK)))
        return ERROR_CLUB_INSUFFICIENT_PRIVILEGES;

    // Echo the new settings to every guild member so their club model stays in sync. The attributes
    // themselves are not persisted server side: none of the guild functionality the server
    // implements reads them, and the recruitment state they gate is carried by the club finder
    // posting instead (CMSG_CLUB_FINDER_POST).
    club::v1::client::SettingsChangedNotification settingsChangedNotification;
    settingsChangedNotification.set_allocated_agent_id(ClubMembershipService::CreateClubMemberId(player->GetGUID()).release());
    settingsChangedNotification.set_club_id(guild->GetId());
    settingsChangedNotification.mutable_assignment()->mutable_settings()->CopyFrom(request->options().settings());

    guild->BroadcastWorker([&](Player const* receiver)
    {
        WorldserverService<club::v1::client::ClubListener>(receiver->GetSession()).OnSettingsChanged(&settingsChangedNotification, true, true);
    });

    return ERROR_OK;
}

// C_Club.EditClub lands here. The 12.1 client sends the guild's recruitment description through
// this RPC (present only when touched - the options presence mask) because the native
// C_ClubFinder.PostClub silently drops its description argument. Persist it onto the club finder
// posting so the LOOKUP record and the editor's prefill carry it.
uint32 ClubService::HandleUpdateClubState(club::v1::client::UpdateClubStateRequest const* request, NoData* /*response*/,
    std::function<void(ServiceBase*, uint32, google::protobuf::Message const*)>& /*continuation*/)
{
    Player* player = _session->GetPlayer();

    if (!player)
        return ERROR_INTERNAL;

    Guild* guild = player->GetGuild();

    if (!guild)
        return ERROR_CLUB_NO_CLUB;

    if (request->club_id() != guild->GetId())
        return ERROR_CLUB_NO_CLUB;

    Guild::Member const* guildMember = static_cast<Guild const*>(guild)->GetMember(player->GetGUID());

    if (!guildMember)
        return ERROR_CLUB_NOT_MEMBER;

    if (!guildMember->IsRank(GuildRankId::GuildMaster) && !guild->HasAnyRankRight(guildMember->GetRankId(), GuildRankRights(GR_RIGHT_OFFCHATLISTEN | GR_RIGHT_OFFCHATSPEAK)))
        return ERROR_CLUB_INSUFFICIENT_PRIVILEGES;

    auto const& options = request->options();

    if (options.has_description())
    {
        // Same dictionary gate as CMSG_CLUB_FINDER_POST: this RPC is the other way a recruitment
        // description reaches the database (C_Club.EditClub), so it has to refuse the same texts.
        if (ClubFinderMgr::IsProfaneText(options.description()))
        {
            TC_LOG_DEBUG("network", "ClubService.UpdateClubState [{}]: description rejected (forbidden word).",
                _session->GetPlayerInfo());
            return ERROR_INVALID_ARGS;
        }

        if (ClubFinderPosting const* existing = sClubFinderMgr->GetPostingForClub(guild->GetId()))
        {
            ClubFinderPosting updated = *existing;
            updated.Description = options.description();
            sClubFinderMgr->SavePosting(std::move(updated));
        }
        else if (!options.description().empty())
        {
            // No posting yet (first-time setup): stage the description on a minimal posting so the
            // follow-up CMSG_CLUB_FINDER_POST - which never carries a description in 12.1 - keeps it.
            ClubFinderPosting posting;
            posting.ClubId = guild->GetId();
            posting.Name = guild->GetName();
            posting.Description = options.description();
            posting.LastPosterGUID = player->GetGUID();
            sClubFinderMgr->SavePosting(std::move(posting));
        }

        // On retail the club description and the guild information text are one storage; the
        // recruitment editor edits the same value the guild window's info tab shows, so keep the
        // guild-side text in step (it also enforces the modify-info rank right).
        guild->HandleSetInfo(_session, options.description());
    }

    return ERROR_OK;
}

uint32 ClubService::HandleGetStreamHistory(club::v1::client::GetStreamHistoryRequest const* request, club::v1::client::GetStreamHistoryResponse* response,
    std::function<void(ServiceBase*, uint32, google::protobuf::Message const*)>& /*continuation*/)
{
    Player const* player = _session->GetPlayer();

    if (!player)
        return ERROR_INTERNAL;

    Guild const* guild = player->GetGuild();

    if (!guild)
        return ERROR_CLUB_NO_CLUB;

    if (request->club_id() != guild->GetId())
        return ERROR_CLUB_NO_CLUB;

    // Basic sanity check until full communities are implemented.
    // 1 - Guild, 2 - Officer chat stream.
    if (request->stream_id() != AsUnderlyingType(ClubStreamType::Guild) && request->stream_id() != AsUnderlyingType(ClubStreamType::Officer))
        return ERROR_CLUB_STREAM_NO_STREAM;

    bgs::protocol::GetEventOptions const& options = request->options();
    uint64 fetchFrom = options.has_fetch_from() ? options.fetch_from() : 0;
    uint64 fetchUntil = options.has_fetch_until() ? options.fetch_until() : std::numeric_limits<uint64>::max();
    uint32 maxEvents = options.has_max_events() ? options.max_events() : 100;

    // Newest first: the client fills its chat view with the most recent messages and pages backwards
    // through the club stream history from there.
    for (ClubStreamMessage const* message : sClubStreamHistoryMgr->GetHistory(guild->GetId(), request->stream_id(), fetchFrom, fetchUntil, maxEvents, false))
        FillStreamMessage(response->add_message(), message->Content, message->Epoch, message->Position, message->AuthorGuid);

    return ERROR_OK;
}

uint32 ClubService::HandleGetStreamMessage(club::v1::client::GetStreamMessageRequest const* request, club::v1::client::GetStreamMessageResponse* response,
    std::function<void(ServiceBase*, uint32, google::protobuf::Message const*)>& /*continuation*/)
{
    Player const* player = _session->GetPlayer();

    if (!player)
        return ERROR_INTERNAL;

    Guild const* guild = player->GetGuild();

    if (!guild)
        return ERROR_CLUB_NO_CLUB;

    if (request->club_id() != guild->GetId())
        return ERROR_CLUB_NO_CLUB;

    // Basic sanity check until full communities are implemented.
    // 1 - Guild, 2 - Officer chat stream.
    if (request->stream_id() != AsUnderlyingType(ClubStreamType::Guild) && request->stream_id() != AsUnderlyingType(ClubStreamType::Officer))
        return ERROR_CLUB_STREAM_NO_STREAM;

    if (!request->has_message_id())
        return ERROR_CLUB_STREAM_NO_SUCH_MESSAGE;

    ClubStreamMessage const* message = sClubStreamHistoryMgr->GetMessage(guild->GetId(), request->stream_id(),
        request->message_id().epoch(), request->message_id().position());

    if (!message)
        return ERROR_CLUB_STREAM_NO_SUCH_MESSAGE;

    FillStreamMessage(response->mutable_message(), message->Content, message->Epoch, message->Position, message->AuthorGuid);

    return ERROR_OK;
}

void ClubService::FillStreamMessage(club::v1::client::StreamMessage* message, std::string_view msg, uint64 epoch, uint64 position, ObjectGuid author)
{
    message->mutable_id()->set_epoch(epoch);
    message->mutable_id()->set_position(position);

    message->mutable_author()->set_allocated_id(ClubMembershipService::CreateClubMemberId(author).release());

    club::v1::client::ContentChain* contentChain = message->add_content_chain();

    contentChain->set_content(msg.data(), msg.size());
    contentChain->set_edit_time(epoch);
}
}
