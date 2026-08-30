#include "DxvUI/widgets/Label.h"

#include <algorithm>
#include <utility>

#include "DxvUI/Scene.h"
#include "DxvUI/UIBinding.h"
#include "DxvUI/interfaces/IRenderer.h"
#include "DxvUI/layout/LayoutManager.h"
#include "DxvUI/style/Colors.h"
#include "DxvUI/style/Theme.h"

namespace DxvUI {

// --- Self-registration of default styles ---
namespace {
// Единый источник имени типа: используется и в getNodeType(), и как ключ
// регистрации стилей, чтобы строка не могла разойтись с типом виджета.
constexpr const char* kWidgetType = "Label";

struct LabelStyleRegistrar {
    LabelStyleRegistrar() {
        Theme::registerDefaultStyle(kWidgetType, {{WidgetState::Normal,
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

const char* Label::getNodeType() const noexcept { return kWidgetType; }

void Label::setText(std::string newText) {
    // bound: set() no-ops when the value did not change, so no Change (and no
    // onChange -> relayout) fires for an identical text. No need to compare here.
    getBinding()->set(std::move(newText));
}

std::string Label::getText() const { return getBinding()->getString(); }

void Label::onChange(const UIBinding& /*binding*/) {
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
        auto font =
            engine.getFontForFamily(computedAppearance.fontFamily, computedAppearance.fontSize);
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
    auto font = engine.getFontForFamily(computedAppearance.fontFamily, computedAppearance.fontSize);
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
        // Рисуем текстуру в натуральном размере, выравнивая в контент-боксе по
        // style.textAlign/textAlignVertical. Увеличение не применяется (иначе
        // текст размывается); если бокс меньше текстуры, она уменьшается.
        const int drawW = std::min(textTexture->getWidth(), contentRect.width);
        const int drawH = std::min(textTexture->getHeight(), contentRect.height);

        int drawX = contentRect.x;
        int drawY = contentRect.y;
        switch (computedAppearance.textAlign) {
            case Alignment::Center:
                drawX += (contentRect.width - drawW) / 2;
                break;
            case Alignment::End:
                drawX += contentRect.width - drawW;
                break;
            case Alignment::Start:
            case Alignment::Stretch:
                break;
        }
        switch (computedAppearance.textAlignVertical) {
            case Alignment::Center:
                drawY += (contentRect.height - drawH) / 2;
                break;
            case Alignment::End:
                drawY += contentRect.height - drawH;
                break;
            case Alignment::Start:
            case Alignment::Stretch:
                break;
        }

        renderer.drawTexture(textTexture, {drawX, drawY, drawW, drawH});
    }
}

}  // namespace DxvUI
