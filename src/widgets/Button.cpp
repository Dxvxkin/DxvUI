#include "DxvUI/widgets/Button.h"
#include "DxvUI/Scene.h"
#include "DxvUI/LayoutProperties.h"
#include <utility>

namespace DxvUI {

    Button::Button(std::string id, std::optional<ActionCallback> onClick)
        : SceneNode(std::move(id)), pendingAction(std::move(onClick)) {}

    void Button::setScene(const std::shared_ptr<Scene>& newScene) {
        SceneNode::setScene(newScene);
        if (pendingAction && newScene) {
            newScene->getActionRegistry().registerAction(getId(), *pendingAction);
            pendingAction.reset();
        }
    }

    bool Button::handleEvent(const DxvEvent& event) {
        // Let children handle the event first (e.g., a text label on the button)
        if (SceneNode::handleEvent(event)) return true;

        // If no child consumed it, check if this button is clicked.
        if (event.type == EventType::MouseDown) {
            // getGlobalBounds() now returns the final, correct bounds calculated during the layout pass.
            if (getGlobalBounds().contains(event.x, event.y)) {
                if (auto scn = getScene()) {
                    if (auto action = scn->getActionRegistry().getAction(getId())) {
                        action(this, event);
                    }
                }
                return true; // Event consumed!
            }
        }

        return false;
    }

    void Button::draw(IRenderer& renderer) {
        // getGlobalBounds() returns the final, correct bounds. No calculation needed here.
        Rect bounds = getGlobalBounds();

        if (borderRadius > 0) {
            renderer.fillRoundRect(bounds, borderRadius, backgroundColor, {borderColor, borderWidth});
        } else {
            renderer.fillRect(bounds, backgroundColor, {borderColor, borderWidth});
        }

        // Draw children on top
        SceneNode::draw(renderer);
    }

}
