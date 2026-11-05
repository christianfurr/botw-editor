#!/bin/bash
set -euo pipefail
REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
mkdir -p "$REPO_ROOT/build"
xcrun clang -std=gnu11 -O1 -Wall -Wextra -Werror -Wno-unused-function -fobjc-arc \
  -target arm64-apple-macosx26.0 "$REPO_ROOT/tests/postfx_gpu.m" \
  -framework Foundation -framework Metal -o "$REPO_ROOT/build/postfx-gpu-checks"
if "$REPO_ROOT/build/postfx-gpu-checks" "$REPO_ROOT/build"; then
  exit 0
else
  result=$?
fi
if [[ "$result" == 77 && "${CI:-}" == true ]]; then
  if xcrun --find metal >/dev/null 2>&1; then
    xcrun --sdk macosx metal -c "$REPO_ROOT/build/postfx-compile-only.metal" -o "$REPO_ROOT/build/postfx-compile-only.air"
    printf 'SKIP GPU execution: CI has no Metal device. Offline Metal shader compile passed.\n'
  else
    printf 'SKIP GPU execution: CI has no Metal device or offline Metal compiler. GPU assertions were not run.\n'
  fi
  exit 0
fi
exit "$result"
