#include "DxvUI/SceneNode.h"

#include <algorithm>
#include <string>
#include <utility>

#include "DxvUI/Log.h"
#include "DxvUI/Scene.h"
#include "DxvUI/Utils.h"

namespace DxvUI {

int SceneNode::nodeCount = 0;

SceneNode::SceneNode(std::string id) : id(std::move(id)) { nodeCount++; }

SceneNode::~SceneNode() {
    nodeCount--;
    Log::trace("{} Destroying node {}", indent(this), id);
}

int SceneNode::getNodeCount() { return nodeCount; }

void SceneNode::addChild(const SharedPtr& child) {
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

void SceneNode::detachAllChildren() {
    auto childrenCopy = children;
    for (const auto& child : childrenCopy) {
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

    markStyleDirty();  // When scene changes, styles need re-evaluation
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

const char* SceneNode::getNodeType() const { return "SceneNode"; }

std::shared_ptr<SceneNode> SceneNode::findNodeById(const std::string& searchId) const {
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

const Style& SceneNode::getStyle() const { return style; }

std::uint64_t SceneNode::getStyleVersion() const noexcept { return style.getVersion(); }

void SceneNode::markStyleDirty() {
    if (style.isDirty()) return;
    style.markDirty();
    markLayoutDirty();
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
        if (auto s = scene.lock()) s->forceLayoutUpdate();
    }
    const auto* computedLayout = style.getComputedLayout(getCurrentState());
    return computedLayout ? computedLayout->computedBounds : Rect{};
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

bool SceneNode::isVisible() const { return visible; }

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

void SceneNode::on(EventType type, ActionCallback callback) {
    eventHandlers[type].push_back(std::move(callback));
}

void SceneNode::dispatchEvent(DxvEvent& event) {
    if (event.target.expired()) {
        return;
    }
    event.currentTarget = weak_from_this();

    if (eventHandlers.contains(event.type)) {
        for (const auto& callback : eventHandlers[event.type]) {
            callback(event);
            if (event.handled) {
                return;
            }
        }
    }

    if (!event.handled) {
        if (const auto p = parent.lock()) {
            p->dispatchEvent(event);
        }
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

const ComputedAppearanceStyle& SceneNode::getComputedAppearance(WidgetState state) const {
    if (const auto* computed = style.getComputedAppearance(state)) {
        return *computed;
    }
    Log::error(
        "FATAL: getComputedAppearance failed for node '{}' (state {}). Cache not populated "
        "before use. This indicates a severe logic error in the layout/style update cycle.",
        id, (int)state);
    static const ComputedAppearanceStyle empty{};
    return empty;
}

const ComputedLayoutStyle& SceneNode::getComputedLayout(WidgetState state) const {
    if (const auto* computed = style.getComputedLayout(state)) {
        return *computed;
    }
    Log::error(
        "FATAL: getComputedLayout failed for node '{}' (state {}). Cache not populated before "
        "use. This indicates a severe logic error in the layout/style update cycle.",
        id, (int)state);
    static constexpr ComputedLayoutStyle empty{};
    return empty;
}

void SceneNode::sortChildrenIfDirty() {
    if (childrenOrderDirty) {
        std::ranges::stable_sort(
            children, [](const auto& a, const auto& b) { return a->getZIndex() < b->getZIndex(); });
        childrenOrderDirty = false;
    }
}

Size SceneNode::measure(const Size& availableSize) {
    if (!isLayoutDirty) return desiredSize;

    if (!visible) {
        desiredSize = {0, 0};
        return desiredSize;
    }

    Size size = measureOverride(availableSize);
    applySizeConstraints(size);
    desiredSize = size;
    return desiredSize;
}

Size SceneNode::measureOverride(const Size& /*availableSize*/) { return {0, 0}; }

void SceneNode::applySizeConstraints(Size& size) const {
    const auto& computedLayout = getComputedLayout(getCurrentState());

    // An explicit size from the style wins over both the measured size and the
    // min/max constraints.
    if (computedLayout.width > 0) {
        size.width = computedLayout.width;
    } else {
        if (computedLayout.minWidth.has_value()) {
            size.width = std::max(size.width, computedLayout.minWidth.value());
        }
        if (computedLayout.maxWidth.has_value()) {
            size.width = std::min(size.width, computedLayout.maxWidth.value());
        }
    }

    if (computedLayout.height > 0) {
        size.height = computedLayout.height;
    } else {
        if (computedLayout.minHeight.has_value()) {
            size.height = std::max(size.height, computedLayout.minHeight.value());
        }
        if (computedLayout.maxHeight.has_value()) {
            size.height = std::min(size.height, computedLayout.maxHeight.value());
        }
    }
}

void SceneNode::arrange(const Rect& finalRect) {
    if (!visible) {
        style.setComputedBounds(getCurrentState(), {finalRect.x, finalRect.y, 0, 0});
    } else {
        style.setComputedBounds(getCurrentState(), finalRect);
    }

    isLayoutDirty = false;
}

void SceneNode::draw(IRenderer& renderer) {
    if (!visible) {
        return;
    }

    sortChildrenIfDirty();
    for (const auto& child : children) {
        child->draw(renderer);
    }
}

void SceneNode::bind(const std::shared_ptr<UIBinding>& binding) {
    connection_.reset();
    binding_ = binding;
    if (binding_) {
        connection_ = binding_->subscribe(
            [this](const UIBinding& value) { this->onBindingChange(std::move(value)); });
    }
}

std::shared_ptr<UIBinding> SceneNode::getBinding() const { return binding_; }

void SceneNode::onBindingChange(const UIBinding& binding) {
    DxvEvent event;
    event.target = weak_from_this();
    event.type = EventType::Change;
    onChange(std::move(binding));
    dispatchEvent(event);
}

std::size_t SceneNode::getDepth() const noexcept {
    std::size_t depth = 0;
    for (auto p = parent.lock(); p != nullptr; p = p->parent.lock()) {
        depth++;
    }
    return depth;
}

void SceneNode::onChange(const UIBinding& binding) {}
}  // namespace DxvUI
