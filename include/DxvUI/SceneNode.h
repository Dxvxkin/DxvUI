#ifndef DXVUI_SCENENODE_H
#define DXVUI_SCENENODE_H

#include <functional>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "DxvUI/DxvEvent.h"
#include "DxvUI/UIBinding.h"
#include "DxvUI/core.h"
#include "DxvUI/interfaces/IRenderer.h"
#include "DxvUI/layout/LayoutData.h"
#include "DxvUI/style/Style.h"

namespace DxvUI {

class Scene;
class EventManager;
class StyleManager;
class LayoutManager;

/**
 * @class SceneNode
 * @brief The base class for all objects in the UI scene graph.
 *
 * SceneNode represents a single element in the UI hierarchy. It manages parent-child
 * relationships, styling, layout, events, and rendering. It is not typically
 * used directly, but rather subclassed to create specific widgets (like Button)
 * or containers (like HorizontalContainer).
 */
class SceneNode : public std::enable_shared_from_this<SceneNode> {
   public:
    /**
     * @brief Constructs a SceneNode with a unique identifier.
     * @param id A string identifier for the node. Must be unique within the scene.
     */
    explicit SceneNode(std::string id);

    /**
     * @brief Virtual destructor.
     */
    virtual ~SceneNode();

    //----------------------------------------------------------------
    // @name Hierarchy Management
    //----------------------------------------------------------------
    ///@{

    /**
     * @brief Adds a child node to this node.
     * @param child A shared pointer to the child node to add.
     */
    void addChild(const std::shared_ptr<SceneNode>& child);

    /**
     * @brief Removes a specific child node from this node.
     * @param child A shared pointer to the child node to remove.
     */
    void removeChild(const std::shared_ptr<SceneNode>& child);

    /**
     * @brief Detaches this node from its parent.
     */
    void detach();

    /**
     * @brief Detaches this node and every descendant from their parents.
     *
     * The whole subtree is detached recursively: children are removed from this
     * node, grandchildren from their parents, and this node from its parent.
     * Detaching does not destroy nodes; they stay alive as long as the
     * application still holds a reference to them.
     */
    void detachSubtree();

    /**
     * @brief Sets the scene this node belongs to.
     * @param scene A shared pointer to the scene.
     */
    virtual void setScene(const std::shared_ptr<Scene>& scene);

    /**
     * @brief Gets the scene this node belongs to.
     * @return A shared pointer to the scene, or nullptr if not attached.
     */
    std::shared_ptr<Scene> getScene() const;

    /**
     * @brief Gets this node's parent, if any.
     * @return A weak pointer to the parent, or an expired weak pointer if the
     * node is a root. Lock it with `lock()` before use.
     */
    std::weak_ptr<SceneNode> getParent() const;

    /**
     * @brief Whether this node is a (strict) ancestor of the given node.
     * @param descendant The node to test.
     * @return True if this node appears in the ancestor chain of descendant.
     */
    bool isAncestorOf(const std::shared_ptr<SceneNode>& descendant) const;

    /**
     * @brief Gets the direct children of this node.
     * @return A const reference to the children vector.
     */
    const std::vector<std::shared_ptr<SceneNode>>& getChildren() const;

    ///@}

    //----------------------------------------------------------------
    // @name Identification and Searching
    //----------------------------------------------------------------
    ///@{

    /**
     * @brief Gets the unique identifier of the node.
     * @return A const reference to the node's ID string.
     */
    const std::string& getId() const;

    /**
     * @brief Sets a new unique identifier for the node.
     * @param newId The new ID string.
     */
    void setId(const std::string& newId);

    /**
     * @brief Finds the first node whose ID matches within the subtree of this node.
     *
     * Depth-first search: this node is checked first, then its children in draw
     * order. Duplicate IDs in different subtrees are allowed; the first match
     * in traversal order wins. Hidden nodes are searched too (a structural
     * lookup, not a hit-test).
     * @param searchId The ID of the node to find.
     * @return A shared pointer to the found node, or nullptr if not found.
     */
    std::shared_ptr<SceneNode> findNodeById(const std::string& searchId);

    /**
     * @brief Finds the topmost node at a given screen coordinate.
     * @param x The x-coordinate.
     * @param y The y-coordinate.
     * @return A shared pointer to the node at the given coordinates, or nullptr.
     */
    virtual std::shared_ptr<SceneNode> findNodeAt(int x, int y);

    /**
     * @brief Whether any visible sibling (of this node or of any ancestor) that
     * is drawn in front of this node intersects the given bounds.
     *
     * A rect-level test used by the event system's hit-test cache: it is
     * computed once when a cache entry is rebuilt, so a node with no covering
     * sibling can be cached O(1) without a per-event sibling walk. Children are
     * kept in draw order (later = on top).
     * @param bounds The bounds to test (typically this node's global bounds).
     * @return True if a node drawn on top could cover part of the bounds.
     */
    bool hasNodeInFront(const Rect& bounds);

    /**
     * @brief Performs a safe dynamic cast of the node to a derived type.
     * @tparam T The type to cast to.
     * @return A pointer to the casted type, or nullptr if the cast fails.
     */
    template <typename T>
    [[nodiscard]] T* as() {
        return dynamic_cast<T*>(this);
    }

    /**
     * @brief Performs a safe dynamic cast of the node to a const derived type.
     * @tparam T The type to cast to.
     * @return A const pointer to the casted type, or nullptr if the cast fails.
     */
    template <typename T>
    [[nodiscard]] const T* as() const {
        return dynamic_cast<const T*>(this);
    }

    /**
     * @brief Gets the node's type name as a string.
     * @return A C-style string representing the node's type (e.g., "Button").
     */
    virtual const char* getNodeType() const;

    ///@}

    //----------------------------------------------------------------
    // @name Style and Layout
    //----------------------------------------------------------------
    ///@{

    /**
     * @brief Sets or overwrites the node's local style rule for a given state.
     *
     * Invalidates the style (and, when any layout property changed, the layout)
     * only when the new rule actually differs from the current one.
     * @param rule The style rule to set.
     * @param state The widget state to target.
     */
    void setStyle(const StyleRule& rule, WidgetState state = WidgetState::Normal);

    /**
     * @brief Merges style updates into the node's existing rule for a given state.
     *
     * Same invalidation semantics as setStyle(): a merge that touches only
     * appearance properties does not force a relayout.
     * @param updates A StyleRule containing only the properties to change.
     * @param state The widget state to target.
     */
    void updateStyle(const StyleRule& updates, WidgetState state = WidgetState::Normal);

    /**
     * @brief Gets a read-only reference to the node's local style rules.
     * @return A const reference to the Style object.
     */
    const Style& getStyle() const;

    /**
     * @brief Marks the node's style as dirty, forcing a recomputation.
     */
    void markStyleDirty();

    /**
     * @brief Marks the node's layout as dirty, forcing a remeasure and rearrange.
     *
     * Sets this node's dirty flag and propagates the dirty-subtree flag to every
     * ancestor. The LayoutManager prunes clean subtrees, so the next pass only
     * re-lays-out the affected branch instead of the whole scene.
     */
    void markLayoutDirty();

    /**
     * @brief Marks the layout of this node and every descendant as dirty.
     *
     * A single ancestor-level markLayoutDirty() only re-lays-out that branch:
     * clean subtrees are pruned by the LayoutManager. This is used when a theme
     * mutation changes layout properties, which can affect any node in the tree.
     */
    void markLayoutDirtyRecursive();

    /**
     * @brief Marks the style of this node and every descendant as dirty.
     *
     * A single ancestor-level markStyleDirty() only resolves that branch:
     * descendants are re-resolved only when the inherited text properties
     * actually changed. This forces a full-tree re-resolution and is used when
     * a theme mutation changes the defaults of any node type.
     */
    void markStyleDirtyRecursive();

    /**
     * @brief Gets the computed appearance properties for a given state.
     * @param state The widget state (e.g., Normal, Hovered).
     * @return A const reference to the computed appearance style.
     */
    const ComputedAppearanceStyle& getComputedAppearance(WidgetState state) const;

    /**
     * @brief Gets the computed appearance properties for the current state.
     * @return A const reference to the computed appearance style.
     */
    const ComputedAppearanceStyle& getComputedAppearance() const;

    /**
     * @brief Gets the computed layout properties for a given state.
     * @param state The widget state (e.g., Normal, Hovered).
     * @return A const reference to the computed layout style.
     */
    const ComputedLayoutStyle& getComputedLayout(WidgetState state) const;

    /**
     * @brief Gets the computed layout properties for the current state.
     * @return A const reference to the computed layout style.
     */
    const ComputedLayoutStyle& getComputedLayout() const;

    /**
     * @brief Gets the node's final position and size in global screen coordinates.
     * @return A Rect representing the global bounds.
     */
    Rect getGlobalBounds() const;

    /**
     * @brief Gets the desired size of the node as calculated by the last measure pass.
     * @return The desired size.
     */
    Size getDesiredSize() const;

    /**
     * @brief Whether this node (or a descendant) needs a fresh layout pass.
     *
     * Exposed for containers so layout passes can skip children whose cached
     * measure/arrange results are still valid.
     */
    bool isLayoutDirty() const;

    /**
     * @brief Gets the measure constraints of the last measure pass.
     *
     * Exposed for containers so a cached desired size is only reused while it
     * matches the constraints it was computed with.
     */
    const Size& getLastMeasureConstraints() const;

    /**
     * @brief Gets the cached measure/arrange state of the node.
     *
     * Exposed for containers so arrange passes can skip nodes that kept their
     * previous rect (AbsoluteContainer/CenterContainer prune on it). Read-only;
     * the LayoutData is maintained exclusively by the LayoutManager.
     */
    const LayoutData& getLayoutData() const;

    /**
     * @brief Gets the current interaction state of the node (e.g., Normal, Hovered).
     * @return The current WidgetState.
     */
    WidgetState getCurrentState() const;

    ///@}

    //----------------------------------------------------------------
    // @name State and Hierarchy
    //----------------------------------------------------------------
    ///@{

    /**
     * @brief Checks whether the node is at the top of its subtree (has no parent).
     *
     * Within an attached tree this is equivalent to being the scene's root node.
     * A detached subtree reports true as well, since root-ness here is purely
     * structural and does not depend on the scene.
     * @return True if the node has no parent, false otherwise.
     */
    bool isRoot() const;

    /**
     * @brief Sets the Z-index of the node for controlling draw order.
     * @param newZIndex The new Z-index value.
     */
    void setZIndex(int newZIndex);

    /**
     * @brief Gets the Z-index of the node.
     * @return The Z-index value.
     */
    int getZIndex() const;

    /**
     * @brief Sets the hovered state of the node.
     * @param hovered True to set as hovered, false otherwise.
     */
    void setHovered(bool hovered);

    /**
     * @brief Sets the pressed state of the node.
     * @param pressed True to set as pressed, false otherwise.
     */
    void setPressed(bool pressed);

    /**
     * @brief Sets the focused state of the node.
     *
     * Focus is driven by the event manager (a mouse-down or a node removal
     * changes which node owns the keyboard focus); widgets do not normally call
     * this directly. Like setHovered()/setPressed(), it invalidates the layout
     * so a state-specific style rule is picked up.
     * @param focused True to set as focused, false otherwise.
     */
    void setFocused(bool focused);

    /**
     * @brief Checks if the node is currently visible.
     * @return True if visible, false otherwise.
     */
    bool isVisible() const;

    /**
     * @brief Sets the visibility of the node.
     * @param visible True to make visible, false to hide.
     */
    void setVisible(bool visible);

    ///@}

    //----------------------------------------------------------------
    // @name Events and Lifecycle
    //----------------------------------------------------------------
    ///@{

    using handlerID = uint64_t;

    /**
     * @brief RAII handle for a registered event handler.
     *
     * Destroying the connection removes the handler. A handler that must live
     * for the node's lifetime has to keep the returned connection alive
     * (mirrors UIBinding::Connection). A connection whose node was destroyed is
     * a no-op.
     */
    class Connection {
       public:
        ~Connection();
        Connection(Connection&&) = delete;
        Connection& operator=(Connection&&) = delete;
        Connection(const Connection&) = delete;
        Connection& operator=(const Connection&) = delete;

        /**
         * @brief Whether the owning node has been destroyed.
         */
        bool expired() const noexcept { return node.expired(); }

       private:
        friend class SceneNode;
        Connection(std::weak_ptr<SceneNode> node, EventType type, handlerID id);
        std::weak_ptr<SceneNode> node;
        EventType type;
        handlerID id;
    };

    /**
     * @brief Registers a callback for a specific event type.
     * @param type The type of event to listen for.
     * @param callback The function to execute when the event occurs.
     * @return A connection that removes the handler when destroyed.
     */
    std::unique_ptr<Connection> on(EventType type, ActionCallback callback);

    /**
     * @brief Dispatches an event through the node's hierarchy.
     * @param event The event to dispatch.
     */
    virtual void dispatchEvent(DxvEvent& event);

    /**
     * @brief Called when the node is attached to a scene.
     */
    virtual void onAttach();

    /**
     * @brief Called when the node is detached from its parent.
     */
    virtual void onDetach();

    ///@}

    //----------------------------------------------------------------
    // @name Rendering Pipeline
    //----------------------------------------------------------------
    ///@{

    /**
     * @brief First pass of layout: calculates the desired size of the node.
     *
     * Thin entry point that delegates to LayoutManager::measureNode(), which
     * prunes clean nodes and applies the resolved size constraints (explicit
     * width/height from style, then min/max clamping) on top of the onMeasure()
     * result, so widget authors never have to handle them.
     * @param availableSize The size available from the parent.
     * @return The desired size required by this node.
     */
    Size measure(const Size& availableSize);

    /**
     * @brief Second pass of layout: sets the final size and position of the node.
     *
     * Thin entry point that delegates to LayoutManager::arrangeNode(), which
     * prunes unchanged subtrees before calling onArrange().
     * @param finalRect The final rectangle allocated by the parent.
     */
    void arrange(const Rect& finalRect);

    /**
     * @brief Draws the node and its children.
     *
     * Template method: it checks visibility and viewport intersection, then
     * calls the drawBackground() and drawContent() hooks, then draws the
     * children. Subclasses that only need a styled background/border or simple
     * content should override the protected hooks instead of this method.
     * @param renderer The renderer to use for drawing operations.
     */
    virtual void draw(IRenderer& renderer);

    ///@}

    //----------------------------------------------------------------
    // @name Data Binding
    //----------------------------------------------------------------
    ///@{

    /**
     * @brief Binds this node to a data model.
     * @param binding The UIBinding object to connect to.
     */
    void bind(const std::shared_ptr<UIBinding>& binding);

    /**
     * @brief Gets the data binding associated with this node.
     * @return A shared pointer to the UIBinding object.
     */
    std::shared_ptr<UIBinding> getBinding() const;

    ///@}

    //----------------------------------------------------------------
    // @name Framework Internals
    //----------------------------------------------------------------
    ///@{
    /**
     * @brief Gets the total number of SceneNode instances currently allocated.
     * @return The total node count.
     */
    static int getNodeCount();

    ///@}
    ///
    /**
     * @brief Gets the depth of this node in the scene graph hierarchy.
     *
     * The root node has a depth of 0. Its direct children have a depth of 1, and so on.
     *
     * @return The depth of the node.
     * @complexity O(D), where D is the depth of the node. In the worst case, D can be N (number of
     * nodes).
     * @exception safety Nothrow.
     */
    [[nodiscard]] std::size_t getDepth() const noexcept;

   protected:
    virtual void onChange(const UIBinding& binding);
    void onBindingChange(const UIBinding& binding);

    /**
     * @brief Default-action hook: the widget's own behavior for a dispatched
     * event.
     *
     * dispatchEvent() runs the user listeners registered through on() first,
     * then calls this hook with the same event. The hook is skipped when a
     * listener called preventDefault(). A widget that consumes the event (so it
     * must not reach its parents) calls event.stopPropagation() inside the hook.
     * Override instead of overriding dispatchEvent(); the default is a no-op.
     * @param event The event being dispatched (type is the original, non-mutated one).
     */
    virtual void onEvent(DxvEvent& event);

    /**
     * @brief Computes the intrinsic desired size of the node.
     *
     * Override this in widgets and containers instead of measure(); the base
     * class applies style size constraints afterwards. Only invoked by
     * LayoutManager when the node actually needs re-measuring.
     * @param availableSize The size available from the parent.
     * @return The desired size before any style constraints are applied.
     */
    virtual Size onMeasure(const Size& availableSize);

    /**
     * @brief Lays out the node's children within the allocated final rect.
     *
     * Override this in containers instead of arrange(); LayoutManager sets the
     * node's own bounds and clears its dirty flags before calling this hook.
     * @param finalRect The final rectangle allocated by the parent.
     */
    virtual void onArrange(const Rect& finalRect);

    /**
     * @brief Draws the node's background and border.
     *
     * Default implementation fills a rounded rect built from the node's
     * computed appearance (borderRadius, backgroundColor, border). It is a
     * no-op when the background is transparent and the border is zero-thick.
     * Override for widgets with a fully custom background.
     * @param renderer The renderer to use for drawing operations.
     */
    virtual void drawBackground(IRenderer& renderer);

    /**
     * @brief Draws the node's content, on top of the background and before
     * the children.
     *
     * Default implementation is a no-op. Override to render widget-specific
     * content (e.g. Label draws its text texture here).
     * @param renderer The renderer to use for drawing operations.
     */
    virtual void drawContent(IRenderer& renderer);

    friend class StyleManager;
    friend class LayoutManager;

    std::shared_ptr<UIBinding> binding_;
    std::unique_ptr<UIBinding::Connection> connection_;

   private:
    // Hierarchy and identification are owned exclusively by SceneNode; widgets
    // and containers reach them through the public accessors (getChildren(),
    // getId(), getLayoutData()) and the framework managers read them as friends.
    std::weak_ptr<SceneNode> parent;
    std::vector<std::shared_ptr<SceneNode>> children;

    std::string id;
    Style style;

    LayoutData layoutData;

    // Sets the style subtree-dirty flag on this node and every ancestor up to
    // the root. Used by markStyleDirty() so the StyleManager can prune its
    // resolution traversal.
    void markStyleSubtreeDirty();

    void sortChildrenIfDirty();

    // Removes the handler with the given id for the given event type. Called by
    // Connection's destructor; a no-op when the handler is already gone.
    void removeHandler(EventType type, handlerID id);

    // Recursive draw used by the public draw(); carries the viewport rect so
    // the whole tree is culled against it in O(visible) instead of O(all nodes).
    void drawImpl(IRenderer& renderer, const Rect& viewportRect);

    std::weak_ptr<Scene> scene;
    bool isHovered = false;
    bool isPressed = false;
    bool isFocused = false;
    bool visible = true;
    static int nodeCount;
    int zIndex = 0;
    bool childrenOrderDirty = false;
    // Handlers are keyed by id per event type so that registration, removal and
    // snapshot dispatch are all safe while a handler is running.
    std::map<EventType, std::map<handlerID, ActionCallback>> eventHandlers;
    handlerID handlerIdCounter = 0;
};

}  // namespace DxvUI

#endif  // DXVUI_SCENENODE_H