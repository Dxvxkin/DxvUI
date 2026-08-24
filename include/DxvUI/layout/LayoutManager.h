#ifndef DXVUI_LAYOUTMANAGER_H
#define DXVUI_LAYOUTMANAGER_H

#include <memory>

#include "DxvUI/core.h"

namespace DxvUI {

class SceneNode;

/**
 * @brief Which axes of a child's slot a parent lets the alignment apply on.
 *
 * A container manages one axis itself (e.g. the flow axis of a row) and
 * disables alignment on it; the other axis is handed to alignChild().
 */
struct AlignAxes {
    bool horizontal = false;
    bool vertical = false;
};

/**
 * @class LayoutManager
 * @brief Drives the two-pass measure/arrange layout cycle for a scene tree.
 *
 * A pure computation unit: it depends only on the node tree, so it can be
 * tested without a Scene or a renderer. layout() prunes clean subtrees, so a
 * clean frame costs O(1) and a localized edit only walks the affected branch.
 */
class LayoutManager {
   public:
    /**
     * @brief Runs the measure + arrange passes for a tree rooted at @p root.
     *
     * Fast path: when nothing in the tree is dirty and the viewport constraints
     * did not change, the call returns immediately.
     * @param root The root of the tree to lay out.
     * @param viewportSize The size available to the root.
     */
    void layout(const std::shared_ptr<SceneNode>& root, const Size& viewportSize);

    /**
     * @brief Computes the desired size of a node, re-measuring only when needed.
     * @param node The node to measure.
     * @param availableSize The size available from the parent.
     * @return The node's desired size.
     */
    static Size measureNode(SceneNode& node, const Size& availableSize);

    /**
     * @brief Assigns a node's final rect, re-arranging only when needed.
     * @param node The node to arrange.
     * @param finalRect The rect allocated by the parent.
     */
    static void arrangeNode(SceneNode& node, const Rect& finalRect);

    /**
     * @brief The node's rect shrunk by its computed padding.
     * @param node The container to compute the content rect for.
     * @param outerRect The container's own rect.
     * @return The inner rect available to children.
     */
    static Rect contentRect(const SceneNode& node, const Rect& outerRect);

    /**
     * @brief Adds the node's padding to a size.
     * @param size The inner size (e.g. the sum of children's desired sizes).
     * @param padding The padding to expand by.
     * @return The size inflated by @p padding on all four sides.
     */
    static Size addPadding(const Size& size, const Thickness& padding);

    /**
     * @brief Subtracts the node's padding from an available size.
     * @param size The outer available size.
     * @param padding The padding to shrink by.
     * @return The inner size left for children (may go negative).
     */
    static Size subtractPadding(const Size& size, const Thickness& padding);

    /**
     * @brief Shrinks a rect by padding on all four sides.
     * @param rect The outer rect.
     * @param padding The padding to shrink by.
     * @return The inner rect available to children.
     */
    static Rect shrinkRect(const Rect& rect, const Thickness& padding);

    /**
     * @brief Measures a child the way its parent would, accounting for its margin.
     *
     * The child's own margin reduces the size available to it, and the returned
     * size is the child's outer box (measured size inflated by the margin).
     * Invisible children contribute nothing.
     * @param child The child to measure.
     * @param availableSize The size available to the parent.
     * @return The child's outer size, including its margin.
     */
    static Size measureChild(SceneNode& child, const Size& availableSize);

    /**
     * @brief Arranges a hidden child into a zero-sized rect.
     *
     * Containers use this for invisible children so that their dirty flags get
     * cleared and their own children are arranged into zero-sized rects too.
     * @param node The hidden child.
     * @param parentRect The parent's content rect (origin is reused).
     */
    static void arrangeInvisible(SceneNode& node, const Rect& parentRect);

    /**
     * @brief Positions a child inside a slot according to its alignment and margin.
     *
     * On a disabled axis the child is placed at the slot's origin as-is (the
     * caller owns that axis and bakes any margin offset into the slot). On an
     * enabled axis the child's margin-box is aligned inside the slot by the
     * child's computed horizontal/vertical alignment (Start keeps the position,
     * Center/End shift it), then the child itself is offset by its margin. On a
     * Stretch axis the child fills the slot (minus margin) instead.
     * @param child The child to position (reads its computed margin and alignment).
     * @param childSize The child's final size (explicit width/height or measured).
     * @param containerRect The slot the parent gives to the child.
     * @param axes The axes to apply alignment on.
     * @return The child's own rect, ready to pass to arrange().
     */
    static Rect alignChild(SceneNode& child, const Size& childSize, const Rect& containerRect,
                           const AlignAxes& axes);
};

}  // namespace DxvUI

#endif  // DXVUI_LAYOUTMANAGER_H
