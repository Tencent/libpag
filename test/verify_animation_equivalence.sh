#!/usr/bin/env bash
# P2-7 animation equivalence acceptance (design doc 13.4):
#   For every animated sample (animation, state_machine, viewmodel, static_animated), render
#   the PAGX XML source and the exported PAG v4 binary at multiple playback times and compare
#   pixel-by-pixel. The default timeline drives both sides (C23), so a StateMachine sample
#   advances from its initial state at the same frame steps on both.
# Usage: bash test/verify_animation_equivalence.sh [time...]
#   Custom time points in seconds (defaults: 0 0.5 1.0).

set -u
REPO="$(cd "$(dirname "$0")/.." && pwd)"
PAGX_BIN="$REPO/cmake-build-debug/pagx"
OUT_DIR="$REPO/test/out/animation_equivalence"
TIMES=("$@")
if [ ${#TIMES[@]} -eq 0 ]; then
  TIMES=(0 0.5 1.0)
fi

SECTIONS=(
  "resources/pagx_to_pag/animation"
  "resources/pagx_to_pag/state_machine"
  "resources/pagx_to_pag/viewmodel"
  "resources/pagx_to_pag/static_animated"
)

mkdir -p "$OUT_DIR"
total=0
passed=0
failed=0

for srcdir in "${SECTIONS[@]}"; do
  section="$(basename "$srcdir")"
  shopt -s nullglob
  files=("$srcdir"/*.pagx)
  shopt -u nullglob
  [ ${#files[@]} -eq 0 ] && continue
  echo "== $section (${#files[@]} samples) =="
  for pagx in "${files[@]}"; do
    name="$(basename "$pagx" .pagx)"
    total=$((total + 1))
    ok=1
    worst=0
    for t in "${TIMES[@]}"; do
      if ! "$PAGX_BIN" export --input "$pagx" --output "$OUT_DIR/$name.pag" --format pag >/dev/null 2>&1; then
        ok=0
        break
      fi
      if ! "$PAGX_BIN" render --time "$t" -o "$OUT_DIR/${name}_x.png" "$pagx" >/dev/null 2>&1; then
        ok=0
        break
      fi
      if ! "$PAGX_BIN" render --time "$t" -o "$OUT_DIR/${name}_v.png" "$OUT_DIR/$name.pag" >/dev/null 2>&1; then
        ok=0
        break
      fi
      d=$(python3 -c "
import sys
from PIL import Image
import numpy as np
a = Image.open(sys.argv[1]).convert('RGBA')
b = Image.open(sys.argv[2]).convert('RGBA')
if a.size != b.size:
    print(-1)
else:
    d = np.abs(np.asarray(a, dtype=np.int16) - np.asarray(b, dtype=np.int16))
    print(int((d.max(axis=2) > 0).sum()))
" "$OUT_DIR/${name}_x.png" "$OUT_DIR/${name}_v.png")
      if [ "$d" != "0" ]; then
        ok=0
        worst=$d
        break
      fi
    done
    if [ $ok -eq 1 ]; then
      passed=$((passed + 1))
    else
      failed=$((failed + 1))
      echo "  FAIL $name (t=$t, differing pixels: $worst)"
    fi
  done
done

echo ""
echo "Animation equivalence: $passed passed, $failed failed (of $total samples x ${#TIMES[@]} time points)"
[ $failed -eq 0 ]
