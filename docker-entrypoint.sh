#!/bin/sh
set -eu

mkdir -p "$(dirname "${DB_URL}")"
export DATABASE_URL="sqlite:${DB_URL}"
dbmate --migrations-dir /app/migrations --no-dump-schema up

if [ "$#" -eq 0 ]; then
  set -- ticketeer --bind 0.0.0.0 --port 5521
fi

exec "$@"
