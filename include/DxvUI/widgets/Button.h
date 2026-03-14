#ifndef DXVUI_BUTTON_H
#define DXVUI_BUTTON_H

#include "../SceneNode.h"
#include "../ActionRegistry.h" // For ActionCallback
#include <string>
#include <functional>
#include <optional>

namespace DxvUI {

    class Button : public SceneNode {
    public:
        /**
         * @brief Constructs a Button.
         * @param id A unique identifier for this button.
         * @param onClick An optional lambda function. If provided, it will be registered
         *                in the ActionRegistry with the given id. If not provided, the button
         *                expects an action to be already registered with this id.
         */
        Button(std::string id, std::optional<ActionCallback> onClick = std::nullopt);

        bool handleEvent(const DxvEvent& event) override;
        void draw(IRenderer& renderer) override;

    private:
        std::string buttonId;
    };

}

#endif //DXVUI_BUTTON_H
