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

#ifndef TRINITYCORE_SHOP2_SSL_CONTEXT_H
#define TRINITYCORE_SHOP2_SSL_CONTEXT_H

#include "Define.h"
#include <boost/asio/ssl/context.hpp>
#include <string>

namespace Shop2
{
// TLS material for the shop2 listener.
//
// The client only ever talks https:// to the shop2 host (CURLOPT_PROTOCOLS_STR is pinned to "https"
// in the client, see SHOP2_API_SCHEMA_68275.md section 0), so this listener is TLS-only: if the
// certificate or the private key cannot be loaded we refuse to start rather than silently falling
// back to plaintext, which the client could never reach anyway.
//
// This is deliberately separate from Battlenet::SslContext (bnetserver-only, and keyed off the
// bnetserver config) so the world server can use its own certificate on its own port.
class TC_GAME_API SslContext
{
public:
    // Loads the certificate chain + private key. Returns false (and logs) if anything is missing or
    // unreadable. Safe to call once per process; a second successful call reloads nothing.
    static bool Initialize(std::string const& certificateChainFile, std::string const& privateKeyFile,
        std::string const& privateKeyPassword);

    static boost::asio::ssl::context& instance();
};
}

#endif // TRINITYCORE_SHOP2_SSL_CONTEXT_H
