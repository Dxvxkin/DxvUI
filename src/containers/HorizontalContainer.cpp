#include "DxvUI/containers/HorizontalContainer.h"

#include <algorithm>
#include <numeric>

namespace DxvUI {

void HorizontalContainer::setSpacing(float spacing) {
    if (spacing_ != spacing) {
        spacing_ = spacing;
        markLayoutDirty();
    }
}

float HorizontalContainer::getSpacing() const { return spacing_; }

Size HorizontalContainer::measure(const Size& availableSize) {
    if (!isLayoutDirty) {
        return desiredSize;
    }

    float totalWidth = 0.0f;
    float maxHeight = 0.0f;
    bool firstVisibleChild = true;

    for (const auto& child : children) {
        if (!child->isVisible()) continue;

        // Add spacing before the element (but not for the first one)
        if (!firstVisibleChild) {
            totalWidth += spacing_;
        }

        const Size childDesiredSize = child->measure(availableSize);
        totalWidth += childDesiredSize.width;
        maxHeight = std::max(maxHeight, childDesiredSize.height);

        firstVisibleChild = false;
    }

    const auto& computedLayout = getComputedLayout(getCurrentState());
    const auto& padding = computedLayout.padding;

    desiredSize = {totalWidth + padding.left + padding.right,
                   maxHeight + padding.top + padding.bottom};

    // Explicit size from style overrides calculated size
    if (computedLayout.width > 0) desiredSize.width = computedLayout.width;
    if (computedLayout.height > 0) desiredSize.height = computedLayout.height;

    return desiredSize;
}

void HorizontalContainer::arrange(const Rect& finalRect) {
    style.setComputedBounds(getCurrentState(), finalRect);

    const auto& computedLayout = getComputedLayout(getCurrentState());

    const auto& padding = computedLayout.padding;
    const Rect contentRect = {finalRect.x + static_cast<int>(padding.left),
                              finalRect.y + static_cast<int>(padding.top),
                              finalRect.width - static_cast<int>(padding.left + padding.right),
                              finalRect.height - static_cast<int>(padding.top + padding.bottom)};

    float currentX = static_cast<float>(contentRect.x);

    for (const auto& child : children) {
        if (!child->isVisible()) {
            child->arrange({static_cast<int>(currentX), contentRect.y, 0, 0});
            continue;
        }

        const auto& childComputedLayout = child->getComputedLayout(child->getCurrentState());
        Size childDesiredSize = child->getDesiredSize();

        // Use the width from the style if it's specified, otherwise use the measured width.
        float finalWidth =
            (childComputedLayout.width > 0) ? childComputedLayout.width : childDesiredSize.width;
        float finalHeight =
            (childComputedLayout.height > 0) ? childComputedLayout.height : childDesiredSize.height;

        const Rect childFinalRect = {
            .x = static_cast<int>(currentX),
            .y = contentRect.y,
            .width = static_cast<int>(finalWidth),
            .height = static_cast<int>(finalHeight)};  // Use finalHeight here

        child->arrange(childFinalRect);

        currentX += finalWidth + spacing_;  // Use finalWidth here
    }

    isLayoutDirty = false;
}
void HorizontalContainer::draw(IRenderer& renderer) {
    const auto& computedAppearance = getComputedAppearance(getCurrentState());
    const auto& computedLayout = getComputedLayout(getCurrentState());

    // 1. Draw the button's background
    renderer.fillRoundRect(computedLayout.computedBounds, computedAppearance.borderRadius,
                           computedAppearance.backgroundColor,
                           {computedAppearance.borderColor, computedAppearance.borderThickness});
    Container::draw(renderer);
}

}  // namespace DxvUI