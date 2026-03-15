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
        if (SceneNode::handleEvent(event)) {
            return true;
        }

        if (event.type == EventType::MouseDown) {
            if (getGlobalBounds().contains(event.x, event.y)) {
                if (auto action = ActionRegistry::instance().getAction(buttonId)) {
                    action(this, event);
                }
                return true;
            }
        }

        return false;
    }

    void Button::draw(IRenderer& renderer) {
        Rect bounds = getGlobalBounds();
        
        // The new renderer API makes drawing complex shapes trivial.
        // We can draw a filled, rounded rectangle with a border in a single call.
        if (borderWidth > 0) {
            Border border{ .color = borderColor, .thickness = borderWidth };
            renderer.fillRoundRect(bounds, borderRadius, backgroundColor, border);
        } else {
            // If there's no border, just draw the filled rounded rectangle.
            renderer.fillRoundRect(bounds, borderRadius, backgroundColor);
        }

        SceneNode::draw(renderer);
    }

}
