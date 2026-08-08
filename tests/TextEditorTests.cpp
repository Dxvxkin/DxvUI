#include <gtest/gtest.h>

#include <string>

#include "DxvUI/text/TextEditor.h"

using namespace DxvUI;

namespace {

// "Аб" = U+0410 (2 bytes) + U+0431 (2 bytes); each is a single code point.
constexpr const char* kCyrillic = "\xD0\x90\xD0\xB1";

}  // namespace

TEST(TextEditor, InitialState) {
    TextEditor editor;
    EXPECT_TRUE(editor.empty());
    EXPECT_EQ(editor.getCaret(), 0u);
    EXPECT_FALSE(editor.hasSelection());
}

TEST(TextEditor, ConstructorTakesText) {
    TextEditor editor("hello");
    EXPECT_EQ(editor.getText(), "hello");
    EXPECT_EQ(editor.length(), 5u);
    EXPECT_EQ(editor.getCaret(), 5u);
}

TEST(TextEditor, InsertText) {
    TextEditor editor;
    editor.insertText("ab");
    EXPECT_EQ(editor.getText(), "ab");
    EXPECT_EQ(editor.getCaret(), 2u);

    editor.setCaret(1);
    editor.insertText("X");
    EXPECT_EQ(editor.getText(), "aXb");
    EXPECT_EQ(editor.getCaret(), 2u);
}

TEST(TextEditor, InsertUtf8MovesCaretByBytes) {
    TextEditor editor;
    editor.insertText(kCyrillic);
    // "Аб" is 4 bytes, and the caret lands past them all.
    EXPECT_EQ(editor.length(), 4u);
    EXPECT_EQ(editor.getCaret(), 4u);
}

TEST(TextEditor, BackspaceDeletesWholeCodePoint) {
    TextEditor editor(kCyrillic);  // "Аб"
    editor.backspace();
    // One full code point (2 bytes) is removed, not just one byte.
    EXPECT_EQ(editor.getText(), "\xD0\x90");
    EXPECT_EQ(editor.getCaret(), 2u);
    editor.backspace();
    EXPECT_TRUE(editor.empty());
    EXPECT_EQ(editor.getCaret(), 0u);
}

TEST(TextEditor, BackspaceAtStartIsNoOp) {
    TextEditor editor("abc");
    editor.setCaret(0);
    editor.backspace();
    EXPECT_EQ(editor.getText(), "abc");
}

TEST(TextEditor, DeleteForwardRemovesWholeCodePoint) {
    TextEditor editor(kCyrillic);  // "Аб"
    editor.setCaret(0);
    editor.deleteForward();
    EXPECT_EQ(editor.getText(), "\xD0\xB1");
    EXPECT_EQ(editor.getCaret(), 0u);
}

TEST(TextEditor, DeleteForwardAtEndIsNoOp) {
    TextEditor editor("abc");
    editor.deleteForward();
    EXPECT_EQ(editor.getText(), "abc");
}

TEST(TextEditor, MoveCaretSkipsWholeCodePoints) {
    TextEditor editor(kCyrillic);  // "Аб"
    editor.moveCaretHome();
    EXPECT_EQ(editor.getCaret(), 0u);
    editor.moveCaretRight();
    // One code point = 2 bytes.
    EXPECT_EQ(editor.getCaret(), 2u);
    editor.moveCaretRight();
    EXPECT_EQ(editor.getCaret(), 4u);
    editor.moveCaretLeft();
    EXPECT_EQ(editor.getCaret(), 2u);
    editor.moveCaretHome();
    EXPECT_EQ(editor.getCaret(), 0u);
    editor.moveCaretEnd();
    EXPECT_EQ(editor.getCaret(), 4u);
}

TEST(TextEditor, SelectionReplaceSingleUndoEntry) {
    TextEditor editor("abcdef");
    editor.setSelection(2, 5);
    ASSERT_EQ(editor.selectedText(), "cde");
    editor.insertText("X");
    EXPECT_EQ(editor.getText(), "abXf");
    EXPECT_EQ(editor.getCaret(), 3u);
    EXPECT_FALSE(editor.hasSelection());

    // One undo restores the selection state (both delete + insert).
    editor.undo();
    EXPECT_EQ(editor.getText(), "abcdef");
    EXPECT_TRUE(editor.hasSelection());
    EXPECT_EQ(editor.getSelectionStart(), 2u);
    EXPECT_EQ(editor.getSelectionEnd(), 5u);
}

TEST(TextEditor, UndoRedo) {
    TextEditor editor;
    editor.insertText("a");
    editor.insertText("b");
    editor.insertText("c");
    EXPECT_EQ(editor.getText(), "abc");

    editor.undo();
    EXPECT_EQ(editor.getText(), "ab");
    editor.undo();
    EXPECT_EQ(editor.getText(), "a");
    editor.undo();
    EXPECT_TRUE(editor.empty());

    // Nothing left to undo.
    editor.undo();
    EXPECT_TRUE(editor.empty());

    editor.redo();
    EXPECT_EQ(editor.getText(), "a");
    editor.redo();
    EXPECT_EQ(editor.getText(), "ab");
    editor.redo();
    EXPECT_EQ(editor.getText(), "abc");
    editor.redo();
    EXPECT_EQ(editor.getText(), "abc");
}

TEST(TextEditor, UndoRestoresCaret) {
    TextEditor editor("hello");
    editor.setCaret(0);
    editor.insertText(">>");
    ASSERT_EQ(editor.getText(), ">>hello");
    ASSERT_EQ(editor.getCaret(), 2u);

    editor.undo();
    EXPECT_EQ(editor.getText(), "hello");
    EXPECT_EQ(editor.getCaret(), 0u);
}

TEST(TextEditor, UndoDepthIsBounded) {
    TextEditor editor;
    for (size_t i = 0; i < TextEditor::kMaxUndoDepth + 10; ++i) {
        editor.insertText("x");
    }
    // Only the most recent kMaxUndoDepth edits survive.
    size_t undos = 0;
    while (editor.canUndo()) {
        editor.undo();
        ++undos;
    }
    EXPECT_EQ(undos, TextEditor::kMaxUndoDepth);
}

TEST(TextEditor, NewEditClearsRedoStack) {
    TextEditor editor;
    editor.insertText("a");
    editor.undo();
    ASSERT_TRUE(editor.canRedo());

    editor.insertText("b");
    EXPECT_FALSE(editor.canRedo());
}

TEST(TextEditor, SetTextResetsSelectionAndUndoEntry) {
    TextEditor editor("abc");
    editor.setSelection(1, 2);
    editor.setText("xyz");
    EXPECT_EQ(editor.getText(), "xyz");
    EXPECT_FALSE(editor.hasSelection());
    EXPECT_EQ(editor.getCaret(), 3u);

    // setText() records its own undo entry.
    editor.undo();
    EXPECT_EQ(editor.getText(), "abc");
}

TEST(TextEditor, SelectAllAndDeleteSelection) {
    TextEditor editor("abc");
    editor.selectAll();
    ASSERT_TRUE(editor.hasSelection());
    editor.deleteSelection();
    EXPECT_TRUE(editor.empty());
}

TEST(TextEditor, ChangeCallbackFiresOnBufferMutations) {
    TextEditor editor("abc");
    int changes = 0;
    editor.setChangeCallback([&] { ++changes; });

    editor.insertText("d");
    editor.backspace();
    editor.setText("xy");
    EXPECT_EQ(changes, 3);

    // View-state operations do not notify.
    editor.moveCaretLeft();
    editor.setCaret(0);
    editor.setSelection(0, 1);
    editor.clearSelection();
    EXPECT_EQ(changes, 3);
}

TEST(TextEditor, CompositionIsStoredAndClamped) {
    TextEditor editor("abcd");
    editor.setComposition("XYZ", 3, 0);
    EXPECT_EQ(editor.getComposition(), "XYZ");

    // Out-of-range segment is clamped to the buffer.
    editor.setComposition("XY", 10, 5);
    EXPECT_EQ(editor.getComposition(), "XY");

    editor.setComposition("", 0, 0);
    EXPECT_TRUE(editor.getComposition().empty());
}
