---
allowed-tools: Read, Bash(python3:*), Write, Grep
description: Parse a worldserver error log and categorize errors by type with counts
---

## Context

The user wants to parse a worldserver error log to categorize and count errors.

- Error logs are typically large (thousands of lines)
- Common error categories: invalid spell references, missing creatures, bad loot references, SmartAI errors, invalid display IDs, missing quest references, areatrigger issues
- Use Python for parsing — write a temp .py file, run it, delete it
- `grep -P` (PCRE) does NOT work in this bash env — use Python instead

## Arguments

$ARGUMENTS should contain the path to the error log file.

## Your task

1. Read the first 50 lines of the log to understand the format
2. Write a Python script to:
   - Read the full log
   - Categorize errors by type (group by error message pattern)
   - Count occurrences of each category
   - Extract affected IDs for each category
   - Output a summary sorted by count (highest first)
3. Run the script and present the summary
4. Clean up the temp Python file
