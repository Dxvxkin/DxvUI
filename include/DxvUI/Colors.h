#ifndef DXVUI_COLORS_H
#define DXVUI_COLORS_H

#include "Color.h"

namespace DxvUI::Colors {

    // Standard Colors
    constexpr Color Black(0, 0, 0);
    constexpr Color White(255, 255, 255);
    constexpr Color Red(255, 0, 0);
    constexpr Color Green(0, 255, 0);
    constexpr Color Blue(0, 0, 255);
    constexpr Color Yellow(255, 255, 0);
    constexpr Color Cyan(0, 255, 255);
    constexpr Color Magenta(255, 0, 255);

    // Grays
    constexpr Color LightGray(211, 211, 211);
    constexpr Color Gray(128, 128, 128);
    constexpr Color DarkGray(169, 169, 169);

    // Transparent
    constexpr Color Transparent(0, 0, 0, 0);

}

#endif //DXVUI_COLORS_H
