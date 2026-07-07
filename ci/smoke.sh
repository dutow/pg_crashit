#!/usr/bin/env bash
# Basic smoke test: load pg_crashit into a throwaway cluster and exercise the
# crash-now path.  Expects PostgreSQL $1 installed under /usr/lib/postgresql.
set -euo pipefail

PGVER="${1:?usage: smoke.sh <pg-major-version>}"
export PATH="/usr/lib/postgresql/${PGVER}/bin:${PATH}"

PGDATA="$(mktemp -d)/data"
PORT=5599
SOCKDIR="$(mktemp -d)"

initdb -D "$PGDATA" -U postgres -A trust >/dev/null
{
    echo "shared_preload_libraries = 'pg_crashit'"
    echo "port = ${PORT}"
    echo "unix_socket_directories = '${SOCKDIR}'"
    echo "restart_after_crash = on"
} >> "$PGDATA/postgresql.conf"

cleanup() { pg_ctl -D "$PGDATA" -m immediate stop >/dev/null 2>&1 || true; }
trap cleanup EXIT

pg_ctl -D "$PGDATA" -l "$PGDATA/server.log" -w start

run() { psql -h "$SOCKDIR" -p "$PORT" -U postgres -d postgres -v ON_ERROR_STOP=1 "$@"; }

run -c "CREATE EXTENSION pg_crashit;"
run -c "SELECT * FROM crashit_status();"

# backend_fatal must terminate the session but leave the server running
if run -c "SELECT crashit_crash_backend('backend_fatal');"; then
    echo "FAIL: expected the session to be terminated" >&2
    exit 1
fi
if [ "$(run -tAc 'SELECT 1;')" != "1" ]; then
    echo "FAIL: server not reachable after backend_fatal" >&2
    exit 1
fi

echo "smoke test passed for PostgreSQL ${PGVER}"
