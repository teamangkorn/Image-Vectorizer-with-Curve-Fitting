# Demo  (CLI Demo, Project Guideline 2025 §8, type C)

This project ships a self-contained CLI demo that exercises the three
required pieces (input → process → result) and prints performance
numbers.  No external dependencies; the demo builds the project itself
the first time it runs.

## How to run

```bash
bash demo/cli_demo.sh
```

(Or, equivalently, `make demo` after running `cmake -S . -B build`.)

The demo will:

1. Generate a 160 × 160 synthetic annulus (`data/synthetic/demo_disk.pgm`).
2. Vectorize it three times, in modes `raw` (baseline-0, pixel-outline
   polygon, no simplification), `polyline` (baseline-1, RDP only), and
   `bezier` (the proposed full pipeline).
3. Print a single-screen comparison table — raw vertex count, simplified
   vertex count, output primitive count, output file size, and total
   runtime in milliseconds — and (on macOS / Linux) open the
   proposed-mode SVG in the system viewer.

## Screenshots

`screenshots/demo_session.png` — terminal output of one run.<br>
`screenshots/demo_bezier.svg`  — proposed-mode output rendered in the
browser, with the original raster overlay for visual comparison.

## Video

A 1–2 minute demo video is hosted at the URL listed in `video_link.txt`.
