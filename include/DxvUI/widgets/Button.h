#ifndef DXVUI_BUTTON_H
#define DXVUI_BUTTON_H

#include "../SceneNode.h"
#include <string>
#include <functional>

namespace DxvUI {

    class Button : public SceneNode {
    public:
        // Constructor for JSON/Registry-based actions
        explicit Button(const std::string& actionName);

        // Constructor for direct lambda-based actions
        explicit Button(std::function<void()> onClick);

        void handleEvent(const DxvEvent& event) override;
        void draw(IRenderer& renderer) override;

    private:
        std::string actionName;
        std::function<void()> onClickAction;
    };

}

#endif //DXVUI_BUTTON_H
