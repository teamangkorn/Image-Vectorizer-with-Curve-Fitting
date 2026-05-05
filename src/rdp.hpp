#pragma once

#include "geometry.hpp"

namespace vec {

// Ramer-Douglas-Peucker simplification of a polyline.
// `epsilon` is the maximum allowed perpendicular distance (in the same
// units as the points) between any removed vertex and the simplified
// polyline.  Works for both open polylines and closed polygons; for
// closed inputs (first == last) it preserves closure.
Polyline simplifyRDP(const Polyline& points, double epsilon);

} // namespace vec
