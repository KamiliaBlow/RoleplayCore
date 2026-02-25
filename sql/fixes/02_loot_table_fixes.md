# Workflow 2: Loot Table Fixes

**Database**: `world`
**Estimated errors**: ~443,000
**SQL output**: `sql/updates/world/master/2026_02_25_1X_world.sql`
**Priority**: High (largest error category by count)

## Problem Description

Multiple loot tables reference non-existent entries, contain invalid items, or have
probability issues. This is the single largest error category at ~443K lines.

## Error Categories

### 2A. Missing Loot Template Entries (~410,000 errors)

Creatures/gameobjects/spells/items reference a loot template ID, but that ID doesn't
exist in the corresponding `*_loot_template` table.

```
Table 'creature_loot_template' Entry 138993 does not exist but it is used by Creature 138993   (~385,000)
Table 'reference_loot_template' Entry N does not exist but it is used by Reference N            (~23,500)
Table 'skinning_loot_template' Entry N does not exist but it is used by Creature N              (~10,200)
Table 'pickpocketing_loot_template' Entry N does not exist but it is used by Creature N         (~7,500)
Table 'gameobject_loot_template' Entry N does not exist but it is used by Gameobject N          (~4,000)
Table 'spell_loot_template' Entry N does not exist but it is used by Spell N                    (~2,200)
Table 'item_loot_template' Entry N does not exist but it is used by Item N                      (~1,900)
Table 'disenchant_loot_template' Entry N does not exist but it is used by ...                   (~400)
Table 'milling_loot_template' Entry N does not exist but it is used by Item N                   (~49)
Table 'prospecting_loot_template' Entry N does not exist but it is used by Item N               (~29)
```

**Fix strategy**: Clear the loot reference on the source entity so it doesn't point to
a non-existent template. For creatures, this means setting `lootid`/`skinloot`/`pickpocketloot`
to 0 in `creature_template_difficulty`. For gameobjects, clear `data1` (chest loot) or
the relevant data field. For items, clear the loot reference columns.

```sql
-- Example: Clear creature_loot_template references for creatures that have no loot defined
-- First extract the list of bad entries from the error log
-- Then:
UPDATE creature_template_difficulty SET LootID = 0
WHERE LootID IN (<list of missing loot entry IDs>)
AND LootID NOT IN (SELECT DISTINCT Entry FROM creature_loot_template);
```

**Important**: This is the SAFE fix. Do NOT create empty loot templates — that would be
meaningless. The creatures simply don't have loot defined for this expansion yet.

### 2B. Non-existent Items in Loot (~7,000 errors)

```
Table 'creature_loot_template' Entry N ItemType N Item N: item does not exist - skipped         (~7,000)
Table 'gameobject_loot_template' Entry N ItemType N Item N: item does not exist - skipped       (~256)
Table 'disenchant_loot_template' Entry N ItemType N Item N: item does not exist - skipped       (~72)
```

**Fix**: DELETE rows from loot tables that reference items not in `item_template`.

```sql
DELETE FROM creature_loot_template
WHERE Item NOT IN (SELECT entry FROM item_template)
AND ItemType = 0;  -- ItemType 0 = regular item
```

### 2C. Unreferenced Loot Templates (~7,200 errors)

Loot template entries exist but nothing references them.

```
Table 'creature_loot_template' Entry N isn't creature entry and not referenced from loot        (~1,300)
Table 'gameobject_loot_template' Entry N isn't gameobject entry and not referenced from loot    (~5,550)
Table 'pickpocketing_loot_template' Entry N isn't creature pickpocket lootid and not referenced (~332)
Table 'skinning_loot_template' Entry N isn't creature skinning id and not referenced from loot  (~121)
Table 'scrapping_loot_template' Entry N isn't scrapping id and not referenced from loot         (~63)
Table 'item_loot_template' Entry N isn't item entry and not referenced from loot                (~31)
```

**Fix**: DELETE the orphaned loot template entries.

```sql
-- Example: Remove unreferenced gameobject loot templates
-- Extract the list of orphaned Entry IDs from the log, then:
DELETE FROM gameobject_loot_template WHERE Entry IN (<orphaned IDs>);
```

### 2D. Probability Issues (~530 errors)

```
Table 'creature_loot_template' entry N group N has total chance > 100%                          (~490+)
Table 'gameobject_loot_template' entry N group N has total chance > 100%                        (~102)
```

**Fix**: Identify the affected groups and normalize their chances to sum to 100% max,
or reduce the highest-chance item proportionally.

### 2E. Quest Required / Low Chance Warnings (~5,300 errors)

```
Table 'creature_loot_template' Entry N ItemType N Item N: quest required will be ignored        (~3,500)
Table 'creature_loot_template' Entry N ItemType N Item N: low chance (X) - skipped              (~1,730+)
Table 'gameobject_loot_template' Entry N ItemType N Item N: MaxCount < MinCount - skipped       (~34)
```

**Fix for quest required**: Remove items from loot tables that have quest-required flag
but the quest doesn't exist.

**Fix for low chance**: DELETE rows with effectively-zero drop chance.

**Fix for MaxCount < MinCount**: Swap MinCount/MaxCount or set MinCount = MaxCount.

## Recommended Execution Order

1. **2B first** — Delete bad items (reduces noise for other queries)
2. **2A next** — Clear dangling loot references (the bulk of errors)
3. **2C** — Clean up orphaned templates
4. **2D** — Fix probability overflows
5. **2E** — Clean up quest/chance warnings

## SQL File Naming

- `2026_02_25_10_world.sql` — Delete non-existent items from loot tables (2B)
- `2026_02_25_11_world.sql` — Clear dangling creature loot references (2A)
- `2026_02_25_12_world.sql` — Clear dangling GO/spell/item/skinning/pickpocket loot refs (2A)
- `2026_02_25_13_world.sql` — Delete orphaned loot templates (2C)
- `2026_02_25_14_world.sql` — Fix probability overflows (2D)
- `2026_02_25_15_world.sql` — Clean up quest-required and low-chance warnings (2E)

## Key Tables
- `creature_template_difficulty` — has `LootID`, `SkinLootID`, `PickPocketLootID`
- `creature_loot_template`, `reference_loot_template`, `skinning_loot_template`, etc.
- `gameobject_template` — `data1` is the loot ID for chest-type GOs
- `item_template` — reference for valid items
- `spell_loot_template` — loot from spell casts

## Key Source Files
- `src/server/game/Loot/LootMgr.cpp` — loot loading validation
- `src/server/game/Entities/Creature/CreatureData.h` — creature template fields
