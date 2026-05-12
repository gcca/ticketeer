#!/bin/sh
set -eu

mkdir -p "$(dirname "${DB_PATH}")"
export DATABASE_URL="sqlite:${DB_PATH}"
dbmate --migrations-dir /app/migrations --no-dump-schema up

if [ "$#" -eq 0 ]; then
  set -- ticketeer
fi

exec "$@"
