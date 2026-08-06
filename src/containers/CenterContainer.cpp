#include "DxvUI/containers/CenterContainer.h"

#include "DxvUI/core.h"
#include "DxvUI/layout/LayoutManager.h"

namespace DxvUI {

Size CenterContainer::onMeasure(const Size& availableSize) {
    WidgetState currentState = getCurrentState();
    const auto& computedLayout = getComputedLayout(currentState);
    const auto& padding = computedLayout.padding;

    Size contentAvailableSize = LayoutManager::subtractPadding(availableSize, padding);

    Size childDesiredSize = {0, 0};
    if (!children.empty() && children.front()) {
        childDesiredSize = children.front()->measure(contentAvailableSize);
    }

    return LayoutManager::addPadding(childDesiredSize, padding);
}

void CenterContainer::onArrange(const Rect& finalRect) {
    const auto& computedLayout = getComputedLayout(getCurrentState());

    const auto& padding = computedLayout.padding;
    Rect content = LayoutManager::contentRect(*this, finalRect);

    if (!children.empty() && children.front()) {
        auto& child = children.front();
        Size childDesiredSize = child->getDesiredSize();

        int childX = content.x + (content.width - static_cast<int>(childDesiredSize.width)) / 2;
        int childY = content.y + (content.height - static_cast<int>(childDesiredSize.height)) / 2;

        Rect childFinalRect = {childX, childY, static_cast<int>(childDesiredSize.width),
                               static_cast<int>(childDesiredSize.height)};

        child->arrange(childFinalRect);
    }
}

}  // namespace DxvUI
