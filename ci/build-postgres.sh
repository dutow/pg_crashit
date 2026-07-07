#!/usr/bin/env bash
# Build PostgreSQL from git with TAP test support and install it to a prefix.
# usage: build-postgres.sh <git-branch> <prefix> <srcdir>
set -euo pipefail

BRANCH="${1:?usage: build-postgres.sh <git-branch> <prefix> <srcdir>}"
PREFIX="${2:?missing prefix}"
SRCDIR="${3:?missing srcdir}"

if [ ! -d "$SRCDIR" ]; then
    git clone --depth 1 --branch "$BRANCH" \
        https://github.com/postgres/postgres.git "$SRCDIR"
fi

cd "$SRCDIR"

CC="cc"
if command -v ccache >/dev/null; then
    CC="ccache cc"
fi

./configure --prefix="$PREFIX" \
    --enable-tap-tests \
    --enable-debug \
    --enable-cassert \
    --without-icu \
    CC="$CC" >/dev/null

make -s -j"$(nproc)"
make -s install
