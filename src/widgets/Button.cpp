#include "DxvUI/widgets/Button.h"
#include <utility>

namespace DxvUI {

    Button::Button(std::string id, std::optional<ActionCallback> onClick)
        : SceneNode(std::move(id)) { // Pass the id to the base class constructor
        if (onClick) {
            // Use the id from the base class
            ActionRegistry::instance().registerAction(this->id, *onClick);
        }
    }

    bool Button::handleEvent(const DxvEvent& event) {
        if (SceneNode::handleEvent(event)) {
            return true;
        }

        if (event.type == EventType::MouseDown) {
            if (getGlobalBounds().contains(event.x, event.y)) {
                // Use the id from the base class
                if (auto action = ActionRegistry::instance().getAction(getId())) {
                    action(this, event);
                }
                return true;
            }
        }

        return false;
    }

    void Button::draw(IRenderer& renderer) {
        Rect bounds = getGlobalBounds();
        
        if (borderRadius > 0) {
            if (borderWidth > 0) {
                renderer.fillRoundRect(bounds, borderRadius, backgroundColor, {borderColor, borderWidth});
            } else {
                renderer.fillRoundRect(bounds, borderRadius, backgroundColor);
            }
        } else {
            if (borderWidth > 0) {
                renderer.fillRect(bounds, backgroundColor, {borderColor, borderWidth});
            } else {
                renderer.fillRect(bounds, backgroundColor);
            }
        }

        SceneNode::draw(renderer);
    }

}
