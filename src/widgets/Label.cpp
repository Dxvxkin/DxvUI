#include "DxvUI/widgets/Label.h"
#include "DxvUI/interfaces/IRenderer.h"
#include "DxvUI/UIBinding.h"
#include "DxvUI/Scene.h"
#include "DxvUI/style/Theme.h"
#include "DxvUI/style/Colors.h"
#include <utility>


#include "DxvUI/Log.h"

namespace DxvUI {

// --- Self-registration of default styles ---
namespace {
struct LabelStyleRegistrar {
    LabelStyleRegistrar() {
        Theme::registerDefaultStyle("Label", {
                                        {
                                            WidgetState::Normal,
                                            {
                                                .backgroundColor = Colors::Transparent,
                                                .textColor = Colors::Black,
                                            }
                                        }
                                    });
    }
};

const LabelStyleRegistrar registrar;
}

std::shared_ptr<Label> Label::create(std::string id, std::string text) {
    return std::make_shared<Label>(std::move(id), std::move(text));
}

Label::Label(std::string id, std::string text)
    : SceneNode(std::move(id)) {
    auto binding = UIBinding::create(text);
    bind(binding);
}

const char* Label::getNodeType() const noexcept {
    return "Label";
}

void Label::setText(std::string newText) {
    auto current_text = getBinding()->getString().value_or("");
    if (current_text != newText) {
        // Сравнение до перемещения
        binding_->set(std::move(newText));
        markLayoutDirty();
    }
}

const std::string Label::getText() const {
    return getBinding()->getString().value_or("");
}

void Label::onChange(const UIBinding& val) {

}

Size Label::measure(const Size& availableSize) {
    if (!isLayoutDirty) return desiredSize;

    const auto& computedAppearance = getComputedAppearance(getCurrentState());
    auto padding = getComputedLayout(getCurrentState()).padding;

    auto scene = getScene();
    if (scene && scene->getRenderer()) {
        auto text = getText();
        Rect measured = scene->getRenderer()->measureText(text, computedAppearance.fontPath,
                                                          computedAppearance.fontSize);
        desiredSize = {
            static_cast<float>(measured.width + padding.left + padding.right),
            static_cast<float>(measured.height + padding.top + padding.bottom)
        };
    } else {
        desiredSize = {0, 0};
    }

    const auto& computedLayout = getComputedLayout(getCurrentState());
    if (computedLayout.width > 0) desiredSize.width = computedLayout.width;
    if (computedLayout.height > 0) desiredSize.height = computedLayout.height;

    return desiredSize;
}

void Label::draw(IRenderer& renderer) {
    const auto& computedAppearance = getComputedAppearance(getCurrentState());
    const auto& computedLayout = getComputedLayout(getCurrentState());

    renderer.fillRoundRect(computedLayout.computedBounds, computedAppearance.borderRadius,
                           computedAppearance.backgroundColor, {
                               computedAppearance.borderColor, computedAppearance.borderThickness
                           });

    // Определяем, нужно ли пересоздавать текстуру.
    // Это нужно, только если изменился текст, путь к шрифту или его размер.
    // Изменение цвета текста или фона не требует новой текстуры.
    // КРИТИЧЕСКОЕ ИСПРАВЛЕНИЕ: SDLRenderer "запекает" цвет в текстуру, поэтому изменение цвета ТАКЖЕ требует обновления.
    auto text = getText();
    const bool needsTextureUpdate = !textTexture
                                    || cachedText != text
                                    || cachedFontPath != computedAppearance.fontPath
                                    || cachedFontSize != computedAppearance.fontSize
                                    || cachedTextColor != computedAppearance.textColor;

    if (needsTextureUpdate) {
        cachedText = text;
        cachedFontPath = computedAppearance.fontPath;
        cachedFontSize = computedAppearance.fontSize;
        cachedTextColor = computedAppearance.textColor;

        // КРИТИЧЕСКОЕ ИСПРАВЛЕНИЕ: Установка состояния рендерера ПЕРЕД созданием текстуры.
        // SDLRenderer использует свое внутреннее состояние для рендеринга текста.
        renderer.setFont(cachedFontPath, cachedFontSize);
        renderer.setDrawColor(cachedTextColor);

        textTexture = renderer.createTextTexture(cachedText);
        // Теперь этот вызов сработает корректно.
    }

    if (textTexture) {
        //Временное решение для учета отступов
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

}
