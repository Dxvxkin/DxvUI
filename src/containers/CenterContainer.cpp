#include "DxvUI/containers/CenterContainer.h"

namespace DxvUI {

    void CenterContainer::draw(IRenderer& renderer) {
        if (!children.empty()) {
            auto& child = children.front();
            if (child) {
                // Center the child relative to this container
                child->relX = (this->width - child->width) / 2;
                child->relY = (this->height - child->height) / 2;
            }
        }

        // After updating layout, proceed with the standard draw call,
        // which will draw the children at their new positions.
        SceneNode::draw(renderer);
    }

}
