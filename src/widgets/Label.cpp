#include "DxvUI/widgets/Label.h"
#include "DxvUI/interfaces/IRenderer.h"
#include "DxvUI/Scene.h"
#include <utility>

namespace DxvUI {

    std::shared_ptr<Label> Label::create(std::string id, std::string text) {
        return std::make_shared<Label>(std::move(id), std::move(text));
    }

    Label::Label(std::string id, std::string text)
        : SceneNode(std::move(id)), text(std::move(text)) {}

    void Label::setText(const std::string& newText) {
        if (text != newText) {
            text = newText;
            markLayoutDirty();
        }
    }

    const std::string& Label::getText() const {
        return text;
    }

    Size Label::measure(const Size& availableSize) {
        if (isLayoutDirty) {
            auto& computed = getComputedAppearance();
            auto scene = getScene();
            if (scene && scene->getRenderer()) {
                Rect measured = scene->getRenderer()->measureText(text, computed.fontPath, computed.fontSize);
                desiredSize = {(float)measured.width, (float)measured.height};
            } else {
                desiredSize = {0, 0};
            }
        }
        return desiredSize;
    }

    void Label::draw(IRenderer& renderer) {
        const auto& computed = getComputedAppearance();

        // Draw background and border from the unified appearance
        renderer.fillRoundRect(getGlobalBounds(), computed.borderRadius, computed.backgroundColor, {computed.borderColor, computed.borderThickness});

        // Draw the text
        renderer.setFont(computed.fontPath, computed.fontSize);
        renderer.setDrawColor(computed.textColor);
        renderer.drawText(text, getGlobalBounds().x, getGlobalBounds().y);

        SceneNode::draw(renderer); // Draw children if any
    }

}
