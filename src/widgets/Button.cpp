#include "DxvUI/widgets/Button.h"
#include <utility>

namespace DxvUI {

    Button::Button(std::string id, std::optional<ActionCallback> onClick)
        : buttonId(std::move(id)) {
        if (onClick) {
            ActionRegistry::instance().registerAction(buttonId, *onClick);
        }
    }

    bool Button::handleEvent(const DxvEvent& event) {
        // First, let children try to handle the event.
        if (SceneNode::handleEvent(event)) {
            return true;
        }

        // If no child consumed it, check if this button is clicked.
        if (event.type == EventType::MouseDown) {
            int globalX, globalY;
            getGlobalPosition(globalX, globalY);

            bool isClicked = (event.x >= globalX && event.x <= globalX + width &&
                              event.y >= globalY && event.y <= globalY + height);

            if (isClicked) {
                if (auto action = ActionRegistry::instance().getAction(buttonId)) {
                    // Call the action, passing this button as the sender and the event details.
                    action(this, event);
                }
                return true; // Event consumed!
            }
        }

        return false; // Event not consumed
    }

    void Button::draw(IRenderer& renderer) {
        int globalX, globalY;
        getGlobalPosition(globalX, globalY);
        renderer.drawRect(globalX, globalY, width, height);

        // Also draw children
        SceneNode::draw(renderer);
    }

}
