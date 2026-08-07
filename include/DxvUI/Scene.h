#ifndef DXVUI_SCENE_H
#define DXVUI_SCENE_H

#include <memory>

#include "DxvUI/layout/LayoutManager.h"  // Include the new LayoutManager header
#include "DxvUI/style/StyleManager.h"    // Include the new StyleManager header
#include "DxvUI/style/Theme.h"           // Include the new Theme header
#include "EventManager.h"

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

    Theme& getTheme();  // Getter for the theme

    bool unregisterNode(std::weak_ptr<SceneNode>);
    bool registerNode(std::weak_ptr<SceneNode>);
    std::shared_ptr<SceneNode> findNodeById(std::string);

    void processEvent(const DxvEvent& event);
    void update(float deltaTime);
    void updateLayout();
    void draw();

    /**
     * @brief Discards the event manager's hit-test cache.
     *
     * Called whenever a relayout or hierarchy mutation may have moved a node
     * under the cursor; keeping the cache across such a change could make the
     * next event resolve the wrong (occluded) node.
     */
    void invalidateHitTestCache() noexcept { eventManager->invalidateHitTestCache(); }

    void shutdown();

   private:
    Scene();
    void init();

    std::shared_ptr<SceneNode> root;
    std::unique_ptr<EventManager> eventManager;
    Theme theme;  // Add Theme object
    StyleManager styleManager{theme};
    LayoutManager layoutManager;
    IRenderer* renderer = nullptr;
    std::unordered_map<std::string, std::weak_ptr<SceneNode>> nodeById;
};

}  // namespace DxvUI

#endif  // DXVUI_SCENE_H
