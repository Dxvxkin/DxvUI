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
     * @brief Gets the node that currently owns keyboard focus.
     * @return The focused node, or nullptr when nothing is focused.
     */
    std::shared_ptr<SceneNode> getFocusedNode() const;

    /**
     * @brief Moves keyboard focus to the given node.
     *
     * Dispatches FocusLost to the previously focused node and FocusGained to the
     * new one; passing nullptr clears the focus. Used by the scene's public
     * API (e.g. via UIContext) so application code can drive focus directly.
     * @param node The node to focus, or nullptr to unfocus.
     */
    void setFocus(const std::shared_ptr<SceneNode>& node);

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

    /**
     * @brief Clears hover/pressed/focus state tied to a disabled node.
     *
     * Called by SceneNode::setEnabled(false). The disabled node (or a descendant
     * of it) may be the hovered, pressed or focused node; those must be cleared
     * even though the node stays in the tree, so a disabled widget stops
     * receiving interaction events immediately. HoverLeave and FocusLost are
     * dispatched before the state is dropped, like onNodeRemoved().
     * @param node The node that was disabled.
     */
    void onNodeDisabled(const std::shared_ptr<SceneNode>& node);

   private:
    void handleMouseMove(DxvEvent& event);
    void handleMouseDown(DxvEvent& event);
    void handleMouseUp(DxvEvent& event);

    /**
     * @brief Moves keyboard focus to the given node, dispatching FocusLost and
     * FocusGained to the old and new nodes.
     * @param newNode The node to focus, or nullptr to clear the focus.
     */
    void changeFocus(const std::shared_ptr<SceneNode>& newNode);

    /**
     * @brief Clears hover/press/focus state held by a node or any descendant.
     *
     * Shared by onNodeRemoved() and onNodeDisabled(): HoverLeave/FocusLost are
     * dispatched to the affected nodes and their state flags are dropped, so a
     * detached or disabled widget stops receiving interaction events. Assumes
     * @p node is not null.
     * @param node The node whose interaction state must be cleared.
     */
    void clearInteraction(const std::shared_ptr<SceneNode>& node);

    /**
     * @brief Sets the node the cursor physically covers as hovered.
     *
     * Clears the previous hovered node (setHovered(false) + HoverLeave) and
     * marks the new one (setHovered(true) + HoverEnter), mirroring the hover
     * state to the hit-test result on every mouse event. A null or root node
     * means "no hover". The hovered node is tracked independently of the hit-test
     * cache: a cache hit resolves the deepest descendant without refreshing the
     * cache entry, so deriving hover from the cache (as before) could leave a
     * stale node stuck in the Hovered state.
     * @param node The node under the cursor, or nullptr/root for empty space.
     */
    void setHovered(const std::shared_ptr<SceneNode>& node);

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
    std::weak_ptr<SceneNode> hoveredNode;
    PointI lastMousePosition;
};

}  // namespace DxvUI

#endif  // DXVUI_EVENTMANAGER_H
