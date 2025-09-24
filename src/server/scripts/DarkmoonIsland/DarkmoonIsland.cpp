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
#include "Cell.h"
#include "CellImpl.h"
#include "GridNotifiers.h"
#include "GridNotifiersImpl.h"
#include "SpellScript.h"
#include "SpellAuras.h"
#include "ScriptedGossip.h"
#include "MotionMaster.h"
#include "GameObject.h"
#include "Transport.h"
#include "DarkmoonIsland.h"
#include "AchievementMgr.h"
#include "ScriptedCreature.h"
#include "Player.h"
#include "GameObject.h"
#include "GameObjectAI.h"
#include "InstanceScript.h"
#include "Log.h"
#include "ObjectAccessor.h"
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
#include "SharedDefines.h"
#include "TemporarySummon.h"
#include <sstream>

const Position mfPos[22] =
{
    // Snarler spawn
    { -3924.37f, 6303.53f, 17.59f, 1.88f },
    { -4011.98f, 6416.34f, 14.75f, 3.73f },
    { -4097.92f, 6458.10f, 14.80f, 3.19f },
    { -4170.17f, 6503.14f, 13.41f, 1.55f },
    { -4266.80f, 6521.71f, 14.39f, 2.68f },
    { -4318.40f, 6601.40f, 9.853f, 1.47f },
    { -4056.26f, 6664.42f, 13.22f, 2.99f },
    { -4009.05f, 6561.04f, 17.15f, 4.37f },
    { -3932.42f, 6584.56f, 12.91f, 3.70f },
    { -3838.57f, 6461.64f, 11.91f, 3.92f },
    { -4268.83f, 6678.51f, 9.731f, 4.84f },
    // Dreadhowl spawn
    { -4225.82f, 6556.37f, 14.61f, 5.84f },
    { -4141.07f, 6523.72f, 16.81f, 6.06f },
    { -4073.94f, 6580.90f, 16.70f, 0.27f },
    { -3957.37f, 6617.45f, 12.66f, 0.43f },
    { -3865.21f, 6524.91f, 18.89f, 2.94f },
    { -3872.48f, 6498.26f, 17.90f, 3.39f },
    { -3914.52f, 6398.61f, 13.61f, 4.04f },
    { -4038.38f, 6514.68f, 13.36f, 3.01f },
    { -4344.90f, 6583.72f, 10.64f, 1.75f },
    { -4193.76f, 6122.50f, 13.00f, 6.06f },
    { -4082.68f, 6121.38f, 17.41f, 5.37f }
};

// npc - 55089 55093 55397 55398 
struct npc_fire_juggler_darkmoon : public ScriptedAI
{
    npc_fire_juggler_darkmoon(Creature* creature) : ScriptedAI(creature) { }

    EventMap events;

    void Reset() override
    {
        events.Reset();
        events.ScheduleEvent(101, 1s);
    }

    void UpdateAI(uint32 diff) override
    {
        events.Update(diff);

        while (uint32 eventId = events.ExecuteEvent())
        {
            switch (eventId)
            {
            case 101:
            {
                if (!me->HasAura(SPELL_JUGGLE_TORCH_AURA))
                    me->AddAura(SPELL_JUGGLE_TORCH_AURA, me);
                events.ScheduleEvent(101, 3s);
                break;
            }
            }
        }

        UpdateVictim();
    }
};

// npc - 55229 55230 55231
struct npc_fire_eater_darkmoon : public ScriptedAI
{
    explicit npc_fire_eater_darkmoon(Creature* creature) : ScriptedAI(creature) {}

    EventMap events;

    void Reset() override
    {
        events.Reset();
        events.ScheduleEvent(101,
            Seconds(urand(5 * MINUTE, 7 * MINUTE)));
    }

    void UpdateAI(uint32 diff) override
    {
        events.Update(diff);

        while (uint32 eventId = events.ExecuteEvent())
        {
            switch (eventId)
            {
            case 101:
            {
                me->CastSpell(me, SPELL_FIRE_BREATHING_SPELL, true);

                events.ScheduleEvent(101,
                    Seconds(urand(5 * MINUTE, 7 * MINUTE)));
                break;
            }
            }
        }
    }
};

// 71992 - Moonfang <Darkmoon Den Mother>
struct boss_darkmoon_moonfang_mother : public ScriptedAI
{
    boss_darkmoon_moonfang_mother(Creature* creature) : ScriptedAI(creature), summons(me)
    {
        me->SetVisible(false);
        prevEvent1 = true;
        prevEvent2 = false;
        sDiedCount = 0;
    }

    enum eSay
    {
        SAY_SUMM_SNARLER = 0,
        SAY_SUMM_DREADHOWL = 1,
        SAY_SUMM_MOTHER = 2
    };

    enum eCreatures
    {
        NPC_MOONFANG_SNARLER = 56160,
        NPC_MOONFANG_DREADHOWL = 71982
    };

    enum eSpells
    {
        SPELL_LEAP_FOR_THE_KILL = 144546,
        SPELL_FANGS_OF_THE_MOON = 144700,
        SPELL_MOONFANG_TEARS = 144702,
        SPELL_CALL_THE_PACK = 144602,
        SPELL_MOONFANG_CURSE = 144590
    };

    EventMap events;
    SummonList summons;

    bool prevEvent1;
    bool prevEvent2;
    uint8 sDiedCount;

    void Reset() override
    {
        events.Reset();
        summons.DespawnAll();

        if (prevEvent1)
        {
            SummonMoonfang();
            ZoneTalk(SAY_SUMM_SNARLER, nullptr);
        }
    }

    void SummonMoonfang()
    {
        if (prevEvent1)
            for (uint8 i = 0; i < 11; i++)
                me->SummonCreature(NPC_MOONFANG_SNARLER, mfPos[i], TEMPSUMMON_CORPSE_TIMED_DESPAWN, 10000ms);

        if (prevEvent2)
            for (uint8 i = 11; i < 22; i++)
                me->SummonCreature(NPC_MOONFANG_DREADHOWL, mfPos[i], TEMPSUMMON_CORPSE_TIMED_DESPAWN, 10000ms);
    }

    void SummonedCreatureDies(Creature* /*summon*/, Unit* /*killer*/) override
    {
        if (prevEvent1 || prevEvent2)
            sDiedCount++;

        if (sDiedCount == 11)
        {
            prevEvent1 = false;
            prevEvent2 = true;
            ZoneTalk(SAY_SUMM_DREADHOWL, nullptr);
            SummonMoonfang();
        }

        if (sDiedCount == 22)
        {
            prevEvent2 = false;
            me->SetVisible(true);
            ZoneTalk(SAY_SUMM_MOTHER, nullptr);
        }
    }

    void EnterEvadeMode(EvadeReason /*why*/) override
    {
        events.ScheduleEvent(1, 0ms);       // cast leap
        events.ScheduleEvent(2, 10000ms);   // cast stuns the target
        events.ScheduleEvent(3, 8000ms);    // cast tears AOE
        events.ScheduleEvent(4, 64000ms);   // summon moonfangs
        events.ScheduleEvent(5, 180000ms);  // cast mind control
    }

    void JustSummoned(Creature* summon) override
    {
        summons.Summon(summon);
    }

    void UpdateAI(uint32 diff) override
    {
        if (!UpdateVictim() && me->IsInCombat())
            return;

        events.Update(diff);

        if (me->HasUnitState(UNIT_STATE_CASTING))
            return;

        if (uint32 eventId = events.ExecuteEvent())
        {
            switch (eventId)
            {
            case 1:
                if (auto target = SelectTarget(SelectTargetMethod::Random, 1, 40.0f, true))
                    DoCast(target, SPELL_LEAP_FOR_THE_KILL, false);

                events.ScheduleEvent(1, 12000ms);
                break;
            case 2:
                if (auto target = me->GetVictim())
                    DoCast(target, SPELL_FANGS_OF_THE_MOON, false);

                events.ScheduleEvent(2, 10000ms);
                break;
            case 3:
                DoCast(SPELL_MOONFANG_TEARS);
                events.ScheduleEvent(3, 22000ms);
                break;
            case 4:
                DoCast(SPELL_CALL_THE_PACK);
                events.ScheduleEvent(4, 64000ms);
                break;
            case 5:
                DoCast(SPELL_MOONFANG_CURSE);
                events.ScheduleEvent(5, 180000ms);
                break;
            default:
                break;
            }
        }
    }
};

// To the Staging Area! - 101260
class spell_darkmoon_staging_area_teleport : public SpellScript
{

    bool Load() override
    {
        return GetCaster() != nullptr;
    }

    void RelocateDest(SpellEffIndex /*effIndex*/)
    {
        if (Player* caster = GetCaster()->ToPlayer())
        {
            switch (caster->GetMapId())
            {
                case 974: //Darkmoon Island
                    if (caster->GetTeamId() == TEAM_HORDE)
                        GetHitDest()->WorldRelocate(WorldLocation(1, -1454.415894f, 207.967484f, -7.790083f, 0.689538f));
                    else
                        GetHitDest()->WorldRelocate(WorldLocation(0, -9517.5f, 82.3f, 59.51f, 2.92168f));
                    break;
                default:
                    GetHitDest()->WorldRelocate(WorldLocation(974, -3618.669922f, 6315.669922f, 113.190002f, 3.204420f));
                    break;
            }
        }
    }

    void Register() override
    {
        OnEffectLaunch += SpellEffectFn(spell_darkmoon_staging_area_teleport::RelocateDest, EFFECT_0, SPELL_EFFECT_TELEPORT_UNITS);
    }
};

class item_darkmoon_faire_fireworks : public ItemScript
{
public:
    item_darkmoon_faire_fireworks() : ItemScript("item_darkmoon_faire_fireworks") { }

    bool OnUse(Player* player, Item* /*item*/, SpellCastTargets const& /*targets*/, ObjectGuid /*castId*/) override
    {
        AchievementEntry const* AchievFireworksAlliance = sAchievementStore.LookupEntry(6030);
        AchievementEntry const* AchievFireworksHorde = sAchievementStore.LookupEntry(6031);

        if (player->GetTeam() == ALLIANCE)
        {
            player->CompletedAchievement(AchievFireworksAlliance);
            player->CastSpell(player, 103740, false);
        }
        else
        {
            player->CompletedAchievement(AchievFireworksHorde);
            player->CastSpell(player, 103740, false);
        }

        return true;
    }
};

// spells
class spell_gen_repair_damaged_tonk : public SpellScript
{

    SpellCastResult CheckRequirement()
    {
        if (GetCaster()->GetTypeId() != TYPEID_PLAYER)
            return SPELL_FAILED_CANT_DO_THAT_RIGHT_NOW;

        return SPELL_CAST_OK;
    }

    void HandleDummy(SpellEffIndex /*effIndex*/)
    {
        Player* player = GetCaster()->ToPlayer();
        Creature* target = GetHitCreature();

        if (!target)
            return;

        player->KilledMonsterCredit(54504, ObjectGuid::Empty);

        Position myPos = target->GetPosition();
        target->SummonCreature(55356, myPos, TEMPSUMMON_TIMED_DESPAWN, 30000ms);
        target->DespawnOrUnsummon();
    }

    void Register()
    {
        OnEffectHitTarget += SpellEffectFn(spell_gen_repair_damaged_tonk::HandleDummy, EFFECT_0, SPELL_EFFECT_DUMMY);
        OnCheckCast += SpellCheckCastFn(spell_gen_repair_damaged_tonk::CheckRequirement);
    }
};

class spell_gen_shoe_baby : public SpellScript
{

    SpellCastResult CheckRequirement()
    {
        if (GetCaster()->GetTypeId() != TYPEID_PLAYER)
            return SPELL_FAILED_CANT_DO_THAT_RIGHT_NOW;

        return SPELL_CAST_OK;
    }

    void HandleDummy(SpellEffIndex /*effIndex*/)
    {
        Player* player = GetCaster()->ToPlayer();
        player->KilledMonsterCredit(54510, ObjectGuid::Empty);
    }

    void Register()
    {
        OnEffectHitTarget += SpellEffectFn(spell_gen_shoe_baby::HandleDummy, EFFECT_0, SPELL_EFFECT_DUMMY);
        OnCheckCast += SpellCheckCastFn(spell_gen_shoe_baby::CheckRequirement);
    }
};

class spell_cook_crunchy_frog : public SpellScript
{

    SpellCastResult CheckRequirement()
    {
        if (GetCaster()->GetTypeId() != TYPEID_PLAYER)
            return SPELL_FAILED_CANT_DO_THAT_RIGHT_NOW;

        return SPELL_CAST_OK;
    }

    void HandleDummy(SpellEffIndex /*effIndex*/)
    {
        Player* player = GetCaster()->ToPlayer();
        player->AddItem(72058, 1);
    }

    void Register()
    {
        OnEffectHitTarget += SpellEffectFn(spell_cook_crunchy_frog::HandleDummy, EFFECT_0, SPELL_EFFECT_DUMMY);
        OnCheckCast += SpellCheckCastFn(spell_cook_crunchy_frog::CheckRequirement);
    }
};

enum InjuriedCarnieYells
{
    SAY_GREETINGS_1 = 0,
    SAY_GREETINGS_2 = 1,
    SAY_GREETINGS_3 = 2,
    SAY_GREETINGS_4 = 3,
    SAY_GREETINGS_5 = 4,
};

class spell_heal_injuried_carnie : public SpellScript
{

    SpellCastResult CheckRequirement()
    {
        if (GetCaster()->GetTypeId() != TYPEID_PLAYER)
            return SPELL_FAILED_CANT_DO_THAT_RIGHT_NOW;

        return SPELL_CAST_OK;
    }

    void HandleHealPct(SpellEffIndex /*effIndex*/)
    {
        Player* player = GetCaster()->ToPlayer();
        Creature* target = GetHitCreature();

        player->KilledMonsterCredit(54518, ObjectGuid::Empty);

        if (!target)
            return;

        switch (urand(0, 4))
        {
        case 0:
            target->AI()->Talk(SAY_GREETINGS_1);
            target->HandleEmoteCommand(EMOTE_ONESHOT_NONE);
            target->GetMotionMaster()->MoveRandom(30);
            target->DespawnOrUnsummon(8000ms);
            break;
        case 1:
            target->AI()->Talk(SAY_GREETINGS_2);
            target->HandleEmoteCommand(EMOTE_ONESHOT_NONE);
            target->GetMotionMaster()->MoveRandom(30);
            target->DespawnOrUnsummon(8000ms);
            break;
        case 2:
            target->AI()->Talk(SAY_GREETINGS_3);
            target->HandleEmoteCommand(EMOTE_ONESHOT_NONE);
            target->GetMotionMaster()->MoveRandom(30);
            target->DespawnOrUnsummon(8000ms);
            break;
        case 3:
            target->AI()->Talk(SAY_GREETINGS_4);
            target->HandleEmoteCommand(EMOTE_ONESHOT_NONE);
            target->GetMotionMaster()->MoveRandom(30);
            target->DespawnOrUnsummon(8000ms);
            break;
        case 4:
            target->AI()->Talk(SAY_GREETINGS_5);
            target->HandleEmoteCommand(EMOTE_ONESHOT_NONE);
            target->GetMotionMaster()->MoveRandom(30);
            target->DespawnOrUnsummon(8000ms);
            break;
        }
    }

    void Register()
    {
        OnEffectHitTarget += SpellEffectFn(spell_heal_injuried_carnie::HandleHealPct, EFFECT_0, SPELL_EFFECT_HEAL_PCT);
        OnCheckCast += SpellCheckCastFn(spell_heal_injuried_carnie::CheckRequirement);
    }
};

class spell_put_up_darkmoon_banner : public SpellScript
{

    SpellCastResult CheckRequirement()
    {
        if (GetCaster()->GetTypeId() != TYPEID_PLAYER)
            return SPELL_FAILED_CANT_DO_THAT_RIGHT_NOW;

        return SPELL_CAST_OK;
    }

    void HandleDummy(SpellEffIndex /*effIndex*/)
    {
        Player* player = GetCaster()->ToPlayer();
        player->KilledMonsterCredit(54545, ObjectGuid::Empty);

        if (Creature* loosestones = GetCaster()->FindNearestCreature(54545, 10.0f, true))
        {
            loosestones->SummonGameObject(179965, Position(loosestones->GetPositionX(), loosestones->GetPositionY(), loosestones->GetPositionZ(), loosestones->GetOrientation()), QuaternionData(), 0s);
            loosestones->DespawnOrUnsummon(30000ms);
        }
    }

    void Register()
    {
        OnEffectHitTarget += SpellEffectFn(spell_put_up_darkmoon_banner::HandleDummy, EFFECT_0, SPELL_EFFECT_DUMMY);
        OnCheckCast += SpellCheckCastFn(spell_put_up_darkmoon_banner::CheckRequirement);
    }
};


class spell_darkmoon_deathmatch : public SpellScript
{

    SpellCastResult CheckRequirement()
    {
        if (GetCaster()->GetTypeId() != TYPEID_PLAYER)
            return SPELL_FAILED_CANT_DO_THAT_RIGHT_NOW;

        return SPELL_CAST_OK;
    }

    void HandleScript(SpellEffIndex /*effIndex*/)
    {
        Unit* caster = GetCaster();

        if (caster->GetPositionY() <= 6400.0f)
            caster->CastSpell(caster, 108919, false);
        else
            caster->CastSpell(caster, 108923, false);
    }

    void Register()
    {
        OnEffectHitTarget += SpellEffectFn(spell_darkmoon_deathmatch::HandleScript, EFFECT_0, SPELL_EFFECT_SCRIPT_EFFECT);
        OnCheckCast += SpellCheckCastFn(spell_darkmoon_deathmatch::CheckRequirement);
    }
};

// 73814, 73810, 73104, 73815, 70764, 71084 - Citizen Costume 
class spell_darkmoon_citizen_costume : public AuraScript
{
    void OnApply(AuraEffect const* /*aurEff*/, AuraEffectHandleModes /*mode*/)
    {
        static uint32 Alliance_Model[15] = { 2225, 17479, 2220, 1437, 22699, 18231, 25521, 11680, 4888, 1434, 3112, 1482, 18137, 3068, 17241 };
        static uint32 Horde_Model[12] = { 1879, 11756, 5346, 18212, 16758, 4085, 3606, 2667, 2025, 4540, 16814, 27260 };

        if (Creature* l_Creature = GetTarget()->ToCreature())
            l_Creature->SetDisplayId(l_Creature->GetEntry() == 55347 ? Alliance_Model[urand(0, 14)] : Horde_Model[urand(0, 11)]);
    }

    void Register() override
    {
        AfterEffectApply += AuraEffectRemoveFn(spell_darkmoon_citizen_costume::OnApply, EFFECT_0, SPELL_AURA_TRANSFORM, AURA_EFFECT_HANDLE_REAL);
    }
};

void AddSC_darkmoon_island()
{
    RegisterCreatureAI(npc_fire_juggler_darkmoon);
    RegisterCreatureAI(npc_fire_eater_darkmoon);
    RegisterCreatureAI(boss_darkmoon_moonfang_mother);

    RegisterSpellScript(spell_darkmoon_staging_area_teleport);

    new item_darkmoon_faire_fireworks();

    RegisterSpellScript(spell_gen_repair_damaged_tonk);
    RegisterSpellScript(spell_gen_shoe_baby);
    RegisterSpellScript(spell_cook_crunchy_frog);
    RegisterSpellScript(spell_heal_injuried_carnie);
    RegisterSpellScript(spell_put_up_darkmoon_banner);
    RegisterSpellScript(spell_darkmoon_deathmatch);
    RegisterSpellScript(spell_darkmoon_citizen_costume);
};
