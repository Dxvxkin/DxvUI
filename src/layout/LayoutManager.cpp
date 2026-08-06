#include "DxvUI/layout/LayoutManager.h"

#include "DxvUI/SceneNode.h"

namespace DxvUI {

void LayoutManager::layout(const std::shared_ptr<SceneNode>& root, const Size& viewportSize) {
    if (!root) return;

    auto& data = root->layoutData;
    // Fast path: nothing in the tree needs a layout pass and the constraints
    // (e.g. the window viewport) did not change, so clean frames cost O(1).
    if (!data.isDirty && !data.isSubtreeDirty && data.lastMeasureConstraints == viewportSize) {
        return;
    }

    Rect viewportRect = {0, 0, static_cast<int>(viewportSize.width),
                         static_cast<int>(viewportSize.height)};
    root->measure(viewportSize);
    root->arrange(viewportRect);
}

Size LayoutManager::measureNode(SceneNode& node, const Size& availableSize) {
    auto& data = node.layoutData;

    if (!node.visible) {
        // An invisible node has no footprint; arrangeNode() later arranges its
        // descendants into zero-sized rects so their dirty flags get cleared.
        data.desiredSize = {0, 0};
        data.isDirty = false;
        return data.desiredSize;
    }

    // Pruned: nothing changed below and the constraints match, so the cached
    // desired size from the last measure pass is still valid.
    if (!data.isDirty && !data.isSubtreeDirty && data.lastMeasureConstraints == availableSize) {
        return data.desiredSize;
    }
    data.lastMeasureConstraints = availableSize;

    Size size = node.onMeasure(availableSize);
    node.applySizeConstraints(size);
    data.desiredSize = size;
    data.isDirty = false;
    return data.desiredSize;
}

void LayoutManager::arrangeNode(SceneNode& node, const Rect& finalRect) {
    auto& data = node.layoutData;

    if (!node.visible) {
        data.bounds = {finalRect.x, finalRect.y, 0, 0};
        for (const auto& child : node.children) {
            child->arrange({finalRect.x, finalRect.y, 0, 0});
        }
        data.isDirty = false;
        data.isSubtreeDirty = false;
        return;
    }

    // Pruned: nothing changed below and the allocated rect is unchanged, so the
    // whole subtree keeps its previous arrangement.
    if (!data.isDirty && !data.isSubtreeDirty && data.bounds == finalRect) {
        return;
    }

    data.bounds = finalRect;
    node.onArrange(finalRect);
    data.isDirty = false;
    data.isSubtreeDirty = false;
}

Rect LayoutManager::contentRect(const SceneNode& node, const Rect& outerRect) {
    const auto& padding = node.getComputedLayout(node.getCurrentState()).padding;
    return shrinkRect(outerRect, padding);
}

Size LayoutManager::addPadding(const Size& size, const Thickness& padding) {
    return {size.width + padding.left + padding.right, size.height + padding.top + padding.bottom};
}

Size LayoutManager::subtractPadding(const Size& size, const Thickness& padding) {
    return {size.width - (padding.left + padding.right),
            size.height - (padding.top + padding.bottom)};
}

Rect LayoutManager::shrinkRect(const Rect& rect, const Thickness& padding) {
    return {rect.x + static_cast<int>(padding.left), rect.y + static_cast<int>(padding.top),
            rect.width - static_cast<int>(padding.left + padding.right),
            rect.height - static_cast<int>(padding.top + padding.bottom)};
}

Size LayoutManager::measureChild(SceneNode& child, const Size& availableSize) {
    if (!child.visible) {
        return {0, 0};
    }
    const auto& margin = child.getComputedLayout(child.getCurrentState()).margin;
    const Size innerAvailableSize = subtractPadding(availableSize, margin);
    return addPadding(child.measure(innerAvailableSize), margin);
}

void LayoutManager::arrangeInvisible(SceneNode& node, const Rect& parentRect) {
    node.arrange({parentRect.x, parentRect.y, 0, 0});
}

}  // namespace DxvUI
