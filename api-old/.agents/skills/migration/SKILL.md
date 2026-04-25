---
name: migration
description: Create a new SQL migration pair (up + down) in the migrations/ directory. Use when adding or modifying database schema.
argument-hint: <description>
disable-model-invocation: true
allowed-tools: Read Write Bash Glob
context: fork
---

Create a migration pair for: $ARGUMENTS

## Discover context first

- Existing migrations: !`ls migrations/ 2>/dev/null | sort`
- Migration script: !`cat scripts/migrate.fish 2>/dev/null || echo "not found"`
- Current schema (last up migration): !`cat migrations/$(ls migrations/ | grep 'up' | sort | tail -1) 2>/dev/null`

Use the existing migrations to:
1. Determine the next sequence number (pad to 3 digits, e.g. `002`, `003`)
2. Understand the existing schema so the new migration is consistent
3. Understand the DB connection convention from the migrate script

## Files to create

### `migrations/<NNN>-up_<description>.sql`

Forward migration. Follow the schema conventions observed in existing migrations:
- Use the same PostgreSQL schema prefix already in use
- `CREATE TABLE` statements include `id` as primary key, `created_at TIMESTAMPTZ NOT NULL DEFAULT now()`
- Foreign keys reference existing tables with `REFERENCES <schema>.<table>(id)`
- Enums use `CREATE TYPE <schema>.<name> AS ENUM (...)`
- Place `CREATE TYPE` before the table that uses it
- Use `IF NOT EXISTS` where appropriate

### `migrations/<NNN>-down_<description>.sql`

Reverse migration. Drop objects in reverse dependency order:
- `DROP TABLE IF EXISTS` before `DROP TYPE IF EXISTS`
- `DROP TYPE IF EXISTS` before dependent schemas
- Mirror exactly what the up migration creates

## Update migrate script

Read `scripts/migrate.fish` (if it exists). If the script references a specific migration file by name, update it to point to the new up migration. If it runs all pending migrations automatically, no change is needed.

## Naming convention

Derive `<description>` from `$ARGUMENTS`:
- lowercase, words separated by underscores
- descriptive of the change: `add_helpdesk_profile`, `add_ticket_table`, `alter_auth_user_add_email`

Final filenames: `migrations/<NNN>-up_<description>.sql` and `migrations/<NNN>-down_<description>.sql`
