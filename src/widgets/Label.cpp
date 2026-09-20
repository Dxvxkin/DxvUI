#include "DxvUI/widgets/Label.h"

#include <algorithm>
#include <utility>

#include "DxvUI/Scene.h"
#include "DxvUI/UIBinding.h"
#include "DxvUI/interfaces/ITextEngine.h"
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
    // The string copy lives in cachedText_; the binding takes the moved string.
    auto binding = UIBinding::create(std::move(text));
    bind(binding);
}

const char* Label::getNodeType() const noexcept { return kWidgetType; }

void Label::setText(std::string newText) {
    cachedText_ = std::move(newText);
    invalidateLayoutCache();
    getBinding()->set(cachedText_);
}

std::string Label::getText() const {
    // Stage 4: cachedText_ is the single source of truth (constructor, setText
    // and onChange all keep it in sync), so no binding access and no mutex here.
    return cachedText_;
}

void Label::onChange(const UIBinding& binding) {
    // Stage 4: cache string between Change to avoid mutex+allocation each frame
    cachedText_ = binding.getString();
    invalidateLayoutCache();
    markLayoutDirty();
}

void Label::invalidateLayoutCache() { cachedLayout_.reset(); }

const TextLayout& Label::getLayout(ITextEngine& engine, const ComputedAppearanceStyle& appearance) {
    const bool fontKeyChanged = cachedEngine_ != &engine ||
                                cachedFontSize_ != appearance.fontSize ||
                                cachedFontFamily_ != appearance.fontFamily;
    const bool textChanged = cachedLayout_ && cachedLayout_->text != cachedText_;

    // Stage 5 cache hit: same engine, font and text — reuse the layout with no
    // engine lookup and no TextLayout copy (zero allocations on clean frames).
    if (cachedLayout_ && !fontKeyChanged && !textChanged) {
        return *cachedLayout_;
    }

    if (fontKeyChanged) {
        cachedFont_ = engine.getFontForFamily(appearance.fontFamily, appearance.fontSize);
        cachedEngine_ = &engine;
        cachedFontSize_ = appearance.fontSize;
        cachedFontFamily_ = appearance.fontFamily;
    }

    cachedLayout_.reset();
    if (!cachedFont_) {
        // Font unavailable: treat the text as zero size.
        static const TextLayout kEmptyLayout;
        return kEmptyLayout;
    }
    cachedLayout_ = std::make_shared<TextLayout>(engine.layoutText(*cachedFont_, cachedText_));
    return *cachedLayout_;
}

Size Label::onMeasure(const Size& /*availableSize*/) {
    const auto& computedAppearance = getComputedAppearance();
    const Thickness insets = LayoutManager::contentInsets(*this);

    auto scene = getScene();
    if (scene && scene->getTextEngine()) {
        auto& engine = *scene->getTextEngine();
        // Stage 4: use cached text to avoid mutex+allocation
        if (cachedText_.empty()) {
            return LayoutManager::addPadding({0, 0}, insets);
        }
        // Stage 5: measure from the shared cached layout (no font/layout work
        // per call when the key is unchanged).
        const TextLayout& layout = getLayout(engine, computedAppearance);
        if (layout.empty()) {
            return LayoutManager::addPadding({0, 0}, insets);
        }
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
    if (cachedText_.empty()) {
        return;
    }

    auto& engine = pc.text();
    const TextLayout& layout = getLayout(engine, computedAppearance);
    if (layout.empty()) {
        return;
    }

    const Rect contentRect = LayoutManager::contentRect(*this, getGlobalBounds());

    // Stage 5: paint the same layout that onMeasure produced — reused every
    // frame with no engine lookup. Alignment/truncation handled in drawLayout.
    TextPaint paint;
    paint.color = computedAppearance.textColor;
    paint.align = computedAppearance.textAlign;
    paint.verticalAlign = computedAppearance.textAlignVertical;
    paint.truncate = true;

    engine.drawLayout(pc.canvas(), layout, RectF(contentRect), paint);
}

}  // namespace DxvUI
