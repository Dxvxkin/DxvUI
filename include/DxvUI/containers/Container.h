#ifndef DXVUI_CONTAINER_H
#define DXVUI_CONTAINER_H

#include "DxvUI/SceneNode.h"

namespace DxvUI {

class Container : public SceneNode {
   public:
    using SceneNode::SceneNode;  // Inherit constructors

    Size measure(const Size& availableSize) override = 0;

    void arrange(const Rect& finalRect) override = 0;
};

}  // namespace DxvUI

#endif  // DXVUI_CONTAINER_H
