#include "DxvUI/containers/CenterContainer.h"

namespace DxvUI {

    void CenterContainer::updateLayout() {
        // First, let children update their own internal layouts if needed
        SceneNode::updateLayout();

        // Now, apply this container's layout logic
        if (!children.empty()) {
            auto& child = children.front();
            if (child) {
                // Center the child relative to this container
                child->relX = (this->width - child->width) / 2;
                child->relY = (this->height - child->height) / 2;
            }
        }
    }

}
