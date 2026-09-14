#ifndef DXVUI_IMAGEDATA_H
#define DXVUI_IMAGEDATA_H

#include <cstdint>
#include <vector>

namespace DxvUI {

/**
 * @struct ImageData
 * @brief Raw RGBA image data for createTexture (stage 6b).
 *
 * Pixels are 8-bit per channel, row-major, top-left first, no padding.
 * Channels: 4 = RGBA, 3 = RGB (alpha 255), 1 = grey (replicated).
 * Used by IRenderBackend::createTexture and Image widget.
 */
struct ImageData {
    int width = 0;
    int height = 0;
    int channels = 4; // 1,3,4
    std::vector<uint8_t> pixels; // size = width*height*channels

    bool isValid() const {
        return width > 0 && height > 0 && (channels == 1 || channels == 3 || channels == 4) &&
               static_cast<int>(pixels.size()) == width * height * channels;
    }

    static ImageData createCheckerboard(int w, int h, int cell = 8) {
        ImageData img;
        img.width = w;
        img.height = h;
        img.channels = 4;
        img.pixels.resize(w * h * 4);
        for (int y = 0; y < h; ++y) {
            for (int x = 0; x < w; ++x) {
                bool white = ((x / cell) + (y / cell)) % 2 == 0;
                uint8_t c = white ? 220 : 80;
                size_t idx = (y * w + x) * 4;
                img.pixels[idx + 0] = c;
                img.pixels[idx + 1] = c;
                img.pixels[idx + 2] = c;
                img.pixels[idx + 3] = 255;
            }
        }
        return img;
    }

    static ImageData createSolid(int w, int h, uint8_t r, uint8_t g, uint8_t b, uint8_t a = 255) {
        ImageData img;
        img.width = w;
        img.height = h;
        img.channels = 4;
        img.pixels.resize(w * h * 4);
        for (int i = 0; i < w * h; ++i) {
            img.pixels[i * 4 + 0] = r;
            img.pixels[i * 4 + 1] = g;
            img.pixels[i * 4 + 2] = b;
            img.pixels[i * 4 + 3] = a;
        }
        return img;
    }
};

} // namespace DxvUI

#endif // DXVUI_IMAGEDATA_H
