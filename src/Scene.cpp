#include "DxvUI/Scene.h"
#include "DxvUI/SceneNode.h"
#include "DxvUI/interfaces/IRenderer.h"

namespace DxvUI {

    Scene::Scene() = default;

    std::shared_ptr<Scene> Scene::create() {
        auto scene = std::shared_ptr<Scene>(new Scene());
        scene->init();
        return scene;
    }

    void Scene::init() {
        eventManager = std::make_unique<EventManager>(*this);
        root = std::make_shared<SceneNode>("root");
        root->setScene(shared_from_this());
    }

    void Scene::setRoot(const std::shared_ptr<SceneNode>& node) {
        if (root) root->setScene(nullptr);
        root = node;
        if (root) root->setScene(shared_from_this());
        requestLayoutUpdate();
    }

    void Scene::setRenderer(IRenderer* newRenderer) {
        renderer = newRenderer;
    }

    IRenderer* Scene::getRenderer() {
        return renderer;
    }

    std::shared_ptr<SceneNode> Scene::getRoot() const { return root; }
    ActionRegistry& Scene::getActionRegistry() { return actionRegistry; }
    EventManager& Scene::getEventManager() { return *eventManager; }

    void Scene::requestLayoutUpdate() {
        layoutIsDirty = true;
    }

    void Scene::processEvent(const DxvEvent& event) {
        eventManager->processRawEvent(event);
    }

    void Scene::update(float deltaTime) {
        if (root) {
            root->onUpdate(deltaTime);
        }
    }

    void Scene::updateLayout() {
        if (layoutIsDirty && root && renderer) {
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

}
