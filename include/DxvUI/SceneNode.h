#ifndef DXVUI_SCENENODE_H
#define DXVUI_SCENENODE_H

#include <functional>
#include <map>
#include <memory>
#include <string>
#include <vector>

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
    // --- Type Aliases for Smart Pointers ---
    using SharedPtr = std::shared_ptr<SceneNode>;
    using ConstSharedPtr = std::shared_ptr<const SceneNode>;
    using WeakPtr = std::weak_ptr<SceneNode>;
    using UniquePtr = std::unique_ptr<SceneNode>;
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
    void addChild(const SharedPtr& child);

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
     * @brief Removes all child nodes from this node.
     */
    void detachAllChildren();

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
     * @brief Finds a node by its ID within the subtree of this node.
     * @param searchId The ID of the node to find.
     * @return A shared pointer to the found node, or nullptr if not found.
     */
    std::shared_ptr<SceneNode> findNodeById(const std::string& searchId) const;

    /**
     * @brief Finds the topmost node at a given screen coordinate.
     * @param x The x-coordinate.
     * @param y The y-coordinate.
     * @return A shared pointer to the node at the given coordinates, or nullptr.
     */
    virtual std::shared_ptr<SceneNode> findNodeAt(int x, int y);

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
     * @brief Gets a mutable reference to the node's local style rules.
     * @return A reference to the Style object.
     */
    Style& editStyle();

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
     * @brief Gets the computed appearance properties for a given state.
     * @param state The widget state (e.g., Normal, Hovered).
     * @return A const reference to the computed appearance style.
     */
    const ComputedAppearanceStyle& getComputedAppearance(WidgetState state) const;

    /**
     * @brief Gets the computed layout properties for a given state.
     * @param state The widget state (e.g., Normal, Hovered).
     * @return A const reference to the computed layout style.
     */
    const ComputedLayoutStyle& getComputedLayout(WidgetState state) const;

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
     * @brief Checks if this node is the root of the scene.
     * @return True if the node is the root, false otherwise.
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

    /**
     * @brief Registers a callback for a specific event type.
     * @param type The type of event to listen for.
     * @param callback The function to execute when the event occurs.
     */
    void on(EventType type, ActionCallback callback);

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

    /**
     * @brief Called on every frame update.
     * @param deltaTime The time elapsed since the last frame.
     */
    virtual void onUpdate(float deltaTime);

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
    // @name Framework Internals & Public Members
    //----------------------------------------------------------------
    ///@{
    /**
     * @brief Gets the total number of SceneNode instances currently allocated.
     * @return The total node count.
     */
    static int getNodeCount();

    std::weak_ptr<SceneNode> parent;
    std::vector<std::shared_ptr<SceneNode>> children;
    std::weak_ptr<Scene> scene;

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

    friend class StyleManager;
    friend class LayoutManager;

    std::string id;
    Style style;

    LayoutData layoutData;

    std::shared_ptr<UIBinding> binding_;
    std::unique_ptr<UIBinding::Connection> connection_;

   private:
    /**
     * @brief Applies the resolved explicit size and min/max constraints to a size.
     * @param size The size to clamp in place.
     */
    void applySizeConstraints(Size& size) const;

    // Sets the style subtree-dirty flag on this node and every ancestor up to
    // the root. Used by markStyleDirty() so the StyleManager can prune its
    // resolution traversal.
    void markStyleSubtreeDirty();

    void sortChildrenIfDirty();

    bool isHovered = false;
    bool isPressed = false;
    bool visible = true;
    static int nodeCount;
    int zIndex = 0;
    bool childrenOrderDirty = false;
    std::map<EventType, std::vector<ActionCallback>> eventHandlers;
};

}  // namespace DxvUI

#endif  // DXVUI_SCENENODE_H