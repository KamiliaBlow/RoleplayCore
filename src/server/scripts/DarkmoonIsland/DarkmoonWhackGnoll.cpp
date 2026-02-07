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

#include "ScriptMgr.h"
#include "DarkmoonIsland.h"
#include "ScriptedGossip.h"
#include "Cell.h"
#include "CellImpl.h"
#include "GridNotifiers.h"
#include "SpellScript.h"
#include "GameObject.h"
#include "Transport.h"
#include "AchievementMgr.h"
#include "ScriptedCreature.h"
#include "GameObjectAI.h"
#include "InstanceScript.h"
#include "Log.h"
#include "ObjectAccessor.h"
#include "Player.h"
#include "ScriptedEscortAI.h"
#include "WorldSession.h"
#include "Item.h"
#include "SpellInfo.h"
#include "SpellMgr.h"
#include "GameEventMgr.h"
#include "AchievementPackets.h"
#include "DB2HotfixGenerator.h"
#include "DB2Stores.h"
#include "ChatTextBuilder.h"
#include "Containers.h"
#include "DatabaseEnv.h"
#include "GameTime.h"
#include "GridNotifiersImpl.h"
#include "Group.h"
#include "Guild.h"
#include "GuildMgr.h"
#include "Language.h"
#include "Mail.h"
#include "ObjectMgr.h"
#include "RBAC.h"
#include "StringConvert.h"
#include "World.h"
#include "AreaTrigger.h"
#include "AreaTriggerAI.h"
#include "CreatureAI.h"
#include "CreatureAIImpl.h"
#include "TemporarySummon.h"
#include <sstream>


enum whackCreatures
{
    NPC_BARREL              = 54546,
    NPC_GNOLL               = 54444,
    NPC_DOLL                = 54466,
    NPC_BOSS                = 54549,
};

enum whackSpells
{
    SPELL_OVERRIDE_ACTION   = 101612,
    SPELL_ENABLE_POWERBAR   = 110230,
    SPELL_ACTIONBAR_REMOVE  = 102137,
    SPELL_WHACK             = 101604,
    SPELL_KILL_CREDIT       = 101835,
    SPELL_DOLL_STUN         = 101679,

    SPELL_SPAWN_VISUAL      = 102136,
    SPELL_OK_TO_HIT         = 101996,

    SPELL_SUMMON_GNOLL      = 102036,
    SPELL_SUMMON_DOLL       = 102043,
    SPELL_SUMMON_BOSS       = 102044,

    SPELL_FORBIDDEN_ZONE    = 109977,
};

enum whackEvents
{
    EVENT_SUMMON_GNOLL      = 1,
    EVENT_SUMMON_DOLL       = 2,
    EVENT_SUMMON_BOSS       = 3,

    EVENT_CHECK_PLAYER_POS  = 4
};

class npc_whack_gnoll_bunny : public CreatureScript
{
    public:
        npc_whack_gnoll_bunny() : CreatureScript("npc_whack_gnoll_bunny") { }

        CreatureAI* GetAI(Creature* pCreature) const override
        {
            return new npc_whack_gnoll_bunnyAI(pCreature);
        }

        struct npc_whack_gnoll_bunnyAI : ScriptedAI
        {
            npc_whack_gnoll_bunnyAI(Creature* pCreature) : ScriptedAI(pCreature) { }

            std::list<ObjectGuid> barrelList;
            EventMap events;

            void Reset() override
            {
                me->SetReactState(REACT_PASSIVE);
                GetBarrels();

                events.Reset();
                events.ScheduleEvent(EVENT_SUMMON_GNOLL,        3s);
                events.ScheduleEvent(EVENT_SUMMON_DOLL,         6s);
                events.ScheduleEvent(EVENT_SUMMON_BOSS,         10s);
                events.ScheduleEvent(EVENT_CHECK_PLAYER_POS,    1s);
            }

            void GetBarrels()
            {
                barrelList.clear();
                std::list<Creature*> templist;
                me->GetCreatureListWithEntryInGrid(templist, NPC_BARREL, 50.0f);

                for (auto barrel : templist)
                    barrelList.push_back(barrel->GetGUID());
            }

            // We need to find a barrel that do not already have a summon
            Creature* GetRandomBarrel()
            {
                std::list<ObjectGuid> tempList = barrelList;

                while (tempList.size())
                {
                    auto it = tempList.begin();
                    std::advance(it, urand(0, tempList.size() - 1));

                    if (Creature* barrel = ObjectAccessor::GetCreature(*me, *it))
                    {
                        if (barrel->HasAura(SPELL_OK_TO_HIT))
                            tempList.erase(it);
                        else
                            return barrel;
                    }
                }

                return nullptr;
            }

            void UpdateAI(uint32 diff) override
            {
                events.Update(diff);

                if (barrelList.size() != 9)
                    GetBarrels();

                while (uint32 eventId = events.ExecuteEvent())
                {
                    switch (eventId)
                    {
                        case EVENT_SUMMON_GNOLL:
                        {
                            if (Creature* barrel = GetRandomBarrel())
                            {
                                Creature* gnoll = barrel->SummonCreature(NPC_GNOLL, barrel->GetPosition(), TEMPSUMMON_TIMED_DESPAWN, 3s);
                                barrel->CastSpell(barrel, SPELL_SPAWN_VISUAL, false);
                                barrel->AddAura(SPELL_OK_TO_HIT, gnoll);
                            }

                            events.ScheduleEvent(EVENT_SUMMON_GNOLL, 3s);
                            break;
                        }
                        case EVENT_SUMMON_DOLL:
                        {
                            if (Creature* barrel = GetRandomBarrel())
                            {
                                barrel->SummonCreature(NPC_DOLL, barrel->GetPosition(), TEMPSUMMON_TIMED_DESPAWN, 6s);
                                barrel->CastSpell(barrel, SPELL_SPAWN_VISUAL, false);
                            }

                            events.ScheduleEvent(EVENT_SUMMON_DOLL, 6s);
                            break;
                        }
                        case EVENT_SUMMON_BOSS:
                        {
                            if (Creature* barrel = GetRandomBarrel())
                            {
                                Creature* boss = barrel->SummonCreature(NPC_BOSS, barrel->GetPosition(), TEMPSUMMON_TIMED_DESPAWN, 10s);
                                barrel->CastSpell(barrel, SPELL_SPAWN_VISUAL, false);
                                barrel->AddAura(SPELL_OK_TO_HIT, boss);
                            }

                            events.ScheduleEvent(EVENT_SUMMON_BOSS, 10s);
                            break;
                        }
                        case EVENT_CHECK_PLAYER_POS:
                        {
                            std::list<Player*> playerList;
                            me->GetPlayerListInGrid(playerList, 30.0f);

                            for (auto player: playerList)
                            {
                                if (player->HasAura(SPELL_ENABLE_POWERBAR))
                                {
                                    if (me->GetDistance(player) >= 15.0f)
                                        player->NearTeleportTo(-3994.28f, 6283.58f, 13.12f, 0.727784f, true);
                                }
                                else
                                {
                                    if (me->GetDistance(player) < 15.0f)
                                        player->CastSpell(player, SPELL_FORBIDDEN_ZONE, true);
                                }
                            }

                            events.ScheduleEvent(EVENT_CHECK_PLAYER_POS, 1s);
                            break;
                        }
                    }
                }
            }
        };
};

class npc_whack_gnoll_mola : public CreatureScript
{
public:
    npc_whack_gnoll_mola() : CreatureScript("npc_whack_gnoll_mola") { }

    struct npc_whack_gnoll_molaAI : public ScriptedAI
    {
        npc_whack_gnoll_molaAI(Creature* creature) : ScriptedAI(creature) { }

        bool OnGossipHello(Player* player) override
        {
            if (me->IsQuestGiver())
                player->PrepareQuestMenu(me->GetGUID());

            AddGossipItemFor(player, 13018, 0, GOSSIP_SENDER_MAIN, GOSSIP_ACTION_INFO_DEF + 1);
            AddGossipItemFor(player, 13018, 1, GOSSIP_SENDER_MAIN, GOSSIP_ACTION_INFO_DEF + 2);

            player->PlayerTalkClass->SendGossipMenu(player->GetGossipTextId(me), me->GetGUID());
            return true;
        }

        bool OnGossipSelect(Player* player, uint32 /*menuId*/, uint32 gossipListId) override
        {
            uint32 const action = player->PlayerTalkClass->GetGossipOptionAction(gossipListId);
            ClearGossipMenuFor(player);

            switch (action)
            {
                // Info
            case GOSSIP_ACTION_INFO_DEF + 1:
                player->PlayerTalkClass->ClearMenus();
                AddGossipItemFor(player, 16972, 0, GOSSIP_SENDER_MAIN, GOSSIP_ACTION_INFO_DEF + 3);
                    SendGossipMenuFor(player, 18350, me->GetGUID());
                break;
                // Ready to play
            case  GOSSIP_ACTION_INFO_DEF + 2:
                if (player->HasItemCount(ITEM_DARKMOON_TOKEN, 1))
                {
                    CloseGossipMenuFor(player);

                    player->DestroyItemCount(ITEM_DARKMOON_TOKEN, 1, true);
                    player->NearTeleportTo(-3994.28f, 6283.58f, 13.12f, 0.727784f, true);

                    me->AddAura(SPELL_OVERRIDE_ACTION, player);
                    me->AddAura(SPELL_ENABLE_POWERBAR, player);
                    player->SetPower(POWER_ALTERNATE_POWER, player->GetReqKillOrCastCurrentCount(QUEST_WHACK_A_GNOLL, 54505));
                }
                else
                {
                    player->PlayerTalkClass->ClearMenus();
                    return OnGossipHello(player);
                }
                break;
                // I understand
            case GOSSIP_ACTION_INFO_DEF + 3:
                player->PlayerTalkClass->ClearMenus();
                return OnGossipHello(player);
                break;
            }

            return false;
        }
    };

    CreatureAI* GetAI(Creature* creature) const override
    {
        return new npc_whack_gnoll_molaAI(creature);
    }
};

class npc_whack_gnoll_barrel : public CreatureScript
{
    public:
        npc_whack_gnoll_barrel() : CreatureScript("npc_whack_gnoll_barrel") { }

        CreatureAI* GetAI(Creature* Creature) const override
        {
            return new npc_whack_gnoll_barrelAI(Creature);
        }

        struct npc_whack_gnoll_barrelAI : ScriptedAI
        {
            npc_whack_gnoll_barrelAI(Creature* Creature) : ScriptedAI(Creature) {}

            void Reset() override
            {
                me->SetReactState(REACT_PASSIVE);
            }
        };
};

class PositionCheck
{
    public:
        PositionCheck(Unit* caster) : _caster(caster) {}

        bool operator()(WorldObject* unit)
        {
            bool isTooFar = _caster->GetDistance2d(unit) >= 3.0f;
            return isTooFar;
        }

        private:
            Unit* _caster;
};

// SPELL_WHACK : 101604
class spell_whack_gnoll_whack : public SpellScriptLoader
{
    public:
        spell_whack_gnoll_whack() : SpellScriptLoader("spell_whack_gnoll_whack") { }

        class spell_whack_gnoll_whack_SpellScript : public SpellScript
        {

            SpellCastResult CheckCast()
            {
                Unit* caster = GetCaster();
                if (!caster)
                    return SPELL_CAST_OK;

                Player* player = caster->ToPlayer();


                // Todo : On peut rendre le code plus propre en changant la TargetA du spell par 46 et ajouter des conditions, mais perso j'ai la flemme
                std::list<Creature*> targetList;
                caster->GetCreatureListWithEntryInGrid(targetList, NPC_GNOLL, 2.0f);
                caster->GetCreatureListWithEntryInGridAppend(targetList, NPC_DOLL, 2.0f);
                caster->GetCreatureListWithEntryInGridAppend(targetList, NPC_BOSS, 2.0f);

                targetList.remove_if(PositionCheck(GetCaster()));

                if (targetList.empty())
                    return SPELL_CAST_OK;

                Creature* target = targetList.front();

                if (!caster->isInFront(target, static_cast<float>(M_PI / 2)))
                    return SPELL_FAILED_UNIT_NOT_INFRONT;

                int8 score{};

                switch (target->GetEntry())
                {
                    case NPC_GNOLL:
                    {
                        caster->CastSpell(caster, SPELL_KILL_CREDIT, true);
                        score = caster->GetPower(POWER_ALTERNATE_POWER);
                        break;
                    }
                    case NPC_DOLL:
                    {
                        caster->CastSpell(caster, SPELL_DOLL_STUN, true);
                        score = caster->GetPower(POWER_ALTERNATE_POWER);
                        break;
                    }
                    case NPC_BOSS:
                    {
                        for (uint8 i = 0; i < 3; ++i)
                            caster->CastSpell(caster, SPELL_KILL_CREDIT, true);
                            score = caster->GetPower(POWER_ALTERNATE_POWER);

                        break;
                    }
                }

                if (score >= 45)
                    if (AchievementEntry const* achiev = sAchievementStore.LookupEntry(9983))
                        player->CompletedAchievement(achiev);

                caster->Kill(caster, target);
                return SPELL_CAST_OK;
            }

            void Register() override
            {
                OnCheckCast += SpellCheckCastFn(spell_whack_gnoll_whack_SpellScript::CheckCast);
            }

        };

        SpellScript* GetSpellScript() const override
        {
            return new spell_whack_gnoll_whack_SpellScript();
        }
};

// This is the spell the player can cancel (SPELL_OVERRIDE_ACTION : 101612)
class spell_whack_gnoll_override_action : public SpellScriptLoader
{
public:
    spell_whack_gnoll_override_action() : SpellScriptLoader("spell_whack_gnoll_override_action") { }

    class spell_whack_gnoll_override_action_AuraScript : public AuraScript
    {

        bool Validate(SpellInfo const* /*entry*/) override
        {
            return true;
        }

        void HandleEffectRemove(AuraEffect const* /*aurEff*/, AuraEffectHandleModes /*mode*/)
        {
            Unit* target = GetTarget();

            if (!target)
                return;

            target->SetPower(POWER_ALTERNATE_POWER, 0);
            target->RemoveAurasDueToSpell(SPELL_ENABLE_POWERBAR);
            target->RemoveAurasDueToSpell(SPELL_DOLL_STUN);
            target->CastSpell(target, SPELL_FORBIDDEN_ZONE, true);
        }

        void Register() override
        {
            AfterEffectRemove += AuraEffectRemoveFn(spell_whack_gnoll_override_action_AuraScript::HandleEffectRemove, EFFECT_0, SPELL_AURA_OVERRIDE_SPELLS, AURA_EFFECT_HANDLE_REAL);
        }
    };

    AuraScript* GetAuraScript() const override
    {
        return new spell_whack_gnoll_override_action_AuraScript();
    }
};

/*######
## at_whack_a_gnoll_entrance (7344)
######*/

class AreaTrigger_at_whack_a_gnoll_entrance : public AreaTriggerScript
{
public:
    AreaTrigger_at_whack_a_gnoll_entrance() : AreaTriggerScript("at_whack_a_gnoll_entrance") {}

    bool OnTrigger(Player* player, AreaTriggerEntry const* /*trigger*/) override
    {
        if (!player->HasAura(SPELL_ENABLE_POWERBAR) && !player->IsGameMaster())
            player->CastSpell(player, SPELL_FORBIDDEN_ZONE, true);

        return false;
    }
};

void AddSC_darkmoon_whack_gnoll()
{
    new npc_whack_gnoll_bunny();
    new npc_whack_gnoll_mola();
    new npc_whack_gnoll_barrel();
    new spell_whack_gnoll_whack();
    new spell_whack_gnoll_override_action();
    new AreaTrigger_at_whack_a_gnoll_entrance();
};
