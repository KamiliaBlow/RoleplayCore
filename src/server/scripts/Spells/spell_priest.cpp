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

/*
 * Scripts for spells with SPELLFAMILY_PRIEST and SPELLFAMILY_GENERIC spells used by priest players.
 * Ordered alphabetically using scriptname.
 * Scriptnames of files in this file should be prefixed with "spell_pri_".
 */

#include "ScriptMgr.h"
#include "AreaTriggerAI.h"
#include "Containers.h"
#include "G3DPosition.hpp"
#include "GridNotifiers.h"
#include "ListUtils.h"
#include "Log.h"
#include "MoveSplineInitArgs.h"
#include "ObjectAccessor.h"
#include "PathGenerator.h"
#include "Player.h"
#include "SpellAuraEffects.h"
#include "SpellHistory.h"
#include "SpellMgr.h"
#include "SpellScript.h"
#include "TaskScheduler.h"
#include "TemporarySummon.h"

enum PriestSpells
{
    SPELL_PRIEST_ABYSSAL_REVERIE                    = 373054,
    SPELL_PRIEST_ANGELIC_FEATHER_AREATRIGGER        = 158624,
    SPELL_PRIEST_ANGELIC_FEATHER_AURA               = 121557,
    SPELL_PRIEST_ANSWERED_PRAYERS                   = 394289,
    SPELL_PRIEST_APOTHEOSIS                         = 200183,
    SPELL_PRIEST_ARMOR_OF_FAITH                     = 28810,
    SPELL_PRIEST_ASSURED_SAFETY                     = 440766,
    SPELL_PRIEST_ATONEMENT                          = 81749,
    SPELL_PRIEST_ATONEMENT_EFFECT                   = 194384,
    SPELL_PRIEST_ATONEMENT_HEAL                     = 81751,
    SPELL_PRIEST_BENEDICTION                        = 193157,
    SPELL_PRIEST_BENEVOLENCE                        = 415416,
    SPELL_PRIEST_BLAZE_OF_LIGHT                     = 215768,
    SPELL_PRIEST_BLAZE_OF_LIGHT_INCREASE            = 355851,
    SPELL_PRIEST_BLAZE_OF_LIGHT_DECREASE            = 356084,
    SPELL_PRIEST_BLESSED_HEALING                    = 70772,
    SPELL_PRIEST_BLESSED_LIGHT                      = 196813,
    SPELL_PRIEST_BODY_AND_SOUL                      = 64129,
    SPELL_PRIEST_BODY_AND_SOUL_SPEED                = 65081,
    SPELL_PRIEST_CIRCLE_OF_HEALING                  = 204883,
    SPELL_PRIEST_CRYSTALLINE_REFLECTION             = 373457,
    SPELL_PRIEST_CRYSTALLINE_REFLECTION_HEAL        = 373462,
    SPELL_PRIEST_CRYSTALLINE_REFLECTION_REFLECT     = 373464,
    SPELL_PRIEST_DARK_INDULGENCE                    = 372972,
    SPELL_PRIEST_DARK_REPRIMAND                     = 400169,
    SPELL_PRIEST_DARK_REPRIMAND_CHANNEL_DAMAGE      = 373129,
    SPELL_PRIEST_DARK_REPRIMAND_CHANNEL_HEALING     = 400171,
    SPELL_PRIEST_DARK_REPRIMAND_DAMAGE              = 373130,
    SPELL_PRIEST_DARK_REPRIMAND_HEALING             = 400187,
    SPELL_PRIEST_DAZZLING_LIGHT                     = 196810,
    SPELL_PRIEST_DIVINE_AEGIS                       = 47515,
    SPELL_PRIEST_DIVINE_AEGIS_ABSORB                = 47753,
    SPELL_PRIEST_DIVINE_BLESSING                    = 40440,
    SPELL_PRIEST_DIVINE_HYMN_HEAL                   = 64844,
    SPELL_PRIEST_DIVINE_IMAGE_SUMMON                = 392990,
    SPELL_PRIEST_DIVINE_IMAGE_EMPOWER               = 409387,
    SPELL_PRIEST_DIVINE_IMAGE_EMPOWER_STACK         = 405963,
    SPELL_PRIEST_DIVINE_SERVICE                     = 391233,
    SPELL_PRIEST_DIVINE_STAR_HOLY                   = 110744,
    SPELL_PRIEST_DIVINE_STAR_SHADOW                 = 122121,
    SPELL_PRIEST_DIVINE_STAR_HOLY_DAMAGE            = 122128,
    SPELL_PRIEST_DIVINE_STAR_HOLY_HEAL              = 110745,
    SPELL_PRIEST_DIVINE_STAR_SHADOW_DAMAGE          = 390845,
    SPELL_PRIEST_DIVINE_STAR_SHADOW_HEAL            = 390981,
    SPELL_PRIEST_DIVINE_WRATH                       = 40441,
    SPELL_PRIEST_EMPOWERED_RENEW_HEAL               = 391359,
    SPELL_PRIEST_EPIPHANY                           = 414553,
    SPELL_PRIEST_EPIPHANY_HIGHLIGHT                 = 414556,
    SPELL_PRIEST_ESSENCE_DEVOURER                   = 415479,
    SPELL_PRIEST_ESSENCE_DEVOURER_SHADOWFIEND_HEAL  = 415673,
    SPELL_PRIEST_ESSENCE_DEVOURER_MINDBENDER_HEAL   = 415676,
    SPELL_PRIEST_FLASH_HEAL                         = 2061,
    SPELL_PRIEST_FROM_DARKNESS_COMES_LIGHT_AURA     = 390617,
    SPELL_PRIEST_GREATER_HEAL                       = 289666,
    SPELL_PRIEST_FOCUSED_MENDING                    = 372354,
    SPELL_PRIEST_GUARDIAN_SPIRIT_HEAL               = 48153,
    SPELL_PRIEST_HALO_HOLY                          = 120517,
    SPELL_PRIEST_HALO_SHADOW                        = 120644,
    SPELL_PRIEST_HALO_HOLY_DAMAGE                   = 120696,
    SPELL_PRIEST_HALO_HOLY_HEAL                     = 120692,
    SPELL_PRIEST_HALO_SHADOW_DAMAGE                 = 390964,
    SPELL_PRIEST_HALO_SHADOW_HEAL                   = 390971,
    SPELL_PRIEST_HEAL                               = 2060,
    SPELL_PRIEST_HEALING_LIGHT                      = 196809,
    SPELL_PRIEST_HEAVENS_WRATH                      = 421558,
    SPELL_PRIEST_HOLY_FIRE                          = 14914,
    SPELL_PRIEST_HOLY_MENDING_HEAL                  = 391156,
    SPELL_PRIEST_HOLY_NOVA                          = 132157,
    SPELL_PRIEST_HOLY_WORD_CHASTISE                 = 88625,
    SPELL_PRIEST_HOLY_WORD_SALVATION                = 265202,
    SPELL_PRIEST_HOLY_WORD_SANCTIFY                 = 34861,
    SPELL_PRIEST_HOLY_WORD_SERENITY                 = 2050,
    SPELL_PRIEST_HOLY_10_1_CLASS_SET_2P_CHOOSER     = 411097,
    SPELL_PRIEST_HOLY_10_1_CLASS_SET_4P             = 405556,
    SPELL_PRIEST_HOLY_10_1_CLASS_SET_4P_EFFECT      = 409479,
    SPELL_PRIEST_INDEMNITY                          = 373049,
    SPELL_PRIEST_ITEM_EFFICIENCY                    = 37595,
    SPELL_PRIEST_LEAP_OF_FAITH_EFFECT               = 92832,
    SPELL_PRIEST_LEVITATE_EFFECT                    = 111759,
    SPELL_PRIEST_LIGHT_ERUPTION                     = 196812,
    SPELL_PRIEST_LIGHTS_WRATH_VISUAL                = 215795,
    SPELL_PRIEST_MASOCHISM_TALENT                   = 193063,
    SPELL_PRIEST_MASOCHISM_PERIODIC_HEAL            = 193065,
    SPELL_PRIEST_MASTERY_GRACE                      = 271534,
    SPELL_PRIEST_MIND_DEVOURER                      = 373202,
    SPELL_PRIEST_MIND_DEVOURER_AURA                 = 373204,
    SPELL_PRIEST_MINDBENDER_DISC                    = 123040,
    SPELL_PRIEST_MINDBENDER_SHADOW                  = 200174,
    SPELL_PRIEST_MINDGAMES                          = 375901,
    SPELL_PRIEST_MINDGAMES_VENTHYR                  = 323673,
    SPELL_PRIEST_MIND_BOMB_STUN                     = 226943,
    SPELL_PRIEST_MISERY                             = 238558,
    SPELL_PRIEST_ORACULAR_HEAL                      = 26170,
    SPELL_PRIEST_PAIN_TRANSFORMATION                = 372991,
    SPELL_PRIEST_PAIN_TRANSFORMATION_HEAL           = 372994,
    SPELL_PRIEST_PENANCE                            = 47540,
    SPELL_PRIEST_PENANCE_CHANNEL_DAMAGE             = 47758,
    SPELL_PRIEST_PENANCE_CHANNEL_HEALING            = 47757,
    SPELL_PRIEST_PENANCE_DAMAGE                     = 47666,
    SPELL_PRIEST_PENANCE_HEALING                    = 47750,
    SPELL_PRIEST_POWER_LEECH_MINDBENDER_MANA        = 123051,
    SPELL_PRIEST_POWER_LEECH_MINDBENDER_INSANITY    = 200010,
    SPELL_PRIEST_POWER_LEECH_SHADOWFIEND_MANA       = 343727,
    SPELL_PRIEST_POWER_LEECH_SHADOWFIEND_INSANITY   = 262485,
    SPELL_PRIEST_POWER_OF_THE_DARK_SIDE             = 198069,
    SPELL_PRIEST_POWER_OF_THE_DARK_SIDE_TINT        = 225795,
    SPELL_PRIEST_POWER_WORD_LIFE                    = 373481,
    SPELL_PRIEST_POWER_WORD_RADIANCE                = 194509,
    SPELL_PRIEST_POWER_WORD_SHIELD                  = 17,
    SPELL_PRIEST_POWER_WORD_SOLACE_ENERGIZE         = 129253,
    SPELL_PRIEST_PRAYER_OF_HEALING                  = 596,
    SPELL_PRIEST_PRAYER_OF_MENDING                  = 33076,
    SPELL_PRIEST_PRAYER_OF_MENDING_AURA             = 41635,
    SPELL_PRIEST_PRAYER_OF_MENDING_HEAL             = 33110,
    SPELL_PRIEST_PRAYER_OF_MENDING_JUMP             = 155793,
    SPELL_PRIEST_PROTECTIVE_LIGHT_AURA              = 193065,
    SPELL_PRIEST_PURGE_THE_WICKED                   = 204197,
    SPELL_PRIEST_PURGE_THE_WICKED_DUMMY             = 204215,
    SPELL_PRIEST_PURGE_THE_WICKED_PERIODIC          = 204213,
    SPELL_PRIEST_RAPTURE                            = 47536,
    SPELL_PRIEST_RENEW                              = 139,
    SPELL_PRIEST_RENEWED_HOPE                       = 197469,
    SPELL_PRIEST_RENEWED_HOPE_EFFECT                = 197470,
    SPELL_PRIEST_REVEL_IN_PURITY                    = 373003,
    SPELL_PRIEST_SANCTUARY                          = 231682,
    SPELL_PRIEST_SANCTUARY_ABSORB                   = 208771,
    SPELL_PRIEST_SANCTUARY_AURA                     = 208772,
    SPELL_PRIEST_SHADOW_COVENANT                    = 314867,
    SPELL_PRIEST_SHADOW_COVENANT_EFFECT             = 322105,
    SPELL_PRIEST_RHAPSODY_PROC                      = 390636,
    SPELL_PRIEST_SAY_YOUR_PRAYERS                   = 391186,
    SPELL_PRIEST_SCHISM                             = 424509,
    SPELL_PRIEST_SCHISM_AURA                        = 214621,
    SPELL_PRIEST_SEARING_LIGHT                      = 196811,
    SPELL_PRIEST_SHADOW_MEND_DAMAGE                 = 186439,
    SPELL_PRIEST_SHADOW_WORD_DEATH                  = 32379,
    SPELL_PRIEST_SHADOW_WORD_DEATH_DAMAGE           = 32409,
    SPELL_PRIEST_SHADOW_MEND_PERIODIC_DUMMY         = 187464,
    SPELL_PRIEST_SHADOW_WORD_PAIN                   = 589,
    SPELL_PRIEST_SHIELD_DISCIPLINE                  = 197045,
    SPELL_PRIEST_SHIELD_DISCIPLINE_EFFECT           = 47755,
    SPELL_PRIEST_SIN_AND_PUNISHMENT                 = 87204,
    SPELL_PRIEST_SINS_OF_THE_MANY                   = 280398,
    SPELL_PRIEST_SMITE                              = 585,
    SPELL_PRIEST_SPIRIT_OF_REDEMPTION               = 27827,
    SPELL_PRIEST_STRENGTH_OF_SOUL                   = 197535,
    SPELL_PRIEST_STRENGTH_OF_SOUL_EFFECT            = 197548,
    SPELL_PRIEST_SURGE_OF_LIGHT                     = 109186,
    SPELL_PRIEST_SURGE_OF_LIGHT_EFFECT              = 114255,
    SPELL_PRIEST_TRANQUIL_LIGHT                     = 196816,
    SPELL_PRIEST_THE_PENITENT_AURA                  = 200347,
    SPELL_PRIEST_TRAIL_OF_LIGHT_HEAL                = 234946,
    SPELL_PRIEST_TRINITY                            = 214205,
    SPELL_PRIEST_ULTIMATE_PENITENCE                 = 421453,
    SPELL_PRIEST_ULTIMATE_PENITENCE_DAMAGE          = 421543,
    SPELL_PRIEST_ULTIMATE_PENITENCE_HEAL            = 421544,
    SPELL_PRIEST_UNFURLING_DARKNESS                 = 341273,
    SPELL_PRIEST_UNFURLING_DARKNESS_AURA            = 341282,
    SPELL_PRIEST_UNFURLING_DARKNESS_DEBUFF          = 341291,
    SPELL_PRIEST_VAMPIRIC_EMBRACE_HEAL              = 15290,
    SPELL_PRIEST_VAMPIRIC_TOUCH                     = 34914,
    SPELL_PRIEST_VOID_SHIELD                        = 199144,
    SPELL_PRIEST_VOID_SHIELD_EFFECT                 = 199145,
    SPELL_PRIEST_WEAKENED_SOUL                      = 6788,
    SPELL_PRIEST_WHISPERING_SHADOWS                 = 406777,
    SPELL_PRIEST_WHISPERING_SHADOWS_DUMMY           = 391286,
    SPELL_PVP_RULES_ENABLED_HARDCODED               = 134735
};

enum PriestSpellVisuals
{
    SPELL_VISUAL_PRIEST_POWER_WORD_RADIANCE         = 52872,
    SPELL_VISUAL_PRIEST_PRAYER_OF_MENDING           = 38945
};

enum PriestSummons
{
    NPC_PRIEST_DIVINE_IMAGE                         = 198236,
    NPC_PRIEST_MINDBENDER                           = 62982,
    NPC_PRIEST_SHADOWFIEND                          = 19668
};

class RaidCheck
{
    public:
        explicit RaidCheck(Unit const* caster) : _caster(caster) { }

        bool operator()(WorldObject* obj) const
        {
            if (Unit* target = obj->ToUnit())
                return !_caster->IsInRaidWith(target);

            return true;
        }

    private:
        Unit const* _caster;
};

// 121536 - Angelic Feather talent
class spell_pri_angelic_feather_trigger : public SpellScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_PRIEST_ANGELIC_FEATHER_AREATRIGGER });
    }

    void HandleEffectDummy(SpellEffIndex /*effIndex*/) const
    {
        Position destPos = GetHitDest()->GetPosition();
        float radius = GetEffectInfo().CalcRadius();

        // Caster is prioritary
        if (GetCaster()->IsWithinDist2d(&destPos, radius))
        {
            GetCaster()->CastSpell(GetCaster(), SPELL_PRIEST_ANGELIC_FEATHER_AURA, true);
        }
        else
        {
            CastSpellExtraArgs args;
            args.TriggerFlags = TRIGGERED_FULL_MASK;
            args.CastDifficulty = GetCastDifficulty();
            GetCaster()->CastSpell(destPos, SPELL_PRIEST_ANGELIC_FEATHER_AREATRIGGER, args);
        }
    }

    void Register() override
    {
        OnEffectHit += SpellEffectFn(spell_pri_angelic_feather_trigger::HandleEffectDummy, EFFECT_0, SPELL_EFFECT_DUMMY);
    }
};

// Angelic Feather areatrigger - created by SPELL_PRIEST_ANGELIC_FEATHER_AREATRIGGER
struct areatrigger_pri_angelic_feather : AreaTriggerAI
{
    areatrigger_pri_angelic_feather(AreaTrigger* areatrigger) : AreaTriggerAI(areatrigger) { }

    // Called when the AreaTrigger has just been initialized, just before added to map
    void OnInitialize() override
    {
        if (Unit* caster = at->GetCaster())
        {
            std::vector<AreaTrigger*> areaTriggers = caster->GetAreaTriggers(SPELL_PRIEST_ANGELIC_FEATHER_AREATRIGGER);

            if (areaTriggers.size() >= 3)
                areaTriggers.front()->SetDuration(0);
        }
    }

    void OnUnitEnter(Unit* unit) override
    {
        if (Unit* caster = at->GetCaster())
        {
            if (caster->IsFriendlyTo(unit))
            {
                // If target already has aura, increase duration to max 130% of initial duration
                caster->CastSpell(unit, SPELL_PRIEST_ANGELIC_FEATHER_AURA, true);
                at->SetDuration(0);
            }
        }
    }
};

// 391387 - Answered Prayers
class spell_pri_answered_prayers : public AuraScript
{
    bool Validate(SpellInfo const* spellInfo) override
    {
        return ValidateSpellInfo({ SPELL_PRIEST_ANSWERED_PRAYERS, SPELL_PRIEST_APOTHEOSIS })
            && ValidateSpellEffect({ { spellInfo->Id, EFFECT_1 } });
    }

    void HandleOnProc(AuraEffect const* aurEff, ProcEventInfo const& eventInfo) const
    {
        Milliseconds extraDuration = 0ms;
        if (AuraEffect const* durationEffect = GetEffect(EFFECT_1))
            extraDuration = Seconds(durationEffect->GetAmount());

        Unit* target = eventInfo.GetActor();

        Aura* answeredPrayers = target->GetAura(SPELL_PRIEST_ANSWERED_PRAYERS);

        // Note: if caster has no aura, we must cast it first.
        if (!answeredPrayers)
            target->CastSpell(target, SPELL_PRIEST_ANSWERED_PRAYERS, TRIGGERED_IGNORE_CAST_IN_PROGRESS);
        else
        {
            // Note: there's no BaseValue dummy that we can use as reference, so we hardcode the increasing stack value.
            answeredPrayers->ModStackAmount(1);

            // Note: if current stacks match max. stacks, trigger Apotheosis.
            if (answeredPrayers->GetStackAmount() != aurEff->GetAmount())
                return;

            answeredPrayers->Remove();

            if (Aura* apotheosis = GetTarget()->GetAura(SPELL_PRIEST_APOTHEOSIS))
            {
                apotheosis->SetDuration(apotheosis->GetDuration() + extraDuration.count());
                apotheosis->SetMaxDuration(apotheosis->GetMaxDuration() + extraDuration.count());
            }
            else
                target->CastSpell(target, SPELL_PRIEST_APOTHEOSIS,
                    CastSpellExtraArgs(TRIGGERED_FULL_MASK & ~TRIGGERED_CAST_DIRECTLY)
                    .AddSpellMod(SPELLVALUE_DURATION, extraDuration.count()));
        }
    }

    void Register() override
    {
        OnEffectProc += AuraEffectProcFn(spell_pri_answered_prayers::HandleOnProc, EFFECT_0, SPELL_AURA_ADD_FLAT_MODIFIER_BY_SPELL_LABEL);
    }
};

// 26169 - Oracle Healing Bonus
class spell_pri_aq_3p_bonus : public AuraScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_PRIEST_ORACULAR_HEAL });
    }

    void HandleProc(AuraEffect const* aurEff, ProcEventInfo const& eventInfo)
    {
        PreventDefaultAction();
        Unit* caster = eventInfo.GetActor();
        if (caster == eventInfo.GetProcTarget())
            return;

        HealInfo* healInfo = eventInfo.GetHealInfo();
        if (!healInfo || !healInfo->GetHeal())
            return;

        CastSpellExtraArgs args(aurEff);
        args.AddSpellBP0(CalculatePct(static_cast<int32>(healInfo->GetHeal()), 10));
        caster->CastSpell(caster, SPELL_PRIEST_ORACULAR_HEAL, args);
    }

    void Register() override
    {
        OnEffectProc += AuraEffectProcFn(spell_pri_aq_3p_bonus::HandleProc, EFFECT_0, SPELL_AURA_DUMMY);
    }
};

// 81749 - Atonement
class spell_pri_atonement : public AuraScript
{
    bool Validate(SpellInfo const* spellInfo) override
    {
        return ValidateSpellInfo({ SPELL_PRIEST_ATONEMENT_HEAL, SPELL_PRIEST_SINS_OF_THE_MANY })
            && ValidateSpellEffect({ { spellInfo->Id, EFFECT_1 }, { SPELL_PRIEST_SINS_OF_THE_MANY, EFFECT_2 } });
    }

    static bool CheckProc(ProcEventInfo const& eventInfo)
    {
        return eventInfo.GetDamageInfo() != nullptr;
    }

    void HandleOnProc(AuraEffect const* aurEff, ProcEventInfo const& eventInfo)
    {
        TriggerAtonementHealOnTargets(aurEff, eventInfo);
    }

    void Register() override
    {
        DoCheckProc += AuraCheckProcFn(spell_pri_atonement::CheckProc);
        OnEffectProc += AuraEffectProcFn(spell_pri_atonement::HandleOnProc, EFFECT_0, SPELL_AURA_DUMMY);
    }

    std::vector<ObjectGuid> _appliedAtonements;

public:
    void AddAtonementTarget(ObjectGuid const& target)
    {
        _appliedAtonements.push_back(target);

        UpdateSinsOfTheManyValue();
    }

    void RemoveAtonementTarget(ObjectGuid const& target)
    {
        std::erase(_appliedAtonements, target);

        UpdateSinsOfTheManyValue();
    }

    std::vector<ObjectGuid> const& GetAtonementTargets() const
    {
        return _appliedAtonements;
    }

    struct TriggerArgs
    {
        SpellInfo const* TriggeredBy = nullptr;
        SpellSchoolMask DamageSchoolMask = SPELL_SCHOOL_MASK_NONE;
    };

    void TriggerAtonementHealOnTargets(AuraEffect const* atonementEffect, ProcEventInfo const& eventInfo)
    {
        Unit* priest = GetUnitOwner();
        DamageInfo* damageInfo = eventInfo.GetDamageInfo();
        CastSpellExtraArgs args(TRIGGERED_IGNORE_CAST_IN_PROGRESS | TRIGGERED_DONT_REPORT_CAST_ERROR);

        // Note: atonementEffect holds the correct amount since we passed the effect in the AuraScript that calls this method.
        args.AddSpellMod(SPELLVALUE_BASE_POINT0, CalculatePct(damageInfo->GetDamage(), atonementEffect->GetAmount()));

        args.SetCustomArg(TriggerArgs{
            .TriggeredBy = eventInfo.GetSpellInfo(),
            .DamageSchoolMask = eventInfo.GetDamageInfo()->GetSchoolMask() });

        float distanceLimit = GetEffectInfo(EFFECT_1).CalcValue();

        std::erase_if(_appliedAtonements, [priest, distanceLimit, &args](ObjectGuid const& targetGuid)
        {
            if (Unit* target = ObjectAccessor::GetUnit(*priest, targetGuid))
            {
                if (target->IsInDist2d(priest, distanceLimit))
                    priest->CastSpell(target, SPELL_PRIEST_ATONEMENT_HEAL, args);

                return false;
            }

            return true;
        });
    }

    void UpdateSinsOfTheManyValue() const
    {
        // Note: the damage dimish starts at the 6th application as of 10.0.5.
        constexpr std::array<float, 20> damageByStack = { 20.0f, 20.0f, 20.0f, 20.0f, 20.0f, 17.5f, 15.0f, 12.5f, 10.0f, 7.5f, 5.5f, 4.0f, 2.5f, 2.0f, 1.5f, 1.25f, 1.0f, 0.75f, 0.63f, 0.5f };

        for (SpellEffIndex effectIndex : { EFFECT_0, EFFECT_1, EFFECT_2 })
            if (AuraEffect* sinOfTheMany = GetUnitOwner()->GetAuraEffect(SPELL_PRIEST_SINS_OF_THE_MANY, effectIndex))
                sinOfTheMany->ChangeAmount(damageByStack[std::min(_appliedAtonements.size(), damageByStack.size() - 1)]);
    }
};

// 81751 - Atonement (Heal)
class spell_pri_abyssal_reverie : public SpellScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellEffect({ { SPELL_PRIEST_ABYSSAL_REVERIE, EFFECT_0 }  });
    }

    void CalculateHealingBonus(SpellEffectInfo const& /*spellEffectInfo*/, Unit const* /*victim*/, int32 const& /*healing*/, int32 const& /*flatMod*/, float& pctMod) const
    {
        spell_pri_atonement::TriggerArgs const* args = std::any_cast<spell_pri_atonement::TriggerArgs>(&GetSpell()->m_customArg);
        if (!args || !(args->DamageSchoolMask & SPELL_SCHOOL_MASK_SHADOW))
            return;

        if (AuraEffect* const abyssalReverieEffect = GetCaster()->GetAuraEffect(SPELL_PRIEST_ABYSSAL_REVERIE, EFFECT_0))
            AddPct(pctMod, abyssalReverieEffect->GetAmount());
    }

    void Register() override
    {
        CalcHealing += SpellCalcHealingFn(spell_pri_abyssal_reverie::CalculateHealingBonus);
    }
};

// 17 - Power Word: Shield
// 139 - Renew
// 2061 - Flash Heal
// 194509 - Power Word: Radiance
class spell_pri_atonement_effect : public SpellScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo
        ({
            SPELL_PRIEST_ATONEMENT,
            SPELL_PRIEST_ATONEMENT_EFFECT,
            SPELL_PRIEST_TRINITY,
            SPELL_PRIEST_POWER_WORD_RADIANCE,
            SPELL_PRIEST_POWER_WORD_SHIELD
        }) && ValidateSpellEffect({
            { SPELL_PRIEST_POWER_WORD_RADIANCE, EFFECT_3 },
            { SPELL_PRIEST_INDEMNITY, EFFECT_0 }
        });
    }

    bool Load() override
    {
        Unit* caster = GetCaster();
        if (!caster->HasAura(SPELL_PRIEST_ATONEMENT))
            return false;

        // only apply Trinity if the Priest has both Trinity and Atonement and the triggering spell is Power Word: Shield.
        if (caster->HasAura(SPELL_PRIEST_TRINITY))
        {
            if (GetSpellInfo()->Id != SPELL_PRIEST_POWER_WORD_SHIELD)
                return false;
        }

        return true;
    }

    void HandleOnHitTarget() const
    {
        Unit* caster = GetCaster();
        Unit* target = GetHitUnit();

        CastSpellExtraArgs args(TRIGGERED_FULL_MASK);
        args.SetTriggeringSpell(GetSpell());

        switch (GetSpellInfo()->Id)
        {
            case SPELL_PRIEST_POWER_WORD_SHIELD:
                if (AuraEffect const* indemnity = caster->GetAuraEffect(SPELL_PRIEST_INDEMNITY, EFFECT_0))
                    args.AddSpellMod(SPELLVALUE_DURATION,
                        (Seconds(indemnity->GetAmount())
                            + Milliseconds(Aura::CalcMaxDuration(sSpellMgr->AssertSpellInfo(_effectSpellId, GetCastDifficulty()),
                                caster, &GetSpell()->GetPowerCost()))).count());
                break;
            case SPELL_PRIEST_POWER_WORD_RADIANCE:
                // Power Word: Radiance applies Atonement at 60 % (without modifiers) of its total duration.
                args.AddSpellMod(SPELLVALUE_DURATION_PCT, GetEffectInfo(EFFECT_3).CalcValue(caster));
                break;
            default:
                break;
        }

        caster->CastSpell(target, _effectSpellId, args);
    }

    void Register() override
    {
        AfterHit += SpellHitFn(spell_pri_atonement_effect::HandleOnHitTarget);
    }

    uint32 _effectSpellId = SPELL_PRIEST_ATONEMENT_EFFECT;
};

// 194384 - Atonement (Buff), 214206 - Atonement [Trinity] (Buff)
class spell_pri_atonement_effect_aura : public AuraScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_PRIEST_ATONEMENT });
    }

    void HandleOnApply(AuraEffect const* /*aurEff*/, AuraEffectHandleModes /*mode*/)
    {
        RegisterHelper<&spell_pri_atonement::AddAtonementTarget>();
    }

    void HandleOnRemove(AuraEffect const* /*aurEff*/, AuraEffectHandleModes /*mode*/)
    {
        RegisterHelper<&spell_pri_atonement::RemoveAtonementTarget>();
    }

    template<void(spell_pri_atonement::*func)(ObjectGuid const&)>
    void RegisterHelper() const
    {
        if (Unit* caster = GetCaster())
            if (Aura* atonement = caster->GetAura(SPELL_PRIEST_ATONEMENT))
                if (spell_pri_atonement* script = atonement->GetScript<spell_pri_atonement>())
                    (script->*func)(GetTarget()->GetGUID());
    }

    void Register() override
    {
        OnEffectApply += AuraEffectApplyFn(spell_pri_atonement_effect_aura::HandleOnApply, EFFECT_0, SPELL_AURA_DUMMY, AURA_EFFECT_HANDLE_REAL);
        OnEffectRemove += AuraEffectRemoveFn(spell_pri_atonement_effect_aura::HandleOnRemove, EFFECT_0, SPELL_AURA_DUMMY, AURA_EFFECT_HANDLE_REAL);
    }
};

// 195178 - Atonement (Passive)
class spell_pri_atonement_passive : public AuraScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellEffect({ { SPELL_PRIEST_ATONEMENT, EFFECT_0 } });
    }

    static bool CheckProc(ProcEventInfo const& eventInfo)
    {
        return eventInfo.GetDamageInfo() != nullptr;
    }

    void HandleOnProc(AuraEffect const* /*aurEff*/, ProcEventInfo const& eventInfo) const
    {
        Unit* target = GetTarget();
        Unit* summoner = target->GetOwner();
        if (!summoner)
            return;

        if (AuraEffect const* atonementEffect = summoner->GetAuraEffect(SPELL_PRIEST_ATONEMENT, EFFECT_0))
            if (spell_pri_atonement* script = atonementEffect->GetBase()->GetScript<spell_pri_atonement>())
                script->TriggerAtonementHealOnTargets(atonementEffect, eventInfo);
    }

    void Register() override
    {
        OnEffectProc += AuraEffectProcFn(spell_pri_atonement_passive::HandleOnProc, EFFECT_0, SPELL_AURA_DUMMY);
    }
};

// 33110 - Prayer of Mending (Heal)
class spell_pri_benediction : public SpellScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_PRIEST_RENEW })
            && ValidateSpellEffect({ { SPELL_PRIEST_BENEDICTION, EFFECT_0 } });
    }

    void HandleEffectHitTarget(SpellEffIndex /*effIndex*/) const
    {
        if (AuraEffect const* benediction = GetCaster()->GetAuraEffect(SPELL_PRIEST_BENEDICTION, EFFECT_0))
            if (roll_chance_i(benediction->GetAmount()))
                GetCaster()->CastSpell(GetHitUnit(), SPELL_PRIEST_RENEW, TRIGGERED_IGNORE_GCD | TRIGGERED_IGNORE_CAST_IN_PROGRESS);
    }

    void Register() override
    {
        OnEffectHitTarget += SpellEffectFn(spell_pri_benediction::HandleEffectHitTarget, EFFECT_0, SPELL_EFFECT_HEAL);
    }
};

// 215768 - Blaze of Light
class spell_pri_blaze_of_light : public AuraScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo(
        {
            SPELL_PRIEST_BLAZE_OF_LIGHT_DECREASE,
            SPELL_PRIEST_BLAZE_OF_LIGHT_INCREASE
        });
    }

    void HandleProc(ProcEventInfo const& eventInfo) const
    {
        Unit* procTarget = eventInfo.GetProcTarget();
        if (!procTarget)
            return;

        if (GetTarget()->IsValidAttackTarget(procTarget))
            GetTarget()->CastSpell(procTarget, SPELL_PRIEST_BLAZE_OF_LIGHT_DECREASE, TriggerCastFlags(TRIGGERED_CAST_DIRECTLY | TRIGGERED_IGNORE_CAST_IN_PROGRESS));
        else
            GetTarget()->CastSpell(procTarget, SPELL_PRIEST_BLAZE_OF_LIGHT_INCREASE, TriggerCastFlags(TRIGGERED_CAST_DIRECTLY | TRIGGERED_IGNORE_CAST_IN_PROGRESS));
    }

    void Register() override
    {
        OnProc += AuraProcFn(spell_pri_blaze_of_light::HandleProc);
    }
};

// 204883 - Circle of Healing
class spell_pri_circle_of_healing : public SpellScript
{
    bool Validate(SpellInfo const* spellInfo) override
    {
        return ValidateSpellEffect({ { spellInfo->Id, EFFECT_1 } });
    }

    void FilterTargets(std::list<WorldObject*>& targets) const
    {
        // Note: we must remove one since target is always chosen.
        uint32 const maxTargets = uint32(GetSpellInfo()->GetEffect(EFFECT_1).CalcValue(GetCaster()) - 1);

        Trinity::SelectRandomInjuredTargets(targets, maxTargets, true);

        if (Unit* explicitTarget = GetExplTargetUnit())
            targets.push_front(explicitTarget);
    }

    void Register() override
    {
        OnObjectAreaTargetSelect += SpellObjectAreaTargetSelectFn(spell_pri_circle_of_healing::FilterTargets, EFFECT_0, TARGET_UNIT_DEST_AREA_ALLY);
    }
};

// 17 - Power Word: Shield
class spell_pri_crystalline_reflection : public AuraScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo
        ({
            SPELL_PRIEST_CRYSTALLINE_REFLECTION,
            SPELL_PRIEST_CRYSTALLINE_REFLECTION_HEAL,
            SPELL_PRIEST_CRYSTALLINE_REFLECTION_REFLECT,
        }) && ValidateSpellEffect({ { SPELL_PRIEST_CRYSTALLINE_REFLECTION, EFFECT_0 } });
    }

    void HandleOnApply(AuraEffect const* aurEff, AuraEffectHandleModes /*mode*/) const
    {
        Unit* caster = GetCaster();
        if (!caster)
            return;

        // Crystalline Reflection Heal
        if (caster->HasAura(SPELL_PRIEST_CRYSTALLINE_REFLECTION))
            caster->CastSpell(GetTarget(), SPELL_PRIEST_CRYSTALLINE_REFLECTION_HEAL, CastSpellExtraArgs(aurEff)
                .SetTriggerFlags(TRIGGERED_IGNORE_CAST_IN_PROGRESS | TRIGGERED_DONT_REPORT_CAST_ERROR));
    }

    void HandleAfterAbsorb(AuraEffect const* /*aurEff*/, DamageInfo const& dmgInfo, uint32 const& absorbAmount) const
    {
        Unit* caster = GetCaster();
        if (!caster)
            return;

        AuraEffect const* auraEff = caster->GetAuraEffect(SPELL_PRIEST_CRYSTALLINE_REFLECTION, EFFECT_0);
        if (!auraEff)
            return;

        Unit* attacker = dmgInfo.GetAttacker();
        if (!attacker)
            return;

        CastSpellExtraArgs args(TRIGGERED_DONT_REPORT_CAST_ERROR);
        args.AddSpellMod(SPELLVALUE_BASE_POINT0, CalculatePct(absorbAmount, auraEff->GetAmount()));
        caster->CastSpell(attacker, SPELL_PRIEST_CRYSTALLINE_REFLECTION_REFLECT, args);
    }

    void Register() override
    {
        AfterEffectApply += AuraEffectApplyFn(spell_pri_crystalline_reflection::HandleOnApply, EFFECT_0, SPELL_AURA_SCHOOL_ABSORB, AURA_EFFECT_HANDLE_REAL_OR_REAPPLY_MASK);
        AfterEffectAbsorb += AuraEffectAbsorbFn(spell_pri_crystalline_reflection::HandleAfterAbsorb, EFFECT_0);
    }
};

// 8092 - Mind Blast
class spell_pri_dark_indulgence : public SpellScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellEffect({ { SPELL_PRIEST_DARK_INDULGENCE, EFFECT_0 } });
    }

    void HandleEffectHit(SpellEffIndex /*effIndex*/) const
    {
        AuraEffect const* aurEff = GetCaster()->GetAuraEffect(SPELL_PRIEST_DARK_INDULGENCE, EFFECT_0);
        if (!aurEff)
            return;

        if (roll_chance_i(aurEff->GetAmount()))
            GetCaster()->CastSpell(GetCaster(), SPELL_PRIEST_POWER_OF_THE_DARK_SIDE, true);
    }

    void Register() override
    {
        OnEffectHit += SpellEffectFn(spell_pri_dark_indulgence::HandleEffectHit, EFFECT_0, SPELL_EFFECT_SCHOOL_DAMAGE);
    }
};

namespace DivineImageHelpers
{
Unit* GetSummon(Unit const* owner)
{
    for (Unit* summon : owner->m_Controlled)
        if (summon->GetEntry() == NPC_PRIEST_DIVINE_IMAGE)
            return summon;

    return nullptr;
}

Optional<uint32> GetSpellToCast(uint32 spellId)
{
    switch (spellId)
    {
        case SPELL_PRIEST_RENEW:
            return SPELL_PRIEST_TRANQUIL_LIGHT;
        case SPELL_PRIEST_POWER_WORD_SHIELD:
        case SPELL_PRIEST_POWER_WORD_LIFE:
        case SPELL_PRIEST_FLASH_HEAL:
        case SPELL_PRIEST_HEAL:
        case SPELL_PRIEST_GREATER_HEAL:
        case SPELL_PRIEST_HOLY_WORD_SERENITY:
            return SPELL_PRIEST_HEALING_LIGHT;
        case SPELL_PRIEST_PRAYER_OF_MENDING:
        case SPELL_PRIEST_PRAYER_OF_MENDING_HEAL:
            return SPELL_PRIEST_BLESSED_LIGHT;
        case SPELL_PRIEST_PRAYER_OF_HEALING:
        case SPELL_PRIEST_CIRCLE_OF_HEALING:
        case SPELL_PRIEST_HALO_HOLY:
        case SPELL_PRIEST_DIVINE_STAR_HOLY_HEAL:
        case SPELL_PRIEST_DIVINE_HYMN_HEAL:
        case SPELL_PRIEST_HOLY_WORD_SANCTIFY:
        case SPELL_PRIEST_HOLY_WORD_SALVATION:
            return SPELL_PRIEST_DAZZLING_LIGHT;
        case SPELL_PRIEST_SHADOW_WORD_PAIN:
        case SPELL_PRIEST_SMITE:
        case SPELL_PRIEST_HOLY_FIRE:
        case SPELL_PRIEST_SHADOW_WORD_DEATH:
        case SPELL_PRIEST_HOLY_WORD_CHASTISE:
        case SPELL_PRIEST_MINDGAMES:
        case SPELL_PRIEST_MINDGAMES_VENTHYR:
            return SPELL_PRIEST_SEARING_LIGHT;
        case SPELL_PRIEST_HOLY_NOVA:
            return SPELL_PRIEST_LIGHT_ERUPTION;
        default:
            break;
    }

    return {};
}

void Trigger(AuraEffect const* aurEff, ProcEventInfo const& eventInfo)
{
    Unit* target = eventInfo.GetActor();
    if (!target)
        return;

    Unit* divineImage = GetSummon(target);
    if (!divineImage)
        return;

    Optional<uint32> spellId = GetSpellToCast(eventInfo.GetSpellInfo()->Id);
    if (!spellId)
        return;

    divineImage->CastSpell(SpellCastTargets(eventInfo.GetProcSpell()->m_targets), *spellId, aurEff);
}
}

// 392988 - Divine Image
class spell_pri_divine_image : public AuraScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo
        ({
            SPELL_PRIEST_DIVINE_IMAGE_SUMMON,
            SPELL_PRIEST_DIVINE_IMAGE_EMPOWER,
            SPELL_PRIEST_DIVINE_IMAGE_EMPOWER_STACK
        });
    }

    static void HandleProc(AuraEffect const* aurEff, ProcEventInfo const& eventInfo)
    {
        Unit* target = eventInfo.GetActor();
        if (!target)
            return;

        // Note: if target has an active Divine Image, we should empower it rather than summoning a new one.
        if (Unit* divineImage = DivineImageHelpers::GetSummon(target))
        {
            // Note: Divine Image now teleports near the target when they cast a Holy Word spell if the Divine Image is further than 15 yards away (Patch 10.1.0).
            if (target->GetDistance(divineImage) > 15.0f)
                divineImage->NearTeleportTo(target->GetRandomNearPosition(3.0f));

            if (TempSummon* tempSummon = divineImage->ToTempSummon())
                tempSummon->RefreshTimer();

            divineImage->CastSpell(divineImage, SPELL_PRIEST_DIVINE_IMAGE_EMPOWER, eventInfo.GetProcSpell());
        }
        else
        {
            target->CastSpell(target, SPELL_PRIEST_DIVINE_IMAGE_SUMMON, CastSpellExtraArgs()
                .SetTriggeringAura(aurEff)
                .SetTriggeringSpell(eventInfo.GetProcSpell())
                .SetTriggerFlags(TRIGGERED_IGNORE_CAST_IN_PROGRESS | TRIGGERED_DISALLOW_PROC_EVENTS | TRIGGERED_DONT_REPORT_CAST_ERROR));

            // Note: Divine Image triggers a cast immediately based on the Holy Word cast.
            DivineImageHelpers::Trigger(aurEff, eventInfo);
        }

        target->CastSpell(target, SPELL_PRIEST_DIVINE_IMAGE_EMPOWER_STACK, CastSpellExtraArgs()
            .SetTriggeringAura(aurEff)
            .SetTriggeringSpell(eventInfo.GetProcSpell())
            .SetTriggerFlags(TRIGGERED_IGNORE_CAST_IN_PROGRESS | TRIGGERED_DISALLOW_PROC_EVENTS | TRIGGERED_DONT_REPORT_CAST_ERROR));
    }

    void Register() override
    {
        OnEffectProc += AuraEffectProcFn(spell_pri_divine_image::HandleProc, EFFECT_0, SPELL_AURA_DUMMY);
    }
};

// 405216 - Divine Image (Spell Cast Check)
class spell_pri_divine_image_spell_triggered : public AuraScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo
        ({
            SPELL_PRIEST_RENEW,
            SPELL_PRIEST_POWER_WORD_SHIELD,
            SPELL_PRIEST_POWER_WORD_LIFE,
            SPELL_PRIEST_FLASH_HEAL,
            SPELL_PRIEST_HOLY_WORD_SERENITY,
            SPELL_PRIEST_PRAYER_OF_MENDING,
            SPELL_PRIEST_PRAYER_OF_MENDING_HEAL,
            SPELL_PRIEST_PRAYER_OF_HEALING,
            SPELL_PRIEST_CIRCLE_OF_HEALING,
            SPELL_PRIEST_HALO_HOLY,
            SPELL_PRIEST_DIVINE_STAR_HOLY_HEAL,
            SPELL_PRIEST_DIVINE_HYMN_HEAL,
            SPELL_PRIEST_HOLY_WORD_SANCTIFY,
            SPELL_PRIEST_HOLY_WORD_SALVATION,
            SPELL_PRIEST_SMITE,
            SPELL_PRIEST_HOLY_FIRE,
            SPELL_PRIEST_SHADOW_WORD_DEATH,
            SPELL_PRIEST_SHADOW_WORD_PAIN,
            SPELL_PRIEST_MINDGAMES,
            SPELL_PRIEST_MINDGAMES_VENTHYR,
            SPELL_PRIEST_HOLY_WORD_CHASTISE,
            SPELL_PRIEST_HOLY_NOVA,
            SPELL_PRIEST_TRANQUIL_LIGHT,
            SPELL_PRIEST_HEALING_LIGHT,
            SPELL_PRIEST_BLESSED_LIGHT,
            SPELL_PRIEST_DAZZLING_LIGHT,
            SPELL_PRIEST_SEARING_LIGHT,
            SPELL_PRIEST_LIGHT_ERUPTION,
            SPELL_PRIEST_DIVINE_IMAGE_EMPOWER_STACK
        });
    }

    static bool CheckProc(ProcEventInfo const& eventInfo)
    {
        return DivineImageHelpers::GetSummon(eventInfo.GetActor()) != nullptr;
    }

    void HandleAfterRemove(AuraEffect const* /*aurEff*/, AuraEffectHandleModes /*mode*/) const
    {
        GetTarget()->RemoveAurasDueToSpell(SPELL_PRIEST_DIVINE_IMAGE_EMPOWER_STACK);
    }

    void Register() override
    {
        DoCheckProc += AuraCheckProcFn(spell_pri_divine_image_spell_triggered::CheckProc);
        OnEffectProc += AuraEffectProcFn(DivineImageHelpers::Trigger, EFFECT_0, SPELL_AURA_DUMMY);
        AfterEffectRemove += AuraEffectRemoveFn(spell_pri_divine_image_spell_triggered::HandleAfterRemove, EFFECT_0, SPELL_AURA_DUMMY, AURA_EFFECT_HANDLE_REAL);
    }
};

// 405963 Divine Image
// 409387 Divine Image
class spell_pri_divine_image_stack_timer : public AuraScript
{
    void TrackStackApplicationTime(AuraEffect const* /*aurEff*/, AuraEffectHandleModes /*mode*/) const
    {
        GetUnitOwner()->m_Events.AddEventAtOffset([spelId = GetId(), owner = GetUnitOwner()]
        {
            owner->RemoveAuraFromStack(spelId);
        }, Milliseconds(GetMaxDuration()));
    }

    void Register() override
    {
        AfterEffectApply += AuraEffectApplyFn(spell_pri_divine_image_stack_timer::TrackStackApplicationTime, EFFECT_0, SPELL_AURA_ANY, AURA_EFFECT_HANDLE_REAL_OR_REAPPLY_MASK);
    }
};

// 33110 - Prayer of Mending (Heal)
class spell_pri_divine_service : public SpellScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_PRIEST_PRAYER_OF_MENDING_AURA })
            && ValidateSpellEffect({ { SPELL_PRIEST_DIVINE_SERVICE, EFFECT_0 } });
    }

    void CalculateHealingBonus(SpellEffectInfo const& /*spellEffectInfo*/, Unit const* victim, int32& /*healing*/, int32& /*flatMod*/, float& pctMod) const
    {
        if (AuraEffect const* divineServiceEffect = GetCaster()->GetAuraEffect(SPELL_PRIEST_DIVINE_SERVICE, EFFECT_0))
            if (Aura const* prayerOfMending = victim->GetAura(SPELL_PRIEST_PRAYER_OF_MENDING_AURA, GetCaster()->GetGUID()))
                AddPct(pctMod, int32(divineServiceEffect->GetAmount() * prayerOfMending->GetStackAmount()));
    }

    void Register() override
    {
        CalcHealing += SpellCalcHealingFn(spell_pri_divine_service::CalculateHealingBonus);
    }
};

// 122121 - Divine Star (Shadow)
class spell_pri_divine_star_shadow : public SpellScript
{
    void HandleHitTarget(SpellEffIndex effIndex)
    {
        Unit* caster = GetCaster();

        if (caster->GetPowerType() != GetEffectInfo().MiscValue)
            PreventHitDefaultEffect(effIndex);
    }

    void Register() override
    {
        OnEffectHitTarget += SpellEffectFn(spell_pri_divine_star_shadow::HandleHitTarget, EFFECT_2, SPELL_EFFECT_ENERGIZE);
    }
};

// 110744 - Divine Star (Holy)
// 122121 - Divine Star (Shadow)
struct areatrigger_pri_divine_star : AreaTriggerAI
{
    using AreaTriggerAI::AreaTriggerAI;

    void OnInitialize() override
    {
        SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(at->GetSpellId(), DIFFICULTY_NONE);
        if (!spellInfo)
            return;

        if (spellInfo->GetEffects().size() <= EFFECT_1)
            return;

        Unit* caster = at->GetCaster();
        if (!caster)
            return;

        _casterCurrentPosition = caster->GetPosition();

        // Note: max. distance at which the Divine Star can travel to is EFFECT_1's BasePoints yards.
        float maxTravelDistance = float(spellInfo->GetEffect(EFFECT_1).CalcValue(caster));

        Position destPos = _casterCurrentPosition;
        at->MovePositionToFirstCollision(destPos, maxTravelDistance, 0.0f);

        PathGenerator firstPath(at);
        firstPath.CalculatePath(destPos.GetPositionX(), destPos.GetPositionY(), destPos.GetPositionZ(), false);

        at->InitSplines(firstPath.GetPath());
    }

    void OnUpdate(uint32 diff) override
    {
        _scheduler.Update(diff);
    }

    void OnUnitEnter(Unit* unit) override
    {
        HandleUnitEnterExit(unit);
    }

    void OnUnitExit(Unit* unit) override
    {
        // Note: this ensures any unit receives a second hit if they happen to be inside the AT when Divine Star starts its return path.
        HandleUnitEnterExit(unit);
    }

    void HandleUnitEnterExit(Unit* unit)
    {
        Unit* caster = at->GetCaster();
        if (!caster)
            return;

        if (advstd::ranges::contains(_affectedUnits, unit->GetGUID()))
            return;

        constexpr TriggerCastFlags TriggerFlags = TRIGGERED_IGNORE_GCD | TRIGGERED_IGNORE_CAST_IN_PROGRESS;

        if (caster->IsValidAttackTarget(unit))
            caster->CastSpell(unit, at->GetSpellId() == SPELL_PRIEST_DIVINE_STAR_SHADOW ? SPELL_PRIEST_DIVINE_STAR_SHADOW_DAMAGE : SPELL_PRIEST_DIVINE_STAR_HOLY_DAMAGE,
                TriggerFlags);
        else if (caster->IsValidAssistTarget(unit))
            caster->CastSpell(unit, at->GetSpellId() == SPELL_PRIEST_DIVINE_STAR_SHADOW ? SPELL_PRIEST_DIVINE_STAR_SHADOW_HEAL : SPELL_PRIEST_DIVINE_STAR_HOLY_HEAL,
                TriggerFlags);

        _affectedUnits.push_back(unit->GetGUID());
    }

    void OnDestinationReached() override
    {
        Unit* caster = at->GetCaster();
        if (!caster)
            return;

        if (at->GetDistance(_casterCurrentPosition) > 0.05f)
        {
            _affectedUnits.clear();

            ReturnToCaster();
        }
        else
            at->Remove();
    }

    void ReturnToCaster()
    {
        _scheduler.Schedule(0ms, [this](TaskContext task)
        {
            Unit* caster = at->GetCaster();
            if (!caster)
                return;

            _casterCurrentPosition = caster->GetPosition();

            Movement::PointsArray returnSplinePoints;

            returnSplinePoints.push_back(PositionToVector3(at));
            returnSplinePoints.push_back(PositionToVector3(caster));

            at->InitSplines(returnSplinePoints);

            task.Repeat(250ms);
        });
    }

private:
    TaskScheduler _scheduler;
    Position _casterCurrentPosition;
    std::vector<ObjectGuid> _affectedUnits;
};

// 391339 - Empowered Renew
class spell_pri_empowered_renew : public AuraScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_PRIEST_RENEW, SPELL_PRIEST_EMPOWERED_RENEW_HEAL })
            && ValidateSpellEffect({ { SPELL_PRIEST_RENEW, EFFECT_0 } })
            && sSpellMgr->AssertSpellInfo(SPELL_PRIEST_RENEW, DIFFICULTY_NONE)->GetEffect(EFFECT_0).IsAura(SPELL_AURA_PERIODIC_HEAL);
    }

    void HandleProc(AuraEffect* aurEff, ProcEventInfo const& eventInfo) const
    {
        Unit* caster = eventInfo.GetActor();
        Unit* target = eventInfo.GetProcTarget();

        SpellInfo const* renewSpellInfo = sSpellMgr->AssertSpellInfo(SPELL_PRIEST_RENEW, GetCastDifficulty());
        SpellEffectInfo const& renewEffect = renewSpellInfo->GetEffect(EFFECT_0);
        int32 estimatedTotalHeal = AuraEffect::CalculateEstimatedfTotalPeriodicAmount(caster, target, renewSpellInfo, renewEffect, renewEffect.CalcValue(caster), 1);
        int32 healAmount = CalculatePct(estimatedTotalHeal, aurEff->GetAmount());

        caster->CastSpell(target, SPELL_PRIEST_EMPOWERED_RENEW_HEAL, CastSpellExtraArgs(aurEff).AddSpellBP0(healAmount));
    }

    void Register() override
    {
        OnEffectProc += AuraEffectProcFn(spell_pri_empowered_renew::HandleProc, EFFECT_0, SPELL_AURA_DUMMY);
    }
};

// 414553 - Epiphany
class spell_pri_epiphany : public AuraScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_PRIEST_PRAYER_OF_MENDING, SPELL_PRIEST_EPIPHANY_HIGHLIGHT });
    }

    static bool CheckProc(AuraEffect const* aurEff, ProcEventInfo& /*eventInfo*/)
    {
        return roll_chance_i(aurEff->GetAmount());
    }

    void HandleOnProc(AuraEffect const* aurEff, ProcEventInfo& /*eventInfo*/) const
    {
        Unit* target = GetTarget();

        target->GetSpellHistory()->ResetCooldown(SPELL_PRIEST_PRAYER_OF_MENDING, true);

        target->CastSpell(target, SPELL_PRIEST_EPIPHANY_HIGHLIGHT, aurEff);
    }

    void Register() override
    {
        DoCheckEffectProc += AuraCheckEffectProcFn(spell_pri_epiphany::CheckProc, EFFECT_0, SPELL_AURA_DUMMY);
        OnEffectProc += AuraEffectProcFn(spell_pri_epiphany::HandleOnProc, EFFECT_0, SPELL_AURA_DUMMY);
    }
};

// 415673 - Essence Devourer (Heal)
// 415676 - Essence Devourer (Heal)
class spell_pri_essence_devourer_heal : public SpellScript
{
    static void FilterTargets(std::list<WorldObject*>& targets)
    {
        Trinity::SelectRandomInjuredTargets(targets, 1, true);
    }

    void Register() override
    {
        OnObjectAreaTargetSelect += SpellObjectAreaTargetSelectFn(spell_pri_essence_devourer_heal::FilterTargets, EFFECT_0, TARGET_UNIT_DEST_AREA_ALLY);
    }
};

// 246287 - Evangelism
class spell_pri_evangelism : public SpellScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo
        ({
            SPELL_PRIEST_TRINITY,
            SPELL_PRIEST_ATONEMENT_EFFECT,
        });
    }

    void HandleScriptEffect(SpellEffIndex /*effIndex*/) const
    {
        Unit* caster = GetCaster();
        Unit* target = GetHitUnit();

        Aura* atonementAura = target->GetAura(SPELL_PRIEST_ATONEMENT_EFFECT, caster->GetGUID());
        if (!atonementAura)
            return;

        Milliseconds extraDuration = Seconds(GetEffectValue());

        atonementAura->SetDuration(atonementAura->GetDuration() + extraDuration.count());
        atonementAura->SetMaxDuration(atonementAura->GetDuration() + extraDuration.count());
    }

    void Register() override
    {
        OnEffectHitTarget += SpellEffectFn(spell_pri_evangelism::HandleScriptEffect, EFFECT_0, SPELL_EFFECT_SCRIPT_EFFECT);
    }
};

// 33110 - Prayer of Mending (Heal)
class spell_pri_focused_mending : public SpellScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellEffect({ { SPELL_PRIEST_FOCUSED_MENDING, EFFECT_0 } });
    }

    void CalculateHealingBonus(SpellEffectInfo const& /*spellEffectInfo*/, Unit* /*victim*/, int32& /*healing*/, int32& /*flatMod*/, float& pctMod) const
    {
        if (AuraEffect const* focusedMendingEffect = GetCaster()->GetAuraEffect(SPELL_PRIEST_FOCUSED_MENDING, EFFECT_0))
        {
            bool const* isEmpoweredByFocusedMending = std::any_cast<bool>(&GetSpell()->m_customArg);

            if (isEmpoweredByFocusedMending && *isEmpoweredByFocusedMending)
                AddPct(pctMod, focusedMendingEffect->GetAmount());
        }
    }

    void Register() override
    {
        CalcHealing += SpellCalcHealingFn(spell_pri_focused_mending::CalculateHealingBonus);
    }
};

// 390615 - From Darkness Comes Light (Talent)
class spell_pri_from_darkness_comes_light : public AuraScript
{
    void HandleEffectProc(AuraEffect const* aurEff, ProcEventInfo& /*eventInfo*/) const
    {
        GetTarget()->CastSpell(GetTarget(), SPELL_PRIEST_FROM_DARKNESS_COMES_LIGHT_AURA, aurEff);
    }

    void Register() override
    {
        OnEffectProc += AuraEffectProcFn(spell_pri_from_darkness_comes_light::HandleEffectProc, EFFECT_0, SPELL_AURA_DUMMY);
    }
};

// 47788 - Guardian Spirit
class spell_pri_guardian_spirit : public AuraScript
{
    uint32 healPct = 0;

    bool Validate(SpellInfo const* spellInfo) override
    {
        return ValidateSpellInfo({ SPELL_PRIEST_GUARDIAN_SPIRIT_HEAL }) && ValidateSpellEffect({ { spellInfo->Id, EFFECT_1 } });
    }

    bool Load() override
    {
        healPct = GetEffectInfo(EFFECT_1).CalcValue();
        return true;
    }

    static void CalculateAmount(AuraEffect const* /*aurEff*/, int32 & amount, bool & /*canBeRecalculated*/)
    {
        // Set absorbtion amount to unlimited
        amount = -1;
    }

    void Absorb(AuraEffect* /*aurEff*/, DamageInfo const& dmgInfo, uint32 & absorbAmount)
    {
        Unit* target = GetTarget();
        if (dmgInfo.GetDamage() < target->GetHealth())
            return;

        int32 healAmount = int32(target->CountPctFromMaxHealth(healPct));
        // remove the aura now, we don't want 40% healing bonus
        Remove(AURA_REMOVE_BY_ENEMY_SPELL);
        CastSpellExtraArgs args(TRIGGERED_FULL_MASK);
        args.AddSpellBP0(healAmount);
        target->CastSpell(target, SPELL_PRIEST_GUARDIAN_SPIRIT_HEAL, args);
        absorbAmount = dmgInfo.GetDamage();
    }

    void Register() override
    {
        DoEffectCalcAmount += AuraEffectCalcAmountFn(spell_pri_guardian_spirit::CalculateAmount, EFFECT_1, SPELL_AURA_SCHOOL_ABSORB);
        OnEffectAbsorb += AuraEffectAbsorbFn(spell_pri_guardian_spirit::Absorb, EFFECT_1);
    }
};

// 421558 - Heaven's Wrath
class spell_pri_heavens_wrath : public AuraScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_PRIEST_ULTIMATE_PENITENCE });
    }

    static bool CheckProc(ProcEventInfo const& eventInfo)
    {
        return !(eventInfo.GetSpellInfo()->Id == SPELL_PRIEST_ULTIMATE_PENITENCE_DAMAGE || eventInfo.GetSpellInfo()->Id == SPELL_PRIEST_ULTIMATE_PENITENCE_HEAL);
    }

    static void HandleEffectProc(AuraEffect const* aurEff, ProcEventInfo const& eventInfo)
    {
        Unit* caster = eventInfo.GetActor();
        if (!caster)
            return;

        int32 cdReduction = aurEff->GetAmount();
        caster->GetSpellHistory()->ModifyCooldown(SPELL_PRIEST_ULTIMATE_PENITENCE, Seconds(-cdReduction), true);
    }

    void Register() override
    {
        OnEffectProc += AuraEffectProcFn(spell_pri_heavens_wrath::HandleEffectProc, EFFECT_0, SPELL_AURA_DUMMY);
    }
};

// 120644 - Halo (Shadow)
class spell_pri_halo_shadow : public SpellScript
{
    void HandleHitTarget(SpellEffIndex effIndex)
    {
        Unit* caster = GetCaster();

        if (caster->GetPowerType() != GetEffectInfo().MiscValue)
            PreventHitDefaultEffect(effIndex);
    }

    void Register() override
    {
        OnEffectHitTarget += SpellEffectFn(spell_pri_halo_shadow::HandleHitTarget, EFFECT_1, SPELL_EFFECT_ENERGIZE);
    }
};

// 120517 - Halo (Holy)
// 120644 - Halo (Shadow)
struct areatrigger_pri_halo : AreaTriggerAI
{
    areatrigger_pri_halo(AreaTrigger* areatrigger) : AreaTriggerAI(areatrigger) {}

    void OnUnitEnter(Unit* unit) override
    {
        if (Unit* caster = at->GetCaster())
        {
            if (caster->IsValidAttackTarget(unit))
                caster->CastSpell(unit, at->GetSpellId() == SPELL_PRIEST_HALO_SHADOW ? SPELL_PRIEST_HALO_SHADOW_DAMAGE : SPELL_PRIEST_HALO_HOLY_DAMAGE,
                    TRIGGERED_IGNORE_GCD | TRIGGERED_IGNORE_CAST_IN_PROGRESS);
            else if (caster->IsValidAssistTarget(unit))
                caster->CastSpell(unit, at->GetSpellId() == SPELL_PRIEST_HALO_SHADOW ? SPELL_PRIEST_HALO_SHADOW_HEAL : SPELL_PRIEST_HALO_HOLY_HEAL,
                    TRIGGERED_IGNORE_GCD | TRIGGERED_IGNORE_CAST_IN_PROGRESS);
        }
    }
};

// 391154 - Holy Mending
class spell_pri_holy_mending : public AuraScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_PRIEST_RENEW, SPELL_PRIEST_HOLY_MENDING_HEAL });
    }

    static bool CheckProc(AuraEffect const* /*aurEff*/, ProcEventInfo const& procInfo)
    {
        return procInfo.GetProcTarget()->HasAura(SPELL_PRIEST_RENEW, procInfo.GetActor()->GetGUID());
    }

    static void HandleProc(AuraEffect* aurEff, ProcEventInfo const& eventInfo)
    {
        eventInfo.GetActor()->CastSpell(eventInfo.GetProcTarget(), SPELL_PRIEST_HOLY_MENDING_HEAL, CastSpellExtraArgs(aurEff));
    }

    void Register() override
    {
        DoCheckEffectProc += AuraCheckEffectProcFn(spell_pri_holy_mending::CheckProc, EFFECT_0, SPELL_AURA_PROC_TRIGGER_SPELL);
        OnEffectProc += AuraEffectProcFn(spell_pri_holy_mending::HandleProc, EFFECT_0, SPELL_AURA_PROC_TRIGGER_SPELL);
    }
};

// 63733 - Holy Words
class spell_pri_holy_words : public AuraScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo(
        {
            SPELL_PRIEST_HEAL,
            SPELL_PRIEST_FLASH_HEAL,
            SPELL_PRIEST_PRAYER_OF_HEALING,
            SPELL_PRIEST_RENEW,
            SPELL_PRIEST_SMITE,
            SPELL_PRIEST_HOLY_WORD_CHASTISE,
            SPELL_PRIEST_HOLY_WORD_SANCTIFY,
            SPELL_PRIEST_HOLY_WORD_SERENITY
        }) && ValidateSpellEffect(
        {
            { SPELL_PRIEST_HOLY_WORD_SERENITY, EFFECT_1 },
            { SPELL_PRIEST_HOLY_WORD_SANCTIFY, EFFECT_3 },
            { SPELL_PRIEST_HOLY_WORD_CHASTISE, EFFECT_1 }
        });
    }

    void HandleProc(AuraEffect* /*aurEff*/, ProcEventInfo const& eventInfo) const
    {
        SpellInfo const* spellInfo = eventInfo.GetSpellInfo();
        if (!spellInfo)
            return;

        uint32 targetSpellId;
        SpellEffIndex cdReductionEffIndex;
        switch (spellInfo->Id)
        {
            case SPELL_PRIEST_HEAL:
            case SPELL_PRIEST_FLASH_HEAL: // reduce Holy Word: Serenity cd by 6 seconds
                targetSpellId = SPELL_PRIEST_HOLY_WORD_SERENITY;
                cdReductionEffIndex = EFFECT_1;
                // cdReduction = sSpellMgr->GetSpellInfo(SPELL_PRIEST_HOLY_WORD_SERENITY, GetCastDifficulty())->GetEffect(EFFECT_1)->CalcValue(player);
                break;
            case SPELL_PRIEST_PRAYER_OF_HEALING: // reduce Holy Word: Sanctify cd by 6 seconds
                targetSpellId = SPELL_PRIEST_HOLY_WORD_SANCTIFY;
                cdReductionEffIndex = EFFECT_2;
                break;
            case SPELL_PRIEST_RENEW: // reuce Holy Word: Sanctify cd by 2 seconds
                targetSpellId = SPELL_PRIEST_HOLY_WORD_SANCTIFY;
                cdReductionEffIndex = EFFECT_3;
                break;
            case SPELL_PRIEST_SMITE: // reduce Holy Word: Chastise cd by 4 seconds
                targetSpellId = SPELL_PRIEST_HOLY_WORD_CHASTISE;
                cdReductionEffIndex = EFFECT_1;
                break;
            default:
                TC_LOG_WARN("spells.priest", "HolyWords aura has been proced by an unknown spell: {}", GetSpellInfo()->Id);
                return;
        }

        SpellInfo const* targetSpellInfo = sSpellMgr->AssertSpellInfo(targetSpellId, GetCastDifficulty());
        int32 cdReduction = targetSpellInfo->GetEffect(cdReductionEffIndex).CalcValue(GetTarget());
        GetTarget()->GetSpellHistory()->ModifyCooldown(targetSpellInfo, Seconds(-cdReduction), true);
    }

    void Register() override
    {
        OnEffectProc += AuraEffectProcFn(spell_pri_holy_words::HandleProc, EFFECT_0, SPELL_AURA_DUMMY);
    }
};

// 265202 - Holy Word: Salvation
class spell_pri_holy_word_salvation : public SpellScript
{
    bool Validate(SpellInfo const* spellInfo) override
    {
        return ValidateSpellInfo
        ({
            SPELL_PRIEST_PRAYER_OF_MENDING_AURA,
            SPELL_PRIEST_RENEW
        }) && ValidateSpellEffect({
            { SPELL_PRIEST_PRAYER_OF_MENDING_HEAL, EFFECT_0 },
            { spellInfo->Id, EFFECT_1 }
        }) && spellInfo->GetEffect(EFFECT_1).TargetB.GetTarget() == TARGET_UNIT_SRC_AREA_ALLY;
    }

    bool Load() override
    {
        _spellInfoHeal = sSpellMgr->AssertSpellInfo(SPELL_PRIEST_PRAYER_OF_MENDING_HEAL, DIFFICULTY_NONE);
        _healEffectDummy = &_spellInfoHeal->GetEffect(EFFECT_0);
        return true;
    }

    void HandleApplyBuffs(SpellEffIndex /*effIndex*/) const
    {
        Unit* caster = GetCaster();
        Unit* target = GetHitUnit();

        CastSpellExtraArgs args;
        args.TriggerFlags = TRIGGERED_FULL_MASK;

        // amount of Prayer of Mending is SPELL_PRIEST_HOLY_WORD_SALVATION's EFFECT_1.
        args.AddSpellMod(SPELLVALUE_AURA_STACK, GetEffectValue());

        int32 basePoints = caster->SpellHealingBonusDone(target, _spellInfoHeal, _healEffectDummy->CalcValue(caster), HEAL, *_healEffectDummy);
        args.AddSpellMod(SPELLVALUE_BASE_POINT0, basePoints);
        caster->CastSpell(target, SPELL_PRIEST_PRAYER_OF_MENDING_AURA, args);

        // a full duration Renew is triggered.
        caster->CastSpell(target, SPELL_PRIEST_RENEW, CastSpellExtraArgs(TRIGGERED_FULL_MASK).SetTriggeringSpell(GetSpell()));
    }

    void Register() override
    {
        OnEffectHitTarget += SpellEffectFn(spell_pri_holy_word_salvation::HandleApplyBuffs, EFFECT_1, SPELL_EFFECT_DUMMY);
    }

    SpellInfo const* _spellInfoHeal = nullptr;
    SpellEffectInfo const* _healEffectDummy = nullptr;
};

// 2050 - Holy Word: Serenity
// 34861 - Holy Word: Sanctify
class spell_pri_holy_word_salvation_cooldown_reduction : public SpellScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_PRIEST_HOLY_WORD_SALVATION })
            && ValidateSpellEffect({ { SPELL_PRIEST_HOLY_WORD_SALVATION, EFFECT_2 } });
    }

    bool Load() override
    {
        return GetCaster()->HasSpell(SPELL_PRIEST_HOLY_WORD_SALVATION);
    }

    void ReduceCooldown() const
    {
        // cooldown reduced by SPELL_PRIEST_HOLY_WORD_SALVATION's Seconds(EFFECT_2).
        int32 cooldownReduction = sSpellMgr->AssertSpellInfo(SPELL_PRIEST_HOLY_WORD_SALVATION, GetCastDifficulty())->GetEffect(EFFECT_2).CalcValue(GetCaster());

        GetCaster()->GetSpellHistory()->ModifyCooldown(SPELL_PRIEST_HOLY_WORD_SALVATION, Seconds(-cooldownReduction), true);
    }

    void Register() override
    {
        AfterCast += SpellCastFn(spell_pri_holy_word_salvation_cooldown_reduction::ReduceCooldown);
    }
};

// 40438 - Priest Tier 6 Trinket
class spell_pri_item_t6_trinket : public AuraScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_PRIEST_DIVINE_BLESSING, SPELL_PRIEST_DIVINE_WRATH });
    }

    void HandleProc(AuraEffect* /*aurEff*/, ProcEventInfo const& eventInfo)
    {
        PreventDefaultAction();
        Unit* caster = eventInfo.GetActor();
        if (eventInfo.GetSpellTypeMask() & PROC_SPELL_TYPE_HEAL)
            caster->CastSpell(nullptr, SPELL_PRIEST_DIVINE_BLESSING, true);

        if (eventInfo.GetSpellTypeMask() & PROC_SPELL_TYPE_DAMAGE)
            caster->CastSpell(nullptr, SPELL_PRIEST_DIVINE_WRATH, true);
    }

    void Register() override
    {
        OnEffectProc += AuraEffectProcFn(spell_pri_item_t6_trinket::HandleProc, EFFECT_0, SPELL_AURA_DUMMY);
    }
};

// 92833 - Leap of Faith
class spell_pri_leap_of_faith_effect_trigger : public SpellScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_PRIEST_LEAP_OF_FAITH_EFFECT });
    }

    void HandleEffectDummy(SpellEffIndex /*effIndex*/) const
    {
        Position destPos = GetHitDest()->GetPosition();

        SpellCastTargets targets;
        targets.SetDst(destPos);
        targets.SetUnitTarget(GetCaster());
        GetHitUnit()->CastSpell(std::move(targets), GetEffectValue(), GetCastDifficulty());
    }

    void Register() override
    {
        OnEffectHitTarget += SpellEffectFn(spell_pri_leap_of_faith_effect_trigger::HandleEffectDummy, EFFECT_0, SPELL_EFFECT_DUMMY);
    }
};

// 1706 - Levitate
class spell_pri_levitate : public SpellScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_PRIEST_LEVITATE_EFFECT });
    }

    void HandleDummy(SpellEffIndex /*effIndex*/) const
    {
        GetCaster()->CastSpell(GetHitUnit(), SPELL_PRIEST_LEVITATE_EFFECT, true);
    }

    void Register() override
    {
        OnEffectHitTarget += SpellEffectFn(spell_pri_levitate::HandleDummy, EFFECT_0, SPELL_EFFECT_DUMMY);
    }
};

// 373178 - Light's Wrath
class spell_pri_lights_wrath : public SpellScript
{
    bool Validate(SpellInfo const* spellInfo) override
    {
        return ValidateSpellEffect({ { spellInfo->Id, EFFECT_1 } });
    }

    void OnPrecast() override
    {
        Aura const* atonement = GetCaster()->GetAura(SPELL_PRIEST_ATONEMENT);
        if (!atonement)
            return;

        spell_pri_atonement const* script = atonement->GetScript<spell_pri_atonement>();
        if (!script)
            return;

        for (ObjectGuid const& atonementTarget : script->GetAtonementTargets())
        {
            if (Unit* target = ObjectAccessor::GetUnit(*GetCaster(), atonementTarget))
            {
                target->CancelSpellMissiles(SPELL_PRIEST_LIGHTS_WRATH_VISUAL, false, false);
                target->CastSpell(GetCaster(), SPELL_PRIEST_LIGHTS_WRATH_VISUAL, TRIGGERED_IGNORE_CAST_IN_PROGRESS);
            }
        }
    }

    void CalculateDamageBonus(SpellEffectInfo const& /*spellEffectInfo*/, Unit const* /*victim*/, int32 const& /*damage*/, int32 const& /*flatMod*/, float& pctMod) const
    {
        Aura const* atonement = GetCaster()->GetAura(SPELL_PRIEST_ATONEMENT);
        if (!atonement)
            return;

        // Atonement size may have changed when missile hits, we need to take an updated count of Atonement applications.
        if (spell_pri_atonement const* script = atonement->GetScript<spell_pri_atonement>())
            AddPct(pctMod, GetEffectInfo(EFFECT_1).CalcValue(GetCaster()) * script->GetAtonementTargets().size());
    }

    void Register() override
    {
        CalcDamage += SpellCalcDamageFn(spell_pri_lights_wrath::CalculateDamageBonus);
    }
};

// 205369 - Mind Bomb
class spell_pri_mind_bomb : public AuraScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_PRIEST_MIND_BOMB_STUN });
    }

    void RemoveEffect(AuraEffect const* /*aurEff*/, AuraEffectHandleModes /*mode*/) const
    {
        if (GetTargetApplication()->GetRemoveMode() == AURA_REMOVE_BY_DEATH || GetTargetApplication()->GetRemoveMode() == AURA_REMOVE_BY_EXPIRE)
            if (Unit* caster = GetCaster())
                caster->CastSpell(GetTarget()->GetPosition(), SPELL_PRIEST_MIND_BOMB_STUN, true);
    }

    void Register() override
    {
        AfterEffectRemove += AuraEffectRemoveFn(spell_pri_mind_bomb::RemoveEffect, EFFECT_0, SPELL_AURA_DUMMY, AURA_EFFECT_HANDLE_REAL);
    }
};

// 373202 - Mind Devourer
// Triggered by 8092 - Mind Blast
class spell_pri_mind_devourer : public SpellScript
{
     bool Validate(SpellInfo const* /*spellInfo*/) override
     {
         return ValidateSpellInfo({ SPELL_PRIEST_MIND_DEVOURER_AURA })
             && ValidateSpellEffect({ { SPELL_PRIEST_MIND_DEVOURER, EFFECT_0 } });
     }

     bool Load() override
     {
         return GetCaster()->HasAura(SPELL_PRIEST_MIND_DEVOURER);
     }

    void HandleEffectHitTarget(SpellEffIndex /*effIndex*/) const
    {
        AuraEffect const* aurEff = GetCaster()->GetAuraEffect(SPELL_PRIEST_MIND_DEVOURER, EFFECT_0);
        if (aurEff && roll_chance_i(aurEff->GetAmount()))
            GetCaster()->CastSpell(GetCaster(), SPELL_PRIEST_MIND_DEVOURER_AURA, GetSpell());
    }

    void Register() override
    {
        OnEffectHitTarget += SpellEffectFn(spell_pri_mind_devourer::HandleEffectHitTarget, EFFECT_0, SPELL_EFFECT_SCHOOL_DAMAGE);
    }
};

// 373204 - Mind Devourer (Aura)
// Attached to 335467 - Devouring Plague
class spell_pri_mind_devourer_buff_aura : public AuraScript
{
    void CalculateDamage(AuraEffect const* /*aurEff*/, Unit* /*victim*/, int32& /*damage*/, int32& /*flatMod*/, float& pctMod) const
    {
        AddPct(pctMod, DamageIncrease);
    }

    void Register() override
    {
        DoEffectCalcDamageAndHealing += AuraEffectCalcDamageFn(spell_pri_mind_devourer_buff_aura::CalculateDamage, EFFECT_1, SPELL_AURA_PERIODIC_LEECH);
    }

public:
    float DamageIncrease = 0.0f;
};

class spell_pri_mind_devourer_buff : public SpellScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellEffect({ { SPELL_PRIEST_MIND_DEVOURER_AURA, EFFECT_1 } });
    }

    void OnPrecast() override
    {
        AuraEffect const* mindDevourer = GetCaster()->GetAuraEffect(SPELL_PRIEST_MIND_DEVOURER_AURA, EFFECT_1);
        if (!mindDevourer || !GetSpell()->m_appliedMods.contains(mindDevourer->GetBase()))
            return;

        _damageIncrease = mindDevourer->GetAmount();
    }

    void CalculateDamage(SpellEffectInfo const& /*spellEffectInfo*/, Unit* /*victim*/, int32& /*damage*/, int32& /*flatMod*/, float& pctMod) const
    {
        AddPct(pctMod, _damageIncrease);
    }

    void ModifyAuraValueAndRemoveBuff(SpellEffIndex /*effIndex*/) const
    {
        if (!_damageIncrease)
            return;

        if (Aura* devouringPlague = GetHitAura())
            if (spell_pri_mind_devourer_buff_aura* script = devouringPlague->GetScript<spell_pri_mind_devourer_buff_aura>())
                script->DamageIncrease = _damageIncrease;

        GetCaster()->RemoveAurasDueToSpell(SPELL_PRIEST_MIND_DEVOURER_AURA);
    }

    void Register() override
    {
        CalcDamage += SpellCalcDamageFn(spell_pri_mind_devourer_buff::CalculateDamage);
        OnEffectHitTarget += SpellEffectFn(spell_pri_mind_devourer_buff::ModifyAuraValueAndRemoveBuff, EFFECT_1, SPELL_EFFECT_APPLY_AURA);
    }

    float _damageIncrease = 0.0f;
};

// 390686 - Painful Punishment
class spell_pri_painful_punishment : public AuraScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo
        ({
            SPELL_PRIEST_SHADOW_WORD_PAIN,
            SPELL_PRIEST_PURGE_THE_WICKED_PERIODIC
        });
    }

    static void HandleEffectProc(AuraEffect const* aurEff, ProcEventInfo const& eventInfo)
    {
        Unit* caster = eventInfo.GetActor();
        Unit* target = eventInfo.GetActionTarget();
        if (!caster || !target)
            return;

        int32 additionalDuration = aurEff->GetAmount();

        if (Aura* shadowWordPain = target->GetOwnedAura(SPELL_PRIEST_SHADOW_WORD_PAIN, caster->GetGUID()))
            shadowWordPain->SetDuration(shadowWordPain->GetDuration() + additionalDuration);

        if (Aura* purgeTheWicked = target->GetOwnedAura(SPELL_PRIEST_PURGE_THE_WICKED_PERIODIC, caster->GetGUID()))
            purgeTheWicked->SetDuration(purgeTheWicked->GetDuration() + additionalDuration);
    }

    void Register() override
    {
        OnEffectProc += AuraEffectProcFn(spell_pri_painful_punishment::HandleEffectProc, EFFECT_0, SPELL_AURA_DUMMY);
    }
};

// 372991 - Pain Transformation
// Triggered by 33206 - Pain Suppression
class spell_pri_pain_transformation : public SpellScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo
        ({
            SPELL_PRIEST_ATONEMENT_EFFECT,
            SPELL_PRIEST_TRINITY,
            SPELL_PRIEST_PAIN_TRANSFORMATION,
            SPELL_PRIEST_PAIN_TRANSFORMATION_HEAL
        });
    }

    bool Load() override
    {
        return GetCaster()->HasAura(SPELL_PRIEST_PAIN_TRANSFORMATION) && !GetCaster()->HasAura(SPELL_PRIEST_TRINITY);
    }

    void HandleHit(SpellEffIndex /*effIndex*/) const
    {
        CastSpellExtraArgs args(GetSpell());
        args.SetTriggerFlags(TRIGGERED_IGNORE_CAST_IN_PROGRESS | TRIGGERED_DONT_REPORT_CAST_ERROR);

        GetCaster()->CastSpell(GetHitUnit(), SPELL_PRIEST_PAIN_TRANSFORMATION_HEAL, args);
        GetCaster()->CastSpell(GetHitUnit(), SPELL_PRIEST_ATONEMENT_EFFECT, args);
    }

    void Register() override
    {
        OnEffectHitTarget += SpellEffectFn(spell_pri_pain_transformation::HandleHit, EFFECT_0, SPELL_EFFECT_APPLY_AURA);
    }
};

// 47540 - Penance
// 400169 - Dark Reprimand
class spell_pri_penance : public SpellScript
{
public:
    spell_pri_penance(uint32 damageSpellId, uint32 healingSpellId) : _damageSpellId(damageSpellId), _healingSpellId(healingSpellId)
    {
    }

    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ _damageSpellId, _healingSpellId });
    }

    SpellCastResult CheckCast() const
    {
        Unit* caster = GetCaster();

        if (Unit* target = GetExplTargetUnit())
        {
            if (!caster->IsFriendlyTo(target))
            {
                if (!caster->IsValidAttackTarget(target))
                    return SPELL_FAILED_BAD_TARGETS;

                if (!caster->isInFront(target))
                    return SPELL_FAILED_UNIT_NOT_INFRONT;
            }
        }

        return SPELL_CAST_OK;
    }

    void HandleDummy(SpellEffIndex /*effIndex*/) const
    {
        Unit* caster = GetCaster();

        if (Unit* target = GetHitUnit())
        {
            if (caster->IsFriendlyTo(target))
                caster->CastSpell(target, _healingSpellId, CastSpellExtraArgs(TRIGGERED_IGNORE_GCD | TRIGGERED_IGNORE_SPELL_AND_CATEGORY_CD)
                    .SetTriggeringSpell(GetSpell()));
            else
                caster->CastSpell(target, _damageSpellId, CastSpellExtraArgs(TRIGGERED_IGNORE_GCD | TRIGGERED_IGNORE_SPELL_AND_CATEGORY_CD)
                    .SetTriggeringSpell(GetSpell()));
        }
    }

    void Register() override
    {
        OnCheckCast += SpellCheckCastFn(spell_pri_penance::CheckCast);
        OnEffectHitTarget += SpellEffectFn(spell_pri_penance::HandleDummy, EFFECT_0, SPELL_EFFECT_DUMMY);
    }

private:
    uint32 _damageSpellId;
    uint32 _healingSpellId;
};

// 47758 - Penance (Channel Damage), 47757 - Penance (Channel Healing)
// 373129 - Dark Reprimand (Channel Damage), 400171 - Dark Reprimand (Channel Healing)
class spell_pri_penance_or_dark_reprimand_channeled : public AuraScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_PRIEST_POWER_OF_THE_DARK_SIDE });
    }

    void HandleOnRemove(AuraEffect const* /*aurEff*/, AuraEffectHandleModes /*mode*/) const
    {
        if (Unit* caster = GetCaster())
            caster->RemoveAura(SPELL_PRIEST_POWER_OF_THE_DARK_SIDE);
    }

    void Register() override
    {
        OnEffectRemove += AuraEffectRemoveFn(spell_pri_penance_or_dark_reprimand_channeled::HandleOnRemove, EFFECT_0, SPELL_AURA_DUMMY, AURA_EFFECT_HANDLE_REAL);
    }
};

// 114239 - Phantasm
class spell_pri_phantasm : public SpellScript
{
    void HandleEffectHit(SpellEffIndex /*effIndex*/) const
    {
        GetCaster()->RemoveMovementImpairingAuras(false);
    }

    void Register() override
    {
        OnEffectHit += SpellEffectFn(spell_pri_phantasm::HandleEffectHit, EFFECT_0, SPELL_EFFECT_DUMMY);
    }
};

// 262484 - Power Leech (Passive for Shadowfiend)
// 284621 - Power Leech (Passive for Mindbender)
class spell_pri_power_leech_passive : public AuraScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo
        ({
            SPELL_PRIEST_POWER_LEECH_SHADOWFIEND_INSANITY,
            SPELL_PRIEST_POWER_LEECH_SHADOWFIEND_MANA,
            SPELL_PRIEST_POWER_LEECH_MINDBENDER_INSANITY,
            SPELL_PRIEST_POWER_LEECH_MINDBENDER_MANA,
            SPELL_PRIEST_ESSENCE_DEVOURER,
            SPELL_PRIEST_ESSENCE_DEVOURER_SHADOWFIEND_HEAL,
            SPELL_PRIEST_ESSENCE_DEVOURER_MINDBENDER_HEAL
        })
            && ValidateSpellEffect
        ({
            { SPELL_PRIEST_POWER_LEECH_SHADOWFIEND_INSANITY, EFFECT_0 },
            { SPELL_PRIEST_POWER_LEECH_SHADOWFIEND_MANA, EFFECT_0 },
            { SPELL_PRIEST_POWER_LEECH_MINDBENDER_INSANITY, EFFECT_0 },
            { SPELL_PRIEST_POWER_LEECH_MINDBENDER_MANA, EFFECT_0 }
        });
    }

    static bool CheckProc(ProcEventInfo const& eventInfo)
    {
        return eventInfo.GetDamageInfo() != nullptr;
    }

    void HandleOnProc(AuraEffect const* aurEff, ProcEventInfo& /*eventInfo*/) const
    {
        Unit* target = GetTarget();
        Player* summoner = Object::ToPlayer(target->GetOwner());
        if (!summoner)
            return;

        SpellInfo const* spellInfo = nullptr;
        int32 divisor = 1;

        if (summoner->GetPrimarySpecialization() != ChrSpecialization::PriestShadow)
        {
            if (target->GetEntry() == NPC_PRIEST_SHADOWFIEND)
            {
                // Note: divisor is 100 because effect value is 5 and its supposed to restore 0.5%
                spellInfo = sSpellMgr->AssertSpellInfo(SPELL_PRIEST_POWER_LEECH_SHADOWFIEND_MANA, GetCastDifficulty());
                divisor = 10;
            }
            else
            {
                // Note: divisor is 100 because effect value is 20 and its supposed to restore 0.2%
                spellInfo = sSpellMgr->AssertSpellInfo(SPELL_PRIEST_POWER_LEECH_MINDBENDER_MANA, GetCastDifficulty());
                divisor = 100;
            }
        }
        else
            spellInfo = sSpellMgr->AssertSpellInfo(target->GetEntry() == NPC_PRIEST_SHADOWFIEND
                ? SPELL_PRIEST_POWER_LEECH_SHADOWFIEND_INSANITY
                : SPELL_PRIEST_POWER_LEECH_MINDBENDER_INSANITY, GetCastDifficulty());

        target->CastSpell(summoner, spellInfo->Id, CastSpellExtraArgs(aurEff)
            .AddSpellMod(SPELLVALUE_BASE_POINT0, spellInfo->GetEffect(EFFECT_0).CalcValue() / divisor));

        // Note: Essence Devourer talent.
        if (summoner->HasAura(SPELL_PRIEST_ESSENCE_DEVOURER))
            summoner->CastSpell(nullptr, target->GetEntry() == NPC_PRIEST_SHADOWFIEND ? SPELL_PRIEST_ESSENCE_DEVOURER_SHADOWFIEND_HEAL : SPELL_PRIEST_ESSENCE_DEVOURER_MINDBENDER_HEAL, aurEff);
    }

    void Register() override
    {
        DoCheckProc += AuraCheckProcFn(spell_pri_power_leech_passive::CheckProc);
        OnEffectProc += AuraEffectProcFn(spell_pri_power_leech_passive::HandleOnProc, EFFECT_0, SPELL_AURA_DUMMY);
    }
};

// 198069 - Power of the Dark Side
class spell_pri_power_of_the_dark_side : public AuraScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_PRIEST_POWER_OF_THE_DARK_SIDE_TINT });
    }

    void HandleOnApply(AuraEffect const* /*aurEff*/, AuraEffectHandleModes /*mode*/) const
    {
        if (Unit* caster = GetCaster())
            caster->CastSpell(caster, SPELL_PRIEST_POWER_OF_THE_DARK_SIDE_TINT, true);
    }

    void HandleOnRemove(AuraEffect const* /*aurEff*/, AuraEffectHandleModes /*mode*/) const
    {
        if (Unit* caster = GetCaster())
            caster->RemoveAura(SPELL_PRIEST_POWER_OF_THE_DARK_SIDE_TINT);
    }

    void Register() override
    {
        OnEffectApply += AuraEffectApplyFn(spell_pri_power_of_the_dark_side::HandleOnApply, EFFECT_0, SPELL_AURA_DUMMY, AURA_EFFECT_HANDLE_REAL);
        OnEffectRemove += AuraEffectRemoveFn(spell_pri_power_of_the_dark_side::HandleOnRemove, EFFECT_0, SPELL_AURA_DUMMY, AURA_EFFECT_HANDLE_REAL);
    }
};

// 47666 - Penance (Damage)
// 373130 - Dark Reprimand (Damage)
class spell_pri_power_of_the_dark_side_damage_bonus : public SpellScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_PRIEST_POWER_OF_THE_DARK_SIDE });
    }

    void CalculateDamageBonus(SpellEffectInfo const& /*spellEffectInfo*/, Unit* /*victim*/, int32& /*damage*/, int32& /*flatMod*/, float& pctMod) const
    {
        if (AuraEffect* powerOfTheDarkSide = GetCaster()->GetAuraEffect(SPELL_PRIEST_POWER_OF_THE_DARK_SIDE, EFFECT_0))
            AddPct(pctMod, powerOfTheDarkSide->GetAmount());
    }

    void Register() override
    {
        CalcDamage += SpellCalcDamageFn(spell_pri_power_of_the_dark_side_damage_bonus::CalculateDamageBonus);
    }
};

// 47750 - Penance (Healing)
// 400187 - Dark Reprimand (Healing)
class spell_pri_power_of_the_dark_side_healing_bonus : public SpellScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_PRIEST_POWER_OF_THE_DARK_SIDE });
    }

    void CalculateHealingBonus(SpellEffectInfo const& /*spellEffectInfo*/, Unit* /*victim*/, int32& /*healing*/, int32& /*flatMod*/, float& pctMod) const
    {
        if (AuraEffect* powerOfTheDarkSide = GetCaster()->GetAuraEffect(SPELL_PRIEST_POWER_OF_THE_DARK_SIDE, EFFECT_0))
            AddPct(pctMod, powerOfTheDarkSide->GetAmount());
    }

    void Register() override
    {
        CalcHealing += SpellCalcHealingFn(spell_pri_power_of_the_dark_side_healing_bonus::CalculateHealingBonus);
    }
};

// 194509 - Power Word: Radiance
class spell_pri_power_word_radiance : public SpellScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_PRIEST_ATONEMENT_EFFECT });
    }

    void FilterTargets(std::list<WorldObject*>& targets)
    {
        Unit* explTarget = GetExplTargetUnit();

        // we must add one since explicit target is always chosen.
        uint32 maxTargets = GetEffectInfo(EFFECT_2).CalcValue(GetCaster()) + 1;

        if (targets.size() > maxTargets)
        {
            // priority is: a) no Atonement b) injured c) anything else (excluding explicit target which is always added).
            targets.sort([this, explTarget](WorldObject* lhs, WorldObject* rhs)
            {
                if (lhs == explTarget) // explTarget > anything: always true
                    return true;
                if (rhs == explTarget) // anything > explTarget: always false
                    return false;

                return MakeSortTuple(lhs) > MakeSortTuple(rhs);
            });

            targets.resize(maxTargets);
        }

        for (WorldObject* target : targets)
        {
            if (target == explTarget)
                continue;

            _visualTargets.push_back(target->GetGUID());
        }
    }

    void HandleEffectHitTarget(SpellEffIndex /*effIndex*/) const
    {
        for (ObjectGuid const& guid : _visualTargets)
            if (Unit* target = ObjectAccessor::GetUnit(*GetHitUnit(), guid))
                GetHitUnit()->SendPlaySpellVisual(target, SPELL_VISUAL_PRIEST_POWER_WORD_RADIANCE, 0, 0, 70.0f);
    }

    void Register() override
    {
        OnObjectAreaTargetSelect += SpellObjectAreaTargetSelectFn(spell_pri_power_word_radiance::FilterTargets, EFFECT_1, TARGET_UNIT_DEST_AREA_ALLY);
        OnEffectHitTarget += SpellEffectFn(spell_pri_power_word_radiance::HandleEffectHitTarget, EFFECT_0, SPELL_EFFECT_DUMMY);
    }

private:
    std::tuple<bool, bool> MakeSortTuple(WorldObject* obj) const
    {
        return std::make_tuple(IsUnitWithNoAtonement(obj), IsUnitInjured(obj));
    }

    // Returns true if obj is a unit but has no atonement
    bool IsUnitWithNoAtonement(WorldObject* obj) const
    {
        Unit* unit = obj->ToUnit();
        return unit && !unit->HasAura(SPELL_PRIEST_ATONEMENT_EFFECT, GetCaster()->GetGUID());
    }

    // Returns true if obj is a unit and is injured
    static bool IsUnitInjured(WorldObject* obj)
    {
        Unit* unit = obj->ToUnit();
        return unit && !unit->IsFullHealth();
    }

    std::vector<ObjectGuid> _visualTargets;
};

// 17 - Power Word: Shield
class spell_pri_power_word_shield : public AuraScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo
        ({
            SPELL_PRIEST_STRENGTH_OF_SOUL,
            SPELL_PRIEST_STRENGTH_OF_SOUL_EFFECT,
            SPELL_PRIEST_ATONEMENT_EFFECT,
            SPELL_PRIEST_SHIELD_DISCIPLINE,
            SPELL_PRIEST_SHIELD_DISCIPLINE_EFFECT,
            SPELL_PVP_RULES_ENABLED_HARDCODED
        }) && ValidateSpellEffect({
            { SPELL_PRIEST_MASTERY_GRACE, EFFECT_0 },
            { SPELL_PRIEST_RAPTURE, EFFECT_1 },
            { SPELL_PRIEST_BENEVOLENCE, EFFECT_0 },
            { SPELL_PRIEST_DIVINE_AEGIS, EFFECT_0 }
        });
    }

    void CalculateAmount(AuraEffect const* auraEffect, int32& amount, bool& canBeRecalculated) const
    {
        canBeRecalculated = false;

        if (Unit* caster = GetCaster())
        {
            float modifiedAmount = caster->SpellBaseDamageBonusDone(GetSpellInfo()->GetSchoolMask()) * 4.638;

            if (Player* player = caster->ToPlayer())
            {
                AddPct(modifiedAmount, player->GetRatingBonusValue(CR_VERSATILITY_DAMAGE_DONE));

                // Mastery: Grace (TBD: move into DoEffectCalcDamageAndHealing hook with a new SpellScript and AuraScript).
                if (AuraEffect const* masteryGraceEffect = caster->GetAuraEffect(SPELL_PRIEST_MASTERY_GRACE, EFFECT_0))
                    if (GetUnitOwner()->HasAura(SPELL_PRIEST_ATONEMENT_EFFECT))
                        AddPct(modifiedAmount, masteryGraceEffect->GetAmount());

                switch (player->GetPrimarySpecialization())
                {
                    case ChrSpecialization::PriestDiscipline:
                        modifiedAmount *= 1.37f;
                        break;
                    case ChrSpecialization::PriestShadow:
                        modifiedAmount *= 1.25f;
                        if (caster->HasAura(SPELL_PVP_RULES_ENABLED_HARDCODED))
                            modifiedAmount *= 0.8f;
                        break;
                    default:
                        break;
                }
            }

            float critChanceDone = caster->SpellCritChanceDone(nullptr, auraEffect, GetSpellInfo()->GetSchoolMask(), GetSpellInfo()->GetAttackType());
            float critChanceTaken = GetUnitOwner()->SpellCritChanceTaken(caster, nullptr, auraEffect, GetSpellInfo()->GetSchoolMask(), critChanceDone, GetSpellInfo()->GetAttackType());

            if (roll_chance_f(critChanceTaken))
            {
                modifiedAmount *= 2;

                // Divine Aegis
                if (AuraEffect const* divineEff = caster->GetAuraEffect(SPELL_PRIEST_DIVINE_AEGIS, EFFECT_0))
                    AddPct(modifiedAmount, divineEff->GetAmount());
            }

            // Rapture talent (TBD: move into DoEffectCalcDamageAndHealing hook).
            if (AuraEffect const* raptureEffect = caster->GetAuraEffect(SPELL_PRIEST_RAPTURE, EFFECT_1))
                AddPct(modifiedAmount, raptureEffect->GetAmount());

            // Benevolence talent
            if (AuraEffect const* benevolenceEffect = caster->GetAuraEffect(SPELL_PRIEST_BENEVOLENCE, EFFECT_0))
                AddPct(modifiedAmount, benevolenceEffect->GetAmount());

            amount = modifiedAmount;
        }
    }

    void HandleOnApply(AuraEffect const* aurEff, AuraEffectHandleModes /*mode*/) const
    {
        Unit* caster = GetCaster();
        if (!caster)
            return;

        // Note: Strength of Soul PvP talent.
        if (caster->HasAura(SPELL_PRIEST_STRENGTH_OF_SOUL))
            caster->CastSpell(GetTarget(), SPELL_PRIEST_STRENGTH_OF_SOUL_EFFECT, aurEff);
    }

    void HandleOnRemove(AuraEffect const* aurEff, AuraEffectHandleModes /*mode*/) const
    {
        GetTarget()->RemoveAura(SPELL_PRIEST_STRENGTH_OF_SOUL_EFFECT);

        // Note: Shield Discipline talent.
        if (Unit* caster = GetCaster())
            if (GetTargetApplication()->GetRemoveMode() == AURA_REMOVE_BY_ENEMY_SPELL && caster->HasAura(SPELL_PRIEST_SHIELD_DISCIPLINE))
                caster->CastSpell(caster, SPELL_PRIEST_SHIELD_DISCIPLINE_EFFECT, aurEff);
    }

    void Register() override
    {
        DoEffectCalcAmount += AuraEffectCalcAmountFn(spell_pri_power_word_shield::CalculateAmount, EFFECT_0, SPELL_AURA_SCHOOL_ABSORB);
        AfterEffectApply += AuraEffectApplyFn(spell_pri_power_word_shield::HandleOnApply, EFFECT_0, SPELL_AURA_SCHOOL_ABSORB, AURA_EFFECT_HANDLE_REAL_OR_REAPPLY_MASK);
        AfterEffectRemove += AuraEffectRemoveFn(spell_pri_power_word_shield::HandleOnRemove, EFFECT_0, SPELL_AURA_SCHOOL_ABSORB, AURA_EFFECT_HANDLE_REAL);
    }
};

// 47515 - Divine Aegis
class spell_pri_divine_aegis : public AuraScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellEffect({ { SPELL_PRIEST_DIVINE_AEGIS_ABSORB, EFFECT_0 } });
    }

    static bool CheckProc(ProcEventInfo const& eventInfo)
    {
        return eventInfo.GetHealInfo() != nullptr;
    }

    static void HandleProc(AuraEffect const* aurEff, ProcEventInfo const& eventInfo)
    {
        Unit* caster = eventInfo.GetActor();
        if (!caster)
            return;

        int32 aegisAmount = CalculatePct(eventInfo.GetHealInfo()->GetHeal(), aurEff->GetAmount());

        CastSpellExtraArgs args(aurEff);
        args.SetTriggerFlags(TRIGGERED_IGNORE_CAST_IN_PROGRESS | TRIGGERED_DONT_REPORT_CAST_ERROR);
        args.AddSpellMod(SPELLVALUE_BASE_POINT0, aegisAmount);
        caster->CastSpell(eventInfo.GetProcTarget(), SPELL_PRIEST_DIVINE_AEGIS_ABSORB, args);
    }

    void Register() override
    {
        OnEffectProc += AuraEffectProcFn(spell_pri_divine_aegis::HandleProc, EFFECT_0, SPELL_AURA_DUMMY);
    }
};

// 129250 - Power Word: Solace
class spell_pri_power_word_solace : public SpellScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_PRIEST_POWER_WORD_SOLACE_ENERGIZE });
    }

    void RestoreMana(SpellEffIndex /*effIndex*/) const
    {
        GetCaster()->CastSpell(GetCaster(), SPELL_PRIEST_POWER_WORD_SOLACE_ENERGIZE,
            CastSpellExtraArgs(TRIGGERED_IGNORE_CAST_IN_PROGRESS).SetTriggeringSpell(GetSpell())
                .AddSpellMod(SPELLVALUE_BASE_POINT0, GetEffectValue() / 100));
    }

    void Register() override
    {
        OnEffectLaunch += SpellEffectFn(spell_pri_power_word_solace::RestoreMana, EFFECT_1, SPELL_EFFECT_DUMMY);
    }
};

// Base class used by various Prayer of Mending spells
class spell_pri_prayer_of_mending_SpellScriptBase : public SpellScript
{
public:
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_PRIEST_PRAYER_OF_MENDING_HEAL, SPELL_PRIEST_PRAYER_OF_MENDING_AURA })
            && ValidateSpellEffect({ { SPELL_PRIEST_PRAYER_OF_MENDING_HEAL, EFFECT_0 } });
    }

    bool Load() override
    {
        _spellInfoHeal = sSpellMgr->AssertSpellInfo(SPELL_PRIEST_PRAYER_OF_MENDING_HEAL, DIFFICULTY_NONE);
        return true;
    }

    void CastPrayerOfMendingAura(Unit* caster, Unit* target, Unit* visualSender, uint8 stack, bool firstCast) const
    {
        CastSpellExtraArgs args;
        args.TriggerFlags = TRIGGERED_FULL_MASK;
        args.AddSpellMod(SPELLVALUE_AURA_STACK, stack);

        // Note: this line's purpose is to show the correct amount in Points field in SMSG_AURA_UPDATE.
        SpellEffectInfo const* healEffectDummy = &_spellInfoHeal->GetEffect(EFFECT_0);
        uint32 basePoints = caster->SpellHealingBonusDone(target, _spellInfoHeal, healEffectDummy->CalcValue(caster), HEAL, *healEffectDummy);
        args.AddSpellMod(SPELLVALUE_BASE_POINT0, basePoints);

        // Note: Focused Mending talent.
        args.SetCustomArg(firstCast);

        caster->CastSpell(target, SPELL_PRIEST_PRAYER_OF_MENDING_AURA, args);

        // Note: the visualSender is the priest if it is first cast or the aura holder when the aura triggers.
        visualSender->SendPlaySpellVisual(target, SPELL_VISUAL_PRIEST_PRAYER_OF_MENDING, 0, 0, 40.0f);
    }

protected:
    SpellInfo const* _spellInfoHeal = nullptr;
};

// 33076 - Prayer of Mending (Dummy)
class spell_pri_prayer_of_mending_dummy : public spell_pri_prayer_of_mending_SpellScriptBase
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo
        ({
            SPELL_PRIEST_PRAYER_OF_MENDING_AURA,
            SPELL_PRIEST_EPIPHANY,
            SPELL_PRIEST_EPIPHANY_HIGHLIGHT
        });
    }

    void HandleEffectDummy(SpellEffIndex /*effIndex*/) const
    {
        Unit* caster = GetCaster();
        Unit* target = GetHitUnit();

        // Note: we need to increase BasePoints by 1 since it's 4 as default. Also HACKFIX, we shouldn't reduce it by 1 if the target has the aura already.
        uint8 stackAmount = target->HasAura(SPELL_PRIEST_PRAYER_OF_MENDING_AURA, caster->GetGUID()) ? GetEffectValue() : GetEffectValue() + 1;

        CastPrayerOfMendingAura(caster, target, caster, stackAmount, true);

        // Note: Epiphany talent.
        if (caster->HasAura(SPELL_PRIEST_EPIPHANY))
            caster->RemoveAurasDueToSpell(SPELL_PRIEST_EPIPHANY_HIGHLIGHT);
    }

    void Register() override
    {
        OnEffectHitTarget += SpellEffectFn(spell_pri_prayer_of_mending_dummy::HandleEffectDummy, EFFECT_0, SPELL_EFFECT_DUMMY);
    }
};

// 41635 - Prayer of Mending (Aura)
class spell_pri_prayer_of_mending_aura : public AuraScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_PRIEST_PRAYER_OF_MENDING_HEAL, SPELL_PRIEST_PRAYER_OF_MENDING_JUMP })
            && ValidateSpellEffect({ { SPELL_PRIEST_SAY_YOUR_PRAYERS, EFFECT_0 } });
    }

    void HandleHeal(AuraEffect const* aurEff, ProcEventInfo const& /*eventInfo*/)
    {
        // Note: caster is the priest who cast the spell and target is current holder of the aura.
        Unit* target = GetTarget();

        if (Unit* caster = GetCaster())
        {
            CastSpellExtraArgs args(aurEff);
            args.SetCustomArg(_isEmpoweredByFocusedMending);

            caster->CastSpell(target, SPELL_PRIEST_PRAYER_OF_MENDING_HEAL, args);

            // Note: jump is only executed if higher than 1 stack.
            int32 stackAmount = GetStackAmount();
            if (stackAmount > 1)
            {
                args.OriginalCaster = caster->GetGUID();

                int32 newStackAmount = stackAmount - 1;
                if (AuraEffect* sayYourPrayers = caster->GetAuraEffect(SPELL_PRIEST_SAY_YOUR_PRAYERS, EFFECT_0))
                    if (roll_chance_i(sayYourPrayers->GetAmount()))
                        ++newStackAmount;

                args.AddSpellMod(SPELLVALUE_BASE_POINT0, newStackAmount);

                target->CastSpell(target, SPELL_PRIEST_PRAYER_OF_MENDING_JUMP, args);
            }

            Remove();
        }
    }

    void Register() override
    {
        OnEffectProc += AuraEffectProcFn(spell_pri_prayer_of_mending_aura::HandleHeal, EFFECT_0, SPELL_AURA_DUMMY);
    }

public:
    void SetEmpoweredByFocusedMending(bool isEmpowered)
    {
        _isEmpoweredByFocusedMending = isEmpowered;
    }

private:
    bool _isEmpoweredByFocusedMending = false;
};

class spell_pri_prayer_of_mending : public SpellScript
{
    void HandleEffectDummy(SpellEffIndex /*effIndex*/) const
    {
        Aura* aura = GetHitAura();
        if (!aura)
            return;

        spell_pri_prayer_of_mending_aura* script = aura->GetScript<spell_pri_prayer_of_mending_aura>();
        if (!script)
            return;

        if (bool const* isEmpoweredByFocusedMending = std::any_cast<bool>(&GetSpell()->m_customArg))
            script->SetEmpoweredByFocusedMending(isEmpoweredByFocusedMending);
    }

    void Register() override
    {
        OnEffectHitTarget += SpellEffectFn(spell_pri_prayer_of_mending::HandleEffectDummy, EFFECT_0, SPELL_EFFECT_APPLY_AURA);
    }
};

// 155793 - Prayer of Mending (Jump)
class spell_pri_prayer_of_mending_jump : public spell_pri_prayer_of_mending_SpellScriptBase
{
    static void FilterTargets(std::list<WorldObject*>& targets)
    {
        Trinity::SelectRandomInjuredTargets(targets, 1, true);
    }

    void HandleJump(SpellEffIndex /*effIndex*/) const
    {
        if (Unit* origCaster = GetOriginalCaster())
            CastPrayerOfMendingAura(origCaster, GetHitUnit(), GetCaster(), GetEffectValue(), false);
    }

    void Register() override
    {
        OnObjectAreaTargetSelect += SpellObjectAreaTargetSelectFn(spell_pri_prayer_of_mending_jump::FilterTargets, EFFECT_0, TARGET_UNIT_SRC_AREA_ALLY);
        OnEffectHitTarget += SpellEffectFn(spell_pri_prayer_of_mending_jump::HandleJump, EFFECT_0, SPELL_EFFECT_DUMMY);
    }
};

// 193063 - Protective Light (Aura)
class spell_pri_protective_light : public AuraScript
{
    bool CheckEffectProc(AuraEffect const* /*aurEff*/, ProcEventInfo const& eventInfo) const
    {
        return eventInfo.GetProcTarget() == GetCaster();
    }

    void HandleEffectProc(AuraEffect const* aurEff, ProcEventInfo& /*eventInfo*/) const
    {
        GetCaster()->CastSpell(GetCaster(), SPELL_PRIEST_PROTECTIVE_LIGHT_AURA, aurEff);
    }

    void Register() override
    {
        DoCheckEffectProc += AuraCheckEffectProcFn(spell_pri_protective_light::CheckEffectProc, EFFECT_0, SPELL_AURA_DUMMY);
        OnEffectProc += AuraEffectProcFn(spell_pri_protective_light::HandleEffectProc, EFFECT_0, SPELL_AURA_DUMMY);
    }
};

// 405554 - Priest Holy 10.1 Class Set 2pc
class spell_pri_holy_10_1_class_set_2pc : public AuraScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_PRIEST_HOLY_10_1_CLASS_SET_2P_CHOOSER })
            && ValidateSpellEffect({ { SPELL_PRIEST_PRAYER_OF_MENDING, EFFECT_0 } });
    }

    static bool CheckProc(AuraEffect const* aurEff, ProcEventInfo& /*eventInfo*/)
    {
        return roll_chance_i(aurEff->GetAmount());
    }

    void HandleProc(AuraEffect const* aurEff, ProcEventInfo const& eventInfo) const
    {
        CastSpellExtraArgs args(aurEff);
        args.SetTriggeringSpell(eventInfo.GetProcSpell());
        args.AddSpellMod(SPELLVALUE_BASE_POINT0, sSpellMgr->AssertSpellInfo(SPELL_PRIEST_PRAYER_OF_MENDING, GetCastDifficulty())->GetEffect(EFFECT_0).CalcValue(GetCaster()));

        GetTarget()->CastSpell(GetTarget(), SPELL_PRIEST_HOLY_10_1_CLASS_SET_2P_CHOOSER, args);
    }

    void Register() override
    {
        DoCheckEffectProc += AuraCheckEffectProcFn(spell_pri_holy_10_1_class_set_2pc::CheckProc, EFFECT_0, SPELL_AURA_DUMMY);
        OnEffectProc += AuraEffectProcFn(spell_pri_holy_10_1_class_set_2pc::HandleProc, EFFECT_0, SPELL_AURA_DUMMY);
    }
};

// 411097 - Priest Holy 10.1 Class Set 2pc (Chooser)
class spell_pri_holy_10_1_class_set_2pc_chooser : public spell_pri_prayer_of_mending_SpellScriptBase
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_PRIEST_PRAYER_OF_MENDING_AURA });
    }

    static void FilterTargets(std::list<WorldObject*>& targets)
    {
        Trinity::SelectRandomInjuredTargets(targets, 1, true);
    }

    void HandleEffectDummy(SpellEffIndex /*effIndex*/) const
    {
        Unit* caster = GetCaster();
        Unit* target = GetHitUnit();

        // Note: we need to increase BasePoints by 1 since it's 4 as default. Also HACKFIX, we shouldn't reduce it by 1 if the target has the aura already.
        uint8 stackAmount = target->HasAura(SPELL_PRIEST_PRAYER_OF_MENDING_AURA, caster->GetGUID()) ? GetEffectValue() : GetEffectValue() + 1;

        CastPrayerOfMendingAura(caster, target, caster, stackAmount, true);
    }

    void Register() override
    {
        OnObjectAreaTargetSelect += SpellObjectAreaTargetSelectFn(spell_pri_holy_10_1_class_set_2pc_chooser::FilterTargets, EFFECT_0, TARGET_UNIT_DEST_AREA_ENTRY);
        OnEffectHitTarget += SpellEffectFn(spell_pri_holy_10_1_class_set_2pc_chooser::HandleEffectDummy, EFFECT_0, SPELL_EFFECT_DUMMY);
    }
};

// 155793 - Prayer of Mending (Jump)
class spell_pri_holy_10_1_class_set_4pc : public SpellScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_PRIEST_HOLY_10_1_CLASS_SET_4P, SPELL_PRIEST_HOLY_10_1_CLASS_SET_4P_EFFECT });
    }

    void HandleEffectDummy(SpellEffIndex /*effIndex*/) const
    {
        if (GetOriginalCaster()->HasAura(SPELL_PRIEST_HOLY_10_1_CLASS_SET_4P))
            GetOriginalCaster()->CastSpell(GetOriginalCaster(), SPELL_PRIEST_HOLY_10_1_CLASS_SET_4P_EFFECT, TRIGGERED_IGNORE_GCD);
    }

    void Register() override
    {
        OnEffectHitTarget += SpellEffectFn(spell_pri_holy_10_1_class_set_4pc::HandleEffectDummy, EFFECT_0, SPELL_EFFECT_DUMMY);
    }
};

// 41635 - Prayer of Mending (Aura)
class spell_pri_holy_10_1_class_set_4pc_aura : public AuraScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_PRIEST_HOLY_10_1_CLASS_SET_4P, SPELL_PRIEST_HOLY_10_1_CLASS_SET_4P_EFFECT });
    }

    void HandleOnRemove(AuraEffect const* aurEff, AuraEffectHandleModes /*mode*/) const
    {
        if (GetTargetApplication()->GetRemoveMode() != AURA_REMOVE_BY_EXPIRE)
            return;

        if (GetCaster()->HasAura(SPELL_PRIEST_HOLY_10_1_CLASS_SET_4P))
            GetCaster()->CastSpell(GetCaster(), SPELL_PRIEST_HOLY_10_1_CLASS_SET_4P_EFFECT, CastSpellExtraArgs(TRIGGERED_IGNORE_GCD).SetTriggeringAura(aurEff));
    }

    void Register() override
    {
        OnEffectRemove += AuraEffectRemoveFn(spell_pri_holy_10_1_class_set_4pc_aura::HandleOnRemove, EFFECT_0, SPELL_AURA_DUMMY, AURA_EFFECT_HANDLE_REAL);
    }
};

// 17 - Power Word: Shield
class spell_pri_assured_safety : public spell_pri_prayer_of_mending_SpellScriptBase
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellEffect({ { SPELL_PRIEST_ASSURED_SAFETY, EFFECT_0 } });
    }

    bool Load() override
    {
        return GetCaster()->HasAura(SPELL_PRIEST_ASSURED_SAFETY);
    }

    void HandleEffectHitTarget(SpellEffIndex /*effIndex*/) const
    {
        Unit* caster = GetCaster();
        if (AuraEffect const* effect = caster->GetAuraEffect(SPELL_PRIEST_ASSURED_SAFETY, EFFECT_0))
            CastPrayerOfMendingAura(caster, GetHitUnit(), caster, effect->GetAmount(), false);
    }

    void Register() override
    {
        OnEffectHitTarget += SpellEffectFn(spell_pri_assured_safety::HandleEffectHitTarget, EFFECT_0, SPELL_EFFECT_APPLY_AURA);
    }
};

// 204197 - Purge the Wicked
// Called by Penance - 47540, Dark Reprimand - 400169
class spell_pri_purge_the_wicked : public SpellScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo
        ({
            SPELL_PRIEST_PURGE_THE_WICKED_PERIODIC,
            SPELL_PRIEST_PURGE_THE_WICKED_DUMMY
        });
    }

    void HandleDummy(SpellEffIndex /*effIndex*/) const
    {
        Unit* caster = GetCaster();
        Unit* target = GetHitUnit();

        if (target->HasAura(SPELL_PRIEST_PURGE_THE_WICKED_PERIODIC, caster->GetGUID()))
            caster->CastSpell(target, SPELL_PRIEST_PURGE_THE_WICKED_DUMMY, TRIGGERED_IGNORE_GCD | TRIGGERED_IGNORE_CAST_IN_PROGRESS);
    }

    void Register() override
    {
        OnEffectHitTarget += SpellEffectFn(spell_pri_purge_the_wicked::HandleDummy, EFFECT_0, SPELL_EFFECT_DUMMY);
    }
};

// 204215 - Purge the Wicked
class spell_pri_purge_the_wicked_dummy : public SpellScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_PRIEST_PURGE_THE_WICKED_PERIODIC, SPELL_PRIEST_REVEL_IN_PURITY })
            && ValidateSpellEffect({ { SPELL_PRIEST_REVEL_IN_PURITY, EFFECT_1 } });
    }

    void FilterTargets(std::list<WorldObject*>& targets) const
    {
        Unit* caster = GetCaster();
        Unit* explTarget = GetExplTargetUnit();

        targets.remove_if([&](WorldObject* object) -> bool
        {
            // Note: we must remove any non-unit target, the explicit target and any other target that may be under any crowd control aura.
            Unit* target = object->ToUnit();
            return !target || target == explTarget || target->HasBreakableByDamageCrowdControlAura();
        });

        if (targets.empty())
            return;

        // Note: there's no SPELL_EFFECT_DUMMY with BasePoints 1 in any of the spells related to use as reference so we hardcode the value.
        uint32 spreadCount = 1;

        // Note: we must sort our list of targets whose priority is 1) aura, 2) distance, and 3) duration.
        targets.sort([&](WorldObject const* lhs, WorldObject const* rhs) -> bool
        {
            Unit const* targetA = lhs->ToUnit();
            Unit const* targetB = rhs->ToUnit();

            Aura* auraA = targetA->GetAura(SPELL_PRIEST_PURGE_THE_WICKED_PERIODIC, caster->GetGUID());
            Aura* auraB = targetB->GetAura(SPELL_PRIEST_PURGE_THE_WICKED_PERIODIC, caster->GetGUID());

            if (!auraA)
            {
                if (auraB)
                    return true;
                return explTarget->GetExactDist(targetA) < explTarget->GetExactDist(targetB);
            }
            if (!auraB)
                return false;

            return auraA->GetDuration() < auraB->GetDuration();
        });

        // Note: Revel in Purity talent.
        if (caster->HasAura(SPELL_PRIEST_REVEL_IN_PURITY))
            spreadCount += sSpellMgr->AssertSpellInfo(SPELL_PRIEST_REVEL_IN_PURITY, DIFFICULTY_NONE)->GetEffect(EFFECT_1).CalcValue(GetCaster());

        if (targets.size() > spreadCount)
            targets.resize(spreadCount);
    }

    void HandleDummy(SpellEffIndex /*effIndex*/) const
    {
        Unit* caster = GetCaster();
        Unit* target = GetHitUnit();

        caster->CastSpell(target, SPELL_PRIEST_PURGE_THE_WICKED_PERIODIC, TRIGGERED_IGNORE_GCD | TRIGGERED_IGNORE_CAST_IN_PROGRESS);
    }

    void Register() override
    {
        OnObjectAreaTargetSelect += SpellObjectAreaTargetSelectFn(spell_pri_purge_the_wicked_dummy::FilterTargets, EFFECT_1, TARGET_UNIT_DEST_AREA_ENEMY);
        OnEffectHitTarget += SpellEffectFn(spell_pri_purge_the_wicked_dummy::HandleDummy, EFFECT_1, SPELL_EFFECT_DUMMY);
    }
};

// 390622 - Rhapsody
class spell_pri_rhapsody : public AuraScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_PRIEST_RHAPSODY_PROC });
    }

    void HandlePeriodic(AuraEffect const* aurEff) const
    {
        Unit* target = GetTarget();
        if (Aura* rhapsodyStack = target->GetAura(SPELL_PRIEST_RHAPSODY_PROC, GetCasterGUID()))
            rhapsodyStack->ModStackAmount(1);
        else
            target->CastSpell(target, SPELL_PRIEST_RHAPSODY_PROC,
                CastSpellExtraArgs(aurEff).SetTriggerFlags(TRIGGERED_IGNORE_CAST_IN_PROGRESS | TRIGGERED_DONT_REPORT_CAST_ERROR));
    }

    void Register() override
    {
        OnEffectPeriodic += AuraEffectPeriodicFn(spell_pri_rhapsody::HandlePeriodic, EFFECT_0, SPELL_AURA_PERIODIC_DUMMY);
    }
};

// 390636 - Rhapsody
class spell_pri_rhapsody_proc : public AuraScript
{
    void PreventChargeDrop(ProcEventInfo const& /*eventInfo*/)
    {
        PreventDefaultAction();
    }

    void RemoveAura(ProcEventInfo const& /*eventInfo*/) const
    {
        // delay charge drop to allow spellmod to be applied to both damaging and healing spells
        GetAura()->DropChargeDelayed(1);
    }

    void Register() override
    {
        DoPrepareProc += AuraProcFn(spell_pri_rhapsody_proc::PreventChargeDrop);
        AfterProc += AuraProcFn(spell_pri_rhapsody_proc::RemoveAura);
    }
};

// 47536 - Rapture
class spell_pri_rapture : public SpellScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_PRIEST_POWER_WORD_SHIELD });
    }

    void HandleEffectDummy(SpellEffIndex /*effIndex*/)
    {
        _raptureTarget = GetHitUnit()->GetGUID();
    }

    void HandleAfterCast() const
    {
        Unit* caster = GetCaster();

        if (Unit* target = ObjectAccessor::GetUnit(*caster, _raptureTarget))
            caster->CastSpell(target, SPELL_PRIEST_POWER_WORD_SHIELD,
                CastSpellExtraArgs(TRIGGERED_IGNORE_GCD | TRIGGERED_IGNORE_POWER_COST | TRIGGERED_IGNORE_CAST_IN_PROGRESS)
                .SetTriggeringSpell(GetSpell()));
    }

    void Register() override
    {
        OnEffectHitTarget += SpellEffectFn(spell_pri_rapture::HandleEffectDummy, EFFECT_0, SPELL_EFFECT_DUMMY);
        AfterCast += SpellCastFn(spell_pri_rapture::HandleAfterCast);
    }

private:
    ObjectGuid _raptureTarget;
};

// 8092 - Mind Blast
class spell_pri_schism : public SpellScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo
        ({
            SPELL_PRIEST_SCHISM,
            SPELL_PRIEST_SCHISM_AURA
        });
    }

    void HandleEffectHitTarget(SpellEffIndex /*effIndex*/) const
    {
        if (GetCaster()->HasAura(SPELL_PRIEST_SCHISM))
            GetCaster()->CastSpell(GetHitUnit(), SPELL_PRIEST_SCHISM_AURA, true);
    }

    void Register() override
    {
        OnEffectHitTarget += SpellEffectFn(spell_pri_schism::HandleEffectHitTarget, EFFECT_0, SPELL_EFFECT_SCHOOL_DAMAGE);
    }
};

// 208771 - Sanctuary (Absorb)
class spell_pri_sanctuary_absorb : public AuraScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_PRIEST_SANCTUARY_AURA });
    }

    void CalcAbsorbAmount(AuraEffect const* /*aurEff*/, DamageInfo& dmgInfo, uint32& /*absorbAmount*/)
    {
        PreventDefaultAction();

        Unit const* attacker = dmgInfo.GetAttacker();
        if (!attacker)
            return;

        AuraEffect* amountHolderEffect = attacker->GetAuraEffect(SPELL_PRIEST_SANCTUARY_AURA, EFFECT_0, GetCasterGUID());
        if (!amountHolderEffect)
            return;

        if (dmgInfo.GetDamage() >= uint32(amountHolderEffect->GetAmount()))
        {
            amountHolderEffect->GetBase()->Remove(AURA_REMOVE_BY_ENEMY_SPELL);
            dmgInfo.AbsorbDamage(amountHolderEffect->GetAmount());
        }
        else
        {
            amountHolderEffect->ChangeAmount(amountHolderEffect->GetAmount() - int32(dmgInfo.GetDamage()));
            dmgInfo.AbsorbDamage(dmgInfo.GetDamage());
        }
    }

    void Register() override
    {
        OnEffectAbsorb += AuraEffectAbsorbFn(spell_pri_sanctuary_absorb::CalcAbsorbAmount, EFFECT_0);
    }
};

// Smite - 585
class spell_pri_sanctuary_trigger : public SpellScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_PRIEST_SANCTUARY, SPELL_PRIEST_SANCTUARY_AURA, SPELL_PRIEST_SANCTUARY_ABSORB });
    }

    void HandleEffectHit(SpellEffIndex /*effIndex*/) const
    {
        Player* caster = Object::ToPlayer(GetCaster());
        if (!caster)
            return;

        if (AuraEffect const* sanctuaryEffect = caster->GetAuraEffect(SPELL_PRIEST_SANCTUARY, EFFECT_0))
        {
            if (Unit* target = GetHitUnit())
            {
                float absorbAmount = CalculatePct<float, float>(caster->SpellBaseDamageBonusDone(SPELL_SCHOOL_MASK_SHADOW), sanctuaryEffect->GetAmount());
                AddPct(absorbAmount, caster->GetRatingBonusValue(CR_VERSATILITY_DAMAGE_DONE));

                caster->CastSpell(caster, SPELL_PRIEST_SANCTUARY_ABSORB, CastSpellExtraArgs()
                    .SetTriggerFlags(TRIGGERED_IGNORE_CAST_IN_PROGRESS | TRIGGERED_DONT_REPORT_CAST_ERROR)
                    .SetTriggeringSpell(GetSpell()));

                caster->CastSpell(target, SPELL_PRIEST_SANCTUARY_AURA, CastSpellExtraArgs()
                    .AddSpellMod(SPELLVALUE_BASE_POINT0, absorbAmount)
                    .SetTriggerFlags(TRIGGERED_IGNORE_CAST_IN_PROGRESS | TRIGGERED_DONT_REPORT_CAST_ERROR)
                    .SetTriggeringSpell(GetSpell()));
            }
        }
    }

    void Register() override
    {
        OnEffectHitTarget += SpellEffectFn(spell_pri_sanctuary_trigger::HandleEffectHit, EFFECT_0, SPELL_EFFECT_SCHOOL_DAMAGE);
    }
};

// 280391 - Sins of the Many
class spell_pri_sins_of_the_many : public AuraScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_PRIEST_SINS_OF_THE_MANY });
    }

    void HandleOnApply(AuraEffect const* /*aurEff*/, AuraEffectHandleModes /*mode*/) const
    {
        GetTarget()->CastSpell(GetTarget(), SPELL_PRIEST_SINS_OF_THE_MANY, true);
    }

    void HandleOnRemove(AuraEffect const* /*aurEff*/, AuraEffectHandleModes /*mode*/) const
    {
        GetTarget()->RemoveAura(SPELL_PRIEST_SINS_OF_THE_MANY);
    }

    void Register() override
    {
        OnEffectApply += AuraEffectApplyFn(spell_pri_sins_of_the_many::HandleOnApply, EFFECT_0, SPELL_AURA_DUMMY, AURA_EFFECT_HANDLE_REAL);
        OnEffectRemove += AuraEffectRemoveFn(spell_pri_sins_of_the_many::HandleOnRemove, EFFECT_0, SPELL_AURA_DUMMY, AURA_EFFECT_HANDLE_REAL);
    }
};

// 20711 - Spirit of Redemption
class spell_pri_spirit_of_redemption : public AuraScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_PRIEST_SPIRIT_OF_REDEMPTION });
    }

    void HandleAbsorb(AuraEffect const* aurEff, DamageInfo const& /*dmgInfo*/, uint32 const& /*absorbAmount*/) const
    {
        Unit* target = GetTarget();
        target->CastSpell(target, SPELL_PRIEST_SPIRIT_OF_REDEMPTION, aurEff);
        target->SetFullHealth();
    }

    void Register() override
    {
        OnEffectAbsorb += AuraEffectAbsorbOverkillFn(spell_pri_spirit_of_redemption::HandleAbsorb, EFFECT_0);
    }
};

// 34433 - Shadowfiend
// 123040 - Mindbender (Discipline)
// 451235 - Voidwrath
class spell_pri_shadow_covenant : public SpellScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_PRIEST_SHADOW_COVENANT, SPELL_PRIEST_SHADOW_COVENANT_EFFECT });
    }

    bool Load() override
    {
        return GetCaster()->HasAura(SPELL_PRIEST_SHADOW_COVENANT);
    }

    void HandleAfterCast() const
    {
        GetCaster()->CastSpell(GetCaster(), SPELL_PRIEST_SHADOW_COVENANT_EFFECT, CastSpellExtraArgsInit{
            .TriggerFlags = TRIGGERED_IGNORE_CAST_IN_PROGRESS | TRIGGERED_DONT_REPORT_CAST_ERROR,
            .TriggeringSpell = GetSpell()
        });
    }

    void Register() override
    {
        AfterCast += SpellCastFn(spell_pri_shadow_covenant::HandleAfterCast);
    }
};

// 186263 - Shadow Mend
class spell_pri_shadow_mend : public SpellScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo
        ({
            SPELL_PRIEST_ATONEMENT,
            SPELL_PRIEST_ATONEMENT_EFFECT,
            SPELL_PRIEST_TRINITY,
            SPELL_PRIEST_MASOCHISM_TALENT,
            SPELL_PRIEST_MASOCHISM_PERIODIC_HEAL,
            SPELL_PRIEST_SHADOW_MEND_PERIODIC_DUMMY
        });
    }

    void HandleEffectHit() const
    {
        if (Unit* target = GetHitUnit())
        {
            Unit* caster = GetCaster();

            int32 periodicAmount = GetHitHeal() / 20;
            int32 damageForAuraRemoveAmount = periodicAmount * 10;

            // Handle Masochism talent
            if (caster->HasAura(SPELL_PRIEST_MASOCHISM_TALENT) && caster->GetGUID() == target->GetGUID())
                caster->CastSpell(caster, SPELL_PRIEST_MASOCHISM_PERIODIC_HEAL, CastSpellExtraArgs(GetSpell()).AddSpellMod(SPELLVALUE_BASE_POINT0, periodicAmount));
            else if (target->IsInCombat() && periodicAmount)
            {
                CastSpellExtraArgs args(TRIGGERED_FULL_MASK);
                args.SetTriggeringSpell(GetSpell());
                args.AddSpellMod(SPELLVALUE_BASE_POINT0, periodicAmount);
                args.AddSpellMod(SPELLVALUE_BASE_POINT1, damageForAuraRemoveAmount);
                caster->CastSpell(target, SPELL_PRIEST_SHADOW_MEND_PERIODIC_DUMMY, args);
            }
        }
    }

    void Register() override
    {
        AfterHit += SpellHitFn(spell_pri_shadow_mend::HandleEffectHit);
    }
};

// 187464 - Shadow Mend (Damage)
class spell_pri_shadow_mend_periodic_damage : public AuraScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_PRIEST_SHADOW_MEND_DAMAGE });
    }

    void HandleDummyTick(AuraEffect const* aurEff) const
    {
        CastSpellExtraArgs args(TRIGGERED_FULL_MASK);
        args.SetOriginalCaster(GetCasterGUID());
        args.SetTriggeringAura(aurEff);
        args.AddSpellMod(SPELLVALUE_BASE_POINT0, aurEff->GetAmount());
        GetTarget()->CastSpell(GetTarget(), SPELL_PRIEST_SHADOW_MEND_DAMAGE, args);
    }

    static bool CheckProc(ProcEventInfo const& eventInfo)
    {
        return eventInfo.GetDamageInfo() != nullptr;
    }

    void HandleProc(AuraEffect* aurEff, ProcEventInfo const& eventInfo)
    {
        int32 newAmount = aurEff->GetAmount() - eventInfo.GetDamageInfo()->GetDamage();

        aurEff->ChangeAmount(newAmount);
        if (newAmount < 0)
            Remove();
    }

    void Register() override
    {
        OnEffectPeriodic += AuraEffectPeriodicFn(spell_pri_shadow_mend_periodic_damage::HandleDummyTick, EFFECT_0, SPELL_AURA_PERIODIC_DUMMY);
        DoCheckProc += AuraCheckProcFn(spell_pri_shadow_mend_periodic_damage::CheckProc);
        OnEffectProc += AuraEffectProcFn(spell_pri_shadow_mend_periodic_damage::HandleProc, EFFECT_1, SPELL_AURA_DUMMY);
    }
};

// 32379 - Shadow Word: Death
class spell_pri_shadow_word_death : public SpellScript
{
    static constexpr Seconds BACKLASH_DELAY = 1s;

    bool Validate(SpellInfo const* spellInfo) override
    {
        return ValidateSpellInfo({ SPELL_PRIEST_SHADOW_WORD_DEATH_DAMAGE })
            && ValidateSpellEffect({ { spellInfo->Id, EFFECT_5 } })
            && spellInfo->GetEffect(EFFECT_2).IsEffect(SPELL_EFFECT_DUMMY)
            && spellInfo->GetEffect(EFFECT_3).IsEffect(SPELL_EFFECT_SCRIPT_EFFECT)
            && spellInfo->GetEffect(EFFECT_5).IsEffect(SPELL_EFFECT_DUMMY);
    }

    void HandleDamageCalculation(SpellEffectInfo const& /*spellEffectInfo*/, Unit const* victim, int32 const& /*damage*/, int32 const& /*flatMod*/, float& pctMod) const
    {
        if (victim->HealthBelowPct(GetEffectInfo(EFFECT_2).CalcValue(GetCaster())))
            AddPct(pctMod, GetEffectInfo(EFFECT_3).CalcValue(GetCaster()));
    }

    void DetermineKillStatus(DamageInfo const& damageInfo, uint32& /*resistAmount*/, int32& /*absorbAmount*/) const
    {
        bool killed = damageInfo.GetDamage() >= damageInfo.GetVictim()->GetHealth();
        if (!killed)
        {
            Unit* caster = GetCaster();
            int32 backlashDamage = caster->CountPctFromMaxHealth(GetEffectInfo(EFFECT_5).CalcValue(caster));
            caster->m_Events.AddEventAtOffset([caster, originalCastId = GetSpell()->m_castId, backlashDamage]
            {
                caster->CastSpell(caster, SPELL_PRIEST_SHADOW_WORD_DEATH_DAMAGE, CastSpellExtraArgs()
                    .SetTriggerFlags(TRIGGERED_IGNORE_CAST_IN_PROGRESS | TRIGGERED_DONT_REPORT_CAST_ERROR)
                    .SetOriginalCastId(originalCastId)
                    .AddSpellMod(SPELLVALUE_BASE_POINT0, backlashDamage));

            }, BACKLASH_DELAY);
        }
    }

    void Register() override
    {
        CalcDamage += SpellCalcDamageFn(spell_pri_shadow_word_death::HandleDamageCalculation);

        // abuse OnCalculateResistAbsorb to determine if this spell will kill target or not (its still not perfect - happens before absorbs are applied)
        OnCalculateResistAbsorb += SpellOnResistAbsorbCalculateFn(spell_pri_shadow_word_death::DetermineKillStatus);
    }
};

// 109186 - Surge of Light
class spell_pri_surge_of_light : public AuraScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo
        ({
            SPELL_PRIEST_SMITE,
            SPELL_PRIEST_SURGE_OF_LIGHT_EFFECT
        });
    }

    static bool CheckProc(ProcEventInfo const& eventInfo)
    {
        if (eventInfo.GetSpellInfo()->Id == SPELL_PRIEST_SMITE)
            return true;

        if (eventInfo.GetSpellInfo()->SpellFamilyName == SPELLFAMILY_PRIEST)
            return eventInfo.GetHealInfo();

        return false;
    }

    void HandleEffectProc(AuraEffect const* aurEff, ProcEventInfo& /*eventInfo*/) const
    {
        if (roll_chance_i(aurEff->GetAmount()))
            GetTarget()->CastSpell(GetTarget(), SPELL_PRIEST_SURGE_OF_LIGHT_EFFECT, aurEff);
    }

    void Register() override
    {
        DoCheckProc += AuraCheckProcFn(spell_pri_surge_of_light::CheckProc);
        OnEffectProc += AuraEffectProcFn(spell_pri_surge_of_light::HandleEffectProc, EFFECT_0, SPELL_AURA_DUMMY);
    }
};

// 28809 - Greater Heal
class spell_pri_t3_4p_bonus : public AuraScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_PRIEST_ARMOR_OF_FAITH });
    }

    void HandleProc(AuraEffect const* aurEff, ProcEventInfo const& eventInfo)
    {
        PreventDefaultAction();
        eventInfo.GetActor()->CastSpell(eventInfo.GetProcTarget(), SPELL_PRIEST_ARMOR_OF_FAITH, aurEff);
    }

    void Register() override
    {
        OnEffectProc += AuraEffectProcFn(spell_pri_t3_4p_bonus::HandleProc, EFFECT_0, SPELL_AURA_DUMMY);
    }
};

// 37594 - Greater Heal Refund
class spell_pri_t5_heal_2p_bonus : public AuraScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_PRIEST_ITEM_EFFICIENCY });
    }

    static bool CheckProc(ProcEventInfo const& eventInfo)
    {
        if (HealInfo* healInfo = eventInfo.GetHealInfo())
            if (Unit* healTarget = healInfo->GetTarget())
                if (healInfo->GetEffectiveHeal())
                    if (healTarget->GetHealth() >= healTarget->GetMaxHealth())
                        return true;

        return false;
    }

    void HandleProc(AuraEffect const* aurEff, ProcEventInfo& /*eventInfo*/)
    {
        PreventDefaultAction();
        GetTarget()->CastSpell(GetTarget(), SPELL_PRIEST_ITEM_EFFICIENCY, aurEff);
    }

    void Register() override
    {
        DoCheckProc += AuraCheckProcFn(spell_pri_t5_heal_2p_bonus::CheckProc);
        OnEffectProc += AuraEffectProcFn(spell_pri_t5_heal_2p_bonus::HandleProc, EFFECT_0, SPELL_AURA_PROC_TRIGGER_SPELL);
    }
};

// 70770 - Item - Priest T10 Healer 2P Bonus
class spell_pri_t10_heal_2p_bonus : public AuraScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_PRIEST_BLESSED_HEALING });
    }

    void HandleProc(AuraEffect const* aurEff, ProcEventInfo const& eventInfo)
    {
        PreventDefaultAction();

        HealInfo* healInfo = eventInfo.GetHealInfo();
        if (!healInfo || !healInfo->GetHeal())
            return;

        SpellInfo const* spellInfo = sSpellMgr->AssertSpellInfo(SPELL_PRIEST_BLESSED_HEALING, GetCastDifficulty());
        int32 amount = CalculatePct(static_cast<int32>(healInfo->GetHeal()), aurEff->GetAmount());

        ASSERT(spellInfo->GetMaxTicks() > 0);
        amount /= spellInfo->GetMaxTicks();

        Unit* caster = eventInfo.GetActor();
        Unit* target = eventInfo.GetProcTarget();

        CastSpellExtraArgs args(aurEff);
        args.AddSpellBP0(amount);
        caster->CastSpell(target, SPELL_PRIEST_BLESSED_HEALING, args);
    }

    void Register() override
    {
        OnEffectProc += AuraEffectProcFn(spell_pri_t10_heal_2p_bonus::HandleProc, EFFECT_0, SPELL_AURA_DUMMY);
    }
};

// 200128 - Trail of Light
class spell_pri_trail_of_light : public AuraScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_PRIEST_TRAIL_OF_LIGHT_HEAL });
    }

    bool CheckProc(ProcEventInfo const& eventInfo)
    {
        if (_healQueue.empty() || _healQueue.back() != eventInfo.GetActionTarget()->GetGUID())
            _healQueue.push(eventInfo.GetActionTarget()->GetGUID());

        if (_healQueue.size() > 2)
            _healQueue.pop();

        if (_healQueue.size() == 2)
            return true;

        return false;
    }

    void HandleOnProc(AuraEffect* aurEff, ProcEventInfo const& eventInfo) const
    {
        Unit* caster = GetTarget();
        Unit* oldTarget = ObjectAccessor::GetUnit(*caster, _healQueue.front());
        if (!oldTarget)
            return;

        // Note: old target may not be friendly anymore due to charm and faction change effects.
        if (!caster->IsValidAssistTarget(oldTarget))
            return;

        SpellInfo const* healSpellInfo = sSpellMgr->GetSpellInfo(SPELL_PRIEST_TRAIL_OF_LIGHT_HEAL, DIFFICULTY_NONE);
        if (!healSpellInfo)
            return;

        // Note: distance may be greater than the heal's spell range.
        if (!caster->IsWithinDist(oldTarget, healSpellInfo->GetMaxRange(true, caster)))
            return;

        uint32 healAmount = CalculatePct(eventInfo.GetHealInfo()->GetHeal(), aurEff->GetAmount());

        caster->CastSpell(oldTarget, SPELL_PRIEST_TRAIL_OF_LIGHT_HEAL, CastSpellExtraArgs(aurEff).AddSpellBP0(healAmount));
    }

    void Register() override
    {
        DoCheckProc += AuraCheckProcFn(spell_pri_trail_of_light::CheckProc);
        OnEffectProc += AuraEffectProcFn(spell_pri_trail_of_light::HandleOnProc, EFFECT_0, SPELL_AURA_DUMMY);
    }

private:
    std::queue<ObjectGuid> _healQueue;
};

// 390693 - Train of Thought
// Called by Flash Heal, Renew, Smite
class spell_pri_train_of_thought : public AuraScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo
        ({
            SPELL_PRIEST_POWER_WORD_SHIELD,
            SPELL_PRIEST_PENANCE
        });
    }

    static bool CheckEffect0(AuraEffect const* /*aurEff*/, ProcEventInfo const& eventInfo)
    {
        // Renew & Flash Heal
        return eventInfo.GetSpellInfo()->IsAffected(SPELLFAMILY_PRIEST, { 0x840 });
    }

    static bool CheckEffect1(AuraEffect const* /*aurEff*/, ProcEventInfo const& eventInfo)
    {
        // Smite
        return eventInfo.GetSpellInfo()->IsAffected(SPELLFAMILY_PRIEST, { 0x80 });
    }

    void ReducePowerWordShieldCooldown(AuraEffect const* aurEff, ProcEventInfo const& /*eventInfo*/) const
    {
        GetTarget()->GetSpellHistory()->ModifyCooldown(SPELL_PRIEST_POWER_WORD_SHIELD, Milliseconds(aurEff->GetAmount()));
    }

    void ReducePenanceCooldown(AuraEffect const* aurEff, ProcEventInfo const& /*eventInfo*/) const
    {
        GetTarget()->GetSpellHistory()->ModifyCooldown(SPELL_PRIEST_PENANCE, Milliseconds(aurEff->GetAmount()));
    }

    void Register() override
    {
        DoCheckEffectProc += AuraCheckEffectProcFn(spell_pri_train_of_thought::CheckEffect0, EFFECT_0, SPELL_AURA_DUMMY);
        OnEffectProc += AuraEffectProcFn(spell_pri_train_of_thought::ReducePowerWordShieldCooldown, EFFECT_0, SPELL_AURA_DUMMY);
        DoCheckEffectProc += AuraCheckEffectProcFn(spell_pri_train_of_thought::CheckEffect1, EFFECT_1, SPELL_AURA_DUMMY);
        OnEffectProc += AuraEffectProcFn(spell_pri_train_of_thought::ReducePenanceCooldown, EFFECT_1, SPELL_AURA_DUMMY);
    }
};

// 109142 - Twist of Fate (Shadow)
// 265259 - Twist of Fate (Discipline)
class spell_pri_twist_of_fate : public AuraScript
{
    static bool CheckProc(AuraEffect const* aurEff, ProcEventInfo const& eventInfo)
    {
        return eventInfo.GetProcTarget()->GetHealthPct() < aurEff->GetAmount();
    }

    void Register() override
    {
        DoCheckEffectProc += AuraCheckEffectProcFn(spell_pri_twist_of_fate::CheckProc, EFFECT_0, SPELL_AURA_PROC_TRIGGER_SPELL);
    }
};

// 341273 - Unfurling Darkness
// Triggered by 34914 - Vampiric Touch
class spell_pri_unfurling_darkness : public SpellScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_PRIEST_UNFURLING_DARKNESS, SPELL_PRIEST_UNFURLING_DARKNESS_DEBUFF })
            && ValidateSpellEffect({ { SPELL_PRIEST_UNFURLING_DARKNESS_AURA, EFFECT_0 } });
    }

    void PreventDirectDamage(WorldObject*& target) const
    {
        bool canTriggerDirectDamage = [&]
        {
            if (!GetSpell()->m_originalCastId.IsEmpty())
                return false;  // not when triggered by Shadow Crash (Whispering Shadows talent)
            if (AuraEffect const* unfurlingDarkness = GetCaster()->GetAuraEffect(SPELL_PRIEST_UNFURLING_DARKNESS_AURA, EFFECT_0))
                if (GetSpell()->m_appliedMods.contains(unfurlingDarkness->GetBase()))
                    return true;
            return false;
        }();

        if (!canTriggerDirectDamage)
            target = nullptr;
    }

    void TriggerUnfurlingDarkness() const
    {
        if (!GetSpell()->m_originalCastId.IsEmpty())
            return; // not when triggered by Shadow Crash (Whispering Shadows talent)

        Unit* caster = GetCaster();

        if (AuraEffect const* unfurlingDarkness = GetCaster()->GetAuraEffect(SPELL_PRIEST_UNFURLING_DARKNESS_AURA, EFFECT_0))
        {
            if (GetSpell()->m_appliedMods.contains(unfurlingDarkness->GetBase()))
            {
                unfurlingDarkness->GetBase()->Remove();
                return;
            }
        }

        if (!caster->HasAura(SPELL_PRIEST_UNFURLING_DARKNESS_DEBUFF))
            caster->CastSpell(caster, SPELL_PRIEST_UNFURLING_DARKNESS_AURA, true);
    }

    void Register() override
    {
        OnObjectTargetSelect += SpellObjectTargetSelectFn(spell_pri_unfurling_darkness::PreventDirectDamage, EFFECT_3, TARGET_UNIT_TARGET_ENEMY);
        AfterCast += SpellCastFn(spell_pri_unfurling_darkness::TriggerUnfurlingDarkness);
    }
};

// 15286 - Vampiric Embrace
class spell_pri_vampiric_embrace : public AuraScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_PRIEST_VAMPIRIC_EMBRACE_HEAL });
    }

    static bool CheckProc(ProcEventInfo const& eventInfo)
    {
        // Not proc from Mind Sear
        return !(eventInfo.GetDamageInfo()->GetSpellInfo()->SpellFamilyFlags[1] & 0x80000);
    }

    void HandleEffectProc(AuraEffect const* aurEff, ProcEventInfo const& eventInfo)
    {
        PreventDefaultAction();
        DamageInfo* damageInfo = eventInfo.GetDamageInfo();
        if (!damageInfo || !damageInfo->GetDamage())
            return;

        int32 selfHeal = int32(CalculatePct(damageInfo->GetDamage(), aurEff->GetAmount()));
        int32 teamHeal = selfHeal / 2;

        CastSpellExtraArgs args(aurEff);
        args.AddSpellMod(SPELLVALUE_BASE_POINT0, teamHeal);
        args.AddSpellMod(SPELLVALUE_BASE_POINT1, selfHeal);
        GetTarget()->CastSpell(nullptr, SPELL_PRIEST_VAMPIRIC_EMBRACE_HEAL, args);
    }

    void Register() override
    {
        DoCheckProc += AuraCheckProcFn(spell_pri_vampiric_embrace::CheckProc);
        OnEffectProc += AuraEffectProcFn(spell_pri_vampiric_embrace::HandleEffectProc, EFFECT_0, SPELL_AURA_DUMMY);
    }
};

// 15290 - Vampiric Embrace (heal)
class spell_pri_vampiric_embrace_target : public SpellScript
{
    void FilterTargets(std::list<WorldObject*>& unitList) const
    {
        unitList.remove(GetCaster());
    }

    void Register() override
    {
        OnObjectAreaTargetSelect += SpellObjectAreaTargetSelectFn(spell_pri_vampiric_embrace_target::FilterTargets, EFFECT_0, TARGET_UNIT_CASTER_AREA_PARTY);
    }
};

// 34914 - Vampiric Touch
class spell_pri_vampiric_touch : public AuraScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_PRIEST_SIN_AND_PUNISHMENT, SPELL_PRIEST_SHADOW_WORD_PAIN });
    }

    void HandleDispel(DispelInfo const* dispelInfo) const
    {
        if (Unit* caster = GetCaster())
            caster->CastSpell(dispelInfo->GetDispeller(), SPELL_PRIEST_SIN_AND_PUNISHMENT, true);
    }

    void HandleApplyEffect(AuraEffect const* /*aurEff*/, AuraEffectHandleModes /*mode*/) const
    {
        if (Unit* caster = GetCaster())
            if (caster->HasAura(SPELL_PRIEST_MISERY))
                caster->CastSpell(GetTarget(), SPELL_PRIEST_SHADOW_WORD_PAIN, true);
    }

    void Register() override
    {
        AfterDispel += AuraDispelFn(spell_pri_vampiric_touch::HandleDispel);
        OnEffectApply += AuraEffectApplyFn(spell_pri_vampiric_touch::HandleApplyEffect, EFFECT_0, SPELL_AURA_DUMMY, AURA_EFFECT_HANDLE_REAL_OR_REAPPLY_MASK);
    }
};

// 205385 - Shadow Crash
class spell_pri_whispering_shadows : public SpellScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_PRIEST_WHISPERING_SHADOWS });
    }

    void HandleEffectHitTarget(SpellEffIndex effIndex)
    {
        if (!GetCaster()->HasAura(SPELL_PRIEST_WHISPERING_SHADOWS))
            PreventHitDefaultEffect(effIndex);
    }

    void Register() override
    {
        OnEffectHit += SpellEffectFn(spell_pri_whispering_shadows::HandleEffectHitTarget, EFFECT_2, SPELL_EFFECT_TRIGGER_MISSILE);
    }
};

// 391286 - Whispering Shadows (Dot Application)
class spell_pri_whispering_shadows_effect : public SpellScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_PRIEST_VAMPIRIC_TOUCH });
    }

    void FilterTargets(std::list<WorldObject*>& targets) const
    {
        if (targets.size() <= GetSpellValue()->MaxAffectedTargets)
            return;

        auto getVampiricTouch = [&](WorldObject const* target)
        {
            return target->ToUnit()->GetAura(SPELL_PRIEST_VAMPIRIC_TOUCH, GetCaster()->GetGUID());
        };

        // prioritize targets without Vampiric Touch
        targets.sort([&](WorldObject const* target1, WorldObject const* target2)
        {
            int32 duration1 = 0;
            if (Aura const* aura1 = getVampiricTouch(target1))
                duration1 = aura1->GetDuration();
            int32 duration2 = 0;
            if (Aura const* aura2 = getVampiricTouch(target2))
                duration2 = aura2->GetDuration();
            return duration1 < duration2;
        });

        // remove targets that definitely will not get Vampiric Touch applied (excess targets with longest remaining duration)
        while (targets.size() > GetSpellValue()->MaxAffectedTargets && getVampiricTouch(targets.back()) != nullptr)
            targets.pop_back();

        Trinity::Containers::RandomResize(targets, GetSpellValue()->MaxAffectedTargets);
    }

    void HandleEffectHitTarget(SpellEffIndex /*effIndex*/) const
    {
        GetCaster()->CastSpell(GetHitUnit(), SPELL_PRIEST_VAMPIRIC_TOUCH, CastSpellExtraArgs()
            .SetTriggeringSpell(GetSpell())
            .SetTriggerFlags(TRIGGERED_IGNORE_GCD | TRIGGERED_IGNORE_SPELL_AND_CATEGORY_CD | TRIGGERED_IGNORE_POWER_COST | TRIGGERED_IGNORE_CAST_IN_PROGRESS | TRIGGERED_IGNORE_CAST_TIME | TRIGGERED_DONT_REPORT_CAST_ERROR));
    }

    void Register() override
    {
        OnObjectAreaTargetSelect += SpellObjectAreaTargetSelectFn(spell_pri_whispering_shadows_effect::FilterTargets, EFFECT_0, TARGET_UNIT_DEST_AREA_ENEMY);
        OnEffectHitTarget += SpellEffectFn(spell_pri_whispering_shadows_effect::HandleEffectHitTarget, EFFECT_0, SPELL_EFFECT_DUMMY);
    }
};

// 186723 - Penance
class spell_pri_penance_620 : public SpellScript
{
    void HandleOnCast()
    {
        Unit* caster = GetCaster();
        if (!caster || !caster->IsPlayer())
            return;

        Player* player = caster->ToPlayer();
        int32 numBolts = 3;

        if (player->HasAura(193134))
            numBolts += 1;

        if (player->HasAura(373183))
            numBolts += 3;

        if (Aura* penanceAura = caster->GetAura(186723))
        {
            if (AuraEffect* aurEff = penanceAura->GetEffect(EFFECT_0))
            {
                aurEff->SetAmount(numBolts);
            }
        }
    }

    void HandleOnHit()
    {
        Unit* caster = GetCaster();
        Unit* target = GetHitUnit();
        if (!caster || !target)
            return;

        float damageModifier = 1.0f;
        float healModifier = 1.0f;

        //TODO: Need to test to see if it breaks the balance too much.
        //Is it worth adding this? Or is it automatically included in the math of the formula?

        if (caster->HasAura(198069))
        {
            damageModifier *= 1.3f;
            healModifier *= 1.3f;
        }

        if (caster->HasAura(390706))
        {
            damageModifier *= 1.15f;
        }

        if (caster->IsHostileTo(target))
        {
            int32 damage = GetHitDamage();
            SetHitDamage(int32(damage * damageModifier));
        }
        else
        {
            int32 healing = GetHitHeal();
            SetHitHeal(int32(healing * healModifier));

            ApplyAtonementHealing(caster);
        }
    }

    void ApplyAtonementHealing(Unit* caster)
    {
        if (!caster || !caster->IsPlayer())
            return;

        Player* player = caster->ToPlayer();
        float spellPower = player->GetStat(STAT_INTELLECT);
        int32 atonementHealing = int32(spellPower * 0.1872f);

        std::list<Unit*> targets;
        Trinity::AnyFriendlyUnitInObjectRangeCheck check(caster, caster, 40.0f);
        Trinity::UnitListSearcher<Trinity::AnyFriendlyUnitInObjectRangeCheck> searcher(caster, targets, check);
        Cell::VisitAllObjects(caster, searcher, 40.0f);

        for (Unit* target : targets)
        {
            if (target->HasAura(194384, caster->GetGUID()))
            {
                CastSpellExtraArgs args;
                args.TriggerFlags = TRIGGERED_FULL_MASK;
                args.AddSpellBP0(atonementHealing);
                caster->CastSpell(target, 81751, args);
            }
        }
    }

    void Register() override
    {
        OnCast += SpellCastFn(spell_pri_penance_620::HandleOnCast);
        OnHit += SpellHitFn(spell_pri_penance_620::HandleOnHit);
    }
};

class spell_pri_penance_620_aura : public AuraScript
{
    void HandleEffectCalcAmount(AuraEffect const* /*aurEff*/, int32& amount, bool& /*canBeRecalculated*/)
    {
        Unit* caster = GetCaster();
        if (!caster || !caster->IsPlayer())
            return;

        Player* player = caster->ToPlayer();
        float spellPower = player->GetStat(STAT_INTELLECT);

        float damagePerTick = spellPower * 0.795f;
        float healingPerTick = spellPower * 3.42f;

        Unit* target = GetUnitOwner();
        if (caster->IsHostileTo(target))
        {
            amount = int32(damagePerTick);
        }
        else
        {
            amount = int32(healingPerTick);
        }
    }

    void Register() override
    {
        DoEffectCalcAmount += AuraEffectCalcAmountFn(spell_pri_penance_620_aura::HandleEffectCalcAmount, EFFECT_0, SPELL_AURA_PERIODIC_HEAL);
    }
};

void AddSC_priest_spell_scripts()
{
    RegisterSpellScript(spell_pri_angelic_feather_trigger);
    RegisterAreaTriggerAI(areatrigger_pri_angelic_feather);
    RegisterSpellScript(spell_pri_abyssal_reverie);
    RegisterSpellScript(spell_pri_answered_prayers);
    RegisterSpellScript(spell_pri_aq_3p_bonus);
    RegisterSpellScript(spell_pri_assured_safety);
    RegisterSpellScript(spell_pri_atonement);
    RegisterSpellScript(spell_pri_atonement_effect);
    RegisterSpellScript(spell_pri_atonement_effect_aura);
    RegisterSpellScript(spell_pri_atonement_passive);
    RegisterSpellScript(spell_pri_benediction);
    RegisterSpellScript(spell_pri_blaze_of_light);
    RegisterSpellScript(spell_pri_circle_of_healing);
    RegisterSpellScript(spell_pri_crystalline_reflection);
    RegisterSpellScript(spell_pri_dark_indulgence);
    RegisterSpellScript(spell_pri_divine_aegis);
    RegisterSpellScript(spell_pri_divine_image);
    RegisterSpellScript(spell_pri_divine_image_spell_triggered);
    RegisterSpellScript(spell_pri_divine_image_stack_timer);
    RegisterSpellScript(spell_pri_divine_service);
    RegisterSpellScript(spell_pri_divine_star_shadow);
    RegisterAreaTriggerAI(areatrigger_pri_divine_star);
    RegisterSpellScript(spell_pri_empowered_renew);
    RegisterSpellScript(spell_pri_epiphany);
    RegisterSpellScript(spell_pri_essence_devourer_heal);
    RegisterSpellScript(spell_pri_evangelism);
    RegisterSpellScript(spell_pri_focused_mending);
    RegisterSpellScript(spell_pri_from_darkness_comes_light);
    RegisterSpellScript(spell_pri_guardian_spirit);
    RegisterSpellScript(spell_pri_halo_shadow);
    RegisterAreaTriggerAI(areatrigger_pri_halo);
    RegisterSpellScript(spell_pri_heavens_wrath);
    RegisterSpellScript(spell_pri_holy_mending);
    RegisterSpellScript(spell_pri_holy_words);
    RegisterSpellScript(spell_pri_holy_word_salvation);
    RegisterSpellScript(spell_pri_holy_word_salvation_cooldown_reduction);
    RegisterSpellScript(spell_pri_item_t6_trinket);
    RegisterSpellScript(spell_pri_leap_of_faith_effect_trigger);
    RegisterSpellScript(spell_pri_levitate);
    RegisterSpellScript(spell_pri_lights_wrath);
    RegisterSpellScript(spell_pri_mind_bomb);
    RegisterSpellScript(spell_pri_mind_devourer);
    RegisterSpellAndAuraScriptPair(spell_pri_mind_devourer_buff, spell_pri_mind_devourer_buff_aura);
    RegisterSpellScript(spell_pri_painful_punishment);
    RegisterSpellScript(spell_pri_pain_transformation);
    RegisterSpellScriptWithArgs(spell_pri_penance, "spell_pri_penance", SPELL_PRIEST_PENANCE_CHANNEL_DAMAGE, SPELL_PRIEST_PENANCE_CHANNEL_HEALING);
    RegisterSpellScriptWithArgs(spell_pri_penance, "spell_pri_dark_reprimand", SPELL_PRIEST_DARK_REPRIMAND_CHANNEL_DAMAGE, SPELL_PRIEST_DARK_REPRIMAND_CHANNEL_HEALING);
    RegisterSpellScript(spell_pri_penance_or_dark_reprimand_channeled);
    RegisterSpellScript(spell_pri_phantasm);
    RegisterSpellScript(spell_pri_power_leech_passive);
    RegisterSpellScript(spell_pri_power_of_the_dark_side);
    RegisterSpellScript(spell_pri_power_of_the_dark_side_damage_bonus);
    RegisterSpellScript(spell_pri_power_of_the_dark_side_healing_bonus);
    RegisterSpellScript(spell_pri_power_word_radiance);
    RegisterSpellScript(spell_pri_power_word_shield);
    RegisterSpellScript(spell_pri_power_word_solace);
    RegisterSpellScript(spell_pri_prayer_of_mending_dummy);
    RegisterSpellAndAuraScriptPair(spell_pri_prayer_of_mending, spell_pri_prayer_of_mending_aura);
    RegisterSpellScript(spell_pri_prayer_of_mending_jump);
    RegisterSpellScript(spell_pri_protective_light);
    RegisterSpellScript(spell_pri_holy_10_1_class_set_2pc);
    RegisterSpellScript(spell_pri_holy_10_1_class_set_2pc_chooser);
    RegisterSpellScript(spell_pri_holy_10_1_class_set_4pc);
    RegisterSpellScript(spell_pri_holy_10_1_class_set_4pc_aura);
    RegisterSpellScript(spell_pri_purge_the_wicked);
    RegisterSpellScript(spell_pri_purge_the_wicked_dummy);
    RegisterSpellScript(spell_pri_rapture);
    RegisterSpellScript(spell_pri_sanctuary_absorb);
    RegisterSpellScript(spell_pri_sanctuary_trigger);
    RegisterSpellScript(spell_pri_rhapsody);
    RegisterSpellScript(spell_pri_rhapsody_proc);
    RegisterSpellScript(spell_pri_schism);
    RegisterSpellScript(spell_pri_sins_of_the_many);
    RegisterSpellScript(spell_pri_spirit_of_redemption);
    RegisterSpellScript(spell_pri_shadow_covenant);
    RegisterSpellScript(spell_pri_shadow_mend);
    RegisterSpellScript(spell_pri_shadow_mend_periodic_damage);
    RegisterSpellScript(spell_pri_shadow_word_death);
    RegisterSpellScript(spell_pri_surge_of_light);
    RegisterSpellScript(spell_pri_trail_of_light);
    RegisterSpellScript(spell_pri_train_of_thought);
    RegisterSpellScript(spell_pri_t3_4p_bonus);
    RegisterSpellScript(spell_pri_t5_heal_2p_bonus);
    RegisterSpellScript(spell_pri_t10_heal_2p_bonus);
    RegisterSpellScript(spell_pri_twist_of_fate);
    RegisterSpellScript(spell_pri_unfurling_darkness);
    RegisterSpellScript(spell_pri_vampiric_embrace);
    RegisterSpellScript(spell_pri_vampiric_embrace_target);
    RegisterSpellScript(spell_pri_vampiric_touch);
    RegisterSpellScript(spell_pri_whispering_shadows);
    RegisterSpellScript(spell_pri_whispering_shadows_effect);

    //New
    RegisterSpellAndAuraScriptPair(spell_pri_penance_620, spell_pri_penance_620_aura);
}
