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

#include "Shop2HttpSession.h"
#include "HttpSslSocket.h"
#include "Log.h"
#include "Shop2Service.h"
#include "Shop2SslContext.h"
#include "SslStream.h"
#include <boost/container/static_vector.hpp>

namespace
{
class Shop2HttpSocketImpl final : public Trinity::Net::Http::SslSocket
{
public:
    using BaseSocket = Trinity::Net::Http::SslSocket;

    explicit Shop2HttpSocketImpl(Trinity::Net::IoContextTcpSocket&& socket, Shop2::Shop2HttpSession& owner)
        : BaseSocket(std::move(socket), Shop2::SslContext::instance()), _owner(owner)
    {
    }

    Shop2HttpSocketImpl(Shop2HttpSocketImpl const&) = delete;
    Shop2HttpSocketImpl(Shop2HttpSocketImpl&&) = delete;
    Shop2HttpSocketImpl& operator=(Shop2HttpSocketImpl const&) = delete;
    Shop2HttpSocketImpl& operator=(Shop2HttpSocketImpl&&) = delete;

    ~Shop2HttpSocketImpl() = default;

    void Start() override
    {
        boost::container::static_vector<std::shared_ptr<Trinity::Net::SocketConnectionInitializer>, 3> initializers;

        initializers.stable_emplace_back(std::make_shared<Trinity::Net::SslHandshakeConnectionInitializer<BaseSocket>>(this));
        initializers.stable_emplace_back(std::make_shared<Trinity::Net::Http::HttpConnectionInitializer<BaseSocket>>(this));
        initializers.stable_emplace_back(std::make_shared<Trinity::Net::ReadConnectionInitializer<BaseSocket>>(this));

        Trinity::Net::SocketConnectionInitializer::SetupChain(std::span(initializers.data(), initializers.size()))->Start();
    }

    Trinity::Net::Http::RequestHandlerResult RequestHandler(Trinity::Net::Http::RequestContext& context) override
    {
        return sShop2Service.HandleRequest(_owner.shared_from_this(), context);
    }

protected:
    std::shared_ptr<Trinity::Net::Http::SessionState> ObtainSessionState(Trinity::Net::Http::RequestContext& /*context*/) const override
    {
        // shop2 does not use cookies - the client carries "Authorization: Bearer <token>" on every
        // API call instead. One state per connection is all the framework needs.
        return sShop2Service.CreateNewSessionState(this->GetRemoteIpAddress());
    }

    Shop2::Shop2HttpSession& _owner;
};
}

namespace Shop2
{
Shop2HttpSession::Shop2HttpSession(Trinity::Net::IoContextTcpSocket&& socket)
    : _socket(std::make_shared<Shop2HttpSocketImpl>(std::move(socket), *this))
{
}

Shop2HttpSession::~Shop2HttpSession() = default;

void Shop2HttpSession::Start()
{
    TC_LOG_TRACE("server.http.session.shop2", "{} Accepted connection", GetClientInfo());

    return _socket->Start();
}

bool Shop2HttpSession::Update()
{
    return _socket->Update();
}
}
