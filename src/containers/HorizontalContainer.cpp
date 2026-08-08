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
    const auto& computedLayout = getComputedLayout();
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

        const Size childOuterSize = LayoutManager::measureChild(*child, contentAvailableSize);
        totalWidth += childOuterSize.width;
        maxHeight = std::max(maxHeight, childOuterSize.height);

        firstVisibleChild = false;
    }

    return LayoutManager::addPadding({totalWidth, maxHeight}, padding);
}

void HorizontalContainer::onArrange(const Rect& finalRect) {
    const auto& computedLayout = getComputedLayout();

    const auto& padding = computedLayout.padding;
    const Rect content = LayoutManager::contentRect(*this, finalRect);

    float currentX = static_cast<float>(content.x);

    for (const auto& child : children) {
        if (!child->isVisible()) {
            LayoutManager::arrangeInvisible(*child, content);
            continue;
        }

        const auto& childComputedLayout = child->getComputedLayout(child->getCurrentState());
        const auto& margin = childComputedLayout.margin;
        Size childDesiredSize = child->getDesiredSize();

        // Use the width from the style if it's specified, otherwise use the measured width.
        float finalWidth =
            (childComputedLayout.width > 0) ? childComputedLayout.width : childDesiredSize.width;
        float finalHeight =
            (childComputedLayout.height > 0) ? childComputedLayout.height : childDesiredSize.height;

        // The main axis is managed by the flow (currentX + spacing); only the
        // cross axis (vertical) is aligned by the child's verticalAlignment.
        const Rect childSlot = {static_cast<int>(currentX + margin.left), content.y,
                                static_cast<int>(finalWidth), content.height};
        const Rect childFinalRect = LayoutManager::alignChild(
            *child, {finalWidth, finalHeight}, childSlot, {.horizontal = false, .vertical = true});

        child->arrange(childFinalRect);

        currentX += margin.left + finalWidth + margin.right + spacing_;  // Use finalWidth here
    }
}

}  // namespace DxvUI
