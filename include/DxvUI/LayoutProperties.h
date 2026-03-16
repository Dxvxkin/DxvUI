#ifndef DXVUI_LAYOUTPROPERTIES_H
#define DXVUI_LAYOUTPROPERTIES_H

#include "core.h"
#include <optional>

namespace DxvUI {

    class LayoutProperties {
    public:
        // Sizing
        std::optional<float> width;
        std::optional<float> height;

        // Spacing
        Thickness margin;
        Thickness padding;

        // Positioning
        PositionType positionType = PositionType::Relative;
        std::optional<float> left;
        std::optional<float> top;
        std::optional<float> right;
        std::optional<float> bottom;

        // Alignment (used by parent container)
        Alignment horizontalAlignment = Alignment::Stretch;
        Alignment verticalAlignment = Alignment::Stretch;

        // --- Internal values computed by the layout pass ---
        // These are the final, calculated values used for rendering.
        Rect computedBounds; // Position and size relative to the parent
        Rect computedInnerBounds; // Area inside padding
    };

}

#endif //DXVUI_LAYOUTPROPERTIES_H
