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
#include "AccountMgr.h"
#include "AchievementMgr.h"
#include "AchievementPackets.h"
#include "AreaTriggerPackets.h"
#include "Battleground.h"
#include "CharacterPackets.h"
#include "Chat.h"
#include "CinematicMgr.h"
#include "ClientConfigPackets.h"
#include "Common.h"
#include "ConditionMgr.h"
#include "Conversation.h"
#include "ConversationAI.h"
#include "Corpse.h"
#include "DatabaseEnv.h"
#include "DB2Stores.h"
#include "GameTime.h"
#include "GossipDef.h"
#include "Group.h"
#include "Guild.h"
#include "GuildMgr.h"
#include "InstancePackets.h"
#include "InstanceScript.h"
#include "Language.h"
#include "Log.h"
#include "Map.h"
#include "MiscPackets.h"
#include "Object.h"
#include "ObjectAccessor.h"
#include "ObjectMgr.h"
#include "OutdoorPvP.h"
#include "PhasingHandler.h"
#include "Player.h"
#include "ReputationMgr.h"
#include "RestMgr.h"
#include "ScriptMgr.h"
#include "Spell.h"
#ifdef ELUNA
#include "LuaEngine.h"
#endif
#include "SpellPackets.h"
#include "WhoListStorage.h"
#include "WhoPackets.h"
#include "World.h"
#include <cstdarg>
#include <zlib.h>

void WorldSession::HandleRepopRequest(WorldPackets::Misc::RepopRequest& /*packet*/)
{
    if (GetPlayer()->IsAlive() || GetPlayer()->HasPlayerFlag(PLAYER_FLAGS_GHOST))
        return;

    if (GetPlayer()->HasAuraType(SPELL_AURA_PREVENT_RESURRECTION))
        return; // silently return, client should display the error by itself

    // the world update order is sessions, players, creatures
    // the netcode runs in parallel with all of these
    // creatures can kill players
    // so if the server is lagging enough the player can
    // release spirit after he's killed but before he is updated
    if (GetPlayer()->getDeathState() == JUST_DIED)
    {
        TC_LOG_DEBUG("network", "HandleRepopRequestOpcode: got request after player {} {} was killed and before he was updated",
            GetPlayer()->GetName(), GetPlayer()->GetGUID().ToString());
        GetPlayer()->KillPlayer();
    }

#ifdef ELUNA
    if (Eluna* e = GetPlayer()->GetEluna())
        e->OnRepop(GetPlayer());
#endif

    //this is spirit release confirm?
    GetPlayer()->RemovePet(nullptr, PET_SAVE_NOT_IN_SLOT, true);
    GetPlayer()->BuildPlayerRepop();
    GetPlayer()->RepopAtGraveyard();
}

void WorldSession::HandleWhoOpcode(WorldPackets::Who::WhoRequestPkt& whoRequest)
{
    WorldPackets::Who::WhoRequest& request = whoRequest.Request;

    TC_LOG_DEBUG("network", "WorldSession::HandleWhoOpcode: MinLevel: {}, MaxLevel: {}, Name: {} (VirtualRealmName: {}), Guild: {} (GuildVirtualRealmName: {}), RaceFilter: 0x{:X}{:08X}, ClassFilter: {}, Areas: {}, Words: {}.",
        request.MinLevel, request.MaxLevel, request.Name, request.VirtualRealmName, request.Guild, request.GuildVirtualRealmName,
        request.RaceFilter.RawValue[1], request.RaceFilter.RawValue[0], request.ClassFilter, whoRequest.Areas.size(), request.Words.size());

    // zones count, client limit = 10 (2.0.10)
    // can't be received from real client or broken packet
    if (whoRequest.Areas.size() > 10)
        return;

    // user entered strings count, client limit=4 (checked on 2.0.10)
    // can't be received from real client or broken packet
    if (request.Words.size() > 4)
        return;

    /// @todo: handle following packet values
    /// VirtualRealmNames
    /// ShowEnemies
    /// ShowArenaPlayers
    /// ExactName
    /// ServerInfo

    std::vector<std::wstring> wWords;
    wWords.resize(request.Words.size());
    for (size_t i = 0; i < request.Words.size(); ++i)
    {
        TC_LOG_DEBUG("network", "WorldSession::HandleWhoOpcode: Word: {}", request.Words[i].Word);

        // user entered string, it used as universal search pattern(guild+player name)?
        if (!Utf8toWStr(request.Words[i].Word, wWords[i]))
            continue;

        wstrToLower(wWords[i]);
    }

    std::wstring wPlayerName;
    std::wstring wGuildName;

    if (!(Utf8toWStr(request.Name, wPlayerName) && Utf8toWStr(request.Guild, wGuildName)))
        return;

    wstrToLower(wPlayerName);
    wstrToLower(wGuildName);

    // client send in case not set max level value 100 but Trinity supports 255 max level,
    // update it to show GMs with characters after 100 level
    if (whoRequest.Request.MaxLevel >= MAX_LEVEL)
        whoRequest.Request.MaxLevel = STRONG_MAX_LEVEL;

    uint32 team = _player->GetTeam();

    uint32 gmLevelInWhoList  = sWorld->getIntConfig(CONFIG_GM_LEVEL_IN_WHO_LIST);

    WorldPackets::Who::WhoResponsePkt response;
    response.Token = whoRequest.Token;

    WhoListInfoVector const& whoList = sWhoListStorageMgr->GetWhoList();
    for (WhoListPlayerInfo const& target : whoList)
    {
        // player can see member of other team only if has RBAC_PERM_TWO_SIDE_WHO_LIST
        if (target.GetTeam() != team && !HasPermission(rbac::RBAC_PERM_TWO_SIDE_WHO_LIST))
            continue;

        // player can see MODERATOR, GAME MASTER, ADMINISTRATOR only if has RBAC_PERM_WHO_SEE_ALL_SEC_LEVELS
        if (target.GetSecurity() > AccountTypes(gmLevelInWhoList) && !HasPermission(rbac::RBAC_PERM_WHO_SEE_ALL_SEC_LEVELS))
            continue;

        // check if target is globally visible for player
        if (_player->GetGUID() != target.GetGuid() && !target.IsVisible())
            if (AccountMgr::IsPlayerAccount(_player->GetSession()->GetSecurity()) || target.GetSecurity() > _player->GetSession()->GetSecurity())
                continue;

        // check if target's level is in level range
        uint8 lvl = target.GetLevel();
        if (lvl < request.MinLevel || lvl > request.MaxLevel)
            continue;

        // check if class matches classmask
        if (request.ClassFilter >= 0 && !(request.ClassFilter & (1 << target.GetClass())))
            continue;

        // check if race matches racemask
        if (!request.RaceFilter.HasRace(target.GetRace()))
            continue;

        if (!whoRequest.Areas.empty())
        {
            if (std::find(whoRequest.Areas.begin(), whoRequest.Areas.end(), int32(target.GetZoneId())) == whoRequest.Areas.end())
                continue;
        }

        std::wstring const& wTargetName = target.GetWidePlayerName();
        if (!(wPlayerName.empty() || wTargetName.find(wPlayerName) != std::wstring::npos))
            continue;

        std::wstring const& wTargetGuildName = target.GetWideGuildName();

        if (!wGuildName.empty() && wTargetGuildName.find(wGuildName) == std::wstring::npos)
            continue;

        if (!wWords.empty())
        {
            std::string aName;
            if (AreaTableEntry const* areaEntry = sAreaTableStore.LookupEntry(target.GetZoneId()))
                aName = areaEntry->AreaName[GetSessionDbcLocale()];

            bool show = false;
            for (size_t i = 0; i < wWords.size(); ++i)
            {
                if (!wWords[i].empty())
                {
                    if (wTargetName.find(wWords[i]) != std::wstring::npos ||
                        wTargetGuildName.find(wWords[i]) != std::wstring::npos ||
                        Utf8FitTo(aName, wWords[i]))
                    {
                        show = true;
                        break;
                    }
                }
            }

            if (!show)
                continue;
        }

        WorldPackets::Who::WhoEntry whoEntry;
        if (!whoEntry.PlayerData.Initialize(target.GetGuid(), nullptr))
            continue;

        if (!target.GetGuildGuid().IsEmpty())
        {
            whoEntry.GuildGUID = target.GetGuildGuid();
            whoEntry.GuildVirtualRealmAddress = GetVirtualRealmAddress();
            whoEntry.GuildName = target.GetGuildName();
        }

        whoEntry.AreaID = target.GetZoneId();
        whoEntry.IsGM = target.IsGameMaster();

        response.Response.Entries.push_back(whoEntry);

        // 50 is maximum player count sent to client - can be overridden
        // through config, but is unstable
        if (response.Response.Entries.size() >= sWorld->getIntConfig(CONFIG_MAX_WHO))
            break;
    }

    SendPacket(response.Write());
}

void WorldSession::HandleLogoutRequestOpcode(WorldPackets::Character::LogoutRequest& logoutRequest)
{
    if (!GetPlayer()->GetLootGUID().IsEmpty())
        GetPlayer()->SendLootReleaseAll();

    bool instantLogout = GetPlayer()->IsInFlight();
    if (!logoutRequest.IdleLogout)
        instantLogout |= (GetPlayer()->HasPlayerFlag(PLAYER_FLAGS_RESTING) && !GetPlayer()->IsInCombat())
            || HasPermission(rbac::RBAC_PERM_INSTANT_LOGOUT);

    /// TODO: Possibly add RBAC permission to log out in combat
    bool canLogoutInCombat = GetPlayer()->HasPlayerFlag(PLAYER_FLAGS_RESTING);

    uint32 reason = 0;
    if (GetPlayer()->IsInCombat() && !canLogoutInCombat)
        reason = 1;
    else if (GetPlayer()->IsFalling())
        reason = 3;                                         // is jumping or falling
    else if (GetPlayer()->duel || GetPlayer()->HasAura(9454)) // is dueling or frozen by GM via freeze command
        reason = 2;                                         // FIXME - Need the correct value

    WorldPackets::Character::LogoutResponse logoutResponse;
    logoutResponse.LogoutResult = reason;
    logoutResponse.Instant = instantLogout;
    SendPacket(logoutResponse.Write());

    if (reason)
    {
        SetLogoutStartTime(0);
        return;
    }

    // instant logout in taverns/cities or on taxi or for admins, gm's, mod's if its enabled in worldserver.conf
    if (instantLogout)
    {
        LogoutPlayer(true);
        return;
    }

    // not set flags if player can't free move to prevent lost state at logout cancel
    if (GetPlayer()->CanFreeMove())
    {
        if (GetPlayer()->GetStandState() == UNIT_STAND_STATE_STAND)
            GetPlayer()->SetStandState(UNIT_STAND_STATE_SIT);
        GetPlayer()->SetRooted(true);
        GetPlayer()->SetUnitFlag(UNIT_FLAG_STUNNED);
    }

    SetLogoutStartTime(GameTime::GetGameTime());
}

void WorldSession::HandleLogoutCancelOpcode(WorldPackets::Character::LogoutCancel& /*logoutCancel*/)
{
    // Player have already logged out serverside, too late to cancel
    if (!GetPlayer())
        return;

    SetLogoutStartTime(0);

    SendPacket(WorldPackets::Character::LogoutCancelAck().Write());

    // not remove flags if can't free move - its not set in Logout request code.
    if (GetPlayer()->CanFreeMove())
    {
        //!we can move again
        GetPlayer()->SetRooted(false);

        //! Stand Up
        GetPlayer()->SetStandState(UNIT_STAND_STATE_STAND);

        //! DISABLE_ROTATE
        GetPlayer()->RemoveUnitFlag(UNIT_FLAG_STUNNED);
    }
}

void WorldSession::HandleTogglePvP(WorldPackets::Misc::TogglePvP& /*packet*/)
{
    if (!GetPlayer()->HasPlayerFlag(PLAYER_FLAGS_IN_PVP))
    {
        GetPlayer()->SetPlayerFlag(PLAYER_FLAGS_IN_PVP);
        GetPlayer()->RemovePlayerFlag(PLAYER_FLAGS_PVP_TIMER);
        if (!GetPlayer()->IsPvP() || GetPlayer()->pvpInfo.EndTimer)
            GetPlayer()->UpdatePvP(true, true);
    }
    else if (!GetPlayer()->IsWarModeLocalActive())
    {
        GetPlayer()->RemovePlayerFlag(PLAYER_FLAGS_IN_PVP);
        GetPlayer()->SetPlayerFlag(PLAYER_FLAGS_PVP_TIMER);
        if (!GetPlayer()->pvpInfo.IsHostile && GetPlayer()->IsPvP())
            GetPlayer()->pvpInfo.EndTimer = GameTime::GetGameTime();     // start toggle-off
    }
}

void WorldSession::HandleSetPvP(WorldPackets::Misc::SetPvP& packet)
{
    if (packet.EnablePVP)
    {
        GetPlayer()->SetPlayerFlag(PLAYER_FLAGS_IN_PVP);
        GetPlayer()->RemovePlayerFlag(PLAYER_FLAGS_PVP_TIMER);
        if (!GetPlayer()->IsPvP() || GetPlayer()->pvpInfo.EndTimer)
            GetPlayer()->UpdatePvP(true, true);
    }
    else if (!GetPlayer()->IsWarModeLocalActive())
    {
        GetPlayer()->RemovePlayerFlag(PLAYER_FLAGS_IN_PVP);
        GetPlayer()->SetPlayerFlag(PLAYER_FLAGS_PVP_TIMER);
        if (!GetPlayer()->pvpInfo.IsHostile && GetPlayer()->IsPvP())
            GetPlayer()->pvpInfo.EndTimer = GameTime::GetGameTime();     // start toggle-off
    }
}

void WorldSession::HandleSetWarMode(WorldPackets::Misc::SetWarMode& packet)
{
    _player->SetWarModeDesired(packet.Enable);
}

void WorldSession::HandlePortGraveyard(WorldPackets::Misc::PortGraveyard& /*packet*/)
{
    if (GetPlayer()->IsAlive() || !GetPlayer()->HasPlayerFlag(PLAYER_FLAGS_GHOST))
        return;
    GetPlayer()->RepopAtGraveyard();
}

void WorldSession::HandleRequestCemeteryList(WorldPackets::Misc::RequestCemeteryList& /*packet*/)
{
    uint32 zoneId = _player->GetZoneId();
    uint32 team = _player->GetTeam();

    std::vector<uint32> graveyardIds;
    auto range = sObjectMgr->GraveyardStore.equal_range(zoneId);

    for (auto it = range.first; it != range.second && graveyardIds.size() < 16; ++it) // client max
    {
        ConditionSourceInfo conditionSource(_player);
        if (!it->second.Conditions.Meets(conditionSource))
            continue;

        graveyardIds.push_back(it->first);
    }

    if (graveyardIds.empty())
    {
        TC_LOG_DEBUG("network", "No graveyards found for zone {} for {} (team {}) in CMSG_REQUEST_CEMETERY_LIST",
            zoneId, _player->GetGUID().ToString(), team);
        return;
    }

    WorldPackets::Misc::RequestCemeteryListResponse packet;
    packet.IsGossipTriggered = false;
    packet.CemeteryID.reserve(graveyardIds.size());

    for (uint32 id : graveyardIds)
        packet.CemeteryID.push_back(id);

    SendPacket(packet.Write());
}

void WorldSession::HandleSetSelectionOpcode(WorldPackets::Misc::SetSelection& packet)
{
#ifndef DISABLE_DRESSNPCS_CORESOUNDS
    if (packet.Selection.IsAnyTypeCreature())
        if (Creature* creature = _player->GetMap()->GetCreature(packet.Selection))
            creature->SendMirrorSound(_player, 0);
#endif
    _player->SetSelection(packet.Selection);
}

void WorldSession::HandleStandStateChangeOpcode(WorldPackets::Misc::StandStateChange& packet)
{
    switch (packet.StandState)
    {
        case UNIT_STAND_STATE_STAND:
        case UNIT_STAND_STATE_SIT:
        case UNIT_STAND_STATE_SLEEP:
        case UNIT_STAND_STATE_KNEEL:
            break;
        default:
            return;
    }

    _player->SetStandState(packet.StandState);
}

void WorldSession::HandleReclaimCorpse(WorldPackets::Misc::ReclaimCorpse& /*packet*/)
{
    if (_player->IsAlive())
        return;

    // do not allow corpse reclaim in arena
    if (_player->InArena())
        return;

    // body not released yet
    if (!_player->HasPlayerFlag(PLAYER_FLAGS_GHOST))
        return;

    Corpse* corpse = _player->GetCorpse();
    if (!corpse)
        return;

    // prevent resurrect before 30-sec delay after body release not finished
    if (time_t(corpse->GetGhostTime() + _player->GetCorpseReclaimDelay(corpse->GetType() == CORPSE_RESURRECTABLE_PVP)) > time_t(GameTime::GetGameTime()))
        return;

    if (!corpse->IsWithinDistInMap(_player, CORPSE_RECLAIM_RADIUS, true))
        return;

    // resurrect
    _player->ResurrectPlayer(_player->InBattleground() ? 1.0f : 0.5f);

    // spawn bones
    _player->SpawnCorpseBones();
}

void WorldSession::HandleResurrectResponse(WorldPackets::Misc::ResurrectResponse& packet)
{
    if (GetPlayer()->IsAlive())
        return;

    if (packet.Response != 0) // Accept = 0 Decline = 1 Timeout = 2
    {
        GetPlayer()->ClearResurrectRequestData();           // reject
        return;
    }

    if (!GetPlayer()->IsResurrectRequestedBy(packet.Resurrecter))
        return;

    if (Player* ressPlayer = ObjectAccessor::GetPlayer(*GetPlayer(), packet.Resurrecter))
    {
        if (InstanceScript* instance = ressPlayer->GetInstanceScript())
        {
            if (instance->IsEncounterInProgress())
            {
                if (!instance->GetCombatResurrectionCharges())
                    return;
                else
                    instance->UseCombatResurrection();
            }
        }
    }

    GetPlayer()->ResurrectUsingRequestData();
}

void WorldSession::HandleAreaTriggerOpcode(WorldPackets::AreaTrigger::AreaTrigger& packet)
{
    Player* player = GetPlayer();
    if (player->IsInFlight())
    {
        TC_LOG_DEBUG("network", "HandleAreaTriggerOpcode: Player '{}' {} in flight, ignore Area Trigger ID: {}",
            player->GetName(), player->GetGUID().ToString(), packet.AreaTriggerID);
        return;
    }

    AreaTriggerEntry const* atEntry = sAreaTriggerStore.LookupEntry(packet.AreaTriggerID);
    if (!atEntry)
    {
        TC_LOG_DEBUG("network", "HandleAreaTriggerOpcode: Player '{}' {} send unknown (by DBC) Area Trigger ID: {}",
            player->GetName(), player->GetGUID().ToString(), packet.AreaTriggerID);
        return;
    }

    if (packet.Entered != player->IsInAreaTrigger(atEntry))
    {
        TC_LOG_DEBUG("network", "HandleAreaTriggerOpcode: Player '{}' {} too far, ignore Area Trigger ID: {}",
            player->GetName(), player->GetGUID().ToString(), packet.AreaTriggerID);
        return;
    }

    if (player->isDebugAreaTriggers)
        ChatHandler(player->GetSession()).PSendSysMessage(packet.Entered ? LANG_DEBUG_AREATRIGGER_ENTERED : LANG_DEBUG_AREATRIGGER_LEFT, packet.AreaTriggerID);

    if (!sConditionMgr->IsObjectMeetingNotGroupedConditions(CONDITION_SOURCE_TYPE_AREATRIGGER_CLIENT_TRIGGERED, atEntry->ID, player))
        return;

    if (sScriptMgr->OnAreaTrigger(player, atEntry, packet.Entered))
        return;

    if (atEntry->AreaTriggerActionSetID)
    {
        if (packet.Entered)
            player->UpdateCriteria(CriteriaType::EnterAreaTriggerWithActionSet, atEntry->AreaTriggerActionSetID);
        else
            player->UpdateCriteria(CriteriaType::LeaveAreaTriggerWithActionSet, atEntry->AreaTriggerActionSetID);
    }

    if (player->IsAlive() && packet.Entered)
    {
        // not using Player::UpdateQuestObjectiveProgress, ObjectID in quest_objectives can be set to -1, areatrigger_involvedrelation then holds correct id
        if (std::unordered_set<uint32> const* quests = sObjectMgr->GetQuestsForAreaTrigger(packet.AreaTriggerID))
        {
            bool anyObjectiveChangedCompletionState = false;
            for (uint32 questId : *quests)
            {
                Quest const* qInfo = sObjectMgr->GetQuestTemplate(questId);
                uint16 slot = player->FindQuestSlot(questId);
                if (qInfo && slot < MAX_QUEST_LOG_SIZE && player->GetQuestStatus(questId) == QUEST_STATUS_INCOMPLETE)
                {
                    for (QuestObjective const& obj : qInfo->Objectives)
                    {
                        if (obj.Type != QUEST_OBJECTIVE_AREATRIGGER)
                            continue;

                        if (!player->IsQuestObjectiveCompletable(slot, qInfo, obj))
                            continue;

                        if (player->IsQuestObjectiveComplete(slot, qInfo, obj))
                            continue;

                        if (obj.ObjectID != -1 && obj.ObjectID != packet.AreaTriggerID)
                            continue;

                        player->SetQuestObjectiveData(obj, 1);
                        player->SendQuestUpdateAddCreditSimple(obj);
                        anyObjectiveChangedCompletionState = true;
                        break;
                    }

                    if (qInfo->HasFlag(QUEST_FLAGS_COMPLETION_AREA_TRIGGER))
                        player->AreaExploredOrEventHappens(questId);

                    if (player->CanCompleteQuest(questId))
                        player->CompleteQuest(questId);
                }
            }

            if (anyObjectiveChangedCompletionState)
                player->UpdateVisibleObjectInteractions(true, false, false, true);
        }
    }

    if (sObjectMgr->IsTavernAreaTrigger(packet.AreaTriggerID))
    {
        // set resting flag we are in the inn
        if (packet.Entered)
        {
            player->GetRestMgr().SetInnTrigger(InnAreaTrigger{ .IsDBC = true, .AreaTriggerEntryId = atEntry->ID });
        }
        else
        {
            player->GetRestMgr().RemoveRestFlag(REST_FLAG_IN_TAVERN);
            player->GetRestMgr().SetInnTrigger(std::nullopt);
        }

        if (sWorld->IsFFAPvPRealm())
        {
            if (packet.Entered)
                player->RemovePvpFlag(UNIT_BYTE2_FLAG_FFA_PVP);
            else
                player->SetPvpFlag(UNIT_BYTE2_FLAG_FFA_PVP);
        }

        return;
    }

    if (OutdoorPvP* pvp = player->GetOutdoorPvP())
        if (pvp->HandleAreaTrigger(_player, packet.AreaTriggerID, packet.Entered))
            return;

    if (!packet.Entered)
        return;

    AreaTriggerTeleport const* at = sObjectMgr->GetAreaTrigger(packet.AreaTriggerID);
    if (!at)
        return;

    bool teleported = false;
    if (player->GetMapId() != at->Loc.GetMapId())
    {
        if (!player->IsAlive())
        {
            if (player->HasCorpse())
            {
                // let enter in ghost mode in instance that connected to inner instance with corpse
                uint32 corpseMap = player->GetCorpseLocation().GetMapId();
                do
                {
                    if (corpseMap == at->Loc.GetMapId())
                        break;

                    InstanceTemplate const* corpseInstance = sObjectMgr->GetInstanceTemplate(corpseMap);
                    corpseMap = corpseInstance ? corpseInstance->Parent : 0;
                } while (corpseMap);

                if (!corpseMap)
                {
                    SendPacket(WorldPackets::AreaTrigger::AreaTriggerNoCorpse().Write());
                    return;
                }

                TC_LOG_DEBUG("maps", "MAP: Player '{}' has corpse in instance {} and can enter.", player->GetName(), at->Loc.GetMapId());
            }
            else
            {
                TC_LOG_DEBUG("maps", "Map::CanPlayerEnter - player '{}' is dead but does not have a corpse!", player->GetName());
                SendPacket(WorldPackets::AreaTrigger::AreaTriggerNoCorpse().Write());
                return;
            }
        }

        if (TransferAbortParams denyReason = Map::PlayerCannotEnter(at->Loc.GetMapId(), player))
        {
            switch (denyReason.Reason)
            {
                case TRANSFER_ABORT_MAP_NOT_ALLOWED:
                    TC_LOG_DEBUG("maps", "MAP: Player '{}' attempted to enter map with id {} which has no entry", player->GetName(), at->Loc.GetMapId());
                    break;
                case TRANSFER_ABORT_DIFFICULTY:
                    TC_LOG_DEBUG("maps", "MAP: Player '{}' attempted to enter instance map {} but the requested difficulty was not found", player->GetName(), at->Loc.GetMapId());
                    break;
                case TRANSFER_ABORT_NEED_GROUP:
                    TC_LOG_DEBUG("maps", "MAP: Player '{}' must be in a raid group to enter map {}", player->GetName(), at->Loc.GetMapId());
                    player->SendRaidGroupOnlyMessage(RAID_GROUP_ERR_ONLY, 0);
                    break;
                case TRANSFER_ABORT_LOCKED_TO_DIFFERENT_INSTANCE:
                    TC_LOG_DEBUG("maps", "MAP: Player '{}' cannot enter instance map {} because their permanent bind is incompatible with their group's", player->GetName(), at->Loc.GetMapId());
                    break;
                case TRANSFER_ABORT_ALREADY_COMPLETED_ENCOUNTER:
                    TC_LOG_DEBUG("maps", "MAP: Player '{}' cannot enter instance map {} because their permanent bind is incompatible with their group's", player->GetName(), at->Loc.GetMapId());
                    break;
                case TRANSFER_ABORT_TOO_MANY_INSTANCES:
                    TC_LOG_DEBUG("maps", "MAP: Player '{}' cannot enter instance map {} because he has exceeded the maximum number of instances per hour.", player->GetName(), at->Loc.GetMapId());
                    break;
                case TRANSFER_ABORT_MAX_PLAYERS:
                    break;
                case TRANSFER_ABORT_ZONE_IN_COMBAT:
                    break;
                case TRANSFER_ABORT_NOT_FOUND:
                    TC_LOG_DEBUG("maps", "MAP: Player '{}' cannot enter instance map {} because instance is resetting.", player->GetName(), at->Loc.GetMapId());
                    break;
                default:
                    break;
            }

            if (denyReason.Reason != TRANSFER_ABORT_NEED_GROUP)
                player->SendTransferAborted(at->Loc.GetMapId(), denyReason.Reason, denyReason.Arg, denyReason.MapDifficultyXConditionId);

            if (!player->IsAlive() && player->HasCorpse())
            {
                if (player->GetCorpseLocation().GetMapId() == at->Loc.GetMapId())
                {
                    player->ResurrectPlayer(0.5f);
                    player->SpawnCorpseBones();
                }
            }

            return;
        }

        if (Group* group = player->GetGroup())
            if (group->isLFGGroup() && player->GetMap()->IsDungeon())
                teleported = player->TeleportToBGEntryPoint();
    }

    if (!teleported)
    {
        WorldSafeLocsEntry const* entranceLocation = player->GetInstanceEntrance(at->Loc.GetMapId());
        if (entranceLocation && player->GetMapId() != at->Loc.GetMapId())
            player->TeleportTo(entranceLocation->Loc, TELE_TO_NOT_LEAVE_TRANSPORT);
        else
            player->TeleportTo(at->Loc, TELE_TO_NOT_LEAVE_TRANSPORT);
    }
}

void WorldSession::HandleUpdateAccountData(WorldPackets::ClientConfig::UserClientUpdateAccountData& packet)
{
    TC_LOG_DEBUG("network", "WORLD: Received CMSG_UPDATE_ACCOUNT_DATA: type {}, time {}, decompressedSize {}",
        packet.DataType, packet.Time.AsUnderlyingType(), packet.Size);

    if (packet.DataType >= NUM_ACCOUNT_DATA_TYPES)
        return;

    if (packet.Size == 0)                               // erase
    {
        SetAccountData(AccountDataType(packet.DataType), 0, "");

        WorldPackets::ClientConfig::UpdateAccountDataComplete updateAccountDataComplete;
        updateAccountDataComplete.Player = packet.PlayerGuid;
        updateAccountDataComplete.DataType = packet.DataType;
        updateAccountDataComplete.Result = 0;
        SendPacket(updateAccountDataComplete.Write());

        return;
    }

    if (packet.Size > 0xFFFF)
    {
        TC_LOG_ERROR("network", "UAD: Account data packet too big, size {}", packet.Size);
        return;
    }

    std::string dest;
    dest.resize(packet.Size);

    uLongf realSize = packet.Size;
    if (uncompress(reinterpret_cast<Bytef*>(dest.data()), &realSize, packet.CompressedData.data(), packet.CompressedData.size()) != Z_OK)
    {
        TC_LOG_ERROR("network", "UAD: Failed to decompress account data");
        return;
    }

    SetAccountData(AccountDataType(packet.DataType), packet.Time, dest);

    WorldPackets::ClientConfig::UpdateAccountDataComplete updateAccountDataComplete;
    updateAccountDataComplete.Player = packet.PlayerGuid;
    updateAccountDataComplete.DataType = packet.DataType;
    updateAccountDataComplete.Result = 0;
    SendPacket(updateAccountDataComplete.Write());
}

void WorldSession::HandleRequestAccountData(WorldPackets::ClientConfig::RequestAccountData& request)
{
    TC_LOG_DEBUG("network", "WORLD: Received CMSG_REQUEST_ACCOUNT_DATA: type {}", request.DataType);

    if (request.DataType >= NUM_ACCOUNT_DATA_TYPES)
        return;

    AccountData const* adata = GetAccountData(AccountDataType(request.DataType));

    WorldPackets::ClientConfig::UpdateAccountData data;
    data.Player = _player ? _player->GetGUID() : ObjectGuid::Empty;
    data.Time = adata->Time;
    data.Size = adata->Data.size();
    data.DataType = request.DataType;

    uLongf destSize = compressBound(data.Size);

    data.CompressedData.resize(destSize);

    if (data.Size && compress(data.CompressedData.data(), &destSize, (uint8 const*)adata->Data.c_str(), data.Size) != Z_OK)
    {
        TC_LOG_ERROR("network", "RAD: Failed to compress account data");
        return;
    }

    data.CompressedData.resize(destSize);

    SendPacket(data.Write());
}

void WorldSession::HandleSetActionButtonOpcode(WorldPackets::Spells::SetActionButton& packet)
{
    uint64 action = ACTION_BUTTON_ACTION(packet.Action);
    uint8 type = ACTION_BUTTON_TYPE(packet.Action);

    TC_LOG_DEBUG("network", "CMSG_SET_ACTION_BUTTON Button: {} Action: {} Type: {}", packet.Index, action, uint32(type));

    if (!packet.Action)
        GetPlayer()->RemoveActionButton(packet.Index);
    else
        GetPlayer()->AddActionButton(packet.Index, action, type);
}

void WorldSession::HandleCompleteCinematic(WorldPackets::Misc::CompleteCinematic& /*packet*/)
{
    // If player has sight bound to visual waypoint NPC we should remove it
    GetPlayer()->GetCinematicMgr()->EndCinematic();
}

void WorldSession::HandleNextCinematicCamera(WorldPackets::Misc::NextCinematicCamera& /*packet*/)
{
    // Sent by client when cinematic actually begun. So we begin the server side process
    GetPlayer()->GetCinematicMgr()->NextCinematicCamera();
}

void WorldSession::HandleCompleteMovie(WorldPackets::Misc::CompleteMovie& /*packet*/)
{
    uint32 movie = _player->GetMovie();
    if (!movie)
        return;

    _player->SetMovie(0);
    sScriptMgr->OnMovieComplete(_player, movie);
}

void WorldSession::HandleSetActionBarToggles(WorldPackets::Character::SetActionBarToggles& packet)
{
    if (!GetPlayer())                                        // ignore until not logged (check needed because STATUS_AUTHED)
    {
        if (packet.Mask != 0)
            TC_LOG_ERROR("network", "WorldSession::HandleSetActionBarToggles in not logged state with value: {}, ignored", uint32(packet.Mask));
        return;
    }

    GetPlayer()->SetMultiActionBars(packet.Mask);
}

void WorldSession::HandlePlayedTime(WorldPackets::Character::RequestPlayedTime& packet)
{
    WorldPackets::Character::PlayedTime playedTime;
    playedTime.TotalTime = _player->GetTotalPlayedTime();
    playedTime.LevelTime = _player->GetLevelPlayedTime();
    playedTime.TriggerEvent = packet.TriggerScriptEvent;  // 0-1 - will not show in chat frame
    SendPacket(playedTime.Write());
}

void WorldSession::HandleWhoIsOpcode(WorldPackets::Who::WhoIsRequest& packet)
{
    TC_LOG_DEBUG("network", "Received whois command from player {} for character {}",
        GetPlayer()->GetName(), packet.CharName);

    if (!HasPermission(rbac::RBAC_PERM_OPCODE_WHOIS))
    {
        SendNotification(LANG_YOU_NOT_HAVE_PERMISSION);
        return;
    }

    if (packet.CharName.empty() || !normalizePlayerName(packet.CharName))
    {
        SendNotification(LANG_NEED_CHARACTER_NAME);
        return;
    }

    Player* player = ObjectAccessor::FindConnectedPlayerByName(packet.CharName);
    if (!player)
    {
        SendNotification(LANG_PLAYER_NOT_EXIST_OR_OFFLINE, packet.CharName.c_str());
        return;
    }

    LoginDatabasePreparedStatement* stmt = LoginDatabase.GetPreparedStatement(LOGIN_SEL_ACCOUNT_WHOIS);
    stmt->setUInt32(0, player->GetSession()->GetAccountId());

    PreparedQueryResult result = LoginDatabase.Query(stmt);
    if (!result)
    {
        SendNotification(LANG_ACCOUNT_FOR_PLAYER_NOT_FOUND, packet.CharName.c_str());
        return;
    }

    Field* fields = result->Fetch();
    std::string acc = fields[0].GetString();
    if (acc.empty())
        acc = "Unknown";

    std::string email = fields[1].GetString();
    if (email.empty())
        email = "Unknown";

    std::string lastip = fields[2].GetString();
    if (lastip.empty())
        lastip = "Unknown";

    WorldPackets::Who::WhoIsResponse response;
    response.AccountName = packet.CharName + "'s " + "account is " + acc + ", e-mail: " + email + ", last ip: " + lastip;
    SendPacket(response.Write());
}

void WorldSession::HandleFarSightOpcode(WorldPackets::Misc::FarSight& packet)
{
    if (packet.Enable)
    {
        TC_LOG_DEBUG("network", "Added FarSight {} to player {}", _player->m_activePlayerData->FarsightObject->ToString(), _player->GetGUID().ToString());
        if (WorldObject* target = _player->GetViewpoint())
            _player->SetSeer(target);
        else
            TC_LOG_DEBUG("network", "Player {} {} requests non-existing seer {}", _player->GetName(), _player->GetGUID().ToString(), _player->m_activePlayerData->FarsightObject->ToString());
    }
    else
    {
        TC_LOG_DEBUG("network", "Player {} set vision to self", _player->GetGUID().ToString());
        _player->SetSeer(_player);
    }

    GetPlayer()->UpdateVisibilityForPlayer();
}

void WorldSession::HandleSetTitleOpcode(WorldPackets::Character::SetTitle& packet)
{
    // -1 at none
    if (packet.TitleID > 0)
    {
       if (!GetPlayer()->HasTitle(packet.TitleID))
            return;
    }
    else
        packet.TitleID = 0;

    GetPlayer()->SetChosenTitle(packet.TitleID);
}

void WorldSession::HandleResetInstancesOpcode(WorldPackets::Instance::ResetInstances& /*packet*/)
{
    Map* map = _player->FindMap();
    if (map && map->Instanceable())
        return;

    if (Group* group = _player->GetGroup())
    {
        if (!group->IsLeader(_player->GetGUID()))
            return;

        if (group->isLFGGroup())
            return;

        group->ResetInstances(InstanceResetMethod::Manual, _player);
    }
    else
        _player->ResetInstances(InstanceResetMethod::Manual);
}

void WorldSession::HandleSetDungeonDifficultyOpcode(WorldPackets::Misc::SetDungeonDifficulty& setDungeonDifficulty)
{
    DifficultyEntry const* difficultyEntry = sDifficultyStore.LookupEntry(setDungeonDifficulty.DifficultyID);
    if (!difficultyEntry)
    {
        TC_LOG_DEBUG("network", "WorldSession::HandleSetDungeonDifficultyOpcode: {} sent an invalid instance mode {}!",
            _player->GetGUID().ToString(), setDungeonDifficulty.DifficultyID);
        return;
    }

    if (difficultyEntry->InstanceType != MAP_INSTANCE)
    {
        TC_LOG_DEBUG("network", "WorldSession::HandleSetDungeonDifficultyOpcode: {} sent an non-dungeon instance mode {}!",
            _player->GetGUID().ToString(), difficultyEntry->ID);
        return;
    }

    if (!(difficultyEntry->Flags & DIFFICULTY_FLAG_CAN_SELECT))
    {
        TC_LOG_DEBUG("network", "WorldSession::HandleSetDungeonDifficultyOpcode: player {} sent unselectable instance mode {}!",
            _player->GetGUID().ToString(), difficultyEntry->ID);
        return;
    }

    // cannot reset while in an instance
    Map* map = _player->FindMap();
    if (map && map->Instanceable())
    {
        TC_LOG_DEBUG("network", "WorldSession::HandleSetDungeonDifficultyOpcode: player (Name: {}, {}) tried to reset the instance while player is inside!",
            _player->GetName(), _player->GetGUID().ToString());
        return;
    }

    Difficulty difficultyID = Difficulty(difficultyEntry->ID);

    Group* group = _player->GetGroup();
    if (group)
    {
        if (difficultyID == group->GetDungeonDifficultyID())
            return;

        if (!group->IsLeader(_player->GetGUID()))
            return;

        if (group->isLFGGroup())
            return;

        // the difficulty is set even if the instances can't be reset
        group->ResetInstances(InstanceResetMethod::OnChangeDifficulty, _player);
        group->SetDungeonDifficultyID(difficultyID);
    }

    if (difficultyID == _player->GetDungeonDifficultyID())
        return;

    if (!group)
        _player->ResetInstances(InstanceResetMethod::OnChangeDifficulty);

    _player->SetDungeonDifficultyID(difficultyID);
    _player->SendDungeonDifficulty();
}

void WorldSession::HandleSetRaidDifficultyOpcode(WorldPackets::Misc::SetRaidDifficulty& setRaidDifficulty)
{
    DifficultyEntry const* difficultyEntry = sDifficultyStore.LookupEntry(setRaidDifficulty.DifficultyID);
    if (!difficultyEntry)
    {
        TC_LOG_DEBUG("network", "WorldSession::HandleSetDungeonDifficultyOpcode: {} sent an invalid instance mode {}!",
            _player->GetGUID().ToString(), setRaidDifficulty.DifficultyID);
        return;
    }

    if (difficultyEntry->InstanceType != MAP_RAID)
    {
        TC_LOG_DEBUG("network", "WorldSession::HandleSetDungeonDifficultyOpcode: {} sent an non-dungeon instance mode {}!",
            _player->GetGUID().ToString(), difficultyEntry->ID);
        return;
    }

    if (!(difficultyEntry->Flags & DIFFICULTY_FLAG_CAN_SELECT))
    {
        TC_LOG_DEBUG("network", "WorldSession::HandleSetDungeonDifficultyOpcode: player {} sent unselectable instance mode {}!",
            _player->GetGUID().ToString(), difficultyEntry->ID);
        return;
    }

    if (((difficultyEntry->Flags & DIFFICULTY_FLAG_LEGACY) != 0) != setRaidDifficulty.Legacy)
    {
        TC_LOG_DEBUG("network", "WorldSession::HandleSetDungeonDifficultyOpcode: {} sent not matching legacy difficulty {}!",
            _player->GetGUID().ToString(), difficultyEntry->ID);
        return;
    }

    // cannot reset while in an instance
    Map* map = _player->FindMap();
    if (map && map->Instanceable())
    {
        TC_LOG_DEBUG("network", "WorldSession::HandleSetRaidDifficultyOpcode: player (Name: {}, {}) tried to reset the instance while player is inside!",
            _player->GetName(), _player->GetGUID().ToString());
        return;
    }

    Difficulty difficultyID = Difficulty(difficultyEntry->ID);

    Group* group = _player->GetGroup();
    if (group)
    {
        if (difficultyID == (setRaidDifficulty.Legacy ? group->GetLegacyRaidDifficultyID() : group->GetRaidDifficultyID()))
            return;

        if (!group->IsLeader(_player->GetGUID()))
            return;

        if (group->isLFGGroup())
            return;

        // the difficulty is set even if the instances can't be reset
        group->ResetInstances(InstanceResetMethod::OnChangeDifficulty, _player);
        if (setRaidDifficulty.Legacy)
            group->SetLegacyRaidDifficultyID(difficultyID);
        else
            group->SetRaidDifficultyID(difficultyID);
    }

    if (difficultyID == (setRaidDifficulty.Legacy ? _player->GetLegacyRaidDifficultyID() : _player->GetRaidDifficultyID()))
        return;

    if (!group)
        _player->ResetInstances(InstanceResetMethod::OnChangeDifficulty);

    if (setRaidDifficulty.Legacy)
        _player->SetLegacyRaidDifficultyID(difficultyID);
    else
        _player->SetRaidDifficultyID(difficultyID);

    _player->SendRaidDifficulty(setRaidDifficulty.Legacy != 0);
}

void WorldSession::HandleSetTaxiBenchmark(WorldPackets::Misc::SetTaxiBenchmarkMode& packet)
{
    if (packet.Enable)
        _player->SetPlayerFlag(PLAYER_FLAGS_TAXI_BENCHMARK);
    else
        _player->RemovePlayerFlag(PLAYER_FLAGS_TAXI_BENCHMARK);
}

void WorldSession::HandleGuildSetFocusedAchievement(WorldPackets::Achievement::GuildSetFocusedAchievement& setFocusedAchievement)
{
    if (Guild* guild = sGuildMgr->GetGuildById(_player->GetGuildId()))
        guild->GetAchievementMgr().SendAchievementInfo(_player, setFocusedAchievement.AchievementID);
}

void WorldSession::HandleServerTimeOffsetRequest(WorldPackets::Misc::ServerTimeOffsetRequest& /*request*/)
{
    WorldPackets::Misc::ServerTimeOffset response;
    response.Time = GameTime::GetSystemTime();
    SendPacket(response.Write());
}

void WorldSession::HandleInstanceLockResponse(WorldPackets::Instance::InstanceLockResponse& packet)
{
    if (!_player->HasPendingBind())
    {
        TC_LOG_INFO("network", "InstanceLockResponse: Player {} {} tried to bind himself/teleport to graveyard without a pending bind!",
            _player->GetName(), _player->GetGUID().ToString());
        return;
    }

    if (packet.AcceptLock)
        _player->ConfirmPendingBind();
    else
        _player->RepopAtGraveyard();

    _player->SetPendingBind(0, 0);
}

void WorldSession::HandleViolenceLevel(WorldPackets::Misc::ViolenceLevel& /*violenceLevel*/)
{
    // do something?
}

void WorldSession::HandleObjectUpdateFailedOpcode(WorldPackets::Misc::ObjectUpdateFailed& objectUpdateFailed)
{
    TC_LOG_ERROR("network", "Object update failed for {} for player {} ({})", objectUpdateFailed.ObjectGUID.ToString(), GetPlayerName(), _player->GetGUID().ToString());

    // If create object failed for current player then client will be stuck on loading screen
    if (_player->GetGUID() == objectUpdateFailed.ObjectGUID)
    {
        LogoutPlayer(true);
        return;
    }

    // Pretend we've never seen this object
    _player->m_clientGUIDs.erase(objectUpdateFailed.ObjectGUID);
}

void WorldSession::HandleObjectUpdateRescuedOpcode(WorldPackets::Misc::ObjectUpdateRescued& objectUpdateRescued)
{
    TC_LOG_ERROR("network", "Object update rescued for {} for player {} ({})", objectUpdateRescued.ObjectGUID.ToString(), GetPlayerName(), _player->GetGUID().ToString());

    // Client received values update after destroying object
    // re-register object in m_clientGUIDs to send DestroyObject on next visibility update
    _player->m_clientGUIDs.insert(objectUpdateRescued.ObjectGUID);
}

void WorldSession::HandleSaveCUFProfiles(WorldPackets::Misc::SaveCUFProfiles& packet)
{
    if (packet.CUFProfiles.size() > MAX_CUF_PROFILES)
    {
        TC_LOG_ERROR("entities.player", "HandleSaveCUFProfiles - {} tried to save more than {} CUF profiles. Hacking attempt?", GetPlayerName(), MAX_CUF_PROFILES);
        return;
    }

    for (uint8 i = 0; i < packet.CUFProfiles.size(); ++i)
        GetPlayer()->SaveCUFProfile(i, std::move(packet.CUFProfiles[i]));

    for (uint8 i = packet.CUFProfiles.size(); i < MAX_CUF_PROFILES; ++i)
        GetPlayer()->SaveCUFProfile(i, nullptr);
}

void WorldSession::SendLoadCUFProfiles()
{
    Player* player = GetPlayer();

    WorldPackets::Misc::LoadCUFProfiles loadCUFProfiles;

    for (uint8 i = 0; i < MAX_CUF_PROFILES; i++)
        if (CUFProfile* cufProfile = player->GetCUFProfile(i))
            loadCUFProfiles.CUFProfiles.push_back(cufProfile);
    SendPacket(loadCUFProfiles.Write());
}

void WorldSession::HandleSetAdvancedCombatLogging(WorldPackets::ClientConfig::SetAdvancedCombatLogging& setAdvancedCombatLogging)
{
    _player->SetAdvancedCombatLogging(setAdvancedCombatLogging.Enable);
}

void WorldSession::HandleMountSpecialAnimOpcode(WorldPackets::Misc::MountSpecial& mountSpecial)
{
    WorldPackets::Misc::SpecialMountAnim specialMountAnim;
    specialMountAnim.UnitGUID = _player->GetGUID();
    std::copy(mountSpecial.SpellVisualKitIDs.begin(), mountSpecial.SpellVisualKitIDs.end(), std::back_inserter(specialMountAnim.SpellVisualKitIDs));
    specialMountAnim.SequenceVariation = mountSpecial.SequenceVariation;
    GetPlayer()->SendMessageToSet(specialMountAnim.Write(), false);
}

void WorldSession::HandleMountSetFavorite(WorldPackets::Misc::MountSetFavorite& mountSetFavorite)
{
    _collectionMgr->MountSetFavorite(mountSetFavorite.MountSpellID, mountSetFavorite.IsFavorite);
}

void WorldSession::HandleCloseInteraction(WorldPackets::Misc::CloseInteraction& closeInteraction)
{
#ifndef DISABLE_DRESSNPCS_CORESOUNDS
    if (closeInteraction.SourceGuid.IsAnyTypeCreature())
        if (Creature* creature = _player->GetMap()->GetCreature(closeInteraction.SourceGuid))
            creature->SendMirrorSound(_player, 1);
#endif
        
    if (_player->PlayerTalkClass->GetInteractionData().IsLaunchedByQuest)
        _player->PlayerTalkClass->GetInteractionData().IsLaunchedByQuest = false;
    else if (_player->PlayerTalkClass->GetInteractionData().SourceGuid == closeInteraction.SourceGuid)
        _player->PlayerTalkClass->GetInteractionData().Reset();

    if (_player->GetStableMaster() == closeInteraction.SourceGuid)
        _player->SetStableMaster(ObjectGuid::Empty);
}

void WorldSession::HandleConversationLineStarted(WorldPackets::Misc::ConversationLineStarted& conversationLineStarted)
{
    if (Conversation* conversation = ObjectAccessor::GetConversation(*_player, conversationLineStarted.ConversationGUID))
        conversation->AI()->OnLineStarted(conversationLineStarted.LineID, _player);
}

void WorldSession::HandleRequestLatestSplashScreen(WorldPackets::Misc::RequestLatestSplashScreen& /*requestLatestSplashScreen*/)
{
    UISplashScreenEntry const* splashScreen = nullptr;
    for (auto itr = sUISplashScreenStore.begin(); itr != sUISplashScreenStore.end(); ++itr)
    {
        if (!ConditionMgr::IsPlayerMeetingCondition(_player, itr->CharLevelConditionID))
            continue;

        splashScreen = *itr;
    }

    WorldPackets::Misc::SplashScreenShowLatest splashScreenShowLatest;
    splashScreenShowLatest.UISplashScreenID = splashScreen ? splashScreen->ID : 0;
    SendPacket(splashScreenShowLatest.Write());
}

void WorldSession::HandleQueryCountdownTimer(WorldPackets::Misc::QueryCountdownTimer& queryCountdownTimer)
{
    Group const* group = _player->GetGroup();
    if (!group)
        return;

    Group::CountdownInfo const* info = group->GetCountdownInfo(queryCountdownTimer.TimerType);
    if (!info)
        return;

    WorldPackets::Misc::StartTimer startTimer;
    startTimer.Type = queryCountdownTimer.TimerType;
    startTimer.TimeLeft = info->GetTimeLeft();
    startTimer.TotalTime = info->GetTotalTime();

    _player->SendDirectMessage(startTimer.Write());
}

void WorldSession::HandleSetCurrencyFlags(WorldPackets::Misc::SetCurrencyFlags const& setCurrenctFlags)
{
    _player->SetCurrencyFlagsFromClient(setCurrenctFlags.CurrencyID, setCurrenctFlags.Flags);
}

void WorldSession::HandleSelectFactionOpcode(WorldPackets::Misc::FactionSelect& selectFaction)
{
    if (!_player)
        return;

    enum FactionSelection
    {
        JOIN_HORDE = 0,
        JOIN_ALLIANCE = 1,

        SPELL_TRIGGER_FACTION_CHOICE_ALLIANCE = 113244,
        SPELL_TRIGGER_FACTION_CHOICE_HORDE = 113245
    };

    TC_LOG_INFO("entities.player", "HandleSelectFactionOpcode: Player {} (GUID: {}) attempting to select faction: {}",
        _player ? _player->GetName() : "<null>",
        _player ? _player->GetGUID().ToString() : "<null>",
        selectFaction.FactionChoice);

    if (_player->GetRace() != RACE_PANDAREN_NEUTRAL)
    {
        TC_LOG_WARN("entities.player", "HandleSelectFactionOpcode: Player {} (GUID: {}) is not neutral pandaren (race: {}), rejecting faction selection",
            _player ? _player->GetName() : "<null>",
            _player ? _player->GetGUID().ToString() : "<null>",
            _player ? _player->GetRace() : 0);

        // Send error result to client
        WorldPackets::Character::NeutralPlayerFactionSelectResult result;
        result.Success = false;
        result.NewRaceID = _player ? _player->GetRace() : 0;
        _player->GetSession()->SendPacket(result.Write());
        return;
    }

    if (selectFaction.FactionChoice > JOIN_ALLIANCE)
    {
        TC_LOG_WARN("entities.player", "HandleSelectFactionOpcode: Player {} (GUID: {}) sent invalid faction choice: {}",
            _player->GetName(), _player->GetGUID().ToString(), selectFaction.FactionChoice);

        // Send error result to client
        WorldPackets::Character::NeutralPlayerFactionSelectResult result;
        result.Success = false;
        result.NewRaceID = _player->GetRace();
        _player->GetSession()->SendPacket(result.Write());
        return;
    }

    // Additional validation: check if player already has a faction (shouldn't happen but safety check)
    if (_player->GetRace() == RACE_PANDAREN_ALLIANCE || _player->GetRace() == RACE_PANDAREN_HORDE)
    {
        TC_LOG_WARN("entities.player", "HandleSelectFactionOpcode: Player {} (GUID: {}) already has faction (race: {}), rejecting faction selection",
            _player->GetName(), _player->GetGUID().ToString(), _player->GetRace());

        // Send error result to client
        WorldPackets::Character::NeutralPlayerFactionSelectResult result;
        result.Success = false;
        result.NewRaceID = _player->GetRace();
        _player->GetSession()->SendPacket(result.Write());
        return;
    }

    Races newRace = RACE_NONE;
    uint32 languageSpell1 = 0;
    uint32 languageSpell2 = 0;
    uint32 triggerSpell = 0;

    switch (selectFaction.FactionChoice)
    {
    case JOIN_ALLIANCE:
        newRace = RACE_PANDAREN_ALLIANCE;
        languageSpell1 = SPELL_LEARN_LANGUAGE_COMMON;
        languageSpell2 = SPELL_LEARN_LANGUAGE_PANDAREN_ALLIANCE;
        triggerSpell = SPELL_TRIGGER_FACTION_CHOICE_ALLIANCE;
        break;
    case JOIN_HORDE:
        newRace = RACE_PANDAREN_HORDE;
        languageSpell1 = SPELL_LEARN_LANGUAGE_ORCISH;
        languageSpell2 = SPELL_LEARN_LANGUAGE_PANDAREN_HORDE;
        triggerSpell = SPELL_TRIGGER_FACTION_CHOICE_HORDE;
        break;
    default:
        break;
    }

    _player->SetRace(newRace);
    _player->SetFactionForRace(newRace);
    _player->SaveToDB();
    _player->LearnSpell(languageSpell1, false);
    _player->LearnSpell(languageSpell2, false);
    _player->CastSpell(_player, triggerSpell, true);

    // Force client to refresh all reputation factions or sides
    uint32 headerFactionId = (newRace == RACE_PANDAREN_ALLIANCE) ? 469 : 67;

    for (FactionEntry const* fe : sFactionStore)
    {
        if (!fe->CanHaveReputation())
            continue;

        if (fe->ParentFactionID != headerFactionId)
            continue;

        _player->GetReputationMgr().SetVisible(fe);

        int32 base = fe->ReputationBase[0];

        // Apply the base reputation
        _player->GetReputationMgr().SetOneFactionReputation(fe, base, false);

        // Get the updated state
        FactionState* fs = const_cast<FactionState*>(_player->GetReputationMgr().GetState(fe));
        if (!fs)
            continue;

        // Mark for sending
        fs->needSend = true;

        // Trigger the system messages
        fs->VisualStandingIncrease = base;

        // Send the update
        _player->GetReputationMgr().SendState(fs);
    }

    const char* sideName = (selectFaction.FactionChoice == 0) ? "Horde" : "Alliance";
    TC_LOG_INFO("entities.player", "HandleSelectFactionOpcode: Player {} (GUID: {}) successfully joined {}",
        _player->GetName(), _player->GetGUID().ToString(), sideName);

    // Send success result to client
    WorldPackets::Character::NeutralPlayerFactionSelectResult result;
    result.Success = true;
    result.NewRaceID = newRace;
    _player->GetSession()->SendPacket(result.Write());
}

void WorldSession::HandleOverrideScreenFlash(WorldPackets::Misc::OverrideScreenFlash& overrideScreenFlash)
{
    _player->SetOverrideScreenFlash(overrideScreenFlash.BlackScreenOrRedScreen);
}

void WorldSession::HandleAccountNotificationAcknowledge(WorldPackets::Misc::AccountNotificationAcknowledge& /*packet*/)
{
}

void WorldSession::HandleShowTradeSkill(WorldPackets::Misc::ShowTradeSkill& packet)
{
    if (!sSkillLineStore.LookupEntry(packet.SkillLineID) || !sSpellMgr->GetSpellInfo(packet.SpellID, DIFFICULTY_NONE))
        return;

    Player* player = ObjectAccessor::FindPlayer(packet.PlayerGUID);
    if (!player)
        return;

    std::set<uint32> relatedSkills;
    relatedSkills.insert(packet.SkillLineID);

    for (SkillLineEntry const* skillLine : sSkillLineStore)
    {
        if (skillLine->ParentSkillLineID != packet.SkillLineID)
            continue;

        if (!player->HasSkill(skillLine->ParentSkillLineID))
            continue;

        relatedSkills.insert(skillLine->ParentSkillLineID);
    }

    std::set<uint32> profSpells;
    for (auto const& v : player->GetSpellMap())
    {
        if (v.second.state == PLAYERSPELL_REMOVED)
            continue;

        if (!v.second.active || v.second.disabled)
            continue;

        for (auto const& s : relatedSkills)
            if (IsPartOfSkillLine(s, v.first))
                profSpells.insert(v.first);
    }

    if (profSpells.empty())
        return;

    WorldPackets::Misc::ShowTradeSkillResponse response;
    response.PlayerGUID = packet.PlayerGUID;
    response.SpellId = packet.SpellID;

    for (uint32 const& x : profSpells)
        response.KnownAbilitySpellIDs.push_back(x);

    for (uint32 const& v : relatedSkills)
    {
        response.SkillLineIDs.push_back(v);
        response.SkillRanks.push_back(player->GetSkillValue(v));
        response.SkillMaxRanks.push_back(player->GetMaxSkillValue(v));
    }

    _player->SendDirectMessage(response.Write());
}

void WorldSession::HandleActivateSoulbind(WorldPackets::Misc::ActivateSoulbind& /*packet*/)
{
    // Need IMP
}

void WorldSession::HandleChromieTimeSelectExpansion(WorldPackets::Misc::ChromieTimeSelectExpansion& chromieTimeSelectExpansion)
{
    Player* player = GetPlayer();
    if (!player)
        return;

    // Wire format (12.0.5): PackedGuid Vendor + int32 ExpansionID, where ExpansionID is the
    // UIChromieTimeExpansionInfo.ID (DB2 record id), not the Expansions enum.
    // Verify the vendor is a gossip NPC the player is actually interacting with.
    Creature const* vendor = player->GetNPCIfCanInteractWith(chromieTimeSelectExpansion.Vendor, UNIT_NPC_FLAG_GOSSIP, UNIT_NPC_FLAG_2_NONE);
    if (!vendor)
        return;

    // Require an active ChromieTime interaction with this exact NPC, mirroring other
    // interaction-driven handlers. The client always packages the interaction-source guid
    // into the CMSG (SelectChromieTimeOption RVA 0xB79106), so a legitimate select can
    // only arrive while the type-45 interaction started by the gossip option is open.
    if (!player->PlayerTalkClass->GetInteractionData().IsInteractingWith(chromieTimeSelectExpansion.Vendor, PlayerInteractionType::ChromieTime))
        return;

    int32 expansionId = chromieTimeSelectExpansion.ExpansionID;

    // 0 = "Return to the present"; always allowed (a chromie player above the entry
    // ceiling - the 70..80 scaling band - must still be able to leave).
    if (expansionId == 0)
    {
        player->SetChromieTime(0);
        player->SendDirectMessage(WorldPackets::Misc::ChromieTimeSelectExpansionSuccess().Write());
        return;
    }

    // Retail 12.0.x entry gate: level band [10, 70). The @68887 ShowPlayerConditionIDs
    // contain no level clause, so the ceiling is server policy (see Player.h, audit R10).
    if (player->GetLevel() < Player::ChromieTimeMinLevel || player->GetLevel() >= Player::ChromieTimeMaxEntryLevel)
        return;

    UIChromieTimeExpansionInfoEntry const* entry = sUIChromieTimeExpansionInfoStore.LookupEntry(uint32(expansionId));
    if (!entry)
        return;

    // Do NOT require ShowPlayerConditionID here: decoded @68887 each of those conditions is
    // ModifierTree { All -> PlayerIsInChromieTime(own id) } with PlayerCondition flags 0x21
    // (no InvertModifierTree) - i.e. "player is ALREADY in this timeline", the client UI's
    // alreadyOn marker, not an eligibility gate. Requiring it inverted the gate and made
    // every first-time selection fail (audit R10 decode).

    player->SetChromieTime(expansionId);

    player->SendDirectMessage(WorldPackets::Misc::ChromieTimeSelectExpansionSuccess().Write());

    // Audit R13 FIX 1: retail completes quest 85026 "Where Legends are Made" (objective
    // 453674 = QUEST_OBJECTIVE_MONSTER, ObjectID 167032 = Chromie, Amount 1,
    // "Timewalking Campaign selected") by granting Chromie kill-credit on a successful
    // selection. Always safe: KilledMonsterCredit no-ops if the player is not on the quest.
    player->KilledMonsterCredit(167032);

    // Audit R14 (G1 immersion): Chromie speaks an era-specific flavor line the instant a timeline
    // is selected. Verified against the 12.1.0.69382 ChromieOrgrimmar capture: each of the 9
    // CMSG_CHROMIE_TIME_SELECT_EXPANSION was immediately followed by a CHAT_MSG_MONSTER_SAY from
    // Chromie (SMSG_CHAT) carrying the exact lines below - extracted from the sniff, not invented.
    // The line is spoken by the interacted Chromie vendor, so it works for either faction's Chromie
    // NPC and either capital without depending on a specific creature entry / creature_text rows.
    // NOTE: the campaign-intro movie (id 470) also seen in the capture played only ONCE, before any
    // select (SMSG_PLAY_MOVIE, one occurrence in the whole capture), so it is a one-time cinematic,
    // NOT a per-select movie, and is deliberately not replayed here (replaying it every switch would
    // be un-Blizzlike). expansionId = UiChromieTimeExpansionInfo.ID.
    auto chromieSelectLine = [](int32 exp) -> char const*
    {
        switch (exp)
        {
        case  5: return "Who would have thought someone named Deathwing would bring about so much destruction?";
        case  6: return "You look quite prepared!";
        case  7: return "Do not exercise restraint when showing your great power to the Scourge!";
        case  8: return "Whatever you do, do not get between a pandaren and their brew. It'll be unbearably painful if you do!";
        case  9: return "If a scary orc offers you something to drink, you probably want to say no.";
        case 10: return "If you find a powerful weapon, just make sure it isn't corrupted by the Burning Legion or Old Gods, okay?";
        case 14: return "Death it is! And you know what they say... what doesn't kill you makes you stronger.";
        case 15: return "This might be the most important battle of them all. For our world, worth fighting for!";
        case 16: return "Dragons, dragons, dragons! I'm definitely not biased...";
        default: return nullptr;
        }
    };
    if (char const* line = chromieSelectLine(expansionId))
        const_cast<Creature*>(vendor)->Say(line, LANG_UNIVERSAL, player);

    // Audit R13 FIX 2: auto-offer the era's Chromie Time breadcrumb for the chosen timeline,
    // matching retail's per-expansion intro quest. The breadcrumbs are FACTION-SPECIFIC; each
    // expansion has an Alliance/Horde quest pair, and CanTakeQuest (AllowableRaces) rejects a
    // wrong-faction id, so offering only one faction's id reproduces the "no breadcrumb" bug.
    //
    // The HORDE column is byte/temporal-verified against the 12.1.0.69382 ChromieOrgrimmar
    // capture (C:/sniff/ymir_retail_12.1.0.69299/dumps/ChromieOrgrimmar): a Horde Undead
    // Warlock clicked every timeline, and each CMSG_CHROMIE_TIME_SELECT_EXPANSION was followed
    // within a handful of records by exactly ONE breadcrumb quest-add. Correlated select->quest
    // (TCHarvest.lua quest_template titles confirm the era):
    //   Cata(5)=60887 "Onward to Adventure in Kalimdor", TBC(6)=60123 "To Outland!",
    //   WotLK(7)=60097 "To Northrend!", MoP(8)=60126 "To Pandaria!", WoD(9)=34398 "The Dark
    //   Portal" (faction-neutral), Legion(10)=43926 "The Legion Returns", SL(14)=61874
    //   "A Chilling Summons" (Meet Darion Mograine at Grommash Hold), BfA(15)=51443 "Mission
    //   Statement" (harvest quest_accepted=51443, quest_detail offered+accepted),
    //   DF(16)=65435 "The Dragon Isles Await".
    // The previously-used Horde ids 60961/60963/60968/60970/53372 do NOT appear anywhere in the
    // capture and were wrong (53372 is the BfA war-campaign "Hour of Reckoning", not the Chromie
    // breadcrumb); corrected below to the sniff-verified ids. SL (14) Horde was deferred and is
    // now resolved to 61874.
    //
    // The ALLIANCE column is branch-asserted and NOT verified by this Horde-only capture. Cata
    // (60891) and MoP (60125) are the plausible paired counterparts of the verified Horde ids;
    // TBC/WotLK/WoD/Legion/BfA/SL Alliance ids need an Alliance-side sniff to confirm and are
    // suspect (the branch's Horde ids for those rows were wrong, so the Alliance ids likely are
    // too - e.g. BfA A=53370 is the Alliance war-campaign "Hour of Reckoning", not the Chromie
    // breadcrumb). CanTakeQuest re-validates AllowableRaces, so a wrong/absent id yields no offer
    // rather than a wrong-faction quest, keeping this safe until an Alliance capture is taken.
    //
    // expansionId = UiChromieTimeExpansionInfo.ID (DB2 record id): Cata=5, TBC=6, WotLK=7,
    // MoP=8, WoD=9, Legion=10, SL=14, BfA=15, DF=16. Excluded: The War Within - breadcrumbs
    // 81930/78713 exist and are Chromie-started, but TWW has NO UiChromieTimeExpansionInfo row
    // so it can never arrive as a selection.
    struct ChromieIntroQuest { int32 ExpansionId; uint32 AllianceQuest; uint32 HordeQuest; };
    static constexpr ChromieIntroQuest ChromieIntroQuests[] =
    {
        {  5, 60891, 60887 }, // Cataclysm              (H 60887 "...Kalimdor" verified; A 60891 = Eastern Kingdoms)
        {  6, 60959, 60123 }, // Burning Crusade        (H 60123 "To Outland!" verified; A unverified)
        {  7, 60962, 60097 }, // Wrath of the Lich King (H 60097 "To Northrend!" verified; A unverified)
        {  8, 60125, 60126 }, // Mists of Pandaria      (H 60126 "To Pandaria!" verified)
        {  9, 60969, 34398 }, // Warlords of Draenor    (H 34398 "The Dark Portal" verified, faction-neutral; A unverified)
        { 10, 60971, 43926 }, // Legion                 (H 43926 "The Legion Returns" verified; A unverified)
        { 14, 60545, 61874 }, // Shadowlands            (H 61874 "A Chilling Summons" verified; A 60545 branch-asserted)
        { 15, 53370, 51443 }, // Battle for Azeroth     (H 51443 "Mission Statement" verified; A 53370 suspect = war-campaign)
        { 16, 65436, 65435 }, // Dragonflight           (H 65435 "The Dragon Isles Await" verified)
    };

    for (ChromieIntroQuest const& intro : ChromieIntroQuests)
    {
        if (intro.ExpansionId != expansionId)
            continue;

        uint32 introQuestId = (player->GetTeam() == ALLIANCE) ? intro.AllianceQuest : intro.HordeQuest;
        if (Quest const* introQuest = sObjectMgr->GetQuestTemplate(introQuestId))
            if (player->GetQuestStatus(introQuestId) == QUEST_STATUS_NONE && player->CanTakeQuest(introQuest, false))
                player->AddQuestAndCheckCompletion(introQuest, nullptr);

        break;
    }
}

void WorldSession::HandleRequestStoreFrontInfoUpdate(WorldPackets::Misc::RequestStoreFrontInfoUpdate& packet)
{
    WorldPackets::Misc::AccountStoreFrontUpdate response;
    response.StoreFrontID = packet.StoreFrontID;
    response.Result = 0;  // Success
    response.Unknown = 0;
    SendPacket(response.Write());
}
