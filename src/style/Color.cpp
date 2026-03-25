#include "DxvUI/style/Color.h"
#include <algorithm>
#include <cmath>
#include <sstream>
#include <iomanip>

namespace DxvUI {

    uint32_t Color::toUint32() const {
        return (static_cast<uint32_t>(r) << 24) |
               (static_cast<uint32_t>(g) << 16) |
               (static_cast<uint32_t>(b) << 8)  |
               static_cast<uint32_t>(a);
    }

    Color Color::fromUint32(uint32_t val) {
        return Color(
            static_cast<uint8_t>((val >> 24) & 0xFF),
            static_cast<uint8_t>((val >> 16) & 0xFF),
            static_cast<uint8_t>((val >> 8) & 0xFF),
            static_cast<uint8_t>(val & 0xFF)
        );
    }

    Hsv Color::toHsv() const {
        Hsv out;
        float r_ = r / 255.0f;
        float g_ = g / 255.0f;
        float b_ = b / 255.0f;

        float cmax = std::max({r_, g_, b_});
        float cmin = std::min({r_, g_, b_});
        float diff = cmax - cmin;

        if (cmax == cmin) {
            out.h = 0; // For grays, hue is undefined; conventionally 0
        } else if (cmax == r_) {
            out.h = fmod(60 * ((g_ - b_) / diff) + 360, 360);
        } else if (cmax == g_) {
            out.h = fmod(60 * ((b_ - r_) / diff) + 120, 360);
        } else if (cmax == b_) {
            out.h = fmod(60 * ((r_ - g_) / diff) + 240, 360);
        }

        out.s = (cmax == 0) ? 0 : (diff / cmax);
        out.v = cmax;

        return out;
    }

    Color Color::fromHsv(const Hsv& hsv) {
        float s = hsv.s;
        float v = hsv.v;
        float c = s * v;
        float x = c * (1 - std::abs(fmod(hsv.h / 60.0, 2) - 1));
        float m = v - c;
        float r_, g_, b_;

        // If saturation is 0, it's a gray color. Hue doesn't matter.
        if (s == 0) {
            r_ = g_ = b_ = v;
        } else {
            int h_i = static_cast<int>(hsv.h / 60.0f);
            switch (h_i) {
                case 0: r_ = c; g_ = x; b_ = 0; break;
                case 1: r_ = x; g_ = c; b_ = 0; break;
                case 2: r_ = 0; g_ = c; b_ = x; break;
                case 3: r_ = 0; g_ = x; b_ = c; break;
                case 4: r_ = x; g_ = 0; b_ = c; break;
                default: r_ = c; g_ = 0; b_ = x; break;
            }
            r_ += m;
            g_ += m;
            b_ += m;
        }

        return Color(
            static_cast<uint8_t>(r_ * 255),
            static_cast<uint8_t>(g_ * 255),
            static_cast<uint8_t>(b_ * 255)
            // Alpha is not part of HSV conversion
        );
    }

    std::string Color::toHex() const {
        std::stringstream ss;
        ss << "#" << std::hex << std::setfill('0')
           << std::setw(2) << static_cast<int>(r)
           << std::setw(2) << static_cast<int>(g)
           << std::setw(2) << static_cast<int>(b)
           << std::setw(2) << static_cast<int>(a);
        return ss.str();
    }

    Color Color::fromHex(std::string hex) {
        if (hex.empty()) return {};
        if (hex[0] == '#') hex.erase(0, 1);

        unsigned long val = std::stoul(hex, nullptr, 16);

        if (hex.length() == 8) { // RRGGBBAA
            return fromUint32(val);
        }
        if (hex.length() == 6) { // RRGGBB
            return fromUint32((val << 8) | 0xFF);
        }
        return {}; // Invalid format
    }

    Color Color::lighten(float amount) const {
        Hsv hsv = toHsv();
        hsv.v = std::clamp(hsv.v + amount, 0.0f, 1.0f);
        Color result = fromHsv(hsv);
        result.a = this->a; // Preserve original alpha
        return result;
    }

    Color Color::darken(float amount) const {
        Hsv hsv = toHsv();
        hsv.v = std::clamp(hsv.v - amount, 0.0f, 1.0f);
        Color result = fromHsv(hsv);
        result.a = this->a; // Preserve original alpha
        return result;
    }

    Color Color::inverse() const {
        return Color(static_cast<uint8_t>(255 - r), static_cast<uint8_t>(255 - g), static_cast<uint8_t>(255 - b), a);
    }

    Color Color::grayscale() const {
        auto gray = static_cast<uint8_t>(0.299 * r + 0.587 * g + 0.114 * b);
        return Color(gray, gray, gray, a);
    }

    Color Color::lerp(const Color& start, const Color& end, float t) {
        t = std::clamp(t, 0.0f, 1.0f);
        return Color(
            static_cast<uint8_t>(start.r + (end.r - start.r) * t),
            static_cast<uint8_t>(start.g + (end.g - start.g) * t),
            static_cast<uint8_t>(start.b + (end.b - start.b) * t),
            static_cast<uint8_t>(start.a + (end.a - start.a) * t)
        );
    }

    bool Color::operator==(const Color& other) const {
        return r == other.r && g == other.g && b == other.b && a == other.a;
    }

    bool Color::operator!=(const Color& other) const {
        return !(*this == other);
    }
}
