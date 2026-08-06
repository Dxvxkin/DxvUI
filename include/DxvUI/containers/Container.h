#ifndef DXVUI_CONTAINER_H
#define DXVUI_CONTAINER_H

#include "DxvUI/SceneNode.h"

namespace DxvUI {

/**
 * @class Container
 * @brief Semantic base class for all container widgets.
 *
 * Kept intentionally empty: it exists so user code and the framework can detect
 * "this node arranges children" via dynamic_cast. Containers override the
 * protected SceneNode::onMeasure()/onArrange() hooks for their layout logic.
 */
class Container : public SceneNode {
   public:
    using SceneNode::SceneNode;  // Inherit constructors
};

}  // namespace DxvUI

#endif  // DXVUI_CONTAINER_H
