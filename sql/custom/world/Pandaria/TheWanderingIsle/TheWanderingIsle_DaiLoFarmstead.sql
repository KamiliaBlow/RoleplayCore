-- Quest Templates
DELETE FROM `quest_template_addon` WHERE `ID` IN (29768, 29771, 29769, 29772, 29774, 29775);
INSERT INTO `quest_template_addon` (`ID`, `MaxLevel`, `AllowableClasses`, `SourceSpellID`, `PrevQuestID`, `NextQuestID`, `ExclusiveGroup`, `BreadcrumbForQuestId`, `RewardMailTemplateID`, `RewardMailDelay`, `RequiredSkillID`, `RequiredSkillPoints`, `RequiredMinRepFaction`, `RequiredMaxRepFaction`, `RequiredMinRepValue`, `RequiredMaxRepValue`, `ProvidedItemCount`, `SpecialFlags`, `ScriptName`) VALUES 
('29768', '0', '0', '0', '29769', '29772', '-29768', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', ''), -- Quest: 29768 Missing Mallet
('29771', '0', '0', '0', '29769', '29772', '-29768', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', ''), -- Quest: 29771 Stronger Than Wood
('29769', '0', '0', '0', '29680', '29768', '-29769', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', ''), -- Quest: 29769 Rascals
('29772', '0', '0', '0', '29768', '29774', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', ''), -- Quest: 29772 Raucous Rousing
('29774', '0', '0', '0', '29772', '29775', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', ''), -- Quest: 29774 Not In the Face!
('29775', '0', '0', '104017', '29774', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', ''); -- Quest: 29775 The Spirit and Body of Shen-zin Su

DELETE FROM `creature_queststarter` WHERE `quest` IN (29768, 29771);
INSERT INTO `creature_queststarter` (`id`, `quest`, `VerifiedBuild`) VALUES ('55477', '29768', '0'); -- Quest: 29768 Missing Mallet
INSERT INTO `creature_queststarter` (`id`, `quest`, `VerifiedBuild`) VALUES ('55478', '29771', '0'); -- Quest: 29771 Stronger Than Wood
DELETE FROM `creature_queststarter` WHERE `quest` IN (29772,29774,29775);
INSERT INTO `creature_queststarter` (`id`, `quest`, `VerifiedBuild`) VALUES ('55477', '29772', '0');
INSERT INTO `creature_queststarter` (`id`, `quest`, `VerifiedBuild`) VALUES ('55477', '29774', '0');
INSERT INTO `creature_queststarter` (`id`, `quest`, `VerifiedBuild`) VALUES ('55477', '29775', '0');

DELETE FROM `areatrigger_scripts` WHERE `entry` IN (7822);
INSERT INTO `areatrigger_scripts` (`entry`, `ScriptName`) VALUES
(7822, 'SmartTrigger');

DELETE FROM `smart_scripts` WHERE `entryorguid` IN (-451179) AND `source_type` IN (0, 9);
DELETE FROM `smart_scripts` WHERE `entryorguid` IN (7822) AND `source_type` IN (2);
INSERT INTO `smart_scripts` (`entryorguid`, `source_type`, `id`, `link`, `event_type`, `event_phase_mask`, `event_chance`, 
`event_flags`, `event_param1`, `event_param2`, `event_param3`, `event_param4`, `event_param5`, `action_type`, `action_param1`, 
`action_param2`, `action_param3`, `action_param4`, `action_param5`, `action_param6`, `target_type`, `target_param1`, `target_param2`, 
`target_param3`, `target_x`, `target_y`, `target_z`, `target_o`, `comment`) VALUES
-- Trigger 7822 - Cart - The Dai-Lo Farmstead
(7822, 2, 0, 0, 46, 0, 100, 0, 0, 0, 0, 0, 0, 86, 88811, 2, 10, 451179, 57712, 0, 7, 0, 0, 0, 0, 0, 0, 0, 'On Trigger - Cross Cast Area Trigger Dummy Timer Aura'),
-- Delivery Cart Tender
(-451179, 0, 0, 0, 31, 0, 100, 0, 88811, 0, 0, 0, 0, 1, 1, 0, 0, 0, 0, 0, 7, 0, 0, 0, 0, 0, 0, 0, 'Delivery Cart Tender - On Spellhit Target - Talk');

DELETE FROM `gossip_menu` WHERE `MenuID` IN (13140,14617);
INSERT INTO `gossip_menu` (`MenuID`, `TextID`, `VerifiedBuild`) VALUES 
('13140', '18503', '0'),
(14617, 20678, 0); -- Lamplighter Sunny


DELETE FROM `gossip_menu_option` WHERE `MenuID` = 13140 AND `GossipOptionID` = 18503;
INSERT INTO `gossip_menu_option` (`MenuID`, `GossipOptionID`, `OptionID`, `OptionNpc`, `OptionText`, `OptionBroadcastTextID`, `Language`, `Flags`, `SpellID`) VALUES 
('13140', '18503', '0', '0', 'Shu, can you wake up Wuguo for me?', '54025', '0', '1', 104017);

DELETE FROM `conditions` WHERE `SourceTypeOrReferenceId` = 15 AND `ConditionValue1` = 29774;
DELETE FROM `conditions` WHERE `SourceTypeOrReferenceId` = 13 AND `SourceEntry` IN (118030, 118033, 108830, 108831,104017,118036,118027,114453,114454);
INSERT INTO `conditions` VALUES
(15, 13140, 0, 0, 0, 47, 0, 29774, 8, 0, '', 0, 0, 0, '', 'Player for which gossip text is shown has quest 29774 in progress'),
(13, 1, 118030, 0, 1, 51, 0, 6, 0, 0, '', 0, 0, 0, '', 'Water Spout Burst target player'),
(13, 1, 118033, 0, 0, 51, 0, 5, 55556, 0, '', 0, 0, 0, '', 'Water Spout Laugh Trigger target Shu'),
(13, 1, 108830, 0, 0, 51, 0, 5, 55478, 0, '', 0, 0, 0, '', 'Summon Jojo Ironbrow target Jojo Ironbrow'),
(13, 1, 108831, 0, 0, 51, 0, 5, 57667, 0, '', 0, 0, 0, '', 'Jojo Headbash, Stack of Planks Impact target Stack of Planks'),
(13, 1, 104017, 0, 0, 51, 0, 5, 55556, 0, '', 0, 0, 0, '', 'Summon Spirit of Water target Shu'),
(13, 1, 118036, 0, 0, 51, 0, 5, 55539, 0, '', 0, 0, 0, '', 'Summon Spirit of Earth target Wugou'),
(13, 1, 118027, 0, 0, 51, 0, 5, 60916, 0, '', 0, 0, 0, '', 'Shu''s Water Splash target Wugou'),
(13, 1, 114453, 0, 0, 51, 0, 5, 59497, 0, '', 0, 0, 0, '', 'Summon Ox Cart, Farmlands -> Temple target Delivery Cart'),
(13, 1, 114454, 0, 0, 51, 0, 5, 59499, 0, '', 0, 0, 0, '', 'Summon Ox, Farmlands -> Temple target Nourished Yak');

DELETE FROM `creature_template_addon` WHERE `entry` IN (65473,55539, 55213, 55556, 64939,55478,57669,55506,56241,55483, 55504,55477,55558,60916,
57667,66941);
INSERT INTO `creature_template_addon` (`entry`, `PathId`, `mount`, `MountCreatureID`, `StandState`, `AnimTier`, `VisFlags`, `SheathState`, `PvPFlags`, `emote`, `aiAnimKit`, `movementAnimKit`, `meleeAnimKit`, `visibilityDistanceType`, `auras`) VALUES 
(55556, 0, 0, 0, 0, 0, 1, 1, 0, 0, 0, 0, 0, 0, '49414'), -- Shu at farmstead
(55558, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, '105890'), -- shu spawn
(60916, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, '105889 42386'), -- wugou spawn
(55477, 0, 0, 0, 0, 0, 1, 1, 0, 0, 0, 0, 0, 0, ''),
(55483, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, ''), 
(55504, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, ''), 
(55506, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, ''), 
(56241, 0, 0, 0, 0, 0, 0, 1, 0, 0, 1904, 0, 0, 0, ''),
(65473, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, ''),
(55213, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, '103245'),
(55539, 0, 0, 0, 0, 0, 1, 1, 0, 0, 0, 0, 0, 0, '80797 42386'),
('64939', '6493900', '0', '0', '0', '0', '0', '1', '0', '0', '0', '0', '0', '0', '126986'), -- Lamplighter Sunny
('55478', '0', '0', '0', '1', '0', '1', '1', '0', '461', '0', '0', '0', '0', '60921'), -- Jojo at farmstead
('57669', '5766900', '0', '0', '0', '0', '0', '1', '0', '0', '2935', '0', '0', '0', ''), -- Jojo spawned at farmstead
(57667, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, ''), -- Stack of planks
(66941, 0, 0, 0, 0, 0, 1, 1, 0, 0, 0, 0, 0, 0, '80797 116695'); -- Water Spout Bunny at farmstead -- not sure of the invis aura ????? 

DELETE FROM `creature_template_difficulty` WHERE `entry` IN (55556, 64939,55478,57669,55506,56241,55483, 55504,55477, 55213, 55539, 55558,60916,65473,
57667);
INSERT INTO `creature_template_difficulty` VALUES
(57667, 0, 0, 0, 80, 4, 1, 1, 1, 1, 26314, 1610612752, 6, 0, 0, 0, 0, 0, 0, 536871168, 0, 0, 0, 0, 0, 0, 0, 0), -- Stack of planks
(55556, 0, 0, 0, 80, 4, 1, 1, 1, 0.2, 29560, 0, 0, 0, 0, 0, 0, 0, 0, 268435456, 0, 0, 0, 0, 0, 0, 0, 56647), -- Shu at farmstead
(55558, 0, 0, 0, 80, 4, 1, 1, 1, 1, 29555, 0, 0, 0, 0, 0, 0, 0, 0, 268435456, 0, 0, 0, 0, 0, 0, 0, 56647),
(60916, 0, 0, 0, 80, 4, 1, 1, 1, 1, 20539, 0, 0, 0, 0, 0, 0, 0, 0, 268435456, 0, 0, 0, 0, 0, 0, 0, 56647),
(55213, 0, 0, 0, 80, 4, 1, 1, 1, 1, 30184, 0, 0, 0, 0, 0, 0, 0, 0, 268435456, 0, 0, 0, 0, 0, 0, 0, 56647), -- Shu follower
(55539, 0, 0, 0, 80, 4, 1, 1, 1, 0.2, 29594, 0, 0, 0, 0, 0, 0, 0, 0, 268435712, 0, 0, 0, 0, 0, 0, 0, 56647), -- Wugou at Farmstead
(55477, 0, 0, 0, 80, 4, 1, 1, 1, 0.2, 29691, 0, 0, 0, 0, 0, 0, 0, 0, 268435456, 0, 0, 0, 0, 0, 0, 0, 56647), -- Ji at farmstead
(55483, 0, 0, 0, 80, 4, 1, 1, 1, 0.2, 29679, 0, 0, 0, 55483, 0, 0, 7, 7, 268435456, 0, 0, 0, 0, 0, 0, 0, 56647), -- Plump Virmen
(55504, 0, 0, 0, 80, 4, 1, 1, 1, 0.2, 29636, 0, 0, 0, 55504, 0, 0, 7, 7, 268435456, 0, 0, 0, 0, 0, 0, 0, 56647), -- Plump Carrotcruncher
(55506, 0, 0, 0, 80, 4, 3, 1, 1, 0.2, 29633, 2147483648, 0, 0, 55506, 0, 0, 7, 7, 268435456, 0, 0, 0, 0, 0, 0, 0, 56647), -- Raggis
(56241, 0, 0, 0, 80, 4, 1, 1, 1, 0.2, 28345, 4096, 0, 0, 0, 0, 0, 0, 0, 268435456, 0, 0, 0, 0, 0, 0, 0, 56647), -- Dai-Lo Farmer
(65473, 0, 0, 0, 80, 4, 1, 1, 1, 0.2, 59133, 4096, 0, 0, 0, 0, 0, 0, 0, 268435456, 0, 0, 0, 0, 0, 0, 0, 56647), -- Dai-Lo Farmer
(55478, 0, 0, 0, 80, 4, 1, 1, 1, 0.2, 29689, 0, 0, 0, 0, 0, 0, 0, 0, 268435456, 0, 0, 0, 0, 0, 0, 0, 56647),
(57669, 0, 0, 0, 80, 4, 1, 1, 1, 1, 26310, 2097160, 0, 0, 0, 0, 0, 0, 0, 268435456, 0, 0, 0, 0, 0, 0, 0, 56647),
(64939, 0, 0, 0, 80, 4, 1, 1, 1, 0.2, 58335, 0, 0, 0, 0, 0, 0, 0, 0, 268435456, 0, 0, 0, 0, 0, 0, 0, 56647); -- Lamplighter Sunny

DELETE FROM `creature_addon` WHERE `guid` IN (451186, 451266, 451356, 451264, 451353, 451290, 451247, 451234, 451139, 451224, 451215, 
451271, 451248, 451169, 451134, 451272, 451296, 451288, 451354, 451269, 451214, 451212, 451143, 451308, 451331, 451312,451201);
INSERT INTO `creature_addon` VALUES
-- Field Yak
(451201, 5725800, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, ''),
-- Plump Vermin
(451186, 5548300, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, ''),
(451266, 5548301, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, ''),
(451356, 5548302, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, ''),
(451264, 5548303, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, ''),
(451353, 5548304, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, ''),
(451290, 5548305, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, ''),
(451247, 5548306, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, ''),
(451234, 5548307, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, ''),
(451139, 5548308, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, ''),
(451224, 5548309, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, ''),
(451215, 5548310, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, ''),
(451271, 5548311, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, ''),
(451248, 5548312, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, ''),
(451169, 5548313, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, ''),
(451134, 5548314, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, ''),
(451272, 5548315, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, ''),
(451296, 5548316, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, ''),
(451288, 5548317, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, ''),
(451354, 5548318, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, ''),
(451269, 5548319, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, ''),
(451214, 5548320, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, ''),
(451212, 5548321, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, ''),
(451143, 5548322, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, ''),
(451308, 5548323, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, ''),
(451331, 5548324, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, ''),
(451312, 5548325, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, '');

UPDATE `creature` SET `MovementType` = 2 WHERE `guid` IN (451186, 451266, 451356, 451264, 451353, 451290, 451247, 451234, 451139, 451224, 451215, 
451271, 451248, 451169, 451134, 451272, 451296, 451288, 451354, 451269, 451214, 451212, 451143, 451308, 451331, 451312,451201);

-- Ji second spawn
SET @CGUID := 900000;
DELETE FROM `creature` WHERE `guid` IN (@CGUID+750, @CGUID+806, @CGUID+807, @CGUID+808, @CGUID+809);
INSERT INTO `creature` (`guid`, `id`, `map`, `zoneId`, `areaId`, `spawnDifficulties`, `PhaseId`, `PhaseGroup`, `modelid`, `equipment_id`, `position_x`, `position_y`, `position_z`, `orientation`, `spawntimesecs`, `wander_distance`, 
`currentwaypoint`, `MovementType`, `npcflag`, `unit_flags`, `unit_flags2`, `unit_flags3`, `ScriptName`, `StringId`, `VerifiedBuild`, `size`) VALUES
(@CGUID+750, 55477, 860, 0, 0, 0, 169, 0, 0, 0, 629.354, 3139.15, 87.837, 0.558505, 120, 5, 0, 1, NULL, NULL, NULL, NULL, '', NULL, 0, -1); -- Ji at farmstead as there are 2 spawns with different invis aura

INSERT INTO `creature` VALUES
(@CGUID+806, '65473', '860', '0', '0', '0', '0', 169, '0', '-1', '0', '1', '733.453', '2886.23', '76.3088', '4.55946', '120', '0', '0', '100', '2', NULL, NULL, NULL, NULL, '', NULL, '20886', -1),
(@CGUID+807, '65473', '860', '0', '0', '0', '0', 169, '0', '-1', '0', '1', '736.863', '2888.66', '76.5924', '4.58896', '120', '0', '0', '100', '0', NULL, NULL, NULL, NULL, '', NULL, '20886', -1),
(@CGUID+808, '65473', '860', '0', '0', '0', '0', 169, '0', '-1', '0', '1', '760.439', '3315.01', '88.1761', '1.71042', '120', '0', '0', '100', '0', NULL, NULL, NULL, NULL, '', NULL, '20886', -1),
(@CGUID+809, '65473', '860', '0', '0', '0', '0', 169, '0', '-1', '0', '1', '667.608', '3279.48', '74.9715', '2.78173', '120', '0', '0', '100', '2', NULL, NULL, NULL, NULL, '', NULL, '20886', -1);


-- Lamp Lighter needs waypoint movement
UPDATE `creature` SET `zoneId` = '5736', `areaId` = '5881', `wander_distance` = '0', `MovementType` = '2' WHERE (`guid` = '451131');
UPDATE `gameobject` SET `PhaseId` = 1510 WHERE `id` = 214406; -- Mallet object is in phase 1510

-- Dai-Lo farmers with waypoints
UPDATE `creature` SET `MovementType` = 2 WHERE `guid` IN (451192,451297,451326,451187,451182,451125,451358,@CGUID+806,@CGUID+809);

DELETE FROM `creature_addon` WHERE `guid` IN (451158,451160,451170,451171,451173,451174,451178, 451166, @CGUID+750, 451128, 451126, 451154, 451281, 
451307, 451223, 451233, 451309, 451133, 451149, 451164, 451206, 451216, 451229, 451297, 451326, 451187, 451182, 451125, @CGUID+806,@CGUID+809,451192,451358,@CGUID+808);
INSERT INTO `creature_addon` (`guid`, `PathId`, `mount`, `StandState`, `AnimTier`, `VisFlags`, `SheathState`, `PvpFlags`, `emote`, `aiAnimKit`, `movementAnimKit`, `meleeAnimKit`, `visibilityDistanceType`, `auras`) VALUES
-- Dai-Lo Farmer
(451128, 0, 0, 0, 0, 0, 1, 0, 425, 0, 0, 0, 0, ''),
(451126, 0, 0, 0, 0, 0, 1, 0, 425, 0, 0, 0, 0, ''),
(451154, 0, 0, 0, 0, 0, 1, 0, 0, 1904, 0, 0, 0, ''),
(451281, 0, 0, 0, 0, 0, 1, 0, 425, 0, 0, 0, 0, ''),
(451307, 0, 0, 0, 0, 0, 1, 0, 425, 0, 0, 0, 0, ''),
(451223, 0, 0, 0, 0, 0, 1, 0, 425, 0, 0, 0, 0, ''),
(451233, 0, 0, 0, 0, 0, 1, 0, 425, 0, 0, 0, 0, ''),
(451309, 0, 0, 0, 0, 0, 1, 0, 425, 0, 0, 0, 0, ''),
(451133, 0, 0, 0, 0, 0, 1, 0, 425, 0, 0, 0, 0, ''),
(451149, 0, 0, 0, 0, 0, 1, 0, 425, 0, 0, 0, 0, ''),
(451164, 0, 0, 0, 0, 0, 1, 0, 425, 0, 0, 0, 0, ''),
(451206, 0, 0, 0, 0, 0, 1, 0, 425, 0, 0, 0, 0, ''),
(451216, 0, 0, 0, 0, 0, 1, 0, 425, 0, 0, 0, 0, ''),
(451229, 0, 0, 0, 0, 0, 1, 0, 425, 0, 0, 0, 0, ''),
(@CGUID+808, 0, 0, 0, 0, 0, 1, 0, 1, 0, 0, 0, 0, ''),

(451297, 6547300, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, ''), 
(451192, 6547301, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, ''), 
(451187, 5624100, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, ''), 
(451182, 5624101, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, ''), 
(451125, 5624102, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, ''), 
(@CGUID+806, 5624103, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, ''),
(451326, 6547302, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, ''),
(451358, 6547303, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, ''),
(@CGUID+809, 6547304, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, ''),

-- Ji at farmstead
(451166, '0', '0', '0', '0', '1', '1', '0', '0', '0', '0', '0', '0', '49415'),
(@CGUID+750, '0', '0', '0', '0', '1', '1', '0', '0', '0', '0', '0', '0', '85096'),
-- Jojo quest crowd
('451158', '0', '0', '0', '0', '1', '1', '0', '0', '0', '0', '0', '0', '82343'), -- farmstead jojo power quest
('451160', '0', '0', '0', '0', '1', '1', '0', '0', '0', '0', '0', '0', '82343'),
('451170', '0', '0', '0', '0', '1', '1', '0', '0', '0', '0', '0', '0', '82343'),
('451171', '0', '0', '0', '0', '1', '1', '0', '0', '0', '0', '0', '0', '82343'),
('451173', '0', '0', '0', '0', '1', '1', '0', '0', '0', '0', '0', '0', '82343'),
('451174', '0', '0', '0', '0', '1', '1', '0', '0', '0', '0', '0', '0', '82343'),
('451178', '0', '0', '0', '0', '1', '1', '0', '0', '0', '0', '0', '0', '82343');

DELETE FROM `creature_formations` WHERE `leaderGUID` IN (@CGUID+806, 451125,451187,451297);
INSERT INTO `creature_formations` (`leaderGUID`, `memberGUID`, `dist`, `angle`, `groupAI`, `point_1`, `point_2`) VALUES
(@CGUID+806, @CGUID+806, 0, 0, 515, 0, 0), -- Dai-Lo Farmer
(@CGUID+806, @CGUID+807, 3, 270, 515, 5, 16), -- Dai-Lo Farmer
(451297, 451297, 0, 0, 515, 0, 0), -- Dai-Lo Farmer
(451297, 451298, 3, 90, 515, 5, 14), -- Dai-Lo Farmer
(451187, 451187, 0, 0, 515, 0, 0), -- Dai-Lo Farmer
(451187, 451188, 3, 270, 515, 8, 17), -- Dai-Lo Farmer
(451125, 451125, 0, 0, 515, 0, 0), -- Dai-Lo Farmer
(451125, 451122, 3, 90, 515, 1, 5); -- Dai-Lo Farmer

UPDATE `creature_template` SET `AIName` = 'SmartAI' WHERE `Entry` IN (57669);
DELETE FROM `smart_scripts` WHERE `entryorguid` IN (57669,5766900) AND `source_type` IN (0,9);
INSERT INTO `smart_scripts` (`entryorguid`,`source_type`,`id`,`link`,`event_type`,`event_phase_mask`,`event_chance`,`event_flags`,`event_param1`,`event_param2`,`event_param3`,`event_param4`,`event_param5`,`event_param_string`,`action_type`,`action_param1`,`action_param2`,`action_param3`,`action_param4`,`action_param5`,`action_param6`,`target_type`,`target_param1`,`target_param2`,`target_param3`,`target_x`,`target_y`,`target_z`,`target_o`,`comment`) VALUES 
('57669', '0', '0', '0', '54', '0', '100', '0', '0', '0', '0', '0', '0', '', '80', '5766900', '0', '0', '0', '0', '0', '1', '0', '0', '0', '0', '0', '0', '0', 'Jojo Ironbrow - Just Spawned - Run Script'),
('57669', '0', '1', '0', '58', '0', '100', '0', '50', '6', '0', '0', '0', '', '41', '0', '0', '0', '0', '0', '0', '1', '0', '0', '0', '0', '0', '0', '0', 'Jojo Ironbrow - On WP Ended - Despawn'),
('5766900', '9', '0', '0', '0', '0', '100', '0', '1000', '1000', '1000', '1000', '0', '', '1', '0', '0', '0', '0', '0', '0', '1', '0', '0', '0', '0', '0', '0', '0', 'Jojo Ironbrow - Talk'),
('5766900', '9', '1', '0', '0', '0', '100', '0', '7000', '7000', '1000', '1000', '0', '', '128', '1', '1', '0', '0', '0', '0', '1', '0', '0', '0', '0', '0', '0', '0', 'Jojo Ironbrow - Set AI AnimKit'),
('5766900', '9', '2', '0', '0', '0', '100', '0', '0', '0', '1000', '1000', '0', '', '69', '1', '0', '0', '0', '0', '0', '8', '0', '0', '0', '599.215', '3132.27', '89.0657', '0', 'Jojo Ironbrow - Move to Pos'),
('5766900', '9', '3', '0', '0', '0', '100', '0', '5000', '5000', '1000', '1000', '0', '', '11', '129293', '0', '0', '0', '0', '0', '1', '0', '0', '0', '0', '0', '0', '0', 'Jojo Ironbrow - Cast Jojo Headbash, Planks Cast'),
('5766900', '9', '4', '0', '0', '0', '100', '0', '2500', '2500', '1000', '1000', '0', '', '1', '1', '0', '0', '0', '0', '0', '1', '0', '0', '0', '0', '0', '0', '0', 'Jojo Ironbrow - Talk'),
('5766900', '9', '5', '0', '0', '0', '100', '0', '100', '100', '0', '0', '0', '', '28', '108831', '0', '0', '0', '0', '0', '1', '0', '0', '0', '0', '0', '0', '0', 'Jojo Ironbrow - Remove Auras'),
('5766900', '9', '6', '0', '0', '0', '100', '0', '100', '100', '0', '0', '0', '', '28', '108831', '0', '0', '0', '0', '0', '18', '50', '0', '0', '0', '0', '0', '0', 'Jojo Ironbrow - Remove Player Auras'),
('5766900', '9', '7', '0', '0', '0', '100', '0', '0', '0', '1000', '1000', '0', '', '45', '1', '1', '0', '0', '0', '0', '11', '0', '10', '0', '0', '0', '0', '0', 'Jojo Ironbrow - Set Data'),
('5766900', '9', '8', '0', '0', '0', '100', '0', '8000', '8000', '1000', '1000', '0', '', '53', '0', '5766900', '0', '0', '0', '0', '1', '0', '0', '0', '0', '0', '0', '0', 'Jojo Ironbrow - Start WP');

UPDATE `creature_template` SET `AIName` = 'SmartAI' WHERE `Entry` IN (55506);
DELETE FROM `smart_scripts` WHERE `entryorguid` IN (55506) AND `source_type` = 0;
INSERT INTO `smart_scripts` (`entryorguid`,`source_type`,`id`,`link`,`event_type`,`event_phase_mask`,`event_chance`,`event_flags`,`event_param1`,`event_param2`,`event_param3`,`event_param4`,`event_param5`,`event_param_string`,`action_type`,`action_param1`,`action_param2`,`action_param3`,`action_param4`,`action_param5`,`action_param6`,`target_type`,`target_param1`,`target_param2`,`target_param3`,`target_x`,`target_y`,`target_z`,`target_o`,`comment`) VALUES 
('55506', '0', '0', '0', '0', '0', '100', '0', '8000', '12000', '35000', '40000', '0', '', '11', '125383', '0', '0', '0', '0', '0', '1', '0', '0', '0', '0', '0', '0', '0', 'Raggis - Update IC - Cast Burrow');

-- Quest: 29769 Rascals
-- Quest: 29770 Still Good!
DELETE FROM `creature_queststarter` WHERE `quest` IN (29769,29770);
INSERT INTO `creature_queststarter` (`id`, `quest`, `VerifiedBuild`) VALUES ('55477', '29769', '0');
INSERT INTO `creature_queststarter` (`id`, `quest`, `VerifiedBuild`) VALUES ('55479', '29770', '0');

UPDATE `creature_template` SET `AIName` = 'SmartAI' WHERE `Entry` IN (55477,55483,55504);
DELETE FROM `smart_scripts` WHERE `entryorguid` IN (-900750,55483,55504,90075000) AND `source_type` IN (0,9);
INSERT INTO `smart_scripts` (`entryorguid`,`source_type`,`id`,`link`,`event_type`,`event_phase_mask`,`event_chance`,`event_flags`,`event_param1`,`event_param2`,`event_param3`,`event_param4`,`event_param5`,`event_param_string`,`action_type`,`action_param1`,`action_param2`,`action_param3`,`action_param4`,`action_param5`,`action_param6`,`target_type`,`target_param1`,`target_param2`,`target_param3`,`target_x`,`target_y`,`target_z`,`target_o`,`comment`) VALUES 
('55483', '0', '0', '0', '4', '0', '25', '0', '0', '0', '0', '0', '0', '', '1', '0', '0', '0', '0', '0', '0', '1', '0', '0', '0', '0', '0', '0', '0', 'Plump Virmen - On Aggro - Talk'),
('55483', '0', '1', '0', '4', '0', '100', '0', '0', '0', '0', '0', '0', '', '30', '1', '2', '0', '0', '0', '0', '1', '0', '0', '0', '0', '0', '0', '0', 'Plump Virmen - On Aggro - Set Random Phase'),
('55483', '0', '2', '0', '0', '0', '100', '0', '2000', '5000', '5000', '8000', '0', '', '11', '107936', '0', '0', '0', '0', '0', '2', '0', '0', '0', '0', '0', '0', '0', 'Plump Virmen - Update IC - Cast Back Kick'),
('55483', '0', '3', '0', '0', '1', '100', '0', '5000', '8000', '8000', '12000', '0', '', '11', '107938', '0', '0', '0', '0', '0', '2', '0', '0', '0', '0', '0', '0', '0', 'Plump Virmen - Update IC - Cast Tail Thrash'),
('55483', '0', '4', '0', '0', '2', '100', '0', '5000', '8000', '15000', '18000', '0', '', '11', '107944', '0', '0', '0', '0', '0', '1', '0', '0', '0', '0', '0', '0', '0', 'Plump Virmen - Update IC - Cast Carrot Breath'),
('55504', '0', '0', '0', '4', '0', '25', '0', '0', '0', '0', '0', '0', '', '1', '0', '0', '0', '0', '0', '0', '1', '0', '0', '0', '0', '0', '0', '0', 'Plump Carrotcruncher - On Aggro - Talk'),
('55504', '0', '1', '0', '4', '0', '100', '0', '0', '0', '0', '0', '0', '', '30', '1', '2', '0', '0', '0', '0', '1', '0', '0', '0', '0', '0', '0', '0', 'Plump Carrotcruncher - On Aggro - Set Random Phase'),
('55504', '0', '2', '0', '0', '0', '100', '0', '2000', '5000', '5000', '8000', '0', '', '11', '107936', '0', '0', '0', '0', '0', '2', '0', '0', '0', '0', '0', '0', '0', 'Plump Carrotcruncher - Update IC - Cast Back Kick'),
('55504', '0', '3', '0', '0', '1', '100', '0', '5000', '8000', '8000', '12000', '0', '', '11', '107938', '0', '0', '0', '0', '0', '2', '0', '0', '0', '0', '0', '0', '0', 'Plump Carrotcruncher - Update IC - Cast Tail Thrash'),
('55504', '0', '4', '0', '0', '2', '100', '0', '5000', '8000', '15000', '20000', '0', '', '11', '107944', '0', '0', '0', '0', '0', '1', '0', '0', '0', '0', '0', '0', '0', 'Plump Carrotcruncher - Update IC - Cast Carrot Breath'),
('-900750', '0', '0', '0', '11', '0', '100', '0', '0', '0', '0', '0', '0', '', '59', '0', '0', '0', '0', '0', '0', '1', '0', '0', '0', '0', '0', '0', '0', 'Ji Firepaw - On Respawn - Set Run'),
('-900750', '0', '1', '0', '1', '0', '100', '0', '1000', '1000', '116000', '116000', '0', '', '80', '90075000', '0', '0', '0', '0', '0', '1', '0', '0', '0', '0', '0', '0', '0', 'Ji Firepaw - Update OOC - Run Script'),
('90075000', '9', '0', '0', '0', '0', '100', '0', '0', '0', '1000', '1000', '0', '', '1', '0', '0', '0', '0', '0', '0', '1', '0', '0', '0', '0', '0', '0', '0', 'Ji Firepaw - Talk'),
('90075000', '9', '1', '0', '0', '0', '100', '0', '16000', '16000', '1000', '1000', '0', '', '1', '1', '0', '0', '0', '0', '0', '1', '0', '0', '0', '0', '0', '0', '0', 'Ji Firepaw - Talk'),
('90075000', '9', '2', '0', '0', '0', '100', '0', '17000', '17000', '1000', '1000', '0', '', '1', '2', '0', '0', '0', '0', '0', '1', '0', '0', '0', '0', '0', '0', '0', 'Ji Firepaw - Talk'),
('90075000', '9', '3', '0', '0', '0', '100', '0', '18000', '18000', '1000', '1000', '0', '', '69', '1', '0', '0', '0', '0', '0', '8', '0', '0', '0', '628.734', '3138.05', '87.837', '0', 'Ji Firepaw - Move to Pos'),
('90075000', '9', '4', '0', '0', '0', '100', '0', '1000', '1000', '1000', '1000', '0', '', '66', '0', '0', '0', '0', '0', '0', '8', '0', '0', '0', '0', '0', '0', '6.21337', 'Ji Firepaw - Set Orientation'),
('90075000', '9', '5', '0', '0', '0', '100', '0', '0', '0', '1000', '1000', '0', '', '1', '3', '0', '0', '0', '0', '0', '1', '0', '0', '0', '0', '0', '0', '0', 'Ji Firepaw - Talk'),
('90075000', '9', '6', '0', '0', '0', '100', '0', '4000', '4000', '1000', '1000', '0', '', '11', '118026', '0', '0', '0', '0', '0', '1', '0', '0', '0', '0', '0', '0', '0', 'Ji Firepaw - Cast SpecialUnarmed w/ Gong Song'),
('90075000', '9', '7', '0', '0', '0', '100', '0', '3500', '3500', '1000', '1000', '0', '', '1', '4', '0', '0', '0', '0', '0', '1', '0', '0', '0', '0', '0', '0', '0', 'Ji Firepaw - Talk'),
('90075000', '9', '8', '0', '0', '0', '100', '0', '0', '0', '1000', '1000', '0', '', '69', '2', '0', '0', '0', '0', '0', '8', '0', '0', '0', '629.354', '3139.15', '87.837', '0', 'Ji Firepaw - Move to Pos'),
('90075000', '9', '9', '0', '0', '0', '100', '0', '1000', '1000', '1000', '1000', '0', '', '66', '0', '0', '0', '0', '0', '0', '8', '0', '0', '0', '0', '0', '0', '0.558505', 'Ji Firepaw - Set Orientation'),
('90075000', '9', '10', '0', '0', '0', '100', '0', '4000', '4000', '1000', '1000', '0', '', '1', '5', '0', '0', '0', '0', '0', '1', '0', '0', '0', '0', '0', '0', '0', 'Ji Firepaw - Talk'),
('90075000', '9', '11', '0', '0', '0', '100', '0', '0', '0', '1000', '1000', '0', '', '17', '510', '0', '0', '0', '0', '0', '1', '0', '0', '0', '0', '0', '0', '0', 'Ji Firepaw - Set Emote State'),
('90075000', '9', '12', '0', '0', '0', '100', '0', '1000', '1000', '1000', '1000', '0', '', '1', '6', '0', '0', '0', '0', '0', '1', '0', '0', '0', '0', '0', '0', '0', 'Ji Firepaw - Talk'),
('90075000', '9', '13', '0', '0', '0', '100', '0', '1000', '1000', '1000', '1000', '0', '', '1', '7', '0', '0', '0', '0', '0', '1', '0', '0', '0', '0', '0', '0', '0', 'Ji Firepaw - Talk'),
('90075000', '9', '14', '0', '0', '0', '100', '0', '1000', '1000', '1000', '1000', '0', '', '1', '8', '0', '0', '0', '0', '0', '1', '0', '0', '0', '0', '0', '0', '0', 'Ji Firepaw - Talk'),
('90075000', '9', '15', '0', '0', '0', '100', '0', '0', '0', '1000', '1000', '0', '', '17', '30', '0', '0', '0', '0', '0', '1', '0', '0', '0', '0', '0', '0', '0', 'Ji Firepaw - Set Emote State'),
('90075000', '9', '16', '0', '0', '0', '100', '0', '1000', '1000', '1000', '1000', '0', '', '1', '9', '0', '0', '0', '0', '0', '1', '0', '0', '0', '0', '0', '0', '0', 'Ji Firepaw - Talk'),
('90075000', '9', '17', '0', '0', '0', '100', '0', '20000', '20000', '1000', '1000', '0', '', '1', '10', '0', '0', '0', '0', '0', '1', '0', '0', '0', '0', '0', '0', '0', 'Ji Firepaw - Talk'),
('90075000', '9', '18', '0', '0', '0', '100', '0', '13000', '13000', '1000', '1000', '0', '', '5', '20', '0', '0', '0', '0', '0', '1', '0', '0', '0', '0', '0', '0', '0', 'Ji Firepaw - Play Emote');


DELETE FROM `conditions` 
WHERE SourceTypeOrReferenceId = 22
  AND SourceGroup = 2  -- id + 1 (id = 1 for CALL_SCRIPT)
  AND SourceEntry = -900750
  AND ConditionTypeOrReference = 29
  AND ConditionValue1 = 55539;

INSERT INTO `conditions` (SourceTypeOrReferenceId, SourceGroup, SourceEntry, SourceId, ElseGroup, ConditionTypeOrReference, ConditionTarget, ConditionValue1, ConditionValue2, ConditionValue3, NegativeCondition, COMMENT
) VALUES
(22, 2, -900750, 0, 0, 29, 1, 55539, 5, 0, 0, 'Second Ji: run SAI only Wugou is around');

-- Gong spell conditions
DELETE FROM `conditions` WHERE `SourceTypeOrReferenceId`=13 AND `SourceEntry` IN (104012,108830,108831);
INSERT INTO `conditions` (`SourceTypeOrReferenceId`, `SourceGroup`, `SourceEntry`, `SourceId`, `ElseGroup`, `ConditionTypeOrReference`, `ConditionTarget`, `ConditionValue1`, `ConditionValue2`, `ConditionValue3`, `ConditionStringValue1`, `NegativeCondition`, `ErrorType`, `ErrorTextId`, `ScriptName`, `Comment`) VALUES
(13, 1, 108830, 0, 0, 51, 0, 5, 55478, 0, '', 0, 0, 0, '', 'Summon Jojo Ironbrow target Jojo Ironbrow'),
(13, 1, 108831, 0, 0, 51, 0, 5, 57667, 0, '', 0, 0, 0, '', 'Jojo Headbash, Stack of Planks Impact target Stack of Planks'),
(13, 2, 104012, 0, 0, 51, 0, 5, 55539, 0, '', 0, 0, 0, '', 'Break Gong Credit target Wugou'),
(13, 4, 104012, 0, 1, 51, 0, 5, 55477, 0, '', 0, 0, 0, '', 'Break Gong Credit target Ji Firepaw'),
(13, 8, 104012, 0, 2, 51, 0, 8, 209626, 0, '', 0, 0, 0, '', 'Break Gong Credit target Break Gong');

UPDATE `creature_template` SET `unit_flags2` = 67110912, `BaseAttackTime` = 2000 WHERE `entry` = 57667; -- Stack of Planks
UPDATE `quest_template` SET `RewardSpell` = 108827 WHERE `ID` = 29771; -- Stronger than Wood reward spell

UPDATE `gameobject_template` SET `ScriptName` = 'go_209626_gong', `Data1` = '29772', `Data10` = '0' WHERE `Entry` = 209626; -- Data10 was 104012

UPDATE `creature_template` SET `ScriptName` = 'npc_shu_at_farmstead_pool', `AIName` = '' WHERE `Entry` = 55556;
UPDATE `creature_template` SET `ScriptName` = 'npc_shu_at_farmstead_play', `AIName` = '' WHERE `Entry` = 55558;

UPDATE `creature_template` SET `AIName` = 'SmartAI' WHERE `Entry` IN (55539);
DELETE FROM `smart_scripts` WHERE `entryorguid` IN (55539) AND `source_type` IN (0,9);
DELETE FROM `smart_scripts` WHERE `entryorguid` IN (-900750) AND `event_type` IN (38,61);
INSERT INTO `smart_scripts` (`entryorguid`,`source_type`,`id`,`link`,`event_type`,`event_phase_mask`,`event_chance`,`event_flags`,`event_param1`,`event_param2`,`event_param3`,`event_param4`,`event_param5`,`event_param_string`,`action_type`,`action_param1`,`action_param2`,`action_param3`,`action_param4`,`action_param5`,`action_param6`,`target_type`,`target_param1`,`target_param2`,`target_param3`,`target_x`,`target_y`,`target_z`,`target_o`,`comment`) VALUES 
('55539', '0', '0', '0', '38', '0', '100', '0', '1', '1', '1000', '1000', '0', '', '1', '0', '0', '0', '0', '0', '0', '1', '0', '0', '0', '0', '0', '0', '0', 'Wugou - On Data Set - Talk'),
('-900750', '0', '2', '3', '38', '0', '100', '0', '2', '2', '5000', '5000', '0', '', '1', '11', '0', '0', '0', '0', '0', '1', '0', '0', '0', '0', '0', '0', '0', 'Ji Firepaw - Talk'),
('-900750', '0', '3', '0', '61', '0', '100', '0', '0', '0', '0', '0', '0', '', '33', '55546', '0', '0', '0', '0', '0', '18', '20', '0', '0', '0', '0', '0', '0', 'Ji Firepaw - Quest Credit');

-- Water spout bunny
UPDATE `creature_template` SET `AIName` = 'SmartAI', `unit_flags` = '33554944', `unit_flags2` = '2048' WHERE (`entry` = '66941');

-- these flags disable gravity for the bunny so it can stay above water
UPDATE `creature_template_difficulty` SET `StaticFlags1` = '536870912' WHERE (`Entry` = '66941') and (`DifficultyID` = '0');

DELETE FROM `smart_scripts` WHERE `entryorguid` IN (66941,6694100); -- 15880 was the original model but not working
INSERT INTO `smart_scripts` VALUES -- we use model 21072 now taken from wowhead
('66941', '0', '0', '1', '0', '54', '0', '100', '0', '0', '0', '0', '0', '0', '', '3', '0', '21072', '0', '0', '0', '0', '0', '', '1', '0', '0', '0', '0', '', '0', '0', '0', '0', 'Water Spout - Just Spawned - Morph'),
('66941', '0', '1', '0', '0', '61', '0', '100', '0', '0', '0', '0', '0', '0', '', '80', '6694100', '2', '0', '0', '0', '0', '0', '', '1', '0', '0', '0', '0', '', '0', '0', '0', '0', 'Water Spout - Just Spawned - Run Script'),
('6694100', '9', '0', '0', '', '0', '0', '100', '0', '5000', '5000', '1000', '1000', '0', '', '11', '117057', '2', '0', '0', '0', '0', '0', NULL, '1', '0', '0', '0', '0', NULL, '0', '0', '0', '0', 'Water Spout - Cast Water Spout Geyser Aura'),
('6694100', '9', '1', '0', '', '0', '0', '100', '0', '0', '0', '1000', '1000', '0', '', '11', '116696', '2', '0', '0', '0', '0', '0', NULL, '1', '0', '0', '0', '0', NULL, '0', '0', '0', '0', 'Water Spout - Cast Water Spout Burst'),
('6694100', '9', '2', '0', '', '0', '0', '100', '0', '0', '0', '3000', '3000', '0', '', '28', '116695', '0', '0', '0', '0', '0', '0', NULL, '1', '0', '0', '0', '0', NULL, '0', '0', '0', '0', 'Water Spout - Remove Aura'),
('6694100', '9', '3', '0', '', '0', '0', '100', '0', '3000', '3000', '1000', '1000', '0', '', '41', '0', '0', '0', '0', '0', '0', '0', NULL, '1', '0', '0', '0', '0', NULL, '0', '0', '0', '0', 'Water Spout - Despawn');


UPDATE `creature_template` SET `AIName` = 'SmartAI' WHERE `Entry` IN (56241);
DELETE FROM `smart_scripts` WHERE `entryorguid` IN (56241) AND `source_type` IN (0,9);
INSERT INTO `smart_scripts` (entryorguid,    source_type,    id,    link,    event_type,    event_phase_mask,    event_chance,    event_flags,    event_param1,    event_param2,
    event_param3,    event_param4,    event_param5,    event_param_string,    action_type,    action_param1,    action_param2,    action_param3,
    action_param4,    action_param5,    action_param6,    target_type,    target_param1,    target_param2,    target_param3,    target_x,    target_y,
    target_z,    target_o,    comment) VALUES
(56241, 0, 0, 0, 0, 0, 100, 0, 2000, 5000, 5000, 8000, 0, '', 11, 128435, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 'Dai-Lo Farmer - Update IC - Cast Palm Strike'),
(56241, 0, 1, 0, 0, 0, 100, 0, 5000, 8000, 8000, 12000, 0, '', 11, 128436, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 'Dai-Lo Farmer - Update IC - Cast Round Kick');

-- Loot tables
-- Delete only reference loot items (ItemType = 1) for specific NPCs
DELETE FROM creature_loot_template 
WHERE Entry IN (55504,55483,55506) 
AND ItemType = 1;

INSERT INTO creature_loot_template (Entry, ItemType, Item, Chance, QuestRequired, LootMode, MinCount, MaxCount, Comment) VALUES
(55504, 1, 1, 100, 0, 1, 1, 1, 'General loot'),
(55504, 1, 4, 100, 0, 1, 1, 1, 'Humanoids'),
(55483, 1, 1, 100, 0, 1, 1, 1, 'General loot'),
(55483, 1, 4, 100, 0, 1, 1, 1, 'Humanoids'),
(55506, 1, 1, 100, 0, 1, 1, 1, 'General loot'),
(55506, 1, 4, 100, 0, 1, 1, 1, 'Humanoids');

DELETE FROM `npc_vendor` WHERE `entry` IN (57618);
INSERT INTO `npc_vendor` (entry, slot, item, maxcount, incrtime, type, VerifiedBuild) VALUES
(57618, 1, 2880, 0, 0, 1, 63906),
(57618, 2, 2901, 0, 0, 1, 63906),
(57618, 3, 3466, 0, 0, 1, 63906),
(57618, 4, 3857, 0, 0, 1, 63906),
(57618, 5, 5956, 0, 0, 1, 63906),
(57618, 6, 18567, 0, 0, 1, 63906),
(57618, 7, 160298, 0, 0, 1, 63906),
(57618, 8, 180733, 0, 0, 1, 63906);

-- Fixes

-- Fix missing model info for Dai-Lo farmer new models
DELETE FROM `creature_model_info` WHERE `DisplayID` IN (140145,140146,140147);
INSERT INTO `creature_model_info` (`DisplayID`, `BoundingRadius`, `CombatReach`, `DisplayID_Other_Gender`, `VerifiedBuild`) VALUES 
(140146, 0.347222, 1.5, 0, 66384),
(140145, 0.347222, 1.5, 0, 66384),
(140147, 0.347222, 1.5, 0, 66384);

DELETE FROM `creature_template_model` WHERE `CreatureID` IN (65473);
INSERT INTO `creature_template_model` (`CreatureID`, `Idx`, `CreatureDisplayID`, `DisplayScale`, `Probability`, `VerifiedBuild`) VALUES
(65473, 0, 44082, 1, 1, 66384),
(65473, 1, 44083, 1, 1, 66384),
(65473, 2, 140146, 1, 1, 66384),
(65473, 3, 140145, 1, 1, 66384),
(65473, 4, 140147, 1, 1, 66384);