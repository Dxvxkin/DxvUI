#ifndef DXVUI_CENTERCONTAINER_H
#define DXVUI_CENTERCONTAINER_H

#include <memory>
#include <string>

#include "Container.h"

namespace DxvUI {

/**
 * @brief A container that centers its first child within its own bounds.
 */
class CenterContainer : public Container {
   public:
    static std::shared_ptr<CenterContainer> create(std::string id);

    using Container::Container;  // Inherit constructors

   protected:
    Size onMeasure(const Size& availableSize) override;
    void onArrange(const Rect& finalRect) override;
};

}  // namespace DxvUI

#endif  // DXVUI_CENTERCONTAINER_H
