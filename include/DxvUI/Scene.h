#ifndef DXVUI_SCENE_H
#define DXVUI_SCENE_H

#include <memory>
#include "ActionRegistry.h"
#include "SceneNode.h"

namespace DxvUI {

    class Scene : public std::enable_shared_from_this<Scene> {
    public:
        static std::shared_ptr<Scene> create();

        void setRoot(const std::shared_ptr<SceneNode>& node);
        std::shared_ptr<SceneNode> getRoot() const;

        ActionRegistry& getActionRegistry();

        std::shared_ptr<SceneNode> findNodeById(const std::string& id);
        bool handleEvent(const DxvEvent& event); // Changed to return bool
        void updateLayout();
        void draw(IRenderer& renderer);

    private:
        Scene();
        void init();

        std::shared_ptr<SceneNode> root;
        ActionRegistry actionRegistry;
    };

}

#endif //DXVUI_SCENE_H
