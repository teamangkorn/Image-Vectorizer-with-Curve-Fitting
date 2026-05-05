#!/usr/bin/env bash
# demo/cli_demo.sh
#
# Mandatory CLI demo (Project Guideline 2025, section 8 / type C).
#
# Shows the three required pieces of information:
#   1. the input image (a synthetic annulus written to data/synthetic/),
#   2. the algorithm working in three modes (raw / polyline / bezier),
#   3. the result -- a smooth SVG plus performance numbers
#      (raw_v -> simp_v -> Bezier segments, runtime in ms).
#
# Run with:   bash demo/cli_demo.sh
# (or the equivalent `make demo` shortcut documented in README.md.)

set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
BUILD="$ROOT/build"
DATA="$ROOT/data/synthetic"
SHOTS="$ROOT/demo/screenshots"
GEN="$BUILD/gen_synthetic"
VEC="$BUILD/vectorizer"

# 0. Build if necessary -------------------------------------------------------
if [[ ! -x "$VEC" || ! -x "$GEN" ]]; then
    echo ">>> building..."
    cmake -S "$ROOT" -B "$BUILD" -DCMAKE_BUILD_TYPE=Release >/dev/null
    cmake --build "$BUILD" -j >/dev/null
fi
mkdir -p "$DATA" "$SHOTS"

# 1. INPUT -------------------------------------------------------------------
echo
echo "================================================================="
echo " STEP 1 / 3   INPUT: synthetic annulus (160 x 160 px)"
echo "================================================================="
IMG="$DATA/demo_disk.pgm"
"$GEN" "$IMG" 64 22 >/dev/null
echo "    wrote $IMG"

# 2. ALGORITHM ---------------------------------------------------------------
echo
echo "================================================================="
echo " STEP 2 / 3   PROCESS: pipeline in three modes"
echo "================================================================="
HDR="mode      | raw_v  simp_v  out_seg  out_bytes   total_ms"
echo "$HDR"
echo "----------+-----------------------------------------------"
for mode in raw polyline bezier; do
    LINE=$("$VEC" "$IMG" --mode "$mode" -o "$DATA/demo_${mode}.svg" \
                  -e 1.0 -b 1.0 --bench --quiet)
    awk -F, -v m="$mode" '{
        printf "%-9s | %5d  %6d  %7d  %9d   %8.2f\n",
               m, $4, $5, $6, $7, $12
    }' <<< "$LINE"
done

# 3. RESULT ------------------------------------------------------------------
echo
echo "================================================================="
echo " STEP 3 / 3   RESULT: vector outputs in $DATA"
echo "================================================================="
for f in "$DATA"/demo_*.svg; do
    printf '    %-40s  %d bytes\n' "$(basename "$f")" "$(wc -c < "$f")"
done

# Try to open the proposed-mode SVG (macOS / Linux) -- safe to ignore failure.
TARGET="$DATA/demo_bezier.svg"
if command -v open >/dev/null 2>&1; then
    echo
    echo "    opening $TARGET ..."
    open "$TARGET" || true
elif command -v xdg-open >/dev/null 2>&1; then
    echo
    echo "    opening $TARGET ..."
    xdg-open "$TARGET" >/dev/null 2>&1 || true
fi

echo
echo "Demo complete.  See README.md (\"Demo\") for screenshots and a video link."
