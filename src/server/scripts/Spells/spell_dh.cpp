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

/*
 * Scripts for spells with SPELLFAMILY_DEMONHUNTER and SPELLFAMILY_GENERIC spells used by demon hunter players.
 * Ordered alphabetically using scriptname.
 * Scriptnames of files in this file should be prefixed with "spell_dh_".
 */

#include "AreaTrigger.h"
#include "AreaTriggerAI.h"
#include "Containers.h"
#include "DB2Stores.h"
#include "PathGenerator.h"
#include "Player.h"
#include "ScriptMgr.h"
#include "Spell.h"
#include "SpellAuraEffects.h"
#include "SpellAuras.h"
#include "SpellHistory.h"
#include "SpellMgr.h"
#include "SpellScript.h"
#include "TaskScheduler.h"
#include <numeric>

enum DemonHunterSpells
{
    SPELL_DH_ABYSSAL_STRIKE                        = 207550,
    SPELL_DH_ANNIHILATION                          = 201427,
    SPELL_DH_ANNIHILATION_MH                       = 227518,
    SPELL_DH_ANNIHILATION_OH                       = 201428,
    SPELL_DH_ARMY_UNTO_ONESELF                     = 442714,
    SPELL_DH_AWAKEN_THE_DEMON_WITHIN_CD            = 207128,
    SPELL_DH_BLADE_WARD                            = 442715,
    SPELL_DH_BLUR                                  = 212800,
    SPELL_DH_BLUR_TRIGGER                          = 198589,
    SPELL_DH_BURNING_ALIVE                         = 207739,
    SPELL_DH_BURNING_ALIVE_TARGET_SELECTOR         = 207760,
    SPELL_DH_CALCIFIED_SPIKES_TALENT               = 389720,
    SPELL_DH_CALCIFIED_SPIKES_MOD_DAMAGE           = 391171,
    SPELL_DH_CHAOS_NOVA                            = 179057,
    SPELL_DH_CHAOS_STRIKE                          = 162794,
    SPELL_DH_CHAOS_STRIKE_ENERGIZE                 = 193840,
    SPELL_DH_CHAOS_STRIKE_MH                       = 222031,
    SPELL_DH_CHAOS_STRIKE_OH                       = 199547,
    SPELL_DH_CHAOS_THEORY_TALENT                   = 389687,
    SPELL_DH_CHAOS_THEORY_CRIT                     = 390195,
    SPELL_DH_CHAOTIC_TRANSFORMATION                = 388112,
    SPELL_DH_CHARRED_WARBLADES_HEAL                = 213011,
    SPELL_DH_COLLECTIVE_ANGUISH                    = 390152,
    SPELL_DH_COLLECTIVE_ANGUISH_EYE_BEAM           = 391057,
    SPELL_DH_COLLECTIVE_ANGUISH_EYE_BEAM_DAMAGE    = 391058,
    SPELL_DH_COLLECTIVE_ANGUISH_FEL_DEVASTATION    = 393831,
    SPELL_DH_CONSUME_ENERGIZE                      = 1261710,
    SPELL_DH_CONSUME_SOUL_SPAWN                    = 1223448,
    SPELL_DH_VOID_METAMORPHOSIS_PASSIVE            = 471306,
    SPELL_DH_VOID_METAMORPHOSIS_COUNTER            = 1225789,
    SPELL_DH_VOID_METAMORPHOSIS_ACTIVE             = 1217605,
    SPELL_DH_VOID_METAMORPHOSIS_BUFF               = 1217607,
    SPELL_DH_CAN_METAMORPHOSIS                     = 1213809,
    SPELL_DH_VOID_RAY                              = 473728,
    SPELL_DH_CONSUME_SOUL_DEVOURER                 = 1223423,
    SPELL_DH_CONSUME_SOUL_HAVOC_DEMON              = 228556,
    SPELL_DH_CONSUME_SOUL_HAVOC_LESSER             = 228542,
    SPELL_DH_CONSUME_SOUL_HAVOC_SHATTERED          = 228540,
    SPELL_DH_CONSUME_SOUL_HEAL                     = 203794,
    SPELL_DH_CONSUME_SOUL_VENGEANCE_DEMON          = 210050,
    SPELL_DH_CONSUME_SOUL_VENGEANCE_LESSER         = 208014,
    SPELL_DH_CONSUME_SOUL_VENGEANCE_SHATTERED      = 210047,
    SPELL_DH_CULL_DAMAGE                           = 1245455,
    SPELL_DH_CYCLE_OF_HATRED_TALENT                = 258887,
    SPELL_DH_CYCLE_OF_HATRED_COOLDOWN_REDUCTION    = 1214887,
    SPELL_DH_CYCLE_OF_HATRED_REMOVE_STACKS         = 1214890,
    SPELL_DH_DARKGLARE_BOON                        = 389708,
    SPELL_DH_DARKGLARE_BOON_ENERGIZE               = 391345,
    SPELL_DH_DARKNESS_ABSORB                       = 209426,
    SPELL_DH_DEFLECTING_SPIKES                     = 321028,
    SPELL_DH_DEMON_BLADES_DMG                      = 203796,
    SPELL_DH_DEMON_MUZZLE_TALENT                   = 388111,
    SPELL_DH_DEMON_MUZZLE_PROC                     = 394933,
    SPELL_DH_DEMON_SPIKES                          = 203819,
    SPELL_DH_DEMON_SPIKES_TRIGGER                  = 203720,
    SPELL_DH_DEMONIC                               = 213410,
    SPELL_DH_DEMONIC_APPETITE                      = 206478,
    SPELL_DH_DEMONIC_APPETITE_ENERGIZE             = 210041,
    SPELL_DH_DEMONIC_ORIGINS                       = 235893,
    SPELL_DH_DEMONIC_ORIGINS_BUFF                  = 235894,
    SPELL_DH_DEMONIC_TRAMPLE_DMG                   = 208645,
    SPELL_DH_DEMONIC_TRAMPLE_STUN                  = 213491,
    SPELL_DH_DEMONS_BITE                           = 162243,
    SPELL_DH_ELYSIAN_DECREE                        = 306830,
    SPELL_DH_ELYSIAN_DECREE_AOE                    = 307046,
    SPELL_DH_ENDURING_TORMENT_BUFF                 = 453314,
    SPELL_DH_ERADICATE_DAMAGE                      = 1225827,
    SPELL_DH_ERADICATE_DAMAGE_METAMORPHOSIS        = 1279200,
    SPELL_DH_ESSENCE_BREAK_DEBUFF                  = 320338,
    SPELL_DH_EYE_BEAM                              = 198013,
    SPELL_DH_EYE_BEAM_DAMAGE                       = 198030,
    SPELL_DH_EYE_OF_LEOTHERAS_DMG                  = 206650,
    SPELL_DH_FEAST_OF_SOULS                        = 207697,
    SPELL_DH_FEAST_OF_SOULS_PERIODIC_HEAL          = 207693,
    SPELL_DH_FEED_THE_DEMON                        = 218612,
    SPELL_DH_FEL_BARRAGE                           = 211053,
    SPELL_DH_FEL_BARRAGE_DMG                       = 211052,
    SPELL_DH_FEL_BARRAGE_PROC                      = 222703,
    SPELL_DH_FEL_DEVASTATION                       = 212084,
    SPELL_DH_FEL_DEVASTATION_DMG                   = 212105,
    SPELL_DH_FEL_DEVASTATION_HEAL                  = 212106,
    SPELL_DH_FEL_FLAME_FORTIFICATION_TALENT        = 389705,
    SPELL_DH_FEL_FLAME_FORTIFICATION_MOD_DAMAGE    = 393009,
    SPELL_DH_FEL_RUSH                              = 195072,
    SPELL_DH_FEL_RUSH_DMG                          = 192611,
    SPELL_DH_FEL_RUSH_GROUND                       = 197922,
    SPELL_DH_FEL_RUSH_WATER_AIR                    = 197923,
    SPELL_DH_FELBLADE                              = 232893,
    SPELL_DH_FELBLADE_CHARGE                       = 213241,
    SPELL_DH_FELBLADE_COOLDOWN_RESET_PROC_HAVOC    = 236167,
    SPELL_DH_FELBLADE_COOLDOWN_RESET_PROC_VENGEANCE= 203557,
    SPELL_DH_FELBLADE_COOLDOWN_RESET_PROC_VISUAL   = 204497,
    SPELL_DH_FELBLADE_DAMAGE                       = 213243,
    SPELL_DH_FIERY_BRAND                           = 204021,
    SPELL_DH_FIERY_BRAND_RANK_2                    = 320962,
    SPELL_DH_FIERY_BRAND_DEBUFF_RANK_1             = 207744,
    SPELL_DH_FIERY_BRAND_DEBUFF_RANK_2             = 207771,
    SPELL_DH_FIRST_BLOOD                           = 206416,
    SPELL_DH_FLAME_CRASH                           = 227322,
    SPELL_DH_FRAILTY                               = 224509,
    SPELL_DH_FURIOUS_GAZE                          = 343311,
    SPELL_DH_FURIOUS_GAZE_BUFF                     = 343312,
    SPELL_DH_FURIOUS_THROWS                        = 393029,
    SPELL_DH_GLAIVE_TEMPEST                        = 342857,
    SPELL_DH_GLIDE                                 = 131347,
    SPELL_DH_GLIDE_DURATION                        = 197154,
    SPELL_DH_GLIDE_KNOCKBACK                       = 196353,
    SPELL_DH_HAVOC_MASTERY                         = 185164,
    SPELL_DH_ILLIDANS_GRASP                        = 205630,
    SPELL_DH_ILLIDANS_GRASP_DAMAGE                 = 208618,
    SPELL_DH_ILLIDANS_GRASP_JUMP_DEST              = 208175,
    SPELL_DH_IMMOLATION_AURA                       = 258920,
    SPELL_DH_INNER_DEMON_BUFF                      = 390145,
    SPELL_DH_INNER_DEMON_DAMAGE                    = 390137,
    SPELL_DH_INNER_DEMON_TALENT                    = 389693,
    SPELL_DH_INFERNAL_STRIKE_CAST                  = 189110,
    SPELL_DH_INFERNAL_STRIKE_IMPACT_DAMAGE         = 189112,
    SPELL_DH_INFERNAL_STRIKE_JUMP                  = 189111,
    SPELL_DH_JAGGED_SPIKES                         = 205627,
    SPELL_DH_JAGGED_SPIKES_DMG                     = 208790,
    SPELL_DH_JAGGED_SPIKES_PROC                    = 208796,
    SPELL_DH_MANA_RIFT_DMG_POWER_BURN              = 235904,
    SPELL_DH_METAMORPHOSIS                         = 191428,
    SPELL_DH_METAMORPHOSIS_DUMMY                   = 191427,
    SPELL_DH_METAMORPHOSIS_IMPACT_DAMAGE           = 200166,
    SPELL_DH_METAMORPHOSIS_RESET                   = 320645,
    SPELL_DH_METAMORPHOSIS_TRANSFORM               = 162264,
    SPELL_DH_METAMORPHOSIS_VENGEANCE_TRANSFORM     = 187827,
    SPELL_DH_MOMENTUM_AURA                         = 206476,
    SPELL_DH_MOMENTUM                              = 208628,
    SPELL_DH_MOMENT_OF_CRAVING_TALENT              = 1238488,
    SPELL_DH_MOMENT_OF_CRAVING_BUFF                = 1238495,
    SPELL_DH_MONSTER_RISING_AGILITY                = 452550,
    SPELL_DH_NEMESIS_ABERRATIONS                   = 208607,
    SPELL_DH_NEMESIS_BEASTS                        = 208608,
    SPELL_DH_NEMESIS_CRITTERS                      = 208609,
    SPELL_DH_NEMESIS_DEMONS                        = 208608,
    SPELL_DH_NEMESIS_DRAGONKIN                     = 208610,
    SPELL_DH_NEMESIS_ELEMENTALS                    = 208611,
    SPELL_DH_NEMESIS_GIANTS                        = 208612,
    SPELL_DH_NEMESIS_HUMANOIDS                     = 208605,
    SPELL_DH_NEMESIS_MECHANICALS                   = 208613,
    SPELL_DH_NEMESIS_UNDEAD                        = 208614,
    SPELL_DH_PAINBRINGER_DUMMY                     = 225413,
    SPELL_DH_PAINBRINGER_STACK                     = 212988,
    SPELL_DH_RAIN_FROM_ABOVE                       = 206803,
    SPELL_DH_RAIN_OF_CHAOS                         = 205628,
    SPELL_DH_RAIN_OF_CHAOS_IMPACT                  = 232538,
    SPELL_DH_RAZOR_SPIKES                          = 210003,
    SPELL_DH_REAP                                  = 1226019,
    SPELL_DH_REAP_DAMAGE                           = 1225823,
    SPELL_DH_REPEAT_DECREE_CONDUIT                 = 339895,
    SPELL_DH_RESTLESS_HUNTER_TALENT                = 390142,
    SPELL_DH_RESTLESS_HUNTER_BUFF                  = 390212,
    SPELL_DH_RETALIATION_TALENT                    = 389729,
    SPELL_DH_RETALIATION_PROC                      = 391160,
    SPELL_DH_SEVER                                 = 235964,
    SPELL_DH_SHATTERED_RESTORATION                 = 389824,
    SPELL_DH_SHATTER_SOUL                          = 210038,
    SPELL_DH_SHATTER_SOUL_VENGEANCE_FRONT_RIGHT    = 209980,
    SPELL_DH_SHATTER_SOUL_VENGEANCE_BACK_RIGHT     = 209981,
    SPELL_DH_SHATTERED_SOUL                        = 226258,
    SPELL_DH_SHATTERED_SOULS_V_DEMON_TRIGGER       = 226264,
    SPELL_DH_SHATTERED_SOULS_V_SHATTERED_TRIGGER   = 226263,
    SPELL_DH_SHATTERED_SOUL_DEVOURER_LESSER_RIGHT  = 1223445,
    SPELL_DH_SHATTERED_SOUL_DEVOURER_LESSER_LEFT   = 1223448,
    SPELL_DH_SHATTERED_SOUL_LESSER_RIGHT           = 228533,
    SPELL_DH_SHATTERED_SOUL_LESSER_LEFT            = 237867,
    SPELL_DH_SHATTERED_SOULS_DEVOURER              = 1227619,
    SPELL_DH_SHATTERED_SOULS_DEVOURER_DUMMY        = 1223450,
    SPELL_DH_SHATTERED_SOULS_HAVOC                 = 209651,
    SPELL_DH_SHATTERED_SOULS_HAVOC_DEMON_TRIGGER   = 226370,
    SPELL_DH_SHATTERED_SOULS_HAVOC_LESSER_TRIGGER  = 228536,
    SPELL_DH_SHATTERED_SOULS_HAVOC_SHATTERED_TRIGGER = 209687,
    SPELL_DH_SHATTERED_SOULS_MARKER                = 221461,
    SPELL_DH_SHATTERED_SOULS_VENGEANCE             = 204254,
    SPELL_DH_SHEAR                                 = 203782,
    SPELL_DH_SHEAR_PASSIVE                         = 203783,
    SPELL_DH_SHIFT_VISUAL_DEST                     = 1234818,
    SPELL_DH_SHIFT_CHARGE                          = 1242880,
    SPELL_DH_SIGIL_OF_CHAINS                       = 202138,
    SPELL_DH_SIGIL_OF_CHAINS_GRIP                  = 208674,
    SPELL_DH_SIGIL_OF_CHAINS_JUMP                  = 208674,
    SPELL_DH_SIGIL_OF_CHAINS_SLOW                  = 204843,
    SPELL_DH_SIGIL_OF_CHAINS_SNARE                 = 204843,
    SPELL_DH_SIGIL_OF_CHAINS_TARGET_SELECT         = 204834,
    SPELL_DH_SIGIL_OF_CHAINS_VISUAL                = 208673,
    SPELL_DH_SIGIL_OF_FLAME                        = 204596,
    SPELL_DH_SIGIL_OF_FLAME_AOE                    = 204598,
    SPELL_DH_SIGIL_OF_FLAME_ENERGIZE               = 389787,
    SPELL_DH_SIGIL_OF_FLAME_FLAME_CRASH            = 228973,
    SPELL_DH_SIGIL_OF_FLAME_VISUAL                 = 208710,
    SPELL_DH_SIGIL_OF_MISERY                       = 207684,
    SPELL_DH_SIGIL_OF_MISERY_AOE                   = 207685,
    SPELL_DH_SIGIL_OF_SILENCE                      = 202137,
    SPELL_DH_SIGIL_OF_SILENCE_AOE                  = 204490,
    SPELL_DH_SIGIL_OF_SPITE                        = 390163,
    SPELL_DH_SIGIL_OF_SPITE_AOE                    = 389860,
    SPELL_DH_SOULMONGER_ABSORB                     = 391234,
    SPELL_DH_SOUL_BARRIER                          = 227225,
    SPELL_DH_SOUL_CLEAVE                           = 228477,
    SPELL_DH_SOUL_CLEAVE_DMG                       = 228478,
    SPELL_DH_SOUL_FRAGMENT_COUNTER                 = 203981,
    SPELL_DH_SOUL_FRAGMENT_DEVOURER                = 1223412,
    SPELL_DH_SOUL_FRAGMENTS_DEVOURER_COUNTER       = 1245577,
    SPELL_DH_SOUL_FRAGMENTS_DAMAGE_TAKEN_TRACKER   = 210788,
    SPELL_DH_SOUL_FURNACE_DAMAGE_BUFF              = 391172,
    SPELL_DH_SOUL_RENDING                          = 204909,
    SPELL_DH_SPIRIT_BOMB_DAMAGE                    = 218677,
    SPELL_DH_SPIRIT_BOMB_HEAL                      = 227255,
    SPELL_DH_SPIRIT_BOMB_VISUAL                    = 218678,
    SPELL_DH_STUDENT_OF_SUFFERING_TALENT           = 452412,
    SPELL_DH_STUDENT_OF_SUFFERING_AURA             = 453239,
    SPELL_DH_TACTICAL_RETREAT_ENERGIZE             = 389890,
    SPELL_DH_TACTICAL_RETREAT_TALENT               = 389688,
    SPELL_DH_THROW_GLAIVE                          = 185123,
    SPELL_DH_UNCONTAINED_FEL                       = 209261,
    SPELL_DH_VENGEANCE_DEMON_HUNTER                = 212613,
    SPELL_DH_VENGEFUL_BONDS                        = 320635,
    SPELL_DH_VENGEFUL_RETREAT                      = 198813,
    SPELL_DH_VENGEFUL_RETREAT_TRIGGER              = 198793,
    SPELL_DH_VOIDBLADE_CHARGE                      = 1241285,
    SPELL_DH_VOIDBLADE_DAMAGE                      = 1245414,
    SPELL_DH_VOIDGLARE_BOON_ENERGIZE               = 1241922,
    SPELL_DH_VOIDGLARE_BOON_TALENT                 = 1240202,
    SPELL_DH_VOID_RAY_DAMAGE                       = 1213649,
    SPELL_DH_WAVE_OF_DEBILITATION_TALENT           = 452403,
    SPELL_DH_WAVE_OF_DEBILITATION_SLOW             = 453263,

    SPELL_DH_FEL_RUSH_DAMAGE                       = 223107,
    SPELL_DH_FEL_MASTERY_FURY                      = 234244,
    SPELL_DH_FEL_MASTERY                           = 192939,
    SPELL_DH_FEL_ERUPTION_DAMAGE                   = 225102,
    SPELL_DH_DEMON_REBORN                          = 193897,
    SPELL_DH_BLOODLET_DOT                          = 207690,
    SPELL_DH_INFERNAL_STRIKE_VISUAL                = 208461,
    SPELL_DH_SHATTERED_SOULS_DEMON                 = 204256,
    SPELL_DH_LESSER_SOUL_SHARD                     = 203795,
    SPELL_DH_SOUL_FRAGMENT_HEAL_VENGEANCE          = 210042,
    SPELL_DH_SOUL_FRAGMENT_DEMON_BONUS             = 163073,
    SPELL_DH_PAINBRINGER                           = 207387,
    SPELL_DH_PAINBRINGER_BUFF                      = 212988,
    SPELL_DH_GLUTTONY_BUFF                         = 227330,
    SPELL_DH_NETHER_BOND                           = 207810,
    SPELL_DH_NETHER_BOND_DAMAGE                    = 207812,
    SPELL_DH_NETHER_BOND_PERIODIC                  = 207811,
    SPELL_DH_SOLITUDE_BUFF                         = 211510,
    SPELL_DH_CLEANSED_BY_FLAME                     = 205625,
    SPELL_DH_CLEANSED_BY_FLAME_DISPEL              = 208770,
    SPELL_DH_FALLOUT                               = 227174,
    SPELL_DH_CHARRED_FLESH                         = 336639,
    SPELL_DH_SHATTER_THE_SOULS                     = 212827,
    SPELL_DH_SHEAR_PROC                            = 203783,
    SPELL_DH_COVER_OF_DARKNESS                     = 227635,
    SPELL_DH_FEAST_ON_THE_SOULS                    = 201468,
    SPELL_DH_CHAOS_CLEAVE_PROC                     = 236237,
    SPELL_DK_RAIN_FROM_ABOVE_SLOWFALL              = 206804,
    SPELL_DH_REWARD_PAIN                           = 187724,
    SPELL_DH_BLADE_DANCE                           = 188499,
    SPELL_DH_MANA_RIFT_SPELL                       = 235903,
    SPELL_DH_SOUL_FRAGMENT_HEAL_25_HAVOC           = 178963,
    SPELL_DH_BALANCED_BLADES                       = 201470,
    SPELL_DH_REAP_FURY                             = 1261679,
    SPELL_DH_CULL                                  = 1245453,
    SPELL_DH_DEVOUR                                = 1217610,
    SPELL_DH_ERADICATE_PASSIVE                     = 1226033,
    SPELL_DH_ERADICATE                             = 1225826,
    SPELL_DH_ERADICATE_DAMAGE_META                 = 1279200,
    SPELL_DH_ERADICATE_OVERRIDE                    = 1239524,
    SPELL_DH_COLLAPSING_STAR_PASSIVE               = 1221167,
    SPELL_DH_COLLAPSING_STAR_COUNTER               = 1227702,
    SPELL_DH_COLLAPSING_STAR                       = 1221150,
    SPELL_DH_COLLAPSING_STAR_DAMAGE                = 1221162,
    SPELL_DH_COLLAPSING_STAR_OVERRIDE              = 1221171,
    SPELL_DH_COLLAPSING_STAR_FRAGMENTS             = 1240204,
    SPELL_DH_FEAST_OF_SOULS_PASSIVE                = 1237270,
    SPELL_DH_FEAST_OF_SOULS_BUFF                   = 1232310,
    SPELL_DH_ENTROPY                               = 1261684,
    SPELL_DH_HUNGERING_SLASH_PASSIVE               = 1239519,
    SPELL_DH_HUNGERING_SLASH_OVERRIDE              = 1239525,
    SPELL_DH_HUNGERING_SLASH                       = 1239123,
    SPELL_DH_HUNGERING_SLASH_DAMAGE                = 1239127,
    SPELL_DH_HUNGERING_SLASH_FURY                  = 1239507,
    SPELL_DH_HUNGERING_SLASH_VOIDSTEP              = 1223157,
    SPELL_DH_THE_HUNT_DAMAGE                       = 1246169,
    SPELL_DH_VOIDSTEP_DAMAGE                       = 1239526,
    SPELL_DH_VENGEFUL_RETREAT_DAMAGE               = 198813,
    SPELL_DH_EMPTINESS_PASSIVE                     = 1242492,
    SPELL_DH_EMPTINESS_BUFF                        = 1242504,
    SPELL_DH_ROLLING_TORMENT_PASSIVE               = 1244237,
    SPELL_DH_ROLLING_TORMENT_BUFF                  = 1244235,
    SPELL_DH_SOUL_IMMOLATION                       = 1241937,
    SPELL_DH_SOUL_IMMOLATION_FURY                  = 1242475,
    SPELL_DH_SPONTANEOUS_IMMOLATION                = 1246556,
    SPELL_DH_SPONTANEOUS_IMMOLATION_BUFF           = 1266696,
    SPELL_DH_VOIDFALL_PASSIVE                      = 1253304,
    SPELL_DH_VOIDFALL_STACK                        = 1256301,
    SPELL_DH_VOIDFALL_METEOR_VENGEANCE             = 1256303,
    SPELL_DH_VOIDFALL_METEOR_HAVOC                 = 1256304,
    SPELL_DH_WORLD_KILLER_PASSIVE                  = 1256353,
    SPELL_DH_WORLD_KILLER_METEOR_VENGEANCE         = 1256616,
    SPELL_DH_WORLD_KILLER_METEOR_HAVOC             = 1256618,
    SPELL_DH_CATASTROPHE_PASSIVE                   = 1253769,
    SPELL_DH_CATASTROPHE_DOT                       = 1256667,
    SPELL_DH_METEORIC_RISE_PASSIVE                 = 1253377,
    SPELL_DH_MASS_ACCELERATION_PASSIVE             = 1256295,
    SPELL_DH_METEORIC_FALL_PASSIVE                 = 1253391,
    SPELL_DH_FINAL_HOUR_PASSIVE                    = 1253805,
    SPELL_DH_FINAL_HOUR_BUFF                       = 1256322,
    SPELL_DH_DOOMSAYER_PASSIVE                     = 1253676,
    SPELL_DH_DOOMSAYER_BUFF                        = 1265768,
    SPELL_DH_DARK_MATTER_PASSIVE                   = 1256307,
    SPELL_DH_METEOR_SHOWER_DEVOURER                = 1264126,
    SPELL_DH_METEOR_SHOWER_VENGEANCE               = 1264128,
    SPELL_DH_MIDNIGHT_PASSIVE                      = 1250088,
    SPELL_DH_MIDNIGHT_SOUL_ERUPT                   = 1250094,
    SPELL_DH_DEVOURER_SPEC                         = 1213636,
    SPELL_DH_VENGEANCE_SPEC                        = 212613,
    SPELL_DH_HAVOC_SPEC                            = 212612,
    SPELL_DH_FIRST_IN_LAST_OUT                     = 1266619,
    SPELL_DH_SHIFT                                 = 1234796,
};

enum DemonHunterSpellCategories
{
    SPELL_CATEGORY_DH_EYE_BEAM                     = 1582,
    SPELL_CATEGORY_DH_BLADE_DANCE                  = 1640,
    SPELL_CATEGORY_DH_VOIDBLADE                    = 2426
};

// Called by 232893 - Felblade
class spell_dh_army_unto_oneself : public SpellScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_DH_ARMY_UNTO_ONESELF, SPELL_DH_BLADE_WARD });
    }

    bool Load() override
    {
        return GetCaster()->HasAura(SPELL_DH_ARMY_UNTO_ONESELF);
    }

    void ApplyBladeWard() const
    {
        GetCaster()->CastSpell(GetCaster(), SPELL_DH_BLADE_WARD, CastSpellExtraArgsInit{
            .TriggerFlags = TRIGGERED_IGNORE_CAST_IN_PROGRESS | TRIGGERED_DONT_REPORT_CAST_ERROR,
            .TriggeringSpell = GetSpell()
        });
    }

    void Register() override
    {
        AfterCast += SpellCastFn(spell_dh_army_unto_oneself::ApplyBladeWard);
    }
};

// Called by 203819 - Demon Spikes
class spell_dh_calcified_spikes : public AuraScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_DH_CALCIFIED_SPIKES_TALENT, SPELL_DH_CALCIFIED_SPIKES_MOD_DAMAGE });
    }

    bool Load() override
    {
        return GetUnitOwner()->HasAura(SPELL_DH_CALCIFIED_SPIKES_TALENT);
    }

    void HandleAfterRemove(AuraEffect const* aurEff, AuraEffectHandleModes /*mode*/) const
    {
        Unit* target = GetTarget();
        target->CastSpell(target, SPELL_DH_CALCIFIED_SPIKES_MOD_DAMAGE, CastSpellExtraArgsInit{
            .TriggerFlags = TRIGGERED_IGNORE_CAST_IN_PROGRESS | TRIGGERED_DONT_REPORT_CAST_ERROR,
            .TriggeringAura = aurEff
        });
    }

    void Register() override
    {
        AfterEffectRemove += AuraEffectRemoveFn(spell_dh_calcified_spikes::HandleAfterRemove, EFFECT_1, SPELL_AURA_MOD_ARMOR_PCT_FROM_STAT, AURA_EFFECT_HANDLE_REAL_OR_REAPPLY_MASK);
    }
};

// 391171 - Calcified Spikes
class spell_dh_calcified_spikes_periodic : public AuraScript
{
    bool Validate(SpellInfo const* spellInfo) override
    {
        return ValidateSpellEffect({ { spellInfo->Id, EFFECT_1 } });
    }

    void HandlePeriodic(AuraEffect const* /*aurEff*/) const
    {
        if (AuraEffect* damagePctTaken = GetEffect(EFFECT_0))
            damagePctTaken->ChangeAmount(damagePctTaken->GetAmount() + 1);
    }

    void Register() override
    {
        OnEffectPeriodic += AuraEffectPeriodicFn(spell_dh_calcified_spikes_periodic::HandlePeriodic, EFFECT_1, SPELL_AURA_PERIODIC_TRIGGER_SPELL);
    }
};

// 197125 - Chaos Strike
class spell_dh_chaos_strike : public AuraScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_DH_CHAOS_STRIKE_ENERGIZE });
    }

    void HandleEffectProc(AuraEffect const* /*aurEff*/, ProcEventInfo const& eventInfo)
    {
        PreventDefaultAction();
        GetTarget()->CastSpell(GetTarget(), SPELL_DH_CHAOS_STRIKE_ENERGIZE, CastSpellExtraArgsInit{
            .TriggerFlags = TRIGGERED_IGNORE_CAST_IN_PROGRESS | TRIGGERED_DONT_REPORT_CAST_ERROR,
            .TriggeringSpell = eventInfo.GetProcSpell()
        });
    }

    void Register() override
    {
        OnEffectProc += AuraEffectProcFn(spell_dh_chaos_strike::HandleEffectProc, EFFECT_0, SPELL_AURA_PROC_TRIGGER_SPELL);
    }
};

// 344862 - Chaos Strike
class spell_dh_chaos_strike_initial : public SpellScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_DH_CHAOS_STRIKE });
    }

    void HandleHit(SpellEffIndex /*effIndex*/) const
    {
        GetCaster()->CastSpell(GetHitUnit(), SPELL_DH_CHAOS_STRIKE, CastSpellExtraArgsInit{
            .TriggerFlags = TRIGGERED_IGNORE_GCD | TRIGGERED_IGNORE_POWER_COST | TRIGGERED_IGNORE_CAST_IN_PROGRESS | TRIGGERED_DONT_REPORT_CAST_ERROR,
            .TriggeringSpell = GetSpell()
        });
    }

    void Register() override
    {
        OnEffectHitTarget += SpellEffectFn(spell_dh_chaos_strike_initial::HandleHit, EFFECT_0, SPELL_EFFECT_DUMMY);
    }
};

// Called by 188499 - Blade Dance and 210152 - Death Sweep
class spell_dh_chaos_theory : public SpellScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        if (!ValidateSpellInfo({ SPELL_DH_CHAOS_THEORY_CRIT })
            || !ValidateSpellEffect({ { SPELL_DH_CHAOS_THEORY_TALENT, EFFECT_1 } }))
            return false;

        SpellInfo const* chaosTheory = sSpellMgr->AssertSpellInfo(SPELL_DH_CHAOS_THEORY_TALENT, DIFFICULTY_NONE);
        return chaosTheory->GetEffect(EFFECT_0).CalcValue() < chaosTheory->GetEffect(EFFECT_1).CalcValue();
    }

    bool Load() override
    {
        return GetCaster()->HasAura(SPELL_DH_CHAOS_THEORY_TALENT);
    }

    void ChaosTheory() const
    {
        Unit* caster = GetCaster();
        Aura const* chaosTheory = caster->GetAura(SPELL_DH_CHAOS_THEORY_TALENT);
        if (!chaosTheory)
            return;

        AuraEffect const* min = chaosTheory->GetEffect(EFFECT_0);
        AuraEffect const* max = chaosTheory->GetEffect(EFFECT_1);
        if (!min || !max)
            return;

        SpellEffectValue critChance = frand(min->GetAmount(), max->GetAmount());
        caster->CastSpell(caster, SPELL_DH_CHAOS_THEORY_CRIT, CastSpellExtraArgsInit{
            .TriggerFlags = TRIGGERED_IGNORE_CAST_IN_PROGRESS | TRIGGERED_DONT_REPORT_CAST_ERROR,
            .SpellValueOverrides = { { SPELLVALUE_BASE_POINT0, critChance } }
        });
    }

    void Register() override
    {
        AfterCast += SpellCastFn(spell_dh_chaos_theory::ChaosTheory);
    }
};

// 390195 - Chaos Theory
class spell_dh_chaos_theory_drop_charge : public AuraScript
{
    void Prepare(ProcEventInfo const& /*eventInfo*/)
    {
        PreventDefaultAction();
        // delayed charge drop - this aura must be removed after Chaos Strike does damage and after it procs power refund
        GetAura()->DropChargeDelayed(500);
    }

    void Register() override
    {
        DoPrepareProc += AuraProcFn(spell_dh_chaos_theory_drop_charge::Prepare);
    }
};

// Called by 191427 - Metamorphosis
class spell_dh_chaotic_transformation : public SpellScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_DH_CHAOTIC_TRANSFORMATION })
            && sSpellCategoryStore.LookupEntry(SPELL_CATEGORY_DH_EYE_BEAM)
            && sSpellCategoryStore.LookupEntry(SPELL_CATEGORY_DH_BLADE_DANCE);
    }

    bool Load() override
    {
        return GetCaster()->HasAura(SPELL_DH_CHAOTIC_TRANSFORMATION);
    }

    void HandleCooldown() const
    {
        GetCaster()->GetSpellHistory()->ResetCooldowns([](SpellHistory::CooldownEntry const& cooldown)
        {
            uint32 category = sSpellMgr->AssertSpellInfo(cooldown.SpellId, DIFFICULTY_NONE)->CategoryId;
            return category == SPELL_CATEGORY_DH_EYE_BEAM || category == SPELL_CATEGORY_DH_BLADE_DANCE;
        }, true);
    }

    void Register() override
    {
        AfterCast += SpellCastFn(spell_dh_chaotic_transformation::HandleCooldown);
    }
};

// 213010 - Charred Warblades
class spell_dh_charred_warblades : public AuraScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_DH_CHARRED_WARBLADES_HEAL });
    }

    bool CheckProc(ProcEventInfo& eventInfo)
    {
        return eventInfo.GetDamageInfo() && eventInfo.GetDamageInfo()->GetSchoolMask() & SPELL_SCHOOL_MASK_FIRE;
    }

    void HandleAfterProc(ProcEventInfo& eventInfo)
    {
        _healAmount += CalculatePct(eventInfo.GetDamageInfo()->GetDamage(), GetEffect(EFFECT_0)->GetAmount());
    }

    void HandleDummyTick(AuraEffect const* aurEff)
    {
        if (_healAmount == 0)
            return;

        GetTarget()->CastSpell(GetTarget(), SPELL_DH_CHARRED_WARBLADES_HEAL,
            CastSpellExtraArgs(TRIGGERED_IGNORE_CAST_IN_PROGRESS | TRIGGERED_DONT_REPORT_CAST_ERROR)
            .SetTriggeringAura(aurEff)
            .AddSpellBP0(_healAmount));

        _healAmount = 0;
    }

    void Register() override
    {
        DoCheckProc += AuraCheckProcFn(spell_dh_charred_warblades::CheckProc);
        AfterProc += AuraProcFn(spell_dh_charred_warblades::HandleAfterProc);
        OnEffectPeriodic += AuraEffectPeriodicFn(spell_dh_charred_warblades::HandleDummyTick, EFFECT_0, SPELL_AURA_PERIODIC_DUMMY);
    }

private:
    uint32 _healAmount = 0;
};

// Called by 212084 - Fel Devastation and 198013 - Eye Beam
class spell_dh_collective_anguish : public SpellScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_DH_COLLECTIVE_ANGUISH, SPELL_DH_FEL_DEVASTATION, SPELL_DH_COLLECTIVE_ANGUISH_EYE_BEAM, SPELL_DH_COLLECTIVE_ANGUISH_FEL_DEVASTATION });
    }

    bool Load() override
    {
        return GetCaster()->HasAura(SPELL_DH_COLLECTIVE_ANGUISH);
    }

    void HandleEyeBeam() const
    {
        GetCaster()->CastSpell(GetCaster(), SPELL_DH_COLLECTIVE_ANGUISH_EYE_BEAM, CastSpellExtraArgsInit{
            .TriggerFlags = TRIGGERED_IGNORE_CAST_IN_PROGRESS | TRIGGERED_DONT_REPORT_CAST_ERROR,
            .TriggeringSpell = GetSpell()
        });
    }

    void HandleFelDevastation() const
    {
        GetCaster()->CastSpell(GetCaster(), SPELL_DH_COLLECTIVE_ANGUISH_FEL_DEVASTATION, CastSpellExtraArgsInit{
            .TriggerFlags = TRIGGERED_IGNORE_CAST_IN_PROGRESS | TRIGGERED_DONT_REPORT_CAST_ERROR,
            .TriggeringSpell = GetSpell()
        });
    }

    void Register() override
    {
        if (m_scriptSpellId == SPELL_DH_FEL_DEVASTATION)
            AfterCast += SpellCastFn(spell_dh_collective_anguish::HandleEyeBeam);
        else
            AfterCast += SpellCastFn(spell_dh_collective_anguish::HandleFelDevastation);
    }
};

// 391057 - Eye Beam
class spell_dh_collective_anguish_eye_beam : public AuraScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_DH_COLLECTIVE_ANGUISH_EYE_BEAM_DAMAGE });
    }

    void HandleEffectPeriodic(AuraEffect const* aurEff) const
    {
        if (Unit* caster = GetCaster())
            caster->CastSpell(nullptr, SPELL_DH_COLLECTIVE_ANGUISH_EYE_BEAM_DAMAGE, CastSpellExtraArgsInit{
                .TriggerFlags = TRIGGERED_IGNORE_CAST_IN_PROGRESS | TRIGGERED_DONT_REPORT_CAST_ERROR,
                .TriggeringAura = aurEff
            });
    }

    void Register() override
    {
        OnEffectPeriodic += AuraEffectPeriodicFn(spell_dh_collective_anguish_eye_beam::HandleEffectPeriodic, EFFECT_0, SPELL_AURA_PERIODIC_TRIGGER_SPELL);
    }
};

// 473662 - Consume
class spell_dh_consume_energize : public SpellScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_DH_CONSUME_ENERGIZE, SPELL_DH_CONSUME_SOUL_SPAWN, SPELL_DH_SHATTERED_SOUL_DEVOURER_LESSER_RIGHT,
            SPELL_DH_VOIDFALL_STACK, SPELL_DH_VOIDFALL_METEOR_VENGEANCE, SPELL_DH_VOIDFALL_METEOR_HAVOC });
    }

    void HandleAfterCast() const
    {
        Unit* caster = GetCaster();
        Unit* target = GetExplTargetUnit();

        caster->CastSpell(caster, SPELL_DH_CONSUME_ENERGIZE, CastSpellExtraArgsInit{
            .TriggerFlags = TRIGGERED_IGNORE_CAST_IN_PROGRESS | TRIGGERED_DONT_REPORT_CAST_ERROR,
            .TriggeringSpell = GetSpell()
        });

        if (target)
        {
            // 1223448 - lesser soul fragment (left)
            target->CastSpell(caster, SPELL_DH_CONSUME_SOUL_SPAWN, CastSpellExtraArgsInit{
                .TriggerFlags = TRIGGERED_IGNORE_CAST_IN_PROGRESS | TRIGGERED_DONT_REPORT_CAST_ERROR,
                .TriggeringSpell = GetSpell()
                });
            // 1223445 - lesser soul fragment (right)
            target->CastSpell(caster, SPELL_DH_SHATTERED_SOUL_DEVOURER_LESSER_RIGHT, CastSpellExtraArgsInit{
                .TriggerFlags = TRIGGERED_IGNORE_CAST_IN_PROGRESS | TRIGGERED_DONT_REPORT_CAST_ERROR,
                .TriggeringSpell = GetSpell()
                });
        }
    }

    void Register() override
    {
        AfterCast += SpellCastFn(spell_dh_consume_energize::HandleAfterCast);
    }
};

// 203794 - Consume Soul
class spell_dh_consume_soul_vengeance_lesser : public SpellScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellEffect({ { SPELL_DH_SOUL_FRAGMENTS_DAMAGE_TAKEN_TRACKER, EFFECT_0 }, { SPELL_DH_SHEAR_PASSIVE, EFFECT_2 } });
    }

    void CalcHealingFromDamageTaken(SpellEffectInfo const& /*effectInfo*/, Unit const* victim, int32& /*healing*/, int32& flatMod, float& /*pctMod*/) const
    {
        AuraEffect* damageTakenTracker = GetCaster()->GetAuraEffect(SPELL_DH_SOUL_FRAGMENTS_DAMAGE_TAKEN_TRACKER, EFFECT_0);
        if (!damageTakenTracker)
            return;

        Aura const* shearPassive = GetCaster()->GetAura(SPELL_DH_SHEAR_PASSIVE);
        if (!shearPassive || !shearPassive->HasEffect(EFFECT_1) || !shearPassive->HasEffect(EFFECT_2))
            return;

        flatMod += std::max<SpellEffectValue>(CalculatePct(damageTakenTracker->CalculateAmount(GetCaster()), shearPassive->GetEffect(EFFECT_1)->GetAmount()),
            victim->CountPctFromMaxHealth(shearPassive->GetEffect(EFFECT_2)->GetAmount()));
    }

    void Register() override
    {
        CalcHealing += SpellCalcHealingFn(spell_dh_consume_soul_vengeance_lesser::CalcHealingFromDamageTaken);
    }
};

// 320413 - Critical Chaos
class spell_dh_critical_chaos : public AuraScript
{
    void CalcAmount(AuraEffect const* /*aurEff*/, SpellEffectValue& amount, bool const& /*canBeRecalculated*/) const
    {
        if (AuraEffect const* amountHolder = GetEffect(EFFECT_1))
        {
            float critChanceDone = GetUnitOwner()->GetUnitCriticalChanceDone(BASE_ATTACK);
            amount = CalculatePct(critChanceDone, amountHolder->GetAmount());
        }
    }

    void UpdatePeriodic(AuraEffect const* aurEff) const
    {
        if (AuraEffect* bonus = GetEffect(EFFECT_0))
            bonus->RecalculateAmount(aurEff);
    }

    void Register() override
    {
        DoEffectCalcAmount += AuraEffectCalcAmountFn(spell_dh_critical_chaos::CalcAmount, EFFECT_0, SPELL_AURA_ADD_FLAT_MODIFIER);
        OnEffectPeriodic += AuraEffectPeriodicFn(spell_dh_critical_chaos::UpdatePeriodic, EFFECT_1, SPELL_AURA_PERIODIC_DUMMY);
    }
};

// 1226019 - Reap
class spell_dh_cull : public SpellScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_DH_CULL_DAMAGE });
    }

    void HandleDamage(SpellEffIndex /*effIndex*/) const
    {
        GetCaster()->CastSpell(GetHitUnit(), SPELL_DH_CULL_DAMAGE, CastSpellExtraArgsInit{
            .TriggerFlags = TRIGGERED_IGNORE_CAST_IN_PROGRESS | TRIGGERED_DONT_REPORT_CAST_ERROR,
            .TriggeringSpell = GetSpell()
        });
    }

    void Register() override
    {
        OnEffectHitTarget += SpellEffectFn(spell_dh_cull::HandleDamage, EFFECT_0, SPELL_EFFECT_DUMMY);
    }
};

// 389718 - Cycle of Binding
class spell_dh_cycle_of_binding : public AuraScript
{
    static constexpr std::array<uint32, 5> SigilSpellsIds = { SPELL_DH_SIGIL_OF_CHAINS, SPELL_DH_SIGIL_OF_FLAME, SPELL_DH_SIGIL_OF_MISERY, SPELL_DH_SIGIL_OF_SILENCE, SPELL_DH_SIGIL_OF_SPITE };

    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo(SigilSpellsIds);
    }

    void HandleEffectProc(AuraEffect const* aurEff, ProcEventInfo const& /*eventInfo*/) const
    {
        SpellHistory* history = GetTarget()->GetSpellHistory();
        SpellHistory::Duration amount = duration_cast<SpellHistory::Duration>(FloatSeconds(-aurEff->GetAmount()));

        for (uint32 spellId : SigilSpellsIds)
            history->ModifyCooldown(spellId, amount);
    }

    void Register() override
    {
        OnEffectProc += AuraEffectProcFn(spell_dh_cycle_of_binding::HandleEffectProc, EFFECT_0, SPELL_AURA_DUMMY);
    }
};

// Called by 198013 - Eye Beam
class spell_dh_cycle_of_hatred : public SpellScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_DH_CYCLE_OF_HATRED_TALENT, SPELL_DH_CYCLE_OF_HATRED_COOLDOWN_REDUCTION, SPELL_DH_CYCLE_OF_HATRED_REMOVE_STACKS });
    }

    bool Load() override
    {
        return GetCaster()->HasAuraEffect(SPELL_DH_CYCLE_OF_HATRED_TALENT, EFFECT_0);
    }

    void HandleCycleOfHatred() const
    {
        Unit* caster = GetCaster();

        // First calculate cooldown then add another stack
        uint32 cycleOfHatredStack = caster->GetAuraCount(SPELL_DH_CYCLE_OF_HATRED_COOLDOWN_REDUCTION);
        AuraEffect const* cycleOfHatred = caster->GetAuraEffect(SPELL_DH_CYCLE_OF_HATRED_TALENT, EFFECT_0);
        caster->GetSpellHistory()->ModifyCooldown(GetSpellInfo(), -Milliseconds(static_cast<int64>(cycleOfHatred->GetAmount() * cycleOfHatredStack)));

        CastSpellExtraArgs args;
        args.SetTriggerFlags(TRIGGERED_IGNORE_CAST_IN_PROGRESS | TRIGGERED_DONT_REPORT_CAST_ERROR);
        args.SetTriggeringSpell(GetSpell());

        caster->CastSpell(caster, SPELL_DH_CYCLE_OF_HATRED_COOLDOWN_REDUCTION, args);
        caster->CastSpell(caster, SPELL_DH_CYCLE_OF_HATRED_REMOVE_STACKS, args);
    }

    void Register() override
    {
        AfterCast += SpellCastFn(spell_dh_cycle_of_hatred::HandleCycleOfHatred);
    }
};

// 1214890 - Cycle of Hatred
class spell_dh_cycle_of_hatred_remove_stacks : public AuraScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_DH_CYCLE_OF_HATRED_COOLDOWN_REDUCTION });
    }

    void OnRemove(AuraEffect const* /*aurEff*/, AuraEffectHandleModes /*mode*/) const
    {
        if (Aura* aura = GetTarget()->GetAura(SPELL_DH_CYCLE_OF_HATRED_COOLDOWN_REDUCTION))
            aura->SetStackAmount(1);
    }

    void Register() override
    {
        AfterEffectRemove += AuraEffectRemoveFn(spell_dh_cycle_of_hatred_remove_stacks::OnRemove, EFFECT_0, SPELL_AURA_DUMMY, AURA_EFFECT_HANDLE_REAL);
    }
};

// 258887 - Cycle of Hatred
class spell_dh_cycle_of_hatred_talent : public AuraScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_DH_CYCLE_OF_HATRED_COOLDOWN_REDUCTION });
    }

    void OnApply(AuraEffect const* /*aurEff*/, AuraEffectHandleModes /*mode*/) const
    {
        Unit* target = GetTarget();
        target->CastSpell(target, SPELL_DH_CYCLE_OF_HATRED_COOLDOWN_REDUCTION, TRIGGERED_IGNORE_CAST_IN_PROGRESS | TRIGGERED_DONT_REPORT_CAST_ERROR);
    }

    void OnRemove(AuraEffect const* /*aurEff*/, AuraEffectHandleModes /*mode*/) const
    {
        GetTarget()->RemoveAurasDueToSpell(SPELL_DH_CYCLE_OF_HATRED_COOLDOWN_REDUCTION);
    }

    void Register() override
    {
        AfterEffectApply += AuraEffectApplyFn(spell_dh_cycle_of_hatred_talent::OnApply, EFFECT_0, SPELL_AURA_DUMMY, AURA_EFFECT_HANDLE_REAL);
        AfterEffectRemove += AuraEffectRemoveFn(spell_dh_cycle_of_hatred_talent::OnRemove, EFFECT_0, SPELL_AURA_DUMMY, AURA_EFFECT_HANDLE_REAL);
    }
};

// Called by 212084 - Fel Devastation
class spell_dh_darkglare_boon : public AuraScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        if (!ValidateSpellInfo({ SPELL_DH_DARKGLARE_BOON_ENERGIZE, SPELL_DH_FEL_DEVASTATION })
            || !ValidateSpellEffect({ { SPELL_DH_DARKGLARE_BOON, EFFECT_3 } }))
            return false;

        SpellInfo const* darkglareBoon = sSpellMgr->GetSpellInfo(SPELL_DH_DARKGLARE_BOON, DIFFICULTY_NONE);
        return darkglareBoon->GetEffect(EFFECT_0).CalcValue() < darkglareBoon->GetEffect(EFFECT_1).CalcValue()
            && darkglareBoon->GetEffect(EFFECT_2).CalcValue() < darkglareBoon->GetEffect(EFFECT_3).CalcValue();
    }

    bool Load() override
    {
        return GetUnitOwner()->HasAura(SPELL_DH_DARKGLARE_BOON);
    }

    void HandleEffectRemove(AuraEffect const* /*aurEff*/, AuraEffectHandleModes /*mode*/) const
    {
        // Tooltip mentions "fully channelled" being a requirement but ingame it always reduces cooldown and energizes, even when manually cancelled
        //if (GetTargetApplication()->GetRemoveMode() != AURA_REMOVE_BY_EXPIRE)
        //    return;

        Unit* target = GetTarget();
        Aura const* darkglareBoon = target->GetAura(SPELL_DH_DARKGLARE_BOON);

        SpellHistory::Duration cooldown, categoryCooldown;
        SpellHistory::GetCooldownDurations(GetSpellInfo(), 0, &cooldown, nullptr, &categoryCooldown);
        SpellEffectValue reductionPct = frand(darkglareBoon->GetEffect(EFFECT_0)->GetAmount(), darkglareBoon->GetEffect(EFFECT_1)->GetAmount());
        SpellHistory::Duration cooldownReduction(CalculatePct(std::max(cooldown, categoryCooldown).count(), reductionPct));

        SpellEffectValue energizeValue = frand(darkglareBoon->GetEffect(EFFECT_2)->GetAmount(), darkglareBoon->GetEffect(EFFECT_3)->GetAmount());

        target->GetSpellHistory()->ModifyCooldown(SPELL_DH_FEL_DEVASTATION, -cooldownReduction);

        target->CastSpell(target, SPELL_DH_DARKGLARE_BOON_ENERGIZE, CastSpellExtraArgsInit{
            .TriggerFlags = TRIGGERED_IGNORE_CAST_IN_PROGRESS | TRIGGERED_DONT_REPORT_CAST_ERROR,
            .SpellValueOverrides = { { SPELLVALUE_BASE_POINT0, energizeValue } }
        });
    }

    void Register() override
    {
        OnEffectRemove += AuraEffectRemoveFn(spell_dh_darkglare_boon::HandleEffectRemove, EFFECT_0, SPELL_AURA_PERIODIC_TRIGGER_SPELL, AURA_EFFECT_HANDLE_REAL);
    }
};

// 209426 - Darkness
class spell_dh_darkness : public AuraScript
{
    bool Validate(SpellInfo const* spellInfo) override
    {
        return ValidateSpellEffect({ { spellInfo->Id, EFFECT_1 } });
    }

    void CalculateAmount(AuraEffect const* /*aurEff*/, SpellEffectValue& amount, bool& /*canBeRecalculated*/)
    {
        // Set absorbtion amount to unlimited
        amount = -1;
    }

    void Absorb(AuraEffect const* /*aurEff*/, DamageInfo& dmgInfo, uint32& absorbAmount) const
    {
        if (AuraEffect const* chanceEffect = GetEffect(EFFECT_1))
            if (roll_chance(chanceEffect->GetAmount()))
                absorbAmount = dmgInfo.GetDamage();
    }

    void Register() override
    {
        DoEffectCalcAmount += AuraEffectCalcAmountFn(spell_dh_darkness::CalculateAmount, EFFECT_0, SPELL_AURA_SCHOOL_ABSORB);
        OnEffectAbsorb += AuraEffectAbsorbFn(spell_dh_darkness::Absorb, EFFECT_0);
    }
};

// 196718 - Darkness
// Id: 6615
struct areatrigger_dh_darkness : AreaTriggerAI
{
    areatrigger_dh_darkness(AreaTrigger* areaTrigger) : AreaTriggerAI(areaTrigger),
        _absorbAuraInfo(sSpellMgr->GetSpellInfo(SPELL_DH_DARKNESS_ABSORB, DIFFICULTY_NONE)) { }

    void OnUnitEnter(Unit* unit) override
    {
        Unit* caster = at->GetCaster();
        if (!caster || !caster->IsValidAssistTarget(unit, _absorbAuraInfo))
            return;

        caster->CastSpell(unit, SPELL_DH_DARKNESS_ABSORB, CastSpellExtraArgsInit{
            .TriggerFlags = TRIGGERED_IGNORE_CAST_IN_PROGRESS | TRIGGERED_DONT_REPORT_CAST_ERROR,
            .SpellValueOverrides = { { SPELLVALUE_DURATION, at->GetDuration() } }
        });
    }

    void OnUnitExit(Unit* unit, AreaTriggerExitReason /*reason*/) override
    {
        unit->RemoveAura(SPELL_DH_DARKNESS_ABSORB, at->GetCasterGuid());
    }

private:
    SpellInfo const* _absorbAuraInfo;
};

// 203819 - Demon Spikes
class spell_dh_deflecting_spikes : public SpellScript
{
    bool Validate(SpellInfo const* spellInfo) override
    {
        return ValidateSpellInfo({ SPELL_DH_DEFLECTING_SPIKES })
            && ValidateSpellEffect({ { spellInfo->Id, EFFECT_0 } })
            && spellInfo->GetEffect(EFFECT_0).IsAura(SPELL_AURA_MOD_PARRY_PERCENT);
    }

    void HandleParryChance(WorldObject*& target) const
    {
        if (!GetCaster()->HasAura(SPELL_DH_DEFLECTING_SPIKES))
            target = nullptr;
    }

    void Register() override
    {
        OnObjectTargetSelect += SpellObjectTargetSelectFn(spell_dh_deflecting_spikes::HandleParryChance, EFFECT_0, TARGET_UNIT_CASTER);
    }
};

// 388111 - Demon Muzzle
class spell_dh_demon_muzzle : public AuraScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_DH_DEMON_MUZZLE_PROC });
    }

    static void HandleProc(AuraScript const&, AuraEffect const*, ProcEventInfo const& procEvent)
    {
        procEvent.GetActor()->CastSpell(procEvent.GetActionTarget(), SPELL_DH_DEMON_MUZZLE_PROC, CastSpellExtraArgsInit{
            .TriggerFlags = TRIGGERED_IGNORE_CAST_IN_PROGRESS | TRIGGERED_DONT_REPORT_CAST_ERROR,
            .TriggeringSpell = procEvent.GetProcSpell()
        });
    }

    void Register() override
    {
        OnEffectProc += AuraEffectProcFn(spell_dh_demon_muzzle::HandleProc, EFFECT_0, SPELL_AURA_DUMMY);
    }
};

// 213410 - Demonic (attached to 212084 - Fel Devastation and 198013 - Eye Beam)
class spell_dh_demonic : public SpellScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ _transformSpellId })
            && ValidateSpellEffect({ { SPELL_DH_DEMONIC, EFFECT_0 } })
            && sSpellMgr->AssertSpellInfo(SPELL_DH_DEMONIC, DIFFICULTY_NONE)->GetEffect(EFFECT_0).IsAura();
    }

    bool Load() override
    {
        return GetCaster()->HasAuraEffect(SPELL_DH_DEMONIC, EFFECT_0);
    }

    void TriggerMetamorphosis() const
    {
        Unit* caster = GetCaster();
        AuraEffect const* demonic = caster->GetAuraEffect(SPELL_DH_DEMONIC, EFFECT_0);
        if (!demonic)
            return;

        int32 duration = demonic->GetAmountAsInt() + GetSpell()->GetChannelDuration();

        if (Aura* aura = caster->GetAura(_transformSpellId))
        {
            aura->SetMaxDuration(aura->GetDuration() + duration);
            aura->SetDuration(aura->GetMaxDuration());
            return;
        }

        SpellCastTargets targets;
        targets.SetUnitTarget(caster);

        Spell* spell = new Spell(caster, sSpellMgr->AssertSpellInfo(_transformSpellId, DIFFICULTY_NONE),
            TRIGGERED_IGNORE_SPELL_AND_CATEGORY_CD | TRIGGERED_IGNORE_CAST_IN_PROGRESS | TRIGGERED_DONT_REPORT_CAST_ERROR,
            ObjectGuid::Empty, GetSpell()->m_castId);
        spell->m_SpellVisual.SpellXSpellVisualID = 0;
        spell->m_SpellVisual.ScriptVisualID = 0;
        spell->SetSpellValue({ SPELLVALUE_DURATION, duration });
        spell->prepare(targets);
    }

    void Register() override
    {
        AfterCast += SpellCastFn(spell_dh_demonic::TriggerMetamorphosis);
    }

    uint32 _transformSpellId;

public:
    explicit spell_dh_demonic(uint32 transformSpellId) : _transformSpellId(transformSpellId) { }
};

// 206478 - Demonic Appetite
class spell_dh_demonic_appetite : public AuraScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_DH_SHATTERED_SOUL_LESSER_RIGHT, SPELL_DH_SHATTERED_SOUL_LESSER_LEFT });
    }

    static void ShatterLesserSoulFragment(AuraScript const&, ProcEventInfo const& procEvent)
    {
        procEvent.GetActionTarget()->CastSpell(procEvent.GetActor(),
            Trinity::Containers::SelectRandomContainerElement(std::array{ SPELL_DH_SHATTERED_SOUL_LESSER_RIGHT, SPELL_DH_SHATTERED_SOUL_LESSER_LEFT }),
            TRIGGERED_DONT_REPORT_CAST_ERROR);
    }

    void Register() override
    {
        OnProc += AuraProcFn(spell_dh_demonic_appetite::ShatterLesserSoulFragment);
    }
};

// 178963 - Consume Soul
// 202644 - Consume Soul
// 228532 - Consume Soul
// 328953 - Consume Soul
// 1238743 - Consume Soul
class spell_dh_demonic_appetite_energize : public SpellScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_DH_DEMONIC_APPETITE_ENERGIZE });
    }

    bool Load() override
    {
        return !GetCaster()->HasAura(SPELL_DH_DEMONIC_APPETITE);
    }

    void Register() override
    {
        for (SpellEffectInfo const& spellEffectInfo : sSpellMgr->AssertSpellInfo(m_scriptSpellId, DIFFICULTY_NONE)->GetEffects())
            if (spellEffectInfo.IsEffect(SPELL_EFFECT_TRIGGER_SPELL) && spellEffectInfo.TriggerSpell == SPELL_DH_DEMONIC_APPETITE_ENERGIZE)
                OnEffectLaunchTarget += SpellEffectFn(spell_dh_demonic_appetite_energize::PreventHitDefaultEffect, spellEffectInfo.EffectIndex, SPELL_EFFECT_TRIGGER_SPELL);
    }
};

// 203720 - Demon Spikes
class spell_dh_demon_spikes : public SpellScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_DH_DEMON_SPIKES });
    }

    void HandleArmor(SpellEffIndex /*effIndex*/)
    {
        GetCaster()->CastSpell(GetCaster(), SPELL_DH_DEMON_SPIKES, CastSpellExtraArgsInit{
            .TriggerFlags = TRIGGERED_IGNORE_CAST_IN_PROGRESS | TRIGGERED_DONT_REPORT_CAST_ERROR,
            .TriggeringSpell = GetSpell()
        });
    }

    void Register() override
    {
        OnEffectHitTarget += SpellEffectFn(spell_dh_demon_spikes::HandleArmor, EFFECT_0, SPELL_EFFECT_DUMMY);
    }
};

struct spell_dh_shattered_souls_base_lesser
{
    static constexpr std::array<DemonHunterSpells, 2> DevourerSpells = { SPELL_DH_SHATTERED_SOUL_DEVOURER_LESSER_RIGHT, SPELL_DH_SHATTERED_SOUL_DEVOURER_LESSER_LEFT };
    static constexpr std::array<DemonHunterSpells, 2> HavocSpells = { SPELL_DH_SHATTERED_SOUL_LESSER_RIGHT, SPELL_DH_SHATTERED_SOUL_LESSER_LEFT };
    static constexpr std::array<DemonHunterSpells, 2> VengeanceSpells = { SPELL_DH_SHATTER_SOUL_VENGEANCE_FRONT_RIGHT, SPELL_DH_SHATTER_SOUL_VENGEANCE_BACK_RIGHT };

    static bool Validate()
    {
        return SpellScriptBase::ValidateSpellInfo(DevourerSpells)
            && SpellScriptBase::ValidateSpellInfo(HavocSpells)
            && SpellScriptBase::ValidateSpellInfo(VengeanceSpells);
    }

    static void CreateFragments(Unit* source, Unit* dh, int32 count)
    {
        std::span<DemonHunterSpells const> spells = HavocSpells;
        if (Player* player = dh->ToPlayer())
        {
            if (player->GetPrimarySpecialization() == ChrSpecialization::DemonHunterDevourer)
                spells = DevourerSpells;
            else if (player->GetPrimarySpecialization() ==  ChrSpecialization::DemonHunterVengeance)
                spells = VengeanceSpells;
        }

        for (int32 i = 0; i < count; ++i)
            source->CastSpell(dh, Trinity::Containers::SelectRandomContainerElement(spells), TRIGGERED_DONT_REPORT_CAST_ERROR);
    }
};

// 452410 - Enduring Torment
class spell_dh_enduring_torment : public AuraScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_DH_ENDURING_TORMENT_BUFF, SPELL_DH_METAMORPHOSIS_TRANSFORM, SPELL_DH_VOID_METAMORPHOSIS_BUFF });
    }

    void HandlePeriodic(AuraEffect const* aurEff) const
    {
        Unit* target = GetTarget();
        Aura* statBuff = target->GetOwnedAura(SPELL_DH_ENDURING_TORMENT_BUFF);

        if (target->HasAura(SPELL_DH_METAMORPHOSIS_TRANSFORM) || target->HasAura(SPELL_DH_VOID_METAMORPHOSIS_BUFF))
        {
            if (statBuff)
                target->RemoveOwnedAura(statBuff);
        }
        else if (!statBuff)
        {
            target->CastSpell(target, SPELL_DH_ENDURING_TORMENT_BUFF, CastSpellExtraArgsInit{
                .TriggerFlags = TRIGGERED_IGNORE_CAST_IN_PROGRESS | TRIGGERED_DONT_REPORT_CAST_ERROR,
                .TriggeringAura = aurEff
            });
        }
    }

    void Register() override
    {
        OnEffectPeriodic += AuraEffectPeriodicFn(spell_dh_enduring_torment::HandlePeriodic, EFFECT_0, SPELL_AURA_PERIODIC_DUMMY);
    }
};

// 453314 - Enduring Torment
class spell_dh_enduring_torment_buff : public SpellScript
{
    bool Load() override
    {
        return GetCaster()->IsPlayer();
    }

    template <ChrSpecialization Spec>
    void PreventEffect(WorldObject*& target) const
    {
        if (GetCaster()->ToPlayer()->GetPrimarySpecialization() != Spec)
            target = nullptr;
    }

    void Register() override
    {
        OnObjectTargetSelect += SpellObjectTargetSelectFn(spell_dh_enduring_torment_buff::PreventEffect<ChrSpecialization::DemonHunterHavoc>, EFFECT_0, TARGET_UNIT_CASTER);
        OnObjectTargetSelect += SpellObjectTargetSelectFn(spell_dh_enduring_torment_buff::PreventEffect<ChrSpecialization::DemonHunterHavoc>, EFFECT_1, TARGET_UNIT_CASTER);
        OnObjectTargetSelect += SpellObjectTargetSelectFn(spell_dh_enduring_torment_buff::PreventEffect<ChrSpecialization::DemonHunterDevourer>, EFFECT_2, TARGET_UNIT_CASTER);
        OnObjectTargetSelect += SpellObjectTargetSelectFn(spell_dh_enduring_torment_buff::PreventEffect<ChrSpecialization::DemonHunterDevourer>, EFFECT_3, TARGET_UNIT_CASTER);
    }
};

// 307046 - Elysian Decree (Kyrian)
// 389860 - Sigil of Spite
class spell_dh_elysian_decree : public SpellScript
{
public:
    spell_dh_elysian_decree(uint32 primarySpellId) : _primarySpellId(primarySpellId) { }

    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellEffect({ { _primarySpellId, EFFECT_2 } })
            && sSpellMgr->AssertSpellInfo(_primarySpellId, DIFFICULTY_NONE)->GetEffect(EFFECT_2).IsEffect(SPELL_EFFECT_DUMMY)
            && spell_dh_shattered_souls_base_lesser::Validate();
    }

    bool Load() override
    {
        _maxFragmentsToCreate = sSpellMgr->AssertSpellInfo(_primarySpellId, GetCastDifficulty())->GetEffect(EFFECT_2).CalcValueAsInt(GetCaster());
        _fragmentsToCreate = _maxFragmentsToCreate;
        return true;
    }

    void CreateLesserSoulFragments(SpellEffIndex effIndex)
    {
        // spawn more than 1 fragment per target if there are less than 3 total targets
        int32 fragments = 1 + std::max(int32(_maxFragmentsToCreate - GetUnitTargetCountForEffect(effIndex)), 0);
        fragments = std::min(fragments, _fragmentsToCreate);

        spell_dh_shattered_souls_base_lesser::CreateFragments(GetHitUnit(), GetCaster(), fragments);

        _fragmentsToCreate -= fragments;
    }

    void Register() override
    {
        OnEffectHitTarget += SpellEffectFn(spell_dh_elysian_decree::CreateLesserSoulFragments, EFFECT_0, SPELL_EFFECT_SCHOOL_DAMAGE);
    }

private:
    uint32 _primarySpellId;
    int32 _maxFragmentsToCreate = 0;
    int32 _fragmentsToCreate = 0;
};

// 1225826 - Eradicate
class spell_dh_eradicate : public SpellScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_DH_ERADICATE_DAMAGE, SPELL_DH_ERADICATE_DAMAGE_METAMORPHOSIS, SPELL_DH_VOID_METAMORPHOSIS_BUFF });
    }

    void HandleDamage(SpellEffIndex /*effIndex*/) const
    {
        Unit* caster = GetCaster();
        caster->CastSpell(GetHitUnit(), caster->HasAura(SPELL_DH_VOID_METAMORPHOSIS_BUFF)
            ? SPELL_DH_ERADICATE_DAMAGE_METAMORPHOSIS : SPELL_DH_ERADICATE_DAMAGE, CastSpellExtraArgsInit{
            .TriggerFlags = TRIGGERED_IGNORE_CAST_IN_PROGRESS | TRIGGERED_DONT_REPORT_CAST_ERROR,
            .TriggeringSpell = GetSpell()
        });
    }

    void Register() override
    {
        OnEffectHitTarget += SpellEffectFn(spell_dh_eradicate::HandleDamage, EFFECT_0, SPELL_EFFECT_DUMMY);
    }
};

// 258860 - Essence Break
class spell_dh_essence_break : public SpellScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_DH_ESSENCE_BREAK_DEBUFF });
    }

    void HandleDebuff(SpellEffIndex /*effIndex*/) const
    {
        Unit* caster = GetCaster();

        // debuff application is slightly delayed on official servers (after animation fully finishes playing)
        caster->m_Events.AddEventAtOffset([caster, targets = CastSpellTargetArg(GetHitUnit())]() mutable
        {
            if (!targets.Targets)
                return;

            targets.Targets->Update(caster);

            caster->CastSpell(targets, SPELL_DH_ESSENCE_BREAK_DEBUFF, TRIGGERED_IGNORE_CAST_IN_PROGRESS | TRIGGERED_DONT_REPORT_CAST_ERROR);
        }, 300ms);
    }

    void Register() override
    {
        OnEffectHitTarget += SpellEffectFn(spell_dh_essence_break::HandleDebuff, EFFECT_0, SPELL_EFFECT_SCHOOL_DAMAGE);
    }
};

// 198013 - Eye Beam
class spell_dh_eye_beam : public AuraScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_DH_EYE_BEAM_DAMAGE });
    }

    void HandleEffectPeriodic(AuraEffect const* aurEff) const
    {
        if (Unit* caster = GetCaster())
            caster->CastSpell(nullptr, SPELL_DH_EYE_BEAM_DAMAGE, CastSpellExtraArgsInit{
                .TriggerFlags = TRIGGERED_IGNORE_CAST_IN_PROGRESS | TRIGGERED_DONT_REPORT_CAST_ERROR,
                .TriggeringAura = aurEff
            });
    }

    void Register() override
    {
        OnEffectPeriodic += AuraEffectPeriodicFn(spell_dh_eye_beam::HandleEffectPeriodic, EFFECT_0, SPELL_AURA_PERIODIC_TRIGGER_SPELL);
    }
};

// Called by 228477 - Soul Cleave
class spell_dh_feast_of_souls : public SpellScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_DH_FEAST_OF_SOULS, SPELL_DH_FEAST_OF_SOULS_PERIODIC_HEAL });
    }

    bool Load() override
    {
        return GetCaster()->HasAura(SPELL_DH_FEAST_OF_SOULS);
    }

    void HandleHeal() const
    {
        GetCaster()->CastSpell(GetCaster(), SPELL_DH_FEAST_OF_SOULS_PERIODIC_HEAL, CastSpellExtraArgsInit{
            .TriggerFlags = TRIGGERED_IGNORE_CAST_IN_PROGRESS | TRIGGERED_DONT_REPORT_CAST_ERROR,
            .TriggeringSpell = GetSpell()
        });
    }

    void Register() override
    {
        AfterCast += SpellCastFn(spell_dh_feast_of_souls::HandleHeal);
    }
};

// 212084 - Fel Devastation
class spell_dh_fel_devastation : public AuraScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_DH_FEL_DEVASTATION_HEAL });
    }

    void HandlePeriodicEffect(AuraEffect const* aurEff) const
    {
        if (Unit* caster = GetCaster())
            caster->CastSpell(caster, SPELL_DH_FEL_DEVASTATION_HEAL, CastSpellExtraArgsInit{
                .TriggerFlags = TRIGGERED_IGNORE_CAST_IN_PROGRESS | TRIGGERED_DONT_REPORT_CAST_ERROR,
                .TriggeringAura = aurEff
            });
    }

    void Register() override
    {
        OnEffectPeriodic += AuraEffectPeriodicFn(spell_dh_fel_devastation::HandlePeriodicEffect, EFFECT_0, SPELL_AURA_PERIODIC_TRIGGER_SPELL);
    }
};

// Called by 258920 - Immolation Aura
class spell_dh_fel_flame_fortification : public AuraScript
{
    bool Validate(SpellInfo const* /*spell*/) override
    {
        return ValidateSpellInfo({ SPELL_DH_FEL_FLAME_FORTIFICATION_TALENT, SPELL_DH_FEL_FLAME_FORTIFICATION_MOD_DAMAGE });
    }

    bool Load() override
    {
        return GetUnitOwner()->HasAura(SPELL_DH_FEL_FLAME_FORTIFICATION_TALENT);
    }

    void OnApply(AuraEffect const* aurEff, AuraEffectHandleModes /*mode*/) const
    {
        Unit* target = GetTarget();
        target->CastSpell(target, SPELL_DH_FEL_FLAME_FORTIFICATION_MOD_DAMAGE, CastSpellExtraArgsInit{
            .TriggerFlags = TRIGGERED_IGNORE_CAST_IN_PROGRESS | TRIGGERED_DONT_REPORT_CAST_ERROR,
            .TriggeringAura = aurEff,
            .OriginalCastId = aurEff->GetBase()->GetCastId()
        });
    }

    void OnRemove(AuraEffect const* /*aurEff*/, AuraEffectHandleModes /*mode*/) const
    {
        GetTarget()->RemoveAurasDueToSpell(SPELL_DH_FEL_FLAME_FORTIFICATION_MOD_DAMAGE);
    }

    void Register() override
    {
        AfterEffectApply += AuraEffectApplyFn(spell_dh_fel_flame_fortification::OnApply, EFFECT_0, SPELL_AURA_PERIODIC_TRIGGER_SPELL, AURA_EFFECT_HANDLE_REAL);
        AfterEffectRemove += AuraEffectRemoveFn(spell_dh_fel_flame_fortification::OnRemove, EFFECT_0, SPELL_AURA_PERIODIC_TRIGGER_SPELL, AURA_EFFECT_HANDLE_REAL);
    }
};

// 232893 - Felblade
class spell_dh_felblade : public SpellScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_DH_FELBLADE_CHARGE });
    }

    void HandleCharge(SpellEffIndex /*effIndex*/) const
    {
        uint32 spellToCast = GetCaster()->IsWithinMeleeRange(GetHitUnit()) ? SPELL_DH_FELBLADE_DAMAGE : SPELL_DH_FELBLADE_CHARGE;
        GetCaster()->CastSpell(GetHitUnit(), spellToCast, CastSpellExtraArgsInit{
            .TriggerFlags = TRIGGERED_IGNORE_CAST_IN_PROGRESS | TRIGGERED_DONT_REPORT_CAST_ERROR,
            .TriggeringSpell = GetSpell()
        });
    }

    void Register() override
    {
        OnEffectHitTarget += SpellEffectFn(spell_dh_felblade::HandleCharge, EFFECT_0, SPELL_EFFECT_DUMMY);
    }
};

// 213241 - Felblade Charge
class spell_dh_felblade_charge : public SpellScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_DH_FELBLADE_DAMAGE });
    }

    void HandleDamage(SpellEffIndex /*effIndex*/) const
    {
        GetCaster()->CastSpell(GetHitUnit(), SPELL_DH_FELBLADE_DAMAGE, CastSpellExtraArgsInit{
            .TriggerFlags = TRIGGERED_IGNORE_CAST_IN_PROGRESS | TRIGGERED_DONT_REPORT_CAST_ERROR,
            .TriggeringSpell = GetSpell()
        });
    }

    void Register() override
    {
        OnEffectHitTarget += SpellEffectFn(spell_dh_felblade_charge::HandleDamage, EFFECT_0, SPELL_EFFECT_CHARGE);
    }
};

// 203557 - Felblade (Vengeance cooldow reset proc aura)
// 236167 - Felblade (Havoc cooldow reset proc aura)
class spell_dh_felblade_cooldown_reset_proc : public AuraScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_DH_FELBLADE });
    }

    void HandleProc(AuraEffect const* /*aurEff*/, ProcEventInfo const& /*eventInfo*/) const
    {
        GetTarget()->GetSpellHistory()->ResetCooldown(SPELL_DH_FELBLADE, true);
    }

    void Register() override
    {
        OnEffectProc += AuraEffectProcFn(spell_dh_felblade_cooldown_reset_proc::HandleProc, EFFECT_0, SPELL_AURA_PROC_TRIGGER_SPELL);
    }
};

// 204021 - Fiery Brand
class spell_dh_fiery_brand : public SpellScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_DH_FIERY_BRAND_DEBUFF_RANK_1, SPELL_DH_FIERY_BRAND_DEBUFF_RANK_2, SPELL_DH_FIERY_BRAND_RANK_2 });
    }

    void HandleDamage(SpellEffIndex /*effIndex*/) const
    {
        GetCaster()->CastSpell(GetHitUnit(), GetCaster()->HasAura(SPELL_DH_FIERY_BRAND_RANK_2) ? SPELL_DH_FIERY_BRAND_DEBUFF_RANK_2 : SPELL_DH_FIERY_BRAND_DEBUFF_RANK_1,
            CastSpellExtraArgsInit{
                .TriggerFlags = TRIGGERED_IGNORE_CAST_IN_PROGRESS | TRIGGERED_DONT_REPORT_CAST_ERROR,
                .TriggeringSpell = GetSpell()
        });
    }

    void Register() override
    {
        OnEffectHitTarget += SpellEffectFn(spell_dh_fiery_brand::HandleDamage, EFFECT_1, SPELL_EFFECT_SCHOOL_DAMAGE);
    }
};

// 206416 - First Blood
class spell_dh_first_blood : public AuraScript
{
public:
    ObjectGuid const& GetFirstTarget() const { return _firstTargetGUID; }
    void SetFirstTarget(ObjectGuid const& targetGuid) { _firstTargetGUID = targetGuid; }

private:
    void Register() override
    {
    }

private:
    ObjectGuid _firstTargetGUID;
};

// Called by 198013 - Eye Beam
class spell_dh_furious_gaze : public AuraScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_DH_FURIOUS_GAZE, SPELL_DH_FURIOUS_GAZE_BUFF });
    }

    bool Load() override
    {
        return GetUnitOwner()->HasAura(SPELL_DH_FURIOUS_GAZE);
    }

    void HandleAfterRemove(AuraEffect const* aurEff, AuraEffectHandleModes /*mode*/) const
    {
        if (GetTargetApplication()->GetRemoveMode() != AURA_REMOVE_BY_EXPIRE)
            return;

        Unit* target = GetTarget();
        target->CastSpell(target, SPELL_DH_FURIOUS_GAZE_BUFF, CastSpellExtraArgsInit{
            .TriggerFlags = TRIGGERED_IGNORE_CAST_IN_PROGRESS | TRIGGERED_DONT_REPORT_CAST_ERROR,
            .TriggeringAura = aurEff
        });
    }

    void Register() override
    {
        AfterEffectRemove += AuraEffectRemoveFn(spell_dh_furious_gaze::HandleAfterRemove, EFFECT_0, SPELL_AURA_PERIODIC_TRIGGER_SPELL, AURA_EFFECT_HANDLE_REAL);
    }
};

// 342817 - Glaive Tempest
// ID - 21832
struct at_dh_glaive_tempest : AreaTriggerAI
{
    using AreaTriggerAI::AreaTriggerAI;

    void OnCreate(Spell const* /*creatingSpell*/) override
    {
        _scheduler.Schedule(0ms, [this](TaskContext& task)
        {
            FloatMilliseconds period = 500ms; // 500ms, affected by haste
            if (Unit* caster = at->GetCaster())
            {
                period *= *caster->m_unitData->ModHaste;
                caster->CastSpell(at->GetPosition(), SPELL_DH_GLAIVE_TEMPEST, TRIGGERED_IGNORE_CAST_IN_PROGRESS | TRIGGERED_DONT_REPORT_CAST_ERROR);
                caster->CastSpell(at->GetPosition(), SPELL_DH_GLAIVE_TEMPEST, TRIGGERED_IGNORE_CAST_IN_PROGRESS | TRIGGERED_DONT_REPORT_CAST_ERROR);
            }
            task.Repeat(duration_cast<Milliseconds>(period));
        });
    }

    void OnUpdate(uint32 diff) override
    {
        _scheduler.Update(diff);
    }

private:
    TaskScheduler _scheduler;
};

// Called by 162264 - Metamorphosis
class spell_dh_inner_demon : public AuraScript
{
    bool Validate(SpellInfo const* /*spell*/) override
    {
        return ValidateSpellInfo({ SPELL_DH_INNER_DEMON_TALENT, SPELL_DH_INNER_DEMON_BUFF });
    }

    bool Load() override
    {
        return GetUnitOwner()->HasAura(SPELL_DH_INNER_DEMON_TALENT); // This spell has a proc, but is just a copypaste from spell 390145 (also don't have a 5s cooldown)
    }

    void OnApply(AuraEffect const* /*aurEff*/, AuraEffectHandleModes /*mode*/) const
    {
        Unit* target = GetTarget();
        target->CastSpell(target, SPELL_DH_INNER_DEMON_BUFF, CastSpellExtraArgsInit{
            .TriggerFlags = TRIGGERED_IGNORE_CAST_IN_PROGRESS | TRIGGERED_DONT_REPORT_CAST_ERROR,
        });
    }

    void Register() override
    {
        AfterEffectApply += AuraEffectApplyFn(spell_dh_inner_demon::OnApply, EFFECT_0, SPELL_AURA_TRANSFORM, AURA_EFFECT_HANDLE_REAL_OR_REAPPLY_MASK);
    }
};

// 390139 - Inner Demon
// ID - 26749
struct at_dh_inner_demon : AreaTriggerAI
{
    using AreaTriggerAI::AreaTriggerAI;

    void OnInitialize() override
    {
        SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(at->GetSpellId(), DIFFICULTY_NONE);
        if (!spellInfo)
            return;

        Unit* caster = at->GetCaster();
        if (!caster)
            return;

        Position destPos = at->GetFirstCollisionPosition(spellInfo->GetEffect(EFFECT_0).CalcValue(caster) + at->GetMaxSearchRadius(), at->GetRelativeAngle(caster));
        PathGenerator path(at);

        path.CalculatePath(destPos.GetPositionX(), destPos.GetPositionY(), destPos.GetPositionZ(), false);

        at->InitSplines(path.GetPath());
    }

    void OnRemove() override
    {
        if (Unit* caster = at->GetCaster())
            caster->CastSpell(caster->GetPosition(), SPELL_DH_INNER_DEMON_DAMAGE, TRIGGERED_IGNORE_CAST_IN_PROGRESS | TRIGGERED_DONT_REPORT_CAST_ERROR);
    }
};

// 388118 - Know Your Enemy
class spell_dh_know_your_enemy : public AuraScript
{
    void CalcAmount(AuraEffect const* /*aurEff*/, SpellEffectValue& amount, bool const& /*canBeRecalculated*/) const
    {
        if (AuraEffect const* amountHolder = GetEffect(EFFECT_1))
        {
            float critChanceDone = GetUnitOwner()->GetUnitCriticalChanceDone(BASE_ATTACK);
            amount = CalculatePct(critChanceDone, amountHolder->GetAmount());
        }
    }

    void UpdatePeriodic(AuraEffect const* aurEff) const
    {
        if (AuraEffect* bonus = GetEffect(EFFECT_0))
            bonus->RecalculateAmount(aurEff);
    }

    void Register() override
    {
        DoEffectCalcAmount += AuraEffectCalcAmountFn(spell_dh_know_your_enemy::CalcAmount, EFFECT_0, SPELL_AURA_MOD_CRIT_DAMAGE_BONUS);
        OnEffectPeriodic += AuraEffectPeriodicFn(spell_dh_know_your_enemy::UpdatePeriodic, EFFECT_1, SPELL_AURA_PERIODIC_DUMMY);
    }
};

// 209258 - Last Resort
class spell_dh_last_resort : public AuraScript
{
    bool Validate(SpellInfo const* spellInfo) override
    {
        return ValidateSpellInfo({ SPELL_DH_UNCONTAINED_FEL, SPELL_DH_METAMORPHOSIS_VENGEANCE_TRANSFORM })
            && ValidateSpellEffect({ { spellInfo->Id, EFFECT_1 } });
    }

    void HandleAbsorb(AuraEffect const* /*aurEff*/, DamageInfo const& /*dmgInfo*/, uint32& absorbAmount)
    {
        Unit* target = GetTarget();
        if (target->HasAura(SPELL_DH_UNCONTAINED_FEL))
        {
            absorbAmount = 0;
            return;
        }

        PreventDefaultAction();

        CastSpellExtraArgs castArgs = TRIGGERED_IGNORE_CAST_IN_PROGRESS | TRIGGERED_DONT_REPORT_CAST_ERROR | TRIGGERED_IGNORE_SPELL_AND_CATEGORY_CD;

        target->CastSpell(target, SPELL_DH_METAMORPHOSIS_VENGEANCE_TRANSFORM, castArgs);
        target->CastSpell(target, SPELL_DH_UNCONTAINED_FEL, castArgs);

        target->SetHealth(target->CountPctFromMaxHealth(GetEffectInfo(EFFECT_1).CalcValue(target)));
    }

    void Register() override
    {
        OnEffectAbsorb += AuraEffectAbsorbOverkillFn(spell_dh_last_resort::HandleAbsorb, EFFECT_0);
    }
};

// 1238488 - Moment of Craving (attached to 473728 - Void Ray)
class spell_dh_moment_of_craving : public AuraScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_DH_MOMENT_OF_CRAVING_TALENT, SPELL_DH_MOMENT_OF_CRAVING_BUFF, SPELL_DH_REAP });
    }

    bool Load() override
    {
        return GetUnitOwner()->HasAura(SPELL_DH_MOMENT_OF_CRAVING_TALENT);
    }

    void HandleAfterRemove(AuraEffect const* /*aurEff*/, AuraEffectHandleModes /*mode*/) const
    {
        if (GetTargetApplication()->GetRemoveMode() != AURA_REMOVE_BY_EXPIRE)
            return;

        Unit* target = GetTarget();
        target->GetSpellHistory()->RestoreCharge(sSpellMgr->AssertSpellInfo(SPELL_DH_REAP, GetCastDifficulty())->ChargeCategoryId);
        target->CastSpell(target, SPELL_DH_MOMENT_OF_CRAVING_BUFF, TRIGGERED_IGNORE_CAST_IN_PROGRESS | TRIGGERED_DONT_REPORT_CAST_ERROR);
    }

    void Register() override
    {
        AfterEffectRemove += AuraEffectRemoveFn(spell_dh_moment_of_craving::HandleAfterRemove, EFFECT_0, SPELL_AURA_PERIODIC_TRIGGER_SPELL, AURA_EFFECT_HANDLE_REAL);
    }
};

// 452414 - Monster Rising
class spell_dh_monster_rising : public AuraScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_DH_MONSTER_RISING_AGILITY, SPELL_DH_METAMORPHOSIS_TRANSFORM, SPELL_DH_METAMORPHOSIS_VENGEANCE_TRANSFORM });
    }

    void HandlePeriodic(AuraEffect const* aurEff) const
    {
        Unit* target = GetTarget();
        AuraApplication* statBuff = target->GetAuraApplication(SPELL_DH_MONSTER_RISING_AGILITY);

        if (target->HasAura(SPELL_DH_METAMORPHOSIS_TRANSFORM) || target->HasAura(SPELL_DH_METAMORPHOSIS_VENGEANCE_TRANSFORM))
        {
            if (statBuff)
                target->RemoveAura(statBuff);
        }
        else if (!statBuff)
        {
            target->CastSpell(target, SPELL_DH_MONSTER_RISING_AGILITY, CastSpellExtraArgsInit{
                .TriggerFlags = TRIGGERED_IGNORE_CAST_IN_PROGRESS | TRIGGERED_DONT_REPORT_CAST_ERROR,
                .TriggeringAura = aurEff
            });
        }
    }

    void Register() override
    {
        OnEffectPeriodic += AuraEffectPeriodicFn(spell_dh_monster_rising::HandlePeriodic, EFFECT_0, SPELL_AURA_PERIODIC_DUMMY);
    }
};

// 207387 - Painbringer
class spell_dh_painbringer : public AuraScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_DH_PAINBRINGER_STACK });
    }

    void HandleProc(ProcEventInfo const& eventInfo) const
    {
        Unit* target = eventInfo.GetActor();
        target->CastSpell(target, SPELL_DH_PAINBRINGER_STACK, CastSpellExtraArgsInit{
            .TriggerFlags = TRIGGERED_IGNORE_CAST_IN_PROGRESS | TRIGGERED_DONT_REPORT_CAST_ERROR,
            .TriggeringSpell = eventInfo.GetProcSpell()
        });
    }

    void Register() override
    {
        OnProc += AuraProcFn(spell_dh_painbringer::HandleProc);
    }
};

// 212988 - Painbringer
class spell_dh_painbringer_reduce_damage : public AuraScript
{
    bool Validate(SpellInfo const* /*spell*/) override
    {
        return ValidateSpellInfo({ SPELL_DH_PAINBRINGER_DUMMY });
    }

    void OnApply(AuraEffect const* aurEff, AuraEffectHandleModes mode) const
    {
        Unit* target = GetTarget();

        if (mode & AURA_EFFECT_HANDLE_REAL)
            target->CastSpell(target, SPELL_DH_PAINBRINGER_DUMMY, CastSpellExtraArgsInit{
                .TriggerFlags = TRIGGERED_IGNORE_CAST_IN_PROGRESS | TRIGGERED_DONT_REPORT_CAST_ERROR,
                .TriggeringAura = aurEff
            });

        target->m_Events.AddEventAtOffset([self = GetAura()->GetWeakPtr()]
        {
            if (Trinity::unique_strong_ref_ptr<Aura> aura = self.lock())
                aura->ModStackAmount(-1, AURA_REMOVE_BY_EXPIRE, false);
        }, Milliseconds(GetMaxDuration()));
    }

    void OnRemove(AuraEffect const* /*aurEff*/, AuraEffectHandleModes /*mode*/) const
    {
        GetTarget()->RemoveAurasDueToSpell(SPELL_DH_PAINBRINGER_DUMMY);
    }

    void Register() override
    {
        AfterEffectApply += AuraEffectApplyFn(spell_dh_painbringer_reduce_damage::OnApply, EFFECT_1, SPELL_AURA_DUMMY, AURA_EFFECT_HANDLE_REAL_OR_REAPPLY_MASK);
        AfterEffectRemove += AuraEffectRemoveFn(spell_dh_painbringer_reduce_damage::OnRemove, EFFECT_1, SPELL_AURA_DUMMY, AURA_EFFECT_HANDLE_REAL);
    }
};

// 188499 - Blade Dance
// 210152 - Death Sweep
class spell_dh_blade_dance : public SpellScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_DH_FIRST_BLOOD });
    }

    void DecideFirstTarget(std::list<WorldObject*>& targetList)
    {
        if (targetList.empty())
            return;

        Aura* aura = GetCaster()->GetAura(SPELL_DH_FIRST_BLOOD);
        if (!aura)
            return;

        ObjectGuid firstTargetGUID = ObjectGuid::Empty;
        ObjectGuid selectedTarget = GetCaster()->GetTarget();

        // Prefer the selected target if he is one of the enemies
        if (targetList.size() > 1 && !selectedTarget.IsEmpty())
        {
            auto it = std::find_if(targetList.begin(), targetList.end(), [selectedTarget](WorldObject* object)
            {
                return object->GetGUID() == selectedTarget;
            });
            if (it != targetList.end())
                firstTargetGUID = (*it)->GetGUID();
        }

        if (firstTargetGUID.IsEmpty())
            firstTargetGUID = targetList.front()->GetGUID();

        if (spell_dh_first_blood* script = aura->GetScript<spell_dh_first_blood>())
            script->SetFirstTarget(firstTargetGUID);
    }

    void Register() override
    {
        OnObjectAreaTargetSelect += SpellObjectAreaTargetSelectFn(spell_dh_blade_dance::DecideFirstTarget, EFFECT_0, TARGET_UNIT_SRC_AREA_ENEMY);
    }
};

// 199552 - Blade Dance
// 200685 - Blade Dance
// 210153 - Death Sweep
// 210155 - Death Sweep
class spell_dh_blade_dance_damage : public SpellScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_DH_FIRST_BLOOD });
    }

    void HandleHitTarget()
    {
        int32 damage = GetHitDamage();

        if (AuraEffect* aurEff = GetCaster()->GetAuraEffect(SPELL_DH_FIRST_BLOOD, EFFECT_0))
            if (spell_dh_first_blood* script = aurEff->GetBase()->GetScript<spell_dh_first_blood>())
                if (GetHitUnit()->GetGUID() == script->GetFirstTarget())
                    AddPct(damage, aurEff->GetAmount());

        SetHitDamage(damage);
    }

    void Register() override
    {
        OnHit += SpellHitFn(spell_dh_blade_dance_damage::HandleHitTarget);
    }
};

// 131347 - Glide
class spell_dh_glide : public SpellScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_DH_GLIDE_KNOCKBACK, SPELL_DH_GLIDE_DURATION, SPELL_DH_VENGEFUL_RETREAT_TRIGGER, SPELL_DH_FEL_RUSH });
    }

    SpellCastResult CheckCast()
    {
        Unit* caster = GetCaster();
        if (caster->IsMounted() || caster->GetVehicleBase())
            return SPELL_FAILED_DONT_REPORT;

        if (!caster->IsFalling())
            return SPELL_FAILED_NOT_ON_GROUND;

        return SPELL_CAST_OK;
    }

    void HandleCast()
    {
        Player* caster = GetCaster()->ToPlayer();
        if (!caster)
            return;

        caster->CastSpell(caster, SPELL_DH_GLIDE_KNOCKBACK, true);
        caster->CastSpell(caster, SPELL_DH_GLIDE_DURATION, true);

        caster->GetSpellHistory()->StartCooldown(sSpellMgr->AssertSpellInfo(SPELL_DH_VENGEFUL_RETREAT_TRIGGER, GetCastDifficulty()), 0, nullptr, false, 250ms);
        caster->GetSpellHistory()->StartCooldown(sSpellMgr->AssertSpellInfo(SPELL_DH_FEL_RUSH, GetCastDifficulty()), 0, nullptr, false, 250ms);
    }

    void Register() override
    {
        OnCheckCast += SpellCheckCastFn(spell_dh_glide::CheckCast);
        BeforeCast += SpellCastFn(spell_dh_glide::HandleCast);
    }
};

// 131347 - Glide
class spell_dh_glide_AuraScript : public AuraScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_DH_GLIDE_DURATION });
    }

    void OnRemove(AuraEffect const* /*aurEff*/, AuraEffectHandleModes /*mode*/)
    {
        GetTarget()->RemoveAura(SPELL_DH_GLIDE_DURATION);
    }

    void Register() override
    {
        AfterEffectRemove += AuraEffectRemoveFn(spell_dh_glide_AuraScript::OnRemove, EFFECT_0, SPELL_AURA_FEATHER_FALL, AURA_EFFECT_HANDLE_REAL);
    }
};

// 197154 - Glide
class spell_dh_glide_timer : public AuraScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_DH_GLIDE });
    }

    void OnRemove(AuraEffect const* /*aurEff*/, AuraEffectHandleModes /*mode*/)
    {
        GetTarget()->RemoveAura(SPELL_DH_GLIDE);
    }

    void Register() override
    {
        AfterEffectRemove += AuraEffectRemoveFn(spell_dh_glide_timer::OnRemove, EFFECT_0, SPELL_AURA_DUMMY, AURA_EFFECT_HANDLE_REAL);
    }
};

// 1226019 - Reap
class spell_dh_reap : public SpellScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_DH_REAP_DAMAGE });
    }

    void HandleDamage(SpellEffIndex /*effIndex*/) const
    {
        GetCaster()->CastSpell(GetHitUnit(), SPELL_DH_REAP_DAMAGE, CastSpellExtraArgsInit{
            .TriggerFlags = TRIGGERED_IGNORE_CAST_IN_PROGRESS | TRIGGERED_DONT_REPORT_CAST_ERROR,
            .TriggeringSpell = GetSpell()
        });
    }

    void Register() override
    {
        OnEffectHitTarget += SpellEffectFn(spell_dh_reap::HandleDamage, EFFECT_0, SPELL_EFFECT_DUMMY);
    }
};

// 339895 - Repeat Decree (attached to 307046 - Elysian Decree and 389860 - Sigil of Spite)
class spell_dh_repeat_decree_conduit : public SpellScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_DH_REPEAT_DECREE_CONDUIT });
    }

    bool Load() override
    {
        return !GetCaster()->HasAura(SPELL_DH_REPEAT_DECREE_CONDUIT);
    }

    void Register() override
    {
        OnEffectLaunch += SpellEffectFn(spell_dh_repeat_decree_conduit::PreventHitDefaultEffect, EFFECT_1, SPELL_EFFECT_TRIGGER_SPELL);
    }
};

// Called by 162264 - Metamorphosis
class spell_dh_restless_hunter : public AuraScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_DH_RESTLESS_HUNTER_TALENT, SPELL_DH_RESTLESS_HUNTER_BUFF, SPELL_DH_FEL_RUSH })
            && sSpellCategoryStore.HasRecord(sSpellMgr->AssertSpellInfo(SPELL_DH_FEL_RUSH, DIFFICULTY_NONE)->ChargeCategoryId);
    }

    bool Load() override
    {
        return GetCaster()->HasAura(SPELL_DH_RESTLESS_HUNTER_TALENT);
    }

    void OnRemove(AuraEffect const* aurEff, AuraEffectHandleModes /*mode*/) const
    {
        Unit* target = GetTarget();

        target->CastSpell(target, SPELL_DH_RESTLESS_HUNTER_BUFF, CastSpellExtraArgsInit{
            .TriggerFlags = TRIGGERED_IGNORE_CAST_IN_PROGRESS | TRIGGERED_DONT_REPORT_CAST_ERROR,
            .TriggeringAura = aurEff
        });

        target->GetSpellHistory()->RestoreCharge(sSpellMgr->AssertSpellInfo(SPELL_DH_FEL_RUSH, GetCastDifficulty())->ChargeCategoryId);
    }

    void Register() override
    {
        AfterEffectRemove += AuraEffectRemoveFn(spell_dh_restless_hunter::OnRemove, EFFECT_0, SPELL_AURA_TRANSFORM, AURA_EFFECT_HANDLE_REAL_OR_REAPPLY_MASK);
    }
};

// 389729 - Retaliation (attached to 203819 - Demon Spikes)
class spell_dh_retaliation : public AuraScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_DH_RETALIATION_TALENT, SPELL_DH_RETALIATION_PROC });
    }

    bool Load() override
    {
        return GetUnitOwner()->HasAura(SPELL_DH_RETALIATION_TALENT);
    }

    void HandleAfterApply(AuraEffect const* aurEff, AuraEffectHandleModes /*mode*/) const
    {
        Unit* target = GetTarget();
        target->CastSpell(target, SPELL_DH_RETALIATION_PROC, CastSpellExtraArgsInit{
            .TriggerFlags = TRIGGERED_IGNORE_CAST_IN_PROGRESS | TRIGGERED_DONT_REPORT_CAST_ERROR,
            .TriggeringAura = aurEff
        });
    }

    void HandleAfterRemove(AuraEffect const* /*aurEff*/, AuraEffectHandleModes /*mode*/) const
    {
        GetTarget()->RemoveAurasDueToSpell(SPELL_DH_RETALIATION_PROC);
    }

    void Register() override
    {
        AfterEffectApply += AuraEffectApplyFn(spell_dh_retaliation::HandleAfterApply, EFFECT_0, SPELL_AURA_MOD_PARRY_PERCENT, AURA_EFFECT_HANDLE_REAL_OR_REAPPLY_MASK);
        AfterEffectRemove += AuraEffectRemoveFn(spell_dh_retaliation::HandleAfterRemove, EFFECT_0, SPELL_AURA_MOD_PARRY_PERCENT, AURA_EFFECT_HANDLE_REAL_OR_REAPPLY_MASK);
    }
};

// 388116 - Shattered Destiny
class spell_dh_shattered_destiny : public AuraScript
{
    bool Validate(SpellInfo const* spellInfo) override
    {
        return ValidateSpellInfo({ SPELL_DH_METAMORPHOSIS_TRANSFORM })
            && ValidateSpellEffect({ { spellInfo->Id, EFFECT_1 } })
            && spellInfo->GetEffect(EFFECT_0).IsAura()
            && spellInfo->GetEffect(EFFECT_1).IsAura();
    }

    bool CheckFurySpent(ProcEventInfo const& eventInfo)
    {
        Spell const* procSpell = eventInfo.GetProcSpell();
        if (!procSpell)
            return false;

        if (!eventInfo.GetActor()->HasAura(SPELL_DH_METAMORPHOSIS_TRANSFORM))
            return false;

        _furySpent += procSpell->GetPowerTypeCostAmount(POWER_FURY).value_or(0);
        return _furySpent >= GetEffect(EFFECT_1)->GetAmountAsInt();
    }

    void HandleProc(ProcEventInfo const& /*eventInfo*/)
    {
        Aura* metamorphosis = GetTarget()->GetAura(SPELL_DH_METAMORPHOSIS_TRANSFORM);
        if (!metamorphosis)
            return;

        int32 requiredFuryAmount = GetEffect(EFFECT_1)->GetAmountAsInt();
        metamorphosis->SetDuration(metamorphosis->GetDuration() + _furySpent / requiredFuryAmount * GetEffect(EFFECT_0)->GetAmountAsInt());
        _furySpent %= requiredFuryAmount;
    }

    void Register() override
    {
        DoCheckProc += AuraCheckProcFn(spell_dh_shattered_destiny::CheckFurySpent);
        OnProc += AuraProcFn(spell_dh_shattered_destiny::HandleProc);
    }

private:
    int32 _furySpent = 0;
};

// 389824 - Shattered Restoration (attached to 202644, 228532, 178963, 210042, 203794 - Consume Soul)
class spell_dh_shattered_restoration : public SpellScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellEffect({ { SPELL_DH_SHATTERED_RESTORATION, EFFECT_0 } });
    }

    void CalculateHealingBonus(SpellEffectInfo const& /*spellEffectInfo*/, Unit const* /*victim*/, int32 const& /*healing*/, int32 const& /*flatMod*/, float& pctMod) const
    {
        if (AuraEffect* const shatteredRestoration = GetCaster()->GetAuraEffect(SPELL_DH_SHATTERED_RESTORATION, EFFECT_0))
            AddPct(pctMod, shatteredRestoration->GetAmount());
    }

    void Register() override
    {
        CalcHealing += SpellCalcHealingFn(spell_dh_shattered_restoration::CalculateHealingBonus);
    }
};

// 178940 - Shattered Souls
// 204254 - Shattered Souls
class spell_dh_shattered_souls : public AuraScript
{
public:
    spell_dh_shattered_souls(uint32 triggeredSpellId) : _triggeredSpellId(triggeredSpellId) { }

    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ _triggeredSpellId });
    }

    void HandleProc(AuraEffect const* /*aurEff*/, ProcEventInfo const& eventInfo) const
    {
        Unit* caster = eventInfo.GetActor();
        Unit* target = eventInfo.GetActionTarget();

        if (!caster || !target)
            return;

        target->CastSpell(caster, _triggeredSpellId, TRIGGERED_IGNORE_CAST_IN_PROGRESS | TRIGGERED_DONT_REPORT_CAST_ERROR);
    }

    void Register() override
    {
        OnEffectProc += AuraEffectProcFn(spell_dh_shattered_souls::HandleProc, EFFECT_0, SPELL_AURA_DUMMY);
    }

private:
    uint32 _triggeredSpellId;
};

// 1227619 - Shattered Souls
class spell_dh_shattered_souls_devourer : public AuraScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return spell_dh_shattered_souls_base_lesser::Validate()
            && ValidateSpellInfo({ SPELL_DH_SOUL_FRAGMENT_DEVOURER });
    }

    static bool CheckProc(AuraScript const&, AuraEffect const* aurEff, ProcEventInfo const& /*eventInfo*/)
    {
        return roll_chance(aurEff->GetAmount());
    }

    static bool CheckReapSoulGatheringProc(AuraScript const&, AuraEffect const* /*aurEff*/, ProcEventInfo const& eventInfo)
    {
        // Eradicate, Reap and Cull
        return eventInfo.GetSpellInfo()->IsAffected(SPELLFAMILY_DEMON_HUNTER, { 0x0, 0x0, 0x0, 0x40 });
    }

    static void HandleProc(AuraScript const&, AuraEffect const* /*aurEff*/, ProcEventInfo const& eventInfo)
    {
        spell_dh_shattered_souls_base_lesser::CreateFragments(eventInfo.GetActionTarget(), eventInfo.GetActor(), 1);
    }

    static void HandleSoulsGathering(AuraScript const&, AuraEffect const* aurEff, ProcEventInfo const& eventInfo)
    {
        Unit* caster = eventInfo.GetActor();
        float range = eventInfo.GetSpellInfo()->GetMaxRange();

        std::vector<AreaTrigger*> soulFragments = caster->GetAreaTriggers(SPELL_DH_SOUL_FRAGMENT_DEVOURER);
        Trinity::Containers::EraseIf(soulFragments, [caster, range](AreaTrigger const* at) { return !at->IsWithinDist(caster, range); });
        if (soulFragments.empty())
            return;

        uint32 maxTargets = aurEff->GetAmountAsInt();
        if (soulFragments.size() > maxTargets)
            soulFragments.resize(maxTargets);

        for (AreaTrigger* soulFragment : soulFragments)
        {
            caster->CastSpell(soulFragment->GetPosition(), SPELL_DH_CONSUME_SOUL_DEVOURER, CastSpellExtraArgsInit{
                .TriggerFlags = TRIGGERED_IGNORE_CAST_IN_PROGRESS | TRIGGERED_DONT_REPORT_CAST_ERROR,
                .TriggeringSpell = eventInfo.GetProcSpell()
            });
            soulFragment->Remove();
        }
    }

    void Register() override
    {
        DoCheckEffectProc += AuraCheckEffectProcFn(spell_dh_shattered_souls_devourer::CheckProc, EFFECT_0, SPELL_AURA_DUMMY);
        DoCheckEffectProc += AuraCheckEffectProcFn(spell_dh_shattered_souls_devourer::CheckReapSoulGatheringProc, EFFECT_1, SPELL_AURA_DUMMY);
        OnEffectProc += AuraEffectProcFn(spell_dh_shattered_souls_devourer::HandleProc, EFFECT_0, SPELL_AURA_DUMMY);
        OnEffectProc += AuraEffectProcFn(spell_dh_shattered_souls_devourer::HandleSoulsGathering, EFFECT_1, SPELL_AURA_DUMMY);
    }
};

// 1223450 - Shattered Souls
class spell_dh_shattered_souls_devourer_dummy : public SpellScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_DH_SOUL_FRAGMENT_DEVOURER });
    }

    void HandleSoulFragment(SpellEffIndex /*effIndex*/) const
    {
        GetCaster()->CastSpell(GetHitDest()->GetPosition(), SPELL_DH_SOUL_FRAGMENT_DEVOURER, CastSpellExtraArgsInit{
            .TriggerFlags = TRIGGERED_IGNORE_CAST_IN_PROGRESS | TRIGGERED_DONT_REPORT_CAST_ERROR,
            .TriggeringSpell = GetSpell()
        });
    }

    void Register() override
    {
        OnEffectHit += SpellEffectFn(spell_dh_shattered_souls_devourer_dummy::HandleSoulFragment, EFFECT_0, SPELL_EFFECT_DUMMY);
    }
};

// 209651 - Shattered Souls
// 210038 - Shatter Soul
// 1223445 - Shattered Souls
// 1223448 - Shattered Souls
class spell_dh_shattered_souls_trigger : public SpellScript
{
public:
    spell_dh_shattered_souls_trigger(uint32 triggeredSpellId, uint32 triggeredSpellIdDemon)
        : _triggeredSpellId(triggeredSpellId), _triggeredSpellIdDemon(triggeredSpellIdDemon) { }

    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ _triggeredSpellId })
            && (!_triggeredSpellIdDemon || ValidateSpellInfo({ _triggeredSpellIdDemon }));
    }

    void HandleSoulFragment(SpellEffIndex /*effIndex*/) const
    {
        if (Unit* target = GetExplTargetUnit())
            target->CastSpell(GetHitDest()->GetPosition(), _triggeredSpellIdDemon && GetCaster()->GetCreatureType() == CREATURE_TYPE_DEMON ? _triggeredSpellIdDemon : _triggeredSpellId, CastSpellExtraArgsInit{
                .TriggerFlags = TRIGGERED_IGNORE_CAST_IN_PROGRESS | TRIGGERED_DONT_REPORT_CAST_ERROR,
                .TriggeringSpell = GetSpell()
            });
    }

    void Register() override
    {
        OnEffectLaunch += SpellEffectFn(spell_dh_shattered_souls_trigger::HandleSoulFragment, EFFECT_1, SPELL_EFFECT_DUMMY);
    }

private:
    uint32 _triggeredSpellId;
    uint32 _triggeredSpellIdDemon;
};

class spell_dh_feast_of_souls_tracker : public AuraScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_DH_FEAST_OF_SOULS_BUFF });
    }

    void HandleRemove(AuraEffect const* /*aurEff*/, AuraEffectHandleModes /*mode*/)
    {
        GetTarget()->RemoveAura(SPELL_DH_FEAST_OF_SOULS_BUFF);
    }

public:
    void AddStack(Unit* unit)
    {
        Player* player = unit ? unit->ToPlayer() : nullptr;
        if (!player)
            return;

        // Compute effective duration: 10s base + Sweet Suffering flat modifier
        int32 stackDuration = 10000;
        if (SpellInfo const* buffInfo = sSpellMgr->GetSpellInfo(SPELL_DH_FEAST_OF_SOULS_BUFF, DIFFICULTY_NONE))
            player->ApplySpellMod(buffInfo, SpellModOp::Duration, stackDuration);
        stackDuration = std::max(stackDuration, 1000);

        if (Aura* buff = player->GetAura(SPELL_DH_FEAST_OF_SOULS_BUFF))
        {
            buff->SetStackAmount(std::min<uint8>(buff->GetStackAmount() + 1, 20));
            buff->SetMaxDuration(stackDuration);
            buff->SetDuration(stackDuration);
        }
        else
        {
            player->CastSpell(player, SPELL_DH_FEAST_OF_SOULS_BUFF,
                TRIGGERED_IGNORE_CAST_IN_PROGRESS | TRIGGERED_DONT_REPORT_CAST_ERROR);
            if (Aura* buff = player->GetAura(SPELL_DH_FEAST_OF_SOULS_BUFF))
            {
                buff->SetMaxDuration(stackDuration);
                buff->SetDuration(stackDuration);
            }
        }

        player->m_Events.AddEventAtOffset([playerGUID = player->GetGUID()]()
            {
                if (Player* p = ObjectAccessor::FindConnectedPlayer(playerGUID))
                    if (Aura* buff = p->GetAura(SPELL_DH_FEAST_OF_SOULS_BUFF))
                    {
                        uint8 stacks = buff->GetStackAmount();
                        if (stacks <= 1)
                            p->RemoveAura(SPELL_DH_FEAST_OF_SOULS_BUFF);
                        else
                            buff->SetStackAmount(stacks - 1);
                    }
            }, Milliseconds(stackDuration));
    }

    void Register() override
    {
        AfterEffectRemove += AuraEffectRemoveFn(spell_dh_feast_of_souls_tracker::HandleRemove, EFFECT_0, SPELL_AURA_DUMMY, AURA_EFFECT_HANDLE_REAL);
    }
};

// 209693 - Shattered Souls, 209788 - Shattered Souls and 1223412 - Soul Fragment
// Id - 3680, 6659 and 36671
template<uint32 SpellId>
struct at_dh_shattered_souls : public AreaTriggerAI
{
    using AreaTriggerAI::AreaTriggerAI;

    uint32 _spawnDelay = 500;

    void OnUpdate(uint32 diff) override
    {
        if (!_spawnDelay)
            return;

        if (_spawnDelay > diff)
        {
            _spawnDelay -= diff;
            return;
        }

        _spawnDelay = 0;

        // Check if caster is already inside now that delay expired
        if (Unit* caster = at->GetCaster())
            if (at->GetInsideUnits().count(caster->GetGUID()))
                Collect(caster);
    }

    void OnUnitEnter(Unit* unit) override
    {
        if (_spawnDelay)
            return;

        Collect(unit);
    }

    void Collect(Unit * unit)
    {
        unit->CastSpell(at->GetPosition(), SpellId, TRIGGERED_IGNORE_CAST_IN_PROGRESS | TRIGGERED_DONT_REPORT_CAST_ERROR);

        if (unit->HasAura(SPELL_DH_VOID_METAMORPHOSIS_PASSIVE) && !unit->HasAura(SPELL_DH_VOID_METAMORPHOSIS_BUFF))
            unit->CastSpell(unit, SPELL_DH_VOID_METAMORPHOSIS_COUNTER,
                TRIGGERED_IGNORE_CAST_IN_PROGRESS | TRIGGERED_DONT_REPORT_CAST_ERROR);

        if (unit->HasAura(SPELL_DH_VOID_METAMORPHOSIS_BUFF) && unit->HasAura(SPELL_DH_COLLAPSING_STAR_PASSIVE))
            unit->CastSpell(unit, SPELL_DH_COLLAPSING_STAR_COUNTER,
                TRIGGERED_IGNORE_CAST_IN_PROGRESS | TRIGGERED_DONT_REPORT_CAST_ERROR);

        if (unit->HasAura(SPELL_DH_VOID_METAMORPHOSIS_BUFF) && unit->HasAura(SPELL_DH_EMPTINESS_PASSIVE))
            unit->CastSpell(unit, SPELL_DH_EMPTINESS_BUFF,
                TRIGGERED_IGNORE_CAST_IN_PROGRESS | TRIGGERED_DONT_REPORT_CAST_ERROR);

        if (Aura* fos = unit->GetAura(SPELL_DH_FEAST_OF_SOULS_PASSIVE))
            if (auto* script = fos->GetScript<spell_dh_feast_of_souls_tracker>())
                script->AddStack(unit);

        at->Remove();
    }

    void OnInitialize() override
    {
        if (Unit* caster = at->GetCaster())
        {
            if (caster->HasAura(SPELL_DH_SHATTERED_SOULS_VENGEANCE))
                caster->CastSpell(caster, SPELL_DH_SOUL_FRAGMENT_COUNTER, TRIGGERED_IGNORE_CAST_IN_PROGRESS | TRIGGERED_DONT_REPORT_CAST_ERROR);
            else if (caster->HasAura(SPELL_DH_SHATTERED_SOULS_DEVOURER))
                caster->CastSpell(caster, SPELL_DH_SOUL_FRAGMENTS_DEVOURER_COUNTER, TRIGGERED_IGNORE_CAST_IN_PROGRESS | TRIGGERED_DONT_REPORT_CAST_ERROR);
        }
    }

    void OnRemove() override
    {
        if (Unit* caster = at->GetCaster())
        {
            caster->RemoveAuraFromStack(SPELL_DH_SOUL_FRAGMENT_COUNTER);
            caster->RemoveAuraFromStack(SPELL_DH_SOUL_FRAGMENTS_DEVOURER_COUNTER);
        }
    }
};

using at_dh_shattered_souls_devourer = at_dh_shattered_souls<SPELL_DH_CONSUME_SOUL_DEVOURER>;
using at_dh_shattered_souls_havoc_demon = at_dh_shattered_souls<SPELL_DH_CONSUME_SOUL_HAVOC_DEMON>;
using at_dh_shattered_souls_havoc_lesser = at_dh_shattered_souls<SPELL_DH_CONSUME_SOUL_HAVOC_LESSER>;
using at_dh_shattered_souls_havoc_shattered = at_dh_shattered_souls<SPELL_DH_CONSUME_SOUL_HAVOC_SHATTERED>;
using at_dh_shattered_souls_vengeance_demon = at_dh_shattered_souls<SPELL_DH_CONSUME_SOUL_VENGEANCE_DEMON>;
using at_dh_shattered_souls_vengeance_lesser = at_dh_shattered_souls<SPELL_DH_CONSUME_SOUL_VENGEANCE_LESSER>;
using at_dh_shattered_souls_vengeance_shattered = at_dh_shattered_souls<SPELL_DH_CONSUME_SOUL_VENGEANCE_SHATTERED>;

// 1234796 - Shift
class spell_dh_shift : public SpellScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_DH_SHIFT_VISUAL_DEST, SPELL_DH_SHIFT_CHARGE });
    }

    void HandleEffectDummy(SpellEffIndex /*effIndex*/) const
    {
        Unit* caster = GetCaster();
        WorldLocation const& target = *GetHitDest();

        CastSpellExtraArgs args;
        args.TriggerFlags = TRIGGERED_IGNORE_CAST_IN_PROGRESS | TRIGGERED_DONT_REPORT_CAST_ERROR;
        args.TriggeringSpell = GetSpell();

        caster->CastSpell(target, SPELL_DH_SHIFT_VISUAL_DEST, args);
        caster->CastSpell(target, SPELL_DH_SHIFT_CHARGE, args);
    }

    void Register() override
    {
        OnEffectHit += SpellEffectFn(spell_dh_shift::HandleEffectDummy, EFFECT_0, SPELL_EFFECT_DUMMY);
    }
};

// 207407 - Soul Carver
class spell_dh_soul_carver : public SpellScript
{
    bool Validate(SpellInfo const* spellInfo) override
    {
        return ValidateSpellEffect({ { spellInfo->Id, EFFECT_2 } })
            && spell_dh_shattered_souls_base_lesser::Validate();
    }

    void HandleSoulFragments(SpellEffIndex /*effIndex*/) const
    {
        spell_dh_shattered_souls_base_lesser::CreateFragments(GetHitUnit(), GetCaster(), GetEffectInfo(EFFECT_2).CalcValueAsInt(GetCaster()));
    }

    void Register() override
    {
        OnEffectHitTarget += SpellEffectFn(spell_dh_soul_carver::HandleSoulFragments, EFFECT_1, SPELL_EFFECT_SCHOOL_DAMAGE);
    }
};

class spell_dh_soul_carver_aura : public AuraScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return spell_dh_shattered_souls_base_lesser::Validate();
    }

    void HandleEffectPeriodic(AuraEffect const* /*aurEff*/) const
    {
        if (Unit* caster = GetCaster())
            spell_dh_shattered_souls_base_lesser::CreateFragments(GetTarget(), caster, GetEffectInfo(EFFECT_3).CalcValueAsInt(caster));
    }

    void Register() override
    {
        OnEffectPeriodic += AuraEffectPeriodicFn(spell_dh_soul_carver_aura::HandleEffectPeriodic, EFFECT_0, SPELL_AURA_PERIODIC_DAMAGE);
    }
};

// 210788 - Soul Fragments
class spell_dh_soul_fragments_damage_taken_tracker : public AuraScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellEffect({ { SPELL_DH_SHEAR_PASSIVE, EFFECT_3 } });
    }

    bool Load() override
    {
        AuraEffect const* seconds = GetUnitOwner()->GetAuraEffect(SPELL_DH_SHEAR_PASSIVE, EFFECT_3);
        if (!seconds)
            return false;

        _damagePerSecond.resize(seconds->GetAmountAsInt());
        return !_damagePerSecond.empty();
    }

    static bool CheckProc(AuraScript const&, ProcEventInfo const& eventInfo)
    {
        return eventInfo.GetDamageInfo() != nullptr;
    }

    void Update(AuraEffect* /*aurEff*/)
    {
        // Move backwards all datas by one from [23][0][0][0][0] -> [0][23][0][0][0]
        std::move_backward(_damagePerSecond.begin(), std::next(_damagePerSecond.begin(), std::ssize(_damagePerSecond) - 1), _damagePerSecond.end());
        _damagePerSecond[0] = 0;
    }

    void HandleCalcAmount(AuraEffect const* /*aurEff*/, SpellEffectValue& amount, bool& canBeRecalculated)
    {
        canBeRecalculated = true;
        amount = std::reduce(_damagePerSecond.begin(), _damagePerSecond.end(), 0u);
    }

    void HandleProc(AuraEffect const* /*aurEff*/, ProcEventInfo const& eventInfo)
    {
        _damagePerSecond[0] += eventInfo.GetDamageInfo()->GetDamage();
    }

    void Register() override
    {
        DoCheckProc += AuraCheckProcFn(spell_dh_soul_fragments_damage_taken_tracker::CheckProc);
        OnEffectProc += AuraEffectProcFn(spell_dh_soul_fragments_damage_taken_tracker::HandleProc, EFFECT_0, SPELL_AURA_PERIODIC_DUMMY);
        DoEffectCalcAmount += AuraEffectCalcAmountFn(spell_dh_soul_fragments_damage_taken_tracker::HandleCalcAmount, EFFECT_0, SPELL_AURA_PERIODIC_DUMMY);
        OnEffectUpdatePeriodic += AuraEffectUpdatePeriodicFn(spell_dh_soul_fragments_damage_taken_tracker::Update, EFFECT_0, SPELL_AURA_PERIODIC_DUMMY);
    }

private:
    std::vector<uint32> _damagePerSecond;
};

// 389711 - Soulmonger
class spell_dh_soulmonger : public AuraScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellEffect({ { SPELL_DH_SOULMONGER_ABSORB, EFFECT_0 } });
    }

    static bool CheckProc(AuraScript const&, ProcEventInfo const& eventInfo)
    {
        return eventInfo.GetActionTarget()->HealthAbovePctHealed(100, eventInfo.GetHealInfo()->GetHeal());
    }

    static void HandleEffectProc(AuraScript const&, AuraEffect const* aurEff, ProcEventInfo const& eventInfo)
    {
        Unit* target = eventInfo.GetActionTarget();
        SpellEffectValue amount = eventInfo.GetHealInfo()->GetHeal();
        if (AuraEffect const* existingAbsorb = target->GetAuraEffect(SPELL_DH_SOULMONGER_ABSORB, EFFECT_0))
            amount += existingAbsorb->GetAmount();

        amount = std::min(amount, SpellEffectValue(target->CountPctFromMaxHealth(aurEff->GetAmount())));

        target->CastSpell(target, SPELL_DH_SOULMONGER_ABSORB, CastSpellExtraArgsInit{
            .TriggerFlags = TRIGGERED_IGNORE_CAST_IN_PROGRESS | TRIGGERED_DONT_REPORT_CAST_ERROR,
            .TriggeringAura = aurEff,
            .SpellValueOverrides = { { SPELLVALUE_BASE_POINT0, amount } }
        });
    }

    void Register() override
    {
        DoCheckProc += AuraCheckProcFn(spell_dh_soulmonger::CheckProc);
        OnEffectProc += AuraEffectProcFn(spell_dh_soulmonger::HandleEffectProc, EFFECT_0, SPELL_AURA_DUMMY);
    }
};

// 391166 - Soul Furnace
class spell_dh_soul_furnace : public AuraScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_DH_SOUL_FURNACE_DAMAGE_BUFF });
    }

    void CalculateSpellMod(AuraEffect const* /*aurEff*/, AuraEffectHandleModes /*mode*/)
    {
        if (GetStackAmount() == GetAura()->CalcMaxStackAmount())
        {
            GetTarget()->CastSpell(GetTarget(), SPELL_DH_SOUL_FURNACE_DAMAGE_BUFF, true);
            Remove();
        }
    }

    void Register() override
    {
        AfterEffectApply += AuraEffectApplyFn(spell_dh_soul_furnace::CalculateSpellMod, EFFECT_0, SPELL_AURA_DUMMY, AURA_EFFECT_HANDLE_REAL_OR_REAPPLY_MASK);
    }
};

// 339424 - Soul Furnace
class spell_dh_soul_furnace_conduit : public AuraScript
{
    void CalculateSpellMod(AuraEffect const* aurEff, SpellModifier*& spellMod)
    {
        if (aurEff->GetAmountAsInt() == 10)
        {
            if (!spellMod)
            {
                spellMod = new SpellPctModifierByClassMask(SpellModOp::HealingAndDamage, GetId(), GetAura(), flag128(0x80000000));
                static_cast<SpellPctModifierByClassMask*>(spellMod)->value = GetEffect(EFFECT_1)->GetAmount() + 1;
            }
        }
    }

    void Register() override
    {
        DoEffectCalcSpellMod += AuraEffectCalcSpellModFn(spell_dh_soul_furnace_conduit::CalculateSpellMod, EFFECT_0, SPELL_AURA_DUMMY);
    }
};

// 395446 - Soul Sigils
class spell_dh_soul_sigils : public AuraScript
{
    void HandleOnProc(AuraEffect const* aurEff, ProcEventInfo const& eventInfo) const
    {
        spell_dh_shattered_souls_base_lesser::CreateFragments(eventInfo.GetActionTarget(), eventInfo.GetActor(), aurEff->GetAmountAsInt());
    }

    void Register() override
    {
        OnEffectProc += AuraEffectProcFn(spell_dh_soul_sigils::HandleOnProc, EFFECT_0, SPELL_AURA_DUMMY);
    }
};

// 202138 - Sigil of Chains
// 204596 - Sigil of Flame
// 207684 - Sigil of Misery
// 202137 - Sigil of Silence
// 390163 - Sigil of Spite
template<uint32 TriggerSpellId, uint32 TriggerSpellId2 = 0>
struct areatrigger_dh_generic_sigil : AreaTriggerAI
{
    using AreaTriggerAI::AreaTriggerAI;

    void OnRemove() override
    {
        if (Unit* caster = at->GetCaster())
        {
            caster->CastSpell(at->GetPosition(), TriggerSpellId, TRIGGERED_IGNORE_CAST_IN_PROGRESS | TRIGGERED_DONT_REPORT_CAST_ERROR);
            if constexpr (TriggerSpellId2 != 0)
                caster->CastSpell(at->GetPosition(), TriggerSpellId2, TRIGGERED_IGNORE_CAST_IN_PROGRESS | TRIGGERED_DONT_REPORT_CAST_ERROR);
        }
    }
};

using at_dh_elysian_decree = areatrigger_dh_generic_sigil<SPELL_DH_ELYSIAN_DECREE_AOE>;
using areatrigger_dh_sigil_of_chains = areatrigger_dh_generic_sigil<SPELL_DH_SIGIL_OF_CHAINS_TARGET_SELECT, SPELL_DH_SIGIL_OF_CHAINS_VISUAL>;
using areatrigger_dh_sigil_of_flame = areatrigger_dh_generic_sigil<SPELL_DH_SIGIL_OF_FLAME_AOE, SPELL_DH_SIGIL_OF_FLAME_VISUAL>;
using areatrigger_dh_sigil_of_silence = areatrigger_dh_generic_sigil<SPELL_DH_SIGIL_OF_SILENCE_AOE>;
using areatrigger_dh_sigil_of_misery = areatrigger_dh_generic_sigil<SPELL_DH_SIGIL_OF_MISERY_AOE>;
using areatrigger_dh_sigil_of_spite = areatrigger_dh_generic_sigil<SPELL_DH_SIGIL_OF_SPITE_AOE>;

// 208673 - Sigil of Chains
class spell_dh_sigil_of_chains : public SpellScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_DH_SIGIL_OF_CHAINS_SLOW, SPELL_DH_SIGIL_OF_CHAINS_GRIP });
    }

    void HandleEffectHitTarget(SpellEffIndex /*effIndex*/)
    {
        if (WorldLocation const* loc = GetExplTargetDest())
        {
            GetCaster()->CastSpell(GetHitUnit(), SPELL_DH_SIGIL_OF_CHAINS_SLOW, true);
            GetHitUnit()->CastSpell(loc->GetPosition(), SPELL_DH_SIGIL_OF_CHAINS_GRIP, true);
        }
    }

    void Register() override
    {
        OnEffectHitTarget += SpellEffectFn(spell_dh_sigil_of_chains::HandleEffectHitTarget, EFFECT_0, SPELL_EFFECT_DUMMY);
    }
};

// 204596 - Sigil of Flame
class spell_dh_sigil_of_flame : public SpellScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_DH_SIGIL_OF_FLAME_ENERGIZE });
    }

    void HandleEnergize(SpellEffIndex /*effIndex*/) const
    {
        Unit* caster = GetCaster();
        caster->CastSpell(caster, SPELL_DH_SIGIL_OF_FLAME_ENERGIZE, CastSpellExtraArgsInit{
            .TriggerFlags = TRIGGERED_IGNORE_CAST_IN_PROGRESS | TRIGGERED_DONT_REPORT_CAST_ERROR,
            .TriggeringSpell = GetSpell()
        });
    }

    void Register() override
    {
        OnEffectHit += SpellEffectFn(spell_dh_sigil_of_flame::HandleEnergize, EFFECT_0, SPELL_EFFECT_DUMMY);
    }
};

// Called by 204598 - Sigil of Flame
class spell_dh_student_of_suffering : public SpellScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_DH_STUDENT_OF_SUFFERING_TALENT, SPELL_DH_STUDENT_OF_SUFFERING_AURA });
    }

    bool Load() override
    {
        return GetCaster()->HasAura(SPELL_DH_STUDENT_OF_SUFFERING_TALENT);
    }

    void HandleStudentOfSuffering() const
    {
        GetCaster()->CastSpell(GetCaster(), SPELL_DH_STUDENT_OF_SUFFERING_AURA, TRIGGERED_IGNORE_CAST_IN_PROGRESS | TRIGGERED_DONT_REPORT_CAST_ERROR);
    }

    void Register() override
    {
        AfterCast += SpellCastFn(spell_dh_student_of_suffering::HandleStudentOfSuffering);
    }
};

// Called by 198793 - Vengeful Retreat
class spell_dh_tactical_retreat : public SpellScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_DH_TACTICAL_RETREAT_TALENT, SPELL_DH_TACTICAL_RETREAT_ENERGIZE });
    }

    bool Load() override
    {
        return GetCaster()->HasAura(SPELL_DH_TACTICAL_RETREAT_TALENT);
    }

    void Energize() const
    {
        GetCaster()->CastSpell(GetCaster(), SPELL_DH_TACTICAL_RETREAT_ENERGIZE, CastSpellExtraArgsInit{
            .TriggerFlags = TRIGGERED_IGNORE_CAST_IN_PROGRESS | TRIGGERED_DONT_REPORT_CAST_ERROR,
            .TriggeringSpell = GetSpell()
        });
    }

    void Register() override
    {
        AfterCast += SpellCastFn(spell_dh_tactical_retreat::Energize);
    }
};

// 444931 - Unhindered Assault
class spell_dh_unhindered_assault : public AuraScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_DH_FELBLADE });
    }

    void HandleOnProc(AuraEffect const* /*aurEff*/, ProcEventInfo const& /*eventInfo*/) const
    {
        GetTarget()->GetSpellHistory()->ResetCooldown(SPELL_DH_FELBLADE, true);
    }

    void Register() override
    {
        OnEffectProc += AuraEffectProcFn(spell_dh_unhindered_assault::HandleOnProc, EFFECT_0, SPELL_AURA_DUMMY);
    }
};

// 198813 - Vengeful Retreat
class spell_dh_vengeful_retreat_damage : public SpellScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_DH_VENGEFUL_BONDS, SPELL_DH_HUNGERING_SLASH_VOIDSTEP, SPELL_DH_VOIDSTEP_DAMAGE });
    }

    void HandleVengefulBonds(std::list<WorldObject*>& targets)
    {
        if (!GetCaster()->HasAura(SPELL_DH_VENGEFUL_BONDS))
            targets.clear();
    }

    void HandleVoidstep() const
    {
        Unit* caster = GetCaster();

        // Consume the "temporary charge" (Evasive Action aura) immediately upon cast 
        // to prevent the player from multi-casting Vengeful Retreat infinitely!
        caster->RemoveAurasDueToSpell(444929);

        if (!caster->HasAura(SPELL_DH_HUNGERING_SLASH_VOIDSTEP))
            return;

        caster->CastSpell(caster, SPELL_DH_VOIDSTEP_DAMAGE,
            TRIGGERED_IGNORE_CAST_IN_PROGRESS | TRIGGERED_DONT_REPORT_CAST_ERROR);
        caster->RemoveAurasDueToSpell(SPELL_DH_HUNGERING_SLASH_VOIDSTEP);
    }

    void Register() override
    {
        OnObjectAreaTargetSelect += SpellObjectAreaTargetSelectFn(spell_dh_vengeful_retreat_damage::HandleVengefulBonds, EFFECT_0, TARGET_UNIT_SRC_AREA_ENEMY);
        AfterCast += SpellCastFn(spell_dh_vengeful_retreat_damage::HandleVoidstep);
    }
};

// 452409 - Violent Transformation
class spell_dh_violent_transformation : public AuraScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_DH_SIGIL_OF_FLAME, SPELL_DH_VENGEANCE_DEMON_HUNTER, SPELL_DH_FEL_DEVASTATION, SPELL_DH_IMMOLATION_AURA });
    }

    void HandleOnProc(AuraEffect const* /*aurEff*/, ProcEventInfo const& /*eventInfo*/) const
    {
        Unit* target = GetTarget();
        target->GetSpellHistory()->RestoreCharge(sSpellMgr->AssertSpellInfo(SPELL_DH_SIGIL_OF_FLAME, GetCastDifficulty())->ChargeCategoryId);

        if (target->HasAura(SPELL_DH_VENGEANCE_DEMON_HUNTER))
            target->GetSpellHistory()->ResetCooldown(SPELL_DH_FEL_DEVASTATION, true);
        else
            target->GetSpellHistory()->RestoreCharge(sSpellMgr->AssertSpellInfo(SPELL_DH_IMMOLATION_AURA, GetCastDifficulty())->ChargeCategoryId);
    }

    void Register() override
    {
        OnEffectProc += AuraEffectProcFn(spell_dh_violent_transformation::HandleOnProc, EFFECT_0, SPELL_AURA_DUMMY);
    }
};

// 1245412 - Voidblade
class spell_dh_voidblade : public SpellScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_DH_VOIDBLADE_CHARGE, SPELL_DH_VOIDBLADE_DAMAGE, SPELL_DH_HUNGERING_SLASH_PASSIVE, SPELL_DH_HUNGERING_SLASH_OVERRIDE });
    }

    void HandleCharge(SpellEffIndex /*effIndex*/) const
    {
        uint32 spellToCast = GetCaster()->IsWithinMeleeRange(GetHitUnit()) ? SPELL_DH_VOIDBLADE_DAMAGE : SPELL_DH_VOIDBLADE_CHARGE;
        GetCaster()->CastSpell(GetHitUnit(), spellToCast, CastSpellExtraArgsInit{
            .TriggerFlags = TRIGGERED_IGNORE_CAST_IN_PROGRESS | TRIGGERED_DONT_REPORT_CAST_ERROR,
            .TriggeringSpell = GetSpell()
        });
    }

    void HandleAfterCast() const
    {
        Unit* caster = GetCaster();
        if (caster->HasAura(SPELL_DH_HUNGERING_SLASH_PASSIVE))
            caster->CastSpell(caster, SPELL_DH_HUNGERING_SLASH_OVERRIDE,
                TRIGGERED_IGNORE_CAST_IN_PROGRESS | TRIGGERED_DONT_REPORT_CAST_ERROR);
    }

    void Register() override
    {
        OnEffectHitTarget += SpellEffectFn(spell_dh_voidblade::HandleCharge, EFFECT_0, SPELL_EFFECT_DUMMY);
        AfterCast += SpellCastFn(spell_dh_voidblade::HandleAfterCast);
    }
};

// 1241285 - Voidblade Charge
class spell_dh_voidblade_charge : public SpellScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_DH_VOIDBLADE_DAMAGE });
    }

    void HandleDamage(SpellEffIndex /*effIndex*/) const
    {
        GetCaster()->CastSpell(GetHitUnit(), SPELL_DH_VOIDBLADE_DAMAGE, CastSpellExtraArgsInit{
            .TriggerFlags = TRIGGERED_IGNORE_CAST_IN_PROGRESS | TRIGGERED_DONT_REPORT_CAST_ERROR,
            .TriggeringSpell = GetSpell()
        });
    }

    void Register() override
    {
        OnEffectHitTarget += SpellEffectFn(spell_dh_voidblade_charge::HandleDamage, EFFECT_0, SPELL_EFFECT_CHARGE);
    }
};

// 1240202 - Voidglare Boon (attached to 473728 - Void Ray)
class spell_dh_voidglare_boon : public AuraScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_DH_VOIDGLARE_BOON_TALENT, SPELL_DH_VOIDGLARE_BOON_ENERGIZE });
    }

    bool Load() override
    {
        return GetUnitOwner()->HasAura(SPELL_DH_VOIDGLARE_BOON_TALENT);
    }

    void HandleEffectRemove(AuraEffect const* /*aurEff*/, AuraEffectHandleModes /*mode*/) const
    {
        if (GetTargetApplication()->GetRemoveMode() != AURA_REMOVE_BY_EXPIRE)
            return;

        Unit* target = GetTarget();
        target->CastSpell(target, SPELL_DH_VOIDGLARE_BOON_ENERGIZE, TRIGGERED_IGNORE_CAST_IN_PROGRESS | TRIGGERED_DONT_REPORT_CAST_ERROR);
    }

    void Register() override
    {
        AfterEffectRemove += AuraEffectRemoveFn(spell_dh_voidglare_boon::HandleEffectRemove, EFFECT_0, SPELL_AURA_PERIODIC_TRIGGER_SPELL, AURA_EFFECT_HANDLE_REAL);
    }
};

static void SetVoidMetaReapOverride(Unit* target, bool enable)
{
    if (Aura* metaBuff = target->GetAura(SPELL_DH_VOID_METAMORPHOSIS_BUFF))
        for (AuraEffect* eff : metaBuff->GetAuraEffects())
            if (eff->GetAuraType() == SPELL_AURA_OVERRIDE_ACTIONBAR_SPELLS && uint32(eff->GetMiscValue()) == SPELL_DH_REAP)
            {
                eff->ChangeAmount(enable ? SPELL_DH_CULL : 0);
                break;
            }
}

// 1253304 - Voidfall passive: Consume hit has 35% chance to grant a stack of 1256301 (max 3)
class spell_dh_voidfall_passive : public AuraScript
{
    uint32 _meteorCount = 0;

    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_DH_VOIDFALL_STACK });
    }

    static bool CheckProc(AuraScript const& script, ProcEventInfo const& eventInfo)
    {
        if (!eventInfo.GetSpellInfo())
            return false;
        int32 chance = static_cast<spell_dh_voidfall_passive const&>(script).GetEffect(EFFECT_2)->GetAmount();
        return roll_chance(chance);
    }

    void HandleProc(AuraEffect const* /*aurEff*/, ProcEventInfo const& /*eventInfo*/) const
    {
        GetTarget()->CastSpell(GetTarget(), SPELL_DH_VOIDFALL_STACK,
            TRIGGERED_IGNORE_CAST_IN_PROGRESS | TRIGGERED_DONT_REPORT_CAST_ERROR);
    }

public:
    uint32 IncrementMeteorCount() { return ++_meteorCount; }

    void Register() override
    {
        DoCheckProc += AuraCheckProcFn(spell_dh_voidfall_passive::CheckProc);
        OnEffectProc += AuraEffectProcFn(spell_dh_voidfall_passive::HandleProc, EFFECT_0, SPELL_AURA_DUMMY);
    }
};

// 1253676 - Doomsayer passive: tracks combat entry time for the 10s window
class spell_dh_doomsayer_passive : public AuraScript
{
    TimePoint _combatEnterTime = TimePoint::min();

    void HandleProc(AuraEffect const* /*aurEff*/, ProcEventInfo const& /*eventInfo*/)
    {
        if (_combatEnterTime == TimePoint::min())
            _combatEnterTime = GameTime::Now();
        // Reset if previous combat ended (gap > window means new fight)
        else if (std::chrono::duration_cast<Milliseconds>(GameTime::Now() - _combatEnterTime).count() > 30000)
            _combatEnterTime = GameTime::Now();
    }

public:
    bool IsWithinCombatWindow(Unit* target) const
    {
        if (_combatEnterTime == TimePoint::min())
            return false;
        // EFFECT_1 BP=2, 1265768 duration=5000ms -> window = 2 * 5000 = 10000ms
        int32 multiplier = target->GetAuraEffectAmount(SPELL_DH_DOOMSAYER_PASSIVE, EFFECT_1);
        int64 windowMs = int64(multiplier) * 5000;
        return std::chrono::duration_cast<Milliseconds>(GameTime::Now() - _combatEnterTime).count() <= windowMs;
    }

    void Register() override
    {
        OnEffectProc += AuraEffectProcFn(spell_dh_doomsayer_passive::HandleProc, EFFECT_0, SPELL_AURA_DUMMY);
    }
};

// 1265768 - Doomsayer buff: next harmful spell fires 3 meteors at target
class spell_dh_doomsayer_buff : public AuraScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_DH_VOIDFALL_METEOR_VENGEANCE, SPELL_DH_VOIDFALL_METEOR_HAVOC });
    }

    void HandleProc(AuraEffect const* /*aurEff*/, ProcEventInfo const& eventInfo) const
    {
        Unit* caster = GetTarget();
        Unit* target = eventInfo.GetActionTarget();
        if (!target)
            return;

        bool isVengeance = caster->HasAura(SPELL_DH_VENGEANCE_SPEC) && !caster->HasAura(SPELL_DH_DEVOURER_SPEC);
        int32 meteors = caster->GetAuraEffectAmount(SPELL_DH_DOOMSAYER_PASSIVE, EFFECT_0);

        auto* vfAura = caster->GetAura(SPELL_DH_VOIDFALL_PASSIVE);
        auto* vfScript = (vfAura && caster->HasAura(SPELL_DH_WORLD_KILLER_PASSIVE)) ? vfAura->GetScript<spell_dh_voidfall_passive>() : nullptr;

        for (int32 i = 0; i < meteors; ++i)
        {
            uint32 meteorCount = 1;
            if (vfScript)
                meteorCount = vfScript->IncrementMeteorCount();

            bool bigMeteor = vfScript && (meteorCount % 3 == 0);
            uint32 meteorSpell;
            if (bigMeteor)
                meteorSpell = isVengeance ? SPELL_DH_WORLD_KILLER_METEOR_VENGEANCE : SPELL_DH_WORLD_KILLER_METEOR_HAVOC;
            else
                meteorSpell = isVengeance ? SPELL_DH_VOIDFALL_METEOR_VENGEANCE : SPELL_DH_VOIDFALL_METEOR_HAVOC;

            caster->CastSpell(target, meteorSpell,
                TRIGGERED_IGNORE_CAST_IN_PROGRESS | TRIGGERED_DONT_REPORT_CAST_ERROR);

            if (caster->HasAura(SPELL_DH_FINAL_HOUR_PASSIVE))
                caster->CastSpell(caster, SPELL_DH_FINAL_HOUR_BUFF,
                    TRIGGERED_IGNORE_CAST_IN_PROGRESS | TRIGGERED_DONT_REPORT_CAST_ERROR);
        }
    }

    void Register() override
    {
        OnEffectProc += AuraEffectProcFn(spell_dh_doomsayer_buff::HandleProc, EFFECT_0, SPELL_AURA_DUMMY);
    }
};

// 473728 - Void Ray
class spell_dh_void_ray : public AuraScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_DH_VOID_RAY_DAMAGE, SPELL_DH_DOOMSAYER_BUFF });
    }

    void HandleApply(AuraEffect const* /*aurEff*/, AuraEffectHandleModes /*mode*/)
    {
        Unit* target = GetTarget();
        if (!target->HasAura(SPELL_DH_DOOMSAYER_PASSIVE))
            return;

        if (!target->IsInCombat())
            target->CastSpell(target, SPELL_DH_DOOMSAYER_BUFF,
                TRIGGERED_IGNORE_CAST_IN_PROGRESS | TRIGGERED_DONT_REPORT_CAST_ERROR);
        else if (Aura const* passive = target->GetAura(SPELL_DH_DOOMSAYER_PASSIVE))
            if (auto* script = passive->GetScript<spell_dh_doomsayer_passive>())
                if (script->IsWithinCombatWindow(target))
                    target->CastSpell(target, SPELL_DH_DOOMSAYER_BUFF,
                        TRIGGERED_IGNORE_CAST_IN_PROGRESS | TRIGGERED_DONT_REPORT_CAST_ERROR);
    }

    void HandleEffectPeriodic(AuraEffect const* aurEff) const
    {
        Unit* caster = GetCaster();
        if (!caster)
            return;

        TriggerCastFlags flags = TriggerCastFlags(TRIGGERED_IGNORE_CAST_IN_PROGRESS | TRIGGERED_DONT_REPORT_CAST_ERROR);
        if (caster->HasAura(SPELL_DH_VOID_METAMORPHOSIS_BUFF))
            flags = TriggerCastFlags(flags | TRIGGERED_IGNORE_POWER_COST);

        caster->CastSpell(nullptr, SPELL_DH_VOID_RAY_DAMAGE, CastSpellExtraArgsInit{
            .TriggerFlags = flags,
            .TriggeringAura = aurEff
            });
    }

    void HandleRemove(AuraEffect const* /*aurEff*/, AuraEffectHandleModes /*mode*/)
    {
        if (GetTargetApplication()->GetRemoveMode() != AURA_REMOVE_BY_EXPIRE)
            return;

        Unit* target = GetTarget();

        if (target->HasAura(SPELL_DH_METEORIC_RISE_PASSIVE))
            target->CastSpell(target, SPELL_DH_VOIDFALL_STACK,
                TRIGGERED_IGNORE_CAST_IN_PROGRESS | TRIGGERED_DONT_REPORT_CAST_ERROR);

        // Moment of Craving: reset Reap CD + apply buff (+6 fragments + Eradicate override)
        if (target->HasAura(SPELL_DH_MOMENT_OF_CRAVING_TALENT))
        {
            target->GetSpellHistory()->ResetCooldown(SPELL_DH_REAP, true);
            target->GetSpellHistory()->ResetCooldown(SPELL_DH_CULL, true);
            target->GetSpellHistory()->ResetCooldown(SPELL_DH_ERADICATE, true);
            target->CastSpell(target, SPELL_DH_MOMENT_OF_CRAVING_BUFF,
                TRIGGERED_IGNORE_CAST_IN_PROGRESS | TRIGGERED_DONT_REPORT_CAST_ERROR);
            target->CastSpell(target, SPELL_DH_ERADICATE_OVERRIDE,
                TRIGGERED_IGNORE_CAST_IN_PROGRESS | TRIGGERED_DONT_REPORT_CAST_ERROR);
            SetVoidMetaReapOverride(target, false);
        }
    }

    void Register() override
    {
        AfterEffectApply += AuraEffectApplyFn(spell_dh_void_ray::HandleApply, EFFECT_0, SPELL_AURA_PERIODIC_TRIGGER_SPELL, AURA_EFFECT_HANDLE_REAL);
        OnEffectPeriodic += AuraEffectPeriodicFn(spell_dh_void_ray::HandleEffectPeriodic, EFFECT_0, SPELL_AURA_PERIODIC_TRIGGER_SPELL);
        AfterEffectRemove += AuraEffectRemoveFn(spell_dh_void_ray::HandleRemove, EFFECT_0, SPELL_AURA_PERIODIC_TRIGGER_SPELL, AURA_EFFECT_HANDLE_REAL);
    }
};

// 179057 - Chaos Nova
class spell_dh_wave_of_debilitation : public AuraScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_DH_WAVE_OF_DEBILITATION_TALENT, SPELL_DH_WAVE_OF_DEBILITATION_SLOW });
    }

    bool Load() override
    {
        return GetCaster()->HasAura(SPELL_DH_WAVE_OF_DEBILITATION_TALENT);
    }

    void OnRemove(AuraEffect const* /*aurEff*/, AuraEffectHandleModes /*mode*/)
    {
        if (GetTargetApplication()->GetRemoveMode() == AURA_REMOVE_BY_DEATH)
            return;

        if (Unit* caster = GetCaster())
            caster->CastSpell(GetTarget(), SPELL_DH_WAVE_OF_DEBILITATION_SLOW, TRIGGERED_IGNORE_CAST_IN_PROGRESS | TRIGGERED_DONT_REPORT_CAST_ERROR);
    }

    void Register() override
    {
        AfterEffectRemove += AuraEffectRemoveFn(spell_dh_wave_of_debilitation::OnRemove, EFFECT_0, SPELL_AURA_MOD_STUN, AURA_EFFECT_HANDLE_REAL);
    }
};

// Fel Rush - 195072
class spell_dh_fel_rush : public SpellScript
{

    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        if (!sSpellMgr->GetSpellInfo(SPELL_DH_FEL_RUSH_GROUND, DIFFICULTY_NONE))
            return false;
        if (!sSpellMgr->GetSpellInfo(SPELL_DH_FEL_RUSH_WATER_AIR, DIFFICULTY_NONE))
            return false;
        return true;
    }

    void HandleDashGround(SpellEffIndex /*effIndex*/)
    {
        if (Unit* caster = GetCaster())
        {
            if (!caster->IsFalling() || caster->IsInWater())
            {
                caster->RemoveAurasDueToSpell(SPELL_DH_GLIDE);
                caster->CastSpell(caster, SPELL_DH_FEL_RUSH_GROUND, true);
                caster->CastSpell(caster, SPELL_DH_FEL_RUSH_DAMAGE, TRIGGERED_FULL_DEBUG_MASK);

                if (caster->HasAura(SPELL_DH_MOMENTUM_AURA))
                    caster->CastSpell(nullptr, SPELL_DH_MOMENTUM, true);
            }
            caster->GetSpellHistory()->AddCooldown(GetSpellInfo()->Id, 0, std::chrono::milliseconds(750));
        }
    }

    void HandleDashAir(SpellEffIndex /*effIndex*/)
    {
        if (Unit* caster = GetCaster())
        {
            if (caster->IsFalling())
            {
                caster->RemoveAurasDueToSpell(SPELL_DH_GLIDE);
                caster->SetDisableGravity(true);
                caster->CastSpell(caster, SPELL_DH_FEL_RUSH_WATER_AIR, true);
                caster->CastSpell(caster, SPELL_DH_FEL_RUSH_DAMAGE, TRIGGERED_FULL_DEBUG_MASK);

                if (caster->HasAura(SPELL_DH_MOMENTUM_AURA))
                    caster->CastSpell(nullptr, SPELL_DH_MOMENTUM, true);

                caster->GetSpellHistory()->AddCooldown(GetSpellInfo()->Id, 0, std::chrono::milliseconds(750));
            }
        }
    }

    void HandleCast()
    {
        if (Unit* caster = GetCaster())
            if (caster->HasAura(427794))
                caster->CastSpell(caster, 427793, true);
    }

    void Register() override
    {
        OnEffectHitTarget += SpellEffectFn(spell_dh_fel_rush::HandleDashGround, EFFECT_0, SPELL_EFFECT_DUMMY);
        OnEffectHitTarget += SpellEffectFn(spell_dh_fel_rush::HandleDashAir, EFFECT_1, SPELL_EFFECT_DUMMY);
    }
};

// Fel Rush air - 197923
class spell_dh_fel_rush_dash_aura : public AuraScript
{
    void AfterRemove(AuraEffect const* /*aurEff*/, AuraEffectHandleModes /*mode*/)
    {
        if (Unit* caster = GetCaster())
            caster->GetScheduler().Schedule(100ms, [caster](TaskContext& /*context*/)
                {
                    if (!caster->HasAura(SPELL_DH_FEL_RUSH_WATER_AIR))
                        caster->SetDisableGravity(false);
                });
    }

    void CalcSpeed(AuraEffect const* /*aurEff*/, SpellEffectValue& amount, bool& /*canBeRecalculated*/)
    {
        amount = 1250;
        RefreshDuration();
    }

    void Register() override
    {
        DoEffectCalcAmount += AuraEffectCalcAmountFn(spell_dh_fel_rush_dash_aura::CalcSpeed, EFFECT_1, SPELL_AURA_MOD_SPEED_NO_CONTROL);
        DoEffectCalcAmount += AuraEffectCalcAmountFn(spell_dh_fel_rush_dash_aura::CalcSpeed, EFFECT_3, SPELL_AURA_MOD_MINIMUM_SPEED);
        AfterEffectRemove += AuraEffectRemoveFn(spell_dh_fel_rush_dash_aura::AfterRemove, EFFECT_9, SPELL_AURA_MOD_MINIMUM_SPEED_RATE, AURA_EFFECT_HANDLE_SEND_FOR_CLIENT_MASK);
    }
};

class spell_dh_fel_rush_dash : public SpellScript
{
    void PreventTrigger(SpellEffIndex effIndex)
    {
        PreventHitEffect(effIndex);
    }

    void Register() override
    {
        OnEffectLaunch += SpellEffectFn(spell_dh_fel_rush_dash::PreventTrigger, EFFECT_6, SPELL_EFFECT_TRIGGER_SPELL);
        OnEffectHit += SpellEffectFn(spell_dh_fel_rush_dash::PreventTrigger, EFFECT_6, SPELL_EFFECT_TRIGGER_SPELL);
    }
};

// Fel Rush air - 197922
class spell_dh_fel_rush_dash_ground : public AuraScript
{
    void AfterRemove(AuraEffect const* /*aurEff*/, AuraEffectHandleModes /*mode*/)
    {
        if (Unit* caster = GetCaster())
        {
            caster->SetDisableGravity(false);
        }
    }

    void CalcSpeed(AuraEffect const* /*aurEff*/, SpellEffectValue& amount, bool& /*canBeRecalculated*/)
    {
        amount = 1250;
        RefreshDuration();
    }

    void Register() override
    {
        DoEffectCalcAmount += AuraEffectCalcAmountFn(spell_dh_fel_rush_dash_ground::CalcSpeed, EFFECT_1, SPELL_AURA_MOD_SPEED_NO_CONTROL);
        DoEffectCalcAmount += AuraEffectCalcAmountFn(spell_dh_fel_rush_dash_ground::CalcSpeed, EFFECT_3, SPELL_AURA_MOD_MINIMUM_SPEED);
        AfterEffectRemove += AuraEffectRemoveFn(spell_dh_fel_rush_dash_ground::AfterRemove, EFFECT_6, SPELL_AURA_MOD_MINIMUM_SPEED_RATE, AURA_EFFECT_HANDLE_SEND_FOR_CLIENT_MASK);
    }
};

// Fel Rush Damage 223107
class spell_dh_fel_rush_damage : public SpellScript
{
    bool targetHit;

    void FilterTargets(std::list<WorldObject*>& targets)
    {
        targets.remove(GetCaster());
    }

    void CountTargets(std::list<WorldObject*>& targets)
    {
        Unit* caster = GetCaster();
        if (!caster)
            return;

        targets.clear();
        std::list<Unit*> units;
        caster->GetAttackableUnitListInRange(units, 25.f);
        units.remove_if([caster](Unit* unit)
            {
                return !caster->HasInLine(unit, 6.f, caster->GetObjectScale());
            });

        for (Unit* unit : units)
            targets.push_back(unit);

        targetHit = !targets.empty();
    }

    void HandleCast()
    {
        if (Unit* caster = GetCaster())
            if (caster->HasAura(SPELL_DH_FEL_MASTERY) && targetHit)
                caster->CastSpell(caster, SPELL_DH_FEL_MASTERY_FURY, true);
    }

    void HandleOnHit()
    {
        if (GetCaster() && GetHitUnit())
        {
            int32 attackPower = GetCaster()->m_unitData->AttackPower / static_cast<float>(100) * 25.3f;
            SetHitDamage(attackPower);
        }
    }

    void Register() override
    {
        OnObjectAreaTargetSelect += SpellObjectAreaTargetSelectFn(spell_dh_fel_rush_damage::FilterTargets, EFFECT_0, TARGET_UNIT_RECT_CASTER_ENEMY);
        OnObjectAreaTargetSelect += SpellObjectAreaTargetSelectFn(spell_dh_fel_rush_damage::CountTargets, EFFECT_0, TARGET_UNIT_RECT_CASTER_ENEMY);
        OnCast += SpellCastFn(spell_dh_fel_rush_damage::HandleCast);
        OnHit += SpellHitFn(spell_dh_fel_rush_damage::HandleOnHit);
    }
};

// Master of the Glaive - 203556
class spell_dh_master_of_the_glaive : public AuraScript
{

    bool CheckProc(ProcEventInfo& eventInfo)
    {
        if (eventInfo.GetSpellInfo()->Id == SPELL_DH_THROW_GLAIVE)
            return true;
        return false;
    }

    void Register() override
    {
        DoCheckProc += AuraCheckProcFn(spell_dh_master_of_the_glaive::CheckProc);
    }
};

// Fel Eruption - 211881
class spell_dh_fel_eruption : public SpellScript
{

    void HandleOnHit()
    {
        Unit* caster = GetCaster();
        Unit* target = GetExplTargetUnit();
        if (!caster || !target)
            return;
        caster->CastSpell(target, SPELL_DH_FEL_ERUPTION_DAMAGE, CastSpellExtraArgs(TRIGGERED_FULL_MASK).AddSpellMod(SPELLVALUE_BASE_POINT1, 1));
    }

    void HandleHit(SpellMissInfo missInfo)
    {
        Unit* caster = GetCaster();
        Unit* target = GetExplTargetUnit();
        if (!caster || !target)
            return;

        if (missInfo == SPELL_MISS_IMMUNE || missInfo == SPELL_MISS_IMMUNE2)
            caster->CastSpell(target, SPELL_DH_FEL_ERUPTION_DAMAGE, CastSpellExtraArgs(TRIGGERED_FULL_MASK).AddSpellMod(SPELLVALUE_BASE_POINT1, 2));
    }

    void Register() override
    {
        BeforeHit += BeforeSpellHitFn(spell_dh_fel_eruption::HandleHit);
        OnHit += SpellHitFn(spell_dh_fel_eruption::HandleOnHit);
    }
};

// Metamorphosis (Havoc) - 191427
class spell_dh_metamorphosis : public SpellScript
{

    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        if (!sSpellMgr->GetSpellInfo(SPELL_DH_METAMORPHOSIS_TRANSFORM, DIFFICULTY_NONE) ||
            !sSpellMgr->GetSpellInfo(SPELL_DH_METAMORPHOSIS, DIFFICULTY_NONE) ||
            !sSpellMgr->GetSpellInfo(SPELL_DH_METAMORPHOSIS_IMPACT_DAMAGE, DIFFICULTY_NONE))
            return false;
        return true;
    }

    void HandleDummy()
    {
        Unit* caster = GetCaster();
        if (!caster)
            return;

        Player* player = caster->ToPlayer();
        if (!player)
            return;

        if (const WorldLocation* dest = GetExplTargetDest())
        {
            player->CastSpell(*dest, SPELL_DH_METAMORPHOSIS, true);
            player->CastSpell(Position(dest->GetPositionX(), dest->GetPositionY(), dest->GetPositionZ()), SPELL_DH_METAMORPHOSIS_IMPACT_DAMAGE, true);
        }

        if (player->HasAura(SPELL_DH_DEMON_REBORN)) // Remove CD of Eye Beam, Chaos Nova and Blur
        {
            player->GetSpellHistory()->ResetCooldown(SPELL_DH_CHAOS_NOVA, true);
            player->GetSpellHistory()->ResetCooldown(SPELL_DH_BLUR, true);
            player->GetSpellHistory()->AddCooldown(SPELL_DH_BLUR, 0, std::chrono::minutes(1));
            player->GetSpellHistory()->ResetCooldown(SPELL_DH_BLUR, true);
            player->GetSpellHistory()->ResetCooldown(SPELL_DH_EYE_BEAM, true);
        }
    }

    void Register() override
    {
        BeforeCast += SpellCastFn(spell_dh_metamorphosis::HandleDummy);
    }
};

// Bloodlet - 206473
class spell_dh_bloodlet : public AuraScript
{

    bool CheckProc(ProcEventInfo& eventInfo)
    {
        if (eventInfo.GetSpellInfo()->Id == SPELL_DH_THROW_GLAIVE)
            return true;
        return false;
    }

    void HandleProc(AuraEffect* /*aurEff*/, ProcEventInfo& eventInfo)
    {
        Unit* caster = GetCaster();
        Unit* target = eventInfo.GetActionTarget();
        if (!caster || !target || !eventInfo.GetDamageInfo() || !GetSpellInfo()->GetEffect(EFFECT_0).IsEffect())
            return;

        int32 basePoints = GetSpellInfo()->GetEffect(EFFECT_0).BasePoints;
        int32 dmg = (eventInfo.GetDamageInfo()->GetDamage() * (float)basePoints) / 100.f;
        float dmgPerTick = (float)dmg / 5.f;

        // Any remaining damage must be added
        if (AuraEffect* dot = target->GetAuraEffect(SPELL_DH_BLOODLET_DOT, EFFECT_0, caster->GetGUID()))
        {
            dmgPerTick += (dot->GetAmount() * (dot->GetTotalTicks() - dot->GetTickNumber())) / 5;
        }

        CastSpellExtraArgs args;
        args.AddSpellBP0(dmgPerTick);
        args.SetTriggerFlags(TRIGGERED_FULL_MASK);
        caster->CastSpell(target, SPELL_DH_BLOODLET_DOT, args);
    }

    void Register() override
    {
        DoCheckProc += AuraCheckProcFn(spell_dh_bloodlet::CheckProc);
        OnEffectProc += AuraEffectProcFn(spell_dh_bloodlet::HandleProc, EFFECT_0, SPELL_AURA_DUMMY);
    }
};

// Fel Barrage - 211053
class spell_dh_fel_barrage : public AuraScript
{

    int32 _charges = 1;

    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        if (!sSpellMgr->GetSpellInfo(SPELL_DH_FEL_BARRAGE, DIFFICULTY_NONE) ||
            !sSpellMgr->GetSpellInfo(SPELL_DH_FEL_BARRAGE_DMG, DIFFICULTY_NONE))
            return false;
        return true;
    }

    bool Load() override
    {
        Unit* caster = GetCaster();
        if (!caster || !GetSpellInfo())
            return false;

        uint32 chargeCategoryId = GetSpellInfo()->ChargeCategoryId;
        while (caster->GetSpellHistory()->HasCharge(chargeCategoryId))
        {
            caster->GetSpellHistory()->ConsumeCharge(chargeCategoryId);
            caster->GetSpellHistory()->ForceSendSpellCharges();
            _charges++;
        }
        return true;
    }

    void HandleTrigger(AuraEffect const* /*aurEff*/)
    {
        Unit* caster = GetCaster();
        Unit* target = GetTarget();
        if (!caster || !target)
            return;

        CastSpellExtraArgs args;
        args.AddSpellBP0(_charges);
        args.SetTriggerFlags(TRIGGERED_FULL_MASK);
        caster->CastSpell(target, SPELL_DH_FEL_BARRAGE_DMG, args);
    }

    void Register() override
    {
        OnEffectPeriodic += AuraEffectPeriodicFn(spell_dh_fel_barrage::HandleTrigger, EFFECT_0, SPELL_AURA_PERIODIC_DUMMY);
    }
};

// Fel Barrage Damage - 211052
class spell_dh_fel_barrage_damage : public SpellScript
{

    void HandleHit()
    {
        int32 chargesUsed = GetSpellValue()->EffectBasePoints[0];
        int32 dmg = GetHitDamage();
        SetHitDamage(float(dmg * chargesUsed) / 5.f);
    }

    void Register() override
    {
        OnHit += SpellHitFn(spell_dh_fel_barrage_damage::HandleHit);
    }
};

// Fel Barrage aura - 222703
class spell_dh_fel_barrage_aura : public AuraScript
{

    bool CheckProc(ProcEventInfo& eventInfo)
    {
        // Blade Dance, Chaos Strike and Annihilation have many damagers,
        // so we accept only 1 of those, and we remove the others
        // Also we remove fel barrage itself too.
        if (!eventInfo.GetSpellInfo())
            return false;

        //Blade Dance    //Chaos Strike   //Fel Barrage
        std::vector<uint32> removeSpellIds{ 199552, 210153, SPELL_DH_CHAOS_STRIKE_MH, SPELL_DH_ANNIHILATION_MH, SPELL_DH_FEL_BARRAGE_DMG };
        return std::find(removeSpellIds.begin(), removeSpellIds.end(), eventInfo.GetSpellInfo()->Id) == removeSpellIds.end();
    }

    void HandleProc(AuraEffect* /*aurEff*/, ProcEventInfo& /*eventInfo*/)
    {
        PreventDefaultAction();

        Unit* caster = GetCaster();
        if (!caster)
            return;

        int32 chargeCatId = sSpellMgr->GetSpellInfo(SPELL_DH_FEL_BARRAGE, DIFFICULTY_NONE)->ChargeCategoryId;
        if (SpellCategoryEntry const* barrage = sSpellCategoryStore.LookupEntry(chargeCatId))
        {
            caster->GetSpellHistory()->RestoreCharge(chargeCatId);
            caster->GetSpellHistory()->ForceSendSpellCharge(barrage);
        }
    }

    void Register() override
    {
        DoCheckProc += AuraCheckProcFn(spell_dh_fel_barrage_aura::CheckProc);
        OnEffectProc += AuraEffectProcFn(spell_dh_fel_barrage_aura::HandleProc, EFFECT_0, SPELL_AURA_DUMMY);
    }
};

// Nemesis - 206491
class spell_dh_nemesis : public AuraScript
{

    void HandleAfterRemove(AuraEffect const* /*aurEff*/, AuraEffectHandleModes /*mode*/)
    {
        if (!GetTargetApplication())
            return;
        if (GetTargetApplication()->GetRemoveMode() != AURA_REMOVE_BY_DEATH)
            return;

        Unit* target = GetTargetApplication()->GetTarget();
        uint32 type = target->GetCreatureType();
        int32 dur = GetTargetApplication()->GetBase()->GetDuration();
        Unit* caster = GetAura()->GetCaster();

        if (!caster || !target)
            return;

        int32 spellId = 0;

        switch (type)
        {
        case CREATURE_TYPE_ABERRATION:
            spellId = SPELL_DH_NEMESIS_ABERRATIONS;
            break;
        case CREATURE_TYPE_BEAST:
            spellId = SPELL_DH_NEMESIS_BEASTS;
            break;
        case CREATURE_TYPE_CRITTER:
            spellId = SPELL_DH_NEMESIS_CRITTERS;
            break;
        case CREATURE_TYPE_DEMON:
            spellId = SPELL_DH_NEMESIS_DEMONS;
            break;
        case CREATURE_TYPE_DRAGONKIN:
            spellId = SPELL_DH_NEMESIS_DRAGONKIN;
            break;
        case CREATURE_TYPE_ELEMENTAL:
            spellId = SPELL_DH_NEMESIS_ELEMENTALS;
            break;
        case CREATURE_TYPE_GIANT:
            spellId = SPELL_DH_NEMESIS_GIANTS;
            break;
        case CREATURE_TYPE_HUMANOID:
            spellId = SPELL_DH_NEMESIS_HUMANOIDS;
            break;
        case CREATURE_TYPE_MECHANICAL:
            spellId = SPELL_DH_NEMESIS_MECHANICALS;
            break;
        case CREATURE_TYPE_UNDEAD:
            spellId = SPELL_DH_NEMESIS_UNDEAD;
            break;
        default:
            break;
        }

        if (spellId)
            if (Aura* aur = caster->AddAura(spellId, caster))
                aur->SetDuration(dur);
    }

    void Register() override
    {
        AfterEffectRemove += AuraEffectRemoveFn(spell_dh_nemesis::HandleAfterRemove, EFFECT_0, SPELL_AURA_MOD_SCHOOL_MASK_DAMAGE_FROM_CASTER, AURA_EFFECT_HANDLE_REAL);
    }
};

class event_dh_dark_matter_meteor : public BasicEvent
{
public:
    event_dh_dark_matter_meteor(Unit* caster, Position dest, uint32 spellId)
        : _casterGUID(caster->GetGUID()), _dest(dest), _spellId(spellId) {
    }

    bool Execute(uint64 /*execTime*/, uint32 /*diff*/) override
    {
        if (Player* caster = ObjectAccessor::FindConnectedPlayer(_casterGUID))
            caster->CastSpell(_dest, _spellId,
                TRIGGERED_IGNORE_CAST_IN_PROGRESS | TRIGGERED_DONT_REPORT_CAST_ERROR);
        return true;
    }

private:
    ObjectGuid _casterGUID;
    Position _dest;
    uint32 _spellId;
};

class event_dh_infernal_strike : public BasicEvent
{
public:
    explicit event_dh_infernal_strike(Unit* caster) : _caster(caster) {}

    bool Execute(uint64 /*execTime*/, uint32 /*diff*/) override
    {
        if (_caster)
        {
            _caster->CastSpell(_caster, SPELL_DH_INFERNAL_STRIKE_IMPACT_DAMAGE, true);

            if (_caster->HasAura(SPELL_DH_RAIN_OF_CHAOS))
                _caster->CastSpell(_caster, SPELL_DH_RAIN_OF_CHAOS_IMPACT, true);

            if (_caster->HasAura(SPELL_DH_ABYSSAL_STRIKE))
                _caster->CastSpell(_caster, SPELL_DH_SIGIL_OF_FLAME_FLAME_CRASH, true);
        }

        return true;
    }

private:
    Unit* _caster;
};

// Infernal Strike - 189110
// 189110 - Infernal Strike
class spell_dh_infernal_strike : public SpellScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({
            SPELL_DH_INFERNAL_STRIKE_JUMP,
            SPELL_DH_INFERNAL_STRIKE_IMPACT_DAMAGE
            });
    }

    void HandleOnHit()
    {
        Unit* caster = GetCaster();
        WorldLocation const* dest = GetHitDest();
        Unit* target = GetHitUnit();
        if (!caster || !dest || !target)
            return;

        if (target->IsHostileTo(caster))
        {
            caster->CastSpell(Position(dest->GetPositionX(), dest->GetPositionY(), dest->GetPositionZ()), SPELL_DH_INFERNAL_STRIKE_JUMP, true);
            caster->CastSpell(caster, SPELL_DH_INFERNAL_STRIKE_VISUAL, true);
        }
    }

    void HandleOnCast()
    {
        if (Unit* caster = GetCaster())
            caster->m_Events.AddEvent(new event_dh_infernal_strike(caster), caster->m_Events.CalculateTime(750ms));
    }

    void Register() override
    {
        OnHit += SpellHitFn(spell_dh_infernal_strike::HandleOnHit);
        OnCast += SpellCastFn(spell_dh_infernal_strike::HandleOnCast);
    }
};

// Soul Cleave - 228477
class spell_dh_soul_cleave : public SpellScript
{

    void HandleHeal(SpellEffIndex /*effIndex*/)
    {
        Unit* caster = GetCaster();
        if (!caster)
            return;

        if (caster->GetTypeId() != TYPEID_PLAYER)
            return;

        if (caster->HasAura(SPELL_DH_FEAST_OF_SOULS))
            caster->CastSpell(caster, SPELL_DH_FEAST_OF_SOULS_PERIODIC_HEAL, true);
    }

    void HandleDummy(SpellEffIndex /*effIndex*/)
    {
        Unit* caster = GetCaster();
        if (!caster)
            return;

        // Consume all soul fragments in 25 yards;
        std::vector<std::vector<AreaTrigger*>> fragments;
        fragments.push_back(caster->GetAreaTriggers(SPELL_DH_SHATTERED_SOULS_HAVOC));
        fragments.push_back(caster->GetAreaTriggers(SPELL_DH_SHATTERED_SOULS_DEMON));
        fragments.push_back(caster->GetAreaTriggers(SPELL_DH_LESSER_SOUL_SHARD));
        int32 range = GetEffectInfo().BasePoints;

        for (std::vector<AreaTrigger*> vec : fragments)
        {
            for (AreaTrigger* at : vec)
            {
                if (!caster->IsWithinDist(at, range))
                    continue;

                if (TempSummon* tempSumm = caster->SummonCreature(WORLD_TRIGGER, at->GetPositionX(), at->GetPositionY(), at->GetPositionZ(), 0, TEMPSUMMON_TIMED_DESPAWN, 100ms))
                {
                    tempSumm->SetFaction(caster->GetFaction());
                    tempSumm->SetSummonerGUID(caster->GetGUID());
                    int32 bp = 0;
                    switch (at->GetTemplate()->Id.Id)
                    {
                    case 6007:
                    case 5997:
                        bp = SPELL_DH_SOUL_FRAGMENT_HEAL_VENGEANCE;
                        break;
                    case 6710:
                        bp = SPELL_DH_CONSUME_SOUL_HEAL;
                        break;
                    }
                    caster->CastSpell(tempSumm, SPELL_DH_CONSUME_SOUL_VENGEANCE_SHATTERED, CastSpellExtraArgs(TRIGGERED_FULL_MASK).AddSpellBP0(bp));
                    if (at->GetTemplate()->Id.Id == 6007)
                        caster->CastSpell(caster, SPELL_DH_SOUL_FRAGMENT_DEMON_BONUS, true);

                    if (caster->HasAura(SPELL_DH_FEED_THE_DEMON))
                        caster->GetSpellHistory()->ReduceChargeCooldown(sSpellMgr->GetSpellInfo(SPELL_DH_DEMON_SPIKES, DIFFICULTY_NONE)->ChargeCategoryId, 1000);

                    if (caster->HasAura(SPELL_DH_PAINBRINGER))
                        caster->CastSpell(caster, SPELL_DH_PAINBRINGER_BUFF, true);

                    if (AuraEffect* soulBarrier = caster->GetAuraEffect(SPELL_DH_SOUL_BARRIER, EFFECT_0))
                    {
                        int32 amount = soulBarrier->GetAmount() + (float(sSpellMgr->GetSpellInfo(SPELL_DH_SOUL_BARRIER, DIFFICULTY_NONE)->GetEffect(EFFECT_1).BasePoints) / 100.f) * caster->GetTotalAttackPowerValue(BASE_ATTACK);
                        soulBarrier->SetAmount(amount);
                    }

                    at->SetDuration(0);
                }
            }
        }
    }

    void Register() override
    {
        OnEffectHitTarget += SpellEffectFn(spell_dh_soul_cleave::HandleDummy, EFFECT_0, SPELL_EFFECT_DUMMY);
        OnEffectHitTarget += SpellEffectFn(spell_dh_soul_cleave::HandleHeal, EFFECT_3, SPELL_EFFECT_HEAL);
    }
};

// Soul cleave damage - 228478
class spell_dh_soul_cleave_damage : public SpellScript
{
    int32 m_ExtraSpellCost{};

    void HandleOnHit()
    {
        Unit* caster = GetCaster();
        if (!caster)
            return;

        float dmg = GetHitDamage() * 2;
        dmg *= caster->VariableStorage.GetValue<float>("lastSoulCleaveMod");
        SetHitDamage(dmg);
    }

    void HandleDamage(SpellEffIndex /*effIndex*/)
    {
        Unit* caster = GetCaster();
        if (!caster)
            return;

        int32 dmg = GetHitDamage() * 2;
        dmg = (float)dmg * (((float)m_ExtraSpellCost + 300.0f) / 600.0f);
        SetHitDamage(dmg);

        caster->SetPower(POWER_PAIN, caster->GetPower(POWER_PAIN) - m_ExtraSpellCost);
        caster->ToPlayer()->SetPower(POWER_PAIN, caster->GetPower(POWER_PAIN) - m_ExtraSpellCost);
        if (caster->HasAura(SPELL_DH_GLUTTONY_BUFF))
            caster->RemoveAurasDueToSpell(SPELL_DH_GLUTTONY_BUFF);
    }

    void Register() override
    {
        OnEffectHitTarget += SpellEffectFn(spell_dh_soul_cleave_damage::HandleDamage, EFFECT_1, SPELL_EFFECT_WEAPON_PERCENT_DAMAGE);
    }
};

// Razor Spikes - 209400
class spell_dh_razor_spikes : public AuraScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_DH_RAZOR_SPIKES });
    }

    bool CheckProc(ProcEventInfo& eventInfo)
    {
        DamageInfo* damageInfo = eventInfo.GetDamageInfo();
        if (!damageInfo)
            return false;

        if (damageInfo->GetAttackType() == BASE_ATTACK ||
            damageInfo->GetAttackType() == OFF_ATTACK)
        {
            Unit* caster = damageInfo->GetAttacker();
            Unit* target = damageInfo->GetVictim();
            if (!caster || !target || !caster->ToPlayer())
                return false;

            if (!caster->IsValidAttackTarget(target))
                return false;

            if (caster->HasAura(SPELL_DH_DEMON_SPIKES))
            {
                caster->GetScheduler().Schedule(750ms, [caster, target](TaskContext& /*context*/)
                    {
                        caster->CastSpell(target, SPELL_DH_RAZOR_SPIKES, true);
                    });
            }
            return true;
        }
        return false;
    }

    void Register() override
    {
        DoCheckProc += AuraCheckProcFn(spell_dh_razor_spikes::CheckProc);
    }
};

// Soul Barrier - 263648
class spell_dh_soul_barrier : public AuraScript
{
    void CalcAmount(AuraEffect const* /*aurEff*/, SpellEffectValue& amount, bool& /*canBeRecalculated*/)
    {
        Unit* caster = GetCaster();
        if (!caster)
            return;

        if (Player* player = caster->ToPlayer())
        {
            float coeff = amount / 100.f;
            float soulShardCoeff = GetSpellInfo()->GetEffect(EFFECT_1).BasePoints / 100.f;
            int32 ap = player->GetTotalAttackPowerValue(BASE_ATTACK);

            amount = coeff * ap;

            // Consume all soul fragments in 25 yards;
            std::vector<std::vector<AreaTrigger*>> fragments;
            fragments.push_back(caster->GetAreaTriggers(SPELL_DH_SHATTERED_SOULS_HAVOC));
            fragments.push_back(caster->GetAreaTriggers(SPELL_DH_SHATTERED_SOULS_DEMON));
            fragments.push_back(caster->GetAreaTriggers(SPELL_DH_LESSER_SOUL_SHARD));
            float range = 25.f;

            for (std::vector<AreaTrigger*> vec : fragments)
            {
                for (AreaTrigger* at : vec)
                {
                    if (!caster->IsWithinDist(at, range))
                        continue;

                    if (TempSummon* tempSumm = caster->SummonCreature(WORLD_TRIGGER, at->GetPositionX(), at->GetPositionY(), at->GetPositionZ(), 0, TEMPSUMMON_TIMED_DESPAWN, 100ms))
                    {
                        tempSumm->SetFaction(caster->GetFaction());
                        tempSumm->SetSummonerGUID(caster->GetGUID());
                        int32 bp = 0;
                        switch (at->GetTemplate()->Id.Id)
                        {
                        case 6007:
                        case 5997:
                            bp = SPELL_DH_SOUL_FRAGMENT_HEAL_VENGEANCE;
                            break;
                        case 6710:
                            bp = SPELL_DH_CONSUME_SOUL_HEAL;
                            break;
                        }

                        caster->CastSpell(tempSumm, SPELL_DH_CONSUME_SOUL_VENGEANCE_SHATTERED, CastSpellExtraArgs(TRIGGERED_FULL_MASK).AddSpellBP0(bp));

                        if (at->GetTemplate()->Id.Id == 6007)
                            caster->CastSpell(caster, SPELL_DH_SOUL_FRAGMENT_DEMON_BONUS, true);

                        if (caster->HasAura(SPELL_DH_FEED_THE_DEMON))
                            caster->GetSpellHistory()->ReduceChargeCooldown(sSpellMgr->GetSpellInfo(SPELL_DH_DEMON_SPIKES, DIFFICULTY_NONE)->ChargeCategoryId, 1000);

                        if (caster->HasAura(SPELL_DH_PAINBRINGER))
                            caster->CastSpell(caster, SPELL_DH_PAINBRINGER_BUFF, true);

                        amount += soulShardCoeff * ap;

                        at->SetDuration(0);
                    }
                }
            }
        }

        if (AuraApplication* app = caster->GetAuraApplication(SPELL_DH_SOUL_BARRIER))
            app->ClientUpdate();
    }

    void HandleAbsorb(AuraEffect* aurEff, DamageInfo& dmgInfo, uint32& absorbAmount)
    {
        Unit* caster = GetCaster();
        if (!caster)
            return;

        uint32 threshold = caster->GetTotalAttackPowerValue(BASE_ATTACK) * 0.4914f;

        if (absorbAmount < dmgInfo.GetDamage())
            aurEff->SetAmount(absorbAmount + threshold);

        if (AuraApplication* app = caster->GetAuraApplication(SPELL_DH_SOUL_BARRIER))
            app->ClientUpdate();
    }

    void Register() override
    {
        DoEffectCalcAmount += AuraEffectCalcAmountFn(spell_dh_soul_barrier::CalcAmount, EFFECT_0, SPELL_AURA_SCHOOL_ABSORB);
        OnEffectAbsorb += AuraEffectAbsorbFn(spell_dh_soul_barrier::HandleAbsorb, EFFECT_0);
    }
};

// Nether Bond - 207810
class spell_dh_nether_bond : public SpellScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        if (!sSpellMgr->GetSpellInfo(SPELL_DH_NETHER_BOND_DAMAGE, DIFFICULTY_NONE) ||
            !sSpellMgr->GetSpellInfo(SPELL_DH_NETHER_BOND_PERIODIC, DIFFICULTY_NONE))
            return false;
        return true;
    }

    void HandleDummy()
    {
        Unit* caster = GetCaster();
        if (!caster)
            return;

        caster->CastSpell(caster, SPELL_DH_NETHER_BOND_PERIODIC, true);
    }

    void Register() override
    {
        OnCast += SpellCastFn(spell_dh_nether_bond::HandleDummy);
    }
};

// Nether Bond periodic - 207811
class spell_dh_nether_bond_periodic : public AuraScript
{
    Unit* m_BondUnit;

    void HandlePeriodic(AuraEffect const* /*aurEff*/)
    {
        Unit* caster = GetCaster();
        if (!caster)
            return;

        // Try to get the bonded Unit
        if (!m_BondUnit)
            m_BondUnit = GetBondUnit();

        // If still not found, return
        if (!m_BondUnit)
            return;

        int32 casterHealBp = 0;
        int32 casterDamageBp = 0;
        int32 targetHealBp = 0;
        int32 targetDamageBp = 0;

        float casterHp = caster->GetHealthPct();
        float targetHp = m_BondUnit->GetHealthPct();
        float healthPct = (casterHp + targetHp) / 2.0f;

        if (casterHp < targetHp)
        {
            casterHealBp = caster->CountPctFromMaxHealth(healthPct) - caster->GetHealth();
            targetDamageBp = m_BondUnit->GetHealth() - m_BondUnit->CountPctFromMaxHealth(healthPct);
        }
        else
        {
            casterDamageBp = caster->GetHealth() - caster->CountPctFromMaxHealth(healthPct);
            targetHealBp = m_BondUnit->CountPctFromMaxHealth(healthPct) - m_BondUnit->GetHealth();
        }

        caster->CastSpell(caster, SPELL_DH_NETHER_BOND_DAMAGE, CastSpellExtraArgs(TRIGGERED_FULL_MASK).AddSpellMod(SPELLVALUE_BASE_POINT0, casterDamageBp).AddSpellMod(SPELLVALUE_BASE_POINT1, casterHealBp));
        caster->CastSpell(m_BondUnit, SPELL_DH_NETHER_BOND_DAMAGE, CastSpellExtraArgs(TRIGGERED_FULL_MASK).AddSpellMod(SPELLVALUE_BASE_POINT0, targetDamageBp).AddSpellMod(SPELLVALUE_BASE_POINT1, targetHealBp));
    }

    Unit* GetBondUnit()
    {
        Unit* caster = GetCaster();
        if (!caster)
            return nullptr;

        std::list<Unit*> units;
        Trinity::AnyUnitInObjectRangeCheck check(caster, 100.0f);
        Trinity::UnitListSearcher<Trinity::AnyUnitInObjectRangeCheck> search(caster, units, check);
        Cell::VisitAllObjects(caster, search, 100.0f);

        for (Unit* u : units)
            if (u->HasAura(SPELL_DH_NETHER_BOND, caster->GetGUID()))
                return u;

        return nullptr;
    }

    void HandleApply(AuraEffect const* /*aurEff*/, AuraEffectHandleModes /*mode*/)
    {
        Unit* caster = GetCaster();
        if (!caster)
            return;

        m_BondUnit = GetBondUnit();
    }

    void Register() override
    {
        AfterEffectApply += AuraEffectApplyFn(spell_dh_nether_bond_periodic::HandleApply, EFFECT_0, SPELL_AURA_PERIODIC_DUMMY, AURA_EFFECT_HANDLE_REAL);
        OnEffectPeriodic += AuraEffectPeriodicFn(spell_dh_nether_bond_periodic::HandlePeriodic, EFFECT_0, SPELL_AURA_PERIODIC_DUMMY);
    }
};

// Solitude - 211509
class spell_dh_solitude : public AuraScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        if (!sSpellMgr->GetSpellInfo(SPELL_DH_SOLITUDE_BUFF, DIFFICULTY_NONE))
            return false;
        return true;
    }

    void HandlePeriodic(AuraEffect const* /*aurEff*/)
    {
        PreventDefaultAction();

        Unit* caster = GetCaster();
        if (!caster || !GetSpellInfo()->GetEffect(EFFECT_1).IsEffect())
            return;

        int32 range = GetSpellInfo()->GetEffect(EFFECT_1).BasePoints;
        std::list<Unit*> allies;
        Trinity::AnyFriendlyUnitInObjectRangeCheck check(caster, caster, range, true);
        Trinity::UnitListSearcher<Trinity::AnyFriendlyUnitInObjectRangeCheck> searcher(caster, allies, check);
        Cell::VisitAllObjects(caster, searcher, range);
        allies.remove(caster);

        if (allies.empty() && !caster->HasAura(SPELL_DH_SOLITUDE_BUFF))
            caster->CastSpell(caster, SPELL_DH_SOLITUDE_BUFF, true);
        else if (!allies.empty())
            caster->RemoveAurasDueToSpell(SPELL_DH_SOLITUDE_BUFF);
    }

    void Register() override
    {
        OnEffectPeriodic += AuraEffectPeriodicFn(spell_dh_solitude::HandlePeriodic, EFFECT_0, SPELL_AURA_PERIODIC_DUMMY);
    }
};

// Awaken the demon - 205598
class spell_dh_awaken_the_demon : public AuraScript
{
    void HandleProc(AuraEffect* aurEff, ProcEventInfo& eventInfo)
    {
        Unit* caster = GetCaster();
        if (!caster || !eventInfo.GetDamageInfo())
            return;

        if (!GetSpellInfo()->GetEffect(EFFECT_1).IsEffect() || !GetSpellInfo()->GetEffect(EFFECT_2).IsEffect())
            return;

        uint64 threshold1 = caster->CountPctFromMaxHealth(aurEff->GetBaseAmount());
        uint64 threshold2 = caster->CountPctFromMaxHealth(GetSpellInfo()->GetEffect(EFFECT_1).BasePoints);
        int32 duration = GetSpellInfo()->GetEffect(EFFECT_2).BasePoints;

        if (caster->GetHealth() - eventInfo.GetDamageInfo()->GetDamage() < threshold1)
        {
            if (caster->HasAura(SPELL_DH_AWAKEN_THE_DEMON_WITHIN_CD))
                return;
            caster->CastSpell(caster, SPELL_DH_AWAKEN_THE_DEMON_WITHIN_CD, true);
            if (Aura* aur = caster->GetAura(SPELL_DH_METAMORPHOSIS_TRANSFORM))
            {
                aur->SetDuration(std::min(duration * IN_MILLISECONDS + aur->GetDuration(), aur->GetMaxDuration()));
                return;
            }
            if (Aura* aur = caster->AddAura(SPELL_DH_METAMORPHOSIS_TRANSFORM, caster))
                aur->SetDuration(duration * IN_MILLISECONDS);
        }

        // Check only if we are above the second threshold and we are falling under it just now
        if (caster->GetHealth() > threshold2 && caster->GetHealth() - eventInfo.GetDamageInfo()->GetDamage() < threshold2)
        {
            if (Aura* aur = caster->GetAura(SPELL_DH_METAMORPHOSIS_TRANSFORM))
            {
                aur->SetDuration(std::min(duration * IN_MILLISECONDS + aur->GetDuration(), aur->GetMaxDuration()));
                return;
            }
        }
    }

    void Register() override
    {
        OnEffectProc += AuraEffectProcFn(spell_dh_awaken_the_demon::HandleProc, EFFECT_0, SPELL_AURA_DUMMY);
    }
};

struct auraData
{
    auraData(uint32 id, ObjectGuid casterGUID) : m_id(id), m_casterGuid(casterGUID) {}
    uint32 m_id;
    ObjectGuid m_casterGuid;
};

// Reverse Magic - 205604
class spell_dh_reverse_magic : public SpellScript
{
    void HandleScript()
    {
        Unit* player = GetCaster();
        if (!player || !player->ToPlayer())
            return;

        Unit* _player = player->ToPlayer();

        std::list<Unit*> allies;
        Trinity::AnyFriendlyUnitInObjectRangeCheck check(_player, _player, 10.f, true);
        Trinity::UnitListSearcher<Trinity::AnyFriendlyUnitInObjectRangeCheck> searcher(_player, allies, check);
        Cell::VisitAllObjects(_player, searcher, 10.f);

        for (Unit* unit : allies)
        {
            std::set<auraData*> auraListToRemove;
            Unit::AuraApplicationMap AuraList = unit->GetAppliedAuras();
            for (Unit::AuraApplicationMap::iterator iter = AuraList.begin(); iter != AuraList.end(); ++iter)
            {
                Aura* aura = iter->second->GetBase();
                if (!aura)
                    continue;

                Unit* caster = aura->GetCaster();
                if (!caster || caster->GetGUID() == unit->GetGUID())
                    continue;

                if (!caster->IsWithinDist(unit, 40.0f))
                    continue;

                if (aura->GetSpellInfo()->IsPositive())
                    continue;

                if (aura->GetSpellInfo()->Dispel != DISPEL_MAGIC)
                    continue;

                if (Creature* creature = caster->ToCreature())
                {
                    if (creature->GetCreatureTemplate()->Classification == CreatureClassifications::Obsolete)
                        continue;
                    if (creature->GetCreatureTemplate()->Classification == CreatureClassifications::Elite && creature->GetMap()->IsDungeon())
                        continue;
                }

                if (Aura* targetAura = unit->AddAura(aura->GetSpellInfo()->Id, caster))
                {
                    for (int i = 0; i < MAX_SPELL_EFFECTS; ++i)
                    {
                        targetAura->SetMaxDuration(aura->GetMaxDuration());
                        targetAura->SetDuration(aura->GetDuration());
                        if (targetAura->GetEffect(i) && aura->GetEffect(i))
                        {
                            AuraEffect* auraEffect = unit->GetAuraEffect(aura->GetSpellInfo()->Id, i);
                            if (!auraEffect)
                                continue;

                            int32 amount = auraEffect->GetAmount();

                            if (auraEffect->GetAuraType() == SPELL_AURA_PERIODIC_DAMAGE ||
                                auraEffect->GetAuraType() == SPELL_AURA_PERIODIC_DAMAGE_PERCENT)
                                amount = caster->SpellDamageBonusDone(unit, aura->GetSpellInfo(), amount, DOT, aura->GetSpellInfo()->GetEffects().at(i), auraEffect->GetBase()->GetStackAmount());

                            targetAura->GetEffect(i)->SetAmount(amount);
                            targetAura->GetEffect(i)->SetPeriodicTimer(auraEffect->GetPeriodicTimer());
                        }
                    }
                    targetAura->SetNeedClientUpdateForTargets();
                }

                auraListToRemove.insert(new auraData(aura->GetSpellInfo()->Id, caster->GetGUID()));
            }

            for (auto aura : auraListToRemove)
            {
                unit->RemoveAura(aura->m_id, aura->m_casterGuid);
                delete aura;
            }

            auraListToRemove.clear();
        }
    }

    void Register() override
    {
        OnCast += SpellCastFn(spell_dh_reverse_magic::HandleScript);
    }
};

// Eye of leotheras - 206649
class spell_dh_eye_of_leotheras : public AuraScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        if (!sSpellMgr->GetSpellInfo(SPELL_DH_EYE_OF_LEOTHERAS_DMG, DIFFICULTY_NONE))
            return false;

        return true;
    }

    bool HandleProc(ProcEventInfo& eventInfo)
    {
        Unit* caster = GetCaster();
        WorldObject* target = GetAura()->GetOwner();
        if (!caster || !target || !eventInfo.GetSpellInfo() || !caster->ToPlayer())
            return false;
        Unit* unitTarget = target->ToUnit();
        if (!unitTarget || eventInfo.GetSpellInfo()->IsPositive())
            return false;

        if (AuraEffect* aurEff = GetAura()->GetEffect(EFFECT_0))
        {
            int32 bp = aurEff->GetAmount();
            GetAura()->RefreshDuration();
            caster->GetScheduler().Schedule(100ms, [caster, unitTarget, bp](TaskContext& /*context*/)
                {
                    caster->CastSpell(unitTarget, SPELL_DH_EYE_OF_LEOTHERAS_DMG, CastSpellExtraArgs(TRIGGERED_FULL_MASK).AddSpellBP0(bp));
                });
            return true;
        }
        return false;
    }

    void Register() override
    {
        DoCheckProc += AuraCheckProcFn(spell_dh_eye_of_leotheras::HandleProc);
    }
};

// Immolation Aura - 258920
class spell_dh_immolation_aura : public SpellScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({
            SPELL_DH_CLEANSED_BY_FLAME,
            SPELL_DH_CLEANSED_BY_FLAME_DISPEL,
            SPELL_DH_FALLOUT,
            SPELL_DH_SHATTERED_SOULS_HAVOC,
            });
    }

    void HandleCast()
    {
        Unit* caster = GetCaster();

        if (caster->HasAura(SPELL_DH_CLEANSED_BY_FLAME))
            caster->CastSpell(caster, SPELL_DH_CLEANSED_BY_FLAME_DISPEL, true);
    }

    void Register() override
    {
        OnCast += SpellCastFn(spell_dh_immolation_aura::HandleCast);
    }
};

// Immolation Aura damage - 258922
class spell_dh_immolation_aura_damage : public SpellScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({
            SPELL_DH_CHARRED_FLESH,
            SPELL_DH_FIERY_BRAND_DEBUFF_RANK_2,
            SPELL_DH_FIERY_BRAND_DEBUFF_RANK_1,
            });
    }

    void HandleHit(SpellEffIndex /*effIndex*/)
    {
        if (Unit* target = GetHitUnit())
        {
            if (GetCaster()->HasAura(SPELL_DH_CHARRED_FLESH))
            {
                for (uint32 spellId : { SPELL_DH_FIERY_BRAND_DEBUFF_RANK_2, SPELL_DH_FIERY_BRAND_DEBUFF_RANK_1 })
                {
                    if (Aura* fieryBrand = target->GetAura(spellId))
                    {
                        int32 durationMod = GetCaster()->GetAuraEffectAmount(SPELL_DH_CHARRED_FLESH, EFFECT_0);
                        fieryBrand->ModDuration(durationMod);
                    }
                }
            }
        }
    }

    void Register() override
    {
        OnEffectHitTarget += SpellEffectFn(spell_dh_immolation_aura_damage::HandleHit, EFFECT_0, SPELL_EFFECT_SCHOOL_DAMAGE);
    }
};

// Jagged Spikes - 208796
class spell_dh_jagged_spikes : public AuraScript
{

    void HandleProc(AuraEffect* /*aurEff*/, ProcEventInfo& eventInfo)
    {
        Unit* caster = GetCaster();
        Unit* target = eventInfo.GetActor();
        if (!caster || !eventInfo.GetDamageInfo())
            return;
        if (caster->IsFriendlyTo(target))
            return;

        int32 pct = caster->GetAuraEffectAmount(SPELL_DH_JAGGED_SPIKES, EFFECT_0);
        int32 damage = eventInfo.GetDamageInfo()->GetDamage();
        ApplyPct(damage, pct);

        caster->CastSpell(target, SPELL_DH_JAGGED_SPIKES_DMG, CastSpellExtraArgs(TRIGGERED_FULL_MASK).AddSpellBP0(damage));
    }

    void Register() override
    {
        OnEffectProc += AuraEffectProcFn(spell_dh_jagged_spikes::HandleProc, EFFECT_0, SPELL_AURA_DUMMY);
    }
};

// 206966 - Fel Lance
class spell_dh_fel_lance : public SpellScript
{

    void HandleHit(SpellEffIndex /*effIndex*/)
    {
        Unit* caster = GetCaster();
        Unit* target = GetHitUnit();
        if (!caster || !target)
            return;

        int32 pct = GetSpellInfo()->GetEffect(EFFECT_0).BasePoints;
        SetHitDamage(GetHitDamage() + target->CountPctFromMaxHealth(pct));
    }

    void Register()
    {
        OnEffectHitTarget += SpellEffectFn(spell_dh_fel_lance::HandleHit, EFFECT_1, SPELL_EFFECT_DUMMY);
    }
};

// 206891 - Intimidated
class spell_dh_intimidated : public AuraScript
{

    void OnProc(AuraEffect* /*aurEff*/, ProcEventInfo& eventInfo)
    {
        Unit* attacker = eventInfo.GetActor();
        WorldObject* auraOwner = GetAura()->GetOwner();
        if (!attacker || !auraOwner)
            return;

        if (attacker == GetCaster())
        {
            RefreshDuration();
            return;
        }

        if (uniqueTargets.size() >= 4 || !auraOwner->ToUnit())
            return;

        if (uniqueTargets.find(attacker->GetGUID()) == uniqueTargets.end())
        {
            attacker->CastSpell(auraOwner->ToUnit(), GetSpellInfo()->Id, true);
            uniqueTargets.insert(attacker->GetGUID());
        }
    }

    void Register()
    {
        OnEffectProc += AuraEffectProcFn(spell_dh_intimidated::OnProc, EFFECT_0, SPELL_AURA_MOD_DAMAGE_PERCENT_TAKEN);
    }

private:
    GuidSet uniqueTargets;
};

// 203753 - Blade Turning
class spell_dh_blade_turning : public AuraScript
{

    bool CheckProc(ProcEventInfo& eventInfo)
    {
        if (eventInfo.GetHitMask() & PROC_HIT_PARRY)
            return true;
        return false;
    }

    void Register()
    {
        DoCheckProc += AuraCheckProcFn(spell_dh_blade_turning::CheckProc);
    }
};

// 203783 - Shear proc
class spell_dh_shear_proc : public AuraScript
{

    void OnProc(AuraEffect* /*aurEff*/, ProcEventInfo& eventInfo)
    {
        PreventDefaultAction();
        Unit* caster = GetCaster();
        if (!caster || !eventInfo.GetSpellInfo())
            return;

        int32 procChance = 100;
        if (eventInfo.GetSpellInfo()->Id == SPELL_DH_SHEAR)
        {
            procChance = 15;
            procChance += caster->GetAuraEffectAmount(SPELL_DH_SHATTER_THE_SOULS, EFFECT_0);
        }

        if (caster->GetSpellHistory()->HasCooldown(SPELL_DH_FELBLADE))
            if (roll_chance(caster->GetAuraEffectAmount(SPELL_DH_SHEAR_PROC, EFFECT_3)))
                caster->GetSpellHistory()->ResetCooldown(SPELL_DH_FELBLADE);
    }

    void Register()
    {
        OnEffectProc += AuraEffectProcFn(spell_dh_shear_proc::OnProc, EFFECT_0, SPELL_AURA_PROC_TRIGGER_SPELL);
    }
};

// 210047 - Consume Soul missile
class spell_dh_consume_soul_missile : public SpellScript
{

    void HandleHit(SpellEffIndex effIndex)
    {
        PreventHitDefaultEffect(effIndex);
        Unit* caster = GetCaster();
        if (!caster)
            return;

        int32 spellToCast = GetSpellValue()->EffectBasePoints[0];
        caster->CastSpell(caster, spellToCast, true);
    }

    void Register()
    {
        OnEffectHit += SpellEffectFn(spell_dh_consume_soul_missile::HandleHit, EFFECT_1, SPELL_EFFECT_TRIGGER_MISSILE);
    }
};

// 209426 - Darkness absorb
class spell_dh_darkness_absorb : public AuraScript
{

    void CalculateAmount(AuraEffect const* /*aurEff*/, SpellEffectValue& amount, bool& /*canBeRecalculated*/)
    {
        amount = -1;
    }

    void OnAbsorb(AuraEffect* /*aurEff*/, DamageInfo& dmgInfo, uint32& absorbAmount)
    {
        Unit* caster = GetCaster();
        if (!caster)
            return;

        int32 chance = GetSpellInfo()->GetEffect(EFFECT_1).BasePoints + caster->GetAuraEffectAmount(SPELL_DH_COVER_OF_DARKNESS, EFFECT_0);
        if (roll_chance(chance))
            absorbAmount = dmgInfo.GetDamage();
    }

    void Register()
    {
        OnEffectAbsorb += AuraEffectAbsorbFn(spell_dh_darkness_absorb::OnAbsorb, EFFECT_0);
        DoEffectCalcAmount += AuraEffectCalcAmountFn(spell_dh_darkness_absorb::CalculateAmount, EFFECT_0, SPELL_AURA_SCHOOL_ABSORB);
    }
};

// 203650 - Prepared
class spell_dh_prepared : public AuraScript
{

    void PeriodicTick(AuraEffect const* aurEff)
    {
        Unit* caster = GetCaster();
        if (!caster)
            return;

        caster->ModifyPower(POWER_FURY, aurEff->GetAmount() / 10.f);
    }

    void Register()
    {
        OnEffectPeriodic += AuraEffectPeriodicFn(spell_dh_prepared::PeriodicTick, EFFECT_0, SPELL_AURA_MOD_POWER_REGEN);
    }
};

// Soul Fragment healing effects - 178963, 203794, 228532
class spell_dh_soul_fragment_heals : public SpellScript
{

    void HandleHit(SpellEffIndex effIndex)
    {
        if (!GetCaster()->HasAura(SPELL_DH_DEMONIC_APPETITE))
            PreventHitDefaultEffect(effIndex);
    }

    void HandleHeal(SpellEffIndex /*effIndex*/)
    {
        Unit* caster = GetCaster();
        if (!caster)
            return;

        if (int32 reductionTime = caster->GetAuraEffectAmount(SPELL_DH_FEAST_ON_THE_SOULS, EFFECT_0))
        {
            caster->GetSpellHistory()->ModifyCooldown(SPELL_DH_CHAOS_NOVA, Seconds(-reductionTime));
            caster->GetSpellHistory()->ModifyCooldown(SPELL_DH_EYE_BEAM, Seconds(-reductionTime));
        }
    }

    void Register()
    {
        OnEffectHitTarget += SpellEffectFn(spell_dh_soul_fragment_heals::HandleHeal, EFFECT_0, SPELL_AURA_ANY);
        OnEffectLaunchTarget += SpellEffectFn(spell_dh_soul_fragment_heals::HandleHit, EFFECT_1, SPELL_EFFECT_TRIGGER_SPELL);
        OnEffectLaunch += SpellEffectFn(spell_dh_soul_fragment_heals::HandleHit, EFFECT_1, SPELL_EFFECT_TRIGGER_SPELL);
    }
};

// 206475 - Chaos Cleave
class spell_dh_chaos_cleave : public AuraScript
{

    void OnProc(AuraEffect* aurEff, ProcEventInfo& eventInfo)
    {
        PreventDefaultAction();
        Unit* caster = GetCaster();
        if (!caster || !eventInfo.GetDamageInfo())
            return;

        int32 damage = CalculatePct(eventInfo.GetDamageInfo()->GetDamage(), aurEff->GetAmount());
        caster->CastSpell(caster, SPELL_DH_CHAOS_CLEAVE_PROC, CastSpellExtraArgs(TRIGGERED_FULL_MASK).AddSpellBP0(damage));
    }

    void Register()
    {
        OnEffectProc += AuraEffectProcFn(spell_dh_chaos_cleave::OnProc, EFFECT_0, SPELL_AURA_PROC_TRIGGER_SPELL);
    }
};

// 205411 - Desperate Instints
class spell_dh_desperate_instincts : public AuraScript
{

    void OnProc(AuraEffect* aurEff, ProcEventInfo& eventInfo)
    {
        PreventDefaultAction();
        Unit* caster = GetCaster();
        if (!caster || !eventInfo.GetDamageInfo())
            return;

        if (caster->GetSpellHistory()->HasCooldown(SPELL_DH_BLUR))
            return;

        uint32 triggerOnHealth = caster->CountPctFromMaxHealth(aurEff->GetAmount());
        uint32 currentHealth = caster->GetHealth();
        // Just falling below threshold
        if (currentHealth > triggerOnHealth && (currentHealth - eventInfo.GetDamageInfo()->GetDamage()) <= triggerOnHealth)
            caster->CastSpell(caster, SPELL_DH_BLUR, false);
    }

    void Register()
    {
        OnEffectProc += AuraEffectProcFn(spell_dh_desperate_instincts::OnProc, EFFECT_0, SPELL_AURA_TRIGGER_SPELL_ON_HEALTH_PCT);
    }
};

// 209795 - Fracture
class spell_dh_fracture : public SpellScriptLoader
{
public:
    spell_dh_fracture() : SpellScriptLoader("spell_dh_fracture") {}

    class spell_dh_fracture_SpellScript : public SpellScript
    {

        void HandleHit(SpellEffIndex /*effIndex*/)
        {
            Unit* caster = GetCaster();
            if (!caster)
                return;
        }

        void Register()
        {
            OnEffectHitTarget += SpellEffectFn(spell_dh_fracture_SpellScript::HandleHit, EFFECT_0, SPELL_EFFECT_DUMMY);
        }
    };

    SpellScript* GetSpellScript() const
    {
        return new spell_dh_fracture_SpellScript();
    }
};

// 224509 - Frailty
class spell_dh_frailty : public SpellScriptLoader
{
public:
    spell_dh_frailty() : SpellScriptLoader("spell_dh_frailty") {}

    class spell_dh_frailty_AuraScript : public AuraScript
    {

        void OnProc(AuraEffect* aurEff, ProcEventInfo& eventInfo)
        {
            PreventDefaultAction();
            Unit* caster = GetCaster();
            if (!caster || caster != eventInfo.GetActor() || !eventInfo.GetDamageInfo())
                return;

            uint32 damage = CalculatePct(eventInfo.GetDamageInfo()->GetDamage(), aurEff->GetAmount());
            damage += GetAura()->Variables.GetValue<uint32>("_frailtyDamage");

            GetAura()->Variables.Set("_frailtyDamage", damage);
        }

        void PeriodicTick(AuraEffect const* /*aurEff*/)
        {
            Unit* caster = GetCaster();
            if (!caster)
                return;

            if (uint32 heal = GetAura()->Variables.GetValue<uint32>("_frailtyDamage"))
            {
                caster->CastSpell(caster, SPELL_DH_SPIRIT_BOMB_HEAL, CastSpellExtraArgs(TRIGGERED_FULL_MASK).AddSpellBP0(heal));
                GetAura()->Variables.Set<uint32>("_frailtyDamage", 0);
            }
        }

        void Register()
        {
            OnEffectPeriodic += AuraEffectPeriodicFn(spell_dh_frailty_AuraScript::PeriodicTick, EFFECT_0, SPELL_AURA_PERIODIC_DUMMY);
            OnEffectProc += AuraEffectProcFn(spell_dh_frailty_AuraScript::OnProc, EFFECT_0, SPELL_AURA_PERIODIC_DUMMY);
        }
    };

    AuraScript* GetAuraScript() const
    {
        return new spell_dh_frailty_AuraScript();
    }
};

// 218679 - Spirit Bomb
class spell_dh_spirit_bomb : public SpellScriptLoader
{
public:
    spell_dh_spirit_bomb() : SpellScriptLoader("spell_dh_spirit_bomb") {}

    class spell_dh_spirit_bomb_SpellScript : public SpellScript
    {

        bool tryCastDamage(Unit* caster, Unit* target, uint32 spellId)
        {
            if (AreaTrigger* at = caster->GetAreaTrigger(spellId))
            {
                caster->CastSpell(target, SPELL_DH_SPIRIT_BOMB_DAMAGE, true);
                at->Remove();
                return true;
            }

            return false;
        }

        void HandleHit()
        {
            Unit* caster = GetCaster();
            Unit* target = GetHitUnit();
            if (!caster || !target)
                return;

            for (uint32 spellId : { SPELL_DH_LESSER_SOUL_SHARD, SPELL_DH_SHATTERED_SOULS_HAVOC, SPELL_DH_SHATTERED_SOULS_DEMON })
                if (tryCastDamage(caster, target, spellId))
                    break;
        }

        SpellCastResult CheckCast()
        {
            Unit* caster = GetCaster();
            if (!caster)
                return SPELL_FAILED_CANT_DO_THAT_RIGHT_NOW;

            if (!caster->GetAreaTrigger(SPELL_DH_LESSER_SOUL_SHARD) && !caster->GetAreaTrigger(SPELL_DH_SHATTERED_SOULS_HAVOC) && !caster->GetAreaTrigger(SPELL_DH_SHATTERED_SOULS_DEMON))
                return SPELL_FAILED_CANT_DO_THAT_RIGHT_NOW;

            return SPELL_CAST_OK;
        }

        void Register()
        {
            OnHit += SpellHitFn(spell_dh_spirit_bomb_SpellScript::HandleHit);
            OnCheckCast += SpellCheckCastFn(spell_dh_spirit_bomb_SpellScript::CheckCast);
        }
    };

    SpellScript* GetSpellScript() const
    {
        return new spell_dh_spirit_bomb_SpellScript();
    }
};

// 236189 - Demonic Infusion
class spell_dh_demonic_infusion : public SpellScriptLoader
{
public:
    spell_dh_demonic_infusion() : SpellScriptLoader("spell_dh_demonic_infusion") {}

    class spell_dh_demonic_infusion_SpellScript : public SpellScript
    {

        void HandleCast()
        {
            Unit* caster = GetCaster();
            if (!caster)
                return;

            caster->GetSpellHistory()->ResetCharges(sSpellMgr->GetSpellInfo(SPELL_DH_DEMON_SPIKES, DIFFICULTY_NONE)->ChargeCategoryId);
            caster->CastSpell(caster, SPELL_DH_DEMON_SPIKES, true);
            caster->GetSpellHistory()->ResetCharges(sSpellMgr->GetSpellInfo(SPELL_DH_DEMON_SPIKES, DIFFICULTY_NONE)->ChargeCategoryId);
        }

        void Register()
        {
            OnCast += SpellCastFn(spell_dh_demonic_infusion_SpellScript::HandleCast);
        }
    };

    SpellScript* GetSpellScript() const
    {
        return new spell_dh_demonic_infusion_SpellScript();
    }
};

// 206803 - Rain from Above
class spell_demon_hunter_rain_from_above : public SpellScriptLoader
{
public:
    spell_demon_hunter_rain_from_above() : SpellScriptLoader("spell_demon_hunter_rain_from_above") {}

    class spell_demon_hunter_rain_from_above_SpellScript : public SpellScript
    {

        void HandleCast()
        {
            Unit* caster = GetCaster();
            if (!caster || !caster->ToPlayer())
                return;

            caster->GetScheduler().Schedule(1750ms, [caster](TaskContext& /*context*/)
                {
                    //SPELLVALUE_BASE_POINT0

                    caster->CastSpell(caster, SPELL_DK_RAIN_FROM_ABOVE_SLOWFALL, true);
                });
        }

        void Register()
        {
            AfterCast += SpellCastFn(spell_demon_hunter_rain_from_above_SpellScript::HandleCast);
        }
    };

    SpellScript* GetSpellScript() const
    {
        return new spell_demon_hunter_rain_from_above_SpellScript();
    }
};

// 162264 - Metamorphosis buffs @ Vengeance
// SpellId - SpellName
class spell_demon_hunter_metamorphosis_buffs : public SpellScriptLoader
{
public:
    spell_demon_hunter_metamorphosis_buffs() : SpellScriptLoader("spell_demon_hunter_metamorphosis_buffs") {}

    class spell_demon_hunter_metamorphosis_buffs_AuraScript : public AuraScript
    {


        void OnApply(AuraEffect const* /*aurEff*/, AuraEffectHandleModes /*mode*/)
        {
            Unit* caster = GetCaster();
            if (!caster)
                return;

            caster->RemoveAura(SPELL_DH_DEMONIC_ORIGINS_BUFF);
        }

        void OnRemove(AuraEffect const* /*aurEff*/, AuraEffectHandleModes /*mode*/)
        {
            Unit* caster = GetCaster();
            if (!caster)
                return;

            if (caster->HasAura(SPELL_DH_DEMONIC_ORIGINS))
                caster->CastSpell(caster, SPELL_DH_DEMONIC_ORIGINS_BUFF, true);
        }

        void Register()
        {
            OnEffectRemove += AuraEffectRemoveFn(spell_demon_hunter_metamorphosis_buffs_AuraScript::OnRemove, EFFECT_0, SPELL_AURA_TRANSFORM, AURA_EFFECT_HANDLE_REAL);
            OnEffectApply += AuraEffectApplyFn(spell_demon_hunter_metamorphosis_buffs_AuraScript::OnApply, EFFECT_0, SPELL_AURA_TRANSFORM, AURA_EFFECT_HANDLE_REAL);
        }
    };

    AuraScript* GetAuraScript() const
    {
        return new spell_demon_hunter_metamorphosis_buffs_AuraScript();
    }
};

// 212105 - Fel Devastation damage
class spell_dh_fel_devastation_damage : public AuraScript
{
    void PeriodicTick(AuraEffect const* aurEff)
    {
        Unit* caster = GetCaster();
        if (!caster)
            return;

        if (aurEff->GetTickNumber() == 1)
            return;

        caster->CastSpell(caster, SPELL_DH_FEL_DEVASTATION_DMG, true);
    }

    void Register()
    {
        OnEffectPeriodic += AuraEffectPeriodicFn(spell_dh_fel_devastation_damage::PeriodicTick, EFFECT_0, SPELL_EFFECT_SCHOOL_DAMAGE);
    }
};

// 162794 - Chaos Strike
// 201427 - Annihilation
class spell_demon_hunter_chaos_strike : public SpellScript
{

    void HandleCast()
    {
        Unit* caster = GetCaster();
        Unit* target = GetExplTargetUnit();
        if (!caster || !target)
            return;

        // Chaos Strike and Annihilation have a mainhand and an offhand spell, but the crit chance should be the same.
        float criticalChances = caster->GetUnitCriticalChanceAgainst(BASE_ATTACK, target);
        caster->VariableStorage.Set("Spells.ChaosStrikeCrit", roll_chance(criticalChances));
        caster->CastSpell(nullptr, SPELL_DH_CHAOS_STRIKE_ENERGIZE, true);
    }

    void Register()
    {
        BeforeCast += SpellCastFn(spell_demon_hunter_chaos_strike::HandleCast);
    }
};

// 185244 - Pain
class spell_demon_hunter_pain : public SpellScriptLoader
{
public:
    spell_demon_hunter_pain() : SpellScriptLoader("spell_demon_hunter_pain") {}

    class spell_demon_hunter_pain_AuraScript : public AuraScript
    {

        void OnProc(AuraEffect* /*aurEff*/, ProcEventInfo& eventInfo)
        {
            Unit* caster = GetCaster();
            if (!caster || !eventInfo.GetDamageInfo())
                return;

            if (eventInfo.GetSpellInfo() && eventInfo.GetSpellInfo()->IsPositive())
                return;

            int32 damageTaken = eventInfo.GetDamageInfo()->GetDamage();
            if (damageTaken <= 0)
                return;

            float painAmount = (50.0f * (float)damageTaken) / (float)caster->GetMaxHealth();
            caster->CastSpell(caster, SPELL_DH_REWARD_PAIN, painAmount);
        }

        void Register()
        {
            OnEffectProc += AuraEffectProcFn(spell_demon_hunter_pain_AuraScript::OnProc, EFFECT_0, SPELL_AURA_MOD_POWER_DISPLAY);
        }
    };

    AuraScript* GetAuraScript() const
    {
        return new spell_demon_hunter_pain_AuraScript();
    }
};

// 203704 - Mana Break
class spell_demon_hunter_mana_break : public SpellScriptLoader
{
public:
    spell_demon_hunter_mana_break() : SpellScriptLoader("spell_demon_hunter_mana_break") {}

    class spell_demon_hunter_mana_break_SpellScript : public SpellScript
    {
    public:

        void HandleHit(SpellEffIndex /*effIndex*/)
        {
            Unit* caster = GetCaster();
            Unit* target = GetHitUnit();
            if (!caster || !target)
                return;

            int32 damage = GetSpellInfo()->GetEffect(EFFECT_1).BasePoints;
            float powerPct = target->GetPowerPct(POWER_MANA);
            if (powerPct >= 1.f)
            {
                damage += (100.f - powerPct) / 10.f * GetSpellInfo()->GetEffect(EFFECT_2).BasePoints;
            }

            damage = std::max<uint32>(GetHitUnit()->CountPctFromMaxHealth(int32(5)), 1);

            SetHitDamage(damage);
        }

        void Register() override
        {
            OnEffectHitTarget += SpellEffectFn(spell_demon_hunter_mana_break_SpellScript::HandleHit, EFFECT_0, SPELL_EFFECT_SCHOOL_DAMAGE);
        }
    };

    SpellScript* GetSpellScript() const override
    {
        return new spell_demon_hunter_mana_break_SpellScript();
    }
};

// 258881 - Trail of Ruin
class spell_demon_hunter_trail_of_ruin : public SpellScriptLoader
{
public:
    spell_demon_hunter_trail_of_ruin() : SpellScriptLoader("spell_demon_hunter_trail_of_ruin") {}

    class spell_demon_hunter_trail_of_ruin_AuraScript : public AuraScript
    {
    public:

        bool CheckProc(ProcEventInfo& eventInfo)
        {
            return eventInfo.GetSpellInfo()->Id == sSpellMgr->GetSpellInfo(SPELL_DH_BLADE_DANCE, DIFFICULTY_NONE)->GetEffect(EFFECT_0).TriggerSpell;
        }

        void Register() override
        {
            DoCheckProc += AuraCheckProcFn(spell_demon_hunter_trail_of_ruin_AuraScript::CheckProc);
        }
    };

    AuraScript* GetAuraScript() const override
    {
        return new spell_demon_hunter_trail_of_ruin_AuraScript();
    }
};

// 213480 - Unending Hatred
class spell_demon_hunter_unending_hatred : public SpellScriptLoader
{
public:
    spell_demon_hunter_unending_hatred() : SpellScriptLoader("spell_demon_hunter_unending_hatred") {}

    class spell_demon_hunter_unending_hatred_AuraScript : public AuraScript
    {
    public:

        bool CheckProc(ProcEventInfo& eventInfo)
        {
            return eventInfo.GetDamageInfo() && eventInfo.GetDamageInfo()->GetSchoolMask() & SPELL_SCHOOL_MASK_MAGIC;
        }

        void Proc(ProcEventInfo& eventInfo)
        {
            Player* caster = GetPlayerCaster();
            if (!caster)
                return;

            int32 pointsGained = GetPointsGained(caster, eventInfo.GetDamageInfo()->GetDamage());

            if (caster->GetPrimarySpecialization() == ChrSpecialization::DemonHunterHavoc)
                caster->EnergizeBySpell(caster, GetSpellInfo(), pointsGained, POWER_FURY);
            else if (caster->GetPrimarySpecialization() == ChrSpecialization::DemonHunterVengeance)
                caster->EnergizeBySpell(caster, GetSpellInfo(), pointsGained, POWER_PAIN);
        }

        Player* GetPlayerCaster()
        {
            Unit* caster = GetCaster();
            if (!caster)
                return nullptr;

            return caster->ToPlayer();
        }

        int32 GetPointsGained(Player* caster, uint32 damage)
        {
            float damagePct = (float)damage / caster->GetMaxHealth() * 100.f / 2;
            int32 max = GetSpellInfo()->GetEffect(EFFECT_0).BasePoints;
            if (damagePct > max)
                return max;
            if (damagePct < 1)
                return 1;
            return 0;
        }

        void Register() override
        {
            DoCheckProc += AuraCheckProcFn(spell_demon_hunter_unending_hatred_AuraScript::CheckProc);
            OnProc += AuraProcFn(spell_demon_hunter_unending_hatred_AuraScript::Proc);
        }
    };

    AuraScript* GetAuraScript() const override
    {
        return new spell_demon_hunter_unending_hatred_AuraScript();
    }
};

// 196718 - Darkness
// MiscId - 6615
struct at_dh_darkness : AreaTriggerAI
{
    at_dh_darkness(AreaTrigger* areatrigger) : AreaTriggerAI(areatrigger) {}

private:
    bool entered;

    void OnInitialize() override
    {
        at->SetDuration(8000);
    }

    void OnUnitEnter(Unit* unit) override
    {
        Unit* caster = at->GetCaster();
        if (!caster || !unit)
            return;

        if (caster->IsFriendlyTo(unit) && !unit->HasAura(SPELL_DH_DARKNESS_ABSORB))
        {
            entered = true;

            if (entered)
            {
                caster->CastSpell(unit, SPELL_DH_DARKNESS_ABSORB, true);
                entered = false;
            }
        }
    }

    void OnUnitExit(Unit* unit, AreaTriggerExitReason /*reason*/) override
    {
        Unit* caster = at->GetCaster();
        if (!caster || !unit)
            return;
        if (unit->HasAura(SPELL_DH_DARKNESS_ABSORB))
            unit->RemoveAurasDueToSpell(SPELL_DH_DARKNESS_ABSORB, caster->GetGUID());
    }
};

// 235903 - Mana Rift
// MiscId - 9695
struct at_demon_hunter_mana_rift : AreaTriggerAI
{
    at_demon_hunter_mana_rift(AreaTrigger* areatrigger) : AreaTriggerAI(areatrigger) {}

    void OnUnitExit(Unit* unit, AreaTriggerExitReason /*reason*/) override
    {
        Unit* caster = at->GetCaster();
        if (!caster || !unit)
            return;

        SpellInfo const* spellProto = sSpellMgr->GetSpellInfo(SPELL_DH_MANA_RIFT_SPELL, DIFFICULTY_NONE);
        if (!spellProto)
            return;

        if (at->IsRemoved())
        {
            if (caster->IsValidAttackTarget(unit))
            {
                int32 hpBp = unit->CountPctFromMaxHealth(spellProto->GetEffect(EFFECT_1).BasePoints);
                int32 manaBp = unit->CountPctFromMaxPower(POWER_MANA, spellProto->GetEffect(EFFECT_2).BasePoints);
                CastSpellExtraArgs args;
                args.AddSpellMod(SPELLVALUE_BASE_POINT0, hpBp);
                args.AddSpellMod(SPELLVALUE_BASE_POINT0, manaBp);
                args.SetTriggerFlags(TRIGGERED_FULL_MASK);
                caster->CastSpell(unit, SPELL_DH_MANA_RIFT_DMG_POWER_BURN, args);
            }
        }
    }
};

// 205629 - Demonic Trample
// MiscId - 6482
struct at_demon_hunter_demonic_trample : AreaTriggerAI
{
    at_demon_hunter_demonic_trample(AreaTrigger* areatrigger) : AreaTriggerAI(areatrigger) {}

    void OnUnitEnter(Unit* unit) override
    {
        Unit* caster = at->GetCaster();

        if (!caster || !unit)
            return;

        if (caster->IsValidAttackTarget(unit))
        {
            caster->CastSpell(unit, SPELL_DH_DEMONIC_TRAMPLE_STUN, true);
            caster->CastSpell(unit, SPELL_DH_DEMONIC_TRAMPLE_DMG, true);
        }
    }
};

//201427 - Annihilation
class spell_dh_annihilation : public SpellScript
{

    void HandleHit(SpellMissInfo /*missInfo*/)
    {
        if (Unit* caster = GetCaster())
        {
            Unit* target = caster->GetVictim();
            if (!target)
                return;

            float attackPower = caster->GetTotalAttackPowerValue(BASE_ATTACK) + 28.7f;
            float damage = GetHitDamage();

            SetHitDamage(damage + attackPower);

            if (roll_chance(20))
                caster->ModifyPower(POWER_FURY, +20);
        }
    }

    void Register() override
    {
        BeforeHit += BeforeSpellHitFn(spell_dh_annihilation::HandleHit);
    }
};

// Chaos Strike (initial, without spec in mardum) - 344862
class spell_dh_chaos_strike_specless : public SpellScriptLoader
{
public:
    spell_dh_chaos_strike_specless() : SpellScriptLoader("spell_dh_chaos_strike_specless") {}

    class spell_dh_chaos_strike_specless_SpellScript : public SpellScript
    {

        void HandleOnCast()
        {
            Unit* caster = GetCaster();

            caster->CastSpell(nullptr, SPELL_DH_CHAOS_STRIKE, true);
        }

        void Register() override
        {
            OnCast += SpellCastFn(spell_dh_chaos_strike_specless_SpellScript::HandleOnCast);
        }
    };

    SpellScript* GetSpellScript() const override
    {
        return new spell_dh_chaos_strike_specless_SpellScript();
    }
};

// Fel Rush (initial, without spec in mardum) - 344865
class spell_dh_fel_rush_specless : public SpellScriptLoader
{
public:
    spell_dh_fel_rush_specless() : SpellScriptLoader("spell_dh_fel_rush_specless") {}

    class spell_dh_fel_rush_specless_SpellScript : public SpellScript
    {

        void HandleOnCast()
        {
            Unit* caster = GetCaster();

            caster->CastSpell(nullptr, SPELL_DH_FEL_RUSH, true);
        }

        void Register() override
        {
            OnCast += SpellCastFn(spell_dh_fel_rush_specless_SpellScript::HandleOnCast);
        }
    };

    SpellScript* GetSpellScript() const override
    {
        return new spell_dh_fel_rush_specless_SpellScript();
    }
};

class DH_DisableDoubleJump_OnMount : public PlayerScript
{
public:
    DH_DisableDoubleJump_OnMount() : PlayerScript("DH_DisableDoubleJump_OnMount") {}

    void OnSpellCast(Player* player, Spell* spell, bool /*skipCheck*/) override
    {
        if (player->GetClass() == CLASS_DEMON_HUNTER
            && player->HasAura(SPELL_DH_DOUBLE_JUMP)
            && spell->GetSpellInfo()->GetEffect(SpellEffIndex::EFFECT_0).ApplyAuraName == SPELL_AURA_MOUNTED)
            player->SetCanDoubleJump(false);
    }

    void OnUpdate(Player* player, uint32 /*diff*/) override
    {
        if (player->GetClass() == CLASS_DEMON_HUNTER
            && player->HasAura(SPELL_DH_DOUBLE_JUMP)
            && !player->IsMounted()
            && !player->HasExtraUnitMovementFlag(MOVEMENTFLAG2_CAN_DOUBLE_JUMP))
            player->SetCanDoubleJump(true);
    }
};

// Allow spec on level 10
class DemonHunterAllowSpec : public PlayerScript
{
public:
    DemonHunterAllowSpec() : PlayerScript("DemonHunterAllowSpec") {}

    void OnLevelChanged(Player* player, uint8 /*oldLevel*/) override
    {
        if (player->GetClass() == CLASS_DEMON_HUNTER && player->GetLevel() >= 10)
        {
            player->LearnSpell(218386, false);
        }
    }
};

// 204255, 204256 - Soul Fragment
// MiscId - 5977, 6007
struct at_dh_soul_fragment_havoc : AreaTriggerAI
{
    at_dh_soul_fragment_havoc(AreaTrigger* areatrigger) : AreaTriggerAI(areatrigger) {}

    void OnUnitEnter(Unit* unit) override
    {
        Unit* caster = at->GetCaster();
        if (!caster || !unit)
            return;

        if (caster == unit)
        {
            if (caster->GetClass() == CLASS_DEMON_HUNTER && at->GetCaster()->ToPlayer()->GetPrimarySpecialization() == ChrSpecialization::DemonHunterHavoc)
            {
                caster->CastSpell(caster, SPELL_DH_SOUL_FRAGMENT_HEAL_25_HAVOC, true);
            }
            else
            {
                caster->CastSpell(caster, SPELL_DH_SOUL_FRAGMENT_HEAL_VENGEANCE, true);
            }

            if (at->GetTemplate() && at->GetTemplate()->Id == AreaTriggerId(6007))
                caster->CastSpell(caster, SPELL_DH_SOUL_FRAGMENT_DEMON_BONUS, true);

            if (caster->HasAura(SPELL_DH_FEED_THE_DEMON))
            {
                if (SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(SPELL_DH_DEMON_SPIKES, DIFFICULTY_NONE))
                {
                    caster->GetSpellHistory()->ReduceChargeCooldown(spellInfo->ChargeCategoryId, 1000);
                }
            }

            // Painbringer
            if (caster->HasAura(SPELL_DH_PAINBRINGER))
                caster->CastSpell(caster, SPELL_DH_PAINBRINGER_BUFF, true);

            if (Aura* soulBarrier = caster->GetAura(SPELL_DH_SOUL_BARRIER))
            {
                if (AuraEffect* soulBarrierEffect = soulBarrier->GetEffect(EFFECT_0))
                {
                    if (SpellInfo const* spellInfo = sSpellMgr->AssertSpellInfo(SPELL_DH_SOUL_BARRIER, DIFFICULTY_NONE))
                    {
                        int32 basePoints = 0;
                        if (SpellEffectInfo const* effect = &spellInfo->GetEffect(EFFECT_1))
                        {
                            basePoints = effect->BasePoints;
                        }

                        float apCoeff = basePoints / 100.0f;

                        int32 bonusAmount = static_cast<int32>(caster->GetTotalAttackPowerValue(BASE_ATTACK) * apCoeff);

                        int32 currentAmount = soulBarrierEffect->GetAmount();
                        soulBarrierEffect->SetAmount(currentAmount + bonusAmount);
                    }
                }
            }

            at->SetDuration(0);
        }
    }
};

static void CollectSoulFragments(Unit* caster, uint32 maxCount, Spell const* triggeringSpell = nullptr)
{
    std::vector<AreaTrigger*> fragments = caster->GetAreaTriggers(SPELL_DH_SOUL_FRAGMENT_DEVOURER);
    uint32 collected = 0;
    for (AreaTrigger* at : fragments)
    {
        if (collected >= maxCount)
            break;

        CastSpellExtraArgs args(TRIGGERED_IGNORE_CAST_IN_PROGRESS | TRIGGERED_DONT_REPORT_CAST_ERROR);
        if (triggeringSpell)
            args.SetTriggeringSpell(triggeringSpell);
        caster->CastSpell(at->GetPosition(), SPELL_DH_CONSUME_SOUL_DEVOURER, args);

        if (caster->HasAura(SPELL_DH_VOID_METAMORPHOSIS_PASSIVE) && !caster->HasAura(SPELL_DH_VOID_METAMORPHOSIS_BUFF))
            caster->CastSpell(caster, SPELL_DH_VOID_METAMORPHOSIS_COUNTER,
                TRIGGERED_IGNORE_CAST_IN_PROGRESS | TRIGGERED_DONT_REPORT_CAST_ERROR);
        if (caster->HasAura(SPELL_DH_VOID_METAMORPHOSIS_BUFF) && caster->HasAura(SPELL_DH_COLLAPSING_STAR_PASSIVE))
            caster->CastSpell(caster, SPELL_DH_COLLAPSING_STAR_COUNTER,
                TRIGGERED_IGNORE_CAST_IN_PROGRESS | TRIGGERED_DONT_REPORT_CAST_ERROR);
        if (caster->HasAura(SPELL_DH_VOID_METAMORPHOSIS_BUFF) && caster->HasAura(SPELL_DH_EMPTINESS_PASSIVE))
            caster->CastSpell(caster, SPELL_DH_EMPTINESS_BUFF,
                TRIGGERED_IGNORE_CAST_IN_PROGRESS | TRIGGERED_DONT_REPORT_CAST_ERROR);
        if (Aura* fos = caster->GetAura(SPELL_DH_FEAST_OF_SOULS_PASSIVE))
            if (auto* script = fos->GetScript<spell_dh_feast_of_souls_tracker>())
                script->AddStack(caster);

        at->Remove();
        ++collected;
    }
}

static void TryFireVoidfallMeteors(Unit* caster, Unit* target)
{
    Aura* vfAura = caster->GetAura(SPELL_DH_VOIDFALL_PASSIVE);
    if (!vfAura)
        return;
    Aura* stack = caster->GetAura(SPELL_DH_VOIDFALL_STACK);
    if (!stack || stack->GetStackAmount() < 3)
        return;

    bool isVengeance = caster->HasAura(SPELL_DH_VENGEANCE_SPEC) && !caster->HasAura(SPELL_DH_DEVOURER_SPEC);
    bool meteoricFall = caster->HasAura(SPELL_DH_METEORIC_FALL_PASSIVE);
    uint32 meteorsToFire = meteoricFall ? stack->GetStackAmount() : 1;
    stack->ModStackAmount(-int32(meteorsToFire));

    bool hasWorldKiller = caster->HasAura(SPELL_DH_WORLD_KILLER_PASSIVE);
    auto* vfScript = hasWorldKiller ? vfAura->GetScript<spell_dh_voidfall_passive>() : nullptr;

    for (uint32 i = 0; i < meteorsToFire; ++i)
    {
        uint32 meteorCount = 1;
        if (vfScript)
            meteorCount = vfScript->IncrementMeteorCount();

        bool bigMeteor = hasWorldKiller && (meteorCount % 3 == 0);
        uint32 meteorSpell;
        if (bigMeteor)
            meteorSpell = isVengeance ? SPELL_DH_WORLD_KILLER_METEOR_VENGEANCE : SPELL_DH_WORLD_KILLER_METEOR_HAVOC;
        else
            meteorSpell = isVengeance ? SPELL_DH_VOIDFALL_METEOR_VENGEANCE : SPELL_DH_VOIDFALL_METEOR_HAVOC;

        caster->CastSpell(target, meteorSpell,
            TRIGGERED_IGNORE_CAST_IN_PROGRESS | TRIGGERED_DONT_REPORT_CAST_ERROR);

        if (bigMeteor && isVengeance)
            caster->CastSpell(caster, SPELL_DH_CONSUME_SOUL_SPAWN,
                TRIGGERED_IGNORE_CAST_IN_PROGRESS | TRIGGERED_DONT_REPORT_CAST_ERROR);

        if (caster->HasAura(SPELL_DH_FINAL_HOUR_PASSIVE))
            caster->CastSpell(caster, SPELL_DH_FINAL_HOUR_BUFF,
                TRIGGERED_IGNORE_CAST_IN_PROGRESS | TRIGGERED_DONT_REPORT_CAST_ERROR);
    }
}

// 1225789 - Void Metamorphosis (stack counter)
class spell_dh_void_metamorphosis_counter : public AuraScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_DH_CAN_METAMORPHOSIS });
    }

    void HandleApply(AuraEffect const* /*aurEff*/, AuraEffectHandleModes /*mode*/)
    {
        if (GetStackAmount() >= static_cast<int32>(GetSpellInfo()->StackAmount))
            GetTarget()->CastSpell(GetTarget(), SPELL_DH_CAN_METAMORPHOSIS,
                TRIGGERED_IGNORE_CAST_IN_PROGRESS | TRIGGERED_DONT_REPORT_CAST_ERROR);
    }

    void HandleRemove(AuraEffect const* /*aurEff*/, AuraEffectHandleModes /*mode*/)
    {
        GetTarget()->RemoveAura(SPELL_DH_CAN_METAMORPHOSIS);
    }

    void Register() override
    {
        AfterEffectApply += AuraEffectApplyFn(spell_dh_void_metamorphosis_counter::HandleApply, EFFECT_0, SPELL_AURA_DUMMY, AURA_EFFECT_HANDLE_REAPPLY);
        AfterEffectRemove += AuraEffectRemoveFn(spell_dh_void_metamorphosis_counter::HandleRemove, EFFECT_0, SPELL_AURA_DUMMY, AURA_EFFECT_HANDLE_REAL);
    }
};

// 1217607 - Void Metamorphosis (active buff)
class spell_dh_void_metamorphosis_buff : public AuraScript
{
    void HandleApply(AuraEffect const* /*aurEff*/, AuraEffectHandleModes /*mode*/)
    {
        SetMaxDuration(-1);
        SetDuration(-1);

        Unit* target = GetTarget();
        target->RemoveAura(SPELL_DH_VOID_METAMORPHOSIS_COUNTER);

        if (target->HasAura(SPELL_DH_MASS_ACCELERATION_PASSIVE))
        {
            int32 stacks = target->GetAuraEffectAmount(SPELL_DH_MASS_ACCELERATION_PASSIVE, EFFECT_0);
            for (int32 i = 0; i < stacks; ++i)
                target->CastSpell(target, SPELL_DH_VOIDFALL_STACK,
                    TRIGGERED_IGNORE_CAST_IN_PROGRESS | TRIGGERED_DONT_REPORT_CAST_ERROR);
            target->GetSpellHistory()->ResetCooldown(SPELL_DH_REAP, true);
        }

        if (target->HasAura(SPELL_DH_DARK_MATTER_PASSIVE))
            _darkMatterReady = true;

        if (target->HasAura(SPELL_DH_MIDNIGHT_SOUL_ERUPT))
        {
            int32 fragments = target->GetAuraEffectAmount(SPELL_DH_MIDNIGHT_SOUL_ERUPT, EFFECT_0);
            for (int32 i = 0; i < fragments; ++i)
                target->CastSpell(target, SPELL_DH_CONSUME_SOUL_SPAWN,
                    TRIGGERED_IGNORE_CAST_IN_PROGRESS | TRIGGERED_DONT_REPORT_CAST_ERROR);
            target->CastSpell(target, SPELL_DH_COLLAPSING_STAR_OVERRIDE,
                TRIGGERED_IGNORE_CAST_IN_PROGRESS | TRIGGERED_DONT_REPORT_CAST_ERROR);
        }
    }


    bool _darkMatterReady = false;
    float _drainAmount = 10.0f;

public:
    bool ConsumeDarkMatter()
    {
        if (!_darkMatterReady)
            return false;
        _darkMatterReady = false;
        return true;
    }

private:
    void HandlePeriodic(AuraEffect const* /*aurEff*/)
    {
        Unit* target = GetTarget();

        if (target->HasAura(SPELL_DH_VOID_RAY))
            return;

        // Sniff: 20 Fury/sec base, +2.5/sec per tick (EFFECT_10 BP=25 / 10)
        // Collapsing Star channel reduces drain by 70%
        float drain = _drainAmount;
        if (target->FindCurrentSpellBySpellId(SPELL_DH_COLLAPSING_STAR))
            drain *= 0.3f;
        target->ModifyPower(POWER_FURY, -int32(drain));
        _drainAmount += 1.5f;

        if (target->GetPower(POWER_FURY) <= 0)
        {
            target->SetPower(POWER_FURY, 0);
            Remove();
        }
    }

    void HandleRemove(AuraEffect const* /*aurEff*/, AuraEffectHandleModes /*mode*/)
    {
        Unit* target = GetTarget();

        if (target->HasAura(SPELL_DH_ROLLING_TORMENT_PASSIVE))
        {
            uint32 unusedFragments = uint32(target->GetAreaTriggers(SPELL_DH_SOUL_FRAGMENT_DEVOURER).size());
            if (Aura const* csCounter = target->GetAura(SPELL_DH_COLLAPSING_STAR_COUNTER))
                unusedFragments += uint32(csCounter->GetStackAmount());
            if (unusedFragments > 0)
            {
                int32 furyPerFragment = target->GetAuraEffectAmount(SPELL_DH_ROLLING_TORMENT_PASSIVE, EFFECT_0);
                target->ModifyPower(POWER_FURY, furyPerFragment * int32(unusedFragments));
                for (uint32 i = 0; i < unusedFragments; ++i)
                    target->CastSpell(target, SPELL_DH_ROLLING_TORMENT_BUFF,
                        TRIGGERED_IGNORE_CAST_IN_PROGRESS | TRIGGERED_DONT_REPORT_CAST_ERROR);
            }
        }

        target->RemoveAura(SPELL_DH_VOID_METAMORPHOSIS_COUNTER);
        target->RemoveAura(SPELL_DH_COLLAPSING_STAR_COUNTER);
        target->RemoveAura(SPELL_DH_COLLAPSING_STAR_OVERRIDE);
        target->RemoveAura(SPELL_DH_EMPTINESS_BUFF);
    }

    void PreventBreakableCC(AuraEffect* /*aurEff*/, ProcEventInfo& /*eventInfo*/)
    {
        PreventDefaultAction();
    }

    void Register() override
    {
        AfterEffectApply += AuraEffectApplyFn(spell_dh_void_metamorphosis_buff::HandleApply, EFFECT_6, SPELL_AURA_PERIODIC_DUMMY, AURA_EFFECT_HANDLE_REAL);
        OnEffectPeriodic += AuraEffectPeriodicFn(spell_dh_void_metamorphosis_buff::HandlePeriodic, EFFECT_6, SPELL_AURA_PERIODIC_DUMMY);
        AfterEffectRemove += AuraEffectRemoveFn(spell_dh_void_metamorphosis_buff::HandleRemove, EFFECT_6, SPELL_AURA_PERIODIC_DUMMY, AURA_EFFECT_HANDLE_REAL);
        OnEffectProc += AuraEffectProcFn(spell_dh_void_metamorphosis_buff::PreventBreakableCC, EFFECT_11, SPELL_AURA_TRANSFORM);
        OnEffectProc += AuraEffectProcFn(spell_dh_void_metamorphosis_buff::PreventBreakableCC, EFFECT_12, SPELL_AURA_TRANSFORM);
    }
};

// 1217610 - Devour (enhanced Consume during Void Metamorphosis)
class spell_dh_devour : public SpellScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_DH_REAP, SPELL_DH_CONSUME_SOUL_SPAWN, SPELL_DH_SHATTERED_SOUL_DEVOURER_LESSER_RIGHT });
    }

    void HandleAfterCast()
    {
        Unit* caster = GetCaster();
        Unit* target = GetExplTargetUnit();
        if (!target)
            return;

        target->CastSpell(caster, SPELL_DH_CONSUME_SOUL_SPAWN, CastSpellExtraArgsInit{
            .TriggerFlags = TRIGGERED_IGNORE_CAST_IN_PROGRESS | TRIGGERED_DONT_REPORT_CAST_ERROR,
            .TriggeringSpell = GetSpell()
            });
        target->CastSpell(caster, SPELL_DH_SHATTERED_SOUL_DEVOURER_LESSER_RIGHT, CastSpellExtraArgsInit{
            .TriggerFlags = TRIGGERED_IGNORE_CAST_IN_PROGRESS | TRIGGERED_DONT_REPORT_CAST_ERROR,
            .TriggeringSpell = GetSpell()
            });

        caster->GetSpellHistory()->ModifyCooldown(SPELL_DH_REAP, Milliseconds(-1000));
        caster->GetSpellHistory()->ModifyCooldown(SPELL_DH_CULL, Milliseconds(-1000));
        caster->GetSpellHistory()->ModifyCooldown(SPELL_DH_ERADICATE, Milliseconds(-1000));
    }

    void Register() override
    {
        AfterCast += SpellCastFn(spell_dh_devour::HandleAfterCast);
    }
};

class spell_dh_entropy : public AuraScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_DH_CONSUME_SOUL_SPAWN, SPELL_DH_VOID_METAMORPHOSIS_COUNTER });
    }

    void HandlePeriodic(AuraEffect const* /*aurEff*/)
    {
        Unit* target = GetTarget();

        if (target->IsInCombat())
        {
            target->CastSpell(target, SPELL_DH_CONSUME_SOUL_SPAWN,
                TRIGGERED_IGNORE_CAST_IN_PROGRESS | TRIGGERED_DONT_REPORT_CAST_ERROR);
        }
        else if (target->HasAura(SPELL_DH_VOID_METAMORPHOSIS_PASSIVE) && !target->HasAura(SPELL_DH_VOID_METAMORPHOSIS_BUFF))
        {
            if (Aura* counter = target->GetAura(SPELL_DH_VOID_METAMORPHOSIS_COUNTER))
            {
                if (counter->GetStackAmount() < 25)
                    target->CastSpell(target, SPELL_DH_VOID_METAMORPHOSIS_COUNTER,
                        TRIGGERED_IGNORE_CAST_IN_PROGRESS | TRIGGERED_DONT_REPORT_CAST_ERROR);
            }
            else
                target->CastSpell(target, SPELL_DH_VOID_METAMORPHOSIS_COUNTER,
                    TRIGGERED_IGNORE_CAST_IN_PROGRESS | TRIGGERED_DONT_REPORT_CAST_ERROR);
        }
    }

    void Register() override
    {
        OnEffectPeriodic += AuraEffectPeriodicFn(spell_dh_entropy::HandlePeriodic, EFFECT_0, SPELL_AURA_PERIODIC_DUMMY);
    }
};

// 1250088 - Midnight: Collapsing Star crit damage += SpellCrit% * EFFECT_2_BP%; recalculated every 1s via PERIODIC_DUMMY
class spell_dh_midnight_passive : public AuraScript
{
    void HandlePeriodic(AuraEffect const* /*aurEff*/)
    {
        Player* player = GetTarget()->ToPlayer();
        if (!player)
            return;

        float critPct = *player->m_activePlayerData->SpellCritPercentage;
        int32 scalingPct = GetEffect(EFFECT_2)->GetAmount();
        int32 newAmount = int32(critPct * float(scalingPct) / 100.0f);
        GetEffect(EFFECT_0)->ChangeAmount(newAmount);
    }

    void Register() override
    {
        OnEffectPeriodic += AuraEffectPeriodicFn(spell_dh_midnight_passive::HandlePeriodic, EFFECT_1, SPELL_AURA_PERIODIC_DUMMY);
    }
};

// 1221167 - Collapsing Star (passive talent marker)
class spell_dh_collapsing_star_counter : public AuraScript
{
    void HandleApply(AuraEffect const* /*aurEff*/, AuraEffectHandleModes /*mode*/)
    {
        if (GetStackAmount() >= 30)
            GetTarget()->CastSpell(GetTarget(), SPELL_DH_COLLAPSING_STAR_OVERRIDE,
                TRIGGERED_IGNORE_CAST_IN_PROGRESS | TRIGGERED_DONT_REPORT_CAST_ERROR);
    }

    void Register() override
    {
        AfterEffectApply += AuraEffectApplyFn(spell_dh_collapsing_star_counter::HandleApply, EFFECT_0, SPELL_AURA_SET_ACTION_BUTTON_SPELL_COUNT, AURA_EFFECT_HANDLE_REAPPLY);
    }
};

// 1221150 - Collapsing Star
class spell_dh_collapsing_star : public SpellScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_DH_COLLAPSING_STAR_DAMAGE, SPELL_DH_METEOR_SHOWER_DEVOURER, SPELL_DH_METEOR_SHOWER_VENGEANCE });
    }

    void HandleDummy(SpellEffIndex /*effIndex*/)
    {
        Unit* caster = GetCaster();
        Unit* target = GetHitUnit();
        if (!target)
            return;

        caster->CastSpell(target, SPELL_DH_COLLAPSING_STAR_DAMAGE,
            CastSpellExtraArgs(TRIGGERED_IGNORE_CAST_IN_PROGRESS | TRIGGERED_DONT_REPORT_CAST_ERROR)
            .SetTriggeringSpell(GetSpell()));

        if (caster->HasAura(SPELL_DH_COLLAPSING_STAR_FRAGMENTS))
        {
            for (uint32 i = 0; i < 3; ++i)
                caster->CastSpell(caster, SPELL_DH_CONSUME_SOUL_SPAWN,
                    CastSpellExtraArgs(TRIGGERED_IGNORE_CAST_IN_PROGRESS | TRIGGERED_DONT_REPORT_CAST_ERROR)
                    .SetTriggeringSpell(GetSpell()));
        }

        // Dark Matter: first Collapsing Star after Void Meta fires meteor shower
        if (Aura* metaBuff = caster->GetAura(SPELL_DH_VOID_METAMORPHOSIS_BUFF))
        {
            if (auto* metaScript = metaBuff->GetScript<spell_dh_void_metamorphosis_buff>())
            {
                if (metaScript->ConsumeDarkMatter())
                {
                    bool isVengeance = caster->HasAura(SPELL_DH_VENGEANCE_SPEC) && !caster->HasAura(SPELL_DH_DEVOURER_SPEC);
                    uint32 showerSpell = isVengeance ? SPELL_DH_METEOR_SHOWER_VENGEANCE : SPELL_DH_METEOR_SHOWER_DEVOURER;
                    int32 count = caster->GetAuraEffectAmount(SPELL_DH_DARK_MATTER_PASSIVE, EFFECT_0);
                    for (int32 i = 0; i < count; ++i)
                    {
                        float angle = frand(0.0f, float(2 * M_PI));
                        float dist = frand(0.0f, 8.0f);
                        Position dest = target->GetPosition();
                        dest.m_positionX += dist * std::cos(angle);
                        dest.m_positionY += dist * std::sin(angle);
                        caster->m_Events.AddEventAtOffset(
                            new event_dh_dark_matter_meteor(caster, dest, showerSpell),
                            Milliseconds(i * 500));
                    }
                }
            }
        }

        caster->RemoveAura(SPELL_DH_COLLAPSING_STAR_COUNTER);
        caster->RemoveAura(SPELL_DH_COLLAPSING_STAR_OVERRIDE);
    }

    void Register() override
    {
        OnEffectHitTarget += SpellEffectFn(spell_dh_collapsing_star::HandleDummy, EFFECT_0, SPELL_EFFECT_DUMMY);
    }
};

// 1246169 - The Hunt damage
class spell_dh_the_hunt_damage : public SpellScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_DH_HUNGERING_SLASH_PASSIVE, SPELL_DH_HUNGERING_SLASH_OVERRIDE });
    }

    void HandleAfterCast() const
    {
        Unit* caster = GetCaster();
        if (caster->HasAura(SPELL_DH_HUNGERING_SLASH_PASSIVE))
            caster->CastSpell(caster, SPELL_DH_HUNGERING_SLASH_OVERRIDE,
                TRIGGERED_IGNORE_CAST_IN_PROGRESS | TRIGGERED_DONT_REPORT_CAST_ERROR);
    }

    void Register() override
    {
        AfterCast += SpellCastFn(spell_dh_the_hunt_damage::HandleAfterCast);
    }
};

// 1239123 - Hungering Slash
class spell_dh_hungering_slash : public SpellScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_DH_HUNGERING_SLASH_DAMAGE, SPELL_DH_HUNGERING_SLASH_FURY,
            SPELL_DH_HUNGERING_SLASH_VOIDSTEP, SPELL_DH_VENGEFUL_RETREAT_TRIGGER });
    }

    void HandleAfterCast() const
    {
        Unit* caster = GetCaster();

        caster->CastSpell(caster, SPELL_DH_HUNGERING_SLASH_DAMAGE,
            CastSpellExtraArgs(TRIGGERED_IGNORE_CAST_IN_PROGRESS | TRIGGERED_DONT_REPORT_CAST_ERROR)
            .SetTriggeringSpell(GetSpell()));

        caster->CastSpell(caster, SPELL_DH_HUNGERING_SLASH_FURY,
            CastSpellExtraArgs(TRIGGERED_IGNORE_CAST_IN_PROGRESS | TRIGGERED_DONT_REPORT_CAST_ERROR)
            .SetTriggeringSpell(GetSpell()));

        uint32 maxFragments = GetSpellInfo()->GetEffect(EFFECT_0).CalcValue(caster);
        std::list<Unit*> nearbyEnemies;
        Trinity::AnyUnfriendlyUnitInObjectRangeCheck check(caster, caster, 20.0f);
        Trinity::UnitListSearcher<Trinity::AnyUnfriendlyUnitInObjectRangeCheck> searcher(caster, nearbyEnemies, check);
        Cell::VisitAllObjects(caster, searcher, 20.0f);
        nearbyEnemies.sort(Trinity::ObjectDistanceOrderPred(caster));

        uint32 shattered = 0;
        for (Unit* enemy : nearbyEnemies)
        {
            if (shattered >= maxFragments)
                break;
            enemy->CastSpell(caster, SPELL_DH_CONSUME_SOUL_SPAWN,
                CastSpellExtraArgs(TRIGGERED_IGNORE_CAST_IN_PROGRESS | TRIGGERED_DONT_REPORT_CAST_ERROR)
                .SetTriggeringSpell(GetSpell()));
            ++shattered;
        }

        caster->CastSpell(caster, 444929, CastSpellExtraArgs(TRIGGERED_IGNORE_CAST_IN_PROGRESS | TRIGGERED_DONT_REPORT_CAST_ERROR).SetTriggeringSpell(GetSpell()));

        if (Aura* freeCastAura = caster->GetAura(444929))
        {
            freeCastAura->SetDuration(6000);
            freeCastAura->SetMaxDuration(6000);
        }

        caster->CastSpell(caster, SPELL_DH_HUNGERING_SLASH_VOIDSTEP,
            CastSpellExtraArgs(TRIGGERED_IGNORE_CAST_IN_PROGRESS | TRIGGERED_DONT_REPORT_CAST_ERROR)
            .SetTriggeringSpell(GetSpell()));

        caster->RemoveAurasDueToSpell(SPELL_DH_HUNGERING_SLASH_OVERRIDE);
    }

    void Register() override
    {
        AfterCast += SpellCastFn(spell_dh_hungering_slash::HandleAfterCast);
    }
};

// 1225827/1279200 - Eradicate damage
class spell_dh_eradicate_damage : public SpellScript
{
    void FilterTargets(std::list<WorldObject*>& targets)
    {
        Unit* caster = GetCaster();
        targets.remove_if([&](WorldObject* target) -> bool
            {
                return !caster->IsWithinDist(target, 25.0f) || !caster->HasInArc(float(M_PI) / 2.0f, target);
            });
    }

    void Register() override
    {
        OnObjectAreaTargetSelect += SpellObjectAreaTargetSelectFn(spell_dh_eradicate_damage::FilterTargets, EFFECT_0, TARGET_UNIT_LINE_CASTER_TO_DEST_ENEMY);
    }
};

// 1242504 - Emptiness (0.25% haste per stack, up to 100 stacks = 25%)
class spell_dh_emptiness_buff : public AuraScript
{
    float _appliedHaste = 0.0f;

    void HandleCalcAmount(AuraEffect const* /*aurEff*/, SpellEffectValue& amount, bool& canBeRecalculated)
    {
        amount = 0;
        canBeRecalculated = false;
    }

    void HandleApply(AuraEffect const* /*aurEff*/, AuraEffectHandleModes /*mode*/)
    {
        Unit* target = GetTarget();
        float newHaste = 0.25f * float(GetStackAmount());
        if (_appliedHaste > 0.0f)
        {
            target->ApplyCastTimePercentMod(_appliedHaste, false);
            target->ApplyAttackTimePercentMod(BASE_ATTACK, _appliedHaste, false);
            target->ApplyAttackTimePercentMod(OFF_ATTACK, _appliedHaste, false);
            target->ApplyAttackTimePercentMod(RANGED_ATTACK, _appliedHaste, false);
        }
        target->ApplyCastTimePercentMod(newHaste, true);
        target->ApplyAttackTimePercentMod(BASE_ATTACK, newHaste, true);
        target->ApplyAttackTimePercentMod(OFF_ATTACK, newHaste, true);
        target->ApplyAttackTimePercentMod(RANGED_ATTACK, newHaste, true);
        _appliedHaste = newHaste;
    }

    void HandleRemove(AuraEffect const* /*aurEff*/, AuraEffectHandleModes /*mode*/)
    {
        if (_appliedHaste > 0.0f)
        {
            Unit* target = GetTarget();
            target->ApplyCastTimePercentMod(_appliedHaste, false);
            target->ApplyAttackTimePercentMod(BASE_ATTACK, _appliedHaste, false);
            target->ApplyAttackTimePercentMod(OFF_ATTACK, _appliedHaste, false);
            target->ApplyAttackTimePercentMod(RANGED_ATTACK, _appliedHaste, false);
            _appliedHaste = 0.0f;
        }
    }

    void Register() override
    {
        DoEffectCalcAmount += AuraEffectCalcAmountFn(spell_dh_emptiness_buff::HandleCalcAmount, EFFECT_0, SPELL_AURA_MELEE_SLOW);
        AfterEffectApply += AuraEffectApplyFn(spell_dh_emptiness_buff::HandleApply, EFFECT_0, SPELL_AURA_MELEE_SLOW, AURA_EFFECT_HANDLE_REAL_OR_REAPPLY_MASK);
        AfterEffectRemove += AuraEffectRemoveFn(spell_dh_emptiness_buff::HandleRemove, EFFECT_0, SPELL_AURA_MELEE_SLOW, AURA_EFFECT_HANDLE_REAL);
    }
};

// 1256305/1256306/1256617/1256619 - Voidfall/World Killer meteor damage: apply Catastrophe DoT if passive present
class spell_dh_voidfall_meteor_damage : public SpellScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_DH_CATASTROPHE_DOT });
    }

    void HandleHit(SpellEffIndex /*effIndex*/)
    {
        Unit* caster = GetOriginalCaster();
        if (!caster)
            caster = GetCaster();
        Unit* target = GetHitUnit();
        if (!caster || !target)
            return;

        if (!caster->HasAura(SPELL_DH_CATASTROPHE_PASSIVE))
            return;

        int32 pct = caster->GetAuraEffectAmount(SPELL_DH_CATASTROPHE_PASSIVE, EFFECT_1);
        int32 dotDmg = int32(GetHitDamage() * pct / 100) / 4;
        caster->CastSpell(target, SPELL_DH_CATASTROPHE_DOT,
            CastSpellExtraArgs(TRIGGERED_IGNORE_CAST_IN_PROGRESS | TRIGGERED_DONT_REPORT_CAST_ERROR)
            .AddSpellMod(SPELLVALUE_BASE_POINT0, dotDmg));
    }

    void Register() override
    {
        OnEffectHitTarget += SpellEffectFn(spell_dh_voidfall_meteor_damage::HandleHit, EFFECT_0, SPELL_EFFECT_SCHOOL_DAMAGE);
    }
};


// 1246556 - Spontaneous Immolation: 5% chance on damaging spell cast to trigger Soul Immolation
class spell_dh_spontaneous_immolation : public AuraScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_DH_SPONTANEOUS_IMMOLATION_BUFF });
    }

    static bool CheckProc(AuraScript const& script, ProcEventInfo const& eventInfo)
    {
        if (!eventInfo.GetSpellInfo())
            return false;
        int32 chance = static_cast<spell_dh_spontaneous_immolation const&>(script).GetEffect(EFFECT_0)->GetAmount();
        return roll_chance(chance);
    }

    void HandleProc(AuraEffect const* /*aurEff*/, ProcEventInfo const& /*eventInfo*/) const
    {
        GetTarget()->CastSpell(GetTarget(), SPELL_DH_SPONTANEOUS_IMMOLATION_BUFF,
            TRIGGERED_FULL_MASK | TRIGGERED_SUPPRESS_CASTER_ANIM);
    }

    void Register() override
    {
        DoCheckProc += AuraCheckProcFn(spell_dh_spontaneous_immolation::CheckProc);
        OnEffectProc += AuraEffectProcFn(spell_dh_spontaneous_immolation::HandleProc, EFFECT_0, SPELL_AURA_DUMMY);
    }
};

// 1241937 - Soul Immolation: periodic dummy spawns soul fragments + grants Fury each tick
class spell_dh_soul_immolation : public AuraScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_DH_CONSUME_SOUL_SPAWN, SPELL_DH_SOUL_IMMOLATION_FURY });
    }

    void HandlePeriodic(AuraEffect const* aurEff)
    {
        Unit* target = GetTarget();
        int32 totalFragments = aurEff->GetAmount();
        if (int32(aurEff->GetTickNumber()) <= totalFragments)
            target->CastSpell(target, SPELL_DH_CONSUME_SOUL_SPAWN,
                TRIGGERED_IGNORE_CAST_IN_PROGRESS | TRIGGERED_DONT_REPORT_CAST_ERROR);

        target->CastSpell(target, SPELL_DH_SOUL_IMMOLATION_FURY,
            TRIGGERED_IGNORE_CAST_IN_PROGRESS | TRIGGERED_DONT_REPORT_CAST_ERROR);
    }

    void Register() override
    {
        OnEffectPeriodic += AuraEffectPeriodicFn(spell_dh_soul_immolation::HandlePeriodic, EFFECT_0, SPELL_AURA_PERIODIC_DUMMY);
    }
};

// 1266619 - First In, Last Out
class spell_first_in_last_out : public AuraScript
{
    int32 _initialAbsorb = 0;
    int32 _decayPerTick = 0;

    bool CheckProc(ProcEventInfo& eventInfo)
    {
        if (eventInfo.GetSpellInfo()->Id == SPELL_DH_INFERNAL_STRIKE_CAST
            || eventInfo.GetSpellInfo()->Id == SPELL_DH_FEL_RUSH
            || eventInfo.GetSpellInfo()->Id == SPELL_DH_SHIFT)
            return true;
        return false;
    }

    void CalcAmount(AuraEffect const* /*aurEff*/, SpellEffectValue& amount, bool& /*canBeRecalculated*/)
    {
        Unit* caster = GetCaster();
        if (!caster)
            return;

        _initialAbsorb = CalculatePct(caster->GetMaxHealth(), 6);
        amount = _initialAbsorb;

        int32 duration = GetSpellInfo()->GetDuration();
        int32 periodic = GetSpellInfo()->GetEffect(EFFECT_1).ApplyAuraPeriod;
        if (periodic > 0 && duration > 0)
            _decayPerTick = std::max<int32>(1, _initialAbsorb / (duration / periodic));
    }

    void HandlePeriodic(AuraEffect const* /*aurEff*/)
    {
        if (_initialAbsorb <= 0 || _decayPerTick <= 0)
            return;

        Aura* aura = GetAura();
        if (!aura)
            return;

        AuraEffect* absorbEff = aura->GetEffect(EFFECT_0);
        if (!absorbEff)
            return;

        int32 currentAmount = absorbEff->GetAmount();
        if (currentAmount <= 0)
            return;

        int32 newAmount = std::max<int32>(0, currentAmount - _decayPerTick);
        absorbEff->SetAmount(newAmount);

        if (Unit* target = GetTarget())
            if (AuraApplication* app = target->GetAuraApplication(SPELL_DH_FIRST_IN_LAST_OUT))
                app->ClientUpdate();
    }

    void Register() override
    {
        DoCheckProc += AuraCheckProcFn(spell_first_in_last_out::CheckProc);
        DoEffectCalcAmount += AuraEffectCalcAmountFn(spell_first_in_last_out::CalcAmount, EFFECT_0, SPELL_AURA_SCHOOL_ABSORB);
        OnEffectPeriodic += AuraEffectPeriodicFn(spell_first_in_last_out::HandlePeriodic, EFFECT_1, SPELL_AURA_PERIODIC_DUMMY);
    }
};

void AddSC_demon_hunter_spell_scripts()
{
    RegisterSpellScript(spell_dh_army_unto_oneself);
    RegisterSpellScript(spell_dh_calcified_spikes);
    RegisterSpellScript(spell_dh_calcified_spikes_periodic);
    RegisterSpellScript(spell_dh_chaos_strike);
    RegisterSpellScript(spell_dh_chaos_strike_initial);
    RegisterSpellScript(spell_dh_chaos_theory);
    RegisterSpellScript(spell_dh_chaos_theory_drop_charge);
    RegisterSpellScript(spell_dh_chaotic_transformation);
    RegisterSpellScript(spell_dh_charred_warblades);
    RegisterSpellScript(spell_dh_collective_anguish);
    RegisterSpellScript(spell_dh_collective_anguish_eye_beam);
    RegisterSpellScript(spell_dh_consume_energize);
    RegisterSpellScript(spell_dh_consume_soul_vengeance_lesser);
    RegisterSpellScript(spell_dh_critical_chaos);
    RegisterSpellScript(spell_dh_cull);
    RegisterSpellScript(spell_dh_cycle_of_binding);
    RegisterSpellScript(spell_dh_cycle_of_hatred);
    RegisterSpellScript(spell_dh_cycle_of_hatred_remove_stacks);
    RegisterSpellScript(spell_dh_cycle_of_hatred_talent);
    RegisterSpellScript(spell_dh_darkglare_boon);
    RegisterSpellScript(spell_dh_darkness);
    RegisterSpellScript(spell_dh_deflecting_spikes);
    RegisterSpellScript(spell_dh_demon_muzzle);
    RegisterSpellScriptWithArgs(spell_dh_demonic, "spell_dh_demonic_havoc", SPELL_DH_METAMORPHOSIS_TRANSFORM);
    RegisterSpellScriptWithArgs(spell_dh_demonic, "spell_dh_demonic_vengeance", SPELL_DH_METAMORPHOSIS_VENGEANCE_TRANSFORM);
    RegisterSpellScript(spell_dh_demonic_appetite);
    RegisterSpellScript(spell_dh_demonic_appetite_energize);
    RegisterSpellScript(spell_dh_demon_spikes);
    RegisterSpellScriptWithArgs(spell_dh_elysian_decree, "spell_dh_elysian_decree", SPELL_DH_ELYSIAN_DECREE);
    RegisterAreaTriggerAI(at_dh_elysian_decree);
    RegisterSpellScript(spell_dh_enduring_torment);
    RegisterSpellScript(spell_dh_enduring_torment_buff);
    RegisterSpellScript(spell_dh_eradicate);
    RegisterSpellScript(spell_dh_essence_break);
    RegisterSpellScript(spell_dh_eye_beam);
    RegisterSpellScript(spell_dh_feast_of_souls);
    RegisterSpellScript(spell_dh_fel_devastation);
    RegisterSpellScript(spell_dh_fel_flame_fortification);
    RegisterSpellScript(spell_dh_felblade);
    RegisterSpellScript(spell_dh_felblade_charge);
    RegisterSpellScript(spell_dh_felblade_cooldown_reset_proc);
    RegisterSpellScript(spell_dh_fiery_brand);
    RegisterSpellScript(spell_dh_furious_gaze);
    RegisterAreaTriggerAI(at_dh_glaive_tempest);
    RegisterSpellScript(spell_dh_inner_demon);
    RegisterAreaTriggerAI(at_dh_inner_demon);
    RegisterSpellScript(spell_dh_know_your_enemy);
    RegisterSpellScript(spell_dh_last_resort);
    RegisterSpellScript(spell_dh_moment_of_craving);
    RegisterSpellScript(spell_dh_monster_rising);
    RegisterSpellScript(spell_dh_painbringer);
    RegisterSpellScript(spell_dh_painbringer_reduce_damage);
    RegisterSpellScript(spell_dh_reap);
    RegisterSpellScript(spell_dh_repeat_decree_conduit);
    RegisterSpellScript(spell_dh_restless_hunter);
    RegisterSpellScript(spell_dh_retaliation);
    RegisterSpellScript(spell_dh_shattered_destiny);
    RegisterSpellScript(spell_dh_shattered_restoration);
    RegisterSpellScriptWithArgs(spell_dh_shattered_souls, "spell_dh_shattered_souls_havoc", SPELL_DH_SHATTERED_SOULS_HAVOC);
    RegisterSpellScriptWithArgs(spell_dh_shattered_souls, "spell_dh_shattered_souls_vengeance", SPELL_DH_SHATTER_SOUL);
    RegisterSpellScriptWithArgs(spell_dh_shattered_souls_trigger, "spell_dh_shattered_souls_devourer_trigger", SPELL_DH_SHATTERED_SOULS_DEVOURER_DUMMY, 0);
    RegisterSpellScriptWithArgs(spell_dh_shattered_souls_trigger, "spell_dh_shattered_souls_havoc_trigger", SPELL_DH_SHATTERED_SOULS_HAVOC_SHATTERED_TRIGGER, SPELL_DH_SHATTERED_SOULS_HAVOC_DEMON_TRIGGER);
    RegisterSpellScriptWithArgs(spell_dh_shattered_souls_trigger, "spell_dh_shattered_souls_havoc_trigger_lesser", SPELL_DH_SHATTERED_SOULS_HAVOC_LESSER_TRIGGER, 0);
    RegisterSpellScriptWithArgs(spell_dh_shattered_souls_trigger, "spell_dh_shattered_souls_vengeance_trigger", SPELL_DH_SHATTERED_SOULS_V_SHATTERED_TRIGGER, SPELL_DH_SHATTERED_SOULS_V_DEMON_TRIGGER);
    RegisterSpellScriptWithArgs(spell_dh_shattered_souls_trigger, "spell_dh_shattered_souls_vengeance_trigger_lesser", SPELL_DH_SHATTERED_SOUL, 0);
    RegisterSpellScript(spell_dh_shattered_souls_devourer);
    RegisterSpellScript(spell_dh_shattered_souls_devourer_dummy);
    RegisterAreaTriggerAI(at_dh_shattered_souls_devourer);
    RegisterAreaTriggerAI(at_dh_shattered_souls_havoc_demon);
    RegisterAreaTriggerAI(at_dh_shattered_souls_havoc_lesser);
    RegisterAreaTriggerAI(at_dh_shattered_souls_havoc_shattered);
    RegisterAreaTriggerAI(at_dh_shattered_souls_vengeance_demon);
    RegisterAreaTriggerAI(at_dh_shattered_souls_vengeance_lesser);
    RegisterAreaTriggerAI(at_dh_shattered_souls_vengeance_shattered);
    RegisterSpellScript(spell_dh_shift);
    RegisterSpellScript(spell_dh_sigil_of_chains);
    RegisterSpellScript(spell_dh_sigil_of_flame);
    RegisterSpellScriptWithArgs(spell_dh_elysian_decree, "spell_dh_sigil_of_spite", SPELL_DH_SIGIL_OF_SPITE);
    RegisterSpellScript(spell_dh_soulmonger);
    RegisterSpellAndAuraScriptPair(spell_dh_soul_carver, spell_dh_soul_carver_aura);
    RegisterSpellScript(spell_dh_soul_fragments_damage_taken_tracker);
    RegisterSpellScript(spell_dh_soul_sigils);
    RegisterSpellScript(spell_dh_student_of_suffering);
    RegisterSpellScript(spell_dh_tactical_retreat);
    RegisterSpellScript(spell_dh_unhindered_assault);
    RegisterSpellScript(spell_dh_vengeful_retreat_damage);
    RegisterSpellScript(spell_dh_violent_transformation);
    RegisterSpellScript(spell_dh_voidblade);
    RegisterSpellScript(spell_dh_voidblade_charge);
    RegisterSpellScript(spell_dh_voidglare_boon);
    RegisterSpellScript(spell_dh_entropy);
    RegisterSpellScript(spell_dh_feast_of_souls_tracker);
    RegisterSpellScript(spell_dh_midnight_passive);
    RegisterSpellScript(spell_dh_collapsing_star_counter);
    RegisterSpellScript(spell_dh_collapsing_star);
    RegisterSpellScript(spell_dh_void_ray);
    RegisterSpellScript(spell_dh_wave_of_debilitation);

    RegisterAreaTriggerAI(areatrigger_dh_darkness);
    RegisterAreaTriggerAI(areatrigger_dh_sigil_of_chains);
    RegisterAreaTriggerAI(areatrigger_dh_sigil_of_flame);
    RegisterAreaTriggerAI(areatrigger_dh_sigil_of_silence);
    RegisterAreaTriggerAI(areatrigger_dh_sigil_of_misery);
    RegisterAreaTriggerAI(areatrigger_dh_sigil_of_spite);

    // Havoc

    RegisterSpellScript(spell_dh_first_blood);
    RegisterSpellScript(spell_dh_blade_dance);
    RegisterSpellScript(spell_dh_blade_dance_damage);

    // Vengeance
    RegisterSpellScript(spell_dh_soul_furnace);

    // Vengeance & Havoc
    RegisterSpellAndAuraScriptPair(spell_dh_glide, spell_dh_glide_AuraScript);
    RegisterSpellScript(spell_dh_glide_timer);

    // Soulbind conduits
    RegisterSpellScript(spell_dh_soul_furnace_conduit);

    //Custom
    RegisterSpellScript(spell_dh_fel_rush);
    RegisterSpellAndAuraScriptPair(spell_dh_fel_rush_dash, spell_dh_fel_rush_dash_aura);
    RegisterSpellScript(spell_dh_fel_rush_dash_ground);
    RegisterSpellScript(spell_dh_fel_rush_damage);
    RegisterSpellScript(spell_dh_master_of_the_glaive);
    RegisterSpellScript(spell_dh_fel_eruption);
    RegisterSpellScript(spell_dh_metamorphosis);
    RegisterSpellScript(spell_dh_bloodlet);
    RegisterSpellScript(spell_dh_fel_barrage);
    RegisterSpellScript(spell_dh_fel_barrage_damage);
    RegisterSpellScript(spell_dh_fel_barrage_aura);
    RegisterSpellScript(spell_dh_nemesis);
    RegisterSpellScript(spell_dh_infernal_strike);
    RegisterSpellScript(spell_dh_soul_cleave);
    RegisterSpellScript(spell_dh_soul_cleave_damage);
    RegisterSpellScript(spell_dh_razor_spikes);
    RegisterSpellScript(spell_dh_soul_barrier);
    RegisterSpellScript(spell_dh_nether_bond);
    RegisterSpellScript(spell_dh_nether_bond_periodic);
    RegisterSpellScript(spell_dh_solitude);
    RegisterSpellScript(spell_dh_awaken_the_demon);
    RegisterSpellScript(spell_dh_reverse_magic);
    RegisterSpellScript(spell_dh_eye_of_leotheras);
    RegisterSpellScript(spell_dh_immolation_aura);
    RegisterSpellScript(spell_dh_immolation_aura_damage);
    RegisterSpellScript(spell_dh_jagged_spikes);
    RegisterSpellScript(spell_dh_fel_lance);
    RegisterSpellScript(spell_dh_intimidated);
    RegisterSpellScript(spell_dh_blade_turning);
    RegisterSpellScript(spell_dh_shear_proc);
    RegisterSpellScript(spell_dh_consume_soul_missile);
    RegisterSpellScript(spell_dh_darkness_absorb);
    RegisterSpellScript(spell_dh_prepared);
    RegisterSpellScript(spell_dh_soul_fragment_heals);
    RegisterSpellScript(spell_dh_chaos_cleave);
    RegisterSpellScript(spell_dh_desperate_instincts);
    new spell_dh_fracture();
    new spell_dh_frailty();
    new spell_dh_spirit_bomb();
    new spell_dh_demonic_infusion();
    new spell_demon_hunter_rain_from_above();
    new spell_demon_hunter_metamorphosis_buffs();
    RegisterSpellScript(spell_dh_fel_devastation_damage);
    RegisterSpellScript(spell_demon_hunter_chaos_strike);
    new spell_demon_hunter_pain();
    new spell_demon_hunter_mana_break();
    new spell_demon_hunter_trail_of_ruin();
    new spell_demon_hunter_unending_hatred();
    RegisterAreaTriggerAI(at_dh_darkness);
    RegisterAreaTriggerAI(at_demon_hunter_mana_rift);
    RegisterAreaTriggerAI(at_demon_hunter_demonic_trample);
    RegisterSpellScript(spell_dh_annihilation);
    new spell_dh_chaos_strike_specless();
    new spell_dh_fel_rush_specless();
    RegisterPlayerScript(DH_DisableDoubleJump_OnMount);
    new DemonHunterAllowSpec();
    RegisterAreaTriggerAI(at_dh_soul_fragment_havoc);
    RegisterSpellScript(spell_dh_void_metamorphosis_counter);
    RegisterSpellScript(spell_dh_void_metamorphosis_buff);
    RegisterSpellScript(spell_dh_devour);
    RegisterSpellScript(spell_dh_eradicate_damage);
    RegisterSpellScript(spell_dh_hungering_slash);
    RegisterSpellScript(spell_dh_the_hunt_damage);
    RegisterSpellScript(spell_dh_emptiness_buff);
    RegisterSpellScript(spell_dh_soul_immolation);
    RegisterSpellScript(spell_dh_spontaneous_immolation);
    RegisterSpellScript(spell_dh_voidfall_passive);
    RegisterSpellScript(spell_dh_doomsayer_passive);
    RegisterSpellScript(spell_dh_doomsayer_buff);
    RegisterSpellScript(spell_dh_voidfall_meteor_damage);
    RegisterSpellScript(spell_first_in_last_out);
}
