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
#include "AreaTrigger.h"
#include "AreaTriggerAI.h"
#include "Log.h"
#include "Player.h"
#include "WorldSession.h"
#include "HousingPackets.h"

enum HousingPlotSpells
{
    SPELL_VISITING_PLOT_AURA    = 1239847,
    SPELL_OWN_PLOT_AURA         = 1266699
};

struct at_housing_plot : AreaTriggerAI
{
    using AreaTriggerAI::AreaTriggerAI;

    void OnUnitEnter(Unit* unit) override
    {
        Player* player = unit->ToPlayer();
        if (!player)
            return;

        ObjectGuid ownerGuid;
        if (at->m_housingPlotAreaTriggerData.has_value())
            ownerGuid = at->m_housingPlotAreaTriggerData->HouseOwnerGUID;

        bool isOwnPlot = !ownerGuid.IsEmpty() && player->GetGUID() == ownerGuid;

        if (isOwnPlot)
            player->CastSpell(player, SPELL_OWN_PLOT_AURA, true);
        else
            player->CastSpell(player, SPELL_VISITING_PLOT_AURA, true);

        // Notify the client that the player has entered a plot
        WorldPackets::Neighborhood::NeighborhoodPlayerEnterPlot enterPlot;
        enterPlot.PlotAreaTriggerGuid = at->GetGUID();
        player->SendDirectMessage(enterPlot.Write());

        TC_LOG_DEBUG("housing", "at_housing_plot: Player {} entered plot AT {} (own: {})",
            player->GetGUID().ToString(), at->GetGUID().ToString(), isOwnPlot);
    }

    void OnUnitExit(Unit* unit, AreaTriggerExitReason reason) override
    {
        if (reason != AreaTriggerExitReason::NotInside)
            return;

        Player* player = unit->ToPlayer();
        if (!player)
            return;

        // Remove both auras (only the active one will actually be removed)
        player->RemoveAurasDueToSpell(SPELL_OWN_PLOT_AURA);
        player->RemoveAurasDueToSpell(SPELL_VISITING_PLOT_AURA);

        // Notify the client that the player has left the plot
        WorldPackets::Neighborhood::NeighborhoodPlayerLeavePlot leavePlot;
        player->SendDirectMessage(leavePlot.Write());

        TC_LOG_DEBUG("housing", "at_housing_plot: Player {} left plot AT {}",
            player->GetGUID().ToString(), at->GetGUID().ToString());
    }
};

void AddSC_at_housing_plot()
{
    RegisterAreaTriggerAI(at_housing_plot);
}
