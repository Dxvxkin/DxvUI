#include "DxvUI/widgets/Checkbox.h"

#include <algorithm>
#include <utility>

#include "DxvUI/DxvEvent.h"
#include "DxvUI/interfaces/IRenderer.h"
#include "DxvUI/layout/LayoutManager.h"
#include "DxvUI/style/Colors.h"
#include "DxvUI/style/Theme.h"
#include "DxvUI/widgets/Label.h"

namespace DxvUI {

// --- Self-registration of default styles ---
namespace {
struct CheckboxStyleRegistrar {
    CheckboxStyleRegistrar() {
        Theme::registerDefaultStyle(
            "Checkbox", {{WidgetState::Normal,
                          {.textColor = Colors::Black,
                           .borderColor = Colors::Gray,
                           .borderThickness = 1,
                           .borderRadius = 3,
                           .cursor = CursorType::Hand,
                           .padding = {{2, 2, 2, 2}}}},
                         {WidgetState::Hovered, {.borderColor = Colors::CornflowerBlue}},
                         {WidgetState::Pressed, {.borderColor = Colors::MidnightBlue}},
                         {WidgetState::Focused, {.borderColor = Colors::CornflowerBlue}}});
    }
};

const CheckboxStyleRegistrar registrar;
}  // namespace

std::shared_ptr<Checkbox> Checkbox::create(std::string id, std::string text) {
    return std::shared_ptr<Checkbox>(new Checkbox(std::move(id), std::move(text)));
}

Checkbox::Checkbox(std::string id, std::string text) : SceneNode(std::move(id)) {
    // bind() wires the binding subscription so that setChecked() -> binding_->set()
    // dispatches a Change event with this checkbox as the target.
    bind(UIBinding::create(false));
    labelText = std::move(text);
}

const char* Checkbox::getNodeType() const { return "Checkbox"; }

void Checkbox::onAttach() {
    SceneNode::onAttach();
    if (!label) {
        label = Label::create(getId() + "_label", labelText);
        addChild(label);
    }
}

Size Checkbox::onMeasure(const Size& availableSize) {
    float labelWidth = 0.0f;
    float labelHeight = 0.0f;
    if (!getChildren().empty()) {
        const Size labelDesiredSize =
            LayoutManager::measureChild(*getChildren().front(), availableSize);
        labelWidth = labelDesiredSize.width;
        labelHeight = labelDesiredSize.height;
    }

    const float width = kBoxSize + kGap + labelWidth;
    const float height = std::max(kBoxSize, labelHeight);
    return LayoutManager::addPadding({width, height}, getComputedLayout().padding);
}

void Checkbox::onArrange(const Rect& finalRect) {
    if (getChildren().empty()) {
        return;
    }

    const Rect content = LayoutManager::contentRect(*this, finalRect);
    const Rect labelRect = LayoutManager::alignChild(
        *getChildren().front(), getChildren().front()->getDesiredSize(),
        {content.x + static_cast<int>(kBoxSize) + static_cast<int>(kGap), content.y,
         content.width - static_cast<int>(kBoxSize) - static_cast<int>(kGap),
         static_cast<int>(kBoxSize)},
        {.horizontal = false, .vertical = true});
    getChildren().front()->arrange(labelRect);
}

void Checkbox::drawContent(IRenderer& renderer) {
    const auto& appearance = getComputedAppearance();
    const Rect content = LayoutManager::contentRect(*this, getGlobalBounds());
    const Rect boxRect = {content.x, content.y, static_cast<int>(kBoxSize),
                          static_cast<int>(kBoxSize)};

    renderer.fillRoundRect(
        boxRect, 2, Colors::White,
        {.color = appearance.borderColor, .thickness = appearance.borderThickness});

    if (isChecked()) {
        // Закрашенный скруглённый квадрат, центрированный в боксе: отступ = 25%
        // размера бокса, залитая часть = 50% — масштабируется вместе с kBoxSize.
        const int inset = static_cast<int>(kBoxSize) / 4;
        const int fillSize = static_cast<int>(kBoxSize) - 2 * inset;
        renderer.fillRoundRect({boxRect.x + inset, boxRect.y + inset, fillSize, fillSize},
                               fillSize / 4, appearance.textColor);
    }
}

void Checkbox::onEvent(DxvEvent& event) {
    switch (event.type) {
        case EventType::Click:
        case EventType::KeyDown:
            if (event.type == EventType::KeyDown &&
                !(event.key.sym == KeyCode::Space && getCurrentState() == WidgetState::Focused)) {
                break;
            }
            toggle();
            event.stopPropagation();
            break;
        default:
            break;
    }
}

std::shared_ptr<SceneNode> Checkbox::findNodeAt(int x, int y) {
    if (!isVisible() || !getGlobalBounds().contains(x, y)) {
        return nullptr;
    }
    return shared_from_this();
}

void Checkbox::setChecked(bool checked) { getBinding()->set(checked); }

bool Checkbox::isChecked() const { return getBinding()->getBoolOr(false); }

void Checkbox::setText(std::string text) {
    if (label) {
        label->setText(std::move(text));
    } else {
        labelText = std::move(text);
    }
}

std::string Checkbox::getText() const { return label ? label->getText() : labelText; }

void Checkbox::toggle() { setChecked(!isChecked()); }

}  // namespace DxvUI
