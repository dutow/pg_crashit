#!/usr/bin/env bash
# Build a postgresql-<PGVER>-crashit .deb for the given PostgreSQL major
# version.  Requires postgresql-server-dev-<PGVER> and dpkg-dev.
set -euo pipefail

PGVER="${1:?usage: build-deb.sh <pg-major-version>}"
PGCONFIG="/usr/lib/postgresql/${PGVER}/bin/pg_config"

CODENAME="$(lsb_release -cs)"
GITDATE="$(date -u +%Y%m%d)"
SHA="$(git rev-parse --short HEAD)"
EXTVER="$(grep -oP "default_version\s*=\s*'\K[^']+" pg_crashit.control)"
DEBVER="${EXTVER}~git${GITDATE}.${SHA}~${CODENAME}"
ARCH="$(dpkg --print-architecture)"
PKG="postgresql-${PGVER}-crashit"
MAINTAINER="${DEB_MAINTAINER:-pg_crashit maintainers <noreply@example.com>}"

make clean PG_CONFIG="$PGCONFIG" >/dev/null 2>&1 || true
make PG_CONFIG="$PGCONFIG"

ROOT="$(pwd)/debroot"
rm -rf "$ROOT"
make install DESTDIR="$ROOT" PG_CONFIG="$PGCONFIG"

mkdir -p "$ROOT/DEBIAN"
{
    echo "Package: ${PKG}"
    echo "Version: ${DEBVER}"
    echo "Architecture: ${ARCH}"
    echo "Maintainer: ${MAINTAINER}"
    echo "Depends: postgresql-${PGVER}"
    echo "Section: database"
    echo "Priority: optional"
    [ -n "${DEB_HOMEPAGE:-}" ] && echo "Homepage: ${DEB_HOMEPAGE}"
    echo "Description: pg_crashit fault-injection extension for PostgreSQL ${PGVER}"
    echo " Deliberately crashes a backend or the postmaster on configurable"
    echo " (controlled or randomized) triggers, for fault-injection testing."
    echo " ."
    echo " This extension exists to break the server on purpose. Do not install"
    echo " it on production systems."
} > "$ROOT/DEBIAN/control"

DEB="${PKG}_${DEBVER}_${ARCH}.deb"
dpkg-deb --root-owner-group --build "$ROOT" "$DEB"
echo "built ${DEB}"
