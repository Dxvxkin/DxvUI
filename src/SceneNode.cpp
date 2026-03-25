#include "DxvUI/Scene.h"

#include "DxvUI/Log.h"
#include "DxvUI/style/StyleResolver.h"
#include <utility>
#include <algorithm>
#include <string>

namespace DxvUI {

    int SceneNode::nodeCount = 0;

    // --- Logging Helpers ---
    static int get_depth(const SceneNode* node) {
        int depth = 0;
        if (node) {
            auto p = node->parent.lock();
            while (p) {
                depth++;
                p = p->parent.lock();
            }
        }
        return depth;
    }
    static std::string indent(const SceneNode* node) { return std::string(get_depth(node) * 2, ' '); }
    static std::string state_to_string(WidgetState state) {
        switch (state) {
            case WidgetState::Normal: return "Normal";
            case WidgetState::Hovered: return "Hovered";
            case WidgetState::Pressed: return "Pressed";
            case WidgetState::Disabled: return "Disabled";
        }
        return "Unknown";
    }

    SceneNode::SceneNode(std::string id) : id(std::move(id)) {
        nodeCount++;
    }

    SceneNode::~SceneNode() {
        nodeCount--;
    }

    int SceneNode::getNodeCount() {
        return nodeCount;
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

    void SceneNode::detachAllChildren()
    {
        auto childrenCopy = children;
        for (const auto& child : childrenCopy)
        {
            child->detachAllChildren();
        }
        detach();
    }

    void SceneNode::setScene(const std::shared_ptr<Scene>& newScene) {
        if (scene.lock() == newScene) return;

        if (const auto oldScene = scene.lock()) {
            oldScene->unregisterNode(shared_from_this());
        }

        scene = newScene;

        if (const auto currentScene = scene.lock()) {
            if (!currentScene->registerNode(shared_from_this())) {
                Log::error("{} Failed to register node '{}' to new scene", indent(this), id);
            }
        }

        markStyleDirty(); // When scene changes, styles need re-evaluation
        for (const auto& child : children) {
            child->setScene(newScene);
        }
    }

    std::shared_ptr<Scene> SceneNode::getScene() const { return scene.lock(); }
    const std::string& SceneNode::getId() const { return id; }

    void SceneNode::setId(const std::string& newId) {
        if (id == newId) return;

        if (const auto s = getScene()) {
            s->unregisterNode(shared_from_this());
        }

        id = newId;

        if (const auto s = getScene()) {
            s->registerNode(shared_from_this());
        }
    }

    const char* SceneNode::getNodeType() const {
        return "SceneNode";
    }

    std::shared_ptr<SceneNode> SceneNode::findNodeById(const std::string& searchId) const
    {
        if (auto s = getScene()) {
            return s->findNodeById(searchId);
        }
        return nullptr;
    }

    std::shared_ptr<SceneNode> SceneNode::findNodeAt(int x, int y) {
        if (!visible || !getGlobalBounds().contains(x, y)) {
            return nullptr;
        }
        sortChildrenIfDirty();
        for (auto it = children.rbegin(); it != children.rend(); ++it) {
            if (auto found = (*it)->findNodeAt(x, y)) {
                return found;
            }
        }
        return shared_from_this();
    }

    Style& SceneNode::editStyle() {
        markStyleDirty();
        return style;
    }

    const Style& SceneNode::getStyle() const {
        return style;
    }

    void SceneNode::markStyleDirty() {
        if (isStyleDirty) return;
        isStyleDirty = true;
        markLayoutDirty();
        for (auto& child : children) {
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

    Rect SceneNode::getGlobalBounds() const {
        if (isLayoutDirty) {
            if(auto s = scene.lock()) s->updateLayout();
        }
        auto it = layoutCache.find(getCurrentState());
        return it != layoutCache.end() ? it->second.computedBounds : Rect{};
    }
    Size SceneNode::getDesiredSize() const { return desiredSize; }

    WidgetState SceneNode::getCurrentState() const {
        if (isPressed) return WidgetState::Pressed;
        if (isHovered) return WidgetState::Hovered;
        return WidgetState::Normal;
    }

    bool SceneNode::isRoot() const {
        if (auto s = scene.lock()) {
            return s->getRoot().get() == this;
        }
        return false;
    }

    void SceneNode::setHovered(bool hovered) {
        if (isHovered != hovered) {
            isHovered = hovered;
            markStyleDirty();
        }
    }

    void SceneNode::setPressed(bool pressed) {
        if (isPressed != pressed) {
            isPressed = pressed;
            markStyleDirty();
        }
    }

    bool SceneNode::isVisible() const {
        return visible;
    }

    void SceneNode::setVisible(bool newVisible) {
        if (visible != newVisible) {
            visible = newVisible;
            markLayoutDirty();
        }
    }

    void SceneNode::setZIndex(int newZIndex) {
        if (zIndex != newZIndex) {
            zIndex = newZIndex;
            if (auto p = parent.lock()) p->childrenOrderDirty = true;
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
        if (!event.handled && parent.lock()) {
            parent.lock()->dispatchEvent(event);
        }
    }

    void SceneNode::onAttach() {}
    void SceneNode::onDetach() {}

    void SceneNode::onUpdate(float deltaTime) {
        if (!visible) return;
        for (const auto& child : children) {
            child->onUpdate(deltaTime);
        }
    }

    void SceneNode::recomputeStyles() {
        appearanceCache.clear();
        layoutCache.clear();

        for (int i = 0; i < 4; ++i) {
            WidgetState s = static_cast<WidgetState>(i);
            appearanceCache[s] = StyleResolver::resolveAppearance(*this, s);
            layoutCache[s] = StyleResolver::resolveLayout(*this, s);
        }
        isStyleDirty = false;
    }

    const ComputedAppearanceStyle& SceneNode::getComputedAppearance(WidgetState state) const {
        // This check is a safeguard. In a correct implementation, the Scene's updateLayout
        // should have already called recomputeStyles() before the draw phase.
        if (isStyleDirty) {
            Log::warn("getComputedAppearance called on dirty node '{}' during draw. Styles should be resolved before this.", id);
        }
        return appearanceCache.at(state);
    }

    const ComputedLayoutStyle& SceneNode::getComputedLayout(WidgetState state) const {
        // During the measure/arrange pass, it's expected that this might be called on a dirty node
        // before its own arrange() method has had a chance to clear the isLayoutDirty flag.
        // Therefore, we don't warn here.
        return layoutCache.at(state);
    }

    void SceneNode::sortChildrenIfDirty() {
        if (childrenOrderDirty) {
            std::stable_sort(children.begin(), children.end(), [](const auto& a, const auto& b) {
                return a->getZIndex() < b->getZIndex();
            });
            childrenOrderDirty = false;
        }
    }

    Size SceneNode::measure(const Size& availableSize) {
        if (!visible) {
            desiredSize = {0, 0};
            return desiredSize;
        }
        if (!isLayoutDirty && !isStyleDirty) {
            return desiredSize;
        }

        const auto& computedLayout = getComputedLayout(getCurrentState());
        const auto& padding = computedLayout.padding;

        float requiredWidth = 0.0f;
        float requiredHeight = 0.0f;

        for (const auto& child : children) {
            Size childDesiredSize = child->measure(availableSize);
            const auto& childLayout = child->getComputedLayout(child->getCurrentState());

            float childLeft = childLayout.left;
            float childTop = childLayout.top;
            float childWidth = childLayout.width > 0 ? childLayout.width : childDesiredSize.width;
            float childHeight = childLayout.height > 0 ? childLayout.height : childDesiredSize.height;

            requiredWidth = std::max(requiredWidth, childLeft + childWidth);
            requiredHeight = std::max(requiredHeight, childTop + childHeight);
        }

        desiredSize = { requiredWidth + padding.left + padding.right, requiredHeight + padding.top + padding.bottom };

        if (computedLayout.width > 0) desiredSize.width = computedLayout.width;
        if (computedLayout.height > 0) desiredSize.height = computedLayout.height;

        return desiredSize;
    }

    void SceneNode::arrange(const Rect& finalRect) {
        if (!visible) {
            if (layoutCache.count(getCurrentState())) {
                 auto& computedLayout = layoutCache[getCurrentState()];
                 computedLayout.computedBounds = {finalRect.x, finalRect.y, 0, 0};
            }
            return;
        }
        if (!isLayoutDirty && !isStyleDirty) {
            return;
        }

        auto& computedLayout = layoutCache[getCurrentState()];
        computedLayout.computedBounds = finalRect;

        const auto& padding = computedLayout.padding;
        Rect contentRect = {
            finalRect.x + (int)padding.left, finalRect.y + (int)padding.top,
            finalRect.width - (int)(padding.left + padding.right),
            finalRect.height - (int)(padding.top + padding.bottom)
        };

        for (const auto& child : children) {
            const auto& childLayout = child->getComputedLayout(child->getCurrentState());
            Size childDesiredSize = child->getDesiredSize();

            int childX = contentRect.x + (int)childLayout.left;
            int childY = contentRect.y + (int)childLayout.top;
            int childW = (int)(childLayout.width > 0 ? childLayout.width : childDesiredSize.width);
            int childH = (int)(childLayout.height > 0 ? childLayout.height : childDesiredSize.height);

            Rect childFinalRect = {childX, childY, childW, childH};
            child->arrange(childFinalRect);
        }

        isLayoutDirty = false;
    }

    void SceneNode::draw(IRenderer& renderer) {
        if (!visible) {
            return;
        }

        // This call is now just a safeguard. Styles should be clean.
        getComputedAppearance(getCurrentState());

        sortChildrenIfDirty();
        for (const auto& child : children) {
            child->draw(renderer);
        }
    }
}
