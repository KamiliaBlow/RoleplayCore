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
#include "ClubFinderMgr.h"
#include "CharacterCache.h"
#include "DatabaseEnv.h"
#include "DB2Stores.h"
#include "ObjectAccessor.h"
#include "Guild.h"
#include "GuildMgr.h"
#include "Log.h"
#include "Player.h"
#include "WorldSession.h"

// Managing a guild's recruitment (posting, reading its applicant list, responding to and
// whispering applicants) is not leader-only. Retail lets any officer whose guild rank carries the
// recruiter permission act on the posting.
//
// The client and the wire/enum expose no dedicated club-finder/recruiter right, so the guild invite
// right (GR_RIGHT_INVITE) - the same rank right that governs bringing new members into the guild - is
// used as the recruiter-equivalent: managing recruitment is exactly the act of inviting members. The
// guild leader holds the GuildMaster rank (GR_RIGHT_ALL) and therefore always passes; a member whose
// rank lacks the invite right, or a player not in the guild at all, is refused with the existing
// CLUB_FINDER error.
static bool CanManageClubRecruitment(Guild const* guild, Player const* player)
{
    if (!guild || !player)
        return false;

    if (guild->GetLeaderGUID() == player->GetGUID())
        return true;

    Guild::Member const* member = guild->GetMember(player->GetGUID());
    return member && guild->HasAnyRankRight(member->GetRankId(), GuildRankRights(GR_RIGHT_INVITE));
}

// A guild advertises itself for recruitment.
//
// The client clamps Name to 96 and Description to 2048 before sending, and the wire encodes those
// lengths in 7 and 12 bits respectively, so anything that arrives is already within range - the checks
// below are defensive against a hand-crafted packet, not against the real client.
void WorldSession::HandleClubFinderPost(WorldPackets::ClubFinder::ClubFinderPost& clubFinderPost)
{
    Player* player = GetPlayer();
    if (!player)
        return;

    TC_LOG_DEBUG("network", "CMSG_CLUB_FINDER_POST [{}] for club {} (\"{}\")", GetPlayerInfo(), clubFinderPost.ClubId, clubFinderPost.Name);

    // Failures are answered with SMSG_CLUB_FINDER_ERROR_MESSAGE. The 4-bit error selector maps 1:1 onto
    // the client's ERR_CLUB_FINDER_* strings, so these are real, correctly-worded messages.
    auto sendError = [&](uint8 error)
    {
        WorldPackets::ClubFinder::ClubFinderErrorMessage errorMessage;
        errorMessage.Type = clubFinderPost.Type;
        errorMessage.Error = error;
        SendPacket(errorMessage.Write());
    };

    // Only a guild can be posted, and only by someone who speaks for it. The client's own UI gates the
    // button on guild permissions, so a failure here means the request did not come from that UI.
    Guild* guild = sGuildMgr->GetGuildById(player->GetGuildId());
    if (!guild)
    {
        TC_LOG_DEBUG("network", "CMSG_CLUB_FINDER_POST: {} is not in a guild.", GetPlayerInfo());
        sendError(CLUB_FINDER_ERROR_POST_CLUB);
        return;
    }

    if (clubFinderPost.ClubId != guild->GetId())
    {
        TC_LOG_DEBUG("network", "CMSG_CLUB_FINDER_POST: {} tried to post for club {} but is in guild {}.",
            GetPlayerInfo(), clubFinderPost.ClubId, guild->GetId());
        sendError(CLUB_FINDER_ERROR_POST_CLUB);
        return;
    }

    if (!CanManageClubRecruitment(guild, player))
    {
        TC_LOG_DEBUG("network", "CMSG_CLUB_FINDER_POST: {} lacks recruitment permission in guild {}.",
            GetPlayerInfo(), guild->GetId());
        sendError(CLUB_FINDER_ERROR_NO_POSTING_PERMISSIONS);
        return;
    }

    // A community posting advertises a Battle.net club, and this server has none: ClubService supports
    // only guild clubs, so nothing could ever be joined through such a posting. Refusing it is honest;
    // storing it would create an entry that browse silently drops because there is no guild behind it.
    if (clubFinderPost.Type != CLUB_FINDER_REQUEST_TYPE_GUILD)
    {
        TC_LOG_DEBUG("network", "CMSG_CLUB_FINDER_POST: {} posted club type {}, which is not supported.",
            GetPlayerInfo(), clubFinderPost.Type);
        sendError(CLUB_FINDER_ERROR_FINDER_NOT_AVAILABLE);
        return;
    }

    ClubFinderPosting const* current = sClubFinderMgr->GetPostingForClub(clubFinderPost.ClubId);

    // Moderation: a posting flagged for a forced rename or rewrite may not be re-listed until the
    // offending text actually changes. The client enforces this too, but the check has to hold here or
    // it is trivially bypassed. Satisfying it clears the flag.
    if (current)
    {
        uint32 clearedFlags = 0;
        if (current->DisplayFlags & CLUB_FINDER_POSTING_FLAG_FORCE_NAME_CHANGE)
        {
            if (current->Name == clubFinderPost.Name)
            {
                sendError(CLUB_FINDER_ERROR_POST_CLUB);
                return;
            }

            clearedFlags |= CLUB_FINDER_POSTING_FLAG_FORCE_NAME_CHANGE;
        }

        if (current->DisplayFlags & CLUB_FINDER_POSTING_FLAG_FORCE_DESCRIPTION_CHANGE)
        {
            if (current->Description == clubFinderPost.Description)
            {
                sendError(CLUB_FINDER_ERROR_POST_CLUB);
                return;
            }

            clearedFlags |= CLUB_FINDER_POSTING_FLAG_FORCE_DESCRIPTION_CHANGE;
        }

        // A banned posting cannot be revived by editing it.
        if (current->DisplayFlags & CLUB_FINDER_POSTING_FLAG_BANNED)
        {
            sendError(CLUB_FINDER_ERROR_POST_CLUB);
            return;
        }

        if (clearedFlags)
            sClubFinderMgr->RemovePostingDisplayFlags(current->PostingId, clearedFlags);
    }

    // The authoritative dictionary gate on the description. The client checks its downloaded word
    // list too, but its regex engine cannot fire \b next to non-ASCII characters, so its Cyrillic
    // filtering is unreliable; this server's ChatProfanity table decides what gets stored.
    if (ClubFinderMgr::IsProfaneText(clubFinderPost.Description))
    {
        TC_LOG_DEBUG("network", "CMSG_CLUB_FINDER_POST: {} used a forbidden word in the description.",
            GetPlayerInfo());
        sendError(CLUB_FINDER_ERROR_POST_CLUB);
        return;
    }

    ClubFinderPosting posting;
    posting.ClubId               = clubFinderPost.ClubId;
    posting.Name                 = clubFinderPost.Name;
    posting.Description          = clubFinderPost.Description;
    // The 12.1 client's editor cannot round-trip the spec mask into its dropdown (the
    // record's specIds arrive as {0}), so an unedited dialog submits an empty mask. An
    // empty mask must not wipe the stored recruitment specs.
    posting.RecruitingSpecs      = !clubFinderPost.RecruitingSpecs && current
        ? current->RecruitingSpecs
        : clubFinderPost.RecruitingSpecs;
    posting.RecruitmentFlags     = clubFinderPost.RecruitmentFlags;
    posting.ItemLevelRequirement = clubFinderPost.ItemLevelRequirement;
    posting.AvatarId             = clubFinderPost.AvatarId;
    posting.Type                 = clubFinderPost.Type;
    posting.CrossFaction         = clubFinderPost.CrossFaction;
    posting.LastPosterGUID       = player->GetGUID();

    ClubFinderPosting const* stored = sClubFinderMgr->SavePosting(std::move(posting));
    if (!stored)
    {
        sendError(CLUB_FINDER_ERROR_POST_CLUB);
        return;
    }

    WorldPackets::ClubFinder::ClubFinderResponsePostRecruitmentMessage response;
    response.ClubFinderGUID = stored->GetClubFinderGUID();
    response.Result = current ? 1 : 0;  // 0 = created, 1 = updated
    response.Unused = 1;                // 1 in every captured retail reply

    SendPacket(response.Write());

    TC_LOG_INFO("network", "ClubFinder: {} posted guild {} as posting {} (\"{}\").",
        GetPlayerInfo(), stored->ClubId, stored->PostingId, stored->Name);
}

// The client resolves a guild finder record's emblem style through its GuildTabardEmblem store, whose
// keys are the table's EmblemID column (not the primary ID), and drops the whole tabard when the lookup
// misses. Mirror that key space here so hotfixed tables keep working without a hardcoded range.
static bool IsValidEmblemStyle(uint32 emblemId)
{
    // The packed wire slot gives the style 8 bits and the value is stored +1.
    if (emblemId > 0xFE)
        return false;
    for (GuildTabardEmblemEntry const* emblem : sGuildTabardEmblemStore)
        if (emblem->EmblemID == emblemId)
            return true;
    return false;
}

// Bit layout of the finder record's tabardInfo slot: a 32-bit packet of 1-based table indices
// decoded by the client's packed-tabard unpacker - [EmblemID:8][emblemColor:6][unused:6]
// [borderColor:6][backgroundColor:6]. The layout lives in the client binary, not in DB2: table
// changes can add or remove rows (validity is checked through the stores above) but cannot move
// the fields; only a client patch changes it, together with the rest of the packet format.
enum PackedTabardShift
{
    PACKED_TABARD_EMBLEM_STYLE_SHIFT    = 24,
    PACKED_TABARD_EMBLEM_COLOR_SHIFT    = 18,
    PACKED_TABARD_UNUSED_SHIFT          = 12,   // six bits the client ignores
    PACKED_TABARD_BORDER_COLOR_SHIFT    = 6,
    // background color occupies the low six bits
};

static uint32 PackGuildTabard(EmblemInfo const& emblem)
{
    return ((emblem.GetStyle() + 1) << PACKED_TABARD_EMBLEM_STYLE_SHIFT)
        |  ((emblem.GetColor() + 1) << PACKED_TABARD_EMBLEM_COLOR_SHIFT)
        |  ((emblem.GetBorderColor() + 1) << PACKED_TABARD_BORDER_COLOR_SHIFT)
        |  (emblem.GetBackgroundColor() + 1);
}

// Fills one browse record from a stored posting. The counts and the leader name are read from the live
// guild rather than cached on the posting, so a browsing player sees the guild's real current state.
// isOwnPosting relaxes only the listing/expiry parts of the visibility rule: a guild member's client
// refreshes the recruitment editor and the posting-expiry readout from this record, and a delisted
// posting must reach them with its real flags so the readout disappears immediately (the client's own
// Lua drops the countdown when the record's listing flag is off - an empty response instead leaves its
// cache stale until relog). Moderation removals still hide the posting even from its own guild.
static bool BuildClubCacheData(ClubFinderPosting const& posting, WorldPackets::ClubFinder::ClubFinderLookupClubPostingsList::ClubCacheData& data, bool isOwnPosting = false)
{
    if (isOwnPosting)
    {
        if (posting.DisplayFlags & (CLUB_FINDER_POSTING_FLAG_POST_DELISTED | CLUB_FINDER_POSTING_FLAG_PENDING_DELETE | CLUB_FINDER_POSTING_FLAG_BANNED))
            return false;
    }
    // A direct REQUEST_CLUBS_DATA lookup names posting ids straight out and must not become a way to
    // read the postings moderation hid from search: apply the same visibility predicate Search uses, so
    // a crafted request cannot enumerate banned, delisted, pending-delete, unlisted or expired postings.
    else if (!ClubFinderMgr::IsPostingVisible(posting))
        return false;

    Guild* guild = sGuildMgr->GetGuildById(posting.ClubId);
    if (!guild)
        return false;

    data.ClubName         = posting.Name;
    data.Comment          = posting.Description;
    data.ClubFinderGUID   = posting.GetClubFinderGUID();
    data.LastPosterGUID   = posting.LastPosterGUID;
    data.RecruitingSpecs  = posting.RecruitingSpecs;
    data.ClubID           = posting.ClubId;
    data.LastUpdatedTime  = posting.LastUpdatedTime;
    data.NumActiveMembers = guild->GetMembersCount();
    // Guild and community records share this slot with different meanings: a GUILD record carries
    // a packed tabard (PackGuildTabard); a COMMUNITY record carries the chosen avatar id raw
    // (rendered via SetAvatarTexture). A zero top byte makes the client read a guild value as an
    // avatar id instead, and a GuildTabardEmblem lookup miss drops the whole tabard, so an
    // unknown style sends 0.
    if (posting.Type == CLUB_FINDER_REQUEST_TYPE_GUILD)
    {
        EmblemInfo const& emblem = guild->GetEmblemInfo();
        data.TabardInfo = IsValidEmblemStyle(emblem.GetStyle()) ? PackGuildTabard(emblem) : 0;
    }
    else
        data.TabardInfo = posting.AvatarId;
    data.RecruitmentFlags = int32(posting.RecruitmentFlags);
    data.MinIlvl          = int32(posting.ItemLevelRequirement);
    data.CrossFaction     = posting.CrossFaction;
    // Same-realm postings carry no realm name on the wire (the field is the client's
    // cross-realm display; retail leaves it empty for the searcher's own realm).

    // The wire field is the guild leader's name; the posting only stores who last edited it.
    sCharacterCache->GetCharacterNameByGuid(guild->GetLeaderGUID(), data.GuildLeader);

    return true;
}

// The client asks which of the clubs it knows about currently have a posting.
void WorldSession::HandleClubFinderRequestSubscribedClubPostingIDs(WorldPackets::ClubFinder::ClubFinderRequestSubscribedClubPostingIds& request)
{
    TC_LOG_DEBUG("network", "CMSG_CLUB_FINDER_REQUEST_SUBSCRIBED_CLUB_POSTING_IDS [{}]: ClubIds: {}",
        GetPlayerInfo(), uint32(request.ClubIds.size()));

    // One answer packet per request: a u32 count plus one { club id, guild id } record per
    // subscribed club that has a posting. The client keys its club-to-posting map off these
    // pairs; an empty or malformed map greys out the applicant tabs.
    WorldPackets::ClubFinder::ClubFinderGetClubPostingIdsResponse response;

    for (uint64 clubId : request.ClubIds)
    {
        ClubFinderPosting const* posting = sClubFinderMgr->GetPostingForClub(clubId);
        if (!posting)
            continue;

        WorldPackets::ClubFinder::ClubFinderGetClubPostingIdsResponse::ClubPostingClubIDMap& entry = response.PostingIds.emplace_back();
        entry.ClubID  = clubId;
        entry.GuildID = posting->ClubId;
    }

    SendPacket(response.Write());
}

// The client asks for the full posting records behind a set of posting ids.
void WorldSession::HandleClubFinderRequestClubsData(WorldPackets::ClubFinder::ClubFinderRequestClubsData& request)
{
    TC_LOG_DEBUG("network", "CMSG_CLUB_FINDER_REQUEST_CLUBS_DATA [{}]: PostingIds: {}, Type: {}, LinkedLookup: {}",
        GetPlayerInfo(), uint32(request.ClubPostingIDs.size()), uint32(request.Type), request.LinkedLookup);

    WorldPackets::ClubFinder::ClubFinderLookupClubPostingsList response;

    // Both envelope fields are echoes of the request.
    response.Type = request.Type;
    response.LinkedLookup = request.LinkedLookup;

    for (uint32 clubPostingId : request.ClubPostingIDs)
    {
        // The client asks by CLUB (guild) id: browse requests echo the guild ids the search
        // returned, and the own-posting fetch after a subscribed-ids answer asks for the guild
        // id from that map. Resolve by guild first; the posting-id fallback only covers a
        // client still holding ids from an older server build.
        ClubFinderPosting const* posting = sClubFinderMgr->GetPostingForClub(clubPostingId);
        if (!posting)
            posting = sClubFinderMgr->GetPosting(clubPostingId);
        if (!posting)
            continue;

        // The guild's own members fetch their posting to drive the editor and the expiry readout;
        // everyone else goes through the full search visibility rule.
        Player* player = GetPlayer();
        bool isOwnPosting = player && player->GetGuildId() == posting->ClubId;

        WorldPackets::ClubFinder::ClubFinderLookupClubPostingsList::ClubCacheData& data = response.Postings.emplace_back();
        if (!BuildClubCacheData(*posting, data, isOwnPosting))
            response.Postings.pop_back();
    }

    SendPacket(response.Write());
    TC_LOG_DEBUG("network", "SMSG_CLUB_FINDER_LOOKUP_CLUB_POSTINGS_LIST [{}]: {} record(s)",
        GetPlayerInfo(), response.Postings.size());
}


// Decodes the client's filter list into search criteria. Filter types 1, 2, 3, 4, 5 and 6 all map onto
// data the posting carries: 1/2/4 are bit groups of the posting's recruitmentFlags, 6 its packed
// recruitment locale.
static void ApplySearchFilters(std::vector<WorldPackets::ClubFinder::ClubFinderPostingFilter> const& filters,
    ClubFinderMgr::SearchCriteria& criteria)
{
    for (WorldPackets::ClubFinder::ClubFinderPostingFilter const& filter : filters)
    {
        switch (filter.Type)
        {
            case 1:     // focus flags, same bit space as the posting's recruitmentFlags
                criteria.FocusFlags = filter.UintValue;
                break;
            case 2:     // guild size flags, likewise
                criteria.SizeFlags = filter.UintValue;
                break;
            case 3:     // the searching player's average item level
                criteria.ItemLevel = filter.UintValue;
                break;
            case 5:     // specialization bitmask
                criteria.Specs = filter.Uint64Value;
                break;
            case 4:
                // Captured 12.1 searches send { type 4, value = player level } next to the item level
                // filter (e.g. level 15 with average ilvl 8). It is NOT a recruitmentFlags role mask:
                // reading it as role bits (bits 9-11) rejected every posting, because a level never
                // has those bits set, so the search always came back empty. Postings carry no level
                // range, so there is nothing to match here.
                break;
            case 6:     // applicant locale flags, a bitmask of (1 << WowLocale). The client applies no
                            // validation to this value, so it is masked to the legal locale set here.
                criteria.LocaleFlags = filter.UintValue & CLUB_FINDER_LOCALE_FLAGS_ALL;
                break;
            default:
                break;
        }
    }
}

// The Club Finder search. The answer is the complete list of matching posting IDS, not the postings
// themselves: the client stores that list, derives its page count from its length, and then asks for
// the records of each page through CMSG_CLUB_FINDER_REQUEST_CLUBS_DATA. Truncating the list here would
// silently shrink the client page count, so every match is sent.
void WorldSession::HandleClubFinderRequestClubsList(WorldPackets::ClubFinder::ClubFinderRequestClubsList& request)
{
    TC_LOG_DEBUG("network", "CMSG_CLUB_FINDER_REQUEST_CLUBS_LIST [{}]: Type: {}, Search: \"{}\", Filters: {}",
        GetPlayerInfo(), uint32(request.Type), request.SearchString, uint32(request.Filters.size()));

    ClubFinderMgr::SearchCriteria criteria;
    criteria.SearchString = request.SearchString;
    criteria.Type = request.Type;

    // The searcher's faction drives cross-faction search visibility - an opposite-faction posting
    // is hidden from search unless its guild advertises cross-faction. Apply/accept remain ungated;
    // this is a search-visibility filter only.
    if (Player* player = GetPlayer())
        criteria.SearcherTeamId = int8(player->GetTeamId());

    ApplySearchFilters(request.Filters, criteria);

    WorldPackets::ClubFinder::ClubFinderReturnRecruitingClubs response;
    response.Type = request.Type;

    for (ClubFinderPosting const* posting : sClubFinderMgr->Search(criteria))
        response.ClubPostingIDs.push_back(posting->ClubId);

    TC_LOG_DEBUG("network", "SMSG_CLUB_FINDER_RETURN_RECRUITING_CLUBS [{}]: {} posting(s) matched",
        GetPlayerInfo(), response.ClubPostingIDs.size());

    SendPacket(response.Write());
}

// ---------------------------------------------------------------------------------------------
// Applications
// ---------------------------------------------------------------------------------------------

// The posting a clubFinderGUID refers to.
static ClubFinderPosting const* GetPostingFromGUID(ObjectGuid const& clubFinderGUID)
{
    // Posting id is the low dword of the HIGH qword (see ClubFinderPosting::GetClubFinderGUID).
    return sClubFinderMgr->GetPosting(uint32(clubFinderGUID.GetRawValue(1) & 0xFFFFFFFF));
}

static void FillApplicationList(WorldPackets::ClubFinder::ClubFinderApplicationList& packet,
    std::vector<ClubFinderApplication const*> const& applications)
{
    for (ClubFinderApplication const* application : applications)
    {
        ClubFinderPosting const* posting = sClubFinderMgr->GetPosting(application->PostingId);
        if (!posting)
            continue;

        // The client treats an application older than a week as expired; do not list it as live.
        if (application->Status == CLUB_FINDER_APPLICATION_PENDING && ClubFinderMgr::IsApplicationExpired(*application))
            continue;

        WorldPackets::ClubFinder::ClubFinderApplicationList::PendingApplication& entry = packet.Applications.emplace_back();
        entry.ClubFinderGUID    = posting->GetClubFinderGUID();
        entry.PlayerGUID        = application->PlayerGuid;
        entry.LastUpdatedTime   = application->LastUpdatedTime;
        entry.ApplicationStatus = application->Status;

        // Retail keeps this slot 0 in every captured state (pending, approved, joined) - the
        // status byte alone drives which list an entry lands in.
        entry.Closed = 0;
    }
}

void WorldSession::SendClubFinderPendingApplications(uint8 type)
{
    WorldPackets::ClubFinder::ClubFinderApplicationList response(SMSG_CLUB_FINDER_RESPONSE_CHARACTER_APPLICATION_LIST);
    response.Type = type;
    FillApplicationList(response, sClubFinderMgr->GetApplicationsForPlayer(GetPlayer()->GetGUID()));
    SendPacket(response.Write());
}

// A player applies to a guild posting.
void WorldSession::HandleClubFinderRequestMembershipToClub(WorldPackets::ClubFinder::ClubFinderRequestMembershipToClub& request)
{
    Player* player = GetPlayer();
    if (!player)
        return;

    TC_LOG_DEBUG("network", "CMSG_CLUB_FINDER_REQUEST_MEMBERSHIP_TO_CLUB [{}]: ClubFinderGUID: {}",
        GetPlayerInfo(), request.ClubFinderGUID.ToString());

    auto sendError = [&](uint8 error)
    {
        WorldPackets::ClubFinder::ClubFinderErrorMessage errorMessage;
        errorMessage.Type = CLUB_FINDER_REQUEST_TYPE_GUILD;
        errorMessage.Error = error;
        SendPacket(errorMessage.Write());
    };

    ClubFinderPosting const* posting = GetPostingFromGUID(request.ClubFinderGUID);
    if (!posting)
    {
        sendError(CLUB_FINDER_ERROR_APPLY_CLUB);
        return;
    }

    // Applying to your own guild is meaningless, and IsPostingVisible also rejects a
    // delisted, expired or moderation-removed posting, so a stale clubFinderGUID cannot
    // lodge an application against it.
    if (player->GetGuildId() == posting->ClubId || !ClubFinderMgr::IsPostingVisible(*posting))
    {
        sendError(CLUB_FINDER_ERROR_APPLY_CLUB);
        return;
    }

    ClubFinderApplication application;
    application.PostingId  = posting->PostingId;
    application.PlayerGuid = player->GetGUID();
    application.Comment    = request.Comment.substr(0, 512);
    application.Specs      = request.RecruitingSpecs;
    application.Status     = CLUB_FINDER_APPLICATION_PENDING;

    // An auto-accept guild admits the applicant without the officer step, but only when the
    // member add actually succeeds; otherwise the application stays PENDING so an officer
    // can still act on it. A player already in a guild is simply left pending.
    if (posting->RecruitmentFlags & CLUB_FINDER_SETTING_AUTO_ACCEPT)
    {
        Guild* guild = sGuildMgr->GetGuildById(posting->ClubId);
        if (guild && !player->GetGuildId())
        {
            CharacterDatabaseTransaction trans = CharacterDatabase.BeginTransaction();
            if (guild->AddMember(trans, player->GetGUID()))
            {
                CharacterDatabase.CommitTransaction(trans);
                application.Status = CLUB_FINDER_APPLICATION_JOINED;
            }
        }
    }

    sClubFinderMgr->SaveApplication(std::move(application));

    // Answer with a single-record UPDATE_APPLICATIONS echoing the new application; the
    // pending list arrives with the client's next poll.
    {
        ClubFinderApplication const* storedApplication = sClubFinderMgr->GetApplication(posting->PostingId, player->GetGUID());
        if (storedApplication)
        {
            WorldPackets::ClubFinder::ClubFinderApplicationList update(SMSG_CLUB_FINDER_UPDATE_APPLICATIONS);
            WorldPackets::ClubFinder::ClubFinderApplicationList::PendingApplication& record = update.Applications.emplace_back();
            record.ClubFinderGUID    = posting->GetClubFinderGUID();
            record.PlayerGUID        = storedApplication->PlayerGuid;
            record.LastUpdatedTime   = storedApplication->LastUpdatedTime;
            record.ApplicationStatus = storedApplication->Status;
            update.Type = CLUB_FINDER_REQUEST_TYPE_GUILD;
            SendPacket(update.Write());
        }
    }

    TC_LOG_INFO("network", "ClubFinder: {} applied to posting {} (guild {}).",
        GetPlayerInfo(), posting->PostingId, posting->ClubId);
}

// A guild officer asks for the applicants to their own posting. The request carries no club GUID, so
// the posting is resolved from the sender guild membership. Answered with exactly one
// SMSG_RETURN_APPLICANT_LIST and nothing else: the client resolves applicant names itself and
// fetches the posting record itself when it needs one.
void WorldSession::HandleClubFinderGetApplicantsList(WorldPackets::ClubFinder::ClubFinderGetApplicantsList& request)
{
    Player* player = GetPlayer();
    if (!player)
        return;

    // The polls are the natural heartbeat for application retention; the cleanup throttles
    // itself to one pass per hour.
    sClubFinderMgr->CleanupApplications();

    WorldPackets::ClubFinder::ClubFinderApplicantsList response;
    response.Type = request.Type;

    Guild* guild = sGuildMgr->GetGuildById(player->GetGuildId());
    ClubFinderPosting const* posting = guild ? sClubFinderMgr->GetPostingForClub(guild->GetId()) : nullptr;

    // No posting, or no authority over it: an empty list is the truthful answer, and never another
    // guild's applicants. Any officer with the recruiter (invite) right may read it, not just the leader.
    if (!posting || !CanManageClubRecruitment(guild, player))
    {
        SendPacket(response.Write());
        return;
    }

    response.ClubFinderGUID = posting->GetClubFinderGUID();

    for (ClubFinderApplication const* application : sClubFinderMgr->GetApplicationsForPosting(posting->PostingId))
    {
        // The client treats an application older than a week as expired; do not list it as live.
        if (application->Status == CLUB_FINDER_APPLICATION_PENDING && ClubFinderMgr::IsApplicationExpired(*application))
            continue;

        CharacterCacheEntry const* character = sCharacterCache->GetCharacterCacheByGuid(application->PlayerGuid);
        if (!character)
            continue;

        WorldPackets::ClubFinder::ClubFinderApplicantsList::Applicant& entry = response.Applicants.emplace_back();
        entry.ClubFinderGUID   = posting->GetClubFinderGUID();
        entry.PlayerGUID       = application->PlayerGuid;
        // Retail sends no name body and a -1 item level here; level comes from the character
        // cache, everything else the client resolves through its own name queries.
        entry.Message          = application->Comment;
        entry.Level            = character->Level;
        entry.RecruitingSpecs  = application->Specs;
        entry.LastUpdatedTime  = application->LastUpdatedTime;
        entry.RequestStatus    = application->Status;
        // A live application goes to ReturnClubApplicantList (Closed = 0); everything else is
        // history and only shows in ReturnPendingClubApplicantList.
        entry.Closed           = application->Status != CLUB_FINDER_APPLICATION_PENDING;
    }

    SendPacket(response.Write());
}

void WorldSession::HandleClubFinderRequestPendingClubsList(WorldPackets::ClubFinder::ClubFinderRequestPendingClubsList& request)
{
    if (!GetPlayer())
        return;

    // Every player's client polls this on login and periodically while the guild UI is open; see
    // the matching call in HandleClubFinderGetApplicantsList.
    sClubFinderMgr->CleanupApplications();

    SendClubFinderPendingApplications(request.Type);
}

// A guild officer accepts or declines an applicant.
void WorldSession::HandleClubFinderRespondToApplicant(WorldPackets::ClubFinder::ClubFinderRespondToApplicant& request)
{
    Player* player = GetPlayer();
    if (!player)
        return;

    TC_LOG_DEBUG("network", "CMSG_CLUB_FINDER_RESPOND_TO_APPLICANT [{}]: ClubFinderGUID: {}, Player: {}, Accept: {}",
        GetPlayerInfo(), request.ClubFinderGUID.ToString(), request.PlayerGUID.ToString(), request.ShouldAccept);

    auto sendError = [&](uint8 error)
    {
        WorldPackets::ClubFinder::ClubFinderErrorMessage errorMessage;
        errorMessage.Type = request.Type;
        errorMessage.Error = error;
        SendPacket(errorMessage.Write());
    };

    ClubFinderPosting const* posting = GetPostingFromGUID(request.ClubFinderGUID);
    Guild* guild = posting ? sGuildMgr->GetGuildById(posting->ClubId) : nullptr;
    if (!posting || !guild)
    {
        sendError(CLUB_FINDER_ERROR_RESPOND_APPLICANT);
        return;
    }

    if (!CanManageClubRecruitment(guild, player))
    {
        sendError(CLUB_FINDER_ERROR_NO_INVITE_PERMISSIONS);
        return;
    }

    ClubFinderApplication const* existing = sClubFinderMgr->GetApplication(posting->PostingId, request.PlayerGUID);
    if (!existing)
    {
        sendError(CLUB_FINDER_ERROR_RESPOND_APPLICANT);
        return;
    }

    // Consent guard: only a still-live request may be acted on - a withdrawn, declined,
    // already-joined or expired application must not admit the player against their consent.
    if ((existing->Status != CLUB_FINDER_APPLICATION_PENDING && existing->Status != CLUB_FINDER_APPLICATION_AUTO_APPROVED)
        || ClubFinderMgr::IsApplicationExpired(*existing))
    {
        sendError(CLUB_FINDER_ERROR_RESPOND_APPLICANT);
        return;
    }

    ClubFinderApplication updated = *existing;
    updated.Status = request.ShouldAccept ? CLUB_FINDER_APPLICATION_APPROVED : CLUB_FINDER_APPLICATION_DECLINED;

    // The accept does not admit the player on the spot and does not care whether the applicant
    // belongs to another guild: the application moves to APPROVED and the client turns it into
    // an invitation card, whose Accept button it disables while the player is in a guild - the
    // invitation simply waits for them to leave. (Downgrading the status here would render
    // nothing at all in the applicant's own list.)

    sClubFinderMgr->SaveApplication(std::move(updated));

    // Retail answers the decision with a single-record UPDATE_APPLICATIONS back to the acting
    // officer. The applicant learns of the approval through their next 14D poll (the E2 record
    // with status APPROVED turns into the invitation card), so the applicant session is never
    // pushed here.
    {
        ClubFinderApplication const* storedApplication = sClubFinderMgr->GetApplication(posting->PostingId, request.PlayerGUID);
        if (storedApplication)
        {
            WorldPackets::ClubFinder::ClubFinderApplicationList update(SMSG_CLUB_FINDER_UPDATE_APPLICATIONS);
            WorldPackets::ClubFinder::ClubFinderApplicationList::PendingApplication& record = update.Applications.emplace_back();
            record.ClubFinderGUID    = posting->GetClubFinderGUID();
            record.PlayerGUID        = storedApplication->PlayerGuid;
            record.LastUpdatedTime   = storedApplication->LastUpdatedTime;
            record.ApplicationStatus = storedApplication->Status;
            update.Type = request.Type;
            SendPacket(update.Write());
        }
    }

    TC_LOG_INFO("network", "ClubFinder: {} responded to applicant {} for posting {} (accepted: {}).",
        GetPlayerInfo(), request.PlayerGUID.ToString(), posting->PostingId, request.ShouldAccept);
}

// The applicant accepts an invite, or withdraws. DeclineInvite is never emitted by the client - a
// declined invite arrives as Cancel - so both are handled as a withdrawal.
// The applicant answers an invitation (accept / decline), or withdraws their request. DeclineInvite
// is never emitted by the client - a declined invite arrives as Cancel - so both are handled as a
// withdrawal. Accepting is the only step that actually admits the member: the officer's accept
// merely marked the application APPROVED and delivered the invitation (see
// HandleClubFinderRespondToApplicant).
void WorldSession::HandleClubFinderApplicationResponse(WorldPackets::ClubFinder::ClubFinderApplicationResponse& request)
{
    Player* player = GetPlayer();
    if (!player)
        return;

    TC_LOG_DEBUG("network", "CMSG_CLUB_FINDER_APPLICATION_RESPONSE [{}]: ClubFinderGUID: {}, UpdateType: {}",
        GetPlayerInfo(), request.ClubFinderGUID.ToString(), uint32(request.UpdateType));

    ClubFinderPosting const* posting = GetPostingFromGUID(request.ClubFinderGUID);
    if (!posting)
        return;

    ClubFinderApplication const* existing = sClubFinderMgr->GetApplication(posting->PostingId, player->GetGUID());
    if (!existing)
        return;

    ClubFinderApplication updated = *existing;
    switch (request.UpdateType)
    {
        case CLUB_FINDER_APPLICATION_UPDATE_ACCEPT_INVITE:
        {
            // Consent guard: only an unexpired invitation the guild actually extended may be
            // accepted into membership. Anything else keeps its current status.
            if (existing->Status != CLUB_FINDER_APPLICATION_APPROVED
                && existing->Status != CLUB_FINDER_APPLICATION_AUTO_APPROVED)
                return;

            if (ClubFinderMgr::IsApplicationExpired(*existing))
                return;

            // Only reachable through a hand-crafted packet (the client disables Accept while
            // guilded): leave the application APPROVED so the invitation keeps waiting.
            if (player->GetGuildId())
            {
                TC_LOG_DEBUG("network", "ClubFinder: {} tried to accept an invitation while still in guild {}.",
                    GetPlayerInfo(), player->GetGuildId());
                return;
            }

            Guild* guild = sGuildMgr->GetGuildById(posting->ClubId);
            if (!guild)
                return;

            CharacterDatabaseTransaction trans = CharacterDatabase.BeginTransaction();
            if (guild->AddMember(trans, player->GetGUID()))
            {
                CharacterDatabase.CommitTransaction(trans);
                updated.Status = CLUB_FINDER_APPLICATION_JOINED;
            }
            else
                updated.Status = CLUB_FINDER_APPLICATION_DECLINED; // the guild cannot currently add the player
            break;
        }
        case CLUB_FINDER_APPLICATION_UPDATE_DECLINE_INVITE:
        case CLUB_FINDER_APPLICATION_UPDATE_CANCEL:
            updated.Status = CLUB_FINDER_APPLICATION_CANCELED;
            break;
        default:
            return;
    }

    sClubFinderMgr->SaveApplication(std::move(updated));

    // Answer with a single-record UPDATE_APPLICATIONS carrying the final status; the client
    // re-polls its pending list right after this packet.
    {
        ClubFinderApplication const* storedApplication = sClubFinderMgr->GetApplication(posting->PostingId, player->GetGUID());
        if (storedApplication)
        {
            WorldPackets::ClubFinder::ClubFinderApplicationList update(SMSG_CLUB_FINDER_UPDATE_APPLICATIONS);
            WorldPackets::ClubFinder::ClubFinderApplicationList::PendingApplication& record = update.Applications.emplace_back();
            record.ClubFinderGUID    = posting->GetClubFinderGUID();
            record.PlayerGUID        = storedApplication->PlayerGuid;
            record.LastUpdatedTime   = storedApplication->LastUpdatedTime;
            record.ApplicationStatus = storedApplication->Status;
            update.Type = request.Type;
            SendPacket(update.Write());
        }
    }
}

// An officer asks whether they may whisper an applicant. Answering opens the whisper window client
// side, so it is gated on the officer actually owning the posting and the target actually having
// applied to it.
void WorldSession::HandleClubFinderWhisperApplicantRequest(WorldPackets::ClubFinder::ClubFinderWhisperApplicantRequest& request)
{
    Player* player = GetPlayer();
    if (!player)
        return;

    TC_LOG_DEBUG("network", "CMSG_CLUB_FINDER_WHISPER_APPLICANT_REQUEST [{}]: ClubFinderGUID: {}, Player: {}",
        GetPlayerInfo(), request.ClubFinderGUID.ToString(), request.PlayerGUID.ToString());

    ClubFinderPosting const* posting = GetPostingFromGUID(request.ClubFinderGUID);
    Guild* guild = posting ? sGuildMgr->GetGuildById(posting->ClubId) : nullptr;
    if (!posting || !guild || !CanManageClubRecruitment(guild, player))
        return;

    if (!sClubFinderMgr->GetApplication(posting->PostingId, request.PlayerGUID))
        return;

    WorldPackets::ClubFinder::ClubFinderWhisperApplicantResponse response;
    response.ClubFinderGUID = request.ClubFinderGUID;
    response.PlayerGUID = request.PlayerGUID;
    SendPacket(response.Write());
}
