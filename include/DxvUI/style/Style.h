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

// Fully-resolved appearance properties after applying the style cascade.
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

// Fully-resolved layout properties after applying the style cascade.
struct ComputedLayoutStyle {
    float left, top, width, height;
    Thickness padding;
    Thickness margin;
    Alignment horizontalAlignment;
    Alignment verticalAlignment;
    Rect computedBounds;
};

/**
 * @class Style
 * @brief Owns the local style rules of a node plus its computed style cache.
 *
 * The node's author-provided rules are stored per WidgetState. The StyleManager
 * resolves them (together with theme defaults and inherited text properties)
 * into the computed cache, which is later consumed during layout and drawing.
 * The cache for a state is populated lazily: consumers must call
 * StyleManager::resolveDirtyStyles() before reading computed styles.
 */
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

    // --- Computed Style Cache ---

    /**
     * @brief Checks whether the computed cache must be re-resolved.
     * @return True if the style was modified since the last resolution.
     */
    [[nodiscard]] bool isDirty() const noexcept { return dirty; }

    /**
     * @brief Marks the style as requiring re-resolution.
     *
     * Because descendants inherit text properties, StyleManager re-resolves the
     * whole subtree below a dirty node in the next resolve pass.
     */
    void markDirty() noexcept { dirty = true; }

    /**
     * @brief Marks the computed cache as up-to-date.
     */
    void markClean() noexcept { dirty = false; }

    /**
     * @brief Stores the resolved appearance for a given state.
     * @param state The widget state.
     * @param computed The resolved appearance style.
     */
    void setComputedAppearance(WidgetState state, const ComputedAppearanceStyle& computed) {
        appearanceCache[state] = computed;
    }

    /**
     * @brief Stores the resolved layout properties for a given state.
     * @param state The widget state.
     * @param computed The resolved layout style.
     */
    void setComputedLayout(WidgetState state, const ComputedLayoutStyle& computed) {
        layoutCache[state] = computed;
    }

    /**
     * @brief Writes the final bounds produced by the arrange (layout) pass.
     *
     * Bounds are layout output, not a style change, so this does not mark the
     * style dirty.
     * @param state The widget state the bounds were computed for.
     * @param bounds The final rectangle of the node.
     */
    void setComputedBounds(WidgetState state, const Rect& bounds) {
        layoutCache[state].computedBounds = bounds;
    }

    /**
     * @brief Gets the resolved appearance for a given state.
     * @param state The widget state.
     * @return A const pointer to the computed appearance, or nullptr if the
     * cache has not been populated for this state.
     */
    [[nodiscard]] const ComputedAppearanceStyle* getComputedAppearance(
        WidgetState state) const noexcept {
        if (auto it = appearanceCache.find(state); it != appearanceCache.end()) {
            return &it->second;
        }
        return nullptr;
    }

    /**
     * @brief Gets the resolved layout properties for a given state.
     * @param state The widget state.
     * @return A const pointer to the computed layout, or nullptr if the cache
     * has not been populated for this state.
     */
    [[nodiscard]] const ComputedLayoutStyle* getComputedLayout(WidgetState state) const noexcept {
        if (auto it = layoutCache.find(state); it != layoutCache.end()) {
            return &it->second;
        }
        return nullptr;
    }

   private:
    std::map<WidgetState, StyleRule> stateStyles;
    std::map<WidgetState, ComputedAppearanceStyle> appearanceCache;
    std::map<WidgetState, ComputedLayoutStyle> layoutCache;
    bool dirty = true;
};

}  // namespace DxvUI

#endif  // DXVUI_STYLE_H
