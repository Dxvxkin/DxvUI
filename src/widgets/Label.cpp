#include "DxvUI/widgets/Label.h"

#include <utility>

#include "DxvUI/Log.h"
#include "DxvUI/Scene.h"
#include "DxvUI/UIBinding.h"
#include "DxvUI/interfaces/IRenderer.h"
#include "DxvUI/style/Colors.h"
#include "DxvUI/style/Theme.h"

namespace DxvUI {

// --- Self-registration of default styles ---
namespace {
struct LabelStyleRegistrar {
    LabelStyleRegistrar() {
        Theme::registerDefaultStyle("Label", {{WidgetState::Normal,
                                               {
                                                   .backgroundColor = Colors::Transparent,
                                                   .textColor = Colors::Black,
                                               }}});
    }
};

const LabelStyleRegistrar registrar;
}  // namespace

std::shared_ptr<Label> Label::create(std::string id, std::string text) {
    return std::make_shared<Label>(std::move(id), std::move(text));
}

Label::Label(std::string id, std::string text) : SceneNode(std::move(id)) {
    auto binding = UIBinding::create(text);
    bind(binding);
}

const char* Label::getNodeType() const noexcept { return "Label"; }

void Label::setText(std::string newText) {
    auto current_text = getBinding()->getString().value_or("");
    if (current_text != newText) {
        // Сравнение до перемещения
        binding_->set(std::move(newText));
        markLayoutDirty();
    }
}

const std::string Label::getText() const { return getBinding()->getString().value_or(""); }

void Label::onChange(const UIBinding& val) {}

Size Label::measureOverride(const Size& availableSize) {
    const auto& computedAppearance = getComputedAppearance(getCurrentState());
    auto padding = getComputedLayout(getCurrentState()).padding;

    auto scene = getScene();
    if (scene && scene->getRenderer()) {
        auto text = getText();
        Rect measured = scene->getRenderer()->measureText(text, computedAppearance.fontPath,
                                                          computedAppearance.fontSize);
        return {static_cast<float>(measured.width + padding.left + padding.right),
                static_cast<float>(measured.height + padding.top + padding.bottom)};
    }
    return {0, 0};
}

void Label::draw(IRenderer& renderer) {
    const auto& computedAppearance = getComputedAppearance(getCurrentState());
    const auto& computedLayout = getComputedLayout(getCurrentState());

    renderer.fillRoundRect(computedLayout.computedBounds, computedAppearance.borderRadius,
                           computedAppearance.backgroundColor,
                           {computedAppearance.borderColor, computedAppearance.borderThickness});

    // Определяем, нужно ли пересоздавать текстуру.
    // Это нужно, только если изменился текст или параметры, влияющие на
    // отрисовку текста (шрифт, размер, цвет). SDLRenderer "запекает" цвет в
    // текстуру, поэтому сравнение идёт по всему computed appearance: если оно
    // изменилось, текстура пересоздаётся.
    auto text = getText();
    const bool needsTextureUpdate =
        !textTexture || cachedText != text || cachedAppearance != computedAppearance;

    if (needsTextureUpdate) {
        cachedText = text;
        cachedAppearance = computedAppearance;

        renderer.setFont(computedAppearance.fontPath, computedAppearance.fontSize);
        renderer.setDrawColor(computedAppearance.textColor);

        textTexture = renderer.createTextTexture(cachedText);
    }

    if (textTexture) {
        // Временное решение для учета отступов
        auto dstRect = computedLayout.computedBounds;
        dstRect.x += computedLayout.padding.left;
        dstRect.y += computedLayout.padding.top;
        dstRect.width -= computedLayout.padding.right + computedLayout.padding.left;
        dstRect.height -= computedLayout.padding.bottom + computedLayout.padding.top;
        renderer.drawTexture(textTexture, dstRect);
        // Эта функция не принимает цвет, т.к. он уже в текстуре
    }

    SceneNode::draw(renderer);
}

}  // namespace DxvUI
