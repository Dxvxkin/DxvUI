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

    TextMetrics textMetrics = engine.measure(font, text + composition);
    // Empty buffer (no composition): render the placeholder instead, if one was
    // provided. Its metrics drive the same alignment/scrolling the text would
    // get, so an empty focused field still shows the caret in the right spot.
    const bool showPlaceholder = text.empty() && composition.empty();
    if (showPlaceholder) {
        if (options.placeholder.empty()) {
            return;
        }
        textMetrics = engine.measure(font, options.placeholder);
        if (textMetrics.height <= 0) {
            return;
        }
    }
    const int textY = contentRect.y + std::max(0, (contentRect.height - textMetrics.height) / 2);

    // Horizontal alignment only applies while the text fits: once it overflows
    // the box, the horizontal scroll takes over and alignment is meaningless.
    int alignOffsetX = 0;
    if (textMetrics.width <= contentRect.width) {
        switch (options.horizontalAlign) {
            case Alignment::Center:
                alignOffsetX = (contentRect.width - textMetrics.width) / 2;
                break;
            case Alignment::End:
                alignOffsetX = contentRect.width - textMetrics.width;
                break;
            case Alignment::Start:
            case Alignment::Stretch:
                break;
        }
    }
    const int textAreaX = contentRect.x + alignOffsetX;

    // Horizontal scroll: keep the caret visible, clamped so the text never
    // scrolls past its own end.
    const int maxScroll = std::max(0, textMetrics.width - contentRect.width);
    const int caretX = engine.measurePrefix(font, text, editor.getCaret());
    if (caretX < scrollOffsetX_) {
        scrollOffsetX_ = caretX;
    } else if (caretX > scrollOffsetX_ + contentRect.width) {
        scrollOffsetX_ = caretX - contentRect.width;
    }
    scrollOffsetX_ = std::clamp(scrollOffsetX_, 0, maxScroll);

    // Visible slice of the buffer: rasterize only the visible range.
    const size_t visibleStart = engine.charIndexAtX(font, text, scrollOffsetX_);
    const size_t visibleEnd = engine.charIndexAtX(font, text, scrollOffsetX_ + contentRect.width);

    renderer.pushClipRect(contentRect);

    // Selection highlight (clipped to the visible window).
    if (editor.hasSelection()) {
        const int selStart = engine.measurePrefix(font, text, editor.getSelectionStart());
        const int selEnd = engine.measurePrefix(font, text, editor.getSelectionEnd());
        const int selLeft = std::max(selStart, scrollOffsetX_);
        const int selRight = std::min(selEnd, scrollOffsetX_ + contentRect.width);
        if (selRight > selLeft) {
            renderer.fillRect(Rect{textAreaX + selLeft - scrollOffsetX_, textY, selRight - selLeft,
                                   textMetrics.height},
                              options.selectionColor);
        }
    }

    if (visibleEnd > visibleStart) {
        const std::string visibleText = text.substr(visibleStart, visibleEnd - visibleStart);
        const int sliceX =
            textAreaX - scrollOffsetX_ + engine.measurePrefix(font, text, visibleStart);
        if (auto texture = engine.rasterize(font, visibleText, options.textColor)) {
            renderer.drawTexture(texture,
                                 Rect{sliceX, textY, texture->getWidth(), texture->getHeight()});
        }
    }

    // Empty buffer: draw the placeholder in its muted color at the same
    // position the text would occupy. The caret is still drawn while focused.
    if (showPlaceholder) {
        if (auto texture = engine.rasterize(font, options.placeholder, options.placeholderColor)) {
            renderer.drawTexture(texture, Rect{textAreaX - scrollOffsetX_, textY,
                                               texture->getWidth(), texture->getHeight()});
        }
    }

    // IME composition is short and always at the end; rasterize separately.
    if (!composition.empty()) {
        const int compStart =
            textAreaX - scrollOffsetX_ + engine.measurePrefix(font, text, text.size());
        const int compEnd = compStart + engine.measure(font, composition).width;
        const int underlineY = textY + engine.lineMetrics(font).ascent + 1;
        renderer.drawLine(compStart, underlineY, compEnd, underlineY, options.compositionColor);
        if (auto texture = engine.rasterize(font, composition, options.textColor)) {
            renderer.drawTexture(texture,
                                 Rect{compStart, textY, texture->getWidth(), texture->getHeight()});
        }
    }

    if (options.showCaret && composition.empty() && isCaretVisible()) {
        const int visibleCaretX = textAreaX + caretX - scrollOffsetX_;
        renderer.drawLine(visibleCaretX, textY, visibleCaretX, textY + textMetrics.height,
                          options.caretColor);
    }
    renderer.popClipRect();
}

size_t SDLTextEditorView::hitTestAt(ITextEngine& engine, const IFont& font,
                                    const TextEditor& editor, const Rect& contentRect, int globalX,
                                    Alignment horizontalAlign) {
    int alignOffsetX = 0;
    const int textWidth = engine.measure(font, editor.getText()).width;
    if (textWidth <= contentRect.width) {
        switch (horizontalAlign) {
            case Alignment::Center:
                alignOffsetX = (contentRect.width - textWidth) / 2;
                break;
            case Alignment::End:
                alignOffsetX = contentRect.width - textWidth;
                break;
            case Alignment::Start:
            case Alignment::Stretch:
                break;
        }
    }
    const int localX = std::max(0, globalX - (contentRect.x + alignOffsetX) + scrollOffsetX_);
    return engine.charIndexAtX(font, editor.getText(), localX);
}

}  // namespace DxvUI
