#ifndef DXVUI_SCENE_H
#define DXVUI_SCENE_H

#include <memory>
#include "EventManager.h"
#include "ActionRegistry.h"

namespace DxvUI {

    class SceneNode;
    class IRenderer;

    class Scene : public std::enable_shared_from_this<Scene> {
    public:
        static std::shared_ptr<Scene> create();

        void setRoot(const std::shared_ptr<SceneNode>& node);
        std::shared_ptr<SceneNode> getRoot() const;

        void setRenderer(IRenderer* renderer); // New method
        IRenderer* getRenderer();

        ActionRegistry& getActionRegistry();
        EventManager& getEventManager();

        bool unregisterNode(std::weak_ptr<SceneNode>);
        bool registerNode(std::weak_ptr<SceneNode>);
        std::shared_ptr<SceneNode> findNodeById(std::string);

        void requestLayoutUpdate();

        void processEvent(const DxvEvent& event);
        void update(float deltaTime);
        void updateLayout(); // No longer needs renderer passed in
        void draw();         // No longer needs renderer passed in

        void shutdown(); // New method for explicit cleanup
        ~Scene();

    private:
        Scene();
        void init();

        std::shared_ptr<SceneNode> root;
        std::unique_ptr<EventManager> eventManager;
        ActionRegistry actionRegistry;
        IRenderer* renderer = nullptr; // Now a persistent pointer
        bool layoutIsDirty = true;
        std::unordered_map<std::string, std::weak_ptr<SceneNode>> nodeById;
    };

}

#endif //DXVUI_SCENE_H
