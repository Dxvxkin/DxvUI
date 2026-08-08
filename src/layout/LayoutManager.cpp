#include "DxvUI/layout/LayoutManager.h"

#include <algorithm>

#include "DxvUI/SceneNode.h"

namespace DxvUI {

namespace {

// Applies the resolved explicit size and min/max constraints to a size. This is
// the sizing step of the measure pass: an explicit width/height from the style
// wins over the measured size and the min/max clamping.
void applySizeConstraints(const SceneNode& node, Size& size) {
    const auto& computedLayout = node.getComputedLayout(node.getCurrentState());

    if (computedLayout.width > 0) {
        size.width = computedLayout.width;
    } else {
        if (computedLayout.minWidth.has_value()) {
            size.width = std::max(size.width, computedLayout.minWidth.value());
        }
        if (computedLayout.maxWidth.has_value()) {
            size.width = std::min(size.width, computedLayout.maxWidth.value());
        }
    }

    if (computedLayout.height > 0) {
        size.height = computedLayout.height;
    } else {
        if (computedLayout.minHeight.has_value()) {
            size.height = std::max(size.height, computedLayout.minHeight.value());
        }
        if (computedLayout.maxHeight.has_value()) {
            size.height = std::min(size.height, computedLayout.maxHeight.value());
        }
    }
}

}  // namespace

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
        // An invisible node has no footprint. Parents skip invisible children
        // in measureChild(), so this branch only guards direct measure() calls;
        // isSubtreeDirty is deliberately kept set here and cleared later by
        // arrangeNode()'s invisible branch, which also zeroes the descendants.
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
    applySizeConstraints(node, size);
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

    data.lastArrangeRect = data.bounds;
    data.bounds = finalRect;
    node.onArrange(finalRect);
    data.isDirty = false;
    data.isSubtreeDirty = false;
}

Rect LayoutManager::contentRect(const SceneNode& node, const Rect& outerRect) {
    const auto& computed = node.getComputedLayout(node.getCurrentState());
    const int border = node.getComputedAppearance(node.getCurrentState()).borderThickness;
    Thickness inset = computed.padding;
    inset.left += border;
    inset.top += border;
    inset.right += border;
    inset.bottom += border;
    return shrinkRect(outerRect, inset);
}

Size LayoutManager::addPadding(const Size& size, const Thickness& padding) {
    return {size.width + padding.left + padding.right, size.height + padding.top + padding.bottom};
}

Size LayoutManager::subtractPadding(const Size& size, const Thickness& padding) {
    return {size.width - (padding.left + padding.right),
            size.height - (padding.top + padding.bottom)};
}

Rect LayoutManager::shrinkRect(const Rect& rect, const Thickness& padding) {
    const int dx = static_cast<int>(padding.left + padding.right);
    const int dy = static_cast<int>(padding.top + padding.bottom);
    return {rect.x + static_cast<int>(padding.left), rect.y + static_cast<int>(padding.top),
            std::max(0, rect.width - dx), std::max(0, rect.height - dy)};
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

Rect LayoutManager::alignChild(SceneNode& child, const Size& childSize, const Rect& containerRect,
                               const AlignAxes& axes) {
    const auto& layout = child.getComputedLayout(child.getCurrentState());
    const auto& margin = layout.margin;

    // On a disabled axis the caller owns the placement, so the slot origin is
    // used as-is (the caller bakes any margin offset into it).
    int x = containerRect.x;
    if (axes.horizontal) {
        const int available = containerRect.width - static_cast<int>(margin.left + margin.right);
        float offset = 0.0f;
        if (layout.horizontalAlignment == Alignment::Center) {
            offset = (available - childSize.width) / 2.0f;
        } else if (layout.horizontalAlignment == Alignment::End) {
            offset = available - childSize.width;
        }
        x = containerRect.x + static_cast<int>(margin.left) + static_cast<int>(offset);
    }

    int y = containerRect.y;
    if (axes.vertical) {
        const int available = containerRect.height - static_cast<int>(margin.top + margin.bottom);
        float offset = 0.0f;
        if (layout.verticalAlignment == Alignment::Center) {
            offset = (available - childSize.height) / 2.0f;
        } else if (layout.verticalAlignment == Alignment::End) {
            offset = available - childSize.height;
        }
        y = containerRect.y + static_cast<int>(margin.top) + static_cast<int>(offset);
    }

    return {x, y, static_cast<int>(childSize.width), static_cast<int>(childSize.height)};
}

}  // namespace DxvUI
