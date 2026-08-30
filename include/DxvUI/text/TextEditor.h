#ifndef DXVUI_TEXTEDITOR_H
#define DXVUI_TEXTEDITOR_H

#include <cstddef>
#include <functional>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "DxvUI/text/ITextValidator.h"

namespace DxvUI {

/**
 * @brief Backend-neutral single-line text editing model.
 *
 * Owns the buffer and the caret (and, for future steps, selection, undo/redo
 * and IME composition) and implements every editing operation with UTF-8-aware
 * code point boundaries. The model knows nothing about rendering, events or
 * the scene graph; a widget (e.g. TextField) bridges scene events to these
 * operations and renders the result.
 *
 * All indices are byte offsets into the UTF-8 buffer, so they can be passed
 * straight to std::string::substr and ITextEngine::measurePrefix.
 *
 * The change callback fires only on buffer mutations (insert/delete/set/
 * undo/redo); caret and selection moves are transient view state and are
 * re-read on every draw, so they need no notification.
 */
class TextEditor {
   public:
    using ChangeCallback = std::function<void()>;

    static constexpr size_t kMaxUndoDepth = 64;

    explicit TextEditor(std::string text = "");

    // --- Buffer ---
    std::string getText() const { return text_; }
    bool empty() const { return text_.empty(); }
    size_t length() const { return text_.size(); }
    void setText(std::string text);

    // --- Caret ---
    size_t getCaret() const { return caret_; }
    void setCaret(size_t byteOffset);
    void moveCaretLeft();
    void moveCaretRight();
    void moveCaretHome();
    void moveCaretEnd();

    // --- Editing ---
    void insertText(const std::string& utf8);
    void backspace();
    void deleteForward();

    // --- Selection (wired to the UI in a later step; model already supports it) ---
    bool hasSelection() const { return selectionEnd_ > selectionStart_; }
    void setSelection(size_t start, size_t end);
    void clearSelection();
    size_t getSelectionStart() const { return selectionStart_; }
    size_t getSelectionEnd() const { return selectionEnd_; }
    std::string selectedText() const;
    void deleteSelection();
    void selectAll();

    // --- Undo/redo ---
    void undo();
    void redo();
    bool canUndo() const { return !undoStack_.empty(); }
    bool canRedo() const { return !redoStack_.empty(); }

    // --- Validation ---
    // A validator gates every buffer mutation (insertText/setText): the edit
    // is dropped when the resulting text would be rejected. nullptr (default)
    // disables validation. Backspace/Delete always shorten the buffer and are
    // never blocked. See DxvUI/text/ITextValidator.h for the built-in
    // validators (validators::digitsOnly(), range(), hex(), ...).
    void setValidator(std::shared_ptr<validators::ITextValidator> validator) {
        validator_ = std::move(validator);
    }
    const std::shared_ptr<validators::ITextValidator>& getValidator() const { return validator_; }

    /**
     * @brief Stores an IME composition in progress (v1 stores only; merging
     * the composition into the buffer comes with the IME integration step).
     * @param text The composed text.
     * @param start Byte offset where the composition begins.
     * @param length Byte length of the composed segment.
     */
    void setComposition(const std::string& text, size_t start, size_t length);
    // The text currently being composed by the IME, or "" when no composition
    // is active.
    std::string getComposition() const { return composition_; }

    // --- Change notification ---
    void setChangeCallback(ChangeCallback cb) { onChange_ = std::move(cb); }
    void notifyChanged();

   private:
    // Returns the byte offset of the start of the code point preceding caret.
    static size_t prevCodePoint(const std::string& text, size_t caret);
    // Returns the byte offset of the start of the code point following caret.
    static size_t nextCodePoint(const std::string& text, size_t caret);

    struct Snapshot {
        std::string text;
        size_t caret;
        size_t selectionStart;
        size_t selectionEnd;
    };
    void pushUndo();
    // Erases the current selection and moves the caret to its start, without
    // touching the undo stack (callers that snapshot first use this for a
    // single undo entry covering "delete selection + typed replacement").
    void deleteSelectionNoUndo();

    std::string text_;
    size_t caret_ = 0;
    size_t selectionStart_ = 0;
    size_t selectionEnd_ = 0;
    std::vector<Snapshot> undoStack_;
    std::vector<Snapshot> redoStack_;
    std::string composition_;
    size_t compositionStart_ = 0;
    size_t compositionLength_ = 0;
    ChangeCallback onChange_;
    std::shared_ptr<validators::ITextValidator> validator_;
};

}  // namespace DxvUI

#endif  // DXVUI_TEXTEDITOR_H
