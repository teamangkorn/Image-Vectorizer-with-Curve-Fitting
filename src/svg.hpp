#pragma once

#include <string>
#include <vector>

#include "geometry.hpp"

namespace vec {

struct SvgPath {
    // Outer ring + holes, each as a sequence of cubic Bezier segments.
    std::vector<std::vector<CubicBezier>> rings;
};

// Build a polyline-only SVG (no curve fitting).
std::string writePolylineSvg(int width, int height,
                             const std::vector<Contour>& contours,
                             const std::string& fillColor);

// Build a Bezier SVG.  `paths[i].rings[0]` is the outer ring and any
// further rings are holes (rendered with fill-rule="evenodd").
std::string writeBezierSvg(int width, int height,
                           const std::vector<SvgPath>& paths,
                           const std::string& fillColor);

} // namespace vec
