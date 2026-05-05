// Raster -> Vector image converter.
//
// Pipeline:
//   1.  Load image (stb_image) and binarize against a luma threshold.
//   2.  Trace pixel-outline contours of every connected component
//       (Moore-Neighbour boundary tracing) and detect interior holes.
//   3.  Simplify each polyline with Ramer-Douglas-Peucker.
//   4.  (Optional) Fit cubic Bezier curves with Schneider's algorithm,
//       which uses de Casteljau evaluation throughout.
//   5.  Emit SVG.

#include <chrono>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <string>

#include "bezier.hpp"
#include "image.hpp"
#include "rdp.hpp"
#include "svg.hpp"
#include "tracer.hpp"

namespace {

// Three pipelines, used to compare a naive baseline against the proposed
// approach (see report §Baseline / §Proposed Approach).
//   raw       baseline-0: emit the raw pixel-outline polygon, no
//             simplification, no curve fitting.
//   polyline  baseline-1: RDP-only polyline output (no curves).
//   bezier    proposed:  RDP + Schneider de-Casteljau Bézier fit.
enum class Mode { Raw, Polyline, Bezier };

struct Options {
    std::string input;
    std::string output      = "out.svg";
    int    threshold        = 128;
    bool   invert           = false;
    double epsilon          = 1.5;     // RDP tolerance, pixels
    double bezierError      = 2.0;     // max sqrt-error for Bezier fit, pixels
    Mode   mode             = Mode::Bezier;
    int    minArea          = 4;       // ignore tiny specks (pixels)
    std::string fill        = "#000000";
    bool   bench            = false;   // print one CSV-style line on stdout
    bool   quiet            = false;   // suppress per-stage progress messages
};

using Clock = std::chrono::steady_clock;
static double ms_since(Clock::time_point t0) {
    using namespace std::chrono;
    return duration<double, std::milli>(Clock::now() - t0).count();
}

void usage(const char* argv0) {
    std::cerr <<
"Usage: " << argv0 << " <input-image> [options]\n"
"\n"
"Converts a raster image (PNG/JPG/BMP) into a scalable SVG.\n"
"\n"
"Options:\n"
"  -o, --output <file>      Output SVG path (default: out.svg)\n"
"  -t, --threshold <int>    Binarization threshold 0..255 (default: 128)\n"
"      --invert             Treat lighter pixels as foreground\n"
"  -e, --epsilon <float>    RDP simplification tolerance, in pixels (default: 1.5)\n"
"  -b, --bezier-error <f>   Max Bezier fitting error, in pixels (default: 2.0)\n"
"  -m, --mode <name>        Pipeline mode: 'raw' (baseline-0, no simplification),\n"
"                           'polyline' (baseline-1, RDP only) or\n"
"                           'bezier' (proposed, default).\n"
"      --no-curves          Alias for --mode polyline\n"
"      --min-area <int>     Drop components smaller than N pixels (default: 4)\n"
"      --fill <color>       SVG fill color (default: #000000)\n"
"      --bench              Emit one CSV line on stdout:\n"
"                           mode,W,H,raw_v,simp_v,out_seg,out_bytes,t_load,t_trace,t_simp,t_fit,t_total\n"
"      --quiet              Suppress progress messages on stderr\n"
"  -h, --help               Show this message\n";
}

bool parseArgs(int argc, char** argv, Options& opt) {
    if (argc < 2) return false;
    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        auto need = [&](const char* name) -> const char* {
            if (i + 1 >= argc) {
                std::cerr << "Missing value for " << name << '\n';
                std::exit(2);
            }
            return argv[++i];
        };
        if      (a == "-h" || a == "--help")                 { return false; }
        else if (a == "-o" || a == "--output")               { opt.output      = need("--output"); }
        else if (a == "-t" || a == "--threshold")            { opt.threshold   = std::atoi(need("--threshold")); }
        else if (a == "--invert")                            { opt.invert      = true; }
        else if (a == "-e" || a == "--epsilon")              { opt.epsilon     = std::atof(need("--epsilon")); }
        else if (a == "-b" || a == "--bezier-error")         { opt.bezierError = std::atof(need("--bezier-error")); }
        else if (a == "--no-curves")                         { opt.mode        = Mode::Polyline; }
        else if (a == "-m" || a == "--mode") {
            std::string v = need("--mode");
            if      (v == "raw")      opt.mode = Mode::Raw;
            else if (v == "polyline") opt.mode = Mode::Polyline;
            else if (v == "bezier")   opt.mode = Mode::Bezier;
            else { std::cerr << "Unknown mode: " << v << " (raw|polyline|bezier)\n"; return false; }
        }
        else if (a == "--min-area")                          { opt.minArea     = std::atoi(need("--min-area")); }
        else if (a == "--fill")                              { opt.fill        = need("--fill"); }
        else if (a == "--bench")                             { opt.bench       = true; }
        else if (a == "--quiet")                             { opt.quiet       = true; }
        else if (!a.empty() && a[0] == '-') {
            std::cerr << "Unknown option: " << a << '\n';
            return false;
        }
        else if (opt.input.empty())                          { opt.input = a; }
        else {
            std::cerr << "Unexpected positional argument: " << a << '\n';
            return false;
        }
    }
    return !opt.input.empty();
}

} // namespace

int main(int argc, char** argv) {
    Options opt;
    if (!parseArgs(argc, argv, opt)) {
        usage(argv[0]);
        return 1;
    }

    auto log = [&](const std::string& s) { if (!opt.quiet) std::cerr << s; };
    const char* modeName =
        opt.mode == Mode::Raw      ? "raw"      :
        opt.mode == Mode::Polyline ? "polyline" : "bezier";

    try {
        const auto t_total0 = Clock::now();

        log("[1/4] Loading & binarizing '" + opt.input + "'...\n");
        const auto t0 = Clock::now();
        const auto img = vec::loadAndBinarize(opt.input, opt.threshold, opt.invert);
        const double t_load = ms_since(t0);
        log("      image: " + std::to_string(img.width) + " x "
            + std::to_string(img.height) + "\n");

        log("[2/4] Tracing pixel outlines...\n");
        const auto t1 = Clock::now();
        auto contours = vec::traceAll(img, opt.minArea);
        const double t_trace = ms_since(t1);
        std::size_t rawPts = 0;
        for (const auto& c : contours) {
            rawPts += c.outer.size();
            for (const auto& h : c.holes) rawPts += h.size();
        }
        log("      " + std::to_string(contours.size()) + " contour(s), "
            + std::to_string(rawPts) + " raw vertices\n");

        // Stage 3: simplification (skipped in baseline `raw` mode).
        double t_simp = 0.0;
        std::size_t simpPts = rawPts;
        if (opt.mode != Mode::Raw) {
            log("[3/4] RDP simplification (eps = " + std::to_string(opt.epsilon) + ")...\n");
            const auto t2 = Clock::now();
            simpPts = 0;
            for (auto& c : contours) {
                c.outer = vec::simplifyRDP(c.outer, opt.epsilon);
                simpPts += c.outer.size();
                for (auto& h : c.holes) {
                    h = vec::simplifyRDP(h, opt.epsilon);
                    simpPts += h.size();
                }
            }
            t_simp = ms_since(t2);
            log("      " + std::to_string(simpPts) + " vertices after RDP\n");
        } else {
            log("[3/4] (baseline raw) skipping RDP\n");
        }

        // Stage 4: emission.  Counts the geometric primitives in the
        // output (vertices for polyline modes, cubic segments for Bezier).
        std::string svg;
        std::size_t outSegments = 0;
        double t_fit = 0.0;
        if (opt.mode == Mode::Bezier) {
            log("[4/4] Fitting Bezier curves (err = " + std::to_string(opt.bezierError) + ")...\n");
            const auto t3 = Clock::now();
            std::vector<vec::SvgPath> paths;
            paths.reserve(contours.size());
            for (const auto& c : contours) {
                vec::SvgPath p;
                p.rings.push_back(vec::fitCurves(c.outer, opt.bezierError));
                outSegments += p.rings.back().size();
                for (const auto& h : c.holes) {
                    p.rings.push_back(vec::fitCurves(h, opt.bezierError));
                    outSegments += p.rings.back().size();
                }
                paths.push_back(std::move(p));
            }
            t_fit = ms_since(t3);
            log("      " + std::to_string(outSegments) + " cubic Bezier segments\n");
            svg = vec::writeBezierSvg(img.width, img.height, paths, opt.fill);
        } else {
            // polyline / raw: write as polygons, count vertices.
            log("[4/4] Writing polyline SVG (" + std::string(modeName) + ")...\n");
            outSegments = simpPts;
            svg = vec::writePolylineSvg(img.width, img.height, contours, opt.fill);
        }

        std::ofstream f(opt.output);
        if (!f) {
            std::cerr << "Failed to open '" << opt.output << "' for writing\n";
            return 1;
        }
        f << svg;
        log("Wrote " + opt.output + "\n");

        const double t_total = ms_since(t_total0);
        const std::size_t outBytes = svg.size();

        if (opt.bench) {
            // CSV line consumed by experiments/benchmark.sh.
            std::cout
                << modeName        << ','
                << img.width       << ',' << img.height   << ','
                << rawPts          << ',' << simpPts      << ','
                << outSegments     << ',' << outBytes     << ','
                << t_load          << ',' << t_trace      << ','
                << t_simp          << ',' << t_fit        << ','
                << t_total         << '\n';
        }
    }
    catch (const std::exception& ex) {
        std::cerr << "Error: " << ex.what() << '\n';
        return 1;
    }
    return 0;
}
