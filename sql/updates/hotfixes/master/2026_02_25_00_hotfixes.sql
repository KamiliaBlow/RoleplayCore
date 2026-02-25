-- Remove hotfix_blob entries for DB2 stores that have proper typed tables.
--
-- When a TableHash in hotfix_blob corresponds to a loaded DB2 store, TrinityCore
-- skips the blob entirely and logs:
--   "Table hash 0x... points to a loaded DB2 store ..., fill related table instead of hotfix_blob"
--
-- These rows are dead data — the server never uses them. Deleting them eliminates
-- ~6,758 error log lines on every worldserver boot.
--
-- Source: DB2Manager::LoadHotfixBlob() in DB2Stores.cpp:1935-1980
-- The validation at line 1954-1959 checks _stores.find(tableHash) and skips with continue.
--
-- Hash-to-store mapping (from error log):
--   0xC2B150C7 = SoundKitEntry.db2           (4407 errors)
--   0xC0A5F8C6 = SoundKitAdvanced.db2        ( 924 errors)
--   0x83467FEB = QuestPOIPoint.db2            ( 700 errors)
--   0xE111669E = Spell.db2                    ( 618 errors)
--   0xF24979A5 = ModifiedCraftingCategory.db2 (  40 errors)
--   0x986F8CD0 = ItemDisplayInfo.db2          (  27 errors)
--   0xE598C085 = ModifiedCraftingReagentItem.db2 (20 errors)
--   0xBF0BC27A = GlobalStrings.db2            (  10 errors)
--   0x88C61AE6 = CreatureDisplayInfoOption.db2(   9 errors)
--   0x76C04C97 = NPCModelItemSlotDisplayInfo.db2( 3 errors)

-- SoundKitEntry.db2 (largest — 4407 entries)
DELETE FROM `hotfix_blob` WHERE `TableHash` = 0xC2B150C7;

-- SoundKitAdvanced.db2 (924 entries)
DELETE FROM `hotfix_blob` WHERE `TableHash` = 0xC0A5F8C6;

-- QuestPOIPoint.db2 (700 entries)
DELETE FROM `hotfix_blob` WHERE `TableHash` = 0x83467FEB;

-- Spell.db2 (618 entries)
DELETE FROM `hotfix_blob` WHERE `TableHash` = 0xE111669E;

-- ModifiedCraftingCategory.db2 (40 entries)
DELETE FROM `hotfix_blob` WHERE `TableHash` = 0xF24979A5;

-- ItemDisplayInfo.db2 (27 entries)
DELETE FROM `hotfix_blob` WHERE `TableHash` = 0x986F8CD0;

-- ModifiedCraftingReagentItem.db2 (20 entries)
DELETE FROM `hotfix_blob` WHERE `TableHash` = 0xE598C085;

-- GlobalStrings.db2 (10 entries)
DELETE FROM `hotfix_blob` WHERE `TableHash` = 0xBF0BC27A;

-- CreatureDisplayInfoOption.db2 (9 entries)
DELETE FROM `hotfix_blob` WHERE `TableHash` = 0x88C61AE6;

-- NPCModelItemSlotDisplayInfo.db2 (3 entries)
DELETE FROM `hotfix_blob` WHERE `TableHash` = 0x76C04C97;
