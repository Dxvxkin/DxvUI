#include "DxvUI/style/Theme.h"

#include <utility>

#include "DxvUI/Log.h"

namespace DxvUI {

// Use the "Construct on First Use" idiom to avoid the static initialization order fiasco.
// This function's local static variable is guaranteed to be initialized only once,
// the first time this function is called.
static auto& getFrameworkDefaults() {
    static std::map<std::string, Theme::StateStyleMap> frameworkDefaults;
    return frameworkDefaults;
}

void Theme::registerDefaultStyle(const std::string& widgetType, StateStyleMap styles) {
    auto& defaults = getFrameworkDefaults();

    // Check for collisions to prevent accidental overwrites.
    if (defaults.contains(widgetType)) {
        // We can't use the logger here, as it might not be initialized yet itself.
        // This is a classic issue with static initialization.
        // For now, we'll just allow overwrites. A more robust system might
        // queue up log messages.
    }

    defaults[widgetType] = std::move(styles);
}

void Theme::setDefaultStyle(const std::string& widgetType, const StateStyleMap& styles) {
    // Start from the framework defaults for the widget type, then merge the
    // instance rules on top so an override can tweak a single property.
    auto& merged = overrides_[widgetType];
    const auto& defaults = getFrameworkDefaults();
    if (auto it = defaults.find(widgetType); it != defaults.end()) {
        merged = it->second;
    } else {
        merged.clear();
    }

    for (const auto& [state, rule] : styles) {
        merged[state].merge(rule);
    }

    version_++;
}

void Theme::clearDefaultStyle(const std::string& widgetType) {
    if (overrides_.erase(widgetType) > 0) {
        version_++;
    }
}

void Theme::clear() {
    if (overrides_.empty()) return;
    overrides_.clear();
    version_++;
}

const StyleRule* Theme::getDefaultRule(const std::string& widgetType, WidgetState state) const {
    // 1. Prefer the instance override, which already contains the framework
    //    defaults merged in at setDefaultStyle time.
    if (auto it = overrides_.find(widgetType); it != overrides_.end()) {
        auto state_it = it->second.find(state);
        return state_it != it->second.end() ? &state_it->second : nullptr;
    }

    // 2. Fall back to the framework defaults.
    const auto& defaults = getFrameworkDefaults();
    auto type_it = defaults.find(widgetType);
    if (type_it == defaults.end()) {
        return nullptr;  // No styles registered for this widget type.
    }

    auto state_it = type_it->second.find(state);
    if (state_it == type_it->second.end()) {
        return nullptr;  // This widget has styles, but not for this specific state.
    }

    return &state_it->second;
}

}  // namespace DxvUI
