#ifndef DXVUI_STYLE_H
#define DXVUI_STYLE_H

#include "DxvUI/style/Color.h"
#include "DxvUI/core.h" // For core types like Thickness, Alignment, CursorType
#include <optional>
#include <string>
#include <map>

namespace DxvUI {

    enum class WidgetState {
        Normal,
        Hovered,
        Pressed,
        Disabled
    };

    // A single rule containing all possible style and layout properties.
    // std::optional is used to signify "not set".
    struct StyleRule {
        // Appearance Properties
        std::optional<Color> backgroundColor;
        std::optional<Color> textColor;
        std::optional<Color> borderColor;
        std::optional<int> borderThickness;
        std::optional<int> borderRadius;
        std::optional<CursorType> cursor;

        // Text Properties
        std::optional<int> fontSize;
        std::optional<std::string> fontPath;

        // Layout Properties (Absolute Positioning)
        std::optional<float> left, top, right, bottom;

        // Layout Properties (Sizing)
        std::optional<float> width, height;
        std::optional<float> minWidth, minHeight;
        std::optional<float> maxWidth, maxHeight;

        // Layout Properties (Alignment & Spacing)
        std::optional<Thickness> padding;
        std::optional<Thickness> margin;
        std::optional<Alignment> horizontalAlignment;
        std::optional<Alignment> verticalAlignment;
    };

    // The Style class manages a collection of StyleRules for different states.
    class Style {
    public:
        void set(WidgetState state, StyleRule rule) {
            // For now, we just overwrite. A more advanced implementation could merge.
            stateStyles[state] = rule;
        }

        const StyleRule* get(WidgetState state) const {
            auto it = stateStyles.find(state);
            if (it != stateStyles.end()) {
                return &it->second;
            }
            return nullptr;
        }

    private:
        std::map<WidgetState, StyleRule> stateStyles;
    };

    struct ComputedAppearanceStyle {
        Color backgroundColor;
        Color textColor;
        Color borderColor;
        int borderThickness;
        int borderRadius;
        CursorType cursor;
        int fontSize;
        std::string fontPath;

        bool operator==(const ComputedAppearanceStyle& other) const {
            return backgroundColor == other.backgroundColor &&
                   textColor == other.textColor &&
                   borderColor == other.borderColor &&
                   borderThickness == other.borderThickness &&
                   borderRadius == other.borderRadius &&
                   cursor == other.cursor &&
                   fontSize == other.fontSize &&
                   fontPath == other.fontPath;
        }
    };

    struct ComputedLayoutStyle {
        float left, top, width, height;
        Thickness padding;
        Thickness margin;
        Alignment horizontalAlignment;
        Alignment verticalAlignment;
        Rect computedBounds;
    };

}

#endif // DXVUI_STYLE_H
