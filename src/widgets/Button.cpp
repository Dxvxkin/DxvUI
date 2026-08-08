#include "DxvUI/widgets/Button.h"

#include <utility>

#include "DxvUI/containers/CenterContainer.h"
#include "DxvUI/layout/LayoutManager.h"
#include "DxvUI/style/Colors.h"
#include "DxvUI/style/Theme.h"
#include "DxvUI/widgets/Label.h"

namespace DxvUI {

// --- Self-registration of default styles ---
namespace {
struct ButtonStyleRegistrar {
    ButtonStyleRegistrar() {
        Theme::registerDefaultStyle("Button", {{WidgetState::Normal,
                                                {.backgroundColor = Colors::CornflowerBlue,
                                                 .textColor = Colors::White,
                                                 .borderRadius = 5,
                                                 .cursor = CursorType::Hand,
                                                 .padding = {{5, 5, 5, 5}}}},
                                               {WidgetState::Hovered,
                                                {
                                                    .backgroundColor = Colors::RoyalBlue,
                                                }},
                                               {WidgetState::Pressed,
                                                {
                                                    .backgroundColor = Colors::MidnightBlue,
                                                }}});
    }
};

const ButtonStyleRegistrar registrar;
}  // namespace

std::shared_ptr<Button> Button::create(std::string id, std::string text) {
    return std::shared_ptr<Button>(new Button(std::move(id), std::move(text)));
}

Button::Button(std::string id, std::string text) : SceneNode(std::move(id)) {
    binding_ = UIBinding::create(std::move(text));
}

const char* Button::getNodeType() const { return "Button"; }

void Button::onAttach() {
    SceneNode::onAttach();
    if (!label) {
        label = Label::create(id + "_label", getBinding()->getString().value_or(""));
        label->bind(binding_);

        auto centerContainer = std::make_shared<CenterContainer>(id + "_center");
        centerContainer->addChild(label);

        addChild(centerContainer);
    }
}

Size Button::onMeasure(const Size& availableSize) {
    Size childDesiredSize = {0, 0};
    if (!children.empty()) {
        childDesiredSize = LayoutManager::measureChild(*children.front(), availableSize);
    }

    const auto& computedLayout = getComputedLayout(getCurrentState());
    const auto& padding = computedLayout.padding;

    return LayoutManager::addPadding(childDesiredSize, padding);
}

void Button::onArrange(const Rect& finalRect) {
    if (!children.empty()) {
        const Rect content = LayoutManager::contentRect(*this, finalRect);
        const auto& margin =
            children.front()->getComputedLayout(children.front()->getCurrentState()).margin;
        const Rect childRect = LayoutManager::shrinkRect(content, margin);
        children.front()->arrange(childRect);
    }
}

std::shared_ptr<SceneNode> Button::findNodeAt(int x, int y) {
    if (!isVisible() || !getGlobalBounds().contains(x, y)) {
        return nullptr;
    }
    return shared_from_this();
}

void Button::setText(std::string text) { getBinding()->set(std::move(text)); }

const std::string Button::getText() const { return getBinding()->getString().value_or(""); }

}  // namespace DxvUI