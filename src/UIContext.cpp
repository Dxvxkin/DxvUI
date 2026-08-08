#include "DxvUI/UIContext.h"

#include "DxvUI/Scene.h"
#include "DxvUI/SceneNode.h"
#include "DxvUI/interfaces/IRenderer.h"
#include "DxvUI/style/Theme.h"

namespace DxvUI {

std::shared_ptr<SceneNode> UIContext::getRoot() const {
    return scene_ ? scene_->getRoot() : nullptr;
}

std::shared_ptr<SceneNode> UIContext::findNodeById(const std::string& id) const {
    return scene_ ? scene_->findNodeById(id) : nullptr;
}

std::shared_ptr<SceneNode> UIContext::getFocusedNode() const {
    return scene_ ? scene_->getFocusedNode() : nullptr;
}

void UIContext::setFocus(const std::shared_ptr<SceneNode>& node) const {
    if (scene_) {
        scene_->setFocus(node);
    }
}

void UIContext::updateLayout() const {
    if (scene_) {
        scene_->updateLayout();
    }
}

Theme* UIContext::getTheme() const { return scene_ ? &scene_->getTheme() : nullptr; }

IRenderer* UIContext::getRenderer() const { return scene_ ? scene_->getRenderer() : nullptr; }

Size UIContext::getViewport() const {
    if (auto* renderer = getRenderer()) {
        return renderer->getViewportSize();
    }
    return {};
}

}  // namespace DxvUI
