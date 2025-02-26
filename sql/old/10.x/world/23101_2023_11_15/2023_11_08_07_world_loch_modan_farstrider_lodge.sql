SET @OGUID := 3004377;

-- Gameobject templates
UPDATE `gameobject_template` SET `ContentTuningId`=425, `VerifiedBuild`=51886 WHERE `entry`=208124; -- Candy Bucket

UPDATE `gameobject_template_addon` SET `faction`=1732 WHERE `entry`=208124; -- Candy Bucket

-- Quests
UPDATE `quest_template` SET `Expansion`=9, `VerifiedBuild`=51886 WHERE `ID`=28963; -- Candy Bucket

DELETE FROM `quest_offer_reward` WHERE `ID`=28963;
INSERT INTO `quest_offer_reward` (`ID`, `Emote1`, `Emote2`, `Emote3`, `Emote4`, `EmoteDelay1`, `EmoteDelay2`, `EmoteDelay3`, `EmoteDelay4`, `RewardText`, `VerifiedBuild`) VALUES
(28963, 0, 0, 0, 0, 0, 0, 0, 0, 'Candy buckets like this are located in inns throughout the realms. Go ahead... take some!', 51886); -- Candy Bucket

DELETE FROM `gameobject_queststarter` WHERE `id`=208124;
INSERT INTO `gameobject_queststarter` (`id`, `quest`, `VerifiedBuild`) VALUES
(208124, 28963, 51886);

UPDATE `gameobject_questender` SET `VerifiedBuild`=51886 WHERE (`id`=208124 AND `quest`=28963);

DELETE FROM `game_event_gameobject_quest` WHERE `id`=208124;

-- Old gameobject spawns
DELETE FROM `gameobject` WHERE `guid` BETWEEN 230962 AND 231017;
DELETE FROM `gameobject` WHERE `guid`=232003;
DELETE FROM `game_event_gameobject` WHERE `guid` BETWEEN 230962 AND 231017;
DELETE FROM `game_event_gameobject` WHERE `guid`=232003;

-- Gameobject spawns
DELETE FROM `gameobject` WHERE `guid` BETWEEN @OGUID+0 AND @OGUID+66;
INSERT INTO `gameobject` (`guid`, `id`, `map`, `zoneId`, `areaId`, `spawnDifficulties`, `PhaseId`, `PhaseGroup`, `position_x`, `position_y`, `position_z`, `orientation`, `rotation0`, `rotation1`, `rotation2`, `rotation3`, `spawntimesecs`, `animprogress`, `state`, `VerifiedBuild`) VALUES
(@OGUID+0, 180405, 0, 38, 147, '0', 0, 0, -5637.2744140625, -4294.43603515625, 407.863739013671875, 0.994837164878845214, 0, 0, 0.477158546447753906, 0.878817260265350341, 120, 255, 1, 51886), -- G_Pumpkin_01 (Area: The Farstrider Lodge - Difficulty: 0) CreateObject1
(@OGUID+1, 180405, 0, 38, 147, '0', 0, 0, -5673.17041015625, -4239.890625, 407.028228759765625, 4.101525306701660156, 0, 0, -0.88701057434082031, 0.461749136447906494, 120, 255, 1, 51886), -- G_Pumpkin_01 (Area: The Farstrider Lodge - Difficulty: 0) CreateObject1
(@OGUID+2, 180405, 0, 38, 147, '0', 0, 0, -5680.9130859375, -4244.95166015625, 406.984771728515625, 3.769911527633666992, 0, 0, -0.95105648040771484, 0.309017121791839599, 120, 255, 1, 51886), -- G_Pumpkin_01 (Area: The Farstrider Lodge - Difficulty: 0) CreateObject1
(@OGUID+3, 180406, 0, 38, 147, '0', 0, 0, -5646.236328125, -4300.33837890625, 407.85760498046875, 1.012289404869079589, 0, 0, 0.484808921813964843, 0.87462007999420166, 120, 255, 1, 51886), -- G_Pumpkin_02 (Area: The Farstrider Lodge - Difficulty: 0) CreateObject1
(@OGUID+4, 180407, 0, 38, 147, '0', 0, 0, -5656.16162109375, -4275.203125, 409.881683349609375, 0.942476630210876464, 0, 0, 0.453989982604980468, 0.891006767749786376, 120, 255, 1, 51886), -- G_Pumpkin_03 (Area: The Farstrider Lodge - Difficulty: 0) CreateObject1
(@OGUID+5, 180408, 0, 38, 147, '0', 0, 0, -5680.90087890625, -4244.923828125, 408.58099365234375, 3.368495941162109375, 0, 0, -0.99357128143310546, 0.113208353519439697, 120, 255, 1, 51886), -- G_WitchHat_01 (Area: The Farstrider Lodge - Difficulty: 0) CreateObject1
(@OGUID+6, 180408, 0, 38, 147, '0', 0, 0, -5673.19287109375, -4239.890625, 408.630096435546875, 3.700104713439941406, 0, 0, -0.96126079559326171, 0.275640487670898437, 120, 255, 1, 51886), -- G_WitchHat_01 (Area: The Farstrider Lodge - Difficulty: 0) CreateObject1
(@OGUID+7, 180411, 0, 38, 147, '0', 0, 0, -5639.2568359375, -4301.05029296875, 413.21649169921875, 2.391098499298095703, 0, 0, 0.930417060852050781, 0.366502493619918823, 120, 255, 1, 51886), -- G_Ghost_01 (Area: The Farstrider Lodge - Difficulty: 0) CreateObject1
(@OGUID+8, 180415, 0, 38, 147, '0', 0, 0, -5641.8369140625, -4257.1962890625, 409.459747314453125, 0, 0, 0, 0, 1, 120, 255, 1, 51886), -- CandleBlack01 (Area: The Farstrider Lodge - Difficulty: 0) CreateObject1
(@OGUID+9, 180415, 0, 38, 147, '0', 0, 0, -5625.921875, -4246.96728515625, 409.476043701171875, 0, 0, 0, 0, 1, 120, 255, 1, 51886), -- CandleBlack01 (Area: The Farstrider Lodge - Difficulty: 0) CreateObject1
(@OGUID+10, 180415, 0, 38, 147, '0', 0, 0, -5694.251953125, -4291.17724609375, 409.475128173828125, 0, 0, 0, 0, 1, 120, 255, 1, 51886), -- CandleBlack01 (Area: The Farstrider Lodge - Difficulty: 0) CreateObject1
(@OGUID+11, 180415, 0, 38, 147, '0', 0, 0, -5678.40625, -4280.84228515625, 409.459747314453125, 0, 0, 0, 0, 1, 120, 255, 1, 51886), -- CandleBlack01 (Area: The Farstrider Lodge - Difficulty: 0) CreateObject1
(@OGUID+12, 180415, 0, 38, 147, '0', 0, 0, -5631.876953125, -4250.79541015625, 409.459747314453125, 0, 0, 0, 0, 1, 120, 255, 1, 51886), -- CandleBlack01 (Area: The Farstrider Lodge - Difficulty: 0) CreateObject1
(@OGUID+13, 180415, 0, 38, 147, '0', 0, 0, -5635.86962890625, -4253.345703125, 409.459747314453125, 0, 0, 0, 0, 1, 120, 255, 1, 51886), -- CandleBlack01 (Area: The Farstrider Lodge - Difficulty: 0) CreateObject1
(@OGUID+14, 180415, 0, 38, 147, '0', 0, 0, -5622.11474609375, -4244.408203125, 409.50885009765625, 0, 0, 0, 0, 1, 120, 255, 1, 51886), -- CandleBlack01 (Area: The Farstrider Lodge - Difficulty: 0) CreateObject1
(@OGUID+15, 180415, 0, 38, 147, '0', 0, 0, -5698.02587890625, -4293.74853515625, 409.508819580078125, 0, 0, 0, 0, 1, 120, 255, 1, 51886), -- CandleBlack01 (Area: The Farstrider Lodge - Difficulty: 0) CreateObject1
(@OGUID+16, 180415, 0, 38, 147, '0', 0, 0, -5684.220703125, -4284.81103515625, 409.459747314453125, 0, 0, 0, 0, 1, 120, 255, 1, 51886), -- CandleBlack01 (Area: The Farstrider Lodge - Difficulty: 0) CreateObject1
(@OGUID+17, 180415, 0, 38, 147, '0', 0, 0, -5688.142578125, -4287.3525390625, 409.459747314453125, 0, 0, 0, 0, 1, 120, 255, 1, 51886), -- CandleBlack01 (Area: The Farstrider Lodge - Difficulty: 0) CreateObject1
(@OGUID+18, 180415, 0, 38, 147, '0', 0, 0, -5674.296875, -4278.35400390625, 409.459747314453125, 0, 0, 0, 0, 1, 120, 255, 1, 51886), -- CandleBlack01 (Area: The Farstrider Lodge - Difficulty: 0) CreateObject1
(@OGUID+19, 180415, 0, 38, 147, '0', 0, 0, -5645.720703125, -4259.76904296875, 409.459747314453125, 0, 0, 0, 0, 1, 120, 255, 1, 51886), -- CandleBlack01 (Area: The Farstrider Lodge - Difficulty: 0) CreateObject1
(@OGUID+20, 180425, 0, 38, 147, '0', 0, 0, -5643.814453125, -4258.51220703125, 409.459747314453125, 5.410521507263183593, 0, 0, -0.42261791229248046, 0.906307935714721679, 120, 255, 1, 51886), -- SkullCandle01 (Area: The Farstrider Lodge - Difficulty: 0) CreateObject1
(@OGUID+21, 180425, 0, 38, 147, '0', 0, 0, -5633.86962890625, -4252.11962890625, 409.459747314453125, 5.148722648620605468, 0, 0, -0.53729915618896484, 0.843391716480255126, 120, 255, 1, 51886), -- SkullCandle01 (Area: The Farstrider Lodge - Difficulty: 0) CreateObject1
(@OGUID+22, 180425, 0, 38, 147, '0', 0, 0, -5686.21533203125, -4286.13720703125, 409.459747314453125, 5.131268978118896484, 0, 0, -0.54463863372802734, 0.838670849800109863, 120, 255, 1, 51886), -- SkullCandle01 (Area: The Farstrider Lodge - Difficulty: 0) CreateObject1
(@OGUID+23, 180425, 0, 38, 147, '0', 0, 0, -5676.2568359375, -4279.62158203125, 409.459747314453125, 5.235987663269042968, 0, 0, -0.5, 0.866025388240814208, 120, 255, 1, 51886), -- SkullCandle01 (Area: The Farstrider Lodge - Difficulty: 0) CreateObject1
(@OGUID+24, 180425, 0, 38, 147, '0', 0, 0, -5624.00537109375, -4245.689453125, 409.4942626953125, 5.393068790435791015, 0, 0, -0.43051052093505859, 0.902585566043853759, 120, 255, 1, 51886), -- SkullCandle01 (Area: The Farstrider Lodge - Difficulty: 0) CreateObject1
(@OGUID+25, 180425, 0, 38, 147, '0', 0, 0, -5695.98974609375, -4292.51220703125, 409.49285888671875, 5.340708732604980468, 0, 0, -0.45398998260498046, 0.891006767749786376, 120, 255, 1, 51886), -- SkullCandle01 (Area: The Farstrider Lodge - Difficulty: 0) CreateObject1
(@OGUID+26, 180426, 0, 38, 147, '0', 0, 0, -5692.3037109375, -4215.56787109375, 401.80633544921875, 5.096362113952636718, 0, 0, -0.55919265747070312, 0.829037725925445556, 120, 255, 1, 51886), -- Bat01 (Area: The Farstrider Lodge - Difficulty: 0) CreateObject1
(@OGUID+27, 180426, 0, 38, 147, '0', 0, 0, -5689.41845703125, -4215.12158203125, 397.439117431640625, 4.45059061050415039, 0, 0, -0.79335308074951171, 0.608761727809906005, 120, 255, 1, 51886), -- Bat01 (Area: The Farstrider Lodge - Difficulty: 0) CreateObject1
(@OGUID+28, 180426, 0, 38, 147, '0', 0, 0, -5692.265625, -4214.39404296875, 399.35845947265625, 3.124123096466064453, 0, 0, 0.99996185302734375, 0.008734640665352344, 120, 255, 1, 51886), -- Bat01 (Area: The Farstrider Lodge - Difficulty: 0) CreateObject1
(@OGUID+29, 180426, 0, 38, 147, '0', 0, 0, -5633.04345703125, -4220.88037109375, 392.00274658203125, 0.907570242881774902, 0, 0, 0.438370704650878906, 0.898794233798980712, 120, 255, 1, 51886), -- Bat01 (Area: The Farstrider Lodge - Difficulty: 0) CreateObject1
(@OGUID+30, 180426, 0, 38, 147, '0', 0, 0, -5640.876953125, -4221.267578125, 396.80322265625, 1.85004889965057373, 0, 0, 0.798635482788085937, 0.60181504487991333, 120, 255, 1, 51886), -- Bat01 (Area: The Farstrider Lodge - Difficulty: 0) CreateObject1
(@OGUID+31, 180426, 0, 38, 147, '0', 0, 0, -5636.98974609375, -4218.828125, 401.769561767578125, 4.398232460021972656, 0, 0, -0.80901622772216796, 0.587786316871643066, 120, 255, 1, 51886), -- Bat01 (Area: The Farstrider Lodge - Difficulty: 0) CreateObject1
(@OGUID+32, 180427, 0, 38, 147, '0', 0, 0, -5646.3681640625, -4206.1630859375, 413.4278564453125, 2.600535154342651367, 0, 0, 0.963629722595214843, 0.26724100112915039, 120, 255, 1, 51886), -- Bat02 (Area: The Farstrider Lodge - Difficulty: 0) CreateObject1
(@OGUID+33, 180427, 0, 38, 147, '0', 0, 0, -5636.91845703125, -4211.8037109375, 411.973114013671875, 0.610863447189331054, 0, 0, 0.3007049560546875, 0.953717231750488281, 120, 255, 1, 51886), -- Bat02 (Area: The Farstrider Lodge - Difficulty: 0) CreateObject1
(@OGUID+34, 180427, 0, 38, 147, '0', 0, 0, -5677.5087890625, -4211.05908203125, 410.945587158203125, 3.665196180343627929, 0, 0, -0.96592521667480468, 0.258821308612823486, 120, 255, 1, 51886), -- Bat02 (Area: The Farstrider Lodge - Difficulty: 0) CreateObject1
(@OGUID+35, 180427, 0, 38, 147, '0', 0, 0, -5689.49658203125, -4212.70654296875, 408.18804931640625, 4.468043327331542968, 0, 0, -0.7880105972290039, 0.615661680698394775, 120, 255, 1, 51886), -- Bat02 (Area: The Farstrider Lodge - Difficulty: 0) CreateObject1
(@OGUID+36, 180427, 0, 38, 147, '0', 0, 0, -5660.27978515625, -4208.73095703125, 409.994781494140625, 3.176533222198486328, 0, 0, -0.999847412109375, 0.017469281330704689, 120, 255, 1, 51886), -- Bat02 (Area: The Farstrider Lodge - Difficulty: 0) CreateObject1
(@OGUID+37, 180471, 0, 38, 147, '0', 0, 0, -5641.87353515625, -4257.15478515625, 413.416961669921875, 0.715584874153137207, 0, 0, 0.350207328796386718, 0.936672210693359375, 120, 255, 1, 51886), -- HangingSkullLight01 (Area: The Farstrider Lodge - Difficulty: 0) CreateObject1
(@OGUID+38, 180471, 0, 38, 147, '0', 0, 0, -5631.98291015625, -4250.74462890625, 413.416107177734375, 0.506144583225250244, 0, 0, 0.250379562377929687, 0.968147754669189453, 120, 255, 1, 51886), -- HangingSkullLight01 (Area: The Farstrider Lodge - Difficulty: 0) CreateObject1
(@OGUID+39, 180471, 0, 38, 147, '0', 0, 0, -5624.033203125, -4245.70166015625, 413.547271728515625, 0.488691210746765136, 0, 0, 0.241921424865722656, 0.970295846462249755, 120, 255, 1, 51886), -- HangingSkullLight01 (Area: The Farstrider Lodge - Difficulty: 0) CreateObject1
(@OGUID+40, 180471, 0, 38, 147, '0', 0, 0, -5694.1787109375, -4291.22412109375, 413.594635009765625, 0.767943859100341796, 0, 0, 0.374606132507324218, 0.927184045314788818, 120, 255, 1, 51886), -- HangingSkullLight01 (Area: The Farstrider Lodge - Difficulty: 0) CreateObject1
(@OGUID+41, 180471, 0, 38, 147, '0', 0, 0, -5645.86474609375, -4305.95654296875, 413.4664306640625, 1.48352813720703125, 0, 0, 0.675589561462402343, 0.737277925014495849, 120, 255, 1, 51886), -- HangingSkullLight01 (Area: The Farstrider Lodge - Difficulty: 0) CreateObject1
(@OGUID+42, 180471, 0, 38, 147, '0', 0, 0, -5684.23095703125, -4284.767578125, 413.36029052734375, 0.558503925800323486, 0, 0, 0.275636672973632812, 0.961261868476867675, 120, 255, 1, 51886), -- HangingSkullLight01 (Area: The Farstrider Lodge - Difficulty: 0) CreateObject1
(@OGUID+43, 180471, 0, 38, 147, '0', 0, 0, -5645.798828125, -4259.74658203125, 413.34197998046875, 0.837757468223571777, 0, 0, 0.406736373901367187, 0.913545548915863037, 120, 255, 1, 51886), -- HangingSkullLight01 (Area: The Farstrider Lodge - Difficulty: 0) CreateObject1
(@OGUID+44, 180471, 0, 38, 147, '0', 0, 0, -5678.23779296875, -4247.19287109375, 414.426422119140625, 3.752462387084960937, 0, 0, -0.95371627807617187, 0.300707906484603881, 120, 255, 1, 51886), -- HangingSkullLight01 (Area: The Farstrider Lodge - Difficulty: 0) CreateObject1
(@OGUID+45, 180471, 0, 38, 147, '0', 0, 0, -5665.783203125, -4265.89599609375, 414.515167236328125, 0.27925160527229309, 0, 0, 0.139172554016113281, 0.990268170833587646, 120, 255, 1, 51886), -- HangingSkullLight01 (Area: The Farstrider Lodge - Difficulty: 0) CreateObject1
(@OGUID+46, 180471, 0, 38, 147, '0', 0, 0, -5660.41650390625, -4261.9462890625, 414.42901611328125, 0.855210542678833007, 0, 0, 0.414692878723144531, 0.909961462020874023, 120, 255, 1, 51886), -- HangingSkullLight01 (Area: The Farstrider Lodge - Difficulty: 0) CreateObject1
(@OGUID+47, 180471, 0, 38, 147, '0', 0, 0, -5688.17529296875, -4287.34375, 413.58673095703125, 0.959929943084716796, 0, 0, 0.461748123168945312, 0.887011110782623291, 120, 255, 1, 51886), -- HangingSkullLight01 (Area: The Farstrider Lodge - Difficulty: 0) CreateObject1
(@OGUID+48, 180471, 0, 38, 147, '0', 0, 0, -5628.85791015625, -4295.220703125, 413.2857666015625, 1.48352813720703125, 0, 0, 0.675589561462402343, 0.737277925014495849, 120, 255, 1, 51886), -- HangingSkullLight01 (Area: The Farstrider Lodge - Difficulty: 0) CreateObject1
(@OGUID+49, 180471, 0, 38, 147, '0', 0, 0, -5674.34228515625, -4278.3212890625, 413.58673095703125, 0.506144583225250244, 0, 0, 0.250379562377929687, 0.968147754669189453, 120, 255, 1, 51886), -- HangingSkullLight01 (Area: The Farstrider Lodge - Difficulty: 0) CreateObject1
(@OGUID+50, 180471, 0, 38, 147, '0', 0, 0, -5686.283203125, -4286.09375, 413.440826416015625, 6.161012649536132812, 0, 0, -0.06104850769042968, 0.998134791851043701, 120, 255, 1, 51886), -- HangingSkullLight01 (Area: The Farstrider Lodge - Difficulty: 0) CreateObject1
(@OGUID+51, 180471, 0, 38, 147, '0', 0, 0, -5698.03662109375, -4293.74658203125, 413.581878662109375, 0.628316879272460937, 0, 0, 0.309016227722167968, 0.95105677843093872, 120, 255, 1, 51886), -- HangingSkullLight01 (Area: The Farstrider Lodge - Difficulty: 0) CreateObject1
(@OGUID+52, 180471, 0, 38, 147, '0', 0, 0, -5633.98974609375, -4252.095703125, 413.31298828125, 0.663223206996917724, 0, 0, 0.325567245483398437, 0.945518851280212402, 120, 255, 1, 51886), -- HangingSkullLight01 (Area: The Farstrider Lodge - Difficulty: 0) CreateObject1
(@OGUID+53, 180471, 0, 38, 147, '0', 0, 0, -5625.94970703125, -4246.92041015625, 413.324249267578125, 0.698131442070007324, 0, 0, 0.342020034790039062, 0.939692676067352294, 120, 255, 1, 51886), -- HangingSkullLight01 (Area: The Farstrider Lodge - Difficulty: 0) CreateObject1
(@OGUID+54, 180471, 0, 38, 147, '0', 0, 0, -5632.283203125, -4297.1005859375, 413.535552978515625, 0.034906249493360519, 0, 0, 0.017452239990234375, 0.999847710132598876, 120, 255, 1, 51886), -- HangingSkullLight01 (Area: The Farstrider Lodge - Difficulty: 0) CreateObject1
(@OGUID+55, 180471, 0, 38, 147, '0', 0, 0, -5678.376953125, -4280.91162109375, 413.571807861328125, 0.907570242881774902, 0, 0, 0.438370704650878906, 0.898794233798980712, 120, 255, 1, 51886), -- HangingSkullLight01 (Area: The Farstrider Lodge - Difficulty: 0) CreateObject1
(@OGUID+56, 180471, 0, 38, 147, '0', 0, 0, -5672.5625, -4243.4443359375, 414.311676025390625, 3.892086982727050781, 0, 0, -0.93041706085205078, 0.366502493619918823, 120, 255, 1, 51886), -- HangingSkullLight01 (Area: The Farstrider Lodge - Difficulty: 0) CreateObject1
(@OGUID+57, 180471, 0, 38, 147, '0', 0, 0, -5635.892578125, -4253.330078125, 413.431671142578125, 0.733038187026977539, 0, 0, 0.358367919921875, 0.933580458164215087, 120, 255, 1, 51886), -- HangingSkullLight01 (Area: The Farstrider Lodge - Difficulty: 0) CreateObject1
(@OGUID+58, 180471, 0, 38, 147, '0', 0, 0, -5696.04150390625, -4292.4931640625, 413.314727783203125, 0.523597896099090576, 0, 0, 0.258818626403808593, 0.965925931930541992, 120, 255, 1, 51886), -- HangingSkullLight01 (Area: The Farstrider Lodge - Difficulty: 0) CreateObject1
(@OGUID+59, 180471, 0, 38, 147, '0', 0, 0, -5676.3037109375, -4279.564453125, 413.330322265625, 0.593410074710845947, 0, 0, 0.292370796203613281, 0.95630502700805664, 120, 255, 1, 51886), -- HangingSkullLight01 (Area: The Farstrider Lodge - Difficulty: 0) CreateObject1
(@OGUID+60, 180471, 0, 38, 147, '0', 0, 0, -5649.1787109375, -4308.265625, 413.274566650390625, 0, 0, 0, 0, 1, 120, 255, 1, 51886), -- HangingSkullLight01 (Area: The Farstrider Lodge - Difficulty: 0) CreateObject1
(@OGUID+61, 180471, 0, 38, 147, '0', 0, 0, -5643.8525390625, -4258.50341796875, 413.24169921875, 0.610863447189331054, 0, 0, 0.3007049560546875, 0.953717231750488281, 120, 255, 1, 51886), -- HangingSkullLight01 (Area: The Farstrider Lodge - Difficulty: 0) CreateObject1
(@OGUID+62, 180471, 0, 38, 147, '0', 0, 0, -5622.15625, -4244.39404296875, 413.381011962890625, 0.436331570148468017, 0, 0, 0.216439247131347656, 0.976296067237854003, 120, 255, 1, 51886), -- HangingSkullLight01 (Area: The Farstrider Lodge - Difficulty: 0) CreateObject1
(@OGUID+63, 180472, 0, 38, 147, '0', 0, 0, -5663.376953125, -4263.90625, 416.525665283203125, 5.288348197937011718, 0, 0, -0.4771585464477539, 0.878817260265350341, 120, 255, 1, 51886), -- HangingSkullLight02 (Area: The Farstrider Lodge - Difficulty: 0) CreateObject1
(@OGUID+64, 180472, 0, 38, 147, '0', 0, 0, -5675.50537109375, -4245.3056640625, 416.476776123046875, 2.059488296508789062, 0, 0, 0.857167243957519531, 0.515038192272186279, 120, 255, 1, 51886), -- HangingSkullLight02 (Area: The Farstrider Lodge - Difficulty: 0) CreateObject1
(@OGUID+65, 180523, 0, 38, 147, '0', 0, 0, -5648.705078125, -4278.234375, 407.81256103515625, 5.864306926727294921, 0, 0, -0.20791149139404296, 0.978147625923156738, 120, 255, 1, 51886), -- Apple Bob (Area: The Farstrider Lodge - Difficulty: 0) CreateObject1
(@OGUID+66, 208124, 0, 38, 147, '0', 0, 0, -5656.1962890625, -4283.87353515625, 407.81732177734375, 1.151916384696960449, 0, 0, 0.544638633728027343, 0.838670849800109863, 120, 255, 1, 51886); -- Candy Bucket (Area: The Farstrider Lodge - Difficulty: 0) CreateObject1

-- Event spawns
DELETE FROM `game_event_gameobject` WHERE `eventEntry`=12 AND `guid` BETWEEN @OGUID+0 AND @OGUID+66;
INSERT INTO `game_event_gameobject` (`eventEntry`, `guid`) VALUES
(12, @OGUID+0),
(12, @OGUID+1),
(12, @OGUID+2),
(12, @OGUID+3),
(12, @OGUID+4),
(12, @OGUID+5),
(12, @OGUID+6),
(12, @OGUID+7),
(12, @OGUID+8),
(12, @OGUID+9),
(12, @OGUID+10),
(12, @OGUID+11),
(12, @OGUID+12),
(12, @OGUID+13),
(12, @OGUID+14),
(12, @OGUID+15),
(12, @OGUID+16),
(12, @OGUID+17),
(12, @OGUID+18),
(12, @OGUID+19),
(12, @OGUID+20),
(12, @OGUID+21),
(12, @OGUID+22),
(12, @OGUID+23),
(12, @OGUID+24),
(12, @OGUID+25),
(12, @OGUID+26),
(12, @OGUID+27),
(12, @OGUID+28),
(12, @OGUID+29),
(12, @OGUID+30),
(12, @OGUID+31),
(12, @OGUID+32),
(12, @OGUID+33),
(12, @OGUID+34),
(12, @OGUID+35),
(12, @OGUID+36),
(12, @OGUID+37),
(12, @OGUID+38),
(12, @OGUID+39),
(12, @OGUID+40),
(12, @OGUID+41),
(12, @OGUID+42),
(12, @OGUID+43),
(12, @OGUID+44),
(12, @OGUID+45),
(12, @OGUID+46),
(12, @OGUID+47),
(12, @OGUID+48),
(12, @OGUID+49),
(12, @OGUID+50),
(12, @OGUID+51),
(12, @OGUID+52),
(12, @OGUID+53),
(12, @OGUID+54),
(12, @OGUID+55),
(12, @OGUID+56),
(12, @OGUID+57),
(12, @OGUID+58),
(12, @OGUID+59),
(12, @OGUID+60),
(12, @OGUID+61),
(12, @OGUID+62),
(12, @OGUID+63),
(12, @OGUID+64),
(12, @OGUID+65),
(12, @OGUID+66);
