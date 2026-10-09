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
#include "Player.h"
#include "SpellInfo.h"
#include "SpellScript.h"
#include "WorldSession.h"

// 1225512 - Open Neighborhood Charter (use of item 239098)
class spell_housing_neighborhood_charter : public SpellScript
{
    void HandleDummy(SpellEffIndex /*effIndex*/) const
    {
        if (Player* player = GetCaster()->ToPlayer())
            player->GetSession()->SendNeighborhoodCharterOpenUI();
    }

    void Register() override
    {
        OnEffectHit += SpellEffectFn(spell_housing_neighborhood_charter::HandleDummy, EFFECT_0, SPELL_EFFECT_DUMMY);
    }
};

void AddSC_housing_spell_scripts()
{
    RegisterSpellScript(spell_housing_neighborhood_charter);
}
