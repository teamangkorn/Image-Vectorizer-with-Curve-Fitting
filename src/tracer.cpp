#include "tracer.hpp"

#include <algorithm>
#include <array>
#include <queue>
#include <stdexcept>

namespace vec {
namespace {

// 8-connected Moore neighbourhood, listed clockwise starting from "east".
//                       E   SE  S   SW  W   NW  N   NE
constexpr int dx8[8] = { 1,  1,  0, -1, -1, -1,  0,  1};
constexpr int dy8[8] = { 0,  1,  1,  1,  0, -1, -1, -1};

// Returns the index in [0, 8) such that (px - cx, py - cy) ==
// (dx8[idx], dy8[idx]).  Caller must guarantee that p is one of the 8
// neighbours of c.
int neighbourIndex(int cx, int cy, int px, int py) {
    for (int i = 0; i < 8; ++i) {
        if (cx + dx8[i] == px && cy + dy8[i] == py) return i;
    }
    return 0; // unreachable for valid inputs
}

// 4-connectivity flood-fill labelling.  Cells of `img` matching the
// `selector` predicate are grouped into components; output `labels`
// has the same dimensions as `img` and contains 0 for non-matching
// cells, otherwise a 1-based component id.  Returns the number of
// components found.
template <class Selector>
int labelComponents(const BinaryImage& img, std::vector<int>& labels,
                    Selector selector) {
    const int W = img.width;
    const int H = img.height;
    labels.assign(static_cast<std::size_t>(W) * H, 0);

    int next = 0;
    std::queue<std::pair<int, int>> q;

    for (int y = 0; y < H; ++y) {
        for (int x = 0; x < W; ++x) {
            const std::size_t idx = static_cast<std::size_t>(y) * W + x;
            if (labels[idx] != 0)         continue;
            if (!selector(img.at(x, y))) continue;

            ++next;
            labels[idx] = next;
            q.emplace(x, y);
            while (!q.empty()) {
                auto [cx, cy] = q.front();
                q.pop();
                static constexpr int dx4[4] = { 1, -1,  0, 0};
                static constexpr int dy4[4] = { 0,  0,  1,-1};
                for (int k = 0; k < 4; ++k) {
                    const int nx = cx + dx4[k];
                    const int ny = cy + dy4[k];
                    if (nx < 0 || ny < 0 || nx >= W || ny >= H) continue;
                    const std::size_t ni = static_cast<std::size_t>(ny) * W + nx;
                    if (labels[ni] != 0) continue;
                    if (!selector(img.at(nx, ny))) continue;
                    labels[ni] = next;
                    q.emplace(nx, ny);
                }
            }
        }
    }
    return next;
}

// Moore-Neighbour boundary tracing with Jacob's stopping criterion.
// `inside(x, y)` returns true for pixels that belong to the region
// being traced.  `start` must be the topmost-leftmost cell of the
// region (so its left neighbour is guaranteed to be outside).
//
// Produces a closed polyline (last point == first point) of pixel
// centres.  For an isolated single pixel the result is just [start].
Polyline traceBoundary(int sx, int sy,
                       int W, int H,
                       const std::vector<std::uint8_t>& mask) {
    auto inside = [&](int x, int y) {
        if (x < 0 || y < 0 || x >= W || y >= H) return false;
        return mask[static_cast<std::size_t>(y) * W + x] != 0;
    };

    Polyline poly;
    poly.emplace_back(sx + 0.5, sy + 0.5);

    // Initial "previous" cell: the one to the west of the start.  By
    // the row-major scan we know that cell is not inside the region.
    int bx = sx, by = sy;        // current boundary cell
    int px = sx - 1, py = sy;    // previous (outside) cell

    // Single-pixel region: no neighbours are inside.
    bool anyInside = false;
    for (int i = 0; i < 8; ++i) {
        if (inside(sx + dx8[i], sy + dy8[i])) { anyInside = true; break; }
    }
    if (!anyInside) {
        poly.push_back(poly.front()); // close
        return poly;
    }

    const int startX = sx;
    const int startY = sy;
    const int startPrevX = px;
    const int startPrevY = py;

    while (true) {
        // Scan clockwise starting *after* the previous cell.
        const int startIdx = (neighbourIndex(bx, by, px, py) + 1) % 8;
        int found = -1;
        int prevIdx = (startIdx + 7) % 8; // the cell we will mark as "previous"
        for (int k = 0; k < 8; ++k) {
            const int idx = (startIdx + k) % 8;
            const int nx = bx + dx8[idx];
            const int ny = by + dy8[idx];
            if (inside(nx, ny)) {
                found = idx;
                break;
            }
            prevIdx = idx;
        }
        if (found < 0) break; // should not happen because anyInside == true

        // Update previous and boundary.
        px = bx + dx8[prevIdx];
        py = by + dy8[prevIdx];
        bx = bx + dx8[found];
        by = by + dy8[found];

        poly.emplace_back(bx + 0.5, by + 0.5);

        // Jacob's stopping criterion: returned to start with the same
        // entry direction.
        if (bx == startX && by == startY &&
            px == startPrevX && py == startPrevY) {
            break;
        }
        // Safety net.
        if (poly.size() > static_cast<std::size_t>(W) * H * 4) break;
    }

    if (!(poly.back().x == poly.front().x && poly.back().y == poly.front().y)) {
        poly.push_back(poly.front());
    }
    return poly;
}

} // namespace

std::vector<Contour> traceAll(const BinaryImage& img, int minArea) {
    const int W = img.width;
    const int H = img.height;
    if (W <= 0 || H <= 0) return {};

    // --- Foreground components ----------------------------------------------
    std::vector<int> fgLabels;
    const int fgCount = labelComponents(img, fgLabels,
                                        [](std::uint8_t v) { return v != 0; });

    // First pixel (top-left scan) and area for every fg component.
    std::vector<std::array<int, 2>> fgStart(fgCount + 1, {-1, -1});
    std::vector<int> fgArea(fgCount + 1, 0);
    for (int y = 0; y < H; ++y) {
        for (int x = 0; x < W; ++x) {
            const int l = fgLabels[static_cast<std::size_t>(y) * W + x];
            if (l == 0) continue;
            ++fgArea[l];
            if (fgStart[l][0] < 0) fgStart[l] = {x, y};
        }
    }

    // --- Background components (for hole detection) -------------------------
    std::vector<int> bgLabels;
    const int bgCount = labelComponents(img, bgLabels,
                                        [](std::uint8_t v) { return v == 0; });

    // A bg component that touches the image border is "outside".  All
    // other bg components are holes.
    std::vector<bool> bgIsHole(bgCount + 1, true);
    bgIsHole[0] = false;
    for (int x = 0; x < W; ++x) {
        if (int l = bgLabels[x];                         l) bgIsHole[l] = false;
        if (int l = bgLabels[(H - 1) * W + x];           l) bgIsHole[l] = false;
    }
    for (int y = 0; y < H; ++y) {
        if (int l = bgLabels[y * W];                     l) bgIsHole[l] = false;
        if (int l = bgLabels[y * W + (W - 1)];           l) bgIsHole[l] = false;
    }

    // For each hole bg component, find its parent fg label by looking
    // at any 4-neighbour that is foreground.
    std::vector<int> holeStart;             // flat pairs: index in bgLabels
    std::vector<int> holeParent(bgCount + 1, 0);
    std::vector<std::array<int, 2>> bgStart(bgCount + 1, {-1, -1});
    std::vector<int> bgArea(bgCount + 1, 0);
    for (int y = 0; y < H; ++y) {
        for (int x = 0; x < W; ++x) {
            const int l = bgLabels[static_cast<std::size_t>(y) * W + x];
            if (l == 0) continue;
            ++bgArea[l];
            if (bgStart[l][0] < 0) bgStart[l] = {x, y};
            if (bgIsHole[l] && holeParent[l] == 0) {
                static constexpr int dx4[4] = { 1, -1,  0, 0};
                static constexpr int dy4[4] = { 0,  0,  1,-1};
                for (int k = 0; k < 4; ++k) {
                    const int nx = x + dx4[k];
                    const int ny = y + dy4[k];
                    if (nx < 0 || ny < 0 || nx >= W || ny >= H) continue;
                    const int parent = fgLabels[
                        static_cast<std::size_t>(ny) * W + nx];
                    if (parent != 0) { holeParent[l] = parent; break; }
                }
            }
        }
    }

    // --- Trace outer contours ----------------------------------------------
    std::vector<Contour> contours;
    std::vector<int> fgToContour(fgCount + 1, -1);
    for (int l = 1; l <= fgCount; ++l) {
        if (fgArea[l] < minArea) continue;

        // Build a 0/1 mask isolating just this fg component.
        std::vector<std::uint8_t> mask(static_cast<std::size_t>(W) * H, 0);
        for (std::size_t i = 0; i < mask.size(); ++i) {
            if (fgLabels[i] == l) mask[i] = 1;
        }
        Contour c;
        c.outer = traceBoundary(fgStart[l][0], fgStart[l][1], W, H, mask);
        fgToContour[l] = static_cast<int>(contours.size());
        contours.push_back(std::move(c));
    }

    // --- Trace holes --------------------------------------------------------
    for (int l = 1; l <= bgCount; ++l) {
        if (!bgIsHole[l])           continue;
        if (bgArea[l] < minArea)    continue;
        const int parent = holeParent[l];
        if (parent == 0)            continue;
        const int ci = fgToContour[parent];
        if (ci < 0)                 continue;

        std::vector<std::uint8_t> mask(static_cast<std::size_t>(W) * H, 0);
        for (std::size_t i = 0; i < mask.size(); ++i) {
            if (bgLabels[i] == l) mask[i] = 1;
        }
        Polyline hole = traceBoundary(bgStart[l][0], bgStart[l][1], W, H, mask);
        contours[ci].holes.push_back(std::move(hole));
    }

    return contours;
}

} // namespace vec
