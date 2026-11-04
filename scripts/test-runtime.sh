#!/bin/bash
set -euo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="$REPO_ROOT/build"

if [[ "$(uname -s)" != "Darwin" || "$(uname -m)" != "arm64" ]]; then
    printf 'The runtime guard checks require an Apple silicon Mac.\n' >&2
    exit 1
fi

mkdir -p "$BUILD_DIR"
xcrun clang -std=gnu11 -Wall -Wextra -Werror -O1 \
    -I "$REPO_ROOT" "$REPO_ROOT/tests/runtime_checks.c" \
    -o "$BUILD_DIR/runtime-checks"
"$BUILD_DIR/runtime-checks"
