#ifndef DXVUI_CORE_H
#define DXVUI_CORE_H

#include <cmath>
#include <vector> // For PointI in IRenderer
#include "Color.h" // Assuming Color.h is self-contained

namespace DxvUI {

    // --- Enums ---
    enum class EventType { None, MouseDown, MouseUp, MouseMove, KeyDown, KeyUp, Quit };
    enum class MouseButton { None, Left, Middle, Right };
    enum class PositionType { Relative, Absolute };
    enum class Alignment { Start, Center, End, Stretch };

    // --- Structs ---
    struct DxvEvent {
        EventType type = EventType::None;
        int x = 0;
        int y = 0;
        MouseButton button = MouseButton::None;
    };

    struct Rect {
        int x = 0, y = 0, width = 0, height = 0;
        bool contains(int pX, int pY) const { return (pX >= x && pX < (x + width) && pY >= y && pY < (y + height)); }
    };

    struct Thickness {
        float top = 0, right = 0, bottom = 0, left = 0;
    };

    struct Border {
        Color color;
        int thickness = 1;
    };

    template<typename T>
    struct Point {
        T x = 0, y = 0;
        // ... (operators and methods as before)
    };

    using PointI = Point<int>;
    using PointF = Point<float>;

}

#endif //DXVUI_CORE_H
