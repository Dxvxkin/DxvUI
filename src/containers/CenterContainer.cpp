#include "DxvUI/containers/CenterContainer.h"
#include "DxvUI/LayoutProperties.h"

namespace DxvUI {

    void CenterContainer::updateLayout(const Rect& parentContentRect) {
        // Do not call the base method, as we are fully overriding the layout logic for children.

        // --- Phase 1: Calculate our own bounds (logic copied from SceneNode::updateLayout) ---
        if (!isLayoutDirty) return;

        auto& layout = getLayout();

        int finalWidth = layout.width.value_or(parentContentRect.width);
        int finalHeight = layout.height.value_or(parentContentRect.height);
        int finalX = parentContentRect.x + layout.left.value_or(0);
        int finalY = parentContentRect.y + layout.top.value_or(0);

        layout.computedBounds = {finalX, finalY, finalWidth, finalHeight};
        layout.computedInnerBounds = {
            finalX + (int)layout.padding.left,
            finalY + (int)layout.padding.top,
            finalWidth - (int)(layout.padding.left + layout.padding.right),
            finalHeight - (int)(layout.padding.top + layout.padding.bottom)
        };

        // --- Phase 2: Apply this container's specific logic ---
        if (!children.empty()) {
            auto& child = children.front();
            if (child) {
                auto& childLayout = child->getLayout();

                float childWidth = childLayout.width.value_or(0);
                float childHeight = childLayout.height.value_or(0);

                // Center the child within our inner bounds
                childLayout.left = (layout.computedInnerBounds.width - childWidth) / 2;
                childLayout.top = (layout.computedInnerBounds.height - childHeight) / 2;
            }
        }

        // --- Phase 3: Recursively update all children ---
        // Now they will use the `left` and `top` we just set.
        for (auto& child : children) {
            child->updateLayout(layout.computedInnerBounds);
        }

        isLayoutDirty = false;
    }

}
