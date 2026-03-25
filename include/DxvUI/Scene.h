#ifndef DXVUI_SCENE_H
#define DXVUI_SCENE_H

#include <memory>
#include "EventManager.h"
#include "ActionRegistry.h"
#include "DxvUI/style/Theme.h" // Include the new Theme header

namespace DxvUI {

    class SceneNode;
    class IRenderer;

    class Scene : public std::enable_shared_from_this<Scene> {
    public:
        static std::shared_ptr<Scene> create();
        ~Scene();

        void setRoot(const std::shared_ptr<SceneNode>& node);
        std::shared_ptr<SceneNode> getRoot() const;

        void setRenderer(IRenderer* renderer);
        IRenderer* getRenderer();

        ActionRegistry& getActionRegistry();
        EventManager& getEventManager();
        Theme& getTheme(); // Getter for the theme

        bool unregisterNode(std::weak_ptr<SceneNode>);
        bool registerNode(std::weak_ptr<SceneNode>);
        std::shared_ptr<SceneNode> findNodeById(std::string);

        void requestLayoutUpdate();

        void processEvent(const DxvEvent& event);
        void update(float deltaTime);
        void updateLayout();
        void draw();

        void shutdown();

    private:
        Scene();
        void init();
        void resolveDirtyStyles();

        std::shared_ptr<SceneNode> root;
        std::unique_ptr<EventManager> eventManager;
        ActionRegistry actionRegistry;
        Theme theme; // Add Theme object
        IRenderer* renderer = nullptr;
        bool layoutIsDirty = true;
        std::unordered_map<std::string, std::weak_ptr<SceneNode>> nodeById;
    };

}

#endif //DXVUI_SCENE_H
