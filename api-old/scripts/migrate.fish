#!/usr/bin/env fish

set -l migration migrations/001-up_initial.sql

if not test -f $migration
    echo "Migration file not found: $migration" >&2
    exit 1
end

env \
    PGDATABASE=ticketeer \
    PGUSER=postgres \
    PGPASSWORD= \
    psql -f $migration
