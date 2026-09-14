#include "DxvUI/text/DefaultTextEditorView.h"

#include <algorithm>
#include <chrono>

#include "DxvUI/interfaces/ICanvas.h"
#include "DxvUI/interfaces/ITextEngine.h"
#include "DxvUI/text/TextEditor.h"

namespace DxvUI {

bool DefaultTextEditorView::isCaretVisible(double timeMs) {
    if (timeMs <= 0) {
        // Fallback when FrameInfo has no time (tests): use steady_clock
        auto now = std::chrono::steady_clock::now();
        timeMs = std::chrono::duration<double, std::milli>(now.time_since_epoch()).count();
    }
    return (static_cast<uint64_t>(timeMs / kCaretBlinkMs) % 2) == 0;
}

void DefaultTextEditorView::draw(PaintContext& pc, const IFont& font, const TextEditor& editor,
                                 const Rect& contentRect, const Options& options) {
    ICanvas& canvas = pc.canvas();
    ITextEngine& engine = pc.text();

    const std::string text = editor.getText();
    const std::string composition = editor.getComposition();

    const bool showPlaceholder = text.empty() && composition.empty();
    std::string displayText;
    TextLayout layout;

    if (showPlaceholder) {
        if (options.placeholder.empty()) {
            return;
        }
        displayText = options.placeholder;
        layout = engine.layoutText(font, displayText);
        if (layout.metrics.height <= 0 && layout.lineMetrics.lineHeight <= 0) {
            return;
        }
    } else {
        displayText = text + composition;
        layout = engine.layoutText(font, displayText);
    }

    const int textHeight = layout.metrics.height > 0 ? layout.metrics.height
                                                      : layout.lineMetrics.lineHeight;
    const int textY = contentRect.y + std::max(0, (contentRect.height - textHeight) / 2);

    // Horizontal alignment only when text fits
    int alignOffsetX = 0;
    if (layout.metrics.width <= contentRect.width) {
        switch (options.horizontalAlign) {
            case Alignment::Center:
                alignOffsetX = (contentRect.width - layout.metrics.width) / 2;
                break;
            case Alignment::End:
                alignOffsetX = contentRect.width - layout.metrics.width;
                break;
            case Alignment::Start:
            case Alignment::Stretch:
            default:
                break;
        }
    }
    const int textAreaX = contentRect.x + alignOffsetX;

    // Caret X in layout coordinates
    const int caretX = layout.caretXAt(editor.getCaret());

    // Scroll to keep caret visible
    const int maxScroll = std::max(0, layout.metrics.width - contentRect.width);
    if (caretX < scrollOffsetX_) {
        scrollOffsetX_ = caretX;
    } else if (caretX > scrollOffsetX_ + contentRect.width) {
        scrollOffsetX_ = caretX - contentRect.width;
    }
    scrollOffsetX_ = std::clamp(scrollOffsetX_, 0, maxScroll);

    ClipGuard clipGuard(canvas, contentRect, true);

    // Selection highlight (clipped to visible window)
    if (editor.hasSelection()) {
        const int selStart = layout.caretXAt(editor.getSelectionStart());
        const int selEnd = layout.caretXAt(editor.getSelectionEnd());
        const int selLeft = std::max(selStart, scrollOffsetX_);
        const int selRight = std::min(selEnd, scrollOffsetX_ + contentRect.width);
        if (selRight > selLeft) {
            canvas.fillRect(RectF{static_cast<float>(textAreaX + selLeft - scrollOffsetX_),
                                  static_cast<float>(textY),
                                  static_cast<float>(selRight - selLeft),
                                  static_cast<float>(textHeight)},
                            Fill{options.selectionColor});
        }
    }

    // Draw text glyphs (or placeholder) with tint, clipped by scroll
    {
        TextPaint paint;
        paint.color = showPlaceholder ? options.placeholderColor : options.textColor;
        paint.align = Alignment::Start;
        paint.verticalAlign = Alignment::Start;
        paint.truncate = false; // we handle scroll manually

        // Build a box that represents the visible area shifted by scroll
        // We draw the full layout at (textAreaX - scrollOffsetX_, textY) and rely on clip.
        RectF box{static_cast<float>(textAreaX - scrollOffsetX_), static_cast<float>(textY),
                  static_cast<float>(layout.metrics.width),
                  static_cast<float>(textHeight)};

        // For placeholder, same path
        engine.drawLayout(canvas, layout, box, paint);
    }

    // IME composition underline (composition is at end of text)
    if (!composition.empty()) {
        // Composition starts after text
        TextLayout textOnlyLayout = engine.layoutText(font, text);
        const int compStart = textAreaX - scrollOffsetX_ + textOnlyLayout.metrics.width;
        TextLayout compLayout = engine.layoutText(font, composition);
        const int compEnd = compStart + compLayout.metrics.width;
        const int underlineY = textY + engine.lineMetrics(font).ascent + 1;
        canvas.drawLine(PointF(static_cast<float>(compStart), static_cast<float>(underlineY)),
                        PointF(static_cast<float>(compEnd), static_cast<float>(underlineY)),
                        Stroke{options.compositionColor});
        // Composition text already drawn as part of displayText (text+composition) above,
        // but if we want separate tint, we already drew it. To avoid double draw, we drew
        // displayText = text+composition, so composition is already there. The underline is extra.
        // If we want to avoid double, we could draw text and composition separately.
        // For simplicity, we already drew text+composition together, so nothing more.
    }

    if (options.showCaret && composition.empty() && isCaretVisible(pc.frame().timeMs)) {
        const int visibleCaretX = textAreaX + caretX - scrollOffsetX_;
        canvas.drawLine(PointF(static_cast<float>(visibleCaretX), static_cast<float>(textY)),
                        PointF(static_cast<float>(visibleCaretX),
                               static_cast<float>(textY + textHeight)),
                        Stroke{options.caretColor});
    }
}

size_t DefaultTextEditorView::hitTestAt(ITextEngine& engine, const IFont& font,
                                        const TextEditor& editor, const Rect& contentRect,
                                        int globalX, Alignment horizontalAlign) {
    // Build layout for current text
    TextLayout layout = engine.layoutText(font, editor.getText());

    int alignOffsetX = 0;
    if (layout.metrics.width <= contentRect.width) {
        switch (horizontalAlign) {
            case Alignment::Center:
                alignOffsetX = (contentRect.width - layout.metrics.width) / 2;
                break;
            case Alignment::End:
                alignOffsetX = contentRect.width - layout.metrics.width;
                break;
            case Alignment::Start:
            case Alignment::Stretch:
            default:
                break;
        }
    }

    const int localX = std::max(0, globalX - (contentRect.x + alignOffsetX) + scrollOffsetX_);
    return layout.charIndexAtX(localX);
}

} // namespace DxvUI
