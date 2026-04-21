#ifndef DXVUI_FREECONTAINER_H
#define DXVUI_FREECONTAINER_H

#include "Container.h"

namespace DxvUI {

/**
 * @brief A container that arranges its children based on their 'left', 'top',
 *        'width', and 'height' style properties (absolute positioning).
 *
 * This was the default layout behavior of SceneNode before the refactoring.
 */
class AbsoluteContainer : public Container {
   public:
    using Container::Container;  // Inherit constructors

    Size measure(const Size& availableSize) override;
    void arrange(const Rect& finalRect) override;
};

}  // namespace DxvUI

#endif  // DXVUI_FREECONTAINER_H
