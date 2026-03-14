#include "DxvUI/Button.h"
#include "DxvUI/ActionRegistry.h"

namespace DxvUI {

    Button::Button(const std::string& actionName) : actionName(actionName) {}

    void Button::handleEvent(const DxvEvent& event) {
        if (event.type == EventType::MouseDown) {
            int globalX, globalY;
            getGlobalPosition(globalX, globalY);
            if (event.x >= globalX && event.x <= globalX + width &&
                event.y >= globalY && event.y <= globalY + height) {
                if (auto action = ActionRegistry::instance().getAction(actionName)) {
                    action();
                }
            }
        }
        SceneNode::handleEvent(event);
    }

    void Button::draw(IRenderer& renderer) {
        int globalX, globalY;
        getGlobalPosition(globalX, globalY);
        renderer.drawRect(globalX, globalY, width, height);
        SceneNode::draw(renderer);
    }

}
