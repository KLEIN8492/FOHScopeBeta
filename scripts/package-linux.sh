#!/bin/bash
# Packages a built FOHScope binary into dist/FOHScope-<ver>-linux-x86_64.AppImage with linuxdeploy.
# Usage: scripts/package-linux.sh [build-dir]   (default: build). Needs linuxdeploy-x86_64.AppImage
# on PATH or in ./tools (CI downloads it).
set -euo pipefail
cd "$(dirname "$0")/.."
BUILD=${1:-build}
VER=$(cmake -P scripts/version.cmake)
LINUXDEPLOY=$(command -v linuxdeploy-x86_64.AppImage || echo ./tools/linuxdeploy-x86_64.AppImage)

APPDIR=$(mktemp -d)/AppDir
install -Dm755 "$BUILD/FOHScope_artefacts/Release/FOHScope" "$APPDIR/usr/bin/FOHScope"
install -Dm644 packaging/linux/fohscope.desktop "$APPDIR/usr/share/applications/fohscope.desktop"
install -Dm644 Resources/fohscope.png "$APPDIR/usr/share/icons/hicolor/256x256/apps/fohscope.png"
install -Dm644 LICENSE "$APPDIR/usr/share/doc/fohscope/LICENSE"
install -Dm644 THIRD_PARTY_NOTICES.md "$APPDIR/usr/share/doc/fohscope/THIRD_PARTY_NOTICES.md"

mkdir -p dist
# APPIMAGE_EXTRACT_AND_RUN: no FUSE needed on CI runners. ALSA / JACK client libraries are
# loaded from the user's system (JACK at runtime only), as distros expect.
APPIMAGE_EXTRACT_AND_RUN=1 OUTPUT="dist/FOHScope-$VER-linux-x86_64.AppImage" \
  "$LINUXDEPLOY" --appdir "$APPDIR" --output appimage \
  --exclude-library 'libjack*' --exclude-library 'libasound*'
ls -la dist
