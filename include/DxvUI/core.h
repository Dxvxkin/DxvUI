#ifndef DXVUI_CORE_H
#define DXVUI_CORE_H

namespace DxvUI {

    enum class EventType {
        MouseDown,
        MouseUp,
        MouseMove,
        KeyDown,
        KeyUp
    };

    struct DxvEvent {
        EventType type;
        int x;
        int y;
    };

    struct Thickness {
        int left = 0;
        int top = 0;
        int right = 0;
        int bottom = 0;
    };

    enum class Anchor {
        TopLeft,
        TopCenter,
        TopRight,
        CenterLeft,
        Center,
        CenterRight,
        BottomLeft,
        BottomCenter,
        BottomRight
    };

}

#endif //DXVUI_CORE_H
