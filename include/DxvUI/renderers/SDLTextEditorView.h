#ifndef DXVUI_SDLTEXTEDITORVIEW_H
#define DXVUI_SDLTEXTEDITORVIEW_H

#include <cstdint>

#include "DxvUI/text/TextEditorView.h"

namespace DxvUI {

/**
 * @brief Default TextEditorView implementation.
 *
 * All drawing goes through the backend-neutral IRenderer/ITextEngine pair; the
 * only SDL dependency is the caret blink timer (SDL_GetTicks), which has no
 * portable equivalent in the interfaces. The instance keeps no per-editor
 * state, so a single view can be shared by every editor in a scene.
 */
class SDLTextEditorView : public TextEditorView {
   public:
    SDLTextEditorView() = default;
    ~SDLTextEditorView() override = default;

    SDLTextEditorView(const SDLTextEditorView&) = delete;
    SDLTextEditorView& operator=(const SDLTextEditorView&) = delete;

    void draw(IRenderer& renderer, ITextEngine& engine, const IFont& font, const TextEditor& editor,
              const Rect& contentRect, const Options& options) override;

    size_t hitTestAt(ITextEngine& engine, const IFont& font, const TextEditor& editor,
                     const Rect& contentRect, int globalX) override;

   private:
    // Half-period of the caret blink in ms; the caret is visible for one
    // period, hidden for the next.
    static constexpr uint32_t kCaretBlinkMs = 530;

    // Current horizontal scroll position in pixels.
    int scrollOffsetX_ = 0;

    static bool isCaretVisible();
};

}  // namespace DxvUI

#endif  // DXVUI_SDLTEXTEDITORVIEW_H
