#ifndef DXVUI_TEXTEDITORVIEW_H
#define DXVUI_TEXTEDITORVIEW_H

#include <cstddef>
#include <string>

#include "DxvUI/core.h"

namespace DxvUI {

class IRenderer;
class ITextEngine;
struct IFont;
class TextEditor;

/**
 * @brief Renders a TextEditor model into a single-line box.
 *
 * The view is the presentation side of text editing: it owns none of the
 * editing state (text, caret, selection — those live in TextEditor) and keeps
 * only presentation-only state (the caret blink phase). It draws text with the
 * selection highlight, the composition (IME preedit) and the caret through the
 * backend-neutral IRenderer/ITextEngine pair, so it does not leak backend
 * types, mirroring how widgets draw. One instance can serve many editors: the
 * per-editor state is passed in on every call.
 */
class TextEditorView {
   public:
    virtual ~TextEditorView() = default;

    /**
     * @brief Colors and toggles for the editor's decoration.
     */
    struct Options {
        Color textColor{0, 0, 0, 255};
        Color selectionColor{120, 160, 255, 140};
        Color caretColor{0, 0, 0, 255};
        Color compositionColor{0, 0, 0, 255};
        // Shown while the buffer is empty. The widget hides it (e.g. while it
        // owns focus) by not passing it; an empty string means "no placeholder".
        std::string placeholder;
        Color placeholderColor{160, 160, 160, 255};
        // Horizontal alignment of the text inside the content rect. Only matters
        // when the text is narrower than the rect (no horizontal scroll); a
        // Start value keeps the current left-aligned behavior.
        Alignment horizontalAlign = Alignment::Start;
        // Set false while the widget does not own keyboard focus so the caret
        // stays hidden (blinking is handled inside the view).
        bool showCaret = true;
    };

    /**
     * @brief Draws the editor's content into the content rect.
     *
     * The text (plus the active IME composition, if any) is drawn at natural
     * size, centered vertically in the rect. The selection highlight, the
     * composition underline and the caret are computed from the editor state via
     * ITextEngine::measurePrefix()/lineMetrics() and drawn underneath/around the
     * text. A caret is only drawn when it is visible and no composition is
     * active.
     * @param renderer The renderer to draw with.
     * @param engine The text engine to measure/rasterize with.
     * @param font A font obtained from engine.getFont().
     * @param editor The model to render.
     * @param contentRect The box (after padding) the editor occupies.
     * @param options Presentation options.
     */
    virtual void draw(IRenderer& renderer, ITextEngine& engine, const IFont& font,
                      const TextEditor& editor, const Rect& contentRect,
                      const Options& options) = 0;

    /**
     * @brief Maps a click inside the content rect to a caret byte offset.
     *
     * The offset always lands on a UTF-8 code point boundary and is clamped to
     * the text length, so the result is always a valid caret position. Positions
     * beyond the visible width map to the last whole code point that fits.
     * @param engine The text engine used for the hit test.
     * @param font A font obtained from engine.getFont().
     * @param editor The model being clicked.
     * @param contentRect The box (after padding) the editor occupies.
     * @param globalX The click's x in the same coordinate space as contentRect.
     * @param horizontalAlign The same alignment value passed to draw(), so the
     * hit test maps the click to the same text position the renderer used.
     * @return The caret byte offset for the click.
     */
    virtual size_t hitTestAt(ITextEngine& engine, const IFont& font, const TextEditor& editor,
                             const Rect& contentRect, int globalX,
                             Alignment horizontalAlign) = 0;
};

}  // namespace DxvUI

#endif  // DXVUI_TEXTEDITORVIEW_H
