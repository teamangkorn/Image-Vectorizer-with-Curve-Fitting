#pragma once

#include <vector>

#include "geometry.hpp"

namespace vec {

// Evaluate the cubic Bezier curve `b` at parameter `t in [0, 1]` using
// the de Casteljau subdivision algorithm.
Point deCasteljau(const CubicBezier& b, double t);

// Fit a sequence of cubic Bezier segments to `points` so that the
// maximum squared distance from each input point to the corresponding
// fitted curve does not exceed `errorTolerance` (in the same units as
// the points).  Implementation: Schneider, "An Algorithm for
// Automatically Fitting Digitized Curves" (Graphics Gems I, 1990).
//
// `points` must have at least two entries.  For closed inputs (first
// point equals last point) the algorithm still works because endpoint
// tangents are estimated from the immediate neighbours.
std::vector<CubicBezier> fitCurves(const Polyline& points,
                                   double errorTolerance);

} // namespace vec
