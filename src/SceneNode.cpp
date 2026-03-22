#include "DxvUI/SceneNode.h"
#include "DxvUI/Scene.h"
#include "DxvUI/Colors.h"
#include <utility>
#include <algorithm>
#include <iostream>
#include <limits>

namespace DxvUI {

    SceneNode::SceneNode(std::string id) : id(std::move(id)) {
        computedAppearance.backgroundColor = Colors::Transparent;
        computedAppearance.textColor = Colors::Black;
        computedAppearance.borderColor = Colors::Transparent;
        computedAppearance.borderThickness = 0;
        computedAppearance.borderRadius = 0;
        computedAppearance.fontSize = 14;
        computedAppearance.fontPath = getDefaultFontPath();
        computedAppearance.cursor = CursorType::Arrow;
    }

    void SceneNode::addChild(const std::shared_ptr<SceneNode>& child) {
        if (!child) return;
        child->detach();
        children.push_back(child);
        child->parent = shared_from_this();
        child->setScene(this->getScene());
        child->onAttach();
        DxvEvent event;
        event.type = EventType::Attach;
        event.target = child;
        child->dispatchEvent(event);
        childrenOrderDirty = true;
        markLayoutDirty();
    }

    void SceneNode::removeChild(const std::shared_ptr<SceneNode>& child) {
        if (!child) return;
        auto it = std::remove_if(children.begin(), children.end(),
                                 [&](const std::shared_ptr<SceneNode>& p) { return p == child; });
        if (it != children.end()) {
            child->onDetach();
            DxvEvent event;
            event.type = EventType::Detach;
            event.target = child;
            child->dispatchEvent(event);
            children.erase(it, children.end());
            child->parent.reset();
            child->setScene(nullptr);
            markLayoutDirty();
        }
    }

    void SceneNode::detach() {
        if (auto p = parent.lock()) {
            p->removeChild(shared_from_this());
        }
    }

    void SceneNode::setScene(const std::shared_ptr<Scene>& newScene) {
        if (scene.lock() == newScene) return;
        scene = newScene;
        markLayoutDirty();
        for (const auto& child : children) {
            child->setScene(newScene);
        }
    }

    std::shared_ptr<Scene> SceneNode::getScene() const { return scene.lock(); }
    const std::string& SceneNode::getId() const { return id; }
    void SceneNode::setId(const std::string& newId) { id = newId; }

    std::shared_ptr<SceneNode> SceneNode::findNodeById(const std::string& searchId) {
        if (!searchId.empty() && id == searchId) return shared_from_this();
        for (const auto& child : children) {
            if (auto found = child->findNodeById(searchId)) return found;
        }
        return nullptr;
    }

    std::shared_ptr<SceneNode> SceneNode::findNodeAt(int x, int y) {
        if (!getGlobalBounds().contains(x, y)) return nullptr;
        sortChildrenIfDirty();
        for (auto it = children.rbegin(); it != children.rend(); ++it) {
            if (auto found = (*it)->findNodeAt(x, y)) return found;
        }
        return shared_from_this();
    }

    Appearance& SceneNode::getAppearance() {
        markStyleDirty();
        return appearance;
    }

    LayoutProperties& SceneNode::getLayout() {
        markLayoutDirty();
        return layoutProperties;
    }

    const LayoutProperties& SceneNode::getLayout() const {
        return layoutProperties;
    }

    void SceneNode::markStyleDirty() {
        if (isStyleDirty) return;
        isStyleDirty = true;
        for (const auto& child : children) {
            child->markStyleDirty();
        }
    }

    void SceneNode::markLayoutDirty() {
        if (isLayoutDirty) return;
        isLayoutDirty = true;
        if (auto p = parent.lock()) {
            p->markLayoutDirty();
        } else {
            if (auto s = getScene()) s->requestLayoutUpdate();
        }
    }

    void SceneNode::setPosition(float x, float y) {
        getLayout().left = x;
        getLayout().top = y;
    }

    void SceneNode::setSize(float width, float height) {
        getLayout().width = width;
        getLayout().height = height;
    }

    Rect SceneNode::getGlobalBounds() const { return layoutProperties.computedBounds; }
    Size SceneNode::getDesiredSize() const { return desiredSize; }
    void SceneNode::setZIndex(int newZIndex) {
        if (zIndex != newZIndex) {
            zIndex = newZIndex;
            if (auto p = parent.lock()) p->childrenOrderDirty = true;
            markLayoutDirty();
        }
    }
    int SceneNode::getZIndex() const { return zIndex; }

    void SceneNode::on(EventType type, ActionCallback callback) { eventHandlers[type].push_back(std::move(callback)); }

    void SceneNode::dispatchEvent(DxvEvent& event) {
        if (eventHandlers.count(event.type)) {
            for (const auto& callback : eventHandlers[event.type]) {
                if (auto targetNode = event.target.lock()) callback(event);
                if (event.handled) return;
            }
        }
        if (!event.handled && parent.lock()) parent.lock()->dispatchEvent(event);
    }

    void SceneNode::onAttach() { /* Base implementation is empty */ }
    void SceneNode::onDetach() { /* Base implementation is empty */ }

    void SceneNode::onUpdate(float deltaTime) {
        for (const auto& child : children) {
            child->onUpdate(deltaTime);
        }
    }

    const ComputedAppearance& SceneNode::getComputedAppearance() {
        if (isStyleDirty) {
            resolveAndCacheStyles();
        }
        return computedAppearance;
    }

    void SceneNode::resolveAndCacheStyles() {
        const auto& parentAppearance = parent.lock() ? parent.lock()->getComputedAppearance() : computedAppearance;
        computedAppearance.backgroundColor = appearance.backgroundColor.value_or(parentAppearance.backgroundColor);
        computedAppearance.textColor = appearance.textColor.value_or(parentAppearance.textColor);
        computedAppearance.borderColor = appearance.borderColor.value_or(parentAppearance.borderColor);
        computedAppearance.borderThickness = appearance.borderThickness.value_or(parentAppearance.borderThickness);
        computedAppearance.borderRadius = appearance.borderRadius.value_or(parentAppearance.borderRadius);
        computedAppearance.fontSize = appearance.fontSize.value_or(parentAppearance.fontSize);
        computedAppearance.fontPath = appearance.fontPath.value_or(parentAppearance.fontPath);
        computedAppearance.cursor = appearance.cursor.value_or(parentAppearance.cursor);
        isStyleDirty = false;
    }

    Size SceneNode::measure(const Size& availableSize) {
        if (!isLayoutDirty) return desiredSize;

        const auto& padding = layoutProperties.padding;
        Size contentAvailableSize = {
            availableSize.width - (padding.left + padding.right),
            availableSize.height - (padding.top + padding.bottom)
        };

        float requiredWidth = 0.0f;
        float requiredHeight = 0.0f;

        for (const auto& child : children) {
            Size childDesiredSize = child->measure(contentAvailableSize);
            const auto& childLayout = child->getLayout();

            float childLeft = childLayout.left.value_or(0);
            float childTop = childLayout.top.value_or(0);
            float childWidth = childLayout.width.value_or(childDesiredSize.width);
            float childHeight = childLayout.height.value_or(childDesiredSize.height);

            requiredWidth = std::max(requiredWidth, childLeft + childWidth);
            requiredHeight = std::max(requiredHeight, childTop + childHeight);
        }

        desiredSize = {
            requiredWidth + padding.left + padding.right,
            requiredHeight + padding.top + padding.bottom
        };

        if (layoutProperties.width.has_value()) {
            desiredSize.width = layoutProperties.width.value();
        }
        if (layoutProperties.height.has_value()) {
            desiredSize.height = layoutProperties.height.value();
        }
        return desiredSize;
    }

    void SceneNode::arrange(const Rect& finalRect) {
        if (!isLayoutDirty) return;

        layoutProperties.computedBounds = finalRect;

        const auto& padding = layoutProperties.padding;
        Rect contentRect = {
            finalRect.x + static_cast<int>(padding.left),
            finalRect.y + static_cast<int>(padding.top),
            finalRect.width - static_cast<int>(padding.left + padding.right),
            finalRect.height - static_cast<int>(padding.top + padding.bottom)
        };

        for (const auto& child : children) {
            const auto& childLayout = child->getLayout();
            Size childDesiredSize = child->getDesiredSize();

            int childX = contentRect.x + static_cast<int>(childLayout.left.value_or(0));
            int childY = contentRect.y + static_cast<int>(childLayout.top.value_or(0));
            int childW = static_cast<int>(childLayout.width.value_or(childDesiredSize.width));
            int childH = static_cast<int>(childLayout.height.value_or(childDesiredSize.height));

            child->arrange({childX, childY, childW, childH});
        }

        isLayoutDirty = false;
    }

    void SceneNode::draw(IRenderer& renderer) {
        if (isStyleDirty) {
            resolveAndCacheStyles();
        }

        sortChildrenIfDirty();
        for (const auto& child : children) {
            child->draw(renderer);
        }
    }

    void SceneNode::sortChildrenIfDirty() {
        if (childrenOrderDirty) {
            std::stable_sort(children.begin(), children.end(), [](const auto& a, const auto& b) {
                return a->getZIndex() < b->getZIndex();
            });
            childrenOrderDirty = false;
        }
    }
}
