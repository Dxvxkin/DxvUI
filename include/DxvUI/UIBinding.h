//
// Created by ACER on 15.04.2026.
//

#ifndef DXVUI_UIBINDING_H
#define DXVUI_UIBINDING_H

#include <algorithm>
#include <cctype>
#include <charconv>
#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <system_error>
#include <variant>

// Helper for std::visit
namespace DxvUI {
template <class... Ts>
struct overloaded : Ts... {
    using Ts::operator()...;
};
template <class... Ts>
overloaded(Ts...) -> overloaded<Ts...>;

class UIBinding : public std::enable_shared_from_this<UIBinding> {
   public:
    class Connection;  // Forward declaration

    using value_t = std::variant<int, float, std::string, bool>;
    using callback_t = std::function<void(const UIBinding&)>;
    using callbackID = uint64_t;

    // --- Factory and Public API ---
    [[nodiscard]] static std::shared_ptr<UIBinding> create(value_t val = {});

    [[nodiscard]] std::unique_ptr<Connection> subscribe(const callback_t& callback,
                                                        bool notifyImmediately = true);
    void set(value_t newValue);

    // --- Inline, Type-safe Getters for Performance ---
    [[nodiscard]] std::optional<int> getInt() const noexcept {
        std::lock_guard<std::mutex> lock(mutex_);
        return std::visit(
            overloaded{[](int arg) -> std::optional<int> { return arg; },
                       [](float arg) -> std::optional<int> { return static_cast<int>(arg); },
                       [](const std::string& arg) -> std::optional<int> {
                           int result;
                           auto [ptr, ec] =
                               std::from_chars(arg.data(), arg.data() + arg.size(), result);
                           if (ec == std::errc()) return result;
                           return std::nullopt;
                       },
                       [](bool arg) -> std::optional<int> { return arg ? 1 : 0; }},
            value_);
    }

    [[nodiscard]] int getIntOr(int fallback = 0) const noexcept {
        return getInt().value_or(fallback);
    }

    [[nodiscard]] std::optional<float> getFloat() const noexcept {
        std::lock_guard<std::mutex> lock(mutex_);
        return std::visit(
            overloaded{[](int arg) -> std::optional<float> { return static_cast<float>(arg); },
                       [](float arg) -> std::optional<float> { return arg; },
                       [](const std::string& arg) -> std::optional<float> {
                           float result;
                           auto [ptr, ec] =
                               std::from_chars(arg.data(), arg.data() + arg.size(), result);
                           if (ec == std::errc()) return result;
                           return std::nullopt;
                       },
                       [](bool arg) -> std::optional<float> { return arg ? 1.0f : 0.0f; }},
            value_);
    }

    [[nodiscard]] float getFloatOr(float fallback = 0.0f) const noexcept {
        return getFloat().value_or(fallback);
    }

    [[nodiscard]] std::string getString() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return std::visit(
            overloaded{[](int arg) { return std::to_string(arg); },
                       [](float arg) { return std::to_string(arg); },
                       [](const std::string& arg) { return arg; },
                       [](bool arg) { return arg ? std::string("true") : std::string("false"); }},
            value_);
    }

    [[nodiscard]] std::optional<bool> getBool() const noexcept {
        std::lock_guard<std::mutex> lock(mutex_);
        return std::visit(overloaded{[](int arg) -> std::optional<bool> { return arg != 0; },
                                     [](float arg) -> std::optional<bool> { return arg != 0.0f; },
                                     [](const std::string& arg) -> std::optional<bool> {
                                         std::string lower_arg;
                                         lower_arg.resize(arg.size());
                                         std::ranges::transform(
                                             arg, lower_arg.begin(),
                                             [](unsigned char c) { return std::tolower(c); });
                                         if (lower_arg == "true" || lower_arg == "1") return true;
                                         if (lower_arg == "false" || lower_arg == "0") return false;
                                         return std::nullopt;
                                     },
                                     [](bool arg) -> std::optional<bool> { return arg; }},
                          value_);
    }

    [[nodiscard]] bool getBoolOr(bool fallback = false) const noexcept {
        return getBool().value_or(fallback);
    }

   private:
    struct MakeSharedEnabler;

    explicit UIBinding(value_t initialValue);
    void unsubscribe(callbackID id);

   public:  // Connection must be public to be used in unique_ptr, but its constructor is private to
            // UIBinding
    class Connection {
       public:
        ~Connection();
        Connection(Connection&&) = delete;
        Connection& operator=(Connection&&) = delete;
        Connection(const Connection&) = delete;
        Connection& operator=(const Connection&) = delete;

       private:
        friend class UIBinding;  // Allow UIBinding to access the private constructor
        Connection(callbackID id, std::weak_ptr<UIBinding> binding);
        callbackID id;
        std::weak_ptr<UIBinding> binding;
    };

   private:
    value_t value_;
    mutable std::mutex mutex_;
    callbackID id_counter_ = 0;
    std::map<callbackID, callback_t> callbacks_;
};
}  // namespace DxvUI

#endif  // DXVUI_UIBINDING_H
