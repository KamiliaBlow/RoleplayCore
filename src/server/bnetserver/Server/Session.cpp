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

#include "Session.h"
#include "AccountService.h"
#include "AuthenticationService.h"
#include "BattlenetRpcErrorCodes.h"
#include "ByteConverter.h"
#include "CryptoRandom.h"
#include "DatabaseEnv.h"
#include "Errors.h"
#include "Hash.h"
#include "IPLocation.h"
#include "IpBanCheckConnectionInitializer.h"
#include "LoginRESTService.h"
#include "MapUtils.h"
#include "ProtobufJSON.h"
#include "QueryCallback.h"
#include "RealmList.h"
#include "RealmList.pb.h"
#include "ServiceDispatcher.h"
#include "SslContext.h"
#include "Timezone.h"
#include "attribute_types.pb.h"
#include "challenge_service.pb.h"
#include "game_utilities_service.pb.h"
#include "rpc_types.pb.h"
#include <rapidjson/document.h>
#include <zlib.h>

void Battlenet::Session::AccountInfo::LoadResult(PreparedQueryResult result)
{
    // ba.id, ba.email, ba.locked, ba.lock_country, ba.last_ip, ba.LoginTicketExpiry, bab.unbandate > UNIX_TIMESTAMP() OR bab.unbandate = bab.bandate, bab.unbandate = bab.bandate FROM battlenet_accounts ba LEFT JOIN battlenet_account_bans bab WHERE email = ?
    Field* fields = result->Fetch();
    Id = fields[0].GetUInt32();
    Login = fields[1].GetString();
    IsLockedToIP = fields[2].GetBool();
    LockCountry = fields[3].GetString();
    LastIP = fields[4].GetString();
    LoginTicketExpiry = fields[5].GetUInt32();
    IsBanned = fields[6].GetUInt64() != 0;
    IsPermanenetlyBanned = fields[7].GetUInt64() != 0;

    static constexpr uint32 GameAccountFieldsOffset = 8;

    do
    {
        GameAccounts[result->Fetch()[GameAccountFieldsOffset].GetUInt32()].LoadResult(result->Fetch() + GameAccountFieldsOffset);

    } while (result->NextRow());
}

void Battlenet::Session::GameAccountInfo::LoadResult(Field const* fields)
{
    // a.id, a.username, ab.unbandate, ab.unbandate = ab.bandate, aa.SecurityLevel
    Id = fields[0].GetUInt32();
    Name = fields[1].GetString();
    UnbanDate = fields[2].GetUInt32();
    IsPermanenetlyBanned = fields[3].GetUInt32() != 0;
    IsBanned = IsPermanenetlyBanned || UnbanDate > time(nullptr);
    SecurityLevel = AccountTypes(fields[4].GetUInt8());

    std::size_t hashPos = Name.find('#');
    if (hashPos != std::string::npos)
        DisplayName = std::string("WoW") + Name.substr(hashPos + 1);
    else
        DisplayName = Name;
}

Battlenet::Session::Session(Trinity::Net::IoContextTcpSocket&& socket) : BaseSocket(std::move(socket), SslContext::instance()),
    _accountInfo(new AccountInfo()), _gameAccountInfo(nullptr), _locale(),
    _os(), _build(0), _clientInfo(), _timezoneOffset(0min), _ipCountry(), _clientSecret(), _authed(false), _requestToken(0)
{
    _headerLengthBuffer.Resize(2);
}

Battlenet::Session::~Session() = default;

void Battlenet::Session::Start()
{
    TC_LOG_TRACE("session", "{} Accepted connection", GetClientInfo());

    // build initializer chain
    std::array<std::shared_ptr<Trinity::Net::SocketConnectionInitializer>, 3> initializers =
    { {
        std::make_shared<Trinity::Net::IpBanCheckConnectionInitializer<Session>>(this),
        std::make_shared<Trinity::Net::SslHandshakeConnectionInitializer<Session>>(this),
        std::make_shared<Trinity::Net::ReadConnectionInitializer<Session>>(this),
    } };

    Trinity::Net::SocketConnectionInitializer::SetupChain(initializers)->Start();
}

bool Battlenet::Session::Update()
{
    if (!BaseSocket::Update())
        return false;

    _queryProcessor.ProcessReadyCallbacks();

    return true;
}

void Battlenet::Session::AsyncWrite(MessageBuffer* packet)
{
    if (!IsOpen())
        return;

    QueuePacket(std::move(*packet));
}

void Battlenet::Session::SendResponse(uint32 token, pb::Message const* response)
{
    Header header;
    header.set_token(token);
    header.set_service_id(0xFE);
    header.set_size(response->ByteSize());

    uint16 headerSize = header.ByteSize();
    EndianConvertReverse(headerSize);

    MessageBuffer packet(sizeof(headerSize) + header.GetCachedSize() + response->GetCachedSize());
    packet.Write(&headerSize, sizeof(headerSize));
    uint8* ptr = packet.GetWritePointer();
    packet.WriteCompleted(header.GetCachedSize());
    header.SerializePartialToArray(ptr, header.GetCachedSize());
    ptr = packet.GetWritePointer();
    packet.WriteCompleted(response->GetCachedSize());
    response->SerializeToArray(ptr, response->GetCachedSize());

    AsyncWrite(&packet);
}

void Battlenet::Session::SendResponse(uint32 token, uint32 status)
{
    Header header;
    header.set_token(token);
    header.set_status(status);
    header.set_service_id(0xFE);

    uint16 headerSize = header.ByteSize();
    EndianConvertReverse(headerSize);

    MessageBuffer packet(sizeof(headerSize) + header.GetCachedSize());
    packet.Write(&headerSize, sizeof(headerSize));
    uint8* ptr = packet.GetWritePointer();
    packet.WriteCompleted(header.GetCachedSize());
    header.SerializeToArray(ptr, header.GetCachedSize());

    AsyncWrite(&packet);
}

void Battlenet::Session::SendRequest(uint32 serviceHash, uint32 methodId, pb::Message const* request)
{
    Header header;
    header.set_service_id(0);
    header.set_service_hash(serviceHash);
    header.set_method_id(methodId);
    header.set_size(request->ByteSize());
    header.set_token(_requestToken++);

    uint16 headerSize = header.ByteSize();
    EndianConvertReverse(headerSize);

    MessageBuffer packet(sizeof(headerSize) + header.GetCachedSize() + request->GetCachedSize());
    packet.Write(&headerSize, sizeof(headerSize));
    uint8* ptr = packet.GetWritePointer();
    packet.WriteCompleted(header.GetCachedSize());
    header.SerializeToArray(ptr, header.GetCachedSize());
    ptr = packet.GetWritePointer();
    packet.WriteCompleted(request->GetCachedSize());
    request->SerializeToArray(ptr, request->GetCachedSize());

    AsyncWrite(&packet);
}

void Battlenet::Session::QueueQuery(QueryCallback&& queryCallback)
{
    _queryProcessor.AddCallback(std::move(queryCallback));
}

uint32 Battlenet::Session::HandleLogon(authentication::v1::LogonRequest const* logonRequest, std::function<void(ServiceBase*, uint32, ::google::protobuf::Message const*)>& continuation)
{
    if (logonRequest->program() != "WoW")
    {
        TC_LOG_DEBUG("session", "[Battlenet::LogonRequest] {} attempted to log in with game other than WoW (using {})!", GetClientInfo(), logonRequest->program());
        return ERROR_BAD_PROGRAM;
    }

    if (!ClientBuild::Platform::IsValid(logonRequest->platform()))
    {
        TC_LOG_DEBUG("session", "[Battlenet::LogonRequest] {} attempted to log in from an unsupported platform (using {})!", GetClientInfo(), logonRequest->platform());
        return ERROR_BAD_PLATFORM;
    }

    if (!IsValidLocale(GetLocaleByName(logonRequest->locale())))
    {
        TC_LOG_DEBUG("session", "[Battlenet::LogonRequest] {} attempted to log in with unsupported locale (using {})!", GetClientInfo(), logonRequest->locale());
        return ERROR_BAD_LOCALE;
    }

    _locale = logonRequest->locale();
    _os = logonRequest->platform();
    _build = logonRequest->application_version();

    _timezoneOffset = [&]
    {
        if (!logonRequest->has_device_id())
            return 0min;

        rapidjson::Document doc;
        doc.Parse(logonRequest->device_id().c_str(), logonRequest->device_id().length());
        if (doc.HasParseError())
            return 0min;

        auto itr = doc.FindMember("UTCO");
        if (itr == doc.MemberEnd())
            return 0min;

        if (!itr->value.IsUint())
            return 0min;

        return Trinity::Timezone::GetOffsetByHash(itr->value.GetUint());
    }();

    if (logonRequest->has_cached_web_credentials())
        return VerifyWebCredentials(logonRequest->cached_web_credentials(), continuation);

    challenge::v1::ChallengeExternalRequest externalChallenge;
    externalChallenge.set_payload_type("web_auth_url");
    externalChallenge.set_payload(Trinity::StringFormat("http{}://{}:{}/bnetserver/login/", !SslContext::UsesDevWildcardCertificate() ? "s" : "",
        sLoginService.GetHostnameForClient(GetRemoteIpAddress()), sLoginService.GetPort()));
    Service<challenge::v1::ChallengeListener>(this).OnExternalChallenge(&externalChallenge);
    return ERROR_OK;
}

uint32 Battlenet::Session::HandleVerifyWebCredentials(authentication::v1::VerifyWebCredentialsRequest const* verifyWebCredentialsRequest, std::function<void(ServiceBase*, uint32, ::google::protobuf::Message const*)>& continuation)
{
    if (verifyWebCredentialsRequest->has_web_credentials())
        return VerifyWebCredentials(verifyWebCredentialsRequest->web_credentials(), continuation);

    return ERROR_DENIED;
}

uint32 Battlenet::Session::HandleGenerateWebCredentials(authentication::v1::GenerateWebCredentialsRequest const* request, std::function<void(ServiceBase*, uint32, google::protobuf::Message const*)>& continuation)
{
    if (!_authed)
        return ERROR_DENIED;

    if (request->program() != 0x576F57)
    {
        auto asPrintable = [](char c) { return std::isprint(c) ? c : ' '; };

        TC_LOG_DEBUG("session", "[Battlenet::HandleGenerateWebCredentials] {} attempted to generate web cretentials with game other than WoW (using {}{}{}{})!",
            GetClientInfo(), asPrintable((request->program() >> 24) & 0xFF), asPrintable((request->program() >> 16) & 0xFF),
            asPrintable((request->program() >> 8) & 0xFF), asPrintable(request->program() & 0xFF));
        return ERROR_BAD_PROGRAM;
    }

    LoginDatabasePreparedStatement* stmt = LoginDatabase.GetPreparedStatement(LOGIN_SEL_BNET_EXISTING_AUTHENTICATION_BY_ID);
    stmt->setUInt32(0, _accountInfo->Id);

    QueueQuery(LoginDatabase.AsyncQuery(stmt).WithPreparedCallback([this, asyncContinuation = std::move(continuation)](PreparedQueryResult result)
    {
        // just send existing credentials back (not the best but it works for now with them being stored in db)
        Battlenet::Services::Authentication asyncContinuationService(this);
        authentication::v1::GenerateWebCredentialsResponse response;
        response.set_web_credentials((*result)[0].GetCString());
        asyncContinuation(&asyncContinuationService, ERROR_OK, &response);
    }));

    return ERROR_OK;
}

uint32 Battlenet::Session::VerifyWebCredentials(std::string const& webCredentials, std::function<void(ServiceBase*, uint32, ::google::protobuf::Message const*)>& continuation)
{
    if (webCredentials.empty())
        return ERROR_DENIED;

    LoginDatabasePreparedStatement* stmt = LoginDatabase.GetPreparedStatement(LOGIN_SEL_BNET_ACCOUNT_INFO);
    stmt->setString(0, webCredentials);

    std::function<void(ServiceBase*, uint32, ::google::protobuf::Message const*)> asyncContinuation = std::move(continuation);
    std::shared_ptr<AccountInfo> accountInfo = std::make_shared<AccountInfo>();
    QueueQuery(LoginDatabase.AsyncQuery(stmt).WithChainingPreparedCallback([this, accountInfo, asyncContinuation](QueryCallback& callback, PreparedQueryResult result)
    {
        Battlenet::Services::Authentication asyncContinuationService(this);
        NoData response;
        if (!result)
        {
            asyncContinuation(&asyncContinuationService, ERROR_DENIED, &response);
            return;
        }

        accountInfo->LoadResult(result);

        if (accountInfo->LoginTicketExpiry < time(nullptr))
        {
            asyncContinuation(&asyncContinuationService, ERROR_TIMED_OUT, &response);
            return;
        }

        LoginDatabasePreparedStatement* stmt = LoginDatabase.GetPreparedStatement(LOGIN_SEL_BNET_CHARACTER_COUNTS_BY_BNET_ID);
        stmt->setUInt32(0, accountInfo->Id);
        callback.SetNextQuery(LoginDatabase.AsyncQuery(stmt));
    })
        .WithChainingPreparedCallback([accountInfo](QueryCallback& callback, PreparedQueryResult characterCountsResult)
    {
        if (characterCountsResult)
        {
            do
            {
                Field* fields = characterCountsResult->Fetch();
                accountInfo->GameAccounts[fields[0].GetUInt32()]
                    .CharacterCounts[Battlenet::RealmHandle{ fields[3].GetUInt8(), fields[4].GetUInt8(), fields[2].GetUInt32() }.GetAddress()] = fields[1].GetUInt8();

            } while (characterCountsResult->NextRow());
        }

        LoginDatabasePreparedStatement* stmt = LoginDatabase.GetPreparedStatement(LOGIN_SEL_BNET_LAST_PLAYER_CHARACTERS);
        stmt->setUInt32(0, accountInfo->Id);
        callback.SetNextQuery(LoginDatabase.AsyncQuery(stmt));
    })
        .WithPreparedCallback([this, accountInfo, asyncContinuation](PreparedQueryResult lastPlayerCharactersResult)
    {
        if (lastPlayerCharactersResult)
        {
            do
            {
                Field* fields = lastPlayerCharactersResult->Fetch();
                Battlenet::RealmHandle realmId{ fields[1].GetUInt8(), fields[2].GetUInt8(), fields[3].GetUInt32() };
                Battlenet::Session::LastPlayedCharacterInfo& lastPlayedCharacter = accountInfo->GameAccounts[fields[0].GetUInt32()]
                    .LastPlayedCharacters[realmId.GetSubRegionAddress()];

                lastPlayedCharacter.RealmId = realmId;
                lastPlayedCharacter.CharacterName = fields[4].GetString();
                lastPlayedCharacter.CharacterGUID = fields[5].GetUInt64();
                lastPlayedCharacter.LastPlayedTime = fields[6].GetUInt32();

            } while (lastPlayerCharactersResult->NextRow());
        }

        _accountInfo = accountInfo;
        Battlenet::Services::Authentication asyncContinuationService(this);
        NoData response;

        std::string ip_address = GetRemoteIpAddress().to_string();

        // If the IP is 'locked', check that the player comes indeed from the correct IP address
        if (_accountInfo->IsLockedToIP)
        {
            TC_LOG_DEBUG("session", "[Session::HandleVerifyWebCredentials] Account '{}' is locked to IP - '{}' is logging in from '{}'",
                _accountInfo->Login, _accountInfo->LastIP, ip_address);

            if (_accountInfo->LastIP != ip_address)
            {
                asyncContinuation(&asyncContinuationService, ERROR_RISK_ACCOUNT_LOCKED, &response);
                return;
            }
        }
        else
        {
            if (IpLocationRecord const* location = sIPLocation->GetLocationRecord(ip_address))
                _ipCountry = location->CountryCode;

            TC_LOG_DEBUG("session", "[Session::HandleVerifyWebCredentials] Account '{}' is not locked to ip", _accountInfo->Login);
            if (_accountInfo->LockCountry.empty() || _accountInfo->LockCountry == "00")
                TC_LOG_DEBUG("session", "[Session::HandleVerifyWebCredentials] Account '{}' is not locked to country", _accountInfo->Login);
            else if (!_accountInfo->LockCountry.empty() && !_ipCountry.empty())
            {
                TC_LOG_DEBUG("session", "[Session::HandleVerifyWebCredentials] Account '{}' is locked to country: '{}' Player country is '{}'",
                    _accountInfo->Login, _accountInfo->LockCountry, _ipCountry);

                if (_ipCountry != _accountInfo->LockCountry)
                {
                    asyncContinuation(&asyncContinuationService, ERROR_RISK_ACCOUNT_LOCKED, &response);
                    return;
                }
            }
        }

        // If the account is banned, reject the logon attempt
        if (_accountInfo->IsBanned)
        {
            if (_accountInfo->IsPermanenetlyBanned)
            {
                TC_LOG_DEBUG("session", "{} [Session::HandleVerifyWebCredentials] Banned account {} tried to login!", GetClientInfo(), _accountInfo->Login);
                asyncContinuation(&asyncContinuationService, ERROR_GAME_ACCOUNT_BANNED, &response);
                return;
            }
            else
            {
                TC_LOG_DEBUG("session", "{} [Session::HandleVerifyWebCredentials] Temporarily banned account {} tried to login!", GetClientInfo(), _accountInfo->Login);
                asyncContinuation(&asyncContinuationService, ERROR_GAME_ACCOUNT_SUSPENDED, &response);
                return;
            }
        }

        authentication::v1::LogonResult logonResult;
        logonResult.set_error_code(0);
        logonResult.mutable_account_id()->set_low(_accountInfo->Id);
        logonResult.mutable_account_id()->set_high(UI64LIT(0x100000000000000));
        for (auto const& [id, gameAccountInfo] : accountInfo->GameAccounts)
        {
            EntityId* gameAccountId = logonResult.add_game_account_id();
            gameAccountId->set_low(gameAccountInfo.Id);
            gameAccountId->set_high(UI64LIT(0x200000200576F57));
        }

        if (!_ipCountry.empty())
            logonResult.set_geoip_country(_ipCountry);

        std::array<uint8, 64> k = Trinity::Crypto::GetRandomBytes<64>();
        logonResult.set_session_key(k.data(), 64);

        _authed = true;

        asyncContinuation(&asyncContinuationService, ERROR_OK, &response);
        Service<authentication::v1::AuthenticationListener>(this).OnLogonComplete(&logonResult);
    }));

    return ERROR_OK;
}

uint32 Battlenet::Session::HandleGetAccountState(account::v1::GetAccountStateRequest const* request, account::v1::GetAccountStateResponse* response)
{
    if (!_authed)
        return ERROR_DENIED;

    if (request->options().field_privacy_info())
    {
        response->mutable_state()->mutable_privacy_info()->set_is_using_rid(false);
        response->mutable_state()->mutable_privacy_info()->set_is_visible_for_view_friends(false);
        response->mutable_state()->mutable_privacy_info()->set_is_hidden_from_friend_finder(true);

        response->mutable_tags()->set_privacy_info_tag(0xD7CA834D);
    }

    return ERROR_OK;
}

uint32 Battlenet::Session::HandleGetGameAccountState(account::v1::GetGameAccountStateRequest const* request, account::v1::GetGameAccountStateResponse* response)
{
    if (!_authed)
        return ERROR_DENIED;

    if (request->options().field_game_level_info())
    {
        auto itr = _accountInfo->GameAccounts.find(request->game_account_id().low());
        if (itr != _accountInfo->GameAccounts.end())
        {
            response->mutable_state()->mutable_game_level_info()->set_name(itr->second.DisplayName);
            response->mutable_state()->mutable_game_level_info()->set_program(5730135); // WoW
        }

        response->mutable_tags()->set_game_level_info_tag(0x5C46D483);
    }

    if (request->options().field_game_status())
    {
        auto itr = _accountInfo->GameAccounts.find(request->game_account_id().low());
        if (itr != _accountInfo->GameAccounts.end())
        {
            response->mutable_state()->mutable_game_status()->set_is_suspended(itr->second.IsBanned);
            response->mutable_state()->mutable_game_status()->set_is_banned(itr->second.IsPermanenetlyBanned);
            response->mutable_state()->mutable_game_status()->set_suspension_expires(uint64(itr->second.UnbanDate) * 1000000);
        }

        response->mutable_state()->mutable_game_status()->set_program(5730135); // WoW
        response->mutable_tags()->set_game_status_tag(0x98B75F99);
    }

    return ERROR_OK;
}

std::unordered_map<std::string, Battlenet::Session::ClientRequestHandler> const Battlenet::Session::ClientRequestHandlers =
{
    { "Command_RealmListTicketRequest_v1", &Battlenet::Session::GetRealmListTicket },
    { "Command_LastCharPlayedRequest_v1", &Battlenet::Session::GetLastCharPlayed },
    { "Command_RealmListRequest_v1", &Battlenet::Session::GetRealmList },
    { "Command_RealmJoinRequest_v1", &Battlenet::Session::JoinRealm },
};

uint32 Battlenet::Session::HandleProcessClientRequest(game_utilities::v1::ClientRequest const* request, game_utilities::v1::ClientResponse* response)
{
    if (!_authed)
        return ERROR_DENIED;

    Attribute const* command = nullptr;
    std::unordered_map<std::string, Variant const*> params;
    auto removeSuffix = [](std::string const& string) -> std::string
    {
        size_t pos = string.rfind('_');
        if (pos != std::string::npos)
            return string.substr(0, pos);

        return string;
    };

    for (int32 i = 0; i < request->attribute_size(); ++i)
    {
        Attribute const& attr = request->attribute(i);
        if (strstr(attr.name().c_str(), "Command_") == attr.name().c_str())
        {
            command = &attr;
            params[removeSuffix(attr.name())] = &attr.value();
        }
        else
            params[attr.name()] = &attr.value();
    }

    if (!command)
    {
        TC_LOG_ERROR("session.rpc", "{} sent ClientRequest with no command.", GetClientInfo());
        return ERROR_RPC_MALFORMED_REQUEST;
    }

    auto itr = ClientRequestHandlers.find(removeSuffix(command->name()));
    if (itr == ClientRequestHandlers.end())
    {
        TC_LOG_ERROR("session.rpc", "{} sent ClientRequest with unknown command {}.", GetClientInfo(), removeSuffix(command->name()));
        return ERROR_RPC_NOT_IMPLEMENTED;
    }

    return (this->*itr->second)(params, response);
}

uint32 Battlenet::Session::GetRealmListTicket(std::unordered_map<std::string, Variant const*> const& params, game_utilities::v1::ClientResponse* response)
{
    if (Variant const* identity = Trinity::Containers::MapGetValuePtr(params, "Param_Identity"))
    {
        ::JSON::RealmList::RealmListTicketIdentity data;
        std::size_t jsonStart = identity->blob_value().find(':');
        if (jsonStart != std::string::npos && ::JSON::Deserialize(identity->blob_value().substr(jsonStart + 1), &data))
        {
            auto itr = _accountInfo->GameAccounts.find(data.gameaccountid());
            if (itr != _accountInfo->GameAccounts.end())
                _gameAccountInfo = &itr->second;
        }
    }

    if (!_gameAccountInfo)
        return ERROR_UTIL_SERVER_INVALID_IDENTITY_ARGS;

    if (_gameAccountInfo->IsPermanenetlyBanned)
        return ERROR_GAME_ACCOUNT_BANNED;
    else if (_gameAccountInfo->IsBanned)
        return ERROR_GAME_ACCOUNT_SUSPENDED;

    bool clientInfoOk = false;
    if (Variant const* clientInfo = Trinity::Containers::MapGetValuePtr(params, "Param_ClientInfo"))
    {
        ::JSON::RealmList::RealmListTicketClientInformation data;
        std::size_t jsonStart = clientInfo->blob_value().find(':');
        if (jsonStart != std::string::npos && ::JSON::Deserialize(clientInfo->blob_value().substr(jsonStart + 1), &data))
        {
            if (_clientSecret.size() == data.info().secret().size())
            {
                clientInfoOk = true;
                memcpy(_clientSecret.data(), data.info().secret().data(), _clientSecret.size());
            }

            _clientInfo = { .Platform = data.info().platformtype(), .Arch = data.info().clientarch(), .Type = data.info().type() };
        }
    }

    if (!clientInfoOk)
        return ERROR_WOW_SERVICES_DENIED_REALM_LIST_TICKET;

    LoginDatabasePreparedStatement* stmt = LoginDatabase.GetPreparedStatement(LOGIN_UPD_BNET_LAST_LOGIN_INFO);
    stmt->setString(0, GetRemoteIpAddress().to_string());
    stmt->setUInt8(1, GetLocaleByName(_locale));
    stmt->setString(2, _os);
    stmt->setUInt32(3, _accountInfo->Id);

    LoginDatabase.Execute(stmt);

    Attribute* attribute = response->add_attribute();
    attribute->set_name("Param_RealmListTicket");
    attribute->mutable_value()->set_blob_value("AuthRealmListTicket");

    return ERROR_OK;
}

uint32 Battlenet::Session::GetLastCharPlayed(std::unordered_map<std::string, Variant const*> const& params, game_utilities::v1::ClientResponse* response)
{
    if (Variant const* subRegion = Trinity::Containers::MapGetValuePtr(params, "Command_LastCharPlayedRequest_v1"))
    {
        auto lastPlayerChar = _gameAccountInfo->LastPlayedCharacters.find(subRegion->string_value());
        if (lastPlayerChar != _gameAccountInfo->LastPlayedCharacters.end())
        {
            std::vector<uint8> compressed = sRealmList->GetRealmEntryJSON(lastPlayerChar->second.RealmId, _build, _gameAccountInfo->SecurityLevel);

            if (compressed.empty())
                return ERROR_UTIL_SERVER_FAILED_TO_SERIALIZE_RESPONSE;

            Attribute* attribute = response->add_attribute();
            attribute->set_name("Param_RealmEntry");
            attribute->mutable_value()->set_blob_value(compressed.data(), compressed.size());

            attribute = response->add_attribute();
            attribute->set_name("Param_CharacterName");
            attribute->mutable_value()->set_string_value(lastPlayerChar->second.CharacterName);

            attribute = response->add_attribute();
            attribute->set_name("Param_CharacterGUID");
            attribute->mutable_value()->set_blob_value(&lastPlayerChar->second.CharacterGUID, sizeof(lastPlayerChar->second.CharacterGUID));

            attribute = response->add_attribute();
            attribute->set_name("Param_LastPlayedTime");
            attribute->mutable_value()->set_int_value(int32(lastPlayerChar->second.LastPlayedTime));
        }

        return ERROR_OK;
    }

    return ERROR_UTIL_SERVER_UNKNOWN_REALM;
}

uint32 Battlenet::Session::GetRealmList(std::unordered_map<std::string, Variant const*> const& params, game_utilities::v1::ClientResponse* response)
{
    if (!_gameAccountInfo)
        return ERROR_USER_SERVER_BAD_WOW_ACCOUNT;

    std::string subRegionId;
    if (Variant const* subRegion = Trinity::Containers::MapGetValuePtr(params, "Command_RealmListRequest_v1"))
        subRegionId = subRegion->string_value();

    std::vector<uint8> compressed = sRealmList->GetRealmList(_build, _gameAccountInfo->SecurityLevel, subRegionId);

    if (compressed.empty())
        return ERROR_UTIL_SERVER_FAILED_TO_SERIALIZE_RESPONSE;

    Attribute* attribute = response->add_attribute();
    attribute->set_name("Param_RealmList");
    attribute->mutable_value()->set_blob_value(compressed.data(), compressed.size());

    ::JSON::RealmList::RealmCharacterCountList realmCharacterCounts;
    for (auto const& characterCount : _gameAccountInfo->CharacterCounts)
    {
        ::JSON::RealmList::RealmCharacterCountEntry* countEntry = realmCharacterCounts.add_counts();
        countEntry->set_wowrealmaddress(characterCount.first);
        countEntry->set_count(characterCount.second);
    }

    std::string json = "JSONRealmCharacterCountList:" + ::JSON::Serialize(realmCharacterCounts);

    uLongf compressedLength = compressBound(json.length());
    compressed.resize(4 + compressedLength);
    *reinterpret_cast<uint32*>(compressed.data()) = json.length() + 1;

    if (compress(compressed.data() + 4, &compressedLength, reinterpret_cast<uint8 const*>(json.c_str()), json.length() + 1) != Z_OK)
        return ERROR_UTIL_SERVER_FAILED_TO_SERIALIZE_RESPONSE;

    attribute = response->add_attribute();
    attribute->set_name("Param_CharacterCountList");
    attribute->mutable_value()->set_blob_value(compressed.data(), compressedLength + 4);
    return ERROR_OK;
}

uint32 Battlenet::Session::JoinRealm(std::unordered_map<std::string, Variant const*> const& params, game_utilities::v1::ClientResponse* response)
{
    if (Variant const* realmAddress = Trinity::Containers::MapGetValuePtr(params, "Param_RealmAddress"))
        return sRealmList->JoinRealm(realmAddress->uint_value(), _build, _clientInfo, GetRemoteIpAddress(), _clientSecret, GetLocaleByName(_locale),
            _os, _timezoneOffset, _gameAccountInfo->Name, _gameAccountInfo->SecurityLevel, response);

    return ERROR_WOW_SERVICES_INVALID_JOIN_TICKET;
}

uint32 Battlenet::Session::HandleGetAllValuesForAttribute(game_utilities::v1::GetAllValuesForAttributeRequest const* request, game_utilities::v1::GetAllValuesForAttributeResponse* response)
{
    if (!_authed)
        return ERROR_DENIED;

    if (request->attribute_key().find("Command_RealmListRequest_v1") == 0)
    {
        sRealmList->WriteSubRegions(response);
        return ERROR_OK;
    }

    return ERROR_RPC_NOT_IMPLEMENTED;
}

template<bool(Battlenet::Session::*processMethod)(), MessageBuffer Battlenet::Session::*outputBuffer>
static inline Optional<Trinity::Net::SocketReadCallbackResult> PartialProcessPacket(Battlenet::Session* session, MessageBuffer& inputBuffer)
{
    MessageBuffer& buffer = session->*outputBuffer;

    // We have full read header, now check the data payload
    if (buffer.GetRemainingSpace() > 0)
    {
        // need more data in the payload
        std::size_t readDataSize = std::min(inputBuffer.GetActiveSize(), buffer.GetRemainingSpace());
        buffer.Write(inputBuffer.GetReadPointer(), readDataSize);
        inputBuffer.ReadCompleted(readDataSize);
    }
    else
        return { }; // go to next buffer

    if (buffer.GetRemainingSpace() > 0)
    {
        // Couldn't receive the whole data this time.
        ASSERT(inputBuffer.GetActiveSize() == 0);
        return Trinity::Net::SocketReadCallbackResult::KeepReading;
    }

    // just received fresh new payload
    if (!(session->*processMethod)())
    {
        session->CloseSocket();
        return Trinity::Net::SocketReadCallbackResult::Stop;
    }

    return { }; // go to next buffer
}

Trinity::Net::SocketReadCallbackResult Battlenet::Session::ReadHandler()
{
    MessageBuffer& packet = GetReadBuffer();
    while (packet.GetActiveSize() > 0)
    {
        if (Optional<Trinity::Net::SocketReadCallbackResult> partialResult = PartialProcessPacket<&Session::ReadHeaderLengthHandler, &Session::_headerLengthBuffer>(this, packet))
            return *partialResult;

        if (Optional<Trinity::Net::SocketReadCallbackResult> partialResult = PartialProcessPacket<&Session::ReadHeaderHandler, &Session::_headerBuffer>(this, packet))
            return *partialResult;

        if (Optional<Trinity::Net::SocketReadCallbackResult> partialResult = PartialProcessPacket<&Session::ReadDataHandler, &Session::_packetBuffer>(this, packet))
            return *partialResult;

        _headerLengthBuffer.Reset();
        _headerBuffer.Reset();
    }

    return Trinity::Net::SocketReadCallbackResult::KeepReading;
}

bool Battlenet::Session::ReadHeaderLengthHandler()
{
    uint16 len = *reinterpret_cast<uint16*>(_headerLengthBuffer.GetReadPointer());
    EndianConvertReverse(len);
    _headerBuffer.Resize(len);
    return true;
}

bool Battlenet::Session::ReadHeaderHandler()
{
    Header header;
    if (!header.ParseFromArray(_headerBuffer.GetReadPointer(), _headerBuffer.GetActiveSize()))
        return false;

    _packetBuffer.Resize(header.size());
    return true;
}

bool Battlenet::Session::ReadDataHandler()
{
    Header header;
    bool parseSuccess = header.ParseFromArray(_headerBuffer.GetReadPointer(), _headerBuffer.GetActiveSize());
    ASSERT(parseSuccess);

    if (header.service_id() != 0xFE)
    {
        sServiceDispatcher.Dispatch(this, header.service_hash(), header.token(), header.method_id(), std::move(_packetBuffer));
    }
    else
    {
        auto itr = _responseCallbacks.find(header.token());
        if (itr != _responseCallbacks.end())
        {
            itr->second(std::move(_packetBuffer));
            _responseCallbacks.erase(header.token());
        }
        else
            _packetBuffer.Reset();
    }

    return true;
}

std::string Battlenet::Session::GetClientInfo() const
{
    std::ostringstream stream;
    stream << '[' << GetRemoteIpAddress() << ':' << GetRemotePort();
    if (_accountInfo && !_accountInfo->Login.empty())
        stream << ", Account: " << _accountInfo->Login;

    if (_gameAccountInfo)
        stream << ", Game account: " << _gameAccountInfo->Name;

    stream << ']';

    return std::move(stream).str();
}
