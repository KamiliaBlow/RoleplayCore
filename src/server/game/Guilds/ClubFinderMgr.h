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

#ifndef TRINITYCORE_CLUB_FINDER_MGR_H
#define TRINITYCORE_CLUB_FINDER_MGR_H

#include "Define.h"
#include "ObjectGuid.h"
#include <ctime>
#include <string>
#include <unordered_map>
#include <vector>

// ClubFinderRequestType, from the client's own Lua API documentation. Carried as 3 bits on the wire.
enum ClubFinderRequestType : uint8
{
    CLUB_FINDER_REQUEST_TYPE_NONE      = 0,
    CLUB_FINDER_REQUEST_TYPE_GUILD     = 1,
    CLUB_FINDER_REQUEST_TYPE_COMMUNITY = 2,
    CLUB_FINDER_REQUEST_TYPE_ALL       = 3
};

// The 4-bit error selector of SMSG_CLUB_FINDER_ERROR_MESSAGE. Recovered by decompiling the client's
// handler sub_7FF72ACABB30, whose switch maps each value 1:1 onto an ERR_CLUB_FINDER_* global string.
// Values 12, 13 and 15 fall through to a no-op in the client.
enum ClubFinderErrorType : uint8
{
    CLUB_FINDER_ERROR_POST_CLUB                 = 0,
    CLUB_FINDER_ERROR_RESPOND_APPLICANT         = 1,    // client also re-requests the applicant list
    CLUB_FINDER_ERROR_APPLY_CLUB                = 2,
    CLUB_FINDER_ERROR_CANCEL_APPLICATION        = 3,    // client also re-requests the pending list
    CLUB_FINDER_ERROR_ACCEPT_APPLICATION        = 4,    // client also re-requests the pending list
    CLUB_FINDER_ERROR_NO_INVITE_PERMISSIONS     = 5,
    CLUB_FINDER_ERROR_NO_POSTING_PERMISSIONS    = 6,
    CLUB_FINDER_ERROR_APPLICANT_LIST            = 7,
    CLUB_FINDER_ERROR_APPLICANT_LIST_NO_PERM    = 8,
    CLUB_FINDER_ERROR_FINDER_NOT_AVAILABLE      = 9,
    CLUB_FINDER_ERROR_GET_POSTING_IDS           = 10,
    CLUB_FINDER_ERROR_JOIN_APPLICATION          = 11,
    CLUB_FINDER_ERROR_REALM_NOT_ELIGIBLE        = 14
};

// ClubFinderClubPostingStatusFlags. The wire field postingDisplayFlags is a mask of (1 << value):
// C_ClubFinder.GetStatusOfPostingFromClubId walks bits 1..8 and returns the set bit indices, and
// PostClub's validation tests the same stored u32 with & 4 and & 8 for the two forced-change flags.
enum ClubFinderPostingStatusFlag : uint32
{
    CLUB_FINDER_POSTING_FLAG_NEEDS_CACHE_UPDATE       = 1 << 1,
    CLUB_FINDER_POSTING_FLAG_FORCE_DESCRIPTION_CHANGE = 1 << 2,
    CLUB_FINDER_POSTING_FLAG_FORCE_NAME_CHANGE        = 1 << 3,
    CLUB_FINDER_POSTING_FLAG_UNDER_REVIEW             = 1 << 4,
    CLUB_FINDER_POSTING_FLAG_BANNED                   = 1 << 5,
    CLUB_FINDER_POSTING_FLAG_FAKE_POST                = 1 << 6,
    CLUB_FINDER_POSTING_FLAG_PENDING_DELETE           = 1 << 7,
    CLUB_FINDER_POSTING_FLAG_POST_DELISTED            = 1 << 8
};

// ClubFinderSettingFlags, recovered from the client's enum registrar. Both the posting's
// recruitmentFlags and the searcher's applicantSettings are bit-index masks over these values, which
// is why a focus or size filter can be matched directly against a posting's flags.
enum ClubFinderSettingFlag : uint32
{
    CLUB_FINDER_SETTING_DUNGEONS         = 1 << 1,
    CLUB_FINDER_SETTING_RAIDS            = 1 << 2,
    CLUB_FINDER_SETTING_PVP              = 1 << 3,
    CLUB_FINDER_SETTING_RP               = 1 << 4,
    CLUB_FINDER_SETTING_SOCIAL           = 1 << 5,
    CLUB_FINDER_SETTING_SMALL            = 1 << 6,
    CLUB_FINDER_SETTING_MEDIUM           = 1 << 7,
    CLUB_FINDER_SETTING_LARGE            = 1 << 8,
    CLUB_FINDER_SETTING_TANK             = 1 << 9,
    CLUB_FINDER_SETTING_HEALER           = 1 << 10,
    CLUB_FINDER_SETTING_DAMAGE           = 1 << 11,
    CLUB_FINDER_SETTING_ENABLE_LISTING   = 1 << 12,
    CLUB_FINDER_SETTING_MAX_LEVEL_ONLY   = 1 << 13,
    CLUB_FINDER_SETTING_AUTO_ACCEPT      = 1 << 14,
    CLUB_FINDER_SETTING_FACTION_HORDE    = 1 << 15,
    CLUB_FINDER_SETTING_FACTION_ALLIANCE = 1 << 16,
    CLUB_FINDER_SETTING_FACTION_NEUTRAL  = 1 << 17,

    // The masks the client itself slices out when building filters 1, 2 and 4.
    CLUB_FINDER_SETTING_MASK_FOCUS       = 0x3E,    // Dungeons .. Social
    CLUB_FINDER_SETTING_MASK_SIZE        = 0x1C0,   // Small / Medium / Large
    CLUB_FINDER_SETTING_MASK_ROLE        = 0xE00    // Tank / Healer / Damage (bits 9-11)
};

// The client renders a posting as expired after 30 days without an update, and an application after 7
// (Blizzard_Communities/ClubFinder.lua). The server enforces the same windows so its view agrees with
// what players are shown.
constexpr uint32 CLUB_FINDER_POSTING_EXPIRY_DAYS     = 30;
constexpr uint32 CLUB_FINDER_APPLICATION_EXPIRY_DAYS = 7;

// Locale is packed as (locale + 1) into bits 21-25 of a posting's recruitmentFlags, while an
// applicant's locale filter is a bitmask of (1 << WowLocale). Both sides use the same numbering
// (Locale.db2 WowLocale, identical to TrinityCore's LocaleConstant); bit 9 is a hole and bits above
// 11 are unused, so incoming filter values are masked to the legal set.
constexpr uint32 CLUB_FINDER_LOCALE_SHIFT     = 21;
constexpr uint32 CLUB_FINDER_LOCALE_MASK      = 0x1F;
constexpr uint32 CLUB_FINDER_LOCALE_FLAGS_ALL = 0xEFF;

// PlayerClubRequestStatus, from the client's enum registrar at 0x7FF729698C40. Four bits on the wire.
enum ClubFinderApplicationStatus : uint8
{
    CLUB_FINDER_APPLICATION_NONE           = 0,
    CLUB_FINDER_APPLICATION_PENDING        = 1,
    CLUB_FINDER_APPLICATION_AUTO_APPROVED  = 2,
    CLUB_FINDER_APPLICATION_DECLINED       = 3,
    CLUB_FINDER_APPLICATION_APPROVED       = 4,
    CLUB_FINDER_APPLICATION_JOINED         = 5,
    CLUB_FINDER_APPLICATION_JOINED_ANOTHER = 6,
    CLUB_FINDER_APPLICATION_CANCELED       = 7
};

// ClubFinderApplicationUpdateType. DeclineInvite is registered but never emitted by the client - a
// declined invite is sent as Cancel - so both must be treated as "withdraw or decline".
enum ClubFinderApplicationUpdateType : uint8
{
    CLUB_FINDER_APPLICATION_UPDATE_NONE           = 0,
    CLUB_FINDER_APPLICATION_UPDATE_ACCEPT_INVITE  = 1,
    CLUB_FINDER_APPLICATION_UPDATE_DECLINE_INVITE = 2,
    CLUB_FINDER_APPLICATION_UPDATE_CANCEL         = 3
};

// A player's application to a posting.
struct ClubFinderApplication
{
    uint32 PostingId       = 0;
    ObjectGuid PlayerGuid;
    std::string Comment;
    uint64 Specs           = 0;
    uint8 Status           = CLUB_FINDER_APPLICATION_PENDING;
    time_t LastUpdatedTime = 0;
};

// The client accepts 0 and 1 as success in the post response and treats everything else as a failure.
enum ClubFinderPostResult : uint8
{
    CLUB_FINDER_POST_RESULT_OK = 0
};

// A guild's recruitment posting, as the client posts it via C_ClubFinder.PostClub.
struct ClubFinderPosting
{
    uint32 PostingId            = 0;
    uint64 ClubId               = 0;    // the guild id this posting advertises
    std::string Name;
    std::string Description;
    uint64 RecruitingSpecs      = 0;
    uint32 RecruitmentFlags     = 0;    // ClubFinderSettingFlags bit-index mask; locale in bits 21-25
    uint32 ItemLevelRequirement = 0;
    uint32 AvatarId             = 0;
    uint32 DisplayFlags         = 0;    // mask of ClubFinderPostingStatusFlag; moderation state
    uint8 Type                  = CLUB_FINDER_REQUEST_TYPE_GUILD;
    bool CrossFaction           = false;
    ObjectGuid LastPosterGUID;
    time_t LastUpdatedTime      = 0;

    // The posting id is not a wire field of its own: the client reads it back out of the low 32 bits
    // of the clubFinderGUID's high qword. This mints the GUID the client expects.
    //
    // The GUID's type field is an intrinsic club type, NOT an echo of the request that produced it:
    // C_ClubFinder.GetClubTypeFromFinderGUID decodes it as `hi >> 33` and accepts only 1 (Guild) and
    // 2 (Community), returning nothing for anything else. Echoing a request type of All (3) would
    // therefore make the client's own getter fail.
    ObjectGuid GetClubFinderGUID() const;
};

// Registry of guild recruitment postings (Club Finder P0).
//
// Postings are guild-backed: one posting per guild, created and updated by CMSG_CLUB_FINDER_POST.
// Persisted in the character database because guilds live there.
class TC_GAME_API ClubFinderMgr
{
public:
    static ClubFinderMgr* instance();

    void Load();
    void LoadApplications();

    ClubFinderPosting const* GetPosting(uint32 postingId) const;
    ClubFinderPosting const* GetPostingForClub(uint64 clubId) const;

    // Creates or updates the posting for a club and persists it. Returns the stored posting.
    ClubFinderPosting const* SavePosting(ClubFinderPosting posting);

    // Adds moderation flags to a posting and persists them. Used when a posting is reported.
    bool AddPostingDisplayFlags(uint32 postingId, uint32 flags);
    bool RemovePostingDisplayFlags(uint32 postingId, uint32 flags);

    static bool IsPostingExpired(ClubFinderPosting const& posting);
    static bool IsApplicationExpired(ClubFinderApplication const& application);

    // Whether a posting may be shown to a browsing/looking-up player at all: it must not be under a
    // moderation removal (banned / delisted / pending delete), must have its listing enabled, and must
    // not have expired. Search and the direct posting-id lookup share this predicate so a crafted
    // lookup cannot enumerate postings the moderation system hid from search.
    static bool IsPostingVisible(ClubFinderPosting const& posting);

    // True when the text matches a ChatProfanity.db2 row of the realm's language (plus the shared
    // English list). Rows are regular expressions; matching is per-character (wchar_t) and
    // case-insensitive for ASCII and Cyrillic, with the tables' word boundaries rewritten into
    // letter classes so a bounded root matches a standalone word instead of any substring. This is
    // the authoritative check for texts the server stores: the client applies its downloaded
    // dictionary too, but its regex engine cannot fire \b next to non-ASCII characters, so its
    // Cyrillic filtering is unreliable either way.
    static bool IsProfaneText(std::string const& text);

    // All currently listed postings, for the browse responses built on top of this in P1.
    std::vector<ClubFinderPosting const*> GetAllPostings() const;

    // The search criteria the client sends, already decoded out of its filter list.
    struct SearchCriteria
    {
        std::string SearchString;
        uint64 Specs        = 0;    // filter 5: recruiting-spec bitmask
        uint32 ItemLevel    = 0;    // filter 3: the searcher's average item level
        uint32 FocusFlags   = 0;    // filter 1: Dungeons / Raids / PvP / RP / Social
        uint32 SizeFlags    = 0;    // filter 2: Small / Medium / Large
        uint32 LocaleFlags  = 0;    // filter 6: bitmask of (1 << WowLocale)
        uint32 RoleFlags    = 0;    // filter 4: recruited class-role bits (Tank / Healer / Damage)
        uint8 Type          = CLUB_FINDER_REQUEST_TYPE_ALL;

        // Cross-faction search visibility: the searching player's faction, as a TeamId
        // (TEAM_ALLIANCE / TEAM_HORDE). -1 means "no faction filter" (a console/GM search that is not
        // tied to a character). A posting whose guild is the opposite faction is hidden from the
        // search UNLESS the posting advertises cross-faction; same-faction postings are always shown.
        int8 SearcherTeamId = -1;
    };

    std::vector<ClubFinderPosting const*> Search(SearchCriteria const& criteria) const;

    // The client packs a set of specialisations into a uint64 as OR of (1 << bitIndex), where the bit
    // index is the rank of the specialisation's id in the ascending list of every ChrSpecialization
    // row with a non-zero ClassID. Rebuilt here from our own store so the two agree exactly.
    void BuildSpecBitIndex();
    uint64 GetSpecMaskForClass(uint8 classId) const;

    // Applications.
    std::vector<ClubFinderApplication const*> GetApplicationsForPosting(uint32 postingId) const;
    std::vector<ClubFinderApplication const*> GetApplicationsForPlayer(ObjectGuid playerGuid) const;
    ClubFinderApplication const* GetApplication(uint32 postingId, ObjectGuid playerGuid) const;
    ClubFinderApplication const* SaveApplication(ClubFinderApplication application);

private:
    ClubFinderMgr() = default;
    ~ClubFinderMgr() = default;
    ClubFinderMgr(ClubFinderMgr const&) = delete;
    ClubFinderMgr& operator=(ClubFinderMgr const&) = delete;

    std::unordered_map<uint32, ClubFinderPosting> _postings;    // by posting id
    std::unordered_map<uint64, uint32> _postingsByClub;         // club id -> posting id
    std::unordered_map<uint32, uint64> _specMaskByClass;        // class id -> mask of its spec bits
    std::vector<ClubFinderApplication> _applications;
    uint32 _maxPostingId = 0;
};

#define sClubFinderMgr ClubFinderMgr::instance()

#endif // TRINITYCORE_CLUB_FINDER_MGR_H
