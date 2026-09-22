#include "DxvUI/Scene.h"

#include <chrono>

#include "DxvUI/Log.h"
#include "DxvUI/SceneNode.h"
#include "DxvUI/containers/AbsoluteContainer.h"
#include "DxvUI/interfaces/ICanvas.h"
#include "DxvUI/interfaces/IPlatformServices.h"
#include "DxvUI/interfaces/IRenderBackend.h"

namespace DxvUI {

Scene::Scene() = default;

Scene::~Scene() { shutdown(); }

void Scene::addDamageRect(const Rect& rect) {
    if (rect.width <= 0 || rect.height <= 0) return;
    if (!hasDamage_) {
        damageUnion_ = rect;
        hasDamage_ = true;
    } else {
        // Union
        int x1 = std::min(damageUnion_.x, rect.x);
        int y1 = std::min(damageUnion_.y, rect.y);
        int x2 = std::max(damageUnion_.x + damageUnion_.width, rect.x + rect.width);
        int y2 = std::max(damageUnion_.y + damageUnion_.height, rect.y + rect.height);
        damageUnion_ = {x1, y1, x2 - x1, y2 - y1};
    }
    damageRects_.push_back(rect);
}

void Scene::clearDamage() {
    hasDamage_ = false;
    damageUnion_ = {0, 0, 0, 0};
    damageRects_.clear();
    fullRedraw_ = false;
}

void Scene::shutdown() {
    if (!root) {
        return;
    }

    Log::trace("Scene shutdown requested.");
    root->detachSubtree();
    root->setScene(nullptr);
    root.reset();

    Log::trace("Scene shutdown complete.");
}

std::shared_ptr<Scene> Scene::create() {
    auto scene = std::shared_ptr<Scene>(new Scene());
    scene->init();
    return scene;
}

void Scene::init() {
    eventManager = std::make_unique<EventManager>(*this);
    root = std::make_shared<AbsoluteContainer>("root");
    root->setScene(shared_from_this());
}

void Scene::setRoot(const std::shared_ptr<SceneNode>& node) {
    // Use shutdown to clear the old root, ensuring consistent cleanup logic.
    shutdown();
    root = node;
    if (root) {
        root->setScene(shared_from_this());
    }
    // The new root is freshly constructed (layout dirty from birth), so the next
    // layout pass picks it up without an explicit request.
}

void Scene::setRenderBackend(IRenderBackend* backend) {
    renderBackend = backend;
    // Propagate to EventManager's platform services if backend also implements IPlatformServices
    if (auto* ps = dynamic_cast<IPlatformServices*>(backend)) {
        platformServices = ps;
    }
}

void Scene::setPlatformServices(IPlatformServices* services) {
    platformServices = services;
    if (auto* b = dynamic_cast<IRenderBackend*>(services)) {
        renderBackend = b;
    }
}

IRenderBackend* Scene::getRenderBackend() { return renderBackend; }
IPlatformServices* Scene::getPlatformServices() { return platformServices; }

ITextEngine* Scene::getTextEngine() {
    if (renderBackend) return &renderBackend->getTextEngine();
    return nullptr;
}

std::shared_ptr<SceneNode> Scene::getRoot() const { return root; }

Theme& Scene::getTheme() { return theme; }

std::shared_ptr<SceneNode> Scene::findNodeById(const std::string& id) {
    return root ? root->findNodeById(id) : nullptr;
}

std::shared_ptr<SceneNode> Scene::getFocusedNode() const {
    return eventManager ? eventManager->getFocusedNode() : nullptr;
}

void Scene::setFocus(const std::shared_ptr<SceneNode>& node) {
    if (eventManager) {
        eventManager->setFocus(node);
    }
}

void Scene::processEvent(const DxvEvent& event) {
    // A window resize is not a widget event; re-layout against the new viewport
    // instead of dispatching it through the event manager.
    if (event.type == EventType::Resize) {
        // Damage rects from the old viewport are meaningless and stale-coordinate
        // culling could leave a wrong frame; force a full redraw until damage is
        // tracked viewport-relative.
        fullRedraw_ = true;
        updateLayout();
        return;
    }
    // An external host may forward input events (mouse move, key, ...) before
    // the first update()/draw() pass, i.e. before updateLayout() ever ran. The
    // event manager hit-tests the event immediately, which reads the computed
    // style cache of the target nodes; a cold tree has an empty cache and would
    // otherwise FATAL on getComputedAppearance(). Resolve on demand so the
    // first event is handled correctly regardless of host frame ordering; the
    // StyleManager/LayoutManager fast paths make this O(1) once the tree is
    // clean.
    if (root && root->getStyle().getComputedAppearance(WidgetState::Normal) == nullptr) {
        // The tree may also be clean-but-cold (no style rule was ever resolved
        // because nothing flagged it dirty). Marking the root dirty forces the
        // resolve pass to actually run instead of taking its own clean fast
        // path, which guarantees the cache is populated from the first event.
        root->markStyleDirty();
        updateLayout();
    }

    eventManager->processRawEvent(event);
}

void Scene::dispatch(DxvEvent& event) {
    if (eventManager) {
        eventManager->dispatch(event);
    }
}

void Scene::raise(EventType type, const std::shared_ptr<SceneNode>& target) {
    if (eventManager) {
        eventManager->raise(type, target);
    }
}

void Scene::onNodeRemoved(const std::shared_ptr<SceneNode>& node) {
    if (node) {
        addDamageRect(node->getGlobalBounds());
    }
    if (eventManager) {
        eventManager->onNodeRemoved(node);
    }
}

void Scene::onNodeDisabled(const std::shared_ptr<SceneNode>& node) {
    if (eventManager) {
        eventManager->onNodeDisabled(node);
        eventManager->invalidateHitTestCache();
    }
}

void Scene::update() {
    if (root) {
        // Resolve dirty styles and re-lay-out the tree if needed.
        updateLayout();
    }
}

void Scene::updateLayout() {
    if (!root) return;
    IRenderBackend* backend = renderBackend;
    if (!backend) return;

    // Resolve dirty styles first; this is O(1) when the tree is clean, and the
    // StyleManager detects theme mutations itself (marking the root dirty and
    // scheduling a relayout), so no separate theme subscription is needed.
    styleManager.resolveDirtyStyles(root);

    // The layout pass prunes clean subtrees, so it is O(1) on clean frames and
    // only walks the affected branch otherwise.
    Size viewportSize = backend->getViewportSize();
    // A relayout can move the node under the cursor or move a sibling on top of
    // it, so the event manager's hit-test cache must not outlive the pass. The
    // layout fast path returns early only when nothing could have moved.
    const bool layoutWillRun =
        root->isLayoutDirty() || root->getLastMeasureConstraints() != viewportSize;
    layoutManager.layout(root, viewportSize);
    if (layoutWillRun) {
        eventManager->invalidateHitTestCache();
    }
}

void Scene::draw() {
    if (!root) return;
    IRenderBackend* backend = renderBackend;
    if (!backend) return;

    // Stage 6b: damage tracking – if no damage and not full redraw, we can skip
    // For now we still draw full frame but pass damage union in FrameInfo for culling
    // Future: skip draw when !hasDamage_ && !fullRedraw_ (except caret blink)
    Size viewportSize = backend->getViewportSize();
    const double nowMs = std::chrono::duration<double, std::milli>(
                             std::chrono::steady_clock::now().time_since_epoch())
                             .count();

    // For external renderer mode, host already cleared, so we pass transparent clear
    // and let backend decide (ownsResources check). For owned mode, backend clears.
    ICanvas& canvas = backend->beginFrame(clearColor_);

    FrameInfo frame{.viewport = {0, 0, static_cast<int>(viewportSize.width),
                                 static_cast<int>(viewportSize.height)},
                    .timeMs = nowMs,
                    .damageUnion = damageUnion_,
                    .hasDamage = hasDamage_,
                    .fullRedraw = fullRedraw_};

    PaintContext pc(canvas, backend->getTextEngine(), frame);
    root->draw(pc);

    backend->endFrame();

    // Clear damage after frame
    clearDamage();
}

}  // namespace DxvUI
