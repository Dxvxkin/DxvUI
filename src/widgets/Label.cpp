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
        auto& engine = scene->getRenderer()->getTextEngine();
        auto font = engine.getFont(computedAppearance.fontPath, computedAppearance.fontSize);
        if (!font) {
            return {0, 0};
        }
        auto text = getText();
        auto measured = engine.measure(*font, text);
        return LayoutManager::addPadding(
            {static_cast<float>(measured.width), static_cast<float>(measured.height)}, padding);
    }
    return {0, 0};
}

void Label::drawContent(IRenderer& renderer) {
    const auto& computedAppearance = getComputedAppearance();

    auto text = getText();
    if (text.empty()) {
        return;
    }

    // Текстура текста кешируется внутри текстового движка по ключу
    // (шрифт, текст, цвет), поэтому виджету не нужно собственное кеширование.
    auto& engine = renderer.getTextEngine();
    auto font = engine.getFont(computedAppearance.fontPath, computedAppearance.fontSize);
    if (!font) {
        return;
    }

    const Rect contentRect = LayoutManager::contentRect(*this, getGlobalBounds());

    // Обрезаем текст до видимого префикса, который помещается в ширину контент-бокса,
    // чтобы не пытаться создать текстуру больше ширины лейбла и не превысить лимит SDL (16384px).
    std::string drawText = text;
    if (contentRect.width > 0) {
        const size_t fitBytes = engine.charIndexAtX(*font, text, contentRect.width);
        if (fitBytes > 0 && fitBytes < text.size()) {
            drawText = text.substr(0, fitBytes);
        } else if (fitBytes == 0) {
            drawText.clear();
        }
    }

    if (drawText.empty()) {
        return;
    }

    auto textTexture = engine.rasterize(*font, drawText, computedAppearance.textColor);

    if (textTexture) {
        // Рисуем текстуру в натуральном размере, центрируя в контент-боксе.
        // Увеличение не применяется (иначе текст размывается); если бокс меньше
        // текстуры, она уменьшается, чтобы влезть.
        const int drawW = std::min(textTexture->getWidth(), contentRect.width);
        const int drawH = std::min(textTexture->getHeight(), contentRect.height);
        const Rect dstRect = {contentRect.x + (contentRect.width - drawW) / 2,
                              contentRect.y + (contentRect.height - drawH) / 2, drawW, drawH};
        renderer.drawTexture(textTexture, dstRect);
    }
}

}  // namespace DxvUI
