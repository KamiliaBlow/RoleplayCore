/*
 * This file is part of the Stefal WoW Project.
 * It is designed to work exclusively with the TrinityCore framework.
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
 * This code is provided for personal and educational use within the
 * Stefal WoW Project. It is not intended for commercial distribution,
 * resale, or any form of monetization.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include "AreaTriggerAI.h"
#include "Creature.h"
#include "CreatureAI.h"
#include "CreatureAIImpl.h"
#include "DB2Stores.h"
#include "Map.h"
#include "MotionMaster.h"
#include "ObjectGuid.h"
#include "Player.h"
#include "Position.h"
#include "ScriptMgr.h"
#include "TaskScheduler.h"
#include "Unit.h"

#include "Extended_Westfall_Defines.h"

namespace Scripts::EasternKingdoms::Westfall
{
    // Helper class for player mapped cooldown
    class PlayerAreaTriggerCooldown
    {
    public:
        PlayerAreaTriggerCooldown() = default;

        template<class Rep, class Period>
        bool CanTrigger(Player* player, uint32 areaTriggerId, std::chrono::duration<Rep, Period> cooldown)
        {
            using namespace std::chrono;

            auto now = steady_clock::now();
            auto& playerMap = lastTrigger[player->GetGUID()];
            auto& last = playerMap[areaTriggerId];

            // If this is the first time, last == time_point{} ? always allow
            if (now - last < cooldown)
                return false;

            last = now;
            return true;
        }

    private:
        std::unordered_map<ObjectGuid, std::unordered_map<uint32, std::chrono::steady_clock::time_point>> lastTrigger;
    };

    static PlayerAreaTriggerCooldown g_areaTriggerCooldown;

    // 5989 - Westfall - Furlbrow's Farm - Lou
    class at_westfall_furlsbrow_farm_5989 : public AreaTriggerScript
    {
    public:
        at_westfall_furlsbrow_farm_5989() : AreaTriggerScript("at_westfall_furlsbrow_farm_5989") {}

        bool OnTrigger(Player* player, AreaTriggerEntry const* areaTrigger) override
        {
            // We should not add cooldown here otherwise we block the quest logic below this
            // Moreover the roll chance here should ensure that the random chatter does not trigger always as seen on live
            Creature* hustler = player->FindNearestCreature(Creatures::SmallTimeHustler, 20.f);
            if (hustler && hustler->IsAlive())
            {
                // We should not trigger the random talk when player comes for the Investigation dialogue
                if (player->GetQuestStatus(Quests::LivinTheLife) == QUEST_STATUS_COMPLETE && player->GetQuestStatus(Quests::LousPartingThoughts) != QUEST_STATUS_COMPLETE)
                {
                    hustler->AI()->Talk(0, player);
                    hustler->AI()->Talk(2, player);
                }
                else if (roll_chance(30))
                {
                    int8 randomLine = RAND(1, 3);
                    if (randomLine == 1)
                    {
                        hustler->AI()->Talk(0);
                        hustler->AI()->Talk(1, player);
                    }
                    else hustler->AI()->Talk(3, player);
                }

            }

            if (player->GetQuestStatus(Quests::LousPartingThoughts) == QUEST_STATUS_COMPLETE)
            {
                // add cooldown of 120s to prevent spam talk
                if (!g_areaTriggerCooldown.CanTrigger(player, areaTrigger->ID, 2min))
                    return false;

                Creature* investigator = player->GetMap()->GetCreatureBySpawnId(Spawns::StormwindInvestigator);
                if (investigator && investigator->IsAlive())
                {
                    investigator->AI()->Talk(0);

                    Creature* citizen = player->GetMap()->GetCreatureBySpawnId(Spawns::StormwindCitizenInterogation);
                    if (citizen && citizen->IsAlive())
                    {
                        citizen->m_Events.AddEventAtOffset([citizen]()
                            {
                                if (citizen && citizen->IsAlive())
                                    citizen->AI()->Talk(11);
                            }, std::chrono::seconds(5));
                    }

                    investigator->m_Events.AddEventAtOffset([investigator]()
                        {
                            if (investigator && investigator->IsAlive())
                                investigator->AI()->Talk(1);
                        }, std::chrono::seconds(10));

                    Creature* horatio = player->FindNearestCreature(Creatures::LtHorationLaine, 20.f);
                    if (horatio && horatio->IsAlive())
                    {
                        horatio->CastSpell(horatio, Spells::HoratiosSunglasses);

                        auto ScheduleTalk = [&](uint32 delay, uint32 textId)
                            {
                                horatio->m_Events.AddEventAtOffset([horatio, textId]()
                                    {
                                        if (horatio && horatio->IsAlive())
                                            horatio->AI()->Talk(textId);
                                    }, std::chrono::seconds(delay));
                            };

                        ScheduleTalk(15, 0);
                        ScheduleTalk(18, 1);
                        ScheduleTalk(23, 2);
                        ScheduleTalk(28, 3);
                        ScheduleTalk(33, 4);

                    }
                }
            }
            return true;
        }
    };

    // 5993 - Westfall - Sentinel Hill Tower
    class at_westfall_sentinel_hill_tower_5993 : public AreaTriggerScript
    {
    public:
        at_westfall_sentinel_hill_tower_5993() : AreaTriggerScript("at_westfall_sentinel_hill_tower_5993") {}

        bool OnTrigger(Player* player, AreaTriggerEntry const* areaTrigger) override
        {
            if (player->GetQuestStatus(Quests::HopeForThePeople) == QUEST_STATUS_COMPLETE)
            {
                // add cooldown of 120s to prevent spam talk
                if (!g_areaTriggerCooldown.CanTrigger(player, areaTrigger->ID, 2min))
                    return false;

                Creature* gryan = player->FindNearestCreature(Creatures::GryanStoutmantle, 20.f);
                if(gryan && gryan->IsAlive())
                {
                    gryan->AI()->Talk(1);

                    gryan->m_Events.AddEventAtOffset([gryan]()
                        {
                            if (gryan && gryan->IsAlive())
                                gryan->AI()->Talk(2);
                        }, std::chrono::seconds(18));
                }

                Creature* ripsnarl = player->FindNearestCreature(Creatures::RipsnarlAtTower, 20.f);
                if (ripsnarl && ripsnarl->IsAlive())
                {
                    ripsnarl->m_Events.AddEventAtOffset([ripsnarl]()
                        {
                            if (ripsnarl && ripsnarl->IsAlive())
                                ripsnarl->AI()->Talk(0);
                        }, std::chrono::seconds(25));
                }

                Creature* horatio = player->FindNearestCreature(Creatures::LtHorationLaineAtTower, 20.f);
                if (horatio && horatio->IsAlive())
                {
                    auto ScheduleTalk = [&](uint32 delay, uint32 textId)
                        {
                            horatio->m_Events.AddEventAtOffset([horatio, textId, player]()
                                {
                                    if (horatio && horatio->IsAlive())
                                        horatio->AI()->Talk(textId, player);
                                }, std::chrono::seconds(delay));
                        };

                    ScheduleTalk(7, 3); // Good to hear...
                    ScheduleTalk(13, 4); // Might I ask
                    ScheduleTalk(30, 5); // Wow
                    ScheduleTalk(34, 6); // bark
                    ScheduleTalk(38, 7); // bite
                }
            }

            return true;
        }
    };

    // 5994 - Westfall - Mortwake Tower
    class at_westfall_mortwake_tower_5994 : public AreaTriggerScript
    {
    public:
        at_westfall_mortwake_tower_5994() : AreaTriggerScript("at_westfall_mortwake_tower_5994") {}

        bool OnTrigger(Player* player, AreaTriggerEntry const* areaTrigger) override
        {
            // Check if player has the quest active and the required aura
            if (player->GetQuestStatus(Quests::SecretsOfTheTower) == QUEST_STATUS_INCOMPLETE && player->HasAura(Spells::PotionShrouding))
            {
                // add cooldown of 120s to prevent spam talk
                if (!g_areaTriggerCooldown.CanTrigger(player, areaTrigger->ID, 2min))
                    return false;

                // Find the trigger NPC
                Creature* trigger = player->FindNearestCreature(Creatures::MortwakeTrigger, 50.f);
                if (trigger)
                    // Set guid on the NPC with player GUID to start the event
                    trigger->AI()->SetGUID(player->GetGUID(), 1);
            }

            return true;
        }
    };

    // 5998 - Westfall - Moonbrook Center
    class at_westfall_moonbrook_center_5998 : public AreaTriggerScript
    {
    public:
        at_westfall_moonbrook_center_5998() : AreaTriggerScript("at_westfall_moonbrook_center_5998") {}

        bool OnTrigger(Player* player, AreaTriggerEntry const* areaTrigger) override
        {
            // Check if player has the quest active
            if (player->GetQuestStatus(Quests::TheDawningOfANewDay) == QUEST_STATUS_INCOMPLETE)
            {
                // add cooldown of 5min to prevent spam talk
                if (!g_areaTriggerCooldown.CanTrigger(player, areaTrigger->ID, 5min))
                    return false;

                // Find the trigger NPC
                Creature* trigger = player->FindNearestCreature(Creatures::MoonbrookPlayerTrigger, 10.f);
                if (trigger)
                    // Set guid on the NPC with player GUID to start the event
                    trigger->AI()->SetGUID(player->GetGUID(), 1);
            }

            return true;
        }
    };

    // 6080 - Westfall - Moonbrook Defias Hideout Entrance
    class at_westfall_moonbrook_defias_entrance_6080 : public AreaTriggerScript
    {
    public:
        at_westfall_moonbrook_defias_entrance_6080() : AreaTriggerScript("at_westfall_moonbrook_defias_entrance_6080") {}

        bool OnTrigger(Player* player, AreaTriggerEntry const* areaTrigger) override
        {
            // Check if player has the quest active
            if (player->GetQuestStatus(Quests::AVisionOfThePast) == QUEST_STATUS_INCOMPLETE)
            {
                // add cooldown of 5min to prevent spam trigger
                if (!g_areaTriggerCooldown.CanTrigger(player, areaTrigger->ID, 5min))
                    return false;

                // Find the trigger NPC
                Creature* trigger = player->FindNearestCreature(Creatures::MoonbrookPlayerTrigger, 10.f);
                if (trigger)
                    trigger->AI()->Talk(Talks::Q26320TriggerWarning, player);
            }

            return true;
        }
    };

    // 8481 - Westfall - Old MacDonald
    class at_westfall_old_macdonald_8481 : public AreaTriggerScript
    {
    public:
        at_westfall_old_macdonald_8481() : AreaTriggerScript("at_westfall_old_macdonald_8481") {}

        bool OnTrigger(Player* player, AreaTriggerEntry const* areaTrigger) override
        {
            // add cooldown of 5min to prevent spam trigger
            if (!g_areaTriggerCooldown.CanTrigger(player, areaTrigger->ID, 5min))
                return false;

            // Find the Old MacDonald NPC
            Creature* mac = player->FindNearestCreature(Creatures::OldMacDonald, 20.f);
            if (mac)
                mac->AI()->Talk(0, player);

            return true;
        }
    };
}

void AddSC_custom_westfall_at()
{
    using namespace Scripts::EasternKingdoms::Westfall;

    new at_westfall_furlsbrow_farm_5989();
    new at_westfall_sentinel_hill_tower_5993();
    new at_westfall_mortwake_tower_5994();
    new at_westfall_moonbrook_center_5998();
    new at_westfall_moonbrook_defias_entrance_6080();
    new at_westfall_old_macdonald_8481();
}