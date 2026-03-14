#include "DxvUI/SceneNode.h"
#include <algorithm>

namespace DxvUI {

    SceneNode::SceneNode() = default;

    void SceneNode::setParent(std::shared_ptr<SceneNode> newParent) {
        if (auto p = parent.lock()) {
            p->removeChild(shared_from_this());
        }
        parent = newParent;
    }

    void SceneNode::addChild(std::shared_ptr<SceneNode> child) {
        children.push_back(child);
        child->setParent(shared_from_this());
    }

    void SceneNode::removeChild(std::shared_ptr<SceneNode> child) {
        children.erase(std::remove(children.begin(), children.end(), child), children.end());
        child->parent.reset();
    }

    void SceneNode::getGlobalPosition(int& x, int& y) const {
        x = relX;
        y = relY;
        if (auto p = parent.lock()) {
            int parentX, parentY;
            p->getGlobalPosition(parentX, parentY);
            x += parentX;
            y += parentY;
        }
    }

    void SceneNode::handleEvent(const DxvEvent& event) {
        for (auto& child : children) {
            child->handleEvent(event);
        }
    }

    void SceneNode::draw(IRenderer& renderer) {
        for (auto& child : children) {
            child->draw(renderer);
        }
    }

}
