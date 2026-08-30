#include <gtest/gtest.h>

#include <memory>
#include <string>

#include "DxvUI/text/ITextValidator.h"
#include "DxvUI/text/TextEditor.h"

using namespace DxvUI;

namespace {

using validators::decimal;
using validators::digitsOnly;
using validators::hex;
using validators::lambda;
using validators::range;
using validators::regex;

bool accepts(const std::shared_ptr<validators::ITextValidator>& validator, std::string text) {
    return validator->validate(text);
}

}  // namespace

// --- DigitsOnly ---

TEST(DigitsOnlyValidator, AcceptsDigitsAndEmpty) {
    auto v = digitsOnly();
    EXPECT_TRUE(accepts(v, ""));
    EXPECT_TRUE(accepts(v, "0"));
    EXPECT_TRUE(accepts(v, "123"));
    EXPECT_TRUE(accepts(v, "007"));
}

TEST(DigitsOnlyValidator, RejectsNonDigits) {
    auto v = digitsOnly();
    EXPECT_FALSE(accepts(v, "-5"));
    EXPECT_FALSE(accepts(v, "12a"));
    EXPECT_FALSE(accepts(v, "1 2"));
    EXPECT_FALSE(accepts(v, "1.5"));
}

// --- Hex ---

TEST(HexValidator, AcceptsHexAndEmpty) {
    auto v = hex();
    EXPECT_TRUE(accepts(v, ""));
    EXPECT_TRUE(accepts(v, "0"));
    EXPECT_TRUE(accepts(v, "FF"));
    EXPECT_TRUE(accepts(v, "beef"));
    EXPECT_TRUE(accepts(v, "0xFF"));
    EXPECT_TRUE(accepts(v, "0X1a"));
    // Bare prefix is accepted as in-progress input.
    EXPECT_TRUE(accepts(v, "0x"));
    EXPECT_TRUE(accepts(v, "0X"));
}

TEST(HexValidator, RejectsNonHex) {
    auto v = hex();
    EXPECT_FALSE(accepts(v, "0xZZ"));
    EXPECT_FALSE(accepts(v, "12G"));
    EXPECT_FALSE(accepts(v, "xyz"));
    EXPECT_FALSE(accepts(v, "0x 1F"));
}

// --- Range ---

TEST(RangeValidator, AcceptsInRangeValues) {
    auto v = range(0, 100);
    EXPECT_TRUE(accepts(v, ""));
    EXPECT_TRUE(accepts(v, "0"));
    EXPECT_TRUE(accepts(v, "50"));
    EXPECT_TRUE(accepts(v, "100"));
}

TEST(RangeValidator, RejectsOutOfRangeValues) {
    auto v = range(0, 100);
    EXPECT_FALSE(accepts(v, "-1"));
    EXPECT_FALSE(accepts(v, "101"));
    EXPECT_FALSE(accepts(v, "1000"));
}

TEST(RangeValidator, RejectsNonNumeric) {
    auto v = range(0, 100);
    EXPECT_FALSE(accepts(v, "abc"));
    EXPECT_FALSE(accepts(v, "1a"));
    EXPECT_FALSE(accepts(v, " 5"));
    EXPECT_FALSE(accepts(v, "5 "));
}

TEST(RangeValidator, NegativeRangeAllowsPartialSign) {
    auto v = range(-10, 10);
    // A bare minus is an in-progress negative number.
    EXPECT_TRUE(accepts(v, "-"));
    EXPECT_TRUE(accepts(v, "-5"));
    EXPECT_TRUE(accepts(v, "-10"));
    EXPECT_FALSE(accepts(v, "-11"));
}

TEST(RangeValidator, NonNegativeRangeRejectsSign) {
    auto v = range(0, 100);
    EXPECT_FALSE(accepts(v, "-"));
    EXPECT_FALSE(accepts(v, "-1"));
}

// --- Decimal ---

TEST(DecimalValidator, AcceptsDecimalsAndEmpty) {
    auto v = decimal();
    EXPECT_TRUE(accepts(v, ""));
    EXPECT_TRUE(accepts(v, "0"));
    EXPECT_TRUE(accepts(v, "1.5"));
    EXPECT_TRUE(accepts(v, "0.4"));
    EXPECT_TRUE(accepts(v, "-3.14"));
}

TEST(DecimalValidator, AcceptsPartialInput) {
    auto v = decimal();
    // Leading/trailing dot and bare sign are in-progress states.
    EXPECT_TRUE(accepts(v, "."));
    EXPECT_TRUE(accepts(v, ".6"));
    EXPECT_TRUE(accepts(v, "1."));
    EXPECT_TRUE(accepts(v, "-"));
    EXPECT_TRUE(accepts(v, "-."));
}

TEST(DecimalValidator, RejectsMalformedNumbers) {
    auto v = decimal();
    EXPECT_FALSE(accepts(v, "1.2.3"));
    EXPECT_FALSE(accepts(v, "abc"));
    EXPECT_FALSE(accepts(v, "--1"));
    EXPECT_FALSE(accepts(v, ".6.7"));
    EXPECT_FALSE(accepts(v, "1..2"));
}

TEST(DecimalValidator, NegativeCanBeDisallowed) {
    auto v = decimal(false);
    EXPECT_TRUE(accepts(v, ""));
    EXPECT_TRUE(accepts(v, "1.5"));
    EXPECT_TRUE(accepts(v, ".6"));
    EXPECT_FALSE(accepts(v, "-"));
    EXPECT_FALSE(accepts(v, "-3.14"));
    EXPECT_FALSE(accepts(v, "-."));
}

// --- Regex ---

TEST(RegexValidator, MatchesFullText) {
    auto v = regex(std::string(R"(^\d{3}-\d{2}$)"));
    EXPECT_TRUE(accepts(v, ""));
    EXPECT_TRUE(accepts(v, "123-45"));
    EXPECT_FALSE(accepts(v, "12-3"));
    EXPECT_FALSE(accepts(v, "123-456"));
    EXPECT_FALSE(accepts(v, "abc"));
}

// --- Lambda ---

TEST(LambdaValidator, UsesPredicate) {
    auto v = lambda([](const std::string& s) { return s.size() <= 3; });
    EXPECT_TRUE(accepts(v, ""));
    EXPECT_TRUE(accepts(v, "a"));
    EXPECT_TRUE(accepts(v, "abc"));
    EXPECT_FALSE(accepts(v, "abcd"));
}

// --- TextEditor integration ---

TEST(TextEditorValidation, DigitsOnlyRejectsNonDigits) {
    TextEditor editor;
    editor.setValidator(digitsOnly());
    editor.insertText("12");
    EXPECT_EQ(editor.getText(), "12");
    editor.insertText("a");
    EXPECT_EQ(editor.getText(), "12");
    editor.insertText("34");
    EXPECT_EQ(editor.getText(), "1234");
}

TEST(TextEditorValidation, PasteIsValidatedAsWhole) {
    TextEditor editor;
    editor.setValidator(digitsOnly());
    // "12a" as a whole paste must be rejected, not partially accepted.
    editor.insertText("12a");
    EXPECT_TRUE(editor.empty());
}

TEST(TextEditorValidation, SetTextIsValidated) {
    TextEditor editor;
    editor.setValidator(digitsOnly());
    editor.setText("abc");
    EXPECT_TRUE(editor.empty());
    editor.setText("42");
    EXPECT_EQ(editor.getText(), "42");
}

TEST(TextEditorValidation, RejectedEditDoesNotPolluteUndoStack) {
    TextEditor editor;
    editor.setValidator(digitsOnly());
    editor.insertText("1");
    editor.insertText("2");
    editor.insertText("x");  // rejected, should record no undo entry
    editor.undo();
    EXPECT_EQ(editor.getText(), "1");
}

TEST(TextEditorValidation, RangeBlocksOutOfBoundsEdit) {
    TextEditor editor("100");
    editor.setValidator(range(0, 100));
    editor.setCaret(0);
    editor.insertText("1");  // "1100" > 100
    EXPECT_EQ(editor.getText(), "100");
}

TEST(TextEditorValidation, SelectionReplacementIsValidated) {
    TextEditor editor("150");
    editor.setValidator(range(0, 100));
    editor.selectAll();
    editor.insertText("20");
    EXPECT_EQ(editor.getText(), "20");
}

TEST(TextEditorValidation, CaretMidBufferEditIsValidated) {
    TextEditor editor("12");
    editor.setValidator(digitsOnly());
    editor.setCaret(1);
    editor.insertText("a");  // rejected
    EXPECT_EQ(editor.getText(), "12");
    editor.insertText("3");
    EXPECT_EQ(editor.getText(), "132");
}

TEST(TextEditorValidation, HexAllowsInProgressPrefix) {
    TextEditor editor;
    editor.setValidator(hex());
    editor.insertText("0x");
    EXPECT_EQ(editor.getText(), "0x");
    editor.insertText("F");
    EXPECT_EQ(editor.getText(), "0xF");
    editor.insertText("F");
    EXPECT_EQ(editor.getText(), "0xFF");
    editor.insertText("G");
    EXPECT_EQ(editor.getText(), "0xFF");
}

TEST(TextEditorValidation, DecimalLeadingDotWorkflow) {
    TextEditor editor;
    editor.setValidator(decimal());
    editor.insertText(".");
    editor.insertText("6");
    EXPECT_EQ(editor.getText(), ".6");
    editor.insertText(".");
    EXPECT_EQ(editor.getText(), ".6");
}

TEST(TextEditorValidation, BackspaceStillAllowsClearing) {
    TextEditor editor("12a");
    editor.setValidator(digitsOnly());
    // The buffer is already in a state the validator rejects (set programmatically
    // before the validator was attached); deletion stays safe.
    editor.backspace();
    EXPECT_EQ(editor.getText(), "12");
    EXPECT_TRUE(accepts(editor.getValidator(), editor.getText()));
}

TEST(TextEditorValidation, NoValidatorAttachedNoValidation) {
    TextEditor editor;
    editor.insertText("abc 123");
    EXPECT_EQ(editor.getText(), "abc 123");
}

TEST(TextEditorValidation, GetValidatorReturnsAttached) {
    TextEditor editor;
    EXPECT_EQ(editor.getValidator(), nullptr);
    auto v = digitsOnly();
    editor.setValidator(v);
    EXPECT_EQ(editor.getValidator(), v);
}