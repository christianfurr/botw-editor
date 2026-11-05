#!/bin/bash
set -euo pipefail
REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
mkdir -p "$REPO_ROOT/build"
xcrun clang -std=c11 -Wall -Wextra -Werror -fsanitize=address,undefined -g \
  -I "$REPO_ROOT/runtime" "$REPO_ROOT/tests/temporal_config_checks.c" \
  -o "$REPO_ROOT/build/temporal-config-checks"
"$REPO_ROOT/build/temporal-config-checks"
xcrun clang -std=gnu11 -O1 -Wall -Wextra -Werror -fobjc-arc \
  -target arm64-apple-macosx26.0 "$REPO_ROOT/tests/temporal_gpu.m" \
  -framework Foundation -framework Metal -framework MetalFX \
  -o "$REPO_ROOT/build/temporal-gpu-checks"
if "$REPO_ROOT/build/temporal-gpu-checks"; then
  exit 0
else
  result=$?
fi
if [[ "$result" == 77 && "${CI:-}" == true ]]; then
  printf 'SKIP temporal GPU execution: CI has no capable Metal device. GPU frames were not tested.\n'
  exit 0
fi
exit "$result"
