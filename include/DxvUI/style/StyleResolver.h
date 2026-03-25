#ifndef DXVUI_STYLERESOLVER_H
#define DXVUI_STYLERESOLVER_H

#include "DxvUI/SceneNode.h"

namespace DxvUI {

    // Forward declarations
    class Theme;

    class StyleResolver {
    public:
        static ComputedAppearanceStyle resolveAppearance(const SceneNode& node, WidgetState state);
        static ComputedLayoutStyle resolveLayout(const SceneNode& node, WidgetState state);
    };

}

#endif //DXVUI_STYLERESOLVER_H
