-- Fix disallowed unit_flags, unit_flags2, and unit_flags3 on creature_template and creature
--
-- ObjectMgr::CheckCreatureTemplate() and ObjectMgr::LoadCreatures() strip bits that
-- belong to UNIT_FLAG_DISALLOWED / UNIT_FLAG2_DISALLOWED / UNIT_FLAG3_DISALLOWED at
-- runtime, producing thousands of "disallowed `unit_flags*`" log errors every startup.
-- Persisting the fix in DB eliminates ~30,800 error log entries on every startup.
--
-- Mask values derived from UnitDefines.h:
--   UNIT_FLAG_DISALLOWED  = 0xFDFFDCBF  (UNIT_FLAG_ALLOWED  = 0x02002340)
--   UNIT_FLAG2_DISALLOWED = 0xFBFCB7DC  (UNIT_FLAG2_ALLOWED = 0x04034823)
--   UNIT_FLAG3_DISALLOWED = 0xFEB21F49  (UNIT_FLAG3_ALLOWED = 0x014DE0B6)
--
-- The fix uses bitwise AND with the ALLOWED mask to clear only the disallowed bits,
-- preserving all valid flag bits.  Bitwise AND is naturally idempotent.

-- ============================================================================
-- creature_template: unit_flags  (9142 log entries)
-- ============================================================================
UPDATE `creature_template`
   SET `unit_flags` = `unit_flags` & 0x02002340
 WHERE (`unit_flags` & 0xFDFFDCBF) != 0;

-- ============================================================================
-- creature_template: unit_flags2  (6787 log entries)
-- ============================================================================
UPDATE `creature_template`
   SET `unit_flags2` = `unit_flags2` & 0x04034823
 WHERE (`unit_flags2` & 0xFBFCB7DC) != 0;

-- ============================================================================
-- creature_template: unit_flags3  (5443 log entries)
-- ============================================================================
UPDATE `creature_template`
   SET `unit_flags3` = `unit_flags3` & 0x014DE0B6
 WHERE (`unit_flags3` & 0xFEB21F49) != 0;

-- ============================================================================
-- creature: unit_flags  (8775 log entries)
-- ============================================================================
UPDATE `creature`
   SET `unit_flags` = `unit_flags` & 0x02002340
 WHERE `unit_flags` IS NOT NULL
   AND (`unit_flags` & 0xFDFFDCBF) != 0;

-- ============================================================================
-- creature: unit_flags2  (608 log entries)
-- ============================================================================
UPDATE `creature`
   SET `unit_flags2` = `unit_flags2` & 0x04034823
 WHERE `unit_flags2` IS NOT NULL
   AND (`unit_flags2` & 0xFBFCB7DC) != 0;

-- ============================================================================
-- creature: unit_flags3  (63 log entries)
-- ============================================================================
UPDATE `creature`
   SET `unit_flags3` = `unit_flags3` & 0x014DE0B6
 WHERE `unit_flags3` IS NOT NULL
   AND (`unit_flags3` & 0xFEB21F49) != 0;
