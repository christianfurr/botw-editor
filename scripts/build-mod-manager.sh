#!/bin/bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD="$ROOT/build"
APP="$BUILD/manager/MacNative Mods.app"
if [[ "$(uname -s)" != Darwin || "$(uname -m)" != arm64 ]]; then
    printf 'MacNative Mods requires Apple silicon.\n' >&2
    exit 1
fi
mkdir -p "$APP/Contents/MacOS" "$APP/Contents/Resources/Mods"
xcrun swiftc -O -warnings-as-errors -target arm64-apple-macosx26.0 \
    "$ROOT/Sources/MacNativeMods.swift" "$ROOT/Sources/ModsModel.swift" "$ROOT/Sources/ModsView.swift" \
    -o "$APP/Contents/MacOS/MacNativeMods" -framework SwiftUI -framework AppKit

cp "$ROOT/scripts/"{mods,mod_environment,mod_import,mod_demos,extract_destinations,extract_items,extract_map}.py \
    "$APP/Contents/Resources/Mods/"
cp "$ROOT/LICENSE" "$APP/Contents/Resources/"
cat > "$APP/Contents/Info.plist" <<'PLIST'
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "https://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0"><dict>
<key>CFBundleIdentifier</key><string>dev.christianfurr.macnative-mods</string>
<key>CFBundleName</key><string>MacNative Mods</string>
<key>CFBundleExecutable</key><string>MacNativeMods</string>
<key>CFBundlePackageType</key><string>APPL</string>
<key>CFBundleShortVersionString</key><string>0.1.0</string>
<key>CFBundleVersion</key><string>1</string>
<key>LSMinimumSystemVersion</key><string>26.0</string>
<key>LSApplicationCategoryType</key><string>public.app-category.utilities</string>
</dict></plist>
PLIST

/usr/bin/xattr -dr com.apple.FinderInfo "$APP" 2>/dev/null || true
/usr/bin/xattr -dr com.apple.ResourceFork "$APP" 2>/dev/null || true
/usr/bin/codesign --force --sign - "$APP"
/usr/bin/codesign --verify --strict "$APP"

python3 - "$APP" "$BUILD/MacNative.Mods.zip" <<'PY'
import sys
import zipfile
from pathlib import Path

app, archive = map(Path, sys.argv[1:])
with zipfile.ZipFile(archive, "w", compression=zipfile.ZIP_DEFLATED) as output:
    for file in sorted(app.rglob("*")):
        if file.is_file():
            output.write(file, file.relative_to(app.parent))
PY
(cd "$BUILD" && /usr/bin/shasum -a 256 MacNative.Mods.zip > MacNative.Mods.zip.sha256)
printf 'App: %s\nArchive: %s\n' "$APP" "$BUILD/MacNative.Mods.zip"
