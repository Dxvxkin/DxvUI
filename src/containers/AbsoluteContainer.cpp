#include "DxvUI/containers/AbsoluteContainer.h"

#include <algorithm>

#include "DxvUI/layout/LayoutManager.h"

namespace DxvUI {

Size AbsoluteContainer::onMeasure(const Size& availableSize) {
    const auto& computedLayout = getComputedLayout(getCurrentState());
    const auto& padding = computedLayout.padding;

    float requiredWidth = 0.0f;
    float requiredHeight = 0.0f;

    for (const auto& child : children) {
        if (!child->isVisible()) continue;

        Size childDesiredSize = child->measure(availableSize);
        const auto& childLayout = child->getComputedLayout(child->getCurrentState());

        float childWidth = childLayout.width > 0 ? childLayout.width : childDesiredSize.width;
        float childHeight = childLayout.height > 0 ? childLayout.height : childDesiredSize.height;

        // If only 'right'/'bottom' are set, the child is anchored to the
        // opposite edge, so it contributes to the required size as if it were
        // offset by its own size plus the margin.
        float childLeft = childLayout.left.value_or(0.0f);
        if (!childLayout.left.has_value() && childLayout.right.has_value()) {
            childLeft = childWidth + childLayout.right.value();
        }
        float childTop = childLayout.top.value_or(0.0f);
        if (!childLayout.top.has_value() && childLayout.bottom.has_value()) {
            childTop = childHeight + childLayout.bottom.value();
        }

        requiredWidth = std::max(requiredWidth, childLeft + childWidth);
        requiredHeight = std::max(requiredHeight, childTop + childHeight);
    }

    return {requiredWidth + padding.left + padding.right,
            requiredHeight + padding.top + padding.bottom};
}

void AbsoluteContainer::onArrange(const Rect& finalRect) {
    const auto& computedLayout = getComputedLayout(getCurrentState());

    const auto& padding = computedLayout.padding;
    Rect content = LayoutManager::contentRect(*this, finalRect);

    for (const auto& child : children) {
        if (!child->isVisible()) {
            LayoutManager::arrangeInvisible(*child, content);
            continue;
        }

        const auto& childLayout = child->getComputedLayout(child->getCurrentState());
        Size childDesiredSize = child->getDesiredSize();

        int childW =
            static_cast<int>(childLayout.width > 0 ? childLayout.width : childDesiredSize.width);
        int childH =
            static_cast<int>(childLayout.height > 0 ? childLayout.height : childDesiredSize.height);

        // Position: explicit 'left'/'top' win; otherwise anchor to the
        // 'right'/'bottom' edge; otherwise stick to the top-left corner.
        int childX;
        if (childLayout.left.has_value()) {
            childX = content.x + static_cast<int>(childLayout.left.value());
        } else if (childLayout.right.has_value()) {
            childX =
                content.x + content.width - childW - static_cast<int>(childLayout.right.value());
        } else {
            childX = content.x;
        }

        int childY;
        if (childLayout.top.has_value()) {
            childY = content.y + static_cast<int>(childLayout.top.value());
        } else if (childLayout.bottom.has_value()) {
            childY =
                content.y + content.height - childH - static_cast<int>(childLayout.bottom.value());
        } else {
            childY = content.y;
        }

        Rect childFinalRect = {childX, childY, childW, childH};
        child->arrange(childFinalRect);
    }
}

}  // namespace DxvUI
