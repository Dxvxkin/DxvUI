#include "DxvUI/containers/CenterContainer.h"

#include "DxvUI/core.h"
#include "DxvUI/layout/LayoutManager.h"

namespace DxvUI {

Size CenterContainer::onMeasure(const Size& availableSize) {
    WidgetState currentState = getCurrentState();
    const auto& computedLayout = getComputedLayout(currentState);
    const auto& padding = computedLayout.padding;

    Size contentAvailableSize = LayoutManager::subtractPadding(availableSize, padding);

    Size childOuterSize = {0, 0};
    if (!children.empty() && children.front()) {
        auto& child = children.front();
        const auto& margin = child->getComputedLayout(child->getCurrentState()).margin;

        // Clean children keep the result of their last measure pass; reuse the
        // cached outer size instead of re-entering measureChild()/measureNode().
        if (!child->isLayoutDirty() &&
            child->getLastMeasureConstraints() ==
                LayoutManager::subtractPadding(contentAvailableSize, margin)) {
            childOuterSize = LayoutManager::addPadding(child->getDesiredSize(), margin);
        } else {
            childOuterSize = LayoutManager::measureChild(*child, contentAvailableSize);
        }
    }

    return LayoutManager::addPadding(childOuterSize, padding);
}

void CenterContainer::onArrange(const Rect& finalRect) {
    const auto& computedLayout = getComputedLayout(getCurrentState());

    const auto& padding = computedLayout.padding;
    Rect content = LayoutManager::contentRect(*this, finalRect);

    if (!children.empty() && children.front()) {
        auto& child = children.front();

        // If neither this container nor the child changed, the centered slot
        // computed in the previous arrange pass is still valid.
        if (layoutData.lastArrangeRect == finalRect && !layoutData.isDirty &&
            !child->isLayoutDirty()) {
            return;
        }

        const auto& margin = child->getComputedLayout(child->getCurrentState()).margin;
        Size childDesiredSize = child->getDesiredSize();

        // Center the child's margin-box in the content area, then offset the
        // child itself by its margin.
        int marginBoxW = static_cast<int>(childDesiredSize.width + margin.left + margin.right);
        int marginBoxH = static_cast<int>(childDesiredSize.height + margin.top + margin.bottom);
        int childX = content.x + (content.width - marginBoxW) / 2 + static_cast<int>(margin.left);
        int childY = content.y + (content.height - marginBoxH) / 2 + static_cast<int>(margin.top);

        Rect childFinalRect = {childX, childY, static_cast<int>(childDesiredSize.width),
                               static_cast<int>(childDesiredSize.height)};

        child->arrange(childFinalRect);
    }
}

}  // namespace DxvUI
