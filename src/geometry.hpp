// Geometric primitives shared across the project.
#pragma once

#include <cmath>
#include <vector>

namespace vec {

struct Point {
    double x{0.0};
    double y{0.0};

    Point() = default;
    Point(double xx, double yy) : x(xx), y(yy) {}

    Point operator+(const Point& o) const { return {x + o.x, y + o.y}; }
    Point operator-(const Point& o) const { return {x - o.x, y - o.y}; }
    Point operator*(double s)       const { return {x * s,   y * s  }; }
    Point operator/(double s)       const { return {x / s,   y / s  }; }

    double dot(const Point& o) const { return x * o.x + y * o.y; }
    double norm()              const { return std::sqrt(x * x + y * y); }
    Point  unit()              const {
        const double n = norm();
        return n > 0.0 ? Point{x / n, y / n} : Point{0.0, 0.0};
    }
};

using Polyline = std::vector<Point>;

// A cubic Bezier segment defined by four control points.
struct CubicBezier {
    Point p0, p1, p2, p3;
};

// One closed contour: an outer boundary plus zero or more holes.
struct Contour {
    Polyline outer;
    std::vector<Polyline> holes;
};

} // namespace vec
