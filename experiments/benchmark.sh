#!/usr/bin/env bash
# experiments/benchmark.sh
#
# Scaling experiment for the Computational Geometry final project.
#
# Generates synthetic images of three sizes that produce raw boundary
# polylines of approximately n = 1k / 10k / 100k vertices, then runs
# the vectorizer in three modes:
#
#   raw        baseline-0  -- pixel-outline polygon, no simplification
#   polyline   baseline-1  -- RDP-only polyline output
#   bezier     proposed    -- RDP + Schneider + de Casteljau Bezier fit
#
# Results are written as CSV to experiments/results/runtime.csv with
# columns:
#   n,mode,W,H,raw_v,simp_v,out_seg,out_bytes,t_load,t_trace,t_simp,t_fit,t_total
#
# Each (n, mode) row is the median of REPEAT measurements (default 5).

set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
BUILD="$ROOT/build"
DATA="$ROOT/data/synthetic"
OUT="$ROOT/experiments/results"
GEN="$BUILD/gen_synthetic"
VEC="$BUILD/vectorizer"

REPEAT="${REPEAT:-5}"
EPSILON="${EPSILON:-1.0}"
BERR="${BERR:-1.0}"

# (label, disk-radius, hole-radius, grid-size)
# perimeter ~ 8 * radius * grid * grid     (Moore-Neighbour discretization).
# Grid layouts keep individual rasters small; a single 100k-vertex disk
# would need a ~25k x 25k raster (>600 MB) which exceeds stb_image's
# size limit, whereas a 12 x 12 grid of small disks needs only ~2k x 2k.
SIZES=(
    "1k    128  42   1"
    "10k    80  27   4"
    "100k   85  28  12"
)

# -----------------------------------------------------------------------------
[[ -x "$GEN" && -x "$VEC" ]] || {
    echo "Build the project first:  cmake --build $BUILD -j" >&2
    exit 1
}

mkdir -p "$DATA" "$OUT"
CSV="$OUT/runtime.csv"
echo "n,mode,W,H,raw_v,simp_v,out_seg,out_bytes,t_load,t_trace,t_simp,t_fit,t_total" > "$CSV"

# Run vectorizer once and emit its single CSV --bench line on stdout.
run_one () {
    local mode="$1" img="$2" out="$3"
    "$VEC" "$img" --mode "$mode" -o "$out" \
        -e "$EPSILON" -b "$BERR" --bench --quiet
}

# Median of REPEAT runs, by t_total (column 12).
median_of () {
    sort -t, -k12 -g | awk -v n="$REPEAT" 'NR==int((n+1)/2){print; exit}'
}

for entry in "${SIZES[@]}"; do
    read -r label radius hole grid <<< "$entry"
    img="$DATA/disk_${label}.pgm"

    echo ">>> generating n=$label  (radius=$radius, hole=$hole, grid=${grid}x${grid})"
    "$GEN" "$img" "$radius" "$hole" "$grid" >/dev/null

    for mode in raw polyline bezier; do
        echo "    running mode=$mode  x $REPEAT ..."
        out="$DATA/disk_${label}_${mode}.svg"
        rows=""
        for i in $(seq 1 "$REPEAT"); do
            rows+=$(run_one "$mode" "$img" "$out")$'\n'
        done
        median=$(printf '%s' "$rows" | median_of)
        printf '%s,%s\n' "$label" "$median" >> "$CSV"
    done
done

echo
echo "Done.  Results written to $CSV"
column -s, -t "$CSV"
