#ifndef DXVUI_DXVEVENT_H
#define DXVUI_DXVEVENT_H

#include <cstdint>
#include <functional>  // For std::function
#include <memory>
#include <string>

namespace DxvUI {

class SceneNode;  // Forward declaration

// --- Enums ---
enum class EventType {
    None,
    // Raw input events
    MouseDown,
    MouseUp,
    MouseMove,
    KeyDown,
    KeyUp,
    TextInput,
    Quit,
    // Derived UI events
    Click,
    HoverEnter,
    HoverLeave,
    FocusGained,
    FocusLost,
    Drag,
    Drop,
    Attach,
    Detach,
    Change
};

enum class MouseButton { None, Left, Middle, Right };

enum KeyModifier : uint16_t {
    None = 0x0000,
    LShift = 0x0001,
    RShift = 0x0002,
    LCtrl = 0x0040,
    RCtrl = 0x0080,
    LAlt = 0x0100,
    RAlt = 0x0200,
    Shift = LShift | RShift,
    Ctrl = LCtrl | RCtrl,
    Alt = LAlt | RAlt,
};

// --- Event Struct & Callback ---
struct DxvEvent {
    EventType type = EventType::None;
    std::weak_ptr<SceneNode> target;
    std::weak_ptr<SceneNode> currentTarget;
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

    // --- Convenience Accessors ---
    // Return nullptr when the referenced node has been destroyed.
    [[nodiscard]] std::shared_ptr<SceneNode> getTarget() const { return target.lock(); }
    [[nodiscard]] std::shared_ptr<SceneNode> getCurrentTarget() const {
        return currentTarget.lock();
    }
    [[nodiscard]] std::shared_ptr<SceneNode> getRelatedNode() const { return relatedNode.lock(); }

    // Logging helper: returns "" instead of a dangling dereference.
    [[nodiscard]] std::string getTargetId() const;
};

using ActionCallback = std::function<void(DxvEvent&)>;

}  // namespace DxvUI

#endif  // DXVUI_DXVEVENT_H
