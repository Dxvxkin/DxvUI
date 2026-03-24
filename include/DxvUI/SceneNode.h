#ifndef DXVUI_SCENENODE_H
#define DXVUI_SCENENODE_H

#include <memory>
#include <vector>
#include <string>
#include <map>
#include <functional>
#include "core.h"
#include "Style.h"
#include "interfaces/IRenderer.h"

namespace DxvUI {

    class Scene;
    class EventManager;

    struct ComputedAppearanceStyle {
        Color backgroundColor;
        Color textColor;
        Color borderColor;
        int borderThickness;
        int borderRadius;
        CursorType cursor;
        int fontSize;
        std::string fontPath;
    };

    struct ComputedLayoutStyle {
        float left, top, width, height;
        Thickness padding;
        Thickness margin;
        Alignment horizontalAlignment;
        Alignment verticalAlignment;
        Rect computedBounds;
    };

    class SceneNode : public std::enable_shared_from_this<SceneNode> {
    public:
        explicit SceneNode(std::string id = "");
        virtual ~SceneNode();

        void addChild(const std::shared_ptr<SceneNode>& child);
        void removeChild(const std::shared_ptr<SceneNode>& child);
        void detach();
        void detachAllChildren();

        virtual void setScene(const std::shared_ptr<Scene>& scene);
        std::shared_ptr<Scene> getScene() const;

        const std::string& getId() const;
        void setId(const std::string& newId);
        std::shared_ptr<SceneNode> findNodeById(const std::string& searchId) const;
        virtual std::shared_ptr<SceneNode> findNodeAt(int x, int y);

        template <typename T> [[nodiscard]] T* as() { return dynamic_cast<T*>(this); }
        template <typename T> [[nodiscard]] const T* as() const { return dynamic_cast<const T*>(this); }

        // --- Style & Layout ---
        Style& editStyle(); // Should be renamed to editStyle() for clarity
        void markStyleDirty();
        void markLayoutDirty();
        const ComputedAppearanceStyle& getComputedAppearance(WidgetState state);
        const ComputedLayoutStyle& getComputedLayout(WidgetState state);
        Rect getGlobalBounds() const;
        Size getDesiredSize() const;
        WidgetState getCurrentState() const;

        // --- State & Hierarchy ---
        bool isRoot() const;
        void setZIndex(int newZIndex);
        int getZIndex() const;
        void setHovered(bool hovered);
        void setPressed(bool pressed);

        bool isVisible() const;
        void setVisible(bool visible);


        // --- Events & Lifecycle (Framework-level API) ---
        // "Eternal" version for callbacks not tied to an object's lifecycle (e.g., from main)
        void on(EventType type, std::function<void(DxvEvent&)> callback);
        // Safe version for callbacks tied to an owner's lifecycle
        void on(EventType type, const std::shared_ptr<SceneNode>& owner, std::function<void(DxvEvent&)> callback);

        virtual void dispatchEvent(DxvEvent& event);
        virtual void onAttach();
        virtual void onDetach();
        virtual void onUpdate(float deltaTime);

        // --- Rendering Pipeline (Framework-level API) ---
        virtual Size measure(const Size& availableSize);
        virtual void arrange(const Rect& finalRect);
        virtual void draw(IRenderer& renderer);

        static int getNodeCount();

        std::weak_ptr<SceneNode> parent;
        std::vector<std::shared_ptr<SceneNode>> children;
        std::weak_ptr<Scene> scene;

    protected:
        std::string id;
        Style style;

        std::map<WidgetState, ComputedAppearanceStyle> appearanceCache;
        std::map<WidgetState, ComputedLayoutStyle> layoutCache;
        Size desiredSize;

        bool isLayoutDirty = true;
        bool isStyleDirty = true;

    private:
        void resolveAppearance(WidgetState state);
        void resolveLayout(WidgetState state);
        void sortChildrenIfDirty();

        bool isHovered = false;
        bool isPressed = false;
        bool visible = true;
        static int nodeCount;
        int zIndex = 0;
        bool childrenOrderDirty = false;
        std::map<EventType, std::vector<ActionCallback>> eventHandlers;
    };

}

#endif //DXVUI_SCENENODE_H
