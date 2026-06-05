#!/usr/bin/env bash
# Capture one screenshot per SEED_STATE (0=list, 1-4=detail).
# Run from the PebbleKuma project root.
set -e
export PATH="$HOME/.local/bin:$PATH"

FRAMES="$(dirname "$0")/gif_frames"
mkdir -p "$FRAMES"
SRC="src/c/pebblekuma.c"

capture() {
  local state=$1
  local out=$2

  # Patch SEED_STATE in-place
  sed -i "s/^#define SEED_STATE .*/#define SEED_STATE $state/" "$SRC"

  pebble build 2>&1 | grep -E '(error:|finished)' | grep -viE 'SyntaxWarning|escape sequence'

  pkill -x qemu-pebble 2>/dev/null || true
  sleep 1

  pebble install --emulator emery 2>&1 | grep -viE 'SyntaxWarning|escape sequence|:param'

  # Wait for animations to settle (uptime count-up = 350ms, heartbeat sweep = 450ms)
  sleep 1

  pebble screenshot --emulator emery "$FRAMES/$out" 2>&1 | grep 'Saved\|Error'
  echo "  -> $out"
}

capture 0 "f1_list.png"
capture 1 "f2_detail_down.png"
capture 2 "f3_detail_pending.png"
capture 3 "f4_detail_up.png"
capture 4 "f5_detail_maint.png"
capture 0 "f6_list_end.png"

echo "All frames captured in $FRAMES"
