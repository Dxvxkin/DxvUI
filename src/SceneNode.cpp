#include "DxvUI/SceneNode.h"

#include <algorithm>
#include <chrono>
#include <string>
#include <utility>

#include "DxvUI/Log.h"
#include "DxvUI/Scene.h"
#include "DxvUI/UIContext.h"
#include "DxvUI/Utils.h"
#include "DxvUI/interfaces/IRenderer.h"
#include "DxvUI/layout/LayoutManager.h"
#include "backend/CanvasAdapter.h"

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
    child->markStyleDirty();
    child->onAttach();
    if (auto s = child->getScene()) {
        s->raise(EventType::Attach, child);
    }
    childrenOrderDirty = true;
    markLayoutDirty();
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

    markStyleDirty();
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
    if (!state_.test(NodeState::Flag::Visible) || !getGlobalBounds().contains(x, y)) {
        return nullptr;
    }
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
    if (auto sc = scene.lock()) {
        sc->invalidateHitTestCache();
    }
}

bool SceneNode::isHitTestable() const { return hitTestable_; }

bool SceneNode::hasNodeInFront(const Rect& bounds) {
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
    if (old && *old == rule) return;
    const bool layoutChanged =
        old ? detail::layoutPropsDiffer(*old, rule) : detail::hasLayoutProps(rule);
    const bool textMetricsChanged =
        old ? detail::textMetricsPropsDiffer(*old, rule) : detail::hasTextMetricsProps(rule);
    style.set(rule, state);
    markStyleDirty();
    if (textMetricsChanged) {
        markLayoutDirtyRecursive();
    } else if (layoutChanged) {
        markLayoutDirty();
    }
}

void SceneNode::updateStyle(const StyleRule& updates, WidgetState state) {
    const StyleRule* old = style.get(state);
    if (!old) {
        setStyle(updates, state);
        return;
    }
    StyleRule merged = *old;
    merged.merge(updates);
    if (merged == *old) return;
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
    for (SceneNode* n = this; n != nullptr; n = n->parent.lock().get()) {
        n->style.markSubtreeDirty();
    }
}

void SceneNode::markLayoutDirty() {
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
    if (!state_.test(NodeState::Flag::Enabled)) return WidgetState::Disabled;
    if (state_.test(NodeState::Flag::Pressed)) return WidgetState::Pressed;
    if (state_.test(NodeState::Flag::Focused)) return WidgetState::Focused;
    if (state_.test(NodeState::Flag::Hovered)) return WidgetState::Hovered;
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
    // Stage 4: hover/press/focus should not cause relayout when only appearance changes.
    // We mark style dirty always, and only mark layout dirty if computed layout for old vs new state differs.
    WidgetState oldState = getCurrentState();
    const ComputedLayoutStyle* oldLayoutPtr = style.getComputedLayout(oldState);
    ComputedLayoutStyle oldLayout = oldLayoutPtr ? *oldLayoutPtr : ComputedLayoutStyle{};
    bool hadOldLayout = oldLayoutPtr != nullptr;

    if (state_.take(NodeState::Flag::Hovered, hovered)) {
        markStyleDirty();
        WidgetState newState = getCurrentState();
        const ComputedLayoutStyle* newLayoutOldPtr = style.getComputedLayout(newState);
        if (hadOldLayout && newLayoutOldPtr) {
            if (oldLayout != *newLayoutOldPtr) {
                // If text metrics (fontSize/family) changed, need recursive
                const auto* oldApp = style.getComputedAppearance(oldState);
                const auto* newAppOld = style.getComputedAppearance(newState);
                bool textMetricsChanged = false;
                if (oldApp && newAppOld) {
                    textMetricsChanged = (oldApp->fontSize != newAppOld->fontSize ||
                                          oldApp->fontFamily != newAppOld->fontFamily);
                }
                if (textMetricsChanged) markLayoutDirtyRecursive();
                else markLayoutDirty();
            }
        } else {
            // Fallback: check if Hovered state's own style has layout props
            const StyleRule* rule = style.get(WidgetState::Hovered);
            if (rule && (detail::hasLayoutProps(*rule) || detail::hasTextMetricsProps(*rule))) {
                if (detail::hasTextMetricsProps(*rule)) markLayoutDirtyRecursive();
                else markLayoutDirty();
            } else {
                // Also check old state's rule when leaving
                const StyleRule* oldRule = style.get(oldState);
                if (oldRule && (detail::hasLayoutProps(*oldRule) || detail::hasTextMetricsProps(*oldRule))) {
                    if (detail::hasTextMetricsProps(*oldRule)) markLayoutDirtyRecursive();
                    else markLayoutDirty();
                }
            }
        }
    }
}

void SceneNode::setPressed(bool pressed) {
    WidgetState oldState = getCurrentState();
    const ComputedLayoutStyle* oldLayoutPtr = style.getComputedLayout(oldState);
    ComputedLayoutStyle oldLayout = oldLayoutPtr ? *oldLayoutPtr : ComputedLayoutStyle{};
    bool hadOldLayout = oldLayoutPtr != nullptr;

    if (state_.take(NodeState::Flag::Pressed, pressed)) {
        markStyleDirty();
        WidgetState newState = getCurrentState();
        const ComputedLayoutStyle* newLayoutOldPtr = style.getComputedLayout(newState);
        if (hadOldLayout && newLayoutOldPtr) {
            if (oldLayout != *newLayoutOldPtr) {
                const auto* oldApp = style.getComputedAppearance(oldState);
                const auto* newAppOld = style.getComputedAppearance(newState);
                bool textMetricsChanged = false;
                if (oldApp && newAppOld) {
                    textMetricsChanged = (oldApp->fontSize != newAppOld->fontSize ||
                                          oldApp->fontFamily != newAppOld->fontFamily);
                }
                if (textMetricsChanged) markLayoutDirtyRecursive();
                else markLayoutDirty();
            }
        } else {
            const StyleRule* rule = style.get(WidgetState::Pressed);
            if (rule && (detail::hasLayoutProps(*rule) || detail::hasTextMetricsProps(*rule))) {
                if (detail::hasTextMetricsProps(*rule)) markLayoutDirtyRecursive();
                else markLayoutDirty();
            } else {
                const StyleRule* oldRule = style.get(oldState);
                if (oldRule && (detail::hasLayoutProps(*oldRule) || detail::hasTextMetricsProps(*oldRule))) {
                    if (detail::hasTextMetricsProps(*oldRule)) markLayoutDirtyRecursive();
                    else markLayoutDirty();
                }
            }
        }
    }
}

void SceneNode::setFocused(bool focused) {
    WidgetState oldState = getCurrentState();
    const ComputedLayoutStyle* oldLayoutPtr = style.getComputedLayout(oldState);
    ComputedLayoutStyle oldLayout = oldLayoutPtr ? *oldLayoutPtr : ComputedLayoutStyle{};
    bool hadOldLayout = oldLayoutPtr != nullptr;

    if (state_.take(NodeState::Flag::Focused, focused)) {
        markStyleDirty();
        WidgetState newState = getCurrentState();
        const ComputedLayoutStyle* newLayoutOldPtr = style.getComputedLayout(newState);
        if (hadOldLayout && newLayoutOldPtr) {
            if (oldLayout != *newLayoutOldPtr) {
                const auto* oldApp = style.getComputedAppearance(oldState);
                const auto* newAppOld = style.getComputedAppearance(newState);
                bool textMetricsChanged = false;
                if (oldApp && newAppOld) {
                    textMetricsChanged = (oldApp->fontSize != newAppOld->fontSize ||
                                          oldApp->fontFamily != newAppOld->fontFamily);
                }
                if (textMetricsChanged) markLayoutDirtyRecursive();
                else markLayoutDirty();
            }
        } else {
            const StyleRule* rule = style.get(WidgetState::Focused);
            if (rule && (detail::hasLayoutProps(*rule) || detail::hasTextMetricsProps(*rule))) {
                if (detail::hasTextMetricsProps(*rule)) markLayoutDirtyRecursive();
                else markLayoutDirty();
            } else {
                const StyleRule* oldRule = style.get(oldState);
                if (oldRule && (detail::hasLayoutProps(*oldRule) || detail::hasTextMetricsProps(*oldRule))) {
                    if (detail::hasTextMetricsProps(*oldRule)) markLayoutDirtyRecursive();
                    else markLayoutDirty();
                }
            }
        }
    }
}

bool SceneNode::isVisible() const { return state_.test(NodeState::Flag::Visible); }

void SceneNode::setVisible(bool newVisible) {
    if (state_.take(NodeState::Flag::Visible, newVisible)) {
        markLayoutDirty();
    }
}

bool SceneNode::isEnabled() const { return state_.test(NodeState::Flag::Enabled); }

void SceneNode::setEnabled(bool enabled) {
    WidgetState oldState = getCurrentState();
    const ComputedLayoutStyle* oldLayoutPtr = style.getComputedLayout(oldState);
    ComputedLayoutStyle oldLayout = oldLayoutPtr ? *oldLayoutPtr : ComputedLayoutStyle{};
    bool hadOldLayout = oldLayoutPtr != nullptr;

    if (state_.take(NodeState::Flag::Enabled, enabled)) {
        markStyleDirty();
        WidgetState newState = getCurrentState();
        const ComputedLayoutStyle* newLayoutOldPtr = style.getComputedLayout(newState);
        if (hadOldLayout && newLayoutOldPtr) {
            if (oldLayout != *newLayoutOldPtr) {
                const auto* oldApp = style.getComputedAppearance(oldState);
                const auto* newAppOld = style.getComputedAppearance(newState);
                bool textMetricsChanged = false;
                if (oldApp && newAppOld) {
                    textMetricsChanged = (oldApp->fontSize != newAppOld->fontSize ||
                                          oldApp->fontFamily != newAppOld->fontFamily);
                }
                if (textMetricsChanged) markLayoutDirtyRecursive();
                else markLayoutDirty();
            }
        } else {
            const StyleRule* rule = style.get(WidgetState::Disabled);
            if (rule && (detail::hasLayoutProps(*rule) || detail::hasTextMetricsProps(*rule))) {
                if (detail::hasTextMetricsProps(*rule)) markLayoutDirtyRecursive();
                else markLayoutDirty();
            }
        }
        if (!enabled) {
            if (auto s = scene.lock()) {
                s->onNodeDisabled(shared_from_this());
            }
        }
    }
}

void SceneNode::setZIndex(int newZIndex) {
    if (zIndex != newZIndex) {
        zIndex = newZIndex;
        if (auto p = parent.lock()) p->childrenOrderDirty = true;
        if (auto s = scene.lock()) s->invalidateHitTestCache();
    }
}

int SceneNode::getZIndex() const { return zIndex; }

std::unique_ptr<SceneNode::Connection> SceneNode::on(EventType type, ActionCallback callback) {
    const handlerID id = target_.addHandler(type, std::move(callback));
    return std::unique_ptr<Connection>(new Connection(weak_from_this(), type, id));
}

std::unique_ptr<SceneNode::Connection> SceneNode::onCapture(EventType type,
                                                            ActionCallback callback) {
    const handlerID id = target_.addCaptureHandler(type, std::move(callback));
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
    if (phase == EventPhase::Capture) {
        if (!target_.hasAnyCaptureHandlers()) {
            return;
        }
        const EventType eventType = event.type;
        event.currentTarget = weak_from_this();
        event.phase_ = phase;
        if (target_.hasCaptureHandler(eventType)) {
            const UIContext ctx(getScene().get());
            target_.runCaptureHandlers(eventType, event, ctx);
            if (event.isImmediatePropagationStopped()) {
                return;
            }
        }
        return;
    }

    const EventType eventType = event.type;
    if (phase == EventPhase::Bubble) {
        if (!target_.hasHandler(eventType)) {
            return;
        }
    }

    event.currentTarget = weak_from_this();
    event.phase_ = phase;

    const UIContext ctx(getScene().get());

    if (target_.hasHandler(eventType)) {
        target_.runHandlers(eventType, event, ctx);
    }

    if (phase == EventPhase::Target) {
        event.type = eventType;
        if (event.cancelable() && !event.isDefaultPrevented()) {
            onEvent(event);
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
        "before use.",
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
        "use.",
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

void SceneNode::draw(PaintContext& pc) { drawImpl(pc, pc.frame().viewport); }

void SceneNode::draw(IRenderer& renderer) {
    CanvasAdapter canvas(renderer);
    const Size viewportSize = renderer.getViewportSize();
    const double nowMs = std::chrono::duration<double, std::milli>(
                             std::chrono::steady_clock::now().time_since_epoch())
                             .count();
    PaintContext pc(canvas, renderer.getTextEngine(),
                    FrameInfo{.viewport = {0, 0, static_cast<int>(viewportSize.width),
                                           static_cast<int>(viewportSize.height)},
                              .timeMs = nowMs});
    draw(pc);
}

void SceneNode::drawImpl(PaintContext& pc, const Rect& viewportRect) {
    if (!state_.test(NodeState::Flag::Visible)) {
        return;
    }

    if (!getGlobalBounds().intersects(viewportRect)) {
        return;
    }

    onPaintBackground(pc);

    const bool clip = getComputedAppearance().clipContent;
    ClipGuard clipGuard(pc.canvas(), getGlobalBounds(), clip);

    onPaint(pc);

    sortChildrenIfDirty();
    for (const auto& child : children) {
        child->drawImpl(pc, viewportRect);
    }
}

void SceneNode::onPaintBackground(PaintContext& pc) {
    const auto& computedAppearance = getComputedAppearance();
    if (computedAppearance.backgroundColor.a == 0 && computedAppearance.borderThickness <= 0) {
        return;
    }

    Brush brush;
    if (computedAppearance.backgroundColor.a > 0) {
        brush.fill = Fill{computedAppearance.backgroundColor};
    }
    if (computedAppearance.borderThickness > 0) {
        brush.stroke = Stroke{computedAppearance.borderColor,
                              static_cast<float>(computedAppearance.borderThickness)};
    }

    pc.canvas().fillRoundRect(getGlobalBounds(),
                              static_cast<float>(computedAppearance.borderRadius), brush);
}

void SceneNode::onPaint(PaintContext& /*pc*/) {}

void SceneNode::bind(const std::shared_ptr<UIBinding>& binding) {
    connection_.reset();
    binding_ = binding;
    if (binding_) {
        connection_ =
            binding_->subscribe([this](const UIBinding& value) { this->onBindingChange(value); });
    }
}

std::shared_ptr<UIBinding> SceneNode::getBinding() const { return binding_; }

void SceneNode::onBindingChange(const UIBinding& binding) {
    onChange(binding);
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
