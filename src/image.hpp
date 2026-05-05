#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace vec {

// A binary (0 / 1) image stored as one byte per pixel, row-major.
struct BinaryImage {
    int width  = 0;
    int height = 0;
    std::vector<std::uint8_t> data;   // 0 = background, 1 = foreground

    std::uint8_t at(int x, int y) const {
        if (x < 0 || y < 0 || x >= width || y >= height) return 0;
        return data[static_cast<std::size_t>(y) * width + x];
    }
};

// Load `path` (PNG/JPG/BMP via stb_image), convert to grayscale, then
// binarize: pixels with luminance < threshold become foreground (1).
// If `invert` is true, the comparison flips (lighter pixels are foreground).
// Throws std::runtime_error on failure.
BinaryImage loadAndBinarize(const std::string& path, int threshold, bool invert);

} // namespace vec
