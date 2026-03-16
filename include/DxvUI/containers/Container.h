#ifndef DXVUI_CONTAINER_H
#define DXVUI_CONTAINER_H

#include "DxvUI/SceneNode.h"

namespace DxvUI {

    /**
     * @brief Base class for all layout containers.
     * A container is a SceneNode that arranges its children according to specific layout rules.
     */
    class Container : public SceneNode {
    public:
        using SceneNode::SceneNode; // Inherit constructors

        // In the future, we might add common container properties here.
    };

}

#endif //DXVUI_CONTAINER_H
