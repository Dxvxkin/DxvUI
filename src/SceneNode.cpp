#include "DxvUI/SceneNode.h"

#include <algorithm>
#include <string>
#include <utility>

#include "DxvUI/Log.h"
#include "DxvUI/Scene.h"
#include "DxvUI/UIContext.h"
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
    if (auto s = child->getScene()) {
        s->raise(EventType::Attach, child);
    }
    childrenOrderDirty = true;
    markLayoutDirty();
    // A new sibling can now cover the node under the cursor, so the event
    // manager's hit-test cache must not survive the structural change.
    if (auto s = scene.lock()) s->invalidateHitTestCache();
}

void SceneNode::removeChild(const std::shared_ptr<SceneNode>& child) {
    if (!child) return;
    auto it = std::remove_if(children.begin(), children.end(),
                             [&](const std::shared_ptr<SceneNode>& p) { return p == child; });
    if (it != children.end()) {
        child->onDetach();
        if (auto s = child->getScene()) {
            s->raise(EventType::Detach, child);
        }
        children.erase(it, children.end());
        child->parent.reset();
        child->setScene(nullptr);
        markLayoutDirty();
        // A removed sibling can uncover a node under the cursor, and the removed
        // node (or a descendant) may be the hovered/pressed/focused node: clear
        // that state so detached-but-alive nodes stop receiving events and
        // widgets do not stay stuck in Hovered/Pressed.
        if (auto s = scene.lock()) {
            s->onNodeRemoved(child);
            s->invalidateHitTestCache();
        }
    }
}

void SceneNode::detach() {
    if (auto p = parent.lock()) {
        p->removeChild(shared_from_this());
    }
}

void SceneNode::detachSubtree() {
    auto childrenCopy = children;
    for (const auto& child : childrenCopy) {
        child->detachSubtree();
    }
    detach();
}

void SceneNode::setScene(const std::shared_ptr<Scene>& newScene) {
    if (scene.lock() == newScene) return;

    scene = newScene;

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
    id = newId;
}

const char* SceneNode::getNodeType() const { return "SceneNode"; }

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

std::shared_ptr<SceneNode> SceneNode::findNodeAt(int x, int y) {
    if (!visible || !getGlobalBounds().contains(x, y)) {
        return nullptr;
    }
    // An opaque (hit-testable) node is an atomic target: it accepts the hit
    // itself and never recurses into its children, even though they may cover
    // the point visually. Used by composite leaf widgets (Button, Checkbox,
    // Slider...) that draw their own content and must be clicked as a unit.
    if (hitTestable_) {
        return shared_from_this();
    }
    sortChildrenIfDirty();
    for (auto it = children.rbegin(); it != children.rend(); ++it) {
        if (auto found = (*it)->findNodeAt(x, y)) {
            return found;
        }
    }
    return shared_from_this();
}

void SceneNode::setHitTestable(bool hitTestable) {
    if (hitTestable_ == hitTestable) {
        return;
    }
    hitTestable_ = hitTestable;
    // The hit-test cache may hold the result for a point that now (or no
    // longer) resolves to this node; force the next hit-test to rescan.
    if (auto sc = scene.lock()) {
        sc->invalidateHitTestCache();
    }
}

bool SceneNode::isHitTestable() const { return hitTestable_; }

bool SceneNode::hasNodeInFront(const Rect& bounds) {
    // Children are kept in draw order (later = drawn on top). A sibling drawn
    // in front of this node can cover the cursor without any relayout, so walk
    // the ancestor chain and report whether any visible sibling intersects the
    // given bounds. Rect-based because it is computed once per hit-test cache
    // rebuild instead of per event, where the sibling walk would dominate.
    const SceneNode* pathChild = this;
    for (auto ancestor = parent.lock(); ancestor; ancestor = ancestor->parent.lock()) {
        ancestor->sortChildrenIfDirty();
        const auto& kids = ancestor->children;
        for (auto it = kids.rbegin(); it != kids.rend(); ++it) {
            if ((*it).get() == pathChild) {
                break;
            }
            if ((*it)->isVisible() && (*it)->getGlobalBounds().intersects(bounds)) {
                return true;
            }
        }
        pathChild = ancestor.get();
    }
    return false;
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
        // fontSize/fontFamily are inherited, so a change re-measures this node
        // and every text-bearing descendant; a plain markLayoutDirty() would
        // leave clean child subtrees pruned out of the layout pass.
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

const LayoutData& SceneNode::getLayoutData() const { return layoutData; }

WidgetState SceneNode::getCurrentState() const {
    if (!isEnabled_) return WidgetState::Disabled;
    if (isPressed) return WidgetState::Pressed;
    if (isFocused) return WidgetState::Focused;
    if (isHovered) return WidgetState::Hovered;
    return WidgetState::Normal;
}

bool SceneNode::isRoot() const { return parent.expired(); }

bool SceneNode::isAncestorOf(const std::shared_ptr<SceneNode>& descendant) const {
    for (auto n = descendant ? descendant->parent.lock() : nullptr; n; n = n->parent.lock()) {
        if (n.get() == this) {
            return true;
        }
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

void SceneNode::setFocused(bool focused) {
    if (isFocused != focused) {
        isFocused = focused;
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

bool SceneNode::isEnabled() const { return isEnabled_; }

void SceneNode::setEnabled(bool enabled) {
    if (isEnabled_ == enabled) return;
    isEnabled_ = enabled;
    markLayoutDirty();
    if (!enabled) {
        // A disabled node (or its focused descendant) must stop holding
        // hover/press/focus and stop receiving interaction events right away.
        if (auto s = scene.lock()) {
            s->onNodeDisabled(shared_from_this());
        }
    }
}

void SceneNode::setZIndex(int newZIndex) {
    if (zIndex != newZIndex) {
        zIndex = newZIndex;
        if (auto p = parent.lock()) p->childrenOrderDirty = true;
        // A re-sorted sibling can now cover the node under the cursor.
        if (auto s = scene.lock()) s->invalidateHitTestCache();
    }
}

int SceneNode::getZIndex() const { return zIndex; }

std::unique_ptr<SceneNode::Connection> SceneNode::on(EventType type, ActionCallback callback) {
    const handlerID id = handlerIdCounter++;
    eventHandlers[type][id] = std::move(callback);
    return std::unique_ptr<Connection>(new Connection(weak_from_this(), type, id));
}

std::unique_ptr<SceneNode::Connection> SceneNode::onCapture(EventType type,
                                                            ActionCallback callback) {
    const handlerID id = handlerIdCounter++;
    captureHandlers[type][id] = std::move(callback);
    return std::unique_ptr<Connection>(new Connection(weak_from_this(), type, id));
}

SceneNode::Connection::Connection(std::weak_ptr<SceneNode> node, EventType type, handlerID id)
    : node(std::move(node)), type(type), id(id) {}

SceneNode::Connection::~Connection() {
    if (auto n = node.lock()) {
        n->removeHandler(type, id);
        n->removeCaptureHandler(type, id);
    }
}

void SceneNode::removeHandler(EventType type, handlerID id) {
    if (auto it = eventHandlers.find(type); it != eventHandlers.end()) {
        it->second.erase(id);
        if (it->second.empty()) {
            eventHandlers.erase(it);
        }
    }
}

void SceneNode::removeCaptureHandler(EventType type, handlerID id) {
    if (auto it = captureHandlers.find(type); it != captureHandlers.end()) {
        it->second.erase(id);
        if (it->second.empty()) {
            captureHandlers.erase(it);
        }
    }
}

// The compatibility entry point: dispatches the event to this node in the
// Target phase without walking the tree (the Scene/EventManager owns the walk).
// Used by application code that dispatches directly to a node.
void SceneNode::dispatchEvent(DxvEvent& event) {
    if (!event.getTarget()) {
        return;
    }
    dispatchEvent(event, EventPhase::Target);
}

void SceneNode::dispatchEvent(DxvEvent& event, EventPhase phase) {
    if (!event.getTarget()) {
        return;
    }
    // Snapshot the event type: a handler may mutate event.type, but that must
    // not change which handlers run here.
    const EventType eventType = event.type;
    event.currentTarget = weak_from_this();
    event.phase_ = phase;

    const UIContext ctx(getScene().get());

    // Capture-phase listeners run on the descent (only for captureable
    // events, which is the only case the EventManager walks into capture).
    if (phase == EventPhase::Capture) {
        auto captureIt = captureHandlers.find(eventType);
        if (captureIt != captureHandlers.end()) {
            runListeners(captureIt->second, event, eventType, ctx);
            if (event.isImmediatePropagationStopped()) {
                return;
            }
        }
        return;
    }

    // Regular listeners run in the Target and Bubble phases.
    auto handlerIt = eventHandlers.find(eventType);
    if (handlerIt != eventHandlers.end()) {
        runListeners(handlerIt->second, event, eventType, ctx);
    }

    // Default action runs only on the target, after the user listeners, and is
    // cancelled by preventDefault() (stopPropagation/stopImmediatePropagation
    // do not cancel it, per DOM semantics). Per the DOM UI Events model the
    // default action does not stop propagation. The type is restored first so
    // the hook always sees the originally raised event.
    if (phase == EventPhase::Target) {
        event.type = eventType;
        if (event.cancelable() && !event.isDefaultPrevented()) {
            onEvent(event);
        }
    }
}

void SceneNode::runListeners(std::map<handlerID, ActionCallback>& handlers, DxvEvent& event,
                             const EventType eventType, const UIContext& context) {
    // Snapshot only the handler ids, not the callbacks: std::map iterators stay
    // valid across insert/erase, but a handler may remove itself (or register
    // new ones) while running, so a live iteration is unsafe and a full copy of
    // the std::functions would allocate per dispatched event.
    std::vector<handlerID> ids;
    ids.reserve(handlers.size());
    for (const auto& [id, callback] : handlers) {
        ids.push_back(id);
    }
    for (const handlerID id : ids) {
        const auto callbackIt = handlers.find(id);
        if (callbackIt != handlers.end() && callbackIt->second) {
            callbackIt->second(event, context);
            // stopImmediatePropagation skips the remaining listeners of the
            // current node, but (DOM semantics) not the default action.
            if (event.isImmediatePropagationStopped()) {
                break;
            }
        }
    }
}

void SceneNode::onEvent(DxvEvent& /*event*/) {}

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

const ComputedAppearanceStyle& SceneNode::getComputedAppearance() const {
    return getComputedAppearance(getCurrentState());
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

const ComputedLayoutStyle& SceneNode::getComputedLayout() const {
    return getComputedLayout(getCurrentState());
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

    const bool clip = getComputedAppearance().clipContent;
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
    const auto& computedAppearance = getComputedAppearance();
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
    binding_ = binding;  // TODO: binding присваиваеться без проверки, возможно требует проверок
    if (binding_) {
        connection_ =
            binding_->subscribe([this](const UIBinding& value) { this->onBindingChange(value); });
    }
}

std::shared_ptr<UIBinding> SceneNode::getBinding() const { return binding_; }

void SceneNode::onBindingChange(const UIBinding& binding) {
    onChange(binding);
    // Change is raised through the scene so the event manager controls the
    // propagation. A node outside the scene (attached-after-bind or detached)
    // has no scene to route through; its Change is not delivered.
    if (auto s = scene.lock()) {
        s->raise(EventType::Change, shared_from_this());
    }
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
