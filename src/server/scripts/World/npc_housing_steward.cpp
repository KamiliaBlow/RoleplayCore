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
#include "CreatureAI.h"
#include "GossipDef.h"
#include "Log.h"
#include "Player.h"
#include "ScriptedGossip.h"

enum HousingTutorialData
{
    // Quest: "My First Home" (91863) kill credit NPCs
    NPC_KILL_CREDIT_GREET_STEWARD   = 249851,
    NPC_KILL_CREDIT_ASK_STEWARD     = 248857,

    // Gossip actions
    GOSSIP_ACTION_ASK_TO_JOIN       = 1001,
};

// Lyssabel Dawnpetal (233063) / Tocho (233708) — Housing tutorial steward NPCs.
// When the player interacts with the steward during the "My First Home" quest (91863),
// the gossip grants quest kill credits for greeting the steward and asking them to join.
struct npc_housing_steward : public CreatureAI
{
    npc_housing_steward(Creature* creature) : CreatureAI(creature) { }

    void UpdateAI(uint32 /*diff*/) override { }

    bool OnGossipHello(Player* player) override
    {
        // Grant "Greet the steward" kill credit (quest objective 0)
        player->KilledMonsterCredit(NPC_KILL_CREDIT_GREET_STEWARD);

        // Show gossip menu with option to ask the steward to join the neighborhood
        InitGossipMenuFor(player, 0);
        if (me->IsQuestGiver())
            player->PrepareQuestMenu(me->GetGUID());
        AddGossipItemFor(player, GossipOptionNpc::None,
            "Ask the steward to become your neighbor.",
            GOSSIP_SENDER_MAIN, GOSSIP_ACTION_ASK_TO_JOIN);
        SendGossipMenuFor(player, DEFAULT_GOSSIP_MESSAGE, me->GetGUID());

        TC_LOG_DEBUG("housing", "npc_housing_steward: Player {} greeted steward {} (kill credit {})",
            player->GetGUID().ToString(), me->GetEntry(), NPC_KILL_CREDIT_GREET_STEWARD);

        return true;
    }

    bool OnGossipSelect(Player* player, uint32 /*menuId*/, uint32 gossipListId) override
    {
        uint32 action = GetGossipActionFor(player, gossipListId);
        CloseGossipMenuFor(player);

        if (action == GOSSIP_ACTION_ASK_TO_JOIN)
        {
            // Grant "Ask the steward to join you" kill credit (quest objective 3)
            player->KilledMonsterCredit(NPC_KILL_CREDIT_ASK_STEWARD);

            TC_LOG_DEBUG("housing", "npc_housing_steward: Player {} asked steward {} to join (kill credit {})",
                player->GetGUID().ToString(), me->GetEntry(), NPC_KILL_CREDIT_ASK_STEWARD);
        }

        return true;
    }
};

void AddSC_npc_housing_steward()
{
    RegisterCreatureAI(npc_housing_steward);
}
