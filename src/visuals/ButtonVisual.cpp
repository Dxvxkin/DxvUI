#include "DxvUI/visuals/ButtonVisual.h"
#include "DxvUI/SceneNode.h"

namespace DxvUI {

    void ButtonVisual::draw(IRenderer& renderer, SceneNode* node) {
        if (!node) return;

        Rect bounds = node->getGlobalBounds();

        if (borderRadius > 0) {
            renderer.fillRoundRect(bounds, borderRadius, backgroundColor, {borderColor, borderWidth});
        } else {
            renderer.fillRect(bounds, backgroundColor, {borderColor, borderWidth});
        }
    }

}
