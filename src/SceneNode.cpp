#include "DxvUI/SceneNode.h"
#include "DxvUI/Scene.h"
#include "DxvUI/Colors.h"
#include "DxvUI/Log.h"
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

    // A static, default-constructed style to act as a final fallback for the root node.
    static const ComputedAppearanceStyle defaultAppearance = {
        .backgroundColor = Colors::Transparent, .textColor = Colors::Black, .borderColor = Colors::Transparent,
        .borderThickness = 0, .borderRadius = 0, .cursor = CursorType::Arrow,
        .fontSize = 14, .fontPath = getDefaultFontPath()
    };
    static const ComputedLayoutStyle defaultLayout = {
        .left = 0, .top = 0, .width = 0, .height = 0,
        .padding = {}, .margin = {},
        .horizontalAlignment = Alignment::Start, .verticalAlignment = Alignment::Start,
        .computedBounds = {0,0,0,0}
    };

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

    Style& SceneNode::editStyle() {
        markStyleDirty();
        markLayoutDirty();
        return style;
    }

    void SceneNode::markStyleDirty() {
        if (isStyleDirty) return;
        Log::trace("{}Marking Style Dirty for '{}'", indent(this), id);
        isStyleDirty = true;
        appearanceCache.clear();
        for (const auto& child : children) {
            child->markStyleDirty();
        }
    }

    void SceneNode::markLayoutDirty() {
        if (isLayoutDirty) return;
        Log::trace("{}Marking Layout Dirty for '{}'", indent(this), id);
        isLayoutDirty = true;
        layoutCache.clear();
        if (auto p = parent.lock()) {
            p->markLayoutDirty();
        } else {
            if (auto s = getScene()) s->requestLayoutUpdate();
        }
    }

    Rect SceneNode::getGlobalBounds() const {
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
            markLayoutDirty();
        }
    }

    void SceneNode::setPressed(bool pressed) {
        if (isPressed != pressed) {
            isPressed = pressed;
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
        if (!event.handled && parent.lock()) parent.lock()->dispatchEvent(event);
    }

    void SceneNode::onAttach() { /* Base implementation is empty */ }
    void SceneNode::onDetach() { /* Base implementation is empty */ }

    void SceneNode::onUpdate(float deltaTime) {
        for (const auto& child : children) {
            child->onUpdate(deltaTime);
        }
    }

    const ComputedAppearanceStyle& SceneNode::getComputedAppearance(WidgetState state) {
        if (appearanceCache.find(state) == appearanceCache.end()) {
            resolveAppearance(state);
        }
        return appearanceCache.at(state);
    }

    const ComputedLayoutStyle& SceneNode::getComputedLayout(WidgetState state) {
        if (layoutCache.find(state) == layoutCache.end()) {
            resolveLayout(state);
        }
        return layoutCache.at(state);
    }

    void SceneNode::resolveAppearance(WidgetState state) {
        Log::trace("{}Resolving appearance for state '{}' on node '{}'", indent(this), state_to_string(state), id);
        if (state != WidgetState::Normal && appearanceCache.find(WidgetState::Normal) == appearanceCache.end()) {
            resolveAppearance(WidgetState::Normal);
        }

        const ComputedAppearanceStyle& fallback = (state == WidgetState::Normal)
            ? (parent.lock() ? parent.lock()->getComputedAppearance(WidgetState::Normal) : defaultAppearance)
            : appearanceCache.at(WidgetState::Normal);

        const StyleRule* specificRule = style.get(state);
        const StyleRule* normalRule = style.get(WidgetState::Normal);

        auto& cache = appearanceCache[state];
        cache.backgroundColor = specificRule && specificRule->backgroundColor.has_value() ? specificRule->backgroundColor.value() : (normalRule && normalRule->backgroundColor.has_value() ? normalRule->backgroundColor.value() : fallback.backgroundColor);
        cache.textColor = specificRule && specificRule->textColor.has_value() ? specificRule->textColor.value() : (normalRule && normalRule->textColor.has_value() ? normalRule->textColor.value() : fallback.textColor);
        cache.borderColor = specificRule && specificRule->borderColor.has_value() ? specificRule->borderColor.value() : (normalRule && normalRule->borderColor.has_value() ? normalRule->borderColor.value() : fallback.borderColor);
        cache.borderThickness = specificRule && specificRule->borderThickness.has_value() ? specificRule->borderThickness.value() : (normalRule && normalRule->borderThickness.has_value() ? normalRule->borderThickness.value() : fallback.borderThickness);
        cache.borderRadius = specificRule && specificRule->borderRadius.has_value() ? specificRule->borderRadius.value() : (normalRule && normalRule->borderRadius.has_value() ? normalRule->borderRadius.value() : fallback.borderRadius);
        cache.cursor = specificRule && specificRule->cursor.has_value() ? specificRule->cursor.value() : (normalRule && normalRule->cursor.has_value() ? normalRule->cursor.value() : fallback.cursor);
        cache.fontSize = specificRule && specificRule->fontSize.has_value() ? specificRule->fontSize.value() : (normalRule && normalRule->fontSize.has_value() ? normalRule->fontSize.value() : fallback.fontSize);
        cache.fontPath = specificRule && specificRule->fontPath.has_value() ? specificRule->fontPath.value() : (normalRule && normalRule->fontPath.has_value() ? normalRule->fontPath.value() : fallback.fontPath);
    }

    void SceneNode::resolveLayout(WidgetState state) {
        if (state != WidgetState::Normal && layoutCache.find(WidgetState::Normal) == layoutCache.end()) {
            resolveLayout(WidgetState::Normal);
        }

        const ComputedLayoutStyle& fallback = (state == WidgetState::Normal)
            ? defaultLayout
            : layoutCache.at(WidgetState::Normal);

        const StyleRule* specificRule = style.get(state);
        const StyleRule* normalRule = style.get(WidgetState::Normal);

        auto& cache = layoutCache[state];
        cache.left = specificRule && specificRule->left.has_value() ? specificRule->left.value() : (normalRule && normalRule->left.has_value() ? normalRule->left.value() : fallback.left);
        cache.top = specificRule && specificRule->top.has_value() ? specificRule->top.value() : (normalRule && normalRule->top.has_value() ? normalRule->top.value() : fallback.top);
        cache.width = specificRule && specificRule->width.has_value() ? specificRule->width.value() : (normalRule && normalRule->width.has_value() ? normalRule->width.value() : fallback.width);
        cache.height = specificRule && specificRule->height.has_value() ? specificRule->height.value() : (normalRule && normalRule->height.has_value() ? normalRule->height.value() : fallback.height);
        cache.padding = specificRule && specificRule->padding.has_value() ? specificRule->padding.value() : (normalRule && normalRule->padding.has_value() ? normalRule->padding.value() : fallback.padding);
        cache.margin = specificRule && specificRule->margin.has_value() ? specificRule->margin.value() : (normalRule && normalRule->margin.has_value() ? normalRule->margin.value() : fallback.margin);
        cache.horizontalAlignment = specificRule && specificRule->horizontalAlignment.has_value() ? specificRule->horizontalAlignment.value() : (normalRule && normalRule->horizontalAlignment.has_value() ? normalRule->horizontalAlignment.value() : fallback.horizontalAlignment);
        cache.verticalAlignment = specificRule && specificRule->verticalAlignment.has_value() ? specificRule->verticalAlignment.value() : (normalRule && normalRule->verticalAlignment.has_value() ? normalRule->verticalAlignment.value() : fallback.verticalAlignment);
    }

    Size SceneNode::measure(const Size& availableSize) {
        Log::trace("{}Measuring '{}' | available: ({}, {})", indent(this), id, availableSize.width, availableSize.height);
        if (!isLayoutDirty) {
            Log::trace("{} > Skipping, not dirty. Returning cached: ({}, {})", indent(this), desiredSize.width, desiredSize.height);
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

        Log::trace("{} > Computed desired size: ({}, {})", indent(this), desiredSize.width, desiredSize.height);
        return desiredSize;
    }

    void SceneNode::arrange(const Rect& finalRect) {
        Log::trace("{}Arranging '{}' | finalRect: ({}, {}, {}, {})", indent(this), id, finalRect.x, finalRect.y, finalRect.width, finalRect.height);
        if (!isLayoutDirty) {
            Log::trace("{} > Skipping, not dirty.", indent(this));
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
        if (isStyleDirty) {
            isStyleDirty = false;
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
