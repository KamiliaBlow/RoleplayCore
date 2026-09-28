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
#include "Creature.h"
#include "CreatureAI.h"
#include "GossipDef.h"
#include "Housing.h"
#include "HousingDefines.h"
#include "HousingMgr.h"
#include "Log.h"
#include "NeighborhoodMgr.h"
#include "ObjectMgr.h"
#include "Player.h"
#include "ScriptedGossip.h"
#include "World.h"

enum HousingTutorialData
{
    // Quest IDs
    QUEST_MY_FIRST_HOME             = 91863,

    // Quest: "My First Home" (91863) kill credit NPCs
    NPC_KILL_CREDIT_GREET_STEWARD   = 249851,
    NPC_KILL_CREDIT_ASK_STEWARD     = 248857,

    // Gossip actions
    GOSSIP_ACTION_ASK_TO_JOIN       = 1001,
    GOSSIP_ACTION_FOUND_NEIGHBORHOOD = 1002,
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
        // Grant "Greet the steward" kill credit (quest objective 0: MONSTER 249851)
        player->KilledMonsterCredit(NPC_KILL_CREDIT_GREET_STEWARD);

        // Satisfy "Talk to Lyssabel/Tocho" objective (quest objective 1/2: TALKTO with NPC entry)
        player->TalkedToCreature(me->GetEntry(), me->GetGUID());

        TC_LOG_DEBUG("housing", "npc_housing_steward: Player {} greeted steward {} (kill credit {}, talkto {})",
            player->GetGUID().ToString(), me->GetEntry(), NPC_KILL_CREDIT_GREET_STEWARD, me->GetEntry());

        // Founding path (retail: the steward near the bulletin board offers neighborhood
        // founding). Shown to players who neither own a neighborhood nor carry a charter
        // already, when charter founding is enabled.
        bool const canFoundNeighborhood = sWorld->getBoolConfig(CONFIG_HOUSING_ENABLE_CREATE_CHARTER_NEIGHBORHOOD)
            && !sNeighborhoodMgr.GetNeighborhoodByOwner(player->GetGUID())
            && !player->HasItemCount(ITEM_NEIGHBORHOOD_CHARTER);

        // Only show the custom "Ask the steward to join" gossip when the player is on
        // "My First Home" (91863) and hasn't yet asked the steward (kill credit 248857).
        // For all other interactions (including quest 94210 "Feathering the Nest" turn-in),
        // return false to let the default QuestGiver / gossip pathway proceed.
        bool const onTutorial = player->GetQuestStatus(QUEST_MY_FIRST_HOME) == QUEST_STATUS_INCOMPLETE;

        if (onTutorial || canFoundNeighborhood)
        {
            InitGossipMenuFor(player, 0);
            if (me->IsQuestGiver())
                player->PrepareQuestMenu(me->GetGUID());

            if (onTutorial)
                AddGossipItemFor(player, GossipOptionNpc::None,
                    "Ask the steward to become your neighbor.",
                    GOSSIP_SENDER_MAIN, GOSSIP_ACTION_ASK_TO_JOIN);

            if (canFoundNeighborhood)
                AddGossipItemFor(player, GossipOptionNpc::None,
                    "I'm interested in founding my own Neighborhood.",
                    GOSSIP_SENDER_MAIN, GOSSIP_ACTION_FOUND_NEIGHBORHOOD);

            SendGossipMenuFor(player, DEFAULT_GOSSIP_MESSAGE, me->GetGUID());
            return true;
        }

        return false;
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
        else if (action == GOSSIP_ACTION_FOUND_NEIGHBORHOOD)
        {
            // Hand out the Neighborhood Charter; using it opens the client's charter UI,
            // which drives the CMSG_NEIGHBORHOOD_CHARTER_* flow (create / sign / finalize).
            if (!sObjectMgr->GetItemTemplate(ITEM_NEIGHBORHOOD_CHARTER))
            {
                TC_LOG_ERROR("housing", "npc_housing_steward: Item {} (Neighborhood Charter) missing from item_template, cannot hand it to player {}",
                    ITEM_NEIGHBORHOOD_CHARTER, player->GetGUID().ToString());
                return true;
            }

            if (!player->AddItem(ITEM_NEIGHBORHOOD_CHARTER, 1))
            {
                player->SendEquipError(EQUIP_ERR_BAG_FULL, nullptr, nullptr, ITEM_NEIGHBORHOOD_CHARTER);
                TC_LOG_DEBUG("housing", "npc_housing_steward: Player {} could not receive the Neighborhood Charter (bags full?)",
                    player->GetGUID().ToString());
                return true;
            }

            TC_LOG_INFO("housing", "npc_housing_steward: Player {} received the Neighborhood Charter from steward {}",
                player->GetGUID().ToString(), me->GetEntry());
        }

        return true;
    }
};

enum HousingHouseUpgrade
{
    // Jorvan Longmoor (255104), Founder's Point
    GOSSIP_MENU_HOUSE_UPGRADE           = 41352,
    GOSSIP_OPTION_UPGRADE_READY         = 0,        // 137141 -> 41353
    GOSSIP_OPTION_UPGRADE_NOT_READY     = 1,        // 137143 -> 41354
    GOSSIP_OPTION_CREATIVE_BLUEPRINTS   = 2,        // 139907, vendor
    GOSSIP_MENU_HOUSE_UPGRADE_CONFIRM   = 41353,    // "Let's go!"

    // [DNT] Level Up Houses - Cover: force-casts 1252051 (SPELL_EFFECT_GIVE_HOUSE_LEVEL) + kill credit 257414
    SPELL_LEVEL_UP_HOUSES_COVER         = 1264549
};

// Jorvan Longmoor (255104) — raises the house level (retail 12.1.0.69933, sniff 11-13-10).
// Menu 41352 shows one of two "I'd like to upgrade my house." options: 137141 when the house has the
// favor for the next level (-> 41353 "Let's go!", which casts 1264549), 137143 otherwise (-> 41354).
struct npc_housing_house_upgrade : public CreatureAI
{
    npc_housing_house_upgrade(Creature* creature) : CreatureAI(creature) { }

    void UpdateAI(uint32 /*diff*/) override { }

    static bool CanUpgrade(Player* player)
    {
        Housing const* housing = player->GetHousing();
        if (!housing || housing->GetLevel() >= MAX_HOUSE_LEVEL)
            return false;

        return housing->GetFavor() >= sHousingMgr.GetFavorThresholdForLevel(housing->GetLevel() + 1);
    }

    bool OnGossipHello(Player* player) override
    {
        InitGossipMenuFor(player, GOSSIP_MENU_HOUSE_UPGRADE);
        if (me->IsQuestGiver())
            player->PrepareQuestMenu(me->GetGUID());

        if (player->GetHousing())
            AddGossipItemFor(player, GOSSIP_MENU_HOUSE_UPGRADE,
                CanUpgrade(player) ? GOSSIP_OPTION_UPGRADE_READY : GOSSIP_OPTION_UPGRADE_NOT_READY, GOSSIP_SENDER_MAIN, 0);
        AddGossipItemFor(player, GOSSIP_MENU_HOUSE_UPGRADE, GOSSIP_OPTION_CREATIVE_BLUEPRINTS, GOSSIP_SENDER_MAIN, 0);

        SendGossipMenuFor(player, player->GetGossipTextId(GOSSIP_MENU_HOUSE_UPGRADE, me), me->GetGUID());
        return true;
    }

    bool OnGossipSelect(Player* player, uint32 menuId, uint32 /*gossipListId*/) override
    {
        if (menuId != GOSSIP_MENU_HOUSE_UPGRADE_CONFIRM)
            return false;

        CloseGossipMenuFor(player);
        if (CanUpgrade(player))
            player->CastSpell(player, SPELL_LEVEL_UP_HOUSES_COVER, true);
        return true;
    }
};

void AddSC_npc_housing_steward()
{
    RegisterCreatureAI(npc_housing_steward);
    RegisterCreatureAI(npc_housing_house_upgrade);
}
