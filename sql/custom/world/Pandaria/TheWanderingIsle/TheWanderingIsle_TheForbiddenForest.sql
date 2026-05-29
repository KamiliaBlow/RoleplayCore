-- NPC: 60858 Deepscale Fleshripper (phase 544)
-- NPC: 60780 Deepscale Ravager (phase 544)
-- NPC: 56360 Deepscale Tormentor (phase 1835)
-- NPC: 60722 Deepscale Tormentor
-- NPC: 56007 Darkened Horror (phase 1835)
-- NPC: 56008 Darkened Terror (phase 1835)
-- NPC: 60685 Deepscale Aggressor
-- NPC: 56009 Vordraka
-- NPC: 53705 Pangolin
-- NPC: 55946 Pei-wu tiger
-- NPC: 56172 Ash moth
-- NPC: 56174 Tiger cub

-- NPC: 55940 Jojo at the Alliance camp
-- NPC: 55942 Ji at the Hut
-- NPC: 55999 Injured Sailor (phase 1835)
-- NPC: 56362 Skyseeker Sailor (phase 1835)
-- NPC: 56476 Injured Sailor Rescue controller (phase 1835)
-- NPC: 56236 Injured Sailor (spawn)
-- NPC: 60897 Injured Sailor
-- NPC: 60900 Ji summoned at the Hut
-- NPC: 56416 Aysa fighting Vordraka
-- NPC: 55943 Wei Palerage
-- NPC: 55944 Delora Lionheart
-- NPC: 60042 Korga
-- NPC: 60055 Makael Bay
-- NPC: 67186 Provisioner Drog
-- NPC: 67185 Provisioner Phelps
-- NPC: 60895 Alliance Priest
-- NPC: 60729 Aysa at the explosion
-- NPC: 60741 Ji at the explosion
-- NPC: 56417 Aysa after explosion?
-- NPC: 56418 Ji after Vordraka
-- NPC: 57739 Ji After healed Shenzinsu
-- NPC: 56013 Spirit of Master Shang
-- NPC: 60877 Alliance Priest
-- NPC: 60878 Alliance Priest
-- NPC: 60770 Horde Druid
-- NPC: 60834 Horde Druid
-- NPC: 60851 Delora Lionheart (during healing)
-- NPC: 60852 Korga (during healing)
-- NPC: 57317 Skyseeker Sailor (during healing)
-- NPC: 60854 Escaped Horde Crewman (during healing)
-- NPC: 60853 Makael Bay (during healing)
-- NPC: 60873 Skyseeker Sailor (during healing)
-- NPC: 56419 Jojo (during healing)
-- NPC: 60889 Delora (after healing)
-- NPC: 60888 Korga (after healing)
-- NPC: 56195 Escaped Horde engineer
-- NPC: 56000 Escaped Horde Crewman

-- Spells: 117600 Summon Ji Cinematic Intro
-- Spells: 117499 Summon Aysa Cinematic Intro

-- Quest: 29665 From bad to worse
-- Quest: 29793 Evil from the seas
-- Quest: 29794 None left behind
-- Quest: 29795 Stocking Stalks
-- Quest: 29796 Urgent News
-- Quest: 29797 Medical Supplies
-- Quest: 29798 An ancient evil
-- Quest: 29799 The Healing of Shen-zin Su
-- Quest: 29800 New Allies
-- Quest: 30589 Wrecking the Wreck
-- Quest: 30590 Handle with care
-- Quest: 30591 Praying on the predators
-- Quest: 30767 Risking it all


DELETE FROM `creature_queststarter` WHERE `quest` IN (30591, 29795, 30589, 29793, 29796, 30590, 29794, 29797, 29665, 29798, 30767, 29799, 29800);
INSERT INTO `creature_queststarter` (`id`, `quest`, `VerifiedBuild`) VALUES
('57739', '29800', '0'), 
('56418', '29799', '0'), 
('56416', '30767', '0'),
('55940', '29665', '0'),
('55940', '29798', '0'),
('55942', '29793', '0'),
('55942', '29796', '0'),
('55943', '30591', '0'),
('55944', '29794', '0'),
('55944', '29797', '0'),
('60055', '30590', '0'),
('60042', '29795', '0'),
('60042', '30589', '0');

DELETE FROM `creature_questender` WHERE `quest` IN (30591, 29795, 30589, 29793, 29796, 30590, 29794, 29797, 29665, 29798, 30767, 29799, 29800);
INSERT INTO `creature_questender` (`id`, `quest`, `VerifiedBuild`) VALUES 
('56013', '29800', '0'), 
('56418', '29799', '0'),
('56418', '30767', '0'),
('56416', '29798', '0'),
('55940', '29665', '0'),
('55942', '29793', '0'),
('55944', '29796', '0'),
('60055', '30590', '0'),
('55943', '30591', '0'),
('55944', '29794', '0'),
('55944', '29797', '0'),
('60042', '29795', '0'),
('60055', '30589', '0');

DELETE FROM `quest_template_addon` WHERE `ID` IN (30591, 29795, 30589, 29793, 29796, 30590, 29794, 29797, 29665, 29798, 30767, 29799, 29800);
INSERT INTO `quest_template_addon` (`ID`, `MaxLevel`, `AllowableClasses`, `SourceSpellID`, `PrevQuestID`, `NextQuestID`, `ExclusiveGroup`, `BreadcrumbForQuestId`, `RewardMailTemplateID`, `RewardMailDelay`, `RequiredSkillID`, `RequiredSkillPoints`, `RequiredMinRepFaction`, `RequiredMaxRepFaction`, `RequiredMinRepValue`, `RequiredMaxRepValue`, `ProvidedItemCount`, `SpecialFlags`, `ScriptName`) VALUES 
('30591', '0', '0', '0', '29792', '30589', '-30591', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', ''),
('29795', '0', '0', '0', '29792', '30589', '-30591', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', ''),
('30589', '0', '0', '117973', '30591', '30590', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', ''),
('29793', '0', '0', '0', '30589', '29796', '-29793', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', ''),
('29796', '0', '0', '89521', '29793', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', ''),
('29794', '0', '0', '0', '29796', '29798', '-29665', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', ''),
('29797', '0', '0', '0', '29796', '29798', '-29665', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', ''),
('30590', '0', '0', '0', '30589', '29796', '-29793', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', ''),
('29665', '0', '0', '0', '29796', '29798', '-29665', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', ''),
('29798', '0', '0', '0', '29665', '30767', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', ''),
('30767', '0', '0', '0', '29798', '29799', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', ''),
('29799', '0', '0', '0', '30767', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', ''),
('29800', '0', '0', '0', '29799', '31450', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '');

DELETE FROM `world_state` WHERE `ID` IN (6488, 6489);
INSERT INTO `world_state` (`ID`, `DefaultValue`, `MapIDs`, `AreaIDs`, `Comment`) VALUES 
('6488', '0', '860', '5833', 'The Wandering Isle - Healing Shen-zin Su healers active'),
('6489', '1', '860', '5833', 'The Wandering Isle - Healing Shen-zin Su healers enabled');

-- Creature Templates
UPDATE `creature_template` SET `ScriptName` = 'npc_injured_sailor_55999', `RegenHealth` = '0' WHERE `entry` = 55999;
UPDATE `creature_template` SET `ScriptName` = 'npc_aysa_vordraka_fight' WHERE `entry` = 56416;
UPDATE `creature_template` SET `ScriptName` = 'npc_vordraka' WHERE `entry` = 56009;
UPDATE `creature_template` SET `ScriptName` = 'npc_aysa_explosion' WHERE `entry` = 60729;
UPDATE `creature_template` SET `npcflag` = 16777216, `VehicleId` = 2171 WHERE `entry` = 60848;

DELETE FROM `vehicle_template` WHERE `creatureId` IN (60848);
INSERT INTO `vehicle_template` (`creatureId`, `despawnDelayMs`) VALUES 
(60848, 0);


UPDATE `creature` SET `PhaseId` = 543 WHERE `id` IN (56009); -- Vordraka
UPDATE `creature` SET `PhaseId` = 1835 WHERE `id` IN (56360, 55999, 56362, 56476);
UPDATE `creature` SET `PhaseId` = 544, `terrainSwapMap` = 975 WHERE id IN (60780, 60858, 57317, 56417, 56418, 56419, 60851, 60853, 60854, 60877, 60878, 60873, 60770, 60834, 60852, 60874);
UPDATE `creature` SET `PhaseId` = 545, `terrainSwapMap` = 976 WHERE id IN (60895, 60888, 60896, 60898, 60894, 60893, 60892, 60891, 60890, 60889, 57743, 57741, 57739);
UPDATE `creature` SET `PhaseId` = 545, `terrainSwapMap` = 976 WHERE guid IN (452340);
UPDATE `creature` SET `PhaseId` = 0, `PhaseGroup` = 641, `terrainSwapMap` = -1 WHERE `guid` IN (451723,451725,451731,451737,451746); -- Wreck camp NPCs
UPDATE `creature` SET `PhaseId` = 544, `ScriptName` = 'npc_healers_active_bunny' WHERE (`guid` = '452176');
UPDATE `creature` SET `PhaseId` = 543, `terrainSwapMap` = -1 WHERE `guid` IN (452131, 452134, 452133, 452127,452130, 452136, 452135, 452129, 452124); -- Deepscale Tormentors
UPDATE `creature` SET `PhaseId` = 169, `terrainSwapMap` = -1 WHERE `guid` IN (451858, 451860, 451965, 451979, 451972,451978,451975,451969,451851,451856,451852,451861,451868);
UPDATE `creature` SET `PhaseId` = 0, `PhaseGroup` = 680 WHERE `guid` IN (451733,451734,452119,452121,452123,452126);
UPDATE `creature` SET `PhaseId` = 903 WHERE `guid` = 451895; -- Ji 55942
UPDATE `creature` SET `PhaseId` = 1835 WHERE `guid` = 451988; -- Ji 55942

DELETE FROM `creature` WHERE `map` = 860 AND `guid` IN (452061, 452110, 452062, 452099, 452114, 452105, 452117, 452351, 452355, 452101, 452111, 452112, 452248, 452229, 452265, 452250,
452252,452254, 452196, 452210, 452268, 452283, 452273, 452253, 452224, 452195, 452266, 452267, 452219, 452237, 452263, 452277, 452211, 452225, 452244, 452262, 452279, 452264); -- duplicate spawns
DELETE FROM `creature` WHERE `map` = 860 AND `id` IN (56236); -- wrong spawns as the sailors are spawned from quest

-- Duplicate Deepscale Ravager Spawns
DELETE FROM `creature` WHERE `map` = 860 AND `id` IN (60780) AND `guid` NOT IN (452194,452235,452246,452141,452234,452199,452223,452200,452201,452189,452202,452276,452226); 
-- Duplicate Deepscale Fleshripper Spawns
DELETE FROM `creature` WHERE `map` = 860 AND `id` IN (60858) AND `guid` NOT IN (452218,452241,452215,452243,452238,452193);
-- Duplicate Tormentor Spawns
DELETE FROM `creature` WHERE `map` = 860 AND `guid` IN (452059,452115,451758,452011);
-- Duplicate Darkened Horror/Terror
DELETE FROM `creature` WHERE `map` = 860 AND `guid` IN (451966,451967,451959,451952);
-- Duplicate Horde Druid
DELETE FROM `creature` WHERE `map` = 860 AND `guid` IN (452222,452172,452216,452239,452249,452258,452269,452281,452217,452247,452228,452259);

UPDATE `gameobject` SET `PhaseId` = 1835 WHERE `id` IN (209793);
UPDATE `gameobject` SET `PhaseId` = 993 WHERE `guid` IN (300834);

DELETE FROM `npc_vendor` WHERE `entry` IN (67185, 67186);
INSERT INTO `npc_vendor` (entry, slot, item, maxcount, incrtime, ExtendedCost, `type`, BonusListIDs, PlayerConditionID, IgnoreFiltering, VerifiedBuild) VALUES
('67186', '1', '117', '0', '0', '0', '1', NULL, '0', '0', '64154'),
('67186', '7', '159', '0', '0', '0', '1', NULL, '0', '0', '64154'),
('67186', '4', '787', '0', '0', '0', '1', NULL, '0', '0', '64154'),
('67186', '2', '2287', '0', '0', '0', '1', NULL, '0', '0', '64154'),
('67186', '3', '3770', '0', '0', '0', '1', NULL, '0', '0', '64154'),
('67186', '5', '4592', '0', '0', '0', '1', NULL, '0', '0', '64154'),
('67186', '6', '4593', '0', '0', '0', '1', NULL, '0', '0', '64154'),
('67186', '8', '90659', '0', '0', '0', '1', NULL, '0', '0', '64154'),
('67186', '9', '90660', '0', '0', '0', '1', NULL, '0', '0', '64154'),
('67185', '1', '159', '0', '0', '0', '1', NULL, '0', '0', '63906'),
('67185', '2', '4536', '0', '0', '0', '1', NULL, '0', '0', '63906'),
('67185', '3', '4538', '0', '0', '0', '1', NULL, '0', '0', '63906'),
('67185', '4', '4540', '0', '0', '0', '1', NULL, '0', '0', '63906'),
('67185', '5', '4541', '0', '0', '0', '1', NULL, '0', '0', '63906'),
('67185', '6', '4542', '0', '0', '0', '1', NULL, '0', '0', '63906'),
('67185', '7', '90659', '0', '0', '0', '1', NULL, '0', '0', '63906'),
('67185', '8', '90660', '0', '0', '0', '1', NULL, '0', '0', '63906');

DELETE FROM `npc_spellclick_spells` WHERE `npc_entry` IN (55999, 60770, 60877, 60878, 60834,60848);
INSERT INTO `npc_spellclick_spells` (`npc_entry`, `spell_id`, `cast_flags`, `user_type`) VALUES
(55999, 129340, 1, 0),
(55999, 56685, 1, 0),
(60770, 56685, 1, 0),
(60877, 56685, 1, 0),
(60834, 56685, 1, 0),
(60878, 56685, 1, 0),
(60848, 117848, 1, 0);

DELETE FROM `scene_template` WHERE `SceneId`=102;
INSERT INTO `scene_template` (`SceneId`, `Flags`, `ScriptPackageID`, `ScriptName`) VALUES
(102, 9, 26, '');

-- Scripts
DELETE FROM `areatrigger_scripts` WHERE `entry` IN (7087,8564,7265);
INSERT INTO `areatrigger_scripts` VALUES
(8564, 'SmartTrigger'),
(7265, 'SmartTrigger'),
(7087, 'at_wreck_of_the_skyseeker_injured_sailor');

DELETE FROM `spell_script_names` WHERE `spell_id` IN (117973, 108806, 129341, 117597, 118233, 117783, 117400, 131983);
INSERT INTO `spell_script_names` (`spell_id`, `ScriptName`) VALUES 
(117400, 'spell_summon_deep_sea_aggressor'),
(117973, 'spell_summon_ji_forlorn_hut'),
(129341, 'spell_rescue_injured_sailor'),
(108806, 'aura_injured_sailor_feign_death'),
(117597, 'spell_summon_ji_wreck_explosion'),
(118233, 'spell_turtle_healed_phase_timer'),
(131983, 'spell_ally_horde_argument'),
(117783, 'spell_healing_shenzin_su');

DELETE FROM `conditions` WHERE `SourceTypeOrReferenceId`=25 AND `SourceEntry` IN (975,976);
DELETE FROM `conditions` WHERE `SourceTypeOrReferenceId`=13 AND `SourceEntry` IN (108932,108933);
DELETE FROM `conditions` WHERE `SourceTypeOrReferenceId`=18 AND `SourceGroup` IN (55999);
INSERT INTO `conditions` (`SourceTypeOrReferenceId`, `SourceGroup`, `SourceEntry`, `SourceId`, `ElseGroup`, `ConditionTypeOrReference`, `ConditionTarget`, `ConditionValue1`, `ConditionValue2`, `ConditionValue3`, `NegativeCondition`, `ErrorType`, `ErrorTextId`, `ScriptName`, `Comment`) VALUES
(13, 1, 108933, 0, 0, 51, 0, 5, 57741, 0, 0, 0, 0, '', 'Summon Ox Cart, Skyfire Crash > Temple target Delivery Cart'),
(13, 1, 108932, 0, 0, 51, 0, 5, 57743, 0, 0, 0, 0, '', 'Summon Ox, Skyfire Crash > Temple target Nourished Yak'),

-- Spell click conditions for Injured sailor - these appear to NOT work?
(18, 55999, 129340, 0, 0, 47, 0, 29794, 8, 0, 0, 0, 0, '', 'Show spellclick only when player has taken quest'),
(18, 55999, 56685, 0, 0, 47, 0, 29794, 8, 0, 0, 0, 0, '', 'Show spellclick only when player has taken quest'),
(18, 55999, 56685, 0, 0, 1, 0, 105520, 0, 0, 1, 0, 0, '', 'Show spellclick only when player has not aura'),
(18, 55999, 56685, 0, 0, 1, 0, 129340, 0, 0, 1, 0, 0, '', 'Show spellclick only when player has not aura'),

-- TerrainSwaps
(25, 0, 975, 0, 0, 47, 0, 30767, 66, 0, 0, 0, 0, '', 'TerrainSwap 975 only when player has quest 30767 complete or rewarded'),
(25, 0, 975, 0, 0, 47, 0, 29799, 64, 0, 1, 0, 0, '', 'TerrainSwap 975 only when player has quest 29799 not rewarded'),
(25, 0, 976, 0, 0, 47, 0, 29799, 64, 0, 0, 0, 0, '', 'TerrainSwap 976 only when player has quest 29799 rewarded');

UPDATE `creature_template` SET `AIName` = 'SmartAI' WHERE `entry` IN (57712);
DELETE FROM `smart_scripts` WHERE `entryorguid` IN (-452340) AND `source_type` IN (0, 9);
DELETE FROM `smart_scripts` WHERE `entryorguid` IN (8564,7265) AND `source_type` IN (2);
INSERT INTO `smart_scripts` (`entryorguid`, `source_type`, `id`, `link`, `event_type`, `event_phase_mask`, `event_chance`, 
`event_flags`, `event_param1`, `event_param2`, `event_param3`, `event_param4`, `event_param5`, `action_type`, `action_param1`, 
`action_param2`, `action_param3`, `action_param4`, `action_param5`, `action_param6`, `target_type`, `target_param1`, `target_param2`, 
`target_param3`, `target_x`, `target_y`, `target_z`, `target_o`, `comment`) VALUES
-- Trigger 8564 Delora/Korga event
(8564, 2, 0, 1, 46, 0, 100, 0, 0, 0, 0, 0, 0, 85, 131983, 2, 0, 0, 0, 0, 7, 0, 0, 0, 0, 0, 0, 0, 'On Trigger - Invoker Cast Ally/Horde Argument Trigger Aura'),
(8564, 2, 1, 0, 61, 0, 100, 0, 0, 0, 0, 0, 0, 85, 131425, 2, 0, 0, 0, 0, 7, 0, 0, 0, 0, 0, 0, 0, 'On Trigger - Invoker Cast CKS Area Trigger Dummy Timer Aura, Self'),
-- Trigger 7265 Cart Driver
(7265, 2, 0, 0, 46, 0, 100, 0, 0, 0, 0, 0, 0, 86, 88811, 2, 10, 452340, 57712, 0, 7, 0, 0, 0, 0, 0, 0, 0, 'On Trigger - Cross Cast Area Trigger Dummy Timer Aura'),
-- Delivery Cart Tender
(-452340, 0, 0, 0, 31, 0, 100, 0, 88811, 0, 0, 0, 0, 1, 1, 0, 0, 0, 0, 0, 7, 0, 0, 0, 0, 0, 0, 0, 'Delivery Cart Tender - On Spellhit Target - Talk');

UPDATE `creature_template` SET `AIName` = 'SmartAI' WHERE `entry` IN (60900, 57317, 60854, 60858, 56417, 60853, 60873, 56419, 60780, 56007, 56008,
56174, 55946, 53705, 56172, 60685);
DELETE FROM `smart_scripts` WHERE `entryorguid` IN (60900, 6090000, 57317, 60854, 60858, 56417, 60853, 60873, 56419, 60780, 57739,
56007, 56008, 56174, 55946, 53705, 56172, 60685) AND `source_type` IN (0, 9);
INSERT INTO `smart_scripts` (entryorguid, source_type, id, link, event_type, event_phase_mask, event_chance, event_flags,  event_param1, event_param2, 
event_param3, event_param4, event_param5, event_param_string, action_type, action_param1, action_param2, action_param3, action_param4, action_param5, 
action_param6, target_type, target_param1, target_param2, target_param3, target_x, target_y, target_z, target_o, comment) VALUES
(60685, 0, 0, 1, 54, 0, 100, 0, 0, 0, 0, 0, 0, '', 117, 1, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 'Deepscale Agressor - Just Spawned - Disable Evade'),
(60685, 0, 1, 0, 61, 0, 100, 0, 0, 0, 0, 0, 0, '', 11, 117407, 0, 0, 0, 0, 0, 7, 0, 0, 0, 0, 0, 0, 0, 'Deepscale Agressor - Just Spawned - Cast Aggresive Leap'),
(60685, 0, 2, 0, 31, 0, 100, 0, 117407, 0, 0, 0, 0, '', 49, 0, 0, 0, 0, 0, 0, 7, 0, 0, 0, 0, 0, 0, 0, 'Deepscale Agressor - On Spellhit Target - Start Attack'),
(56172, 0, 0, 0, 0, 0, 100, 0, 5000, 8000, 8000, 12000, 0, '', 11, 128429, 0, 0, 0, 0, 0, 2, 0, 0, 0, 0, 0, 0, 0, 'Ash Moth - Update IC - Cast Horn'),
(53705, 0, 0, 0, 0, 0, 100, 0, 5000, 8000, 8000, 12000, 0, '', 11, 128406, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 'Pangolin - Update IC - Cast Hardened Shell'),
(55946, 0, 0, 0, 0, 0, 100, 0, 5000, 8000, 8000, 12000, 0, '', 11, 128272, 0, 0, 0, 0, 0, 2, 0, 0, 0, 0, 0, 0, 0, 'Pei-Wu Tiger - Update IC - Cast Tiger Pounce'),
(56174, 0, 0, 0, 0, 0, 100, 0, 5000, 8000, 8000, 12000, 0, '', 11, 128277, 0, 0, 0, 0, 0, 2, 0, 0, 0, 0, 0, 0, 0, 'Pei-Wu Tiger Cub - Update IC - Cast Cub Pounce'),
(56007, 0, 0, 0, 0, 0, 100, 0, 3000, 5000, 8000, 12000, 0, '', 11, 128417, 0, 0, 0, 0, 0, 2, 0, 0, 0, 0, 0, 0, 0, 'Darkened Horror - Update IC - Cast Shadow Geyser'),
(56008, 0, 0, 0, 0, 0, 100, 0, 3000, 5000, 8000, 12000, 0, '', 11, 128424, 0, 0, 0, 0, 0, 2, 0, 0, 0, 0, 0, 0, 0, 'Darkened Terror - Update IC - Cast Backhanded Swipes'),
('57739', '0', '0', '1', '20', '0', '100', '0', '29799', '0', '0', '0', '0', '', '44', '545', '1', '0', '0', '0', '0', '7', '0', '0', '0', '0', '0', '0', '0', 'Ji Firepaw - On Quest Complete - Add Phase'),
('57739', '0', '1', '2', '61', '0', '100', '0', '0', '0', '0', '0', '0', '', '44', '544', '0', '0', '0', '0', '0', '7', '0', '0', '0', '0', '0', '0', '0', 'Ji Firepaw - On Quest Complete - Remove Phase'),
('60900', '0', '0', '0', '54', '0', '100', '0', '0', '0', '0', '0', '0', '', '80', '6090000', '2', '0', '0', '0', '0', '1', '0', '0', '0', '0', '0', '0', '0', 'Ji Firepaw - Just Spawned - Run Script'),
('60900', '0', '1', '2', '40', '0', '100', '0', '2', '6090000', '0', '0', '0', '', '54', '3000', '0', '0', '0', '0', '0', '1', '0', '0', '0', '0', '0', '0', '0', 'Ji Firepaw - On WP Reached - Pause WP'),
('60900', '0', '2', '0', '61', '0', '100', '0', '0', '0', '0', '0', '0', '', '97', '15', '15', '0', '0', '0', '0', '8', '0', '0', '0', '424.019', '3676.25', '78.6968', '0', 'Ji Firepaw - On WP Reached - Jump to Pos'),
('60900', '0', '3', '4', '58', '0', '100', '0', '22', '6090000', '0', '0', '0', '', '66', '0', '0', '0', '0', '0', '0', '8', '0', '0', '0', '0', '0', '0', '0.401426', 'Ji Firepaw - On WP Ended - Set Orientation'),
('60900', '0', '4', '5', '61', '0', '100', '0', '0', '0', '0', '0', '0', '', '28', '117973', '0', '0', '0', '0', '0', '23', '0', '0', '0', '0', '0', '0', '0', 'Ji Firepaw - On WP Ended - Remove Aura'),
('60900', '0', '5', '6', '61', '0', '100', '0', '0', '0', '0', '0', '0', '', '44', '1835', '1', '0', '0', '0', '0', '23', '0', '0', '0', '0', '0', '0', '0', 'Ji Firepaw - On WP Ended - Add Phase'),
('60900', '0', '6', '0', '61', '0', '100', '0', '0', '0', '0', '0', '0', '', '41', '0', '0', '0', '0', '0', '0', '1', '0', '0', '0', '0', '0', '0', '0', 'Ji Firepaw - On WP Ended - Despawn'),
('6090000', '9', '0', '0', '0', '0', '100', '0', '1000', '1000', '1000', '1000', '0', '', '1', '0', '0', '0', '0', '0', '0', '23', '0', '0', '0', '0', '0', '0', '0', 'Ji Firepaw - Talk'),
('6090000', '9', '1', '0', '0', '0', '100', '0', '5000', '5000', '1000', '1000', '0', '', '53', '1', '6090000', '0', '0', '0', '0', '1', '0', '0', '0', '0', '0', '0', '0', 'Ji Firepaw - Start WP'),
('57317', '0', '0', '0', '0', '0', '100', '0', '3000', '5000', '3000', '5000', '0', '', '11', '128440', '0', '0', '0', '0', '0', '2', '0', '0', '0', '0', '0', '0', '0', 'Skyseeker Sailor - Update IC - Cast Piercing Strikes'),
('60854', '0', '0', '0', '0', '0', '100', '0', '5000', '8000', '5000', '8000', '0', '', '11', '128510', '0', '0', '0', '0', '0', '2', '0', '0', '0', '0', '0', '0', '0', 'Escaped Horde Crewman - Update IC - Cast Double Thrust'),
('60854', '0', '1', '0', '1', '0', '100', '0', '2000', '2000', '2000', '2000', '0', '', '49', '0', '0', '0', '0', '0', '0', '19', '0', '10', '0', '0', '0', '0', '0', 'Escaped Horde Crewman - Update OOC - Start Attack'),
('60858', '0', '0', '0', '0', '0', '100', '0', '5000', '8000', '5000', '8000', '0', '', '11', '128533', '0', '0', '0', '0', '0', '2', '0', '0', '0', '0', '0', '0', '0', 'Deepscale Fleshripper - Update IC - Cast Rip Flesh'),
('60858', '0', '1', '0', '1', '0', '100', '0', '2000', '2000', '2000', '2000', '0', '', '49', '0', '0', '0', '0', '0', '0', '19', '0', '10', '0', '0', '0', '0', '0', 'Deepscale Fleshripper - Update OOC - Start Attack'),
('60853', '0', '0', '0', '11', '0', '100', '0', '0', '0', '0', '0', '0', '', '21', '0', '0', '0', '0', '0', '0', '1', '0', '0', '0', '0', '0', '0', '0', 'Makael Bay - On Respawn - Disable Combat Movement'),
('60853', '0', '1', '0', '0', '0', '100', '0', '1000', '1200', '1000', '1200', '0', '', '11', '107115', '0', '0', '0', '0', '0', '2', '0', '0', '0', '0', '0', '0', '0', 'Makael Bay - Update IC - Cast Throw Dynamite'),
('60853', '0', '2', '0', '9', '0', '100', '0', '0', '5', '8000', '12000', '0', '', '11', '128526', '1', '0', '0', '0', '0', '2', '0', '0', '0', '0', '0', '0', '0', 'Makael Bay - On Target In Range - Cast Saw Blade'),
('60873', '0', '0', '0', '1', '0', '100', '0', '2000', '2000', '2000', '2000', '0', '', '49', '0', '0', '0', '0', '0', '0', '19', '0', '10', '0', '0', '0', '0', '0', 'Skyseeker Sailor - Update OOC - Start Attack'),
('60780', '0', '0', '0', '1', '0', '100', '0', '2000', '2000', '2000', '2000', '0', '', '49', '0', '0', '0', '0', '0', '0', '19', '0', '10', '0', '0', '0', '0', '0', 'Deepscale Ravager - Update OOC - Start Attack');

UPDATE `creature_template` SET `ScriptName` = '', `AIName` = 'SmartAI' WHERE `Entry` IN (56007,56008,56360,56419,60851,60852,56417,56418,60848,60834,60878,60770,60877,55940,60858,60780);
DELETE FROM `smart_scripts` WHERE `entryorguid` IN (56007,56008,56360,-452012,-452006,-452103,-452104,-452054,56419,60851,60852,56417,56418,5641800,60848,6083400,
-452192,-452178,6087800,-452157,-452204,6077000,6087700,-452203,-452179,-452163,-452186,-452160,-452232,-452155,-452139,55940,6078000,-452194,-452235,-452246,-452141,
-452234,-452199,-452223,-452200,-452201,-452189,-452202,-452276,-452226,-452218,-452241,-452215,-452243,-452238,-452193) AND `source_type` IN (0,9);
INSERT INTO `smart_scripts` (`entryorguid`, `source_type`, `id`, `link`, `event_type`, `event_phase_mask`, `event_chance`, `event_flags`, `event_param1`, `event_param2`, `event_param3`, `event_param4`, `event_param5`, `action_type`, `action_param1`, `action_param2`, `action_param3`, `action_param4`, `action_param5`, `action_param6`, `target_type`, `target_param1`, `target_param2`, `target_param3`, `target_x`, `target_y`, `target_z`, `target_o`, `comment`) VALUES
-- Horde Druid
(-452203, 0, 0, 1, 11, 0, 100, 0, 0, 0, 0, 0, 0, 103, 1, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 'Horde Druid - On Respawn - Set Root'),
(-452203, 0, 1, 0, 61, 0, 100, 0, 0, 0, 0, 0, 0, 22, 1, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 'Horde Druid - On Respawn - Set Event Phase 1'),
(-452203, 0, 2, 0, 25, 1, 100, 0, 0, 0, 0, 0, 0, 81, 16777216, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 'Horde Druid - On Reset - Set Npcflag'),
(-452203, 0, 3, 0, 4, 1, 100, 0, 0, 0, 0, 0, 0, 83, 16777216, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 'Horde Druid - On Aggro - Remove Npcflag'),
(-452203, 0, 4, 0, 8, 1, 100, 0, 56685, 0, 0, 0, 0, 80, 6077000, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 'Horde Druid - On Spellhit - Run Script'),
(-452203, 0, 5, 0, 52, 2, 100, 0, 0, 60770, 0, 0, 0, 53, 1, 6077000, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 'Horde Druid - On Text over - Start WP'),
(-452203, 0, 6, 7, 58, 2, 100, 0, 8, 6077000, 0, 0, 0, 11, 117932, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 'Horde Druid - On WP Ended - Cast Healing Shen-zin Su''s Wound'),
(-452203, 0, 7, 8, 61, 2, 100, 0, 0, 0, 0, 0, 0, 45, 1, 1, 0, 0, 0, 0, 10, 452176, 40789, 0, 0, 0, 0, 0, 'Horde Druid - On WP Ended - Set Data'),
(-452203, 0, 8, 0, 61, 2, 100, 0, 0, 0, 0, 0, 0, 22, 3, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 'Horde Druid - On WP Ended - Set Event Phase 3'),
(-452203, 0, 9, 0, 6, 4, 100, 0, 0, 0, 0, 0, 0, 45, 2, 1, 0, 0, 0, 0, 10, 452176, 40789, 0, 0, 0, 0, 0, 'Horde Druid - On Death - Set Data'),
(-452203, 0, 10, 0, 1, 4, 100, 0, 1000, 1000, 5000, 5000, 0, 11, 117932, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 'Horde Druid - OOC - Cast Healing Shen-zin Su''s Wound'),
(-452179, 0, 0, 1, 11, 0, 100, 0, 0, 0, 0, 0, 0, 103, 1, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 'Horde Druid - On Respawn - Set Root'),
(-452179, 0, 1, 0, 61, 0, 100, 0, 0, 0, 0, 0, 0, 22, 1, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 'Horde Druid - On Respawn - Set Event Phase 1'),
(-452179, 0, 2, 0, 25, 1, 100, 0, 0, 0, 0, 0, 0, 81, 16777216, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 'Horde Druid - On Reset - Set Npcflag'),
(-452179, 0, 3, 0, 4, 1, 100, 0, 0, 0, 0, 0, 0, 83, 16777216, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 'Horde Druid - On Aggro - Remove Npcflag'),
(-452179, 0, 4, 0, 8, 1, 100, 0, 56685, 0, 0, 0, 0, 80, 6077000, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 'Horde Druid - On Spellhit - Run Script'),
(-452179, 0, 5, 0, 52, 2, 100, 0, 0, 60770, 0, 0, 0, 53, 1, 6077001, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 'Horde Druid - On Text over - Start WP'),
(-452179, 0, 6, 7, 58, 2, 100, 0, 6, 6077001, 0, 0, 0, 11, 117932, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 'Horde Druid - On WP Ended - Cast Healing Shen-zin Su''s Wound'),
(-452179, 0, 7, 8, 61, 2, 100, 0, 0, 0, 0, 0, 0, 45, 1, 1, 0, 0, 0, 0, 10, 452176, 40789, 0, 0, 0, 0, 0, 'Horde Druid - On WP Ended - Set Data'),
(-452179, 0, 8, 0, 61, 2, 100, 0, 0, 0, 0, 0, 0, 22, 3, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 'Horde Druid - On WP Ended - Set Event Phase 3'),
(-452179, 0, 9, 0, 6, 4, 100, 0, 0, 0, 0, 0, 0, 45, 2, 1, 0, 0, 0, 0, 10, 452176, 40789, 0, 0, 0, 0, 0, 'Horde Druid - On Death - Set Data'),
(-452179, 0, 10, 0, 1, 4, 100, 0, 1000, 1000, 5000, 5000, 0, 11, 117932, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 'Horde Druid - OOC - Cast Healing Shen-zin Su''s Wound'),
(-452163, 0, 0, 1, 11, 0, 100, 0, 0, 0, 0, 0, 0, 103, 1, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 'Horde Druid - On Respawn - Set Root'),
(-452163, 0, 1, 0, 61, 0, 100, 0, 0, 0, 0, 0, 0, 22, 1, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 'Horde Druid - On Respawn - Set Event Phase 1'),
(-452163, 0, 2, 0, 25, 1, 100, 0, 0, 0, 0, 0, 0, 81, 16777216, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 'Horde Druid - On Reset - Set Npcflag'),
(-452163, 0, 3, 0, 4, 1, 100, 0, 0, 0, 0, 0, 0, 83, 16777216, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 'Horde Druid - On Aggro - Remove Npcflag'),
(-452163, 0, 4, 0, 8, 1, 100, 0, 56685, 0, 0, 0, 0, 80, 6077000, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 'Horde Druid - On Spellhit - Run Script'),
(-452163, 0, 5, 0, 52, 2, 100, 0, 0, 60770, 0, 0, 0, 53, 1, 6077002, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 'Horde Druid - On Text over - Start WP'),
(-452163, 0, 6, 7, 58, 2, 100, 0, 5, 6077002, 0, 0, 0, 11, 117932, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 'Horde Druid - On WP Ended - Cast Healing Shen-zin Su''s Wound'),
(-452163, 0, 7, 8, 61, 2, 100, 0, 0, 0, 0, 0, 0, 45, 1, 1, 0, 0, 0, 0, 10, 452176, 40789, 0, 0, 0, 0, 0, 'Horde Druid - On WP Ended - Set Data'),
(-452163, 0, 8, 0, 61, 2, 100, 0, 0, 0, 0, 0, 0, 22, 3, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 'Horde Druid - On WP Ended - Set Event Phase 3'),
(-452163, 0, 9, 0, 6, 4, 100, 0, 0, 0, 0, 0, 0, 45, 2, 1, 0, 0, 0, 0, 10, 452176, 40789, 0, 0, 0, 0, 0, 'Horde Druid - On Death - Set Data'),
(-452163, 0, 10, 0, 1, 4, 100, 0, 1000, 1000, 5000, 5000, 0, 11, 117932, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 'Horde Druid - OOC - Cast Healing Shen-zin Su''s Wound'),
(-452186, 0, 0, 1, 11, 0, 100, 0, 0, 0, 0, 0, 0, 103, 1, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 'Horde Druid - On Respawn - Set Root'),
(-452186, 0, 1, 0, 61, 0, 100, 0, 0, 0, 0, 0, 0, 22, 1, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 'Horde Druid - On Respawn - Set Event Phase 1'),
(-452186, 0, 2, 0, 25, 1, 100, 0, 0, 0, 0, 0, 0, 81, 16777216, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 'Horde Druid - On Reset - Set Npcflag'),
(-452186, 0, 3, 0, 4, 1, 100, 0, 0, 0, 0, 0, 0, 83, 16777216, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 'Horde Druid - On Aggro - Remove Npcflag'),
(-452186, 0, 4, 0, 8, 1, 100, 0, 56685, 0, 0, 0, 0, 80, 6077000, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 'Horde Druid - On Spellhit - Run Script'),
(-452186, 0, 5, 0, 52, 2, 100, 0, 0, 60770, 0, 0, 0, 53, 1, 6077003, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 'Horde Druid - On Text over - Start WP'),
(-452186, 0, 6, 7, 58, 2, 100, 0, 6, 6077003, 0, 0, 0, 11, 117932, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 'Horde Druid - On WP Ended - Cast Healing Shen-zin Su''s Wound'),
(-452186, 0, 7, 8, 61, 2, 100, 0, 0, 0, 0, 0, 0, 45, 1, 1, 0, 0, 0, 0, 10, 452176, 40789, 0, 0, 0, 0, 0, 'Horde Druid - On WP Ended - Set Data'),
(-452186, 0, 8, 0, 61, 2, 100, 0, 0, 0, 0, 0, 0, 22, 3, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 'Horde Druid - On WP Ended - Set Event Phase 3'),
(-452186, 0, 9, 0, 6, 4, 100, 0, 0, 0, 0, 0, 0, 45, 2, 1, 0, 0, 0, 0, 10, 452176, 40789, 0, 0, 0, 0, 0, 'Horde Druid - On Death - Set Data'),
(-452186, 0, 10, 0, 1, 4, 100, 0, 1000, 1000, 5000, 5000, 0, 11, 117932, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 'Horde Druid - OOC - Cast Healing Shen-zin Su''s Wound'),
(-452160, 0, 0, 1, 11, 0, 100, 0, 0, 0, 0, 0, 0, 103, 1, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 'Horde Druid - On Respawn - Set Root'),
(-452160, 0, 1, 0, 61, 0, 100, 0, 0, 0, 0, 0, 0, 22, 1, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 'Horde Druid - On Respawn - Set Event Phase 1'),
(-452160, 0, 2, 0, 25, 1, 100, 0, 0, 0, 0, 0, 0, 81, 16777216, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 'Horde Druid - On Reset - Set Npcflag'),
(-452160, 0, 3, 0, 4, 1, 100, 0, 0, 0, 0, 0, 0, 83, 16777216, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 'Horde Druid - On Aggro - Remove Npcflag'),
(-452160, 0, 4, 0, 8, 1, 100, 0, 56685, 0, 0, 0, 0, 80, 6077000, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 'Horde Druid - On Spellhit - Run Script'),
(-452160, 0, 5, 0, 52, 2, 100, 0, 0, 60770, 0, 0, 0, 53, 1, 6077004, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 'Horde Druid - On Text over - Start WP'),
(-452160, 0, 6, 7, 58, 2, 100, 0, 5, 6077004, 0, 0, 0, 11, 117932, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 'Horde Druid - On WP Ended - Cast Healing Shen-zin Su''s Wound'),
(-452160, 0, 7, 8, 61, 2, 100, 0, 0, 0, 0, 0, 0, 45, 1, 1, 0, 0, 0, 0, 10, 452176, 40789, 0, 0, 0, 0, 0, 'Horde Druid - On WP Ended - Set Data'),
(-452160, 0, 8, 0, 61, 2, 100, 0, 0, 0, 0, 0, 0, 22, 3, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 'Horde Druid - On WP Ended - Set Event Phase 3'),
(-452160, 0, 9, 0, 6, 4, 100, 0, 0, 0, 0, 0, 0, 45, 2, 1, 0, 0, 0, 0, 10, 452176, 40789, 0, 0, 0, 0, 0, 'Horde Druid - On Death - Set Data'),
(-452160, 0, 10, 0, 1, 4, 100, 0, 1000, 1000, 5000, 5000, 0, 11, 117932, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 'Horde Druid - OOC - Cast Healing Shen-zin Su''s Wound'),
(-452232, 0, 0, 1, 11, 0, 100, 0, 0, 0, 0, 0, 0, 103, 1, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 'Horde Druid - On Respawn - Set Root'),
(-452232, 0, 1, 0, 61, 0, 100, 0, 0, 0, 0, 0, 0, 22, 1, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 'Horde Druid - On Respawn - Set Event Phase 1'),
(-452232, 0, 2, 0, 25, 1, 100, 0, 0, 0, 0, 0, 0, 81, 16777216, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 'Horde Druid - On Reset - Set Npcflag'),
(-452232, 0, 3, 0, 4, 1, 100, 0, 0, 0, 0, 0, 0, 83, 16777216, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 'Horde Druid - On Aggro - Remove Npcflag'),
(-452232, 0, 4, 0, 8, 1, 100, 0, 56685, 0, 0, 0, 0, 80, 6077000, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 'Horde Druid - On Spellhit - Run Script'),
(-452232, 0, 5, 0, 52, 2, 100, 0, 0, 60770, 0, 0, 0, 53, 1, 6077005, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 'Horde Druid - On Text over - Start WP'),
(-452232, 0, 6, 7, 58, 2, 100, 0, 7, 6077005, 0, 0, 0, 11, 117932, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 'Horde Druid - On WP Ended - Cast Healing Shen-zin Su''s Wound'),
(-452232, 0, 7, 8, 61, 2, 100, 0, 0, 0, 0, 0, 0, 45, 1, 1, 0, 0, 0, 0, 10, 452176, 40789, 0, 0, 0, 0, 0, 'Horde Druid - On WP Ended - Set Data'),
(-452232, 0, 8, 0, 61, 2, 100, 0, 0, 0, 0, 0, 0, 22, 3, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 'Horde Druid - On WP Ended - Set Event Phase 3'),
(-452232, 0, 9, 0, 6, 4, 100, 0, 0, 0, 0, 0, 0, 45, 2, 1, 0, 0, 0, 0, 10, 452176, 40789, 0, 0, 0, 0, 0, 'Horde Druid - On Death - Set Data'),
(-452232, 0, 10, 0, 1, 4, 100, 0, 1000, 1000, 5000, 5000, 0, 11, 117932, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 'Horde Druid - OOC - Cast Healing Shen-zin Su''s Wound'),
(6077000, 9, 0, 0, 0, 0, 100, 0, 0, 0, 0, 0, 0, 11, 117950, 2, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 'Horde Druid - Cast Fighting Healer Rescued Aura'),
(6077000, 9, 1, 0, 0, 0, 100, 0, 0, 0, 0, 0, 0, 103, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 'Horde Druid - Unset Root'),
(6077000, 9, 2, 0, 0, 0, 100, 0, 0, 0, 0, 0, 0, 17, 30, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 'Horde Druid - Set Emote State'),
(6077000, 9, 3, 0, 0, 0, 100, 0, 0, 0, 0, 0, 0, 1, 0, 3000, 0, 0, 0, 0, 7, 0, 0, 0, 0, 0, 0, 0, 'Horde Druid - Talk'),
(6077000, 9, 4, 0, 0, 0, 100, 0, 0, 0, 0, 0, 0, 83, 16777216, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 'Horde Druid - Remove Npcflag'),
(6077000, 9, 5, 0, 0, 0, 100, 0, 0, 0, 0, 0, 0, 22, 2, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 'Horde Druid - Set Event Phase 2'),
-- Alliance Priest
(-452155, 0, 0, 1, 11, 0, 100, 0, 0, 0, 0, 0, 0, 103, 1, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 'Alliance Priest - On Respawn - Set Root'),
(-452155, 0, 1, 0, 61, 0, 100, 0, 0, 0, 0, 0, 0, 22, 1, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 'Alliance Priest - On Respawn - Set Event Phase 1'),
(-452155, 0, 2, 0, 25, 1, 100, 0, 0, 0, 0, 0, 0, 81, 16777216, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 'Alliance Priest - On Reset - Set Npcflag'),
(-452155, 0, 3, 0, 4, 1, 100, 0, 0, 0, 0, 0, 0, 83, 16777216, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 'Alliance Priest - On Aggro - Remove Npcflag'),
(-452155, 0, 4, 0, 8, 1, 100, 0, 56685, 0, 0, 0, 0, 80, 6087700, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 'Alliance Priest - On Spellhit - Run Script'),
(-452155, 0, 5, 0, 52, 2, 100, 0, 0, 60877, 0, 0, 0, 53, 1, 6087700, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 'Alliance Priest - On Text over - Start WP'),
(-452155, 0, 6, 7, 58, 2, 100, 0, 5, 6087700, 0, 0, 0, 11, 117784, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 'Alliance Priest - On WP Ended - Cast Healing Shen-zin Su''s Wound'),
(-452155, 0, 7, 8, 61, 2, 100, 0, 0, 0, 0, 0, 0, 45, 1, 1, 0, 0, 0, 0, 10, 452176, 40789, 0, 0, 0, 0, 0, 'Alliance Priest - On WP Ended - Set Data'),
(-452155, 0, 8, 0, 61, 2, 100, 0, 0, 0, 0, 0, 0, 22, 3, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 'Alliance Priest - On WP Ended - Set Event Phase 3'),
(-452155, 0, 9, 0, 6, 4, 100, 0, 0, 0, 0, 0, 0, 45, 2, 1, 0, 0, 0, 0, 10, 452176, 40789, 0, 0, 0, 0, 0, 'Alliance Priest - On Death - Set Data'),
(-452155, 0, 10, 0, 1, 4, 100, 0, 1000, 1000, 5000, 5000, 0, 11, 117784, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 'Alliance Priest - OOC - Cast Healing Shen-zin Su''s Wound'),
(-452139, 0, 0, 1, 11, 0, 100, 0, 0, 0, 0, 0, 0, 103, 1, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 'Alliance Priest - On Respawn - Set Root'),
(-452139, 0, 1, 0, 61, 0, 100, 0, 0, 0, 0, 0, 0, 22, 1, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 'Alliance Priest - On Respawn - Set Event Phase 1'),
(-452139, 0, 2, 0, 25, 1, 100, 0, 0, 0, 0, 0, 0, 81, 16777216, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 'Alliance Priest - On Reset - Set Npcflag'),
(-452139, 0, 3, 0, 4, 1, 100, 0, 0, 0, 0, 0, 0, 83, 16777216, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 'Alliance Priest - On Aggro - Remove Npcflag'),
(-452139, 0, 4, 0, 8, 1, 100, 0, 56685, 0, 0, 0, 0, 80, 6087700, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 'Alliance Priest - On Spellhit - Run Script'),
(-452139, 0, 5, 0, 52, 2, 100, 0, 0, 60877, 0, 0, 0, 53, 1, 6087701, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 'Alliance Priest - On Text over - Start WP'),
(-452139, 0, 6, 7, 58, 2, 100, 0, 5, 6087701, 0, 0, 0, 11, 117784, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 'Alliance Priest - On WP Ended - Cast Healing Shen-zin Su''s Wound'),
(-452139, 0, 7, 8, 61, 2, 100, 0, 0, 0, 0, 0, 0, 45, 1, 1, 0, 0, 0, 0, 10, 452176, 40789, 0, 0, 0, 0, 0, 'Alliance Priest - On WP Ended - Set Data'),
(-452139, 0, 8, 0, 61, 2, 100, 0, 0, 0, 0, 0, 0, 22, 3, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 'Alliance Priest - On WP Ended - Set Event Phase 3'),
(-452139, 0, 9, 0, 6, 4, 100, 0, 0, 0, 0, 0, 0, 45, 2, 1, 0, 0, 0, 0, 10, 452176, 40789, 0, 0, 0, 0, 0, 'Alliance Priest - On Death - Set Data'),
(-452139, 0, 10, 0, 1, 4, 100, 0, 1000, 1000, 5000, 5000, 0, 11, 117784, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 'Alliance Priest - OOC - Cast Healing Shen-zin Su''s Wound'),
(6087700, 9, 0, 0, 0, 0, 100, 0, 0, 0, 0, 0, 0, 11, 117950, 2, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 'Alliance Priest - Cast Fighting Healer Rescued Aura'),
(6087700, 9, 1, 0, 0, 0, 100, 0, 0, 0, 0, 0, 0, 103, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 'Alliance Priest - Unset Root'),
(6087700, 9, 2, 0, 0, 0, 100, 0, 0, 0, 0, 0, 0, 1, 0, 3000, 0, 0, 0, 0, 7, 0, 0, 0, 0, 0, 0, 0, 'Alliance Priest - Talk'),
(6087700, 9, 3, 0, 0, 0, 100, 0, 0, 0, 0, 0, 0, 83, 16777216, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 'Alliance Priest - Remove Npcflag'),
(6087700, 9, 4, 0, 0, 0, 100, 0, 0, 0, 0, 0, 0, 22, 2, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 'Alliance Priest - Set Event Phase 2'),
-- Loose Wreckage
(60848, 0, 0, 0, 54, 0, 100, 0, 0, 0, 0, 0, 0, 85, 46598, 2, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 'Loose Wreckage - Just Spawned - Invoker Cast Ride Vehicle Hardcoded'),
(60848, 0, 1, 0, 27, 0, 100, 0, 0, 0, 0, 0, 0, 81, 16777216, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 'Loose Wreckage - Passenger Boarded - Set Npcflag'),
(60848, 0, 2, 3, 8, 0, 100, 0, 117848, 0, 0, 0, 0, 11, 50630, 2, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 'Loose Wreckage - On Spellhit - Cast Eject All Passengers'),
(60848, 0, 3, 4, 61, 0, 100, 0, 0, 0, 0, 0, 0, 45, 1, 1, 0, 0, 0, 0, 23, 0, 0, 0, 0, 0, 0, 0, 'Loose Wreckage - On Spellhit - Set Data'),
(60848, 0, 4, 5, 61, 0, 100, 0, 0, 0, 0, 0, 0, 11, 117873, 2, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 'Loose Wreckage - On Spellhit - Cast Flying Wreckage'),
(60848, 0, 5, 0, 61, 0, 100, 0, 0, 0, 0, 0, 0, 41, 3000, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 'Loose Wreckage - On Spellhit - Despawn'),
-- Horde Druid - under wreckage
(-452192, 0, 0, 0, 11, 0, 100, 0, 0, 0, 0, 0, 0, 11, 117857, 2, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 'Horde Druid - On Respawn - Cast Summon Possessed Loose Wreckage'),
(-452192, 0, 1, 0, 38, 0, 100, 0, 1, 1, 0, 0, 0, 80, 6083400, 2, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 'Horde Druid - On Data Set - Run Script'),
(-452192, 0, 2, 0, 52, 0, 100, 0, 0, 60834, 0, 0, 0, 53, 1, 6083400, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 'Horde Druid - On Text Over - Start WP'),
(-452192, 0, 3, 4, 58, 0, 100, 0, 3, 6083400, 0, 0, 0, 11, 117932, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 'Horde Druid - On WP Ended - Cast Healing Shen-zin Su''s Wound'),
(-452192, 0, 4, 5, 61, 0, 100, 0, 0, 0, 0, 0, 0, 45, 1, 1, 0, 0, 0, 0, 10, 452176, 40789, 0, 0, 0, 0, 0, 'Horde Druid - On WP Ended - Set Data'),
(-452192, 0, 5, 0, 61, 0, 100, 0, 0, 0, 0, 0, 0, 22, 2, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 'Horde Druid - On WP Ended - Set Event Phase 2'),
(-452192, 0, 6, 0, 6, 2, 100, 0, 0, 0, 0, 0, 0, 45, 2, 1, 0, 0, 0, 0, 10, 452176, 40789, 0, 0, 0, 0, 0, 'Horde Druid - On Death - Set Data'),
(-452192, 0, 7, 0, 3, 1, 100, 0, 0, 40, 20000, 30000, 0, 11, 94527, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 'Horde Druid - On Mana Pct - Cast Drink Mana Potion'),
(-452192, 0, 8, 0, 2, 1, 100, 0, 0, 30, 20000, 30000, 0, 11, 117882, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 'Horde Druid - On Health Pct - Cast Healing Touch'),
(-452192, 0, 9, 0, 0, 1, 100, 0, 0, 2000, 2500, 3000, 0, 11, 117767, 0, 0, 0, 0, 0, 2, 0, 0, 0, 0, 0, 0, 0, 'Horde Druid - Update IC - Cast Wrath'),
(-452192, 0, 10, 0, 1, 2, 100, 0, 1000, 1000, 5000, 5000, 0, 11, 117932, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 'Horde Druid - OOC - Cast Healing Shen-zin Su''s Wound'),
(-452178, 0, 0, 0, 11, 0, 100, 0, 0, 0, 0, 0, 0, 11, 117857, 2, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 'Horde Druid - On Respawn - Cast Summon Possessed Loose Wreckage'),
(-452178, 0, 1, 0, 38, 0, 100, 0, 1, 1, 0, 0, 0, 80, 6083400, 2, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 'Horde Druid - On Data Set - Run Script'),
(-452178, 0, 2, 0, 52, 0, 100, 0, 0, 60834, 0, 0, 0, 53, 1, 6083401, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 'Horde Druid - On Text Over - Start WP'),
(-452178, 0, 3, 4, 58, 0, 100, 0, 7, 6083401, 0, 0, 0, 11, 117932, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 'Horde Druid - On WP Ended - Cast Healing Shen-zin Su''s Wound'),
(-452178, 0, 4, 5, 61, 0, 100, 0, 0, 0, 0, 0, 0, 45, 1, 1, 0, 0, 0, 0, 10, 452176, 40789, 0, 0, 0, 0, 0, 'Horde Druid - On WP Ended - Set Data'),
(-452178, 0, 5, 0, 61, 0, 100, 0, 0, 0, 0, 0, 0, 22, 2, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 'Horde Druid - On WP Ended - Set Event Phase 2'),
(-452178, 0, 6, 0, 6, 2, 100, 0, 0, 0, 0, 0, 0, 45, 2, 1, 0, 0, 0, 0, 10, 452176, 40789, 0, 0, 0, 0, 0, 'Horde Druid - On Death - Set Data'),
(-452178, 0, 7, 0, 3, 1, 100, 0, 0, 40, 20000, 30000, 0, 11, 94527, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 'Horde Druid - On Mana Pct - Cast Drink Mana Potion'),
(-452178, 0, 8, 0, 2, 1, 100, 0, 0, 30, 20000, 30000, 0, 11, 117882, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 'Horde Druid - On Health Pct - Cast Healing Touch'),
(-452178, 0, 9, 0, 0, 1, 100, 0, 0, 2000, 2500, 3000, 0, 11, 117767, 0, 0, 0, 0, 0, 2, 0, 0, 0, 0, 0, 0, 0, 'Horde Druid - Update IC - Cast Wrath'),
(-452178, 0, 10, 0, 1, 2, 100, 0, 1000, 1000, 5000, 5000, 0, 11, 117932, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 'Horde Druid - OOC - Cast Healing Shen-zin Su''s Wound'),
(6083400, 9, 0, 0, 0, 0, 100, 0, 0, 0, 0, 0, 0, 22, 1, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 'Horde Druid - Set Event Phase 1'),
(6083400, 9, 1, 0, 0, 0, 100, 0, 5000, 5000, 0, 0, 0, 1, 0, 5000, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 'Horde Druid - Talk'),
-- Alliance Priest - under wreckage
(-452157, 0, 0, 0, 11, 0, 100, 0, 0, 0, 0, 0, 0, 11, 117857, 2, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 'Alliance Priest - On Respawn - Cast Summon Possessed Loose Wreckage'),
(-452157, 0, 1, 0, 38, 0, 100, 0, 1, 1, 0, 0, 0, 80, 6087800, 2, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 'Alliance Priest - On Data Set - Run Script'),
(-452157, 0, 2, 0, 52, 0, 100, 0, 0, 60878, 0, 0, 0, 53, 1, 6087800, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 'Alliance Priest - On Text Over - Start WP'),
(-452157, 0, 3, 4, 58, 0, 100, 0, 5, 6087800, 0, 0, 0, 11, 117784, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 'Alliance Priest - On WP Ended - Cast Healing Shen-zin Su''s Wound'),
(-452157, 0, 4, 5, 61, 0, 100, 0, 0, 0, 0, 0, 0, 45, 1, 1, 0, 0, 0, 0, 10, 452176, 40789, 0, 0, 0, 0, 0, 'Alliance Priest - On WP Ended - Set Data'),
(-452157, 0, 5, 0, 61, 0, 100, 0, 0, 0, 0, 0, 0, 22, 2, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 'Alliance Priest - On WP Ended - Set Event Phase 2'),
(-452157, 0, 6, 0, 6, 2, 100, 0, 0, 0, 0, 0, 0, 45, 2, 1, 0, 0, 0, 0, 10, 452176, 40789, 0, 0, 0, 0, 0, 'Alliance Priest - On Death - Set Data'),
(-452157, 0, 7, 0, 3, 1, 100, 0, 0, 40, 20000, 30000, 0, 11, 94527, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 'Alliance Priest - On Mana Pct - Cast Drink Mana Potion'),
(-452157, 0, 8, 0, 2, 1, 100, 0, 0, 30, 20000, 30000, 0, 11, 117933, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 'Alliance Priest - On Health Pct - Cast Greater Heal'),
(-452157, 0, 9, 0, 0, 1, 100, 0, 0, 2000, 2500, 3000, 0, 11, 117935, 0, 0, 0, 0, 0, 2, 0, 0, 0, 0, 0, 0, 0, 'Alliance Priest - Update IC - Cast Smite'),
(-452157, 0, 10, 0, 1, 2, 100, 0, 1000, 1000, 5000, 5000, 0, 11, 117784, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 'Alliance Priest - OOC - Cast Healing Shen-zin Su''s Wound'),
(-452204, 0, 0, 0, 11, 0, 100, 0, 0, 0, 0, 0, 0, 11, 117857, 2, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 'Alliance Priest - On Respawn - Cast Summon Possessed Loose Wreckage'),
(-452204, 0, 1, 0, 38, 0, 100, 0, 1, 1, 0, 0, 0, 80, 6087800, 2, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 'Alliance Priest - On Data Set - Run Script'),
(-452204, 0, 2, 0, 52, 0, 100, 0, 0, 60878, 0, 0, 0, 53, 1, 6087801, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 'Alliance Priest - On Text Over - Start WP'),
(-452204, 0, 3, 4, 58, 0, 100, 0, 7, 6087801, 0, 0, 0, 11, 117784, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 'Alliance Priest - On WP Ended - Cast Healing Shen-zin Su''s Wound'),
(-452204, 0, 4, 5, 61, 0, 100, 0, 0, 0, 0, 0, 0, 45, 1, 1, 0, 0, 0, 0, 10, 452176, 40789, 0, 0, 0, 0, 0, 'Alliance Priest - On WP Ended - Set Data'),
(-452204, 0, 5, 0, 61, 0, 100, 0, 0, 0, 0, 0, 0, 22, 2, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 'Alliance Priest - On WP Ended - Set Event Phase 2'),
(-452204, 0, 6, 0, 6, 2, 100, 0, 0, 0, 0, 0, 0, 45, 2, 1, 0, 0, 0, 0, 10, 452176, 40789, 0, 0, 0, 0, 0, 'Alliance Priest - On Death - Set Data'),
(-452204, 0, 7, 0, 3, 1, 100, 0, 0, 40, 20000, 30000, 0, 11, 94527, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 'Alliance Priest - On Mana Pct - Cast Drink Mana Potion'),
(-452204, 0, 8, 0, 2, 1, 100, 0, 0, 30, 20000, 30000, 0, 11, 117933, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 'Alliance Priest - On Health Pct - Cast Greater Heal'),
(-452204, 0, 9, 0, 0, 1, 100, 0, 0, 2000, 2500, 3000, 0, 11, 117935, 0, 0, 0, 0, 0, 2, 0, 0, 0, 0, 0, 0, 0, 'Alliance Priest - Update IC - Cast Smite'),
(-452204, 0, 10, 0, 1, 2, 100, 0, 1000, 1000, 5000, 5000, 0, 11, 117784, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 'Alliance Priest - OOC - Cast Healing Shen-zin Su''s Wound'),
(6087800, 9, 0, 0, 0, 0, 100, 0, 0, 0, 0, 0, 0, 22, 1, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 'Alliance Priest - Set Event Phase 1'),
(6087800, 9, 1, 0, 0, 0, 100, 0, 5000, 5000, 0, 0, 0, 1, 0, 5000, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 'Alliance Priest - Talk'),
-- Aysa Cloudsinger
(56417, 0, 0, 0, 11, 0, 100, 0, 0, 0, 0, 0, 0, 8, 2, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 'Aysa Cloudsinger - On Respawn - Set React State'),
(56417, 0, 1, 0, 1, 0, 100, 0, 2000, 2000, 2000, 2000, 0, 49, 0, 0, 0, 0, 0, 0, 19, 0, 5, 0, 0, 0, 0, 0, 'Aysa Cloudsinger - Update OOC - Start Attack'),
(56417, 0, 2, 0, 0, 0, 100, 0, 2000, 4000, 2000, 4000, 0, 11, 117275, 0, 0, 0, 0, 0, 2, 0, 0, 0, 0, 0, 0, 0, 'Aysa Cloudsinger - Update IC - Cast Tempered Fury'),
(56417, 0, 3, 0, 0, 0, 100, 0, 5000, 8000, 10000, 15000, 0, 11, 117401, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 'Aysa Cloudsinger - Update IC - Cast Spinning Crane Kick'), -- only when multiple targets are nearby
-- Ji Firepaw
(56418, 0, 0, 0, 11, 0, 100, 0, 0, 0, 0, 0, 0, 8, 2, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 'Ji Firepaw - On Respawn - Set React State'),
(56418, 0, 1, 0, 60, 0, 100, 0, 10000, 20000, 110000, 120000, 0, 1, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 'Ji Firepaw - Update - Talk'),
(56418, 0, 2, 0, 60, 0, 100, 0, 10000, 20000, 20000, 60000, 0, 11, 131505, 2, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 'Ji Firepaw - Update - Cast Turtle Rumble'),
(56418, 0, 3, 4, 20, 0, 100, 0, 29799, 0, 0, 0, 0, 85, 108926, 2, 0, 0, 0, 0, 7, 0, 0, 0, 0, 0, 0, 0, 'Ji Firepaw - On Quest Rewarded - Invoker Cast Blackout into Healed Phase'),
(56418, 0, 4, 5, 61, 0, 100, 0, 0, 0, 0, 0, 0, 44, 544, 0, 0, 0, 0, 0, 7, 0, 0, 0, 0, 0, 0, 0, 'Ji Firepaw - On Quest Accepted - Remove Phase'),
(56418, 0, 5, 0, 61, 0, 100, 0, 0, 0, 0, 0, 0, 44, 545, 1, 0, 0, 0, 0, 7, 0, 0, 0, 0, 0, 0, 0, 'Ji Firepaw - On Quest Accepted - Add Phase'),
(56418, 0, 6, 0, 38, 0, 100, 0, 1, 1, 0, 0, 0, 80, 5641800, 2, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 'Ji Firepaw - On Data Set - Run Script'),
(5641800, 9, 0, 0, 0, 0, 100, 0, 2000, 2000, 0, 0, 0, 66, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 'Ji Firepaw - Set Orientation'),
(56418, 0, 7, 0, 1, 0, 100, 0, 2000, 2000, 2000, 2000, 0, 49, 0, 0, 0, 0, 0, 0, 19, 0, 5, 0, 0, 0, 0, 0, 'Ji Firepaw - Update OOC - Start Attack'),
(56418, 0, 8, 0, 0, 0, 100, 0, 0, 0, 3000, 3000, 0, 39, 15, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 'Ji Firepaw - Update IC - Call For Help'),
(56418, 0, 9, 0, 0, 0, 100, 0, 3000, 5000, 6000, 6000, 0, 11, 128631, 0, 0, 0, 0, 0, 2, 0, 0, 0, 0, 0, 0, 0, 'Ji Firepaw - Update IC - Cast Blackout Kick'),
(56418, 0, 10, 0, 0, 0, 100, 0, 3000, 5000, 3000, 5000, 0, 11, 128630, 0, 0, 0, 0, 0, 2, 0, 0, 0, 0, 0, 0, 0, 'Ji Firepaw - Update IC - Cast Jab'),
(56418, 0, 11, 0, 0, 0, 100, 0, 5000, 8000, 10000, 15000, 0, 11, 128635, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 'Ji Firepaw - Update IC - Cast Fists of Fury'), -- only when multiple targets are nearby
(56418, 0, 12, 0, 0, 0, 100, 0, 5000, 8000, 10000, 15000, 0, 11, 128632, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 'Ji Firepaw - Update IC - Cast Spinning Crane Kick'), -- only when multiple targets are nearby
(56418, 0, 13, 0, 0, 0, 100, 0, 10000, 10000, 10000, 10000, 0, 11, 128643, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 'Ji Firepaw - Update IC - Cast Healing Sphere'),
-- Delora Lionheart
(60851, 0, 0, 0, 10, 0, 100, 0, 1, 10, 240000, 300000, 1, 1, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 'Delora Lionheart - OOC LOS - Talk'),
(60851, 0, 1, 0, 0, 0, 100, 0, 5000, 8000, 5000, 8000, 0, 11, 128440, 0, 0, 0, 0, 0, 2, 0, 0, 0, 0, 0, 0, 0, 'Delora Lionheart - Update IC - Cast Piercing Strikes'),
(60851, 0, 2, 0, 0, 0, 100, 0, 2000, 5000, 2000, 5000, 0, 11, 128513, 0, 0, 0, 0, 0, 2, 0, 0, 0, 0, 0, 0, 0, 'Delora Lionheart - Update IC - Cast Strike'),
-- Korga Strongmane
(60852, 0, 0, 0, 10, 0, 100, 0, 1, 10, 240000, 300000, 1, 1, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 'Korga Strongmane - OOC LOS - Talk'),
(60852, 0, 1, 0, 0, 0, 100, 0, 2000, 4000, 5000, 8000, 0, 11, 128515, 0, 0, 0, 0, 0, 2, 0, 0, 0, 0, 0, 0, 0, 'Korga Strongmane - Update IC - Cast Lightning Bolt'),
-- Jojo Ironbrow
(56419, 0, 0, 0, 11, 0, 100, 0, 0, 0, 0, 0, 0, 8, 2, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 'Jojo Ironbrow - On Respawn - Set React State'),
(56419, 0, 1, 0, 9, 0, 50, 0, 5, 30, 5000, 10000, 0, 11, 81574, 0, 0, 0, 0, 0, 2, 0, 0, 0, 0, 0, 0, 0, 'Jojo Ironbrow - On Target In Range - Cast Charge'),
(56419, 0, 2, 0, 0, 0, 100, 0, 2000, 2000, 2000, 2000, 0, 11, 120383, 0, 0, 0, 0, 0, 2, 0, 0, 0, 0, 0, 0, 0, 'Jojo Ironbrow - Update IC - Cast Pillar Strike'),
(56419, 0, 3, 0, 0, 0, 100, 0, 5000, 8000, 8000, 12000, 0, 11, 120372, 1, 0, 0, 0, 0, 2, 0, 0, 0, 0, 0, 0, 0, 'Jojo Ironbrow - Update IC - Cast Pillar Sweep'),
-- Jojo Ironbrow
(55940, 0, 0, 1, 19, 0, 100, 0, 29798, 0, 0, 0, 0, 44, 543, 1, 0, 0, 0, 0, 7, 0, 0, 0, 0, 0, 0, 0, 'Jojo Ironbrow - On Quest Accepted - Add Phase'),
(55940, 0, 1, 0, 61, 0, 100, 0, 0, 0, 0, 0, 0, 44, 1835, 0, 0, 0, 0, 0, 7, 0, 0, 0, 0, 0, 0, 0, 'Jojo Ironbrow - On Quest Accepted - Remove Phase'),
-- Darkened Horror
(56007, 0, 0, 0, 0, 0, 100, 0, 3000, 5000, 8000, 12000, 0, 11, 128417, 0, 0, 0, 0, 0, 2, 0, 0, 0, 0, 0, 0, 0, 'Darkened Horror - Update IC - Cast Shadow Geyser'),
-- Darkened Horror
(-452012, 0, 0, 0, 11, 0, 100, 0, 0, 0, 0, 0, 0, 53, 1, 5600700, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 'Darkened Horror - On Respawn - Start WP'),
(-452012, 0, 1, 2, 58, 0, 100, 0, 9, 5600700, 0, 0, 0, 17, 27, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 'Darkened Horror - On WP Ended - Set Emote State'),
(-452012, 0, 2, 0, 61, 0, 100, 0, 0, 0, 0, 0, 0, 49, 0, 0, 0, 0, 0, 0, 11, 60554, 5, 0, 0, 0, 0, 0, 'Darkened Horror - On WP Ended - Start Attack'),
(-452012, 0, 3, 0, 0, 0, 100, 0, 3000, 5000, 8000, 12000, 0, 11, 128417, 0, 0, 0, 0, 0, 2, 0, 0, 0, 0, 0, 0, 0, 'Darkened Horror - Update IC - Cast Shadow Geyser'),
-- Darkened Horror
(-452006, 0, 0, 0, 11, 0, 100, 0, 0, 0, 0, 0, 0, 53, 1, 5600701, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 'Darkened Horror - On Respawn - Start WP'),
(-452006, 0, 1, 2, 58, 0, 100, 0, 8, 5600701, 0, 0, 0, 17, 27, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 'Darkened Horror - On WP Ended - Set Emote State'),
(-452006, 0, 2, 0, 61, 0, 100, 0, 0, 0, 0, 0, 0, 49, 0, 0, 0, 0, 0, 0, 11, 60554, 5, 0, 0, 0, 0, 0, 'Darkened Horror - On WP Ended - Start Attack'),
(-452006, 0, 3, 0, 0, 0, 100, 0, 3000, 5000, 8000, 12000, 0, 11, 128417, 0, 0, 0, 0, 0, 2, 0, 0, 0, 0, 0, 0, 0, 'Darkened Horror - Update IC - Cast Shadow Geyser'),
-- Darkened Horror
(-452103, 0, 0, 0, 11, 0, 100, 0, 0, 0, 0, 0, 0, 53, 1, 5600702, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 'Darkened Horror - On Respawn - Start WP'),
(-452103, 0, 1, 2, 58, 0, 100, 0, 12, 5600702, 0, 0, 0, 17, 27, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 'Darkened Horror - On WP Ended - Set Emote State'),
(-452103, 0, 2, 0, 61, 0, 100, 0, 0, 0, 0, 0, 0, 49, 0, 0, 0, 0, 0, 0, 11, 60554, 5, 0, 0, 0, 0, 0, 'Darkened Horror - On WP Ended - Start Attack'),
(-452103, 0, 3, 0, 0, 0, 100, 0, 3000, 5000, 8000, 12000, 0, 11, 128417, 0, 0, 0, 0, 0, 2, 0, 0, 0, 0, 0, 0, 0, 'Darkened Horror - Update IC - Cast Shadow Geyser'),
-- Darkened Terror
(56008, 0, 0, 0, 0, 0, 100, 0, 3000, 5000, 8000, 12000, 0, 11, 128424, 0, 0, 0, 0, 0, 2, 0, 0, 0, 0, 0, 0, 0, 'Darkened Terror - Update IC - Cast Backhanded Swipes'),
-- Darkened Terror
(-452104, 0, 0, 0, 11, 0, 100, 0, 0, 0, 0, 0, 0, 53, 1, 5600800, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 'Darkened Terror - On Respawn - Start WP'),
(-452104, 0, 1, 2, 58, 0, 100, 0, 12, 5600800, 0, 0, 0, 17, 27, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 'Darkened Terror - On WP Ended - Set Emote State'),
(-452104, 0, 2, 0, 61, 0, 100, 0, 0, 0, 0, 0, 0, 49, 0, 0, 0, 0, 0, 0, 11, 60554, 5, 0, 0, 0, 0, 0, 'Darkened Terror - On WP Ended - Start Attack'),
(-452104, 0, 3, 0, 0, 0, 100, 0, 3000, 5000, 8000, 12000, 0, 11, 128424, 0, 0, 0, 0, 0, 2, 0, 0, 0, 0, 0, 0, 0, 'Darkened Terror - Update IC - Cast Backhanded Swipes'),
-- Darkened Terror
(-452054, 0, 0, 0, 11, 0, 100, 0, 0, 0, 0, 0, 0, 53, 1, 5600801, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 'Darkened Terror - On Respawn - Start WP'),
(-452054, 0, 1, 2, 58, 0, 100, 0, 9, 5600801, 0, 0, 0, 17, 27, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 'Darkened Terror - On WP Ended - Set Emote State'),
(-452054, 0, 2, 0, 61, 0, 100, 0, 0, 0, 0, 0, 0, 49, 0, 0, 0, 0, 0, 0, 11, 60554, 5, 0, 0, 0, 0, 0, 'Darkened Terror - On WP Ended - Start Attack'),
(-452054, 0, 3, 0, 0, 0, 100, 0, 3000, 5000, 8000, 12000, 0, 11, 128424, 0, 0, 0, 0, 0, 2, 0, 0, 0, 0, 0, 0, 0, 'Darkened Terror - Update IC - Cast Backhanded Swipes'),
-- Deepscale Tormentor
(56360, 0, 0, 0, 11, 0, 100, 0, 0, 0, 0, 0, 0, 49, 0, 0, 0, 0, 0, 0, 9, 56362, 0, 5, 0, 0, 0, 0, 'Deepscale Tormentor - Update OOC - Start Attack'),
(56360, 0, 1, 0, 0, 0, 100, 0, 3000, 5000, 8000, 12000, 0, 11, 128270, 0, 0, 0, 0, 0, 2, 0, 0, 0, 0, 0, 0, 0, 'Deepscale Tormentor - Update IC - Cast Smash'),
-- Deepscale Ravager
(-452194, 0, 0, 0, 11, 0, 100, 0, 0, 0, 0, 0, 0, 97, 15, 15, 0, 0, 0, 0, 8, 0, 0, 0, 233.66, 3885.76, 67.891, 0, 'Deepscale Ravager - On Respawn - Jump to Pos'),
(-452194, 0, 1, 0, 34, 0, 100, 0, 16, 0, 0, 0, 0, 53, 1, 6078000, 0, 0, 0, 2, 1, 0, 0, 0, 0, 0, 0, 0, 'Deepscale Ravager - On Movement Inform - Start WP'),
(-452194, 0, 2, 0, 58, 0, 100, 0, 4, 6078000, 0, 0, 0, 41, 0, 6, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 'Deepscale Ravager - On WP Ended - Despawn'),
(-452235, 0, 0, 0, 11, 0, 100, 0, 0, 0, 0, 0, 0, 97, 15, 15, 0, 0, 0, 0, 8, 0, 0, 0, 278.486, 3856.96, 74.4581, 0, 'Deepscale Ravager - On Respawn - Jump to Pos'),
(-452235, 0, 1, 0, 34, 0, 100, 0, 16, 0, 0, 0, 0, 53, 1, 6078001, 0, 0, 0, 2, 1, 0, 0, 0, 0, 0, 0, 0, 'Deepscale Ravager - On Movement Inform - Start WP'),
(-452235, 0, 2, 0, 58, 0, 100, 0, 5, 6078001, 0, 0, 0, 41, 0, 6, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 'Deepscale Ravager - On WP Ended - Despawn'),
(-452246, 0, 0, 0, 11, 0, 100, 0, 0, 0, 0, 0, 0, 53, 1, 6078002, 0, 0, 0, 2, 1, 0, 0, 0, 0, 0, 0, 0, 'Deepscale Ravager - On Respawn - Start WP'),
(-452246, 0, 1, 0, 40, 0, 100, 0, 3, 6078002, 0, 0, 0, 54, 3000, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 'Deepscale Ravager - On WP Reached - Pause WP'),
(-452246, 0, 2, 0, 58, 0, 100, 0, 11, 6078002, 0, 0, 0, 41, 0, 6, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 'Deepscale Ravager - On WP Ended - Despawn'),
(-452141, 0, 0, 0, 11, 0, 100, 0, 0, 0, 0, 0, 0, 97, 15, 15, 0, 0, 0, 0, 8, 0, 0, 0, 328.392, 3855.01, 78.2913, 0, 'Deepscale Ravager - On Respawn - Jump to Pos'),
(-452141, 0, 1, 0, 34, 0, 100, 0, 16, 0, 0, 0, 0, 53, 1, 6078003, 0, 0, 0, 2, 1, 0, 0, 0, 0, 0, 0, 0, 'Deepscale Ravager - On Movement Inform - Start WP'),
(-452141, 0, 2, 0, 58, 0, 100, 0, 6, 6078003, 0, 0, 0, 41, 0, 6, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 'Deepscale Ravager - On WP Ended - Despawn'),
(-452234, 0, 0, 0, 11, 0, 100, 0, 0, 0, 0, 0, 0, 53, 1, 6078004, 0, 0, 0, 2, 1, 0, 0, 0, 0, 0, 0, 0, 'Deepscale Ravager - On Respawn - Start WP'),
(-452234, 0, 1, 0, 58, 0, 100, 0, 9, 6078004, 0, 0, 0, 41, 0, 6, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 'Deepscale Ravager - On WP Ended - Despawn'),
(-452199, 0, 0, 0, 11, 0, 100, 0, 0, 0, 0, 0, 0, 97, 15, 15, 0, 0, 0, 0, 8, 0, 0, 0, 267.337, 3952.24, 69.7518, 0, 'Deepscale Ravager - On Respawn - Jump to Pos'),
(-452199, 0, 1, 0, 34, 0, 100, 0, 16, 0, 0, 0, 0, 53, 1, 6078005, 0, 0, 0, 2, 1, 0, 0, 0, 0, 0, 0, 0, 'Deepscale Ravager - On Movement Inform - Start WP'),
(-452199, 0, 2, 0, 58, 0, 100, 0, 1, 6078005, 0, 0, 0, 41, 0, 6, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 'Deepscale Ravager - On WP Ended - Despawn'),
(-452223, 0, 0, 0, 11, 0, 100, 0, 0, 0, 0, 0, 0, 53, 1, 6078006, 0, 0, 0, 2, 1, 0, 0, 0, 0, 0, 0, 0, 'Deepscale Ravager - On Respawn - Start WP'),
(-452223, 0, 1, 2, 40, 0, 100, 0, 1, 6078006, 0, 0, 0, 54, 3000, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 'Deepscale Ravager - On WP Reached - Pause WP'),
(-452223, 0, 2, 0, 61, 0, 100, 0, 0, 0, 0, 0, 0, 17, 27, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 'Deepscale Ravager - On WP Reached - Set Emote State'),
(-452223, 0, 3, 0, 56, 0, 100, 0, 2, 6078006, 0, 0, 0, 17, 30, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 'Deepscale Ravager - On WP Resumed - Set Emote State'),
(-452223, 0, 4, 0, 58, 0, 100, 0, 6, 6078006, 0, 0, 0, 41, 0, 6, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 'Deepscale Ravager - On WP Ended - Despawn'),
(-452200, 0, 0, 0, 11, 0, 100, 0, 0, 0, 0, 0, 0, 97, 15, 15, 0, 0, 0, 0, 8, 0, 0, 0, 252.609, 3926.75, 66.9842, 0, 'Deepscale Ravager - On Respawn - Jump to Pos'),
(-452200, 0, 1, 2, 34, 0, 100, 0, 16, 0, 0, 0, 0, 66, 0, 0, 0, 0, 0, 0, 8, 0, 0, 0, 0, 0, 0, 1.570796, 'Deepscale Ravager - On Movement Inform - Set Orientation'),
(-452200, 0, 2, 3, 61, 0, 100, 0, 0, 0, 0, 0, 0, 17, 27, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 'Deepscale Ravager - On Movement Inform - Set Emote State'),
(-452200, 0, 3, 4, 61, 0, 100, 0, 0, 0, 0, 0, 0, 49, 0, 0, 0, 0, 0, 0, 10, 452146, 56418, 0, 0, 0, 0, 0, 'Deepscale Ravager - On Movement Inform - Start Attack'),
(-452200, 0, 4, 0, 61, 0, 100, 0, 0, 0, 0, 0, 0, 80, 6078000, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 'Deepscale Ravager - On Movement Inform - Run Script'),
(-452200, 0, 5, 0, 58, 0, 100, 0, 2, 6078007, 0, 0, 0, 41, 0, 6, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 'Deepscale Ravager - On WP Ended - Despawn'),
(6078000, 9, 0, 0, 0, 0, 100, 0, 3000, 3000, 0, 0, 0, 17, 30, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 'Deepscale Ravager - Set Emote State'),
(6078000, 9, 1, 0, 0, 0, 100, 0, 0, 0, 0, 0, 0, 53, 1, 6078007, 0, 0, 0, 2, 1, 0, 0, 0, 0, 0, 0, 0, 'Deepscale Ravager - Start WP'),
(-452201, 0, 0, 0, 11, 0, 100, 0, 0, 0, 0, 0, 0, 97, 15, 15, 0, 0, 0, 0, 8, 0, 0, 0, 261.281, 3950.65, 68.2835, 0, 'Deepscale Ravager - On Respawn - Jump to Pos'),
(-452201, 0, 1, 0, 34, 0, 100, 0, 16, 0, 0, 0, 0, 53, 1, 6078008, 0, 0, 0, 2, 1, 0, 0, 0, 0, 0, 0, 0, 'Deepscale Ravager - On Movement Inform - Start WP'),
(-452201, 0, 2, 0, 58, 0, 100, 0, 1, 6078008, 0, 0, 0, 41, 0, 6, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 'Deepscale Ravager - On WP Ended - Despawn'),
(-452189, 0, 0, 0, 11, 0, 100, 0, 0, 0, 0, 0, 0, 97, 15, 15, 0, 0, 0, 0, 8, 0, 0, 0, 246.102, 3953.03, 62.8652, 0, 'Deepscale Ravager - On Respawn - Jump to Pos'),
(-452189, 0, 1, 0, 34, 0, 100, 0, 16, 0, 0, 0, 0, 53, 1, 6078009, 0, 0, 0, 2, 1, 0, 0, 0, 0, 0, 0, 0, 'Deepscale Ravager - On Movement Inform - Start WP'),
(-452189, 0, 2, 0, 58, 0, 100, 0, 1, 6078009, 0, 0, 0, 41, 0, 6, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 'Deepscale Ravager - On WP Ended - Despawn'),
(-452202, 0, 0, 0, 11, 0, 100, 0, 0, 0, 0, 0, 0, 97, 15, 15, 0, 0, 0, 0, 8, 0, 0, 0, 251.585, 3950.82, 64.9043, 0, 'Deepscale Ravager - On Respawn - Jump to Pos'),
(-452202, 0, 1, 0, 34, 0, 100, 0, 16, 0, 0, 0, 0, 53, 1, 6078010, 0, 0, 0, 2, 1, 0, 0, 0, 0, 0, 0, 0, 'Deepscale Ravager - On Movement Inform - Start WP'),
(-452202, 0, 2, 0, 58, 0, 100, 0, 1, 6078010, 0, 0, 0, 41, 0, 6, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 'Deepscale Ravager - On WP Ended - Despawn'),
(-452276, 0, 0, 0, 11, 0, 100, 0, 0, 0, 0, 0, 0, 97, 15, 15, 0, 0, 0, 0, 8, 0, 0, 0, 244.701, 4033.27, 59.3918, 0, 'Deepscale Ravager - On Respawn - Jump to Pos'),
(-452276, 0, 1, 0, 34, 0, 100, 0, 16, 0, 0, 0, 0, 53, 1, 6078011, 0, 0, 0, 2, 1, 0, 0, 0, 0, 0, 0, 0, 'Deepscale Ravager - On Movement Inform - Start WP'),
(-452276, 0, 2, 0, 58, 0, 100, 0, 3, 6078011, 0, 0, 0, 41, 0, 6, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 'Deepscale Ravager - On WP Ended - Despawn'),
(-452226, 0, 0, 0, 11, 0, 100, 0, 0, 0, 0, 0, 0, 97, 15, 15, 0, 0, 0, 0, 8, 0, 0, 0, 238.3127, 4034.402, 60.0663, 0, 'Deepscale Ravager - On Respawn - Jump to Pos'),
(-452226, 0, 1, 0, 34, 0, 100, 0, 16, 0, 0, 0, 0, 53, 1, 6078011, 0, 0, 0, 2, 1, 0, 0, 0, 0, 0, 0, 0, 'Deepscale Ravager - On Movement Inform - Start WP'),
(-452226, 0, 2, 0, 58, 0, 100, 0, 3, 6078011, 0, 0, 0, 41, 0, 6, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 'Deepscale Ravager - On WP Ended - Despawn'),
-- Deepscale Fleshripper
(-452218, 0, 0, 0, 11, 0, 100, 0, 0, 0, 0, 0, 0, 53, 1, 6085800, 0, 0, 0, 2, 1, 0, 0, 0, 0, 0, 0, 0, 'Deepscale Fleshripper - On Respawn - Start WP'),
(-452218, 0, 1, 0, 58, 0, 100, 0, 7, 6085800, 0, 0, 0, 41, 0, 6, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 'Deepscale Fleshripper - On WP Ended - Despawn'),
(-452218, 0, 2, 0, 0, 0, 100, 0, 3000, 5000, 5000, 8000, 0, 11, 128533, 0, 0, 0, 0, 0, 2, 0, 0, 0, 0, 0, 0, 0, 'Deepscale Fleshripper - Update IC - Cast Rip Flesh'),
(-452241, 0, 0, 0, 11, 0, 100, 0, 0, 0, 0, 0, 0, 53, 1, 6085801, 0, 0, 0, 2, 1, 0, 0, 0, 0, 0, 0, 0, 'Deepscale Fleshripper - On Respawn - Start WP'),
(-452241, 0, 1, 0, 58, 0, 100, 0, 8, 6085801, 0, 0, 0, 41, 0, 6, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 'Deepscale Fleshripper - On WP Ended - Despawn'),
(-452241, 0, 2, 0, 0, 0, 100, 0, 3000, 5000, 5000, 8000, 0, 11, 128533, 0, 0, 0, 0, 0, 2, 0, 0, 0, 0, 0, 0, 0, 'Deepscale Fleshripper - Update IC - Cast Rip Flesh'),
(-452215, 0, 0, 0, 11, 0, 100, 0, 0, 0, 0, 0, 0, 53, 1, 6085802, 0, 0, 0, 2, 1, 0, 0, 0, 0, 0, 0, 0, 'Deepscale Fleshripper - On Respawn - Start WP'),
(-452215, 0, 1, 0, 58, 0, 100, 0, 7, 6085802, 0, 0, 0, 41, 0, 6, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 'Deepscale Fleshripper - On WP Ended - Despawn'),
(-452215, 0, 2, 3, 40, 0, 100, 0, 3, 6085802, 0, 0, 0, 54, 3000, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 'Deepscale Fleshripper - On WP Reached - Pause WP'),
(-452215, 0, 3, 0, 61, 0, 100, 0, 0, 0, 0, 0, 0, 17, 27, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 'Deepscale Fleshripper - On WP Reached - Set Emote State'),
(-452215, 0, 4, 0, 56, 0, 100, 0, 4, 6085802, 0, 0, 0, 17, 30, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 'Deepscale Fleshripper - On WP Resumed - Set Emote State'),
(-452215, 0, 5, 0, 0, 0, 100, 0, 3000, 5000, 5000, 8000, 0, 11, 128533, 0, 0, 0, 0, 0, 2, 0, 0, 0, 0, 0, 0, 0, 'Deepscale Fleshripper - Update IC - Cast Rip Flesh'),
(-452243, 0, 0, 0, 11, 0, 100, 0, 0, 0, 0, 0, 0, 53, 1, 6085803, 0, 0, 0, 2, 1, 0, 0, 0, 0, 0, 0, 0, 'Deepscale Fleshripper - On Respawn - Start WP'),
(-452243, 0, 1, 0, 58, 0, 100, 0, 8, 6085803, 0, 0, 0, 41, 0, 6, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 'Deepscale Fleshripper - On WP Ended - Despawn'),
(-452243, 0, 2, 0, 0, 0, 100, 0, 3000, 5000, 5000, 8000, 0, 11, 128533, 0, 0, 0, 0, 0, 2, 0, 0, 0, 0, 0, 0, 0, 'Deepscale Fleshripper - Update IC - Cast Rip Flesh'),
(-452238, 0, 0, 0, 11, 0, 100, 0, 0, 0, 0, 0, 0, 53, 1, 6085804, 0, 0, 0, 2, 1, 0, 0, 0, 0, 0, 0, 0, 'Deepscale Fleshripper - On Respawn - Start WP'),
(-452238, 0, 1, 0, 58, 0, 100, 0, 11, 6085804, 0, 0, 0, 41, 0, 6, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 'Deepscale Fleshripper - On WP Ended - Despawn'),
(-452238, 0, 2, 0, 0, 0, 100, 0, 3000, 5000, 5000, 8000, 0, 11, 128533, 0, 0, 0, 0, 0, 2, 0, 0, 0, 0, 0, 0, 0, 'Deepscale Fleshripper - Update IC - Cast Rip Flesh'),
(-452193, 0, 0, 0, 11, 0, 100, 0, 0, 0, 0, 0, 0, 53, 1, 6085805, 0, 0, 0, 2, 1, 0, 0, 0, 0, 0, 0, 0, 'Deepscale Fleshripper - On Respawn - Start WP'),
(-452193, 0, 1, 2, 40, 0, 100, 0, 1, 6085805, 0, 0, 0, 54, 3000, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 'Deepscale Fleshripper - On WP Reached - Pause WP'),
(-452193, 0, 2, 0, 61, 0, 100, 0, 0, 0, 0, 0, 0, 49, 0, 0, 0, 0, 0, 0, 19, 60877, 30, 0, 0, 0, 0, 0, 'Deepscale Fleshripper - On WP Reached - Start Attack'),
(-452193, 0, 3, 0, 58, 0, 100, 0, 6, 6085805, 0, 0, 0, 41, 0, 6, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 'Deepscale Fleshripper - On WP Ended - Despawn'),
(-452193, 0, 4, 0, 0, 0, 100, 0, 3000, 5000, 5000, 8000, 0, 11, 128533, 0, 0, 0, 0, 0, 2, 0, 0, 0, 0, 0, 0, 0, 'Deepscale Fleshripper - Update IC - Cast Rip Flesh');


DELETE FROM `creature_template_difficulty` WHERE `entry` IN (60888, 60889, 60685, 60554, 65742, 55946, 56195, 56174,60848);
INSERT INTO `creature_template_difficulty` VALUES
(60848, 0, 0, 0, 80, 4, 1, 1, 1, 1, 20670, 536870912, 0, 0, 0, 0, 0, 0, 0, 536871168, 0, 33554432, 0, 0, 0, 0, 0, 56647),
(65742, 0, 0, 0, 80, 4, 1, 1, 1, 0.2, 59502, 4096, 0, 0, 0, 0, 0, 0, 0, 268435456, 0, 0, 0, 0, 0, 0, 0, 56647),
(60554, 0, 0, 0, 80, 4, 1, 1, 1, 1, 21169, 4096, 0, 0, 0, 0, 0, 0, 0, 268435456, 0, 0, 0, 0, 0, 0, 0, 56647),
(60685, 0, 0, 0, 80, 4, 0.5, 1, 1, 1, 20951, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 256, 0, 0, 0, 0, 0, 56647),
(60888, 0, 0, 0, 80, 4, 5, 1, 1, 0.01, 20592, 4096, 0, 0, 0, 0, 0, 0, 0, 268435456, 0, 0, 0, 0, 0, 0, 0, 56647),
(60889, 0, 0, 0, 80, 4, 5, 1, 1, 0.2, 20590, 4096, 0, 0, 0, 0, 0, 0, 0, 268435456, 0, 0, 0, 0, 0, 0, 0, 56647),
(56174, 0, 0, 0, 80, 4, 0.5, 1, 1, 0.2, 28481, 0, 0, 0, 56174, 0, 1, 0, 0, 268435456, 0, 0, 0, 0, 0, 0, 0, 56647),
(55946, 0, 0, 0, 80, 4, 1, 1, 1, 0.2, 28860, 1, 0, 0, 55946, 0, 1, 0, 0, 268435456, 0, 0, 0, 0, 0, 0, 0, 56647),
(56195, 0, 0, 0, 80, 4, 1, 1, 1, 0.2, 28424, 0, 0, 0, 0, 0, 0, 0, 0, 268435712, 0, 0, 0, 0, 0, 0, 0, 56647);

DELETE FROM `creature_template_addon` WHERE `entry` IN (60848,60888, 60889, 60685, 60554, 65742, 55946, 56195, 56174);
INSERT INTO `creature_template_addon` (`entry`, `PathId`, `mount`, `MountCreatureId`, `StandState`, `AnimTier`, `VisFlags`, `SheathState`, `PvPFlags`, `emote`, `aiAnimKit`, `movementAnimKit`, `meleeAnimKit`, `visibilityDistanceType`, `auras`) VALUES 
(60848, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, '117855'),
(60554, 0, 0, 0, 0, 0, 0, 1, 0, 425, 0, 0, 0, 0, ''),
(65742, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, ''),
(60685, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, ''),
(60888, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, ''),
(60889, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, ''),
(56174, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, ''), -- tiger cub
('55946', '0', '0', '0', '0', '0', '0', '1', '0', '0', '0', '0', '0', '0', ''), -- tiger
(56195, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, '96733'); -- Escaped horde engineer


DELETE FROM `creature_addon` WHERE `guid` IN (451865, 451813, 451815, 451817, 451821, 451836, 451844, 451847, 451900, 451905, 451916, 451923, 451929, 451932, 
451339, 451352, 452010, 452034, 452038, 452041,451350, 451840, 451867, 451901, 452033, 452040, 452042,451944,452052,452098,451984,451763,451751,451747,451953,
452005,452051,452009,451965,451979,451858,451731,451725,452374,452053,452001,452070,452083,452073);
INSERT INTO `creature_addon` (`guid`, `PathId`, `mount`, `MountCreatureID`, `StandState`, `VisFlags`, `SheathState`, `PvPFlags`, `emote`, `aiAnimKit`, `movementAnimKit`, `meleeAnimKit`, `visibilityDistanceType`, `auras`) VALUES 
-- tiger cub spawns with zzz aura
(451350, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, '120703'),
(451840, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, '120703'),
(451867, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, '120703'),
(451901, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, '120703'),
(452033, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, '120703'),
(452040, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, '120703'),
(452042, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, '120703'),
-- tiger spawns with zzz aura
(451339, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, '120703'),
(451352, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, '120703'),
(451813, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, '120703'),
(451815, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, '120703'),
(451817, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, '120703'),
(451821, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, '120703'),
(451836, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, '120703'),
(451844, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, '120703'),
(451847, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, '120703'),
(451865, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, '120703'),
(451900, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, '120703'),
(451905, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, '120703'),
(451916, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, '120703'),
(451923, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, '120703'),
(451929, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, '120703'),
(451932, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, '120703'),
(452010, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, '120703'),
(452034, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, '120703'),
(452038, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, '120703'),
(452041, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, '120703'),
-- Delora
(452374, 0, 0, 0, 0, 1, 1, 0, 0, 0, 0, 0, 0, '80797'),
-- Alliance priest
(451731, 6089500, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, ''),
(451725, 0, 0, 0, 1, 0, 1, 0, 0, 0, 0, 0, 0, ''),
-- Horde Druid
(451965, 6089600, 0, 0, 8, 0, 1, 0, 0, 0, 0, 0, 0, ''),
(451979, 0, 0, 0, 8, 0, 1, 0, 0, 0, 0, 0, 0, ''),
(451858, 6089601, 0, 0, 8, 0, 1, 0, 0, 0, 0, 0, 0, ''),
-- Deepscale Tormentor
(452053, 5636000, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, ''),
(452001, 5636001, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, ''),
(452070, 5636002, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, ''),
(452083, 5636003, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, ''),
(452073, 5636004, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, ''),
-- Terror/Horror with path
(451944, 5600802, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, ''),
(452052, 5600803, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, ''),
(452098, 5600703, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, ''),
(451984, 5600704, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, ''),
(451763, 5600705, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, ''),
(451751, 5600706, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, ''),
(451747, 5600707, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, ''),
(451953, 5600708, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, ''),
(452005, 5600709, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, ''),
(452051, 5600710, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, ''),
(452009, 5600711, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, '');



DELETE FROM `creature_addon` WHERE `guid` IN (451348, 451352, 451756, 451788, 451835, 451839, 451845, 451849, 451878, 451880, 451883, 451886, 
451888, 451893, 451912, 451918, 451926, 451930, 451933, 451937, 451941, 451942, 451983, 452015, 452019, 452025, 451928, 451810, 451875, 451857);
INSERT INTO `creature_addon` (`guid`, `PathId`, `mount`, `MountCreatureID`, `StandState`, `VisFlags`, `SheathState`, `PvPFlags`, `emote`, 
`aiAnimKit`, `movementAnimKit`, `meleeAnimKit`, `visibilityDistanceType`, `auras`) VALUES 
-- Escaped Horde Crewman with path
(451928, 6055400, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, ''),
(451810, 6055401, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, ''),
(451875, 6574200, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, '129315'),
(451857, 6574201, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, '129315'),
-- tiger spawns with path
(451348, 5594600, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, ''),
(451352, 5594601, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, ''),
(451756, 5594602, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, ''),
(451788, 5594603, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, ''),
(451835, 5594604, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, ''),
(451839, 5594605, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, ''),
(451845, 5594606, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, ''),
(451849, 5594607, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, ''),
(451878, 5594608, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, ''),
(451880, 5594609, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, ''),
(451883, 5594610, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, ''),
(451886, 5594611, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, ''),
(451888, 5594612, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, ''),
(451893, 5594613, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, ''),
(451912, 5594614, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, ''),
(451918, 5594615, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, ''),
(451926, 5594616, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, ''),
(451930, 5594617, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, ''),
(451933, 5594618, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, ''),
(451937, 5594619, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, ''),
(451941, 5594620, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, ''),
(451942, 5594621, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, ''),
(451983, 5594622, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, ''),
(452015, 5594623, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, ''),
(452019, 5594624, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, ''),
(452025, 5594625, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, '');

UPDATE `creature` SET `MovementType` = 2 WHERE `guid` IN (451857, 451875, 451810, 451928, 451348, 451352, 451756, 451788, 451835, 451839, 451845, 451849, 451878, 
451880, 451883, 451886, 451888, 451893, 451912, 451918, 451926, 451930, 451933, 451937, 451941, 451942, 451983, 452015, 452019, 452025,451944,452052,452098,451984,
451763,451751,451747,451953,452005,452051,452009,451965,451858,451731,452053,452001,452070,452083,452073,452164,452144,452162);
UPDATE `creature` SET `MovementType` = 1, `wander_distance` = 5 WHERE `id` IN (55946) and `guid` not in 
(451348, 451352, 451756, 451788, 451835, 451839, 451845, 451849, 451878, 451880, 451883, 451886, 451888, 451893, 451912, 451918, 451926, 451930, 451933, 451937, 451941, 451942, 451983, 452015, 452019, 452025, 451865, 451813, 451815, 451817, 451821, 451836, 451844, 451847, 451900, 451905, 451916, 451923, 451929, 451932, 451339, 451352, 452010, 452034, 452038, 452041);

UPDATE `creature` SET `position_x` = 548.625, `position_y` = 3492.5, `position_z` = 95.0985, `orientation` = 1.64148 WHERE `guid` = 451840; -- tiger cub pos fix
UPDATE `creature` SET `position_x` = 225.803, `position_y` = 4051.25, `position_z` = 60.2276, `orientation` = 5.43847 WHERE `guid` = 452226; -- Deepscale Ravager pos fix

-- Spawns
SET @CGUID := 900000;
DELETE FROM `creature` WHERE `guid` = 452141; -- Deescale Ravager wrong spawns reuse
DELETE FROM `creature` WHERE `guid` BETWEEN @CGUID+751 AND @CGUID+805;
DELETE FROM `creature` WHERE `id` IN (60897,56416,60722);
INSERT INTO `creature` VALUES
('452141', '60780', '860', '0', '0', '0', '0', '544', '0', '975', '0', '0', '343.309', '3834.16', '85.7566', '2.0274', '0', '0', '0', '100', '0', NULL, NULL, NULL, NULL, '', NULL, '20886', -1),
(@CGUID+751, '60897', '860', '0', '0', '0', '0', '0', '641', '-1', '0', '0', '239.771', '3842.15', '73.6128', '0.752572', '120', '0', '0', '100', '0', NULL, NULL, NULL, NULL, '', NULL, '20886', -1),
(@CGUID+752, '60897', '860', '0', '0', '0', '0', '0', '641', '-1', '0', '0', '230.028', '3849.29', '73.4149', '0.570887', '120', '0', '0', '100', '0', NULL, NULL, NULL, NULL, '', NULL, '20886', -1),
(@CGUID+753, '60897', '860', '0', '0', '0', '0', '0', '641', '-1', '0', '0', '251.891', '3841.52', '73.8628', '1.51058', '120', '0', '0', '100', '0', NULL, NULL, NULL, NULL, '', NULL, '20886', -1),
(@CGUID+754, '60897', '860', '0', '0', '0', '0', '0', '641', '-1', '0', '0', '231.281', '3847.75', '73.4115', '0.861924', '120', '0', '0', '100', '0', NULL, NULL, NULL, NULL, '', NULL, '20886', -1),
(@CGUID+755, '60897', '860', '0', '0', '0', '0', '0', '641', '-1', '0', '0', '240.887', '3850.61', '73.1612', '0.735617', '120', '0', '0', '100', '0', NULL, NULL, NULL, NULL, '', NULL, '20886', -1),
(@CGUID+756, '60897', '860', '0', '0', '0', '0', '0', '641', '-1', '0', '0', '242.866', '3841.47', '73.7188', '1.11273', '120', '0', '0', '100', '0', NULL, NULL, NULL, NULL, '', NULL, '20886', -1),
(@CGUID+757, '60897', '860', '0', '0', '0', '0', '0', '641', '-1', '0', '0', '239.288', '3851.72', '73.0833', '0.994168', '120', '0', '0', '100', '0', NULL, NULL, NULL, NULL, '', NULL, '20886', -1),
(@CGUID+758, '60897', '860', '0', '0', '0', '0', '0', '641', '-1', '0', '0', '250.134', '3841.75', '73.7274', '1.42777', '120', '0', '0', '100', '0', NULL, NULL, NULL, NULL, '', NULL, '20886', -1),
(@CGUID+759, '60897', '860', '0', '0', '0', '0', '545', '0', '976', '0', '0', '266.352', '3851.38', '73.7918', '3.05194', '120', '0', '0', '100', '0', NULL, NULL, NULL, NULL, '', NULL, '20886', -1),
(@CGUID+760, '60897', '860', '0', '0', '0', '0', '545', '0', '976', '0', '0', '266.035', '3853.73', '73.8248', '2.79333', '120', '0', '0', '100', '0', NULL, NULL, NULL, NULL, '', NULL, '20886', -1),
(@CGUID+761, '60897', '860', '0', '0', '0', '0', '545', '0', '976', '0', '0', '240.767', '3850.78', '73.1488', '0.735617', '120', '0', '0', '100', '0', NULL, NULL, NULL, NULL, '', NULL, '20886', -1),
(@CGUID+762, '60897', '860', '0', '0', '0', '0', '545', '0', '976', '0', '0', '239.651', '3842.32', '73.6021', '0.752572', '120', '0', '0', '100', '0', NULL, NULL, NULL, NULL, '', NULL, '20886', -1),
(@CGUID+763, '60897', '860', '0', '0', '0', '0', '545', '0', '976', '0', '0', '250.014', '3841.92', '73.7118', '1.42777', '120', '0', '0', '100', '0', NULL, NULL, NULL, NULL, '', NULL, '20886', -1),
(@CGUID+764, '60897', '860', '0', '0', '0', '0', '545', '0', '976', '0', '0', '231.161', '3847.92', '73.4108', '0.861924', '120', '0', '0', '100', '0', NULL, NULL, NULL, NULL, '', NULL, '20886', -1),
(@CGUID+765, '60897', '860', '0', '0', '0', '0', '545', '0', '976', '0', '0', '239.168', '3851.89', '73.0701', '0.994168', '120', '0', '0', '100', '0', NULL, NULL, NULL, NULL, '', NULL, '20886', -1),
(@CGUID+766, '60897', '860', '0', '0', '0', '0', '545', '0', '976', '0', '0', '242.747', '3841.64', '73.7026', '1.11273', '120', '0', '0', '100', '0', NULL, NULL, NULL, NULL, '', NULL, '20886', -1),
(@CGUID+767, '60897', '860', '0', '0', '0', '0', '545', '0', '976', '0', '0', '251.771', '3841.7', '73.8428', '1.51058', '120', '0', '0', '100', '0', NULL, NULL, NULL, NULL, '', NULL, '20886', -1),
(@CGUID+768, '60897', '860', '0', '0', '0', '0', '545', '0', '976', '0', '0', '229.908', '3849.46', '73.4182', '0.570887', '120', '0', '0', '100', '0', NULL, NULL, NULL, NULL, '', NULL, '20886', -1),
(@CGUID+769, '56416', '860', '0', '0', '0', '0', '543', '0', '-1', '0', '0', '286.953', '4004.33', '74.7734', '3.03321', '120', '0', '0', '100', '0', '2', NULL, NULL, NULL, '', NULL, '20886', -1),
(@CGUID+770, '56416', '860', '0', '0', '0', '0', '993', '0', '-1', '0', '0', '286.953', '4004.33', '74.6184', '3.03321', '120', '0', '0', '100', '0', '2', NULL, NULL, NULL, '', NULL, '20886', -1),
(@CGUID+771, '60722', '860', '0', '0', '0', '0', '0', '680', '-1', '0', '0', '272.712', '3898.62', '74.0749', '5.14483', '120', '0', '0', '100', '0', NULL, NULL, NULL, NULL, '', NULL, '20886', -1),
(@CGUID+772, '60722', '860', '0', '0', '0', '0', '0', '680', '-1', '0', '0', '256.92', '3931.9', '68.2501', '4.22962', '120', '0', '0', '100', '0', NULL, NULL, NULL, NULL, '', NULL, '20886', -1),
(@CGUID+773, '60722', '860', '0', '0', '0', '0', '0', '680', '-1', '0', '0', '283.771', '3904.66', '74.3394', '5.27164', '120', '0', '0', '100', '0', NULL, NULL, NULL, NULL, '', NULL, '20886', -1),
(@CGUID+774, '60722', '860', '0', '0', '0', '0', '0', '680', '-1', '0', '0', '232.344', '3931.99', '62.8128', '4.03979', '120', '0', '0', '100', '0', NULL, NULL, NULL, NULL, '', NULL, '20886', -1),
(@CGUID+775, '60722', '860', '0', '0', '0', '0', '0', '680', '-1', '0', '0', '240.266', '3899.3', '67.3702', '4.68496', '120', '0', '0', '100', '0', NULL, NULL, NULL, NULL, '', NULL, '20886', -1),
(@CGUID+776, '60722', '860', '0', '0', '0', '0', '0', '680', '-1', '0', '0', '252.333', '4008.32', '88.9643', '2.94997', '120', '0', '0', '100', '0', NULL, NULL, NULL, NULL, '', NULL, '20886', -1),
(@CGUID+777, '60722', '860', '0', '0', '0', '0', '0', '680', '-1', '0', '0', '247.415', '3995.6', '61.6317', '4.98626', '120', '0', '0', '100', '0', NULL, NULL, NULL, NULL, '', NULL, '20886', -1),
(@CGUID+778, '60722', '860', '0', '0', '0', '0', '0', '680', '-1', '0', '0', '235.812', '3980.62', '60.4728', '5.46494', '120', '0', '0', '100', '0', NULL, NULL, NULL, NULL, '', NULL, '20886', -1),
(@CGUID+779, '60722', '860', '0', '0', '0', '0', '0', '680', '-1', '0', '0', '250.292', '4012.09', '89.0477', '3.33673', '120', '0', '0', '100', '0', NULL, NULL, NULL, NULL, '', NULL, '20886', -1),
(@CGUID+780, '60722', '860', '0', '0', '0', '0', '0', '680', '-1', '0', '0', '291.932', '3971.23', '76.2108', '1.42247', '120', '0', '0', '100', '0', NULL, NULL, NULL, NULL, '', NULL, '20886', -1),
(@CGUID+781, '60722', '860', '0', '0', '0', '0', '0', '680', '-1', '0', '0', '254.259', '3978.61', '87.3085', '2.47727', '120', '0', '0', '100', '0', NULL, NULL, NULL, NULL, '', NULL, '20886', -1),
(@CGUID+782, '60722', '860', '0', '0', '0', '0', '0', '680', '-1', '0', '0', '255.899', '4020.24', '64.1299', '0.117328', '120', '0', '0', '100', '0', NULL, NULL, NULL, NULL, '', NULL, '20886', -1),
(@CGUID+783, '60722', '860', '0', '0', '0', '0', '0', '680', '-1', '0', '0', '232.519', '4012.78', '76.9504', '0.288392', '120', '0', '0', '100', '0', NULL, NULL, NULL, NULL, '', NULL, '20886', -1),
(@CGUID+784, '60722', '860', '0', '0', '0', '0', '0', '680', '-1', '0', '0', '261.969', '4019', '79.3792', '5.828', '120', '0', '0', '100', '0', NULL, NULL, NULL, NULL, '', NULL, '20886', -1),
(@CGUID+785, '60722', '860', '0', '0', '0', '0', '0', '680', '-1', '0', '0', '294.356', '4039.98', '76.8561', '0.926414', '120', '0', '0', '100', '0', NULL, NULL, NULL, NULL, '', NULL, '20886', -1),
(@CGUID+786, '60722', '860', '0', '0', '0', '0', '545', '0', '976', '0', '0', '209.575', '3935', '60.8106', '5.49304', '120', '0', '0', '100', '0', NULL, NULL, NULL, NULL, '', NULL, '20886', -1),
(@CGUID+787, '60722', '860', '0', '0', '0', '0', '545', '0', '976', '0', '0', '221.316', '3894.56', '63.1523', '1.12152', '120', '0', '0', '100', '0', NULL, NULL, NULL, NULL, '', NULL, '20886', -1),
(@CGUID+788, '60722', '860', '0', '0', '0', '0', '545', '0', '976', '0', '0', '237.965', '3912.55', '66.0017', '1.16188', '120', '0', '0', '100', '0', NULL, NULL, NULL, NULL, '', NULL, '20886', -1),
(@CGUID+789, '60722', '860', '0', '0', '0', '0', '545', '0', '976', '0', '0', '247.878', '3937.65', '65.4195', '1.27846', '120', '0', '0', '100', '0', NULL, NULL, NULL, NULL, '', NULL, '20886', -1),
(@CGUID+790, '60722', '860', '0', '0', '0', '0', '545', '0', '976', '0', '0', '204.064', '3923.52', '60.409', '6.21836', '120', '0', '0', '100', '0', NULL, NULL, NULL, NULL, '', NULL, '20886', -1),
(@CGUID+791, '60722', '860', '0', '0', '0', '0', '545', '0', '976', '0', '0', '264.731', '3923.87', '69.1558', '2.3018', '120', '0', '0', '100', '0', NULL, NULL, NULL, NULL, '', NULL, '20886', -1),
(@CGUID+792, '60722', '860', '0', '0', '0', '0', '545', '0', '976', '0', '0', '200.071', '3930.87', '59.5958', '5.88259', '120', '0', '0', '100', '0', NULL, NULL, NULL, NULL, '', NULL, '20886', -1),
(@CGUID+793, '60722', '860', '0', '0', '0', '0', '545', '0', '976', '0', '0', '207.116', '3916.25', '61.5171', '6.21118', '120', '0', '0', '100', '0', NULL, NULL, NULL, NULL, '', NULL, '20886', -1),
(@CGUID+794, '60722', '860', '0', '0', '0', '0', '545', '0', '976', '0', '0', '246.576', '3941.56', '64.5067', '0.633349', '120', '0', '0', '100', '0', NULL, NULL, NULL, NULL, '', NULL, '20886', -1),
(@CGUID+795, '60722', '860', '0', '0', '0', '0', '545', '0', '976', '0', '0', '227.837', '3959.7', '60.4804', '1.13224', '120', '0', '0', '100', '0', NULL, NULL, NULL, NULL, '', NULL, '20886', -1),
(@CGUID+796, '60722', '860', '0', '0', '0', '0', '545', '0', '976', '0', '0', '242.587', '3909.47', '67.0764', '1.65851', '120', '0', '0', '100', '0', NULL, NULL, NULL, NULL, '', NULL, '20886', -1),
(@CGUID+797, '60722', '860', '0', '0', '0', '0', '545', '0', '976', '0', '0', '226.481', '3867.18', '70.161', '1.1512', '120', '0', '0', '100', '0', NULL, NULL, NULL, NULL, '', NULL, '20886', -1),
(@CGUID+798, '60722', '860', '0', '0', '0', '0', '545', '0', '976', '0', '0', '321.484', '3897.04', '78.261', '2.55027', '120', '0', '0', '100', '0', NULL, NULL, NULL, NULL, '', NULL, '20886', -1),
(@CGUID+799, '60722', '860', '0', '0', '0', '0', '545', '0', '976', '0', '0', '324.547', '3902.42', '77.9113', '3.52403', '120', '0', '0', '100', '0', NULL, NULL, NULL, NULL, '', NULL, '20886', -1),
(@CGUID+800, '60722', '860', '0', '0', '0', '0', '545', '0', '976', '0', '0', '269.861', '3831.57', '75.8999', '1.82062', '120', '0', '0', '100', '0', NULL, NULL, NULL, NULL, '', NULL, '20886', -1),
(@CGUID+801, '60722', '860', '0', '0', '0', '0', '545', '0', '976', '0', '0', '303.498', '3830.24', '77.2888', '2.50251', '120', '0', '0', '100', '0', NULL, NULL, NULL, NULL, '', NULL, '20886', -1),
(@CGUID+802, '60722', '860', '0', '0', '0', '0', '545', '0', '976', '0', '0', '308.156', '3830.97', '77.3845', '2.17751', '120', '0', '0', '100', '0', NULL, NULL, NULL, NULL, '', NULL, '20886', -1),
(@CGUID+803, '60722', '860', '0', '0', '0', '0', '545', '0', '976', '0', '0', '275.092', '3830.08', '76.1604', '2.24546', '120', '0', '0', '100', '0', NULL, NULL, NULL, NULL, '', NULL, '20886', -1),
(@CGUID+804, '60722', '860', '0', '0', '0', '0', '545', '0', '976', '0', '0', '333.399', '3868.55', '77.8553', '3.28074', '120', '0', '0', '100', '0', NULL, NULL, NULL, NULL, '', NULL, '20886', -1),
(@CGUID+805, '60722', '860', '0', '0', '0', '0', '545', '0', '976', '0', '0', '310.946', '3834.7', '77.6682', '1.80546', '120', '0', '0', '100', '0', NULL, NULL, NULL, NULL, '', NULL, '20886', -1);

DELETE FROM `creature_addon` WHERE `guid` IN (@CGUID+769,@CGUID+770);
INSERT INTO `creature_addon` (`guid`, `PathId`, `mount`, `MountCreatureID`, `StandState`, `VisFlags`, `SheathState`, `PvPFlags`, `emote`, 
`aiAnimKit`, `movementAnimKit`, `meleeAnimKit`, `visibilityDistanceType`, `auras`) VALUES 
(@CGUID+769, 0, 0, 0, 0, 0, 1, 0, 27, 0, 0, 0, 0, ''),
(@CGUID+770, 0, 0, 0, 0, 1, 1, 0, 510, 0, 0, 0, 0, '83305');


-- Loot tables
-- Delete only reference loot items (ItemType = 1) for specific NPCs
DELETE FROM creature_loot_template 
WHERE Entry IN (53705, 55946, 56172, 56174) 
AND ItemType = 1;

-- NPC: 53705 Pangolin (Beast)
INSERT INTO creature_loot_template (Entry, ItemType, Item, Chance, QuestRequired, LootMode, MinCount, MaxCount, COMMENT) VALUES
(53705, 1, 1, 100, 0, 1, 1, 1, 'General loot'),
(53705, 1, 2, 100, 0, 1, 1, 1, 'Beasts');

-- NPC: 55946 Pei-wu tiger (Beast) 
INSERT INTO creature_loot_template (Entry, ItemType, Item, Chance, QuestRequired, LootMode, MinCount, MaxCount, COMMENT) VALUES
(55946, 1, 1, 100, 0, 1, 1, 1, 'General loot'),
(55946, 1, 2, 100, 0, 1, 1, 1, 'Beasts');

-- NPC: 56172 Ash moth (Beast)
INSERT INTO creature_loot_template (Entry, ItemType, Item, Chance, QuestRequired, LootMode, MinCount, MaxCount, COMMENT) VALUES
(56172, 1, 1, 100, 0, 1, 1, 1, 'General loot');

-- NPC: 56174 Tiger cub (Beast)
INSERT INTO creature_loot_template (Entry, ItemType, Item, Chance, QuestRequired, LootMode, MinCount, MaxCount, COMMENT) VALUES
(56174, 1, 1, 100, 0, 1, 1, 1, 'General loot'),
(56174, 1, 2, 100, 0, 1, 1, 1, 'Beasts');

-- Delete existing reference loot for humanoid NPCs with scales
DELETE FROM creature_loot_template 
WHERE Entry IN (60858, 60780, 56360, 60722, 56007, 56008, 60685, 56009) 
AND ItemType = 1;

-- NPC: 60858 Deepscale Fleshripper (Humanoid with scales)
INSERT INTO creature_loot_template (Entry, ItemType, Item, Chance, QuestRequired, LootMode, MinCount, MaxCount, Comment) VALUES
(60858, 1, 1, 100, 0, 1, 1, 1, 'General loot'),
(60858, 1, 3, 100, 0, 1, 1, 1, 'Water related beasts'),
(60858, 1, 4, 100, 0, 1, 1, 1, 'Humanoids');

-- NPC: 60780 Deepscale Ravager (Humanoid with scales)
INSERT INTO creature_loot_template (Entry, ItemType, Item, Chance, QuestRequired, LootMode, MinCount, MaxCount, Comment) VALUES
(60780, 1, 1, 100, 0, 1, 1, 1, 'General loot'),
(60780, 1, 3, 100, 0, 1, 1, 1, 'Water related beasts'),
(60780, 1, 4, 100, 0, 1, 1, 1, 'Humanoids');

-- NPC: 56360 Deepscale Tormentor (Humanoid with scales)
INSERT INTO creature_loot_template (Entry, ItemType, Item, Chance, QuestRequired, LootMode, MinCount, MaxCount, Comment) VALUES
(56360, 1, 1, 100, 0, 1, 1, 1, 'General loot'),
(56360, 1, 3, 100, 0, 1, 1, 1, 'Water related beasts'),
(56360, 1, 4, 100, 0, 1, 1, 1, 'Humanoids');

-- NPC: 60722 Deepscale Tormentor (Humanoid with scales)
INSERT INTO creature_loot_template (Entry, ItemType, Item, Chance, QuestRequired, LootMode, MinCount, MaxCount, Comment) VALUES
(60722, 1, 1, 100, 0, 1, 1, 1, 'General loot'),
(60722, 1, 3, 100, 0, 1, 1, 1, 'Water related beasts'),
(60722, 1, 4, 100, 0, 1, 1, 1, 'Humanoids');

-- NPC: 56007 Darkened Horror (Humanoid with scales)
INSERT INTO creature_loot_template (Entry, ItemType, Item, Chance, QuestRequired, LootMode, MinCount, MaxCount, Comment) VALUES
(56007, 1, 1, 100, 0, 1, 1, 1, 'General loot'),
(56007, 1, 3, 100, 0, 1, 1, 1, 'Water related beasts'),
(56007, 1, 4, 100, 0, 1, 1, 1, 'Humanoids');

-- NPC: 56008 Darkened Terror (Humanoid with scales)
INSERT INTO creature_loot_template (Entry, ItemType, Item, Chance, QuestRequired, LootMode, MinCount, MaxCount, Comment) VALUES
(56008, 1, 1, 100, 0, 1, 1, 1, 'General loot'),
(56008, 1, 3, 100, 0, 1, 1, 1, 'Water related beasts'),
(56008, 1, 4, 100, 0, 1, 1, 1, 'Humanoids');

-- NPC: 60685 Deepscale Aggressor (Humanoid with scales)
INSERT INTO creature_loot_template (Entry, ItemType, Item, Chance, QuestRequired, LootMode, MinCount, MaxCount, Comment) VALUES
(60685, 1, 1, 100, 0, 1, 1, 1, 'General loot'),
(60685, 1, 3, 100, 0, 1, 1, 1, 'Water related beasts'),
(60685, 1, 4, 100, 0, 1, 1, 1, 'Humanoids');

-- NPC: 56009 Vordraka (Humanoid with scales)
INSERT INTO creature_loot_template (Entry, ItemType, Item, Chance, QuestRequired, LootMode, MinCount, MaxCount, Comment) VALUES
(56009, 1, 1, 100, 0, 1, 1, 1, 'General loot'),
(56009, 1, 3, 100, 0, 1, 1, 1, 'Water related beasts'),
(56009, 1, 4, 100, 0, 1, 1, 1, 'Humanoids');