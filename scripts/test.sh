#!/bin/bash
set -euo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="$REPO_ROOT/build"

if [[ "$(uname -s)" != "Darwin" || "$(uname -m)" != "arm64" ]]; then
    printf 'The editor checks require an Apple silicon Mac.\n' >&2
    exit 1
fi

mkdir -p "$BUILD_DIR"
xcrun swiftc -O -target arm64-apple-macosx26.0 -D MODEL_CHECKS \
    "$REPO_ROOT/Sources/PlaygroundModel.swift" \
    "$REPO_ROOT/Sources/ModelChecks.swift" \
    -o "$BUILD_DIR/model-checks"
"$BUILD_DIR/model-checks"
# Compile the real UI formatting helpers without starting an app or game session.
xcrun swiftc -O -target arm64-apple-macosx26.0 -D UI_SNAPSHOT \
    "$REPO_ROOT/Sources/PlaygroundModel.swift" \
    "$REPO_ROOT/Sources/BOTWPlayground.swift" \
    "$REPO_ROOT/tests/ui_value_checks.swift" \
    -o "$BUILD_DIR/ui-value-checks" -framework SwiftUI -framework AppKit
"$BUILD_DIR/ui-value-checks"
bash "$REPO_ROOT/scripts/test-runtime.sh"
python3 "$REPO_ROOT/scripts/test_install_bridge.py"
