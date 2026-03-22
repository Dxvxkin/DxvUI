#include "DxvUI/containers/CenterContainer.h"
#include "DxvUI/LayoutProperties.h"
#include "DxvUI/core.h"


namespace DxvUI {

    Size CenterContainer::measure(const Size& availableSize) {
        if (!isLayoutDirty) {
            return desiredSize;
        }

        // Start with our own padding
        const auto& padding = layoutProperties.padding;
        Size contentAvailableSize = {
            availableSize.width - (padding.left + padding.right),
            availableSize.height - (padding.top + padding.bottom)
        };

        Size childDesiredSize = {0, 0};
        if (!children.empty() && children.front()) {
            // Measure the first child. CenterContainer typically gives its child
            // as much space as possible, or a constrained size if the container itself
            // has explicit dimensions. For now, let's give it the contentAvailableSize.
            childDesiredSize = children.front()->measure(contentAvailableSize);
        }

        // Our desired size is the child's desired size plus our padding.
        desiredSize = {
            childDesiredSize.width + padding.left + padding.right,
            childDesiredSize.height + padding.top + padding.bottom
        };

        // If a specific size is set on the container, it overrides the measured size.
        if (layoutProperties.width.has_value()) {
            desiredSize.width = layoutProperties.width.value();
        }
        if (layoutProperties.height.has_value()) {
            desiredSize.height = layoutProperties.height.value();
        }

        return desiredSize;
    }

    void CenterContainer::arrange(const Rect& finalRect) {
        // Accept the final rectangle given by the parent.
        layoutProperties.computedBounds = finalRect;

        // Calculate the content area for children, considering our padding.
        const auto& padding = layoutProperties.padding;
        Rect contentRect = {
            finalRect.x + static_cast<int>(padding.left),
            finalRect.y + static_cast<int>(padding.top),
            finalRect.width - static_cast<int>(padding.left + padding.right),
            finalRect.height - static_cast<int>(padding.top + padding.bottom)
        };

        if (!children.empty() && children.front()) {
            auto& child = children.front();
            Size childDesiredSize = child->getDesiredSize(); // Use the new getter

            // Center the child within our contentRect
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
        markLayoutDirty();
    }

}
