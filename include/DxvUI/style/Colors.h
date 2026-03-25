#ifndef DXVUI_COLORS_H
#define DXVUI_COLORS_H

#include "Color.h"

namespace DxvUI::Colors {

    // Standard Colors
    constexpr Color Black(0, 0, 0);
    constexpr Color White(255, 255, 255);
    constexpr Color Transparent(0, 0, 0, 0);

    // Grays
    constexpr Color LightGray(211, 211, 211);
    constexpr Color Gray(128, 128, 128);
    constexpr Color DarkGray(169, 169, 169);

    // Reds
    constexpr Color Red(255, 0, 0);
    constexpr Color IndianRed(205, 92, 92);
    constexpr Color LightCoral(240, 128, 128);
    constexpr Color DarkRed(139, 0, 0);

    // Greens
    constexpr Color Green(0, 128, 0);
    constexpr Color Lime(0, 255, 0);
    constexpr Color DarkGreen(0, 100, 0);
    constexpr Color ForestGreen(34, 139, 34);
    constexpr Color SeaGreen(46, 139, 87);

    // Blues
    constexpr Color Blue(0, 0, 255);
    constexpr Color CornflowerBlue(100, 149, 237);
    constexpr Color RoyalBlue(65, 105, 225);
    constexpr Color MidnightBlue(25, 25, 112);
    constexpr Color SkyBlue(135, 206, 235);

    // Yellows & Oranges
    constexpr Color Yellow(255, 255, 0);
    constexpr Color Gold(255, 215, 0);
    constexpr Color Orange(255, 165, 0);
    constexpr Color DarkOrange(255, 140, 0);

    // Purples & Pinks
    constexpr Color Magenta(255, 0, 255);
    constexpr Color Purple(128, 0, 128);
    constexpr Color Indigo(75, 0, 130);
    constexpr Color HotPink(255, 105, 180);

}

#endif //DXVUI_COLORS_H
