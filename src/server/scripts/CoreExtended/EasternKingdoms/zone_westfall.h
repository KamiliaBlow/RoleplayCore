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

#include "Position.h"

enum Custom_Westfall_Generic_NPCs
{
    NPC_NORTHSHIRE_WORG = 49871,
    NPC_WESTFALL_THUG = 42387,
    NPC_WESTFALL_Q26232_TRIGGER = 42562,
    NPC_WESTFALL_Q26232_CREDIT = 42417
};

enum Custom_Westfall_Quests
{
    QUEST_LOUS_PARTING_THOUGHTS = 26232,
};

enum Custom_Westfall_Spells
{
    SPELL_WESTFALL_DETECT_QUEST_INVIS_1 = 79229,
    SPELL_WESTFALL_DETECT_QUEST_INVIS_2 = 79341,
};

static Position const Q26232ThugPositions[4] =
{
    { -9859.36f, 1332.42f, 41.985f, 2.49f },
    { -9862.51f, 1332.08f, 41.985f, 0.85f },
    { -9863.49f, 1335.49f, 41.985f, 5.63f },
    { -9860.42f, 1335.46f, 41.985f, 4.11f },
};

constexpr char SAY_FURLBROW_RETURN[] = "Hurry back to the Furlbrow's Cottage.";

enum Custom_Westfall_Q26232_TALK
{
    Q26232_SAY_THREATEN_PLAYER = 0,
    Q26232_SAY_ASK_MEET = 1,
    Q26232_SAY_NOTICE_PLAYER = 2,
    Q26232_SAY_CONFIRM_MEET = 3,
    Q26232_SAY_CONGRATULATE = 4,
    Q26232_SAY_ASK_IDENTITY = 5,
    Q26232_SAY_DIE = 6,
};
