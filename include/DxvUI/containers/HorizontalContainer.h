#pragma once

#include "DxvUI/containers/Container.h"

namespace DxvUI {

/**
 * @brief A container that arranges its children in a horizontal line, one after another.
 */
class HorizontalContainer : public Container {
   public:
    using Container::Container;  // Inherit constructors

    /**
     * @brief Sets the spacing (in pixels) between each child element.
     * @param spacing The space between elements.
     */
    void setSpacing(float spacing);

    /**
     * @brief Gets the current spacing between elements.
     * @return The space between elements.
     */
    float getSpacing() const;

   protected:
    Size onMeasure(const Size& availableSize) override;
    void onArrange(const Rect& finalRect) override;

   private:
    float spacing_ = 0.0f;
};

}  // namespace DxvUI
