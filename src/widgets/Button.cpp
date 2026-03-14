#include "DxvUI/widgets/Button.h"
#include "DxvUI/ActionRegistry.h"

namespace DxvUI {

    // Constructor for JSON/Registry-based actions
    Button::Button(const std::string& actionName) : actionName(actionName), onClickAction(nullptr) {}

    // Constructor for direct lambda-based actions
    Button::Button(std::function<void()> onClick) : onClickAction(std::move(onClick)) {}

    void Button::handleEvent(const DxvEvent& event) {
        if (event.type == EventType::MouseDown) {
            int globalX, globalY;
            getGlobalPosition(globalX, globalY);

            bool isClicked = (event.x >= globalX && event.x <= globalX + width &&
                              event.y >= globalY && event.y <= globalY + height);

            if (isClicked) {
                // Priority 1: Direct lambda
                if (onClickAction) {
                    onClickAction();
                }
                // Priority 2: Action Registry
                else if (!actionName.empty()) {
                    if (auto action = ActionRegistry::instance().getAction(actionName)) {
                        action();
                    }
                }
            }
        }
        // Propagate event to children anyway
        SceneNode::handleEvent(event);
    }

    void Button::draw(IRenderer& renderer) {
        int globalX, globalY;
        getGlobalPosition(globalX, globalY);
        renderer.drawRect(globalX, globalY, width, height);
        SceneNode::draw(renderer);
    }

}
