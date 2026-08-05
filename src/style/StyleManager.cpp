#include "DxvUI/style/StyleManager.h"

#include <queue>

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

const ComputedLayoutStyle FRAMEWORK_DEFAULT_LAYOUT = {.left = 0,
                                                      .top = 0,
                                                      .width = 0,
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
    if (rule->backgroundColor.has_value()) computed.backgroundColor = rule->backgroundColor.value();
    if (rule->textColor.has_value()) computed.textColor = rule->textColor.value();
    if (rule->borderColor.has_value()) computed.borderColor = rule->borderColor.value();
    if (rule->borderThickness.has_value()) computed.borderThickness = rule->borderThickness.value();
    if (rule->borderRadius.has_value()) computed.borderRadius = rule->borderRadius.value();
    if (rule->cursor.has_value()) computed.cursor = rule->cursor.value();
    if (rule->fontSize.has_value()) computed.fontSize = rule->fontSize.value();
    if (rule->fontPath.has_value()) computed.fontPath = rule->fontPath.value();
}

void StyleManager::applyRule(ComputedLayoutStyle& computed, const StyleRule* rule) {
    if (!rule) return;
    if (rule->left.has_value()) computed.left = rule->left.value();
    if (rule->top.has_value()) computed.top = rule->top.value();
    if (rule->width.has_value()) computed.width = rule->width.value();
    if (rule->height.has_value()) computed.height = rule->height.value();
    if (rule->padding.has_value()) computed.padding = rule->padding.value();
    if (rule->margin.has_value()) computed.margin = rule->margin.value();
    if (rule->horizontalAlignment.has_value())
        computed.horizontalAlignment = rule->horizontalAlignment.value();
    if (rule->verticalAlignment.has_value())
        computed.verticalAlignment = rule->verticalAlignment.value();
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
        computed.textColor = parentStyle.textColor;
        computed.fontSize = parentStyle.fontSize;
        computed.fontPath = parentStyle.fontPath;
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

    // Use a queue for a breadth-first (top-down) traversal.
    // This ensures parent styles are resolved before their children need to
    // inherit from them.
    std::queue<std::shared_ptr<SceneNode>> nodesToProcess;
    nodesToProcess.push(root);

    while (!nodesToProcess.empty()) {
        auto node = nodesToProcess.front();
        nodesToProcess.pop();

        if (node->style.isDirty()) {
            for (int i = 0; i < 4; ++i) {
                WidgetState s = static_cast<WidgetState>(i);
                node->style.setComputedAppearance(s, resolveAppearance(*node, s));
                node->style.setComputedLayout(s, resolveLayout(*node, s));
            }
            node->style.markClean();

            // Resolving a node can change the text properties its children
            // inherit, so the whole subtree below a dirty node must be
            // re-resolved as well. The children are already queued below and
            // get processed in this same pass, parent before child.
            for (const auto& child : node->children) {
                child->style.markDirty();
            }
        }

        for (const auto& child : node->children) {
            nodesToProcess.push(child);
        }
    }
}

}  // namespace DxvUI
