#ifndef DXVUI_TEXTEDIT_H
#define DXVUI_TEXTEDIT_H

#include <functional>
#include <memory>
#include <string>

#include "DxvUI/SceneNode.h"
#include "DxvUI/text/TextEditor.h"
#include "DxvUI/text/TextEditorView.h"

namespace DxvUI {

/**
 * @brief Single-line editable text field widget.
 *
 * Owns a backend-neutral TextEditor model and renders it through a
 * TextEditorView (an SDLTextEditorView is created by default). The widget
 * bridges scene events to editing operations: a mouse press places the caret
 * and requests focus (the event manager grants it), dragging extends the
 * selection, and the editing keys (arrows, Home/End, Backspace, Delete,
 * Ctrl+A/Z/Y/X/C/V, Enter) edit the buffer. Keyboard events only reach the
 * widget while it owns focus.
 *
 * The text lives in the editor model: read/edit it through getText()/setText()
 * or getEditor() directly. setText() records a single undo entry, like a
 * user-typed replacement.
 */
class TextEdit : public SceneNode {
   public:
    using SubmitCallback = std::function<void(const std::string&)>;

    static std::shared_ptr<TextEdit> create(std::string id, std::string text = "");

    explicit TextEdit(std::string id, std::string text = "");
    ~TextEdit() override = default;

    TextEdit(const TextEdit&) = delete;
    TextEdit& operator=(const TextEdit&) = delete;

    const char* getNodeType() const noexcept override;

    // --- Text ---
    std::string getText() const;
    void setText(std::string text);

    /**
     * @brief Gets the underlying editor model (for tests / advanced usage).
     */
    TextEditor& getEditor() { return editor_; }
    const TextEditor& getEditor() const { return editor_; }

    /**
     * @brief Registers a callback invoked when Enter is pressed while focused.
     * @param cb Receives the current text.
     */
    void setOnSubmit(SubmitCallback cb) { onSubmit_ = std::move(cb); }

   protected:
    Size onMeasure(const Size& availableSize) override;
    void drawContent(IRenderer& renderer) override;

   private:
    // --- Event handling ---
    void handleKeyDown(DxvEvent& event);
    void handleTextInput(DxvEvent& event);
    void handleMouseDown(DxvEvent& event);
    void handleMouseDrag(DxvEvent& event);

    void moveCaretBy(int delta, bool extend);
    void moveCaretToBoundary(size_t boundary, bool extend);

    // Returns the text engine and font for the current style, or false when the
    // widget is not attached to a renderer or the font could not be loaded.
    bool getEditContext(ITextEngine** engine, const IFont** font);

    TextEditor editor_;
    std::unique_ptr<TextEditorView> view_;
    SubmitCallback onSubmit_;
    // Selection anchor for shift+arrows and mouse dragging.
    size_t selectionAnchor_ = 0;
};

}  // namespace DxvUI

#endif  // DXVUI_TEXTEDIT_H
