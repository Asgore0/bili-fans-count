#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
SRC="$SCRIPT_DIR/src/BILIFansCard.m"
TEST_SRC="$SCRIPT_DIR/tests/UIDParsingTest.m"
TEST_BIN="${TMPDIR:-/tmp}/bilifans-uid-parse-test"

clang -fobjc-arc -Wall -Wextra -Werror -fsyntax-only \
    -mmacosx-version-min=12.0 \
    "$SRC"

clang -fobjc-arc -Wall -Wextra -Werror \
    -mmacosx-version-min=12.0 \
    -framework Cocoa \
    -framework QuartzCore \
    -framework UserNotifications \
    -framework ServiceManagement \
    "$TEST_SRC" \
    -o "$TEST_BIN"

"$TEST_BIN"
