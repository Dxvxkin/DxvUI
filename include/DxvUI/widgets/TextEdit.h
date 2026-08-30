#ifndef DXVUI_TEXTEDIT_H
#define DXVUI_TEXTEDIT_H

#include <functional>
#include <memory>
#include <string>
#include <utility>

#include "DxvUI/SceneNode.h"
#include "DxvUI/text/TextEditor.h"
#include "DxvUI/text/TextEditorView.h"

namespace DxvUI {

class IClipboard;

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
 *
 * The value is observable through the standard EventType::Change channel, like
 * the other data-bearing widgets (Label, Checkbox, Slider): the widget binds a
 * UIBinding that mirrors the editor's text, so subscribing with
 * field->on(EventType::Change, ...) fires on every buffer mutation. Read the
 * value with getText() or event.getTarget()->getBinding()->getString(). React
 * to changes via on(Change), not via getEditor().setChangeCallback() (the model
 * callback is owned internally by the widget to sync the binding).
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

    // --- Placeholder ---
    // A hint shown while the buffer is empty and the field does not own focus.
    // It disappears as soon as the field is focused (HTML convention).
    void setPlaceholder(std::string placeholder) { placeholder_ = std::move(placeholder); }
    std::string getPlaceholder() const { return placeholder_; }

    /**
     * @brief Gets the underlying editor model (for tests / advanced usage).
     *
     * The model owns caret/selection/undo/redo/IME state that the widget does
     * not re-expose. For reacting to value changes prefer on(EventType::Change)
     * over the model callback; the callback is used internally to mirror the
     * text into the widget's binding.
     */
    TextEditor& getEditor() { return editor_; }
    const TextEditor& getEditor() const { return editor_; }

    /**
     * @brief Registers a callback invoked when Enter is pressed while focused.
     * @param cb Receives the current text.
     */
    void setOnSubmit(SubmitCallback cb) { onSubmit_ = std::move(cb); }

    // --- Validation ---
    // Gates input at the model level: insertText()/setText() in the
    // underlying TextEditor silently drop edits that would produce rejected
    // text, so e.g. a digits-only field never shows a letter typed into it.
    // nullptr (default) disables validation. Built-in validators live in
    // DxvUI::validators: digitsOnly(), range(), hex(), decimal(), regex().
    void setValidator(std::shared_ptr<validators::ITextValidator> validator) {
        editor_.setValidator(std::move(validator));
    }
    const std::shared_ptr<validators::ITextValidator>& getValidator() const {
        return editor_.getValidator();
    }

   protected:
    // Model mutation -> binding mirror -> onChange(): invalidates the layout so
    // a text-length change triggers a remeasure on the next pass.
    void onChange(const UIBinding& binding) override;

    Size onMeasure(const Size& availableSize) override;
    void drawContent(IRenderer& renderer) override;
    void onEvent(DxvEvent& event) override;

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

    // Returns the clipboard from the renderer, or nullptr if not attached.
    IClipboard* getClipboard();

    TextEditor editor_;
    std::unique_ptr<TextEditorView> view_;
    SubmitCallback onSubmit_;
    // Selection anchor for shift+arrows and mouse dragging.
    size_t selectionAnchor_ = 0;
    // Hint text shown while the buffer is empty and the field is not focused.
    std::string placeholder_;
};

}  // namespace DxvUI

#endif  // DXVUI_TEXTEDIT_H
