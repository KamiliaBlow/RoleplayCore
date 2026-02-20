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
    SPELL_TRIGGER_CONVO_UNOWNED_PLOT = 1266097
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

void AddSC_housing_spell_scripts()
{
    RegisterSpellScript(spell_housing_trigger_convo_unowned_plot);
}
