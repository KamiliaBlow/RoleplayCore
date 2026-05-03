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
#include "GameObject.h"
#include "Log.h"
#include "Player.h"
#include "SpellScript.h"

enum HousingCornerstoneSpells
{
    SPELL_TRIGGER_CONVO_UNOWNED_PLOT = 1266097,
    SPELL_HOUSING_DOOR_OPEN = 1271876   // 12.0.5 retail housing-front-door open spell
};

// 1266097 - [DNT] Trigger Convo for Unowned Plot
// Cast by Cornerstone GO (entry 457142, type UILink) when a player clicks it.
// The SMSG_NPC_INTERACTION_OPEN_RESULT with CornerstoneInteraction (type 70) is
// already sent by the UILink Use() handler before this spell fires.
// This dummy effect provides server-side validation and logging.
class spell_housing_trigger_convo_unowned_plot : public SpellScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return true;
    }

    void HandleDummy(SpellEffIndex /*effIndex*/) const
    {
        Player* caster = GetCaster()->ToPlayer();
        if (!caster)
            return;

        TC_LOG_DEBUG("housing", "spell_housing_trigger_convo_unowned_plot: Spell {} fired for player {} ({})",
            GetSpellInfo()->Id, caster->GetName(), caster->GetGUID().ToString());
    }

    void Register() override
    {
        OnEffectHit += SpellEffectFn(spell_housing_trigger_convo_unowned_plot::HandleDummy, EFFECT_0, SPELL_EFFECT_DUMMY);
    }
};

// 1271876 - Housing Door Open (12.0.5 retail)
// Cast by client when a player clicks a housing front door GO. Sniff-decoded from
// dump_12.0.5.67186_2026-04-28_*.pkt: bytes 0x15-0x18 of CMSG_CAST_SPELL body =
// 0x00136644 = 1271876, with the door GO as the spell target. Both faction sniffs
// agree on the spell ID (only the target PackedGUID differs).
//
// The spell row in spell_misc / spell_effect we ship for it (sql/updates/hotfixes/
// master/2026_04_29_00_hotfixes.sql) is a minimal SPELL_EFFECT_DUMMY targeting
// TARGET_GAMEOBJECT_TARGET so server-side validation accepts the cast and the
// dummy hook fires. We intentionally do NOT use SPELL_EFFECT_OPEN_LOCK or
// SPELL_EFFECT_USE_GAMEOBJECT ? those would double-trigger Use() through the spell
// effect machinery in addition to the explicit Use() this script issues. Routing
// through one well-defined path keeps the door teleport semantics owned by the
// existing go_housing_door::OnGossipHello (which already handles edit-mode gating,
// visitor permissions, and the interior?exterior round trip).
class spell_housing_door_open : public SpellScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return true;
    }

    void HandleDummy(SpellEffIndex /*effIndex*/) const
    {
        Player* caster = GetCaster()->ToPlayer();
        if (!caster)
            return;

        WorldObject* target = GetExplTargetWorldObject();
        if (!target)
            target = GetHitGObj();
        if (!target)
        {
            TC_LOG_DEBUG("housing", "spell_housing_door_open: Spell {} cast by {} but no GO target ? ignored",
                GetSpellInfo()->Id, caster->GetGUID().ToString());
            return;
        }

        GameObject* doorGo = target->ToGameObject();
        if (!doorGo)
        {
            TC_LOG_DEBUG("housing", "spell_housing_door_open: Spell {} cast by {} on non-GO target {} ? ignored",
                GetSpellInfo()->Id, caster->GetGUID().ToString(), target->GetGUID().ToString());
            return;
        }

        TC_LOG_DEBUG("housing", "spell_housing_door_open: Player {} cast {} on door GO {} (entry={}) ? invoking Use()",
            caster->GetGUID().ToString(), GetSpellInfo()->Id, doorGo->GetGUID().ToString(), doorGo->GetEntry());

        // Route to the standard interaction path so go_housing_door::OnGossipHello
        // performs the teleport. Use() handles edit-mode gating + animation; the
        // gossip script then resolves the destination plot/interior.
        doorGo->Use(caster);
    }

    void Register() override
    {
        OnEffectHit += SpellEffectFn(spell_housing_door_open::HandleDummy, EFFECT_0, SPELL_EFFECT_DUMMY);
    }
};

void AddSC_housing_spell_scripts()
{
    RegisterSpellScript(spell_housing_trigger_convo_unowned_plot);
    RegisterSpellScript(spell_housing_door_open);
}
