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
#include "Housing.h"
#include "HousingDefines.h"
#include "HousingMap.h"
#include "HousingMgr.h"
#include "HousingPackets.h"
#include "Log.h"
#include "ObjectAccessor.h"
#include "PhasingHandler.h"
#include "Player.h"
#include "SpellMgr.h"
#include "WorldSession.h"

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
        uint32 plotId = 0;
        if (at->m_housingPlotAreaTriggerData.has_value())
        {
            ownerGuid = at->m_housingPlotAreaTriggerData->HouseOwnerGUID;
            plotId = at->m_housingPlotAreaTriggerData->PlotID;
        }

        bool isOwnPlot = !ownerGuid.IsEmpty() && player->GetGUID() == ownerGuid;

        // Check visitor access permissions for exterior (plot) access
        if (!isOwnPlot && !ownerGuid.IsEmpty())
        {
            if (Player* owner = ObjectAccessor::FindPlayer(ownerGuid))
            {
                if (Housing const* ownerHousing = owner->GetHousing())
                {
                    if (!sHousingMgr.CanVisitorAccess(player, owner, ownerHousing->GetSettingsFlags(), false))
                    {
                        TC_LOG_DEBUG("housing", "at_housing_plot: Player {} denied plot access (owner {} flags 0x{:X})",
                            player->GetGUID().ToString(), ownerGuid.ToString(), ownerHousing->GetSettingsFlags());
                        return;
                    }
                }
            }
        }

        // Cast plot auras if the spells exist in DB2
        uint32 auraSpell = isOwnPlot ? SPELL_OWN_PLOT_AURA : SPELL_VISITING_PLOT_AURA;
        if (sSpellMgr->GetSpellInfo(auraSpell, DIFFICULTY_NONE))
            player->CastSpell(player, auraSpell, true);

        // Track which plot the player is on (for HouseStatus queries)
        if (HousingMap* housingMap = dynamic_cast<HousingMap*>(player->GetMap()))
            housingMap->SetPlayerCurrentPlot(player->GetGUID(), static_cast<uint8>(plotId));

        // Notify the client that the player has entered a plot
        WorldPackets::Neighborhood::NeighborhoodPlayerEnterPlot enterPlot;
        enterPlot.PlotAreaTriggerGuid = at->GetGUID();
        player->SendDirectMessage(enterPlot.Write());

        // Cosmetic phase shift: when entering own plot, remove 16 cosmetic phases
        // after a ~10 second delay (sniff-verified retail behavior).
        // Only applies to the plot owner, not visitors.
        if (isOwnPlot)
        {
            ObjectGuid playerGuid = player->GetGUID();
            player->m_Events.AddEventAtOffset([playerGuid]()
            {
                Player* p = ObjectAccessor::FindPlayer(playerGuid);
                if (!p || !p->IsInWorld())
                    return;

                for (uint32 i = 0; i < HOUSING_COSMETIC_PHASE_COUNT; ++i)
                    PhasingHandler::RemovePhase(p, HOUSING_COSMETIC_PHASES[i], false);

                PhasingHandler::SendToPlayer(p);

                TC_LOG_DEBUG("housing", "at_housing_plot: Removed {} cosmetic phases for plot owner {}",
                    HOUSING_COSMETIC_PHASE_COUNT, playerGuid.ToString());
            }, Milliseconds(HOUSING_COSMETIC_PHASE_DELAY_MS));
        }

        TC_LOG_DEBUG("housing", "at_housing_plot: Player {} entered plot {} AT {} (own: {}, owner: {})",
            player->GetGUID().ToString(), plotId, at->GetGUID().ToString(), isOwnPlot,
            ownerGuid.IsEmpty() ? "none" : ownerGuid.ToString());
    }

    void OnUnitExit(Unit* unit, AreaTriggerExitReason reason) override
    {
        if (reason != AreaTriggerExitReason::NotInside)
            return;

        Player* player = unit->ToPlayer();
        if (!player)
            return;

        ObjectGuid ownerGuid;
        if (at->m_housingPlotAreaTriggerData.has_value())
            ownerGuid = at->m_housingPlotAreaTriggerData->HouseOwnerGUID;

        bool isOwnPlot = !ownerGuid.IsEmpty() && player->GetGUID() == ownerGuid;

        // Remove plot auras if they exist
        if (sSpellMgr->GetSpellInfo(SPELL_OWN_PLOT_AURA, DIFFICULTY_NONE))
            player->RemoveAurasDueToSpell(SPELL_OWN_PLOT_AURA);
        if (sSpellMgr->GetSpellInfo(SPELL_VISITING_PLOT_AURA, DIFFICULTY_NONE))
            player->RemoveAurasDueToSpell(SPELL_VISITING_PLOT_AURA);

        // Clear plot tracking
        if (HousingMap* housingMap = dynamic_cast<HousingMap*>(player->GetMap()))
            housingMap->ClearPlayerCurrentPlot(player->GetGUID());

        // Notify the client that the player has left the plot
        WorldPackets::Neighborhood::NeighborhoodPlayerLeavePlot leavePlot;
        player->SendDirectMessage(leavePlot.Write());

        // Cosmetic phase shift: when leaving own plot, restore 16 cosmetic phases
        // after a ~10 second delay (sniff-verified retail behavior).
        if (isOwnPlot)
        {
            ObjectGuid playerGuid = player->GetGUID();
            player->m_Events.AddEventAtOffset([playerGuid]()
            {
                Player* p = ObjectAccessor::FindPlayer(playerGuid);
                if (!p || !p->IsInWorld())
                    return;

                for (uint32 i = 0; i < HOUSING_COSMETIC_PHASE_COUNT; ++i)
                    PhasingHandler::AddPhase(p, HOUSING_COSMETIC_PHASES[i], false);

                PhasingHandler::SendToPlayer(p);

                TC_LOG_DEBUG("housing", "at_housing_plot: Restored {} cosmetic phases for plot owner {}",
                    HOUSING_COSMETIC_PHASE_COUNT, playerGuid.ToString());
            }, Milliseconds(HOUSING_COSMETIC_PHASE_DELAY_MS));
        }

        TC_LOG_DEBUG("housing", "at_housing_plot: Player {} left plot AT {} (own: {})",
            player->GetGUID().ToString(), at->GetGUID().ToString(), isOwnPlot);
    }
};

void AddSC_at_housing_plot()
{
    RegisterAreaTriggerAI(at_housing_plot);
}
