/*
 * This file is part of the TrinityCore Project. See AUTHORS file for Copyright information
 *
 * This program is free software; you can redistribute it and/or modify it under the terms of
 * the GNU General Public License as published by the Free Software Foundation; either version
 * 2 of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY,
 * without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with this program;
 * if not, see <http://www.gnu.org/licenses/>.
 */

#ifndef TRINITYCORE_PROFANITY_BLOB_SERVER_H
#define TRINITYCORE_PROFANITY_BLOB_SERVER_H

#include "Define.h"
#include <string>

// Serves the client's profanity-filter dictionary from this worldserver, so the dictionary no
// longer depends on Blizzard's depot being reachable. The filter list lives in a plain text
// file ("<data path>/profanity_filters.conf", one regular expression per line under
// [bad]/[reserved]/[hatespeech] sections) that the operator edits freely; the server packs it
// into the depot container format the client expects ("ZpmC" + u32 + u64 size + zlib of the
// WordFilters protobuf) and publishes it over plain HTTP. The content hash published to the
// client is the SHA-256 of the packed blob, so every edit produces a new hash and the client
// re-downloads instead of serving a stale cache.
//
// Enabled by setting, e.g.:
//     ProfanityFilter.ServeURL = http://192.168.0.10:8085/${hash}.${usage}
// in worldserver.conf. The URL template is handed to the client verbatim (it substitutes
// ${hash} and ${usage}); the server binds the port parsed out of it on all interfaces. When
// the option is unset the ResourcesService answers with the retail depot template and nothing
// is served locally.
class ProfanityBlobServer
{
public:
    static ProfanityBlobServer* Instance();

    // Reads the config option, builds the blob from the filter file (writing the retail
    // defaults on first run) and starts the HTTP listener. Cheap no-op when the option is
    // unset; safe to call from the ResourcesService on every content-handle request.
    void InitIfNeeded();

    bool IsServing() const { return !_urlTemplate.empty(); }
    std::string const& GetURLTemplate() const { return _urlTemplate; }
    std::string const& GetHashHex() const { return _hashHex; }
    std::string const& GetBlob() const { return _blob; }

private:
    ProfanityBlobServer() = default;

    void BuildAndServe(std::string urlTemplate);
    void StartListener(uint16 port);

    std::string _urlTemplate;
    std::string _blob;
    std::string _hashHex;
    bool _initialized = false;
};

#define sProfanityBlobServer ProfanityBlobServer::Instance()

#endif
