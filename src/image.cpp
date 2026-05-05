#include "image.hpp"

#include <stdexcept>

#include "stb_image.h"

namespace vec {

BinaryImage loadAndBinarize(const std::string& path, int threshold, bool invert) {
    int w = 0;
    int h = 0;
    int comp = 0;
    // Force 4 channels (RGBA) so we can also handle alpha-keyed PNGs uniformly.
    stbi_uc* pixels = stbi_load(path.c_str(), &w, &h, &comp, 4);
    if (!pixels) {
        throw std::runtime_error("Failed to load image '" + path +
                                 "': " + stbi_failure_reason());
    }

    BinaryImage img;
    img.width  = w;
    img.height = h;
    img.data.resize(static_cast<std::size_t>(w) * h, 0);

    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            const stbi_uc* p = pixels + (static_cast<std::size_t>(y) * w + x) * 4;
            const int r = p[0];
            const int g = p[1];
            const int b = p[2];
            const int a = p[3];

            // Composite over white background using alpha so transparent
            // PNG areas behave like background.
            const double af = a / 255.0;
            const double rf = r * af + 255.0 * (1.0 - af);
            const double gf = g * af + 255.0 * (1.0 - af);
            const double bf = b * af + 255.0 * (1.0 - af);

            // Rec. 601 luma.
            const double luma = 0.299 * rf + 0.587 * gf + 0.114 * bf;

            const bool fg = invert ? (luma >  threshold)
                                   : (luma <  threshold);
            img.data[static_cast<std::size_t>(y) * w + x] = fg ? 1 : 0;
        }
    }

    stbi_image_free(pixels);
    return img;
}

} // namespace vec
