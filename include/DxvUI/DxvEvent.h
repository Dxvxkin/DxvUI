#ifndef DXVUI_DXVEVENT_H
#define DXVUI_DXVEVENT_H

#include <cstdint>
#include <functional>  // For std::function
#include <memory>
#include <string>

namespace DxvUI {

class SceneNode;  // Forward declaration
class UIContext;  // Forward declaration

// --- Enums ---
enum class EventType {
    None,
    // Raw input events
    MouseDown,
    MouseUp,
    MouseMove,
    MouseWheel,
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

/**
 * @brief Backend-neutral key identifiers.
 *
 * The event source translates the backend's physical key symbols (e.g.
 * SDL_Keycode) into these values, so UI code never touches backend key
 * enums. Letters map to their uppercase value (SDLK_a and SDLK_A both arrive
 * as KeyCode::A; case is conveyed by KeyModifier::Shift).
 */
enum class KeyCode : int {
    Unknown = 0,
    Backspace,
    Tab,
    Enter,
    Escape,
    Space,
    Left,
    Right,
    Up,
    Down,
    Home,
    End,
    PageUp,
    PageDown,
    Delete,
    Insert,
    A,
    B,
    C,
    D,
    E,
    F,
    G,
    H,
    I,
    J,
    K,
    L,
    M,
    N,
    O,
    P,
    Q,
    R,
    S,
    T,
    U,
    V,
    W,
    X,
    Y,
    Z,
    Digit0,
    Digit1,
    Digit2,
    Digit3,
    Digit4,
    Digit5,
    Digit6,
    Digit7,
    Digit8,
    Digit9,
    F1,
    F2,
    F3,
    F4,
    F5,
    F6,
    F7,
    F8,
    F9,
    F10,
    F11,
    F12,
};

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

    struct {
        int x = 0;
        int y = 0;
        int dx = 0;
        int dy = 0;
        MouseButton button = MouseButton::None;
    } mouse;

    struct {
        KeyCode sym = KeyCode::Unknown;
        uint16_t mod = 0;
        // Non-zero when the OS key autorepeat is delivering a held-down key.
        // Needed so editors can hold Backspace or the arrow keys to repeat an
        // action instead of doing one step per physical press.
        uint8_t repeat = 0;
    } key;

    std::string text;

    // --- Event flow control (DOM-style) ---
    // Events are single-use: the event manager builds a fresh DxvEvent per
    // dispatch, so the flags are never reset between bubble levels.
    //
    //   preventDefault()        cancels the widget's default action (onEvent)
    //   stopPropagation()       stops bubbling to parent nodes
    //   stopImmediatePropagation()  stops the remaining listeners on the current
    //                          node and bubbling; the default action still runs
    void stopPropagation() { propagationStopped_ = true; }
    void stopImmediatePropagation() {
        immediatePropagationStopped_ = true;
        propagationStopped_ = true;
    }
    void preventDefault() { defaultPrevented_ = true; }

    [[nodiscard]] bool isPropagationStopped() const { return propagationStopped_; }
    [[nodiscard]] bool isImmediatePropagationStopped() const {
        return immediatePropagationStopped_;
    }
    [[nodiscard]] bool isDefaultPrevented() const { return defaultPrevented_; }

    // --- Convenience Accessors ---
    // Return nullptr when the referenced node has been destroyed.
    [[nodiscard]] std::shared_ptr<SceneNode> getTarget() const { return target.lock(); }
    [[nodiscard]] std::shared_ptr<SceneNode> getCurrentTarget() const {
        return currentTarget.lock();
    }
    [[nodiscard]] std::shared_ptr<SceneNode> getRelatedNode() const { return relatedNode.lock(); }

    // Logging helper: returns "" instead of a dangling dereference.
    [[nodiscard]] std::string getTargetId() const;

   private:
    bool propagationStopped_ = false;
    bool immediatePropagationStopped_ = false;
    bool defaultPrevented_ = false;
};

using ActionCallback = std::function<void(DxvEvent&, const UIContext&)>;

}  // namespace DxvUI

#endif  // DXVUI_DXVEVENT_H
