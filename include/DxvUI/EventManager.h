#ifndef DXVUI_EVENTMANAGER_H
#define DXVUI_EVENTMANAGER_H

#include <map>
#include <memory>

#include "DxvEvent.h"
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
     * node under the cursor. The hover state itself (the cached node) is kept
     * so HoverLeave still reaches the previously hovered node; only the O(1)
     * cache shortcut is disabled until the next hit-test refreshes it.
     */
    void invalidateHitTestCache() noexcept { hitTestCache.valid = false; }

    /**
     * @brief Clears hover/pressed/focus state tied to a removed node.
     *
     * Called by SceneNode when a node leaves the tree. A detached-but-alive node
     * (the application may still hold a reference) must stop receiving events,
     * and its Hovered/Pressed visual state must not stay stuck: the removed node
     * or any descendant may be the hovered, pressed or focused node. HoverLeave
     * and FocusLost are dispatched before the state is dropped.
     * @param node The node that was removed.
     */
    void onNodeRemoved(const std::shared_ptr<SceneNode>& node);

   private:
    void handleMouseMove(DxvEvent& event);
    void handleMouseDown(DxvEvent& event);
    void handleMouseUp(DxvEvent& event);

    /**
     * @brief Finds the topmost node at the given coordinates, using the hovered
     * node as a cache.
     *
     * A stationary mouse keeps hitting the same node, so a bounds check against
     * the cached node short-circuits the O(n) reverse sibling scan that
     * dominates a fresh hit-test. The cache is only trusted while it is marked
     * valid (see invalidateHitTestCache()), the node is visible, contains the
     * point, and no sibling drawn in front of it overlaps its bounds (the
     * covered flag, recomputed whenever the cache entry is rebuilt). The result
     * is otherwise identical to a fresh root scan: the deepest visible
     * descendant under the point.
     * @param x The x-coordinate.
     * @param y The y-coordinate.
     * @return The topmost node at the coordinates, or nullptr.
     */
    std::shared_ptr<SceneNode> hitTest(int x, int y);

    Scene& ownerScene;

    // The hovered node is a cache: while valid it is a fresh hit-test result,
    // and reusing it turns a repeated hit-test into a bounds check. A relayout
    // or hierarchy mutation (see invalidateHitTestCache()) can move the node
    // under the cursor, so the cache is rebuilt by a fresh scan then.
    struct HitTestCache {
        std::weak_ptr<SceneNode> node;
        // Whether node is a fresh hit-test result. False while a relayout or
        // hierarchy mutation could have moved the cursor's node underneath it.
        bool valid = false;
        // Whether any visible sibling is drawn in front of node and overlaps
        // its bounds. Computed once when the entry is built (a rect-level
        // ancestor walk, O(depth x siblings)); caching it keeps the steady-state
        // hit-test O(1). A covering sibling can occlude the cursor point without
        // any relayout, so while true the cache must not short-circuit.
        bool covered = false;
    };
    HitTestCache hitTestCache;

    // The pressed node is tracked per mouse button, so a drag with the left
    // button is not lost when another button is pressed, and each button-up
    // releases only its own press. startPosition feeds the click-vs-drag
    // threshold.
    struct PressRecord {
        std::weak_ptr<SceneNode> node;
        PointI startPosition;
    };
    // Maximum pointer travel between press and release that still reports a
    // Click; beyond it the gesture is a drag. Compared against squared distance.
    static constexpr int dragThreshold = 5;
    std::map<MouseButton, PressRecord> pressedNodes;
    std::weak_ptr<SceneNode> focusedNode;
    PointI lastMousePosition;
};

}  // namespace DxvUI

#endif  // DXVUI_EVENTMANAGER_H
