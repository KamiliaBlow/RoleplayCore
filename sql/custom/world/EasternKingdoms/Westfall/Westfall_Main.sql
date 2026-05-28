-- Westfall Main changes

-- NPC: 95 Defias Smuggler
-- NPC: 98 Riverpaw Taskmaster
-- NPC: 114 Harvest Watchers
-- NPC: 115 Harvest Reaper
-- NPC: 117 Riverpaw Gnoll
-- NPC: 121 Defias Pathstalker
-- NPC: 122 Defias Highwayman
-- NPC: 123 Riverpaw Mongrel
-- NPC: 124 Riverpaw Brute
-- NPC: 126 Murloc Coastrunner
-- NPC: 127 Murloc Tidehunter
-- NPC: 154 Greater Fleshripper
-- NPC: 157 Goretusk
-- NPC: 171 Murloc Warrior
-- NPC: 199 Young Fleshripper
-- NPC: 233 Farmer Saldean
-- NPC: 235 Salma Saldean
-- NPC: 237 Farmer Furlbrow
-- NPC: 238 Verna Furlbrow
-- NPC: 391 Murloc Old Murk
-- NPC: 392 Captain Grayson
-- NPC: 449 Defias Knuckleduster
-- NPC: 452 Riverpaw Bandit
-- NPC: 453 Riverpaw Mystic
-- NPC: 454 Young Goretusk
-- NPC: 456 Murloc Minor Oracle
-- NPC: 458 Murloc Hunter
-- NPC: 462 Vultros
-- NPC: 467 Defias Traitor
-- NPC: 480 Rusty Harvest Golem
-- NPC: 481 Defias Footpad
-- NPC: 500 Riverpaw Scout
-- NPC: 501 Riverpaw Herbalist
-- NPC: 504 Defias Trapper
-- NPC: 513 Murloc Netter
-- NPC: 515 Murloc Raider
-- NPC: 517 Murloc Oracle
-- NPC: 519 Murloc Slark
-- NPC: 520 Murloc Brack
-- NPC: 550 Defias Messenger
-- NPC: 582 Old Blanchy
-- NPC: 589 Defias Pillager
-- NPC: 590 Defias Looter
-- NPC: 594 Defias Hencheman
-- NPC: 830 Sand Crawler
-- NPC: 831 Sea Crawler
-- NPC: 832 Unbound Cyclone
-- NPC: 834 Coyote
-- NPC: 1065 Riverpaw Shaman
-- NPC: 1109 Fleshripper
-- NPC: 1236 Kobold Digger
-- NPC: 1424 Master Digger
-- NPC: 6250 Crawler
-- NPC: 7024 Agent Kearnen
-- NPC: 17234 Trigger for Mortwake tower
-- NPC: 42308 Lieutenant Horatio
-- NPC: 42342 Energized Harvest Reaper
-- NPC: 42357 Hulking Goretusk
-- NPC: 42383 Transient
-- NPC: 42384 Homeless Stormwind Citizen
-- NPC: 42386 Homeless Stormwind Citizen (Female)
-- NPC: 42387 Thug
-- NPC: 42390 Small-Time Hustler
-- NPC: 42391 West Plains Drifter
-- NPC: 42405 Two-Shoed Lou
-- NPC: 42562 Thug Trigger
-- NPC: 42653 Riverpaw Jango
-- NPC: 42656 Mercenary
-- NPC: 42662 Shadowy Figure
-- NPC: 42669 Chasm Slime
-- NPC: 42677 Moonbrook Thug
-- NPC: 900000 copy of Farmer Furlbrow for phasing
-- NPC: 900001 copy of Verna Furlbrow
-- NPC: 900002 copy of Old Blanchy
-- NPC: 636 Edwin's Guards
-- NPC: 639 Edwin VC
-- NPC: 42371 Vanessa VC
-- NPC: 646 Mr Smite (Vision)
-- NPC: 657 Defias Pirate (Vision of the past)
-- NPC: 1732 Defias Squallshaper (Vision)
-- NPC: 3450 Defias Companion (Vision)
-- NPC: 3947 Goblin Ship Builder (Vision)


-- Quest: 9 The killing fields
-- Quest: 22 Goretusk Liver Pie
-- Quest: 36 Westfall Stew
-- Quest: 64 The Forgotten Heirloom
-- Quest: 103 Keeper of the flame
-- Quest: 109 Report to Gryan Stoutmantle
-- Quest: 151 Poor Old Blanchy
-- Quest: 184 Furlbrows Deed
-- Quest: 26209 Murder Was The Case That They Gave Me
-- Quest: 26213 Hot On the Trail: The Riverpaw Clan
-- Quest: 26214 Hot On the Trail: Murlocs
-- Quest: 26215 Meet Two-Shoed Lou
-- Quest: 26232 Lou's Parting Thoughts
-- Quest: 26290 Secrets of the tower

-- Spell: 76633 Apply Quest Invis Zone 1 (Lou in Westfall)
-- Spell: 79229 Detect Quest Invis 1 (same effect as the above)
-- Spell: 79489 Detect Quest Invis 0 (for Laine and murder npcs)
-- Spell: 79341 Detect Quest Invis 2 (for Lou murder)

-- Spell: 5172 Bloodsail Companion
-- Spell: 24623 Summon Edwin VC
-- Spell: 5200 Summon VC Allies

-- Phase: 231 Vision of the past
-- Phase: 232 Rise of the Brotherhood

-- AT: 5989 Furlbrow's Pumpkin Farm (Lou)
-- AT: 5993 Sentinel Hill Tower
-- AT: 5994 Mortwake Tower (top)
-- AT: 5998 Moonbrook Center
-- AT: 6080 Moonbrook Defias Hideout Entrance
-- AT: 8481 Westfall - Old MacDonald

-- Phases
DELETE FROM `phase_area` WHERE `PhaseId` IN (50006, 226,232);
INSERT INTO `phase_area` VALUES
(108, 226, 'Westfall - Act 2'),
(108, 232, 'Westfall - Rise of the Brotherhood'),
(40, 50006, 'Westfall - Horatio & Investigators at Silent Hill'); -- until i can find the proper invisibility spells

-- Phase Conditions
DELETE FROM `conditions` WHERE `SourceTypeOrReferenceId` = 26 AND `SourceGroup` IN (50006, 226, 232);
INSERT INTO `conditions` VALUES
-- Phase 50006 for Sentinel Hill Tower murder dialogue
('26', '50006', 0, '0', '0', '47', '0', '26266', '74', '0', '', '0', '0', '0', '', 'Westfall - Add phase 50006 if 26266 IS taken, complete, rewarded'),
('26', '50006', 0, '0', '0', '47', '0', '26322', '74', '0', '', '1', '0', '0', '', 'Westfall - Add phase 50006 if 26322 IS NOT in progress'),
-- Phase 232 Rise of the Brotherhood
('26', '232', 0, '0', '0', '47', '0', '26322', '8', '0', '', '0', '0', '0', '', 'Westfall - Add phase 238 if 26322 IS in progress'),
('26', '232', 0, '0', '0', '47', '0', '26322', '64', '0', '', '1', '0', '0', '', 'Westfall - Add phase 238 if 26322 IS NOT rewarded'),
-- Phase 226 Westfall Act 2
('26', '226', 0, '0', '0', '47', '0', '26322', '64', '0', '', '0', '0', '0', '', 'Westfall - Add phase 226 if 26322 IS rewarded'),
('26', '226', 0, '0', '0', '47', '0', '26761', '64', '0', '', '1', '0', '0', '', 'Westfall - Add phase 226 if 26761 IS NOT rewarded');

-- AT scripts
DELETE FROM `areatrigger_scripts` WHERE `entry` IN (5989,5993,5994,5998,6080,8481);
INSERT INTO `areatrigger_scripts` (`entry`, `ScriptName`) VALUES 
(5989, 'at_westfall_furlsbrow_farm_5989'),
(5993, 'at_westfall_sentinel_hill_tower_5993'),
(5994, 'at_westfall_mortwake_tower_5994'),
(5998, 'at_westfall_moonbrook_center_5998'),
(6080, 'at_westfall_moonbrook_defias_entrance_6080'),
(8481, 'at_westfall_old_macdonald_8481');

-- SpellArea
DELETE FROM `spell_area` WHERE (`spell`=79346 AND `area`=40) OR (`spell`=79229 AND `area`=40);
INSERT INTO `spell_area` (`spell`, `area`, `quest_start`, `quest_end`, `aura_spell`, `racemask`, `gender`, `flags`, `quest_start_status`, `quest_end_status`) VALUES
(79346, 40, 26232, 26236, 0, 0, 2, 3, 66, 11),
(79229, 40, 0, 26232, 0, 0, 2, 3, 64, 9);

-- Game Events
DELETE FROM `game_event` WHERE `eventEntry` = 25;
INSERT INTO `game_event` (`eventEntry`, `start_time`, `end_time`, `occurence`, `length`, `holiday`, `holidayStage`, `WorldStateId`, `description`, `world_event`, `announce`) VALUES
(25, '2026-10-28 20:00:00', '2036-12-31 06:00:00', 1440, 720, 0, 0, NULL, 'Nights', 0, 2);