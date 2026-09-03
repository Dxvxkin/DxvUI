#ifndef DXVUI_DXVEVENT_H
#define DXVUI_DXVEVENT_H

#include <cstdint>
#include <functional>  // For std::function
#include <memory>
#include <string>

namespace DxvUI {

class SceneNode;     // Forward declaration
class UIContext;     // Forward declaration
class EventManager;  // Forward declaration

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
    // The host SDL window changed size (SDL_WINDOWEVENT_SIZE_CHANGED /
    // SDL_WINDOWEVENT_RESIZED). Width/height in the `resize` field. Relevant
    // mainly in external-renderer mode, where the host owns the window.
    Resize,
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
 * @brief The propagation phase an event is currently in (DOM UI Events model).
 *
 * A dispatched event walks the tree in three phases: Capture (root down to the
 * target's parent), Target (the target node itself) and Bubble (the target's
 * parents back up to the root). Set by the EventManager as it walks the path
 * and read through DxvEvent::getPhase().
 */
enum class EventPhase { None, Capture, Target, Bubble };

/**
 * @brief Immutable per-type event behaviour (single source of truth).
 *
 * These describe how a given EventType propagates regardless of any single
 * event instance, so the behaviour cannot be overridden per-instance (matching
 * the DOM UI Events model). Read through the DxvEvent accessors, never through
 * this struct directly outside DxvEvent.h/DxvEvent.cpp.
 *
 *   bubbles     whether the event travels back up from the target to the root
 *               (W3C: keyboard/pointer/click/wheel/drag bubble; focus/hover
 *               use the non-bubbling focus/blur, mouseenter/mouseleave forms).
 *   cancelable  whether preventDefault() cancels the target's default action
 *               (onEvent). Input/click/wheel events are cancelable; lifecycle
 *               and state events (focus/hover/attach/change) are not.
 *   captureable whether the event is delivered through the Capture phase.
 *               Only raw input (pointer/key/text) travels root->target, so only
 *               those can be intercepted before reaching their target.
 */
struct EventMeta {
    bool bubbles;
    bool cancelable;
    bool captureable;
};

/**
 * @brief The immutable per-type event metadata table.
 * @param type The event type.
 * @return The propagation metadata for @p type.
 */
constexpr EventMeta eventMeta(EventType type) {
    switch (type) {
        case EventType::MouseDown:
        case EventType::MouseUp:
        case EventType::MouseMove:
        case EventType::MouseWheel:
        case EventType::KeyDown:
        case EventType::KeyUp:
        case EventType::TextInput:
        case EventType::Click:
        case EventType::Drag:
        case EventType::Drop:
            return {true, true, true};
        case EventType::Attach:
        case EventType::Detach:
        case EventType::Change:
            return {true, false, false};
        case EventType::HoverEnter:
        case EventType::HoverLeave:
        case EventType::FocusGained:
        case EventType::FocusLost:
            return {false, false, false};
        case EventType::Quit:
        case EventType::Resize:
        case EventType::None:
        default:
            return {false, false, false};
    }
}

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

    struct {
        int width = 0;
        int height = 0;
    } resize;

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

    // --- Propagation behaviour (from the immutable eventMeta table) ---
    // These cannot be overridden per-instance; they describe the event type's
    // behaviour and match the DOM UI Events model (W3C).
    [[nodiscard]] bool bubbles() const { return eventMeta(type).bubbles; }
    [[nodiscard]] bool cancelable() const { return eventMeta(type).cancelable; }
    [[nodiscard]] bool captureable() const { return eventMeta(type).captureable; }

    // --- Propagation phase ---
    // Set by the EventManager as it walks the path; read by listeners to tell
    // which phase they are in.
    [[nodiscard]] EventPhase getPhase() const { return phase_; }

    // Logging helper: returns "" instead of a dangling dereference.
    [[nodiscard]] std::string getTargetId() const;

   private:
    bool propagationStopped_ = false;
    bool immediatePropagationStopped_ = false;
    bool defaultPrevented_ = false;
    EventPhase phase_ = EventPhase::None;

    // The EventManager drives the phased walk and must mutate the phase;
    // SceneNode writes it in its per-phase dispatch.
    friend class EventManager;
    friend class SceneNode;
};

using ActionCallback = std::function<void(DxvEvent&, const UIContext&)>;

}  // namespace DxvUI

#endif  // DXVUI_DXVEVENT_H
