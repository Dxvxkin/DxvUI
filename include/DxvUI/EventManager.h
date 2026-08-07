#ifndef DXVUI_EVENTMANAGER_H
#define DXVUI_EVENTMANAGER_H

#include <memory>

#include "core.h"

namespace DxvUI {

class SceneNode;
class Scene;

class EventManager {
   public:
    explicit EventManager(Scene& scene);

    void processRawEvent(const DxvEvent& event);

    /**
     * @brief Marks the cached hovered node as untrustworthy.
     *
     * Called by Scene when a relayout or hierarchy mutation could have moved a
     * node under the cursor. The hover state itself (nodeUnderMouse) is kept so
     * HoverLeave still reaches the previously hovered node; only the O(1)
     * cache shortcut is disabled until the next hit-test refreshes it.
     */
    void invalidateHitTestCache() noexcept { nodeUnderMouseValid = false; }

   private:
    void handleMouseMove(DxvEvent& event);
    void handleMouseDown(DxvEvent& event);
    void handleMouseUp(DxvEvent& event);

    /**
     * @brief Finds the topmost node at the given coordinates, using the hovered
     * node as a cache.
     *
     * A stationary mouse keeps hitting the same node, so a bounds check against
     * the cached node short-circuits the O(n) reverse sibling scan. The cache is
     * only trusted while it is marked valid (see invalidateHitTestCache()) and
     * the node is visible and still contains the point. The result is identical
     * to a fresh root scan: the deepest visible descendant under the point.
     * @param x The x-coordinate.
     * @param y The y-coordinate.
     * @return The topmost node at the coordinates, or nullptr.
     */
    std::shared_ptr<SceneNode> hitTest(int x, int y);

    Scene& ownerScene;
    std::weak_ptr<SceneNode> nodeUnderMouse;
    // Whether nodeUnderMouse is a fresh hit-test result. False while a relayout
    // or hierarchy mutation could have moved the cursor's node underneath it.
    bool nodeUnderMouseValid = false;
    std::weak_ptr<SceneNode> pressedNode;
    std::weak_ptr<SceneNode> focusedNode;
    PointI lastMousePosition;
};

}  // namespace DxvUI

#endif  // DXVUI_EVENTMANAGER_H
