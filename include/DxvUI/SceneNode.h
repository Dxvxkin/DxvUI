#ifndef DXVUI_SCENENODE_H
#define DXVUI_SCENENODE_H

#include <memory>
#include <vector>
#include <string>
#include <algorithm>
#include "core.h"
#include "interfaces/IRenderer.h"

namespace DxvUI {

    class SceneNode : public std::enable_shared_from_this<SceneNode> {
    public:
        explicit SceneNode(std::string id = "");
        virtual ~SceneNode() = default;

        void setParent(std::shared_ptr<SceneNode> newParent);
        void addChild(std::shared_ptr<SceneNode> child);
        void removeChild(std::shared_ptr<SceneNode> child);

        const std::string& getId() const;
        std::shared_ptr<SceneNode> findNodeById(const std::string& searchId);

        void getGlobalPosition(int& x, int& y) const;
        Rect getGlobalBounds() const;

        void setZIndex(int newZIndex);
        int getZIndex() const;

        virtual bool handleEvent(const DxvEvent& event);
        virtual void updateLayout();
        virtual void draw(IRenderer& renderer);

        std::weak_ptr<SceneNode> parent;
        std::vector<std::shared_ptr<SceneNode>> children;

        int relX = 0, relY = 0;
        int width = 0, height = 0;

    protected:
        std::string id;

    private:
        void sortChildrenIfDirty();

        int zIndex = 0;
        bool childrenOrderDirty = false;
    };

}

#endif //DXVUI_SCENENODE_H
