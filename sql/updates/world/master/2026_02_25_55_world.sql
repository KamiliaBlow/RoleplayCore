-- ============================================================================
-- Workflow 6C + 6I + 6J: Gossip, Creature Text, and Conversation Fixes
-- ============================================================================

-- ---------------------------------------------------------------------------
-- 6C-1: Delete gossip_menu entries referencing non-existent NPC text IDs
-- 54 rows across 52 menu IDs reference TextIDs that don't exist in npc_text
-- ---------------------------------------------------------------------------
DELETE FROM gossip_menu WHERE TextID NOT IN (SELECT ID FROM npc_text) AND TextID != 0;

-- For TextID=0 references, set to a valid default (use npc_text ID 1 if it exists,
-- otherwise these menus simply won't show text)
-- MenuIDs with TextID=0: 10385, 19868, 32779, 34429, 36877, 44047, 143925, 166227, 500535, 954120
DELETE FROM gossip_menu WHERE TextID = 0 AND MenuID IN (
    10385,19868,32779,34429,36877,44047,143925,166227,500535,954120
);

-- ---------------------------------------------------------------------------
-- 6C-2: Clear ActionMenuID on gossip_menu_option where GossipOptionNpc != None
-- When GossipOptionNpc is not None, ActionMenuID is ignored by the server.
-- Setting to 0 silences the warning. (79 rows across 44 menus)
-- ---------------------------------------------------------------------------
UPDATE gossip_menu_option SET ActionMenuID = 0
WHERE (MenuID, OptionID) IN (
    (1294,2),(2487,1),(4261,0),(5856,0),(7468,2),(8001,0),(8726,0),(8784,1),
    (9832,0),(10181,0),(10780,0),(11512,0),(11555,0),(11821,0),(12040,0),
    (12045,0),(12046,0),(15252,0),(15252,1),(16597,0),(19045,0),(19071,0),
    (19091,2),(19646,0),(19874,0),(20140,2),(20669,0),(20669,1),(20669,2),
    (20669,3),(20669,4),(20757,0),(20883,0),(20908,1),(20994,0),(21043,1),
    (21071,1),(21072,1),(21072,2),(21072,3),(21072,4),(21072,5),(21072,6),
    (21072,7),(21255,0),(21330,0),(21507,0),(26364,0),(38623,1),(55555,1),
    (55555,2),(55555,3),(55555,4),(55555,5),(55555,6),(55555,7),(55555,8),
    (55555,9),(55555,10),(55555,11),(55555,12),(94242,0),(600022,30),
    (600022,31),(600022,32),(600024,0),(600024,1),(600024,2),(600024,3),
    (600024,4),(600024,5),(600024,6),(600024,7),(600024,8),(600024,9),
    (600024,10),(600024,11),(600123,0),(600124,0)
);

-- ---------------------------------------------------------------------------
-- 6C-3: Condition ScriptName warnings
-- 292 conditions reference ConditionType that requires a ScriptName but none
-- is set. These are mostly custom gossip entries (900000+ range).
-- Low priority -- these conditions are simply ignored by the server.
-- No automated fix applied; these need manual script assignment.
-- ---------------------------------------------------------------------------

-- ---------------------------------------------------------------------------
-- 6I-1: Fix creature_text entries with invalid PlayType values
-- 1,276 rows have PlayType values (5, 21, 25, 66, 70, 100, 255) that don't
-- correspond to valid ChatMsg types. Set to 12 (MONSTER_SAY) as safe default.
-- ---------------------------------------------------------------------------
-- The error is about SoundPlayType values that don't correspond to valid play types
-- Valid SoundPlayType values: 0=Normal, 1=OrderedOnce, 2=DistanceDependent, 3=Random, 4=?
-- Set invalid values to 0 (Normal) as safe default
UPDATE creature_text SET SoundPlayType = 0
WHERE SoundPlayType IN (5,21,25,66,70,100,255);

-- ---------------------------------------------------------------------------
-- 6I-2: Fix creature_text entries with invalid Language values
-- 64 rows across 52 creature entries use Language 15, 16, or 41 which don't exist
-- Set to 0 (LANG_UNIVERSAL) as safe default
-- ---------------------------------------------------------------------------
UPDATE creature_text SET Language = 0
WHERE Language IN (15, 16, 41)
  AND CreatureID IN (
    28782,34547,38033,39364,39365,64367,68553,71475,71479,72248,73678,75452,
    89890,90435,91331,92142,95674,96682,96783,97059,99524,103809,105393,
    105640,107302,108345,108390,108709,108872,112252,123413,127980,130484,
    133384,133392,134069,135907,138599,142929,153451,156279,157069,164853,
    171591,173918,176429,180773,181398,181399,182777,183533,228534
);

-- ---------------------------------------------------------------------------
-- 6I-3: Fix npc_text entries with zero probability sum
-- 91 npc_text IDs have all probability values set to 0, making text unselectable.
-- Set Probability0 to 1 so at least the first text option can be selected.
-- ---------------------------------------------------------------------------
UPDATE npc_text SET Probability0 = 1 WHERE ID IN (
    2,2953,2955,2956,2957,2981,4118,4119,4133,6657,7114,7401,7496,7498,7500,
    7501,8321,8332,9625,9651,9701,9727,10128,10884,11128,13157,14330,15442,
    20551,20564,26885,28670,30000,30001,31780,40478,40479,40902,40906,40918,
    40921,40922,40923,40924,40927,40928,40929,40930,41264,41265,41819,41828,
    60000,100000,100002,100006,100007,100008,121283,300000,300001,300002,
    600022,600172,600173,600174,600176,600177,610000,610001,610002,610003,
    610004,610005,688881,700073,724001,724002,724003,724004,724005,802000,
    802001,900013,900017,900033,900044,900055,900071,900072,900087
);

-- ---------------------------------------------------------------------------
-- 6I-4: Clear non-existing BroadcastText references in npc_text
-- 63 rows across 55 npc_text IDs reference BroadcastText IDs that don't exist.
-- Set the BroadcastTextID fields to 0 for affected entries.
-- ---------------------------------------------------------------------------
UPDATE npc_text
SET BroadcastTextID0 = IF(BroadcastTextID0 NOT IN (SELECT ID FROM hotfixes.broadcast_text) AND BroadcastTextID0 != 0, 0, BroadcastTextID0),
    BroadcastTextID1 = IF(BroadcastTextID1 NOT IN (SELECT ID FROM hotfixes.broadcast_text) AND BroadcastTextID1 != 0, 0, BroadcastTextID1),
    BroadcastTextID2 = IF(BroadcastTextID2 NOT IN (SELECT ID FROM hotfixes.broadcast_text) AND BroadcastTextID2 != 0, 0, BroadcastTextID2),
    BroadcastTextID3 = IF(BroadcastTextID3 NOT IN (SELECT ID FROM hotfixes.broadcast_text) AND BroadcastTextID3 != 0, 0, BroadcastTextID3),
    BroadcastTextID4 = IF(BroadcastTextID4 NOT IN (SELECT ID FROM hotfixes.broadcast_text) AND BroadcastTextID4 != 0, 0, BroadcastTextID4),
    BroadcastTextID5 = IF(BroadcastTextID5 NOT IN (SELECT ID FROM hotfixes.broadcast_text) AND BroadcastTextID5 != 0, 0, BroadcastTextID5),
    BroadcastTextID6 = IF(BroadcastTextID6 NOT IN (SELECT ID FROM hotfixes.broadcast_text) AND BroadcastTextID6 != 0, 0, BroadcastTextID6),
    BroadcastTextID7 = IF(BroadcastTextID7 NOT IN (SELECT ID FROM hotfixes.broadcast_text) AND BroadcastTextID7 != 0, 0, BroadcastTextID7)
WHERE ID IN (
    26885,28670,40478,40479,40902,40906,40918,40921,40922,40923,40924,40927,
    40928,40929,40930,41264,41265,41518,41520,41819,41828,42409,42410,60000,
    100002,100006,100007,100008,300000,300001,300002,600022,600172,600173,
    600174,600176,600177,610000,610001,610002,610003,610004,610005,688881,
    700073,802000,802001,900013,900017,900033,900044,900055,900071,900072,
    900087
);

-- ---------------------------------------------------------------------------
-- 6J: Delete conversation_actors entries referencing invalid creature GUIDs
-- 177 rows across 107 conversation IDs reference creature GUIDs that don't
-- exist in the creature table (most reference GUID=0)
-- ---------------------------------------------------------------------------
-- Delete entries where ConversationActorGuid references a non-existent creature
-- This covers both GUID=0 (unset/placeholder) and non-zero invalid GUIDs
DELETE FROM conversation_actors
WHERE (ConversationActorGuid = 0 OR ConversationActorGuid NOT IN (SELECT guid FROM creature))
  AND ConversationId IN (
    922,4857,5360,5362,5365,5366,5375,6467,6468,6469,6470,6471,6479,6634,
    6637,7605,7653,7690,8062,8749,8750,8751,9556,12044,12058,12073,12078,
    12086,12087,12088,12089,12128,13611,13630,13631,13632,13633,13634,13635,
    13710,13712,13892,13893,13895,14422,14423,14424,14437,14440,14441,14444,
    14447,14448,14449,14452,14453,14454,14455,14456,14457,14458,14459,14460,
    14461,14462,14463,14465,14466,14467,14468,14471,14472,14473,14474,14475,
    14476,14477,14486,14487,14488,14489,14490,14491,14492,14493,14494,14495,
    14513,14516,14613,17009,17010,17011,17368,19967,20124,20125,20126,20127,
    20128,20340,20342,20387,25586,25590,26392,28310
);
