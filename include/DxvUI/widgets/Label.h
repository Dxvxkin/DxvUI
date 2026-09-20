#ifndef DXVUI_LABEL_H
#define DXVUI_LABEL_H

#include <memory>
#include <string>

#include "DxvUI/SceneNode.h"

namespace DxvUI {

class ITextEngine;
struct IFont;
struct TextLayout;

class Label : public SceneNode {
   public:
    static std::shared_ptr<Label> create(std::string id, std::string text = "");

    explicit Label(std::string id = "", std::string text = "");

    void setText(std::string text);
    std::string getText() const;

    // --- Overrides ---
    void onChange(const UIBinding& binding) override;

    const char* getNodeType() const noexcept override;
    // ---------------------

   protected:
    Size onMeasure(const Size& availableSize) override;
    void onPaint(PaintContext& pc) override;

   private:
    // Stage 4: cache string between Change to avoid mutex+allocation each frame
    std::string cachedText_;

    // Stage 5: cached font handle + last TextLayout. The cache key is
    // (engine, font family, font size, text); getLayout() rebuilds on a
    // mismatch, so onMeasure and onPaint share one layout and clean frames
    // perform no engine lookup and no TextLayout copy (zero allocations).
    std::shared_ptr<IFont> cachedFont_;
    std::string cachedFontFamily_;
    int cachedFontSize_ = 0;
    const ITextEngine* cachedEngine_ = nullptr;
    std::shared_ptr<TextLayout> cachedLayout_;

    // Returns a valid layout for the current cachedText_/appearance, rebuilding
    // the cache entry when the key changed. An empty layout means the font is
    // unavailable (callers treat it as zero size).
    const TextLayout& getLayout(ITextEngine& engine, const ComputedAppearanceStyle& appearance);
    void invalidateLayoutCache();
};

}  // namespace DxvUI

#endif  // DXVUI_LABEL_H
