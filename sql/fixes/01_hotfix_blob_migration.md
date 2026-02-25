# Workflow 1: Hotfix Blob Migration

**Database**: `hotfixes`
**Estimated errors**: ~6,700
**SQL output**: `sql/updates/hotfixes/master/2026_02_25_0X_hotfixes.sql`
**Priority**: Medium (these are warnings, not crashes)

## Problem Description

The `hotfix_blob` table contains raw binary overrides for DB2 stores. When TrinityCore
has a proper typed table for a given DB2 store, it warns that the data should live in
that typed table instead of `hotfix_blob`.

Error pattern:
```
Table hash 0x76C04C97 points to a loaded DB2 store NPCModelItemSlotDisplayInfo.db2, fill related table instead of hotfix_blob
Table hash 0x83467FEB points to a loaded DB2 store QuestPOIPoint.db2, fill related table instead of hotfix_blob
Table hash 0xCA54AB01 points to a loaded DB2 store SoundKitEntry.db2, fill related table instead of hotfix_blob
Table hash 0xC4A4BFCC points to a loaded DB2 store SoundKitAdvanced.db2, fill related table instead of hotfix_blob
Table hash 0x0E5E5EB1 points to a loaded DB2 store Spell.db2, fill related table instead of hotfix_blob
Table hash 0x0FA3E1CB points to a loaded DB2 store ModifiedCraftingCategory.db2, fill related table instead of hotfix_blob
```

## Affected DB2 Stores (by error count)

| Hash | DB2 Store | Count |
|------|-----------|-------|
| 0x83467FEB | QuestPOIPoint.db2 | ~700 |
| 0x76C04C97 | NPCModelItemSlotDisplayInfo.db2 | 3 |
| 0xCA54AB01 | SoundKitEntry.db2 | ~4,400 |
| 0xC4A4BFCC | SoundKitAdvanced.db2 | ~924 |
| 0x0E5E5EB1 | Spell.db2 | ~618 |
| 0x0FA3E1CB | ModifiedCraftingCategory.db2 | ~40 |

## Steps

### Step 1: Understand the Schema
1. Read the TrinityCore source to find the C++ loader for each affected DB2 store:
   - Search for the table hash constant or DB2 store name in `src/server/`
   - Find the corresponding `hotfix_data` and typed hotfix table (e.g., `quest_poi_points`, `sound_kit_entry`)
2. Check what columns the typed table expects by reading the DB2 loading code

### Step 2: Identify the Hotfix Blob Records
```sql
-- Example: Find all QuestPOIPoint blob entries
SELECT * FROM hotfixes.hotfix_blob WHERE TableHash = 0x83467FEB;

-- Example: Find all SoundKitEntry blob entries
SELECT * FROM hotfixes.hotfix_blob WHERE TableHash = 0xCA54AB01;
```

### Step 3: Determine the Right Fix

For each DB2 store, one of two approaches:

**Option A — Migrate to typed table** (preferred if the data is meaningful):
- Parse the blob data according to the DB2 field structure
- INSERT the parsed rows into the correct typed hotfix table
- DELETE the old rows from `hotfix_blob`

**Option B — Delete the blob entries** (if the hotfixes are outdated/unnecessary for 12.x):
- Verify the hotfixes aren't needed (check if the base DB2 already has correct data)
- DELETE from `hotfix_blob` WHERE `TableHash` = <hash>

### Step 4: Generate SQL
- Output to `sql/updates/hotfixes/master/2026_02_25_00_hotfixes.sql` (and `_01`, `_02` etc.)
- Make all statements idempotent
- Include comments explaining what each block does

### Step 5: Verify
- Apply the SQL to your local `hotfixes` database
- Restart worldserver and confirm the warnings are gone
- Grep the new error log for the affected table hashes

## Key Source Files to Read
- `src/server/game/DataStores/DB2LoadInfo.h` — DB2 field definitions
- `src/server/game/DataStores/DB2Stores.cpp` — store loading code
- `src/server/game/DataStores/DB2HotfixGenerator.cpp` — hotfix generation
- `src/server/database/Database/Implementation/HotfixDatabase.cpp` — hotfix DB queries
