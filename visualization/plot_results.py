#!/usr/bin/env python3
"""visualization/plot_results.py

Plot the CSV produced by experiments/benchmark.sh.
Generates two PNGs in experiments/results/:

  runtime_vs_n.png   -- log-log runtime, baseline vs proposed
  size_vs_n.png      -- output file size, baseline vs proposed

Usage:
    python3 visualization/plot_results.py
"""
import csv
import os
import sys

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), os.pardir))
CSV  = os.path.join(ROOT, "experiments", "results", "runtime.csv")
OUT  = os.path.join(ROOT, "experiments", "results")

try:
    import matplotlib
    matplotlib.use("Agg")
    import matplotlib.pyplot as plt
except ImportError:
    sys.stderr.write("matplotlib is not installed; skipping plot generation.\n"
                     "  pip install matplotlib\n")
    sys.exit(0)

if not os.path.isfile(CSV):
    sys.stderr.write(f"missing {CSV}; run experiments/benchmark.sh first.\n")
    sys.exit(1)

# label -> approximate vertex count (matches the SIZES table in benchmark.sh).
N = {"1k": 1_000, "10k": 10_000, "100k": 100_000}
data = {"raw": [], "polyline": [], "bezier": []}

with open(CSV) as f:
    for row in csv.DictReader(f):
        n = N.get(row["n"])
        if n is None:
            continue
        data[row["mode"]].append(
            (n, float(row["t_total"]), int(row["out_bytes"]), int(row["out_seg"]))
        )

for k in data:
    data[k].sort()

# ---------- runtime ---------------------------------------------------------
fig, ax = plt.subplots(figsize=(6, 4))
for mode, marker in (("raw", "o"), ("polyline", "s"), ("bezier", "^")):
    if not data[mode]:
        continue
    xs = [x for x, *_ in data[mode]]
    ys = [t for _, t, *_ in data[mode]]
    ax.plot(xs, ys, marker=marker, label=mode)
ax.set_xscale("log"); ax.set_yscale("log")
ax.set_xlabel("input boundary vertices  n"); ax.set_ylabel("total runtime  (ms)")
ax.set_title("Runtime vs input size")
ax.grid(True, which="both", linestyle="--", alpha=0.4)
ax.legend()
fig.tight_layout()
fig.savefig(os.path.join(OUT, "runtime_vs_n.png"), dpi=140)

# ---------- output size -----------------------------------------------------
fig, ax = plt.subplots(figsize=(6, 4))
for mode, marker in (("raw", "o"), ("polyline", "s"), ("bezier", "^")):
    if not data[mode]:
        continue
    xs = [x for x, *_ in data[mode]]
    ys = [b for _, _, b, _ in data[mode]]
    ax.plot(xs, ys, marker=marker, label=mode)
ax.set_xscale("log"); ax.set_yscale("log")
ax.set_xlabel("input boundary vertices  n"); ax.set_ylabel("output SVG size  (bytes)")
ax.set_title("Output size vs input size")
ax.grid(True, which="both", linestyle="--", alpha=0.4)
ax.legend()
fig.tight_layout()
fig.savefig(os.path.join(OUT, "size_vs_n.png"), dpi=140)

print("wrote runtime_vs_n.png and size_vs_n.png to", OUT)
