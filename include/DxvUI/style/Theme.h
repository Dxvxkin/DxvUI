#ifndef DXVUI_THEME_H
#define DXVUI_THEME_H

#include <cstdint>
#include <functional>
#include <map>
#include <string>
#include <utility>

#include "Style.h"

namespace DxvUI {

/**
 * @class Theme
 * @brief Owns the default styles per widget type and applies instance-level
 * overrides on top of the framework-wide widget defaults.
 *
 * Widget types self-register their framework defaults once (static registry,
 * see registerDefaultStyle). A Theme instance can additionally override those
 * defaults via setDefaultStyle(); the overrides are merged on top of the
 * framework defaults, so tweaking a single property keeps the rest. Every
 * mutation bumps getVersion() so that a StyleManager can detect that cached
 * styles must be re-resolved.
 */
class Theme {
   public:
    Theme() = default;

    // A map from WidgetState to the StyleRule for that state.
    using StateStyleMap = std::map<WidgetState, StyleRule>;

    // --- Static API for Widget Default Style Registration ---

    // Registers a map of default styles for a specific widget type.
    // This should be called by widgets in their .cpp file to self-register.
    static void registerDefaultStyle(const std::string& widgetType, StateStyleMap styles);

    // --- Instance API for Theme Customization ---

    /**
     * @brief Overrides the default styles for a widget type in this theme.
     *
     * The provided rules are merged on top of the framework defaults, so only
     * the explicitly set properties differ from the framework look.
     * @param widgetType The widget type to customize.
     * @param styles The state rules to apply on top of the framework defaults.
     * @exceptionGuarantee Basic exception guarantee.
     */
    void setDefaultStyle(const std::string& widgetType, const StateStyleMap& styles);

    /**
     * @brief Removes the instance override for a widget type.
     * @param widgetType The widget type to reset to framework defaults.
     */
    void clearDefaultStyle(const std::string& widgetType);

    /**
     * @brief Removes all instance overrides from this theme.
     */
    void clear();

    /**
     * @brief Gets the theme's mutation version.
     *
     * Incremented on every setDefaultStyle/clearDefaultStyle/clear call.
     * @return The current version.
     */
    [[nodiscard]] std::uint64_t getVersion() const noexcept { return version_; }

    /**
     * @brief Sets a callback invoked whenever the theme is mutated.
     *
     * Scene uses this to schedule a layout/style refresh when the theme changes.
     * @param callback The callback to invoke, may be empty to disable.
     */
    void setOnChanged(std::function<void()> callback) { onChanged_ = std::move(callback); }

    // --- Instance API for Style Resolution ---

    // Retrieves the resolved default style rule for a widget type and state:
    // the framework default with the instance override merged in.
    const StyleRule* getDefaultRule(const std::string& widgetType, WidgetState state) const;

   private:
    void notifyChanged();

    // Instance-level overrides, already merged with the framework defaults at
    // setDefaultStyle time.
    std::map<std::string, StateStyleMap> overrides_;
    std::function<void()> onChanged_;
    std::uint64_t version_ = 0;
};

}  // namespace DxvUI

#endif  // DXVUI_THEME_H
