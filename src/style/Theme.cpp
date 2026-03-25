#include "DxvUI/style/Theme.h"
#include "DxvUI/style/Colors.h"
#include "DxvUI/Log.h"

namespace DxvUI {

    // A static, constant map to hold the framework's built-in default styles.
    // This is declarative and easy to read.
    static const std::unordered_map<std::string, StyleRule> frameworkDefaults = {
        {
            "Button",
            {
                .backgroundColor = {{220, 220, 220, 255}},
                .textColor = {{0, 0, 0, 255}},
                .padding = {{5, 10, 5, 10}},
                .horizontalAlignment = {Alignment::Center},
                .verticalAlignment = {Alignment::Center}
            }
        },
        {
            "Label",
            {
                .backgroundColor = {Colors::Transparent},
                .textColor = {{0, 0, 0, 255}}
            }
        }
    };

    Theme::Theme() {
        Log::trace("Theme object created.");
    }

    void Theme::setDefaultRule(const std::string& widgetType, StyleRule rule) {
        Log::trace("Setting user-defined default rule for widget type '{}'", widgetType);
        typeStyles[widgetType] = std::move(rule);
    }

    const StyleRule* Theme::getDefaultRule(const std::string& widgetType) const {
        Log::trace("Querying default rule for widget type '{}'", widgetType);

        // 1. First, check for a user-defined override.
        auto it = typeStyles.find(widgetType);
        if (it != typeStyles.end()) {
            Log::trace("  > Found user-defined override.");
            return &it->second;
        }

        // 2. If no override exists, check for a framework-defined default.
        auto framework_it = frameworkDefaults.find(widgetType);
        if (framework_it != frameworkDefaults.end()) {
            Log::trace("  > Found framework-defined default.");
            return &framework_it->second;
        }

        // 3. If no style is found, return nullptr.
        Log::trace("  > No default rule found for this type.");
        return nullptr;
    }

}
