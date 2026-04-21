#include "DxvUI/style/Theme.h"

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

const StyleRule* Theme::getDefaultRule(const std::string& widgetType, WidgetState state) const {
    const auto& defaults = getFrameworkDefaults();

    // 1. Find the style map for the given widget type.
    auto type_it = defaults.find(widgetType);
    if (type_it == defaults.end()) {
        return nullptr;  // No styles registered for this widget type.
    }

    // 2. Find the specific rule for the given state within that widget's style map.
    const auto& state_styles = type_it->second;
    auto state_it = state_styles.find(state);
    if (state_it == state_styles.end()) {
        return nullptr;  // This widget has styles, but not for this specific state.
    }

    // 3. Return the found rule.
    return &state_it->second;
}

}  // namespace DxvUI
