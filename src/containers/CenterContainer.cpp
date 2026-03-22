#include "DxvUI/containers/CenterContainer.h"
#include "DxvUI/core.h"
#include <limits>

namespace DxvUI {

    // Helper to get the current state of a node
    static WidgetState getCurrentState(const SceneNode* node) {
        if (node->isPressed) return WidgetState::Pressed;
        if (node->isHovered) return WidgetState::Hovered;
        return WidgetState::Normal;
    }

    Size CenterContainer::measure(const Size& availableSize) {
        if (!isLayoutDirty) {
            return desiredSize;
        }

        WidgetState currentState = getCurrentState();
        const auto& computedLayout = getComputedLayout(currentState);
        const auto& padding = computedLayout.padding;

        Size contentAvailableSize = {
            availableSize.width - (padding.left + padding.right),
            availableSize.height - (padding.top + padding.bottom)
        };

        Size childDesiredSize = {0, 0};
        if (!children.empty() && children.front()) {
            childDesiredSize = children.front()->measure(contentAvailableSize);
        }

        desiredSize = {
            childDesiredSize.width + padding.left + padding.right,
            childDesiredSize.height + padding.top + padding.bottom
        };

        if (computedLayout.width > 0) {
            desiredSize.width = computedLayout.width;
        }
        if (computedLayout.height > 0) {
            desiredSize.height = computedLayout.height;
        }

        return desiredSize;
    }

    void CenterContainer::arrange(const Rect& finalRect) {
        WidgetState currentState = getCurrentState();
        auto& computedLayout = layoutCache[currentState];
        computedLayout.computedBounds = finalRect;

        const auto& padding = computedLayout.padding;
        Rect contentRect = {
            finalRect.x + static_cast<int>(padding.left),
            finalRect.y + static_cast<int>(padding.top),
            finalRect.width - static_cast<int>(padding.left + padding.right),
            finalRect.height - static_cast<int>(padding.top + padding.bottom)
        };

        if (!children.empty() && children.front()) {
            auto& child = children.front();
            Size childDesiredSize = child->getDesiredSize();

            int childX = contentRect.x + (contentRect.width - static_cast<int>(childDesiredSize.width)) / 2;
            int childY = contentRect.y + (contentRect.height - static_cast<int>(childDesiredSize.height)) / 2;

            Rect childFinalRect = {
                childX,
                childY,
                static_cast<int>(childDesiredSize.width),
                static_cast<int>(childDesiredSize.height)
            };

            child->arrange(childFinalRect);
        }

        isLayoutDirty = false;
    }

}
