#include "DxvUI/widgets/Button.h"
#include "DxvUI/Scene.h"
#include "DxvUI/visuals/ButtonVisual.h"
#include <utility>

namespace DxvUI {

    Button::Button(std::string id, std::optional<ActionCallback> onClick)
        : SceneNode(std::move(id)), pendingAction(std::move(onClick)) {
        // Assign a default visual component for a Button
        setVisual(std::make_unique<ButtonVisual>());
    }

    void Button::setScene(const std::shared_ptr<Scene>& newScene) {
        SceneNode::setScene(newScene);
        if (pendingAction && newScene) {
            newScene->getActionRegistry().registerAction(getId(), *pendingAction);
            pendingAction.reset();
        }
    }

    bool Button::handleEvent(const DxvEvent& event) {
        // The base class now handles hit-testing, so we just need to check for the click
        // if the event reaches us.
        if (SceneNode::handleEvent(event)) return true;

        if (event.type == EventType::MouseDown) {
            // If we are here, it means the click was inside our bounds but not handled by a child.
            if (auto scn = getScene()) {
                if (auto action = scn->getActionRegistry().getAction(getId())) {
                    action(this, event);
                }
            }
            return true; // Consume the click
        }

        return false;
    }

    void Button::draw(IRenderer& renderer) {
        // The base class will call our visual component.
        // We override this only if we need to do something extra during the draw pass.
        // For now, just let the base class do its job.
        SceneNode::draw(renderer);
    }

    ButtonVisual* Button::getButtonVisual() const {
        // Use dynamic_cast to safely get the specific visual type
        return dynamic_cast<ButtonVisual*>(visual.get());
    }

}
