#include <charconv>
#include <functional>
#include <optional>
#include <regex>
#include <string>
#include <system_error>
#include <utility>

#include "DxvUI/Log.h"
#include "DxvUI/text/ITextValidator.h"

namespace DxvUI::validators {

namespace {

class DigitsOnlyValidator : public ITextValidator {
   public:
    bool validate(const std::string& text) const override {
        for (size_t i = 0; i < text.size(); ++i) {
            const unsigned char c = static_cast<unsigned char>(text[i]);
            if (c < '0' || c > '9') return false;
        }
        return true;
    }
};

class HexValidator : public ITextValidator {
   public:
    bool validate(const std::string& text) const override {
        if (text.empty()) return true;
        size_t start = 0;
        if (text.size() >= 2 && text[0] == '0' && (text[1] == 'x' || text[1] == 'X')) {
            start = 2;
            // A bare "0x" is an in-progress value.
            if (text.size() == 2) return true;
        }
        for (size_t i = start; i < text.size(); ++i) {
            const unsigned char c = static_cast<unsigned char>(text[i]);
            const bool digit = c >= '0' && c <= '9';
            const bool lower = c >= 'a' && c <= 'f';
            const bool upper = c >= 'A' && c <= 'F';
            if (!digit && !lower && !upper) return false;
        }
        return true;
    }
};

class RangeValidator : public ITextValidator {
   public:
    RangeValidator(int min, int max) : min_(min), max_(max) {}

    bool validate(const std::string& text) const override {
        if (text.empty()) return true;
        // A bare minus is an in-progress negative number; only useful when
        // negatives can actually land in range.
        if (text == "-") return min_ < 0;

        long long value = 0;
        const std::from_chars_result res =
            std::from_chars(text.data(), text.data() + text.size(), value);
        if (res.ec != std::errc() || res.ptr != text.data() + text.size()) return false;
        return value >= min_ && value <= max_;
    }

   private:
    int min_;
    int max_;
};

class DecimalValidator : public ITextValidator {
   public:
    explicit DecimalValidator(bool allowNegative) : allowNegative_(allowNegative) {}

    bool validate(const std::string& text) const override {
        if (text.empty()) return true;
        // Lone dot / sign-plus-dot: in-progress values.
        if (text == ".") return true;
        if (text == "-.") return allowNegative_;

        size_t i = 0;
        if (allowNegative_ && text[i] == '-') {
            ++i;
            if (i == text.size()) return true;  // bare sign: in-progress
        }

        size_t integerDigits = 0;
        while (i < text.size() && text[i] >= '0' && text[i] <= '9') {
            ++i;
            ++integerDigits;
        }

        size_t fractionalDigits = 0;
        if (i < text.size() && text[i] == '.') {
            ++i;
            while (i < text.size() && text[i] >= '0' && text[i] <= '9') {
                ++i;
                ++fractionalDigits;
            }
        }

        // Every byte consumed and at least one digit somewhere.
        return i == text.size() && (integerDigits > 0 || fractionalDigits > 0);
    }

   private:
    bool allowNegative_;
};

class RegexValidator : public ITextValidator {
   public:
    explicit RegexValidator(std::string pattern) : pattern_(std::move(pattern)) {
        try {
            regex_.emplace(pattern_);
        } catch (const std::regex_error& e) {
            valid_ = false;
            Log::warn("RegexValidator: invalid pattern '{}': {}", pattern_, e.what());
        }
    }

    bool validate(const std::string& text) const override {
        if (text.empty()) return true;
        if (!valid_) return false;
        return std::regex_match(text, *regex_);
    }

   private:
    std::string pattern_;
    // Engaged only when the pattern compiles; empty optional = invalid pattern.
    std::optional<std::regex> regex_;
    bool valid_ = true;
};

class LambdaValidator : public ITextValidator {
   public:
    explicit LambdaValidator(std::function<bool(const std::string&)> fn) : fn_(std::move(fn)) {}

    bool validate(const std::string& text) const override { return fn_ ? fn_(text) : true; }

   private:
    std::function<bool(const std::string&)> fn_;
};

}  // namespace

std::shared_ptr<ITextValidator> digitsOnly() { return std::make_shared<DigitsOnlyValidator>(); }

std::shared_ptr<ITextValidator> hex() { return std::make_shared<HexValidator>(); }

std::shared_ptr<ITextValidator> range(int min, int max) {
    return std::make_shared<RangeValidator>(min, max);
}

std::shared_ptr<ITextValidator> decimal(bool allowNegative) {
    return std::make_shared<DecimalValidator>(allowNegative);
}

std::shared_ptr<ITextValidator> regex(std::string pattern) {
    return std::make_shared<RegexValidator>(std::move(pattern));
}

std::shared_ptr<ITextValidator> lambda(std::function<bool(const std::string&)> fn) {
    return std::make_shared<LambdaValidator>(std::move(fn));
}

}  // namespace DxvUI::validators