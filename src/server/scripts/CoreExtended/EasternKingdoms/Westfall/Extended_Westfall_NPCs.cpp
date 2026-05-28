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

#include "Containers.h"
#include "Creature.h"
#include "CreatureAIImpl.h"
#include "EventMap.h"
#include "GameObject.h"
#include "Map.h"
#include "MotionMaster.h"
#include "ObjectAccessor.h"
#include "ObjectGuid.h"
#include "ObjectMgr.h"
#include "PhasingHandler.h"
#include "Player.h"
#include "ScriptedCreature.h"
#include "ScriptMgr.h"
#include "SpellInfo.h"
#include "TaskScheduler.h"
#include "TemporarySummon.h"
#include "Unit.h"

#include <queue>

#include "Extended_Westfall_Defines.h"

namespace Scripts::EasternKingdoms::Westfall
{
    /*######
    ## npc_custom_westfall_stew_42617
    ######*/

    struct npc_custom_westfall_stew_42617 : public ScriptedAI
    {
        npc_custom_westfall_stew_42617(Creature* creature) : ScriptedAI(creature) { }

        void Reset() override
        {
            me->m_Events.AddEventAtOffset([this]()
                {
                    if (me && me->IsAlive())
                        FindHobosToFeed();
                }, std::chrono::seconds(1));
        }

        void FindHobosToFeed()
        {
            std::list<Creature*> list;
            GetCreatureListWithEntryInGrid(list, me, Creatures::HomelessStormwindCitizen, 10.f);
            GetCreatureListWithEntryInGrid(list, me, Creatures::HomelessStormwindCitizen2, 10.f);

            for (Creature* hungryHobo : list)
            {
                if (!hungryHobo || !hungryHobo->IsAlive())
                    continue;

                if (hungryHobo->HasAura(Spells::FullBelly))
                    continue;

                if (hungryHobo->HasAura(Spells::SleepAura))
                    hungryHobo->RemoveAurasDueToSpell(Spells::SleepAura);

                hungryHobo->SetStandState(UNIT_STAND_STATE_STAND);

                hungryHobo->m_Events.AddEventAtOffset([hungryHobo, this]()
                    {
                        if (hungryHobo && hungryHobo->IsAlive())
                            hungryHobo->GetMotionMaster()->MovePoint(1, me->GetRandomNearPosition(2.f));
                    }, std::chrono::seconds(1));

                hungryHobo->m_Events.AddEventAtOffset([hungryHobo, this]()
                    {
                        if (hungryHobo && hungryHobo->IsAlive())
                        {
                            hungryHobo->SetFacingToObject(me, true);
                            hungryHobo->SetStandState(UNIT_STAND_STATE_SIT);
                            hungryHobo->AI()->Talk(13);
                        }
                    }, std::chrono::seconds(4));

                hungryHobo->m_Events.AddEventAtOffset([hungryHobo, this]()
                    {
                        if (hungryHobo && hungryHobo->IsAlive())
                        {
                            hungryHobo->CastSpell(hungryHobo, Spells::FullBelly, true);
                            WorldObject* summoner = me->ToTempSummon()->GetSummoner();
                            if (summoner && summoner->ToPlayer())
                                summoner->ToPlayer()->KilledMonsterCredit(Creatures::WestfallStew);

                            hungryHobo->DespawnOrUnsummon(100s, 10s);
                        }
                    }, std::chrono::seconds(7));
            }

        }
    };

    /*######
    ## npc_custom_westfall_guard_42407
    ## ID 42407, SpawnID 275390
    ######*/

    struct npc_custom_westfall_guard_42407 : public ScriptedAI
    {
        npc_custom_westfall_guard_42407(Creature* creature) : ScriptedAI(creature) { }

        void Reset() override
        {
            _events.Reset();

            // This is needed since there might be an issue with factions OR
            // The npcs at the Sentinel Hill need to be passive
            _events.ScheduleEvent(Events::WestfalGuardPacifyDrifters, 500ms);

            _events.ScheduleEvent(Events::WestfalGuardCrowdReaction, 11s, 19s); // weird timers to create randomness
            _events.ScheduleEvent(Events::WestfalGuardCrowdReaction, 13s, 26s); // weird timers to create randomness
            _events.ScheduleEvent(Events::WestfalGuardCrowdReaction, 35s, 46s); // weird timers to create randomness
        }

        void MoveInLineOfSight(Unit* who) override
        {
            ScriptedAI::MoveInLineOfSight(who);

            if (!who || !who->IsCreature())
                return;

            Creature* drifterWithCartBuddy = who->ToCreature();

            if (drifterWithCartBuddy->GetSpawnId() == Spawns::DrifterWithCartBuddy)
            {
                if (!_drifterWithCartConvoOnCooldown && me->IsWithinDistInMap(who, 15.0f))
                {
                    _drifterWithCartConvoOnCooldown = true;
                    StartGuardConversation(drifterWithCartBuddy);
                }
            }
        }

        void UpdateAI(uint32 diff) override
        {
            _events.Update(diff);

            while (uint32 eventId = _events.ExecuteEvent())
            {
                switch (eventId)
                {
                case Events::WestfalGuardCrowdReaction:
                {
                    // Get crowd
                    std::list<Creature*> crowd;
                    GetCreatureListWithEntryInGrid(crowd, me, Creatures::HomelessStormwindCitizen, 50.f);
                    GetCreatureListWithEntryInGrid(crowd, me, Creatures::HomelessStormwindCitizen2, 50.f);
                    GetCreatureListWithEntryInGrid(crowd, me, Creatures::Transient, 50.f);
                    GetCreatureListWithEntryInGrid(crowd, me, Creatures::WestPlainsDrifter, 50.f);
                    GetCreatureListWithEntryInGrid(crowd, me, Creatures::SmallTimeHustler, 50.f);

                    // Remove hobos that are sitting down
                    crowd.remove_if([](Creature* c)
                        {
                            return !c || c->GetStandState() == UNIT_STAND_STATE_SIT || c->HasAura(Spells::SleepAura);
                        });

                    Creature* hobo = Trinity::Containers::SelectRandomContainerElement(crowd);
                    if(!hobo || !hobo->IsAlive())
                        hobo = Trinity::Containers::SelectRandomContainerElement(crowd);

                    if (hobo)
                    {
                        // Get guard target
                        // First check if we are left/right side citizens for Hobo Stew spit
                        if (hobo->GetSpawnId() == Spawns::LeftSideCitizen ||
                            hobo->GetSpawnId() == Spawns::RightSideCitizen ||
                            hobo->GetSpawnId() == Spawns::RightBackCitizen ||
                            hobo->GetSpawnId() == Spawns::LeftBackCitizen)
                        {
                            Creature* guard = hobo->FindNearestCreature(Creatures::SentinelHillGuard, 10.f);
                            if (guard)
                                hobo->CastSpell(guard, Spells::HoboStew);

                            _events.ScheduleEvent(Events::WestfalGuardCrowdReaction, 10s, 30s); // weird timers to create randomness
                            break; // break here for this action
                        }

                        // roll for throw banana/apple VS talk
                        if (roll_chance(50))
                        {
                            uint32 spell = RAND(Spells::RottenAppleToss, Spells::RottenBananaToss);
                            Creature* guard = hobo->FindNearestCreature(Creatures::SentinelHillGuard, 20.f);
                            if (guard)
                                hobo->CastSpell(guard, spell);

                            _events.ScheduleEvent(Events::WestfalGuardCrowdReaction, 10s, 30s); // weird timers to create randomness
                            break;
                        }

                        hobo->AI()->Talk(12);
                    }

                    _events.ScheduleEvent(Events::WestfalGuardCrowdReaction, 10s, 30s); // weird timers to create randomness
                    break;
                }
                case Events::WestfalGuardPacifyDrifters:
                {
                    // We do this by adding Immune To NPC to the Homeless
                    std::list<Creature*> list;
                    GetCreatureListWithEntryInGrid(list, me, Creatures::HomelessStormwindCitizen, 50.f);
                    GetCreatureListWithEntryInGrid(list, me, Creatures::HomelessStormwindCitizen2, 50.f);
                    GetCreatureListWithEntryInGrid(list, me, Creatures::SmallTimeHustler, 50.f);

                    for (Creature* hobo : list)
                    {
                        if (!hobo->IsAlive())
                            continue;

                        hobo->SetImmuneToNPC(true);
                    }
                    break;
                }
                }
            }
        }

        void StartGuardConversation(Creature* drifter)
        {
            Talk(0);

            me->m_Events.AddEventAtOffset([this]()
                {
                    if(me && me->IsAlive())
                        Talk(1);
                }, std::chrono::seconds(4));

            drifter->m_Events.AddEventAtOffset([drifter]()
                {
                    if (drifter && drifter->IsAlive())
                        drifter->AI()->Talk(11);
                }, std::chrono::seconds(10));

            me->m_Events.AddEventAtOffset([this]()
                {
                    if (me && me->IsAlive())
                        Talk(2);
                }, std::chrono::seconds(15));

            me->m_Events.AddEventAtOffset([this]()
                {
                    if (me && me->IsAlive())
                        Talk(3);
                }, std::chrono::seconds(20));

            if (Creature* cart = me->GetMap()->GetCreatureBySpawnId(Spawns::CartBuddy))
            {
                cart->m_Events.AddEventAtOffset([cart]()
                    {
                        if (cart && cart->IsAlive())
                            cart->AI()->Talk(0);
                    }, std::chrono::seconds(25));

                cart->m_Events.AddEventAtOffset([cart, drifter]()
                    {
                        if (cart && cart->IsAlive())
                        {
                            cart->AI()->Talk(1);
                            cart->DespawnOrUnsummon(20s);
                            drifter->DespawnOrUnsummon(20s);
                        }
                    }, std::chrono::seconds(30));
            }
        }

    private:
        EventMap _events;
        bool _drifterWithCartConvoOnCooldown = false;
    };

    /*######
    ## npc_custom_lous_parting_thoughts_trigger
    ## ID 42562
    ######*/

    struct npc_custom_lous_parting_thoughts_trigger : public ScriptedAI
    {
        npc_custom_lous_parting_thoughts_trigger(Creature* creature) : ScriptedAI(creature) { }

        void Reset() override
        {
            _phase = 0;
            _deadThugs = 0;
            _eventStarted = false;
            _eventLocked = false;
            _phaseTimer = 1000;

            for (auto& guid : _thugGuids)
                guid.Clear();
        }

        void MoveInLineOfSight(Unit* who) override
        {
            if (_eventStarted)
                return;

            Player* player = who->ToPlayer();
            if (!player)
                return;

            if (player->GetQuestStatus(Quests::LousPartingThoughts) != QUEST_STATUS_INCOMPLETE)
                return;

            if (!who->IsWithinDistInMap(me, 30.0f))
                return;

            _playerGuid = player->GetGUID();

            StartEvent();
        }

        void StartEvent()
        {
            _eventStarted = true;

            for (uint8 i = 0; i < 4; ++i)
            {
                if (Creature* thug = me->SummonCreature(Creatures::WestfallThug, Positions::Q26232ThugPositions[i], TEMPSUMMON_TIMED_DESPAWN_OUT_OF_COMBAT, 90s))
                {
                    thug->SetReactState(REACT_PASSIVE);
                    thug->SetUnitFlag(UNIT_FLAG_NON_ATTACKABLE | UNIT_FLAG_IMMUNE_TO_PC);

                    _thugGuids[i] = thug->GetGUID();
                }
            }

            _phaseTimer = 2000;
        }

        Creature* GetThug(uint8 index)
        {
            return ObjectAccessor::GetCreature(*me, _thugGuids[index]);
        }

        void SetData(uint32 id, uint32 /*value*/) override
        {
            if (id != 1)
                return;

            ++_deadThugs;

            if (_deadThugs >= 4)
            {
                if (Player* player = ObjectAccessor::GetPlayer(*me, _playerGuid))
                {
                    me->m_Events.AddEventAtOffset([this, player]()
                        {
                            if (me)
                                me->PlayDirectSound(Sounds::WeaponFire1, player);
                        }, std::chrono::seconds(1));

                    me->m_Events.AddEventAtOffset([this, player]()
                        {
                            if (me)
                                me->PlayDirectSound(Sounds::WeaponFire2, player);
                        }, std::chrono::seconds(2));

                    me->m_Events.AddEventAtOffset([this, player]()
                        {
                            if (me)
                                me->PlayDirectSound(Sounds::WomanScream, player);
                        }, std::chrono::seconds(3));

                    me->m_Events.AddEventAtOffset([this, player]()
                        {
                            if (me)
                                me->PlayDirectSound(Sounds::Warning, player);
                            Talk(Talks::Q26232ReturnToFurlsbrowCottage);
                        }, std::chrono::seconds(5));

                    player->KilledMonsterCredit(Creatures::LousPartingThoughtsCredit, _playerGuid);
                }
                Reset();
            }
        }

        void UpdateAI(uint32 diff) override
        {
            if (!_eventStarted || _eventLocked)
                return;

            if (_phaseTimer > diff)
            {
                _phaseTimer -= diff;
                return;
            }

            // Out of bounds = event finished
            if (_phase >= std::size(Dialogue::Q26232Steps))
                return;

            auto const& step = Dialogue::Q26232Steps[_phase];

            // Handle normal talk phases
            if (step.thugIndex != 255)
            {
                if (Creature* thug = GetThug(step.thugIndex))
                    thug->AI()->Talk(step.talkId);
            }
            else
            {
                // Handle special phases
                switch (_phase)
                {
                case 4: // Face player
                    if (Player* player = ObjectAccessor::GetPlayer(*me, _playerGuid))
                        for (auto guid : _thugGuids)
                            if (Creature* thug = ObjectAccessor::GetCreature(*me, guid))
                                thug->SetFacingToObject(player);
                    break;

                case 8: // Enable combat
                    for (auto guid : _thugGuids)
                        if (Creature* thug = ObjectAccessor::GetCreature(*me, guid))
                        {
                            thug->RemoveUnitFlag(UNIT_FLAG_NON_ATTACKABLE | UNIT_FLAG_IMMUNE_TO_PC);
                            thug->SetFaction(14);
                            thug->SetReactState(REACT_AGGRESSIVE);
                        }
                    break;

                case 9: // Engage player
                    if (Player* player = ObjectAccessor::GetPlayer(*me, _playerGuid))
                        for (auto guid : _thugGuids)
                            if (Creature* thug = ObjectAccessor::GetCreature(*me, guid))
                                thug->EngageWithTarget(player);

                    _eventLocked = true;
                    break;
                }
            }

            // Set next timer and advance phase
            _phaseTimer = step.delay;
            ++_phase;
        }

    private:
        ObjectGuid _playerGuid;
        std::array<ObjectGuid, 4> _thugGuids;
        uint32 _phaseTimer = 0;
        uint8  _phase = 0;
        uint8  _deadThugs = 0;
        bool   _eventStarted = false;
        bool   _eventLocked = false;
    };

    /*######
    ## npc_custom_lous_parting_thoughts_thug
    ## ID 42387
    ######*/

    struct npc_custom_lous_parting_thoughts_thug : public ScriptedAI
    {
        npc_custom_lous_parting_thoughts_thug(Creature* creature) : ScriptedAI(creature) { }

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

    /*######
    ## npc_custom_salma_saldean_235
    ## ID 235
    ######*/

    struct npc_custom_salma_saldean_235 : public ScriptedAI
    {
        npc_custom_salma_saldean_235(Creature* creature) : ScriptedAI(creature) { }

        void OnQuestReward(Player* player, Quest const* quest, LootItemType /*type*/, uint32 /*opt*/) override
        {
            if (quest->GetQuestId() == Quests::NewWestfallStew)
            {
                // We remove the flag for the duration of the event
                // To avoid multiple triggers
                me->RemoveNpcFlag(UNIT_NPC_FLAG_QUESTGIVER); 
                Talk(0, player);
                me->SetWalk(true);

                me->m_Events.AddEventAtOffset([this]()
                    {
                        if (me && me->IsAlive())
                            me->GetMotionMaster()->MovePoint(1, Positions::SalmaSaldeanStew);
                    }, std::chrono::seconds(2));

                if (Creature* orphan = player->FindNearestCreature(Creatures::WestfallOrphan, 10.f))
                {
                    orphan->m_Events.AddEventAtOffset([orphan, player]()
                        {
                            if (orphan && orphan->IsAlive())
                                orphan->AI()->Talk(0, player);
                        }, std::chrono::seconds(15));

                    orphan->m_Events.AddEventAtOffset([orphan, player]()
                        {
                            if (orphan && orphan->IsAlive())
                            {
                                orphan->AI()->Talk(1, player);
                                orphan->AI()->Talk(2);
                            }
                        }, std::chrono::seconds(20));
                }
            }
        }

        void MovementInform(uint32 type, uint32 pointId) override
        {
            if (type != POINT_MOTION_TYPE)
                return;

            if (pointId == 1)
            {
                me->HandleEmoteCommand(EMOTE_ONESHOT_USE_STANDING);

                me->m_Events.AddEventAtOffset([this]()
                    {
                        if (me && me->IsAlive())
                        {
                            me->SetOrientation(2.4434f);
                            Talk(1);

                            std::list<Creature*> orphans;
                            GetCreatureListWithEntryInGrid(orphans, me, Creatures::WestfallOrphan, 20.f);

                            for (Creature* orphan : orphans)
                            {
                                if (orphan && orphan->IsAlive())
                                {
                                    orphan->GetMotionMaster()->MovePoint(1, me->GetRandomNearPosition(2.5f));
                                    orphan->DespawnOrUnsummon(15s, 1s);
                                }

                            }

                            me->DespawnOrUnsummon(15s, 1s);
                        }
                    }, std::chrono::seconds(5));

            }
        }
    };

    /*######
    ## npc_custom_gryan_stoutmantle_234
    ## ID 234
    ######*/

    struct npc_custom_gryan_stoutmantle_234 : public ScriptedAI
    {
        npc_custom_gryan_stoutmantle_234(Creature* creature) : ScriptedAI(creature) { }

        void OnQuestReward(Player* player, Quest const* quest, LootItemType /*type*/, uint32 /*opt*/) override
        {
            if (quest->GetQuestId() == Quests::TheDefiasBrotherhoodFinal)
                Talk(0, player);

            if (quest->GetQuestId() == Quests::InDefenseOfWestfall)
            {
                me->SetWalk(true);
                me->GetMotionMaster()->MovePoint(1, Positions::GryanStoutmantleTalkRipsnarl);
            }
        }

        void MovementInform(uint32 type, uint32 pointId) override
        {
            if (type != POINT_MOTION_TYPE)
                return;

            if (pointId == 1)
            {
                Talk(3);

                Creature* ripsnarl = me->FindNearestCreatureWithOptions(10.f, { .CreatureId = Creatures::RipsnarlAtTower, .IgnorePhases = true });
                if (ripsnarl && ripsnarl->IsAlive())
                {
                    me->SetFacingToObject(ripsnarl);

                    ripsnarl->m_Events.AddEventAtOffset([ripsnarl]()
                        {
                            if (ripsnarl && ripsnarl->IsAlive())
                                ripsnarl->AI()->Talk(1);
                        }, std::chrono::seconds(7));
                }

                me->m_Events.AddEventAtOffset([this]()
                    {
                        if (me && me->IsAlive())
                            me->AI()->Talk(4);
                    }, std::chrono::seconds(12));

                me->m_Events.AddEventAtOffset([this]()
                    {
                        if (me && me->IsAlive())
                            me->GetMotionMaster()->MovePoint(2, me->GetHomePosition());
                    }, std::chrono::seconds(17));
            }
        }
    };

    /*######
    ## npc_custom_agent_kearnen
    ## ID 7024
    ######*/

    struct npc_custom_agent_kearnen : public ScriptedAI
    {
        npc_custom_agent_kearnen(Creature* creature) : ScriptedAI(creature) { }

        void Reset() override
        {
            _events.Reset();
            _playerGuid.Clear();

            std::queue<ObjectGuid> empty;
            _pendingTargets = {};
        }

        void Assist(Player* player, Creature* target)
        {
            if (!player || !target || !target->IsAlive())
                return;

            _playerGuid = player->GetGUID();
            _pendingTargets.push(target->GetGUID());
            _events.ScheduleEvent(Events::AgentKearnenCastKillShot, 5s, 10s);
        }

        void UpdateAI(uint32 diff) override
        {
            _events.Update(diff);

            while (uint32 eventId = _events.ExecuteEvent())
            {
                switch (eventId)
                {
                case Events::AgentKearnenCastKillShot:
                {
                    if (_pendingTargets.empty())
                        break;

                    ObjectGuid TargetGUID = _pendingTargets.front();
                    _pendingTargets.pop();

                    Creature* target = ObjectAccessor::GetCreature(*me, TargetGUID);
                    Player* player = ObjectAccessor::GetPlayer(*me, _playerGuid);

                    if (target && target->IsAlive() && player)
                    {
                        me->SetFacingToObject(target);
                        me->CastSpell(target, Spells::KillShot, true);
                        Talk(0, player);
                    }

                    // If more targets remain, schedule again
                    if (!_pendingTargets.empty())
                        _events.Repeat(5s, 10s);

                    break;
                }
                }
            }
        }

    private:
        EventMap _events;
        ObjectGuid _playerGuid;
        std::queue<ObjectGuid> _pendingTargets;
    };

    /*######
    ## npc_custom_elite_mercenary
    ## ID 42656
    ######*/

    struct npc_custom_elite_mercenary : public ScriptedAI
    {
        npc_custom_elite_mercenary(Creature* creature) : ScriptedAI(creature) { }

        void Reset() override
        {
            _assistTriggered = false;
        }

        void JustEngagedWith(Unit* who) override
        {
            if (!who)
                return;

            Player* player = who->GetCharmerOrOwnerPlayerOrPlayerItself();
            if (!player)
                return;

            if (_assistTriggered)
                return;

            if (player->GetQuestStatus(Quests::SecretsOfTheTower) != QUEST_STATUS_INCOMPLETE)
                return;

            Creature* agent = me->FindNearestCreature(Creatures::AgentKearnen, 150.0f, true);
            if (!agent)
                return;

            if (auto* ai = CAST_AI(npc_custom_agent_kearnen, agent->AI()))
                ai->Assist(player, me);

            _assistTriggered = true;
        }

    private:
        bool _assistTriggered = false;
    };

    /*######
    ## npc_custom_trigger_mortwake_tower
    ## ID 17234
    ######*/

    struct npc_custom_trigger_mortwake_tower : public ScriptedAI
    {
        npc_custom_trigger_mortwake_tower(Creature* creature) : ScriptedAI(creature) {}

        void Reset() override
        {
            _stepIndex = 0;
            _summonTimer = 2000;
            _bSumm = false;
            _bSumm1 = false;
            _bSumm2 = false;
            _eventRunning = false;
            _playerGUID.Clear();
            _oafGUID.Clear();
            _helixGUID.Clear();
            _shadowyGUID.Clear();
        }

        void SetGUID(ObjectGuid const& guid, int32 id) override
        {
            if (id == 1 && !_eventRunning)
            {
                _playerGUID = guid;
                StartEvent();
                _eventRunning = true;
            }
        }

        void StartEvent()
        {
            if (_bSumm)
                return;

            if (!_bSumm1)
            {
                if (Creature* shadowy = me->SummonCreature(Creatures::MortwakeShadowyFigure, -11138.659f, 545.20f, 70.30f, 0.19f, TEMPSUMMON_TIMED_DESPAWN_OUT_OF_COMBAT, 90s))
                {
                    _shadowyGUID = shadowy->GetGUID();
                    shadowy->SetUnitFlag(UNIT_FLAG_UNINTERACTIBLE);
                    shadowy->SetWalk(true);
                    shadowy->GetMotionMaster()->MovePoint(0, -11131.710f, 546.810f, 70.380f);
                    _bSumm1 = true;
                }
            }

            if (!_bSumm2)
            {
                if (Creature* oaf = me->SummonCreature(Creatures::MortwakeOaf, -11128.11f, 547.52f, 70.41f, 3.32f, TEMPSUMMON_TIMED_DESPAWN_OUT_OF_COMBAT, 90s))
                {
                    _oafGUID = oaf->GetGUID();
                    oaf->SetUnitFlag(UNIT_FLAG_UNINTERACTIBLE);

                    if (Creature* helix = me->SummonCreature(Creatures::MortwakeHelix, -11128.11f, 547.52f, 70.41f, 3.32f, TEMPSUMMON_TIMED_DESPAWN_OUT_OF_COMBAT, 90s))
                    {
                        _helixGUID = helix->GetGUID();
                        helix->SetUnitFlag(UNIT_FLAG_UNINTERACTIBLE);
                        helix->EnterVehicle(oaf, 0);
                        _bSumm2 = true;
                    }
                }
            }

            if (_bSumm1 && _bSumm2)
            {
                _bSumm = true;
                _summonTimer = 2000;
                _stepIndex = 0;
            }
        }

        void UpdateAI(uint32 diff) override
        {
            if (!_eventRunning || !_bSumm)
                return;

            if (_summonTimer > diff)
            {
                _summonTimer -= diff;
                return;
            }

            Player* player = ObjectAccessor::GetPlayer(*me, _playerGUID);
            Creature* oaf = ObjectAccessor::GetCreature(*me, _oafGUID);
            Creature* helix = ObjectAccessor::GetCreature(*me, _helixGUID);
            Creature* shadowy = ObjectAccessor::GetCreature(*me, _shadowyGUID);

            if (!player || !oaf || !helix || !shadowy)
                return;

            // Initial facing once, before dialogue
            if (_stepIndex == 0)
                oaf->SetInFront(me);

            if (_stepIndex >= std::size(Dialogue::MortwakeSequence))
                return;

            Dialogue::MortwakeStep const& step = Dialogue::MortwakeSequence[_stepIndex];

            switch (step.actor)
            {
            case Dialogue::MortwakeActor::Helix:
                if (step.textId != -1)
                    helix->AI()->Talk(step.textId);
                break;
            case Dialogue::MortwakeActor::Shadowy:
                if (step.textId != -1)
                    shadowy->AI()->Talk(step.textId);
                break;
            case Dialogue::MortwakeActor::PlayerCredit:
                player->CastSpell(player, Spells::QuestCredit26290, true);
                break;
            case Dialogue::MortwakeActor::Exit:
            {
                shadowy->CastSpell(shadowy, Spells::TeleportVisual, true);
                shadowy->DespawnOrUnsummon(1s);
                oaf->CastSpell(shadowy, Spells::TeleportVisual, true);
                helix->DespawnOrUnsummon(1s);
                oaf->DespawnOrUnsummon(1s);
                Reset();
                return;
            }
            }

            _summonTimer = step.delayMs;
            ++_stepIndex;
        }

        private:
            uint32 _summonTimer = 0;
            uint8 _stepIndex = 0;
            ObjectGuid _playerGUID;
            ObjectGuid _oafGUID;
            ObjectGuid _helixGUID;
            ObjectGuid _shadowyGUID;

            bool _bSumm = false;
            bool _bSumm1 = false;
            bool _bSumm2 = false;
            bool _eventRunning = false;
    };

    /*######
    ## npc_custom_ripsnarl
    ## ID 42635
    ## ID 42748
    ######*/

    struct npc_custom_ripsnarl : public ScriptedAI
    {
        npc_custom_ripsnarl(Creature* c) : ScriptedAI(c) { }

        void Reset() override
        {
            ScriptedAI::Reset();
            me->SetFaction(7);
            me->SetReactState(REACT_PASSIVE);
            me->SetUnitFlag(UnitFlags(UNIT_FLAG_NON_ATTACKABLE));
            me->SetWalk(true);
        }

        void SpellHit(WorldObject* /*caster*/, SpellInfo const* spellInfo) override
        {
            if (spellInfo->Id == Spells::HitMe)
            {
                me->RemoveAllAuras();
                me->SetAIAnimKitId(0);
                DoCast(Spells::TransformHuman);

                WorldObject* summoner = me->ToTempSummon()->GetSummoner();
                if (summoner && summoner->ToPlayer())
                {

                    me->m_Events.AddEventAtOffset([this, summoner]()
                        {
                            Talk(0, summoner);
                        },
                        std::chrono::seconds(7)
                    );
                }

                me->m_Events.AddEventAtOffset([this]()
                    {
                        DoCast(Spells::AdmiralHat);
                    },
                    std::chrono::seconds(9)
                );

                me->m_Events.AddEventAtOffset([this]()
                    {
                        me->GetMotionMaster()->MovePoint(1, Positions::BRRipsnarl1);
                    },
                    std::chrono::seconds(10)
                );
            }
        }

        void MovementInform(uint32 type, uint32 pointId) override
        {
            if (type != POINT_MOTION_TYPE)
                return;

            if (pointId == 1)
            {
                me->GetMotionMaster()->MovePoint(2, Positions::BRRipsnarl2);
            }

            if (pointId == 2)
            {
                me->GetMotionMaster()->MovePoint(3, Positions::BRRipsnarl3);
            }
        }
    };

    /*######
    ## npc_custom_moonbrook_player_trigger
    ## ID 43515
    ######*/

    struct npc_custom_moonbrook_player_trigger : public ScriptedAI
    {
        npc_custom_moonbrook_player_trigger(Creature* creature) : ScriptedAI(creature) {}

        void Reset() override
        {
            _stepIndex = 0;
            _summonTimer = 2000;
            _eventRunning = false;
            _playerGUID.Clear();
            _shadowyGUID.Clear();
        }

        void SetGUID(ObjectGuid const& guid, int32 id) override
        {
            if (id == 1 && !_eventRunning)
            {
                _playerGUID = guid;
                StartSpeech();
                _eventRunning = true;
            }
        }

        void StartSpeech()
        {
            Player* player = ObjectAccessor::FindPlayer(_playerGUID);
            if (!player)
                return;

            if (Creature* shadowy = player->FindNearestCreature(Creatures::SpawnedShadowyFigureAtMoonbrook, 50.f))
            {
                _shadowyGUID = shadowy->GetGUID();
                _summonTimer = 2000;
                _stepIndex = 0;
            }
        }

        void UpdateAI(uint32 diff) override
        {
            if (!_eventRunning)
                return;

            if (_summonTimer > diff)
            {
                _summonTimer -= diff;
                return;
            }

            Player* player = ObjectAccessor::GetPlayer(*me, _playerGUID);
            Creature* shadowy = ObjectAccessor::GetCreature(*me, _shadowyGUID);

            if (!player || !shadowy)
                return;

            if (_stepIndex >= std::size(Dialogue::MoonbrookSequence))
                return;

            Dialogue::MoonbrookStep const& step = Dialogue::MoonbrookSequence[_stepIndex];

            // Normal dialogue steps
            if (step.textId != -1)
            {
                switch (step.actor)
                {
                case Dialogue::MoonbrookActor::Trigger:
                    Talk(step.textId, player);
                    break;

                case Dialogue::MoonbrookActor::Shadowy:
                    if (shadowy)
                        shadowy->AI()->Talk(step.textId, player);
                    break;
                }
                _summonTimer = step.delayMs;
                ++_stepIndex;
                return;
            }

            // Final step: crowd reacts + credit + despawn
            HandleFinalStep(player, shadowy);
            Reset(); // clean up state
        }

        void HandleFinalStep(Player* player, Creature* shadowy)
        {
            std::list<Creature*> crowd;
            GetCreatureListWithEntryInGrid(crowd, me, Creatures::HomelessStormwindCitizen, 30.f);
            GetCreatureListWithEntryInGrid(crowd, me, Creatures::HomelessStormwindCitizen2, 30.f);
            GetCreatureListWithEntryInGrid(crowd, me, Creatures::WestPlainsDrifter, 30.f);
            GetCreatureListWithEntryInGrid(crowd, me, Creatures::SmallTimeHustler, 30.f);

            for (Creature* listener : crowd)
            {
                int8 delay = urand(500, 4000);
                listener->m_Events.AddEventAtOffset(
                    [listener]()
                    {
                        if (listener && listener->IsAlive())
                            listener->HandleEmoteCommand(EMOTE_ONESHOT_APPLAUD);
                    },
                    std::chrono::milliseconds(delay)
                );

                listener->m_Events.AddEventAtOffset([listener, player]()
                    {
                        if (listener && listener->IsAlive())
                        {
                            listener->PlayDistanceSound(Sounds::CrowdCheer, player);
                            listener->HandleEmoteCommand(EMOTE_ONESHOT_CHEER);
                            listener->DespawnOrUnsummon(10s);
                        }
                    },
                    std::chrono::seconds(5)
                );
            }

            me->m_Events.AddEventAtOffset([this, shadowy, player]()
                {
                    if (player)
                        player->KilledMonsterCredit(Creatures::SpawnedShadowyFigureAtMoonbrook, _playerGUID);

                    if (shadowy && shadowy->IsAlive())
                    {
                        shadowy->CastSpell(shadowy, Spells::TeleportVisual, true);
                        shadowy->DespawnOrUnsummon(1s);
                    }
                    Reset();
                },
                std::chrono::seconds(10)
            );
        }

        private:
            uint32 _summonTimer = 0;
            uint8 _stepIndex = 0;

            ObjectGuid _playerGUID;
            ObjectGuid _shadowyGUID;

            bool _eventRunning = false;
    };

    /*######
    ## npc_vision_of_the_past
    ## ID 42693
    ######*/

    struct npc_vision_of_the_past : public ScriptedAI
    {
        npc_vision_of_the_past(Creature* creature) : ScriptedAI(creature) {}

        void Reset() override
        {
            _events.Reset();
            me->SetCanFly(true);
            me->SetWalk(false);
            me->SetSpeed(MOVE_FLIGHT, 5.f);
            me->SetDisableGravity(true);
        }

        // Player boards the vision NPC (vehicle)
        void PassengerBoarded(Unit* who, int8 /*seatId*/, bool apply) override
        {
            if (!apply)
                return; // we need this to avoid double triggers upon removal from vehicle

            me->NearTeleportTo(Positions::VisionEntryPosition, false);
            Talk(Talks::VisionIntro, who);
            _events.ScheduleEvent(Events::VisionOfThePastStartPath, 3s);
        }

        void WaypointPathEnded(uint32 /*nodeId*/, uint32 pathId) override
        {
            if (pathId != Paths::VisionOfThePast)
                return;

            // combat event logic starts from here
            _events.ScheduleEvent(Events::VisionOfThePastCombat, 3s);
        }

        void UpdateAI(uint32 diff) override
        {
            _events.Update(diff);

            while (uint32 id = _events.ExecuteEvent())
            {
                switch (id)
                {
                case Events::VisionOfThePastStartPath:
                    me->GetMotionMaster()->MovePath(Paths::VisionOfThePast, false);
                    break;
                case Events::VisionOfThePastCombat:
                {
                    Creature* vc = me->SummonCreature(Creatures::VisionOfThePastEdwinVC, Positions::VisionVC);
                    if (!vc)
                        return;

                    _allies.mage = me->SummonCreature(Creatures::VisionAllianceMage, Positions::VisionAllianceNPCS, TEMPSUMMON_TIMED_DESPAWN, 5min);
                    _allies.warrior = me->SummonCreature(Creatures::VisionAllianceWarrior, Positions::VisionAllianceNPCS, TEMPSUMMON_TIMED_DESPAWN, 5min);
                    _allies.rogue = me->SummonCreature(Creatures::VisionAllianceRogue, Positions::VisionAllianceNPCS, TEMPSUMMON_TIMED_DESPAWN, 5min);
                    _allies.priest = me->SummonCreature(Creatures::VisionAlliancePriest, Positions::VisionAllianceNPCS, TEMPSUMMON_TIMED_DESPAWN, 5min);
                    _allies.hunter = me->SummonCreature(Creatures::VisionAllianceHunter, Positions::VisionAllianceNPCS, TEMPSUMMON_TIMED_DESPAWN, 5min);
                    if (_allies.warrior)
                    {
                        if (_allies.rogue) _allies.rogue->GetMotionMaster()->MoveFollow(_allies.warrior, 2.f, float(M_PI));

                        if (_allies.mage)
                        {
                            if(Creature* mageBunny = _allies.mage->GetMap()->GetCreatureBySpawnId(Spawns::VisionBunnyForMage))
                                _allies.mage->GetMotionMaster()->MovePoint(1, mageBunny->GetPosition());
                        }
                        
                        if (_allies.priest)
                        {
                            if(Creature* priestBunny = _allies.priest->GetMap()->GetCreatureBySpawnId(Spawns::VisionBunnyForPriest))
                                _allies.priest->GetMotionMaster()->MovePoint(1, priestBunny->GetPosition());
                        }

                        if (_allies.hunter)
                        {
                            if(Creature* hunterBunny = _allies.hunter->GetMap()->GetCreatureBySpawnId(Spawns::VisionBunnyForHunter1))
                                _allies.hunter->GetMotionMaster()->MovePoint(1, hunterBunny->GetPosition());
                        }

                        Schedule(_allies.warrior, [this]() { _allies.warrior->GetMotionMaster()->MovePoint(1, Positions::VisionAllianceNPCSForward); }, 1s);
                        Schedule(_allies.warrior, [this,vc]()
                            {
                                _allies.warrior->SetFacingToObject(vc, true);
                                _allies.warrior->SetHomePosition(_allies.warrior->GetRandomPoint(Positions::VisionAllianceNPCSForward, 3.f));
                                _allies.warrior->AI()->Talk(Talks::VisionAllianceWarr0);
                            }, 6s);
                        Schedule(_allies.warrior, [this]() { _allies.warrior->AI()->Talk(Talks::VisionAllianceWarr1); }, 11s);
                        Schedule(_allies.warrior, [this, vc]() { vc->AI()->AttackStart(_allies.warrior); }, 15s);
                        Schedule(_allies.warrior, [this, vc]() { _allies.warrior->AI()->AttackStart(vc); }, 17s);

                        Schedule(_allies.priest, [this, vc]()
                            {
                                _allies.priest->SetFacingToObject(vc, true);
                                _allies.priest->SetHomePosition(_allies.priest->GetRandomPoint(Positions::VisionAllianceNPCSForward, 3.f));
                                _allies.priest->CastSpell(_allies.priest, Spells::AlliancePriestFortitude);
                            }, 6s);
                        Schedule(_allies.priest, [this, vc]() { _allies.priest->AI()->AttackStartCaster(vc, 40.f); }, 17s);

                        Schedule(_allies.mage, [this, vc]()
                            {
                                _allies.mage->SetFacingToObject(vc, true);
                                _allies.mage->SetHomePosition(_allies.mage->GetRandomPoint(Positions::VisionAllianceNPCSForward, 3.f));
                            }, 6s);
                        Schedule(_allies.mage, [this, vc]() { _allies.mage->AI()->AttackStartCaster(vc, 40.f); }, 17s);

                        Schedule(_allies.hunter, [this, vc]()
                            {
                                _allies.hunter->SetFacingToObject(vc, true);
                                _allies.hunter->SetHomePosition(_allies.hunter->GetRandomPoint(Positions::VisionAllianceNPCSForward, 3.f));
                            }, 6s);
                        Schedule(_allies.hunter, [this, vc]() { _allies.hunter->AI()->AttackStart(vc); }, 17s);
                        Schedule(_allies.hunter, [this]()
                            {
                                _allies.hunter->GetMotionMaster()->Clear();
                                if(Creature* hunterBunny = _allies.hunter->GetMap()->GetCreatureBySpawnId(Spawns::VisionBunnyForHunter2))
                                    _allies.hunter->GetMotionMaster()->MovePoint(2, hunterBunny->GetPosition());
                            }, 18s);

                        Schedule(_allies.rogue, [this, vc]()
                            {
                                _allies.rogue->SetFacingToObject(vc, true);
                                _allies.rogue->SetHomePosition(_allies.rogue->GetRandomPoint(Positions::VisionAllianceNPCSForward, 3.f));
                            }, 6s);
                        Schedule(_allies.rogue, [this, vc]() { _allies.rogue->AI()->AttackStart(vc); }, 17s);
                    }

                    _events.ScheduleEvent(Events::VisionAllianceNPCSLeave, 25s);
                    break;
                }
                case Events::VisionAllianceNPCSLeave:
                {
                    if(Creature* warrior = me->FindNearestCreature(Creatures::VisionAllianceWarrior, 100.f))
                    {
                        warrior->AI()->Talk(Talks::VisionAllianceWarr2);
                        Schedule(_allies.warrior, [this]() { _allies.warrior->AI()->Talk(Talks::VisionAllianceWarr3); }, 5s);
                        Schedule(_allies.warrior, [this]()
                            {
                                _allies.warrior->DespawnOrUnsummon(10s);
                                _allies.warrior->GetMotionMaster()->MovePoint(2, Positions::VisionAllianceNPCSLeave);
                            }, 10s);

                        Schedule(_allies.rogue, [this]()
                            {
                                _allies.rogue->DespawnOrUnsummon(10s);
                                _allies.rogue->GetMotionMaster()->MovePoint(2, Positions::VisionAllianceNPCSLeave);
                            }, 10s);

                        Schedule(_allies.mage, [this]()
                            {
                                _allies.mage->DespawnOrUnsummon(10s);
                                _allies.mage->GetMotionMaster()->MovePoint(2, Positions::VisionAllianceNPCSLeave);
                            }, 10s);

                        Schedule(_allies.priest, [this]()
                            {
                                _allies.priest->DespawnOrUnsummon(10s);
                                _allies.priest->GetMotionMaster()->MovePoint(2, Positions::VisionAllianceNPCSLeave);
                            }, 10s);

                        Schedule(_allies.hunter, [this]()
                            {
                                _allies.hunter->DespawnOrUnsummon(10s);
                                _allies.hunter->GetMotionMaster()->MovePoint(3, Positions::VisionAllianceNPCSLeave);
                            }, 10s);
                    }
                    _events.ScheduleEvent(Events::VisionChildScene, 15s);
                    break;
                }
                case Events::VisionChildScene:
                {
                    if (Creature* vanessa = me->SummonCreature(Creatures::VisionOfThePastVanessaVC, Positions::VisionVC, TEMPSUMMON_TIMED_DESPAWN, 2min))
                    {
                        if (Creature* vc = vanessa->FindNearestCreature(Creatures::VisionOfThePastEdwinVC, 50.f, false))
                        {
                            vanessa->SetWalk(true);
                            vanessa->GetMotionMaster()->MoveCloserAndStop(1, vc, 2.f);
                            vanessa->m_Events.AddEventAtOffset([vanessa]()
                                {
                                    if (vanessa && vanessa->IsAlive())
                                    {
                                        vanessa->AI()->Talk(Talks::VisionVanessa0);
                                        vanessa->DespawnOrUnsummon(10s);
                                    }
                                }, std::chrono::seconds(8));
                        }
                    }
                    _events.ScheduleEvent(Events::VisionOfThePastQuestReward, 15s);
                    break;
                }
                case Events::VisionOfThePastQuestReward:
                {
                    if (Unit* passenger = me->GetCharmerOrOwnerPlayerOrPlayerItself())
                        if (Player* player = passenger->ToPlayer())
                            player->CastSpell(player, Spells::VisionOfThePastCredit, true);

                    _events.ScheduleEvent(Events::VisionOfThePastExit, 3s);

                    break;
                }
                case Events::VisionOfThePastExit:
                {
                    me->NearTeleportTo(Positions::VisionLeavePosition, false);
                    _events.ScheduleEvent(Events::VisionRemovePassenger, 1s);
                    break;
                }
                case Events::VisionRemovePassenger:
                {
                    if (Unit* passenger = me->GetCharmerOrOwnerPlayerOrPlayerItself())
                        passenger->ExitVehicle();
                    me->DespawnOrUnsummon(1s);
                    break;
                }
                }
            }
        }

    private:
        EventMap _events;
        AllianceGroup _allies = {};
    };

    /*######
    ## npc_vision_defias_pirate
    ## ID 657
    ######*/

    struct npc_vision_defias_pirate : public ScriptedAI
    {
        npc_vision_defias_pirate(Creature* c) : ScriptedAI(c) { }

        void Reset() override
        {
            ScriptedAI::Reset();
            me->CastSpell(me, Spells::BloodsailCompanion, true);
        }
    };

    /*######
    ## npc_gryan_stoutmantle_act2
    ## ID 42750
    ######*/

    struct npc_gryan_stoutmantle_act2 : public ScriptedAI
    {
        npc_gryan_stoutmantle_act2(Creature* c) : ScriptedAI(c) { }

        void Reset() override
        {
            ScriptedAI::Reset();
        }

        void SetGUID(ObjectGuid const& guid, int32 id) override
        {
            if (id == 1)
            {
                _playerGuid = guid;
                Player* player = ObjectAccessor::GetPlayer(*me, _playerGuid);
                if (player)
                {
                    Talk(0, player); // Stay alert

                    if (Creature* hope = player->FindNearestCreature(Creatures::SpawnedHopeSaldeanAtTower, 50.f))
                        hope->AI()->SetGUID(_playerGuid, 1);
                }
            }
        }

        void SetData(uint32 id, uint32 value) override
        {
            if (id == 1 && value == 1)
            {
                Player* player = ObjectAccessor::GetPlayer(*me, _playerGuid);
                if (player)
                {
                    Talk(1, player);
                }
            }

            if (id == 1 && value == 2)
            {
                Player* player = ObjectAccessor::GetPlayer(*me, _playerGuid);
                if (player)
                {
                    Talk(2, player);

                    std::list<Player*> players;
                    me->GetPlayerListInGrid(players, 50.0f);

                    for (std::list<Player*>::const_iterator itr = players.begin(); itr != players.end(); ++itr)
                        (*itr)->GroupEventHappens(Quests::RiseOfTheBrotherhood, me);
                }
            }
        }

    private:
        ObjectGuid _playerGuid;
    };

    /*######
    ## npc_hope_act2
    ## ID 42749
    ######*/

    struct npc_hope_act2 : public ScriptedAI
    {
        npc_hope_act2(Creature* c) : ScriptedAI(c) { }

        void Reset() override
        {
            ScriptedAI::Reset();

            me->SetWalk(true);
        }

        void SetGUID(ObjectGuid const& guid, int32 id) override
        {
            if (id == 1)
            {
                _playerGuid = guid;
                Player* player = ObjectAccessor::GetPlayer(*me, _playerGuid);
                if (player)
                {
                    _brGroup.gryan = player->FindNearestCreature(Creatures::SpawnedGryanStoutmantleAtTower, 30.f);
                    _brGroup.ripsnarl = player->FindNearestCreature(Creatures::SpawnedRipsnarlAtTower, 30.f);
                    _brGroup.horatio = player->FindNearestCreature(Creatures::SpawnedHoratioLaineAtTower, 30.f);

                    _events.ScheduleEvent(Events::BRHopeWalkToGryan, 2s);
                }
            }
        }

        void MovementInform(uint32 type, uint32 pointId) override
        {
            if (type != POINT_MOTION_TYPE)
                return;

            if (pointId == 1)
            {
                Player* player = ObjectAccessor::GetPlayer(*me, _playerGuid);
                if (!player)
                    return;

                Talk(0, player); // You bastards will burn

                _events.ScheduleEvent(Events::BRHopeSetData1Gryan, 5s); // Hope! Wha...
                _events.ScheduleEvent(Events::BRHopeMonologue, 9s);
            }

            if (pointId == 2)
            {
                Player* player = ObjectAccessor::GetPlayer(*me, _playerGuid);
                if (!player)
                    return;

                if (_brGroup.ripsnarl)
                    DoCast(_brGroup.ripsnarl, Spells::HitMe);

                me->m_Events.AddEventAtOffset([this, player]()
                    {
                        Talk(5, player); // Admiral hat
                    },
                    std::chrono::seconds(5)
                );

                me->m_Events.AddEventAtOffset([this, player]()
                    {
                        me->SetFacingToObject(player, true);
                        Talk(6, player); // Vanessa to player
                    },
                    std::chrono::seconds(20)
                );

                me->m_Events.AddEventAtOffset([this]()
                    {
                        me->GetMotionMaster()->MovePoint(3, Positions::BRVanessaWalkingAway);
                    },
                    std::chrono::seconds(26)
                );

                if (_brGroup.horatio)
                {
                    Creature* horatio = _brGroup.horatio;
                    horatio->m_Events.AddEventAtOffset([horatio, player]()
                        {
                            horatio->AI()->Talk(0, player);
                        },
                        std::chrono::seconds(28)
                    );

                    horatio->m_Events.AddEventAtOffset([horatio, player]()
                        {
                            horatio->AI()->Talk(1, player);
                        },
                        std::chrono::seconds(31)
                    );
                }
            }

            if (pointId == 3)
            {
                if(_brGroup.horatio)
                    me->SetFacingToObject(_brGroup.horatio, true);

                Player* player = ObjectAccessor::GetPlayer(*me, _playerGuid);
                if (player)
                {
                    Talk(7, player);

                    me->m_Events.AddEventAtOffset([this, player]()
                        {
                            Talk(8, player);
                        },
                        std::chrono::seconds(8)
                    );

                    me->m_Events.AddEventAtOffset([this, player]()
                        {
                            Talk(9, player);
                        },
                        std::chrono::seconds(13)
                    );
                }

                _events.ScheduleEvent(Events::BRHopeBurnCity, 15s);
                _events.ScheduleEvent(Events::BRHopeDeparture, 18s);
                _events.ScheduleEvent(Events::BRHopeSetData2Gryan, 23s);
            }
        }

        void SpellHit(WorldObject* /*caster*/, SpellInfo const* spellInfo) override
        {
            if (spellInfo->Id == Spells::TransformVaneesa)
            {
                Player* player = ObjectAccessor::GetPlayer(*me, _playerGuid);
                if (player)
                {
                    Talk(3, player); // RISE UP BROTHERHOOD

                    _events.ScheduleEvent(Events::BRHopeSummonBrotherhood, 3s);
                    _events.ScheduleEvent(Events::BRHopeTieThemUp, 5s);
                }
            }
        }

        void UpdateAI(uint32 diff) override
        {
            _events.Update(diff);

            while (uint32 eventId = _events.ExecuteEvent())
            {
                switch (eventId)
                {
                case Events::BRHopeWalkToGryan:
                {
                    me->GetMotionMaster()->MovePoint(1, Positions::BRHopeStart);

                    if (_brGroup.gryan)
                    {
                        _brGroup.gryan->m_Events.AddEventAtOffset([this]()
                            {
                                _brGroup.gryan->SetFacingToObject(me, true);
                            },
                            std::chrono::seconds(2)
                        );
                    }
                    break;
                }

                case Events::BRHopeSetData1Gryan:
                {
                    if (_brGroup.gryan)
                        _brGroup.gryan->AI()->SetData(1, 1); // Gryan will say: Wha....
                    break;
                }

                case Events::BRHopeMonologue:
                {
                    Player* player = ObjectAccessor::GetPlayer(*me, _playerGuid);
                    if (player)
                    {
                        Talk(1, player); // I saw father...

                        me->m_Events.AddEventAtOffset([this, player]()
                            {
                                Talk(2, player); // Hope is a cruel joke
                            },
                            std::chrono::seconds(5)
                        );

                        me->m_Events.AddEventAtOffset([this]()
                            {
                                me->CastSpell(me, Spells::TransformVaneesa, true);
                            },
                            std::chrono::seconds(10)
                        );
                    }
                    break;
                }

                case Events::BRHopeSummonBrotherhood:
                {
                    Player* player = ObjectAccessor::GetPlayer(*me, _playerGuid);
                    if (player)
                    {
                        SummonBrotherhood(player);
                        player->CastSpell(player, Spells::SummonGlubtok, true);
                        player->CastSpell(player, Spells::SummonHelix, true);

                        _events.ScheduleEvent(Events::BRStoreHelixGlubtok, 1s);
                    }
                    break;
                }

                case Events::BRStoreHelixGlubtok:
                {
                    Player* player = ObjectAccessor::GetPlayer(*me, _playerGuid);
                    if (player)
                    {
                        if (Creature* helix = player->FindNearestCreature(Creatures::SpawnedHelixAtTower, 50.f))
                        {
                            if (_brGroup.ripsnarl)
                            {
                                helix->SetWalk(true);
                                helix->GetMotionMaster()->MoveCloserAndStop(1, _brGroup.ripsnarl, 7.f);
                            }

                            _brGroup.helix = helix;
                        }

                        if (Creature* glubtok = player->FindNearestCreature(Creatures::SpawnedGlubtokAtTower, 30.f))
                        {
                            glubtok->SetWalk(true);
                            _brGroup.glubtok = glubtok;
                        }
                    }

                    break;
                }

                case Events::BRHopeTieThemUp:
                {
                    Player* player = ObjectAccessor::GetPlayer(*me, _playerGuid);
                    if (player)
                    {
                        Talk(4, player); // Tie them up

                        me->m_Events.AddEventAtOffset([this]()
                            {
                                DoCast(Spells::TiedUpGoodGuysForceCast);
                            },
                            std::chrono::seconds(1)
                        );

                        me->m_Events.AddEventAtOffset([this]()
                            {
                                me->GetMotionMaster()->MovePoint(2, Positions::BRVanessaToAdmiral);
                            },
                            std::chrono::seconds(3)
                        );
                    }
                    break;
                }

                case Events::BRHopeBurnCity:
                {
                    ForEachBrotherhood([](Creature* c)
                        {
                            c->AI()->SetData(1, 1);
                        });
                    break;
                }

                case Events::BRHopeSetData2Gryan:
                {
                    if(_brGroup.gryan)
                        _brGroup.gryan->AI()->SetData(1, 2);

                    break;
                }

                case Events::BRHopeDeparture:
                {
                    me->GetMotionMaster()->MovePoint(4, Positions::BRVanessaDeparture);

                    if(_brGroup.helix)
                        _brGroup.helix->GetMotionMaster()->MovePoint(5, Positions::BRHelixDeparture);

                    if (_brGroup.glubtok)
                        _brGroup.glubtok->GetMotionMaster()->MovePoint(6, Positions::BRGlubtokDeparture);

                    break;
                }
                }
            }
        }

    private:
        EventMap _events;
        ObjectGuid _playerGuid;
        std::vector<ObjectGuid> _brotherhoodGuids;
        BRGroup _brGroup = {};

        void SummonBrotherhood(Player* player)
        {
            if (!player)
                return;

            for (auto const& spawn : Brotherhood)
            {
                if (Creature* c = player->SummonCreature(
                    spawn.entry,
                    spawn.pos,
                    TEMPSUMMON_TIMED_DESPAWN_OUT_OF_COMBAT,
                    std::chrono::seconds(spawn.despawnTime)))
                {
                    c->SetReactState(REACT_PASSIVE);
                    _brotherhoodGuids.push_back(c->GetGUID());
                }
            }
        }

        template <typename Func>
        void ForEachBrotherhood(Func&& fn)
        {
            for (auto const& guid : _brotherhoodGuids)
                if (Creature* c = ObjectAccessor::GetCreature(*me, guid))
                    fn(c);
        }
    };

    /*######
    ## npc_defias_blackguard
    ## ID 42769
    ######*/

    struct npc_defias_blackguard : public ScriptedAI
    {
        npc_defias_blackguard(Creature* creature) : ScriptedAI(creature) {}

        void Reset() override
        {
            Defias_DoFire = false;
            ThrowTimer = 4000;

            DoCast(Spells::Smoke);
            DoCast(Spells::Sneak);
        }

        // Triggered externally via SetData(1,1)
        void SetData(uint32 type, uint32 data) override
        {
            if (type == 1 && data == 1)
                Defias_DoFire = true;
        }

        void UpdateAI(uint32 diff) override
        {
            if (!Defias_DoFire)
                return;

            if (Creature* pTarget = me->FindNearestCreature(Creatures::SentinelHillFireTrigger, 30.0f, true))
            {
                if (ThrowTimer <= diff)
                {
                    DoCast(pTarget, Spells::ThrowTorch, true);
                    ThrowTimer = 4000;

                    me->SetWalk(true);
                    me->GetMotionMaster()->MovePoint(
                        0,
                        me->GetPositionX() - 5,
                        me->GetPositionY() + 5,
                        me->GetPositionZ());
                }
                else
                    ThrowTimer -= diff;
            }
        }

    private:
        uint32 ThrowTimer = 0;
        bool Defias_DoFire = false;
    };

    /*######
    ## npc_sentinel_hill_fire_trigger
    ## ID 42793
    ######*/

    struct npc_sentinel_hill_fire_trigger : public ScriptedAI
    {
        npc_sentinel_hill_fire_trigger(Creature* creature) : ScriptedAI(creature) {}

        void Reset() override
        {
            _spawnFire = false;
        }

        void SpellHit(WorldObject* /*caster*/, SpellInfo const* spellInfo) override
        {
            if (_spawnFire)
                return;

            if (spellInfo->Id == Spells::ThrowTorch)
            {
                if (GameObject* fire = me->SummonGameObject(Gameobjects::Fire, me->GetPosition(), QuaternionData(), 0s))
                {
                    _spawnFire = true;
                    fire->DespawnOrUnsummon(6min);
                    me->DespawnOrUnsummon(6min, 1s);
                }
            }
        }

    private:
        bool _spawnFire = false;
    };
}

void AddSC_custom_westfall_npcs()
{
    using namespace Scripts::EasternKingdoms::Westfall;

    RegisterCreatureAI(npc_custom_westfall_stew_42617);
    RegisterCreatureAI(npc_custom_westfall_guard_42407);
    RegisterCreatureAI(npc_custom_lous_parting_thoughts_trigger);
    RegisterCreatureAI(npc_custom_lous_parting_thoughts_thug);
    RegisterCreatureAI(npc_custom_salma_saldean_235);
    RegisterCreatureAI(npc_custom_gryan_stoutmantle_234);
    RegisterCreatureAI(npc_custom_ripsnarl);
    RegisterCreatureAI(npc_custom_agent_kearnen);
    RegisterCreatureAI(npc_custom_elite_mercenary);
    RegisterCreatureAI(npc_custom_trigger_mortwake_tower);
    RegisterCreatureAI(npc_custom_moonbrook_player_trigger);
    RegisterCreatureAI(npc_vision_of_the_past);
    RegisterCreatureAI(npc_vision_defias_pirate);
    RegisterCreatureAI(npc_gryan_stoutmantle_act2);
    RegisterCreatureAI(npc_hope_act2);
    RegisterCreatureAI(npc_defias_blackguard);
    RegisterCreatureAI(npc_sentinel_hill_fire_trigger);
}