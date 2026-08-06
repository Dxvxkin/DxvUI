#ifndef DXVUI_LAYOUTMANAGER_H
#define DXVUI_LAYOUTMANAGER_H

#include <memory>

#include "DxvUI/core.h"

namespace DxvUI {

class SceneNode;

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
     * @brief Arranges a hidden child into a zero-sized rect.
     *
     * Containers use this for invisible children so that their dirty flags get
     * cleared and their own children are arranged into zero-sized rects too.
     * @param node The hidden child.
     * @param parentRect The parent's content rect (origin is reused).
     */
    static void arrangeInvisible(SceneNode& node, const Rect& parentRect);
};

}  // namespace DxvUI

#endif  // DXVUI_LAYOUTMANAGER_H
