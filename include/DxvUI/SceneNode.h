#ifndef DXVUI_SCENENODE_H
#define DXVUI_SCENENODE_H

#include <memory>
#include <vector>
#include <string>
#include "core.h"
#include "LayoutProperties.h"
#include "interfaces/IRenderer.h"

namespace DxvUI {

    class Scene;

    class SceneNode : public std::enable_shared_from_this<SceneNode> {
    public:
        explicit SceneNode(std::string id = "");
        virtual ~SceneNode() = default;

        void addChild(const std::shared_ptr<SceneNode>& child);
        void removeChild(const std::shared_ptr<SceneNode>& child);
        void detach();

        virtual void setScene(const std::shared_ptr<Scene>& scene);
        std::shared_ptr<Scene> getScene() const;

        const std::string& getId() const;
        void setId(const std::string& newId);
        std::shared_ptr<SceneNode> findNodeById(const std::string& searchId);

        template <typename T> [[nodiscard]] T* as() { return dynamic_cast<T*>(this); }
        template <typename T> [[nodiscard]] const T* as() const { return dynamic_cast<const T*>(this); }

        LayoutProperties& getLayout();
        const LayoutProperties& getLayout() const;
        void markLayoutDirty();

        void setPosition(float x, float y);
        void setSize(float width, float height);
        void setMargin(const Thickness& margin);
        void setPadding(const Thickness& padding);

        Rect getGlobalBounds() const;

        void setZIndex(int newZIndex);
        int getZIndex() const;

        virtual bool handleEvent(const DxvEvent& event);
        // The root of the layout calculation process
        void updateLayoutTree();
        virtual void updateLayout(const Rect& parentInnerBounds);
        virtual void draw(IRenderer& renderer);

        std::weak_ptr<SceneNode> parent;
        std::vector<std::shared_ptr<SceneNode>> children;
        std::weak_ptr<Scene> scene;

    protected:
        std::string id;
        std::unique_ptr<LayoutProperties> layoutProperties;
        bool isLayoutDirty = true;

    private:
        void sortChildrenIfDirty();
        int zIndex = 0;
        bool childrenOrderDirty = false;
    };

}

#endif //DXVUI_SCENENODE_H
