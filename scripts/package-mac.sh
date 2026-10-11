#!/bin/bash
# Builds the universal (arm64 + x86_64) app and wraps it in dist/FOHScope-<ver>-mac-universal.dmg.
# Usage: scripts/package-mac.sh [build-dir]   (default: build-universal)
set -euo pipefail
cd "$(dirname "$0")/.."
BUILD=${1:-build-universal}
VER=$(cmake -P scripts/version.cmake)

cmake -B "$BUILD" -G "Unix Makefiles" -DCMAKE_BUILD_TYPE=Release -DCMAKE_OSX_ARCHITECTURES="arm64;x86_64"
cmake --build "$BUILD" --target FOHScope -j"$(sysctl -n hw.ncpu)"

APP="$BUILD/FOHScope_artefacts/Release/FOHScope.app"
codesign --force --deep --sign - "$APP" # ad-hoc: lets Apple Silicon run it; not notarised
lipo -archs "$APP/Contents/MacOS/FOHScope"

STAGE=$(mktemp -d)
cp -R "$APP" "$STAGE/"
ln -s /Applications "$STAGE/Applications"
cp "packaging/READ ME FIRST (Mac).txt" "$STAGE/READ ME FIRST.txt"
cp LICENSE "$STAGE/LICENSE.txt" 2>/dev/null || true

mkdir -p dist
OUT="dist/FOHScope-$VER-mac-universal.dmg"
rm -f "$OUT"
hdiutil create -volname "FOHScope $VER" -srcfolder "$STAGE" -format UDZO -ov "$OUT"
rm -rf "$STAGE"
echo "$OUT"
