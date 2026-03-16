#include "DxvUI/Scene.h"

namespace DxvUI {

    Scene::Scene() = default;

    std::shared_ptr<Scene> Scene::create() {
        auto scene = std::shared_ptr<Scene>(new Scene());
        scene->init();
        return scene;
    }

    void Scene::init() {
        root = std::make_shared<SceneNode>("");
        root->setScene(shared_from_this());
    }

    void Scene::setRoot(const std::shared_ptr<SceneNode>& node) {
        if (root) {
            root->setScene(nullptr);
        }
        root = node;
        if (root) {
            root->setScene(shared_from_this());
        }
    }

    std::shared_ptr<SceneNode> Scene::getRoot() const {
        return root;
    }

    ActionRegistry& Scene::getActionRegistry() {
        return actionRegistry;
    }

    std::shared_ptr<SceneNode> Scene::findNodeById(const std::string& id) {
        if (root) {
            return root->findNodeById(id);
        }
        return nullptr;
    }

    bool Scene::handleEvent(const DxvEvent& event) {
        if (root) {
            return root->handleEvent(event);
        }
        return false;
    }

    void Scene::updateLayout() {
        if (root) {
            // This is the entry point for the entire layout calculation process.
            root->updateLayoutTree();
        }
    }

    void Scene::draw(IRenderer& renderer) {
        if (root) {
            root->draw(renderer);
        }
    }

}
