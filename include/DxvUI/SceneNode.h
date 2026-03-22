#ifndef DXVUI_SCENENODE_H
#define DXVUI_SCENENODE_H

#include <memory>
#include <vector>
#include <string>
#include <map>
#include <functional>
#include "core.h"
#include "LayoutProperties.h"
#include "Appearance.h"
#include "interfaces/IRenderer.h"

namespace DxvUI {

    class Scene;
    class EventManager;

    struct ComputedAppearance {
        Color backgroundColor;
        Color textColor;
        Color borderColor;
        int borderThickness;
        int borderRadius;
        int fontSize;
        std::string fontPath;
        CursorType cursor;
    };

    class SceneNode : public std::enable_shared_from_this<SceneNode> {
        friend class EventManager;
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
        virtual std::shared_ptr<SceneNode> findNodeAt(int x, int y);

        template <typename T> [[nodiscard]] T* as() { return dynamic_cast<T*>(this); }
        template <typename T> [[nodiscard]] const T* as() const { return dynamic_cast<const T*>(this); }

        Appearance& getAppearance();
        LayoutProperties& getLayout();
        const LayoutProperties& getLayout() const;
        void markStyleDirty();
        void markLayoutDirty();

        void setPosition(float x, float y);
        void setSize(float width, float height);
        Size getDesiredSize() const;
        Rect getGlobalBounds() const;
        void setZIndex(int newZIndex);
        int getZIndex() const;

        void on(EventType type, ActionCallback callback);
        virtual void dispatchEvent(DxvEvent& event);

        virtual void onAttach();
        virtual void onDetach();
        virtual void onUpdate(float deltaTime);

        virtual Size measure(const Size& availableSize);
        virtual void arrange(const Rect& finalRect);
        virtual void draw(IRenderer& renderer);

        bool isHovered = false;
        bool isPressed = false;

        std::weak_ptr<SceneNode> parent;
        std::vector<std::shared_ptr<SceneNode>> children;
        std::weak_ptr<Scene> scene;

    protected:
        const ComputedAppearance& getComputedAppearance();

        std::string id;

        Appearance appearance;
        LayoutProperties layoutProperties;

        ComputedAppearance computedAppearance;
        Size desiredSize;

        bool isLayoutDirty = true;
        bool isStyleDirty = true;

    private:
        void resolveAndCacheStyles();
        void sortChildrenIfDirty();

        int zIndex = 0;
        bool childrenOrderDirty = false;
        std::map<EventType, std::vector<ActionCallback>> eventHandlers;
    };

}

#endif //DXVUI_SCENENODE_H
