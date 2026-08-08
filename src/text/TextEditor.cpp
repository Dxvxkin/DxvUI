#include "DxvUI/text/TextEditor.h"

#include <algorithm>
#include <utility>

namespace DxvUI {

namespace {
// Whether byte `b` is a UTF-8 continuation byte (0b10xxxxxx).
bool isContinuationByte(unsigned char b) { return (b & 0xC0) == 0x80; }
}  // namespace

TextEditor::TextEditor(std::string text) : text_(std::move(text)) {
    // A fresh buffer starts with the caret at its end (a typical "initial
    // value" for a text field), regardless of the caret's default 0.
    caret_ = text_.size();
}

void TextEditor::setText(std::string text) {
    if (text == text_) return;
    pushUndo();
    text_ = std::move(text);
    caret_ = std::min(caret_, text_.size());
    clearSelection();
    notifyChanged();
}

void TextEditor::setCaret(size_t byteOffset) { caret_ = std::min(byteOffset, text_.size()); }

void TextEditor::moveCaretLeft() { caret_ = prevCodePoint(text_, caret_); }

void TextEditor::moveCaretRight() { caret_ = nextCodePoint(text_, caret_); }

void TextEditor::moveCaretHome() { caret_ = 0; }

void TextEditor::moveCaretEnd() { caret_ = text_.size(); }

void TextEditor::insertText(const std::string& utf8) {
    if (utf8.empty()) return;
    pushUndo();
    if (hasSelection()) {
        deleteSelectionNoUndo();
    }
    text_.insert(caret_, utf8);
    caret_ += utf8.size();
    notifyChanged();
}

void TextEditor::backspace() {
    if (hasSelection()) {
        deleteSelection();
        return;
    }
    if (caret_ == 0) return;
    pushUndo();
    const size_t start = prevCodePoint(text_, caret_);
    text_.erase(start, caret_ - start);
    caret_ = start;
    notifyChanged();
}

void TextEditor::deleteForward() {
    if (hasSelection()) {
        deleteSelection();
        return;
    }
    if (caret_ >= text_.size()) return;
    pushUndo();
    const size_t end = nextCodePoint(text_, caret_);
    text_.erase(caret_, end - caret_);
    notifyChanged();
}

void TextEditor::setSelection(size_t start, size_t end) {
    start = std::min(start, text_.size());
    end = std::min(end, text_.size());
    if (start > end) std::swap(start, end);
    selectionStart_ = start;
    selectionEnd_ = end;
}

void TextEditor::clearSelection() { selectionStart_ = selectionEnd_ = 0; }

std::string TextEditor::selectedText() const {
    if (!hasSelection()) return "";
    return text_.substr(selectionStart_, selectionEnd_ - selectionStart_);
}

void TextEditor::deleteSelection() {
    if (!hasSelection()) return;
    pushUndo();
    deleteSelectionNoUndo();
    notifyChanged();
}

void TextEditor::selectAll() { setSelection(0, text_.size()); }

void TextEditor::undo() {
    if (undoStack_.empty()) return;
    redoStack_.push_back({text_, caret_, selectionStart_, selectionEnd_});
    Snapshot prev = undoStack_.back();
    undoStack_.pop_back();
    text_ = std::move(prev.text);
    caret_ = std::min(prev.caret, text_.size());
    // The selection is view state, but restoring it makes "select, type, undo"
    // round-trip back to the pre-edit selection instead of a bare caret.
    setSelection(prev.selectionStart, prev.selectionEnd);
    notifyChanged();
}

void TextEditor::redo() {
    if (redoStack_.empty()) return;
    undoStack_.push_back({text_, caret_, selectionStart_, selectionEnd_});
    Snapshot next = redoStack_.back();
    redoStack_.pop_back();
    text_ = std::move(next.text);
    caret_ = std::min(next.caret, text_.size());
    setSelection(next.selectionStart, next.selectionEnd);
    notifyChanged();
}

void TextEditor::setComposition(const std::string& text, size_t start, size_t length) {
    composition_ = text;
    compositionStart_ = std::min(start, text_.size());
    compositionLength_ = std::min(length, text_.size() - compositionStart_);
}

void TextEditor::notifyChanged() {
    if (onChange_) onChange_();
}

size_t TextEditor::prevCodePoint(const std::string& text, size_t caret) {
    if (caret == 0) return 0;
    size_t i = caret - 1;
    while (i > 0 && isContinuationByte(static_cast<unsigned char>(text[i]))) {
        --i;
    }
    return i;
}

size_t TextEditor::nextCodePoint(const std::string& text, size_t caret) {
    const size_t len = text.size();
    if (caret >= len) return len;
    size_t i = caret + 1;
    while (i < len && isContinuationByte(static_cast<unsigned char>(text[i]))) {
        ++i;
    }
    return i;
}

void TextEditor::pushUndo() {
    undoStack_.push_back({text_, caret_, selectionStart_, selectionEnd_});
    if (undoStack_.size() > kMaxUndoDepth) {
        undoStack_.erase(undoStack_.begin());
    }
    redoStack_.clear();
}

void TextEditor::deleteSelectionNoUndo() {
    text_.erase(selectionStart_, selectionEnd_ - selectionStart_);
    caret_ = selectionStart_;
    clearSelection();
}

}  // namespace DxvUI
