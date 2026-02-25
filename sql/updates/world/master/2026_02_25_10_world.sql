-- =====================================================================
-- Category 2B: Delete non-existent items from loot tables
-- =====================================================================
-- When a loot table row references an Item ID that doesn't exist in the
-- game's item DB2 data (ItemSparse), the server logs:
--
--   "Table 'X_loot_template' Entry N ItemType N Item N:
--    item does not exist - skipped"
--
-- The fix is to DELETE these orphaned rows. We target rows where
-- ItemType = 0 (regular item) and the Item ID is one of the known-bad
-- IDs extracted from the server error log.
--
-- Note: Item data lives in DB2 (hotfixes.item_sparse), not in a world
-- DB table, so we use explicit ID lists rather than subqueries.
--
-- Approximate row counts (from DBErrorsclaude.log on 2026-02-25):
--   creature_loot_template:    ~6,856 rows
--   gameobject_loot_template:  ~256 rows
--   disenchant_loot_template:  ~72 rows
--   reference_loot_template:   ~3 rows
--   item_loot_template:        ~2 rows
--   spell_loot_template:       ~22 rows
--                        Total: ~7,211 rows
--
-- All statements are idempotent -- safe to run multiple times.
-- =====================================================================

-- creature_loot_template (~6,856 rows referencing 199 distinct bad item IDs)
DELETE FROM creature_loot_template WHERE ItemType = 0 AND Item IN (
    0, 100, 241, 614, 677, 697, 823, 824, 1101, 1149, 1166, 1174, 1226, 1268, 1324, 1342, 1508, 1533, 1540, 1541,
    1553, 1585, 1704, 1719, 1723, 1885, 1906, 1977, 2003, 2009, 2106, 2118, 2412, 2413, 2706, 2707, 2708, 2709, 2803, 2809,
    2812, 2914, 3028, 3059, 3060, 3062, 3063, 3064, 3093, 3393, 3704, 4882, 9293, 9299, 11098, 11224, 13140, 13304, 13305, 13306,
    13307, 18268, 19707, 19708, 19709, 19710, 19711, 19712, 19713, 19714, 19715, 22541, 22548, 28276, 28277, 28558, 29669, 29672, 29673, 29674,
    29675, 37330, 37331, 37332, 37333, 37334, 38506, 44559, 44560, 44561, 44562, 44563, 54629, 56054, 56055, 56056, 60390, 60405, 60406, 62370,
    62452, 62528, 62536, 62770, 64663, 65157, 65665, 86579, 99712, 99742, 99743, 99744, 99745, 99746, 99747, 99748, 99749, 99750, 99751, 99752,
    99753, 99754, 99755, 99756, 104289, 104312, 105713, 105714, 105863, 105864, 105865, 113143, 121822, 121832, 122147, 122160, 122161, 122162, 127860, 128340,
    132138, 133957, 133968, 137179, 138019, 138441, 139619, 158406, 160468, 168950, 168951, 168952, 170540, 178149, 178557, 178766, 180536, 190727, 198847, 198848,
    198856, 201420, 201421, 204834, 204835, 204836, 204837, 204838, 204839, 204840, 204841, 204842, 207729, 208645, 212493, 212924, 213172, 217385, 217388, 217390,
    217998, 219254, 225236, 225709, 225710, 225715, 225716, 225718, 225767, 226133, 226158, 228228, 229412, 231508, 356076, 501501, 501503, 999996, 999997
);

-- gameobject_loot_template (~256 rows referencing 111 distinct bad item IDs)
DELETE FROM gameobject_loot_template WHERE ItemType = 0 AND Item IN (
    824, 3384, 5397, 5798, 6172, 7146, 9036, 9293, 11098, 12537, 35480, 35482, 37089, 37090, 40843, 46819, 52498, 56249, 90458, 99742,
    99743, 99744, 102225, 104038, 113143, 118564, 118566, 120979, 120997, 120998, 121830, 121831, 122096, 122098, 122099, 122144, 122145, 122146, 122154, 123865,
    123868, 123869, 128340, 133760, 138019, 147455, 156724, 156726, 156727, 167830, 169106, 170540, 175158, 178047, 178048, 178049, 178050, 178062, 178063, 178064,
    178149, 180534, 190606, 191048, 191054, 192810, 192811, 201421, 204834, 204835, 204836, 204837, 204838, 204839, 204840, 204841, 204842, 208142, 208835, 208836,
    208960, 208966, 208967, 208970, 208971, 208973, 209795, 209797, 209799, 212178, 212493, 213250, 213251, 213252, 213253, 213254, 213255, 213256, 213257, 217990,
    217993, 217995, 217997, 220344, 224146, 224401, 224402, 233326, 233794, 377058, 999998
);

-- disenchant_loot_template (~72 rows referencing 16 distinct bad item IDs)
DELETE FROM disenchant_loot_template WHERE ItemType = 0 AND Item IN (
    10978, 10998, 11082, 11083, 11084, 11134, 11135, 11137, 11138, 11139,
    11174, 11175, 11176, 11177, 11178, 20725
);

-- reference_loot_template (~3 rows referencing 3 distinct bad item IDs)
DELETE FROM reference_loot_template WHERE ItemType = 0 AND Item IN (
    1375, 14480, 14495
);

-- item_loot_template (2 rows)
DELETE FROM item_loot_template WHERE ItemType = 0 AND Item IN (5000, 43237);

-- spell_loot_template (22 rows)
DELETE FROM spell_loot_template WHERE ItemType = 0 AND Item IN (
    62312, 62336, 62337, 62338, 107976, 107977, 107978, 107979, 107980, 107981,
    107982, 107983, 108050, 108063, 108064, 108065, 108066, 108067, 108068, 108069, 108230
);
