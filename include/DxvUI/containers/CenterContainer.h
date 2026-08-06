#ifndef DXVUI_CENTERCONTAINER_H
#define DXVUI_CENTERCONTAINER_H

#include "Container.h"

namespace DxvUI {

/**
 * @brief A container that centers its first child within its own bounds.
 */
class CenterContainer : public Container {
   public:
    using Container::Container;  // Inherit constructors

   protected:
    Size onMeasure(const Size& availableSize) override;
    void onArrange(const Rect& finalRect) override;
};

}  // namespace DxvUI

#endif  // DXVUI_CENTERCONTAINER_H
