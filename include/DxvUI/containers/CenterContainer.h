#ifndef DXVUI_CENTERCONTAINER_H
#define DXVUI_CENTERCONTAINER_H

#include "Container.h"

namespace DxvUI {

    /**
     * @brief A container that centers its first child within its own bounds.
     */
    class CenterContainer : public Container {
    public:
        using Container::Container; // Inherit constructors

        void updateLayout(const Rect& parentInnerBounds) override;
    };

}

#endif //DXVUI_CENTERCONTAINER_H
