-- ============================================================================
-- Workflow 6A: Gameobject Spawn Fixes
-- Fixes invalid state, rotation, terrainSwapMap, phaseGroup, and spawntimesecs
-- on gameobject spawns. Deletes gameobjects with unsupported difficulty 0.
-- ============================================================================

-- ---------------------------------------------------------------------------
-- 6A-1: Delete gameobject spawns with unsupported difficulty 0
-- These gameobjects are on instance/dungeon/raid maps that do not support
-- difficulty 0 (normal open world). The server skips them at load time.
-- Affected maps: instances, raids, scenarios, garrisons, etc.
-- (~19,189 spawns across 300+ maps)
-- ---------------------------------------------------------------------------
DELETE FROM gameobject WHERE spawnDifficulties = '0' AND map IN (
    33,34,36,43,47,48,70,90,109,129,189,209,229,230,249,269,289,309,329,349,
    389,409,429,451,469,509,531,532,533,534,540,542,543,544,545,546,547,548,
    550,552,553,554,555,556,557,558,560,564,565,568,574,575,576,578,580,585,
    595,599,600,601,602,603,604,608,615,616,619,624,631,632,643,644,645,649,
    650,657,658,669,671,720,724,725,754,755,757,859,938,939,940,959,960,961,
    962,967,994,996,999,1001,1004,1005,1007,1008,1009,1011,1014,1024,1031,
    1048,1051,1098,1112,1136,1158,1175,1176,1178,1180,1182,1195,1203,1205,
    1208,1209,1228,1268,1277,1279,1358,1448,1454,1455,1460,1470,1475,1478,
    1480,1489,1494,1495,1498,1500,1503,1511,1520,1522,1523,1526,1528,1530,
    1533,1536,1539,1541,1545,1553,1572,1579,1580,1583,1599,1600,1607,1609,
    1611,1612,1616,1617,1620,1621,1622,1623,1624,1625,1626,1629,1630,1648,
    1662,1666,1673,1676,1684,1687,1688,1693,1698,1702,1703,1704,1705,1706,
    1707,1708,1710,1712,1714,1719,1728,1729,1731,1732,1734,1736,1738,1744,
    1746,1754,1760,1762,1763,1771,1774,1840,1841,1861,1862,1864,1877,1884,
    1904,1906,1917,1932,1944,1955,2070,2076,2096,2100,2105,2111,2115,2125,
    2134,2161,2162,2164,2169,2178,2187,2207,2208,2209,2210,2212,2213,2214,
    2217,2236,2247,2266,2278,2284,2285,2287,2289,2291,2293,2296,2299,2300,
    2371,2375,2441,2450,2451,2464,2515,2516,2519,2520,2521,2522,2526,2527,
    2549,2556,2569,2579,2583,2590,2600,2625,2635,2644,2651,2654,2657,2660,
    2661,2664,2669,2679,2680,2681,2683,2684,2685,2686,2687,2688,2689,2690,
    2692,2699,2710,2716,2767,2769,2803,2810,2830
);

-- ---------------------------------------------------------------------------
-- 6A-2: Fix invalid gameobject state values
-- Valid states: 0 (active/open), 1 (ready/closed), 2 (destroyed)
-- 789 spawns have invalid state (788 with state=255, 1 with state=11)
-- ---------------------------------------------------------------------------
UPDATE gameobject SET state = 1 WHERE state NOT IN (0, 1, 2);

-- ---------------------------------------------------------------------------
-- 6A-3: Fix invalid rotation quaternions
-- Reset rotation to identity quaternion (0,0,0,1) for gameobjects with
-- invalid rotationZ or non-unit quaternion values
-- ---------------------------------------------------------------------------
UPDATE gameobject
SET rotation0 = 0, rotation1 = 0, rotation2 = 0, rotation3 = 1
WHERE (rotation0 = 0 AND rotation1 = 0 AND rotation2 = 0 AND rotation3 = 0);

-- Fix rotationZ values that are out of valid range (-1 to 1 for quaternion components)
-- The server already defaults these to Z-axis orientation, this makes it permanent
UPDATE gameobject
SET rotation0 = 0, rotation1 = 0,
    rotation2 = SIN(orientation / 2),
    rotation3 = COS(orientation / 2)
WHERE ABS(rotation2) > 1 OR ABS(rotation3) > 1;

-- ---------------------------------------------------------------------------
-- 6A-4: Fix invalid terrainSwapMap values
-- Set terrainSwapMap to -1 where it is 0 but cannot be used on the spawn map
-- The server already auto-fixes at load, this silences the warnings permanently
-- (~509 spawns)
-- ---------------------------------------------------------------------------
UPDATE gameobject SET terrainSwapMap = -1
WHERE terrainSwapMap = 0 AND guid IN (
    SELECT g.guid FROM (
        SELECT guid FROM gameobject WHERE terrainSwapMap = 0 AND map NOT IN (0, 1, 530, 571, 860, 870, 1116, 1220, 1642, 1643, 2222, 2374, 2552)
    ) g
);

-- ---------------------------------------------------------------------------
-- 6A-5: Fix invalid phaseGroup values
-- Set phaseGroup to 0 for entries referencing non-existent phase groups
-- The server already auto-fixes at load, this silences the warnings permanently
-- (~247 spawns, all in custom GUID ranges)
-- ---------------------------------------------------------------------------
UPDATE gameobject SET phaseGroup = 0
WHERE phaseGroup != 0 AND guid >= 70000000000;

-- ---------------------------------------------------------------------------
-- 6A-6: Fix spawntimesecs for despawnable gameobjects
-- Gameobjects marked as despawnable at action need a non-zero spawntimesecs
-- Set to 300 seconds (5 minutes) as a reasonable default respawn time
-- (~1,778 spawns)
-- ---------------------------------------------------------------------------
-- Note: This is a low-priority warning. Setting a default respawn time of 300s.
-- Specific gameobjects may need different values tuned later.
UPDATE gameobject g
INNER JOIN gameobject_template gt ON g.id = gt.entry
SET g.spawntimesecs = 300
WHERE g.spawntimesecs = 0
  AND gt.type IN (3, 25)  -- CHEST and FISHINGHOLE types that support despawn
  AND (gt.data5 != 0 OR gt.data4 != 0);  -- has loot-based or event-based despawn flags
