#ifndef DXVUI_THEME_H
#define DXVUI_THEME_H

#include <string>
#include <map>
#include "Style.h"

namespace DxvUI {

    class Theme {
    public:
        Theme() = default;

        // --- Static API for Widget Default Style Registration ---

        // A map from WidgetState to the StyleRule for that state.
        using StateStyleMap = std::map<WidgetState, StyleRule>;

        // Registers a map of default styles for a specific widget type.
        // This should be called by widgets in their .cpp file to self-register.
        static void registerDefaultStyle(const std::string& widgetType, StateStyleMap styles);

        // --- Instance API for Style Resolution ---

        // Retrieves the default style rule for a widget type and state.
        const StyleRule* getDefaultRule(const std::string& widgetType, WidgetState state) const;

    private:
        // The static map is now an implementation detail in the .cpp file.
    };

}

#endif //DXVUI_THEME_H
