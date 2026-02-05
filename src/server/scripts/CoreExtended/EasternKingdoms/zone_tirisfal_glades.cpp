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

#include "AreaTrigger.h"
#include "AreaTriggerAI.h"
#include "CellImpl.h"
#include "CombatAI.h"
#include "Containers.h"
#include "Conversation.h"
#include "ConversationAI.h"
#include "CreatureAIImpl.h"
#include "EventProcessor.h"
#include "GridNotifiersImpl.h"
#include "MapUtils.h"
#include "MotionMaster.h"
#include "ObjectAccessor.h"
#include "PathGenerator.h"
#include "PassiveAI.h"
#include "PhasingHandler.h"
#include "Player.h"
#include "ScriptMgr.h"
#include "ScriptedCreature.h"
#include "ScriptedGossip.h"
#include "SpellAuras.h"
#include "SpellMgr.h"
#include "SpellScript.h"
#include "TemporarySummon.h"

#define QUEST_THE_SHADOW_GRAVE 28608
#define QUEST_BEYOND_THE_GRAVE 25089
#define NPC_DARNEL             49141

// Area IDs
#define AREA_NIGHT_WEBS_HOLLOW 2117
#define AREA_DEATHKNELL_GRAVEYARD 5692

enum PlaceDescription {
    UNKNOWN                    = 0,
    OUTSIDE,
    ENTRANCE,
    STAIRS1,
    STAIRS2,
    GROUND,
};

// 49141 - Darnell for quest 28608
struct npc_darnell_grave : public ScriptedAI
{
    npc_darnell_grave(Creature* creature) : ScriptedAI(creature)
    {
        Reset();
    }

    ObjectGuid playerGUID;
    uint32 m_timer;
    uint8 m_counter;
    uint8 m_modus;
    uint8 m_path;
    bool m_arrived;
    bool m_FoundGround;
    bool m_ItemsFound;
    Position m_OldPosition;

    void Reset() override
    {
        m_timer = 1000;
        m_modus = 0;
        m_path = 0;
        m_counter = 0;
        m_arrived = false;
        m_FoundGround = false;
        m_ItemsFound = false;
        playerGUID.Clear();

        Unit* owner = me->GetCharmerOrOwner();
        if (owner && owner->IsPlayer())
        {
            Player* player = owner->ToPlayer();
            if (player && player->GetQuestStatus(QUEST_THE_SHADOW_GRAVE) == QUEST_STATUS_INCOMPLETE)
            {
                playerGUID = player->GetGUID();
                m_modus = 1;
                m_OldPosition = player->GetPosition();
                Talk(0, player);
            }
        }
    }

    void MovementInform(uint32 type, uint32 /*id*/) override
    {
        if (!CheckPlayerValid())
            return;

        if (type == POINT_MOTION_TYPE)
            m_arrived = true;
    }

    void UpdateAI(uint32 diff) override
    {
        if (!CheckPlayerValid())
            return;

        Player* player = ObjectAccessor::GetPlayer(*me, playerGUID);
        if (!player) return;

        if (player->GetQuestStatus(QUEST_THE_SHADOW_GRAVE) == QUEST_STATUS_COMPLETE &&
            player->GetAreaId() != AREA_NIGHT_WEBS_HOLLOW)
        {
            me->DespawnOrUnsummon(100ms);
            return;
        }

        if (m_arrived && me->GetMotionMaster()->GetCurrentMovementGeneratorType() != FOLLOW_MOTION_TYPE)
        {
            if (me->GetDistance(player) < 4.0f)
            {
                me->GetMotionMaster()->MoveFollow(player, 0.0f, 0.0f);
            }
        }

        if (m_timer <= diff)
        {
            m_timer = 1000;
            DoWork();
        }
        else
            m_timer -= diff;
    }

    void DoWork()
    {
        if (m_FoundGround)
        {
            SearchOnGround();
            return;
        }

        switch (GetPlaceDescription())
        {
        case PlaceDescription::OUTSIDE:
            InviteToFollow();
            break;
        case PlaceDescription::ENTRANCE:
            InviteToFollowDeeper1();
            break;
        case PlaceDescription::STAIRS1:
            InviteToFollowDeeper2();
            break;
        case PlaceDescription::STAIRS2:
            InviteToFollowToGround();
            break;
        case PlaceDescription::GROUND:
            SearchOnGround();
            break;
        default:
            break;
        }
    }

    PlaceDescription GetPlaceDescription()
    {
        Player* player = ObjectAccessor::GetPlayer(*me, playerGUID);
        if (!player) return UNKNOWN;

        uint32 areaId = player->GetAreaId();
        float z = player->GetPositionZ();

        if (areaId == AREA_DEATHKNELL_GRAVEYARD)
            return PlaceDescription::OUTSIDE;

        if (areaId == AREA_NIGHT_WEBS_HOLLOW)
        {
            if (z < 123.0f)
            {
                m_FoundGround = true;
                return PlaceDescription::GROUND;
            }

            if (z < 127.0f)
                return PlaceDescription::STAIRS2;

            if (z < 133.0f)
                return PlaceDescription::STAIRS1;

            return PlaceDescription::ENTRANCE;
        }

        return UNKNOWN;
    }

    float GetMovedPlayerDistance()
    {
        Player* player = ObjectAccessor::GetPlayer(*me, playerGUID);
        if (!player) return 100.0f;

        float dist = player->GetDistance(m_OldPosition);
        m_OldPosition = player->GetPosition();
        return dist;
    }

    void InviteToFollow()
    {
        m_counter++;
        if (GetMovedPlayerDistance() > 1.0f)
            m_counter = 0;

        if (m_counter >= 5)
        {
            Talk(2);
            me->GetMotionMaster()->MovePoint(1, 1665.368896f, 1662.722656f, 141.850983f);
            m_path = 1;
            m_arrived = false;
            m_counter = 0;
        }
    }

    void InviteToFollowDeeper1()
    {
        m_counter++;
        if (GetMovedPlayerDistance() > 1.0f)
            m_counter = 0;

        if (m_counter >= 5)
        {
            Talk(2);
            me->GetMotionMaster()->MovePoint(2, 1642.761963f, 1662.547729f, 132.477753f);
            m_path = 2;
            m_arrived = false;
            m_counter = 0;
        }
    }

    void InviteToFollowDeeper2()
    {
        m_counter++;
        if (GetMovedPlayerDistance() > 1.0f)
            m_counter = 0;

        if (m_counter >= 3)
        {
            me->GetMotionMaster()->MovePoint(3, 1642.498779f, 1677.809937f, 126.932129f);
            m_path = 3;
            m_arrived = false;
            m_counter = 0;
        }
    }

    void InviteToFollowToGround()
    {
        m_counter++;
        if (GetMovedPlayerDistance() > 1.0f)
            m_counter = 0;

        if (m_counter >= 3)
        {
            me->GetMotionMaster()->MovePoint(4, 1656.714478f, 1678.538330f, 120.718788f);
            m_path = 4;
            m_arrived = false;
            m_counter = 0;
        }
    }

    void SearchOnGround()
    {
        if (m_ItemsFound)
            return;

        if (CheckPlayerFoundItems())
        {
            m_ItemsFound = true;
            Talk(9);
            m_timer = 10000;
            return;
        }

        switch (m_modus)
        {
        case 2: MoveToCenter(); break;
        case 3: MoveToRandomCorner(); break;
        case 4: SearchingOnCorner(); break;
        default: m_modus = 2; break;
        }
    }

    void MoveToCenter()
    {
        if (m_path != 8)
        {
            me->GetMotionMaster()->MovePoint(8, 1664.128052f, 1679.201294f, 120.530205f);
            m_arrived = false;
            m_path = 8;
        }
        else if (m_arrived == false)
        {
        }
        else
        {
            m_modus = 3;
        }
    }

    void MoveToRandomCorner()
    {
        if (m_path == 8)
        {
            switch (urand(1, 4))
            {
            case 1:
                me->GetMotionMaster()->MovePoint(4, 1663.849609f, 1694.495239f, 120.719284f);
                m_arrived = false;
                m_path = 4;
                break;
            case 2:
                me->GetMotionMaster()->MovePoint(5, 1672.939331f, 1668.029541f, 120.719307f);
                m_arrived = false;
                m_path = 5;
                break;
            case 3:
                me->GetMotionMaster()->MovePoint(6, 1656.963379f, 1667.456299f, 120.719093f);
                m_arrived = false;
                m_path = 6;
                break;
            case 4:
                me->GetMotionMaster()->MovePoint(7, 1656.098999f, 1688.312866f, 120.719093f);
                m_arrived = false;
                m_path = 7;
                break;
            }
        }
        else if (m_arrived == false)
        {
        }
        else
        {
            m_modus = 4;
        }
    }

    void SearchingOnCorner()
    {
        Player* player = ObjectAccessor::GetPlayer(*me, playerGUID);
        if (player)
            Talk(urand(3, 8), player);
        m_timer = 6000;
        m_modus = 2;
    }

    bool CheckPlayerFoundItems()
    {
        Player* player = ObjectAccessor::GetPlayer(*me, playerGUID);
        if (!player) return false;

        return player->GetQuestStatus(QUEST_THE_SHADOW_GRAVE) == QUEST_STATUS_COMPLETE;
    }

    bool CheckPlayerValid()
    {
        Player* player = ObjectAccessor::GetPlayer(*me, playerGUID);
        if (!player) return false;

        if (!player->IsInWorld() ||
            player->isDead() ||
            player->GetQuestStatus(QUEST_THE_SHADOW_GRAVE) == QUEST_STATUS_NONE ||
            player->GetQuestStatus(QUEST_THE_SHADOW_GRAVE) == QUEST_STATUS_FAILED)
        {
            me->DespawnOrUnsummon();
            return false;
        }
        return true;
    }
};

// 49337 - Darnell for quest 25089 and 26800
struct npc_darnell_deathknell_corpse : public ScriptedAI
{
    npc_darnell_deathknell_corpse(Creature* creature) : ScriptedAI(creature)
    {
        isWaitingForTurnIn = false;
        eventTriggered = false;
    }

    ObjectGuid playerGUID;
    bool isWaitingForTurnIn;
    bool eventTriggered;

    void Reset() override
    {
        isWaitingForTurnIn = false;
        eventTriggered = false;
        me->SetReactState(REACT_PASSIVE);
    }

    void IsSummonedBy(WorldObject * summoner) override
    {
        Player* player = nullptr;
        if (summoner->ToPlayer())
            player = summoner->ToPlayer();
        else
            player = me->SelectNearestPlayer(10.0f);

        if (player)
        {
            playerGUID = player->GetGUID();
            Talk(0);
        }
    }

    void UpdateAI(uint32 diff) override
    {
        Player* player = ObjectAccessor::GetPlayer(*me, playerGUID);
        if (!player)
            return;

        if (isWaitingForTurnIn)
        {
            QuestStatus status = player->GetQuestStatus(QUEST_BEYOND_THE_GRAVE);

            if (status == QUEST_STATUS_REWARDED)
            {
                isWaitingForTurnIn = false;
            }
            else if (status == QUEST_STATUS_NONE || status == QUEST_STATUS_FAILED)
            {
                me->DespawnOrUnsummon();
            }

            return;
        }

        if (!eventTriggered)
        {
            if (player->IsWithinDist3d(1685.670f, 1647.569f, 137.326f, 15.0f))
            {
                eventTriggered = true;
                isWaitingForTurnIn = true;

                Talk(2);

                me->StopMoving();
                me->GetMotionMaster()->Clear();
                me->GetMotionMaster()->MoveIdle();

                me->GetMotionMaster()->MovePoint(1, 1864.78f, 1604.89f, 94.606f);
            }
        }
    }

    void MovementInform(uint32 moveType, uint32 pointId) override
    {
        if (moveType != POINT_MOTION_TYPE)
            return;

        if (pointId == 1)
        {
            Talk(3);

            me->StopMoving();
            me->GetMotionMaster()->Clear();
            me->GetMotionMaster()->MoveIdle();
        }
    }
};

void AddSC_zone_tirisfal_glades()
{
    RegisterCreatureAI(npc_darnell_grave);
    RegisterCreatureAI(npc_darnell_deathknell_corpse);
}
