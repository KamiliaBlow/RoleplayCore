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
 * Scripts for spells with SPELLFAMILY_ROGUE and SPELLFAMILY_GENERIC spells used by rogue players.
 * Ordered alphabetically using scriptname.
 * Scriptnames of files in this file should be prefixed with "spell_rog_".
 */

#include "ScriptMgr.h"
#include "Containers.h"
#include "ObjectAccessor.h"
#include "Player.h"
#include "Spell.h"
#include "SpellAuraEffects.h"
#include "SpellHistory.h"
#include "SpellMgr.h"
#include "SpellScript.h"

enum RogueSpells
{
    SPELL_ROGUE_ACROBATIC_STRIKES_PROC              = 455144,
    SPELL_ROGUE_ADRENALINE_RUSH                     = 13750,
    SPELL_ROGUE_AIRBORNE_IRRITANT                   = 200733,
    SPELL_ROGUE_AMPLIFYING_POISON                   = 381664,
    SPELL_ROGUE_AMPLIFYING_POISON_DEBUFF            = 383414,
    SPELL_ROGUE_ATROPHIC_POISON                     = 381637,
    SPELL_ROGUE_ATROPHIC_POISON_DEBUFF              = 392388,
    SPELL_ROGUE_BETWEEN_THE_EYES                    = 199804,
    SPELL_ROGUE_BLACKJACK_TALENT                    = 379005,
    SPELL_ROGUE_BLACKJACK                           = 394119,
    SPELL_ROGUE_BLADE_FLURRY                        = 13877,
    SPELL_ROGUE_BLADE_FLURRY_EXTRA_ATTACK           = 22482,
    SPELL_ROGUE_BLIND_AREA                          = 427773,
    SPELL_ROGUE_BROADSIDE                           = 193356,
    SPELL_ROGUE_BURIED_TREASURE                     = 199600,
    SPELL_ROGUE_CHEAT_DEATH_DUMMY                   = 31231,
    SPELL_ROGUE_CHEATED_DEATH                       = 45181,
    SPELL_ROGUE_CHEATING_DEATH                      = 45182,
    SPELL_ROGUE_CLOAKED_IN_SHADOWS_TALENT           = 382515,
    SPELL_ROGUE_CLOAKED_IN_SHADOWS_ABSORB           = 386165,
    SPELL_ROGUE_CRIPPLING_POISON                    = 3408,
    SPELL_ROGUE_CRIPPLING_POISON_DEBUFF             = 3409,
    SPELL_ROGUE_DEADLY_POISON                       = 2823,
    SPELL_ROGUE_DEADLY_POISON_DEBUFF                = 2818,
    SPELL_ROGUE_DEADLY_POISON_INSTANT_DAMAGE        = 113780,
    SPELL_ROGUE_GRAND_MELEE                         = 193358,
    SPELL_ROGUE_GRAPPLING_HOOK                      = 195457,
    SPELL_ROGUE_IMPROVED_GARROTE_AFTER_STEALTH      = 392401,
    SPELL_ROGUE_IMPROVED_GARROTE_STEALTH            = 392403,
    SPELL_ROGUE_IMPROVED_GARROTE_TALENT             = 381632,
    SPELL_ROGUE_IMPROVED_SHIV                       = 319032,
    SPELL_ROGUE_INSTANT_POISON                      = 315584,
    SPELL_ROGUE_INSTANT_POISON_DAMAGE               = 315585,
    SPELL_ROGUE_KILLING_SPREE                       = 51690,
    SPELL_ROGUE_KILLING_SPREE_TELEPORT              = 57840,
    SPELL_ROGUE_KILLING_SPREE_WEAPON_DMG            = 57841,
    SPELL_ROGUE_KILLING_SPREE_DMG_BUFF              = 61851,
    SPELL_ROGUE_LEECHING_POISON_TALENT              = 280716,
    SPELL_ROGUE_LEECHING_POISON_AURA                = 108211,
    SPELL_ROGUE_MARKED_FOR_DEATH                    = 137619,
    SPELL_ROGUE_MAIN_GAUCHE                         = 86392,
    SPELL_ROGUE_NIGHT_TERRORS                       = 277953,
    SPELL_ROGUE_NUMBING_POISON                      = 5761,
    SPELL_ROGUE_NUMBING_POISON_DEBUFF               = 5760,
    SPELL_ROGUE_PREMEDITATION_PASSIVE               = 343160,
    SPELL_ROGUE_PREMEDITATION_AURA                  = 343173,
    SPELL_ROGUE_PREMEDITATION_ENERGIZE              = 343170,
    SPELL_ROGUE_PREY_ON_THE_WEAK_TALENT             = 131511,
    SPELL_ROGUE_PREY_ON_THE_WEAK                    = 255909,
    SPELL_ROGUE_RUTHLESS_PRECISION                  = 193357,
    SPELL_ROGUE_SANCTUARY                           = 98877,
    SPELL_ROGUE_SKULL_AND_CROSSBONES                = 199603,
    SPELL_ROGUE_SHADOW_DANCE                        = 185313,
    SPELL_ROGUE_SHADOW_FOCUS                        = 108209,
    SPELL_ROGUE_SHADOW_FOCUS_EFFECT                 = 112942,
    SPELL_ROGUE_SHADOWS_GRASP                       = 206760,
    SPELL_ROGUE_SHIV_NATURE_DAMAGE                  = 319504,
    SPELL_ROGUE_SHOT_IN_THE_DARK_TALENT             = 257505,
    SPELL_ROGUE_SHOT_IN_THE_DARK_BUFF               = 257506,
    SPELL_ROGUE_SHURIKEN_STORM_DAMAGE               = 197835,
    SPELL_ROGUE_SHURIKEN_STORM_ENERGIZE             = 212743,
    SPELL_ROGUE_SLICE_AND_DICE                      = 315496,
    SPELL_ROGUE_SPRINT                              = 2983,
    SPELL_ROGUE_SOOTHING_DARKNESS_TALENT            = 393970,
    SPELL_ROGUE_SOOTHING_DARKNESS_HEAL              = 393971,
    SPELL_ROGUE_STEALTH                             = 1784,
    SPELL_ROGUE_STEALTH_STEALTH_AURA                = 158185,
    SPELL_ROGUE_STEALTH_SHAPESHIFT_AURA             = 158188,
    SPELL_ROGUE_SYMBOLS_OF_DEATH_CRIT_AURA          = 227151,
    SPELL_ROGUE_SYMBOLS_OF_DEATH_RANK2              = 328077,
    SPELL_ROGUE_TRUE_BEARING                        = 193359,
    SPELL_ROGUE_TURN_THE_TABLES_BUFF                = 198027,
    SPELL_ROGUE_VANISH                              = 1856,
    SPELL_ROGUE_VANISH_AURA                         = 11327,
    SPELL_ROGUE_TRICKS_OF_THE_TRADE                 = 57934,
    SPELL_ROGUE_TRICKS_OF_THE_TRADE_PROC            = 59628,
    SPELL_ROGUE_HONOR_AMONG_THIEVES_ENERGIZE        = 51699,
    SPELL_ROGUE_T5_2P_SET_BONUS                     = 37169,
    SPELL_ROGUE_VENOMOUS_WOUNDS                     = 79134,
    SPELL_ROGUE_WOUND_POISON                        = 8679,
    SPELL_ROGUE_WOUND_POISON_DEBUFF                 = 8680,
    SPELL_ROGUE_SPELL_NIGHTSTALKER_AURA             = 14062,
    SPELL_ROGUE_SPELL_NIGHTSTALKER_DAMAGE_DONE      = 130493,
    SPELL_ROGUE_FAN_OF_KNIVES                       = 51723,
    SPELL_ROGUE_GARROTE_DOT                         = 703,
    SPELL_ROGUE_GARROTE_SILENCE                     = 1330,
    SPELL_ROGUE_THUGGEE                             = 196861,
    SPELL_ROGUE_GRAPPLING_HOOK_TRIGGER              = 230149,
    SPELL_ROGUE_SHURIKEN_STORM                      = 197835,
    SPELL_ROGUE_SECRET_TECHNIQUE                    = 280719,
    SPELL_ROGUE_SHADOWSTRIKE                        = 185438,
    SPELL_ROGUE_CHEAP_SHOT                          = 1833,
    SPELL_ROGUE_BACKSTAB                            = 53,
};

static constexpr std::array<std::pair<uint32 /*poisonAura*/, uint32 /*triggeredPoisonSpell*/>, 7> PoisonAuraToDebuff
{{
    { SPELL_ROGUE_WOUND_POISON,      SPELL_ROGUE_WOUND_POISON_DEBUFF      },
    { SPELL_ROGUE_DEADLY_POISON,     SPELL_ROGUE_DEADLY_POISON_DEBUFF     },
    { SPELL_ROGUE_AMPLIFYING_POISON, SPELL_ROGUE_AMPLIFYING_POISON_DEBUFF },
    { SPELL_ROGUE_CRIPPLING_POISON,  SPELL_ROGUE_CRIPPLING_POISON_DEBUFF  },
    { SPELL_ROGUE_NUMBING_POISON,    SPELL_ROGUE_NUMBING_POISON_DEBUFF    },
    { SPELL_ROGUE_INSTANT_POISON,    SPELL_ROGUE_INSTANT_POISON_DAMAGE    },
    { SPELL_ROGUE_ATROPHIC_POISON,   SPELL_ROGUE_ATROPHIC_POISON_DEBUFF   }
}};

/* Returns true if the spell is a finishing move.
 * A finishing move is a spell that cost combo points */
Optional<int32> GetFinishingMoveCPCost(Spell const* spell)
{
    if (!spell)
        return { };

    return spell->GetPowerTypeCostAmount(POWER_COMBO_POINTS);
}

/* Return true if the spell is a finishing move.
 * A finishing move is a spell that cost combo points */
bool IsFinishingMove(Spell const* spell)
{
    return GetFinishingMoveCPCost(spell).has_value();
}

static constexpr bool IsLethalPoison(uint32 spellId)
{
    switch (spellId)
    {
        case SPELL_ROGUE_DEADLY_POISON:
        case SPELL_ROGUE_WOUND_POISON:
        case SPELL_ROGUE_INSTANT_POISON:
        case SPELL_ROGUE_AMPLIFYING_POISON:
            return true;
        default:
            return false;
    }
}

// 455143 - Acrobatic Strikes
class spell_rog_acrobatic_strikes : public AuraScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_ROGUE_ACROBATIC_STRIKES_PROC });
    }

    void HandleProc(AuraEffect const* aurEff, ProcEventInfo& /*eventInfo*/) const
    {
        GetTarget()->CastSpell(GetTarget(), SPELL_ROGUE_ACROBATIC_STRIKES_PROC, CastSpellExtraArgsInit{
            .TriggerFlags = TRIGGERED_IGNORE_CAST_IN_PROGRESS | TRIGGERED_DONT_REPORT_CAST_ERROR,
            .TriggeringAura = aurEff
        });
    }

    void Register() override
    {
        // Midnight 12.0.1: spell 455143 has no DBC effect data — handler disabled
#if 0
        OnEffectProc += AuraEffectProcFn(spell_rog_acrobatic_strikes::HandleProc, EFFECT_0, SPELL_AURA_DUMMY);
#endif
    }
};

// Called by 2094 - Blind
class spell_rog_airborne_irritant : public SpellScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_ROGUE_AIRBORNE_IRRITANT, SPELL_ROGUE_BLIND_AREA });
    }

    bool Load() override
    {
        return GetCaster()->HasAura(SPELL_ROGUE_AIRBORNE_IRRITANT);
    }

    void HandleHit(SpellEffIndex /*effIndex*/) const
    {
        GetCaster()->CastSpell(GetHitUnit(), SPELL_ROGUE_BLIND_AREA, CastSpellExtraArgsInit{
            .TriggerFlags = TRIGGERED_IGNORE_CAST_IN_PROGRESS | TRIGGERED_DONT_REPORT_CAST_ERROR,
            .TriggeringSpell = GetSpell()
        });
    }

    void Register() override
    {
        OnEffectHit += SpellEffectFn(spell_rog_airborne_irritant::HandleHit, EFFECT_0, SPELL_EFFECT_APPLY_AURA);
    }
};

// 427773 - Blind
class spell_rog_airborne_irritant_target_selection : public SpellScript
{
    void FilterTargets(std::list<WorldObject*>& targets) const
    {
        targets.remove(GetExplTargetWorldObject());
    }

    void Register() override
    {
        OnObjectAreaTargetSelect += SpellObjectAreaTargetSelectFn(spell_rog_airborne_irritant_target_selection::FilterTargets, EFFECT_ALL, TARGET_UNIT_DEST_AREA_ENEMY);
    }
};

// 53 - Backstab
class spell_rog_backstab : public SpellScript
{
    bool Validate(SpellInfo const* spellInfo) override
    {
        return ValidateSpellEffect({ { spellInfo->Id, EFFECT_3 } });
    }

    void HandleHitDamage(SpellEffIndex /*effIndex*/)
    {
        Unit* hitUnit = GetHitUnit();
        if (!hitUnit)
            return;

        Unit* caster = GetCaster();
        if (hitUnit->isInBack(caster))
        {
            float currDamage = float(GetHitDamage());
            float newDamage = AddPct(currDamage, float(GetEffectInfo(EFFECT_3).CalcValue(caster)));
            SetHitDamage(newDamage);
        }
    }

    void Register() override
    {
        OnEffectHitTarget += SpellEffectFn(spell_rog_backstab::HandleHitDamage, EFFECT_1, SPELL_EFFECT_SCHOOL_DAMAGE);
    }
};

// 379005 - Blackjack
// Called by Sap - 6770 and Blind - 2094
class spell_rog_blackjack : public AuraScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_ROGUE_BLACKJACK_TALENT, SPELL_ROGUE_BLACKJACK });
    }

    void EffectRemove(AuraEffect const* /*aurEff*/, AuraEffectHandleModes /*mode*/) const
    {
        if (Unit* caster = GetCaster())
            if (caster->HasAura(SPELL_ROGUE_BLACKJACK_TALENT))
                caster->CastSpell(GetTarget(), SPELL_ROGUE_BLACKJACK, true);
    }

    void Register() override
    {
        AfterEffectRemove += AuraEffectApplyFn(spell_rog_blackjack::EffectRemove, EFFECT_0, SPELL_AURA_ANY, AURA_EFFECT_HANDLE_REAL);
    }
};

// 13877, 33735, (check 51211, 65956) - Blade Flurry
class spell_rog_blade_flurry : public AuraScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_ROGUE_BLADE_FLURRY_EXTRA_ATTACK });
    }

    bool CheckProc(ProcEventInfo& eventInfo)
    {
        _procTarget = GetTarget()->SelectNearbyTarget(eventInfo.GetProcTarget());
        return _procTarget && eventInfo.GetDamageInfo();
    }

    void HandleProc(AuraEffect* aurEff, ProcEventInfo& eventInfo)
    {
        PreventDefaultAction();

        if (DamageInfo* damageInfo = eventInfo.GetDamageInfo())
        {
            CastSpellExtraArgs args(aurEff);
            args.AddSpellBP0(damageInfo->GetDamage());
            GetTarget()->CastSpell(_procTarget, SPELL_ROGUE_BLADE_FLURRY_EXTRA_ATTACK, args);
        }
    }

    void Register() override
    {
        DoCheckProc += AuraCheckProcFn(spell_rog_blade_flurry::CheckProc);
        // Midnight 12.0.1: spell 13877 EFFECT_0 aura is now DUMMY(4)
        if (m_scriptSpellId == SPELL_ROGUE_BLADE_FLURRY)
            OnEffectProc += AuraEffectProcFn(spell_rog_blade_flurry::HandleProc, EFFECT_0, SPELL_AURA_DUMMY);
        else
            OnEffectProc += AuraEffectProcFn(spell_rog_blade_flurry::HandleProc, EFFECT_0, SPELL_AURA_MOD_MELEE_HASTE);
    }

    Unit* _procTarget = nullptr;
};

// 31230 - Cheat Death
class spell_rog_cheat_death : public AuraScript
{
    bool Validate(SpellInfo const* spellInfo) override
    {
        return ValidateSpellInfo({ SPELL_ROGUE_CHEAT_DEATH_DUMMY, SPELL_ROGUE_CHEATED_DEATH, SPELL_ROGUE_CHEATING_DEATH })
            && ValidateSpellEffect({ { spellInfo->Id, EFFECT_1 } });
    }

    void HandleAbsorb(AuraEffect const* /*aurEff*/, DamageInfo const& /*dmgInfo*/, uint32& absorbAmount)
    {
        Unit* target = GetTarget();
        if (target->HasAura(SPELL_ROGUE_CHEATED_DEATH))
        {
            absorbAmount = 0;
            return;
        }

        PreventDefaultAction();

        target->CastSpell(target, SPELL_ROGUE_CHEAT_DEATH_DUMMY, TRIGGERED_IGNORE_CAST_IN_PROGRESS | TRIGGERED_DONT_REPORT_CAST_ERROR);
        target->CastSpell(target, SPELL_ROGUE_CHEATED_DEATH, TRIGGERED_DONT_REPORT_CAST_ERROR);
        target->CastSpell(target, SPELL_ROGUE_CHEATING_DEATH, TRIGGERED_DONT_REPORT_CAST_ERROR);

        target->SetHealth(target->CountPctFromMaxHealth(GetEffectInfo(EFFECT_1).CalcValue(target)));
    }

    void Register() override
    {
        OnEffectAbsorb += AuraEffectAbsorbOverkillFn(spell_rog_cheat_death::HandleAbsorb, EFFECT_0);
    }
};

// 382515 - Cloaked in Shadows (attached to 1856 - Vanish)
class spell_rog_cloaked_in_shadows : public SpellScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_ROGUE_CLOAKED_IN_SHADOWS_ABSORB })
            && ValidateSpellEffect({ { SPELL_ROGUE_CLOAKED_IN_SHADOWS_TALENT, EFFECT_0 } });
    }

    bool Load() override
    {
        return GetCaster()->HasAuraEffect(SPELL_ROGUE_CLOAKED_IN_SHADOWS_TALENT, EFFECT_0);
    }

    void HandleCloakedInShadows() const
    {
        Unit* caster = GetCaster();

        AuraEffect const* cloakedInShadows = caster->GetAuraEffect(SPELL_ROGUE_CLOAKED_IN_SHADOWS_TALENT, EFFECT_0);
        if (!cloakedInShadows)
            return;

        int32 amount = caster->CountPctFromMaxHealth(cloakedInShadows->GetAmount());

        caster->CastSpell(caster, SPELL_ROGUE_CLOAKED_IN_SHADOWS_ABSORB, CastSpellExtraArgsInit{
            .TriggerFlags = TRIGGERED_IGNORE_CAST_IN_PROGRESS | TRIGGERED_DONT_REPORT_CAST_ERROR,
            .TriggeringSpell = GetSpell(),
            .SpellValueOverrides = { { SPELLVALUE_BASE_POINT0, amount } }
        });
    }

    void Register() override
    {
        AfterCast += SpellCastFn(spell_rog_cloaked_in_shadows::HandleCloakedInShadows);
    }
};

// 2818 - Deadly Poison
class spell_rog_deadly_poison : public SpellScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_ROGUE_DEADLY_POISON_INSTANT_DAMAGE });
    }

    void HandleInstantDamage(SpellEffIndex /*effIndex*/) const
    {
        Unit* caster = GetCaster();
        Unit* target = GetHitUnit();
        if (target->HasAura(GetSpellInfo()->Id, caster->GetGUID()))
            caster->CastSpell(target, SPELL_ROGUE_DEADLY_POISON_INSTANT_DAMAGE, CastSpellExtraArgsInit{
                .TriggerFlags = TRIGGERED_IGNORE_CAST_IN_PROGRESS | TRIGGERED_DONT_REPORT_CAST_ERROR
            });
    }

    void Register() override
    {
        OnEffectLaunchTarget += SpellEffectFn(spell_rog_deadly_poison::HandleInstantDamage, EFFECT_0, SPELL_EFFECT_APPLY_AURA);
    }
};

// 185314 - Deepening Shadows
class spell_rog_deepening_shadows : public AuraScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_ROGUE_SHADOW_DANCE });
    }

    static bool CheckProc(AuraScript const&, AuraEffect const* /*aurEff*/, ProcEventInfo const& procEvent)
    {
        if (Spell const* procSpell = procEvent.GetProcSpell())
            return procSpell->GetPowerTypeCostAmount(POWER_COMBO_POINTS) > 0;

        return false;
    }

    void HandleProc(AuraEffect const* aurEff, ProcEventInfo const& procInfo) const
    {
        Milliseconds amount = -Seconds(aurEff->GetAmount()) * *procInfo.GetProcSpell()->GetPowerTypeCostAmount(POWER_COMBO_POINTS);
        GetTarget()->GetSpellHistory()->ModifyChargeRecoveryTime(sSpellMgr->AssertSpellInfo(SPELL_ROGUE_SHADOW_DANCE, GetCastDifficulty())->ChargeCategoryId, amount / 10);
    }

    void Register() override
    {
        // Midnight 12.0.1: spell 185314 EFFECT_0 aura is LINKED_2(285), not DUMMY(4)
        DoCheckEffectProc += AuraCheckEffectProcFn(spell_rog_deepening_shadows::CheckProc, EFFECT_0, SPELL_AURA_LINKED_2);
        OnEffectProc += AuraEffectProcFn(spell_rog_deepening_shadows::HandleProc, EFFECT_0, SPELL_AURA_LINKED_2);
    }
};

// 32645 - Envenom
class spell_rog_envenom : public SpellScript
{
    void CalculateDamage(SpellEffectInfo const& /*spellEffectInfo*/, Unit* /*victim*/, int32& /*damage*/, int32& flatMod, float& pctMod) const
    {
        pctMod *= GetSpell()->GetPowerTypeCostAmount(POWER_COMBO_POINTS).value_or(0);

        if (AuraEffect const* t5 = GetCaster()->GetAuraEffect(SPELL_ROGUE_T5_2P_SET_BONUS, EFFECT_0))
            flatMod += t5->GetAmount();
    }

    void Register() override
    {
        CalcDamage += SpellCalcDamageFn(spell_rog_envenom::CalculateDamage);
    }
};

// 196819 - Eviscerate
class spell_rog_eviscerate : public SpellScript
{
    void CalculateDamage(SpellEffIndex /*effIndex*/)
    {
        Unit* caster = GetCaster();
        Unit* target = GetHitUnit();
        if (!caster || !target)
            return;

        int32 damagePerCombo = GetHitDamage();

        if (AuraEffect const* t5 = caster->GetAuraEffect(SPELL_ROGUE_T5_2P_SET_BONUS, EFFECT_0))
            damagePerCombo += t5->GetAmount();

        int32 comboPoints = 0;
        if (Spell* spell = GetSpell())
            comboPoints = spell->GetUsedComboPoints();

        if (comboPoints == 0)
            comboPoints = 1;

        int32 totalDamage = damagePerCombo * comboPoints;
        SetHitDamage(totalDamage);

        if (AuraEffect const* darkFinish = caster->GetAuraEffect(382511, EFFECT_0))
        {
            if (target->HasAura(91021))
            {
                int32 bonusPct = darkFinish->GetAmount();
                int32 shadowDamage = CalculatePct(totalDamage, bonusPct);

                caster->DealDamage(caster, target, shadowDamage, nullptr, DIRECT_DAMAGE, SPELL_SCHOOL_MASK_SHADOW, GetSpellInfo(), false);
            }
        }
    }

    void Register() override
    {
        OnEffectHitTarget += SpellEffectFn(spell_rog_eviscerate::CalculateDamage, EFFECT_0, SPELL_EFFECT_SCHOOL_DAMAGE);
    }
};

// 193358 - Grand Melee
class spell_rog_grand_melee : public AuraScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_ROGUE_SLICE_AND_DICE });
    }

    bool HandleCheckProc(ProcEventInfo& eventInfo)
    {
        Spell const* procSpell = eventInfo.GetProcSpell();
        return procSpell && procSpell->HasPowerTypeCost(POWER_COMBO_POINTS);
    }

    void HandleProc(AuraEffect* aurEff, ProcEventInfo& procInfo)
    {
        Spell const* procSpell = procInfo.GetProcSpell();
        int32 amount = aurEff->GetAmount() * *procSpell->GetPowerTypeCostAmount(POWER_COMBO_POINTS) * 1000;

        if (Unit* target = GetTarget())
        {
            if (Aura* aura = target->GetAura(SPELL_ROGUE_SLICE_AND_DICE))
                aura->SetDuration(aura->GetDuration() + amount);
            else
            {
                CastSpellExtraArgs args;
                args.TriggerFlags = TRIGGERED_FULL_MASK;
                args.AddSpellMod(SPELLVALUE_DURATION, amount);
                target->CastSpell(target, SPELL_ROGUE_SLICE_AND_DICE, args);
            }
        }
    }

    void Register() override
    {
        DoCheckProc += AuraCheckProcFn(spell_rog_grand_melee::HandleCheckProc);
        // Midnight 12.0.1: spell 193358 EFFECT_0 aura is ADD_PCT_MODIFIER(108), not DUMMY(4)
        OnEffectProc += AuraEffectProcFn(spell_rog_grand_melee::HandleProc, EFFECT_0, SPELL_AURA_ADD_PCT_MODIFIER);
    }
};

// 198031 - Honor Among Thieves
/// 7.1.5
class spell_rog_honor_among_thieves : public AuraScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_ROGUE_HONOR_AMONG_THIEVES_ENERGIZE });
    }

    void HandleProc(AuraEffect* aurEff, ProcEventInfo& /*eventInfo*/)
    {
        PreventDefaultAction();

        Unit* target = GetTarget();
        target->CastSpell(target, SPELL_ROGUE_HONOR_AMONG_THIEVES_ENERGIZE, aurEff);
    }

    void Register() override
    {
        OnEffectProc += AuraEffectProcFn(spell_rog_honor_among_thieves::HandleProc, EFFECT_0, SPELL_AURA_DUMMY);
    }
};

// Called by 1784 - Stealth
class spell_rog_improved_garrote : public AuraScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo
        ({
            SPELL_ROGUE_IMPROVED_GARROTE_AFTER_STEALTH,
            SPELL_ROGUE_IMPROVED_GARROTE_STEALTH,
            SPELL_ROGUE_IMPROVED_GARROTE_TALENT,
        });
    }

    bool Load() override
    {
        return GetCaster()->HasAura(SPELL_ROGUE_IMPROVED_GARROTE_TALENT);
    }

    void HandleBuff(uint32 spellToCast, uint32 auraToRemove) const
    {
        Unit* target = GetTarget();

        target->RemoveAurasDueToSpell(auraToRemove);
        target->CastSpell(target, spellToCast, CastSpellExtraArgsInit{
            .TriggerFlags = TRIGGERED_IGNORE_CAST_IN_PROGRESS | TRIGGERED_DONT_REPORT_CAST_ERROR
        });

    }

    void HandleEffectApply(AuraEffect const* /*aurEff*/, AuraEffectHandleModes /*mode*/) const
    {
        HandleBuff(SPELL_ROGUE_IMPROVED_GARROTE_STEALTH, SPELL_ROGUE_IMPROVED_GARROTE_AFTER_STEALTH);
    }

    void HandleEffectRemove(AuraEffect const* /*aurEff*/, AuraEffectHandleModes /*mode*/) const
    {
        HandleBuff(SPELL_ROGUE_IMPROVED_GARROTE_AFTER_STEALTH, SPELL_ROGUE_IMPROVED_GARROTE_STEALTH);
    }

    void Register() override
    {
        AfterEffectApply += AuraEffectApplyFn(spell_rog_improved_garrote::HandleEffectApply, EFFECT_1, SPELL_AURA_DUMMY, AURA_EFFECT_HANDLE_REAL);
        AfterEffectRemove += AuraEffectRemoveFn(spell_rog_improved_garrote::HandleEffectRemove, EFFECT_1, SPELL_AURA_DUMMY, AURA_EFFECT_HANDLE_REAL);
    }
};

// 703 - Garrote
class spell_rog_improved_garrote_damage : public AuraScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo
        ({
            SPELL_ROGUE_IMPROVED_GARROTE_AFTER_STEALTH,
            SPELL_ROGUE_IMPROVED_GARROTE_STEALTH,
            SPELL_ROGUE_IMPROVED_GARROTE_TALENT,
        });
    }

    void CalculateBonus(AuraEffect const* /*aurEff*/, int32& /*amount*/, bool& /*canBeRecalculated*/)
    {
        _pctMod = 1.0f;
        Unit* caster = GetCaster();
        if (!caster)
            return;

        if (AuraEffect const* improvedGarroteStealth = caster->GetAuraEffect(SPELL_ROGUE_IMPROVED_GARROTE_AFTER_STEALTH, EFFECT_1))
            AddPct(_pctMod, improvedGarroteStealth->GetAmount());
        else if (AuraEffect const* improvedGarroteAfterStealth = caster->GetAuraEffect(SPELL_ROGUE_IMPROVED_GARROTE_STEALTH, EFFECT_1))
            AddPct(_pctMod, improvedGarroteAfterStealth->GetAmount());
    }

    void CalculateDamage(AuraEffect const* /*aurEff*/, Unit const* /*victim*/, int32& /*damage*/, int32& /*flatMod*/, float& pctMod) const
    {
        pctMod *= _pctMod;
    }

    void Register() override
    {
        DoEffectCalcAmount += AuraEffectCalcAmountFn(spell_rog_improved_garrote_damage::CalculateBonus, EFFECT_0, SPELL_AURA_PERIODIC_DAMAGE);
        DoEffectCalcDamageAndHealing += AuraEffectCalcDamageFn(spell_rog_improved_garrote_damage::CalculateDamage, EFFECT_0, SPELL_AURA_PERIODIC_DAMAGE);
    }

private:
    float _pctMod = 1.0f;
};

// 5938 - Shiv
class spell_rog_improved_shiv : public SpellScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_ROGUE_SHIV_NATURE_DAMAGE });
    }

    bool Load() override
    {
        return GetCaster()->HasAura(SPELL_ROGUE_IMPROVED_SHIV);
    }

    void HandleDummy(SpellEffIndex /*effIndex*/) const
    {
        GetCaster()->CastSpell(GetHitUnit(), SPELL_ROGUE_SHIV_NATURE_DAMAGE, CastSpellExtraArgs()
            .SetTriggerFlags(TRIGGERED_IGNORE_CAST_IN_PROGRESS | TRIGGERED_DONT_REPORT_CAST_ERROR)
            .SetTriggeringSpell(GetSpell()));
    }

    void HandleOnHit()
    {
        Unit* Caster = GetCaster();
        Unit* Target = GetHitUnit();
        Player* Owner = Caster->GetSpellModOwner();

        if (Owner == nullptr || Target == nullptr)
            return;

        if (Owner->HasAura(SPELL_ROGUE_CRIPPLING_POISON))
            Caster->CastSpell(Target, SPELL_ROGUE_CRIPPLING_POISON_DEBUFF, true);
        else if (Owner->HasAura(SPELL_ROGUE_NUMBING_POISON))
            Caster->CastSpell(Target, SPELL_ROGUE_NUMBING_POISON_DEBUFF, true);
        else if (Owner->HasAura(SPELL_ROGUE_WOUND_POISON))
            Caster->CastSpell(Target, SPELL_ROGUE_WOUND_POISON_DEBUFF, true);
        else if (Owner->HasAura(SPELL_ROGUE_INSTANT_POISON))
            Caster->CastSpell(Target, SPELL_ROGUE_INSTANT_POISON_DAMAGE, true);
        else if (Owner->HasAura(SPELL_ROGUE_ATROPHIC_POISON))
            Caster->CastSpell(Target, SPELL_ROGUE_ATROPHIC_POISON_DEBUFF, true);
    }

    void Register() override
    {
        OnEffectHitTarget += SpellEffectFn(spell_rog_improved_shiv::HandleDummy, EFFECT_0, SPELL_EFFECT_SCHOOL_DAMAGE);
    }
};

// 51690 - Killing Spree
class spell_rog_killing_spree_aura : public AuraScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo(
        {
            SPELL_ROGUE_KILLING_SPREE_TELEPORT,
            SPELL_ROGUE_KILLING_SPREE_WEAPON_DMG,
            SPELL_ROGUE_KILLING_SPREE_DMG_BUFF
        });
    }

    void HandleApply(AuraEffect const* /*aurEff*/, AuraEffectHandleModes /*mode*/)
    {
        GetTarget()->CastSpell(GetTarget(), SPELL_ROGUE_KILLING_SPREE_DMG_BUFF, true);
    }

    void HandleEffectPeriodic(AuraEffect const* /*aurEff*/)
    {
        while (!_targets.empty())
        {
            ObjectGuid guid = Trinity::Containers::SelectRandomContainerElement(_targets);
            if (Unit* target = ObjectAccessor::GetUnit(*GetTarget(), guid))
            {
                GetTarget()->CastSpell(target, SPELL_ROGUE_KILLING_SPREE_TELEPORT, true);
                GetTarget()->CastSpell(target, SPELL_ROGUE_KILLING_SPREE_WEAPON_DMG, true);
                break;
            }
            else
                _targets.remove(guid);
        }
    }

    void HandleRemove(AuraEffect const* /*aurEff*/, AuraEffectHandleModes /*mode*/)
    {
        GetTarget()->RemoveAurasDueToSpell(SPELL_ROGUE_KILLING_SPREE_DMG_BUFF);
    }

    void Register() override
    {
        AfterEffectApply += AuraEffectApplyFn(spell_rog_killing_spree_aura::HandleApply, EFFECT_0, SPELL_AURA_PERIODIC_DUMMY, AURA_EFFECT_HANDLE_REAL);
        OnEffectPeriodic += AuraEffectPeriodicFn(spell_rog_killing_spree_aura::HandleEffectPeriodic, EFFECT_0, SPELL_AURA_PERIODIC_DUMMY);
        AfterEffectRemove += AuraEffectRemoveFn(spell_rog_killing_spree_aura::HandleRemove, EFFECT_0, SPELL_AURA_PERIODIC_DUMMY, AURA_EFFECT_HANDLE_REAL);
    }

public:
    void AddTarget(Unit* target)
    {
        _targets.push_back(target->GetGUID());
    }

private:
    GuidList _targets;
};

class spell_rog_killing_spree : public SpellScript
{
    void FilterTargets(std::list<WorldObject*>& targets)
    {
        if (targets.empty() || GetCaster()->GetVehicleBase())
            FinishCast(SPELL_FAILED_OUT_OF_RANGE);
    }

    void HandleDummy(SpellEffIndex /*effIndex*/)
    {
        if (Aura* aura = GetCaster()->GetAura(SPELL_ROGUE_KILLING_SPREE))
            if (spell_rog_killing_spree_aura* script = aura->GetScript<spell_rog_killing_spree_aura>())
                script->AddTarget(GetHitUnit());
    }

    void Register() override
    {
        // Midnight 12.0.1: spell 51690 EFFECT_1 is APPLY_AURA/MOD_DECREASE_SPEED, not DUMMY; target is not area — handlers disabled
#if 0
        OnObjectAreaTargetSelect += SpellObjectAreaTargetSelectFn(spell_rog_killing_spree::FilterTargets, EFFECT_1, TARGET_UNIT_DEST_AREA_ENEMY);
        OnEffectHitTarget += SpellEffectFn(spell_rog_killing_spree::HandleDummy, EFFECT_1, SPELL_EFFECT_DUMMY);
#endif
    }
};

// 385627 - Kingsbane
class spell_rog_kingsbane : public AuraScript
{
    bool CheckProc(AuraEffect const* /*aurEff*/, ProcEventInfo& procInfo)
    {
        return procInfo.GetActionTarget()->HasAura(GetId(), GetCasterGUID());
    }

    void Register() override
    {
        // Midnight 12.0.1: spell 385627 EFFECT_4 aura is DUMMY(4), not PROC_TRIGGER_SPELL(42)
        DoCheckEffectProc += AuraCheckEffectProcFn(spell_rog_kingsbane::CheckProc, EFFECT_4, SPELL_AURA_DUMMY);
    }
};

// 2823 - Deadly Poison
// 8679 - Wound Poison
// 315584 - Instant Poison
// 381664 - Amplifying Poison
class spell_rog_leeching_poison : public AuraScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_ROGUE_LEECHING_POISON_TALENT, SPELL_ROGUE_LEECHING_POISON_AURA });
    }

    bool Load() override
    {
        return GetCaster()->HasAura(SPELL_ROGUE_LEECHING_POISON_TALENT);
    }

    void HandleOnApply(AuraEffect const* aurEff, AuraEffectHandleModes /*mode*/)
    {
        GetTarget()->CastSpell(GetTarget(), SPELL_ROGUE_LEECHING_POISON_AURA, CastSpellExtraArgsInit{
            .TriggerFlags = TRIGGERED_IGNORE_CAST_IN_PROGRESS | TRIGGERED_DONT_REPORT_CAST_ERROR,
            .TriggeringAura = aurEff
        });
    }

    void HandleOnRemove(AuraEffect const* /*aurEff*/, AuraEffectHandleModes /*mode*/)
    {
        bool hasLethalPoisonActive = GetTarget()->HasAura([](Aura const* aura) -> bool
        {
            return IsLethalPoison(aura->GetId());
        });

        if (!hasLethalPoisonActive)
            GetTarget()->RemoveAurasDueToSpell(SPELL_ROGUE_LEECHING_POISON_AURA);
    }

    void Register() override
    {
        AfterEffectApply += AuraEffectApplyFn(spell_rog_leeching_poison::HandleOnApply, EFFECT_0, SPELL_AURA_ANY, AURA_EFFECT_HANDLE_REAL);
        AfterEffectRemove += AuraEffectRemoveFn(spell_rog_leeching_poison::HandleOnRemove, EFFECT_0, SPELL_AURA_ANY, AURA_EFFECT_HANDLE_REAL);
    }
};

// 76806 - Mastery: Main Gauche
class spell_rog_mastery_main_gauche : public AuraScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_ROGUE_MAIN_GAUCHE });
    }

    bool HandleCheckProc(ProcEventInfo& eventInfo)
    {
        return eventInfo.GetDamageInfo() && eventInfo.GetDamageInfo()->GetVictim();
    }

    void HandleProc(AuraEffect* aurEff, ProcEventInfo& procInfo)
    {
        if (Unit* target = GetTarget())
            target->CastSpell(procInfo.GetDamageInfo()->GetVictim(), SPELL_ROGUE_MAIN_GAUCHE, aurEff);
    }

    void Register() override
    {
        DoCheckProc += AuraCheckProcFn(spell_rog_mastery_main_gauche::HandleCheckProc);
        OnEffectProc += AuraEffectProcFn(spell_rog_mastery_main_gauche::HandleProc, EFFECT_0, SPELL_AURA_DUMMY);
    }
};

// 277953 - Night Terrors (attached to 197835 - Shuriken Storm)
class spell_rog_night_terrors : public SpellScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo ({ SPELL_ROGUE_NIGHT_TERRORS, SPELL_ROGUE_SHADOWS_GRASP });
    }

    bool Load() override
    {
        return GetCaster()->HasAura(SPELL_ROGUE_NIGHT_TERRORS);
    }

    void HandleEnergize(SpellEffIndex /*effIndex*/) const
    {
        GetCaster()->CastSpell(GetHitUnit(), SPELL_ROGUE_SHADOWS_GRASP, CastSpellExtraArgsInit{
            .TriggerFlags = TRIGGERED_IGNORE_CAST_IN_PROGRESS | TRIGGERED_DONT_REPORT_CAST_ERROR,
            .TriggeringSpell = GetSpell()
        });
    }

    void Register() override
    {
        OnEffectHitTarget += SpellEffectFn(spell_rog_night_terrors::HandleEnergize, EFFECT_0, SPELL_EFFECT_SCHOOL_DAMAGE);
    }
};

class spell_rog_pickpocket : public SpellScript
{
    SpellCastResult CheckCast()
    {
        if (!GetExplTargetUnit() || !GetCaster()->IsValidAttackTarget(GetExplTargetUnit(), GetSpellInfo()))
            return SPELL_FAILED_BAD_TARGETS;

        return SPELL_CAST_OK;
    }

    void Register() override
    {
        OnCheckCast += SpellCheckCastFn(spell_rog_pickpocket::CheckCast);
    }
};

// 185565 - Poisoned Knife
class spell_rog_poisoned_knife : public SpellScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo
        ({
            SPELL_ROGUE_WOUND_POISON,
            SPELL_ROGUE_WOUND_POISON_DEBUFF,
            SPELL_ROGUE_DEADLY_POISON,
            SPELL_ROGUE_DEADLY_POISON_DEBUFF,
            SPELL_ROGUE_AMPLIFYING_POISON,
            SPELL_ROGUE_AMPLIFYING_POISON_DEBUFF,
            SPELL_ROGUE_CRIPPLING_POISON,
            SPELL_ROGUE_CRIPPLING_POISON_DEBUFF,
            SPELL_ROGUE_NUMBING_POISON,
            SPELL_ROGUE_NUMBING_POISON_DEBUFF,
            SPELL_ROGUE_INSTANT_POISON,
            SPELL_ROGUE_INSTANT_POISON_DAMAGE,
            SPELL_ROGUE_ATROPHIC_POISON,
            SPELL_ROGUE_ATROPHIC_POISON_DEBUFF
        });
    }

    void HandleHit(SpellEffIndex /*effIndex*/) const
    {
        Unit* caster = GetCaster();
        for (auto const& [poisonAura, debuffSpellId] : PoisonAuraToDebuff)
            if (caster->HasAura(poisonAura))
                caster->CastSpell(GetHitUnit(), debuffSpellId, CastSpellExtraArgsInit{
                    .TriggerFlags = TRIGGERED_IGNORE_CAST_IN_PROGRESS | TRIGGERED_DONT_REPORT_CAST_ERROR,
                    .TriggeringSpell = GetSpell()
                });
    }

    void Register() override
    {
        OnEffectHitTarget += SpellEffectFn(spell_rog_poisoned_knife::HandleHit, EFFECT_0, SPELL_EFFECT_SCHOOL_DAMAGE);
    }
};

// Called by 1784 - Stealth
class spell_rog_premeditation : public AuraScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_ROGUE_PREMEDITATION_PASSIVE, SPELL_ROGUE_PREMEDITATION_AURA });
    }

    bool Load() override
    {
        return GetCaster()->HasAura(SPELL_ROGUE_PREMEDITATION_PASSIVE);
    }

    void HandleEffectApply(AuraEffect const* aurEff, AuraEffectHandleModes /*mode*/)
    {
        GetCaster()->CastSpell(GetCaster(), SPELL_ROGUE_PREMEDITATION_AURA, CastSpellExtraArgsInit{
            .TriggerFlags = TRIGGERED_IGNORE_CAST_IN_PROGRESS | TRIGGERED_DONT_REPORT_CAST_ERROR,
            .TriggeringAura = aurEff
        });
    }

    void Register() override
    {
        AfterEffectApply += AuraEffectApplyFn(spell_rog_premeditation::HandleEffectApply, EFFECT_1, SPELL_AURA_DUMMY, AURA_EFFECT_HANDLE_REAL);
    }
};

// 343173 - Premeditation (proc)
class spell_rog_premeditation_proc : public AuraScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_ROGUE_PREMEDITATION_ENERGIZE });
    }

    void HandleProc(AuraEffect* aurEff, ProcEventInfo& /*procInfo*/)
    {
        GetCaster()->CastSpell(GetCaster(), SPELL_ROGUE_PREMEDITATION_ENERGIZE, CastSpellExtraArgsInit{
            .TriggerFlags = TRIGGERED_IGNORE_CAST_IN_PROGRESS | TRIGGERED_DONT_REPORT_CAST_ERROR,
            .TriggeringAura = aurEff
        });
    }

    void Register() override
    {
        OnEffectProc += AuraEffectProcFn(spell_rog_premeditation_proc::HandleProc, EFFECT_0, SPELL_AURA_DUMMY);
    }
};

// 131511 - Prey on the Weak
// Called by Cheap Shot - 1833 and Kidney Shot - 408
class spell_rog_prey_on_the_weak : public AuraScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_ROGUE_PREY_ON_THE_WEAK_TALENT, SPELL_ROGUE_PREY_ON_THE_WEAK });
    }

    void OnApply(AuraEffect const* /*aurEff*/, AuraEffectHandleModes /*mode*/) const
    {
        if (Unit* caster = GetCaster())
            if (caster->HasAura(SPELL_ROGUE_PREY_ON_THE_WEAK_TALENT))
                caster->CastSpell(GetTarget(), SPELL_ROGUE_PREY_ON_THE_WEAK, true);
    }

    void Register() override
    {
        AfterEffectApply += AuraEffectApplyFn(spell_rog_prey_on_the_weak::OnApply, EFFECT_0, SPELL_AURA_MOD_STUN, AURA_EFFECT_HANDLE_REAL);
    }
};

// 79096 - Restless Blades
class spell_rog_restless_blades : public AuraScript
{
    static uint32 constexpr Spells[] = { SPELL_ROGUE_ADRENALINE_RUSH, SPELL_ROGUE_BETWEEN_THE_EYES, SPELL_ROGUE_SPRINT,
        SPELL_ROGUE_GRAPPLING_HOOK, SPELL_ROGUE_VANISH, SPELL_ROGUE_KILLING_SPREE, SPELL_ROGUE_MARKED_FOR_DEATH };

    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo(Spells);
    }

    void HandleProc(AuraEffect* aurEff, ProcEventInfo& procInfo)
    {
        if (Optional<int32> spentCP = GetFinishingMoveCPCost(procInfo.GetProcSpell()))
        {
            int32 cdExtra = -(float(aurEff->GetAmount() * *spentCP) * 0.1f);

            SpellHistory* history = GetTarget()->GetSpellHistory();
            for (uint32 spellId : Spells)
                history->ModifyCooldown(spellId, Seconds(cdExtra), true);
        }
    }

    void Register() override
    {
        OnEffectProc += AuraEffectProcFn(spell_rog_restless_blades::HandleProc, EFFECT_0, SPELL_AURA_DUMMY);
    }
};

// 315508 - Roll the Bones
class spell_rog_roll_the_bones : public SpellScript
{
    static uint32 constexpr Spells[] = { SPELL_ROGUE_SKULL_AND_CROSSBONES, SPELL_ROGUE_GRAND_MELEE, SPELL_ROGUE_RUTHLESS_PRECISION,
        SPELL_ROGUE_TRUE_BEARING, SPELL_ROGUE_BURIED_TREASURE, SPELL_ROGUE_BROADSIDE };

    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo(Spells);
    }

    void HandleDummy(SpellEffIndex /*effIndex*/)
    {
        int32 currentDuration = 0;
        for (uint32 spellId : Spells)
        {
            if (Aura* aura = GetCaster()->GetAura(spellId))
            {
                currentDuration = aura->GetDuration();
                GetCaster()->RemoveAura(aura);
            }
        }

        std::vector<uint32> possibleBuffs(std::begin(Spells), std::end(Spells));
        Trinity::Containers::RandomShuffle(possibleBuffs);

        // https://www.icy-veins.com/wow/outlaw-rogue-pve-dps-rotation-cooldowns-abilities
        // 1 Roll the Bones buff  : 100.0 % chance;
        // 2 Roll the Bones buffs : 19 % chance;
        // 5 Roll the Bones buffs : 1 % chance.
        int32 chance = irand(1, 100);
        int32 numBuffs = 1;
        if (chance <= 1)
            numBuffs = 5;
        else if (chance <= 20)
            numBuffs = 2;

        for (int32 i = 0; i < numBuffs; ++i)
        {
            uint32 spellId = possibleBuffs[i];
            CastSpellExtraArgs args;
            args.TriggerFlags = TRIGGERED_FULL_MASK;
            args.AddSpellMod(SPELLVALUE_DURATION, GetSpellInfo()->GetDuration() + currentDuration);
            GetCaster()->CastSpell(GetCaster(), spellId, args);
        }
    }

    void Register() override
    {
        OnEffectHitTarget += SpellEffectFn(spell_rog_roll_the_bones::HandleDummy, EFFECT_0, SPELL_EFFECT_APPLY_AURA);
    }
};

// 1943 - Rupture
class spell_rog_rupture : public AuraScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_ROGUE_VENOMOUS_WOUNDS });
    }

    void OnEffectRemoved(AuraEffect const* /*aurEff*/, AuraEffectHandleModes /*mode*/)
    {
        if (GetTargetApplication()->GetRemoveMode() != AURA_REMOVE_BY_DEATH)
            return;

        Aura* aura = GetAura();
        Unit* caster = aura->GetCaster();
        if (!caster)
            return;

        Aura* auraVenomousWounds = caster->GetAura(SPELL_ROGUE_VENOMOUS_WOUNDS);
        if (!auraVenomousWounds)
            return;

        // Venomous Wounds: if unit dies while being affected by rupture, regain energy based on remaining duration
        Optional<SpellPowerCost> cost = GetSpellInfo()->CalcPowerCost(POWER_ENERGY, false, caster, GetSpellInfo()->GetSchoolMask(), nullptr);
        if (!cost)
            return;

        float pct = float(aura->GetDuration()) / float(aura->GetMaxDuration());
        int32 extraAmount = float(cost->Amount) * pct;
        caster->ModifyPower(POWER_ENERGY, extraAmount);
    }

    void Register() override
    {
        OnEffectRemove += AuraEffectRemoveFn(spell_rog_rupture::OnEffectRemoved, EFFECT_0, SPELL_AURA_PERIODIC_DAMAGE, AURA_EFFECT_HANDLE_REAL);
    }
};

// 14161 - Ruthlessness
class spell_rog_ruthlessness : public AuraScript
{
    void HandleProc(AuraEffect* aurEff, ProcEventInfo& procInfo)
    {
        Unit* target = GetTarget();

        if (Optional<int32> cost = GetFinishingMoveCPCost(procInfo.GetProcSpell()))
            if (roll_chance_i(aurEff->GetSpellEffectInfo().PointsPerResource * (*cost)))
                target->ModifyPower(POWER_COMBO_POINTS, 1);
    }

    void Register() override
    {
        OnEffectProc += AuraEffectProcFn(spell_rog_ruthlessness::HandleProc, EFFECT_0, SPELL_AURA_DUMMY);
    }
};

// 185438 - Shadowstrike
class spell_rog_shadowstrike : public SpellScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_ROGUE_PREMEDITATION_AURA, SPELL_ROGUE_SLICE_AND_DICE, SPELL_ROGUE_PREMEDITATION_PASSIVE })
            && ValidateSpellEffect({ { SPELL_ROGUE_PREMEDITATION_PASSIVE, EFFECT_0 } });
    }

    SpellCastResult HandleCheckCast()
    {
        // Because the premeditation aura is removed when we're out of stealth,
        // when we reach HandleEnergize the aura won't be there, even if it was when player launched the spell
        _hasPremeditationAura = GetCaster()->HasAura(SPELL_ROGUE_PREMEDITATION_AURA);
        return SPELL_FAILED_SUCCESS;
    }

    void HandleEnergize(SpellEffIndex /*effIndex*/)
    {
        Unit* caster = GetCaster();
        if (_hasPremeditationAura)
        {
            if (caster->HasAura(SPELL_ROGUE_SLICE_AND_DICE))
                if (Aura* premeditationPassive = caster->GetAura(SPELL_ROGUE_PREMEDITATION_PASSIVE))
                    if (AuraEffect const* auraEff = premeditationPassive->GetEffect(EFFECT_1))
                        SetHitDamage(GetHitDamage() + auraEff->GetAmount());

            // Grant 10 seconds of slice and dice
            int32 duration = sSpellMgr->AssertSpellInfo(SPELL_ROGUE_PREMEDITATION_PASSIVE, DIFFICULTY_NONE)->GetEffect(EFFECT_0).CalcValue(GetCaster());

            CastSpellExtraArgs args;
            args.TriggerFlags = TRIGGERED_FULL_MASK;
            args.AddSpellMod(SPELLVALUE_DURATION, duration * IN_MILLISECONDS);
            caster->CastSpell(caster, SPELL_ROGUE_SLICE_AND_DICE, args);
        }
    }

    void Register() override
    {
        OnCheckCast += SpellCheckCastFn(spell_rog_shadowstrike::HandleCheckCast);
        OnEffectHitTarget += SpellEffectFn(spell_rog_shadowstrike::HandleEnergize, EFFECT_1, SPELL_EFFECT_ENERGIZE);
    }

private:
    bool _hasPremeditationAura = false;
};

// Called by 1784 - Stealth and 185313 - Shadow Dance
class spell_rog_shadow_focus : public AuraScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_ROGUE_SHADOW_FOCUS, SPELL_ROGUE_SHADOW_FOCUS_EFFECT });
    }

    bool Load() override
    {
        return GetCaster()->HasAura(SPELL_ROGUE_SHADOW_FOCUS);
    }

    void HandleEffectApply(AuraEffect const* aurEff, AuraEffectHandleModes /*mode*/)
    {
        GetTarget()->CastSpell(GetTarget(), SPELL_ROGUE_SHADOW_FOCUS_EFFECT, CastSpellExtraArgsInit{
            .TriggerFlags = TRIGGERED_IGNORE_CAST_IN_PROGRESS | TRIGGERED_DONT_REPORT_CAST_ERROR,
            .TriggeringAura = aurEff
        });
    }

    void HandleEffectRemove(AuraEffect const* /*aurEff*/, AuraEffectHandleModes /*mode*/)
    {
        GetTarget()->RemoveAurasDueToSpell(SPELL_ROGUE_SHADOW_FOCUS_EFFECT);
    }

    void Register() override
    {
        AfterEffectApply += AuraEffectApplyFn(spell_rog_shadow_focus::HandleEffectApply, EFFECT_1, SPELL_AURA_ANY, AURA_EFFECT_HANDLE_REAL);
        AfterEffectRemove += AuraEffectRemoveFn(spell_rog_shadow_focus::HandleEffectRemove, EFFECT_1, SPELL_AURA_ANY, AURA_EFFECT_HANDLE_REAL);
    }
};

// 257505 - Shot in the Dark (attached to 1784 - Stealth and 185313 - Shadow Dance)
class spell_rog_shot_in_the_dark : public SpellScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_ROGUE_SHOT_IN_THE_DARK_TALENT, SPELL_ROGUE_SHOT_IN_THE_DARK_BUFF });
    }

    bool Load() override
    {
        return GetCaster()->HasAura(SPELL_ROGUE_SHOT_IN_THE_DARK_TALENT);
    }

    void HandleAfterCast() const
    {
        Unit* caster = GetCaster();
        caster->CastSpell(caster, SPELL_ROGUE_SHOT_IN_THE_DARK_BUFF, CastSpellExtraArgsInit{
            .TriggerFlags = TRIGGERED_IGNORE_CAST_IN_PROGRESS | TRIGGERED_DONT_REPORT_CAST_ERROR,
            .TriggeringSpell = GetSpell()
        });
    }

    void Register() override
    {
        AfterCast += SpellCastFn(spell_rog_shot_in_the_dark::HandleAfterCast);
    }
};

// 257506 - Shot in the Dark (attached to 185422 - Shadow Dance and 158185 - Stealth)
class spell_rog_shot_in_the_dark_buff : public AuraScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_ROGUE_SHOT_IN_THE_DARK_BUFF });
    }

    void HandleEffectRemove(AuraEffect const* /*aurEff*/, AuraEffectHandleModes /*mode*/) const
    {
        GetTarget()->RemoveAurasDueToSpell(SPELL_ROGUE_SHOT_IN_THE_DARK_BUFF);
    }

    void Register() override
    {
        AfterEffectRemove += AuraEffectRemoveFn(spell_rog_shot_in_the_dark_buff::HandleEffectRemove, EFFECT_0, SPELL_AURA_ANY, AURA_EFFECT_HANDLE_REAL);
    }
};

// 197835 - Shuriken Storm
class spell_rog_shuriken_storm : public SpellScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo ({ SPELL_ROGUE_SHURIKEN_STORM_ENERGIZE });
    }

    void HandleEnergize(SpellEffIndex effIndex) const
    {
        GetCaster()->CastSpell(GetCaster(), SPELL_ROGUE_SHURIKEN_STORM_ENERGIZE, CastSpellExtraArgsInit{
            .TriggerFlags = TRIGGERED_IGNORE_CAST_IN_PROGRESS | TRIGGERED_DONT_REPORT_CAST_ERROR,
            .TriggeringSpell = GetSpell(),
            .SpellValueOverrides = { { SPELLVALUE_BASE_POINT0, static_cast<int32>(GetUnitTargetCountForEffect(effIndex))  } }
        });
    }

    void Register() override
    {
        OnEffectHit += SpellEffectFn(spell_rog_shuriken_storm::HandleEnergize, EFFECT_0, SPELL_EFFECT_SCHOOL_DAMAGE);
    }
};

// 277925 - Shuriken Tornado
class spell_rog_shuriken_tornado : public AuraScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_ROGUE_SHURIKEN_STORM_DAMAGE });
    }

    void HandlePeriodicEffect(AuraEffect const* aurEff) const
    {
        GetTarget()->CastSpell(nullptr, SPELL_ROGUE_SHURIKEN_STORM_DAMAGE, CastSpellExtraArgsInit{
            .TriggerFlags = TRIGGERED_IGNORE_GCD | TRIGGERED_IGNORE_POWER_COST | TRIGGERED_IGNORE_CAST_IN_PROGRESS | TRIGGERED_DONT_REPORT_CAST_ERROR,
            .TriggeringAura = aurEff
        });
    }

    void Register() override
    {
        OnEffectPeriodic += AuraEffectPeriodicFn(spell_rog_shuriken_tornado::HandlePeriodicEffect, EFFECT_0, SPELL_AURA_PERIODIC_TRIGGER_SPELL);
    }
};

// 193315 - Sinister Strike
class spell_rog_sinister_strike : public SpellScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_ROGUE_T5_2P_SET_BONUS });
    }

    void HandleDummy(SpellEffIndex /*effIndex*/)
    {
        int32 damagePerCombo = GetHitDamage();
        if (AuraEffect const* t5 = GetCaster()->GetAuraEffect(SPELL_ROGUE_T5_2P_SET_BONUS, EFFECT_0))
            damagePerCombo += t5->GetAmount();

        int32 finalDamage = damagePerCombo;
        if (Optional<int32> comboPointCost = GetSpell()->GetPowerTypeCostAmount(POWER_COMBO_POINTS))
            finalDamage *= *comboPointCost;

        SetHitDamage(finalDamage);
    }

    void Register() override
    {
        // Midnight 12.0.1: spell 193315 has no EFFECT_2 (only 2 effects) — handler disabled
#if 0
        OnEffectHitTarget += SpellEffectFn(spell_rog_sinister_strike::HandleDummy, EFFECT_2, SPELL_EFFECT_DUMMY);
#endif
    }
};

// Called by 1856 - Vanish
class spell_rog_soothing_darkness : public SpellScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_ROGUE_SOOTHING_DARKNESS_TALENT, SPELL_ROGUE_SOOTHING_DARKNESS_HEAL });
    }

    bool Load() override
    {
        return GetCaster()->HasAura(SPELL_ROGUE_SOOTHING_DARKNESS_TALENT);
    }

    void Heal() const
    {
        GetCaster()->CastSpell(GetCaster(), SPELL_ROGUE_SOOTHING_DARKNESS_HEAL, CastSpellExtraArgsInit{
            .TriggerFlags = TRIGGERED_IGNORE_CAST_IN_PROGRESS | TRIGGERED_DONT_REPORT_CAST_ERROR,
            .TriggeringSpell = GetSpell()
        });
    }

    void Register() override
    {
        AfterCast += SpellCastFn(spell_rog_soothing_darkness::Heal);
    }
};

// 1784 - Stealth
class spell_rog_stealth : public AuraScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo(
        {
            SPELL_ROGUE_SANCTUARY,
            SPELL_ROGUE_STEALTH_STEALTH_AURA,
            SPELL_ROGUE_STEALTH_SHAPESHIFT_AURA
        });
    }

    void HandleEffectApply(AuraEffect const* aurEff, AuraEffectHandleModes /*mode*/)
    {
        Unit* target = GetTarget();

        target->CastSpell(target, SPELL_ROGUE_SANCTUARY, CastSpellExtraArgsInit{
            .TriggerFlags = TRIGGERED_IGNORE_CAST_IN_PROGRESS | TRIGGERED_DONT_REPORT_CAST_ERROR,
            .TriggeringAura = aurEff
        });
        target->CastSpell(target, SPELL_ROGUE_STEALTH_STEALTH_AURA, CastSpellExtraArgsInit{
            .TriggerFlags = TRIGGERED_IGNORE_CAST_IN_PROGRESS | TRIGGERED_DONT_REPORT_CAST_ERROR,
            .TriggeringAura = aurEff
        });
        target->CastSpell(target, SPELL_ROGUE_STEALTH_SHAPESHIFT_AURA, CastSpellExtraArgsInit{
            .TriggerFlags = TRIGGERED_IGNORE_CAST_IN_PROGRESS | TRIGGERED_DONT_REPORT_CAST_ERROR,
            .TriggeringAura = aurEff
        });
    }

    void HandleEffectRemove(AuraEffect const* /*aurEff*/, AuraEffectHandleModes /*mode*/)
    {
        Unit* target = GetTarget();

        target->RemoveAurasDueToSpell(SPELL_ROGUE_STEALTH_STEALTH_AURA);
        target->RemoveAurasDueToSpell(SPELL_ROGUE_STEALTH_SHAPESHIFT_AURA);
    }

    void Register() override
    {
        AfterEffectApply += AuraEffectApplyFn(spell_rog_stealth::HandleEffectApply, EFFECT_1, SPELL_AURA_DUMMY, AURA_EFFECT_HANDLE_REAL);
        AfterEffectRemove += AuraEffectRemoveFn(spell_rog_stealth::HandleEffectRemove, EFFECT_1, SPELL_AURA_DUMMY, AURA_EFFECT_HANDLE_REAL);
    }
};

// 212283 - Symbols of Death
class spell_rog_symbols_of_death : public SpellScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_ROGUE_SYMBOLS_OF_DEATH_RANK2, SPELL_ROGUE_SYMBOLS_OF_DEATH_CRIT_AURA });
    }

    void HandleEffectHitTarget(SpellEffIndex /*effIndex*/)
    {
        if (GetCaster()->HasAura(SPELL_ROGUE_SYMBOLS_OF_DEATH_RANK2))
            GetCaster()->CastSpell(GetCaster(), SPELL_ROGUE_SYMBOLS_OF_DEATH_CRIT_AURA, true);
    }

    void Register() override
    {
        OnEffectHitTarget += SpellEffectFn(spell_rog_symbols_of_death::HandleEffectHitTarget, EFFECT_0, SPELL_EFFECT_APPLY_AURA);
    }
};

// 57934 - Tricks of the Trade
class spell_rog_tricks_of_the_trade_aura : public AuraScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_ROGUE_TRICKS_OF_THE_TRADE_PROC });
    }

    void OnRemove(AuraEffect const* /*aurEff*/, AuraEffectHandleModes /*mode*/)
    {
        if (GetTargetApplication()->GetRemoveMode() != AURA_REMOVE_BY_DEFAULT || !GetTarget()->HasAura(SPELL_ROGUE_TRICKS_OF_THE_TRADE_PROC))
            GetTarget()->GetThreatManager().UnregisterRedirectThreat(SPELL_ROGUE_TRICKS_OF_THE_TRADE);
    }

    void HandleProc(AuraEffect* aurEff, ProcEventInfo& /*eventInfo*/)
    {
        PreventDefaultAction();

        Unit* rogue = GetTarget();
        if (ObjectAccessor::GetUnit(*rogue, _redirectTarget))
            rogue->CastSpell(rogue, SPELL_ROGUE_TRICKS_OF_THE_TRADE_PROC, aurEff);
        Remove(AURA_REMOVE_BY_DEFAULT);
    }

    void Register() override
    {
        AfterEffectRemove += AuraEffectRemoveFn(spell_rog_tricks_of_the_trade_aura::OnRemove, EFFECT_1, SPELL_AURA_DUMMY, AURA_EFFECT_HANDLE_REAL);
        OnEffectProc += AuraEffectProcFn(spell_rog_tricks_of_the_trade_aura::HandleProc, EFFECT_1, SPELL_AURA_DUMMY);
    }

    ObjectGuid _redirectTarget;
public:
    void SetRedirectTarget(ObjectGuid guid) { _redirectTarget = guid; }
};

// 57934 - Tricks of the Trade
class spell_rog_tricks_of_the_trade : public SpellScript
{
    void DoAfterHit()
    {
        if (Aura* aura = GetHitAura())
            if (spell_rog_tricks_of_the_trade_aura* script = aura->GetScript<spell_rog_tricks_of_the_trade_aura>())
            {
                if (Unit* explTarget = GetExplTargetUnit())
                    script->SetRedirectTarget(explTarget->GetGUID());
                else
                    script->SetRedirectTarget(ObjectGuid::Empty);
            }
    }

    void Register() override
    {
        AfterHit += SpellHitFn(spell_rog_tricks_of_the_trade::DoAfterHit);
    }
};

// 59628 - Tricks of the Trade (Proc)
class spell_rog_tricks_of_the_trade_proc : public AuraScript
{
    void HandleRemove(AuraEffect const* /*aurEff*/, AuraEffectHandleModes /*mode*/)
    {
        GetTarget()->GetThreatManager().UnregisterRedirectThreat(SPELL_ROGUE_TRICKS_OF_THE_TRADE);
    }

    void Register() override
    {
        AfterEffectRemove += AuraEffectRemoveFn(spell_rog_tricks_of_the_trade_proc::HandleRemove, EFFECT_0, SPELL_AURA_DUMMY, AURA_EFFECT_HANDLE_REAL);
    }
};

// 198020 - Turn the Tables (PvP Talent)
class spell_rog_turn_the_tables : public AuraScript
{
    bool CheckForStun(AuraEffect const* /*aurEff*/, ProcEventInfo& eventInfo)
    {
        return eventInfo.GetProcSpell() && eventInfo.GetProcSpell()->GetSpellInfo()->HasAura(SPELL_AURA_MOD_STUN);
    }

    void Register() override
    {
        DoCheckEffectProc += AuraCheckEffectProcFn(spell_rog_turn_the_tables::CheckForStun, EFFECT_0, SPELL_AURA_PROC_TRIGGER_SPELL);
    }
};

// 198023 - Turn the Tables (periodic)
class spell_rog_turn_the_tables_periodic_check : public AuraScript
{
    bool Validate(SpellInfo const*) override
    {
        return ValidateSpellInfo({ SPELL_ROGUE_TURN_THE_TABLES_BUFF });
    }

    void CheckForStun(AuraEffect const* aurEff)
    {
        Unit* target = GetTarget();
        if (!target->HasAuraType(SPELL_AURA_MOD_STUN))
        {
            target->CastSpell(target, SPELL_ROGUE_TURN_THE_TABLES_BUFF, aurEff);
            PreventDefaultAction();
            Remove();
        }
    }

    void Register() override
    {
        OnEffectPeriodic += AuraEffectPeriodicFn(spell_rog_turn_the_tables_periodic_check::CheckForStun, EFFECT_0, SPELL_AURA_PERIODIC_DUMMY);
    }
};

// 1856 - Vanish - SPELL_ROGUE_VANISH
class spell_rog_vanish : public SpellScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_ROGUE_VANISH_AURA, SPELL_ROGUE_STEALTH_SHAPESHIFT_AURA });
    }

    void OnLaunchTarget(SpellEffIndex effIndex)
    {
        PreventHitDefaultEffect(effIndex);

        Unit* target = GetHitUnit();

        target->RemoveAurasByType(SPELL_AURA_MOD_STALKED);
        if (target->GetTypeId() != TYPEID_PLAYER)
            return;

        if (target->HasAura(SPELL_ROGUE_VANISH_AURA))
            return;

        target->CastSpell(target, SPELL_ROGUE_VANISH_AURA, TRIGGERED_FULL_MASK);
        target->CastSpell(target, SPELL_ROGUE_STEALTH_SHAPESHIFT_AURA, TRIGGERED_FULL_MASK);
    }

    void Register() override
    {
        OnEffectLaunchTarget += SpellEffectFn(spell_rog_vanish::OnLaunchTarget, EFFECT_1, SPELL_EFFECT_TRIGGER_SPELL);
    }
};

// 11327 - Vanish
class spell_rog_vanish_aura : public AuraScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_ROGUE_STEALTH });
    }

    void HandleEffectRemove(AuraEffect const* /*aurEff*/, AuraEffectHandleModes /*mode*/)
    {
        GetTarget()->CastSpell(GetTarget(), SPELL_ROGUE_STEALTH, TRIGGERED_FULL_MASK);
    }

    void Register() override
    {
        AfterEffectRemove += AuraEffectRemoveFn(spell_rog_vanish_aura::HandleEffectRemove, EFFECT_0, SPELL_AURA_DUMMY, AURA_EFFECT_HANDLE_REAL);
    }
};

// 79134 - Venomous Wounds
class spell_rog_venomous_wounds : public AuraScript
{
    void HandleProc(AuraEffect* aurEff, ProcEventInfo& /*eventInfo*/)
    {
        int32 extraEnergy = aurEff->GetAmount();
        GetTarget()->ModifyPower(POWER_ENERGY, extraEnergy);
    }

    void Register() override
    {
        OnEffectProc += AuraEffectProcFn(spell_rog_venomous_wounds::HandleProc, EFFECT_1, SPELL_AURA_DUMMY);
    }
};

// Stealth (with subterfuge) - 115191
class spell_rog_stealth_with_subterfuge : public AuraScript
{
    void OnRemove(AuraEffect const* /*aurEff*/, AuraEffectHandleModes /*mode*/)
    {
        if (!GetCaster())
            return;

        GetCaster()->RemoveAura(115191);
        GetCaster()->RemoveAura(115192);
    }

    void Register()
    {
        // Midnight 12.0.1: spell 115191 EFFECT_0 aura is DUMMY(4), not MOD_SHAPESHIFT(36)
        AfterEffectRemove += AuraEffectRemoveFn(spell_rog_stealth_with_subterfuge::OnRemove, EFFECT_0, SPELL_AURA_DUMMY, AURA_EFFECT_HANDLE_REAL);
    }
};

// Called by Stealth - 1784
// Nightstalker - 14062
class spell_rog_nightstalker : public SpellScriptLoader
{
public:
    spell_rog_nightstalker() : SpellScriptLoader("spell_rog_nightstalker") { }

    class spell_rog_nightstalker_SpellScript : public SpellScript
    {
        void HandleOnHit()
        {
            if (Player* _player = GetCaster()->ToPlayer())
            {
                if (_player->HasAura(SPELL_ROGUE_SPELL_NIGHTSTALKER_AURA))
                    _player->CastSpell(_player, SPELL_ROGUE_SPELL_NIGHTSTALKER_DAMAGE_DONE, true);

                if (_player->HasAura(SPELL_ROGUE_SHADOW_FOCUS))
                    _player->CastSpell(_player, SPELL_ROGUE_SHADOW_FOCUS_EFFECT, true);
            }
        }

        void Register()
        {
            OnHit += SpellHitFn(spell_rog_nightstalker_SpellScript::HandleOnHit);
        }
    };

    SpellScript* GetSpellScript() const
    {
        return new spell_rog_nightstalker_SpellScript();
    }

    class spell_rog_nightstalker_AuraScript : public AuraScript
    {
        void HandleRemove(AuraEffect const* /*aurEff*/, AuraEffectHandleModes /*mode*/)
        {
            if (GetCaster())
            {
                if (GetCaster()->HasAura(SPELL_ROGUE_SHADOW_FOCUS_EFFECT))
                    GetCaster()->RemoveAura(SPELL_ROGUE_SHADOW_FOCUS_EFFECT);
            }
        }

        void Register()
        {
            AfterEffectRemove += AuraEffectRemoveFn(spell_rog_nightstalker_AuraScript::HandleRemove, EFFECT_0, SPELL_AURA_MOD_SHAPESHIFT, AURA_EFFECT_HANDLE_REAL);
        }
    };

    AuraScript* GetAuraScript() const
    {
        return new spell_rog_nightstalker_AuraScript();
    }
};

// 315496 - Slice and Dice
class spell_rog_slice_and_dice : public SpellScript
{

    void HandleAfterHit()
    {
        if (Player* _player = GetCaster()->ToPlayer())
        {
            if (Aura* sliceAndDice = _player->GetAura(SPELL_ROGUE_SLICE_AND_DICE))
            {
                std::vector<SpellPowerCost> const& costs = GetSpell()->GetPowerCost();
                auto c = std::find_if(costs.begin(), costs.end(), [](SpellPowerCost const& cost) { return cost.Power == POWER_COMBO_POINTS; });
                if (c != costs.end())
                {
                    if (c->Amount == 1)
                        sliceAndDice->SetDuration(12 * IN_MILLISECONDS);
                    else if (c->Amount == 2)
                        sliceAndDice->SetDuration(18 * IN_MILLISECONDS);
                    else if (c->Amount == 3)
                        sliceAndDice->SetDuration(24 * IN_MILLISECONDS);
                    else if (c->Amount == 4)
                        sliceAndDice->SetDuration(30 * IN_MILLISECONDS);
                    else if (c->Amount == 5)
                        sliceAndDice->SetDuration(36 * IN_MILLISECONDS);
                }
            }
        }
    }

    void Register() override
    {
        AfterHit += SpellHitFn(spell_rog_slice_and_dice::HandleAfterHit);
    }
};

enum ePoisons
{
    WoundPoison = 8679,
    MindNumbingPoison = 5761,
    CripplingPoison = 3408,
    LeechingPoison = 108211,
    ParalyticPoison = 108215,
    DeadlyPoison = 2823,
    InstantPoison = 315584,
};

class spell_rog_poisons : public SpellScript
{
    void RemovePreviousPoisons()
    {
        if (Player* plr = GetCaster()->ToPlayer())
        {
            if (plr->HasAura(WoundPoison))
                plr->RemoveAura(WoundPoison);
            if (plr->HasAura(MindNumbingPoison))
                plr->RemoveAura(MindNumbingPoison);
            if (plr->HasAura(CripplingPoison))
                plr->RemoveAura(CripplingPoison);
            if (plr->HasAura(LeechingPoison))
                plr->RemoveAura(LeechingPoison);
            if (plr->HasAura(ParalyticPoison))
                plr->RemoveAura(ParalyticPoison);
            if (plr->HasAura(DeadlyPoison))
                plr->RemoveAura(DeadlyPoison);
            if (plr->HasAura(InstantPoison))
                plr->RemoveAura(InstantPoison);
        }
    }

    void HandleBeforeHit(SpellMissInfo missInfo)
    {
        if (missInfo != SPELL_MISS_NONE)
            return;

        if (Player* _player = GetCaster()->ToPlayer())
        {
            RemovePreviousPoisons();
        }
    }

    void Register() override
    {
        BeforeHit += BeforeSpellHitFn(spell_rog_poisons::HandleBeforeHit);
    }
};

// 385616 - Echoing Reprimand
class spell_rog_echoing_reprimand : public SpellScript
{

    void HandleOnHit()
    {
        Unit* caster = GetCaster();

        caster->AddAura(394136, caster);
    }

    void Register() override
    {
        OnHit += SpellHitFn(spell_rog_echoing_reprimand::HandleOnHit);
    }
};

//
class spell_rog_deadly_poison_instant_damage : public SpellScriptLoader
{
public:
    spell_rog_deadly_poison_instant_damage() : SpellScriptLoader("spell_rog_deadly_poison_instant_damage") { }

    class spell_rog_deadly_poison_instant_damage_SpellScript : public SpellScript
    {
        void HandleOnCast()
        {
            if (Player* _player = GetCaster()->ToPlayer())
                if (Unit* target = GetExplTargetUnit())
                    if (target->HasAura(SPELL_ROGUE_DEADLY_POISON_DEBUFF, _player->GetGUID()))
                        _player->CastSpell(target, SPELL_ROGUE_DEADLY_POISON_INSTANT_DAMAGE, true);
        }

        void Register() override
        {
            OnCast += SpellCastFn(spell_rog_deadly_poison_instant_damage_SpellScript::HandleOnCast);
        }
    };

    SpellScript* GetSpellScript() const override
    {
        return new spell_rog_deadly_poison_instant_damage_SpellScript();
    }
};

// Fan of Knives - 51723
class spell_rog_fan_of_knives : public SpellScript
{
public:

    spell_rog_fan_of_knives()
    {
        _hit = false;
    }

private:

    bool _hit;

    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo(
            {
                SPELL_ROGUE_FAN_OF_KNIVES
            });
    }

    bool Load() override
    {
        return true;
    }

    void AddCp()
    {
        if (!_hit)
        {
            uint8 cp = GetCaster()->GetPower(POWER_COMBO_POINTS);
            if (cp < GetCaster()->GetMaxPower(POWER_COMBO_POINTS))
            {
                GetCaster()->SetPower(POWER_COMBO_POINTS, cp + 1);
            }
            _hit = true;
        }
    }

    void RemoveKS()
    {
        Unit* target = GetHitUnit();
        if (target->HasAura(SPELL_ROGUE_KILLING_SPREE)) //Killing spree debuff #1
            target->RemoveAura(SPELL_ROGUE_KILLING_SPREE);
        if (target->HasAura(SPELL_ROGUE_KILLING_SPREE_DMG_BUFF)) //Killing spree debuff #2
            target->RemoveAura(SPELL_ROGUE_KILLING_SPREE_DMG_BUFF);
    }

    void Register() override
    {
        OnHit += SpellHitFn(spell_rog_fan_of_knives::AddCp);
        AfterHit += SpellHitFn(spell_rog_fan_of_knives::RemoveKS);
    }
};

class spell_rog_garrote : public SpellScriptLoader
{
public:
    spell_rog_garrote() : SpellScriptLoader("spell_rog_garrote") {}

    class spell_rog_garrote_SpellScript : public SpellScript
    {

    public:
        spell_rog_garrote_SpellScript()
        {
            _stealthed = false;
        }

    private:

        bool _stealthed;

        bool Validate(SpellInfo const* /*spellInfo*/) override
        {
            return ValidateSpellInfo(
                {
                    SPELL_ROGUE_GARROTE_DOT,
                    SPELL_ROGUE_GARROTE_SILENCE
                });
        }

        bool Load() override
        {
            if (GetCaster()->HasAuraType(SPELL_AURA_MOD_STEALTH))
                _stealthed = true;
            return true;
        }

        void HandleOnHit()
        {
            Unit* caster = GetCaster();
            Unit* target = GetExplTargetUnit();

            if (_stealthed)
            {
                caster->CastSpell(target, SPELL_ROGUE_GARROTE_SILENCE, true);
            }
        }

        void Register() override
        {
            OnHit += SpellHitFn(spell_rog_garrote_SpellScript::HandleOnHit);
        }
    };

    class spell_rog_garrote_AuraScript : public AuraScript
    {

        bool Validate(SpellInfo const* /*spellInfo*/) override
        {
            return ValidateSpellInfo(
                {
                    SPELL_ROGUE_THUGGEE
                });
        }

        void HandleRemove(AuraEffect const* /*aurEff*/, AuraEffectHandleModes /*mode*/)
        {
            if (GetTargetApplication()->GetRemoveMode() != AURA_REMOVE_BY_DEATH)
                return;

            Unit* caster = GetAura()->GetCaster();

            if (!caster)
                return;

            if (!caster->HasAura(SPELL_ROGUE_THUGGEE))
                return;

            caster->GetSpellHistory()->ResetCooldown(SPELL_ROGUE_GARROTE_DOT, true);
        }

        void Register() override
        {
            AfterEffectRemove += AuraEffectRemoveFn(spell_rog_garrote_AuraScript::HandleRemove, EFFECT_0, SPELL_AURA_PERIODIC_DAMAGE, AURA_EFFECT_HANDLE_REAL);
        }
    };

    SpellScript* GetSpellScript() const override
    {
        return new spell_rog_garrote_SpellScript();
    }

    AuraScript* GetAuraScript() const override
    {
        return new spell_rog_garrote_AuraScript();
    }
};

// Grappling Hook - 195457
class spell_rog_grappling_hook : public SpellScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo(
            {
                SPELL_ROGUE_GRAPPLING_HOOK,
                SPELL_ROGUE_GRAPPLING_HOOK_TRIGGER
            });
    }

    void HandleDummy()
    {
        Unit* caster = GetCaster();
        WorldLocation const* dest = GetExplTargetDest();
        if (!caster || !dest)
            return;

        caster->CastSpell(Position(dest->GetPositionX(), dest->GetPositionY(), dest->GetPositionZ()), SPELL_ROGUE_GRAPPLING_HOOK_TRIGGER, true);
    }

    void Register() override
    {
        OnCast += SpellCastFn(spell_rog_grappling_hook::HandleDummy);
    }
};

// 385616 - Blade Rush
class spell_rog_blade_rush : public SpellScript
{
    void HandleDummy(SpellEffIndex /*effIndex*/)
    {
        Unit* caster = GetCaster();
        Unit* target = GetExplTargetUnit();

        caster->CastSpell(target, 271878, true);
        caster->CastSpell(target, 271881, true);
        caster->AddAura(271896, caster);
    }

    void Register() override
    {
        OnEffectHitTarget += SpellEffectFn(spell_rog_blade_rush::HandleDummy, EFFECT_0, SPELL_EFFECT_DUMMY);
    }
};

// 280719 Secret Technique
class spell_rog_secret_technique : public SpellScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ 280720 });
    }

    void HandleDummy(SpellEffIndex /*effIndex*/)
    {
        Unit* caster = GetCaster();
        Unit* target = GetHitUnit();

        if (!caster || !target)
            return;

        int32 comboPoints = 0;
        if (Spell* spell = GetSpell())
            comboPoints = spell->GetUsedComboPoints();

        if (comboPoints == 0)
            comboPoints = 1;

        SpellInfo const* damageSpellInfo = sSpellMgr->GetSpellInfo(280720, DIFFICULTY_NONE);
        if (!damageSpellInfo)
            return;

        float apCoefficient = damageSpellInfo->GetEffect(EFFECT_0).BonusCoefficientFromAP;

        float ap = caster->GetTotalAttackPowerValue(BASE_ATTACK);

        int32 damage = static_cast<int32>(ap * apCoefficient * comboPoints);

        CastSpellExtraArgs args(TRIGGERED_FULL_MASK);
        args.AddSpellBP0(damage);

        caster->CastSpell(target, 280720, args);
    }

    void HandleAfterCast()
    {
        Unit* caster = GetCaster();
        if (!caster)
            return;

        if (SpellHistory* spellHistory = caster->GetSpellHistory())
        {
            if (SpellInfo const* spellInfo = GetSpellInfo())
            {
                int32 comboPoints = 0;
                if (Spell* spell = GetSpell())
                    comboPoints = spell->GetUsedComboPoints();

                if (comboPoints == 0) comboPoints = 1;

                int32 cdReductionPerPoint = spellInfo->GetEffect(EFFECT_4).CalcValue(caster);

                int32 totalReduction = cdReductionPerPoint * comboPoints * IN_MILLISECONDS;

                spellHistory->ModifyCooldown(SPELL_ROGUE_SECRET_TECHNIQUE, Milliseconds(-totalReduction));
            }
        }
    }

    void Register() override
    {
        OnEffectHitTarget += SpellEffectFn(spell_rog_secret_technique::HandleDummy, EFFECT_0, SPELL_EFFECT_DUMMY);
        AfterCast += SpellCastFn(spell_rog_secret_technique::HandleAfterCast);
    }
};

// 91023 - Find Weakness
class spell_rog_find_weakness : public AuraScript
{
    bool CheckProc(ProcEventInfo& eventInfo)
    {
        if (!eventInfo.GetSpellInfo())
            return false;

        uint32 spellId = eventInfo.GetSpellInfo()->Id;

        if (spellId == SPELL_ROGUE_SHADOWSTRIKE ||
            spellId == SPELL_ROGUE_CHEAP_SHOT ||
            spellId == SPELL_ROGUE_SHURIKEN_STORM ||
            spellId == SPELL_ROGUE_BACKSTAB)
        {
            return true;
        }

        return false;
    }

    void HandleProc(AuraEffect* aurEff, ProcEventInfo& procInfo)
    {

        Unit* caster = GetCaster();
        if (!caster)
            return;

        Unit* target = procInfo.GetActionTarget();
        if (!target || !target->IsAlive())
            return;

        int32 amount = aurEff->GetAmount();

        CastSpellExtraArgs args(TRIGGERED_FULL_MASK);
        args.AddSpellBP0(amount);

        caster->CastSpell(target, 91021, args);
    }

    void Register() override
    {
        DoCheckProc += AuraCheckProcFn(spell_rog_find_weakness::CheckProc);
        OnEffectProc += AuraEffectProcFn(spell_rog_find_weakness::HandleProc, EFFECT_0, SPELL_AURA_DUMMY);
    }
};

void AddSC_rogue_spell_scripts()
{
    RegisterSpellScript(spell_rog_acrobatic_strikes);
    RegisterSpellScript(spell_rog_airborne_irritant);
    RegisterSpellScript(spell_rog_airborne_irritant_target_selection);
    RegisterSpellScript(spell_rog_backstab);
    RegisterSpellScript(spell_rog_blackjack);
    RegisterSpellScript(spell_rog_blade_flurry);
    RegisterSpellScript(spell_rog_cheat_death);
    RegisterSpellScript(spell_rog_cloaked_in_shadows);
    RegisterSpellScript(spell_rog_deadly_poison);
    RegisterSpellScript(spell_rog_deepening_shadows);
    RegisterSpellScript(spell_rog_envenom);
    RegisterSpellScript(spell_rog_eviscerate);
    RegisterSpellScript(spell_rog_grand_melee);
    RegisterSpellScript(spell_rog_honor_among_thieves);
    RegisterSpellScript(spell_rog_improved_garrote);
    RegisterSpellScript(spell_rog_improved_garrote_damage);
    RegisterSpellScript(spell_rog_improved_shiv);
    RegisterSpellAndAuraScriptPair(spell_rog_killing_spree, spell_rog_killing_spree_aura);
    RegisterSpellScript(spell_rog_kingsbane);
    RegisterSpellScript(spell_rog_leeching_poison);
    RegisterSpellScript(spell_rog_mastery_main_gauche);
    RegisterSpellScript(spell_rog_night_terrors);
    RegisterSpellScript(spell_rog_pickpocket);
    RegisterSpellScript(spell_rog_poisoned_knife);
    RegisterSpellScript(spell_rog_premeditation);
    RegisterSpellScript(spell_rog_premeditation_proc);
    RegisterSpellScript(spell_rog_prey_on_the_weak);
    RegisterSpellScript(spell_rog_restless_blades);
    RegisterSpellScript(spell_rog_roll_the_bones);
    RegisterSpellScript(spell_rog_rupture);
    RegisterSpellScript(spell_rog_ruthlessness);
    RegisterSpellScript(spell_rog_shadowstrike);
    RegisterSpellScript(spell_rog_shadow_focus);
    RegisterSpellScript(spell_rog_shot_in_the_dark);
    RegisterSpellScript(spell_rog_shot_in_the_dark_buff);
    RegisterSpellScript(spell_rog_shuriken_storm);
    RegisterSpellScript(spell_rog_shuriken_tornado);
    RegisterSpellScript(spell_rog_sinister_strike);
    RegisterSpellScript(spell_rog_soothing_darkness);
    RegisterSpellScript(spell_rog_stealth);
    RegisterSpellScript(spell_rog_symbols_of_death);
    RegisterSpellAndAuraScriptPair(spell_rog_tricks_of_the_trade, spell_rog_tricks_of_the_trade_aura);
    RegisterSpellScript(spell_rog_tricks_of_the_trade_proc);
    RegisterSpellScript(spell_rog_turn_the_tables);
    RegisterSpellScript(spell_rog_turn_the_tables_periodic_check);
    RegisterSpellScript(spell_rog_vanish);
    RegisterSpellScript(spell_rog_vanish_aura);
    RegisterSpellScript(spell_rog_venomous_wounds);

    //new
    RegisterSpellScript(spell_rog_stealth_with_subterfuge);
    new spell_rog_nightstalker();
    RegisterSpellScript(spell_rog_slice_and_dice);
    RegisterSpellScript(spell_rog_echoing_reprimand);
    new spell_rog_deadly_poison_instant_damage();
    RegisterSpellScript(spell_rog_poisons); //unused
    RegisterSpellScript(spell_rog_fan_of_knives);
    RegisterSpellScript(spell_rog_grappling_hook);
    RegisterSpellScript(spell_rog_blade_rush);
    RegisterSpellScript(spell_rog_secret_technique);
    RegisterSpellScript(spell_rog_find_weakness);
}
