#ifndef DXVUI_ITEXTVALIDATOR_H
#define DXVUI_ITEXTVALIDATOR_H

#include <functional>
#include <memory>
#include <string>

namespace DxvUI::validators {

/**
 * @brief Backend-neutral gatekeeper for text being entered into a TextEditor.
 *
 * A validator describes the set of acceptable buffer contents. TextEditor
 * consults the attached validator before every mutation (insertText/setText)
 * and silently drops the edit when the resulting buffer would be rejected, so
 * a field with a validator simply refuses invalid input as it is typed (and
 * pasted).
 *
 * Validators receive the *full* proposed buffer, not the typed fragment: that
 * lets e.g. a hex validator accept "0x" as an in-progress value and reject
 * only what can never become valid. All built-in validators accept the empty
 * string, so the field can always be cleared.
 */
class ITextValidator {
   public:
    virtual ~ITextValidator() = default;

    /**
     * @brief Whether `text` is acceptable as the field's entire content.
     */
    virtual bool validate(const std::string& text) const = 0;
};

// --- Factory functions (preferred public API) ---

// Accepts decimal digits only ("123", "0").
std::shared_ptr<ITextValidator> digitsOnly();
// Accepts hexadecimal numbers with an optional 0x/0X prefix ("FF", "0x1A",
// "beef"); a bare "0x" is accepted as in-progress input.
std::shared_ptr<ITextValidator> hex();
// Accepts a signed integer within [min, max] (inclusive).
std::shared_ptr<ITextValidator> range(int min, int max);
// Accepts a decimal number ("1.5", ".6", "-3.14"); a leading/trailing dot and
// a bare sign are accepted as in-progress input.
std::shared_ptr<ITextValidator> decimal(bool allowNegative = true);
// Accepts text fully matching `pattern` (std::regex_match).
std::shared_ptr<ITextValidator> regex(std::string pattern);
// Accepts text passing the given predicate.
std::shared_ptr<ITextValidator> lambda(std::function<bool(const std::string&)> fn);

}  // namespace DxvUI::validators

#endif  // DXVUI_ITEXTVALIDATOR_H