#ifndef DXVUI_BUTTONVISUAL_H
#define DXVUI_BUTTONVISUAL_H

#include "DxvUI/interfaces/IVisual.h"
#include "DxvUI/Color.h"

namespace DxvUI {

    class ButtonVisual : public IVisual {
    public:
        void draw(IRenderer& renderer, SceneNode* node) override;

        // Properties that used to be in Button
        Color backgroundColor = {200, 200, 200};
        Color borderColor = {100, 100, 100};
        int borderWidth = 1;
        int borderRadius = 0;
    };

}

#endif //DXVUI_BUTTONVISUAL_H
