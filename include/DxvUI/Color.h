#ifndef DXVUI_COLOR_H
#define DXVUI_COLOR_H

#include <cstdint>
#include <string>

namespace DxvUI {

    struct Color;

    struct Hsv {
        float h = 0.0f;
        float s = 0.0f;
        float v = 0.0f;
    };

    struct Color {
        uint8_t r = 0, g = 0, b = 0, a = 255;

        // --- Constructors ---
        constexpr Color() = default;
        constexpr Color(uint8_t r, uint8_t g, uint8_t b, uint8_t a = 255) : r(r), g(g), b(b), a(a) {}

        // --- Conversions ---
        uint32_t toUint32() const;
        static Color fromUint32(uint32_t val);
        Hsv toHsv() const;
        static Color fromHsv(const Hsv& hsv);
        std::string toHex() const;
        static Color fromHex(std::string hex);

        // --- Manipulations ---
        Color lighten(float amount) const;
        Color darken(float amount) const;
        Color inverse() const;
        Color grayscale() const;

        // --- Interpolation ---
        static Color lerp(const Color& start, const Color& end, float t);

        // --- Operators ---
        bool operator==(const Color& other) const;
        bool operator!=(const Color& other) const;
    };

}

#endif //DXVUI_COLOR_H
