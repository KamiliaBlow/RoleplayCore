# DB Error Fix Workflows

These workflow documents break down the ~1,032,000 worldserver startup errors into
independent workstreams that can be tackled by separate Claude Code agents in parallel.

## Error Summary

| # | Workflow | Errors | Database | File |
|---|---------|--------|----------|------|
| 1 | Hotfix Blob Migration | ~6,700 | hotfixes | `01_hotfix_blob_migration.md` |
| 2 | Loot Tables | ~443,000 | world | `02_loot_table_fixes.md` |
| 3 | Creature Spawn & Template | ~209,000 | world | `03_creature_spawn_template_fixes.md` |
| 4 | SmartAI | ~173,000 | world | `04_smartai_fixes.md` |
| 5 | Quest Data | ~16,000 | world | `05_quest_data_fixes.md` |
| 6 | Gameobject, Gossip & Misc | ~25,000+ | world | `06_gameobject_gossip_misc_fixes.md` |

## How to Use

1. Open a separate Claude Code session/agent for each workflow
2. Give the agent the workflow document as context
3. Each workflow produces `.sql` files in `sql/updates/world/master/` or `sql/updates/hotfixes/master/`
4. Naming convention: `2026_02_25_XX_<db>.sql` — coordinate XX sequence numbers across agents
5. After generating SQL, apply it to your local DB and re-test worldserver boot

## Important Notes

- All SQL must be **idempotent** (safe to run multiple times)
- Prefer DELETE/UPDATE over DROP — never drop tables
- Always back up the database before applying fixes
- Some errors are **informational** and auto-corrected at runtime — these are low priority
- The agent should generate SQL that fixes the root data, not suppress the log messages
- Test with a worldserver boot after each batch of fixes

## Sequence Number Allocation

To avoid file naming collisions, each agent should use its own range:

| Agent | Sequence Range |
|-------|---------------|
| 1 — Hotfix Blob | `2026_02_25_00` through `_09` |
| 2 — Loot Tables | `2026_02_25_10` through `_19` |
| 3 — Creature | `2026_02_25_20` through `_29` |
| 4 — SmartAI | `2026_02_25_30` through `_39` |
| 5 — Quest | `2026_02_25_40` through `_49` |
| 6 — GO/Gossip/Misc | `2026_02_25_50` through `_59` |
