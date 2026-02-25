---
allowed-tools: Bash(mysql:*), Bash(cat:*), Bash(echo:*), Read
description: Apply a SQL file to a database (world, characters, auth, hotfixes, roleplay)
---

## Context

The user wants to apply a SQL file to a MySQL database.

- MySQL binary: `C:/Program Files/MySQL/MySQL Server 8.0/bin/mysql.exe`
- Credentials: root / admin
- Available databases: world, characters, auth, hotfixes, roleplay
- The worldserver may be running, so always prepend `SET innodb_lock_wait_timeout=120;`

## Arguments

$ARGUMENTS should contain: `<database> <file_path>`

Example: `/apply-sql world sql/fixes/my_fix.sql`

## Your task

1. Parse the database name and file path from $ARGUMENTS
2. Verify the SQL file exists using Read (first 20 lines to confirm content)
3. Apply it using: `echo "SET innodb_lock_wait_timeout=120;" | cat - <file> | "C:/Program Files/MySQL/MySQL Server 8.0/bin/mysql.exe" -u root -padmin <database>`
4. Report success or failure concisely
