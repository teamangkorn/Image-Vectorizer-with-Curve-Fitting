// Synthetic test-image generator for the scaling experiment.
//
// Emits a binary PGM (P5) image containing a `grid x grid` array of
// identical filled disks (radius `r`) with concentric circular holes
// (radius `h`).  The Moore-Neighbour tracer therefore returns
// `2 * grid * grid` closed contours whose total raw vertex count is
// approximately `8 * r * grid * grid`, giving us precise control over
// the input size used by the scaling experiment WITHOUT having to
// allocate enormous images: 100k vertices needs only a ~2k x 2k
// raster when stamped as a 12 x 12 grid of small disks, vs. a single
// 25k x 25k disk that overflows the 1-byte-per-pixel raster.
//
// Usage:
//   gen_synthetic <output.pgm> <radius> [hole-radius] [grid]
//
// Defaults:  hole-radius = radius / 3,  grid = 1.

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

int main(int argc, char** argv) {
    if (argc < 3) {
        std::cerr <<
            "Usage: " << argv[0] << " <output.pgm> <radius> [hole-radius] [grid]\n"
            "\n"
            "Writes a binary PGM (P5) image containing a `grid x grid`\n"
            "tiling of filled disks of radius `radius` with concentric\n"
            "holes of radius `hole-radius` (default radius/3, grid=1).\n";
        return 1;
    }

    const std::string out = argv[1];
    const int radius = std::atoi(argv[2]);
    const int hole   = (argc >= 4) ? std::atoi(argv[3]) : (radius / 3);
    const int grid   = (argc >= 5) ? std::atoi(argv[4]) : 1;
    if (radius < 8 || grid < 1) {
        std::cerr << "radius must be >= 8 and grid >= 1\n";
        return 1;
    }

    const int pad  = 8;
    const int cell = 2 * radius + 2 * pad;     // size of one disk cell
    const int W    = cell * grid;
    const int H    = W;
    const long long r2 = static_cast<long long>(radius) * radius;
    const long long h2 = static_cast<long long>(hole)   * hole;

    std::vector<unsigned char> pixels(static_cast<std::size_t>(W) * H, 255);
    for (int gy = 0; gy < grid; ++gy) {
        for (int gx = 0; gx < grid; ++gx) {
            const int cx = gx * cell + cell / 2;
            const int cy = gy * cell + cell / 2;
            const int x0 = cx - radius - pad, x1 = cx + radius + pad;
            const int y0 = cy - radius - pad, y1 = cy + radius + pad;
            for (int y = y0; y < y1; ++y) {
                for (int x = x0; x < x1; ++x) {
                    const long long dx = x - cx, dy = y - cy;
                    const long long d2 = dx * dx + dy * dy;
                    if (d2 <= r2 && d2 >= h2)
                        pixels[static_cast<std::size_t>(y) * W + x] = 0;
                }
            }
        }
    }

    std::ofstream f(out, std::ios::binary);
    if (!f) {
        std::cerr << "cannot open " << out << " for writing\n";
        return 1;
    }
    f << "P5\n" << W << ' ' << H << "\n255\n";
    f.write(reinterpret_cast<const char*>(pixels.data()),
            static_cast<std::streamsize>(pixels.size()));

    std::cerr << "wrote " << out << "  (" << W << " x " << H
              << ", " << grid << "x" << grid << " grid of disks "
              << "r=" << radius << ", hole r=" << hole << ")\n";
    return 0;
}
