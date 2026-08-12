#include "DxvUI/style/StyleManager.h"

#include <queue>
#include <tuple>

#include "DxvUI/SceneNode.h"
#include "DxvUI/style/Colors.h"

namespace DxvUI {

namespace {

// --- Default values for the entire framework ---
const ComputedAppearanceStyle FRAMEWORK_DEFAULT_APPEARANCE = {
    .backgroundColor = Colors::Transparent,
    .textColor = Colors::Black,
    .borderColor = Colors::Transparent,
    .borderThickness = 0,
    .borderRadius = 0,
    .cursor = CursorType::Arrow,
    .clipContent = false,
    .fontSize = 14,
    .fontFamily = ""  // Resolves to the platform default font
};

const ComputedLayoutStyle FRAMEWORK_DEFAULT_LAYOUT = {.width = 0,
                                                      .height = 0,
                                                      .padding = {},
                                                      .margin = {},
                                                      .horizontalAlignment = Alignment::Start,
                                                      .verticalAlignment = Alignment::Start};

}  // namespace

StyleManager::StyleManager(Theme& theme) : theme_(theme) {}

// --- Helper to apply a StyleRule over a computed style ---

void StyleManager::applyRule(ComputedAppearanceStyle& computed, const StyleRule* rule) {
    if (!rule) return;
    std::apply(
        [&](const auto&... prop) { (detail::applyAppearanceProp(computed, *rule, prop), ...); },
        detail::appearanceProps);
}

void StyleManager::applyRule(ComputedLayoutStyle& computed, const StyleRule* rule) {
    if (!rule) return;
    std::apply([&](const auto&... prop) { (detail::applyLayoutProp(computed, *rule, prop), ...); },
               detail::layoutProps);
}

// --- Main Resolution Logic ---

ComputedAppearanceStyle StyleManager::resolveAppearance(const SceneNode& node,
                                                        WidgetState state) const {
    // The cascade order is critical and layered:
    // 1. Base: Start with framework-wide defaults.
    // 2. Inheritance: Inherit text properties from the parent's 'Normal' state.
    // 3. Normal Layer: Establish the full 'Normal' style.
    //    a. Apply theme's 'Normal' style.
    //    b. Apply node's own 'Normal' style, overriding the theme.
    // 4. State Layer: If the state is not 'Normal', apply state-specific styles on top.
    //    a. Apply theme's state-specific style.
    //    b. Apply node's own state-specific style, overriding the theme.

    ComputedAppearanceStyle computed = FRAMEWORK_DEFAULT_APPEARANCE;  // Step 1

    if (auto parent = node.parent.lock()) {  // Step 2
        const auto& parentStyle = parent->getComputedAppearance(WidgetState::Normal);
        std::apply(
            [&](const auto&... prop) {
                (detail::inheritAppearanceProp(computed, parentStyle, prop), ...);
            },
            detail::appearanceProps);
    }

    // --- Step 3: Build the full 'Normal' style ---
    applyRule(computed, theme_.getDefaultRule(node.getNodeType(), WidgetState::Normal));  // 3a
    applyRule(computed, node.getStyle().get(WidgetState::Normal));                        // 3b

    // --- Step 4: Layer state-specific styles on top ---
    if (state != WidgetState::Normal) {
        applyRule(computed, theme_.getDefaultRule(node.getNodeType(), state));  // 4a
        applyRule(computed, node.getStyle().get(state));                        // 4b
    }

    return computed;
}

ComputedLayoutStyle StyleManager::resolveLayout(const SceneNode& node, WidgetState state) const {
    // Layout properties are not inherited. The cascade is layered like appearance.
    // 1. Base: Start with framework defaults.
    // 2. Normal Layer:
    //    a. Apply theme's 'Normal' style.
    //    b. Apply node's own 'Normal' style.
    // 3. State Layer:
    //    a. Apply theme's state-specific style.
    //    b. Apply node's own state-specific style.

    ComputedLayoutStyle computed = FRAMEWORK_DEFAULT_LAYOUT;  // Step 1

    // --- Step 2: Build the full 'Normal' style ---
    applyRule(computed, theme_.getDefaultRule(node.getNodeType(), WidgetState::Normal));  // 2a
    applyRule(computed, node.getStyle().get(WidgetState::Normal));                        // 2b

    // --- Step 3: Layer state-specific styles on top ---
    if (state != WidgetState::Normal) {
        applyRule(computed, theme_.getDefaultRule(node.getNodeType(), state));  // 3a
        applyRule(computed, node.getStyle().get(state));                        // 3b
    }

    return computed;
}

void StyleManager::resolveDirtyStyles(const std::shared_ptr<SceneNode>& root) {
    if (!root) return;

    // If the theme changed since the last pass, the whole tree is stale. A
    // plain root markDirty() would leave clean descendants stale: they are only
    // re-resolved when an inherited property actually changed, and a theme
    // mutation can alter the defaults of any node type. Recursively marking
    // every node dirty forces the cascade below to resolve the whole tree.
    if (theme_.getVersion() != lastResolvedThemeVersion_) {
        lastResolvedThemeVersion_ = theme_.getVersion();
        root->markStyleDirtyRecursive();
    }

    // A theme mutation can also change layout properties; those are tracked by
    // a separate version so a color-only theme tweak re-resolves styles but
    // does not force a relayout. A layout-version change can affect any node in
    // the tree, so the whole tree is marked (a plain root markLayoutDirty()
    // would leave clean subtrees pruned out of the pass). This replaces the
    // Scene's theme-change callback.
    if (theme_.getLayoutVersion() != lastResolvedLayoutThemeVersion_) {
        lastResolvedLayoutThemeVersion_ = theme_.getLayoutVersion();
        root->markLayoutDirtyRecursive();
    }

    // Fast path: nothing in the tree needs a style pass, so we never touch the
    // scene graph at all on clean frames.
    if (!root->style.isSubtreeDirty() && !root->style.isDirty()) return;

    // Pruned top-down traversal. We only enter subtrees whose style-subtree
    // flag is set (a rule somewhere below was modified), and resolve every node
    // whose own cache is dirty. Resolving a node cascades the dirty flag onto
    // its children when the text properties they inherit actually changed;
    // parents are always resolved before their children since each node is
    // enqueued by its parent. The subtree flags are consumed (cleared) right
    // when a node is popped, so no post-pass cleanup is needed and the tree is
    // clean for the next frame.
    std::queue<std::shared_ptr<SceneNode>> nodesToProcess;
    nodesToProcess.push(root);

    while (!nodesToProcess.empty()) {
        auto node = nodesToProcess.front();
        nodesToProcess.pop();

        node->style.clearSubtreeDirty();

        if (node->style.isDirty()) {
            // Baseline of the inherited text properties before this pass; a
            // nullptr means the cache was never populated, so there is nothing
            // to diff against (and every node is dirty on the first pass
            // anyway). Copied before the resolve loop because the resolve
            // overwrites the cache in place.
            const auto* baselinePtr = node->style.getComputedAppearance(WidgetState::Normal);
            const auto baseline =
                baselinePtr ? std::optional<ComputedAppearanceStyle>(*baselinePtr) : std::nullopt;

            for (size_t i = 0; i < kWidgetStateCount; ++i) {
                WidgetState s = static_cast<WidgetState>(i);
                node->style.setComputedAppearance(s, resolveAppearance(*node, s));
                node->style.setComputedLayout(s, resolveLayout(*node, s));
            }
            node->style.markClean();

            const bool inheritedChanged =
                baseline.has_value() &&
                detail::inheritableAppearanceDiffers(
                    *baseline, *node->style.getComputedAppearance(WidgetState::Normal));
            if (inheritedChanged) {
                for (const auto& child : node->children) {
                    child->style.markDirty();
                }
            }
        }

        for (const auto& child : node->children) {
            if (child->style.isSubtreeDirty() || child->style.isDirty()) {
                nodesToProcess.push(child);
            }
        }
    }
}

}  // namespace DxvUI
