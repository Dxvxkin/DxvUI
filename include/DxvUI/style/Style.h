#ifndef DXVUI_STYLE_H
#define DXVUI_STYLE_H

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <tuple>
#include <utility>

#include "DxvUI/core.h"  // For core types like Thickness, Alignment, CursorType
#include "DxvUI/style/Color.h"

namespace DxvUI {

enum class WidgetState { Normal, Hovered, Pressed, Disabled };

// The number of states in WidgetState; keeps the per-node storage a fixed-size
// array instead of a map.
inline constexpr size_t kWidgetStateCount = 4;

/**
 * @brief Converts a WidgetState to a contiguous array index.
 * @param state The widget state.
 * @return An index in the range [0, kWidgetStateCount).
 * @exceptionGuarantee No-throw guarantee.
 */
[[nodiscard]] constexpr size_t state_index(WidgetState state) noexcept {
    return static_cast<size_t>(state);
}

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
    // When true, the node's content and children are clipped to its own bounds.
    std::optional<bool> clipContent;

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
    // How the parent aligns this node within the space it gives it
    // (Start/Center/End; Stretch is reserved and not implemented).
    std::optional<Alignment> horizontalAlignment;
    std::optional<Alignment> verticalAlignment;

    /**
     * @brief Merges another StyleRule into this one.
     *
     * For each property in 'other', if it has a value, it overwrites the
     * corresponding property in this StyleRule. The property list is defined
     * once (see detail::appearanceProps / detail::layoutProps) and shared with
     * the style resolution, so a new property cannot be forgotten here.
     *
     * @param other The StyleRule containing the properties to merge.
     * @exceptionGuarantee Basic exception guarantee. std::string assignment can throw.
     */
    void merge(const StyleRule& other);

    /**
     * @brief Checks whether two rules hold identical properties.
     *
     * Uses the same property descriptors as merge()/resolution, so equality
     * cannot drift from the property list.
     */
    bool operator==(const StyleRule& other) const;
};

// Fully-resolved appearance properties after applying the style cascade.
struct ComputedAppearanceStyle {
    Color backgroundColor;
    Color textColor;
    Color borderColor;
    int borderThickness;
    int borderRadius;
    CursorType cursor;
    bool clipContent = false;
    int fontSize;
    std::string fontPath;

    bool operator==(const ComputedAppearanceStyle& other) const {
        return backgroundColor == other.backgroundColor && textColor == other.textColor &&
               borderColor == other.borderColor && borderThickness == other.borderThickness &&
               borderRadius == other.borderRadius && cursor == other.cursor &&
               clipContent == other.clipContent && fontSize == other.fontSize &&
               fontPath == other.fontPath;
    }
};

// Fully-resolved layout properties after applying the style cascade.
// left/top/right/bottom are optional because "not set" matters for absolute
// positioning (e.g. 'right' only anchors to the right edge). width/height are
// always present: 0 means "derive from measure". min/max sizes are optional
// constraints applied on top of the measured size.
struct ComputedLayoutStyle {
    std::optional<float> left, top, right, bottom;
    float width = 0, height = 0;
    std::optional<float> minWidth, minHeight, maxWidth, maxHeight;
    Thickness padding;
    Thickness margin;
    // How the parent positions this node inside the space it gives it:
    // Start = top-left, Center = centered, End = bottom-right (per axis).
    // Only applied on the axes the parent does not manage itself (e.g. the
    // cross axis of a horizontal container, or unanchored axes of an absolute
    // container). Stretch is reserved and not implemented.
    Alignment horizontalAlignment;
    Alignment verticalAlignment;
};

namespace detail {

/**
 * @brief A single appearance property descriptor: source member in StyleRule
 * and destination member in ComputedAppearanceStyle.
 *
 * The list of descriptors (appearanceProps) is the single source of truth for
 * merge, resolve (applyRule) and inheritance. Adding an inheritable property
 * means adding a field plus one descriptor with inheritable = true.
 */
template <typename Src, typename Dst>
struct AppearanceProp {
    Src StyleRule::* src;
    Dst ComputedAppearanceStyle::* dst;
    bool inheritable = false;
    // True for text-metric properties (fontSize/fontPath): they are resolved by
    // the style system but change the measured size of text widgets, so a change
    // must invalidate the layout as well.
    bool affectsLayout = false;
};

/**
 * @brief A single layout property descriptor: source member in StyleRule and
 * destination member in ComputedLayoutStyle.
 */
template <typename Src, typename Dst>
struct LayoutProp {
    Src StyleRule::* src;
    Dst ComputedLayoutStyle::* dst;
};

template <typename Src, typename Dst>
constexpr void mergeAppearanceProp(StyleRule& dest, const StyleRule& src,
                                   const AppearanceProp<Src, Dst>& prop) {
    if ((src.*prop.src).has_value()) {
        dest.*prop.src = (src.*prop.src).value();
    }
}

template <typename Src, typename Dst>
constexpr void mergeLayoutProp(StyleRule& dest, const StyleRule& src,
                               const LayoutProp<Src, Dst>& prop) {
    if ((src.*prop.src).has_value()) {
        dest.*prop.src = (src.*prop.src).value();
    }
}

template <typename Src, typename Dst>
constexpr void applyAppearanceProp(ComputedAppearanceStyle& dest, const StyleRule& rule,
                                   const AppearanceProp<Src, Dst>& prop) {
    if ((rule.*prop.src).has_value()) {
        dest.*prop.dst = (rule.*prop.src).value();
    }
}

template <typename Src, typename Dst>
constexpr void applyLayoutProp(ComputedLayoutStyle& dest, const StyleRule& rule,
                               const LayoutProp<Src, Dst>& prop) {
    if ((rule.*prop.src).has_value()) {
        dest.*prop.dst = (rule.*prop.src).value();
    }
}

template <typename Src, typename Dst>
constexpr void inheritAppearanceProp(ComputedAppearanceStyle& dest,
                                     const ComputedAppearanceStyle& parent,
                                     const AppearanceProp<Src, Dst>& prop) {
    if (prop.inheritable) {
        dest.*prop.dst = parent.*prop.dst;
    }
}

template <typename Src, typename Dst>
constexpr bool equalAppearanceProp(const StyleRule& a, const StyleRule& b,
                                   const AppearanceProp<Src, Dst>& prop) {
    return (a.*prop.src) == (b.*prop.src);
}

template <typename Src, typename Dst>
constexpr bool equalLayoutProp(const StyleRule& a, const StyleRule& b,
                               const LayoutProp<Src, Dst>& prop) {
    return (a.*prop.src) == (b.*prop.src);
}

// The complete appearance property list. Each entry wires one StyleRule member
// to its ComputedAppearanceStyle counterpart; 'inheritable' marks text
// properties that are inherited from the parent's Normal state, and
// 'affectsLayout' marks text-metric properties (fontSize/fontPath) that change
// the measured size and therefore must also invalidate the layout.
inline constexpr auto appearanceProps = std::tuple{
    AppearanceProp<std::optional<Color>, Color>{&StyleRule::backgroundColor,
                                                &ComputedAppearanceStyle::backgroundColor},
    AppearanceProp<std::optional<Color>, Color>{&StyleRule::textColor,
                                                &ComputedAppearanceStyle::textColor, true},
    AppearanceProp<std::optional<Color>, Color>{&StyleRule::borderColor,
                                                &ComputedAppearanceStyle::borderColor},
    AppearanceProp<std::optional<int>, int>{&StyleRule::borderThickness,
                                            &ComputedAppearanceStyle::borderThickness},
    AppearanceProp<std::optional<int>, int>{&StyleRule::borderRadius,
                                            &ComputedAppearanceStyle::borderRadius},
    AppearanceProp<std::optional<CursorType>, CursorType>{&StyleRule::cursor,
                                                          &ComputedAppearanceStyle::cursor},
    AppearanceProp<std::optional<bool>, bool>{&StyleRule::clipContent,
                                              &ComputedAppearanceStyle::clipContent},
    AppearanceProp<std::optional<int>, int>{&StyleRule::fontSize,
                                            &ComputedAppearanceStyle::fontSize, true, true},
    AppearanceProp<std::optional<std::string>, std::string>{
        &StyleRule::fontPath, &ComputedAppearanceStyle::fontPath, true, true},
};

// The complete layout property list. left/top/right/bottom and min/max sizes
// stay optional in the computed style because "not set" carries meaning there.
inline constexpr auto layoutProps = std::tuple{
    LayoutProp<std::optional<float>, std::optional<float>>{&StyleRule::left,
                                                           &ComputedLayoutStyle::left},
    LayoutProp<std::optional<float>, std::optional<float>>{&StyleRule::top,
                                                           &ComputedLayoutStyle::top},
    LayoutProp<std::optional<float>, std::optional<float>>{&StyleRule::right,
                                                           &ComputedLayoutStyle::right},
    LayoutProp<std::optional<float>, std::optional<float>>{&StyleRule::bottom,
                                                           &ComputedLayoutStyle::bottom},
    LayoutProp<std::optional<float>, float>{&StyleRule::width, &ComputedLayoutStyle::width},
    LayoutProp<std::optional<float>, float>{&StyleRule::height, &ComputedLayoutStyle::height},
    LayoutProp<std::optional<float>, std::optional<float>>{&StyleRule::minWidth,
                                                           &ComputedLayoutStyle::minWidth},
    LayoutProp<std::optional<float>, std::optional<float>>{&StyleRule::minHeight,
                                                           &ComputedLayoutStyle::minHeight},
    LayoutProp<std::optional<float>, std::optional<float>>{&StyleRule::maxWidth,
                                                           &ComputedLayoutStyle::maxWidth},
    LayoutProp<std::optional<float>, std::optional<float>>{&StyleRule::maxHeight,
                                                           &ComputedLayoutStyle::maxHeight},
    LayoutProp<std::optional<Thickness>, Thickness>{&StyleRule::padding,
                                                    &ComputedLayoutStyle::padding},
    LayoutProp<std::optional<Thickness>, Thickness>{&StyleRule::margin,
                                                    &ComputedLayoutStyle::margin},
    LayoutProp<std::optional<Alignment>, Alignment>{&StyleRule::horizontalAlignment,
                                                    &ComputedLayoutStyle::horizontalAlignment},
    LayoutProp<std::optional<Alignment>, Alignment>{&StyleRule::verticalAlignment,
                                                    &ComputedLayoutStyle::verticalAlignment},
};

/**
 * @brief Checks whether two rules differ in any layout property.
 *
 * Used to decide whether a style change invalidates the layout: only width,
 * height, min/max, left/top/right/bottom, padding, margin and alignment affect
 * the measure/arrange cycle.
 */
inline bool layoutPropsDiffer(const StyleRule& a, const StyleRule& b) {
    return !std::apply(
        [&](const auto&... prop) { return (true && ... && equalLayoutProp(a, b, prop)); },
        layoutProps);
}

template <typename Src, typename Dst>
constexpr bool layoutPropIsSet(const StyleRule& rule, const LayoutProp<Src, Dst>& prop) {
    return (rule.*prop.src).has_value();
}

/**
 * @brief Checks whether a rule sets at least one layout property.
 */
inline bool hasLayoutProps(const StyleRule& rule) {
    return std::apply(
        [&](const auto&... prop) { return (false || ... || layoutPropIsSet(rule, prop)); },
        layoutProps);
}

template <typename Src, typename Dst>
constexpr bool appearancePropIsSet(const StyleRule& rule, const AppearanceProp<Src, Dst>& prop) {
    return (rule.*prop.src).has_value();
}

/**
 * @brief Checks whether two rules differ in any text-metric property.
 *
 * fontSize/fontPath change the measured size of text widgets even though they
 * are appearance properties, so they must also invalidate the layout.
 */
inline bool textMetricsPropsDiffer(const StyleRule& a, const StyleRule& b) {
    return !std::apply(
        [&](const auto&... prop) {
            return (true && ... && (!prop.affectsLayout || equalAppearanceProp(a, b, prop)));
        },
        appearanceProps);
}

/**
 * @brief Checks whether a rule sets at least one text-metric property.
 */
inline bool hasTextMetricsProps(const StyleRule& rule) {
    return std::apply(
        [&](const auto&... prop) {
            return (false || ... || (prop.affectsLayout && appearancePropIsSet(rule, prop)));
        },
        appearanceProps);
}

template <typename Src, typename Dst>
constexpr bool inheritablePropDiffers(const ComputedAppearanceStyle& a,
                                      const ComputedAppearanceStyle& b,
                                      const AppearanceProp<Src, Dst>& prop) {
    return prop.inheritable && (a.*prop.dst) != (b.*prop.dst);
}

/**
 * @brief Checks whether two resolved appearance styles differ in any
 * inheritable property.
 *
 * Children only need re-resolution when the text properties they inherit from
 * their parent's Normal state actually changed; a background- or layout-only
 * change on a parent must not re-resolve its whole subtree. The property set
 * comes from the same descriptor list as resolution, so it cannot drift.
 */
inline bool inheritableAppearanceDiffers(const ComputedAppearanceStyle& a,
                                         const ComputedAppearanceStyle& b) {
    return std::apply(
        [&](const auto&... prop) { return (false || ... || inheritablePropDiffers(a, b, prop)); },
        appearanceProps);
}

}  // namespace detail

inline void StyleRule::merge(const StyleRule& other) {
    std::apply([&](const auto&... prop) { (detail::mergeAppearanceProp(*this, other, prop), ...); },
               detail::appearanceProps);
    std::apply([&](const auto&... prop) { (detail::mergeLayoutProp(*this, other, prop), ...); },
               detail::layoutProps);
}

inline bool StyleRule::operator==(const StyleRule& other) const {
    return std::apply(
               [&](const auto&... prop) {
                   return (true && ... && detail::equalAppearanceProp(*this, other, prop));
               },
               detail::appearanceProps) &&
           std::apply(
               [&](const auto&... prop) {
                   return (true && ... && detail::equalLayoutProp(*this, other, prop));
               },
               detail::layoutProps);
}

/**
 * @class Style
 * @brief Owns the local style rules of a node plus its computed style cache.
 *
 * The node's author-provided rules are stored per WidgetState. The StyleManager
 * resolves them (together with theme defaults and inherited text properties)
 * into the computed cache, which is later consumed during layout and drawing.
 * The cache for a state is populated lazily: consumers must call
 * StyleManager::resolveDirtyStyles() before reading computed styles.
 *
 * Besides the local rules and the computed cache, Style also carries the
 * resolution bookkeeping used by the StyleManager traversal: the per-node
 * dirty flag, the subtree flag (this node or a descendant needs resolution)
 * and the modification version.
 */
class Style {
   public:
    /**
     * @brief Gets the style rule for a given state.
     * @param state The widget state to query.
     * @return A const pointer to the StyleRule, or nullptr if not found.
     * @exceptionGuarantee No-throw guarantee.
     */
    [[nodiscard]] const StyleRule* get(WidgetState state = WidgetState::Normal) const noexcept {
        const auto& slot = stateStyles[state_index(state)];
        return slot.has_value() ? &*slot : nullptr;
    }

    // --- Computed Style Cache ---

    /**
     * @brief Checks whether the computed cache must be re-resolved.
     * @return True if the style was modified since the last resolution.
     */
    [[nodiscard]] bool isDirty() const noexcept { return dirty; }

    /**
     * @brief Gets the modification version of the local style rules.
     *
     * Incremented on every set()/update() that actually changes a rule; a
     * no-op write does not bump it. Consumers (e.g. texture caches) can use it
     * to detect that a node's style changed without comparing property values.
     * @return The current version.
     */
    [[nodiscard]] std::uint64_t getVersion() const noexcept { return version_; }

    /**
     * @brief Marks the style as requiring re-resolution.
     *
     * Because descendants inherit text properties, StyleManager re-resolves the
     * whole subtree below a dirty node in the next resolve pass. The previously
     * computed values stay readable until the re-resolution happens.
     */
    void markDirty() noexcept { dirty = true; }

    /**
     * @brief Marks the computed cache as up-to-date.
     */
    void markClean() noexcept {
        dirty = false;
        resolved = true;
    }

    /**
     * @brief Checks whether the node or one of its descendants needs style
     * re-resolution.
     *
     * The flag is set when a rule anywhere in the subtree is marked dirty and
     * is consumed (cleared) by StyleManager::resolveDirtyStyles() once the
     * subtree has been re-resolved. It lets the traversal prune clean subtrees
     * instead of walking the whole scene graph.
     * @return True if style work is pending somewhere in this subtree.
     */
    [[nodiscard]] bool isSubtreeDirty() const noexcept { return subtreeDirty; }

    /**
     * @brief Marks the subtree as requiring a style resolution pass.
     */
    void markSubtreeDirty() noexcept { subtreeDirty = true; }

    /**
     * @brief Clears the subtree-dirty flag after the pending resolution.
     */
    void clearSubtreeDirty() noexcept { subtreeDirty = false; }

    /**
     * @brief Stores the resolved appearance for a given state.
     * @param state The widget state.
     * @param computed The resolved appearance style.
     */
    void setComputedAppearance(WidgetState state, const ComputedAppearanceStyle& computed) {
        appearanceCache[state_index(state)] = computed;
    }

    /**
     * @brief Stores the resolved layout properties for a given state.
     * @param state The widget state.
     * @param computed The resolved layout style.
     */
    void setComputedLayout(WidgetState state, const ComputedLayoutStyle& computed) {
        layoutCache[state_index(state)] = computed;
    }

    /**
     * @brief Gets the resolved appearance for a given state.
     * @param state The widget state.
     * @return A const pointer to the computed appearance, or nullptr if the
     * cache has not been populated for this state.
     */
    [[nodiscard]] const ComputedAppearanceStyle* getComputedAppearance(
        WidgetState state) const noexcept {
        if (!resolved) return nullptr;
        return &appearanceCache[state_index(state)];
    }

    /**
     * @brief Gets the resolved layout properties for a given state.
     * @param state The widget state.
     * @return A const pointer to the computed layout, or nullptr if the cache
     * has not been populated for this state.
     */
    [[nodiscard]] const ComputedLayoutStyle* getComputedLayout(WidgetState state) const noexcept {
        if (!resolved) return nullptr;
        return &layoutCache[state_index(state)];
    }

   private:
    friend class SceneNode;

    /**
     * @brief Sets or overwrites the style rule for a given state.
     *
     * Mutators are private: SceneNode routes them through setStyle()/updateStyle(),
     * which decide whether the change also invalidates the layout. If the new
     * rule equals the current one, nothing changes and the version is not bumped.
     * @param rule The style rule to set.
     * @param state The widget state to target.
     * @exceptionGuarantee Strong exception guarantee.
     */
    void set(const StyleRule& rule, WidgetState state = WidgetState::Normal) {
        auto& slot = stateStyles[state_index(state)];
        if (slot.has_value() && *slot == rule) return;
        slot = rule;
        version_++;
    }

    /**
     * @brief Merges new style properties into the existing rule for a given state.
     * If no rule exists for the state, a new one is created.
     * @param updates A StyleRule containing only the properties to change.
     * @param state The widget state to target.
     * @exceptionGuarantee Strong exception guarantee.
     */
    void update(const StyleRule& updates, WidgetState state = WidgetState::Normal) {
        auto& slot = stateStyles[state_index(state)];
        if (slot.has_value()) {
            const StyleRule before = *slot;
            slot->merge(updates);
            if (!(before == *slot)) version_++;
        } else {
            slot = updates;
            version_++;
        }
    }

    // The node's own rules per state; an empty optional means the state has no
    // local rule.
    std::array<std::optional<StyleRule>, kWidgetStateCount> stateStyles;
    // Computed caches, populated by StyleManager for every state on resolution.
    std::array<ComputedAppearanceStyle, kWidgetStateCount> appearanceCache;
    std::array<ComputedLayoutStyle, kWidgetStateCount> layoutCache;
    bool dirty = true;
    // True once the computed caches have been populated at least once.
    bool resolved = false;
    // True when this node or a descendant needs a style resolution pass.
    // Consumed (cleared) by StyleManager::resolveDirtyStyles().
    bool subtreeDirty = false;
    std::uint64_t version_ = 0;
};

}  // namespace DxvUI

#endif  // DXVUI_STYLE_H
