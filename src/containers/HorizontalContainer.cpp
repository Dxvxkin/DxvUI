#include "DxvUI/containers/HorizontalContainer.h"

#include <algorithm>
#include <numeric>

#include "DxvUI/layout/LayoutManager.h"

namespace DxvUI {

void HorizontalContainer::setSpacing(float spacing) {
    if (spacing_ != spacing) {
        spacing_ = spacing;
        markLayoutDirty();
    }
}

float HorizontalContainer::getSpacing() const { return spacing_; }

Size HorizontalContainer::onMeasure(const Size& availableSize) {
    const auto& computedLayout = getComputedLayout(getCurrentState());
    const auto& padding = computedLayout.padding;

    const Size contentAvailableSize = LayoutManager::subtractPadding(availableSize, padding);

    float totalWidth = 0.0f;
    float maxHeight = 0.0f;
    bool firstVisibleChild = true;

    for (const auto& child : children) {
        if (!child->isVisible()) continue;

        // Add spacing before the element (but not for the first one)
        if (!firstVisibleChild) {
            totalWidth += spacing_;
        }

        const Size childDesiredSize = child->measure(contentAvailableSize);
        totalWidth += childDesiredSize.width;
        maxHeight = std::max(maxHeight, childDesiredSize.height);

        firstVisibleChild = false;
    }

    return LayoutManager::addPadding({totalWidth, maxHeight}, padding);
}

void HorizontalContainer::onArrange(const Rect& finalRect) {
    const auto& computedLayout = getComputedLayout(getCurrentState());

    const auto& padding = computedLayout.padding;
    const Rect content = LayoutManager::contentRect(*this, finalRect);

    float currentX = static_cast<float>(content.x);

    for (const auto& child : children) {
        if (!child->isVisible()) {
            LayoutManager::arrangeInvisible(*child, content);
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
            .y = content.y,
            .width = static_cast<int>(finalWidth),
            .height = static_cast<int>(finalHeight)};  // Use finalHeight here

        child->arrange(childFinalRect);

        currentX += finalWidth + spacing_;  // Use finalWidth here
    }
}
void HorizontalContainer::draw(IRenderer& renderer) {
    const auto& computedAppearance = getComputedAppearance(getCurrentState());

    // 1. Draw the button's background
    renderer.fillRoundRect(getGlobalBounds(), computedAppearance.borderRadius,
                           computedAppearance.backgroundColor,
                           {computedAppearance.borderColor, computedAppearance.borderThickness});
    Container::draw(renderer);
}

}  // namespace DxvUI
