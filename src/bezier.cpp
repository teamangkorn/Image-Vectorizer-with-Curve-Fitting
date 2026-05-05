// Schneider's automatic Bezier curve fitting algorithm (Graphics Gems I,
// 1990) ported to modern C++.  Curve evaluation uses the de Casteljau
// subdivision method.

#include "bezier.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>

namespace vec {
namespace {

// --- de Casteljau evaluation -------------------------------------------------
Point lerp(const Point& a, const Point& b, double t) {
    return { a.x + (b.x - a.x) * t,
             a.y + (b.y - a.y) * t };
}

Point deCasteljauPts(const std::array<Point, 4>& V, double t) {
    std::array<Point, 4> q = V;
    for (int level = 1; level < 4; ++level) {
        for (int i = 0; i < 4 - level; ++i) {
            q[i] = lerp(q[i], q[i + 1], t);
        }
    }
    return q[0];
}

// First and second derivatives of a cubic Bezier expressed as
// quadratic / linear Beziers, also evaluated by de Casteljau.
Point evalQuadratic(const std::array<Point, 3>& V, double t) {
    std::array<Point, 3> q = V;
    for (int level = 1; level < 3; ++level)
        for (int i = 0; i < 3 - level; ++i)
            q[i] = lerp(q[i], q[i + 1], t);
    return q[0];
}

Point bezierFirstDeriv(const std::array<Point, 4>& V, double t) {
    std::array<Point, 3> Q = {
        Point{(V[1].x - V[0].x) * 3.0, (V[1].y - V[0].y) * 3.0},
        Point{(V[2].x - V[1].x) * 3.0, (V[2].y - V[1].y) * 3.0},
        Point{(V[3].x - V[2].x) * 3.0, (V[3].y - V[2].y) * 3.0},
    };
    return evalQuadratic(Q, t);
}

Point bezierSecondDeriv(const std::array<Point, 4>& V, double t) {
    Point Q0{(V[1].x - V[0].x) * 3.0, (V[1].y - V[0].y) * 3.0};
    Point Q1{(V[2].x - V[1].x) * 3.0, (V[2].y - V[1].y) * 3.0};
    Point Q2{(V[3].x - V[2].x) * 3.0, (V[3].y - V[2].y) * 3.0};
    Point R0{(Q1.x - Q0.x) * 2.0, (Q1.y - Q0.y) * 2.0};
    Point R1{(Q2.x - Q1.x) * 2.0, (Q2.y - Q1.y) * 2.0};
    return lerp(R0, R1, t);
}

// --- Helpers used by the fitter ---------------------------------------------
Point unitVec(const Point& a, const Point& b) {
    Point d{a.x - b.x, a.y - b.y};
    const double n = std::sqrt(d.x * d.x + d.y * d.y);
    return n > 0.0 ? Point{d.x / n, d.y / n} : Point{0.0, 0.0};
}

double distance(const Point& a, const Point& b) {
    const double dx = a.x - b.x;
    const double dy = a.y - b.y;
    return std::sqrt(dx * dx + dy * dy);
}

// Initial estimate of curve parameters using chord-length parameterization.
std::vector<double> chordLengthParameterize(const Polyline& d,
                                            std::size_t first,
                                            std::size_t last) {
    std::vector<double> u(last - first + 1, 0.0);
    for (std::size_t i = first + 1; i <= last; ++i) {
        u[i - first] = u[i - first - 1] + distance(d[i], d[i - 1]);
    }
    const double total = u.back();
    if (total > 0.0) {
        for (auto& v : u) v /= total;
    }
    return u;
}

// One Newton-Raphson step refining the parameter estimate `u`.
double newtonRaphsonRootFind(const std::array<Point, 4>& Q,
                             const Point& P, double u) {
    const Point Qu  = deCasteljauPts(Q, u);
    const Point Q1u = bezierFirstDeriv(Q, u);
    const Point Q2u = bezierSecondDeriv(Q, u);

    const double numerX = (Qu.x - P.x) * Q1u.x + (Qu.y - P.y) * Q1u.y;
    const double denomX = Q1u.x * Q1u.x + Q1u.y * Q1u.y +
                          (Qu.x - P.x) * Q2u.x + (Qu.y - P.y) * Q2u.y;
    if (denomX == 0.0) return u;
    return u - numerX / denomX;
}

std::vector<double> reparameterize(const Polyline& d,
                                   std::size_t first, std::size_t last,
                                   const std::vector<double>& u,
                                   const std::array<Point, 4>& bezCurve) {
    std::vector<double> uPrime(u.size());
    for (std::size_t i = 0; i < u.size(); ++i) {
        uPrime[i] = newtonRaphsonRootFind(bezCurve, d[first + i], u[i]);
    }
    return uPrime;
}

// Bernstein basis values.
inline double B0(double u) { const double t = 1.0 - u; return t * t * t; }
inline double B1(double u) { const double t = 1.0 - u; return 3.0 * u * t * t; }
inline double B2(double u) { const double t = 1.0 - u; return 3.0 * u * u * t; }
inline double B3(double u) { return u * u * u; }

// Build a Bezier curve from control points (least-squares fit), given
// the endpoints, end-tangent unit vectors and parameter values.
std::array<Point, 4> generateBezier(const Polyline& d,
                                    std::size_t first, std::size_t last,
                                    const std::vector<double>& uPrime,
                                    const Point& tHat1, const Point& tHat2) {
    (void)last;
    const std::size_t nPts = uPrime.size();
    std::vector<std::array<Point, 2>> A(nPts);
    for (std::size_t i = 0; i < nPts; ++i) {
        A[i][0] = { tHat1.x * B1(uPrime[i]), tHat1.y * B1(uPrime[i]) };
        A[i][1] = { tHat2.x * B2(uPrime[i]), tHat2.y * B2(uPrime[i]) };
    }

    double C[2][2] = {{0, 0}, {0, 0}};
    double X[2]    = {0, 0};
    for (std::size_t i = 0; i < nPts; ++i) {
        C[0][0] += A[i][0].x * A[i][0].x + A[i][0].y * A[i][0].y;
        C[0][1] += A[i][0].x * A[i][1].x + A[i][0].y * A[i][1].y;
        C[1][0]  = C[0][1];
        C[1][1] += A[i][1].x * A[i][1].x + A[i][1].y * A[i][1].y;

        const Point tmp{
            d[first + i].x - (d[first].x * B0(uPrime[i]) +
                              d[first].x * B1(uPrime[i]) +
                              d[last ].x * B2(uPrime[i]) +
                              d[last ].x * B3(uPrime[i])),
            d[first + i].y - (d[first].y * B0(uPrime[i]) +
                              d[first].y * B1(uPrime[i]) +
                              d[last ].y * B2(uPrime[i]) +
                              d[last ].y * B3(uPrime[i])),
        };
        X[0] += A[i][0].x * tmp.x + A[i][0].y * tmp.y;
        X[1] += A[i][1].x * tmp.x + A[i][1].y * tmp.y;
    }

    const double detC0C1 = C[0][0] * C[1][1] - C[1][0] * C[0][1];
    const double detC0X  = C[0][0] * X[1]    - C[0][1] * X[0];
    const double detXC1  = X[0]    * C[1][1] - X[1]    * C[0][1];

    const double alpha_l = (detC0C1 == 0.0) ? 0.0 : detXC1 / detC0C1;
    const double alpha_r = (detC0C1 == 0.0) ? 0.0 : detC0X / detC0C1;

    // Heuristic: if alphas are negative or zero, fall back to a Wu/Barsky
    // estimate of one third of the chord length.
    const double segLength = distance(d[first], d[last]);
    const double epsilon   = 1.0e-6 * segLength;

    std::array<Point, 4> bez;
    bez[0] = d[first];
    bez[3] = d[last];
    if (alpha_l < epsilon || alpha_r < epsilon) {
        const double dist = segLength / 3.0;
        bez[1] = { bez[0].x + tHat1.x * dist, bez[0].y + tHat1.y * dist };
        bez[2] = { bez[3].x + tHat2.x * dist, bez[3].y + tHat2.y * dist };
    } else {
        bez[1] = { bez[0].x + tHat1.x * alpha_l, bez[0].y + tHat1.y * alpha_l };
        bez[2] = { bez[3].x + tHat2.x * alpha_r, bez[3].y + tHat2.y * alpha_r };
    }
    return bez;
}

// Maximum squared deviation of `d[first..last]` from the curve.  The
// index of the worst point is returned via `splitPoint`.
double computeMaxError(const Polyline& d,
                       std::size_t first, std::size_t last,
                       const std::array<Point, 4>& bez,
                       const std::vector<double>& u,
                       std::size_t& splitPoint) {
    splitPoint = (first + last) / 2;
    double maxDist = 0.0;
    for (std::size_t i = first + 1; i < last; ++i) {
        const Point P = deCasteljauPts(bez, u[i - first]);
        const double dx = P.x - d[i].x;
        const double dy = P.y - d[i].y;
        const double dist2 = dx * dx + dy * dy;
        if (dist2 >= maxDist) {
            maxDist = dist2;
            splitPoint = i;
        }
    }
    return maxDist;
}

void fitCubic(const Polyline& d, std::size_t first, std::size_t last,
              Point tHat1, Point tHat2, double errorSq,
              std::vector<CubicBezier>& out) {
    // Trivial 2-point case: just a straight line approximated as a
    // degenerate cubic with control points placed at one third of the
    // chord length from each endpoint.
    if (last - first == 1) {
        const double dist = distance(d[first], d[last]) / 3.0;
        std::array<Point, 4> bez = {
            d[first],
            { d[first].x + tHat1.x * dist, d[first].y + tHat1.y * dist },
            { d[last ].x + tHat2.x * dist, d[last ].y + tHat2.y * dist },
            d[last]
        };
        out.push_back({bez[0], bez[1], bez[2], bez[3]});
        return;
    }

    auto u = chordLengthParameterize(d, first, last);
    auto bez = generateBezier(d, first, last, u, tHat1, tHat2);

    std::size_t splitPoint = 0;
    double maxErr = computeMaxError(d, first, last, bez, u, splitPoint);
    if (maxErr < errorSq) {
        out.push_back({bez[0], bez[1], bez[2], bez[3]});
        return;
    }

    // Try one round of Newton-Raphson reparameterization if the error
    // is not catastrophic.
    const double iterErrorSq = errorSq * 4.0;
    if (maxErr < iterErrorSq) {
        for (int it = 0; it < 4; ++it) {
            auto uPrime = reparameterize(d, first, last, u, bez);
            bez   = generateBezier(d, first, last, uPrime, tHat1, tHat2);
            maxErr = computeMaxError(d, first, last, bez, uPrime, splitPoint);
            u = std::move(uPrime);
            if (maxErr < errorSq) {
                out.push_back({bez[0], bez[1], bez[2], bez[3]});
                return;
            }
        }
    }

    // Recursive subdivision at the worst-error point.
    const Point centerTangent = unitVec(d[splitPoint - 1], d[splitPoint + 1]);
    fitCubic(d, first, splitPoint, tHat1,
             centerTangent, errorSq, out);
    const Point negCenter{-centerTangent.x, -centerTangent.y};
    fitCubic(d, splitPoint, last, negCenter, tHat2, errorSq, out);
}

} // namespace

Point deCasteljau(const CubicBezier& b, double t) {
    return deCasteljauPts({b.p0, b.p1, b.p2, b.p3}, t);
}

std::vector<CubicBezier> fitCurves(const Polyline& points,
                                   double errorTolerance) {
    std::vector<CubicBezier> out;
    if (points.size() < 2) return out;
    if (points.size() == 2) {
        const Point tHat1 = unitVec(points[1], points[0]);
        const Point tHat2 = unitVec(points[0], points[1]);
        fitCubic(points, 0, 1, tHat1, tHat2,
                 errorTolerance * errorTolerance, out);
        return out;
    }

    // Endpoint tangents: forward / backward differences from the first
    // and last neighbours respectively.  For a closed polyline these
    // also give a reasonable estimate.
    const Point tHat1 = unitVec(points[1],                points[0]);
    const Point tHat2 = unitVec(points[points.size() - 2],
                                points[points.size() - 1]);
    fitCubic(points, 0, points.size() - 1, tHat1, tHat2,
             errorTolerance * errorTolerance, out);
    return out;
}

} // namespace vec
