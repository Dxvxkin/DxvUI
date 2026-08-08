#include "DxvUI/widgets/Label.h"

#include <algorithm>
#include <utility>

#include "DxvUI/Log.h"
#include "DxvUI/Scene.h"
#include "DxvUI/UIBinding.h"
#include "DxvUI/interfaces/IRenderer.h"
#include "DxvUI/layout/LayoutManager.h"
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
    auto current_text = getBinding()->getString();
    if (current_text != newText) {
        // Сравнение до перемещения
        binding_->set(std::move(newText));
    }
}

std::string Label::getText() const { return getBinding()->getString(); }

void Label::onChange(const UIBinding& /*val*/) {
    // Binding-driven text changes must re-measure the label: otherwise the
    // bounds stay stale and the new text gets clipped. setText() goes through
    // binding_->set() -> onChange(), so it is covered here too.
    markLayoutDirty();
}

Size Label::onMeasure(const Size& availableSize) {
    const auto& computedAppearance = getComputedAppearance();
    auto padding = getComputedLayout().padding;

    auto scene = getScene();
    if (scene && scene->getRenderer()) {
        auto text = getText();
        Rect measured = scene->getRenderer()->measureText(text, computedAppearance.fontPath,
                                                          computedAppearance.fontSize);
        return LayoutManager::addPadding(
            {static_cast<float>(measured.width), static_cast<float>(measured.height)}, padding);
    }
    return {0, 0};
}

void Label::drawContent(IRenderer& renderer) {
    const auto& computedAppearance = getComputedAppearance();

    // Определяем, нужно ли пересоздавать текстуру.
    // Это нужно, только если изменился текст или параметры, влияющие на
    // отрисовку текста (шрифт, размер, цвет). SDLRenderer "запекает" цвет в
    // текстуру, поэтому сравнение идёт по влияющим на текст полям: если
    // textColor/fontSize/fontPath или текст изменились, текстура пересоздаётся.
    auto text = getText();
    const bool needsTextureUpdate = !textTexture || cachedText != text ||
                                    cachedAppearance.textColor != computedAppearance.textColor ||
                                    cachedAppearance.fontSize != computedAppearance.fontSize ||
                                    cachedAppearance.fontPath != computedAppearance.fontPath;

    if (needsTextureUpdate) {
        cachedText = text;
        cachedAppearance = computedAppearance;

        renderer.setFont(computedAppearance.fontPath, computedAppearance.fontSize);
        renderer.setDrawColor(computedAppearance.textColor);

        textTexture = renderer.createTextTexture(cachedText);
    }

    if (textTexture) {
        const Rect contentRect = LayoutManager::contentRect(*this, getGlobalBounds());

        // Рисуем текстуру в натуральном размере, центрируя в контент-боксе.
        // Увеличение не применяется (иначе текст размывается); если бокс меньше
        // текстуры, она уменьшается, чтобы влезть.
        const int drawW = std::min(textTexture->getWidth(), contentRect.width);
        const int drawH = std::min(textTexture->getHeight(), contentRect.height);
        const Rect dstRect = {contentRect.x + (contentRect.width - drawW) / 2,
                              contentRect.y + (contentRect.height - drawH) / 2, drawW, drawH};
        renderer.drawTexture(textTexture, dstRect);
        // Эта функция не принимает цвет, т.к. он уже в текстуре
    }
}

}  // namespace DxvUI
