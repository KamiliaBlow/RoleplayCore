-- Fix spell_script_names: remove stale spell IDs that no longer exist in Midnight 12.0.1 DBC
-- These rows cause "Scriptname: 'X' spell (Id: Y) does not exist" warnings at startup

-- ============================================================================
-- CLASS SPELLS — Warlock
-- ============================================================================
-- spell_warl_aftermath: spell -85113 removed (no replacement)
DELETE FROM spell_script_names WHERE spell_id = -85113 AND ScriptName = 'spell_warl_aftermath';
-- spell_warl_soul_swap: spell 86121 removed entirely
DELETE FROM spell_script_names WHERE spell_id = 86121 AND ScriptName = 'spell_warl_soul_swap';
-- spell_warl_soul_swap_override: spell 86211 removed entirely
DELETE FROM spell_script_names WHERE spell_id = 86211 AND ScriptName = 'spell_warl_soul_swap_override';
-- spell_warl_soul_swap_exhale: spell 86213 removed entirely
DELETE FROM spell_script_names WHERE spell_id = 86213 AND ScriptName = 'spell_warl_soul_swap_exhale';
-- spell_warl_demonbolt: spell 157695 removed (264178 still valid)
DELETE FROM spell_script_names WHERE spell_id = 157695 AND ScriptName = 'spell_warl_demonbolt';

-- ============================================================================
-- CLASS SPELLS — Warrior
-- ============================================================================
-- spell_warr_shout: spell 469 removed (6673 still valid)
DELETE FROM spell_script_names WHERE spell_id = 469 AND ScriptName = 'spell_warr_shout';
-- spell_warr_sweeping_strikes: spell 12328 removed (no replacement)
DELETE FROM spell_script_names WHERE spell_id = 12328 AND ScriptName = 'spell_warr_sweeping_strikes';
-- spell_warr_vigilance_redirect_threat: spell 59665 removed (no replacement)
DELETE FROM spell_script_names WHERE spell_id = 59665 AND ScriptName = 'spell_warr_vigilance_redirect_threat';
-- spell_warr_ravager: spell 152277 removed (228920 still valid)
DELETE FROM spell_script_names WHERE spell_id = 152277 AND ScriptName = 'spell_warr_ravager';
-- spell_warr_frothing_berserker: spell 215571 removed (no replacement)
DELETE FROM spell_script_names WHERE spell_id = 215571 AND ScriptName = 'spell_warr_frothing_berserker';

-- ============================================================================
-- CLASS SPELLS — Rogue
-- ============================================================================
-- spell_rog_nightstalker: spell 14062 removed (no replacement)
DELETE FROM spell_script_names WHERE spell_id = 14062 AND ScriptName = 'spell_rog_nightstalker';
-- spell_rog_honor_among_thieves: spell 51701 removed (no replacement)
DELETE FROM spell_script_names WHERE spell_id = 51701 AND ScriptName = 'spell_rog_honor_among_thieves';

-- ============================================================================
-- CLASS SPELLS — Priest
-- ============================================================================
-- spell_pri_shadow_protection: spell 27683 removed (no replacement)
DELETE FROM spell_script_names WHERE spell_id = 27683 AND ScriptName = 'spell_pri_shadow_protection';
-- spell_pri_void_tendrils: spell 108920 removed (no replacement)
DELETE FROM spell_script_names WHERE spell_id = 108920 AND ScriptName = 'spell_pri_void_tendrils';
-- spell_pri_power_word_solace: spell 129250 removed (no replacement)
DELETE FROM spell_script_names WHERE spell_id = 129250 AND ScriptName = 'spell_pri_power_word_solace';
-- spell_pri_mind_bomb: spell 205369 removed (no replacement)
DELETE FROM spell_script_names WHERE spell_id = 205369 AND ScriptName = 'spell_pri_mind_bomb';
-- spell_pri_whispering_shadows: spell 205385 removed (no replacement)
DELETE FROM spell_script_names WHERE spell_id = 205385 AND ScriptName = 'spell_pri_whispering_shadows';
-- spell_pri_whispering_shadows_effect: spell 391286 removed (no replacement)
DELETE FROM spell_script_names WHERE spell_id = 391286 AND ScriptName = 'spell_pri_whispering_shadows_effect';
-- spell_pri_atonement_effect_aura: spell 214206 removed (194384 still valid)
DELETE FROM spell_script_names WHERE spell_id = 214206 AND ScriptName = 'spell_pri_atonement_effect_aura';
-- spell_pri_evangelism: spell 246287 removed (no replacement)
DELETE FROM spell_script_names WHERE spell_id = 246287 AND ScriptName = 'spell_pri_evangelism';

-- ============================================================================
-- CLASS SPELLS — Paladin
-- ============================================================================
-- spell_pal_grand_crusader: spell -75806 removed (85043 still valid)
DELETE FROM spell_script_names WHERE spell_id = -75806 AND ScriptName = 'spell_pal_grand_crusader';
-- spell_pal_blessing_of_might: spell 19740 removed (no replacement)
DELETE FROM spell_script_names WHERE spell_id = 19740 AND ScriptName = 'spell_pal_blessing_of_might';
-- spell_pal_blessing_of_kings: spell 20217 removed (no replacement)
DELETE FROM spell_script_names WHERE spell_id = 20217 AND ScriptName = 'spell_pal_blessing_of_kings';

-- ============================================================================
-- CLASS SPELLS — Demon Hunter
-- ============================================================================
-- spell_dh_nemesis: spell 206491 removed (no replacement)
DELETE FROM spell_script_names WHERE spell_id = 206491 AND ScriptName = 'spell_dh_nemesis';
-- spell_dh_razor_spikes: spell 209400 removed (no replacement)
DELETE FROM spell_script_names WHERE spell_id = 209400 AND ScriptName = 'spell_dh_razor_spikes';
-- spell_dh_fracture: spell 209795 removed (no replacement)
DELETE FROM spell_script_names WHERE spell_id = 209795 AND ScriptName = 'spell_dh_fracture';

-- ============================================================================
-- CLASS SPELLS — Druid
-- ============================================================================
-- spell_dru_eclipse_dummy: spell 79577 removed (no replacement)
DELETE FROM spell_script_names WHERE spell_id = 79577 AND ScriptName = 'spell_dru_eclipse_dummy';
-- spell_dru_eclipse_ooc: spell 329910 removed (no replacement)
DELETE FROM spell_script_names WHERE spell_id = 329910 AND ScriptName = 'spell_dru_eclipse_ooc';
-- spell_dru_power_of_the_archdruid: spell 392303 removed (no replacement)
DELETE FROM spell_script_names WHERE spell_id = 392303 AND ScriptName = 'spell_dru_power_of_the_archdruid';

-- ============================================================================
-- CLASS SPELLS — Monk
-- ============================================================================
-- spell_monk_essence_font: spell 191837 removed (no replacement)
DELETE FROM spell_script_names WHERE spell_id = 191837 AND ScriptName = 'spell_monk_essence_font';
-- spell_monk_essence_font_heal: spell 191840 removed (no replacement)
DELETE FROM spell_script_names WHERE spell_id = 191840 AND ScriptName = 'spell_monk_essence_font_heal';

-- ============================================================================
-- CLASS SPELLS — Shaman
-- ============================================================================
-- spell_sha_voltaic_blaze_talent: spell 470053 removed (no replacement)
DELETE FROM spell_script_names WHERE spell_id = 470053 AND ScriptName = 'spell_sha_voltaic_blaze_talent';
-- spell_sha_voltaic_blaze_aura: spell 470058 removed (no replacement)
DELETE FROM spell_script_names WHERE spell_id = 470058 AND ScriptName = 'spell_sha_voltaic_blaze_aura';

-- ============================================================================
-- GENERIC SPELLS
-- ============================================================================
-- spell_gen_bandage: spell -746 removed (no replacement)
DELETE FROM spell_script_names WHERE spell_id = -746 AND ScriptName = 'spell_gen_bandage';
-- spell_gen_mixology_bonus: 4 stale IDs (many others still valid)
DELETE FROM spell_script_names WHERE spell_id IN (17629, 42735, 60345, 62380) AND ScriptName = 'spell_gen_mixology_bonus';
-- spell_gen_interrupt: spell 32748 removed (44835 still valid)
DELETE FROM spell_script_names WHERE spell_id = 32748 AND ScriptName = 'spell_gen_interrupt';
-- spell_gen_whisper_to_controller: spell 53374 removed (many others still valid)
DELETE FROM spell_script_names WHERE spell_id = 53374 AND ScriptName = 'spell_gen_whisper_to_controller';
-- spell_gen_replenishment: spell 61782 removed (57669 still valid)
DELETE FROM spell_script_names WHERE spell_id = 61782 AND ScriptName = 'spell_gen_replenishment';
-- spell_gen_profession_research: 3 stale IDs (60893 still valid)
DELETE FROM spell_script_names WHERE spell_id IN (61177, 61288, 61756) AND ScriptName = 'spell_gen_profession_research';
-- spell_gen_throw_shield: spell 73076 removed (41213, 43416, 69222 still valid)
DELETE FROM spell_script_names WHERE spell_id = 73076 AND ScriptName = 'spell_gen_throw_shield';
-- spell_gen_trigger_exclude_caster_aura_spell: 3 stale IDs (45438 still valid)
DELETE FROM spell_script_names WHERE spell_id IN (145158, 159607, 200153) AND ScriptName = 'spell_gen_trigger_exclude_caster_aura_spell';
-- spell_gen_trigger_exclude_target_aura_spell: 2 stale IDs (many others still valid)
DELETE FROM spell_script_names WHERE spell_id IN (126393, 198758) AND ScriptName = 'spell_gen_trigger_exclude_target_aura_spell';
-- spell_gen_50pct_count_pct_from_max_hp: 3 stale IDs (38441, 66316 still valid)
DELETE FROM spell_script_names WHERE spell_id IN (67100, 67101, 67102) AND ScriptName = 'spell_gen_50pct_count_pct_from_max_hp';

-- ============================================================================
-- ITEM SPELLS
-- ============================================================================
-- spell_item_piccolo_of_the_flaming_fire: spell 17512 removed (no replacement)
DELETE FROM spell_script_names WHERE spell_id = 17512 AND ScriptName = 'spell_item_piccolo_of_the_flaming_fire';
-- spell_item_mana_drain: spell 40336 removed (27522 still valid)
DELETE FROM spell_script_names WHERE spell_id = 40336 AND ScriptName = 'spell_item_mana_drain';

-- ============================================================================
-- INSTANCE SPELLS — Icecrown/Northrend
-- ============================================================================
-- spell_icecrown_through_the_eye_the_eye_of_the_lk: spell 25732 removed (no replacement)
DELETE FROM spell_script_names WHERE spell_id = 25732 AND ScriptName = 'spell_icecrown_through_the_eye_the_eye_of_the_lk';
-- spell_midsummer_test_ribbon_pole_channel: 2 stale IDs (29726 still valid)
DELETE FROM spell_script_names WHERE spell_id IN (29705, 29727) AND ScriptName = 'spell_midsummer_test_ribbon_pole_channel';

-- ============================================================================
-- INSTANCE SPELLS — Outland
-- ============================================================================
-- spell_broggok_poison_cloud: spells 30914, 38462 removed (no replacement)
DELETE FROM spell_script_names WHERE spell_id IN (30914, 38462) AND ScriptName = 'spell_broggok_poison_cloud';

-- ============================================================================
-- INSTANCE SPELLS — Utgarde Keep
-- ============================================================================
-- spell_uk_second_wind: spell 42770 removed (no replacement)
DELETE FROM spell_script_names WHERE spell_id = 42770 AND ScriptName = 'spell_uk_second_wind';

-- ============================================================================
-- INSTANCE SPELLS — Trial of the Crusader
-- ============================================================================
-- spell_mistress_kiss_area: 3 stale IDs (66336 still valid)
DELETE FROM spell_script_names WHERE spell_id IN (67076, 67077, 67078) AND ScriptName = 'spell_mistress_kiss_area';
-- spell_mistress_kiss: 3 stale IDs (66334 still valid)
DELETE FROM spell_script_names WHERE spell_id IN (67905, 67906, 67907) AND ScriptName = 'spell_mistress_kiss';
-- spell_valkyr_essences: 6 stale IDs (65684, 65686 still valid)
DELETE FROM spell_script_names WHERE spell_id IN (67176, 67177, 67178, 67222, 67223, 67224) AND ScriptName = 'spell_valkyr_essences';
-- spell_power_of_the_twins: 6 stale IDs (65879, 65916 still valid)
DELETE FROM spell_script_names WHERE spell_id IN (67244, 67245, 67246, 67248, 67249, 67250) AND ScriptName = 'spell_power_of_the_twins';
-- spell_powering_up: 3 stale IDs (67590 still valid)
DELETE FROM spell_script_names WHERE spell_id IN (67602, 67603, 67604) AND ScriptName = 'spell_powering_up';
-- spell_jormungars_burning_bile: 3 stale IDs (66870 still valid)
DELETE FROM spell_script_names WHERE spell_id IN (67621, 67622, 67623) AND ScriptName = 'spell_jormungars_burning_bile';
-- spell_anubarak_leeching_swarm: 3 stale IDs (66118 still valid)
DELETE FROM spell_script_names WHERE spell_id IN (67630, 68646, 68647) AND ScriptName = 'spell_anubarak_leeching_swarm';
-- spell_faction_champion_death_grip: 3 stale IDs (66017 still valid)
DELETE FROM spell_script_names WHERE spell_id IN (68753, 68754, 68755) AND ScriptName = 'spell_faction_champion_death_grip';
-- spell_faction_champion_dru_lifebloom: 3 stale IDs (66093 still valid)
DELETE FROM spell_script_names WHERE spell_id IN (67957, 67958, 67959) AND ScriptName = 'spell_faction_champion_dru_lifebloom';
-- spell_faction_champion_warl_unstable_affliction: 3 stale IDs (65812 still valid)
DELETE FROM spell_script_names WHERE spell_id IN (68154, 68155, 68156) AND ScriptName = 'spell_faction_champion_warl_unstable_affliction';

-- ============================================================================
-- INSTANCE SPELLS — Trial of the Champion
-- ============================================================================
-- spell_eadric_radiance: spell 67681 removed (66862 still valid)
DELETE FROM spell_script_names WHERE spell_id = 67681 AND ScriptName = 'spell_eadric_radiance';
-- spell_black_knight_ghoul_explode_risen_ghoul: spell 67889 removed (67754 still valid)
DELETE FROM spell_script_names WHERE spell_id = 67889 AND ScriptName = 'spell_black_knight_ghoul_explode_risen_ghoul';

-- ============================================================================
-- INSTANCE SPELLS — Vault of Archavon
-- ============================================================================
-- spell_koralon_meteor_fists_damage: 2 stale IDs (66765, 66809 still valid)
DELETE FROM spell_script_names WHERE spell_id IN (67331, 67333) AND ScriptName = 'spell_koralon_meteor_fists_damage';
-- spell_koralon_meteor_fists: spell 68161 removed (66725 still valid)
DELETE FROM spell_script_names WHERE spell_id = 68161 AND ScriptName = 'spell_koralon_meteor_fists';
-- spell_flame_warder_meteor_fists: spell 68160 removed (66808 still valid)
DELETE FROM spell_script_names WHERE spell_id = 68160 AND ScriptName = 'spell_flame_warder_meteor_fists';

-- ============================================================================
-- INSTANCE SPELLS — Frozen Halls
-- ============================================================================
-- spell_bronjahm_soulstorm_targeting: spell 69049 removed (68921 still valid)
DELETE FROM spell_script_names WHERE spell_id = 69049 AND ScriptName = 'spell_bronjahm_soulstorm_targeting';
-- spell_bronjahm_magic_bane: spell 69050 removed (68793 still valid)
DELETE FROM spell_script_names WHERE spell_id = 69050 AND ScriptName = 'spell_bronjahm_magic_bane';
-- spell_garfrost_permafrost: spell 70336 removed (68786 still valid)
DELETE FROM spell_script_names WHERE spell_id = 70336 AND ScriptName = 'spell_garfrost_permafrost';
-- spell_pos_ice_shards: spell 70827 removed (no replacement)
DELETE FROM spell_script_names WHERE spell_id = 70827 AND ScriptName = 'spell_pos_ice_shards';
-- spell_krick_pursuit_confusion: spell 70850 removed (69029 still valid)
DELETE FROM spell_script_names WHERE spell_id = 70850 AND ScriptName = 'spell_krick_pursuit_confusion';
-- spell_marwyn_shared_suffering: spell 72369 removed (72368 still valid)
DELETE FROM spell_script_names WHERE spell_id = 72369 AND ScriptName = 'spell_marwyn_shared_suffering';

-- ============================================================================
-- INSTANCE SPELLS — Ruby Sanctum
-- ============================================================================
-- spell_halion_blazing_aura: spell 75887 removed (75886 still valid)
DELETE FROM spell_script_names WHERE spell_id = 75887 AND ScriptName = 'spell_halion_blazing_aura';
-- spell_halion_twilight_cutter: 3 stale IDs (74769 still valid)
DELETE FROM spell_script_names WHERE spell_id IN (77844, 77845, 77846) AND ScriptName = 'spell_halion_twilight_cutter';

-- ============================================================================
-- INSTANCE SPELLS — Cataclysm
-- ============================================================================
-- spell_earthrager_ptah_flame_bolt: spell 89881 removed (75540 still valid)
DELETE FROM spell_script_names WHERE spell_id = 89881 AND ScriptName = 'spell_earthrager_ptah_flame_bolt';
-- spell_stalactite_mod_dest_height: 2 stale IDs (80643, 80647, 80654 still valid)
DELETE FROM spell_script_names WHERE spell_id IN (92309, 92653) AND ScriptName = 'spell_stalactite_mod_dest_height';
-- spell_elementium_spike_shield: spell 92429 removed (78835 still valid)
DELETE FROM spell_script_names WHERE spell_id = 92429 AND ScriptName = 'spell_elementium_spike_shield';
-- spell_argaloth_consuming_darkness: spell 95173 removed (88954 still valid)
DELETE FROM spell_script_names WHERE spell_id = 95173 AND ScriptName = 'spell_argaloth_consuming_darkness';
-- spell_occuthar_occuthars_destruction: spell 101009 removed (96942 still valid)
DELETE FROM spell_script_names WHERE spell_id = 101009 AND ScriptName = 'spell_occuthar_occuthars_destruction';
-- spell_alysrazor_fieroblast: 3 stale IDs (100094, 101223 still valid)
DELETE FROM spell_script_names WHERE spell_id IN (101294, 101295, 101296) AND ScriptName = 'spell_alysrazor_fieroblast';
