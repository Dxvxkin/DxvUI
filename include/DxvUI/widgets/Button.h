#ifndef DXVUI_BUTTON_H
#define DXVUI_BUTTON_H

#include "DxvUI/SceneNode.h"
#include "DxvUI/ActionRegistry.h"
#include "DxvUI/Color.h"
#include "DxvUI/visuals/ButtonVisual.h"
#include <string>
#include <functional>
#include <optional>

namespace DxvUI {

    class Button : public SceneNode {
    public:
        Button(std::string id, std::optional<ActionCallback> onClick = std::nullopt);

        void setScene(const std::shared_ptr<Scene>& scene) override;

        bool handleEvent(const DxvEvent& event) override;
        void draw(IRenderer& renderer) override;

        ButtonVisual* getButtonVisual() const;

    private:
        std::optional<ActionCallback> pendingAction;
    };

}

#endif //DXVUI_BUTTON_H
