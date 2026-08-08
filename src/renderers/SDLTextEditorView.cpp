#include "DxvUI/renderers/SDLTextEditorView.h"

#include <SDL.h>

#include <algorithm>

#include "DxvUI/interfaces/IRenderer.h"
#include "DxvUI/text/ITextEngine.h"
#include "DxvUI/text/TextEditor.h"

namespace DxvUI {

bool SDLTextEditorView::isCaretVisible() { return (SDL_GetTicks() / kCaretBlinkMs) % 2 == 0; }

void SDLTextEditorView::draw(IRenderer& renderer, ITextEngine& engine, const IFont& font,
                             const TextEditor& editor, const Rect& contentRect,
                             const Options& options) {
    const std::string text = editor.getText();
    const std::string composition = editor.getComposition();

    // Vertical centering: the rasterized line is drawn at its natural height
    // inside the content box so the caret and selection share its geometry.
    const TextMetrics textMetrics = engine.measure(font, text + composition);
    if (textMetrics.height <= 0) {
        return;
    }
    const int textY = contentRect.y + std::max(0, (contentRect.height - textMetrics.height) / 2);

    // Selection highlight is drawn underneath the glyphs.
    if (editor.hasSelection()) {
        const int selStart =
            contentRect.x + engine.measurePrefix(font, text, editor.getSelectionStart());
        const int selEnd =
            contentRect.x + engine.measurePrefix(font, text, editor.getSelectionEnd());
        const int selWidth = selEnd - selStart;
        if (selWidth > 0) {
            renderer.fillRect(Rect{selStart, textY, selWidth, textMetrics.height},
                              options.selectionColor);
        }
    }

    // The text plus the active IME composition is rasterized as one texture (the
    // engine caches it), so glyphs and underline always stay in sync.
    if (textMetrics.width > 0) {
        auto texture = engine.rasterize(font, text + composition, options.textColor);
        if (texture) {
            renderer.drawTexture(
                texture, Rect{contentRect.x, textY, texture->getWidth(), texture->getHeight()});
        }
    }

    // IME composition is marked by an underline below the preedit string.
    if (!composition.empty()) {
        const int compStart = contentRect.x + engine.measurePrefix(font, text, text.size());
        const int compEnd = compStart + engine.measure(font, composition).width;
        const int underlineY = textY + engine.lineMetrics(font).ascent + 1;
        renderer.drawLine(compStart, underlineY, compEnd, underlineY, options.compositionColor);
    }

    // The caret is a one-pixel vertical line; hidden while composing.
    if (options.showCaret && composition.empty() && isCaretVisible()) {
        const int caretX = contentRect.x + engine.measurePrefix(font, text, editor.getCaret());
        renderer.drawLine(caretX, textY, caretX, textY + textMetrics.height, options.caretColor);
    }
}

size_t SDLTextEditorView::hitTestAt(ITextEngine& engine, const IFont& font,
                                    const TextEditor& editor, const Rect& contentRect,
                                    int globalX) {
    const int localX = std::clamp(globalX - contentRect.x, 0, contentRect.width);
    return engine.charIndexAtX(font, editor.getText(), localX);
}

}  // namespace DxvUI
