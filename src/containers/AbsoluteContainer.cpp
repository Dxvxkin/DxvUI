#include "DxvUI/containers/AbsoluteContainer.h"

#include <algorithm>

namespace DxvUI {

Size AbsoluteContainer::measure(const Size& availableSize) {
    if (!isLayoutDirty) {
        return desiredSize;
    }

    const auto& computedLayout = getComputedLayout(getCurrentState());
    const auto& padding = computedLayout.padding;

    float requiredWidth = 0.0f;
    float requiredHeight = 0.0f;

    for (const auto& child : children) {
        if (!child->isVisible()) continue;

        Size childDesiredSize = child->measure(availableSize);
        const auto& childLayout = child->getComputedLayout(child->getCurrentState());

        float childLeft = childLayout.left;
        float childTop = childLayout.top;
        float childWidth = childLayout.width > 0 ? childLayout.width : childDesiredSize.width;
        float childHeight = childLayout.height > 0 ? childLayout.height : childDesiredSize.height;

        requiredWidth = std::max(requiredWidth, childLeft + childWidth);
        requiredHeight = std::max(requiredHeight, childTop + childHeight);
    }

    desiredSize = {requiredWidth + padding.left + padding.right,
                   requiredHeight + padding.top + padding.bottom};

    if (computedLayout.width > 0) desiredSize.width = computedLayout.width;
    if (computedLayout.height > 0) desiredSize.height = computedLayout.height;

    return desiredSize;
}

void AbsoluteContainer::arrange(const Rect& finalRect) {
    auto& computedLayout = layoutCache[getCurrentState()];
    computedLayout.computedBounds = finalRect;

    const auto& padding = computedLayout.padding;
    Rect contentRect = {finalRect.x + static_cast<int>(padding.left),
                        finalRect.y + static_cast<int>(padding.top),
                        finalRect.width - static_cast<int>(padding.left + padding.right),
                        finalRect.height - static_cast<int>(padding.top + padding.bottom)};

    for (const auto& child : children) {
        if (!child->isVisible()) {
            // For an invisible child, we ask it to arrange itself into a zero-sized rectangle.
            // Its own arrange() method will handle updating its internal state correctly.
            child->arrange({contentRect.x, contentRect.y, 0, 0});
            continue;
        }

        const auto& childLayout = child->getComputedLayout(child->getCurrentState());
        Size childDesiredSize = child->getDesiredSize();

        int childX = contentRect.x + static_cast<int>(childLayout.left);
        int childY = contentRect.y + static_cast<int>(childLayout.top);
        int childW =
            static_cast<int>(childLayout.width > 0 ? childLayout.width : childDesiredSize.width);
        int childH =
            static_cast<int>(childLayout.height > 0 ? childLayout.height : childDesiredSize.height);

        Rect childFinalRect = {childX, childY, childW, childH};
        child->arrange(childFinalRect);
    }

    isLayoutDirty = false;
}

}  // namespace DxvUI
