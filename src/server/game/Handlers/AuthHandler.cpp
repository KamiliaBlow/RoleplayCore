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

#include "WorldSession.h"
#include "AuthenticationPackets.h"
#include "BattlenetRpcErrorCodes.h"
#include "CharacterTemplateDataStore.h"
#include "ClientConfigPackets.h"
#include "DisableMgr.h"
#include "GameTime.h"
#include "ObjectMgr.h"
#include "RBAC.h"
#include "RealmList.h"
#include "SystemPackets.h"
#include "Timezone.h"
#include "Util.h"
#include "World.h"

void WorldSession::SendAuthResponse(uint32 code, bool queued, uint32 queuePos)
{
    WorldPackets::Auth::AuthResponse response;
    response.Result = code;

    if (code == ERROR_OK)
    {
        response.SuccessInfo.emplace();

        response.SuccessInfo->ActiveExpansionLevel = GetExpansion();
        response.SuccessInfo->AccountExpansionLevel = GetAccountExpansion();
        response.SuccessInfo->Time = int32(GameTime::GetGameTime());

        // Send current home realm. Also there is no need to send it later in realm queries.
        if (std::shared_ptr<Realm const> currentRealm = sRealmList->GetCurrentRealm())
        {
            response.SuccessInfo->VirtualRealmAddress = currentRealm->Id.GetAddress();
            response.SuccessInfo->VirtualRealms.emplace_back(currentRealm->Id.GetAddress(), true, false, currentRealm->Name, currentRealm->NormalizedName);
        }

        if (HasPermission(rbac::RBAC_PERM_USE_CHARACTER_TEMPLATES))
            for (auto&& templ : sCharacterTemplateDataStore->GetCharacterTemplates())
                response.SuccessInfo->Templates.push_back(&templ.second);

        response.SuccessInfo->AvailableClasses = &sObjectMgr->GetClassExpansionRequirements();

        // TEMPORARY - prevent creating characters in uncompletable zone
        // This has the side effect of disabling Exile's Reach choice clientside without actually forcing character templates
        response.SuccessInfo->ForceCharacterTemplate = DisableMgr::IsDisabledFor(DISABLE_TYPE_MAP, 2175 /*Exile's Reach*/, nullptr);
    }

    if (queued)
    {
        response.WaitInfo.emplace();
        response.WaitInfo->WaitCount = queuePos;
    }

    SendPacket(response.Write());
}

void WorldSession::SendAuthWaitQueue(uint32 position)
{
    if (position)
    {
        WorldPackets::Auth::WaitQueueUpdate waitQueueUpdate;
        waitQueueUpdate.WaitInfo.WaitCount = position;
        waitQueueUpdate.WaitInfo.WaitTime = 0;
        waitQueueUpdate.WaitInfo.HasFCM = false;
        SendPacket(waitQueueUpdate.Write());
    }
    else
        SendPacket(WorldPackets::Auth::WaitQueueFinish().Write());
}

void WorldSession::SendClientCacheVersion(uint32 version)
{
    WorldPackets::ClientConfig::ClientCacheVersion cache;
    cache.CacheVersion = version;

    SendPacket(cache.Write());
}

void WorldSession::SendSetTimeZoneInformation()
{
    Minutes timezoneOffset = Trinity::Timezone::GetSystemZoneOffset(false);
    std::string realTimezone = Trinity::Timezone::GetSystemZoneName();
    std::string_view clientSupportedTZ = Trinity::Timezone::FindClosestClientSupportedTimezone(realTimezone, timezoneOffset);

    WorldPackets::System::SetTimeZoneInformation packet;
    packet.ServerTimeTZ = clientSupportedTZ;
    packet.GameTimeTZ = clientSupportedTZ;
    packet.ServerRegionalTimeTZ = clientSupportedTZ;
    SendPacket(packet.Write());
}

void WorldSession::SendFeatureSystemStatusGlueScreen()
{
    WorldPackets::System::FeatureSystemStatusGlueScreen features;
    features.BpayStoreAvailable = false;
    features.BpayStoreDisabledByParentalControls = false;
    features.CharUndeleteEnabled = sWorld->getBoolConfig(CONFIG_FEATURE_SYSTEM_CHARACTER_UNDELETE_ENABLED);
    features.MaxCharactersOnThisRealm = sWorld->getIntConfig(CONFIG_CHARACTERS_PER_REALM);
    features.MinimumExpansionLevel = EXPANSION_CLASSIC;
    features.MaximumExpansionLevel = sWorld->getIntConfig(CONFIG_EXPANSION);

    features.EuropaTicketSystemStatus.emplace();
    features.EuropaTicketSystemStatus->ThrottleState.MaxTries = 10;
    features.EuropaTicketSystemStatus->ThrottleState.PerMilliseconds = 60000;
    features.EuropaTicketSystemStatus->ThrottleState.TryCount = 1;
    features.EuropaTicketSystemStatus->ThrottleState.LastResetTimeBeforeNow = 111111;
    features.EuropaTicketSystemStatus->TicketsEnabled = sWorld->getBoolConfig(CONFIG_SUPPORT_TICKETS_ENABLED);
    features.EuropaTicketSystemStatus->BugsEnabled = sWorld->getBoolConfig(CONFIG_SUPPORT_BUGS_ENABLED);
    features.EuropaTicketSystemStatus->ComplaintsEnabled = sWorld->getBoolConfig(CONFIG_SUPPORT_COMPLAINTS_ENABLED);
    features.EuropaTicketSystemStatus->SuggestionsEnabled = sWorld->getBoolConfig(CONFIG_SUPPORT_SUGGESTIONS_ENABLED);

    for (World::GameRule const& gameRule : sWorld->GetGameRules())
    {
        WorldPackets::System::GameRuleValuePair& rule = features.GameRules.emplace_back();
        rule.Rule = AsUnderlyingType(gameRule.Rule);
        std::visit([&]<typename T>(T value)
        {
            if constexpr (std::is_same_v<T, float>)
                rule.ValueF = value;
            else
                rule.Value = value;
        }, gameRule.Value);
    }

    features.AvailableGameModeIDs.push_back(8); // GameMode.db2, standard

    SendPacket(features.Write());

    WorldPackets::System::MirrorVarSingle vars[] =
    {
        { "raidLockoutExtendEnabled"sv, "1"sv },
        { "sellAllJunkEnabled"sv, "1"sv },
        { "bypassItemLevelScalingCode"sv, "0"sv },
        { "shop2Enabled"sv, "0"sv },
        { "bpayStoreEnable"sv, "0"sv },
        { "recentAlliesEnabledClient"sv, "0"sv },
        { "browserEnabled"sv, "0"sv },
        // Housing game rules ? ALL values verified against 12.0.1.65940 sniff packet data (Feb 2026)
        // Service & feature flags (read from config, default true)
        { "performHousingExpansionCheckClient"sv, "1"sv },
        { "housingServiceEnabled"sv, "1"sv },
        { "housingEnableBuyHouse"sv, sWorld->getBoolConfig(CONFIG_HOUSING_ENABLE_BUY_HOUSE) ? "1"sv : "0"sv },
        { "housingEnableDeleteHouse"sv, sWorld->getBoolConfig(CONFIG_HOUSING_ENABLE_DELETE_HOUSE) ? "1"sv : "0"sv },
        { "housingEnableMoveHouse"sv, sWorld->getBoolConfig(CONFIG_HOUSING_ENABLE_MOVE_HOUSE) ? "1"sv : "0"sv },
        { "housingEnableCreateCharterNeighborhood"sv, sWorld->getBoolConfig(CONFIG_HOUSING_ENABLE_CREATE_CHARTER_NEIGHBORHOOD) ? "1"sv : "0"sv },
        { "housingEnableCreateGuildNeighborhood"sv, sWorld->getBoolConfig(CONFIG_HOUSING_ENABLE_CREATE_GUILD_NEIGHBORHOOD) ? "1"sv : "0"sv },
        // Market
        { "housingMarketEnabled"sv, "1"sv },
        { "housingMarketShopEnabled"sv, "1"sv },
        { "housingMarketCartFullRemoveEnabled"sv, "1"sv },
        // Neighborhood & exterior
        { "housingExteriorTypeByNeighborhoodFactionRestriction"sv, "1"sv },
        { "minNeighborhoodGroupMembers"sv, "3"sv },
        // Decoration limits
        { "housingBasicDecor_MaxPreviewLimit"sv, "100"sv },
        { "housingCatalog_CartSizeLimit"sv, "20"sv },
        // Decor scale limits
        { "housingExpertDecor_Scale_Indoor_Min"sv, "0.200000"sv },
        { "housingExpertDecor_Scale_Indoor_Max"sv, "2.000000"sv },
        { "housingExpertDecor_Scale_Outdoor_Min"sv, "0.200000"sv },
        { "housingExpertDecor_Scale_Outdoor_Max"sv, "2.000000"sv },
        // Screenshot report thresholds
        { "housingDecorReportScreenshotFacingDotThreshold"sv, "0.500000"sv },
        { "housingDecorReportScreenshotDistanceThreshold"sv, "150.000000"sv },
        // Market telemetry throttles ? sniff-verified against build 12.0.1.66838,
        // SMSG_MIRROR_VARS at packet idx 9976 (dump_12.0.1.66838_2026-04-15_09-35-59).
        // The client reads these before it will send any SMSG_HOUSING_MARKET_*
        // telemetry CMSGs; without them the market UI may throttle-fail silently.
        { "housingMarketViewInStoreTelemThrottle"sv, "5"sv },
        { "housingMarketViewBundleTelemThrottle"sv, "10"sv },
        { "housingMarketAddToCartTelemThrottle"sv, "15"sv },
        { "housingMarketClearCartTelemThrottle"sv, "5"sv },
        { "housingMarketRemoveFromCartTelemThrottle"sv, "20"sv },
        { "housingMarketThrottleTimePeriodMs"sv, "10000"sv },
        // Situation flags ? driver context for the client's "situation" state
        // machine (automatic/manual triggered events). Retail sends all three
        // set on login; we were sending none. Same sniff reference.
        { "enableAutomaticSituations"sv, "1"sv },
        { "enableManualSituations"sv, "1"sv },
        { "enableTransmogUpdateSituation"sv, "1"sv },
        // Transmog system flags ? the client gates parts of the transmog UI
        // on these being present. Retail sends them at this stage of login.
        { "transmogEnableSystem"sv, "1"sv },
        { "transmogAllowArtifactOverride"sv, "1"sv },
        { "transmogAllowCanUseEverChanges"sv, "0"sv },
        { "transmogEnableOutfitPurchases"sv, "1"sv },
        { "transmogEnableOutfitSlotChanges"sv, "1"sv },
        // --- Additional vars from retail MIRROR_VARS audit (2026-04-21) ---
        // Retail sends 116 MIRROR_VARS entries, we were sending 41. These 40
        // below are the subset that drive client behaviour without requiring
        // Blizzard-specific service URLs (shop2 / Pinterest skipped).
        // Damage meter ? gates the in-game damage meter addon feature
        { "damageMeterCacheEnabled"sv, "1"sv },
        { "damageMeterProcessingEnabled"sv, "1"sv },
        // Addon chat restrictions ? affects WHISPER/GROUP addon message routing
        { "addonChatRestrictionsEnabled"sv, "1"sv },
        { "addonChatRestrictionsEnabledForOutgoingAddonMessages"sv, "1"sv },
        // Lua resource caps ? the client throttles AddOn resources when these
        // are present. Retail's caps, keep identical to not break addons.
        { "limitedLuaResourcesEnabled"sv, "0"sv },
        { "limitedLuaResourcesAddonCapacityAnim"sv, "5000"sv },
        { "limitedLuaResourcesAddonCapacityAnimGroup"sv, "2000"sv },
        { "limitedLuaResourcesAddonCapacityFont"sv, "300"sv },
        { "limitedLuaResourcesAddonCapacityFontString"sv, "5000"sv },
        { "limitedLuaResourcesAddonCapacityFrame"sv, "10000"sv },
        { "limitedLuaResourcesAddonCapacityTexture"sv, "40000"sv },
        { "limitedLuaResourcesAddonCapacityTimer"sv, "500"sv },
        { "limitedLuaResourcesGlobalCapacityAnim"sv, "50000"sv },
        { "limitedLuaResourcesGlobalCapacityAnimGroup"sv, "20000"sv },
        { "limitedLuaResourcesGlobalCapacityFont"sv, "3000"sv },
        { "limitedLuaResourcesGlobalCapacityFontString"sv, "50000"sv },
        { "limitedLuaResourcesGlobalCapacityFrame"sv, "100000"sv },
        { "limitedLuaResourcesGlobalCapacityTexture"sv, "400000"sv },
        { "limitedLuaResourcesGlobalCapacityTimer"sv, "500"sv },
        // Lua script throttling ? bucket limits per second / burst
        { "luaScriptBucketThrottleEnabled"sv, "1"sv },
        { "luaScriptBucketThrottleMaxMsBurstNormal"sv, "20000"sv },
        { "luaScriptBucketThrottleMaxMsBurstRestricted"sv, "1000"sv },
        { "luaScriptBucketThrottleMaxMsPerSecondNormal"sv, "2000"sv },
        { "luaScriptBucketThrottleMaxMsPerSecondRestricted"sv, "500"sv },
        // Hardcore mode throttling ? not used but sent for parity
        { "hardcoreScriptThrottlingEnabled"sv, "0"sv },
        // PvP training grounds ? the PvP duel area feature
        { "pvpTrainingGroundsEnabledClient"sv, "0"sv },
        // Recent allies request throttle
        { "recentAlliesRequestDataThrottle"sv, "5000"sv },
        // LFG text filters
        { "enableEndgameEditRestrictionsForLFGText"sv, "1"sv },
        // Disabled game modes (retail passes an empty string; keep empty)
        { "disabledGamemodes"sv, ""sv },
    };

    WorldPackets::System::MirrorVars variables;
    variables.Variables = vars;
    SendPacket(variables.Write());

    TC_LOG_INFO("housing", "<<< SMSG_MIRROR_VARS sent: housingServiceEnabled=1, MaxExpansionLevel={}, AccountExpansion={}",
        sWorld->getIntConfig(CONFIG_EXPANSION), GetAccountExpansion());
}
