#ifndef DXVUI_CORE_H
#define DXVUI_CORE_H

#include "DxvUI/style/Color.h"

namespace DxvUI {

// How a node is aligned by its parent within the space given to it:
// Start = top-left corner, Center = centered, End = bottom-right corner.
// Stretch is reserved and currently not implemented by the layout.
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

}  // namespace DxvUI

#endif  // DXVUI_CORE_H
