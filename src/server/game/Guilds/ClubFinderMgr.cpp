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

#include "ClubFinderMgr.h"
#include "CharacterCache.h"
#include "DatabaseEnv.h"
#include "DB2Stores.h"
#include "Common.h"
#include "GameTime.h"
#include "Guild.h"
#include "GuildMgr.h"
#include "Log.h"
#include "Player.h"
#include "Timer.h"
#include "World.h"
#include <algorithm>
#include <cctype>
#include <regex>

ObjectGuid ClubFinderPosting::GetClubFinderGUID() const
{
    // Retail layout (verified against a live 12.0.x capture): posting id in the high qword's low
    // dword with a 0x05 type tag at bits 32..34, club id in the low qword. The client decodes the
    // type via hi >> 33; the factory's realm<<42 bit makes that decode fail and the record is
    // dropped, so the posting GUID must be minted with CreateClubFinderPosting instead.
    return ObjectGuidFactory::CreateClubFinderPosting(PostingId, ClubId);
}

ClubFinderMgr* ClubFinderMgr::instance()
{
    static ClubFinderMgr instance;
    return &instance;
}

void ClubFinderMgr::Load()
{
    uint32 oldMSTime = getMSTime();

    _postings.clear();
    _postingsByClub.clear();
    _maxPostingId = 0;

    //                                                       0          1      2            3
    QueryResult result = CharacterDatabase.Query("SELECT postingId, clubId, name, description, "
    //   4                5                 6                     7         8      9             10
        "recruitingSpecs, recruitmentFlags, itemLevelRequirement, avatarId, displayFlags, type, crossFaction, lastPosterGuid, "
    //   11
        "lastUpdatedTime FROM club_finder_posting");

    if (!result)
    {
        TC_LOG_INFO("server.loading", ">> Loaded 0 club finder postings. The table is empty.");
        return;
    }

    do
    {
        Field* fields = result->Fetch();

        ClubFinderPosting posting;
        posting.PostingId            = fields[0].GetUInt32();
        posting.ClubId               = fields[1].GetUInt64();
        posting.Name                 = fields[2].GetString();
        posting.Description          = fields[3].GetString();
        posting.RecruitingSpecs      = fields[4].GetUInt64();
        posting.RecruitmentFlags     = fields[5].GetUInt32();
        posting.ItemLevelRequirement = fields[6].GetUInt32();
        posting.AvatarId             = fields[7].GetUInt32();
        posting.DisplayFlags         = fields[8].GetUInt32();
        posting.Type                 = fields[9].GetUInt8();
        posting.CrossFaction         = fields[10].GetBool();
        posting.LastPosterGUID       = ObjectGuid::Create<HighGuid::Player>(fields[11].GetUInt64());
        posting.LastUpdatedTime      = fields[12].GetInt64();

        _maxPostingId = std::max(_maxPostingId, posting.PostingId);
        _postingsByClub[posting.ClubId] = posting.PostingId;
        _postings[posting.PostingId] = std::move(posting);
    }
    while (result->NextRow());

    TC_LOG_INFO("server.loading", ">> Loaded {} club finder postings in {} ms", _postings.size(), GetMSTimeDiffToNow(oldMSTime));

    LoadApplications();
    BuildSpecBitIndex();

    // Drop rows past their retention right at startup so the first polls of the day already see a
    // clean list (CleanupApplications runs unconditionally on its first call after load).
    CleanupApplications();
}

void ClubFinderMgr::CleanupApplications()
{
    // Purge at most once per hour; the club finder poll handlers call this on every poll, and a
    // retention pass only ever needs to run a few times a day.
    static time_t const purgeInterval = time_t(HOUR);
    static time_t lastPurge = 0;

    time_t const now = GameTime::GetGameTime();
    if (lastPurge && now - lastPurge < purgeInterval)
        return;
    lastPurge = now;

    time_t const retentionCutoff = now - time_t(CLUB_FINDER_APPLICATION_RETENTION_DAYS) * DAY;
    time_t const expiryCutoff = now - time_t(CLUB_FINDER_APPLICATION_EXPIRY_DAYS) * DAY;

    // Active statuses (no verdict yet) lapse with the application expiry; every other status is a
    // verdict and survives until the retention cutoff. Orphans of deleted postings go immediately.
    CharacterDatabasePreparedStatement* stmt = CharacterDatabase.GetPreparedStatement(CHAR_DEL_CLUB_FINDER_APPLICATIONS_EXPIRED);
    stmt->setInt64(0, retentionCutoff);
    stmt->setUInt8(1, CLUB_FINDER_APPLICATION_PENDING);
    stmt->setUInt8(2, CLUB_FINDER_APPLICATION_AUTO_APPROVED);
    stmt->setUInt8(3, CLUB_FINDER_APPLICATION_APPROVED);
    stmt->setInt64(4, expiryCutoff);
    CharacterDatabase.Execute(stmt);

    // Mirror the purge in memory so lists stop reporting rows the database just dropped. A pending
    // or approved row past the expiry is already invisible to every list (IsApplicationExpired), so
    // dropping it here only removes dead weight, not history the UI still shows.
    std::erase_if(_applications, [this, retentionCutoff, expiryCutoff](ClubFinderApplication const& application)
    {
        if (!GetPosting(application.PostingId))
            return true;
        if (application.LastUpdatedTime < retentionCutoff)
            return true;
        if ((application.Status == CLUB_FINDER_APPLICATION_PENDING
            || application.Status == CLUB_FINDER_APPLICATION_AUTO_APPROVED
            || application.Status == CLUB_FINDER_APPLICATION_APPROVED)
            && application.LastUpdatedTime < expiryCutoff)
            return true;
        return false;
    });

    TC_LOG_DEBUG("network", "ClubFinder: application retention pass complete ({} applications kept)", _applications.size());
}

void ClubFinderMgr::LoadApplications()
{
    _applications.clear();

    QueryResult result = CharacterDatabase.Query("SELECT postingId, playerGuid, comment, specs, status, lastUpdatedTime FROM club_finder_application");
    if (!result)
        return;

    do
    {
        Field* fields = result->Fetch();

        ClubFinderApplication application;
        application.PostingId       = fields[0].GetUInt32();
        application.PlayerGuid      = ObjectGuid::Create<HighGuid::Player>(fields[1].GetUInt64());
        application.Comment         = fields[2].GetString();
        application.Specs           = fields[3].GetUInt64();
        application.Status          = fields[4].GetUInt8();
        application.LastUpdatedTime = fields[5].GetInt64();

        _applications.push_back(std::move(application));
    }
    while (result->NextRow());

    TC_LOG_INFO("server.loading", ">> Loaded {} club finder applications", _applications.size());
}

// The client's bit index for a specialisation is its rank in the ascending list of every
// ChrSpecialization id whose ClassID is non-zero. Reproduced here so a class filter can be tested
// against the recruitingSpecs mask a client sent us.
void ClubFinderMgr::BuildSpecBitIndex()
{
    _specMaskByClass.clear();

    std::vector<ChrSpecializationEntry const*> specs;
    for (ChrSpecializationEntry const* spec : sChrSpecializationStore)
        if (spec->ClassID)
            specs.push_back(spec);

    std::sort(specs.begin(), specs.end(), [](ChrSpecializationEntry const* left, ChrSpecializationEntry const* right)
    {
        return left->ID < right->ID;
    });

    if (specs.size() > 64)
    {
        // The client shifts with `bts`, which masks the count to 63 and would silently alias specs
        // onto each other. Refuse to build a mapping we know is wrong rather than mismatch quietly.
        TC_LOG_ERROR("server.loading", "ClubFinder: {} class specialisations exceed the 64 bit recruiting mask; spec filters disabled.", specs.size());
        return;
    }

    for (std::size_t bitIndex = 0; bitIndex < specs.size(); ++bitIndex)
        _specMaskByClass[specs[bitIndex]->ClassID] |= UI64LIT(1) << bitIndex;
}

uint64 ClubFinderMgr::GetSpecMaskForClass(uint8 classId) const
{
    auto itr = _specMaskByClass.find(classId);
    return itr != _specMaskByClass.end() ? itr->second : UI64LIT(0);
}

std::vector<ClubFinderApplication const*> ClubFinderMgr::GetApplicationsForPosting(uint32 postingId) const
{
    std::vector<ClubFinderApplication const*> applications;
    for (ClubFinderApplication const& application : _applications)
        if (application.PostingId == postingId)
            applications.push_back(&application);

    return applications;
}

std::vector<ClubFinderApplication const*> ClubFinderMgr::GetApplicationsForPlayer(ObjectGuid playerGuid) const
{
    std::vector<ClubFinderApplication const*> applications;
    for (ClubFinderApplication const& application : _applications)
        if (application.PlayerGuid == playerGuid)
            applications.push_back(&application);

    return applications;
}

ClubFinderApplication const* ClubFinderMgr::GetApplication(uint32 postingId, ObjectGuid playerGuid) const
{
    for (ClubFinderApplication const& application : _applications)
        if (application.PostingId == postingId && application.PlayerGuid == playerGuid)
            return &application;

    return nullptr;
}

ClubFinderApplication const* ClubFinderMgr::SaveApplication(ClubFinderApplication application)
{
    application.LastUpdatedTime = GameTime::GetGameTime();

    CharacterDatabasePreparedStatement* stmt = CharacterDatabase.GetPreparedStatement(CHAR_REP_CLUB_FINDER_APPLICATION);
    stmt->setUInt32(0, application.PostingId);
    stmt->setUInt64(1, application.PlayerGuid.GetCounter());
    stmt->setString(2, application.Comment);
    stmt->setUInt64(3, application.Specs);
    stmt->setUInt8(4, application.Status);
    stmt->setInt64(5, application.LastUpdatedTime);
    CharacterDatabase.Execute(stmt);

    for (ClubFinderApplication& existing : _applications)
    {
        if (existing.PostingId == application.PostingId && existing.PlayerGuid == application.PlayerGuid)
        {
            existing = std::move(application);
            return &existing;
        }
    }

    _applications.push_back(std::move(application));
    return &_applications.back();
}

ClubFinderPosting const* ClubFinderMgr::GetPosting(uint32 postingId) const
{
    auto itr = _postings.find(postingId);
    return itr != _postings.end() ? &itr->second : nullptr;
}

ClubFinderPosting const* ClubFinderMgr::GetPostingForClub(uint64 clubId) const
{
    auto itr = _postingsByClub.find(clubId);
    return itr != _postingsByClub.end() ? GetPosting(itr->second) : nullptr;
}

std::vector<ClubFinderPosting const*> ClubFinderMgr::GetAllPostings() const
{
    std::vector<ClubFinderPosting const*> postings;
    postings.reserve(_postings.size());
    for (auto const& [postingId, posting] : _postings)
        postings.push_back(&posting);

    return postings;
}

ClubFinderPosting const* ClubFinderMgr::SavePosting(ClubFinderPosting posting)
{
    // One posting per club: re-posting updates the existing entry rather than stacking duplicates,
    // which is what the client's single "post/update" button expects.
    if (ClubFinderPosting const* existing = GetPostingForClub(posting.ClubId))
    {
        posting.PostingId = existing->PostingId;

        // Moderation state belongs to the posting, not to whoever last edited it, so a re-post must
        // not clear it.
        posting.DisplayFlags = existing->DisplayFlags;
    }
    else
        posting.PostingId = ++_maxPostingId;

    posting.LastUpdatedTime = GameTime::GetGameTime();

    CharacterDatabasePreparedStatement* stmt = CharacterDatabase.GetPreparedStatement(CHAR_REP_CLUB_FINDER_POSTING);
    stmt->setUInt32(0, posting.PostingId);
    stmt->setUInt64(1, posting.ClubId);
    stmt->setString(2, posting.Name);
    stmt->setString(3, posting.Description);
    stmt->setUInt64(4, posting.RecruitingSpecs);
    stmt->setUInt32(5, posting.RecruitmentFlags);
    stmt->setUInt32(6, posting.ItemLevelRequirement);
    stmt->setUInt32(7, posting.AvatarId);
    stmt->setUInt32(8, posting.DisplayFlags);
    stmt->setUInt8(9, posting.Type);
    stmt->setBool(10, posting.CrossFaction);
    stmt->setUInt64(11, posting.LastPosterGUID.GetCounter());
    stmt->setInt64(12, posting.LastUpdatedTime);
    CharacterDatabase.Execute(stmt);

    uint32 const postingId = posting.PostingId;
    uint64 const clubId = posting.ClubId;

    _postings[postingId] = std::move(posting);
    _postingsByClub[clubId] = postingId;

    return &_postings[postingId];
}

bool ClubFinderMgr::IsPostingExpired(ClubFinderPosting const& posting)
{
    return posting.LastUpdatedTime
        && GameTime::GetGameTime() - posting.LastUpdatedTime > time_t(CLUB_FINDER_POSTING_EXPIRY_DAYS) * DAY;
}

bool ClubFinderMgr::IsApplicationExpired(ClubFinderApplication const& application)
{
    return application.LastUpdatedTime
        && GameTime::GetGameTime() - application.LastUpdatedTime > time_t(CLUB_FINDER_APPLICATION_EXPIRY_DAYS) * DAY;
}

bool ClubFinderMgr::IsPostingVisible(ClubFinderPosting const& posting)
{
    // A delisted, pending-delete or banned posting has been removed by moderation and must never
    // surface, whether through search or a direct posting-id lookup.
    if (posting.DisplayFlags & (CLUB_FINDER_POSTING_FLAG_POST_DELISTED | CLUB_FINDER_POSTING_FLAG_PENDING_DELETE | CLUB_FINDER_POSTING_FLAG_BANNED))
        return false;

    // A guild that has not enabled its listing is not advertising.
    if (!(posting.RecruitmentFlags & CLUB_FINDER_SETTING_ENABLE_LISTING))
        return false;

    // The client stops showing a posting as active after 30 days; do not offer it either.
    if (IsPostingExpired(posting))
        return false;

    return true;
}

// Matching runs on wchar_t code units, not on the UTF-8 bytes: a byte-level regex treats one byte
// as one character, so a Cyrillic class such as [бп] degenerates into matching lone lead/trail
// bytes and splits innocent words apart. Per-character matching keeps classes and quantifiers
// honest. Malformed sequences decode as U+FFFD.
static std::wstring Utf8ToWide(std::string const& text)
{
    std::wstring wide;
    wide.reserve(text.size());

    for (size_t i = 0; i < text.size();)
    {
        unsigned char c = text[i];
        uint32 cp = 0;
        size_t len = 0;

        if ((c & 0x80) == 0) { cp = c; len = 1; }                // ASCII byte
        else if ((c & 0xE0) == 0xC0) { cp = c & 0x1F; len = 2; } // two-byte lead
        else if ((c & 0xF0) == 0xE0) { cp = c & 0x0F; len = 3; } // three-byte lead
        else if ((c & 0xF8) == 0xF0) { cp = c & 0x07; len = 4; } // four-byte lead

        bool ok = len != 0 && i + len <= text.size();
        for (size_t k = 1; ok && k < len; ++k)
        {
            unsigned char cc = text[i + k];
            if ((cc & 0xC0) != 0x80)
                ok = false;
            else
                cp = (cp << 6) | (cc & 0x3F);
        }

        if (!ok)
        {
            wide += L'\uFFFD';
            ++i;
            continue;
        }

        wide += wchar_t(cp > 0xFFFF ? 0xFFFD : cp); // the word lists are BMP-only
        i += len;
    }

    return wide;
}

// Case fold for the scripts a chat word list realistically uses: ASCII and Cyrillic (Yo is the
// one uppercase Cyrillic letter outside the contiguous block). Anything else passes through
// unchanged, so folding both sides of a comparison with the same rule is always consistent.
static wchar_t FoldWideChar(wchar_t c)
{
    if (c >= 'A' && c <= 'Z')
        return c - 'A' + 'a';

    if (c >= L'\u0410' && c <= L'\u042F') // Cyrillic А..Я
        return c + (L'\u0430' - L'\u0410');

    if (c == L'\u0401') // Ё
        return L'\u0451'; // ё

    return c;
}

static std::wstring ToFoldedWide(std::string const& text)
{
    std::wstring wide = Utf8ToWide(text);
    for (wchar_t& c : wide)
        c = FoldWideChar(c);

    return wide;
}

bool ClubFinderMgr::IsProfaneText(std::string const& text)
{
    if (text.empty())
        return false;

    std::wstring const haystack = ToFoldedWide(text);

    // ChatProfanity rows are regular expressions written for their language's script. Three
    // adjustments make them work with std::wregex: pattern and haystack are folded with the same
    // function (std::regex cannot case-fold Cyrillic), the word-boundary assertions are rewritten
    // into explicit letter classes (a \b resolves through the C locale and never fires next to a
    // non-ASCII character), and only the realm's language list plus the shared English one
    // (-1/any) is applied, the way the client applies its own locale's list.
    int const realmLanguage = int(sWorld->GetDefaultDbcLocale());

    // The tables mark word boundaries with \< \b \>; the letter classes for the rewrite cover the
    // scripts the word lists use, in \u escapes so the literals do not depend on the source-file
    // encoding: ASCII letters/digits, Cyrillic А-Я/а-я, Ё/ё. Rewriting preserves the tables'
    // intent - a bounded root matches a standalone word while derivative spellings are separate
    // unbounded rows - instead of widening every root into a substring, which would false-positive
    // on words merely containing the root (the Scunthorpe problem).
    static std::wstring const wordChars = L"A-Za-z0-9\u0410-\u042F\u0401\u0430-\u044F\u0451";
    static std::wstring const leadBoundary = L"(?:^|[^" + wordChars + L"])";
    static std::wstring const tailBoundary = L"(?![^" + wordChars + L"])";

    for (ChatProfanityEntry const* entry : sChatProfanityStore)
    {
        int const language = int(entry->Language);
        if (language > 0 && language != realmLanguage)
            continue;

        if (!entry->Text || !*entry->Text)
            continue;

        // Text is a single non-localized regex per row (one word list across all languages).
        std::wstring pattern = ToFoldedWide(entry->Text);

        bool replaced = true;
        while (replaced)
        {
            replaced = false;
            for (wchar_t const* lead : { L"\\<", L"\\b" })
                if (pattern.compare(0, 2, lead) == 0)
                {
                    pattern = leadBoundary + pattern.substr(2);
                    replaced = true;
                }

            for (wchar_t const* tail : { L"\\>", L"\\b" })
                if (pattern.size() >= 2 && pattern.compare(pattern.size() - 2, 2, tail) == 0)
                {
                    pattern = pattern.substr(0, pattern.size() - 2) + tailBoundary;
                    replaced = true;
                }
        }

        for (wchar_t const* interior : { L"\\b", L"\\<", L"\\>" })
            for (size_t pos; (pos = pattern.find(interior)) != std::wstring::npos;)
                pattern.erase(pos, 2);

        if (pattern.empty())
            continue;

        try
        {
            if (std::regex_search(haystack, std::wregex(pattern)))
                return true;
        }
        catch (std::regex_error const&)
        {
            // A pattern the standard library cannot compile would otherwise deny every
            // description; skipping the row keeps the rest of the list working.
            TC_LOG_DEBUG("server", "ChatProfanity row {} has an unsupported pattern.", entry->ID);
        }
    }

    return false;
}

bool ClubFinderMgr::AddPostingDisplayFlags(uint32 postingId, uint32 flags)
{
    auto itr = _postings.find(postingId);
    if (itr == _postings.end())
        return false;

    ClubFinderPosting& posting = itr->second;
    if ((posting.DisplayFlags & flags) == flags)
        return false;

    posting.DisplayFlags |= flags;

    CharacterDatabasePreparedStatement* stmt = CharacterDatabase.GetPreparedStatement(CHAR_UPD_CLUB_FINDER_POSTING_FLAGS);
    stmt->setUInt32(0, posting.DisplayFlags);
    stmt->setUInt32(1, posting.PostingId);
    CharacterDatabase.Execute(stmt);

    return true;
}

bool ClubFinderMgr::RemovePostingDisplayFlags(uint32 postingId, uint32 flags)
{
    auto itr = _postings.find(postingId);
    if (itr == _postings.end())
        return false;

    ClubFinderPosting& posting = itr->second;
    if (!(posting.DisplayFlags & flags))
        return false;

    posting.DisplayFlags &= ~flags;

    CharacterDatabasePreparedStatement* stmt = CharacterDatabase.GetPreparedStatement(CHAR_UPD_CLUB_FINDER_POSTING_FLAGS);
    stmt->setUInt32(0, posting.DisplayFlags);
    stmt->setUInt32(1, posting.PostingId);
    CharacterDatabase.Execute(stmt);

    return true;
}

// The faction a posting recruits for is its guild's own faction, and a guild's faction is that
// of its leader. Resolved from the leader's cached race so it works whether or not the leader is
// online. Returns TEAM_NEUTRAL when the guild or leader cannot be resolved, which the search treats
// as "no faction constraint" (permissive) rather than hiding a posting on missing data.
static TeamId GetPostingTeamId(ClubFinderPosting const& posting)
{
    Guild const* guild = sGuildMgr->GetGuildById(posting.ClubId);
    if (!guild)
        return TEAM_NEUTRAL;

    CharacterCacheEntry const* leader = sCharacterCache->GetCharacterCacheByGuid(guild->GetLeaderGUID());
    if (!leader)
        return TEAM_NEUTRAL;

    return Player::TeamIdForRace(leader->Race);
}

std::vector<ClubFinderPosting const*> ClubFinderMgr::Search(SearchCriteria const& criteria) const
{
    std::string needle = criteria.SearchString;
    std::transform(needle.begin(), needle.end(), needle.begin(), [](unsigned char c) { return char(std::tolower(c)); });

    std::vector<ClubFinderPosting const*> results;
    for (auto const& [postingId, posting] : _postings)
    {
        if (criteria.Type != CLUB_FINDER_REQUEST_TYPE_ALL && posting.Type != criteria.Type)
            continue;

        // Moderation removal, unlisted and expiry gates - shared with the direct posting-id lookup so
        // the two cannot disagree about which postings are hidden.
        if (!IsPostingVisible(posting))
            continue;

        // Cross-faction search visibility: a posting whose guild is the OPPOSITE faction from the
        // searching player is hidden here, UNLESS the guild advertises cross-faction (posting.CrossFaction),
        // in which case it stays visible to both factions. Same-faction postings are always visible, and a
        // posting whose faction cannot be resolved (TEAM_NEUTRAL) is left visible. This is deliberately a
        // search-only filter: the direct posting-id lookup (BuildClubCacheData) has no searcher and is not
        // faction-gated, and apply/accept impose no faction gate at all - a cross-faction join is allowed.
        if (criteria.SearcherTeamId != -1 && !posting.CrossFaction)
        {
            TeamId const postingTeam = GetPostingTeamId(posting);
            if (postingTeam != TEAM_NEUTRAL && postingTeam != TeamId(criteria.SearcherTeamId))
                continue;
        }

        // The posting only advertises to players who meet its own item level requirement.
        if (criteria.ItemLevel && posting.ItemLevelRequirement > criteria.ItemLevel)
            continue;

        // Focus and size filters live in the same ClubFinderSettingFlags bit space as the posting's
        // own recruitmentFlags, so they match directly: the guild must share at least one of the
        // requested focuses / sizes.
        if (criteria.FocusFlags && !(posting.RecruitmentFlags & criteria.FocusFlags & CLUB_FINDER_SETTING_MASK_FOCUS))
            continue;

        if (criteria.SizeFlags && !(posting.RecruitmentFlags & criteria.SizeFlags & CLUB_FINDER_SETTING_MASK_SIZE))
            continue;

        // Class-role filter (Tank / Healer / Damage): the client sends the requested role bits in the
        // same recruitmentFlags bit space as focus and size, so it matches directly against the
        // posting's recruited-role bits (9-11) - the guild must recruit at least one requested role.
        if (criteria.RoleFlags && !(posting.RecruitmentFlags & criteria.RoleFlags & CLUB_FINDER_SETTING_MASK_ROLE))
            continue;

        // A spec filter matches when the guild recruits at least one of the requested specs.
        if (criteria.Specs && posting.RecruitingSpecs && !(posting.RecruitingSpecs & criteria.Specs))
            continue;

        // Locale: the posting packs (locale + 1) into bits 21-25 of its flags, the applicant sends a
        // bitmask of (1 << locale). An unset posting locale (packed 0) or an empty applicant mask is
        // treated as "no constraint" - the client gives no evidence either way, so the permissive
        // reading is used rather than silently hiding postings.
        if (criteria.LocaleFlags)
        {
            uint32 const packedLocale = (posting.RecruitmentFlags >> CLUB_FINDER_LOCALE_SHIFT) & CLUB_FINDER_LOCALE_MASK;
            if (packedLocale)
            {
                uint32 const localeId = packedLocale - 1;
                // Bit 9 is a hole in the locale table and anything above 11 is unused.
                if (localeId == 9 || localeId > 11 || !((criteria.LocaleFlags >> localeId) & 1))
                    continue;
            }
        }

        if (!needle.empty())
        {
            std::string name = posting.Name;
            std::transform(name.begin(), name.end(), name.begin(), [](unsigned char c) { return char(std::tolower(c)); });
            if (name.find(needle) == std::string::npos)
                continue;
        }

        results.push_back(&posting);
    }

    return results;
}
