-- Fix invalid auras in creature_addon and creature_template_addon
--
-- Removes bad spell IDs from the space-separated `auras` field to eliminate
-- startup log errors from ObjectMgr::LoadCreatureAddons / LoadCreatureTemplateAddons.
--
-- Error categories fixed:
--   1) Temporary auras (spellInfo->GetDuration() > 0) -- 1459 creature_addon GUIDs, 279 template entries
--   2) Wrong/invalid spell IDs (unparseable or nonexistent) -- 43 creature_addon GUIDs, 31 template entries
--   3) SPELL_AURA_CONTROL_VEHICLE auras -- 93 creature_addon GUIDs, 40 template entries
--
-- Approach:
--   - Wrong/invalid spells: SET auras = '' (field contains corrupted data like semicolons, commas, text)
--   - Temporary/vehicle auras: TRIM(REPLACE(CONCAT(' ',auras,' '), ' SPELL ', ' ')) to surgically
--     remove specific spell IDs while preserving any valid auras in the same row
--   - All statements are idempotent (safe to re-run)

-- ============================================================================
-- Part 1a: Clear corrupted auras for 43 creature_addon GUIDs (wrong spell)
-- ============================================================================

UPDATE `creature_addon` SET `auras` = '' WHERE `guid` IN (73192, 77583, 126044, 126045, 126046, 126047, 126048, 313891, 314291, 386736, 1251841, 1251848, 1251850, 1251855, 1251857, 1251858, 3013891, 41519128, 41519129, 41519130, 41519132, 41519134, 41519135, 401519129, 3000040305, 3000040524, 3000040765, 3000041011, 3000041243, 3000041479, 3000041729, 3000041970, 3000042208, 3000042443, 3000042677, 3000042792, 3000043329, 3000104450, 3000104620, 3000104701, 3000104720, 3000104809, 3000104829);

-- ============================================================================
-- Part 1b: Clear corrupted auras for 31 creature_template_addon entries (wrong spell)
-- ============================================================================

UPDATE `creature_template_addon` SET `auras` = '' WHERE `entry` IN (636, 2434, 3804, 3806, 3807, 39994, 42308, 43119, 43122, 43125, 43126, 43127, 43128, 43129, 43130, 48505, 187082, 187335, 188444, 190103, 191039, 191043, 191656, 191818, 192049, 192491, 192596, 193001, 193113, 194437, 223987);

-- ============================================================================
-- Part 2: Remove temporary aura spells from creature_addon
-- 1459 unique GUIDs, 150 unique spell IDs
-- Uses TRIM(REPLACE(...)) to remove specific spell IDs from space-separated list
-- ============================================================================

UPDATE `creature_addon` SET `auras` = TRIM(REPLACE(CONCAT(' ', `auras`, ' '), ' 5672 ', ' ')) WHERE `guid` IN (3000100723, 3000100750, 3000100751) AND `auras` LIKE '%5672%';
UPDATE `creature_addon` SET `auras` = TRIM(REPLACE(CONCAT(' ', `auras`, ' '), ' 8599 ', ' ')) WHERE `guid` IN (376620, 377427) AND `auras` LIKE '%8599%';
UPDATE `creature_addon` SET `auras` = TRIM(REPLACE(CONCAT(' ', `auras`, ' '), ' 12544 ', ' ')) WHERE `guid` IN (351273, 351275, 351278, 351295, 351296, 351297, 351298, 354229, 354230, 354306, 354307) AND `auras` LIKE '%12544%';
UPDATE `creature_addon` SET `auras` = TRIM(REPLACE(CONCAT(' ', `auras`, ' '), ' 12550 ', ' ')) WHERE `guid` IN (354390, 354391, 354392, 354395, 354403, 354405, 354409, 356863, 356879, 356896, 356900, 356905, 356914, 356974, 357027, 357053, 357090, 357094, 357107, 389426, 391589, 391590, 391591, 391602, 391603, 391604, 391605, 391625, 391626, 391629, 391641, 391646, 391655, 393397, 393420, 393438, 394396, 395055, 850130) AND `auras` LIKE '%12550%';
UPDATE `creature_addon` SET `auras` = TRIM(REPLACE(CONCAT(' ', `auras`, ' '), ' 13326 ', ' ')) WHERE `guid` IN (256668) AND `auras` LIKE '%13326%';
UPDATE `creature_addon` SET `auras` = TRIM(REPLACE(CONCAT(' ', `auras`, ' '), ' 13787 ', ' ')) WHERE `guid` IN (393540, 394399) AND `auras` LIKE '%13787%';
UPDATE `creature_addon` SET `auras` = TRIM(REPLACE(CONCAT(' ', `auras`, ' '), ' 13864 ', ' ')) WHERE `guid` IN (352297, 393543, 394402, 395062) AND `auras` LIKE '%13864%';
UPDATE `creature_addon` SET `auras` = TRIM(REPLACE(CONCAT(' ', `auras`, ' '), ' 19231 ', ' ')) WHERE `guid` IN (452541, 452729) AND `auras` LIKE '%19231%';
UPDATE `creature_addon` SET `auras` = TRIM(REPLACE(CONCAT(' ', `auras`, ' '), ' 19514 ', ' ')) WHERE `guid` IN (352300, 352320, 352336, 352345, 352357, 352398, 352400, 352404, 352424, 352425, 352454, 352481, 352493, 352522, 352563, 352568, 352570, 352581, 352591, 352602, 352614, 352622, 352624, 352625, 352637, 352639, 352645, 352661, 352662, 352674, 352675, 352680, 352706, 352771, 352776, 352820, 352824, 352839, 352915, 352944, 352946, 352969, 352993, 353003, 353004, 353013, 353017, 353033, 353035, 353056, 353796, 353826, 353841, 353859, 353864, 353902, 385850, 385851, 385854, 385874, 385878, 385888, 385889, 385891, 385896, 385937, 385939, 385948, 385969, 385996, 385997, 386007, 386016, 386021, 386024, 386025, 386054, 386058, 386061, 386944, 386947, 387425, 387430) AND `auras` LIKE '%19514%';
UPDATE `creature_addon` SET `auras` = TRIM(REPLACE(CONCAT(' ', `auras`, ' '), ' 22011 ', ' ')) WHERE `guid` IN (850143, 3000104175) AND `auras` LIKE '%22011%';
UPDATE `creature_addon` SET `auras` = TRIM(REPLACE(CONCAT(' ', `auras`, ' '), ' 31765 ', ' ')) WHERE `guid` IN (350098, 356423, 382720) AND `auras` LIKE '%31765%';
UPDATE `creature_addon` SET `auras` = TRIM(REPLACE(CONCAT(' ', `auras`, ' '), ' 32064 ', ' ')) WHERE `guid` IN (357062, 357163, 377568, 377664, 379006, 379007, 379008, 380674, 380680, 380718, 380951, 380956, 380959, 380964, 380965, 380971, 380972, 380978, 380982, 380985, 380988, 380990, 380998, 380999, 381002, 381003, 381011, 381014, 381016, 381021, 381022, 381027, 381030, 381032, 381086, 381093, 383425, 383426, 383429, 383437, 383450, 383454, 383470, 383478, 383483, 383484, 383485, 383489, 383493, 383494, 383496, 383500, 383506, 383508, 383509, 383510, 383513, 383528, 383531, 383538, 383544, 383548, 383766, 383767, 383770, 383776, 383777, 383781, 383784, 383896, 383897, 383903, 383906, 383909, 383913, 383914) AND `auras` LIKE '%32064%';
UPDATE `creature_addon` SET `auras` = TRIM(REPLACE(CONCAT(' ', `auras`, ' '), ' 32734 ', ' ')) WHERE `guid` IN (352377, 353980, 353985) AND `auras` LIKE '%32734%';
UPDATE `creature_addon` SET `auras` = TRIM(REPLACE(CONCAT(' ', `auras`, ' '), ' 34098 ', ' ')) WHERE `guid` IN (254134) AND `auras` LIKE '%34098%';
UPDATE `creature_addon` SET `auras` = TRIM(REPLACE(CONCAT(' ', `auras`, ' '), ' 34664 ', ' ')) WHERE `guid` IN (259287) AND `auras` LIKE '%34664%';
UPDATE `creature_addon` SET `auras` = TRIM(REPLACE(CONCAT(' ', `auras`, ' '), ' 35247 ', ' ')) WHERE `guid` IN (353048) AND `auras` LIKE '%35247%';
UPDATE `creature_addon` SET `auras` = TRIM(REPLACE(CONCAT(' ', `auras`, ' '), ' 35915 ', ' ')) WHERE `guid` IN (378957) AND `auras` LIKE '%35915%';
UPDATE `creature_addon` SET `auras` = TRIM(REPLACE(CONCAT(' ', `auras`, ' '), ' 36513 ', ' ')) WHERE `guid` IN (356447) AND `auras` LIKE '%36513%';
UPDATE `creature_addon` SET `auras` = TRIM(REPLACE(CONCAT(' ', `auras`, ' '), ' 37985 ', ' ')) WHERE `guid` IN (378315, 378316, 378338, 378557, 378714, 383005) AND `auras` LIKE '%37985%';
UPDATE `creature_addon` SET `auras` = TRIM(REPLACE(CONCAT(' ', `auras`, ' '), ' 38316 ', ' ')) WHERE `guid` IN (3000174709, 3000174711, 3000174712, 3000174713, 3000174714, 3000174715, 3000174716, 3000174717, 3000174718, 3000174719, 3000174720, 3000174721, 3000174722, 3000174723, 3000174724, 3000174725, 3000174726, 3000174727, 3000174728) AND `auras` LIKE '%38316%';
UPDATE `creature_addon` SET `auras` = TRIM(REPLACE(CONCAT(' ', `auras`, ' '), ' 42345 ', ' ')) WHERE `guid` IN (391155, 391159, 391163, 391174, 391175, 391192, 391199, 391209, 391210, 391213, 391217, 391218) AND `auras` LIKE '%42345%';
UPDATE `creature_addon` SET `auras` = TRIM(REPLACE(CONCAT(' ', `auras`, ' '), ' 49676 ', ' ')) WHERE `guid` IN (350434, 351834, 351926, 353354, 353404, 353655, 355008, 355012, 355074, 355075) AND `auras` LIKE '%49676%';
UPDATE `creature_addon` SET `auras` = TRIM(REPLACE(CONCAT(' ', `auras`, ' '), ' 54580 ', ' ')) WHERE `guid` IN (377866) AND `auras` LIKE '%54580%';
UPDATE `creature_addon` SET `auras` = TRIM(REPLACE(CONCAT(' ', `auras`, ' '), ' 55059 ', ' ')) WHERE `guid` IN (126025) AND `auras` LIKE '%55059%';
UPDATE `creature_addon` SET `auras` = TRIM(REPLACE(CONCAT(' ', `auras`, ' '), ' 63227 ', ' ')) WHERE `guid` IN (352278, 352279, 352283, 352306, 352307, 352316, 352318, 352323, 352327, 352328, 352332, 352338, 352343, 352348, 352355, 352362, 352374, 352385, 352388, 352391, 352393, 352403, 352406, 352418, 352419, 352422, 352431, 352442, 352446, 352447, 352467, 352476, 352490, 352492, 352497, 352516, 352527, 352531, 352534, 352537, 352551, 352554, 352558, 352561, 352564, 352566, 352569, 352578, 352583, 352598, 352617, 352618, 352620, 352621, 352623, 352630, 352632, 352633, 352644, 352650, 352653, 352656, 352663, 352669, 352672, 352676, 352682, 352690, 352705, 352709, 352714, 352723, 352725, 352730, 352735, 352738, 352743, 352772, 352775, 352779, 352801, 352819, 352827, 352837, 352916, 353002, 353009, 353010, 353012, 353023, 353027, 353031, 353044, 353794, 353798, 353813, 353824, 353830, 353838, 353839, 353858, 353871, 353882, 353895, 353910, 353939, 353943, 353969, 353970, 353972, 353976, 353977, 353981, 353982, 353984, 381621, 381623, 381631, 382554, 382556, 382561, 382566, 382572, 382573, 382574, 382575, 382577, 382578, 382617, 382618, 382621, 382623, 382626, 382628, 382630, 383329, 384257, 384272, 384280, 384286, 384298, 384321, 384330, 384334, 384354, 384420, 384428, 384452, 384460, 384479, 384482, 384485, 384486, 384498, 384507, 384545, 384567, 384588, 384604, 384652) AND `auras` LIKE '%63227%';
UPDATE `creature_addon` SET `auras` = TRIM(REPLACE(CONCAT(' ', `auras`, ' '), ' 68227 ', ' ')) WHERE `guid` IN (389804) AND `auras` LIKE '%68227%';
UPDATE `creature_addon` SET `auras` = TRIM(REPLACE(CONCAT(' ', `auras`, ' '), ' 69942 ', ' ')) WHERE `guid` IN (200879) AND `auras` LIKE '%69942%';
UPDATE `creature_addon` SET `auras` = TRIM(REPLACE(CONCAT(' ', `auras`, ' '), ' 71200 ', ' ')) WHERE `guid` IN (256279, 256282, 256310) AND `auras` LIKE '%71200%';
UPDATE `creature_addon` SET `auras` = TRIM(REPLACE(CONCAT(' ', `auras`, ' '), ' 72381 ', ' ')) WHERE `guid` IN (391590, 391618, 391646, 393397, 393438) AND `auras` LIKE '%72381%';
UPDATE `creature_addon` SET `auras` = TRIM(REPLACE(CONCAT(' ', `auras`, ' '), ' 73561 ', ' ')) WHERE `guid` IN (389379, 389769) AND `auras` LIKE '%73561%';
UPDATE `creature_addon` SET `auras` = TRIM(REPLACE(CONCAT(' ', `auras`, ' '), ' 73852 ', ' ')) WHERE `guid` IN (394544, 394875) AND `auras` LIKE '%73852%';
UPDATE `creature_addon` SET `auras` = TRIM(REPLACE(CONCAT(' ', `auras`, ' '), ' 74005 ', ' ')) WHERE `guid` IN (394995) AND `auras` LIKE '%74005%';
UPDATE `creature_addon` SET `auras` = TRIM(REPLACE(CONCAT(' ', `auras`, ' '), ' 74813 ', ' ')) WHERE `guid` IN (384466) AND `auras` LIKE '%74813%';
UPDATE `creature_addon` SET `auras` = TRIM(REPLACE(CONCAT(' ', `auras`, ' '), ' 75580 ', ' ')) WHERE `guid` IN (385999) AND `auras` LIKE '%75580%';
UPDATE `creature_addon` SET `auras` = TRIM(REPLACE(CONCAT(' ', `auras`, ' '), ' 75808 ', ' ')) WHERE `guid` IN (255058, 255059, 255060) AND `auras` LIKE '%75808%';
UPDATE `creature_addon` SET `auras` = TRIM(REPLACE(CONCAT(' ', `auras`, ' '), ' 75968 ', ' ')) WHERE `guid` IN (394604) AND `auras` LIKE '%75968%';
UPDATE `creature_addon` SET `auras` = TRIM(REPLACE(CONCAT(' ', `auras`, ' '), ' 76406 ', ' ')) WHERE `guid` IN (255095) AND `auras` LIKE '%76406%';
UPDATE `creature_addon` SET `auras` = TRIM(REPLACE(CONCAT(' ', `auras`, ' '), ' 76410 ', ' ')) WHERE `guid` IN (255076, 255080, 255085, 255092) AND `auras` LIKE '%76410%';
UPDATE `creature_addon` SET `auras` = TRIM(REPLACE(CONCAT(' ', `auras`, ' '), ' 76604 ', ' ')) WHERE `guid` IN (255067) AND `auras` LIKE '%76604%';
UPDATE `creature_addon` SET `auras` = TRIM(REPLACE(CONCAT(' ', `auras`, ' '), ' 76625 ', ' ')) WHERE `guid` IN (255115, 255117, 255119, 255120) AND `auras` LIKE '%76625%';
UPDATE `creature_addon` SET `auras` = TRIM(REPLACE(CONCAT(' ', `auras`, ' '), ' 78273 ', ' ')) WHERE `guid` IN (251603, 389426, 391451, 393106) AND `auras` LIKE '%78273%';
UPDATE `creature_addon` SET `auras` = TRIM(REPLACE(CONCAT(' ', `auras`, ' '), ' 79764 ', ' ')) WHERE `guid` IN (354429, 354430) AND `auras` LIKE '%79764%';
UPDATE `creature_addon` SET `auras` = TRIM(REPLACE(CONCAT(' ', `auras`, ' '), ' 79800 ', ' ')) WHERE `guid` IN (354430) AND `auras` LIKE '%79800%';
UPDATE `creature_addon` SET `auras` = TRIM(REPLACE(CONCAT(' ', `auras`, ' '), ' 79833 ', ' ')) WHERE `guid` IN (387825) AND `auras` LIKE '%79833%';
UPDATE `creature_addon` SET `auras` = TRIM(REPLACE(CONCAT(' ', `auras`, ' '), ' 79849 ', ' ')) WHERE `guid` IN (350145, 351673, 351680, 351689, 351692, 351696, 351698, 351735, 351746, 351750, 351755, 351764, 351970, 351971, 351973, 351977, 351979, 351985, 356973, 357065, 382505, 452766, 452767) AND `auras` LIKE '%79849%';
UPDATE `creature_addon` SET `auras` = TRIM(REPLACE(CONCAT(' ', `auras`, ' '), ' 79865 ', ' ')) WHERE `guid` IN (351046, 377511, 377512, 377513, 377515, 377516, 377668, 377685, 377688, 377689, 377766, 377772, 377774, 377782, 377821, 377827, 377830, 377833, 377836, 377839, 377841, 377844) AND `auras` LIKE '%79865%';
UPDATE `creature_addon` SET `auras` = TRIM(REPLACE(CONCAT(' ', `auras`, ' '), ' 79877 ', ' ')) WHERE `guid` IN (259128, 352316, 352326, 352327, 352333, 352369, 352411, 352417, 352431, 352442, 352464, 352473, 352491, 352500, 352509, 352513, 352520, 352524, 352527, 352532, 352537, 352555, 352575, 352576, 352578, 352587, 352594, 352613, 352623, 352640, 352671, 352676, 352679, 352681, 352684, 352691, 352703, 352705, 352708, 352718, 352733, 352738, 352744, 353006, 353040, 353839, 353870, 353883, 353908, 353943, 353973, 353979, 353981, 379005, 379010, 380662, 380664, 380678, 380682, 380722, 380950, 380953, 380958, 380960, 380967, 380969, 380970, 380975, 380977, 380979, 380983, 380986, 380987, 380989, 380994, 381000, 381001, 381004, 381005, 381006, 381010, 381012, 381013, 381023, 381024, 381026, 381028, 381031, 381085, 381098, 383936, 383941) AND `auras` LIKE '%79877%';
UPDATE `creature_addon` SET `auras` = TRIM(REPLACE(CONCAT(' ', `auras`, ' '), ' 79892 ', ' ')) WHERE `guid` IN (352111, 352155, 352174, 352181, 352953, 353047, 353803, 353840, 353881, 379948, 379968, 380526, 381113, 382721) AND `auras` LIKE '%79892%';
UPDATE `creature_addon` SET `auras` = TRIM(REPLACE(CONCAT(' ', `auras`, ' '), ' 79927 ', ' ')) WHERE `guid` IN (352286, 356891, 356917, 357047, 357250, 357262, 379987, 383692, 383741, 383759) AND `auras` LIKE '%79927%';
UPDATE `creature_addon` SET `auras` = TRIM(REPLACE(CONCAT(' ', `auras`, ' '), ' 79934 ', ' ')) WHERE `guid` IN (350146, 356975, 357066) AND `auras` LIKE '%79934%';
UPDATE `creature_addon` SET `auras` = TRIM(REPLACE(CONCAT(' ', `auras`, ' '), ' 79949 ', ' ')) WHERE `guid` IN (254536, 350065, 350088, 354385, 354407, 354408, 354424, 356082, 356370, 356408, 356417, 356418, 356422, 356428, 356877, 356889, 356890, 356906, 356960, 357007, 357063, 357209, 357215, 357220, 357232, 357236, 357257, 357259, 377864, 377897, 379983, 379984, 379996, 380010, 380022, 380026, 380031) AND `auras` LIKE '%79949%';
UPDATE `creature_addon` SET `auras` = TRIM(REPLACE(CONCAT(' ', `auras`, ' '), ' 79968 ', ' ')) WHERE `guid` IN (650072, 650079, 650083, 650089, 650095, 650100, 650104, 650108, 650114, 650125) AND `auras` LIKE '%79968%';
UPDATE `creature_addon` SET `auras` = TRIM(REPLACE(CONCAT(' ', `auras`, ' '), ' 79977 ', ' ')) WHERE `guid` IN (352285, 352294, 352299, 352311, 352321, 352324, 352337, 352339, 352341, 352346, 352352, 352356, 352358, 352364, 352375, 352379, 352380, 352392, 352394, 352405, 352408, 352414, 352415, 352426, 352428, 352435, 352441, 352444, 352448, 352456, 352457, 352466, 352471, 352472, 352474, 352498, 352506, 352512, 352514, 352526, 352528, 352529, 352533, 352535, 352546, 352556, 352559, 352567, 352582, 352589, 352597, 352599, 352607, 352609, 352619, 352626, 352628, 352631, 352638, 352646, 352647, 352649, 352652, 352658, 352660, 352668, 352677, 352683, 352686, 352689, 352693, 352698, 352699, 352702, 352716, 352719, 352721, 352728, 352734, 352741, 352778, 352803, 352813, 352823, 352825, 352835, 352938, 352950, 352954, 352956, 352961, 352971, 352991, 352994, 353007, 353011, 353016, 353022, 353026, 353030, 353032, 353034, 353054, 353055, 353560, 353563, 353564, 353799, 353801, 353810, 353828, 353831, 353844, 353865, 353869, 353901, 353906, 353912, 353941) AND `auras` LIKE '%79977%';
UPDATE `creature_addon` SET `auras` = TRIM(REPLACE(CONCAT(' ', `auras`, ' '), ' 80586 ', ' ')) WHERE `guid` IN (353415, 353417, 353419, 353422, 355036, 355077, 355078, 355079, 355080, 355081, 355082, 355087, 355088) AND `auras` LIKE '%80586%';
UPDATE `creature_addon` SET `auras` = TRIM(REPLACE(CONCAT(' ', `auras`, ' '), ' 80660 ', ' ')) WHERE `guid` IN (351837, 351839, 351845, 351849, 351850, 351851, 351853, 351855, 351856, 351858, 351859, 351860, 351861, 351862, 351863, 351864, 351865, 351870, 351872, 351873, 351874, 351875, 351877, 351878, 351880, 351881, 351882, 353395, 353405, 353409, 353410, 353412, 353413, 353415, 353416, 353417, 353419, 353422, 353439, 353450, 353475, 355007, 355021, 355027, 355028, 355029, 355030, 355033, 355034, 355037, 355038, 355077, 355078, 355079, 355080, 355081, 355082, 355085, 355087, 355088, 355089, 355090, 355091, 355093, 355100, 355102, 355104, 355107, 355112, 355113, 355114, 355124, 355537, 355539, 355542, 355543, 355546, 355547, 355550, 355552, 355553, 355554, 355555, 355556, 355557) AND `auras` LIKE '%80660%';
UPDATE `creature_addon` SET `auras` = TRIM(REPLACE(CONCAT(' ', `auras`, ' '), ' 80983 ', ' ')) WHERE `guid` IN (353893, 383659, 383677, 383682, 383683, 383684, 383685) AND `auras` LIKE '%80983%';
UPDATE `creature_addon` SET `auras` = TRIM(REPLACE(CONCAT(' ', `auras`, ' '), ' 81352 ', ' ')) WHERE `guid` IN (354985, 354986, 355925, 355931, 355943, 355955, 356314, 356315, 356316, 356317, 356318, 356320, 356323, 356324, 356325, 356326, 356327, 356328, 356332, 356338, 356339, 356340, 356344, 356345, 356346, 356347, 356348, 356349, 356350, 356351, 356352, 356353, 356354, 356355, 356356, 356357, 356358, 356359, 356360, 356362, 356363, 356364, 356366, 356373, 356374, 356379, 356380, 356381, 356383, 356384) AND `auras` LIKE '%81352%';
UPDATE `creature_addon` SET `auras` = TRIM(REPLACE(CONCAT(' ', `auras`, ' '), ' 82458 ', ' ')) WHERE `guid` IN (353596, 353599, 353603, 353693) AND `auras` LIKE '%82458%';
UPDATE `creature_addon` SET `auras` = TRIM(REPLACE(CONCAT(' ', `auras`, ' '), ' 82727 ', ' ')) WHERE `guid` IN (356477, 356479, 356487, 356488, 356489, 356499, 356500, 356578, 356581, 356588, 356591, 356592, 356594, 356595, 356596, 356625, 356627, 356629, 356632, 356633, 356634, 356635, 356636, 356637, 356638, 356639, 356641, 356642, 356643, 356644, 356648, 356762) AND `auras` LIKE '%82727%';
UPDATE `creature_addon` SET `auras` = TRIM(REPLACE(CONCAT(' ', `auras`, ' '), ' 82740 ', ' ')) WHERE `guid` IN (356435) AND `auras` LIKE '%82740%';
UPDATE `creature_addon` SET `auras` = TRIM(REPLACE(CONCAT(' ', `auras`, ' '), ' 82741 ', ' ')) WHERE `guid` IN (356586, 356656, 356657, 356709, 356721, 356723, 356743, 356847, 356849, 356856) AND `auras` LIKE '%82741%';
UPDATE `creature_addon` SET `auras` = TRIM(REPLACE(CONCAT(' ', `auras`, ' '), ' 82758 ', ' ')) WHERE `guid` IN (356781, 356782) AND `auras` LIKE '%82758%';
UPDATE `creature_addon` SET `auras` = TRIM(REPLACE(CONCAT(' ', `auras`, ' '), ' 82777 ', ' ')) WHERE `guid` IN (254133) AND `auras` LIKE '%82777%';
UPDATE `creature_addon` SET `auras` = TRIM(REPLACE(CONCAT(' ', `auras`, ' '), ' 82836 ', ' ')) WHERE `guid` IN (258985, 258990, 258993, 258998, 259005, 259008, 259022, 259023, 259037, 259048, 259060, 259065, 259074, 259075, 259083, 259086, 259087, 259092, 259094, 259095, 382581, 382583, 382584, 382585, 382587, 382588, 382589, 382590, 382592, 382593, 382594, 382595, 382596, 382597, 382598, 382602, 382603, 382607, 382608, 382817, 382826, 383439, 383440, 383446, 383451, 383453, 383455, 383456, 383457, 383458, 383460, 383461, 383463, 383464, 383764, 383774) AND `auras` LIKE '%82836%';
UPDATE `creature_addon` SET `auras` = TRIM(REPLACE(CONCAT(' ', `auras`, ' '), ' 82840 ', ' ')) WHERE `guid` IN (350842, 356895, 357073, 357084) AND `auras` LIKE '%82840%';
UPDATE `creature_addon` SET `auras` = TRIM(REPLACE(CONCAT(' ', `auras`, ' '), ' 83126 ', ' ')) WHERE `guid` IN (255406) AND `auras` LIKE '%83126%';
UPDATE `creature_addon` SET `auras` = TRIM(REPLACE(CONCAT(' ', `auras`, ' '), ' 83718 ', ' ')) WHERE `guid` IN (254148) AND `auras` LIKE '%83718%';
UPDATE `creature_addon` SET `auras` = TRIM(REPLACE(CONCAT(' ', `auras`, ' '), ' 83885 ', ' ')) WHERE `guid` IN (100129210, 100129213, 100129214, 100129215, 100129216, 100129218, 100129219, 100129222, 100129255, 100129259, 100129260, 100129268, 100129269, 100129271, 100129274, 100129277, 100129278) AND `auras` LIKE '%83885%';
UPDATE `creature_addon` SET `auras` = TRIM(REPLACE(CONCAT(' ', `auras`, ' '), ' 84160 ', ' ')) WHERE `guid` IN (353709, 355184, 355196, 355201, 355239, 355240, 355332, 355620, 355627) AND `auras` LIKE '%84160%';
UPDATE `creature_addon` SET `auras` = TRIM(REPLACE(CONCAT(' ', `auras`, ' '), ' 84788 ', ' ')) WHERE `guid` IN (380119, 380144, 380155, 380180, 380200, 380202, 380207, 380225, 380230, 380260, 380307, 380308, 380318, 380344) AND `auras` LIKE '%84788%';
UPDATE `creature_addon` SET `auras` = TRIM(REPLACE(CONCAT(' ', `auras`, ' '), ' 84834 ', ' ')) WHERE `guid` IN (378112) AND `auras` LIKE '%84834%';
UPDATE `creature_addon` SET `auras` = TRIM(REPLACE(CONCAT(' ', `auras`, ' '), ' 85170 ', ' ')) WHERE `guid` IN (254870, 254871, 254872, 254873, 254875, 254877, 254878, 254879, 254881, 254884, 254885, 254886, 254888, 254891, 254892, 254895, 254902, 254903, 254908, 254910, 254912) AND `auras` LIKE '%85170%';
UPDATE `creature_addon` SET `auras` = TRIM(REPLACE(CONCAT(' ', `auras`, ' '), ' 85350 ', ' ')) WHERE `guid` IN (381819, 381853, 381861, 381863, 381886, 381887, 381888) AND `auras` LIKE '%85350%';
UPDATE `creature_addon` SET `auras` = TRIM(REPLACE(CONCAT(' ', `auras`, ' '), ' 85501 ', ' ')) WHERE `guid` IN (382129, 382212) AND `auras` LIKE '%85501%';
UPDATE `creature_addon` SET `auras` = TRIM(REPLACE(CONCAT(' ', `auras`, ' '), ' 85507 ', ' ')) WHERE `guid` IN (382064, 382066, 382069, 382073, 382076, 382079, 382080, 382084, 382127, 382132, 382134, 382136, 382137, 382138, 382140) AND `auras` LIKE '%85507%';
UPDATE `creature_addon` SET `auras` = TRIM(REPLACE(CONCAT(' ', `auras`, ' '), ' 86734 ', ' ')) WHERE `guid` IN (353495, 355115, 355549) AND `auras` LIKE '%86734%';
UPDATE `creature_addon` SET `auras` = TRIM(REPLACE(CONCAT(' ', `auras`, ' '), ' 86867 ', ' ')) WHERE `guid` IN (356870, 356935, 356979, 356986, 356996, 357005, 357038, 357042, 357080, 357093, 357148, 357150, 357153) AND `auras` LIKE '%86867%';
UPDATE `creature_addon` SET `auras` = TRIM(REPLACE(CONCAT(' ', `auras`, ' '), ' 88204 ', ' ')) WHERE `guid` IN (253989, 253992, 253994) AND `auras` LIKE '%88204%';
UPDATE `creature_addon` SET `auras` = TRIM(REPLACE(CONCAT(' ', `auras`, ' '), ' 88679 ', ' ')) WHERE `guid` IN (377666, 377673, 377788) AND `auras` LIKE '%88679%';
UPDATE `creature_addon` SET `auras` = TRIM(REPLACE(CONCAT(' ', `auras`, ' '), ' 88845 ', ' ')) WHERE `guid` IN (381152, 381678, 381679, 381681, 381691, 381708, 381725, 381728) AND `auras` LIKE '%88845%';
UPDATE `creature_addon` SET `auras` = TRIM(REPLACE(CONCAT(' ', `auras`, ' '), ' 89645 ', ' ')) WHERE `guid` IN (383436, 383443, 383448, 383707, 383762, 383768) AND `auras` LIKE '%89645%';
UPDATE `creature_addon` SET `auras` = TRIM(REPLACE(CONCAT(' ', `auras`, ' '), ' 90495 ', ' ')) WHERE `guid` IN (255017) AND `auras` LIKE '%90495%';
UPDATE `creature_addon` SET `auras` = TRIM(REPLACE(CONCAT(' ', `auras`, ' '), ' 93373 ', ' ')) WHERE `guid` IN (254060) AND `auras` LIKE '%93373%';
UPDATE `creature_addon` SET `auras` = TRIM(REPLACE(CONCAT(' ', `auras`, ' '), ' 93515 ', ' ')) WHERE `guid` IN (386006) AND `auras` LIKE '%93515%';
UPDATE `creature_addon` SET `auras` = TRIM(REPLACE(CONCAT(' ', `auras`, ' '), ' 93767 ', ' ')) WHERE `guid` IN (383313) AND `auras` LIKE '%93767%';
UPDATE `creature_addon` SET `auras` = TRIM(REPLACE(CONCAT(' ', `auras`, ' '), ' 93829 ', ' ')) WHERE `guid` IN (383415) AND `auras` LIKE '%93829%';
UPDATE `creature_addon` SET `auras` = TRIM(REPLACE(CONCAT(' ', `auras`, ' '), ' 100803 ', ' ')) WHERE `guid` IN (354122, 354186, 355693, 355696, 355697) AND `auras` LIKE '%100803%';
UPDATE `creature_addon` SET `auras` = TRIM(REPLACE(CONCAT(' ', `auras`, ' '), ' 100913 ', ' ')) WHERE `guid` IN (387563, 387717) AND `auras` LIKE '%100913%';
UPDATE `creature_addon` SET `auras` = TRIM(REPLACE(CONCAT(' ', `auras`, ' '), ' 105013 ', ' ')) WHERE `guid` IN (254509, 254510) AND `auras` LIKE '%105013%';
UPDATE `creature_addon` SET `auras` = TRIM(REPLACE(CONCAT(' ', `auras`, ' '), ' 106741 ', ' ')) WHERE `guid` IN (3000102005) AND `auras` LIKE '%106741%';
UPDATE `creature_addon` SET `auras` = TRIM(REPLACE(CONCAT(' ', `auras`, ' '), ' 134735 ', ' ')) WHERE `guid` IN (106950) AND `auras` LIKE '%134735%';
UPDATE `creature_addon` SET `auras` = TRIM(REPLACE(CONCAT(' ', `auras`, ' '), ' 174598 ', ' ')) WHERE `guid` IN (371385) AND `auras` LIKE '%174598%';
UPDATE `creature_addon` SET `auras` = TRIM(REPLACE(CONCAT(' ', `auras`, ' '), ' 181310 ', ' ')) WHERE `guid` IN (3000042208) AND `auras` LIKE '%181310%';
UPDATE `creature_addon` SET `auras` = TRIM(REPLACE(CONCAT(' ', `auras`, ' '), ' 182006 ', ' ')) WHERE `guid` IN (3000042443) AND `auras` LIKE '%182006%';
UPDATE `creature_addon` SET `auras` = TRIM(REPLACE(CONCAT(' ', `auras`, ' '), ' 182069 ', ' ')) WHERE `guid` IN (3000040305) AND `auras` LIKE '%182069%';
UPDATE `creature_addon` SET `auras` = TRIM(REPLACE(CONCAT(' ', `auras`, ' '), ' 188292 ', ' ')) WHERE `guid` IN (3000041011) AND `auras` LIKE '%188292%';
UPDATE `creature_addon` SET `auras` = TRIM(REPLACE(CONCAT(' ', `auras`, ' '), ' 198494 ', ' ')) WHERE `guid` IN (3000100775, 3000100814, 3000104520, 3000104521, 3000104525, 3000104526, 3000104528, 3000104531, 3000104532, 3000104533, 3000104534, 3000104536, 3000104537, 3000104541, 3000104542, 3000104566, 3000104572, 3000104575, 3000104579, 3000104580, 3000104581, 3000104582, 3000104583, 3000104604, 3000104621, 3000104623, 3000104624, 3000104625, 3000104653, 3000104659, 3000104677, 3000104682, 3000104689, 3000104691, 3000104693, 3000104694, 3000104700, 3000104703, 3000104704, 3000104705, 3000104709, 3000104713, 3000104714, 3000104719, 3000104721, 3000104722, 3000104723, 3000104725, 3000104727, 3000104728, 3000104729, 3000104730, 3000104732, 3000104734, 3000104735, 3000104736, 3000104782, 3000104783, 3000104784, 3000104786, 3000104793, 3000104801, 3000104802, 3000104803, 3000104810, 3000104812, 3000104813, 3000104814, 3000104817, 3000104818, 3000104819, 3000104820, 3000104822, 3000104823, 3000104824, 3000104825, 3000104826, 3000104827, 3000104828, 3000104830, 3000104832, 3000104833, 3000104836, 3000104837, 3000104838, 3000104840, 3000104842, 3000104843, 3000104844, 3000104845, 3000104846) AND `auras` LIKE '%198494%';
UPDATE `creature_addon` SET `auras` = TRIM(REPLACE(CONCAT(' ', `auras`, ' '), ' 211762 ', ' ')) WHERE `guid` IN (12898689) AND `auras` LIKE '%211762%';
UPDATE `creature_addon` SET `auras` = TRIM(REPLACE(CONCAT(' ', `auras`, ' '), ' 213503 ', ' ')) WHERE `guid` IN (453609) AND `auras` LIKE '%213503%';
UPDATE `creature_addon` SET `auras` = TRIM(REPLACE(CONCAT(' ', `auras`, ' '), ' 217400 ', ' ')) WHERE `guid` IN (3000041524) AND `auras` LIKE '%217400%';
UPDATE `creature_addon` SET `auras` = TRIM(REPLACE(CONCAT(' ', `auras`, ' '), ' 245570 ', ' ')) WHERE `guid` IN (12899904, 12899905, 12899906, 12899907, 12899908, 12899909, 12899910, 12899911) AND `auras` LIKE '%245570%';
UPDATE `creature_addon` SET `auras` = TRIM(REPLACE(CONCAT(' ', `auras`, ' '), ' 246141 ', ' ')) WHERE `guid` IN (650130) AND `auras` LIKE '%246141%';
UPDATE `creature_addon` SET `auras` = TRIM(REPLACE(CONCAT(' ', `auras`, ' '), ' 247963 ', ' ')) WHERE `guid` IN (12898272, 12898275, 12898279, 12898280, 12898284, 12898285, 12898290, 12898291, 12898293, 12898294, 12898298, 12898420) AND `auras` LIKE '%247963%';
UPDATE `creature_addon` SET `auras` = TRIM(REPLACE(CONCAT(' ', `auras`, ' '), ' 247987 ', ' ')) WHERE `guid` IN (12899821, 12899826, 12899844, 12899846) AND `auras` LIKE '%247987%';
UPDATE `creature_addon` SET `auras` = TRIM(REPLACE(CONCAT(' ', `auras`, ' '), ' 249569 ', ' ')) WHERE `guid` IN (12899435) AND `auras` LIKE '%249569%';
UPDATE `creature_addon` SET `auras` = TRIM(REPLACE(CONCAT(' ', `auras`, ' '), ' 253964 ', ' ')) WHERE `guid` IN (12899100) AND `auras` LIKE '%253964%';
UPDATE `creature_addon` SET `auras` = TRIM(REPLACE(CONCAT(' ', `auras`, ' '), ' 254705 ', ' ')) WHERE `guid` IN (12898954, 12898966, 12899020) AND `auras` LIKE '%254705%';
UPDATE `creature_addon` SET `auras` = TRIM(REPLACE(CONCAT(' ', `auras`, ' '), ' 258008 ', ' ')) WHERE `guid` IN (12898732, 12898733, 12898740, 12898745, 12898766, 12898794, 12898810, 12898819, 12898828, 12898845, 12898855) AND `auras` LIKE '%258008%';
UPDATE `creature_addon` SET `auras` = TRIM(REPLACE(CONCAT(' ', `auras`, ' '), ' 263275 ', ' ')) WHERE `guid` IN (400000479) AND `auras` LIKE '%263275%';
UPDATE `creature_addon` SET `auras` = TRIM(REPLACE(CONCAT(' ', `auras`, ' '), ' 268702 ', ' ')) WHERE `guid` IN (400000476, 400000488) AND `auras` LIKE '%268702%';
UPDATE `creature_addon` SET `auras` = TRIM(REPLACE(CONCAT(' ', `auras`, ' '), ' 315205 ', ' ')) WHERE `guid` IN (210444362, 210444377, 210444392, 210444397) AND `auras` LIKE '%315205%';
UPDATE `creature_addon` SET `auras` = TRIM(REPLACE(CONCAT(' ', `auras`, ' '), ' 316581 ', ' ')) WHERE `guid` IN (210444228, 210444307) AND `auras` LIKE '%316581%';
UPDATE `creature_addon` SET `auras` = TRIM(REPLACE(CONCAT(' ', `auras`, ' '), ' 324578 ', ' ')) WHERE `guid` IN (3000100629) AND `auras` LIKE '%324578%';
UPDATE `creature_addon` SET `auras` = TRIM(REPLACE(CONCAT(' ', `auras`, ' '), ' 346065 ', ' ')) WHERE `guid` IN (567843, 567924) AND `auras` LIKE '%346065%';
UPDATE `creature_addon` SET `auras` = TRIM(REPLACE(CONCAT(' ', `auras`, ' '), ' 346287 ', ' ')) WHERE `guid` IN (567880) AND `auras` LIKE '%346287%';
UPDATE `creature_addon` SET `auras` = TRIM(REPLACE(CONCAT(' ', `auras`, ' '), ' 346435 ', ' ')) WHERE `guid` IN (568833) AND `auras` LIKE '%346435%';
UPDATE `creature_addon` SET `auras` = TRIM(REPLACE(CONCAT(' ', `auras`, ' '), ' 347931 ', ' ')) WHERE `guid` IN (567915) AND `auras` LIKE '%347931%';
UPDATE `creature_addon` SET `auras` = TRIM(REPLACE(CONCAT(' ', `auras`, ' '), ' 347956 ', ' ')) WHERE `guid` IN (567850, 567916) AND `auras` LIKE '%347956%';
UPDATE `creature_addon` SET `auras` = TRIM(REPLACE(CONCAT(' ', `auras`, ' '), ' 353412 ', ' ')) WHERE `guid` IN (567459) AND `auras` LIKE '%353412%';
UPDATE `creature_addon` SET `auras` = TRIM(REPLACE(CONCAT(' ', `auras`, ' '), ' 353582 ', ' ')) WHERE `guid` IN (567483, 567528, 567594, 568613) AND `auras` LIKE '%353582%';
UPDATE `creature_addon` SET `auras` = TRIM(REPLACE(CONCAT(' ', `auras`, ' '), ' 355156 ', ' ')) WHERE `guid` IN (568867) AND `auras` LIKE '%355156%';
UPDATE `creature_addon` SET `auras` = TRIM(REPLACE(CONCAT(' ', `auras`, ' '), ' 355331 ', ' ')) WHERE `guid` IN (567481, 567523) AND `auras` LIKE '%355331%';
UPDATE `creature_addon` SET `auras` = TRIM(REPLACE(CONCAT(' ', `auras`, ' '), ' 355819 ', ' ')) WHERE `guid` IN (567583, 568074, 568075, 568078, 568080, 568083, 568087, 568088, 568091, 568097, 568098, 568107, 568114, 568115, 568117, 568124, 568130, 568131, 568132, 568134, 568135, 568140, 568145, 568146, 568148, 568158) AND `auras` LIKE '%355819%';
UPDATE `creature_addon` SET `auras` = TRIM(REPLACE(CONCAT(' ', `auras`, ' '), ' 356558 ', ' ')) WHERE `guid` IN (568853) AND `auras` LIKE '%356558%';
UPDATE `creature_addon` SET `auras` = TRIM(REPLACE(CONCAT(' ', `auras`, ' '), ' 356812 ', ' ')) WHERE `guid` IN (568627) AND `auras` LIKE '%356812%';
UPDATE `creature_addon` SET `auras` = TRIM(REPLACE(CONCAT(' ', `auras`, ' '), ' 359006 ', ' ')) WHERE `guid` IN (568889) AND `auras` LIKE '%359006%';
UPDATE `creature_addon` SET `auras` = TRIM(REPLACE(CONCAT(' ', `auras`, ' '), ' 369948 ', ' ')) WHERE `guid` IN (3000040241, 3000040248, 3000040249, 3000040256) AND `auras` LIKE '%369948%';
UPDATE `creature_addon` SET `auras` = TRIM(REPLACE(CONCAT(' ', `auras`, ' '), ' 375319 ', ' ')) WHERE `guid` IN (3000043044) AND `auras` LIKE '%375319%';
UPDATE `creature_addon` SET `auras` = TRIM(REPLACE(CONCAT(' ', `auras`, ' '), ' 376798 ', ' ')) WHERE `guid` IN (3000100649) AND `auras` LIKE '%376798%';
UPDATE `creature_addon` SET `auras` = TRIM(REPLACE(CONCAT(' ', `auras`, ' '), ' 382444 ', ' ')) WHERE `guid` IN (3000107414, 3000107474, 3000107475, 3000107479) AND `auras` LIKE '%382444%';
UPDATE `creature_addon` SET `auras` = TRIM(REPLACE(CONCAT(' ', `auras`, ' '), ' 383385 ', ' ')) WHERE `guid` IN (3000107716) AND `auras` LIKE '%383385%';
UPDATE `creature_addon` SET `auras` = TRIM(REPLACE(CONCAT(' ', `auras`, ' '), ' 384339 ', ' ')) WHERE `guid` IN (3000100649) AND `auras` LIKE '%384339%';
UPDATE `creature_addon` SET `auras` = TRIM(REPLACE(CONCAT(' ', `auras`, ' '), ' 388952 ', ' ')) WHERE `guid` IN (3000103617, 3000103625, 3000103822) AND `auras` LIKE '%388952%';
UPDATE `creature_addon` SET `auras` = TRIM(REPLACE(CONCAT(' ', `auras`, ' '), ' 389686 ', ' ')) WHERE `guid` IN (1251916) AND `auras` LIKE '%389686%';
UPDATE `creature_addon` SET `auras` = TRIM(REPLACE(CONCAT(' ', `auras`, ' '), ' 392286 ', ' ')) WHERE `guid` IN (3000100627, 3000100656) AND `auras` LIKE '%392286%';
UPDATE `creature_addon` SET `auras` = TRIM(REPLACE(CONCAT(' ', `auras`, ' '), ' 397215 ', ' ')) WHERE `guid` IN (1251814, 1251836, 1251843) AND `auras` LIKE '%397215%';
UPDATE `creature_addon` SET `auras` = TRIM(REPLACE(CONCAT(' ', `auras`, ' '), ' 398627 ', ' ')) WHERE `guid` IN (3000100225, 3000100226, 3000100227, 3000100229, 3000100232, 3000100233, 3000100234, 3000100236, 3000100237, 3000100240, 3000100242, 3000100243, 3000100245, 3000100246, 3000100249, 3000100250, 3000100253, 3000100255, 3000100256) AND `auras` LIKE '%398627%';
UPDATE `creature_addon` SET `auras` = TRIM(REPLACE(CONCAT(' ', `auras`, ' '), ' 403021 ', ' ')) WHERE `guid` IN (3000100286, 3000100307) AND `auras` LIKE '%403021%';
UPDATE `creature_addon` SET `auras` = TRIM(REPLACE(CONCAT(' ', `auras`, ' '), ' 406069 ', ' ')) WHERE `guid` IN (3000093745, 3000093993) AND `auras` LIKE '%406069%';
UPDATE `creature_addon` SET `auras` = TRIM(REPLACE(CONCAT(' ', `auras`, ' '), ' 406778 ', ' ')) WHERE `guid` IN (3000102688, 3000102691) AND `auras` LIKE '%406778%';
UPDATE `creature_addon` SET `auras` = TRIM(REPLACE(CONCAT(' ', `auras`, ' '), ' 407392 ', ' ')) WHERE `guid` IN (3000101979, 3000102005) AND `auras` LIKE '%407392%';
UPDATE `creature_addon` SET `auras` = TRIM(REPLACE(CONCAT(' ', `auras`, ' '), ' 408813 ', ' ')) WHERE `guid` IN (3000104569, 3000104608, 3000104674, 3000104675) AND `auras` LIKE '%408813%';
UPDATE `creature_addon` SET `auras` = TRIM(REPLACE(CONCAT(' ', `auras`, ' '), ' 409247 ', ' ')) WHERE `guid` IN (3000103370) AND `auras` LIKE '%409247%';
UPDATE `creature_addon` SET `auras` = TRIM(REPLACE(CONCAT(' ', `auras`, ' '), ' 409291 ', ' ')) WHERE `guid` IN (3000100493, 3000100543, 3000100565, 3000100571, 3000100575, 3000100617, 3000100624, 3000100626, 3000100651, 3000100653) AND `auras` LIKE '%409291%';
UPDATE `creature_addon` SET `auras` = TRIM(REPLACE(CONCAT(' ', `auras`, ' '), ' 410128 ', ' ')) WHERE `guid` IN (3000103732, 3000103733, 3000103739, 3000103755, 3000103796, 3000103826) AND `auras` LIKE '%410128%';
UPDATE `creature_addon` SET `auras` = TRIM(REPLACE(CONCAT(' ', `auras`, ' '), ' 413004 ', ' ')) WHERE `guid` IN (3000101703, 3000101747) AND `auras` LIKE '%413004%';
UPDATE `creature_addon` SET `auras` = TRIM(REPLACE(CONCAT(' ', `auras`, ' '), ' 413008 ', ' ')) WHERE `guid` IN (3000101697, 3000101736, 3000101804) AND `auras` LIKE '%413008%';
UPDATE `creature_addon` SET `auras` = TRIM(REPLACE(CONCAT(' ', `auras`, ' '), ' 413309 ', ' ')) WHERE `guid` IN (3000104792) AND `auras` LIKE '%413309%';
UPDATE `creature_addon` SET `auras` = TRIM(REPLACE(CONCAT(' ', `auras`, ' '), ' 413400 ', ' ')) WHERE `guid` IN (3000104543, 3000104684, 3000104790) AND `auras` LIKE '%413400%';
UPDATE `creature_addon` SET `auras` = TRIM(REPLACE(CONCAT(' ', `auras`, ' '), ' 447046 ', ' ')) WHERE `guid` IN (10002434, 10002446) AND `auras` LIKE '%447046%';

-- ============================================================================
-- Part 3: Remove temporary aura spells from creature_template_addon
-- 279 unique entries, 158 unique spell IDs
-- ============================================================================

UPDATE `creature_template_addon` SET `auras` = TRIM(REPLACE(CONCAT(' ', `auras`, ' '), ' 742 ', ' ')) WHERE `entry` IN (8924) AND `auras` LIKE '%742%';
UPDATE `creature_template_addon` SET `auras` = TRIM(REPLACE(CONCAT(' ', `auras`, ' '), ' 3288 ', ' ')) WHERE `entry` IN (41391) AND `auras` LIKE '%3288%';
UPDATE `creature_template_addon` SET `auras` = TRIM(REPLACE(CONCAT(' ', `auras`, ' '), ' 5302 ', ' ')) WHERE `entry` IN (42407) AND `auras` LIKE '%5302%';
UPDATE `creature_template_addon` SET `auras` = TRIM(REPLACE(CONCAT(' ', `auras`, ' '), ' 6431 ', ' ')) WHERE `entry` IN (3892) AND `auras` LIKE '%6431%';
UPDATE `creature_template_addon` SET `auras` = TRIM(REPLACE(CONCAT(' ', `auras`, ' '), ' 6434 ', ' ')) WHERE `entry` IN (38415) AND `auras` LIKE '%6434%';
UPDATE `creature_template_addon` SET `auras` = TRIM(REPLACE(CONCAT(' ', `auras`, ' '), ' 6742 ', ' ')) WHERE `entry` IN (7379) AND `auras` LIKE '%6742%';
UPDATE `creature_template_addon` SET `auras` = TRIM(REPLACE(CONCAT(' ', `auras`, ' '), ' 8599 ', ' ')) WHERE `entry` IN (5786, 36936) AND `auras` LIKE '%8599%';
UPDATE `creature_template_addon` SET `auras` = TRIM(REPLACE(CONCAT(' ', `auras`, ' '), ' 12169 ', ' ')) WHERE `entry` IN (42407, 51915) AND `auras` LIKE '%12169%';
UPDATE `creature_template_addon` SET `auras` = TRIM(REPLACE(CONCAT(' ', `auras`, ' '), ' 12544 ', ' ')) WHERE `entry` IN (710, 818, 881, 2319, 2567, 2573, 2591, 4719, 4844, 5328, 11562, 13096, 13546, 34985, 35007, 35606, 35827, 43992, 45182, 49573) AND `auras` LIKE '%12544%';
UPDATE `creature_template_addon` SET `auras` = TRIM(REPLACE(CONCAT(' ', `auras`, ' '), ' 12550 ', ' ')) WHERE `entry` IN (5343, 7379, 41121, 44363, 44685, 46367, 48118, 144154) AND `auras` LIKE '%12550%';
UPDATE `creature_template_addon` SET `auras` = TRIM(REPLACE(CONCAT(' ', `auras`, ' '), ' 13326 ', ' ')) WHERE `entry` IN (38465) AND `auras` LIKE '%13326%';
UPDATE `creature_template_addon` SET `auras` = TRIM(REPLACE(CONCAT(' ', `auras`, ' '), ' 13787 ', ' ')) WHERE `entry` IN (397, 48312, 144129) AND `auras` LIKE '%13787%';
UPDATE `creature_template_addon` SET `auras` = TRIM(REPLACE(CONCAT(' ', `auras`, ' '), ' 13864 ', ' ')) WHERE `entry` IN (951, 43319, 43913, 46475) AND `auras` LIKE '%13864%';
UPDATE `creature_template_addon` SET `auras` = TRIM(REPLACE(CONCAT(' ', `auras`, ' '), ' 15507 ', ' ')) WHERE `entry` IN (15613, 35880) AND `auras` LIKE '%15507%';
UPDATE `creature_template_addon` SET `auras` = TRIM(REPLACE(CONCAT(' ', `auras`, ' '), ' 17205 ', ' ')) WHERE `entry` IN (7439, 7442) AND `auras` LIKE '%17205%';
UPDATE `creature_template_addon` SET `auras` = TRIM(REPLACE(CONCAT(' ', `auras`, ' '), ' 18100 ', ' ')) WHERE `entry` IN (9451) AND `auras` LIKE '%18100%';
UPDATE `creature_template_addon` SET `auras` = TRIM(REPLACE(CONCAT(' ', `auras`, ' '), ' 18148 ', ' ')) WHERE `entry` IN (11577) AND `auras` LIKE '%18148%';
UPDATE `creature_template_addon` SET `auras` = TRIM(REPLACE(CONCAT(' ', `auras`, ' '), ' 18501 ', ' ')) WHERE `entry` IN (36094) AND `auras` LIKE '%18501%';
UPDATE `creature_template_addon` SET `auras` = TRIM(REPLACE(CONCAT(' ', `auras`, ' '), ' 19231 ', ' ')) WHERE `entry` IN (68868, 68869) AND `auras` LIKE '%19231%';
UPDATE `creature_template_addon` SET `auras` = TRIM(REPLACE(CONCAT(' ', `auras`, ' '), ' 19514 ', ' ')) WHERE `entry` IN (5362, 11744, 36147, 41028, 43156, 43233, 49808) AND `auras` LIKE '%19514%';
UPDATE `creature_template_addon` SET `auras` = TRIM(REPLACE(CONCAT(' ', `auras`, ' '), ' 22011 ', ' ')) WHERE `entry` IN (45066, 45067) AND `auras` LIKE '%22011%';
UPDATE `creature_template_addon` SET `auras` = TRIM(REPLACE(CONCAT(' ', `auras`, ' '), ' 31765 ', ' ')) WHERE `entry` IN (42573, 44646) AND `auras` LIKE '%31765%';
UPDATE `creature_template_addon` SET `auras` = TRIM(REPLACE(CONCAT(' ', `auras`, ' '), ' 32064 ', ' ')) WHERE `entry` IN (2743, 48314) AND `auras` LIKE '%32064%';
UPDATE `creature_template_addon` SET `auras` = TRIM(REPLACE(CONCAT(' ', `auras`, ' '), ' 34098 ', ' ')) WHERE `entry` IN (43688) AND `auras` LIKE '%34098%';
UPDATE `creature_template_addon` SET `auras` = TRIM(REPLACE(CONCAT(' ', `auras`, ' '), ' 34827 ', ' ')) WHERE `entry` IN (41425, 47642) AND `auras` LIKE '%34827%';
UPDATE `creature_template_addon` SET `auras` = TRIM(REPLACE(CONCAT(' ', `auras`, ' '), ' 44977 ', ' ')) WHERE `entry` IN (44443) AND `auras` LIKE '%44977%';
UPDATE `creature_template_addon` SET `auras` = TRIM(REPLACE(CONCAT(' ', `auras`, ' '), ' 46322 ', ' ')) WHERE `entry` IN (55220, 55221, 55341) AND `auras` LIKE '%46322%';
UPDATE `creature_template_addon` SET `auras` = TRIM(REPLACE(CONCAT(' ', `auras`, ' '), ' 47466 ', ' ')) WHERE `entry` IN (26125) AND `auras` LIKE '%47466%';
UPDATE `creature_template_addon` SET `auras` = TRIM(REPLACE(CONCAT(' ', `auras`, ' '), ' 48168 ', ' ')) WHERE `entry` IN (41444, 43731, 50360) AND `auras` LIKE '%48168%';
UPDATE `creature_template_addon` SET `auras` = TRIM(REPLACE(CONCAT(' ', `auras`, ' '), ' 55082 ', ' ')) WHERE `entry` IN (51760) AND `auras` LIKE '%55082%';
UPDATE `creature_template_addon` SET `auras` = TRIM(REPLACE(CONCAT(' ', `auras`, ' '), ' 57886 ', ' ')) WHERE `entry` IN (30837) AND `auras` LIKE '%57886%';
UPDATE `creature_template_addon` SET `auras` = TRIM(REPLACE(CONCAT(' ', `auras`, ' '), ' 61354 ', ' ')) WHERE `entry` IN (1275, 1286) AND `auras` LIKE '%61354%';
UPDATE `creature_template_addon` SET `auras` = TRIM(REPLACE(CONCAT(' ', `auras`, ' '), ' 61853 ', ' ')) WHERE `entry` IN (7381, 7384, 7385, 52831) AND `auras` LIKE '%61853%';
UPDATE `creature_template_addon` SET `auras` = TRIM(REPLACE(CONCAT(' ', `auras`, ' '), ' 63275 ', ' ')) WHERE `entry` IN (3178) AND `auras` LIKE '%63275%';
UPDATE `creature_template_addon` SET `auras` = TRIM(REPLACE(CONCAT(' ', `auras`, ' '), ' 66004 ', ' ')) WHERE `entry` IN (43795) AND `auras` LIKE '%66004%';
UPDATE `creature_template_addon` SET `auras` = TRIM(REPLACE(CONCAT(' ', `auras`, ' '), ' 66247 ', ' ')) WHERE `entry` IN (34811) AND `auras` LIKE '%66247%';
UPDATE `creature_template_addon` SET `auras` = TRIM(REPLACE(CONCAT(' ', `auras`, ' '), ' 70424 ', ' ')) WHERE `entry` IN (8764) AND `auras` LIKE '%70424%';
UPDATE `creature_template_addon` SET `auras` = TRIM(REPLACE(CONCAT(' ', `auras`, ' '), ' 74504 ', ' ')) WHERE `entry` IN (7726) AND `auras` LIKE '%74504%';
UPDATE `creature_template_addon` SET `auras` = TRIM(REPLACE(CONCAT(' ', `auras`, ' '), ' 74813 ', ' ')) WHERE `entry` IN (40147) AND `auras` LIKE '%74813%';
UPDATE `creature_template_addon` SET `auras` = TRIM(REPLACE(CONCAT(' ', `auras`, ' '), ' 75533 ', ' ')) WHERE `entry` IN (53704) AND `auras` LIKE '%75533%';
UPDATE `creature_template_addon` SET `auras` = TRIM(REPLACE(CONCAT(' ', `auras`, ' '), ' 75546 ', ' ')) WHERE `entry` IN (40503) AND `auras` LIKE '%75546%';
UPDATE `creature_template_addon` SET `auras` = TRIM(REPLACE(CONCAT(' ', `auras`, ' '), ' 75580 ', ' ')) WHERE `entry` IN (40340) AND `auras` LIKE '%75580%';
UPDATE `creature_template_addon` SET `auras` = TRIM(REPLACE(CONCAT(' ', `auras`, ' '), ' 75808 ', ' ')) WHERE `entry` IN (40633, 44404) AND `auras` LIKE '%75808%';
UPDATE `creature_template_addon` SET `auras` = TRIM(REPLACE(CONCAT(' ', `auras`, ' '), ' 76295 ', ' ')) WHERE `entry` IN (40844) AND `auras` LIKE '%76295%';
UPDATE `creature_template_addon` SET `auras` = TRIM(REPLACE(CONCAT(' ', `auras`, ' '), ' 76339 ', ' ')) WHERE `entry` IN (40788) AND `auras` LIKE '%76339%';
UPDATE `creature_template_addon` SET `auras` = TRIM(REPLACE(CONCAT(' ', `auras`, ' '), ' 76340 ', ' ')) WHERE `entry` IN (40291) AND `auras` LIKE '%76340%';
UPDATE `creature_template_addon` SET `auras` = TRIM(REPLACE(CONCAT(' ', `auras`, ' '), ' 76604 ', ' ')) WHERE `entry` IN (40936) AND `auras` LIKE '%76604%';
UPDATE `creature_template_addon` SET `auras` = TRIM(REPLACE(CONCAT(' ', `auras`, ' '), ' 76625 ', ' ')) WHERE `entry` IN (40925) AND `auras` LIKE '%76625%';
UPDATE `creature_template_addon` SET `auras` = TRIM(REPLACE(CONCAT(' ', `auras`, ' '), ' 77470 ', ' ')) WHERE `entry` IN (41364) AND `auras` LIKE '%77470%';
UPDATE `creature_template_addon` SET `auras` = TRIM(REPLACE(CONCAT(' ', `auras`, ' '), ' 77507 ', ' ')) WHERE `entry` IN (11858) AND `auras` LIKE '%77507%';
UPDATE `creature_template_addon` SET `auras` = TRIM(REPLACE(CONCAT(' ', `auras`, ' '), ' 77591 ', ' ')) WHERE `entry` IN (41364) AND `auras` LIKE '%77591%';
UPDATE `creature_template_addon` SET `auras` = TRIM(REPLACE(CONCAT(' ', `auras`, ' '), ' 78110 ', ' ')) WHERE `entry` IN (41908) AND `auras` LIKE '%78110%';
UPDATE `creature_template_addon` SET `auras` = TRIM(REPLACE(CONCAT(' ', `auras`, ' '), ' 78273 ', ' ')) WHERE `entry` IN (34695, 49900) AND `auras` LIKE '%78273%';
UPDATE `creature_template_addon` SET `auras` = TRIM(REPLACE(CONCAT(' ', `auras`, ' '), ' 78646 ', ' ')) WHERE `entry` IN (1057) AND `auras` LIKE '%78646%';
UPDATE `creature_template_addon` SET `auras` = TRIM(REPLACE(CONCAT(' ', `auras`, ' '), ' 79446 ', ' ')) WHERE `entry` IN (39855, 40291) AND `auras` LIKE '%79446%';
UPDATE `creature_template_addon` SET `auras` = TRIM(REPLACE(CONCAT(' ', `auras`, ' '), ' 79833 ', ' ')) WHERE `entry` IN (40969, 53841) AND `auras` LIKE '%79833%';
UPDATE `creature_template_addon` SET `auras` = TRIM(REPLACE(CONCAT(' ', `auras`, ' '), ' 79849 ', ' ')) WHERE `entry` IN (2108, 41272, 44818, 46718, 47248, 49760, 49896, 49968, 50716, 50717) AND `auras` LIKE '%79849%';
UPDATE `creature_template_addon` SET `auras` = TRIM(REPLACE(CONCAT(' ', `auras`, ' '), ' 79865 ', ' ')) WHERE `entry` IN (599, 44029, 44875, 46716, 47009, 47247, 62196) AND `auras` LIKE '%79865%';
UPDATE `creature_template_addon` SET `auras` = TRIM(REPLACE(CONCAT(' ', `auras`, ' '), ' 79877 ', ' ')) WHERE `entry` IN (43939) AND `auras` LIKE '%79877%';
UPDATE `creature_template_addon` SET `auras` = TRIM(REPLACE(CONCAT(' ', `auras`, ' '), ' 79892 ', ' ')) WHERE `entry` IN (519, 5461, 42858, 43170) AND `auras` LIKE '%79892%';
UPDATE `creature_template_addon` SET `auras` = TRIM(REPLACE(CONCAT(' ', `auras`, ' '), ' 79927 ', ' ')) WHERE `entry` IN (1165, 1399, 43169, 44668, 44678) AND `auras` LIKE '%79927%';
UPDATE `creature_template_addon` SET `auras` = TRIM(REPLACE(CONCAT(' ', `auras`, ' '), ' 79934 ', ' ')) WHERE `entry` IN (10826, 44823, 47805) AND `auras` LIKE '%79934%';
UPDATE `creature_template_addon` SET `auras` = TRIM(REPLACE(CONCAT(' ', `auras`, ' '), ' 79949 ', ' ')) WHERE `entry` IN (42730, 42731, 43503, 43514, 43809, 44351, 44352, 44633, 44634, 44642, 44672, 44673, 44674, 44676, 44677, 45037, 45038, 54664) AND `auras` LIKE '%79949%';
UPDATE `creature_template_addon` SET `auras` = TRIM(REPLACE(CONCAT(' ', `auras`, ' '), ' 79962 ', ' ')) WHERE `entry` IN (43001) AND `auras` LIKE '%79962%';
UPDATE `creature_template_addon` SET `auras` = TRIM(REPLACE(CONCAT(' ', `auras`, ' '), ' 79968 ', ' ')) WHERE `entry` IN (132262, 132266) AND `auras` LIKE '%79968%';
UPDATE `creature_template_addon` SET `auras` = TRIM(REPLACE(CONCAT(' ', `auras`, ' '), ' 79972 ', ' ')) WHERE `entry` IN (45429, 49793, 51143) AND `auras` LIKE '%79972%';
UPDATE `creature_template_addon` SET `auras` = TRIM(REPLACE(CONCAT(' ', `auras`, ' '), ' 79977 ', ' ')) WHERE `entry` IN (45429, 49793, 51143) AND `auras` LIKE '%79977%';
UPDATE `creature_template_addon` SET `auras` = TRIM(REPLACE(CONCAT(' ', `auras`, ' '), ' 80471 ', ' ')) WHERE `entry` IN (54373) AND `auras` LIKE '%80471%';
UPDATE `creature_template_addon` SET `auras` = TRIM(REPLACE(CONCAT(' ', `auras`, ' '), ' 80472 ', ' ')) WHERE `entry` IN (54373) AND `auras` LIKE '%80472%';
UPDATE `creature_template_addon` SET `auras` = TRIM(REPLACE(CONCAT(' ', `auras`, ' '), ' 80473 ', ' ')) WHERE `entry` IN (54373) AND `auras` LIKE '%80473%';
UPDATE `creature_template_addon` SET `auras` = TRIM(REPLACE(CONCAT(' ', `auras`, ' '), ' 80660 ', ' ')) WHERE `entry` IN (42521) AND `auras` LIKE '%80660%';
UPDATE `creature_template_addon` SET `auras` = TRIM(REPLACE(CONCAT(' ', `auras`, ' '), ' 80983 ', ' ')) WHERE `entry` IN (44152) AND `auras` LIKE '%80983%';
UPDATE `creature_template_addon` SET `auras` = TRIM(REPLACE(CONCAT(' ', `auras`, ' '), ' 81173 ', ' ')) WHERE `entry` IN (39949) AND `auras` LIKE '%81173%';
UPDATE `creature_template_addon` SET `auras` = TRIM(REPLACE(CONCAT(' ', `auras`, ' '), ' 81237 ', ' ')) WHERE `entry` IN (43438) AND `auras` LIKE '%81237%';
UPDATE `creature_template_addon` SET `auras` = TRIM(REPLACE(CONCAT(' ', `auras`, ' '), ' 81954 ', ' ')) WHERE `entry` IN (40168) AND `auras` LIKE '%81954%';
UPDATE `creature_template_addon` SET `auras` = TRIM(REPLACE(CONCAT(' ', `auras`, ' '), ' 82727 ', ' ')) WHERE `entry` IN (44208) AND `auras` LIKE '%82727%';
UPDATE `creature_template_addon` SET `auras` = TRIM(REPLACE(CONCAT(' ', `auras`, ' '), ' 82777 ', ' ')) WHERE `entry` IN (43686) AND `auras` LIKE '%82777%';
UPDATE `creature_template_addon` SET `auras` = TRIM(REPLACE(CONCAT(' ', `auras`, ' '), ' 82836 ', ' ')) WHERE `entry` IN (43575) AND `auras` LIKE '%82836%';
UPDATE `creature_template_addon` SET `auras` = TRIM(REPLACE(CONCAT(' ', `auras`, ' '), ' 83004 ', ' ')) WHERE `entry` IN (44885) AND `auras` LIKE '%83004%';
UPDATE `creature_template_addon` SET `auras` = TRIM(REPLACE(CONCAT(' ', `auras`, ' '), ' 83126 ', ' ')) WHERE `entry` IN (44566) AND `auras` LIKE '%83126%';
UPDATE `creature_template_addon` SET `auras` = TRIM(REPLACE(CONCAT(' ', `auras`, ' '), ' 83718 ', ' ')) WHERE `entry` IN (43689) AND `auras` LIKE '%83718%';
UPDATE `creature_template_addon` SET `auras` = TRIM(REPLACE(CONCAT(' ', `auras`, ' '), ' 83859 ', ' ')) WHERE `entry` IN (44923) AND `auras` LIKE '%83859%';
UPDATE `creature_template_addon` SET `auras` = TRIM(REPLACE(CONCAT(' ', `auras`, ' '), ' 84160 ', ' ')) WHERE `entry` IN (42522) AND `auras` LIKE '%84160%';
UPDATE `creature_template_addon` SET `auras` = TRIM(REPLACE(CONCAT(' ', `auras`, ' '), ' 86867 ', ' ')) WHERE `entry` IN (44683) AND `auras` LIKE '%86867%';
UPDATE `creature_template_addon` SET `auras` = TRIM(REPLACE(CONCAT(' ', `auras`, ' '), ' 88945 ', ' ')) WHERE `entry` IN (47786) AND `auras` LIKE '%88945%';
UPDATE `creature_template_addon` SET `auras` = TRIM(REPLACE(CONCAT(' ', `auras`, ' '), ' 90495 ', ' ')) WHERE `entry` IN (40586) AND `auras` LIKE '%90495%';
UPDATE `creature_template_addon` SET `auras` = TRIM(REPLACE(CONCAT(' ', `auras`, ' '), ' 91279 ', ' ')) WHERE `entry` IN (7035, 48120) AND `auras` LIKE '%91279%';
UPDATE `creature_template_addon` SET `auras` = TRIM(REPLACE(CONCAT(' ', `auras`, ' '), ' 93373 ', ' ')) WHERE `entry` IN (49813) AND `auras` LIKE '%93373%';
UPDATE `creature_template_addon` SET `auras` = TRIM(REPLACE(CONCAT(' ', `auras`, ' '), ' 93397 ', ' ')) WHERE `entry` IN (50301) AND `auras` LIKE '%93397%';
UPDATE `creature_template_addon` SET `auras` = TRIM(REPLACE(CONCAT(' ', `auras`, ' '), ' 93515 ', ' ')) WHERE `entry` IN (50057) AND `auras` LIKE '%93515%';
UPDATE `creature_template_addon` SET `auras` = TRIM(REPLACE(CONCAT(' ', `auras`, ' '), ' 95514 ', ' ')) WHERE `entry` IN (49457) AND `auras` LIKE '%95514%';
UPDATE `creature_template_addon` SET `auras` = TRIM(REPLACE(CONCAT(' ', `auras`, ' '), ' 96010 ', ' ')) WHERE `entry` IN (51710) AND `auras` LIKE '%96010%';
UPDATE `creature_template_addon` SET `auras` = TRIM(REPLACE(CONCAT(' ', `auras`, ' '), ' 96014 ', ' ')) WHERE `entry` IN (47801, 51674) AND `auras` LIKE '%96014%';
UPDATE `creature_template_addon` SET `auras` = TRIM(REPLACE(CONCAT(' ', `auras`, ' '), ' 96577 ', ' ')) WHERE `entry` IN (7382, 189112) AND `auras` LIKE '%96577%';
UPDATE `creature_template_addon` SET `auras` = TRIM(REPLACE(CONCAT(' ', `auras`, ' '), ' 97321 ', ' ')) WHERE `entry` IN (52148) AND `auras` LIKE '%97321%';
UPDATE `creature_template_addon` SET `auras` = TRIM(REPLACE(CONCAT(' ', `auras`, ' '), ' 97987 ', ' ')) WHERE `entry` IN (52956) AND `auras` LIKE '%97987%';
UPDATE `creature_template_addon` SET `auras` = TRIM(REPLACE(CONCAT(' ', `auras`, ' '), ' 99034 ', ' ')) WHERE `entry` IN (53396) AND `auras` LIKE '%99034%';
UPDATE `creature_template_addon` SET `auras` = TRIM(REPLACE(CONCAT(' ', `auras`, ' '), ' 101639 ', ' ')) WHERE `entry` IN (4642, 4658) AND `auras` LIKE '%101639%';
UPDATE `creature_template_addon` SET `auras` = TRIM(REPLACE(CONCAT(' ', `auras`, ' '), ' 102905 ', ' ')) WHERE `entry` IN (55089, 55093, 55225, 55397) AND `auras` LIKE '%102905%';
UPDATE `creature_template_addon` SET `auras` = TRIM(REPLACE(CONCAT(' ', `auras`, ' '), ' 102920 ', ' ')) WHERE `entry` IN (54969) AND `auras` LIKE '%102920%';
UPDATE `creature_template_addon` SET `auras` = TRIM(REPLACE(CONCAT(' ', `auras`, ' '), ' 105013 ', ' ')) WHERE `entry` IN (55911) AND `auras` LIKE '%105013%';
UPDATE `creature_template_addon` SET `auras` = TRIM(REPLACE(CONCAT(' ', `auras`, ' '), ' 105124 ', ' ')) WHERE `entry` IN (56142) AND `auras` LIKE '%105124%';
UPDATE `creature_template_addon` SET `auras` = TRIM(REPLACE(CONCAT(' ', `auras`, ' '), ' 107829 ', ' ')) WHERE `entry` IN (122773, 122778) AND `auras` LIKE '%107829%';
UPDATE `creature_template_addon` SET `auras` = TRIM(REPLACE(CONCAT(' ', `auras`, ' '), ' 108959 ', ' ')) WHERE `entry` IN (54611) AND `auras` LIKE '%108959%';
UPDATE `creature_template_addon` SET `auras` = TRIM(REPLACE(CONCAT(' ', `auras`, ' '), ' 115511 ', ' ')) WHERE `entry` IN (59746) AND `auras` LIKE '%115511%';
UPDATE `creature_template_addon` SET `auras` = TRIM(REPLACE(CONCAT(' ', `auras`, ' '), ' 116562 ', ' ')) WHERE `entry` IN (62193) AND `auras` LIKE '%116562%';
UPDATE `creature_template_addon` SET `auras` = TRIM(REPLACE(CONCAT(' ', `auras`, ' '), ' 130286 ', ' ')) WHERE `entry` IN (59309) AND `auras` LIKE '%130286%';
UPDATE `creature_template_addon` SET `auras` = TRIM(REPLACE(CONCAT(' ', `auras`, ' '), ' 132633 ', ' ')) WHERE `entry` IN (70733, 70740) AND `auras` LIKE '%132633%';
UPDATE `creature_template_addon` SET `auras` = TRIM(REPLACE(CONCAT(' ', `auras`, ' '), ' 157348 ', ' ')) WHERE `entry` IN (77942) AND `auras` LIKE '%157348%';
UPDATE `creature_template_addon` SET `auras` = TRIM(REPLACE(CONCAT(' ', `auras`, ' '), ' 167385 ', ' ')) WHERE `entry` IN (189617) AND `auras` LIKE '%167385%';
UPDATE `creature_template_addon` SET `auras` = TRIM(REPLACE(CONCAT(' ', `auras`, ' '), ' 192491 ', ' ')) WHERE `entry` IN (192491) AND `auras` LIKE '%192491%';
UPDATE `creature_template_addon` SET `auras` = TRIM(REPLACE(CONCAT(' ', `auras`, ' '), ' 192596 ', ' ')) WHERE `entry` IN (192596) AND `auras` LIKE '%192596%';
UPDATE `creature_template_addon` SET `auras` = TRIM(REPLACE(CONCAT(' ', `auras`, ' '), ' 198494 ', ' ')) WHERE `entry` IN (99773) AND `auras` LIKE '%198494%';
UPDATE `creature_template_addon` SET `auras` = TRIM(REPLACE(CONCAT(' ', `auras`, ' '), ' 201649 ', ' ')) WHERE `entry` IN (92879) AND `auras` LIKE '%201649%';
UPDATE `creature_template_addon` SET `auras` = TRIM(REPLACE(CONCAT(' ', `auras`, ' '), ' 204121 ', ' ')) WHERE `entry` IN (103146) AND `auras` LIKE '%204121%';
UPDATE `creature_template_addon` SET `auras` = TRIM(REPLACE(CONCAT(' ', `auras`, ' '), ' 204465 ', ' ')) WHERE `entry` IN (103312) AND `auras` LIKE '%204465%';
UPDATE `creature_template_addon` SET `auras` = TRIM(REPLACE(CONCAT(' ', `auras`, ' '), ' 244300 ', ' ')) WHERE `entry` IN (122322, 122403, 122404, 122405, 122407, 122408, 122412, 122413, 122421, 124171, 125981, 127911) AND `auras` LIKE '%244300%';
UPDATE `creature_template_addon` SET `auras` = TRIM(REPLACE(CONCAT(' ', `auras`, ' '), ' 246141 ', ' ')) WHERE `entry` IN (132210) AND `auras` LIKE '%246141%';
UPDATE `creature_template_addon` SET `auras` = TRIM(REPLACE(CONCAT(' ', `auras`, ' '), ' 255767 ', ' ')) WHERE `entry` IN (127929) AND `auras` LIKE '%255767%';
UPDATE `creature_template_addon` SET `auras` = TRIM(REPLACE(CONCAT(' ', `auras`, ' '), ' 261616 ', ' ')) WHERE `entry` IN (132969) AND `auras` LIKE '%261616%';
UPDATE `creature_template_addon` SET `auras` = TRIM(REPLACE(CONCAT(' ', `auras`, ' '), ' 290038 ', ' ')) WHERE `entry` IN (5498) AND `auras` LIKE '%290038%';
UPDATE `creature_template_addon` SET `auras` = TRIM(REPLACE(CONCAT(' ', `auras`, ' '), ' 358298 ', ' ')) WHERE `entry` IN (187468, 191364) AND `auras` LIKE '%358298%';
UPDATE `creature_template_addon` SET `auras` = TRIM(REPLACE(CONCAT(' ', `auras`, ' '), ' 369509 ', ' ')) WHERE `entry` IN (187194) AND `auras` LIKE '%369509%';
UPDATE `creature_template_addon` SET `auras` = TRIM(REPLACE(CONCAT(' ', `auras`, ' '), ' 369788 ', ' ')) WHERE `entry` IN (186727, 187599, 195032) AND `auras` LIKE '%369788%';
UPDATE `creature_template_addon` SET `auras` = TRIM(REPLACE(CONCAT(' ', `auras`, ' '), ' 373404 ', ' ')) WHERE `entry` IN (187120) AND `auras` LIKE '%373404%';
UPDATE `creature_template_addon` SET `auras` = TRIM(REPLACE(CONCAT(' ', `auras`, ' '), ' 373959 ', ' ')) WHERE `entry` IN (191705, 191805) AND `auras` LIKE '%373959%';
UPDATE `creature_template_addon` SET `auras` = TRIM(REPLACE(CONCAT(' ', `auras`, ' '), ' 374234 ', ' ')) WHERE `entry` IN (190530) AND `auras` LIKE '%374234%';
UPDATE `creature_template_addon` SET `auras` = TRIM(REPLACE(CONCAT(' ', `auras`, ' '), ' 375319 ', ' ')) WHERE `entry` IN (186145) AND `auras` LIKE '%375319%';
UPDATE `creature_template_addon` SET `auras` = TRIM(REPLACE(CONCAT(' ', `auras`, ' '), ' 375666 ', ' ')) WHERE `entry` IN (191704) AND `auras` LIKE '%375666%';
UPDATE `creature_template_addon` SET `auras` = TRIM(REPLACE(CONCAT(' ', `auras`, ' '), ' 377154 ', ' ')) WHERE `entry` IN (193153) AND `auras` LIKE '%377154%';
UPDATE `creature_template_addon` SET `auras` = TRIM(REPLACE(CONCAT(' ', `auras`, ' '), ' 377771 ', ' ')) WHERE `entry` IN (190945) AND `auras` LIKE '%377771%';
UPDATE `creature_template_addon` SET `auras` = TRIM(REPLACE(CONCAT(' ', `auras`, ' '), ' 379033 ', ' ')) WHERE `entry` IN (190962) AND `auras` LIKE '%379033%';
UPDATE `creature_template_addon` SET `auras` = TRIM(REPLACE(CONCAT(' ', `auras`, ' '), ' 381535 ', ' ')) WHERE `entry` IN (187083) AND `auras` LIKE '%381535%';
UPDATE `creature_template_addon` SET `auras` = TRIM(REPLACE(CONCAT(' ', `auras`, ' '), ' 381696 ', ' ')) WHERE `entry` IN (193011, 193468) AND `auras` LIKE '%381696%';
UPDATE `creature_template_addon` SET `auras` = TRIM(REPLACE(CONCAT(' ', `auras`, ' '), ' 382102 ', ' ')) WHERE `entry` IN (203737) AND `auras` LIKE '%382102%';
UPDATE `creature_template_addon` SET `auras` = TRIM(REPLACE(CONCAT(' ', `auras`, ' '), ' 386926 ', ' ')) WHERE `entry` IN (192563) AND `auras` LIKE '%386926%';
UPDATE `creature_template_addon` SET `auras` = TRIM(REPLACE(CONCAT(' ', `auras`, ' '), ' 386935 ', ' ')) WHERE `entry` IN (192563) AND `auras` LIKE '%386935%';
UPDATE `creature_template_addon` SET `auras` = TRIM(REPLACE(CONCAT(' ', `auras`, ' '), ' 387711 ', ' ')) WHERE `entry` IN (187745) AND `auras` LIKE '%387711%';
UPDATE `creature_template_addon` SET `auras` = TRIM(REPLACE(CONCAT(' ', `auras`, ' '), ' 389274 ', ' ')) WHERE `entry` IN (188180) AND `auras` LIKE '%389274%';
UPDATE `creature_template_addon` SET `auras` = TRIM(REPLACE(CONCAT(' ', `auras`, ' '), ' 389947 ', ' ')) WHERE `entry` IN (185168) AND `auras` LIKE '%389947%';
UPDATE `creature_template_addon` SET `auras` = TRIM(REPLACE(CONCAT(' ', `auras`, ' '), ' 391081 ', ' ')) WHERE `entry` IN (190598) AND `auras` LIKE '%391081%';
UPDATE `creature_template_addon` SET `auras` = TRIM(REPLACE(CONCAT(' ', `auras`, ' '), ' 392623 ', ' ')) WHERE `entry` IN (198079, 198146) AND `auras` LIKE '%392623%';
UPDATE `creature_template_addon` SET `auras` = TRIM(REPLACE(CONCAT(' ', `auras`, ' '), ' 392768 ', ' ')) WHERE `entry` IN (192580, 192964) AND `auras` LIKE '%392768%';
UPDATE `creature_template_addon` SET `auras` = TRIM(REPLACE(CONCAT(' ', `auras`, ' '), ' 392771 ', ' ')) WHERE `entry` IN (190516) AND `auras` LIKE '%392771%';
UPDATE `creature_template_addon` SET `auras` = TRIM(REPLACE(CONCAT(' ', `auras`, ' '), ' 393079 ', ' ')) WHERE `entry` IN (188180) AND `auras` LIKE '%393079%';
UPDATE `creature_template_addon` SET `auras` = TRIM(REPLACE(CONCAT(' ', `auras`, ' '), ' 393111 ', ' ')) WHERE `entry` IN (185545) AND `auras` LIKE '%393111%';
UPDATE `creature_template_addon` SET `auras` = TRIM(REPLACE(CONCAT(' ', `auras`, ' '), ' 395100 ', ' ')) WHERE `entry` IN (198835) AND `auras` LIKE '%395100%';
UPDATE `creature_template_addon` SET `auras` = TRIM(REPLACE(CONCAT(' ', `auras`, ' '), ' 405261 ', ' ')) WHERE `entry` IN (200566) AND `auras` LIKE '%405261%';
UPDATE `creature_template_addon` SET `auras` = TRIM(REPLACE(CONCAT(' ', `auras`, ' '), ' 405263 ', ' ')) WHERE `entry` IN (200562) AND `auras` LIKE '%405263%';
UPDATE `creature_template_addon` SET `auras` = TRIM(REPLACE(CONCAT(' ', `auras`, ' '), ' 405265 ', ' ')) WHERE `entry` IN (200563) AND `auras` LIKE '%405265%';
UPDATE `creature_template_addon` SET `auras` = TRIM(REPLACE(CONCAT(' ', `auras`, ' '), ' 406955 ', ' ')) WHERE `entry` IN (200564) AND `auras` LIKE '%406955%';
UPDATE `creature_template_addon` SET `auras` = TRIM(REPLACE(CONCAT(' ', `auras`, ' '), ' 419693 ', ' ')) WHERE `entry` IN (210596, 211370) AND `auras` LIKE '%419693%';
UPDATE `creature_template_addon` SET `auras` = TRIM(REPLACE(CONCAT(' ', `auras`, ' '), ' 424626 ', ' ')) WHERE `entry` IN (211289, 211290, 211291) AND `auras` LIKE '%424626%';
UPDATE `creature_template_addon` SET `auras` = TRIM(REPLACE(CONCAT(' ', `auras`, ' '), ' 424858 ', ' ')) WHERE `entry` IN (211631) AND `auras` LIKE '%424858%';
UPDATE `creature_template_addon` SET `auras` = TRIM(REPLACE(CONCAT(' ', `auras`, ' '), ' 424859 ', ' ')) WHERE `entry` IN (211631) AND `auras` LIKE '%424859%';
UPDATE `creature_template_addon` SET `auras` = TRIM(REPLACE(CONCAT(' ', `auras`, ' '), ' 424873 ', ' ')) WHERE `entry` IN (211644) AND `auras` LIKE '%424873%';
UPDATE `creature_template_addon` SET `auras` = TRIM(REPLACE(CONCAT(' ', `auras`, ' '), ' 424874 ', ' ')) WHERE `entry` IN (211644) AND `auras` LIKE '%424874%';
UPDATE `creature_template_addon` SET `auras` = TRIM(REPLACE(CONCAT(' ', `auras`, ' '), ' 445862 ', ' ')) WHERE `entry` IN (216328) AND `auras` LIKE '%445862%';

-- ============================================================================
-- Part 4: Remove SPELL_AURA_CONTROL_VEHICLE auras
-- ============================================================================

UPDATE `creature_addon` SET `auras` = TRIM(REPLACE(CONCAT(' ', `auras`, ' '), ' 46598 ', ' ')) WHERE `guid` IN (567669, 567670, 567679, 567680, 567718, 567720, 567762, 567763, 567764, 567765, 568592, 568593, 568594, 568614, 568615, 3000042978, 3000042991, 3000042997, 3000043206, 3000043208, 3000043256, 3000062467, 3000100271, 3000100429, 3000100683, 3000100816, 3000102047, 3000102453, 3000102761, 3000102974, 3000103376, 3000103726, 3000103876, 3000104025, 3000104483, 3000104487) AND `auras` LIKE '%46598%';
UPDATE `creature_addon` SET `auras` = TRIM(REPLACE(CONCAT(' ', `auras`, ' '), ' 61424 ', ' ')) WHERE `guid` IN (568588, 568589, 568634, 568635, 568637, 568638, 3000040559, 3000040560, 3000100544, 3000100545, 3000104012, 3000104013) AND `auras` LIKE '%61424%';
UPDATE `creature_addon` SET `auras` = TRIM(REPLACE(CONCAT(' ', `auras`, ' '), ' 87978 ', ' ')) WHERE `guid` IN (567736, 567738, 567739, 567741, 567744, 567745, 567747, 567748, 567978, 567979, 567985, 567987, 567990, 567993, 567996, 567998, 568037, 568039, 568040, 568041, 568043, 568044, 568047, 568051) AND `auras` LIKE '%87978%';
UPDATE `creature_addon` SET `auras` = TRIM(REPLACE(CONCAT(' ', `auras`, ' '), ' 122729 ', ' ')) WHERE `guid` IN (371271, 371273, 3000100442, 3000100443, 3000100620, 3000100621, 3000102997, 3000102998, 3000104007, 3000104008) AND `auras` LIKE '%122729%';
UPDATE `creature_addon` SET `auras` = TRIM(REPLACE(CONCAT(' ', `auras`, ' '), ' 268221 ', ' ')) WHERE `guid` IN (3000037092, 3000037093) AND `auras` LIKE '%268221%';
UPDATE `creature_addon` SET `auras` = TRIM(REPLACE(CONCAT(' ', `auras`, ' '), ' 363264 ', ' ')) WHERE `guid` IN (3000042563) AND `auras` LIKE '%363264%';
UPDATE `creature_addon` SET `auras` = TRIM(REPLACE(CONCAT(' ', `auras`, ' '), ' 369314 ', ' ')) WHERE `guid` IN (3000042412, 3000042413, 3000042415, 3000042550, 3000042551, 3000042554) AND `auras` LIKE '%369314%';
UPDATE `creature_addon` SET `auras` = TRIM(REPLACE(CONCAT(' ', `auras`, ' '), ' 396462 ', ' ')) WHERE `guid` IN (3000037038) AND `auras` LIKE '%396462%';
UPDATE `creature_addon` SET `auras` = TRIM(REPLACE(CONCAT(' ', `auras`, ' '), ' 419579 ', ' ')) WHERE `guid` IN (3000062584) AND `auras` LIKE '%419579%';
UPDATE `creature_template_addon` SET `auras` = TRIM(REPLACE(CONCAT(' ', `auras`, ' '), ' 46598 ', ' ')) WHERE `entry` IN (58727, 59706, 68442, 89715, 99582, 154867, 158637, 183186, 184404, 185489, 185696, 188859, 188860, 189481, 189825, 191931, 194925, 197211, 197786, 198299, 208722, 219201, 225822, 228665, 230853, 237599) AND `auras` LIKE '%46598%';
UPDATE `creature_template_addon` SET `auras` = TRIM(REPLACE(CONCAT(' ', `auras`, ' '), ' 103030 ', ' ')) WHERE `entry` IN (55019, 65468) AND `auras` LIKE '%103030%';
UPDATE `creature_template_addon` SET `auras` = TRIM(REPLACE(CONCAT(' ', `auras`, ' '), ' 106287 ', ' ')) WHERE `entry` IN (186356, 195367) AND `auras` LIKE '%106287%';
UPDATE `creature_template_addon` SET `auras` = TRIM(REPLACE(CONCAT(' ', `auras`, ' '), ' 122729 ', ' ')) WHERE `entry` IN (62821, 62822, 142666, 142668) AND `auras` LIKE '%122729%';
UPDATE `creature_template_addon` SET `auras` = TRIM(REPLACE(CONCAT(' ', `auras`, ' '), ' 268221 ', ' ')) WHERE `entry` IN (203863, 208207, 228881) AND `auras` LIKE '%268221%';
UPDATE `creature_template_addon` SET `auras` = TRIM(REPLACE(CONCAT(' ', `auras`, ' '), ' 358298 ', ' ')) WHERE `entry` IN (187468, 191364) AND `auras` LIKE '%358298%';
UPDATE `creature_template_addon` SET `auras` = TRIM(REPLACE(CONCAT(' ', `auras`, ' '), ' 396462 ', ' ')) WHERE `entry` IN (199748) AND `auras` LIKE '%396462%';
