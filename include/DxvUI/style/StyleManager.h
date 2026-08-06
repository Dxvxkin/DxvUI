#ifndef DXVUI_STYLEMANAGER_H
#define DXVUI_STYLEMANAGER_H

#include <cstdint>
#include <memory>

#include "DxvUI/style/Theme.h"

namespace DxvUI {

class SceneNode;

/**
 * @class StyleManager
 * @brief Resolves and caches computed styles for a scene graph.
 *
 * Owns the style resolution cascade (framework defaults, inherited text
 * properties, theme defaults and node-local rules) and walks the node tree,
 * recomputing only the nodes that have been marked dirty. The manager is a
 * pure computation unit: it depends only on the node tree and the Theme, so it
 * can be tested without a Scene or a renderer.
 */
class StyleManager {
   public:
    /**
     * @brief Constructs a StyleManager bound to a Theme.
     * @param theme The theme providing the widget default styles.
     */
    explicit StyleManager(Theme& theme);

    /**
     * @brief Recomputes cached styles for every dirty node in the subtree.
     *
     * Performs a pruned, top-down traversal: only subtrees flagged as dirty
     * (via the per-node style subtree flag) are entered, so a clean scene graph
     * costs O(1) and a localized edit only visits the affected branch. A parent
     * is always resolved before its children, and resolving a node cascades the
     * dirty flag onto its children so changes to inherited text properties
     * propagate through the subtree in this single pass. Theme mutations are
     * detected by the theme version and re-resolve the whole tree.
     * @param root The root of the subtree to walk.
     */
    void resolveDirtyStyles(const std::shared_ptr<SceneNode>& root);

   private:
    /**
     * @brief Resolves the full appearance cascade for a node state.
     * @param node The node to resolve.
     * @param state The widget state to resolve for.
     * @return The computed appearance style.
     */
    ComputedAppearanceStyle resolveAppearance(const SceneNode& node, WidgetState state) const;

    /**
     * @brief Resolves the full layout cascade for a node state.
     * @param node The node to resolve.
     * @param state The widget state to resolve for.
     * @return The computed layout style.
     */
    ComputedLayoutStyle resolveLayout(const SceneNode& node, WidgetState state) const;

    /**
     * @brief Overlays a style rule on top of a computed appearance style.
     * @param computed The computed style being built.
     * @param rule The rule to apply, or nullptr.
     */
    static void applyRule(ComputedAppearanceStyle& computed, const StyleRule* rule);

    /**
     * @brief Overlays a style rule on top of a computed layout style.
     * @param computed The computed style being built.
     * @param rule The rule to apply, or nullptr.
     */
    static void applyRule(ComputedLayoutStyle& computed, const StyleRule* rule);

    Theme& theme_;
    // Theme version at the time of the last resolution. When the theme is
    // mutated, the whole tree must be re-resolved in the next pass.
    std::uint64_t lastResolvedThemeVersion_ = 0;
};

}  // namespace DxvUI

#endif  // DXVUI_STYLEMANAGER_H
