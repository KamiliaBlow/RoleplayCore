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

#include "Creature.h"
#include "CreatureAIImpl.h"
#include "MotionMaster.h"
#include "PhasingHandler.h"
#include "Player.h"
#include "QuestDef.h"
#include "ScriptedCreature.h"
#include "ScriptMgr.h"
#include "TaskScheduler.h"
#include "TemporarySummon.h"

#include "Extended_Westfall_Defines.h"

namespace Scripts::EasternKingdoms::Westfall
{
    // 26297 - The Dawning of a new day
    class quest_26297_the_dawning_of_a_new_day : public QuestScript
    {
    public:
        quest_26297_the_dawning_of_a_new_day() : QuestScript("quest_26297_the_dawning_of_a_new_day") { }

        void OnQuestStatusChange(Player* player, Quest const* /*quest*/, QuestStatus /*oldStatus*/, QuestStatus newStatus) override
        {
            if (!player)
                return;

            if (newStatus != QUEST_STATUS_INCOMPLETE)
                return;

            player->CastSpell(player, Spells::SummonShadowyFigureMoonbrook);

            SummonCrowd(player);
        }
    };

    // 26322 - Rise of the brotherhood
    class quest_26322_rise_of_the_brotherhood : public QuestScript
    {
    public:
        quest_26322_rise_of_the_brotherhood() : QuestScript("quest_26322_rise_of_the_brotherhood") { }

        void OnQuestStatusChange(Player* player, Quest const* /*quest*/, QuestStatus /*oldStatus*/, QuestStatus newStatus) override
        {
            if (!player)
                return;

            if (newStatus != QUEST_STATUS_INCOMPLETE)
                return;

            _playerGuid = player->GetGUID();
            PhasingHandler::RemovePhase(player, 50005, true);
            PhasingHandler::AddPhase(player, Phases::WestfallRiseBR, true);

            SummonSpellNPCS(player);

            if (Creature* gryan = player->FindNearestCreature(Creatures::SpawnedGryanStoutmantleAtTower, 20.f))
                gryan->AI()->SetGUID(player->GetGUID(), 1);
        }

        void SummonSpellNPCS(Player* player)
        {
            if (!player)
                return;

            for (uint32 spellId : PlayerSummonSpells)
                player->CastSpell(player, spellId, true);
        }

    private:
        ObjectGuid _playerGuid;    
    };
}

void AddSC_custom_westfall_quests()
{
    using namespace Scripts::EasternKingdoms::Westfall;

    new quest_26297_the_dawning_of_a_new_day();
    new quest_26322_rise_of_the_brotherhood();
}