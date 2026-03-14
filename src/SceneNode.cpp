#include "DxvUI/SceneNode.h"

namespace DxvUI {

    SceneNode::SceneNode() = default;

    void SceneNode::setParent(std::shared_ptr<SceneNode> newParent) {
        if (auto p = parent.lock()) {
            p->removeChild(shared_from_this());
        }
        parent = newParent;
        if (auto p = parent.lock()) {
            p->childrenOrderDirty = true;
        }
    }

    void SceneNode::addChild(std::shared_ptr<SceneNode> child) {
        children.push_back(child);
        child->setParent(shared_from_this());
        childrenOrderDirty = true;
    }

    void SceneNode::removeChild(std::shared_ptr<SceneNode> child) {
        children.erase(std::remove(children.begin(), children.end(), child), children.end());
        child->parent.reset();
        childrenOrderDirty = true;
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

    void SceneNode::setZIndex(int newZIndex) {
        if (zIndex != newZIndex) {
            zIndex = newZIndex;
            if (auto p = parent.lock()) {
                p->childrenOrderDirty = true;
            }
        }
    }

    int SceneNode::getZIndex() const {
        return zIndex;
    }

    void SceneNode::sortChildrenIfDirty() {
        if (childrenOrderDirty) {
            std::stable_sort(children.begin(), children.end(), [](const auto& a, const auto& b) {
                return a->zIndex < b->zIndex;
            });
            childrenOrderDirty = false;
        }
    }

    bool SceneNode::handleEvent(const DxvEvent& event) {
        sortChildrenIfDirty();
        // Iterate backwards, because higher z-index/later-added children are on top.
        for (auto it = children.rbegin(); it != children.rend(); ++it) {
            if ((*it)->handleEvent(event)) {
                return true; // Child consumed the event
            }
        }
        return false; // No child consumed the event
    }

    void SceneNode::updateLayout() {
        // Layout update does not depend on Z-order, so no sort is needed here.
        for (auto& child : children) {
            child->updateLayout();
        }
    }

    void SceneNode::draw(IRenderer& renderer) {
        sortChildrenIfDirty();
        for (auto& child : children) {
            child->draw(renderer);
        }
    }

}
