DELETE FROM `spell_script_names` WHERE `ScriptName`='spell_pal_judgment_of_justice';
INSERT INTO `spell_script_names` (`spell_id`, `ScriptName`) VALUES
(20271, 'spell_pal_judgment_of_justice');

DELETE FROM `spell_script_names` WHERE `ScriptName`='spell_pal_expurgation';
INSERT INTO `spell_script_names` (`spell_id`, `ScriptName`) VALUES
(383344, 'spell_pal_expurgation');

DELETE FROM `spell_proc` WHERE `SpellId` IN (383344);
INSERT INTO `spell_proc` (`SpellId`,`SchoolMask`,`SpellFamilyName`,`SpellFamilyMask0`,`SpellFamilyMask1`,`SpellFamilyMask2`,`SpellFamilyMask3`,`ProcFlags`,`ProcFlags2`,`SpellTypeMask`,`SpellPhaseMask`,`HitMask`,`AttributesMask`,`DisableEffectsMask`,`ProcsPerMinute`,`Chance`,`Cooldown`,`Charges`) VALUES
(383344,0x00,10,0x00000000,0x00000000,0x00000000,0x40000000,0x0,0x0,0x1,0x2,0x0,0x0,0x0,0,0,0,0); -- Expurgation

DELETE FROM `spell_proc` WHERE `SpellId` IN (1260264);
INSERT INTO `spell_proc` (`SpellId`,`SchoolMask`,`SpellFamilyName`,`SpellFamilyMask0`,`SpellFamilyMask1`,`SpellFamilyMask2`,`SpellFamilyMask3`,`ProcFlags`,`ProcFlags2`,`SpellTypeMask`,`SpellPhaseMask`,`HitMask`,`AttributesMask`,`DisableEffectsMask`,`ProcsPerMinute`,`Chance`,`Cooldown`,`Charges`) VALUES
(1260264,0x00,5,0x00800001,0x00000000,0x00000000,0x00000000,0x0,0x0,0x1,0x2,0x0,0x0,0x0,0,0,0,0); -- Shard Instability

DELETE FROM `spell_proc` WHERE `SpellId` IN (1260269);
INSERT INTO `spell_proc` (`SpellId`,`SchoolMask`,`SpellFamilyName`,`SpellFamilyMask0`,`SpellFamilyMask1`,`SpellFamilyMask2`,`SpellFamilyMask3`,`ProcFlags`,`ProcFlags2`,`SpellTypeMask`,`SpellPhaseMask`,`HitMask`,`AttributesMask`,`DisableEffectsMask`,`ProcsPerMinute`,`Chance`,`Cooldown`,`Charges`) VALUES
(1260269,0x00,5,0x00000000,0x00000100,0x00000000,0x00000000,0x0,0x0,0x0,0x1,0x0,0x10,0x0,0,0,0,0); -- Shard Instability

DELETE FROM `spell_script_names` WHERE `ScriptName`='spell_warl_shard_instability';
INSERT INTO `spell_script_names` (`spell_id`, `ScriptName`) VALUES
(1260264, 'spell_warl_shard_instability');

DELETE FROM `spell_proc` WHERE `SpellId` IN (1259886);
INSERT INTO `spell_proc` (`SpellId`,`SchoolMask`,`SpellFamilyName`,`SpellFamilyMask0`,`SpellFamilyMask1`,`SpellFamilyMask2`,`SpellFamilyMask3`,`ProcFlags`,`ProcFlags2`,`SpellTypeMask`,`SpellPhaseMask`,`HitMask`,`AttributesMask`,`DisableEffectsMask`,`ProcsPerMinute`,`Chance`,`Cooldown`,`Charges`) VALUES
(1259886,0x00,5,0x00000000,0x00000110,0x00000000,0x00000000,0x0,0x0,0x4,0x1,0x0,0x0,0x0,0,0,0,0); -- Cull the Weak

DELETE FROM `spell_script_names` WHERE `ScriptName`='spell_warl_cull_the_weak';
INSERT INTO `spell_script_names` (`spell_id`, `ScriptName`) VALUES
(1259886, 'spell_warl_cull_the_weak');

DELETE FROM `spell_proc` WHERE `SpellId` IN (453172);
INSERT INTO `spell_proc` (`SpellId`,`SchoolMask`,`SpellFamilyName`,`SpellFamilyMask0`,`SpellFamilyMask1`,`SpellFamilyMask2`,`SpellFamilyMask3`,`ProcFlags`,`ProcFlags2`,`SpellTypeMask`,`SpellPhaseMask`,`HitMask`,`AttributesMask`,`DisableEffectsMask`,`ProcsPerMinute`,`Chance`,`Cooldown`,`Charges`) VALUES
(453172,0x00,5,0x00800001,0x00000000,0x00000000,0x00000000,0x0,0x0,0x1,0x2,0x0,0x0,0x0,0,0,0,0); -- Cunning Cruelty

DELETE FROM `spell_script_names` WHERE `ScriptName`='spell_warl_cunning_cruelty';
INSERT INTO `spell_script_names` (`spell_id`, `ScriptName`) VALUES
(453172, 'spell_warl_cunning_cruelty');

DELETE FROM `spell_script_names` WHERE `ScriptName`='spell_pal_holy_ritual';
INSERT INTO `spell_script_names` (`spell_id`, `ScriptName`) VALUES
(1022, 'spell_pal_holy_ritual'),
(6940, 'spell_pal_holy_ritual');

DELETE FROM `spell_script_names` WHERE `ScriptName`='spell_dk_vestigial_shell';
INSERT INTO `spell_script_names` (`spell_id`, `ScriptName`) VALUES
(48707, 'spell_dk_vestigial_shell');

DELETE FROM `spell_proc` WHERE `SpellId` IN (391546);
INSERT INTO `spell_proc` (`SpellId`,`SchoolMask`,`SpellFamilyName`,`SpellFamilyMask0`,`SpellFamilyMask1`,`SpellFamilyMask2`,`SpellFamilyMask3`,`ProcFlags`,`ProcFlags2`,`SpellTypeMask`,`SpellPhaseMask`,`HitMask`,`AttributesMask`,`DisableEffectsMask`,`ProcsPerMinute`,`Chance`,`Cooldown`,`Charges`) VALUES
(391546,0x00,15,0x00000000,0x00000000,0x00000000,0x08000000,0x0,0x0,0x4,0x2,0x0,0x0,0x0,0,0,0,0); -- March of Darkness