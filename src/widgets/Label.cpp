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

Label::Label(std::string id, std::string text) : SceneNode(std::move(id)), cachedText_(text) {
    auto binding = UIBinding::create(text);
    bind(binding);
}

const char* Label::getNodeType() const noexcept { return kWidgetType; }

void Label::setText(std::string newText) {
    cachedText_ = newText;
    getBinding()->set(newText);
}

std::string Label::getText() const {
    // Stage 4: return cached to avoid mutex+allocation, fallback to binding if empty cache
    if (!cachedText_.empty() || !getBinding()) {
        return cachedText_;
    }
    return getBinding()->getString();
}

void Label::onChange(const UIBinding& binding) {
    // Stage 4: cache string between Change to avoid mutex+allocation each frame
    cachedText_ = binding.getString();
    markLayoutDirty();
}

Size Label::onMeasure(const Size& availableSize) {
    const auto& computedAppearance = getComputedAppearance();
    const Thickness insets = LayoutManager::contentInsets(*this);

    auto scene = getScene();
    if (scene && scene->getRenderer()) {
        auto& engine = scene->getRenderer()->getTextEngine();
        auto font =
            engine.getFontForFamily(computedAppearance.fontFamily, computedAppearance.fontSize);
        if (!font) {
            return {0, 0};
        }
        // Stage 4: use cached text to avoid mutex+allocation
        const auto& text = cachedText_;
        if (text.empty()) {
            return LayoutManager::addPadding({0, 0}, insets);
        }
        // Stage 3: measure via TextLayout (glyph atlas, cached per font+text)
        auto layout = engine.layoutText(*font, text);
        return LayoutManager::addPadding(
            {static_cast<float>(layout.metrics.width),
             static_cast<float>(layout.metrics.height > 0 ? layout.metrics.height
                                                           : layout.lineMetrics.lineHeight)},
            insets);
    }
    return {0, 0};
}

void Label::onPaint(PaintContext& pc) {
    const auto& computedAppearance = getComputedAppearance();

    // Stage 4: use cached text
    const auto& text = cachedText_;
    if (text.empty()) {
        return;
    }

    auto& engine = pc.text();
    auto font = engine.getFontForFamily(computedAppearance.fontFamily, computedAppearance.fontSize);
    if (!font) {
        return;
    }

    const Rect contentRect = LayoutManager::contentRect(*this, getGlobalBounds());

    // Stage 3: single lookup of TextLayout + single drawLayout with tint.
    // Alignment/truncation handled inside drawLayout (previously duplicated).
    TextLayout layout = engine.layoutText(*font, text);

    TextPaint paint;
    paint.color = computedAppearance.textColor;
    paint.align = computedAppearance.textAlign;
    paint.verticalAlign = computedAppearance.textAlignVertical;
    paint.truncate = true;

    engine.drawLayout(pc.canvas(), layout, RectF(contentRect), paint);
}

}  // namespace DxvUI
