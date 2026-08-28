#include "DxvUI/containers/VerticalContainer.h"

#include <algorithm>

#include "DxvUI/layout/LayoutManager.h"

namespace DxvUI {

void VerticalContainer::setSpacing(float spacing) { updateStyle(StyleRule{.gap = spacing}); }

float VerticalContainer::getSpacing() const { return getComputedLayout().gap; }

Size VerticalContainer::onMeasure(const Size& availableSize) {
    const auto& computedLayout = getComputedLayout();
    const auto& padding = computedLayout.padding;
    const float gap = computedLayout.gap;

    const Size contentAvailableSize = LayoutManager::subtractPadding(availableSize, padding);

    float totalHeight = 0.0f;
    float maxWidth = 0.0f;
    bool firstVisibleChild = true;

    for (const auto& child : getChildren()) {
        if (!child->isVisible()) continue;

        // Add spacing before the element (but not for the first one)
        if (!firstVisibleChild) {
            totalHeight += gap;
        }

        const Size childOuterSize = LayoutManager::measureChild(*child, contentAvailableSize);
        totalHeight += childOuterSize.height;
        maxWidth = std::max(maxWidth, childOuterSize.width);

        firstVisibleChild = false;
    }

    return LayoutManager::addPadding({maxWidth, totalHeight}, padding);
}

void VerticalContainer::onArrange(const Rect& finalRect) {
    const auto& computedLayout = getComputedLayout();
    const float gap = computedLayout.gap;
    const Rect content = LayoutManager::contentRect(*this, finalRect);

    float currentY = static_cast<float>(content.y);

    for (const auto& child : getChildren()) {
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

        // The main axis is managed by the flow (currentY + spacing); only the
        // cross axis (horizontal) is aligned by the child's horizontalAlignment.
        const Rect childSlot = {content.x, static_cast<int>(currentY + margin.top),
                                content.width, static_cast<int>(finalHeight)};
        const Rect childFinalRect = LayoutManager::alignChild(
            *child, {finalWidth, finalHeight}, childSlot, {.horizontal = true, .vertical = false});

        child->arrange(childFinalRect);

        currentY += margin.top + finalHeight + margin.bottom + gap;
    }
}

}  // namespace DxvUI
