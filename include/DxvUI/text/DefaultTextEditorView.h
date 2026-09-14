#ifndef DXVUI_DEFAULTTEXTEDITORVIEW_H
#define DXVUI_DEFAULTTEXTEDITORVIEW_H

#include <cstdint>

#include "DxvUI/text/TextEditorView.h"

namespace DxvUI {

/**
 * @brief Default TextEditorView implementation using TextLayout + glyph atlas.
 *
 * Lives in text/ (not backend/) and has no SDL dependency: caret blink uses
 * FrameInfo::timeMs (or chrono fallback). Glyphs are white textures tinted via
 * ICanvas::TextureDraw::tint. Scrolling no longer creates substring textures —
 * it draws a slice of the layout via xOffsets.
 *
 * Presentation-only state (scrollOffsetX_) is kept here, so one view instance
 * can be shared, but each TextEdit owns its own view (as before).
 */
class DefaultTextEditorView : public TextEditorView {
   public:
    DefaultTextEditorView() = default;
    ~DefaultTextEditorView() override = default;

    DefaultTextEditorView(const DefaultTextEditorView&) = delete;
    DefaultTextEditorView& operator=(const DefaultTextEditorView&) = delete;

    void draw(PaintContext& pc, const IFont& font, const TextEditor& editor,
              const Rect& contentRect, const Options& options) override;

    size_t hitTestAt(ITextEngine& engine, const IFont& font, const TextEditor& editor,
                     const Rect& contentRect, int globalX,
                     Alignment horizontalAlign) override;

   private:
    static constexpr uint32_t kCaretBlinkMs = 530;

    int scrollOffsetX_ = 0;

    static bool isCaretVisible(double timeMs);
};

}  // namespace DxvUI

#endif  // DXVUI_DEFAULTTEXTEDITORVIEW_H
