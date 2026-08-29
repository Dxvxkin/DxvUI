#include "DxvUI/style/Theme.h"

#include <utility>
#include <vector>

namespace DxvUI {

namespace {

// Whether any rule in the map sets a layout-affecting property (a layout
// property or a text metric such as fontSize/fontFamily, which change the
// measured size of text widgets).
bool stateMapHasLayoutAffectingProps(const Theme::StateStyleMap& styles) {
    for (const auto& [state, rule] : styles) {
        if (detail::hasLayoutProps(rule) || detail::hasTextMetricsProps(rule)) return true;
    }
    return false;
}

// Whether the two maps differ in any layout-affecting property (a missing rule
// counts as empty, so a prop appearing/disappearing is a difference).
bool stateMapLayoutAffectingPropsDiffer(const Theme::StateStyleMap& a,
                                        const Theme::StateStyleMap& b) {
    std::vector<WidgetState> states;
    for (const auto& [s, r] : a) states.push_back(s);
    for (const auto& [s, r] : b) {
        if (!a.contains(s)) states.push_back(s);
    }

    for (const auto s : states) {
        const StyleRule* ra = nullptr;
        const StyleRule* rb = nullptr;
        if (auto it = a.find(s); it != a.end()) ra = &it->second;
        if (auto it = b.find(s); it != b.end()) rb = &it->second;
        if (ra && rb) {
            if (detail::layoutPropsDiffer(*ra, *rb) || detail::textMetricsPropsDiffer(*ra, *rb)) {
                return true;
            }
        } else if (ra) {
            if (detail::hasLayoutProps(*ra) || detail::hasTextMetricsProps(*ra)) return true;
        } else if (rb) {
            if (detail::hasLayoutProps(*rb) || detail::hasTextMetricsProps(*rb)) return true;
        }
    }
    return false;
}

}  // namespace

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
    const StateStyleMap before = merged;
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
    if (stateMapLayoutAffectingPropsDiffer(before, merged)) layoutVersion_++;
}

void Theme::clearDefaultStyle(const std::string& widgetType) {
    if (auto it = overrides_.find(widgetType); it != overrides_.end()) {
        if (stateMapHasLayoutAffectingProps(it->second)) layoutVersion_++;
        overrides_.erase(it);
        version_++;
    }
}

void Theme::clear() {
    if (overrides_.empty()) return;
    for (const auto& [type, styles] : overrides_) {
        if (stateMapHasLayoutAffectingProps(styles)) {
            layoutVersion_++;
            break;
        }
    }
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
