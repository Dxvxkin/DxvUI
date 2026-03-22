#ifndef DXVUI_APPEARANCE_H
#define DXVUI_APPEARANCE_H

#include "DxvUI/Color.h"
#include "DxvUI/core.h" // For CursorType
#include <optional>
#include <string>

namespace DxvUI {

    struct Appearance {
        std::optional<Color> backgroundColor;
        std::optional<Color> textColor;
        std::optional<Color> borderColor;
        std::optional<int> borderThickness;
        std::optional<int> borderRadius;

        std::optional<int> fontSize;
        std::optional<std::string> fontPath;

        std::optional<CursorType> cursor;
    };

}

#endif // DXVUI_APPEARANCE_H
