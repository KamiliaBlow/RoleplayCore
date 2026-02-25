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
 * Scripts for spells with SPELLFAMILY_GENERIC spells used by dragonrider and advanced fly spells.
 * Scriptnames of files in this file should be prefixed with "spell_dr_".
 */

#include "Containers.h"
#include "DB2Stores.h"
#include "Player.h"
#include "ScriptMgr.h"
#include "Spell.h"
#include "SpellAuraEffects.h"
#include "SpellHistory.h"
#include "SpellMgr.h"
#include "SpellScript.h"
#include <MovementPackets.h>
#include <G3D/g3dmath.h>

enum AdvancedFlyingSpells 
{
    SPELL_DRAGONRIDER_ENERGIZE      = 372606,
    SPELL_VIGOR_CACHE               = 433547,
    SPELL_RIDING_ABROAD             = 432503, // TODO outside of dragon isles
    SPELL_ENERGY_WIDGET             = 423624,

    SWITCH_AF_REGULAR               = 404468,
    SWITCH_AF_DRAGONRIDING          = 404464

};

// 373646 - Soar (Racial)
// 406095 - Skyriding
// 430833 - Soar (Racial)
class spell_af_skyriding : public AuraScript
{
    void OnApply(AuraEffect const* /*aurEff*/, AuraEffectHandleModes /*mode*/)
    {
        if (GetCaster()->HasAura(SWITCH_AF_DRAGONRIDING))
        {
            GetTarget()->CastSpell(GetTarget(), SPELL_ENERGY_WIDGET, TRIGGERED_FULL_MASK);
            GetTarget()->SetPower(POWER_ALTERNATE_MOUNT, GetTarget()->GetPower(POWER_ALTERNATE_MOUNT), true);
        }
    }

    void OnRemove(AuraEffect const* /*aurEff*/, AuraEffectHandleModes /*mode*/)
    {
        GetTarget()->RemoveAurasDueToSpell(SPELL_ENERGY_WIDGET);
    }

    void Register() override
    {
        // Midnight 12.0.1: EFFECT_2 does not exist in DBC (430747); only EFFECT_0 exists; disabled handlers
#if 0
        OnEffectApply += AuraEffectApplyFn(spell_af_skyriding::OnApply, EFFECT_2, SPELL_AURA_ANY, AURA_EFFECT_HANDLE_REAL);
        OnEffectRemove += AuraEffectRemoveFn(spell_af_skyriding::OnRemove, EFFECT_2, SPELL_AURA_ANY, AURA_EFFECT_HANDLE_REAL);
#endif
    }
};

// 372773 - Dragonrider Energy
class spell_af_energy : public AuraScript
{
    void OnApply(AuraEffect const* /*aurEff*/, AuraEffectHandleModes /*mode*/)
    {
        Unit* target = GetTarget();
        if (!target->HasAura(SPELL_VIGOR_CACHE))
        {
            CastSpellExtraArgs extraArgs(TRIGGERED_FULL_MASK);
            extraArgs.AddSpellMod(SPELLVALUE_BASE_POINT0, target->GetPower(POWER_ALTERNATE_MOUNT));
            target->CastSpell(target, SPELL_VIGOR_CACHE, extraArgs);
        }
    }

    void OnPeriodic(AuraEffect* /*aurEff*/)
    {
        if (Unit* caster = GetCaster())
        {
            if (ShouldRegenEnergy(caster))
            {
                if (AuraEffect* subAmountAurEff = caster->GetAuraEffect(SPELL_VIGOR_CACHE, EFFECT_1))
                {
                    int32 baseRegen = 20; // Todo : Calculate this based on talents & if we are thrilled/grounded

                    int32 newAmount = subAmountAurEff->GetAmount() + baseRegen;

                    if (newAmount >= 100)
                    {
                        newAmount -= 100;

                        caster->CastSpell(caster, SPELL_DRAGONRIDER_ENERGIZE, TRIGGERED_FULL_MASK);

                        if (AuraEffect* amountAurEff = caster->GetAuraEffect(SPELL_VIGOR_CACHE, EFFECT_0))
                            amountAurEff->SetAmount(caster->GetPower(POWER_ALTERNATE_MOUNT));
                    }

                    subAmountAurEff->SetAmount(newAmount);
                    subAmountAurEff->GetBase()->SetNeedClientUpdateForTargets();
                }

                int newMaxPower = 3;

                if (caster->HasAura(377920) && !caster->HasAura(377921) && !caster->HasAura(377922))
                {
                    newMaxPower = 4;
                }
                else if (caster->HasAura(377921) && caster->HasAura(377920) && !caster->HasAura(377922))
                {
                    newMaxPower = 5;
                }
                else if (caster->HasAura(377922) && caster->HasAura(377921) && caster->HasAura(377920))
                {
                    newMaxPower = 6;
                }

                caster->SetMaxPower(POWER_ALTERNATE_MOUNT, newMaxPower);
            }
        }
    }

    void OnRemove(AuraEffect const* /*aurEff*/, AuraEffectHandleModes /*mode*/)
    {
        GetTarget()->RemoveAurasDueToSpell(SPELL_VIGOR_CACHE);
    }

    void Register() override
    {
        // Midnight 12.0.1: EFFECT_0 aura is DUMMY(4) not ENABLE_ALT_POWER(346) in DBC (372771)
        OnEffectApply += AuraEffectApplyFn(spell_af_energy::OnApply, EFFECT_0, SPELL_AURA_DUMMY, AURA_EFFECT_HANDLE_REAL);
        OnEffectUpdatePeriodic += AuraEffectUpdatePeriodicFn(spell_af_energy::OnPeriodic, EFFECT_1, SPELL_AURA_PERIODIC_DUMMY);
        // Midnight 12.0.1: EFFECT_0 aura is DUMMY(4) not ENABLE_ALT_POWER(346) in DBC (372771)
        OnEffectRemove += AuraEffectRemoveFn(spell_af_energy::OnRemove, EFFECT_0, SPELL_AURA_DUMMY, AURA_EFFECT_HANDLE_REAL);
    }

private:
    bool ShouldRegenEnergy(Unit const* caster) const
    {
        if (caster->GetPower(POWER_ALTERNATE_MOUNT) == caster->GetMaxPower(POWER_ALTERNATE_MOUNT))
            return false;

        FlightCapabilityEntry const* flightCapabilityEntry = sFlightCapabilityStore.LookupEntry(caster->GetFlightCapabilityID());
        if (!flightCapabilityEntry)
            return false;

        float velocityRegenThreshold = flightCapabilityEntry->MaxVel * flightCapabilityEntry->VigorRegenMaxVelCoefficient;
        if (caster->GetAdvFlyingVelocity() >= velocityRegenThreshold)
            return true;

        return !caster->IsInAir() || caster->IsInWater();
    }
};

// 374763 - Lift off
// 372610 - Skyward Ascent (Dragonriding)
// 386451 - Skyward Ascent (Drakhtyr)
class spell_af_skyward_ascent : public SpellScript
{
    void HandleHitTarget(SpellEffIndex effIndex)
    {
        if (Player* caster = GetCaster()->ToPlayer())
        {
            uint32 ascentSpeed = uint32(GetSpellValue()->EffectBasePoints[effIndex]);
            caster->AddMoveImpulse(Position(0, 0, float(ascentSpeed) / 10));
        }
    }

    void Register() override
    {
        // Midnight 12.0.1: EFFECT_0 is APPLY_AURA(6) not DUMMY(3) in DBC (386451); disabled handler
#if 0
        OnEffectHitTarget += SpellEffectFn(spell_af_skyward_ascent::HandleHitTarget, EFFECT_0, SPELL_EFFECT_DUMMY);
#endif
    }
};

// 372608 - Surge Forward
// 386449 - Surge Forward (Drakhtyr)
class spell_af_surge_forward : public SpellScript
{
    void HandleHitTarget(SpellEffIndex /*effIndex*/)
    {
        if (Player* caster = GetCaster()->ToPlayer())
        {
            float SURGE_SPEED = 14.0f;

            float destX = caster->GetPositionX() + SURGE_SPEED * std::cos(caster->GetOrientation());
            float destY = caster->GetPositionY() + SURGE_SPEED * std::sin(caster->GetOrientation());
            float destZ = caster->GetPositionZ() + SURGE_SPEED * std::tan(caster->m_movementInfo.pitch);

            caster->AddMoveImpulse(Position(destX - caster->GetPositionX(), destY - caster->GetPositionY(), destZ - caster->GetPositionZ()));
        }
    }

    void Register() override
    {
        // Midnight 12.0.1: EFFECT_0 is APPLY_AURA(6) not DUMMY(3) in DBC (386449); disabled handler
#if 0
        OnEffectHitTarget += SpellEffectFn(spell_af_surge_forward::HandleHitTarget, EFFECT_0, SPELL_EFFECT_DUMMY);
#endif
    }
};

// 361584 - Whirling Surge
class spell_af_whirling_surge : public SpellScript
{
    void HandleHitTarget(SpellEffIndex /*effIndex*/)
    {
        if (Player* caster = GetCaster()->ToPlayer())
        {
            float SURGE_SPEED = 60.0f;

            float destX = caster->GetPositionX() + SURGE_SPEED * std::cos(caster->GetOrientation());
            float destY = caster->GetPositionY() + SURGE_SPEED * std::sin(caster->GetOrientation());
            float destZ = caster->GetPositionZ() + SURGE_SPEED * std::tan(caster->m_movementInfo.pitch);

            caster->AddMoveImpulse(Position(destX - caster->GetPositionX(), destY - caster->GetPositionY(), destZ - caster->GetPositionZ()));
        }
    }

    void Register() override
    {
        OnEffectHitTarget += SpellEffectFn(spell_af_whirling_surge::HandleHitTarget, EFFECT_0, SPELL_EFFECT_APPLY_AURA);
    }
};

// 436854 - Switch Flight Style
class spell_switch_flight : public SpellScript
{
    void HandleDummy(SpellEffIndex /*effIndex*/)
    {
        Unit* caster = GetCaster()->ToPlayer();
        if (!caster)
            return;

        bool hasRegular = caster->HasAura(SWITCH_AF_REGULAR);
        bool hasDragonRiding = caster->HasAura(SWITCH_AF_DRAGONRIDING);
        bool hadBaseline = caster->HasAura(406095);

        if (!hasRegular && !hasDragonRiding)
        {
            if (hadBaseline) {
                caster->CastSpell(caster, SWITCH_AF_DRAGONRIDING, TRIGGERED_FULL_MASK);
            }
            else {
                caster->CastSpell(caster, SWITCH_AF_REGULAR, TRIGGERED_FULL_MASK);
            }
        }
        else if (hasRegular && !hasDragonRiding)
        {
            caster->RemoveAura(SWITCH_AF_REGULAR);
            caster->CastSpell(caster, SWITCH_AF_DRAGONRIDING, TRIGGERED_FULL_MASK);
        }
        else if (!hasRegular && hasDragonRiding)
        {
            caster->RemoveAura(SWITCH_AF_DRAGONRIDING);
            caster->CastSpell(caster, SWITCH_AF_REGULAR, TRIGGERED_FULL_MASK);
        }
    }

    void Register() override
    {
        OnEffectHitTarget += SpellEffectFn(spell_switch_flight::HandleDummy, EFFECT_0, SPELL_EFFECT_DUMMY);
    }
};


void AddSC_dragonriding_spell_scripts()
{
    RegisterSpellScript(spell_af_skyriding);
    RegisterSpellScript(spell_af_energy);
    RegisterSpellScript(spell_af_skyward_ascent);
    RegisterSpellScript(spell_af_surge_forward);
    RegisterSpellScript(spell_af_whirling_surge);
    RegisterSpellScript(spell_switch_flight);
}
