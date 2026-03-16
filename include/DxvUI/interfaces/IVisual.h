#ifndef DXVUI_IVISUAL_H
#define DXVUI_IVISUAL_H

#include <memory> // For std::unique_ptr
#include "../core.h" // For Rect, Color etc.
#include "IRenderer.h" // For IRenderer

namespace DxvUI {

    class SceneNode; // Forward declaration

    /**
     * @brief Interface for a visual component that defines how a SceneNode is drawn.
     */
    class IVisual {
    public:
        virtual ~IVisual() = default;

        /**
         * @brief Draws the associated SceneNode.
         * @param renderer The renderer to use for drawing.
         * @param node The SceneNode to be drawn.
         */
        virtual void draw(IRenderer& renderer, SceneNode* node) = 0;
    };

}

#endif //DXVUI_IVISUAL_H
