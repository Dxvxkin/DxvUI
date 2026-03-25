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

    const char* Label::getNodeType() const {
        return "Label";
    }

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
        if (!isLayoutDirty) return desiredSize;

        const auto& computedAppearance = getComputedAppearance(getCurrentState());

        auto scene = getScene();
        if (scene && scene->getRenderer()) {
            Rect measured = scene->getRenderer()->measureText(text, computedAppearance.fontPath, computedAppearance.fontSize);
            desiredSize = {(float)measured.width, (float)measured.height};
        } else {
            desiredSize = {0, 0};
        }

        const auto& computedLayout = getComputedLayout(getCurrentState());
        if (computedLayout.width > 0) desiredSize.width = computedLayout.width;
        if (computedLayout.height > 0) desiredSize.height = computedLayout.height;

        return desiredSize;
    }

    void Label::draw(IRenderer& renderer) {
        const auto& computedAppearance = getComputedAppearance(getCurrentState());
        const auto& computedLayout = getComputedLayout(getCurrentState());

        renderer.fillRoundRect(computedLayout.computedBounds, computedAppearance.borderRadius, computedAppearance.backgroundColor, {computedAppearance.borderColor, computedAppearance.borderThickness});

        renderer.setFont(computedAppearance.fontPath, computedAppearance.fontSize);
        renderer.setDrawColor(computedAppearance.textColor);
        renderer.drawText(text, computedLayout.computedBounds.x, computedLayout.computedBounds.y);

        SceneNode::draw(renderer);
    }

}
