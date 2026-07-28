#ifndef DXVUI_STYLE_H
#define DXVUI_STYLE_H

#include <map>
#include <optional>
#include <string>
#include <utility>

#include "DxvUI/core.h"  // For core types like Thickness, Alignment, CursorType
#include "DxvUI/style/Color.h"

namespace DxvUI {

enum class WidgetState { Normal, Hovered, Pressed, Disabled };

// Helper macro to merge optional fields.
#define MERGE_PROPERTY(prop) \
    if (other.prop) {        \
        prop = other.prop;   \
    }

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

    /**
     * @brief Merges another StyleRule into this one.
     *
     * For each property in 'other', if it has a value, it overwrites the
     * corresponding property in this StyleRule.
     *
     * @param other The StyleRule containing the properties to merge.
     */
    void merge(const StyleRule& other) {
        // Appearance
        MERGE_PROPERTY(backgroundColor);
        MERGE_PROPERTY(textColor);
        MERGE_PROPERTY(borderColor);
        MERGE_PROPERTY(borderThickness);
        MERGE_PROPERTY(borderRadius);
        MERGE_PROPERTY(cursor);

        // Text
        MERGE_PROPERTY(fontSize);
        MERGE_PROPERTY(fontPath);

        // Position
        MERGE_PROPERTY(left);
        MERGE_PROPERTY(top);
        MERGE_PROPERTY(right);
        MERGE_PROPERTY(bottom);

        // Size
        MERGE_PROPERTY(width);
        MERGE_PROPERTY(height);
        MERGE_PROPERTY(minWidth);
        MERGE_PROPERTY(minHeight);
        MERGE_PROPERTY(maxWidth);
        MERGE_PROPERTY(maxHeight);

        // Alignment & Spacing
        MERGE_PROPERTY(padding);
        MERGE_PROPERTY(margin);
        MERGE_PROPERTY(horizontalAlignment);
        MERGE_PROPERTY(verticalAlignment);
    }
};

// The Style class manages a collection of StyleRules for different states.
class Style {
   public:
    /**
     * @brief Overwrites the style rule for a given state.
     * @param rule The complete style rule to set.
     * @param state The widget state to target.
     */
    void set(StyleRule rule, WidgetState state = WidgetState::Normal) {
        stateStyles[state] = std::move(rule);
    }

    /**
     * @brief Merges new style properties into the existing rule for a given state.
     * If no rule exists for the state, a new one is created.
     * @param state The widget state to target.
     * @param updates A StyleRule containing only the properties to change.
     */
    void update(const StyleRule& updates, WidgetState state = WidgetState::Normal) {
        stateStyles[state].merge(updates);
    }

    /**
     * @brief Gets the style rule for a given state.
     * @param state The widget state to query.
     * @return A pointer to the StyleRule, or nullptr if not found.
     */
    const StyleRule* get(WidgetState state = WidgetState::Normal) const {
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
        return backgroundColor == other.backgroundColor && textColor == other.textColor &&
               borderColor == other.borderColor && borderThickness == other.borderThickness &&
               borderRadius == other.borderRadius && cursor == other.cursor &&
               fontSize == other.fontSize && fontPath == other.fontPath;
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

}  // namespace DxvUI

#endif  // DXVUI_STYLE_H