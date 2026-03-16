#ifndef DXVUI_BUTTON_H
#define DXVUI_BUTTON_H

#include "../SceneNode.h"
#include "../ActionRegistry.h"
#include "../Color.h"
#include <string>
#include <functional>
#include <optional>

namespace DxvUI {

    class Button : public SceneNode {
    public:
        Button(std::string id, std::optional<ActionCallback> onClick = std::nullopt);

        // Overriding to handle pending action registration
        void setScene(const std::shared_ptr<Scene>& scene) override;

        bool handleEvent(const DxvEvent& event) override;
        void draw(IRenderer& renderer) override;

        Color backgroundColor = {200, 200, 200};
        Color borderColor = {100, 100, 100};
        int borderWidth = 1;
        int borderRadius = 0;

    private:
        std::optional<ActionCallback> pendingAction;
    };

}

#endif //DXVUI_BUTTON_H
