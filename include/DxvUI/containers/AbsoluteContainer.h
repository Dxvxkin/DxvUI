#ifndef DXVUI_ABSOLUTECONTAINER_H
#define DXVUI_ABSOLUTECONTAINER_H

#include <memory>
#include <string>

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
    static std::shared_ptr<AbsoluteContainer> create(std::string id);

    using Container::Container;  // Inherit constructors

   protected:
    Size onMeasure(const Size& availableSize) override;
    void onArrange(const Rect& finalRect) override;
};

}  // namespace DxvUI

#endif  // DXVUI_ABSOLUTECONTAINER_H
