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
 * Scripts for spells with SPELLFAMILY_MONK and SPELLFAMILY_GENERIC spells used by monk players.
 * Scriptnames of files in this file should be prefixed with "spell_monk_".
 */

#include "ScriptMgr.h"
#include "DB2Stores.h"
#include "Player.h"
#include "Spell.h"
#include "SpellAuraEffects.h"
#include "SpellInfo.h"
#include "SpellMgr.h"
#include "SpellScript.h"

#include "AreaTrigger.h"
#include "AreaTriggerAI.h"
#include "GridNotifiers.h"
#include "SpellHistory.h"
#include "SpellPackets.h"
#include "PhasingHandler.h"
#include "TemporarySummon.h"
#include "PetAI.h"
#include "DynamicObject.h"
#include "Containers.h"
#include "ObjectMgr.h"
#include <numeric>
#include <iostream>

enum MonkSpells
{
    SPELL_MONK_CALMING_COALESCENCE                      = 388220,
    SPELL_MONK_CRACKLING_JADE_LIGHTNING_CHANNEL         = 117952,
    SPELL_MONK_CRACKLING_JADE_LIGHTNING_CHI_PROC        = 123333,
    SPELL_MONK_CRACKLING_JADE_LIGHTNING_KNOCKBACK       = 117962,
    SPELL_MONK_CRACKLING_JADE_LIGHTNING_KNOCKBACK_CD    = 117953,
    SPELL_MONK_POWER_STRIKE_PROC                        = 129914,
    SPELL_MONK_POWER_STRIKE_ENERGIZE                    = 121283,
    SPELL_MONK_PROVOKE_SINGLE_TARGET                    = 116189,
    SPELL_MONK_PROVOKE_AOE                              = 118635,
    SPELL_MONK_NO_FEATHER_FALL                          = 79636,
    SPELL_MONK_OPEN_PALM_STRIKES_TALENT                 = 392970,
    SPELL_MONK_ROLL_BACKWARD                            = 109131,
    SPELL_MONK_ROLL_FORWARD                             = 107427,
    SPELL_MONK_SOOTHING_MIST                            = 115175,
    SPELL_MONK_STANCE_OF_THE_SPIRITED_CRANE             = 154436,
    SPELL_MONK_STAGGER_DAMAGE_AURA                      = 124255,
    SPELL_MONK_STAGGER_HEAVY                            = 124273,
    SPELL_MONK_STAGGER_LIGHT                            = 124275,
    SPELL_MONK_STAGGER_MODERATE                         = 124274,
    SPELL_MONK_SURGING_MIST_HEAL                        = 116995,
    SPELL_MONK_ZEN_PILGRIMAGE                           = 126892,
    SPELL_MONK_ZEN_PILGRIMAGE_RETURN                    = 126895,
    SPELL_MONK_ZEN_PILGRIMAGE_RETURN_AURA               = 126896,
    SPELL_MONK_POWER_STRIKES_AURA                       = 129914,
    SPELL_MONK_TOUCH_OF_DEATH                           = 322109,
    SPELL_MONK_TOUCH_OF_DEATH_DAMAGE                    = 322111,
    SPELL_MONK_TOUCH_OF_DEATH_AMPLIFIER                 = 271232,
    SPELL_MONK_DISABLE                                  = 116095,
    SPELL_MONK_DISABLE_ROOT                             = 116706,
    SPELL_MONK_RING_OF_PEACE_KNOCKBACK                  = 237371,
    SPELL_MONK_SPEAR_HAND_STRIKE_SILENCE                = 173320,
    SPELL_MONK_TRANSCENDENCE_CLONE_TARGET               = 119051,
    SPELL_MONK_TRANSCENDENCE_VISUAL                     = 119053,
    SPELL_MONK_TRANSCENDENCE_TRANSFER                   = 119996,
    SPELL_MONK_TRANSCENDENCE_RANGE_CHECK                = 62388,
    SPELL_RISING_MIST_HEAL                              = 274912,
    SPELL_RISING_MIST                                   = 274909,
    SPELL_MONK_THUNDER_FOCUS_TEA                        = 116680,
    SPELL_MONK_RISING_THUNDER                           = 210804,
    SPELL_MONK_RENEWING_MIST_HOT                        = 119611,
    SPELL_MONK_MORTAL_WOUNDS                            = 115804,
    SPELL_MONK_ESSENCE_FONT_PERIODIC_HEAL               = 191840,
    SPELL_MONK_ENVELOPING_MIST                          = 124682,
    SPELL_MONK_FORTIFYING_BREW                          = 120954,
    SPELL_MONK_MODERATE_STAGGER                         = 124274,
    SPELL_MONK_LIGHT_STAGGER                            = 124275,
    SPELL_MONK_HEAVY_STAGGER                            = 124273,
    SPELL_MONK_DIZZYING_HAZE                            = 116330,
    SPELL_MONK_BREATH_OF_FIRE_DOT                       = 123725,
    SPELL_MONK_KEG_SMASH_AURA                           = 121253,
    SPELL_MONK_CLASH_RUSH                               = 324383,
    SPELL_MONK_CLASH_STUN                               = 324382,
    SPELL_MONK_HEALING_ELIXIRS_RESTORE_HEALTH           = 122281,
    SPELL_MONK_SONG_OF_CHI_JI_STUN                      = 198909,
    SPELL_MONK_CHI_WAVE_HEAL                            = 132463,
    SPELL_MONK_CHI_BURST_HEAL                           = 130654,
    SPELL_MONK_CHI_BURST_DAMAGE                         = 148135,
    SPELL_MONK_PURIFYING_BREW                           = 119582,
    MONK_NPC_JADE_SERPENT_STATUE                        = 60849,
    SPELL_MONK_SPIRIT_OF_THE_CRANE_MANA                 = 210803,
    SPELL_MONK_BLACKOUT_KICK_TRIGGERED                  = 228649,
    SPELL_LIFECYCLES                                    = 197915,
    SPELL_MONK_LIFECYCLES_ENVELOPING_MIST               = 197919,
    SPELL_MONK_SOOTHING_MIST_VISUAL                     = 125955,
    SPELL_MONK_SOOTHING_MIST_ENERGIZE                   = 116335,
    SPELL_SERPENT_STATUE_SOOTHING_MIST                  = 248887,
    SPELL_FOCUSED_THUNDER_TALENT                        = 197895,
    SPELL_MONK_ZEN_PULSE_DAMAGE                         = 124081,
    SPELL_MONK_ZEN_PULSE_HEAL                           = 198487,
    SPELL_MONK_COUNTERACT_MAGIC                         = 202428,
    SPELL_MONK_RENEWING_MIST                            = 115151,
    SPELL_MONK_RENEWING_MIST_JUMP                       = 119607,
    SPELL_MONK_VISUAL_RENEWING_MIST                     = 24599,
    SPELL_MONK_ESSENCE_FONT_HEAL                        = 191840,
    SPELL_MONK_PLUS_ONE_MANA_TEA                        = 123760,
    SPELL_MONK_MANA_TEA_REGEN                           = 115294,
    SPELL_MONK_MANA_TEA_STACKS                          = 115867,
    SPELL_MONK_LIFECYCLES_VIVIFY                        = 197916,
    SPELL_MONK_TOUCH_OF_KARMA                           = 122470,
    SPELL_MONK_TOUCH_OF_KARMA_BUFF                      = 125174,
    SPELL_MONK_TOUCH_OF_KARMA_DAMAGE                    = 124280,
    SPELL_GOOD_KARMA_TALENT                             = 195295,
    SPELL_GOOD_KARMA_TALENT_HEAL                        = 285594,
    SPELL_MONK_FISTS_OF_FURY_DAMAGE                     = 117418,
    SPELL_MONK_XUEN_AURA                                = 123999,
    SPELL_MONK_FISTS_OF_FURY                            = 113656,
    SPELL_MONK_RISING_SUN_KICK                          = 107428,
    SPELL_MONK_WHIRLING_DRAGON_PUNCH_CASTER_AURA        = 196742,
    SPELL_MONK_WHIRLING_DRAGON_PUNCH_DAMAGE             = 158221,
    SPELL_MONK_FLYING_SERPENT_KICK                      = 101545,
    SPELL_MONK_FLYING_SERPENT_KICK_AOE                  = 123586,
    SPELL_MONK_FLYING_SERPENT_KICK_NEW                  = 115057,
    SPELL_MONK_ITEM_PVP_GLOVES_BONUS                    = 124489,
};

enum StormEarthAndFireSpells
{
    SPELL_MONK_SEF                                      = 137639,
    SPELL_MONK_SEF_STORM_VISUAL                         = 138080,
    SPELL_MONK_SEF_FIRE_VISUAL                          = 138081,
    SPELL_MONK_SEF_EARTH_VISUAL                         = 138083,
    SPELL_MONK_SEF_CHARGE                               = 138104,
    SPELL_MONK_SEF_SUMMON_EARTH                         = 138121,
    SPELL_MONK_SEF_SUMMON_FIRE                          = 138123,
    SPELL_MONK_SEF_SUMMONS_STATS                        = 138130,
    SPELL_MONK_SEF_CHARGE_TARGET                        = 196860,
    SPELL_MONK_SEF_FIXATE                               = 221771,

    NPC_FIRE_SPIRIT                                     = 69791,
    NPC_EARTH_SPIRIT                                    = 69792,
};

#define MONK_TRANSCENDENCE_GUID "MONK_TRANSCENDENCE_GUID"

// 117952 - Crackling Jade Lightning
class spell_monk_crackling_jade_lightning : public AuraScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo(
        {
            SPELL_MONK_STANCE_OF_THE_SPIRITED_CRANE,
            SPELL_MONK_CRACKLING_JADE_LIGHTNING_CHI_PROC
        });
    }

    void OnTick(AuraEffect const* /*aurEff*/)
    {
        if (Unit* caster = GetCaster())
            if (caster->HasAura(SPELL_MONK_STANCE_OF_THE_SPIRITED_CRANE))
                caster->CastSpell(caster, SPELL_MONK_CRACKLING_JADE_LIGHTNING_CHI_PROC, TRIGGERED_FULL_MASK);
    }

    void Register() override
    {
        OnEffectPeriodic += AuraEffectPeriodicFn(spell_monk_crackling_jade_lightning::OnTick, EFFECT_0, SPELL_AURA_PERIODIC_DAMAGE);
    }
};

// 117959 - Crackling Jade Lightning
class spell_monk_crackling_jade_lightning_knockback_proc_aura : public AuraScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo(
        {
            SPELL_MONK_CRACKLING_JADE_LIGHTNING_KNOCKBACK,
            SPELL_MONK_CRACKLING_JADE_LIGHTNING_KNOCKBACK_CD
        });
    }

    bool CheckProc(ProcEventInfo& eventInfo)
    {
        if (GetTarget()->HasAura(SPELL_MONK_CRACKLING_JADE_LIGHTNING_KNOCKBACK_CD))
            return false;

        if (eventInfo.GetActor()->HasAura(SPELL_MONK_CRACKLING_JADE_LIGHTNING_CHANNEL, GetTarget()->GetGUID()))
            return false;

        Spell* currentChanneledSpell = GetTarget()->GetCurrentSpell(CURRENT_CHANNELED_SPELL);
        if (!currentChanneledSpell || currentChanneledSpell->GetSpellInfo()->Id != SPELL_MONK_CRACKLING_JADE_LIGHTNING_CHANNEL)
            return false;

        return true;
    }

    void HandleProc(AuraEffect* /*aurEff*/, ProcEventInfo& eventInfo)
    {
        GetTarget()->CastSpell(eventInfo.GetActor(), SPELL_MONK_CRACKLING_JADE_LIGHTNING_KNOCKBACK, TRIGGERED_FULL_MASK);
        GetTarget()->CastSpell(GetTarget(), SPELL_MONK_CRACKLING_JADE_LIGHTNING_KNOCKBACK_CD, TRIGGERED_FULL_MASK);
    }

    void Register() override
    {
        DoCheckProc += AuraCheckProcFn(spell_monk_crackling_jade_lightning_knockback_proc_aura::CheckProc);
        OnEffectProc += AuraEffectProcFn(spell_monk_crackling_jade_lightning_knockback_proc_aura::HandleProc, EFFECT_0, SPELL_AURA_DUMMY);
    }
};

// 116849 - Life Cocoon
class spell_monk_life_cocoon : public SpellScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_MONK_CALMING_COALESCENCE });
    }

    void CalculateAbsorb(SpellEffIndex /*effIndex*/)
    {
        int32 absorb = GetCaster()->CountPctFromMaxHealth(GetEffectValue());
        if (Player* player = GetCaster()->ToPlayer())
            AddPct(absorb, player->GetRatingBonusValue(CR_VERSATILITY_HEALING_DONE));

        if (AuraEffect* calmingCoalescence = GetCaster()->GetAuraEffect(SPELL_MONK_CALMING_COALESCENCE, EFFECT_0, GetCaster()->GetGUID()))
        {
            AddPct(absorb, calmingCoalescence->GetAmount());
            calmingCoalescence->GetBase()->Remove();
        }

        GetSpell()->SetSpellValue(SPELLVALUE_BASE_POINT0, absorb);
    }

    void Register() override
    {
        OnEffectLaunch += SpellEffectFn(spell_monk_life_cocoon::CalculateAbsorb, EFFECT_2, SPELL_EFFECT_DUMMY);
    }
};

// 392972 - Open Palm Strikes
class spell_monk_open_palm_strikes : public AuraScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellEffect({ { SPELL_MONK_OPEN_PALM_STRIKES_TALENT, EFFECT_1} });
    }

    bool CheckProc(AuraEffect const* /*aurEff*/, ProcEventInfo& /*procInfo*/)
    {
        AuraEffect const* talent = GetTarget()->GetAuraEffect(SPELL_MONK_OPEN_PALM_STRIKES_TALENT, EFFECT_1);
        return talent && roll_chance_i(talent->GetAmount());
    }

    void Register() override
    {
        DoCheckEffectProc += AuraCheckEffectProcFn(spell_monk_open_palm_strikes::CheckProc, EFFECT_0, SPELL_AURA_PROC_TRIGGER_SPELL);
    }
};

// 121817 - Power Strike
class spell_monk_power_strike_periodic : public AuraScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_MONK_POWER_STRIKE_PROC });
    }

    void HandlePeriodic(AuraEffect const* /*aurEff*/)
    {
        GetTarget()->CastSpell(GetTarget(), SPELL_MONK_POWER_STRIKE_PROC, true);
    }

    void Register() override
    {
        OnEffectPeriodic += AuraEffectPeriodicFn(spell_monk_power_strike_periodic::HandlePeriodic, EFFECT_0, SPELL_AURA_PERIODIC_DUMMY);
    }
};

// 129914 - Power Strike Proc
class spell_monk_power_strike_proc : public AuraScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_MONK_POWER_STRIKE_ENERGIZE });
    }

    void HandleProc(AuraEffect* /*aurEff*/, ProcEventInfo& /*eventInfo*/)
    {
        GetTarget()->CastSpell(GetTarget(), SPELL_MONK_POWER_STRIKE_ENERGIZE, true);
    }

    void Register() override
    {
        OnEffectProc += AuraEffectProcFn(spell_monk_power_strike_proc::HandleProc, EFFECT_0, SPELL_AURA_DUMMY);
    }
};

// 115546 - Provoke
class spell_monk_provoke : public SpellScript
{
    static uint32 const BlackOxStatusEntry = 61146;

    bool Validate(SpellInfo const* spellInfo) override
    {
        if (!(spellInfo->GetExplicitTargetMask() & TARGET_FLAG_UNIT_MASK)) // ensure GetExplTargetUnit() will return something meaningful during CheckCast
            return false;
        return ValidateSpellInfo(
        {
            SPELL_MONK_PROVOKE_SINGLE_TARGET,
            SPELL_MONK_PROVOKE_AOE
        });
    }

    SpellCastResult CheckExplicitTarget()
    {
        if (GetExplTargetUnit()->GetEntry() != BlackOxStatusEntry)
        {
            SpellInfo const* singleTarget = sSpellMgr->AssertSpellInfo(SPELL_MONK_PROVOKE_SINGLE_TARGET, GetCastDifficulty());
            SpellCastResult singleTargetExplicitResult = singleTarget->CheckExplicitTarget(GetCaster(), GetExplTargetUnit());
            if (singleTargetExplicitResult != SPELL_CAST_OK)
                return singleTargetExplicitResult;
        }
        else if (GetExplTargetUnit()->GetOwnerGUID() != GetCaster()->GetGUID())
            return SPELL_FAILED_BAD_TARGETS;

        return SPELL_CAST_OK;
    }

    void HandleDummy(SpellEffIndex effIndex)
    {
        PreventHitDefaultEffect(effIndex);
        if (GetHitUnit()->GetEntry() != BlackOxStatusEntry)
            GetCaster()->CastSpell(GetHitUnit(), SPELL_MONK_PROVOKE_SINGLE_TARGET, true);
        else
            GetCaster()->CastSpell(GetHitUnit(), SPELL_MONK_PROVOKE_AOE, true);
    }

    void Register() override
    {
        OnCheckCast += SpellCheckCastFn(spell_monk_provoke::CheckExplicitTarget);
        OnEffectHitTarget += SpellEffectFn(spell_monk_provoke::HandleDummy, EFFECT_0, SPELL_EFFECT_DUMMY);
    }
};

// 109132 - Roll
class spell_monk_roll : public SpellScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_MONK_ROLL_BACKWARD, SPELL_MONK_ROLL_FORWARD, SPELL_MONK_NO_FEATHER_FALL });
    }

    SpellCastResult CheckCast()
    {
        if (GetCaster()->HasUnitState(UNIT_STATE_ROOT))
            return SPELL_FAILED_ROOTED;
        return SPELL_CAST_OK;
    }

    void HandleDummy(SpellEffIndex /*effIndex*/)
    {
        GetCaster()->CastSpell(GetCaster(), GetCaster()->HasUnitMovementFlag(MOVEMENTFLAG_BACKWARD) ? SPELL_MONK_ROLL_BACKWARD : SPELL_MONK_ROLL_FORWARD,
            TRIGGERED_IGNORE_CAST_IN_PROGRESS);
        GetCaster()->CastSpell(GetCaster(), SPELL_MONK_NO_FEATHER_FALL, true);
    }

    void Register() override
    {
        OnCheckCast += SpellCheckCastFn(spell_monk_roll::CheckCast);
        OnEffectHitTarget += SpellEffectFn(spell_monk_roll::HandleDummy, EFFECT_0, SPELL_EFFECT_DUMMY);
    }
};

// 107427 - Roll
// 109131 - Roll (backward)
class spell_monk_roll_aura : public AuraScript
{
    void CalcMovementAmount(AuraEffect const* /*aurEff*/, int32& amount, bool& /*canBeRecalculated*/)
    {
        amount += 100;
    }

    void CalcImmunityAmount(AuraEffect const* /*aurEff*/, int32& amount, bool& /*canBeRecalculated*/)
    {
        amount -= 100;
    }

    void ChangeRunBackSpeed(AuraEffect const* /*aurEff*/, AuraEffectHandleModes /*mode*/)
    {
        GetTarget()->SetSpeed(MOVE_RUN_BACK, GetTarget()->GetSpeed(MOVE_RUN));
    }

    void RestoreRunBackSpeed(AuraEffect const* /*aurEff*/, AuraEffectHandleModes /*mode*/)
    {
        GetTarget()->UpdateSpeed(MOVE_RUN_BACK);
    }

    void Register() override
    {
        // Values need manual correction
        DoEffectCalcAmount += AuraEffectCalcAmountFn(spell_monk_roll_aura::CalcMovementAmount, EFFECT_0, SPELL_AURA_MOD_SPEED_NO_CONTROL);
        DoEffectCalcAmount += AuraEffectCalcAmountFn(spell_monk_roll_aura::CalcMovementAmount, EFFECT_2, SPELL_AURA_MOD_MINIMUM_SPEED);
        DoEffectCalcAmount += AuraEffectCalcAmountFn(spell_monk_roll_aura::CalcImmunityAmount, EFFECT_5, SPELL_AURA_MECHANIC_IMMUNITY);
        DoEffectCalcAmount += AuraEffectCalcAmountFn(spell_monk_roll_aura::CalcImmunityAmount, EFFECT_6, SPELL_AURA_MECHANIC_IMMUNITY);

        // This is a special aura that sets backward run speed equal to forward speed
        AfterEffectApply += AuraEffectApplyFn(spell_monk_roll_aura::ChangeRunBackSpeed, EFFECT_4, SPELL_AURA_USE_NORMAL_MOVEMENT_SPEED, AURA_EFFECT_HANDLE_REAL);
        AfterEffectRemove += AuraEffectApplyFn(spell_monk_roll_aura::RestoreRunBackSpeed, EFFECT_4, SPELL_AURA_USE_NORMAL_MOVEMENT_SPEED, AURA_EFFECT_HANDLE_REAL);
    }
};

// Utility for stagger scripts
Aura* FindExistingStaggerEffect(Unit* unit)
{
    if (Aura* auraLight = unit->GetAura(SPELL_MONK_STAGGER_LIGHT))
        return auraLight;

    if (Aura* auraModerate = unit->GetAura(SPELL_MONK_STAGGER_MODERATE))
        return auraModerate;

    if (Aura* auraHeavy = unit->GetAura(SPELL_MONK_STAGGER_HEAVY))
        return auraHeavy;

    return nullptr;
}

static constexpr SpellEffIndex AuraStaggerEffectTick = EFFECT_0;
static constexpr SpellEffIndex AuraStaggerEffectTotal = EFFECT_1;

// 115069 - Stagger
class spell_monk_stagger : public AuraScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_MONK_STAGGER_LIGHT, SPELL_MONK_STAGGER_MODERATE, SPELL_MONK_STAGGER_HEAVY });
    }

    void AbsorbNormal(AuraEffect* /*aurEff*/, DamageInfo& dmgInfo, uint32& /*absorbAmount*/)
    {
        Absorb(dmgInfo, 1.0f);
    }

    void AbsorbMagic(AuraEffect* /*aurEff*/, DamageInfo& dmgInfo, uint32& /*absorbAmount*/)
    {
        AuraEffect const* effect = GetEffect(EFFECT_4);
        if (!effect)
            return;

        Absorb(dmgInfo, float(effect->GetAmount()) / 100.0f);
    }

    void Absorb(DamageInfo& dmgInfo, float multiplier)
    {
        // Prevent default action (which would remove the aura)
        PreventDefaultAction();

        // make sure damage doesn't come from stagger damage spell SPELL_MONK_STAGGER_DAMAGE_AURA
        if (SpellInfo const* dmgSpellInfo = dmgInfo.GetSpellInfo())
            if (dmgSpellInfo->Id == SPELL_MONK_STAGGER_DAMAGE_AURA)
                return;

        AuraEffect const* effect = GetEffect(AuraStaggerEffectTick);
        if (!effect)
            return;

        Unit* target = GetTarget();
        float agility = target->GetStat(STAT_AGILITY);
        float base = CalculatePct(agility, float(effect->GetAmount()));
        float K = sDB2Manager.EvaluateExpectedStat(ExpectedStatType::ArmorConstant, target->GetLevel(), -2, 0, Classes(target->GetClass()), 0);

        float newAmount = (base / (base + K));
        newAmount *= multiplier;

        // Absorb X percentage of the damage
        float absorbAmount = float(dmgInfo.GetDamage()) * newAmount;
        if (absorbAmount > 0)
        {
            dmgInfo.AbsorbDamage(absorbAmount);

            // Cast stagger and make it tick on each tick
            AddAndRefreshStagger(absorbAmount);
        }
    }

    void Register() override
    {
        OnEffectAbsorb += AuraEffectAbsorbFn(spell_monk_stagger::AbsorbNormal, EFFECT_1);
        OnEffectAbsorb += AuraEffectAbsorbFn(spell_monk_stagger::AbsorbMagic, EFFECT_2);
    }

private:
    void AddAndRefreshStagger(float amount)
    {
        Unit* target = GetTarget();
        if (Aura* auraStagger = FindExistingStaggerEffect(target))
        {
            AuraEffect* effStaggerRemaining = auraStagger->GetEffect(AuraStaggerEffectTotal);
            if (!effStaggerRemaining)
                return;

            float newAmount = effStaggerRemaining->GetAmount() + amount;
            uint32 spellId = GetStaggerSpellId(target, newAmount);
            if (spellId == effStaggerRemaining->GetSpellInfo()->Id)
            {
                auraStagger->RefreshDuration();
                effStaggerRemaining->ChangeAmount(newAmount, false, true /* reapply */);
            }
            else
            {
                // amount changed the stagger type so we need to change the stagger amount (e.g. from medium to light)
                GetTarget()->RemoveAura(auraStagger);
                AddNewStagger(target, spellId, newAmount);
            }
        }
        else
            AddNewStagger(target, GetStaggerSpellId(target, amount), amount);
    }

    uint32 GetStaggerSpellId(Unit* unit, float amount)
    {
        const float StaggerHeavy = 0.6f;
        const float StaggerModerate = 0.3f;

        float staggerPct = amount / float(unit->GetMaxHealth());
        return (staggerPct >= StaggerHeavy) ? SPELL_MONK_STAGGER_HEAVY :
            (staggerPct >= StaggerModerate) ? SPELL_MONK_STAGGER_MODERATE :
            SPELL_MONK_STAGGER_LIGHT;
    }

    void AddNewStagger(Unit* unit, uint32 staggerSpellId, float staggerAmount)
    {
        // We only set the total stagger amount. The amount per tick will be set by the stagger spell script
        unit->CastSpell(unit, staggerSpellId, CastSpellExtraArgs(SPELLVALUE_BASE_POINT1, staggerAmount).SetTriggerFlags(TRIGGERED_FULL_MASK));
    }
};

// 124255 - Stagger - SPELL_MONK_STAGGER_DAMAGE_AURA
class spell_monk_stagger_damage_aura : public AuraScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_MONK_STAGGER_LIGHT, SPELL_MONK_STAGGER_MODERATE, SPELL_MONK_STAGGER_HEAVY });
    }

    void OnPeriodicDamage(AuraEffect const* aurEff)
    {
        // Update our light/medium/heavy stagger with the correct stagger amount left
        if (Aura* auraStagger = FindExistingStaggerEffect(GetTarget()))
        {
            if (AuraEffect* auraEff = auraStagger->GetEffect(AuraStaggerEffectTotal))
            {
                float total = float(auraEff->GetAmount());
                float tickDamage = float(aurEff->GetAmount());
                auraEff->ChangeAmount(total - tickDamage);
            }
        }
    }

    void Register() override
    {
        OnEffectPeriodic += AuraEffectPeriodicFn(spell_monk_stagger_damage_aura::OnPeriodicDamage, EFFECT_0, SPELL_AURA_PERIODIC_DAMAGE);
    }
};

// 124273, 124274, 124275 - Light/Moderate/Heavy Stagger - SPELL_MONK_STAGGER_LIGHT / SPELL_MONK_STAGGER_MODERATE / SPELL_MONK_STAGGER_HEAVY
class spell_monk_stagger_debuff_aura : public AuraScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_MONK_STAGGER_DAMAGE_AURA })
            && ValidateSpellEffect({ { SPELL_MONK_STAGGER_DAMAGE_AURA, EFFECT_0 } });
    }

    bool Load() override
    {
        _period = float(sSpellMgr->AssertSpellInfo(SPELL_MONK_STAGGER_DAMAGE_AURA, GetCastDifficulty())->GetEffect(EFFECT_0).ApplyAuraPeriod);
        return true;
    }

    void OnReapply(AuraEffect const* aurEff, AuraEffectHandleModes /*mode*/)
    {
        // Calculate damage per tick
        float total = float(aurEff->GetAmount());
        float perTick = total * _period / float(GetDuration()); // should be same as GetMaxDuration() TODO: verify

        // Set amount on effect for tooltip
        AuraEffect* effInfo = GetAura()->GetEffect(AuraStaggerEffectTick);
        if (effInfo)
            effInfo->ChangeAmount(perTick);

        // Set amount on damage aura (or cast it if needed)
        CastOrChangeTickDamage(perTick);
    }

    void OnRemove(AuraEffect const* /*aurEff*/, AuraEffectHandleModes mode)
    {
        if (mode != AURA_EFFECT_HANDLE_REAL)
            return;

        // Remove damage aura
        GetTarget()->RemoveAura(SPELL_MONK_STAGGER_DAMAGE_AURA);
    }

    void Register() override
    {
        AfterEffectApply += AuraEffectRemoveFn(spell_monk_stagger_debuff_aura::OnReapply, EFFECT_1, SPELL_AURA_DUMMY, AURA_EFFECT_HANDLE_REAL_OR_REAPPLY_MASK);
        AfterEffectRemove += AuraEffectRemoveFn(spell_monk_stagger_debuff_aura::OnRemove, EFFECT_1, SPELL_AURA_DUMMY, AURA_EFFECT_HANDLE_REAL);
    }

private:
    float _period = 0.0f;

    void CastOrChangeTickDamage(float tickDamage)
    {
        Unit* unit = GetTarget();
        Aura* auraDamage = unit->GetAura(SPELL_MONK_STAGGER_DAMAGE_AURA);
        if (!auraDamage)
        {
            unit->CastSpell(unit, SPELL_MONK_STAGGER_DAMAGE_AURA, true);
            auraDamage = unit->GetAura(SPELL_MONK_STAGGER_DAMAGE_AURA);
        }

        if (auraDamage)
            if (AuraEffect* eff = auraDamage->GetEffect(AuraStaggerEffectTick))
                eff->ChangeAmount(tickDamage);
    }
};

// Zen Pilgrimage - 126892, Zen Pilgrimage - 194011
class spell_monk_zen_pilgrimage : public SpellScriptLoader
{
public:
    spell_monk_zen_pilgrimage() : SpellScriptLoader("spell_monk_zen_pilgrimage") { }

    class spell_monk_zen_pilgrimage_SpellScript : public SpellScript
    {
        SpellCastResult CheckDist()
        {
            if (GetSpellInfo()->Id == 194011)
                return SPELL_CAST_OK;

            if (Unit* caster = GetCaster())
                if (Player* _player = caster->ToPlayer())
                    if (_player->IsQuestRewarded(40236)) // Check quest for port to oplot
                    {
                        caster->CastSpell(caster, 194011, false);
                        return SPELL_FAILED_DONT_REPORT;
                    }

            return SPELL_CAST_OK;
        }

        void HandleOnCast()
        {
            if (Unit* caster = GetCaster())
                if (Player* _player = caster->ToPlayer())
                {
                    if (GetSpellInfo()->Id == SPELL_MONK_ZEN_PILGRIMAGE)
                    {
                        _player->SaveManualRecallPosition();
                        _player->CastSpell(_player, SPELL_MONK_ZEN_PILGRIMAGE_RETURN_AURA, true);
                    }

                }
        }

        void Register() override
        {
            OnCast += SpellCastFn(spell_monk_zen_pilgrimage_SpellScript::HandleOnCast);
            OnCheckCast += SpellCheckCastFn(spell_monk_zen_pilgrimage_SpellScript::CheckDist);
        }
    };

    SpellScript* GetSpellScript() const override
    {
        return new spell_monk_zen_pilgrimage_SpellScript();
    }
};

// Zen Pilgrimage : Return - 126895
class spell_monk_zen_pilgrimage_return : public SpellScriptLoader
{
public:
    spell_monk_zen_pilgrimage_return() : SpellScriptLoader("spell_monk_zen_pilgrimage_return") { }

    class spell_monk_zen_pilgrimage_return_SpellScript : public SpellScript
    {
        void HandleDummy(SpellEffIndex /*effIndex*/)
        {
            if (Unit* caster = GetCaster())
            {
                if (Player* _player = caster->ToPlayer())
                {
                    if (GetSpellInfo()->Id == SPELL_MONK_ZEN_PILGRIMAGE_RETURN)
                    {
                        _player->ManualRecall();
                        _player->RemoveAura(SPELL_MONK_ZEN_PILGRIMAGE_RETURN_AURA);
                    }
                }
            }
        }

        void Register() override
        {
            OnEffectHitTarget += SpellEffectFn(spell_monk_zen_pilgrimage_return_SpellScript::HandleDummy, EFFECT_0, SPELL_EFFECT_SCRIPT_EFFECT);
        }
    };

    SpellScript* GetSpellScript() const override
    {
        return new spell_monk_zen_pilgrimage_return_SpellScript();
    }
};

// 100780 - Tiger Palm
class spell_monk_tiger_palm : public SpellScript
{
    void HandleHit(SpellEffIndex /*effIndex*/)
    {
        if (Aura* powerStrikes = GetCaster()->GetAura(SPELL_MONK_POWER_STRIKES_AURA))
        {
            SetEffectValue(GetEffectValue() + powerStrikes->GetEffect(EFFECT_0)->GetBaseAmount());
            powerStrikes->Remove();
        }
    }

    void Register() override
    {
        OnEffectHitTarget += SpellEffectFn(spell_monk_tiger_palm::HandleHit, EFFECT_1, SPELL_EFFECT_ENERGIZE);
    }
};

// Zen Flight - 125883
class spell_monk_zen_flight_check : public SpellScriptLoader
{
public:
    spell_monk_zen_flight_check() : SpellScriptLoader("spell_monk_zen_flight_check") { }

    class spell_monk_zen_flight_check_SpellScript : public SpellScript
    {
        PrepareSpellScript(spell_monk_zen_flight_check_SpellScript);

        SpellCastResult CheckTarget()
        {
            if (Player* _player = GetCaster()->ToPlayer())
            {
                if (_player->GetMap()->IsBattlegroundOrArena())
                    return SPELL_FAILED_NOT_IN_BATTLEGROUND;

                // In Kalimdor or Eastern Kingdom with Flight Master's License
                if (!_player->HasSpell(90267) && (_player->GetMapId() == 1 || _player->GetMapId() == 0))
                    return SPELL_FAILED_NOT_HERE;

                // In Pandaria with Wisdom of the Four Winds
                if (!_player->HasSpell(115913) && (_player->GetMapId() == 870))
                    return SPELL_FAILED_NOT_HERE;

                // Legion, Broken Isles
                if (_player->GetMapId() == 1220)
                    return SPELL_FAILED_NOT_HERE;

                // In BfA Content not yet
                if (_player->GetMapId() == 1642 || _player->GetMapId() == 1643)
                    return SPELL_FAILED_NOT_HERE;
            }

            return SPELL_CAST_OK;
        }

        void Register() override
        {
            OnCheckCast += SpellCheckCastFn(spell_monk_zen_flight_check_SpellScript::CheckTarget);
        }
    };

    SpellScript* GetSpellScript() const override
    {
        return new spell_monk_zen_flight_check_SpellScript();
    }

    class spell_monk_zen_flight_check_AuraScript : public AuraScript
    {
        PrepareAuraScript(spell_monk_zen_flight_check_AuraScript);

        bool Load() override
        {
            return GetCaster() && GetCaster()->GetTypeId() == TYPEID_PLAYER;
        }

        void CalculateAmount(AuraEffect const* /*aurEff*/, int32& amount, bool& /*canBeRecalculated*/)
        {
            if (!GetCaster())
                return;

            if (Player* caster = GetCaster()->ToPlayer())
                if (caster->GetSkillValue(SKILL_RIDING) >= 375)
                    amount = 310;
        }

        void Register() override
        {
            DoEffectCalcAmount += AuraEffectCalcAmountFn(spell_monk_zen_flight_check_AuraScript::CalculateAmount, EFFECT_1, SPELL_AURA_MOD_INCREASE_VEHICLE_FLIGHT_SPEED);
        }
    };

    AuraScript* GetAuraScript() const override
    {
        return new spell_monk_zen_flight_check_AuraScript();
    }
};

// Disable - 116095
class spell_monk_disable : public SpellScript
{
    PrepareSpellScript(spell_monk_disable);

    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_MONK_DISABLE, SPELL_MONK_DISABLE_ROOT });
    }

    void HandleOnEffectHitTarget(SpellEffIndex /*effectIndex*/)
    {
        if (Unit* target = GetExplTargetUnit())
            if (target->HasAuraType(SPELL_AURA_MOD_DECREASE_SPEED))
                GetCaster()->CastSpell(target, SPELL_MONK_DISABLE_ROOT, true);
    }

    void Register() override
    {
        OnEffectHitTarget += SpellEffectFn(spell_monk_disable::HandleOnEffectHitTarget, EFFECT_0, SPELL_EFFECT_APPLY_AURA);
    }
};

// Disable - 116095
class aura_monk_disable : public AuraScript
{
    PrepareAuraScript(aura_monk_disable);

    bool CheckProc(ProcEventInfo& eventInfo)
    {
        if (DamageInfo* damageInfo = eventInfo.GetDamageInfo())
        {
            if ((damageInfo->GetAttackType() == BASE_ATTACK ||
                damageInfo->GetAttackType() == OFF_ATTACK) &&
                damageInfo->GetAttacker() == GetCaster())
            {
                GetAura()->RefreshDuration();
                return true;
            }
        }
        return false;
    }

    void Register() override
    {
        DoCheckProc += AuraCheckProcFn(aura_monk_disable::CheckProc);
    }
};

// Spear Hand Strike - 116705
class spell_monk_spear_hand_strike : public SpellScriptLoader
{
public:
    spell_monk_spear_hand_strike() : SpellScriptLoader("spell_monk_spear_hand_strike") { }

    class spell_monk_spear_hand_strike_SpellScript : public SpellScript
    {
        PrepareSpellScript(spell_monk_spear_hand_strike_SpellScript);

        void HandleOnHit()
        {
            if (Player* _player = GetCaster()->ToPlayer())
            {
                if (Unit* target = GetHitUnit())
                {
                    if (target->isInFront(_player))
                    {
                        _player->CastSpell(target, SPELL_MONK_SPEAR_HAND_STRIKE_SILENCE, true);
                        _player->GetSpellHistory()->AddCooldown(116705, 0, std::chrono::seconds(15));
                    }
                }
            }
        }

        void Register() override
        {
            OnHit += SpellHitFn(spell_monk_spear_hand_strike_SpellScript::HandleOnHit);
        }
    };

    SpellScript* GetSpellScript() const override
    {
        return new spell_monk_spear_hand_strike_SpellScript();
    }
};

// Rising Sun Kick - 107428
class spell_monk_rising_sun_kick : public SpellScript
{
    PrepareSpellScript(spell_monk_rising_sun_kick);

    void HandleOnHit(SpellEffIndex /*effIndex*/)
    {
        Player* caster = GetCaster()->ToPlayer();
        Unit* target = GetHitUnit();
        if (!target || !caster)
            return;

        if (caster->HasAura(SPELL_MONK_RISING_THUNDER))
            caster->ToPlayer()->GetSpellHistory()->ResetCooldown(SPELL_MONK_THUNDER_FOCUS_TEA, true);

        if (caster->GetPrimarySpecialization() == ChrSpecialization::MonkBrewmaster)
            caster->CastSpell(target, SPELL_MONK_MORTAL_WOUNDS, true);

        if (caster->GetPrimarySpecialization() == ChrSpecialization::MonkMistweaver && caster->HasAura(SPELL_RISING_MIST))
        {
            caster->CastSpell(nullptr, SPELL_RISING_MIST_HEAL, true);

            if (Aura* reneWingMist = caster->GetAura(SPELL_MONK_RENEWING_MIST_HOT))
                reneWingMist->RefreshDuration(true);

            if (Aura* envelopingMist = caster->GetAura(SPELL_MONK_ENVELOPING_MIST))
                envelopingMist->RefreshDuration(true);

            if (Aura* essenceFont = caster->GetAura(SPELL_MONK_ESSENCE_FONT_PERIODIC_HEAL))
                essenceFont->RefreshDuration(true);
        }
        std::list<Unit*> u_li;
        caster->GetFriendlyUnitListInRange(u_li, 100.0f);
        for (auto& targets : u_li)
        {
            if (Aura* relatedAuras = targets->GetAura(SPELL_MONK_RENEWING_MIST_HOT || targets->GetAura(SPELL_MONK_ENVELOPING_MIST || targets->GetAura(SPELL_MONK_ESSENCE_FONT_PERIODIC_HEAL))))
                relatedAuras->RefreshDuration(true);
        }
    }

    void Register() override
    {
        OnEffectHitTarget += SpellEffectFn(spell_monk_rising_sun_kick::HandleOnHit, EFFECT_0, SPELL_EFFECT_TRIGGER_SPELL);
    }
};

// Fortifying brew - 115203
class spell_monk_fortifying_brew : public SpellScriptLoader
{
public:
    spell_monk_fortifying_brew() : SpellScriptLoader("spell_monk_fortifying_brew") { }

    class spell_monk_fortifying_brew_SpellScript : public SpellScript
    {
        PrepareSpellScript(spell_monk_fortifying_brew_SpellScript);

        void HandleDummy(SpellEffIndex /*effIndex*/)
        {
            Unit* caster = GetCaster();
            if (caster && caster->GetTypeId() == TYPEID_PLAYER)
                caster->CastSpell(caster, SPELL_MONK_FORTIFYING_BREW, true);
        }

        void Register() override
        {
            OnEffectHitTarget += SpellEffectFn(spell_monk_fortifying_brew_SpellScript::HandleDummy, EFFECT_0, SPELL_EFFECT_DUMMY);
        }
    };

    SpellScript* GetSpellScript() const override
    {
        return new spell_monk_fortifying_brew_SpellScript();
    }
};

// Purifying Brew - 119582
class spell_monk_purifying_brew : public SpellScriptLoader
{
public:
    spell_monk_purifying_brew() : SpellScriptLoader("spell_monk_purifying_brew") { }

    class spell_monk_purifying_brew_SpellScript : public SpellScript
    {
        PrepareSpellScript(spell_monk_purifying_brew_SpellScript);

        void HandleOnHit()
        {
            if (Unit* caster = GetCaster())
            {
                if (Player* _player = caster->ToPlayer())
                {
                    Aura* staggerAmount = _player->GetAura(SPELL_MONK_LIGHT_STAGGER);

                    if (!staggerAmount)
                        staggerAmount = _player->GetAura(SPELL_MONK_MODERATE_STAGGER);
                    if (!staggerAmount)
                        staggerAmount = _player->GetAura(SPELL_MONK_HEAVY_STAGGER);

                    if (staggerAmount)
                    {
                        int32 newStagger = staggerAmount->GetEffect(EFFECT_1)->GetAmount();
                        newStagger = newStagger * 0.5;
                        staggerAmount->GetEffect(EFFECT_1)->ChangeAmount(newStagger);
                    }
                }
            }
        }

        void Register() override
        {
            OnHit += SpellHitFn(spell_monk_purifying_brew_SpellScript::HandleOnHit);
        }
    };

    SpellScript* GetSpellScript() const override
    {
        return new spell_monk_purifying_brew_SpellScript();
    }
};

// 115181 - Breath of Fire
class spell_monk_breath_of_fire : public SpellScriptLoader
{
public:
    spell_monk_breath_of_fire() : SpellScriptLoader("spell_monk_breath_of_fire") { }

    class spell_monk_breath_of_fire_SpellScript : public SpellScript
    {
        PrepareSpellScript(spell_monk_breath_of_fire_SpellScript);

        void HandleAfterHit()
        {
            if (Unit* caster = GetCaster())
            {
                if (Player* _player = caster->ToPlayer())
                {
                    if (Unit* target = GetHitUnit())
                    {
                        // if Dizzying Haze is on the target, they will burn for an additionnal damage over 8s
                        if (target->HasAura(SPELL_MONK_DIZZYING_HAZE))
                            _player->CastSpell(target, SPELL_MONK_BREATH_OF_FIRE_DOT, true);
                        if (target->HasAura(SPELL_MONK_KEG_SMASH_AURA))
                            _player->CastSpell(target, SPELL_MONK_BREATH_OF_FIRE_DOT, true);
                    }
                }
            }
        }

        void Register() override
        {
            AfterHit += SpellHitFn(spell_monk_breath_of_fire_SpellScript::HandleAfterHit);
        }
    };

    SpellScript* GetSpellScript() const override
    {
        return new spell_monk_breath_of_fire_SpellScript();
    }
};

// Dampen Harm - 122278
class spell_monk_dampen_harm : public SpellScriptLoader
{
public:
    spell_monk_dampen_harm() : SpellScriptLoader("spell_monk_dampen_harm") { }
    class spell_monk_dampen_harm_AuraScript : public AuraScript
    {
        PrepareAuraScript(spell_monk_dampen_harm_AuraScript);
        int32 healthPct;

        bool Load() override
        {
            healthPct = GetSpellInfo()->GetEffect(EFFECT_0).CalcValue(GetCaster());
            return GetUnitOwner()->ToPlayer();
        }

        void CalculateAmount(AuraEffect const* /*auraEffect*/, int32& amount, bool& /*canBeRecalculated*/)
        {
            amount = -1;
        }

        void Absorb(AuraEffect* auraEffect, DamageInfo& dmgInfo, uint32& absorbAmount)
        {
            Unit* target = GetTarget();
            uint32 health = target->CountPctFromMaxHealth(healthPct);
            if (dmgInfo.GetDamage() < health)
                return;

            absorbAmount = dmgInfo.GetDamage() * (GetSpellInfo()->GetEffect(EFFECT_0).CalcValue(GetCaster()) / 100);
            auraEffect->GetBase()->DropCharge();
        }
        void Register() override
        {
            DoEffectCalcAmount += AuraEffectCalcAmountFn(spell_monk_dampen_harm_AuraScript::CalculateAmount, EFFECT_0, SPELL_AURA_SCHOOL_ABSORB);
            OnEffectAbsorb += AuraEffectAbsorbFn(spell_monk_dampen_harm_AuraScript::Absorb, EFFECT_0);
        }
    };
    AuraScript* GetAuraScript() const override
    {
        return new spell_monk_dampen_harm_AuraScript();
    }
};

// 324312 - Clash
class spell_monk_clash : public SpellScript
{
    void HandleDummy(SpellEffIndex /*effIndex*/)
    {
        GetCaster()->CastSpell(GetHitUnit(), SPELL_MONK_CLASH_RUSH, true);
        GetCaster()->CastSpell(GetHitUnit(), SPELL_MONK_CLASH_STUN, true);
    }

    void Register() override
    {
        OnEffectHitTarget += SpellEffectFn(spell_monk_clash::HandleDummy, EFFECT_0, SPELL_EFFECT_DUMMY);
    }
};

//122280
class spell_monk_healing_elixirs_aura : public SpellScriptLoader
{
public:
    spell_monk_healing_elixirs_aura() : SpellScriptLoader("spell_monk_healing_elixirs_aura") { }
    class spell_monk_healing_elixirs_aura_AuraScript : public AuraScript
    {
        PrepareAuraScript(spell_monk_healing_elixirs_aura_AuraScript);
        void OnProc(AuraEffect* /*aurEff*/, ProcEventInfo& eventInfo)
        {
            PreventDefaultAction();
            if (!GetCaster())
                return;
            if (!eventInfo.GetDamageInfo())
                return;
            if (!eventInfo.GetDamageInfo()->GetDamage())
                return;
            if (Unit* caster = GetCaster())
            {
                if (caster->HealthBelowPctDamaged(35, eventInfo.GetDamageInfo()->GetDamage()))
                {
                    caster->CastSpell(caster, SPELL_MONK_HEALING_ELIXIRS_RESTORE_HEALTH, true);
                    caster->GetSpellHistory()->ConsumeCharge(SPELL_MONK_HEALING_ELIXIRS_RESTORE_HEALTH);
                }
            }
        }
        void Register() override
        {
            OnEffectProc += AuraEffectProcFn(spell_monk_healing_elixirs_aura_AuraScript::OnProc, EFFECT_0, SPELL_AURA_DUMMY);
        }
    };
    AuraScript* GetAuraScript() const override
    {
        return new spell_monk_healing_elixirs_aura_AuraScript();
    }
};

//  Ring of Peace - 116844
// AreaTriggerID - 718
struct at_monk_ring_of_peace : AreaTriggerAI
{
    at_monk_ring_of_peace(AreaTrigger* areatrigger) : AreaTriggerAI(areatrigger) { }

    void OnUnitEnter(Unit* unit) override
    {
        Unit* caster = at->GetCaster();

        if (!caster || !unit)
            return;

        if (!caster->ToPlayer())
            return;

        if (!caster->IsFriendlyTo(unit))
        {
            caster->CastSpell(unit, SPELL_MONK_RING_OF_PEACE_KNOCKBACK, true);
        }
    }
};

// Song of Chi-Ji - 198898
// AreaTriggerID - 5484 
struct at_monk_song_of_chi_ji : AreaTriggerAI
{
    at_monk_song_of_chi_ji(AreaTrigger* areatrigger) : AreaTriggerAI(areatrigger) { }

    void OnUnitEnter(Unit* unit) override
    {
        Unit* caster = at->GetCaster();

        if (!caster || !unit)
            return;

        if (!caster->ToPlayer())
            return;

        if (!caster->IsFriendlyTo(unit))
        {
            caster->CastSpell(unit, SPELL_MONK_SONG_OF_CHI_JI_STUN, true);
        }
    }
};

// 115098 - Chi Wave
class spell_monk_chi_wave : public SpellScriptLoader
{
public:
    spell_monk_chi_wave() : SpellScriptLoader("spell_monk_chi_wave") {}

    class spell_monk_chi_wave_SpellScript : public SpellScript
    {
        PrepareSpellScript(spell_monk_chi_wave_SpellScript);

        void HandleDummy(SpellEffIndex /*effIndex*/)
        {
            Unit* caster = GetCaster();
            Unit* target = GetHitUnit();
            if (!target)
                return;

            if (caster->IsFriendlyTo(target))
                caster->CastSpell(target, 132464, CastSpellExtraArgs(TRIGGERED_FULL_MASK).AddSpellMod(SPELLVALUE_BASE_POINT1, GetEffectValue()));
            else if (!caster->IsFriendlyTo(target))
                caster->CastSpell(target, 132467, CastSpellExtraArgs(TRIGGERED_FULL_MASK).AddSpellMod(SPELLVALUE_BASE_POINT1, GetEffectValue()));
        }

        void Register() override
        {
            OnEffectHitTarget += SpellEffectFn(spell_monk_chi_wave_SpellScript::HandleDummy, EFFECT_0, SPELL_EFFECT_DUMMY);
        }
    };

    SpellScript* GetSpellScript() const override
    {
        return new spell_monk_chi_wave_SpellScript();
    }
};

// Chi Wave (healing bolt) - 132464
class spell_monk_chi_wave_healing_bolt : public SpellScriptLoader
{
public:
    spell_monk_chi_wave_healing_bolt() : SpellScriptLoader("spell_monk_chi_wave_healing_bolt") { }

    class spell_monk_chi_wave_healing_bolt_SpellScript : public SpellScript
    {
        PrepareSpellScript(spell_monk_chi_wave_healing_bolt_SpellScript);

        void HandleOnHit()
        {
            if (!GetOriginalCaster())
                return;

            if (Player* _player = GetOriginalCaster()->ToPlayer())
                if (Unit* target = GetHitUnit())
                    _player->CastSpell(target, SPELL_MONK_CHI_WAVE_HEAL, true);
        }

        void Register() override
        {
            OnHit += SpellHitFn(spell_monk_chi_wave_healing_bolt_SpellScript::HandleOnHit);
        }
    };

    SpellScript* GetSpellScript() const override
    {
        return new spell_monk_chi_wave_healing_bolt_SpellScript();
    }
};

// 132464 - Chi Wave (heal missile)
class spell_monk_chi_wave_heal_missile : public SpellScriptLoader
{
public:
    spell_monk_chi_wave_heal_missile() : SpellScriptLoader("spell_monk_chi_wave_heal_missile") {}

    class spell_monk_chi_wave_heal_missile_AuraScript : public AuraScript
    {
        PrepareAuraScript(spell_monk_chi_wave_heal_missile_AuraScript);

        void OnRemove(AuraEffect const* aurEff, AuraEffectHandleModes /*mode*/)
        {
            Unit* caster = GetCaster();
            Unit* target = GetTarget();
            if (!target || !caster)
                return;

            caster->CastSpell(target, 132463, true);
            // rerun target selector
            caster->CastSpell(target, 132466, CastSpellExtraArgs(TRIGGERED_FULL_MASK).AddSpellMod(SPELLVALUE_BASE_POINT1, aurEff->GetAmount() - 1).SetTriggeringAura(aurEff));
        }

        void Register() override
        {
            OnEffectRemove += AuraEffectRemoveFn(spell_monk_chi_wave_heal_missile_AuraScript::OnRemove, EFFECT_1, SPELL_AURA_DUMMY, AURA_EFFECT_HANDLE_REAL);
        }
    };

    AuraScript* GetAuraScript() const override
    {
        return new spell_monk_chi_wave_heal_missile_AuraScript();
    }
};

// 132467 - Chi Wave (damage missile)
class spell_monk_chi_wave_damage_missile : public SpellScriptLoader
{
public:
    spell_monk_chi_wave_damage_missile() : SpellScriptLoader("spell_monk_chi_wave_damage_missile") {}

    class spell_monk_chi_wave_damage_missile_AuraScript : public AuraScript
    {
        PrepareAuraScript(spell_monk_chi_wave_damage_missile_AuraScript);

        void OnRemove(AuraEffect const* aurEff, AuraEffectHandleModes /*mode*/)
        {
            Unit* caster = GetCaster();
            Unit* target = GetTarget();
            if (!target || !caster)
                return;

            // rerun target selector
            caster->CastSpell(target, 132466, CastSpellExtraArgs(TRIGGERED_FULL_MASK).AddSpellMod(SPELLVALUE_BASE_POINT1, aurEff->GetAmount() - 1).SetTriggeringAura(aurEff));
        }

        void Register() override
        {
            OnEffectRemove += AuraEffectRemoveFn(spell_monk_chi_wave_damage_missile_AuraScript::OnRemove, EFFECT_1, SPELL_AURA_DUMMY, AURA_EFFECT_HANDLE_REAL);
        }
    };

    AuraScript* GetAuraScript() const override
    {
        return new spell_monk_chi_wave_damage_missile_AuraScript();
    }
};

// 132466 - Chi Wave (target selector)
class spell_monk_chi_wave_target_selector : public SpellScript
{
    class DamageUnitCheck
    {
    public:
        DamageUnitCheck(Unit const* source, float range) : m_source(source), m_range(range) {}
        bool operator()(WorldObject* object)
        {
            Unit* unit = object->ToUnit();
            if (!unit)
                return true;

            if (m_source->IsValidAttackTarget(unit) && unit->isTargetableForAttack() && m_source->IsWithinDistInMap(unit, m_range))
            {
                m_range = m_source->GetDistance(unit);
                return false;
            }

            return true;
        }
    private:
        Unit const* m_source;
        float m_range;
    };

    class HealUnitCheck
    {
    public:
        HealUnitCheck(Unit const* source) : m_source(source) {}
        bool operator()(WorldObject* object)
        {
            Unit* unit = object->ToUnit();
            if (!unit)
                return true;

            if (m_source->IsFriendlyTo(unit))
                return false;

            return true;
        }
    private:
        Unit const* m_source;
    };

    bool Load() override
    {
        m_shouldHeal = true; // just for initializing
        return true;
    }

    void SelectTarget(std::list<WorldObject*>& targets)
    {
        if (targets.empty())
            return;

        SpellInfo const* spellInfo = GetTriggeringSpell();
        if (spellInfo->Id == 132467) // Triggered by damage, so we need heal selector
        {
            targets.remove_if(HealUnitCheck(GetCaster()));
            targets.sort(Trinity::HealthPctOrderPred(false)); // Reverse order due to target is selected via std::list back
            m_shouldHeal = true;
        }
        else if (spellInfo->Id == 132464) // Triggered by heal, so we need damage selector
        {
            targets.remove_if(DamageUnitCheck(GetCaster(), 25.0f));
            m_shouldHeal = false;
        }

        if (targets.empty())
            return;

        WorldObject* target = targets.back();
        if (!target)
            return;

        targets.clear();
        targets.push_back(target);
    }

    void HandleDummy(SpellEffIndex /*effIndex*/)
    {
        if (!GetEffectValue()) // Ran out of bounces
            return;

        if (!GetExplTargetUnit() || !GetOriginalCaster())
            return;

        Unit* target = GetHitUnit();
        if (m_shouldHeal)
            GetExplTargetUnit()->CastSpell(target, 132464, CastSpellExtraArgs(TRIGGERED_FULL_MASK).AddSpellMod(SPELLVALUE_BASE_POINT1, GetEffectValue()).SetOriginalCaster(GetOriginalCaster()->GetGUID()));
        else
            GetExplTargetUnit()->CastSpell(target, 132467, CastSpellExtraArgs(TRIGGERED_FULL_MASK).AddSpellMod(SPELLVALUE_BASE_POINT1, GetEffectValue()).SetOriginalCaster(GetOriginalCaster()->GetGUID()));
    }

    void Register() override
    {
        OnObjectAreaTargetSelect += SpellObjectAreaTargetSelectFn(spell_monk_chi_wave_target_selector::SelectTarget, EFFECT_1, TARGET_UNIT_DEST_AREA_ENTRY);
        OnEffectHitTarget += SpellEffectFn(spell_monk_chi_wave_target_selector::HandleDummy, EFFECT_1, SPELL_EFFECT_DUMMY);
    }

    bool m_shouldHeal;
};

//123986, 5300
struct at_monk_chi_burst : AreaTriggerAI
{
    at_monk_chi_burst(AreaTrigger* at) : AreaTriggerAI(at) { }

    void OnUnitEnter(Unit* unit) override
    {
        Unit* caster = at->GetCaster();

        if (!caster || !unit)
            return;

        if (!caster->ToPlayer())
            return;

        for (auto itr : at->GetInsideUnits())
        {
            Unit* target = ObjectAccessor::GetUnit(*caster, itr);
            if (!caster->IsFriendlyTo(unit))
            {
                caster->CastSpell(unit, SPELL_MONK_CHI_BURST_HEAL, true);
            }
            else
            {
                caster->CastSpell(unit, SPELL_MONK_CHI_BURST_DAMAGE, true);
            }
        }
    }
};

// Chi Burst heal - 130654 - POSSIBLE CRASH
class spell_monk_chi_burst_heal : public SpellScriptLoader
{
public:
    spell_monk_chi_burst_heal() : SpellScriptLoader("spell_monk_chi_burst_heal") { }

    class spell_monk_chi_burst_heal_SpellScript : public SpellScript
    {
        PrepareSpellScript(spell_monk_chi_burst_heal_SpellScript);

        void HandleHeal(SpellEffIndex /*effIndex*/)
        {
            Unit* caster = GetCaster();
            Unit* unit = GetHitUnit();
            if (!caster || !unit)
                return;

            SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(SPELL_MONK_CHI_BURST_HEAL, DIFFICULTY_NONE);
            if (!spellInfo)
                return;

            int32 damage = (float)caster->GetTotalAttackPowerValue(BASE_ATTACK) * 4.125f;
            damage = caster->SpellDamageBonusDone(unit, spellInfo, damage, HEAL, spellInfo->GetEffect(EFFECT_0));
            damage = unit->SpellDamageBonusTaken(caster, spellInfo, damage, HEAL);

            SetHitHeal(damage);
        }

        void Register() override
        {
            OnEffectHitTarget += SpellEffectFn(spell_monk_chi_burst_heal_SpellScript::HandleHeal, EFFECT_0, SPELL_EFFECT_HEAL);
        }
    };

    SpellScript* GetSpellScript() const override
    {
        return new spell_monk_chi_burst_heal_SpellScript();
    }
};

//115399
class spell_monk_black_ox_brew : public SpellScriptLoader
{
public:
    spell_monk_black_ox_brew() : SpellScriptLoader("spell_monk_black_ox_brew") { }

    class spell_monk_black_ox_brew_SpellScript : public SpellScript
    {
        PrepareSpellScript(spell_monk_black_ox_brew_SpellScript);

        void HandleHit(SpellEffIndex /*effIndex*/)
        {
            Unit* caster = GetCaster();
            if (!caster)
                return;

            caster->GetSpellHistory()->ResetCharges(sSpellMgr->GetSpellInfo(SPELL_MONK_PURIFYING_BREW, DIFFICULTY_NONE)->ChargeCategoryId);
        }

        void Register() override
        {
            OnEffectHitTarget += SpellEffectFn(spell_monk_black_ox_brew_SpellScript::HandleHit, EFFECT_0, SPELL_EFFECT_ENERGIZE);
        }
    };

    SpellScript* GetSpellScript() const override
    {
        return new spell_monk_black_ox_brew_SpellScript();
    }
};

//115313 - Summon Jade Serpent Statue
class spell_monk_jade_serpent_statue : public SpellScriptLoader
{
public:
    spell_monk_jade_serpent_statue() : SpellScriptLoader("spell_monk_jade_serpent_statue") { }

    class spell_monk_jade_serpent_statue_SpellScript : public SpellScript
    {
        PrepareSpellScript(spell_monk_jade_serpent_statue_SpellScript);

        void HandleSummon()
        {
            Unit* caster = GetCaster();
            if (!caster)
                return;

            Player* player = caster->ToPlayer();
            if (!player)
                return;

            std::list<Creature*> serpentStatueList;
            player->GetCreatureListWithEntryInGrid(serpentStatueList, MONK_NPC_JADE_SERPENT_STATUE, 500.0f);

            for (std::list<Creature*>::iterator i = serpentStatueList.begin(); i != serpentStatueList.end(); ++i)
            {
                Unit* owner = (*i)->GetOwner();

                if (owner && owner == player && (*i)->IsSummon())
                    continue;

                i = serpentStatueList.erase(i);
            }

            if ((int32)serpentStatueList.size() >= 1)
                serpentStatueList.back()->ToTempSummon()->UnSummon();
        }

        void Register() override
        {
            OnCast += SpellCastFn(spell_monk_jade_serpent_statue_SpellScript::HandleSummon);
        }
    };

    SpellScript* GetSpellScript() const override
    {
        return new spell_monk_jade_serpent_statue_SpellScript();
    }
};

//60849 - Jade Serpent Statue
struct npc_monk_jade_serpent_statue : public ScriptedAI
{
    npc_monk_jade_serpent_statue(Creature* c) : ScriptedAI(c) { }

    void UpdateAI(uint32 diff) override
    {
        if (Unit* owner = me->GetOwner())
        {
            if (Player* player = owner->ToPlayer())
            {
                if (player->GetClass() != CLASS_MONK)
                    me->DespawnOrUnsummon();
            }
        }
    }
};

// 101643
class spell_monk_transcendence : public SpellScript
{
public:

    void HandleSummon(Creature* creature)
    {
        DespawnSpirit(GetCaster());
        GetCaster()->CastSpell(creature, SPELL_MONK_TRANSCENDENCE_CLONE_TARGET, true);
        creature->CastSpell(creature, SPELL_MONK_TRANSCENDENCE_VISUAL, true);
        creature->SetAIAnimKitId(2223); // Sniff Data
        creature->SetDisableGravity(true);
        creature->SetControlled(true, UNIT_STATE_ROOT);
        GetCaster()->VariableStorage.Set(MONK_TRANSCENDENCE_GUID, creature->GetGUID());
    }

    static Creature* GetSpirit(Unit* caster)
    {
        ObjectGuid spiritGuid = caster->VariableStorage.GetValue<ObjectGuid>(MONK_TRANSCENDENCE_GUID, ObjectGuid());

        if (spiritGuid.IsEmpty())
            return nullptr;

        return ObjectAccessor::GetCreature(*caster, spiritGuid);
    }

    static void DespawnSpirit(Unit* caster)
    {
        // Remove previous one if any
        if (Creature* spirit = GetSpirit(caster))
            spirit->DespawnOrUnsummon();

        caster->VariableStorage.Remove(MONK_TRANSCENDENCE_GUID);
    }

    void Register() override
    {
        OnEffectSummon += SpellOnEffectSummonFn(spell_monk_transcendence::HandleSummon);
    }
};

// 210802 - Spirit of the Crane (Passive)
class spell_monk_spirit_of_the_crane_passive : public AuraScript
{

    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo(
            {
                SPELL_MONK_SPIRIT_OF_THE_CRANE_MANA,
                SPELL_MONK_BLACKOUT_KICK_TRIGGERED
            });
    }

    bool CheckProc(ProcEventInfo& eventInfo)
    {
        if (eventInfo.GetSpellInfo()->Id != SPELL_MONK_BLACKOUT_KICK_TRIGGERED)
            return false;
        return true;
    }

    void HandleProc(AuraEffect* /*aurEff*/, ProcEventInfo& /*eventInfo*/)
    {
        // TODO: Basepoints can be float now... this is 1 but needs to be lower.
        GetTarget()->CastSpell(GetTarget(), SPELL_MONK_SPIRIT_OF_THE_CRANE_MANA, true);
    }

    void Register() override
    {
        DoCheckProc += AuraCheckProcFn(spell_monk_spirit_of_the_crane_passive::CheckProc);
        OnEffectProc += AuraEffectProcFn(spell_monk_spirit_of_the_crane_passive::HandleProc, EFFECT_0, SPELL_AURA_DUMMY);
    }
};

// 101643
class aura_monk_transcendence : public AuraScript
{

    void OnRemove(AuraEffect const* /*aurEff*/, AuraEffectHandleModes /*mode*/)
    {
        spell_monk_transcendence::DespawnSpirit(GetTarget());

        if (GetTarget()->HasAura(SPELL_MONK_TRANSCENDENCE_RANGE_CHECK))
        {
            GetTarget()->RemoveAura(SPELL_MONK_TRANSCENDENCE_RANGE_CHECK);
        }
    }

    void HandleDummyTick(AuraEffect const* /*aurEff*/)
    {
        SpellInfo const* spellInfo = sSpellMgr->AssertSpellInfo(SPELL_MONK_TRANSCENDENCE_TRANSFER, GetCastDifficulty());

        Unit* spirit = spell_monk_transcendence::GetSpirit(GetTarget());
        if (spirit && GetTarget()->IsWithinDist(spirit, spellInfo->GetMaxRange(true)))
        {
            if (!GetTarget()->HasAura(SPELL_MONK_TRANSCENDENCE_RANGE_CHECK))
            {
                GetTarget()->CastSpell(GetTarget(), SPELL_MONK_TRANSCENDENCE_RANGE_CHECK, true);
            }
        }
        else
        {
            if (GetTarget()->HasAura(SPELL_MONK_TRANSCENDENCE_RANGE_CHECK))
            {
                GetTarget()->RemoveAura(SPELL_MONK_TRANSCENDENCE_RANGE_CHECK);
            }
        }
    }

    void Register() override
    {
        OnEffectRemove += AuraEffectRemoveFn(aura_monk_transcendence::OnRemove, EFFECT_1, SPELL_AURA_PERIODIC_DUMMY, AURA_EFFECT_HANDLE_REAL);
        OnEffectPeriodic += AuraEffectPeriodicFn(aura_monk_transcendence::HandleDummyTick, EFFECT_1, SPELL_AURA_PERIODIC_DUMMY);
    }
};

// 119996 - Transcendence: Transfer
class spell_monk_transcendence_transfer : public SpellScript
{

    SpellCastResult CheckCast()
    {
        Unit* caster = GetCaster();

        if (!caster)
            return SPELL_FAILED_ERROR;

        Unit* spirit = spell_monk_transcendence::GetSpirit(caster);
        if (!spirit)
        {
            SetCustomCastResultMessage(SPELL_CUSTOM_ERROR_YOU_HAVE_NO_SPIRIT_ACTIVE);
            return SPELL_FAILED_CUSTOM_ERROR;
        }

        if (!spirit->IsWithinDist(caster, GetSpellInfo()->GetMaxRange(true, caster, GetSpell())))
            return SPELL_FAILED_OUT_OF_RANGE;

        return SPELL_CAST_OK;
    }

    void HandleOnCast()
    {
        Unit* caster = GetCaster();
        if (!caster)
            return;

        Unit* spirit = spell_monk_transcendence::GetSpirit(caster);
        if (!spirit)
            return;

        caster->NearTeleportTo(*spirit, true);
        spirit->NearTeleportTo(*caster, true);
    }

    void Register() override
    {
        OnCheckCast += SpellCheckCastFn(spell_monk_transcendence_transfer::CheckCast);
        OnCast += SpellCastFn(spell_monk_transcendence_transfer::HandleOnCast);
    }
};

// 116670 - Vivify
class spell_monk_vivify : public SpellScriptLoader
{
public:
    spell_monk_vivify() : SpellScriptLoader("spell_monk_vivify") { }

    class spell_monk_vivify_SpellScript : public SpellScript
    {

        void FilterRenewingMist(std::list<WorldObject*>& targets)
        {
            targets.remove_if(Trinity::UnitAuraCheck(false, SPELL_MONK_RENEWING_MIST_HOT, GetCaster()->GetGUID()));
        }

        void HandleOnPrepare()
        {
            if (GetCaster()->GetCurrentSpell(CURRENT_CHANNELED_SPELL) && GetCaster()->GetCurrentSpell(CURRENT_CHANNELED_SPELL)->GetSpellInfo()->Id == SPELL_MONK_SOOTHING_MIST)
            {
                GetSpell()->m_castFlagsEx = TRIGGERED_CAST_DIRECTLY;
                SpellCastTargets targets = GetCaster()->GetCurrentSpell(CURRENT_CHANNELED_SPELL)->m_targets;
                GetSpell()->InitExplicitTargets(targets);
            }
        }

        void LifeCycles()
        {
            Player* caster = GetCaster()->ToPlayer();
            if (!caster)
                return;

            if (caster->HasAura(SPELL_LIFECYCLES))
            {
                caster->CastSpell(caster, SPELL_MONK_LIFECYCLES_ENVELOPING_MIST, true);
            }
        }

        void Register() override
        {
            OnPrepare += SpellOnPrepareFn(spell_monk_vivify_SpellScript::HandleOnPrepare);
            OnObjectAreaTargetSelect += SpellObjectAreaTargetSelectFn(spell_monk_vivify_SpellScript::FilterRenewingMist, EFFECT_1, TARGET_UNIT_DEST_AREA_ALLY);
            AfterCast += SpellCastFn(spell_monk_vivify_SpellScript::LifeCycles);
        }
    };

    SpellScript* GetSpellScript() const override
    {
        return new spell_monk_vivify_SpellScript();
    }
};

// Soothing Mist - 115175
class spell_monk_soothing_mist : public SpellScriptLoader
{
public:
    spell_monk_soothing_mist() : SpellScriptLoader("spell_monk_soothing_mist") { }

    class spell_monk_soothing_mist_AuraScript : public AuraScript
    {
        PrepareAuraScript(spell_monk_soothing_mist_AuraScript);

        void OnApply(AuraEffect const* /*aurEff*/, AuraEffectHandleModes /*mode*/)
        {
            if (!GetCaster())
                return;

            if (Unit* target = GetTarget())
                target->CastSpell(target, SPELL_MONK_SOOTHING_MIST_VISUAL, true);

            if (Player* player = GetCaster()->ToPlayer())
            {
                if (player->HasSpell(115294));
                {
                    player->CastSpell(player, SPELL_MONK_MANA_TEA_STACKS, true);
                }

                if (Unit* target = GetTarget())
                {
                    std::list<Unit*> playerList;
                    std::list<Creature*> tempList;
                    std::list<Creature*> statueList;
                    Creature* statue;

                    player->GetPartyMembers(playerList);

                    if (playerList.size() > 1)
                    {
                        playerList.remove(target);
                        playerList.sort(Trinity::HealthPctOrderPred());
                        playerList.resize(1);
                    }

                    player->GetCreatureListWithEntryInGrid(tempList, 60849, 100.0f);
                    player->GetCreatureListWithEntryInGrid(statueList, 60849, 100.0f);

                    for (std::list<Creature*>::iterator i = tempList.begin(); i != tempList.end(); ++i)
                    {
                        Unit* owner = (*i)->GetOwner();
                        if (owner && owner == player && (*i)->IsSummon())
                            continue;

                        statueList.remove((*i));
                    }

                    for (auto itr : playerList)
                    {
                        if (statueList.size() == 1)
                        {
                            for (auto itrBis : statueList)
                                statue = itrBis;

                            if (statue->GetOwner() && statue->GetOwner()->GetGUID() == player->GetGUID())
                            {
                                if (statue->GetOwner() && statue->GetOwner()->GetGUID() == player->GetGUID())
                                    statue->CastSpell(statue->GetOwner()->ToPlayer()->GetSelectedUnit(), SPELL_SERPENT_STATUE_SOOTHING_MIST, false);
                            }
                        }
                    }
                }
            }
        }

        void HandleEffectPeriodic(AuraEffect const* /*aurEff*/)
        {
            if (Unit* caster = GetCaster())
                if (GetTarget())
                    // 25% to give 1 chi per tick
                    if (roll_chance_i(25))
                        caster->CastSpell(caster, SPELL_MONK_SOOTHING_MIST_ENERGIZE, true);
        }

        void OnRemove(AuraEffect const* /* aurEff */, AuraEffectHandleModes /*mode*/)
        {
            if (GetCaster())
                if (Unit* target = GetTarget())
                    if (target->HasAura(SPELL_MONK_SOOTHING_MIST_VISUAL))
                        target->RemoveAura(SPELL_MONK_SOOTHING_MIST_VISUAL);
        }

        void Register() override
        {
            AfterEffectApply += AuraEffectApplyFn(spell_monk_soothing_mist_AuraScript::OnApply, EFFECT_0, SPELL_AURA_PERIODIC_HEAL, AURA_EFFECT_HANDLE_REAL);
            OnEffectPeriodic += AuraEffectPeriodicFn(spell_monk_soothing_mist_AuraScript::HandleEffectPeriodic, EFFECT_0, SPELL_AURA_PERIODIC_HEAL);
            AfterEffectRemove += AuraEffectRemoveFn(spell_monk_soothing_mist_AuraScript::OnRemove, EFFECT_0, SPELL_AURA_PERIODIC_HEAL, AURA_EFFECT_HANDLE_REAL);
        }
    };

    AuraScript* GetAuraScript() const override
    {
        return new spell_monk_soothing_mist_AuraScript();
    }
};

// 116680 Thunder Focus Tea
// 197895 Focused Thunder
class spell_focused_thunder_talent_thunder_focus_tea : public SpellScriptLoader
{
public:
    spell_focused_thunder_talent_thunder_focus_tea() : SpellScriptLoader("spell_focused_thunder_talent_thunder_focus_tea") { }

    class spell_focused_thunder_talent_thunder_focus_tea_AuraScript : public AuraScript
    {

        void OnApply(AuraEffect const* /*aurEff*/, AuraEffectHandleModes /*mode*/)
        {
            Unit* caster = GetCaster()->ToPlayer();
            if (!caster)
                return;

            if (caster->HasAura(SPELL_FOCUSED_THUNDER_TALENT))
            {
                if (Aura* thunder = caster->GetAura(SPELL_MONK_THUNDER_FOCUS_TEA))
                    thunder->SetStackAmount(2);
            }
        }

        void Register()
        {
            OnEffectApply += AuraEffectApplyFn(spell_focused_thunder_talent_thunder_focus_tea_AuraScript::OnApply, EFFECT_0, SPELL_AURA_ADD_FLAT_MODIFIER, AURA_EFFECT_HANDLE_REAL);
        }

    };

    AuraScript* GetAuraScript() const
    {
        return new spell_focused_thunder_talent_thunder_focus_tea_AuraScript();
    }
};

// Zen Pulse - 124081
class spell_monk_zen_pulse : public SpellScriptLoader
{
public:
    spell_monk_zen_pulse() : SpellScriptLoader("spell_monk_zen_pulse") {}

    class spell_monk_zen_pulse_SpellScript : public SpellScript
    {
        void OnHit(SpellEffIndex /*effIndex*/) 
        {
            GetCaster()->CastSpell(GetCaster(), SPELL_MONK_ZEN_PULSE_HEAL, true);
        }

        void Register() override
        {
            OnEffectHitTarget += SpellEffectFn(spell_monk_zen_pulse_SpellScript::OnHit, EFFECT_1, SPELL_EFFECT_SCHOOL_DAMAGE);
        }
    };

    SpellScript* GetSpellScript() const override
    {
        return new spell_monk_zen_pulse_SpellScript();
    }
};

// 115151 - Renewing Mist
class spell_monk_renewing_mist : public SpellScript
{
    PrepareSpellScript(spell_monk_renewing_mist);

    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo(
            {
                SPELL_MONK_RENEWING_MIST_HOT
            });
    }

    void HandleDummy(SpellEffIndex effIndex)
    {
        PreventHitDefaultEffect(effIndex);
        if (Unit* target = GetExplTargetUnit())
            GetCaster()->CastSpell(target, SPELL_MONK_RENEWING_MIST_HOT, true);
    }

    void HandleOnCast()
    {
        if (Unit* caster = GetCaster())
        {
            if (caster->HasSpell(115294))
            {
                caster->CastSpell(caster, SPELL_MONK_MANA_TEA_STACKS, true);
            }
            else
            {
                return;
            }
        }
    }

    void Register() override
    {
        OnCast += SpellCastFn(spell_monk_renewing_mist::HandleOnCast);
        OnEffectLaunch += SpellEffectFn(spell_monk_renewing_mist::HandleDummy, EFFECT_0, SPELL_EFFECT_DUMMY);
    }
};

// 119611 - Renewing Mist (HoT)
class spell_monk_renewing_mist_hot : public AuraScript
{
    PrepareAuraScript(spell_monk_renewing_mist_hot);

    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo(
            {
                SPELL_MONK_RENEWING_MIST_JUMP,
                SPELL_MONK_RENEWING_MIST
            });
    }

    void HandlePeriodicHeal(AuraEffect const* /*aurEff*/)
    {
        Unit* caster = GetCaster();
        if (!caster)
            return;

        if (GetTarget()->IsFullHealth())
            caster->CastSpell(GetTarget(), SPELL_MONK_RENEWING_MIST_JUMP, true);
    }

    void CalcAmount(AuraEffect const* /*aurEff*/, int32& amount, bool& /*canBeRecalculated*/)
    {
        Unit* caster = GetCaster();
        if (Aura* counteractAura = caster->GetAura(SPELL_MONK_COUNTERACT_MAGIC))
        {
            Unit::AuraApplicationMap& appliedAuras = GetUnitOwner()->GetAppliedAuras();
            for (Unit::AuraApplicationMap::iterator iter = appliedAuras.begin(); iter != appliedAuras.end(); ++iter)
            {
                Aura* baseAura = iter->second->GetBase();

                if (baseAura->GetSpellInfo()->IsPositive())
                    continue;

                if (!(baseAura->GetSpellInfo()->GetSchoolMask() & SPELL_SCHOOL_MASK_MAGIC))
                    continue;

                if (!(baseAura->GetSpellInfo()->GetDispelMask() & 1 << DISPEL_MAGIC))
                    continue;

                if (baseAura->HasEffectType(SPELL_AURA_PERIODIC_DAMAGE) ||
                    baseAura->HasEffectType(SPELL_AURA_PERIODIC_DAMAGE_PERCENT))
                {
                    if (AuraEffect const* effInfo = counteractAura->GetEffect(EFFECT_0))
                    {
                        // TODO: Idk why this all is not increasing the amount
                        AddPct(amount, effInfo->GetAmount());
                    }
                }
            }
        }
    }

    void Register() override
    {
        OnEffectPeriodic += AuraEffectPeriodicFn(spell_monk_renewing_mist_hot::HandlePeriodicHeal, EFFECT_0, SPELL_AURA_PERIODIC_HEAL);
        //DoEffectCalcAmount += AuraEffectCalcAmountFn(spell_monk_renewing_mist_hot::CalcAmount, EFFECT_0, SPELL_AURA_PERIODIC_HEAL);
    }
};

// 119607 - Renewing Mist Jump
class spell_monk_renewing_mist_jump : public SpellScript
{
    PrepareSpellScript(spell_monk_renewing_mist_jump);

    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo(
            {
                SPELL_MONK_RENEWING_MIST_HOT
            });
    }

    void HandleTargets(std::list<WorldObject*>& targets)
    {
        Unit* caster = GetCaster();
        Unit* previousTarget = GetExplTargetUnit();

        // Not remove full health targets now, dancing mists talent can jump on full health too
        targets.remove_if([caster, previousTarget](WorldObject* a)
        {
            Unit* ally = a->ToUnit();
            if (!ally || ally->HasAura(SPELL_MONK_RENEWING_MIST_HOT, caster->GetGUID()) || ally == previousTarget)
                return true;

            return false;
        });

        targets.remove_if([](WorldObject* a)
        {
            Unit* ally = a->ToUnit();
            if (!ally || ally->IsFullHealth())
                return true;

            return false;
        });

        if (targets.size() > 1)
        {
            targets.sort(Trinity::HealthPctOrderPred());
            targets.resize(1);
        }

        _previousTargetGuid = previousTarget->GetGUID();
    }

    void HandleHit(SpellEffIndex effIndex)
    {
        PreventHitDefaultEffect(effIndex);
        Unit* caster = GetCaster();
        Unit* previousTarget = ObjectAccessor::GetUnit(*caster, _previousTargetGuid);

        if (previousTarget)
        {
            if (Aura* oldAura = previousTarget->GetAura(SPELL_MONK_RENEWING_MIST_HOT, GetCaster()->GetGUID()))
            {
                if (Aura* newAura = caster->AddAura(SPELL_MONK_RENEWING_MIST_HOT, GetHitUnit()))
                {
                    newAura->SetDuration(oldAura->GetDuration());
                    previousTarget->SendPlaySpellVisual(GetHitUnit()->GetPosition(), previousTarget->GetOrientation(), SPELL_MONK_VISUAL_RENEWING_MIST, 0, 0, 50.f, false);
                    oldAura->Remove();
                }
            }
        }
    }

private:
    ObjectGuid _previousTargetGuid;
    ObjectGuid _additionalTargetGuid;

    void Register() override
    {
        OnObjectAreaTargetSelect += SpellObjectAreaTargetSelectFn(spell_monk_renewing_mist_jump::HandleTargets, EFFECT_1, TARGET_UNIT_DEST_AREA_ALLY);
        OnEffectHitTarget += SpellEffectFn(spell_monk_renewing_mist_jump::HandleHit, EFFECT_1, SPELL_EFFECT_DUMMY);
    }
};

//191837 - Essence Font
class spell_monk_essence_font : public SpellScript
{
    PrepareSpellScript(spell_monk_essence_font);

    void HandleOnCast()
    {
        if (Unit* caster = GetCaster())
        {
            caster->AddAura(SPELL_MONK_ESSENCE_FONT_PERIODIC_HEAL, nullptr);
            std::list<Unit*> u_li;
            uint8 targetLimit = 6;
            Trinity::Containers::RandomResize(u_li, targetLimit);
            caster->GetFriendlyUnitListInRange(u_li, 30.0f, false);
            for (auto& targets : u_li)
            {
                caster->AddAura(SPELL_MONK_ESSENCE_FONT_PERIODIC_HEAL, targets);
            }
        }
    }

    void Register() override
    {
        OnCast += SpellCastFn(spell_monk_essence_font::HandleOnCast);
    }
};

//191840 - Essence Font (Heal)
class spell_monk_essence_font_heal : public SpellScriptLoader
{
public:
    spell_monk_essence_font_heal() : SpellScriptLoader("spell_monk_essence_font_heal") { }

    class spell_monk_essence_font_heal_SpellScript : public SpellScript
    {
        PrepareSpellScript(spell_monk_essence_font_heal_SpellScript);

        void FilterTargets(std::list<WorldObject*>& p_Targets)
        {
            if (Unit* caster = GetCaster())
            {
                p_Targets.remove_if([caster](WorldObject* object) -> bool
                {
                    if (object == nullptr || object->ToUnit() == nullptr)
                        return true;

                    Unit* unit = object->ToUnit();

                    if (unit == caster)
                        return true;
                    //If the target has the aura and the aura has more than 5 second duration (meaning it was cast less than 1 second ago) we dont keep it
                    if (unit->HasAura(SPELL_MONK_ESSENCE_FONT_HEAL) && unit->GetAura(SPELL_MONK_ESSENCE_FONT_HEAL)->GetDuration() > 5 * IN_MILLISECONDS)
                        return true;

                    return false;
                });

                if (p_Targets.size() > 1)
                {
                    p_Targets.sort(Trinity::HealthPctOrderPred());
                    p_Targets.resize(1);
                }
            }
        }

        void Register() override
        {
            OnObjectAreaTargetSelect += SpellObjectAreaTargetSelectFn(spell_monk_essence_font_heal_SpellScript::FilterTargets, EFFECT_0, TARGET_UNIT_DEST_AREA_ALLY);
            OnObjectAreaTargetSelect += SpellObjectAreaTargetSelectFn(spell_monk_essence_font_heal_SpellScript::FilterTargets, EFFECT_1, TARGET_UNIT_DEST_AREA_ALLY);
        }
    };

    SpellScript* GetSpellScript() const override
    {
        return new spell_monk_essence_font_heal_SpellScript();
    }
};

// 115294 - Mana Tea
class spell_monk_mana_tea : public SpellScript
{

    SpellModifier* mod = nullptr;

    void HandleBeforeCast()
    {
        if (Player* _player = GetCaster()->ToPlayer())
        {
            int32 stacks = 0;

            if (Aura* manaTeaStacks = _player->GetAura(SPELL_MONK_MANA_TEA_STACKS))
            {
                stacks = manaTeaStacks->GetStackAmount();

                int32 newDuration = stacks * IN_MILLISECONDS;


                SpellModifierByClassMask* mod = new SpellModifierByClassMask(manaTeaStacks);
                mod->op = SpellModOp::Duration;
                mod->type = SPELLMOD_FLAT;
                mod->spellId = SPELL_MONK_MANA_TEA_REGEN;
                static_cast<SpellModifierByClassMask*>(mod)->value = newDuration;
                mod->mask[1] = 0x200000;
                mod->mask[2] = 0x1;

                _player->AddSpellMod(mod, true);
            }
        }
    }

    void HandleAfterCast()
    {
        if (mod)
            if (Player* _player = GetCaster()->ToPlayer())
                _player->AddSpellMod(mod, false);
    }

    void Register() override
    {
        BeforeCast += SpellCastFn(spell_monk_mana_tea::HandleBeforeCast);
        AfterCast += SpellCastFn(spell_monk_mana_tea::HandleAfterCast);
    }
};

// 115294 - Mana Tea (Aura)
class aura_monk_mana_tea : public AuraScript
{
    void OnTick(const AuraEffect* /*aurEff*/)
    {
        if (GetCaster())
        {
            // remove one charge per tick instead of remove aura on cast
            // "Cancelling the channel will not waste stacks"
            if (Aura* manaTea = GetCaster()->GetAura(SPELL_MONK_MANA_TEA_STACKS))
            {
                if (manaTea->GetStackAmount() > 1)
                    manaTea->SetStackAmount(manaTea->GetStackAmount() - 1);
                else
                    GetCaster()->RemoveAura(SPELL_MONK_MANA_TEA_STACKS);
            }
        }
    }

    void Register() override
    {
        OnEffectPeriodic += AuraEffectPeriodicFn(aura_monk_mana_tea::OnTick, EFFECT_0, SPELL_AURA_MOD_POWER_COST_SCHOOL_PCT);
    }
};

// Enveloping Mist - 124682
class spell_monk_enveloping_mist : public SpellScriptLoader
{
public:
    spell_monk_enveloping_mist() : SpellScriptLoader("spell_monk_enveloping_mist") { }

    class spell_monk_enveloping_mist_SpellScript : public SpellScript
    {
        PrepareSpellScript(spell_monk_enveloping_mist_SpellScript);

        void HandleOnPrepare()
        {
            if (GetCaster()->GetCurrentSpell(CURRENT_CHANNELED_SPELL) && GetCaster()->GetCurrentSpell(CURRENT_CHANNELED_SPELL)->GetSpellInfo()->Id == SPELL_MONK_SOOTHING_MIST)
            {
                GetSpell()->m_castFlagsEx = TRIGGERED_CAST_DIRECTLY;
                SpellCastTargets targets = GetCaster()->GetCurrentSpell(CURRENT_CHANNELED_SPELL)->m_targets;
                GetSpell()->InitExplicitTargets(targets);
            }
        }

        void LifeCycles()
        {
            Player* caster = GetCaster()->ToPlayer();
            if (!caster)
                return;

            if (caster->HasAura(SPELL_LIFECYCLES))
            {
                caster->CastSpell(caster, SPELL_MONK_LIFECYCLES_VIVIFY, true);
            }
        }

        void Register() override
        {
            OnPrepare += SpellOnPrepareFn(spell_monk_enveloping_mist_SpellScript::HandleOnPrepare);
            AfterCast += SpellCastFn(spell_monk_enveloping_mist_SpellScript::LifeCycles);
        }
    };

    SpellScript* GetSpellScript() const override
    {
        return new spell_monk_enveloping_mist_SpellScript();
    }
};

//137639
class spell_monk_storm_earth_and_fire : public AuraScript
{
    PrepareAuraScript(spell_monk_storm_earth_and_fire);

    void HandleApply(AuraEffect const* /*aurEff*/, AuraEffectHandleModes /*mode*/)
    {
        Unit* target = GetTarget();
        target->CastSpell(target, SPELL_MONK_SEF_STORM_VISUAL, true);
        target->CastSpell(target, SPELL_MONK_SEF_SUMMON_EARTH, true);
        target->CastSpell(target, SPELL_MONK_SEF_SUMMON_FIRE, true);
    }

    void HandleRemove(AuraEffect const* /*aurEff*/, AuraEffectHandleModes /*mode*/)
    {
        GetTarget()->RemoveAurasDueToSpell(SPELL_MONK_SEF_STORM_VISUAL);

        if (Creature* fireSpirit = GetTarget()->GetSummonedCreatureByEntry(NPC_FIRE_SPIRIT))
            fireSpirit->ToTempSummon()->DespawnOrUnsummon();

        if (Creature* earthSpirit = GetTarget()->GetSummonedCreatureByEntry(NPC_EARTH_SPIRIT))
            earthSpirit->ToTempSummon()->DespawnOrUnsummon();
    }

    void Register() override
    {
        OnEffectApply += AuraEffectApplyFn(spell_monk_storm_earth_and_fire::HandleApply, EFFECT_0, SPELL_AURA_ADD_PCT_MODIFIER, AURA_EFFECT_HANDLE_REAL);
        OnEffectRemove += AuraEffectRemoveFn(spell_monk_storm_earth_and_fire::HandleRemove, EFFECT_0, SPELL_AURA_ADD_PCT_MODIFIER, AURA_EFFECT_HANDLE_REAL);
    }
};

// 69791 - 69792
struct npc_monk_sef_spirit : public ScriptedAI
{
    npc_monk_sef_spirit(Creature* creature) : ScriptedAI(creature) { }

    void IsSummonedBy(WorldObject* summoner)
    {
        me->SetLevel(summoner->ToUnit()->GetLevel());
        me->SetMaxHealth(summoner->ToUnit()->GetMaxHealth() / 3);
        me->SetFullHealth();
        summoner->CastSpell(me, SPELL_MONK_TRANSCENDENCE_CLONE_TARGET, true);
        me->CastSpell(me, me->GetEntry() == NPC_FIRE_SPIRIT ? SPELL_MONK_SEF_FIRE_VISUAL : SPELL_MONK_SEF_EARTH_VISUAL, true);
        me->CastSpell(me, SPELL_MONK_SEF_SUMMONS_STATS, true);
        int32 attackPower = summoner->ToUnit()->m_unitData->AttackPower / 100 * 45.0f;
        int32 spellPower = summoner->ToUnit()->SpellBaseDamageBonusDone(SPELL_SCHOOL_MASK_NATURE) / 100 * 45.0f;

        if (Unit* target = ObjectAccessor::GetUnit(*summoner, summoner->ToUnit()->GetTarget()))
            me->CastSpell(target, SPELL_MONK_SEF_CHARGE, true);
        else
        {
            if (me->GetEntry() == NPC_FIRE_SPIRIT)
                me->GetMotionMaster()->MoveFollow(summoner->ToUnit(), PET_FOLLOW_DIST, PET_FOLLOW_ANGLE);
            else
                me->GetMotionMaster()->MoveFollow(summoner->ToUnit(), PET_FOLLOW_DIST, PET_FOLLOW_ANGLE * 3);
        }
    }
};

class playerScript_monk_earth_fire_storm : public PlayerScript
{
public:
    playerScript_monk_earth_fire_storm() : PlayerScript("playerScript_monk_earth_fire_storm") { }

    void OnSuccessfulSpellCast(Player* player, Spell* spell) override
    {
        if (player->GetClass() != CLASS_MONK)
            return;

        SpellInfo const* spellInfo = spell->GetSpellInfo();
        if (player->HasAura(SPELL_MONK_SEF) && !spellInfo->IsPositive())
        {
            if (Unit* target = ObjectAccessor::GetUnit(*player, player->GetTarget()))
            {
                if (Creature* fireSpirit = player->GetSummonedCreatureByEntry(NPC_FIRE_SPIRIT))
                {
                    fireSpirit->SetFacingToObject(target, true);
                    fireSpirit->CastSpell(target, spellInfo->Id, true);
                }
                if (Creature* earthSpirit = player->GetSummonedCreatureByEntry(NPC_EARTH_SPIRIT))
                {
                    earthSpirit->SetFacingToObject(target, true);
                    earthSpirit->CastSpell(target, spellInfo->Id, true);
                }
            }
        }
        if (player->HasAura(SPELL_MONK_SEF) && spellInfo->IsPositive())
        {
            if (Unit* GetTarget = player->GetSelectedUnit())
            {
                if (!GetTarget->IsFriendlyTo(player))
                    return;

                if (Creature* fireSpirit = player->GetSummonedCreatureByEntry(NPC_FIRE_SPIRIT))
                {
                    fireSpirit->SetFacingToObject(GetTarget, true);
                    fireSpirit->CastSpell(GetTarget, spellInfo->Id, true);
                }
                if (Creature* earthSpirit = player->GetSummonedCreatureByEntry(NPC_EARTH_SPIRIT))
                {
                    earthSpirit->SetFacingToObject(GetTarget, true);
                    earthSpirit->CastSpell(GetTarget, spellInfo->Id, true);
                }
            }
        }
    }
};

// 122470 - Touch of Karma
class spell_monk_touch_of_karma : public SpellScriptLoader
{
public:
    spell_monk_touch_of_karma() : SpellScriptLoader("spell_monk_touch_of_karma") { }

    class spell_monk_touch_of_karma_AuraScript : public AuraScript
    {
        PrepareAuraScript(spell_monk_touch_of_karma_AuraScript);

        void CalculateAmount(const AuraEffect* aurEff, int32& amount, bool& /*canBeRecalculated*/)
        {
            if (Unit* caster = GetCaster())
                if (int32 effInfo = GetAura()->GetSpellInfo()->GetEffect(EFFECT_2).CalcValue())
                {
                    amount = int32(caster->CountPctFromMaxHealth(effInfo));
                    const_cast<AuraEffect*>(aurEff)->SetAmount(amount);
                }
        }

        void OnAbsorb(AuraEffect* aurEff, DamageInfo& dmgInfo, uint32& /*absorbAmount*/)
        {
            Unit* caster = GetCaster();
            if (!caster)
                return;

            for (AuraApplication* aurApp : caster->GetTargetAuraApplications(SPELL_MONK_TOUCH_OF_KARMA))
                if (aurApp->GetTarget() != caster)
                {
                    int32 periodicDamage = int32(dmgInfo.GetDamage() / sSpellMgr->GetSpellInfo(SPELL_MONK_TOUCH_OF_KARMA_DAMAGE, DIFFICULTY_NONE)->GetMaxTicks());

                    caster->CastSpell(aurApp->GetTarget(), SPELL_MONK_TOUCH_OF_KARMA_DAMAGE, CastSpellExtraArgs(TRIGGERED_FULL_MASK).AddSpellBP0(periodicDamage).SetTriggeringAura(aurEff));
                    if (caster->HasAura(SPELL_GOOD_KARMA_TALENT))
                    {
                        caster->CastSpell(caster, SPELL_GOOD_KARMA_TALENT_HEAL, CastSpellExtraArgs(TRIGGERED_FULL_MASK).AddSpellBP0(periodicDamage).SetTriggeringAura(aurEff));
                    }
                }
        }

        void HandleApply(AuraEffect const* /*aurEff*/, AuraEffectHandleModes /*mode*/)
        {
            GetCaster()->CastSpell(GetCaster(), SPELL_MONK_TOUCH_OF_KARMA_BUFF, true);
        }

        void HandleRemove(AuraEffect const* /*aurEff*/, AuraEffectHandleModes /*mode*/)
        {
            Unit* caster = GetCaster();
            if (!caster)
                return;

            caster->RemoveAura(SPELL_MONK_TOUCH_OF_KARMA_BUFF);
            for (AuraApplication* aurApp : caster->GetTargetAuraApplications(SPELL_MONK_TOUCH_OF_KARMA))
                if (Aura* targetAura = aurApp->GetBase())
                    targetAura->Remove();
        }

        void Register() override
        {
            OnEffectApply += AuraEffectApplyFn(spell_monk_touch_of_karma_AuraScript::HandleApply, EFFECT_0, SPELL_AURA_DUMMY, AURA_EFFECT_HANDLE_REAL);
            DoEffectCalcAmount += AuraEffectCalcAmountFn(spell_monk_touch_of_karma_AuraScript::CalculateAmount, EFFECT_1, SPELL_AURA_SCHOOL_ABSORB);
            OnEffectAbsorb += AuraEffectAbsorbFn(spell_monk_touch_of_karma_AuraScript::OnAbsorb, EFFECT_1);
            OnEffectRemove += AuraEffectRemoveFn(spell_monk_touch_of_karma_AuraScript::HandleRemove, EFFECT_1, SPELL_AURA_SCHOOL_ABSORB, AURA_EFFECT_HANDLE_REAL);
            OnEffectRemove += AuraEffectRemoveFn(spell_monk_touch_of_karma_AuraScript::HandleRemove, EFFECT_0, SPELL_AURA_DUMMY, AURA_EFFECT_HANDLE_REAL);
        }
    };

    AuraScript* GetAuraScript() const override
    {
        return new spell_monk_touch_of_karma_AuraScript();
    }
};

// 125174 - Touch of Karma Buff
class spell_monk_touch_of_karma_buff : public AuraScript
{
    PrepareAuraScript(spell_monk_touch_of_karma_buff);

    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo(
            {
                SPELL_MONK_TOUCH_OF_KARMA
            });
    }

    void HandleRemove(AuraEffect const* /*aurEff*/, AuraEffectHandleModes /*mode*/)
    {
        Unit* caster = GetCaster();
        if (!caster)
            return;

        for (AuraApplication* aurApp : caster->GetTargetAuraApplications(SPELL_MONK_TOUCH_OF_KARMA))
            if (Aura* targetAura = aurApp->GetBase())
                targetAura->Remove();
    }

    void Register() override
    {
        OnEffectRemove += AuraEffectRemoveFn(spell_monk_touch_of_karma_buff::HandleRemove, EFFECT_0, SPELL_AURA_DUMMY, AURA_EFFECT_HANDLE_REAL);
    }
};

// Fists of Fury (stun effect) - 120086
class spell_monk_fists_of_fury_stun : public SpellScriptLoader
{
public:
    spell_monk_fists_of_fury_stun() : SpellScriptLoader("spell_monk_fists_of_fury_stun") { }

    class spell_monk_fists_of_fury_stun_SpellScript : public SpellScript
    {
        PrepareSpellScript(spell_monk_fists_of_fury_stun_SpellScript);

        void RemoveInvalidTargets(std::list<WorldObject*>& targets)
        {
            targets.remove_if(Trinity::UnitAuraCheck(true, GetSpellInfo()->Id));
        }

        void Register() override
        {
            OnObjectAreaTargetSelect += SpellObjectAreaTargetSelectFn(spell_monk_fists_of_fury_stun_SpellScript::RemoveInvalidTargets, EFFECT_0, TARGET_UNIT_CONE_ENEMY_24);
        }
    };

    SpellScript* GetSpellScript() const override
    {
        return new spell_monk_fists_of_fury_stun_SpellScript();
    }
};

class spell_monk_fists_of_fury_visual_filter : public SpellScriptLoader
{
public:
    spell_monk_fists_of_fury_visual_filter() : SpellScriptLoader("spell_monk_fists_of_fury_visual_filter") { }

    class spell_monk_fists_of_fury_visual_filter_SpellScript : public SpellScript
    {
        PrepareSpellScript(spell_monk_fists_of_fury_visual_filter_SpellScript);

        void RemoveInvalidTargets(std::list<WorldObject*>& targets)
        {
            targets.remove_if(Trinity::UnitAuraCheck(true, 123154, GetCaster()->GetGUID()));
        }

        void Register() override
        {
            OnObjectAreaTargetSelect += SpellObjectAreaTargetSelectFn(spell_monk_fists_of_fury_visual_filter_SpellScript::RemoveInvalidTargets, EFFECT_1, TARGET_UNIT_CONE_ENEMY_24);
        }
    };

    SpellScript* GetSpellScript() const override
    {
        return new spell_monk_fists_of_fury_visual_filter_SpellScript();
    }
};

class spell_monk_fists_of_fury_visual : public SpellScriptLoader
{
public:
    spell_monk_fists_of_fury_visual() : SpellScriptLoader("spell_monk_fists_of_fury_visual") { }

    class spell_monk_fists_of_fury_visual_AuraScript : public AuraScript
    {
        PrepareAuraScript(spell_monk_fists_of_fury_visual_AuraScript);

        void OnApply(const AuraEffect* /*aurEff*/, AuraEffectHandleModes /*mode*/)
        {
            SetMaxDuration(1000); //The spell doesn't have a duration on WoWHead and never ends if we don't give it one, so one sec should be good
            SetDuration(1000);    //Same as above
        }

        void Register() override
        {
            OnEffectApply += AuraEffectApplyFn(spell_monk_fists_of_fury_visual_AuraScript::OnApply, EFFECT_0, SPELL_AURA_DUMMY, AURA_EFFECT_HANDLE_REAL);
        }
    };

    AuraScript* GetAuraScript() const override
    {
        return new spell_monk_fists_of_fury_visual_AuraScript();
    }
};

class spell_monk_fists_of_fury : public SpellScriptLoader
{
public:
    spell_monk_fists_of_fury() : SpellScriptLoader("spell_monk_fists_of_fury") { }

    class spell_monk_fists_of_fury_AuraScript : public AuraScript
    {
        PrepareAuraScript(spell_monk_fists_of_fury_AuraScript);

        void HandlePeriodic(AuraEffect const* aurEff)
        {
            Unit* caster = GetCaster();
            if (!caster)
                return;

            if (aurEff->GetTickNumber() % 6 == 0)
                caster->CastSpell(GetTarget(), SPELL_MONK_FISTS_OF_FURY_DAMAGE, true);
        }

        void Register() override
        {
            OnEffectPeriodic += AuraEffectPeriodicFn(spell_monk_fists_of_fury_AuraScript::HandlePeriodic, EFFECT_2, SPELL_AURA_PERIODIC_DUMMY);
        }
    };

    AuraScript* GetAuraScript() const override
    {
        return new spell_monk_fists_of_fury_AuraScript();
    }
};

class spell_monk_fists_of_fury_damage : public SpellScriptLoader
{
public:
    spell_monk_fists_of_fury_damage() : SpellScriptLoader("spell_monk_fists_of_fury_damage") { }

    class spell_monk_fists_of_fury_damage_SpellScript : public SpellScript
    {
        PrepareSpellScript(spell_monk_fists_of_fury_damage_SpellScript);

        void HandleDamage(SpellEffIndex /*effIndex*/)
        {
            if (!GetCaster())
                return;

            Unit* l_Target = GetHitUnit();
            Player* l_Player = GetCaster()->ToPlayer();

            if (l_Target == nullptr || l_Player == nullptr)
                return;

            int32 l_Damage = l_Player->GetTotalAttackPowerValue(BASE_ATTACK) * 5.25f;
            l_Damage = l_Player->SpellDamageBonusDone(l_Target, GetSpellInfo(), l_Damage, SPELL_DIRECT_DAMAGE, GetSpellInfo()->GetEffect(EFFECT_0));
            l_Damage = l_Target->SpellDamageBonusTaken(l_Player, GetSpellInfo(), l_Damage, SPELL_DIRECT_DAMAGE);

            SetHitDamage(l_Damage);
        }

        void Register() override
        {
            OnEffectHitTarget += SpellEffectFn(spell_monk_fists_of_fury_damage_SpellScript::HandleDamage, EFFECT_0, SPELL_EFFECT_SCHOOL_DAMAGE);
        }
    };

    SpellScript* GetSpellScript() const override
    {
        return new spell_monk_fists_of_fury_damage_SpellScript();
    }
};

// 63508
struct npc_monk_xuen : public ScriptedAI
{
    npc_monk_xuen(Creature* creature) : ScriptedAI(creature) { }

    void IsSummonedBy(WorldObject* /*summoner*/)
    {
        me->CastSpell(me, SPELL_MONK_XUEN_AURA, true);
    }
};

// Whirling Dragon Punch - 152175
class playerScript_monk_whirling_dragon_punch : public PlayerScript
{
public:
    playerScript_monk_whirling_dragon_punch() : PlayerScript("playerScript_monk_whirling_dragon_punch") {}

    void OnCooldownStart(Player* player, SpellInfo const* spellInfo, uint32 /*itemId*/, int32& cooldown, uint32& /*categoryId*/, int32& /*categoryCooldown*/) override
    {
        if (spellInfo->Id == SPELL_MONK_FISTS_OF_FURY)
        {
            SpellInfo const* risingSunKickInfo = sSpellMgr->GetSpellInfo(SPELL_MONK_RISING_SUN_KICK, DIFFICULTY_NONE);
            ApplyCasterAura(player, cooldown, player->GetSpellHistory()->GetChargeRecoveryTime(risingSunKickInfo->ChargeCategoryId));
        }
    }

    void OnChargeRecoveryTimeStart(Player* player, uint32 chargeCategoryId, int32& chargeRecoveryTime) override
    {
        SpellInfo const* risingSunKickInfo = sSpellMgr->GetSpellInfo(SPELL_MONK_RISING_SUN_KICK, DIFFICULTY_NONE);
        if (risingSunKickInfo->ChargeCategoryId == chargeCategoryId)
        {
            SpellInfo const* fistsOfFuryInfo = sSpellMgr->GetSpellInfo(SPELL_MONK_RISING_SUN_KICK, DIFFICULTY_NONE);
            ApplyCasterAura(player, chargeRecoveryTime, player->GetSpellHistory()->GetRemainingCooldown(fistsOfFuryInfo) == 0s);
        }
    }

private:
    void ApplyCasterAura(Player* player, int32 cooldown1, int32 cooldown2)
    {
        if (cooldown1 > 0 && cooldown2 > 0)
        {
            uint32 whirlingDragonPunchAuraDuration = std::min(cooldown1, cooldown2);
            player->CastSpell(player, SPELL_MONK_WHIRLING_DRAGON_PUNCH_CASTER_AURA, true);

            if (Aura* aura = player->GetAura(SPELL_MONK_WHIRLING_DRAGON_PUNCH_CASTER_AURA))
                aura->SetDuration(whirlingDragonPunchAuraDuration);
        }
    }
};

// Whirling Dragon Punch - 152175
class spell_monk_whirling_dragon_punch : public AuraScript
{
    PrepareAuraScript(spell_monk_whirling_dragon_punch);

    void OnTick(AuraEffect const* /*aurEff*/)
    {
        if (GetCaster())
            GetCaster()->CastSpell(GetCaster(), SPELL_MONK_WHIRLING_DRAGON_PUNCH_DAMAGE, true);
    }

    void Register() override
    {
        OnEffectPeriodic += AuraEffectPeriodicFn(spell_monk_whirling_dragon_punch::OnTick, EFFECT_0, SPELL_AURA_PERIODIC_DUMMY);
    }
};

// Flying Serpent Kick - 115057
class spell_monk_flying_serpent_kick : public SpellScriptLoader
{
public:
    spell_monk_flying_serpent_kick() : SpellScriptLoader("spell_monk_flying_serpent_kick") { }

    class spell_monk_flying_serpent_kick_SpellScript : public SpellScript
    {
        PrepareSpellScript(spell_monk_flying_serpent_kick_SpellScript);

        bool Validate(SpellInfo const* /*spellInfo*/) override
        {
            if (!sSpellMgr->GetSpellInfo(SPELL_MONK_FLYING_SERPENT_KICK_NEW, DIFFICULTY_NONE))
                return false;
            return true;
        }

        void HandleOnCast()
        {
            if (Unit* caster = GetCaster())
            {
                if (Player* _player = caster->ToPlayer())
                {
                    if (_player->HasAura(SPELL_MONK_FLYING_SERPENT_KICK))
                        _player->RemoveAura(SPELL_MONK_FLYING_SERPENT_KICK);

                    if (caster->HasAura(SPELL_MONK_ITEM_PVP_GLOVES_BONUS))
                        caster->RemoveAurasByType(SPELL_AURA_MOD_DECREASE_SPEED);

                    _player->CastSpell(_player, SPELL_MONK_FLYING_SERPENT_KICK_AOE, true);
                }
            }
        }

        void Register() override
        {
            OnCast += SpellCastFn(spell_monk_flying_serpent_kick_SpellScript::HandleOnCast);
        }
    };

    SpellScript* GetSpellScript() const override
    {
        return new spell_monk_flying_serpent_kick_SpellScript();
    }
};

// 227291 Niuzao's


void AddSC_monk_spell_scripts()
{
    RegisterSpellScript(spell_monk_crackling_jade_lightning);
    RegisterSpellScript(spell_monk_crackling_jade_lightning_knockback_proc_aura);
    RegisterSpellScript(spell_monk_life_cocoon);
    RegisterSpellScript(spell_monk_open_palm_strikes);
    RegisterSpellScript(spell_monk_power_strike_periodic);
    RegisterSpellScript(spell_monk_power_strike_proc);
    RegisterSpellScript(spell_monk_provoke);
    RegisterSpellScript(spell_monk_roll);
    RegisterSpellScript(spell_monk_roll_aura);
    RegisterSpellScript(spell_monk_stagger);
    RegisterSpellScript(spell_monk_stagger_damage_aura);
    RegisterSpellScript(spell_monk_stagger_debuff_aura);

    //new
    new spell_monk_zen_pilgrimage();
    new spell_monk_zen_pilgrimage_return();
    RegisterSpellScript(spell_monk_tiger_palm);
    new spell_monk_zen_flight_check();
    RegisterSpellAndAuraScriptPair(spell_monk_disable, aura_monk_disable);
    new spell_monk_spear_hand_strike();
    RegisterSpellScript(spell_monk_rising_sun_kick);
    new spell_monk_fortifying_brew();
    new spell_monk_purifying_brew();
    new spell_monk_breath_of_fire();
    new spell_monk_dampen_harm();
    RegisterSpellScript(spell_monk_clash); //Dont stun  NEED FIX
    new spell_monk_healing_elixirs_aura();
    RegisterAreaTriggerAI(at_monk_ring_of_peace);
    RegisterAreaTriggerAI(at_monk_song_of_chi_ji);
    new spell_monk_chi_wave();
    new spell_monk_chi_wave_damage_missile();
    new spell_monk_chi_wave_heal_missile();
    new spell_monk_chi_wave_healing_bolt();
    RegisterSpellScript(spell_monk_chi_wave_target_selector);
    RegisterAreaTriggerAI(at_monk_chi_burst);
    new spell_monk_chi_burst_heal();
    new spell_monk_black_ox_brew(); // ??  NEED FIX
    new spell_monk_jade_serpent_statue();
    RegisterCreatureAI(npc_monk_jade_serpent_statue);
    RegisterSpellScript(spell_monk_transcendence);
    RegisterSpellScript(aura_monk_transcendence);
    RegisterSpellScript(spell_monk_transcendence_transfer);
    new spell_monk_vivify();
    new spell_monk_soothing_mist();
    new spell_focused_thunder_talent_thunder_focus_tea();
    new spell_monk_zen_pulse();
    RegisterSpellScript(spell_monk_renewing_mist);
    RegisterSpellScript(spell_monk_renewing_mist_hot);
    RegisterSpellScript(spell_monk_renewing_mist_jump);
    RegisterSpellScript(spell_monk_essence_font);
    new spell_monk_essence_font_heal();
    RegisterSpellAndAuraScriptPair(spell_monk_mana_tea, aura_monk_mana_tea); // Not del stack  NEED FIX
    new spell_monk_enveloping_mist();
    RegisterSpellScript(spell_monk_storm_earth_and_fire); //npc not work and not despawn    NEED FIX
    RegisterCreatureAI(npc_monk_sef_spirit); //npc not work and not despawn                 NEED FIX
    new playerScript_monk_earth_fire_storm; //npc not work and not despawn                  NEED FIX
    new spell_monk_touch_of_karma();
    RegisterSpellScript(spell_monk_touch_of_karma_buff);
    new spell_monk_fists_of_fury();
    new spell_monk_fists_of_fury_damage();
    new spell_monk_fists_of_fury_stun();
    new spell_monk_fists_of_fury_visual();
    new spell_monk_fists_of_fury_visual_filter();
    RegisterCreatureAI(npc_monk_xuen);
    new playerScript_monk_whirling_dragon_punch(); //need testing    NEED FIX
    RegisterSpellScript(spell_monk_whirling_dragon_punch); //not check NEED FIX
    new spell_monk_flying_serpent_kick();

}
