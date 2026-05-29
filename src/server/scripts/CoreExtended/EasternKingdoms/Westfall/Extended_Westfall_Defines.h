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
#include "Position.h"
#include "TemporarySummon.h"

namespace Scripts::EasternKingdoms::Westfall
{
    namespace Quests
    {
        static constexpr uint32 TheDefiasBrotherhoodFinal = 166;

        static constexpr uint32 LivinTheLife = 26228;
        static constexpr uint32 LousPartingThoughts = 26232;
        static constexpr uint32 NewWestfallStew = 26241;
        static constexpr uint32 HopeForThePeople = 26266;
        static constexpr uint32 InDefenseOfWestfall = 26286;
        static constexpr uint32 SecretsOfTheTower = 26290;
        static constexpr uint32 TheDawningOfANewDay = 26297;
        static constexpr uint32 AVisionOfThePast = 26320;
        static constexpr uint32 RiseOfTheBrotherhood = 26322;
    }

    namespace Creatures
    {
        static constexpr uint32 OldMacDonald = 65648;

        static constexpr uint32 WestfallStew = 42617;
        static constexpr uint32 HomelessStormwindCitizen = 42384;
        static constexpr uint32 HomelessStormwindCitizen2 = 42386; // female version
        static constexpr uint32 Transient = 42383;
        static constexpr uint32 WestPlainsDrifter = 42391;
        static constexpr uint32 SentinelHillGuard = 42407;

        static constexpr uint32 SmallTimeHustler = 42390;
        static constexpr uint32 WestfallThug = 42387;
        static constexpr uint32 LousPartingThoughtsTrigger = 42562;
        static constexpr uint32 LousPartingThoughtsCredit = 42417;
        static constexpr uint32 StormwindInvestigator = 42559;
        static constexpr uint32 LtHorationLaine = 42558;

        static constexpr uint32 SalmaSaldean = 235;
        static constexpr uint32 WestfallOrphan = 42385;

        static constexpr uint32 SpawnedScoutGaliaanAtTower = 42752;
        static constexpr uint32 SpawnedCaptainDanuvinAtTower = 42751;
        static constexpr uint32 SpawnedGryanStoutmantleAtTower = 42750;
        static constexpr uint32 SpawnedHopeSaldeanAtTower = 42749;
        static constexpr uint32 SpawnedRipsnarlAtTower = 42748;
        static constexpr uint32 SpawnedStormwindInvestigatorAtTower = 42745;
        static constexpr uint32 SpawnedHoratioLaineAtTower = 42744;
        static constexpr uint32 DefiasBlackguard = 42769;
        static constexpr uint32 SpawnedHelixAtTower = 42753;
        static constexpr uint32 SpawnedGlubtokAtTower = 42755;
        static constexpr uint32 SentinelHillFireTrigger = 42793;

        static constexpr uint32 GryanStoutmantle = 234;
        static constexpr uint32 LtHorationLaineAtTower = 42308;
        static constexpr uint32 RipsnarlAtTower = 42635;

        static constexpr uint32 AgentKearnen = 7024;
        static constexpr uint32 EliteMercenary = 42656;
        static constexpr uint32 MortwakeTrigger = 17234;
        static constexpr uint32 MortwakeShadowyFigure = 42662;
        static constexpr uint32 MortwakeHelix = 42655;
        static constexpr uint32 MortwakeOaf = 42754;

        static constexpr uint32 SpawnedShadowyFigureAtMoonbrook = 42680;
        static constexpr uint32 MoonbrookPlayerTrigger = 43515;

        static constexpr uint32 VisionOfThePast = 42693;
        static constexpr uint32 VisionOfThePastEdwinVC = 639;
        static constexpr uint32 VisionOfThePastVanessaVC = 42371;
        static constexpr uint32 VisionAllianceWarrior = 42699;
        static constexpr uint32 VisionAllianceRogue = 42700;
        static constexpr uint32 VisionAllianceHunter = 42701;
        static constexpr uint32 VisionAllianceMage = 42702;
        static constexpr uint32 VisionAlliancePriest = 42703;
    }

    namespace Gameobjects
    {
        static constexpr uint32 Fire = 204099;
    }

    namespace Spawns
    {
        static constexpr uint32 WestfallGuard = 275390;
        static constexpr uint32 RightSideCitizen = 275382;
        static constexpr uint32 RightBackCitizen = 274898;
        static constexpr uint32 LeftSideCitizen = 275403;
        static constexpr uint32 LeftBackCitizen = 274914;
        static constexpr uint32 DrifterWithCartBuddy = 280419;
        static constexpr uint32 CartBuddy = 280420;
        static constexpr uint32 StormwindInvestigator = 251574;
        static constexpr uint32 StormwindCitizenInterogation = 251572;

        // Vision of the past
        static constexpr uint32 VisionBunnyForHunter1 = 376144;
        static constexpr uint32 VisionBunnyForHunter2 = 376134;
        static constexpr uint32 VisionBunnyForPriest = 376147;
        static constexpr uint32 VisionBunnyForMage = 376128;
    }

    namespace Spells
    {
        static constexpr uint32 FullBelly = 79451;
        static constexpr uint32 SleepAura = 78677;
        static constexpr uint32 RottenAppleToss = 58509;
        static constexpr uint32 RottenBananaToss = 58513;
        static constexpr uint32 HoboStew = 80384;

        // Invis spells
        static constexpr uint32 ApplyQuestInvis1 = 78284;
        static constexpr uint32 ApplyQuestInvis2 = 79343;

        static constexpr uint32 DetectQuestInvis0 = 79489;
        static constexpr uint32 DetectQuestInvis1 = 79229;
        static constexpr uint32 DetectQuestInvis2 = 79341;

        static constexpr uint32 HoratiosSunglasses = 78935;

        // Sentinel Hill
        static constexpr uint32 InStocks = 69196;

        // Mortwake Tower
        static constexpr uint32 PotionShrouding = 79528;
        static constexpr uint32 KillShot = 79525;
        static constexpr uint32 TeleportVisual = 64446;
        static constexpr uint32 QuestCredit26290 = 79534;

        // Moonbrook
        static constexpr uint32 SummonShadowyFigureMoonbrook = 79551;
        static constexpr uint32 CancelShadowyFigureMoonbrook = 79557;

        static constexpr uint32 IncenseBurner = 79586;
        static constexpr uint32 VisionOfThePastCredit = 79620;
        static constexpr uint32 VisionVCSummonAllies = 5200;
        static constexpr uint32 AlliancePriestFortitude = 13864;
        static constexpr uint32 BloodsailCompanion = 5172;

        // Rise of the brotherhood
        static constexpr uint32 SummonRipsnarl = 79670;
        static constexpr uint32 SummonScoutGaliaan = 79669;
        static constexpr uint32 SummonCaptainDanuvin = 79668;
        static constexpr uint32 SummonStormwindInvestigator2 = 79667;
        static constexpr uint32 SummonStormwindInvestigator1 = 79666;
        static constexpr uint32 SummonHopeSaldean = 79665;
        static constexpr uint32 SummonHoratioLaine = 79664;
        static constexpr uint32 SummonGryanStoutmantle = 79663;
        static constexpr uint32 SummonHelix = 79763;
        static constexpr uint32 SummonGlubtok = 79762;

        static constexpr uint32 TransformHuman = 79745;
        static constexpr uint32 AdmiralHat = 79750;
        static constexpr uint32 TransformVaneesa = 79709;
        static constexpr uint32 SummonBlackguard = 79712;
        static constexpr uint32 TiedUpGoodGuysForceCast = 79795;
        static constexpr uint32 TiedUpGoodGuys = 79723;
        static constexpr uint32 TiedUpVisual = 79724;
        static constexpr uint32 DefiasFinaleEventCredit = 79758;
        static constexpr uint32 TossTorch = 79778;
        static constexpr uint32 TossTorchTrigger = 79779;
        static constexpr uint32 ThrowTorch = 83860;
        static constexpr uint32 Smoke = 67690;
        static constexpr uint32 HitMe = 65600;
        static constexpr uint32 Sneak = 22766;
    }

    namespace Events
    {
        static constexpr int8 WestfalGuardPacifyDrifters = 1;
        static constexpr int8 WestfalGuardCrowdReaction = 2;

        static constexpr int8 AgentKearnenCastKillShot = 1;

        static constexpr int8 VisionOfThePastStartPath = 1;
        static constexpr int8 VisionOfThePastCombat = 2;
        static constexpr int8 VisionChildScene = 3;
        static constexpr int8 VisionOfThePastQuestReward = 4;
        static constexpr int8 VisionOfThePastExit = 5;
        static constexpr int8 VisionRemovePassenger = 6;
        static constexpr int8 VisionAllianceNPCSLeave = 7;

        static constexpr int8 BRHopeWalkToGryan = 1;
        static constexpr int8 BRHopeSetData1Gryan = 2;
        static constexpr int8 BRHopeMonologue = 3;
        static constexpr int8 BRHopeSummonBrotherhood = 4;
        static constexpr int8 BRStoreHelixGlubtok = 5;
        static constexpr int8 BRHopeTieThemUp = 6;
        static constexpr int8 BRHopeSetData2Gryan = 7;
        static constexpr int8 BRHopeDeparture = 8;
        static constexpr int8 BRHopeBurnCity = 9;
    }

    namespace Positions
    {
        static constexpr Position Q26232ThugPositions[4] =
        {
            { -9859.36f, 1332.42f, 41.985f, 2.49f },
            { -9862.51f, 1332.08f, 41.985f, 0.85f },
            { -9863.49f, 1335.49f, 41.985f, 5.63f },
            { -9860.42f, 1335.46f, 41.985f, 4.11f },
        };

        static constexpr Position SalmaSaldeanStew = { -10106.9238f, 1038.5223f, 37.947f, 5.64f };

        static constexpr Position GryanStoutmantleTalkRipsnarl = { -10512.5126f, 1046.5996f, 60.5181f, 3.6035f };

        struct CrowdSpawnData
        {
            float x, y, z, o;
        };

        static constexpr CrowdSpawnData CrowdSpawns[] =
        {
            { -11009.036f, 1490.47f, 43.58f, 4.16f },
            { -11010.76f,  1488.21f, 43.57f, 4.33f },
            { -11017.39f,  1491.76f, 43.19f, 4.78f },
            { -11021.73f,  1493.054f,43.184f,5.09f },
            { -11025.74f,  1487.70f, 43.17f, 5.45f },
            { -11025.25f,  1482.23f, 43.03f, 6.04f },
            { -11029.68f,  1481.255f,43.185f,6.20f },
            { -11024.46f,  1473.88f, 43.02f, 0.43f },
            { -11019.49f,  1471.70f, 43.21f, 1.09f },
            { -11021.56f,  1497.053f,43.20f, 5.00f },
            { -11012.29f,  1488.14f, 43.77f, 4.16f },
            { -11014.67f,  1493.14f, 43.23f, 4.60f },
            { -11019.8f,   1494.25f, 43.2f,  4.77f },
            { -11023.6f,   1489.35f, 43.17f, 4.77f },
            { -11023.1f,   1482.51f, 43.07f, 6.25f },
            { -11027.2f,   1494.37f, 43.17f, 5.15f },
            { -11029.6f,   1488.29f, 43.19f, 5.64f },
            { -11030.8f,   1485.13f, 43.31f, 5.94f },
            { -11026.2f,   1478.62f, 42.94f, 6.17f },
            { -11013.2f,   1497.81f, 43.31f, 4.44f },
            { -11011.6f,   1492.61f, 43.39f, 4.44f },
            { -11015.4f,   1489.06f, 43.28f, 4.73f },
            { -11020.2f,   1490.15f, 43.19f, 5.07f },
            { -11009.9f,   1483.52f, 44.06f, 3.88f },
            { -11020.1f,   1484.87f, 43.18f, 5.33f },
            { -11017.7f,   1487.42f, 43.24f, 4.89f },
            { -11017.9f,   1498.24f, 43.20f, 5.04f },
        };

        // Vision of the past
        static constexpr Position VisionEntryPosition = { -97.6376f, -690.562f, 24.3914f };
        static constexpr Position VisionLeavePosition = { -19.5636f, -377.193f, 60.8038f };
        static constexpr Position VisionVC = { -86.0902f, -819.3812f, 39.3065f, 6.176f };
        static constexpr Position VisionAllianceNPCS = { -47.6163f, -808.856f,	42.8273f };
        static constexpr Position VisionAllianceNPCSForward = { -64.4392f, -819.938f, 41.2188f };
        static constexpr Position VisionAllianceNPCSLeave = { -68.508f, -898.203f, 14.2018f };

        // Rise of the Brotherhood
        static constexpr Position BRHopeStart = { -10507.65f, 1042.81f, 60.51f };
        static constexpr Position BRVanessaToAdmiral = { -10512.36f, 1044.36f, 60.518f };
        static constexpr Position BRVanessaWalkingAway = { -10513.37f, 1056.48f, 57.605f };
        static constexpr Position BRVanessaDeparture = { -10518.38f, 1067.99f, 54.84f };
        static constexpr Position BRRipsnarl1 = { -10513.41f, 1041.11f, 60.518f };
        static constexpr Position BRRipsnarl2 = { -10511.34f, 1042.46f, 60.5172f };
        static constexpr Position BRRipsnarl3 = { -10516.64f, 1064.78f, 55.362f };
        static constexpr Position BRHelixDeparture = { -10520.01f, 1062.31f, 55.386f };
        static constexpr Position BRGlubtokDeparture = { -10511.79f, 1065.78f, 55.085f };
    }

    namespace Talks
    {
        // Quest: Lous parting thoughts
        // Thugs
        static constexpr int8 Q26232ThreatenPlayer = 0;
        static constexpr int8 Q26232AskMeet = 1;
        static constexpr int8 Q26232NoticePlayer = 2;
        static constexpr int8 Q26232ConfirmMeet = 3;
        static constexpr int8 Q26232Congratulate = 4;
        static constexpr int8 Q26232AskIdentity = 5;
        static constexpr int8 Q26232Die = 6;
        // Trigger
        static constexpr int8 Q26232ReturnToFurlsbrowCottage = 3;

        // Mortwake
        static constexpr int8 Q26232Helix0 = 0;
        static constexpr int8 Q26232Helix1 = 1;
        static constexpr int8 Q26232Helix2 = 2;
        static constexpr int8 Q26232Helix3 = 3;
        static constexpr int8 Q26232Shadowy0 = 0;
        static constexpr int8 Q26232Shadowy1 = 1;
        static constexpr int8 Q26232Shadowy2 = 2;
        static constexpr int8 Q26232Shadowy3 = 3;
        static constexpr int8 Q26232Shadowy4 = 4;

        // Moonbrook
        static constexpr int8 Q26297TriggerWarning = 0;
        static constexpr int8 Q26297Shadowy0 = 0;
        static constexpr int8 Q26297Shadowy1 = 1;
        static constexpr int8 Q26297Shadowy2 = 2;
        static constexpr int8 Q26297Shadowy3 = 3;
        static constexpr int8 Q26297Shadowy4 = 4;
        static constexpr int8 Q26297Shadowy5 = 5;
        static constexpr int8 Q26297Shadowy6 = 6;
        static constexpr int8 Q26297Shadowy7 = 7;

        static constexpr int8 Q26320TriggerWarning = 1;

        // Vision of the Past
        static constexpr int8 VisionIntro = 0;
        static constexpr int8 VisionVanessa0 = 0;
        static constexpr int8 VisionVCChallenge = 0;
        static constexpr int8 VisionVCLapdogs = 1;
        static constexpr int8 VisionVCCallAllies = 2;
        static constexpr int8 VisionVCFools = 3;
        static constexpr int8 VisionVCPrevail = 5;
        static constexpr int8 VisionAllianceWarr0 = 0;
        static constexpr int8 VisionAllianceWarr1 = 1;
        static constexpr int8 VisionAllianceWarr2 = 2;
        static constexpr int8 VisionAllianceWarr3 = 3;
    }

    namespace Dialogue
    {
        enum class MoonbrookActor : uint8
        {
            Trigger,
            Shadowy
        };

        struct MoonbrookStep
        {
            MoonbrookActor actor;
            int32 textId;    // -1 = no Talk, just run custom logic
            uint32 delayMs;  // delay before next step
        };

        static constexpr MoonbrookStep MoonbrookSequence[] =
        {
            { MoonbrookActor::Trigger, Talks::Q26297TriggerWarning, 1500 },
            { MoonbrookActor::Shadowy, Talks::Q26297Shadowy0,       3000 },
            { MoonbrookActor::Shadowy, Talks::Q26297Shadowy1,       5500 },
            { MoonbrookActor::Shadowy, Talks::Q26297Shadowy2,       5000 },
            { MoonbrookActor::Shadowy, Talks::Q26297Shadowy3,       5500 },
            { MoonbrookActor::Shadowy, Talks::Q26297Shadowy4,       5500 },
            { MoonbrookActor::Shadowy, Talks::Q26297Shadowy5,       5500 },
            { MoonbrookActor::Shadowy, Talks::Q26297Shadowy6,       5500 },
            { MoonbrookActor::Shadowy, Talks::Q26297Shadowy7,       5500 },
            // Last "step" is special: no text, just crowd + credit + despawn
            { MoonbrookActor::Trigger, -1,                           0   },
        };

        enum class MortwakeActor : uint8
        {
            Helix,
            Shadowy,
            PlayerCredit,
            Exit
        };

        struct MortwakeStep
        {
            MortwakeActor actor;
            int32 textId;     // -1 if no Talk
            uint32 delayMs;   // delay before next step
        };

        static constexpr MortwakeStep MortwakeSequence[] =
        {
            { MortwakeActor::Helix,   Talks::Q26232Helix0,   6000 },
            { MortwakeActor::Shadowy, Talks::Q26232Shadowy0, 6000 },
            { MortwakeActor::Helix,   Talks::Q26232Helix1,   6000 },
            { MortwakeActor::Shadowy, Talks::Q26232Shadowy1, 6000 },
            { MortwakeActor::Helix,   Talks::Q26232Helix2,   6000 },
            { MortwakeActor::Shadowy, Talks::Q26232Shadowy2, 6000 },
            { MortwakeActor::Shadowy, Talks::Q26232Shadowy3, 6000 },
            { MortwakeActor::Helix,   Talks::Q26232Helix3,   6000 },
            { MortwakeActor::Shadowy, Talks::Q26232Shadowy4, 2000 },
            { MortwakeActor::PlayerCredit, -1,              1000 },
            { MortwakeActor::Exit,    -1,                   0    },
        };

        // Phase table describing the scripted sequence
        struct Q26232PhaseStep
        {
            uint8 thugIndex;   // 255 = special case
            uint32 talkId;     // 0 = no talk
            uint32 delay;      // next phase delay
        };

        static constexpr Q26232PhaseStep Q26232Steps[] =
        {
            { 1, Talks::Q26232AskMeet,        3500 }, // Phase 0
            { 0, Talks::Q26232ConfirmMeet,    4000 }, // Phase 1
            { 0, Talks::Q26232Congratulate,   7000 }, // Phase 2
            { 3, Talks::Q26232AskIdentity,    4000 }, // Phase 3
            { 255, 0,                         1000 }, // Phase 4: face player
            { 2, Talks::Q26232NoticePlayer,   4500 }, // Phase 5
            { 1, Talks::Q26232ThreatenPlayer, 4500 }, // Phase 6
            { 0, Talks::Q26232Die,            2000 }, // Phase 7
            { 255, 0,                         1000 }, // Phase 8: enable combat
            { 255, 0,                            0 }, // Phase 9: engage
        };
    }

    namespace Sounds
    {
        static constexpr uint32 Warning = 8174;
        static constexpr uint32 WomanScream = 17852;
        static constexpr uint32 WeaponFire1 = 161625;
        static constexpr uint32 WeaponFire2 = 161611;
        static constexpr uint32 CrowdCheer = 15882;
    }

    namespace Paths
    {
        static constexpr uint32 VisionOfThePast = 4269300;
    }

    namespace Phases
    {
        static constexpr uint32 WestfallAct2 = 226;
        static constexpr uint32 WestfallRiseBR = 232;
    }

    struct AllianceGroup
    {
        Creature* warrior = nullptr;
        Creature* mage = nullptr;
        Creature* rogue = nullptr;
        Creature* priest = nullptr;
        Creature* hunter = nullptr;
    };

    template<class F>
    void Schedule(Creature* c, F&& fn, std::chrono::seconds delay)
    {
        if (c && c->IsAlive())
            c->m_Events.AddEventAtOffset(std::forward<F>(fn), delay);
    }

    inline void SummonCrowd(WorldObject* source)
    {
        if (!source)
            return;

        Map* map = source->GetMap();
        if (!map)
            return;

        for (auto const& spawn : Positions::CrowdSpawns)
        {
            uint32 crowdNpc = RAND(Creatures::HomelessStormwindCitizen, Creatures::HomelessStormwindCitizen2,
                Creatures::WestPlainsDrifter, Creatures::SmallTimeHustler);

            if (Creature* c = source->SummonCreature(
                crowdNpc,
                spawn.x, spawn.y, spawn.z, spawn.o,
                TEMPSUMMON_TIMED_DESPAWN_OUT_OF_COMBAT,
                360s
            ))
                c->SetImmuneToAll(true);
        }
    }

    struct BRGroup
    {
        Creature* gryan = nullptr;
        Creature* horatio = nullptr;
        Creature* ripsnarl = nullptr;
        Creature* helix = nullptr;
        Creature* glubtok = nullptr;
    };

    static constexpr uint32 PlayerSummonSpells[] =
    {
        Spells::SummonRipsnarl,
        Spells::SummonScoutGaliaan,
        Spells::SummonCaptainDanuvin,
        Spells::SummonStormwindInvestigator2,
        Spells::SummonStormwindInvestigator1,
        Spells::SummonHopeSaldean,
        Spells::SummonHoratioLaine,
        Spells::SummonGryanStoutmantle,
    };

    struct BrotherhoodSpawn
    {
        uint32 entry;
        Position pos;
        uint32 despawnTime;
    };

    static constexpr BrotherhoodSpawn Brotherhood[] =
    {
        { Creatures::DefiasBlackguard, { -10500.37f, 1042.65f, 60.51f, 3.06f }, 120 },
        { Creatures::DefiasBlackguard, { -10500.99f, 1046.73f, 60.517f, 3.29f }, 120 },
        { Creatures::DefiasBlackguard, { -10505.202f, 1040.46f, 60.51f, 1.88f }, 120 },
        { Creatures::DefiasBlackguard, { -10507.89f, 1039.52f, 60.51f, 1.95f }, 120 },
        { Creatures::DefiasBlackguard, { -10513.54f, 1038.66f, 60.51f, 0.51f }, 120 },
        { Creatures::DefiasBlackguard, { -10514.930f, 1042.012f, 60.51f, 0.399f }, 120 },
        { Creatures::DefiasBlackguard, { -10516.797f, 1048.61f, 59.95f, 5.32f }, 120 },
        { Creatures::DefiasBlackguard, { -10514.032f, 1049.80f, 59.92f, 5.11f} , 120 },
        { Creatures::DefiasBlackguard, { -10509.012f, 1051.92f, 59.85f, 5.11f }, 120 },
        { Creatures::DefiasBlackguard, { -10504.77f, 1053.57f, 59.86f, 4.84f }, 120 },
    };
}

