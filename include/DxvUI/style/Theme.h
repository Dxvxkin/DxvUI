#ifndef DXVUI_THEME_H
#define DXVUI_THEME_H

#include <unordered_map>
#include <string>
#include "Style.h"

namespace DxvUI {

    // A Theme holds the base styles for different widget types.
    // This allows for centralized styling and easy skinning.
    class Theme {
    public:
        Theme();

        // Sets or overrides the default style rule for a given widget type.
        void setDefaultRule(const std::string& widgetType, StyleRule rule);

        // Retrieves the default style rule for a widget type.
        const StyleRule* getDefaultRule(const std::string& widgetType) const;

    private:
        std::unordered_map<std::string, StyleRule> typeStyles;
    };

}

#endif //DXVUI_THEME_H
