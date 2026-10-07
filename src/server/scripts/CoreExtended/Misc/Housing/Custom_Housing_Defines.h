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

#ifndef CUSTOM_HOUSING_DEFINES_H
#define CUSTOM_HOUSING_DEFINES_H

#include "Define.h"

namespace Scripts::Custom::Housing
{
    namespace Quests
    {
        static constexpr uint32 WelcomeHome    = 91968; // remove decor -> KillCredits::RemoveJunk
        static constexpr uint32 TimeToDecorate = 91969; // place decor -> KillCredits::DecorPlaced
    }

    namespace KillCredits
    {
        static constexpr uint32 DecorPlaced = 249327;   // DNT Kill Credit - Decor Placed
        static constexpr uint32 RemoveJunk  = 249304;   // DNT Kill Credit - Remove Junk
    }
}

#endif // CUSTOM_HOUSING_DEFINES_H
