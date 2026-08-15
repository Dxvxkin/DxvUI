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
     *
     * Convenience wrapper over the style-driven `gap` property: the spacing is
     * stored as a StyleRule on the Normal state, so a theme can also set it per
     * widget type and it participates in the normal style/layout invalidation.
     * @param spacing The space between elements.
     */
    void setSpacing(float spacing);

    /**
     * @brief Gets the current spacing between elements.
     *
     * Reads the resolved layout style, so the container must have been resolved
     * (the regular layout cycle) before this returns a meaningful value.
     * @return The space between elements.
     */
    float getSpacing() const;

   protected:
    Size onMeasure(const Size& availableSize) override;
    void onArrange(const Rect& finalRect) override;
};

}  // namespace DxvUI
