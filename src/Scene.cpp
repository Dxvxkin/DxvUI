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
    root->detachAllChildren();
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
    requestLayoutUpdate();
}

void Scene::setRenderer(IRenderer* newRenderer) { renderer = newRenderer; }

IRenderer* Scene::getRenderer() { return renderer; }

std::shared_ptr<SceneNode> Scene::getRoot() const { return root; }

EventManager& Scene::getEventManager() { return *eventManager; }
Theme& Scene::getTheme() { return theme; }

bool Scene::unregisterNode(std::weak_ptr<SceneNode> node) {
    if (auto _node = node.lock()) {
        Log::trace("Unregistering node '{}'", _node->getId());

        if (_node->getId().empty()) return false;
        auto count = nodeById.erase(_node->getId());
        if (count == 0) {
            Log::warn("Attempt to unregister node '{}' that is not registered", _node->getId());
            return false;
        }
        return true;
    }
    Log::warn("Attempt to unregister an expired node");
    return false;
}

void Scene::requestLayoutUpdate() { layoutIsDirty = true; }

bool Scene::registerNode(std::weak_ptr<SceneNode> node) {
    if (auto _node = node.lock()) {
        Log::trace("Registering node '{}'", _node->getId());  // не удалять

        if (_node->getId().empty()) {
            Log::warn("Attempt to register node with empty id");
            return false;
        }
        if (nodeById.contains(_node->getId())) {
            Log::error(
                "Attempt to register node with id '{}' that is already "
                "registered",
                _node->getId());
            return false;
        }

        nodeById[_node->getId()] = node;
        return true;
    }
    Log::warn("Attempt to register an expired node");
    return false;
}

std::shared_ptr<SceneNode> Scene::findNodeById(std::string id) {
    auto it = nodeById.find(id);
    if (it == nodeById.end()) {
        return nullptr;
    }

    if (auto locked = it->second.lock()) {
        return locked;
    }

    return nullptr;
}

void Scene::processEvent(const DxvEvent& event) { eventManager->processRawEvent(event); }

void Scene::update(float deltaTime) {
    if (root) {
        // 1. Perform logical updates (e.g., animations, state changes)
        root->onUpdate(deltaTime);

        // 2. Automatically update layout if needed
        updateLayoutIfNeeded();
    }
}

void Scene::forceLayoutUpdate() {
    requestLayoutUpdate();
    updateLayoutIfNeeded();
}

void Scene::updateLayoutIfNeeded() {
    if (!root || !renderer) return;

    // Resolve dirty styles first; this is O(1) when the tree is clean, and the
    // StyleManager detects theme mutations itself (marking the root dirty and
    // scheduling a relayout), so no separate theme subscription is needed.
    styleManager.resolveDirtyStyles(root);

    if (layoutIsDirty) {
        // Now, perform layout calculations
        Size viewportSize = renderer->getViewportSize();
        Rect viewportRect = {0, 0, (int)viewportSize.width, (int)viewportSize.height};

        root->measure(viewportSize);
        root->arrange(viewportRect);

        layoutIsDirty = false;
    }
}

void Scene::draw() {
    if (root && renderer) {
        root->draw(*renderer);
    }
}

}  // namespace DxvUI
