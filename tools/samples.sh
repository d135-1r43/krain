#!/usr/bin/env bash
# Stages real test material in samples/ by converting from the Logic / GarageBand
# content already installed on this machine.
#
# The audio itself is Apple factory content and is NOT committed - samples/ is in
# .gitignore. This script is committed so the staging is reproducible on any
# machine that has the same libraries.
set -euo pipefail

root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
out="$root/samples"
mkdir -p "$out"

yamaha="/Library/Application Support/Logic/EXS Factory Samples/01 Acoustic Pianos/Yamaha Grand Piano"
loops="/Library/Audio/Apple Loops/Apple"

stage() { # <source> <destination name>
  local source="$1" name="$2"
  if [ ! -f "$source" ]; then
    echo "  skip  $name (not installed)"
    return
  fi
  # Everything lands as 48 kHz stereo float WAV so the renderer treats it uniformly.
  afconvert -f WAVE -d LEF32@48000 -c 2 "$source" "$out/$name" >/dev/null 2>&1 \
    && echo "  ok    $name" \
    || echo "  FAIL  $name"
}

echo "Staging test material into samples/"
stage "$yamaha/069_A3KM56_M.wav"            "piano-note.wav"
stage "$loops/01 Hip Hop/Epoch Ambient Piano.caf"  "piano-ambient.wav"
stage "$loops/04 Modern RnB/Breathless Piano.caf"  "piano-phrase.wav"

echo
echo "Render one through the grain engine with, for example:"
echo "  ./build/tools/graindelay-render --input samples/piano-ambient.wav"
