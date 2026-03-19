/*
 * Advanced Flying / Skyriding spell scripts
 * Based on hwis/DragonCore + TrinityCore PR #30199 (Traesh)
 *
 * Key insight: Active abilities (374763, 372608, 361584) require
 * CasterAuraSpell = 372773 (Vigor) in DB2 SpellAuraRestrictions.
 * Without Vigor aura, client AND server block the cast.
 */

#include "ScriptMgr.h"
#include "Player.h"
#include "SpellScript.h"
#include "SpellAuraEffects.h"

enum AdvancedFlyingSpells
{
    SPELL_STEADY_FLIGHT         = 404468,
    SPELL_SKYRIDING             = 404464,
    SPELL_SKYRIDING_BASICS      = 376777,
    SPELL_SWITCH_FLIGHT_STYLE   = 436854,
    SPELL_VIGOR                 = 372773,
    SPELL_SURGE_FORWARD         = 372608,
    SPELL_WHIRLING_SURGE        = 361584,
};

// Spell 436854 - Switch Flight Style
class spell_switch_flight : public SpellScript
{
    void HandleDummy(SpellEffIndex /*effIndex*/)
    {
        Unit* caster = GetCaster();
        if (!caster)
            return;

        bool hadSkyriding = caster->HasAura(SPELL_SKYRIDING);

        caster->RemoveAura(SPELL_SKYRIDING);
        caster->RemoveAura(SPELL_STEADY_FLIGHT);

        if (hadSkyriding)
        {
            caster->CastSpell(caster, SPELL_STEADY_FLIGHT, true);
            caster->RemoveAura(SPELL_VIGOR);
            if (caster->IsMounted())
            {
                caster->SetCanAdvFly(false);
                caster->SetCanDoubleJump(false);
            }
        }
        else
        {
            caster->CastSpell(caster, SPELL_SKYRIDING, true);
            if (!caster->HasAura(SPELL_VIGOR))
                caster->CastSpell(caster, SPELL_VIGOR, true);
            if (caster->IsMounted())
            {
                caster->SetCanAdvFly(true);
                caster->SetCanDoubleJump(true);
                caster->SetFlightCapabilityID(1, true);
            }
        }
    }

    void Register() override
    {
        OnEffectHitTarget += SpellEffectFn(spell_switch_flight::HandleDummy, EFFECT_0, SPELL_EFFECT_DUMMY);
    }
};

// Skyward Ascent (Abheben) - Spells 374763, 372610, 386451
class spell_af_skyward_ascent : public SpellScript
{
    void HandleDummy(SpellEffIndex /*effIndex*/)
    {
        Player* caster = GetCaster()->ToPlayer();
        if (!caster)
            return;

        float ascentSpeed = static_cast<float>(GetSpellInfo()->GetEffect(EFFECT_0).BasePoints);
        if (ascentSpeed <= 0.0f)
            ascentSpeed = 60.0f;

        caster->AddMoveImpulse(Position(0.0f, 0.0f, ascentSpeed / 10.0f));
    }

    void Register() override
    {
        OnEffectHitTarget += SpellEffectFn(spell_af_skyward_ascent::HandleDummy, EFFECT_0, SPELL_EFFECT_DUMMY);
    }
};

// Surge Forward - Spell 372608
class spell_af_surge_forward : public SpellScript
{
    void HandleDummy(SpellEffIndex /*effIndex*/)
    {
        Player* caster = GetCaster()->ToPlayer();
        if (!caster)
            return;

        float speed = 14.0f;
        float orientation = caster->GetOrientation();
        float pitch = caster->m_movementInfo.pitch;

        float deltaX = speed * std::cos(orientation) * std::cos(pitch);
        float deltaY = speed * std::sin(orientation) * std::cos(pitch);
        float deltaZ = speed * std::sin(pitch);

        caster->AddMoveImpulse(Position(deltaX, deltaY, deltaZ));
    }

    void Register() override
    {
        OnEffectHitTarget += SpellEffectFn(spell_af_surge_forward::HandleDummy, EFFECT_0, SPELL_EFFECT_DUMMY);
    }
};

// Whirling Surge - Spell 361584
class spell_af_whirling_surge : public SpellScript
{
    void HandleDummy()
    {
        Player* caster = GetCaster()->ToPlayer();
        if (!caster)
            return;

        float speed = 60.0f;
        float orientation = caster->GetOrientation();
        float pitch = caster->m_movementInfo.pitch;

        float deltaX = speed * std::cos(orientation) * std::cos(pitch);
        float deltaY = speed * std::sin(orientation) * std::cos(pitch);
        float deltaZ = speed * std::sin(pitch);

        caster->AddMoveImpulse(Position(deltaX, deltaY, deltaZ));
    }

    void Register() override
    {
        AfterCast += SpellCastFn(spell_af_whirling_surge::HandleDummy);
    }
};

// PlayerScript: Auto-teach Skyriding abilities + apply Vigor
class adv_flying_check : public PlayerScript
{
public:
    adv_flying_check() : PlayerScript("adv_flying_check") {}

    void CheckFlying(Player* player) const
    {
        if (!player || !player->IsInWorld())
            return;

        uint8 level = player->GetLevel();

        if (level >= 20)
        {
            auto learn = [&](uint32 spellId) {
                if (!player->HasSpell(spellId))
                    player->LearnSpell(spellId, false);
            };

            learn(SPELL_SKYRIDING_BASICS);
            learn(SPELL_SURGE_FORWARD);
            learn(SPELL_WHIRLING_SURGE);
            learn(SPELL_SWITCH_FLIGHT_STYLE);

            bool hasSkyriding = player->HasAura(SPELL_SKYRIDING);
            bool hasSteady = player->HasAura(SPELL_STEADY_FLIGHT);

            if (hasSkyriding && hasSteady)
            {
                player->RemoveAura(SPELL_STEADY_FLIGHT);
            }
            else if (!hasSkyriding && !hasSteady)
            {
                player->CastSpell(player, SPELL_SKYRIDING, true);
            }

            // Vigor aura: CRITICAL for active abilities (CasterAuraSpell=372773 in DB2)
            if (player->HasAura(SPELL_SKYRIDING) && !player->HasAura(SPELL_VIGOR))
                player->CastSpell(player, SPELL_VIGOR, true);
            if (player->HasAura(SPELL_STEADY_FLIGHT) && player->HasAura(SPELL_VIGOR))
                player->RemoveAura(SPELL_VIGOR);
        }
    }

    void OnLogin(Player* player, bool /*firstLogin*/) override
    {
        CheckFlying(player);
    }

    void OnLevelChanged(Player* player, uint8 /*oldLevel*/) override
    {
        CheckFlying(player);
    }
};

void AddSC_advanced_flying_spell_scripts()
{
    RegisterSpellScript(spell_switch_flight);
    RegisterSpellScript(spell_af_skyward_ascent);
    RegisterSpellScript(spell_af_surge_forward);
    RegisterSpellScript(spell_af_whirling_surge);
    new adv_flying_check();
}
