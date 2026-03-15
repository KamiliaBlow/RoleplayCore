DELETE FROM `spell_script_names` WHERE `ScriptName` IN ('spell_warl_diabolic_ritual_passive', 'spell_warl_diabolic_ritual', 'spell_warl_demonic_art', 'spell_warl_pit_lord_felseeker', 'spell_warl_pit_lord_felseeker_at', 'spell_warl_mother_chaos_missile', 'spell_warl_overlord_wicked_cleave', 'spell_warl_infernal_bolt_empower_aura', 'spell_warl_overfiend_chaos_bolt', 'spell_warl_avatar_of_destruction', 'spell_warl_dimensional_rift_talent', 'spell_warl_ruination', 'spell_warl_ruination_damage', 'spell_warl_ruination_entry_aura', 'spell_warl_diabolic_oculi');
INSERT INTO `spell_script_names` (`spell_id`, `ScriptName`) VALUES
(428514, 'spell_warl_diabolic_ritual_passive'),
(431944, 'spell_warl_diabolic_ritual'),
(432815, 'spell_warl_diabolic_ritual'),
(432816, 'spell_warl_diabolic_ritual'),
(428524, 'spell_warl_demonic_art'),
(432794, 'spell_warl_demonic_art'),
(432795, 'spell_warl_demonic_art'),
(438973, 'spell_warl_pit_lord_felseeker'),
(434404, 'spell_warl_pit_lord_felseeker_at'),
(432596, 'spell_warl_mother_chaos_missile'),
(432113, 'spell_warl_overlord_wicked_cleave'),
(432120, 'spell_warl_overlord_wicked_cleave'),
(433891, 'spell_warl_infernal_bolt_empower_aura'),
(434589, 'spell_warl_overfiend_chaos_bolt'),
(1245089, 'spell_warl_avatar_of_destruction'),
(1280868, 'spell_warl_dimensional_rift_talent'),
(433885, 'spell_warl_ruination_entry_aura'),
(434635, 'spell_warl_ruination'),
(434636, 'spell_warl_ruination_damage'),
(1269643, 'spell_warl_diabolic_oculi');

DELETE FROM `spell_proc` WHERE `SpellId` IN (428514, 428524, 432794, 432795, 1245089, 1280868);
INSERT INTO `spell_proc` (`SpellId`, `SchoolMask`, `SpellFamilyName`, `SpellFamilyMask0`, `SpellFamilyMask1`, `SpellFamilyMask2`, `SpellFamilyMask3`, `ProcFlags`, `ProcFlags2`, `SpellTypeMask`, `SpellPhaseMask`, `HitMask`, `AttributesMask`, `DisableEffectsMask`, `ProcsPerMinute`, `Chance`, `Cooldown`, `Charges`) VALUES
(428514, 0, 5, 0, 0, 0, 0, 0x10000, 0, 1, 2, 0, 0, 0, 0, 100, 0, 0),
(428524, 0, 0, 0, 0, 0, 0, 0, 0x4, 0, 1, 0, 0, 0, 0, 100, 0, 0),
(432794, 0, 0, 0, 0, 0, 0, 0, 0x4, 0, 1, 0, 0, 0, 0, 100, 0, 0),
(432795, 0, 0, 0, 0, 0, 0, 0, 0x4, 0, 1, 0, 0, 0, 0, 100, 0, 0),
(1245089, 0, 5, 0, 0, 0, 0, 0x10000, 0, 1, 2, 0, 0, 0, 0, 100, 0, 0),
(1280868, 0, 5, 0, 0, 0, 0, 0x10000, 0, 1, 2, 0, 0, 0, 0, 10, 0, 0);

UPDATE `creature_template` SET `ScriptName` = 'npc_warl_diabolist_overlord' WHERE `entry` = 228575;
UPDATE `creature_template` SET `ScriptName` = 'npc_warl_diabolist_mother_of_chaos' WHERE `entry` = 228576;
UPDATE `creature_template` SET `ScriptName` = 'npc_warl_diabolist_pit_lord' WHERE `entry` = 228574;
UPDATE `creature_template` SET `ScriptName` = 'npc_warl_avatar_of_destruction_overfiend' WHERE `entry` = 217429;
UPDATE `creature_template` SET `ScriptName` = 'npc_warl_diabolic_imp' WHERE `entry` = 219161;

UPDATE `creature_template` SET `ScriptName` = 'npc_warl_dimensional_rift_unstable_tear' WHERE `entry` = 196280;
UPDATE `creature_template` SET `ScriptName` = 'npc_warl_dimensional_rift_chaos_tear' WHERE `entry` = 108493;
UPDATE `creature_template` SET `ScriptName` = 'npc_warl_dimensional_rift_shadowy_tear' WHERE `entry` = 99887;

