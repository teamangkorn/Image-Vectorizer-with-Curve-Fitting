#include "rdp.hpp"

#include <cmath>
#include <cstddef>
#include <stack>
#include <vector>

namespace vec {
namespace {

// Perpendicular distance from `p` to the (infinite) line through `a`-`b`.
// If a == b, returns the Euclidean distance |p - a|.
double perpendicularDistance(const Point& p, const Point& a, const Point& b) {
    const double dx = b.x - a.x;
    const double dy = b.y - a.y;
    const double len2 = dx * dx + dy * dy;
    if (len2 == 0.0) {
        const double ex = p.x - a.x;
        const double ey = p.y - a.y;
        return std::sqrt(ex * ex + ey * ey);
    }
    // |(b-a) x (p-a)| / |b-a|
    const double cross = std::fabs(dx * (p.y - a.y) - dy * (p.x - a.x));
    return cross / std::sqrt(len2);
}

// Iterative RDP on a sub-range [first, last] (inclusive); marks the
// surviving indices in `keep`.
void rdpRecurseIterative(const Polyline& pts, double epsilon,
                         std::size_t first, std::size_t last,
                         std::vector<char>& keep) {
    std::stack<std::pair<std::size_t, std::size_t>> stk;
    stk.emplace(first, last);
    while (!stk.empty()) {
        auto [lo, hi] = stk.top();
        stk.pop();
        if (hi <= lo + 1) continue;

        double dmax = 0.0;
        std::size_t imax = lo;
        for (std::size_t i = lo + 1; i < hi; ++i) {
            const double d = perpendicularDistance(pts[i], pts[lo], pts[hi]);
            if (d > dmax) { dmax = d; imax = i; }
        }
        if (dmax > epsilon) {
            keep[imax] = 1;
            stk.emplace(lo, imax);
            stk.emplace(imax, hi);
        }
    }
}

} // namespace

Polyline simplifyRDP(const Polyline& pts, double epsilon) {
    const std::size_t n = pts.size();
    if (n < 3) return pts;

    const bool closed = (pts.front().x == pts.back().x &&
                         pts.front().y == pts.back().y);

    std::vector<char> keep(n, 0);
    keep.front() = 1;
    keep.back()  = 1;

    if (closed && n >= 4) {
        // For a closed polygon, choose a second anchor as the vertex
        // farthest from the start so RDP can split the loop into two
        // open sub-paths, neither degenerate.
        double dmax = -1.0;
        std::size_t imax = 0;
        for (std::size_t i = 1; i + 1 < n; ++i) {
            const double dx = pts[i].x - pts.front().x;
            const double dy = pts[i].y - pts.front().y;
            const double d2 = dx * dx + dy * dy;
            if (d2 > dmax) { dmax = d2; imax = i; }
        }
        if (imax > 0 && imax + 1 < n) {
            keep[imax] = 1;
            rdpRecurseIterative(pts, epsilon, 0,    imax, keep);
            rdpRecurseIterative(pts, epsilon, imax, n - 1, keep);
        } else {
            rdpRecurseIterative(pts, epsilon, 0, n - 1, keep);
        }
    } else {
        rdpRecurseIterative(pts, epsilon, 0, n - 1, keep);
    }

    Polyline out;
    out.reserve(n);
    for (std::size_t i = 0; i < n; ++i) {
        if (keep[i]) out.push_back(pts[i]);
    }
    return out;
}

} // namespace vec
