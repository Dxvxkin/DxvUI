#ifndef DXVUI_NODESTATE_H
#define DXVUI_NODESTATE_H

#include <cstdint>

namespace DxvUI {

/**
 * @brief Compact, independent per-node state flags held by a SceneNode.
 *
 * The flags are independent facts ("is hovered?", "is focused?", "is visible?")
 * and so can be combined: a node may be both focused and hovered at once
 * (testAny()/raw() expose that combination). This holder knows nothing about
 * layout, styling or the scene; SceneNode owns the side effects (invalidating
 * layout, notifying the scene) triggered by a change.
 */
class NodeState {
   public:
    enum class Flag : uint8_t {
        Hovered = 1 << 0,
        Pressed = 1 << 1,
        Focused = 1 << 2,
        Enabled = 1 << 3,
        Visible = 1 << 4,
    };

    /**
     * @brief Starts enabled and visible, with no interaction flags set.
     */
    NodeState() noexcept
        : flags_(static_cast<uint8_t>(Flag::Enabled) | static_cast<uint8_t>(Flag::Visible)) {}

    /**
     * @brief Whether the given flag is set.
     */
    bool test(Flag flag) const noexcept { return (flags_ & static_cast<uint8_t>(flag)) != 0; }

    /**
     * @brief Whether any one of the given flags is set.
     *
     * Unlike test(), this accepts a combination of flags, exposing the combined
     * state (e.g. Flag::Focused | Flag::Hovered) instead of collapsing it to a
     * single value.
     */
    bool testAny(uint8_t flags) const noexcept { return (flags_ & flags) != 0; }

    /**
     * @brief Sets or clears a flag.
     * @param flag The flag to change.
     * @param value The new value.
     * @return True if the flag value actually changed, false if it was already
     * the requested value. Callers use the return to skip unnecessary work
     * (e.g. layout invalidation).
     */
    bool take(Flag flag, bool value) noexcept {
        const uint8_t bit = static_cast<uint8_t>(flag);
        const bool was = (flags_ & bit) != 0;
        if (value) {
            flags_ |= bit;
        } else {
            flags_ &= static_cast<uint8_t>(~bit);
        }
        return was != value;
    }

    /**
     * @brief Raw bitmask of the currently set flags.
     *
     * Lets callers inspect the combined state directly instead of collapsing it
     * to a single value.
     */
    uint8_t raw() const noexcept { return flags_; }

   private:
    uint8_t flags_;
};

}  // namespace DxvUI

#endif  // DXVUI_NODESTATE_H
