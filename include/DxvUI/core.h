#ifndef DXVUI_CORE_H
#define DXVUI_CORE_H

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
};

using PointI = Point<int>;

inline const char* getDefaultFontPath() {
#if defined(_WIN32) || defined(_WIN64)
    return "C:/Windows/Fonts/Arial.ttf";
#elif defined(__APPLE__)
    return "/System/Library/Fonts/Supplemental/Arial.ttf";
#elif defined(__linux__)
    return "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf";
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
        {"Sans", "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf"},
        {"Serif", "/usr/share/fonts/truetype/dejavu/DejaVuSerif.ttf"},
        {"Mono", "/usr/share/fonts/truetype/dejavu/DejaVuSansMono.ttf"},
        {"System", "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf"},
#endif
    };
    auto it = defaults.find(family);
    return it != defaults.end() ? it->second : getDefaultFontPath();
}

}  // namespace DxvUI

#endif  // DXVUI_CORE_H
