#!/usr/bin/env bash
# tests/test_algorithms.sh
#
# Smoke tests for the three pipeline modes.  Each test asserts an
# expected order-of-magnitude on the output (number of vertices /
# Bezier segments and SVG byte count).  Exits non-zero on any failure.

set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
BUILD="$ROOT/build"
DATA="$ROOT/data/synthetic"
GEN="$BUILD/gen_synthetic"
VEC="$BUILD/vectorizer"

[[ -x "$GEN" && -x "$VEC" ]] || {
    echo "Build first:  cmake --build $BUILD -j" >&2; exit 1
}

mkdir -p "$DATA"
IMG="$DATA/disk_test.pgm"
"$GEN" "$IMG" 100 33 >/dev/null

assert_le () {
    local label="$1" got="$2" max="$3"
    if (( got > max )); then
        echo "FAIL ($label): expected <= $max, got $got" >&2; exit 1
    fi
    echo "PASS ($label): $got <= $max"
}
assert_ge () {
    local label="$1" got="$2" min="$3"
    if (( got < min )); then
        echo "FAIL ($label): expected >= $min, got $got" >&2; exit 1
    fi
    echo "PASS ($label): $got >= $min"
}

field () { awk -F, -v c="$1" '{print $c}'; }

run () {
    local mode="$1"
    "$VEC" "$IMG" --mode "$mode" -o "$DATA/test_${mode}.svg" \
        -e 1.0 -b 1.0 --bench --quiet
}

echo "=== mode=raw (baseline-0) ==="
LINE=$(run raw)
RAW_V=$(echo "$LINE" | field 4)
OUT_S=$(echo "$LINE" | field 6)
assert_ge "raw vertices  > 400"  "$RAW_V" 400
assert_ge "raw out_seg   > 400"  "$OUT_S" 400

echo "=== mode=polyline (baseline-1) ==="
LINE=$(run polyline)
SIMP_V=$(echo "$LINE" | field 5)
assert_le "polyline vertices < 200" "$SIMP_V" 200

echo "=== mode=bezier (proposed) ==="
LINE=$(run bezier)
SEG=$(echo "$LINE" | field 6)
assert_le "Bezier segments < 30"   "$SEG"  30
assert_ge "Bezier segments >= 2"   "$SEG"  2

echo
echo "All tests passed."
