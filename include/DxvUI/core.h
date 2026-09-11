#ifndef DXVUI_CORE_H
#define DXVUI_CORE_H

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <map>
#include <string>

#include "DxvUI/style/Color.h"

namespace DxvUI {

// How a node is aligned by its parent within the space given to it:
// Start = top-left corner, Center = centered, End = bottom-right corner,
// Stretch = fill the slot (minus margin) on the enabled axis.
enum class Alignment { Start, Center, End, Stretch };

// System cursor types
enum class CursorType {
    Arrow,  // Default pointer
    IBeam,  // Text input
    Wait,   // Busy indicator
    Crosshair,
    Hand,        // Hand pointer for links/buttons
    ResizeNWSE,  // Diagonal resize
    ResizeNESW,  // Diagonal resize
    ResizeWE,    // Horizontal resize
    ResizeNS,    // Vertical resize
    ResizeAll,
    No  // Hidden cursor
};

// --- Structs & Core Types ---
struct Size {
    float width = 0, height = 0;

    bool operator==(const Size& other) const {
        return width == other.width && height == other.height;
    }
};

struct Rect {
    int x = 0, y = 0, width = 0, height = 0;

    bool contains(int pX, int pY) const {
        return (pX >= x && pX < (x + width) && pY >= y && pY < (y + height));
    }

    bool intersects(const Rect& other) const {
        return x < other.x + other.width && x + width > other.x && y < other.y + other.height &&
               y + height > other.y;
    }

    bool operator==(const Rect& other) const {
        return x == other.x && y == other.y && width == other.width && height == other.height;
    }
};

struct Thickness {
    float top = 0, right = 0, bottom = 0, left = 0;

    bool operator==(const Thickness& other) const {
        return top == other.top && right == other.right && bottom == other.bottom &&
               left == other.left;
    }
};

struct Border {
    Color color;
    int thickness = 1;
};

template <typename T>
struct Point {
    T x = 0, y = 0;

    bool operator==(const Point& other) const { return x == other.x && y == other.y; }
    bool operator!=(const Point& other) const { return !(*this == other); }
};

using PointI = Point<int>;

/**
 * @brief A point in painting space (float pixels).
 *
 * Painting is float-based (see docs/RENDERING_REFACTORING.md §3.1): widget
 * paint code and the canvas work with subpixel geometry, while layout stays
 * integer. A PointI converts implicitly (the widening is exact), and the paint
 * -> backend boundary rounds with rounded().
 */
struct PointF {
    float x = 0, y = 0;

    constexpr PointF() = default;
    constexpr PointF(float x, float y) : x(x), y(y) {}
    // Implicit widening from layout space; exact for every int coordinate.
    constexpr PointF(const PointI& point)
        : x(static_cast<float>(point.x)), y(static_cast<float>(point.y)) {}

    /// @brief Rounds to the nearest integer pixel (paint -> backend boundary).
    PointI rounded() const;

    bool operator==(const PointF& other) const { return x == other.x && y == other.y; }
};

/**
 * @brief A rectangle in painting space (float pixels).
 *
 * The painting twin of Rect: nodes and the canvas compute in floats, and the
 * backend-boundary conversion goes through rounded(). A Rect converts
 * implicitly (exact widening), so existing layout-space geometry can be passed
 * to canvas calls unchanged.
 */
struct RectF {
    float x = 0, y = 0, width = 0, height = 0;

    constexpr RectF() = default;
    constexpr RectF(float x, float y, float width, float height)
        : x(x), y(y), width(width), height(height) {}
    // Implicit widening from layout space; exact for every int coordinate.
    constexpr RectF(const Rect& rect)
        : x(static_cast<float>(rect.x)),
          y(static_cast<float>(rect.y)),
          width(static_cast<float>(rect.width)),
          height(static_cast<float>(rect.height)) {}

    constexpr float right() const { return x + width; }
    constexpr float bottom() const { return y + height; }
    constexpr PointF center() const { return {x + width / 2.0f, y + height / 2.0f}; }

    constexpr bool contains(const PointF& point) const {
        return point.x >= x && point.x < right() && point.y >= y && point.y < bottom();
    }

    constexpr bool intersects(const RectF& other) const {
        return x < other.right() && right() > other.x && y < other.bottom() && bottom() > other.y;
    }

    /**
     * @brief Rounds the rectangle to whole pixels (paint -> backend boundary).
     *
     * Both edges are rounded independently (x and right(), y and bottom()) so
     * the result keeps the intended pixel coverage instead of accumulating the
     * rounding error of the size; a degenerate result clamps width/height to 0.
     */
    Rect rounded() const;

    bool operator==(const RectF& other) const {
        return x == other.x && y == other.y && width == other.width && height == other.height;
    }
};

inline PointI PointF::rounded() const {
    return {static_cast<int>(std::lround(x)), static_cast<int>(std::lround(y))};
}

inline Rect RectF::rounded() const {
    const int left = static_cast<int>(std::lround(x));
    const int top = static_cast<int>(std::lround(y));
    const int rightPx = static_cast<int>(std::lround(right()));
    const int bottomPx = static_cast<int>(std::lround(bottom()));
    return {left, top, std::max(0, rightPx - left), std::max(0, bottomPx - top)};
}

inline const char* getDefaultFontPath() {
#if defined(_WIN32) || defined(_WIN64)
    return "C:/Windows/Fonts/Arial.ttf";
#elif defined(__APPLE__)
    return "/System/Library/Fonts/Supplemental/Arial.ttf";
#elif defined(__linux__)
    // Arch/CachyOS keep DejaVu under /usr/share/fonts/TTF/; Debian/Ubuntu under
    // /usr/share/fonts/truetype/dejavu/. Try both, in order of likelihood.
    if (std::filesystem::exists("/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf"))
        return "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf";
    return "/usr/share/fonts/TTF/DejaVuSans.ttf";
#else
    return "";
#endif
}

/**
 * @brief Resolves a logical font family name to a font file path.
 *
 * The built-in map covers the common families on the current platform; unknown
 * or empty names fall back to getDefaultFontPath(), so the result is always a
 * loadable file on the supported platforms. This is the *default* registry:
 * text engines consult it as the fallback for families not registered via
 * ITextEngine::registerFontFamily().
 * @param family The family name (e.g. "Sans", "Serif", "Mono", "System").
 * @return A font file path; never empty on supported platforms.
 */
inline const char* getDefaultFontFamilyPath(const std::string& family) {
    static const std::map<std::string, const char*> defaults = {
#if defined(_WIN32) || defined(_WIN64)
        {"Sans", "C:/Windows/Fonts/Arial.ttf"},
        {"Serif", "C:/Windows/Fonts/Times.ttf"},
        {"Mono", "C:/Windows/Fonts/Consola.ttf"},
        {"System", "C:/Windows/Fonts/segoeui.ttf"},
#elif defined(__APPLE__)
        {"Sans", "/System/Library/Fonts/Supplemental/Arial.ttf"},
        {"Serif", "/System/Library/Fonts/Supplemental/Times New Roman.ttf"},
        {"Mono", "/System/Library/Fonts/Supplemental/Courier New.ttf"},
        {"System", "/System/Library/Fonts/Supplemental/Arial.ttf"},
#elif defined(__linux__)
        {"Sans", "/usr/share/fonts/TTF/DejaVuSans.ttf"},
        {"Serif", "/usr/share/fonts/TTF/DejaVuSerif.ttf"},
        {"Mono", "/usr/share/fonts/TTF/DejaVuSansMono.ttf"},
        {"System", "/usr/share/fonts/TTF/DejaVuSans.ttf"},
#endif
    };
    auto it = defaults.find(family);
    return it != defaults.end() ? it->second : getDefaultFontPath();
}

}  // namespace DxvUI

#endif  // DXVUI_CORE_H
