#include "DxvUI/containers/AbsoluteContainer.h"

#include <algorithm>

#include "DxvUI/layout/LayoutManager.h"

namespace DxvUI {

Size AbsoluteContainer::onMeasure(const Size& availableSize) {
    const auto& computedLayout = getComputedLayout();
    const auto& padding = computedLayout.padding;

    float requiredWidth = 0.0f;
    float requiredHeight = 0.0f;

    for (const auto& child : children) {
        if (!child->isVisible()) continue;

        const auto& childLayout = child->getComputedLayout(child->getCurrentState());

        // Clean children keep the result of their last measure pass; reuse the
        // cached outer size instead of re-entering measureChild()/measureNode().
        // The condition mirrors measureNode()'s prune check (LayoutManager.cpp),
        // so the cached size is only reused while it is actually valid.
        Size childOuterSize;
        if (!child->isLayoutDirty() &&
            child->getLastMeasureConstraints() ==
                LayoutManager::subtractPadding(availableSize, childLayout.margin)) {
            childOuterSize = LayoutManager::addPadding(child->getDesiredSize(), childLayout.margin);
        } else {
            childOuterSize = LayoutManager::measureChild(*child, availableSize);
        }

        // The positioned element is the child's margin-box. If only
        // 'right'/'bottom' are set, the child is anchored to the opposite edge,
        // so it contributes to the required size by its own outer size plus the
        // 'right'/'bottom' value.
        float childLeft = childLayout.left.value_or(0.0f);
        if (!childLayout.left.has_value() && childLayout.right.has_value()) {
            childLeft = childLayout.right.value();
        }
        float childTop = childLayout.top.value_or(0.0f);
        if (!childLayout.top.has_value() && childLayout.bottom.has_value()) {
            childTop = childLayout.bottom.value();
        }

        requiredWidth = std::max(requiredWidth, childLeft + childOuterSize.width);
        requiredHeight = std::max(requiredHeight, childTop + childOuterSize.height);
    }

    return LayoutManager::addPadding({requiredWidth, requiredHeight}, padding);
}

void AbsoluteContainer::onArrange(const Rect& finalRect) {
    const auto& computedLayout = getComputedLayout();

    const auto& padding = computedLayout.padding;
    Rect content = LayoutManager::contentRect(*this, finalRect);

    // Children are positioned from their own styles (absolute anchoring),
    // independent of siblings. If this container neither moved/resized nor
    // changed its own layout state, a clean child keeps the exact rect assigned
    // in the previous arrange pass, so it can be skipped entirely.
    const bool rectChanged = layoutData.lastArrangeRect != finalRect;

    for (const auto& child : children) {
        if (!child->isVisible()) {
            LayoutManager::arrangeInvisible(*child, content);
            continue;
        }

        if (!rectChanged && !layoutData.isDirty && !child->isLayoutDirty()) {
            continue;
        }

        const auto& childLayout = child->getComputedLayout(child->getCurrentState());
        const auto& margin = childLayout.margin;
        Size childDesiredSize = child->getDesiredSize();

        int childW =
            static_cast<int>(childLayout.width > 0 ? childLayout.width : childDesiredSize.width);
        int childH =
            static_cast<int>(childLayout.height > 0 ? childLayout.height : childDesiredSize.height);

        // Position: explicit 'left'/'top' win, otherwise anchor to the
        // 'right'/'bottom' edge; the anchored axis keeps its position (the
        // margin is baked into the slot origin). Axes without anchors are free
        // and get aligned by the child's horizontal/vertical alignment instead
        // of sticking to the top-left corner.
        int slotX = content.x;
        if (childLayout.left.has_value()) {
            slotX = content.x + static_cast<int>(childLayout.left.value() + margin.left);
        } else if (childLayout.right.has_value()) {
            slotX = content.x + content.width - childW -
                    static_cast<int>(childLayout.right.value() + margin.right);
        }

        int slotY = content.y;
        if (childLayout.top.has_value()) {
            slotY = content.y + static_cast<int>(childLayout.top.value() + margin.top);
        } else if (childLayout.bottom.has_value()) {
            slotY = content.y + content.height - childH -
                    static_cast<int>(childLayout.bottom.value() + margin.bottom);
        }

        const AlignAxes axes = {
            .horizontal = !childLayout.left.has_value() && !childLayout.right.has_value(),
            .vertical = !childLayout.top.has_value() && !childLayout.bottom.has_value()};
        const Rect childFinalRect = LayoutManager::alignChild(
            *child, {static_cast<float>(childW), static_cast<float>(childH)},
            {slotX, slotY, content.width, content.height}, axes);

        child->arrange(childFinalRect);
    }
}

}  // namespace DxvUI
