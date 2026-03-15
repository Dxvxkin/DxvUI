#include "DxvUI/SceneNode.h"
#include <utility>

namespace DxvUI {

    SceneNode::SceneNode(std::string id) : id(std::move(id)) {}

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

    const std::string& SceneNode::getId() const {
        return id;
    }

    std::shared_ptr<SceneNode> SceneNode::findNodeById(const std::string& searchId) {
        if (id == searchId) {
            return shared_from_this();
        }
        for (const auto& child : children) {
            if (auto found = child->findNodeById(searchId)) {
                return found;
            }
        }
        return nullptr;
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

    Rect SceneNode::getGlobalBounds() const {
        int globalX, globalY;
        getGlobalPosition(globalX, globalY);
        return {globalX, globalY, width, height};
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
        for (auto it = children.rbegin(); it != children.rend(); ++it) {
            if ((*it)->handleEvent(event)) {
                return true;
            }
        }
        return false;
    }

    void SceneNode::updateLayout() {
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
