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

#include "Shop2SslContext.h"
#include "Log.h"
#include <boost/filesystem/operations.hpp>
#include <boost/filesystem/path.hpp>

namespace Shop2
{
boost::asio::ssl::context& SslContext::instance()
{
    static boost::asio::ssl::context context(boost::asio::ssl::context::tls_server);
    return context;
}

bool SslContext::Initialize(std::string const& certificateChainFile, std::string const& privateKeyFile,
    std::string const& privateKeyPassword)
{
    if (certificateChainFile.empty() || privateKeyFile.empty())
    {
        TC_LOG_ERROR("server.http.shop2", "shop2: Shop.Shop2CertificatesFile and Shop.Shop2PrivateKeyFile must both be set "
            "(the client only speaks https to the shop2 host, so there is no plaintext fallback).");
        return false;
    }

    boost::system::error_code err;

    boost::filesystem::path const certPath = boost::filesystem::absolute(certificateChainFile);
    boost::filesystem::path const keyPath = boost::filesystem::absolute(privateKeyFile);

    if (!boost::filesystem::exists(certPath))
    {
        TC_LOG_ERROR("server.http.shop2", "shop2: certificate chain file '{}' does not exist.", certPath.generic_string());
        return false;
    }

    if (!boost::filesystem::exists(keyPath))
    {
        TC_LOG_ERROR("server.http.shop2", "shop2: private key file '{}' does not exist.", keyPath.generic_string());
        return false;
    }

    boost::asio::ssl::context& context = instance();

    context.set_options(boost::asio::ssl::context::default_workarounds
        | boost::asio::ssl::context::no_sslv2
        | boost::asio::ssl::context::no_sslv3
        | boost::asio::ssl::context::single_dh_use, err);
    if (err)
    {
        TC_LOG_ERROR("server.http.shop2", "shop2: set_options failed: {}", err.message());
        return false;
    }

    context.set_password_callback([privateKeyPassword](std::size_t /*maxLength*/,
        boost::asio::ssl::context::password_purpose /*purpose*/) -> std::string
    {
        return privateKeyPassword;
    }, err);
    if (err)
    {
        TC_LOG_ERROR("server.http.shop2", "shop2: set_password_callback failed: {}", err.message());
        return false;
    }

    context.use_certificate_chain_file(certPath.string(), err);
    if (err)
    {
        TC_LOG_ERROR("server.http.shop2", "shop2: failed to load certificate chain '{}': {}", certPath.generic_string(), err.message());
        return false;
    }

    context.use_private_key_file(keyPath.string(), boost::asio::ssl::context::pem, err);
    if (err)
    {
        TC_LOG_ERROR("server.http.shop2", "shop2: failed to load private key '{}': {}", keyPath.generic_string(), err.message());
        return false;
    }

    TC_LOG_INFO("server.http.shop2", "shop2: TLS material loaded (cert '{}', key '{}').",
        certPath.generic_string(), keyPath.generic_string());
    return true;
}
}
