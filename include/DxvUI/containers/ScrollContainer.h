#ifndef DXVUI_SCROLLCONTAINER_H
#define DXVUI_SCROLLCONTAINER_H

#include <memory>
#include <string>

#include "DxvUI/containers/Container.h"

namespace DxvUI {

/**
 * @class ScrollContainer
 * @brief A viewport that scrolls a single child when its content overflows.
 *
 * The ScrollContainer keeps a fixed viewport size (it takes the space its
 * parent gives it) and scrolls its single child inside it when the child is
 * larger than the viewport. The child (usually a VerticalContainer or
 * HorizontalContainer holding the items) is arranged offset by the current
 * scroll position and clipped to the viewport via the built-in clipContent
 * style, so overflowing content is cut off instead of overflowing visually.
 *
 * Scrolling is driven by the mouse wheel: because the wheel is routed to the
 * hovered node, hovering anywhere over the viewport — including over a child —
 * scrolls the container (the event bubbles up from the leaf). Tuck a nested
 * ScrollContainer only when you want nested scrolling; the deepest one consumes
 * the wheel via stopPropagation().
 *
 * There are no visible scrollbars in this base implementation; the scroll offset
 * is manipulated programmatically (setScrollX/setScrollY/scrollBy) or via the
 * wheel. The content size is tracked during measure, so the offset is clamped to
 * the [0, content - viewport] range and never goes negative.
 *
 * Usage:
 * @code
 * auto scroll = ScrollContainer::create("list");
 * scroll->setStyle({.width = 200, .height = 300}, WidgetState::Normal);
 * auto items = VerticalContainer::create("items");
 * for (...) items->addChild(Label::create("", "Item"));
 * scroll->addChild(items);
 * root->addChild(scroll);
 * @endcode
 */
class ScrollContainer : public Container {
   public:
    static std::shared_ptr<ScrollContainer> create(std::string id);

    explicit ScrollContainer(std::string id);

    /**
     * @brief Sets the horizontal scroll offset (in pixels).
     * @param x The offset, clamped into the valid range.
     */
    void setScrollX(float x);

    /**
     * @brief Sets the vertical scroll offset (in pixels).
     * @param y The offset, clamped into the valid range.
     */
    void setScrollY(float y);

    /**
     * @brief Gets the current horizontal scroll offset.
     */
    float getScrollX() const;

    /**
     * @brief Gets the current vertical scroll offset.
     */
    float getScrollY() const;

    /**
     * @brief Scrolls by the given deltas (in pixels).
     *
     * A positive dy scrolls down (shows further content below), a negative dy
     * scrolls up. The result is clamped into the valid range.
     * @param dx The horizontal delta, in pixels.
     * @param dy The vertical delta, in pixels.
     */
    void scrollBy(float dx, float dy);

    // --- Overrides ---
    const char* getNodeType() const noexcept override;

   protected:
    Size onMeasure(const Size& availableSize) override;
    void onArrange(const Rect& finalRect) override;

   private:
    // Clamps an offset into the valid [0, content - viewport] range for its axis.
    float clampScrollX(float x) const;
    float clampScrollY(float y) const;

    // Registers a self-owned MouseWheel listener so the container reacts to the
    // wheel wherever the cursor hovers over the viewport (its own Target phase,
    // or the Bubble phase when a child is the target). The wheel is a "watched"
    // event of an ancestor rather than this node's default action, so it is
    // handled by a regular listener which consumes it via stopPropagation().
    void setupWheelHandler();

    std::unique_ptr<SceneNode::Connection> wheelConnection_;

    // The scroll offset applied to the child during arrange. Never negative;
    // the upper bound is recomputed from the measured content vs viewport.
    float scrollX_ = 0.0f;
    float scrollY_ = 0.0f;
    // The measured size of the single child (its full content extent), used to
    // clamp the scroll offset to the valid range.
    Size contentSize_{0.0f, 0.0f};
};

}  // namespace DxvUI

#endif  // DXVUI_SCROLLCONTAINER_H
