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

#include "ProfanityBlobServer.h"
#include "Config.h"
#include "CryptoHash.h"
#include "Log.h"
#include "ProfanityFilterDefaults.h"
#include "Util.h"
#include "World.h"
#include "profanity_filter_config.pb.h"
#include <boost/asio.hpp>
#include <charconv>
#include <fstream>
#include <mutex>
#include <thread>
#include <utility>
#include <vector>
#include <zlib.h>

namespace
{
    char const ConfigOption[] = "ProfanityFilter.ServeURL";
    char const FilterFileSuffix[] = "/profanity_filters.conf";

    char const FilterFileHeader[] =
        "# Profanity filter dictionary served to the client over HTTP (see ProfanityFilter.ServeURL\n"
        "# in worldserver.conf; the full format documentation ships as profanity_filters.conf.dist\n"
        "# next to the server binary). '#' starts a comment. The file is re-read at worldserver\n"
        "# startup; the published content hash changes with every edit, so clients re-download\n"
        "# instead of using their cached copy. Keep at least one \"bad\" line: with an empty bad\n"
        "# dictionary the client wipes every recruitment description instead of checking it.\n"
        "\n";

    void WriteDefaultFilterFile(std::string const& path)
    {
        std::ofstream out(path, std::ios::binary);
        out << FilterFileHeader;
        out << "[bad]\n" << RetailBadRegex << "\n" << CyrillicRootsRegex << "\n\n";
        out << "[reserved]\n" << RetailReservedRegex << "\n\n";
        out << "[hatespeech]\n" << RetailHatespeechRegex << "\n";
    }

    std::vector<std::pair<std::string, std::string>> ReadFilterFile(std::string const& path)
    {
        std::vector<std::pair<std::string, std::string>> entries;
        std::ifstream in(path, std::ios::binary);
        std::string line;
        std::string section = "bad";

        while (std::getline(in, line))
        {
            if (!line.empty() && line.back() == '\r')
                line.pop_back();

            if (line.empty() || line[0] == '#')
                continue;

            if (line.front() == '[' && line.back() == ']')
            {
                section = line.substr(1, line.size() - 2);
                continue;
            }

            entries.emplace_back(section, line);
        }

        return entries;
    }

    // The depot container the client expects: "ZpmC" magic, u32 zero, u64 decompressed size,
    // then the zlib stream of the serialized WordFilters protobuf.
    bool BuildBlob(std::vector<std::pair<std::string, std::string>> const& entries, std::string& blob)
    {
        bgs::protocol::profanity::v1::WordFilters filters;
        for (auto const& [type, regex] : entries)
        {
            bgs::protocol::profanity::v1::WordFilter* filter = filters.add_filters();
            filter->set_type(type);
            filter->set_regex(regex);
        }

        std::string const proto = filters.SerializeAsString();

        uLongf compressedSize = compressBound(uLong(proto.size()));
        std::string compressed(compressedSize, '\0');
        if (compress2(reinterpret_cast<Bytef*>(compressed.data()), &compressedSize,
            reinterpret_cast<Bytef const*>(proto.data()), uLong(proto.size()), Z_DEFAULT_COMPRESSION) != Z_OK)
            return false;
        compressed.resize(compressedSize);

        blob.clear();
        blob += "ZpmC";
        blob.append(4, '\0');
        for (uint8 i = 0; i < 8; ++i) // u64 decompressed size, little-endian
            blob += char((uint64(proto.size()) >> (i * 8)) & 0xFF);
        blob += compressed;
        return true;
    }

    void ServeConnection(boost::asio::ip::tcp::socket socket)
    {
        try
        {
            // The request body is irrelevant to the reply; drain the request head.
            boost::asio::streambuf request;
            boost::asio::read_until(socket, request, "\r\n\r\n");

            std::string const& blob = sProfanityBlobServer->GetBlob();
            std::string header = "HTTP/1.1 200 OK\r\nContent-Length: " + std::to_string(blob.size())
                + "\r\nContent-Type: application/octet-stream\r\nConnection: close\r\n\r\n";

            boost::asio::write(socket, boost::asio::buffer(header));
            boost::asio::write(socket, boost::asio::buffer(blob));

            boost::system::error_code ec;
            socket.shutdown(boost::asio::ip::tcp::socket::shutdown_both, ec);
        }
        catch (std::exception const&)
        {
        }
    }

    void ListenLoop(uint16 port)
    {
        try
        {
            boost::asio::io_context io;
            boost::asio::ip::tcp::acceptor acceptor(io,
                boost::asio::ip::tcp::endpoint(boost::asio::ip::tcp::v4(), port));

            TC_LOG_INFO("server", "ProfanityFilter.ServeURL: HTTP listener ready on port {}.", port);

            for (;;)
            {
                boost::asio::ip::tcp::socket socket(io);
                acceptor.accept(socket);

                // Detached on purpose: worldserver is a single-purpose process and the
                // listener lives until it exits.
                std::thread(ServeConnection, std::move(socket)).detach();
            }
        }
        catch (std::exception const& e)
        {
            TC_LOG_ERROR("server", "ProfanityFilter.ServeURL: HTTP listener on port {} failed: {}",
                port, e.what());
        }
    }
}

ProfanityBlobServer* ProfanityBlobServer::Instance()
{
    static ProfanityBlobServer instance;
    return &instance;
}

void ProfanityBlobServer::InitIfNeeded()
{
    static std::once_flag once;
    std::call_once(once, [this]()
    {
        std::string url = sConfigMgr->GetStringDefault(ConfigOption, "");
        if (!url.empty())
            BuildAndServe(url);
    });
}

void ProfanityBlobServer::BuildAndServe(std::string urlTemplate)
{
    std::string filterPath = sWorld->GetDataPath() + FilterFileSuffix;

    std::ifstream probe(filterPath);
    if (!probe)
    {
        WriteDefaultFilterFile(filterPath);
        TC_LOG_INFO("server", "ProfanityFilter.ServeURL: created default {} from the retail ruRU filter list.", filterPath);
    }

    std::vector<std::pair<std::string, std::string>> entries = ReadFilterFile(filterPath);

    // An empty "bad" dictionary would make clients wipe every recruitment description instead
    // of checking it; refuse to serve that and keep the retail depot answer instead.
    bool hasBad = false;
    for (auto const& [type, regex] : entries)
        if (type == "bad" && !regex.empty())
            hasBad = true;

    if (!hasBad)
    {
        TC_LOG_ERROR("server", "ProfanityFilter.ServeURL is set but {} has no [bad] entries; "
            "serving it would blank every recruitment description. Falling back to the retail depot.",
            filterPath);
        return;
    }

    std::string blob;
    if (!BuildBlob(entries, blob))
    {
        TC_LOG_ERROR("server", "ProfanityFilter.ServeURL: failed to pack the filter blob.");
        return;
    }

    // The depot is content-addressed: the hash is the SHA-256 of the served file, which makes
    // every edit of the filter file change the handle and re-download on clients.
    Trinity::Crypto::SHA256::Digest digest = Trinity::Crypto::SHA256::GetDigestOf(
        reinterpret_cast<uint8 const*>(blob.data()), blob.size());
    _hashHex = ByteArrayToHexStr(digest);
    _blob = std::move(blob);

    // Bind the port out of the template's authority ("http://host:port/...").
    std::string_view url(urlTemplate);
    uint16 port = 80;
    if (size_t authorityBegin = url.find("://"); authorityBegin != std::string_view::npos)
    {
        authorityBegin += 3;
        size_t authorityEnd = url.find('/', authorityBegin);
        std::string_view authority = url.substr(authorityBegin,
            (authorityEnd == std::string_view::npos ? url.size() : authorityEnd) - authorityBegin);

        if (size_t colon = authority.rfind(':'); colon != std::string_view::npos)
        {
            uint16 parsed = 0;
            auto [end, ec] = std::from_chars(authority.data() + colon + 1, authority.data() + authority.size(), parsed);
            if (ec == std::errc{})
                port = parsed;
        }
    }

    _urlTemplate = std::move(urlTemplate);
    std::thread(ListenLoop, port).detach();

    TC_LOG_INFO("server", "ProfanityFilter.ServeURL: serving {} filter(s), hash {}, url template \"{}\".",
        entries.size(), _hashHex, _urlTemplate);
}
