/*
 * This file is part of the TrinityCore Project. See AUTHORS file for Copyright information
 *
 * This program is free software; you can redistribute it under the terms of the GNU
 * General Public License as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A
 * PARTICULAR PURPOSE. See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with this
 * program; if not, see <http://www.gnu.org/licenses/>.
 */

#include "ResourcesService.h"
#include "BattlenetRpcErrorCodes.h"
#include "CryptoHash.h"
#include "Log.h"
#include "ProfanityBlobServer.h"
#include "Util.h"
#include <array>

namespace Battlenet::Services
{
namespace
{
    // (program, stream) -> { region, usage, sha256 content hash }, transcribed from retail
    // GetContentHandle responses captured on the world socket. The client downloads
    // "{url}/{hash}.{usage}" from the depot itself; the profanity filter blob that feeds
    // the recruitment-description sanitizer arrives through one of these.
    struct ContentHandleEntry
    {
        uint32 Program;
        uint32 Stream;
        uint32 Region;
        uint32 Usage;
        char const* HashHex;
    };

    ContentHandleEntry const ContentHandles[] =
    {
        { 0x0000424E, 0x61706674, 0x00005545, 0x70667479, "df27067c3319ae468ceaa9e4862fc292ac7f0e02c98d4303287e4d87945eed96" },
        { 0x00417070, 0x72707373, 0x00005545, 0x61707073, "6e9d8f066396a29f5b015de0e6eb61a908a437897ef2fbe1c5e88b188252c675" },
        { 0x00576F57, 0x52505253, 0x00005545, 0x776F7773, "328399874495896089273f55cd234a0906c82c43d6891be8689f8bad42c454c8" },
        { 0x00576F57, 0x41726561, 0x00005545, 0x776F7773, "d9f86012670f595e7d687fd1af2c4edbbf577ed98f5058b74aa450fa6975e897" },
        { 0x00576F57, 0x52656C6D, 0x00005545, 0x776F7773, "4d8f2c688c58cbb462e2c4554213f27220aa54ce70d0db94d5f9ebb504806fde" },
    };
}

ResourcesService::ResourcesService(WorldSession* session) : BaseService(session) { }

uint32 ResourcesService::HandleGetContentHandle(resources::v1::ContentHandleRequest const* request, bgs::protocol::ContentHandle* response,
    std::function<void(ServiceBase*, uint32, ::google::protobuf::Message const*)>& /*continuation*/)
{
    // The profanity blob can be served by this worldserver itself (ProfanityFilter.ServeURL);
    // the listener and blob are built lazily on first use, config already loaded by now.
    sProfanityBlobServer->InitIfNeeded();

    for (ContentHandleEntry const& entry : ContentHandles)
    {
        if (entry.Program != request->program() || entry.Stream != request->stream())
            continue;

        response->set_region(entry.Region);
        response->set_usage(entry.Usage);

        std::string hash;
        std::array<uint8, Trinity::Crypto::SHA256::DIGEST_LENGTH> hashBytes = {};

        // The profanity dictionary is the one blob this server's features depend on, so it is
        // the one eligible for local serving; every other handle keeps the retail depot URL.
        char const* urlTemplate = "https://prod.depot.battle.net/${hash}.${usage}";
        if (entry.Stream == 0x61706674 && sProfanityBlobServer->IsServing())
        {
            urlTemplate = sProfanityBlobServer->GetURLTemplate().c_str();
            HexStrToByteArray(sProfanityBlobServer->GetHashHex(), hashBytes);
        }
        else
            HexStrToByteArray(entry.HashHex, hashBytes);

        response->set_hash(std::string(reinterpret_cast<char*>(hashBytes.data()), hashBytes.size()));

        // Field 4 of the 12.1 client's ContentHandle is the depot URL template; the
        // generated proto predates it, so it travels as an unknown field verbatim.
        response->mutable_unknown_fields()->AddLengthDelimited(4, urlTemplate);

        TC_LOG_DEBUG("network", "ResourcesService.HandleGetContentHandle [{}]: program {:#x} stream {:#x} -> usage {:#x}{}",
            _session->GetPlayerInfo(), request->program(), request->stream(), entry.Usage,
            entry.Stream == 0x61706674 && sProfanityBlobServer->IsServing() ? " (served locally)" : "");
        return ERROR_OK;
    }

    TC_LOG_DEBUG("network", "ResourcesService.HandleGetContentHandle [{}]: unhandled request program {:#x} stream {:#x} version {:#x}",
        _session->GetPlayerInfo(), request->program(), request->stream(), request->version());
    return ERROR_RPC_NOT_IMPLEMENTED;
}
}
