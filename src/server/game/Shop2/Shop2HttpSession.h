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

#ifndef TRINITYCORE_SHOP2_HTTP_SESSION_H
#define TRINITYCORE_SHOP2_HTTP_SESSION_H

#include "BaseHttpSocket.h"

namespace Shop2
{
// One accepted TLS connection from the game client's shop2 (CatalogUtilities.cpp) HTTP layer.
//
// Modelled on Battlenet::LoginHttpSession, minus the cookie/session plumbing (shop2 authenticates
// with a bearer token issued by our own /sso, not with JSESSIONID) and minus the DB query processor
// (every route is answered from a pre-rendered in-memory snapshot).
class TC_GAME_API Shop2HttpSession final : public Trinity::Net::Http::AbstractSocket, public std::enable_shared_from_this<Shop2HttpSession>
{
public:
    explicit Shop2HttpSession(Trinity::Net::IoContextTcpSocket&& socket);
    ~Shop2HttpSession();

    void Start() override;
    bool Update() override;
    boost::asio::ip::address const& GetRemoteIpAddress() const override { return _socket->GetRemoteIpAddress(); }
    bool IsOpen() const override { return _socket->IsOpen(); }
    void CloseSocket() override { return _socket->CloseSocket(); }

    void SendResponse(Trinity::Net::Http::RequestContext& context) override { return _socket->SendResponse(context); }
    std::string GetClientInfo() const override { return _socket->GetClientInfo(); }
    Trinity::Net::Http::SessionState* GetSessionState() const override { return _socket->GetSessionState(); }

private:
    std::shared_ptr<Trinity::Net::Http::AbstractSocket> _socket;
};
}

#endif // TRINITYCORE_SHOP2_HTTP_SESSION_H
