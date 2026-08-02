#!/bin/sh
set -eu

mkdir -p "$(dirname "${PURRBOSS_DBPATH}")"
export DATABASE_URL="sqlite:${PURRBOSS_DBPATH}"
dbmate --migrations-dir /app/migrations --no-dump-schema up

if [ "$#" -eq 0 ]; then
  set -- purrboss --port "${PURRBOSS_PORT}" --database "${PURRBOSS_DBPATH}" \
    --default-ttl-seconds "${PURRBOSS_DEFAULT_TTL_SECONDS}"
fi

exec "$@"
