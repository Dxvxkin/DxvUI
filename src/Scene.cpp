#include "DxvUI/Scene.h"

#include "DxvUI/Log.h"
#include "DxvUI/SceneNode.h"
#include "DxvUI/containers/AbsoluteContainer.h"
#include "DxvUI/interfaces/IRenderer.h"

namespace DxvUI {

Scene::Scene() = default;

Scene::~Scene() { shutdown(); }

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

void Scene::setRenderer(IRenderer* newRenderer) { renderer = newRenderer; }

IRenderer* Scene::getRenderer() { return renderer; }

std::shared_ptr<SceneNode> Scene::getRoot() const { return root; }

Theme& Scene::getTheme() { return theme; }

std::shared_ptr<SceneNode> Scene::findNodeById(std::string id) {
    return root ? root->findNodeById(id) : nullptr;
}

void Scene::processEvent(const DxvEvent& event) { eventManager->processRawEvent(event); }

void Scene::onNodeRemoved(const std::shared_ptr<SceneNode>& node) {
    if (eventManager) {
        eventManager->onNodeRemoved(node);
    }
}

void Scene::update(float deltaTime) {
    if (root) {
        // Resolve dirty styles and re-lay-out the tree if needed.
        updateLayout();
    }
}

void Scene::updateLayout() {
    if (!root || !renderer) return;

    // Resolve dirty styles first; this is O(1) when the tree is clean, and the
    // StyleManager detects theme mutations itself (marking the root dirty and
    // scheduling a relayout), so no separate theme subscription is needed.
    styleManager.resolveDirtyStyles(root);

    // The layout pass prunes clean subtrees, so it is O(1) on clean frames and
    // only walks the affected branch otherwise.
    Size viewportSize = renderer->getViewportSize();
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
    if (root && renderer) {
        root->draw(*renderer);
    }
}

}  // namespace DxvUI
