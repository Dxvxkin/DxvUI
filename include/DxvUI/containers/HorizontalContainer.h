#pragma once

#include "DxvUI/containers/Container.h"

namespace DxvUI {

/**
 * @brief A container that arranges its children in a horizontal line, one after another.
 */
class HorizontalContainer : public Container {
   public:
    using Container::Container;  // Inherit constructors

    Size measure(const Size& availableSize) override;
    void arrange(const Rect& finalRect) override;

    void draw(IRenderer& renderer) override;

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

   private:
    float spacing_ = 0.0f;
};

}  // namespace DxvUI
