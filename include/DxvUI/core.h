#ifndef DXVUI_CORE_H
#define DXVUI_CORE_H

#include <string>

#include <functional> // For std::function
#include <cstdint>  // For uint16_t
#include <memory>

#include "DxvUI/style/Color.h"

namespace DxvUI {

    class SceneNode; // Forward declaration

    // --- Enums ---
    enum class EventType {
        None,
        // Raw input events
        MouseDown, MouseUp, MouseMove, KeyDown, KeyUp, TextInput, Quit,
        // Derived UI events
        Click, HoverEnter, HoverLeave, FocusGained, FocusLost, Drag, Drop, Attach, Detach
    };
    enum class MouseButton { None, Left, Middle, Right };
    enum class PositionType { Relative, Absolute };
    enum class Alignment { Start, Center, End, Stretch };

    // System cursor types
    enum class CursorType {
        Arrow,      // Default pointer
        IBeam,      // Text input
        Wait,       // Busy indicator
        Crosshair,
        Hand,       // Hand pointer for links/buttons
        ResizeNWSE, // Diagonal resize
        ResizeNESW, // Diagonal resize
        ResizeWE,   // Horizontal resize
        ResizeNS,   // Vertical resize
        ResizeAll,
        No          // Hidden cursor
    };

    enum KeyModifier : uint16_t {
        None    = 0x0000,
        LShift  = 0x0001,
        RShift  = 0x0002,
        LCtrl   = 0x0040,
        RCtrl   = 0x0080,
        LAlt    = 0x0100,
        RAlt    = 0x0200,
        Shift   = LShift | RShift,
        Ctrl    = LCtrl | RCtrl,
        Alt     = LAlt | RAlt,
    };

    // --- Structs & Core Types ---
    struct DxvEvent {
        EventType type = EventType::None;
        std::weak_ptr<SceneNode> target;
        std::weak_ptr<SceneNode> relatedNode;
        bool handled = false;

        struct {
            int x = 0;
            int y = 0;
            int dx = 0;
            int dy = 0;
            MouseButton button = MouseButton::None;
        } mouse;

        struct {
            int sym = 0;
            int scancode = 0;
            uint16_t mod = 0;
        } key;

        std::string text;
    };

    using ActionCallback = std::function<void(DxvEvent&)>;

    struct Size {
        float width = 0, height = 0;
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
    };
    using PointI = Point<int>;
    using PointF = Point<float>;

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

}

#endif //DXVUI_CORE_H
