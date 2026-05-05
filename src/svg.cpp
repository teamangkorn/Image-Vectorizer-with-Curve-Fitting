#include "svg.hpp"

#include <sstream>

namespace vec {
namespace {

void appendPolyline(std::ostringstream& os, const Polyline& poly) {
    if (poly.empty()) return;
    os << "M " << poly[0].x << ' ' << poly[0].y;
    for (std::size_t i = 1; i < poly.size(); ++i) {
        os << " L " << poly[i].x << ' ' << poly[i].y;
    }
    os << " Z ";
}

void appendBezierRing(std::ostringstream& os,
                      const std::vector<CubicBezier>& ring) {
    if (ring.empty()) return;
    os << "M " << ring.front().p0.x << ' ' << ring.front().p0.y;
    for (const auto& b : ring) {
        os << " C " << b.p1.x << ' ' << b.p1.y << ", "
                    << b.p2.x << ' ' << b.p2.y << ", "
                    << b.p3.x << ' ' << b.p3.y;
    }
    os << " Z ";
}

} // namespace

std::string writePolylineSvg(int width, int height,
                             const std::vector<Contour>& contours,
                             const std::string& fillColor) {
    std::ostringstream os;
    os.precision(3);
    os << std::fixed;
    os << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
       << "<svg xmlns=\"http://www.w3.org/2000/svg\" "
       << "viewBox=\"0 0 " << width << ' ' << height << "\" "
       << "width=\"" << width << "\" height=\"" << height << "\">\n";

    for (const auto& c : contours) {
        os << "  <path fill=\"" << fillColor
           << "\" fill-rule=\"evenodd\" d=\"";
        appendPolyline(os, c.outer);
        for (const auto& h : c.holes) appendPolyline(os, h);
        os << "\"/>\n";
    }
    os << "</svg>\n";
    return os.str();
}

std::string writeBezierSvg(int width, int height,
                           const std::vector<SvgPath>& paths,
                           const std::string& fillColor) {
    std::ostringstream os;
    os.precision(3);
    os << std::fixed;
    os << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
       << "<svg xmlns=\"http://www.w3.org/2000/svg\" "
       << "viewBox=\"0 0 " << width << ' ' << height << "\" "
       << "width=\"" << width << "\" height=\"" << height << "\">\n";

    for (const auto& p : paths) {
        os << "  <path fill=\"" << fillColor
           << "\" fill-rule=\"evenodd\" d=\"";
        for (const auto& ring : p.rings) appendBezierRing(os, ring);
        os << "\"/>\n";
    }
    os << "</svg>\n";
    return os.str();
}

} // namespace vec
