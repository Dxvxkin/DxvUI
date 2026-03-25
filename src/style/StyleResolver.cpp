#include "DxvUI/style/StyleResolver.h"
#include "DxvUI/Scene.h"
#include "DxvUI/style/Theme.h"
#include "DxvUI/style/Colors.h"
#include "DxvUI/Log.h"
#include <string>

#include "DxvUI/SceneNode.h"

namespace DxvUI {

    // Helper to format Color for logging
    inline std::string toString(const Color& c) {
        return "rgba(" + std::to_string(c.r) + ", " + std::to_string(c.g) + ", " + std::to_string(c.b) + ", " + std::to_string(c.a) + ")";
    }

    // --- Default values for the entire framework ---
    static const ComputedAppearanceStyle FRAMEWORK_DEFAULT_APPEARANCE = {
        .backgroundColor = Colors::Transparent, .textColor = Colors::Black, .borderColor = Colors::Transparent,
        .borderThickness = 0, .borderRadius = 0, .cursor = CursorType::Arrow,
        .fontSize = 14, .fontPath = "" // Let renderer decide
    };

    static const ComputedLayoutStyle FRAMEWORK_DEFAULT_LAYOUT = {
        .left = 0, .top = 0, .width = 0, .height = 0,
        .padding = {}, .margin = {},
        .horizontalAlignment = Alignment::Start, .verticalAlignment = Alignment::Start,
        .computedBounds = {}
    };

    // --- Helper to apply a StyleRule over a computed style ---
    void applyRule(ComputedAppearanceStyle& computed, const StyleRule* rule) {
        if (!rule) return;
        if (rule->backgroundColor.has_value()) {
            Log::trace("    Applying backgroundColor: {}", toString(rule->backgroundColor.value()));
            computed.backgroundColor = rule->backgroundColor.value();
        }
        if (rule->textColor.has_value()) {
            Log::trace("    Applying textColor: {}", toString(rule->textColor.value()));
            computed.textColor = rule->textColor.value();
        }
        if (rule->borderColor.has_value()) {
            Log::trace("    Applying borderColor: {}", toString(rule->borderColor.value()));
            computed.borderColor = rule->borderColor.value();
        }
        if (rule->borderThickness.has_value()) {
            Log::trace("    Applying borderThickness: {}", rule->borderThickness.value());
            computed.borderThickness = rule->borderThickness.value();
        }
        if (rule->borderRadius.has_value()) {
            Log::trace("    Applying borderRadius: {}", rule->borderRadius.value());
            computed.borderRadius = rule->borderRadius.value();
        }
        if (rule->cursor.has_value()) {
            Log::trace("    Applying cursor");
            computed.cursor = rule->cursor.value();
        }
        if (rule->fontSize.has_value()) {
            Log::trace("    Applying fontSize: {}", rule->fontSize.value());
            computed.fontSize = rule->fontSize.value();
        }
        if (rule->fontPath.has_value()) {
            Log::trace("    Applying fontPath: {}", rule->fontPath.value());
            computed.fontPath = rule->fontPath.value();
        }
    }

    void applyRule(ComputedLayoutStyle& computed, const StyleRule* rule) {
        if (!rule) return;
        if (rule->left.has_value()) computed.left = rule->left.value();
        if (rule->top.has_value()) computed.top = rule->top.value();
        if (rule->width.has_value()) computed.width = rule->width.value();
        if (rule->height.has_value()) computed.height = rule->height.value();
        if (rule->padding.has_value()) computed.padding = rule->padding.value();
        if (rule->margin.has_value()) computed.margin = rule->margin.value();
        if (rule->horizontalAlignment.has_value()) computed.horizontalAlignment = rule->horizontalAlignment.value();
        if (rule->verticalAlignment.has_value()) computed.verticalAlignment = rule->verticalAlignment.value();
    }

    // --- Main Resolution Logic ---

    ComputedAppearanceStyle StyleResolver::resolveAppearance(const SceneNode& node, WidgetState state) {
        Log::trace("Resolving appearance for node '{}' ({}) in state {}", node.getId(), node.getNodeType(), (int)state);
        auto scene = node.getScene();
        if (!scene) {
            Log::warn("Node '{}' has no scene, returning framework default appearance.", node.getId());
            return FRAMEWORK_DEFAULT_APPEARANCE;
        }

        ComputedAppearanceStyle computed;
        if (auto parent = node.parent.lock()) {
            Log::trace("  1. Inheriting from parent '{}'", parent->getId());
            computed = parent->getComputedAppearance(WidgetState::Normal);
        } else {
            Log::trace("  1. No parent, starting with framework defaults.");
            computed = FRAMEWORK_DEFAULT_APPEARANCE;
        }

        const auto* themeRule = scene->getTheme().getDefaultRule(node.getNodeType());
        if (themeRule) {
            Log::trace("  2. Applying theme rule for type '{}'", node.getNodeType());
            applyRule(computed, themeRule);
        }

        const auto* normalRule = node.getStyle().get(WidgetState::Normal);
        if (normalRule) {
            Log::trace("  3. Applying node's 'Normal' state rule.");
            applyRule(computed, normalRule);
        }

        if (state != WidgetState::Normal) {
            const auto* stateRule = node.getStyle().get(state);
            if (stateRule) {
                Log::trace("  4. Applying node's state-specific rule for state {}.", (int)state);
                applyRule(computed, stateRule);
            }
        }

        Log::trace("  > Final computed background: {}", toString(computed.backgroundColor));
        return computed;
    }

    ComputedLayoutStyle StyleResolver::resolveLayout(const SceneNode& node, WidgetState state) {
        Log::trace("Resolving layout for node '{}' ({}) in state {}", node.getId(), node.getNodeType(), (int)state);
        auto scene = node.getScene();
        if (!scene) {
            Log::warn("Node '{}' has no scene, returning framework default layout.", node.getId());
            return FRAMEWORK_DEFAULT_LAYOUT;
        }

        ComputedLayoutStyle computed = FRAMEWORK_DEFAULT_LAYOUT;
        Log::trace("  1. Starting with framework default layout.");

        const auto* themeRule = scene->getTheme().getDefaultRule(node.getNodeType());
        if (themeRule) {
            Log::trace("  2. Applying theme rule for type '{}'", node.getNodeType());
            applyRule(computed, themeRule);
        }

        const auto* normalRule = node.getStyle().get(WidgetState::Normal);
        if (normalRule) {
            Log::trace("  3. Applying node's 'Normal' state rule.");
            applyRule(computed, normalRule);
        }

        if (state != WidgetState::Normal) {
            const auto* stateRule = node.getStyle().get(state);
            if (stateRule) {
                Log::trace("  4. Applying node's state-specific rule for state {}.", (int)state);
                applyRule(computed, stateRule);
            }
        }

        return computed;
    }
}
