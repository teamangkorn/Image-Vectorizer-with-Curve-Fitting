#pragma once

#include <vector>

#include "geometry.hpp"
#include "image.hpp"

namespace vec {

// Extract every connected foreground region (4-connectivity) from `img`
// as a closed pixel-center polyline.  Background regions that are
// completely enclosed by foreground are returned as `holes` of the
// containing contour.  All polylines are closed (last point == first
// point) and use *pixel-center* coordinates (x + 0.5, y + 0.5).
//
// Components whose pixel area is below `minArea` are dropped.
std::vector<Contour> traceAll(const BinaryImage& img, int minArea);

} // namespace vec
