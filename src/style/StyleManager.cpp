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
    .fontSize = 14,
    .fontPath = ""  // Let renderer decide
};

const ComputedLayoutStyle FRAMEWORK_DEFAULT_LAYOUT = {.width = 0,
                                                      .height = 0,
                                                      .padding = {},
                                                      .margin = {},
                                                      .horizontalAlignment = Alignment::Start,
                                                      .verticalAlignment = Alignment::Start,
                                                      .computedBounds = {}};

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

    // If the theme changed since the last pass, the whole tree is stale. Mark
    // the root dirty: the cascade below re-resolves every descendant. A theme
    // mutation can also change layout properties, so schedule a relayout here —
    // this replaces the Scene's theme-change callback.
    if (theme_.getVersion() != lastResolvedThemeVersion_) {
        lastResolvedThemeVersion_ = theme_.getVersion();
        root->style.markDirty();
        root->style.markSubtreeDirty();
        root->markLayoutDirty();
    }

    // Fast path: nothing in the tree needs a style pass, so we never touch the
    // scene graph at all on clean frames.
    if (!root->style.isSubtreeDirty() && !root->style.isDirty()) return;

    // Pruned top-down traversal. We only enter subtrees whose style-subtree
    // flag is set (a rule somewhere below was modified), and resolve every node
    // whose own cache is dirty. Resolving a node cascades the dirty flag onto
    // its children because they inherit text properties; the parents are always
    // resolved before their children since each node is enqueued by its parent.
    // The subtree flags are consumed (cleared) right when a node is popped, so
    // no post-pass cleanup is needed and the tree is clean for the next frame.
    std::queue<std::shared_ptr<SceneNode>> nodesToProcess;
    nodesToProcess.push(root);

    while (!nodesToProcess.empty()) {
        auto node = nodesToProcess.front();
        nodesToProcess.pop();

        node->style.clearSubtreeDirty();

        if (node->style.isDirty()) {
            for (size_t i = 0; i < kWidgetStateCount; ++i) {
                WidgetState s = static_cast<WidgetState>(i);
                node->style.setComputedAppearance(s, resolveAppearance(*node, s));
                node->style.setComputedLayout(s, resolveLayout(*node, s));
            }
            node->style.markClean();

            for (const auto& child : node->children) {
                child->style.markDirty();
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
