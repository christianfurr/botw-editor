#!/bin/bash
set -euo pipefail
REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
mkdir -p "$REPO_ROOT/build"
xcrun clang -std=gnu11 -Wall -Wextra -Werror -Wno-unused-function -O1 \
  -fsanitize=address,undefined -fno-omit-frame-pointer -I "$REPO_ROOT" \
  "$REPO_ROOT/tests/runes_checks.c" -o "$REPO_ROOT/build/runes-checks"
"$REPO_ROOT/build/runes-checks"
