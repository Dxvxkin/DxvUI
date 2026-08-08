#ifndef DXVUI_UICONTEXT_H
#define DXVUI_UICONTEXT_H

#include <memory>
#include <string>

#include "DxvUI/core.h"

namespace DxvUI {

class Scene;
class SceneNode;
class Theme;
class IRenderer;

/**
 * @class UIContext
 * @brief A non-owning facade over Scene, handed to event handlers.
 *
 * The scene and the rest of the framework services are passed to a callback
 * instead of being reached by climbing the tree from the event target
 * (getTarget()->getScene()->getRoot()). All methods are null-safe: the context
 * is valid only for the duration of the handler call, and its scene may be null
 * when the event target is no longer attached (e.g. HoverLeave dispatched after
 * a node was removed). A context must never be stored past the callback.
 */
class UIContext {
   public:
    /**
     * @brief Gets the scene's root node.
     * @return The root node, or nullptr when the scene is unavailable.
     */
    std::shared_ptr<SceneNode> getRoot() const;

    /**
     * @brief Finds the first node with the given ID anywhere in the tree.
     * @param id The ID of the node to find.
     * @return The found node, or nullptr.
     */
    std::shared_ptr<SceneNode> findNodeById(const std::string& id) const;

    /**
     * @brief Gets the node that currently owns keyboard focus.
     * @return The focused node, or nullptr when nothing is focused.
     */
    std::shared_ptr<SceneNode> getFocusedNode() const;

    /**
     * @brief Moves keyboard focus to the given node.
     *
     * Dispatches FocusLost to the previously focused node and FocusGained to the
     * new one. Pass nullptr to clear the focus.
     * @param node The node to focus, or nullptr to unfocus.
     */
    void setFocus(const std::shared_ptr<SceneNode>& node) const;

    /**
     * @brief Resolves dirty styles and re-lays-out the tree.
     *
     * Useful after a handler mutated layout properties. A no-op when the scene
     * has no renderer (the viewport size cannot be obtained).
     */
    void updateLayout() const;

    /**
     * @brief Gets the active theme.
     * @return The theme, or nullptr when the scene is unavailable.
     */
    Theme* getTheme() const;

    /**
     * @brief Gets the scene's renderer.
     * @return The renderer, or nullptr when none is set or the scene is gone.
     */
    IRenderer* getRenderer() const;

    /**
     * @brief Gets the logical viewport size.
     * @return The viewport size, or {0, 0} when no renderer is set.
     */
    Size getViewport() const;

   private:
    friend class SceneNode;
    explicit UIContext(Scene* scene) : scene_(scene) {}

    Scene* scene_;
};

}  // namespace DxvUI

#endif  // DXVUI_UICONTEXT_H
