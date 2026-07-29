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

/**
 * @brief Converts a WidgetState enum to its string representation.
 * @param state The widget state.
 * @return A string_view representing the widget state.
 * @exceptionGuarantee No-throw guarantee.
 */
[[nodiscard]] constexpr std::string_view state_to_string(WidgetState state) noexcept {
    switch (state) {
        case WidgetState::Normal:
            return "Normal";
        case WidgetState::Hovered:
            return "Hovered";
        case WidgetState::Pressed:
            return "Pressed";
        case WidgetState::Disabled:
            return "Disabled";
    }
    return "Unknown";
}

namespace detail {
/**
 * @brief Merges an optional property from a source to a destination.
 * If the source optional contains a value, it overwrites the destination.
 * @tparam T The type of the value contained in the optional.
 * @param dest The destination optional.
 * @param src The source optional.
 * @exceptionGuarantee No-throw guarantee (if T's copy assignment is no-throw).
 */
template <typename T>
constexpr void merge_property(std::optional<T>& dest, const std::optional<T>& src) noexcept(
    std::is_nothrow_copy_assignable_v<T>) {
    if (src) {
        dest = src;
    }
}
}  // namespace detail

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
     * @exceptionGuarantee Basic exception guarantee. std::string assignment can throw.
     */
    void merge(const StyleRule& other) {
        // Appearance
        detail::merge_property(backgroundColor, other.backgroundColor);
        detail::merge_property(textColor, other.textColor);
        detail::merge_property(borderColor, other.borderColor);
        detail::merge_property(borderThickness, other.borderThickness);
        detail::merge_property(borderRadius, other.borderRadius);
        detail::merge_property(cursor, other.cursor);

        // Text
        detail::merge_property(fontSize, other.fontSize);
        detail::merge_property(fontPath, other.fontPath);

        // Position
        detail::merge_property(left, other.left);
        detail::merge_property(top, other.top);
        detail::merge_property(right, other.right);
        detail::merge_property(bottom, other.bottom);

        // Size
        detail::merge_property(width, other.width);
        detail::merge_property(height, other.height);
        detail::merge_property(minWidth, other.minWidth);
        detail::merge_property(minHeight, other.minHeight);
        detail::merge_property(maxWidth, other.maxWidth);
        detail::merge_property(maxHeight, other.maxHeight);

        // Alignment & Spacing
        detail::merge_property(padding, other.padding);
        detail::merge_property(margin, other.margin);
        detail::merge_property(horizontalAlignment, other.horizontalAlignment);
        detail::merge_property(verticalAlignment, other.verticalAlignment);
    }
};

// The Style class manages a collection of StyleRules for different states.
class Style {
   public:
    /**
     * @brief Sets or overwrites the style rule for a given state.
     * @param rule The style rule to set.
     * @param state The widget state to target.
     * @exceptionGuarantee Strong exception guarantee.
     */
    void set(const StyleRule& rule, WidgetState state = WidgetState::Normal) {
        stateStyles[state] = rule;
    }

    /**
     * @brief Sets or overwrites the style rule for a given state (move version).
     * @param rule The style rule to move.
     * @param state The widget state to target.
     * @exceptionGuarantee Strong exception guarantee.
     */
    void set(StyleRule&& rule, WidgetState state = WidgetState::Normal) {
        stateStyles[state] = std::move(rule);
    }

    /**
     * @brief Merges new style properties into the existing rule for a given state.
     * If no rule exists for the state, a new one is created.
     * @param updates A StyleRule containing only the properties to change.
     * @param state The widget state to target.
     * @exceptionGuarantee Strong exception guarantee.
     */
    void update(const StyleRule& updates, WidgetState state = WidgetState::Normal) {
        auto it = stateStyles.find(state);
        if (it != stateStyles.end()) {
            it->second.merge(updates);
        } else {
            stateStyles.emplace(state, updates);
        }
    }

    /**
     * @brief Gets the style rule for a given state.
     * @param state The widget state to query.
     * @return A const pointer to the StyleRule, or nullptr if not found.
     * @exceptionGuarantee No-throw guarantee.
     */
    [[nodiscard]] const StyleRule* get(WidgetState state = WidgetState::Normal) const noexcept {
        if (auto it = stateStyles.find(state); it != stateStyles.end()) {
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