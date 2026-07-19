#!/bin/sh
set -eu

mkdir -p "$(dirname "${DB_URL}")"
export DATABASE_URL="sqlite:${DB_URL}"
dbmate --migrations-dir /app/migrations --no-dump-schema up

if [ "$#" -eq 0 ]; then
  set -- purrboss --port "${PORT}" --database "${DB_URL}" \
    --default-ttl-seconds "${DEFAULT_TTL_SECONDS}"
fi

exec "$@"
