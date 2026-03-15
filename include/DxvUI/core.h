#ifndef DXVUI_CORE_H
#define DXVUI_CORE_H

#include <cmath>

#include "Color.h"

namespace DxvUI {

    enum class EventType {
        None, // Represents no event
        MouseDown,
        MouseUp,
        MouseMove,
        KeyDown,
        KeyUp,
        Quit // Represents a request to close the application
    };

    enum class MouseButton {
        None,
        Left,
        Middle,
        Right
    };

    struct DxvEvent {
        EventType type = EventType::None;
        int x = 0;
        int y = 0;
        MouseButton button = MouseButton::None; // Relevant for MouseDown/MouseUp
    };


    struct Rect {
        int x = 0;
        int y = 0;
        int width = 0;
        int height = 0;

        bool contains(int pointX, int pointY) const {
            return (pointX >= x && pointX < (x + width) &&
                    pointY >= y && pointY < (y + height));
        }
    };

    struct Border {
        Color color;
        int thickness = 1;
    };

    template<typename T>
    struct Point {
        T x = 0;
        T y = 0;

        // --- Operators ---
        Point<T> operator+(const Point<T>& other) const { return {x + other.x, y + other.y}; }
        Point<T> operator-(const Point<T>& other) const { return {x - other.x, y - other.y}; }
        Point<T> operator*(T scalar) const { return {x * scalar, y * scalar}; }
        Point<T> operator/(T scalar) const { return {x / scalar, y / scalar}; }

        Point<T>& operator+=(const Point<T>& other) { x += other.x; y += other.y; return *this; }
        Point<T>& operator-=(const Point<T>& other) { x -= other.x; y -= other.y; return *this; }
        Point<T>& operator*=(T scalar) { x *= scalar; y *= scalar; return *this; }
        Point<T>& operator/=(T scalar) { x /= scalar; y /= scalar; return *this; }

        bool operator==(const Point<T>& other) const { return x == other.x && y == other.y; }
        bool operator!=(const Point<T>& other) const { return !(*this == other); }

        // --- Vector methods ---

        // Returns the magnitude (length) of the vector.
        float length() const {
            return std::sqrt(static_cast<float>(x * x + y * y));
        }

        // Returns the squared magnitude. Faster than length() as it avoids a square root.
        T lengthSq() const {
            return x * x + y * y;
        }

        // Returns a new vector with the same direction but a length of 1.
        Point<float> normalized() const {
            float l = length();
            if (l > 0) {
                return {static_cast<float>(x) / l, static_cast<float>(y) / l};
            }
            return {0.0f, 0.0f};
        }
    };

    // Type aliases for common use cases
    using PointI = Point<int>;
    using PointF = Point<float>;

}

#endif //DXVUI_CORE_H
