#include "DxvUI/SceneNode.h"

#include <algorithm>
#include <string>
#include <utility>

#include "DxvUI/Log.h"
#include "DxvUI/Scene.h"
#include "DxvUI/Utils.h"
#include "DxvUI/layout/LayoutManager.h"

namespace DxvUI {

int SceneNode::nodeCount = 0;

SceneNode::SceneNode(std::string id) : id(std::move(id)) { nodeCount++; }

SceneNode::~SceneNode() {
    nodeCount--;
    Log::trace("{} Destroying node {}", indent(this), id);
}

int SceneNode::getNodeCount() { return nodeCount; }

void SceneNode::addChild(const std::shared_ptr<SceneNode>& child) {
    if (!child) return;
    child->detach();
    children.push_back(child);
    child->parent = shared_from_this();
    child->setScene(this->getScene());
    // The child may be freshly constructed (its style is dirty from birth) or
    // be added to a scene-less tree where setScene() early-returns. Either way
    // its dirty flag must propagate up so the StyleManager finds it.
    child->markStyleDirty();
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
    markLayoutDirty();
    for (const auto& child : children) {
        child->setScene(newScene);
    }
}

std::shared_ptr<Scene> SceneNode::getScene() const { return scene.lock(); }
std::weak_ptr<SceneNode> SceneNode::getParent() const { return parent; }
const std::vector<std::shared_ptr<SceneNode>>& SceneNode::getChildren() const { return children; }
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

void SceneNode::setStyle(const StyleRule& rule, WidgetState state) {
    const StyleRule* old = style.get(state);
    if (old && *old == rule) return;  // No-op write: nothing to invalidate.
    const bool layoutChanged =
        old ? detail::layoutPropsDiffer(*old, rule) : detail::hasLayoutProps(rule);
    const bool textMetricsChanged =
        old ? detail::textMetricsPropsDiffer(*old, rule) : detail::hasTextMetricsProps(rule);
    style.set(rule, state);
    markStyleDirty();
    if (textMetricsChanged) {
        // fontSize/fontPath are inherited, so a change re-measures this node and
        // every text-bearing descendant; a plain markLayoutDirty() would leave
        // clean child subtrees pruned out of the layout pass.
        markLayoutDirtyRecursive();
    } else if (layoutChanged) {
        markLayoutDirty();
    }
}

void SceneNode::updateStyle(const StyleRule& updates, WidgetState state) {
    const StyleRule* old = style.get(state);
    if (!old) {
        // No existing rule for the state: creating one is equivalent to a set.
        setStyle(updates, state);
        return;
    }
    StyleRule merged = *old;
    merged.merge(updates);
    if (merged == *old) return;  // No-op merge.
    const bool layoutChanged = detail::layoutPropsDiffer(*old, merged);
    const bool textMetricsChanged = detail::textMetricsPropsDiffer(*old, merged);
    style.update(updates, state);
    markStyleDirty();
    if (textMetricsChanged) {
        markLayoutDirtyRecursive();
    } else if (layoutChanged) {
        markLayoutDirty();
    }
}

const Style& SceneNode::getStyle() const { return style; }

void SceneNode::markStyleDirty() {
    style.markDirty();
    markStyleSubtreeDirty();
}

void SceneNode::markStyleSubtreeDirty() {
    // Always walk all the way up to the root. An early stop on an already-marked
    // node is only valid while every ancestor of a marked node is marked too,
    // but that invariant is broken whenever the parent chain changes: a node can
    // be styled while it has no parent yet (setStyle() on a freshly constructed
    // node) or a dirty subtree can be detached and re-attached elsewhere. In both
    // cases the node's flag is already set while its new ancestors are clean, so
    // an early stop would leave the root unflagged and the StyleManager's prune
    // pass would skip the subtree entirely.
    for (SceneNode* n = this; n != nullptr; n = n->parent.lock().get()) {
        n->style.markSubtreeDirty();
    }
}

void SceneNode::markLayoutDirty() {
    // Like markStyleSubtreeDirty, always walk all the way up to the root. An
    // early stop on an already-marked node is only valid while every ancestor
    // of a marked node is marked too, but that invariant is broken whenever the
    // parent chain changes: a dirty subtree can be detached and re-attached
    // elsewhere, leaving its new ancestors unflagged.
    for (SceneNode* n = this; n != nullptr; n = n->parent.lock().get()) {
        n->layoutData.isSubtreeDirty = true;
    }
    layoutData.isDirty = true;
}

void SceneNode::markLayoutDirtyRecursive() {
    markLayoutDirty();
    for (const auto& child : children) {
        child->markLayoutDirtyRecursive();
    }
}

void SceneNode::markStyleDirtyRecursive() {
    markStyleDirty();
    for (const auto& child : children) {
        child->markStyleDirtyRecursive();
    }
}

Rect SceneNode::getGlobalBounds() const { return layoutData.bounds; }

Size SceneNode::getDesiredSize() const { return layoutData.desiredSize; }

bool SceneNode::isLayoutDirty() const { return layoutData.isDirty || layoutData.isSubtreeDirty; }

const Size& SceneNode::getLastMeasureConstraints() const {
    return layoutData.lastMeasureConstraints;
}

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
    return LayoutManager::measureNode(*this, availableSize);
}

Size SceneNode::onMeasure(const Size& /*availableSize*/) { return {0, 0}; }

void SceneNode::arrange(const Rect& finalRect) { LayoutManager::arrangeNode(*this, finalRect); }

void SceneNode::onArrange(const Rect& /*finalRect*/) {}

void SceneNode::draw(IRenderer& renderer) {
    const Size viewportSize = renderer.getViewportSize();
    drawImpl(renderer,
             {0, 0, static_cast<int>(viewportSize.width), static_cast<int>(viewportSize.height)});
}

void SceneNode::drawImpl(IRenderer& renderer, const Rect& viewportRect) {
    if (!visible) {
        return;
    }

    // Viewport culling: skip everything (background, content and children)
    // once the node is fully outside the visible area. A descendant could
    // theoretically overflow back on screen (absolute anchoring/overflow), but
    // only while its parent is entirely off-screen, which is the rare case.
    if (!getGlobalBounds().intersects(viewportRect)) {
        return;
    }

    drawBackground(renderer);

    const bool clip = getComputedAppearance(getCurrentState()).clipContent;
    if (clip) {
        renderer.pushClipRect(getGlobalBounds());
    }

    drawContent(renderer);

    sortChildrenIfDirty();
    for (const auto& child : children) {
        child->drawImpl(renderer, viewportRect);
    }

    if (clip) {
        renderer.popClipRect();
    }
}

void SceneNode::drawBackground(IRenderer& renderer) {
    const auto& computedAppearance = getComputedAppearance(getCurrentState());
    if (computedAppearance.backgroundColor.a == 0 && computedAppearance.borderThickness <= 0) {
        return;
    }

    renderer.fillRoundRect(
        getGlobalBounds(), computedAppearance.borderRadius, computedAppearance.backgroundColor,
        {.color = computedAppearance.borderColor, .thickness = computedAppearance.borderThickness});
}

void SceneNode::drawContent(IRenderer& /*renderer*/) {}

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
