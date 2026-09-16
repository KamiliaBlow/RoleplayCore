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

#ifndef TRINITYCORE_VAS_TRANSFER_MGR_H
#define TRINITYCORE_VAS_TRANSFER_MGR_H

#include "Define.h"
#include "ObjectGuid.h"
#include <string>
#include <unordered_map>

// Value-Added Service: paid character transfer between the realms this bnetserver fronts.
//
// The realms share one auth DB but each has its own characters DB on the same MySQL instance, so a transfer
// is a cross-database row move of the character and everything keyed to it, then a delete from the source.
// WoW's only per-realm unique key is the character NAME, so the sole hard rejection is a same-name character
// on the target; guids are kept when free and the move is refused (never silently remapped) when a guid would
// collide, so a transfer either completes intact or does not happen - it never corrupts either realm.
//
// The realm -> characters-database mapping is not in any schema, so it comes from one worldserver config line:
//   VAS.TransferRealmDatabases = "1:tc_characters,2:playerbot_characters,3:integ_characters"
class TC_GAME_API VasTransferMgr
{
public:
    enum TransferResult : uint8
    {
        TRANSFER_OK = 0,
        TRANSFER_ERR_NO_TARGET,        // no chars-db configured for the target realm (or it is this realm)
        TRANSFER_ERR_NO_SOURCE,        // no chars-db configured for this realm
        TRANSFER_ERR_CHAR_NOT_FOUND,   // no such character in the source db
        TRANSFER_ERR_IN_WORLD,         // the character is currently online - transfer only from char-select
        TRANSFER_ERR_NAME_TAKEN,       // a character with the same name already exists on the target
        TRANSFER_ERR_GUID_COLLISION,   // the character or one of its item guids already exists on the target
        TRANSFER_ERR_DB                // an unexpected database failure - the transaction rolled back
    };

    static VasTransferMgr* instance();

    VasTransferMgr(VasTransferMgr const&) = delete;
    VasTransferMgr& operator=(VasTransferMgr const&) = delete;

    void LoadConfig();   // World::LoadConfigSettings

    // The characters database configured for a realm id, or empty if none is mapped.
    std::string GetRealmDatabase(uint32 realmId) const;

    bool IsEnabled() const { return _enabled; }

    // Move the character (source realm = this realm) to targetRealmId. Synchronous and transactional; on any
    // rejection nothing is written. outName/outTargetDb are filled for the caller's log/response when non-null.
    // With validateOnly = true it runs every eligibility/collision check and returns the same result WITHOUT
    // moving anything - the client's transfer flow validates before it commits, and so does the server.
    TransferResult TransferCharacter(ObjectGuid::LowType charGuid, uint32 targetRealmId,
        std::string* outName = nullptr, std::string* outTargetDb = nullptr, bool validateOnly = false);

    static char const* ResultString(TransferResult result);

private:
    VasTransferMgr() = default;
    ~VasTransferMgr() = default;

    bool _enabled = false;
    std::unordered_map<uint32, std::string> _realmDatabases;   // realm id -> characters db name
};

#define sVasTransferMgr VasTransferMgr::instance()

#endif // TRINITYCORE_VAS_TRANSFER_MGR_H
