/*
 * This file is part of the Stefal WoW Project.
 * It is designed to work exclusively with the TrinityCore framework.
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
 * This code is provided for personal and educational use within the
 * Stefal WoW Project. It is not intended for commercial distribution,
 * resale, or any form of monetization.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include "ScriptMgr.h"
#include "ScriptedCreature.h"
#include "ObjectAccessor.h"
#include "Player.h"
#include "Creature.h"
#include "CreatureAI.h"
#include "TemporarySummon.h"
#include "Unit.h"
#include "ObjectGuid.h"
#include "SpellInfo.h"
#include "SpellMgr.h"

#include "zone_westfall.h"

 /*######
 ## npc_custom_lous_parting_thoughts_trigger
 ######*/

class npc_custom_lous_parting_thoughts_trigger : public CreatureScript
{
public:
    npc_custom_lous_parting_thoughts_trigger() : CreatureScript("npc_custom_lous_parting_thoughts_trigger") { }

    struct npc_lous_parting_thoughts_triggerAI : public ScriptedAI
    {
        npc_lous_parting_thoughts_triggerAI(Creature* creature) : ScriptedAI(creature) { }

        ObjectGuid PlayerGUID;
        std::array<ObjectGuid, 4> ThugGUIDs;

        uint32 PhaseTimer = 0;
        uint8  Phase = 0;
        uint8  DeadThugs = 0;
        bool   EventStarted = false;
        bool   EventLocked = false;

        void Reset() override
        {
            Phase = 0;
            DeadThugs = 0;
            EventStarted = false;
            EventLocked = false;
            PhaseTimer = 1000;

            for (auto& guid : ThugGUIDs)
                guid.Clear();
        }

        void MoveInLineOfSight(Unit* who) override
        {
            if (EventStarted)
                return;

            Player* player = who->ToPlayer();
            if (!player)
                return;

            if (player->GetQuestStatus(QUEST_LOUS_PARTING_THOUGHTS) != QUEST_STATUS_INCOMPLETE)
                return;

            if (!who->IsWithinDistInMap(me, 30.0f))
                return;

            PlayerGUID = player->GetGUID();
            StartEvent();
        }

        void StartEvent()
        {
            EventStarted = true;

            for (uint8 i = 0; i < 4; ++i)
            {
                if (Creature* thug = me->SummonCreature(
                    NPC_WESTFALL_THUG,
                    Q26232ThugPositions[i],
                    TEMPSUMMON_TIMED_DESPAWN_OUT_OF_COMBAT,
                    90s))
                {
                    thug->SetReactState(REACT_PASSIVE);
                    thug->SetUnitFlag(UNIT_FLAG_NON_ATTACKABLE | UNIT_FLAG_IMMUNE_TO_PC);

                    ThugGUIDs[i] = thug->GetGUID();
                }

            }

            PhaseTimer = 2000;
        }

        Creature* GetThug(uint8 index)
        {
            return ObjectAccessor::GetCreature(*me, ThugGUIDs[index]);
        }

        void SetData(uint32 id, uint32 /*value*/) override
        {
            if (id != 1)
                return;

            ++DeadThugs;

            if (DeadThugs >= 4)
            {
                if (Player* player = ObjectAccessor::GetPlayer(*me, PlayerGUID))
                {
                    Talk(0, player);
                    player->KilledMonsterCredit(NPC_WESTFALL_Q26232_CREDIT, PlayerGUID);
                    player->RemoveAurasDueToSpell(SPELL_WESTFALL_DETECT_QUEST_INVIS_1);
                    player->CastSpell(player, SPELL_WESTFALL_DETECT_QUEST_INVIS_2);
                }


                Reset();
            }
        }

        void UpdateAI(uint32 diff) override
        {
            if (!EventStarted || EventLocked)
                return;

            if (PhaseTimer <= diff)
            {
                switch (Phase)
                {
                case 0:
                    if (Creature* thug = GetThug(1))
                        thug->AI()->Talk(Q26232_SAY_ASK_MEET);
                    PhaseTimer = 3500;
                    break;

                case 1:
                    if (Creature* thug = GetThug(0))
                        thug->AI()->Talk(Q26232_SAY_CONFIRM_MEET);
                    PhaseTimer = 4000;
                    break;

                case 2:
                    if (Creature* thug = GetThug(0))
                        thug->AI()->Talk(Q26232_SAY_CONGRATULATE);
                    PhaseTimer = 7000;
                    break;

                case 3:
                    if (Creature* thug = GetThug(3))
                        thug->AI()->Talk(Q26232_SAY_ASK_IDENTITY);
                    PhaseTimer = 4000;
                    break;

                case 4:
                    if (Player* player = ObjectAccessor::GetPlayer(*me, PlayerGUID))
                        for (auto guid : ThugGUIDs)
                            if (Creature* thug = ObjectAccessor::GetCreature(*me, guid))
                                thug->SetFacingToObject(player);
                    PhaseTimer = 1000;
                    break;

                case 5:
                    if (Creature* thug = GetThug(2))
                        thug->AI()->Talk(Q26232_SAY_NOTICE_PLAYER);
                    PhaseTimer = 4500;
                    break;

                case 6:
                    if (Creature* thug = GetThug(1))
                        thug->AI()->Talk(Q26232_SAY_THREATEN_PLAYER);
                    PhaseTimer = 4500;
                    break;

                case 7:
                    if (Creature* thug = GetThug(0))
                        thug->AI()->Talk(Q26232_SAY_DIE);
                    PhaseTimer = 2000;
                    break;

                case 8:
                    EventLocked = true;

                    for (auto guid : ThugGUIDs)
                    {
                        if (Creature* thug = ObjectAccessor::GetCreature(*me, guid))
                        {
                            thug->RemoveUnitFlag(UNIT_FLAG_NON_ATTACKABLE | UNIT_FLAG_IMMUNE_TO_PC);
                            thug->SetFaction(14);
                            thug->SetReactState(REACT_AGGRESSIVE);
                        }
                    }
                    PhaseTimer = 1000;
                    break;

                case 9:
                    if (Player* player = ObjectAccessor::GetPlayer(*me, PlayerGUID))
                        for (auto guid : ThugGUIDs)
                            if (Creature* thug = ObjectAccessor::GetCreature(*me, guid))
                                thug->AI()->AttackStart(player);
                    PhaseTimer = 0;
                    break;
                }

                ++Phase;
            }
            else
                PhaseTimer -= diff;
        }
    };

    CreatureAI* GetAI(Creature* creature) const override
    {
        return new npc_lous_parting_thoughts_triggerAI(creature);
    }
};

/*######
## npc_custom_lous_parting_thoughts_thug
######*/

class npc_custom_lous_parting_thoughts_thug : public CreatureScript
{
public:
    npc_custom_lous_parting_thoughts_thug() : CreatureScript("npc_custom_lous_parting_thoughts_thug") { }

    struct npc_lous_parting_thoughts_thugAI : public ScriptedAI
    {
        npc_lous_parting_thoughts_thugAI(Creature* creature) : ScriptedAI(creature) { }

        void JustDied(Unit* /*killer*/) override
        {
            if (TempSummon* summon = me->ToTempSummon())
            {
                if (Creature* trigger = ObjectAccessor::GetCreature(*me, summon->GetSummonerGUID()))
                {
                    if (trigger->AI())
                        trigger->AI()->SetData(1, 1); // notify kill
                }
            }
        }

    };

    CreatureAI* GetAI(Creature* creature) const override
    {
        return new npc_lous_parting_thoughts_thugAI(creature);
    }
};

void AddSC_zone_westfall()
{
    new npc_custom_lous_parting_thoughts_trigger();
    new npc_custom_lous_parting_thoughts_thug();
}
