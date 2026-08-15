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
// Единый источник имени типа: используется и в getNodeType(), и как ключ
// регистрации стилей, чтобы строка не могла разойтись с типом виджета.
constexpr const char* kWidgetType = "Button";

struct ButtonStyleRegistrar {
    ButtonStyleRegistrar() {
        Theme::registerDefaultStyle(kWidgetType, {{WidgetState::Normal,
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
                                                   }},
                                                  {WidgetState::Disabled,
                                                   {
                                                       .backgroundColor = Colors::LightGray,
                                                       .textColor = Colors::DarkGray,
                                                       .cursor = CursorType::Arrow,
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

const char* Button::getNodeType() const { return kWidgetType; }

void Button::onAttach() {
    SceneNode::onAttach();
    if (!label) {
        label = Label::create(getId() + "_label", getBinding()->getString());
        label->bind(binding_);

        auto centerContainer = std::make_shared<CenterContainer>(getId() + "_center");
        centerContainer->addChild(label);

        addChild(centerContainer);
    }
}

Size Button::onMeasure(const Size& availableSize) {
    Size childDesiredSize = {0, 0};
    if (!getChildren().empty()) {
        childDesiredSize = LayoutManager::measureChild(*getChildren().front(), availableSize);
    }

    const auto& computedLayout = getComputedLayout();
    const auto& padding = computedLayout.padding;

    return LayoutManager::addPadding(childDesiredSize, padding);
}

void Button::onArrange(const Rect& finalRect) {
    if (!getChildren().empty()) {
        const Rect content = LayoutManager::contentRect(*this, finalRect);
        const auto& margin = getChildren()
                                 .front()
                                 ->getComputedLayout(getChildren().front()->getCurrentState())
                                 .margin;
        const Rect childRect = LayoutManager::shrinkRect(content, margin);
        getChildren().front()->arrange(childRect);
    }
}

std::shared_ptr<SceneNode> Button::findNodeAt(int x, int y) {
    if (!isVisible() || !getGlobalBounds().contains(x, y)) {
        return nullptr;
    }
    return shared_from_this();
}

void Button::setText(std::string text) { getBinding()->set(std::move(text)); }

std::string Button::getText() const { return getBinding()->getString(); }

}  // namespace DxvUI