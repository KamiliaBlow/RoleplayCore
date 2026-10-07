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

#include "ScriptMgr.h"
#include "Player.h"

#include "Custom_Housing_Defines.h"

namespace Scripts::Custom::Housing
{
    // Kill credits are self-scoping - KilledMonsterCredit only counts objectives
    // the player actually has, so no quest checks are needed here.
    class player_housing_quests : public PlayerScript
    {
    public:
        player_housing_quests() : PlayerScript("player_housing_quests") {}

        void OnPlayerHousingDecorAdd(Player* player, uint32 /*decorEntryId*/) override
        {
            player->KilledMonsterCredit(KillCredits::DecorPlaced);
        }

        void OnPlayerHousingDecorRemove(Player* player, uint32 /*decorEntryId*/) override
        {
            player->KilledMonsterCredit(KillCredits::RemoveJunk);
        }
    };
}

void AddSC_custom_housing_player()
{
    using namespace Scripts::Custom::Housing;
    new player_housing_quests();
}
