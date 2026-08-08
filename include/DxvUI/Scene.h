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

    /**
     * @brief Finds the first node with the given ID anywhere in the tree.
     *
     * Delegates to the root's subtree search (SceneNode::findNodeById). Returns
     * nullptr when the scene has no root or no matching node exists.
     * @param id The ID of the node to find.
     * @return A shared pointer to the found node, or nullptr if not found.
     */
    std::shared_ptr<SceneNode> findNodeById(const std::string& id);

    /**
     * @brief Gets the node that currently owns keyboard focus.
     * @return The focused node, or nullptr when nothing is focused.
     */
    std::shared_ptr<SceneNode> getFocusedNode() const;

    /**
     * @brief Moves keyboard focus to the given node.
     *
     * Dispatches FocusLost to the previously focused node and FocusGained to the
     * new one; passing nullptr clears the focus.
     * @param node The node to focus, or nullptr to unfocus.
     */
    void setFocus(const std::shared_ptr<SceneNode>& node);

    void processEvent(const DxvEvent& event);
    void update();
    void updateLayout();
    void draw();

    /**
     * @brief Notifies the event manager that a node left the tree.
     *
     * Called by SceneNode when a child is removed: the removed node (or a
     * descendant of it) may be the hovered, pressed or focused node, whose state
     * must be cleared even if the application still holds a reference to it.
     */
    void onNodeRemoved(const std::shared_ptr<SceneNode>& node);

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
};

}  // namespace DxvUI

#endif  // DXVUI_SCENE_H
