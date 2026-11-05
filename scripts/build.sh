#!/bin/bash
set -euo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="$REPO_ROOT/build"
APP_PATH="$BUILD_DIR/BOTW Editor.app"
ICONSET_PATH="$BUILD_DIR/BOTWEditor.iconset"

if [[ "$(uname -s)" != "Darwin" || "$(uname -m)" != "arm64" ]]; then
    printf 'BOTW Editor requires an Apple silicon Mac.\n' >&2
    exit 1
fi
SDK_VERSION="$(xcrun --sdk macosx --show-sdk-version)"
if (( ${SDK_VERSION%%.*} < 26 )); then
    printf 'Install Xcode command-line tools with a macOS 26 or newer SDK.\n' >&2
    exit 1
fi

"$REPO_ROOT/scripts/test.sh"
mkdir -p "$APP_PATH/Contents/MacOS" "$APP_PATH/Contents/Resources" "$ICONSET_PATH"
xcrun swiftc -O -target arm64-apple-macosx26.0 \
    "$REPO_ROOT/Sources/PlaygroundModel.swift" \
    "$REPO_ROOT/Sources/GraphicsModel.swift" \
    "$REPO_ROOT/Sources/GraphicsSettingsView.swift" \
    "$REPO_ROOT/Sources/BOTWPlayground.swift" \
    -o "$APP_PATH/Contents/MacOS/BOTWEditor" -framework SwiftUI -framework AppKit

xcrun swiftc "$REPO_ROOT/scripts/generate-icon.swift" -o "$BUILD_DIR/generate-icon" -framework AppKit
"$BUILD_DIR/generate-icon" "$ICONSET_PATH"
/usr/bin/iconutil -c icns "$ICONSET_PATH" -o "$APP_PATH/Contents/Resources/AppIcon.icns"
cp "$REPO_ROOT/Sources/catalog.json" "$REPO_ROOT/Sources/ATTRIBUTION.md" \
    "$REPO_ROOT/Sources/CATALOG-LICENSE.txt" "$REPO_ROOT/LICENSE" \
    "$REPO_ROOT/THIRD_PARTY_NOTICES.md" "$APP_PATH/Contents/Resources/"
cp "$REPO_ROOT/Assets/slate-frame.png" "$APP_PATH/Contents/Resources/"

cat > "$APP_PATH/Contents/Info.plist" <<'PLIST'
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0"><dict>
<key>CFBundleIdentifier</key><string>dev.christianfurr.botw-editor</string>
<key>CFBundleName</key><string>BOTW Editor</string>
<key>CFBundleDisplayName</key><string>BOTW Editor</string>
<key>CFBundleExecutable</key><string>BOTWEditor</string>
<key>CFBundleIconFile</key><string>AppIcon</string>
<key>CFBundlePackageType</key><string>APPL</string>
<key>CFBundleShortVersionString</key><string>0.1.1</string>
<key>CFBundleVersion</key><string>2</string>
<key>LSMinimumSystemVersion</key><string>26.0</string>
<key>NSHighResolutionCapable</key><true/>
</dict></plist>
PLIST

# Finder may attach these display attributes while a bundle is being assembled.
/usr/bin/xattr -dr com.apple.FinderInfo "$APP_PATH" 2>/dev/null || true
/usr/bin/xattr -dr com.apple.ResourceFork "$APP_PATH" 2>/dev/null || true
/usr/bin/codesign --force --sign - "$APP_PATH"
/usr/bin/codesign --verify --strict "$APP_PATH"

# Write only regular app files, preserving executable modes without xattr sidecars.
python3 - "$APP_PATH" "$BUILD_DIR/BOTW Editor.zip" <<'PY'
import os,sys,zipfile
root,output=sys.argv[1:]
with zipfile.ZipFile(output,'w',compression=zipfile.ZIP_DEFLATED) as archive:
    for current,dirs,files in os.walk(root):
        for name in sorted(files):
            path=os.path.join(current,name)
            archive.write(path,os.path.relpath(path,os.path.dirname(root)))
PY
(cd "$BUILD_DIR" && /usr/bin/shasum -a 256 'BOTW Editor.zip' > 'BOTW Editor.zip.sha256')
printf 'App: %s\nArchive: %s\n' "$APP_PATH" "$BUILD_DIR/BOTW Editor.zip"
