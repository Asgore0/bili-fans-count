#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
APP="$SCRIPT_DIR/BILIFansCard.app"
SRC="$SCRIPT_DIR/src/BILIFansCard.m"
INFO_PLIST="$APP/Contents/Info.plist"
OUT_DIR="$ROOT_DIR/outputs"
VERSION="$(/usr/libexec/PlistBuddy -c 'Print :CFBundleShortVersionString' "$INFO_PLIST")"
RAW_SUFFIX="${1:-verified}"
SUFFIX="$RAW_SUFFIX"
if [[ "$SUFFIX" == "v${VERSION}" ]]; then
    SUFFIX="verified"
elif [[ "$SUFFIX" == "v${VERSION}-"* ]]; then
    SUFFIX="${SUFFIX#v${VERSION}-}"
fi
DMG="$OUT_DIR/BILIFansCard-macOS-v${VERSION}-${SUFFIX}.dmg"
TMP_ROOT="$(mktemp -d "/tmp/bilifanscard-v${VERSION}.XXXXXX")"
TMP_APP="$TMP_ROOT/BILIFansCard.app"
TMP_STAGE="$TMP_ROOT/dmg-root"
TMP_DMG="/tmp/BILIFansCard-macOS-v${VERSION}-${SUFFIX}.$$.dmg"
MOUNT_POINT=""

cleanup() {
    if [[ -n "$MOUNT_POINT" && -d "$MOUNT_POINT" ]]; then
        hdiutil detach "$MOUNT_POINT" >/dev/null 2>&1 || true
    fi
    rm -f "$TMP_DMG"
    rm -rf "$TMP_ROOT"
}
trap cleanup EXIT

mkdir -p "$APP/Contents/MacOS" "$OUT_DIR"

clang -fobjc-arc -Wall -Wextra -mmacosx-version-min=12.0 \
    -arch arm64 -arch x86_64 \
    -framework Cocoa \
    -framework QuartzCore \
    -framework UserNotifications \
    -framework ServiceManagement \
    "$SRC" \
    -o "$APP/Contents/MacOS/BILIFansCard"

ditto "$APP" "$TMP_APP"
xattr -cr "$TMP_APP" || true
codesign --force --deep --sign - "$TMP_APP"
codesign --verify --deep --strict --verbose=2 "$TMP_APP"
file "$TMP_APP/Contents/MacOS/BILIFansCard"
plutil -p "$TMP_APP/Contents/Info.plist"
mkdir -p "$TMP_STAGE"
ditto "$TMP_APP" "$TMP_STAGE/BILIFansCard.app"
ln -s /Applications "$TMP_STAGE/Applications"
sync

hdiutil create -volname BILIFansCard -srcfolder "$TMP_STAGE" -ov -format UDZO "$TMP_DMG"
hdiutil verify "$TMP_DMG"

ATTACH_OUTPUT="$(hdiutil attach -readonly -noverify -nobrowse "$TMP_DMG")"
MOUNT_POINT="$(printf '%s\n' "$ATTACH_OUTPUT" | sed -n 's#.*\(/Volumes/.*\)$#\1#p' | tail -n 1)"
if [[ -z "$MOUNT_POINT" || ! -d "$MOUNT_POINT" ]]; then
    printf 'Could not find mounted DMG volume.\n' >&2
    exit 1
fi

codesign --verify --deep --strict --verbose=2 "$MOUNT_POINT/BILIFansCard.app"
file "$MOUNT_POINT/BILIFansCard.app/Contents/MacOS/BILIFansCard"
plutil -p "$MOUNT_POINT/BILIFansCard.app/Contents/Info.plist"
hdiutil detach "$MOUNT_POINT"
MOUNT_POINT=""

ditto "$TMP_DMG" "$DMG"
shasum -a 256 "$DMG"
ls -lh "$DMG"
