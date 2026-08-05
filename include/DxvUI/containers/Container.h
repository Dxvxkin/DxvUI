#ifndef DXVUI_CONTAINER_H
#define DXVUI_CONTAINER_H

#include "DxvUI/SceneNode.h"

namespace DxvUI {

class Container : public SceneNode {
   public:
    using SceneNode::SceneNode;  // Inherit constructors

    void arrange(const Rect& finalRect) override = 0;
};

}  // namespace DxvUI

#endif  // DXVUI_CONTAINER_H
