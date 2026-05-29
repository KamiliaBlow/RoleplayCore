-- Sentinel Hill changes and updates

-- NPC: 234 Gryan Stoutmantle
-- NPC: 491 Quartermaster Lewis
-- NPC: 523 Thor
-- NPC: 821 Captain Danuvin
-- NPC: 878 Scout Galiaan
-- NPC: 42384 Homeless Stormwind Citizen
-- NPC: 42386 Homeless Stormwind Citizen (Female
-- NPC: 42575 Hope Saldean
-- NPC: 42635 Ripsnarl
-- NPCL 42748 Ripsnarl Spawned

-- GO: 204005 Heavy Wooden Stocks
-- GO: 204099 Fire Low poly

-- Quest: 12 The People's militia
-- Quest: 13 The People's militia
-- Quest: 14 The People's militia
-- Quest: 102 Patrolling Westfall
-- Quest: 153 Red Leather Bandanas
-- Quest: 6181 A swift message
-- Quest: 6281 Continue to Stormwind
-- Quest: 26271 Feeding the hungry
-- Quest: 26286 In Defense of Westfall

-- Quest: 65 The Defias Brotherhood
-- Quest: 132 The Defias Brotherhood
-- Quest: 135 The Defias Brotherhood
-- Quest: 141 The Defias Brotherhood
-- Quest: 142 The Defias Brotherhood
-- Quest: 155 The Defias Brotherhood
-- Quest: 166 The Defias Brotherhood


-- Quest: 26271 Feeding the hungry
UPDATE `creature_template` SET `ScriptName` = 'npc_custom_westfall_stew_42617' WHERE (`entry` = 42617);

DELETE FROM `creature_addon` WHERE `guid` IN (275405,275407,275377,275376,275379,275378,275283,275259,275257,275258,275137,275096,
275094,274984,274925,274923,274921,274885,280419);
INSERT INTO `creature_addon` (`guid`, `PathId`, `mount`, `MountCreatureID`, `StandState`, `AnimTier`, `VisFlags`, `SheathState`, 
`PvPFlags`, `emote`, `aiAnimKit`, `movementAnimKit`, `meleeAnimKit`, `visibilityDistanceType`, `auras`) VALUES
(280419, 4238300, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, '79116'), -- Drifter with path (budy with cart)
(274885, 0, 0, 0, 1, 0, 0, 1, 0, 0, 0, 0, 0, 0, ''), -- Homeless Stormwind Citizen
(274921, 0, 0, 0, 1, 0, 0, 1, 0, 0, 0, 0, 0, 0, ''), -- Homeless Stormwind Citizen
(274923, 0, 0, 0, 1, 0, 0, 1, 0, 0, 0, 0, 0, 0, ''), -- Homeless Stormwind Citizen
(274925, 0, 0, 0, 1, 0, 0, 1, 0, 0, 0, 0, 0, 0, ''), -- Homeless Stormwind Citizen
(274984, 0, 0, 0, 1, 0, 0, 1, 0, 0, 0, 0, 0, 0, ''), -- Homeless Stormwind Citizen
(275094, 0, 0, 0, 1, 0, 0, 1, 0, 0, 0, 0, 0, 0, ''), -- Homeless Stormwind Citizen
(275096, 0, 0, 0, 1, 0, 0, 1, 0, 0, 0, 0, 0, 0, ''), -- Homeless Stormwind Citizen
(275405, 0, 0, 0, 1, 0, 0, 1, 0, 0, 0, 0, 0, 0, ''), -- Homeless Stormwind Citizen
(275137, 0, 0, 0, 1, 0, 0, 1, 0, 0, 0, 0, 0, 0, ''), -- Homeless Stormwind Citizen
(275257, 0, 0, 0, 1, 0, 0, 1, 0, 0, 0, 0, 0, 0, ''), -- Homeless Stormwind Citizen
(275258, 0, 0, 0, 1, 0, 0, 1, 0, 0, 0, 0, 0, 0, ''), -- Homeless Stormwind Citizen
(275259, 0, 0, 0, 1, 0, 0, 1, 0, 0, 0, 0, 0, 0, ''), -- Homeless Stormwind Citizen
(275283, 0, 0, 0, 1, 0, 0, 1, 0, 0, 0, 0, 0, 0, ''), -- Homeless Stormwind Citizen
(275407, 0, 0, 0, 1, 0, 0, 1, 0, 0, 0, 0, 0, 0, ''), -- Homeless Stormwind Citizen
(275376, 0, 0, 0, 1, 0, 0, 1, 0, 0, 0, 0, 0, 0, ''), -- Homeless Stormwind Citizen
(275377, 0, 0, 0, 1, 0, 0, 1, 0, 0, 0, 0, 0, 0, ''), -- Homeless Stormwind Citizen
(275379, 0, 0, 0, 1, 0, 0, 1, 0, 0, 0, 0, 0, 0, ''), -- Homeless Stormwind Citizen
(275378, 0, 0, 0, 1, 0, 0, 1, 0, 0, 0, 0, 0, 0, ''); -- Homeless Stormwind Citizen

-- Creature fixes
UPDATE `creature_template` SET `faction` = 2280 WHERE `entry` = 42400; -- wrong faction for drifters with cart

UPDATE `creature` SET `id` = 42383 WHERE `guid` = 280419 AND `id` = 42400; -- fix reverse id
UPDATE `creature` SET `id` = 42400 WHERE `guid` = 280420 AND `id` = 42383; -- fix reverse id

DELETE FROM `creature_formations` WHERE `leaderGUID` IN (280419);
INSERT INTO `creature_formations` (`leaderGUID`, `memberGUID`, `dist`, `angle`, `groupAI`, `point_1`, `point_2`) VALUES
(280419, 280419, 0, 0, 515, 0, 0), -- Drifter
(280419, 280420, 3, 270, 515, 8, 17); -- Drifter with cart

-- Waypoints
UPDATE `creature` SET `MovementType` = 2 WHERE `guid` = 280419; -- drifter with cart buddy

-- Hobos event at Sentinel Hill gates
UPDATE `creature` SET `ScriptName` = 'npc_custom_westfall_guard_42407' WHERE `guid` IN (275390,274919);

DELETE FROM `creature_template_difficulty` WHERE `Entry` IN (54373,54371,54372);
INSERT INTO `creature_template_difficulty` (`Entry`,`DifficultyID`,`LevelScalingDeltaMin`,`LevelScalingDeltaMax`,`ContentTuningID`,`HealthScalingExpansion`,`HealthModifier`,`ManaModifier`,`ArmorModifier`,`DamageModifier`,`CreatureDifficultyID`,`TypeFlags`,`TypeFlags2`,`TypeFlags3`,`LootID`,`PickPocketLootID`,`SkinLootID`,`GoldMin`,`GoldMax`,`StaticFlags1`,`StaticFlags2`,`StaticFlags3`,`StaticFlags4`,`StaticFlags5`,`StaticFlags6`,`StaticFlags7`,`StaticFlags8`,`VerifiedBuild`) VALUES 
(54373, 0, 0, 0, 6, 0, 0.7, 1, 1, 0.2, 31357, 0, 0, 0, 54373, 54373, 0, 200, 300, 0, 0, 0, 0, 0, 0, 0, 0, 64978), -- alternate Riverpaw Herbalist
(54372, 0, 0, 0, 6, 0, 1, 1, 1, 0.2, 31359, 0, 0, 0, 54372, 54372, 0, 200, 300, 0, 0, 0, 0, 0, 0, 0, 0, 64978), -- alternate Riverpaw Brute
(54371, 0, 0, 0, 6, 0, 1, 1, 1, 0.2, 31361, 0, 0, 0, 54371, 54371, 0, 200, 300, 0, 0, 0, 0, 0, 0, 0, 0, 64978); -- alternate Riverpaw Bandit

SET @CGUID := 900000;
DELETE FROM `creature` WHERE `guid` IN (@CGUID+818,@CGUID+819,@CGUID+820);
INSERT INTO `creature` VALUES
-- Horatio Laine and Stormwind Investigators for Dialogue with Gryan
(@CGUID+818, '42308', '0', '40', '108', '0', '0', '50006', '0', '-1', '0', '0', '-10507.5', '1049.1', '60.5186', '4.37166', '300', '0', '0', '100', '0', NULL, NULL, NULL, NULL, '', NULL, '0', '-1'),
(@CGUID+819, '42745', '0', '40', '108', '0', '0', '50006', '0', '-1', '0', '0', '-10504.6', '1049.27', '60.5181', '4.09412', '300', '0', '0', '100', '0', NULL, NULL, NULL, NULL, '', NULL, '0', '-1'),
(@CGUID+820, '42745', '0', '40', '108', '0', '0', '50006', '0', '-1', '0', '0', '-10508.6', '1051.57', '60.0498', '4.69275', '300', '0', '0', '100', '0', NULL, NULL, NULL, NULL, '', NULL, '0', '-1');

DELETE FROM `creature_addon` WHERE `guid` = @CGUID+818; -- Horatio Laine addon
INSERT INTO `creature_addon` (`guid`, `VisFlags`, `SheathState`, `auras`) VALUES (@CGUID+818, 0, 1, '');

-- Quest: 26286 In Defense of Westfall
UPDATE `creature_template` SET `ScriptName` = 'npc_custom_gryan_stoutmantle_234' WHERE (`entry` = 234);

-- Ripsnarl
UPDATE `creature_template` SET `ScriptName` = 'npc_custom_ripsnarl'  WHERE `entry` IN (42748,42635);
DELETE FROM `creature_template_addon` WHERE `entry` IN (42748,42635);
INSERT INTO `creature_template_addon` VALUES
(42635, 0, 0, 0, 0, 0, 0, 1, 0, 0, 676, 0, 0, 3, '69196'), 
(42748, 0, 0, 0, 0, 0, 0, 1, 0, 0, 676, 0, 0, 3, '69196');

-- Quest: 26322 Rise of the brotherhood
UPDATE `quest_template_addon` SET `ScriptName` = 'quest_26322_rise_of_the_brotherhood' WHERE ID = 26322;

UPDATE `creature_template` SET `ScriptName` = 'npc_gryan_stoutmantle_act2' WHERE `entry` IN (42750);
UPDATE `creature_template` SET `ScriptName` = 'npc_hope_act2' WHERE `entry` IN (42749);
UPDATE `creature_template` SET `ScriptName` = 'npc_defias_blackguard' WHERE (`entry` = 42769);
UPDATE `creature_template` SET `ScriptName` = 'npc_sentinel_hill_fire_trigger' WHERE (`entry` = 42793);

-- Spell Conditions
DELETE FROM `conditions` WHERE `SourceTypeOrReferenceId`=13 AND `SourceEntry` IN (79723);
INSERT INTO `conditions` (`SourceTypeOrReferenceId`, `SourceGroup`, `SourceEntry`, `SourceId`, `ElseGroup`, `ConditionTypeOrReference`, `ConditionTarget`, `ConditionValue1`, `ConditionValue2`, `ConditionValue3`, `NegativeCondition`, `ErrorType`, `ErrorTextId`, `ScriptName`, `Comment`) VALUES
-- Fix targets for Tie them up
(13, 1, 79723, 0, 0, 51, 0, 5, 42752, 0, 0, 0, 0, '', 'Tie them up target entry 42752'),
(13, 1, 79723, 0, 1, 51, 0, 5, 42751, 0, 0, 0, 0, '', 'Tie them up target entry 42751'),
(13, 1, 79723, 0, 2, 51, 0, 5, 42750, 0, 0, 0, 0, '', 'Tie them up target entry 42750'),
(13, 1, 79723, 0, 3, 51, 0, 5, 42745, 0, 0, 0, 0, '', 'Tie them up target entry 42745'),
(13, 1, 79723, 0, 4, 51, 0, 5, 42744, 0, 0, 0, 0, '', 'Tie them up target entry 42744');

-- Spell Target Positions
DELETE FROM `spell_target_position` WHERE `ID` IN (79664,79666,79667,79670);
INSERT INTO `spell_target_position` (`ID`, `EffectIndex`, `OrderIndex`, `MapID`, `PositionX`, `PositionY`, `Positionz`, `Orientation`, `VerifiedBuild`) VALUES
(79670, 0, 0, 0, -10514.8, 	1045.6,  60.8075, 0.48869, 0), -- Ripsnarl
(79664, 0, 0, 0, -10507.5, 	1049.1,  60.5186, 4.37166, 0), -- Horatio
(79666, 0, 0, 0, -10504.6, 	1049.27, 60.5181, 4.09412, 0), -- Investigator
(79667, 0, 0, 0, -10508.6, 	1051.57, 60.0498, 4.69275, 0); -- Investigator


DELETE FROM `creature` WHERE `guid` BETWEEN @CGUID+887 AND @CGUID+907;
INSERT INTO `creature` VALUES
(@CGUID+887, '42793', '0', '40', '108', '0', '0', '232', '0', '-1', '0', '0', '-10524.1', '1053.08', '58.7482', '0', '300', '3', '0', '100', '1', NULL, NULL, NULL, NULL, '', NULL, '0', '-1'),
(@CGUID+888, '42793', '0', '40', '108', '0', '0', '232', '0', '-1', '0', '0', '-10506.5', '1039.93', '68.2149', '0', '300', '3', '0', '100', '1', NULL, NULL, NULL, NULL, '', NULL, '0', '-1'),
(@CGUID+889, '42793', '0', '40', '108', '0', '0', '232', '0', '-1', '0', '0', '-10487.9', '1046.24', '66.0115', '0', '300', '3', '0', '100', '1', NULL, NULL, NULL, NULL, '', NULL, '0', '-1'),
(@CGUID+890, '42793', '0', '40', '108', '0', '0', '232', '0', '-1', '0', '0', '-10501.8', '1040', '64.7703', '0', '300', '3', '0', '100', '1', NULL, NULL, NULL, NULL, '', NULL, '0', '-1'),
(@CGUID+891, '42793', '0', '40', '108', '0', '0', '232', '0', '-1', '0', '0', '-10502.2', '1061.5', '58.7724', '0', '300', '3', '0', '100', '1', NULL, NULL, NULL, NULL, '', NULL, '0', '-1'),
(@CGUID+892, '42793', '0', '40', '108', '0', '0', '232', '0', '-1', '0', '0', '-10512.3', '1035.46', '62.6866', '0', '300', '3', '0', '100', '1', NULL, NULL, NULL, NULL, '', NULL, '0', '-1'),
(@CGUID+893, '42793', '0', '40', '108', '0', '0', '232', '0', '-1', '0', '0', '-10502.5', '1058.31', '59.8567', '0', '300', '3', '0', '100', '1', NULL, NULL, NULL, NULL, '', NULL, '0', '-1'),
(@CGUID+894, '42793', '0', '40', '108', '0', '0', '232', '0', '-1', '0', '0', '-10493.7', '1036.44', '73.9426', '0', '300', '3', '0', '100', '1', NULL, NULL, NULL, NULL, '', NULL, '0', '-1'),
(@CGUID+895, '42793', '0', '40', '108', '0', '0', '232', '0', '-1', '0', '0', '-10527.9', '1052.86', '60.6721', '0', '300', '3', '0', '100', '1', NULL, NULL, NULL, NULL, '', NULL, '0', '-1'),
(@CGUID+896, '42793', '0', '40', '108', '0', '0', '232', '0', '-1', '0', '0', '-10497.2', '1059.38', '65.9638', '0', '300', '3', '0', '100', '1', NULL, NULL, NULL, NULL, '', NULL, '0', '-1'),
(@CGUID+897, '42793', '0', '40', '108', '0', '0', '232', '0', '-1', '0', '0', '-10498.7', '1037.56', '77.8696', '0', '300', '3', '0', '100', '1', NULL, NULL, NULL, NULL, '', NULL, '0', '-1'),
(@CGUID+898, '42793', '0', '40', '108', '0', '0', '232', '0', '-1', '0', '0', '-10494.1', '1038.58', '65.2946', '0', '300', '3', '0', '100', '1', NULL, NULL, NULL, NULL, '', NULL, '0', '-1'),
(@CGUID+899, '42793', '0', '40', '108', '0', '0', '232', '0', '-1', '0', '0', '-10506.1', '1038.21', '71.9155', '0', '300', '3', '0', '100', '1', NULL, NULL, NULL, NULL, '', NULL, '0', '-1'),
(@CGUID+900, '42793', '0', '40', '108', '0', '0', '232', '0', '-1', '0', '0', '-10528.2', '1056.97', '69.4675', '0', '300', '3', '0', '100', '1', NULL, NULL, NULL, NULL, '', NULL, '0', '-1'),
(@CGUID+901, '42793', '0', '40', '108', '0', '0', '232', '0', '-1', '0', '0', '-10525.4', '1046.31', '65.0217', '0', '300', '3', '0', '100', '1', NULL, NULL, NULL, NULL, '', NULL, '0', '-1'),
(@CGUID+902, '42793', '0', '40', '108', '0', '0', '232', '0', '-1', '0', '0', '-10512.3', '1033.74', '68.4403', '0', '300', '3', '0', '100', '1', NULL, NULL, NULL, NULL, '', NULL, '0', '-1'),
(@CGUID+903, '42793', '0', '40', '108', '0', '0', '232', '0', '-1', '0', '0', '-10490.3', '1061.09', '57.2538', '0', '300', '3', '0', '100', '1', NULL, NULL, NULL, NULL, '', NULL, '0', '-1'),
(@CGUID+904, '42793', '0', '40', '108', '0', '0', '232', '0', '-1', '0', '0', '-10517', '1045.71', '65.7258', '0', '300', '3', '0', '100', '1', NULL, NULL, NULL, NULL, '', NULL, '0', '-1'),
(@CGUID+905, '42793', '0', '40', '108', '0', '0', '232', '0', '-1', '0', '0', '-10510.6', '1033.45', '76.6342', '0', '300', '3', '0', '100', '1', NULL, NULL, NULL, NULL, '', NULL, '0', '-1'),
(@CGUID+906, '42793', '0', '40', '108', '0', '0', '232', '0', '-1', '0', '0', '-10502.9', '1051.42', '64.4438', '0', '300', '3', '0', '100', '1', NULL, NULL, NULL, NULL, '', NULL, '0', '-1'),
(@CGUID+907, '42793', '0', '40', '108', '0', '0', '232', '0', '-1', '0', '0', '-10497.9', '1039.05', '69.8796', '0', '300', '3', '0', '100', '1', NULL, NULL, NULL, NULL, '', NULL, '0', '-1');

DELETE FROM `creature` WHERE `guid` IN (@CGUID+908,@CGUID+909);
INSERT INTO `creature` VALUES
-- Thor
(@CGUID+908, '110587', '0', '40', '108', '0', '0', '232', '0', '-1', '0', '0', '-10553.6', '1034.42', '57.0394', '0.0174533', '120', '0', '0', '100', '0', NULL, NULL, NULL, NULL, '', NULL, '0', '-1'),
(@CGUID+909, '110587', '0', '40', '108', '0', '0', '226', '0', '-1', '0', '0', '-10553.6', '1034.42', '57.0394', '0.0174533', '120', '0', '0', '100', '0', NULL, NULL, NULL, NULL, '', NULL, '0', '-1');

-- Heavy Wooden Stocks
SET @OGUID := 900000;
DELETE FROM `gameobject` WHERE `guid` = @OGUID+71;
INSERT INTO `gameobject` VALUES
(@OGUID+71, 204005, 0, 40, 108, 0, 0, 232, 0, -1, -10514.2, 1045.86, 60.518, 0.453785, 0, 0, 0.224951, 0.97437, 120, 255, 1, '', '', 0, -1, 256);

UPDATE `creature_template` SET `faction` = '35' WHERE entry IN (42769,42755,42753); -- fix faction since the original ones NOT working
UPDATE `creature_template` SET `faction` = '12', `npcflag` = '8195', `unit_flags` = '768', `unit_flags2` = '2048', `flags_extra` = '66' WHERE (`entry` = '110587');

DELETE FROM `creature_template_difficulty` WHERE `entry` = 110587;
INSERT INTO `creature_template_difficulty` VALUES
('110587', '0', '0', '0', '881', '10', '3', '3', '1', '4.6', '112698', '0', '0', '0', '0', '0', '0', '0', '0', '524288', '0', '0', '0', '0', '0', '0', '0', '0');
