-- Westfall Changes and Updates

-- NPC: 42387 Thug
-- NPC: 42677 Moonbrook Thug

-- Quest: 26232 Lou's Parting Thoughts

-- Script for Lou's Parting Thoughts Quest
UPDATE `creature_template` SET `ScriptName` = 'npc_custom_lous_parting_thoughts_trigger' WHERE (`entry` = '42562');
UPDATE `creature_template` SET `ScriptName` = 'npc_custom_lous_parting_thoughts_thug' WHERE (`entry` = '42387');
DELETE FROM `creature` WHERE `ID` = 42387; -- thugs are spawned by script now

-- creature texts for the Thugs
DELETE FROM `creature_text` WHERE `CreatureID` IN (42387);
INSERT INTO creature_text (CreatureID, GroupID, ID, Text, Type, Language, Probability, Emote, Duration, Sound, SoundPlayType, BroadcastTextId, TextRange, comment) VALUES
('42387', '1', '0', 'Did you... Did you meet her?', '12', '0', '100', '6', '0', '0', '0', '42439', '0', 'Thugs'),
('42387', '2', '0', 'Whoa, what do we have here? Looks like we have ourselves an eavesdropper, boys.', '12', '0', '100', '5', '0', '0', '0', '42443', '0', 'Thugs'),
('42387', '3', '0', 'Yep. She\'s for real.', '12', '0', '100', '396', '0', '0', '0', '42440', '0', 'Thugs'),
('42387', '4', '0', 'She wanted for me to tell you lugs that she appreciates the job thet we did for her on the Furlbrows. Gave me a pile o\' gold to split with you all.', '12', '0', '100', '396', '0', '0', '0', '42441', '0', 'Thugs'),
('42387', '5', '0', 'See her face? Is it really...', '12', '0', '100', '6', '0', '0', '0', '42442', '0', 'Thugs'),
('42387', '6', '0', 'DIE!', '12', '0', '100', '6', '0', '0', '0', '42445', '0', 'Thugs'),
('42387', '0', '0', 'Only one thing to do with a lousy, good-for-nothin eavesdropper.', '12', '0', '100', '6', '0', '0', '0', '42444', '0', 'Thugs');

DELETE FROM `creature_text` WHERE `CreatureID` IN (42562);
INSERT INTO creature_text (CreatureID, GroupID, ID, Text, Type, Language, Probability, Emote, Duration, Sound, SoundPlayType, BroadcastTextId, TextRange, comment) VALUES
('42562', '0', '0', 'Hurry back to the Furlbrow\'s Cottage.', '12', '0', '100', '0', '0', '0', '0', '42439', '0', 'Thugs Trigger');
