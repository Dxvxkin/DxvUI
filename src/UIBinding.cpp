#include <DxvUI/UIBinding.h>
#include <vector>
#include <cassert>
#include <mutex>

namespace DxvUI
{
    // --- Factory Function ---
    std::shared_ptr<UIBinding> UIBinding::create(value_t val) {
        // Use the helper struct to access the private constructor, ensuring safe creation
        struct MakeSharedEnabler : public UIBinding {
            explicit MakeSharedEnabler(value_t v) : UIBinding(std::move(v)) {}
        };
        return std::make_shared<MakeSharedEnabler>(std::move(val));
    }

    // --- Connection Constructor and Destructor ---
    UIBinding::Connection::Connection(callbackID id, std::weak_ptr<UIBinding> binding)
        : id(id), binding(std::move(binding)) {}

    UIBinding::Connection::~Connection() {
        if (auto bind = binding.lock()) {
            bind->unsubscribe(id);
        }
    }

    // --- Public API ---
    std::unique_ptr<UIBinding::Connection> UIBinding::subscribe(const callback_t& callback) {
        std::unique_lock<std::shared_mutex> lock(mutex_); // Use unique_lock for exclusive access
        const auto new_id = id_counter_++;
        callbacks_[new_id] = callback;
        // Use `new` here because make_unique cannot access the private constructor of Connection
        return std::unique_ptr<Connection>(new Connection(new_id, shared_from_this()));
    }

    void UIBinding::set(value_t newValue) {
        std::vector<callback_t> callbacks_to_call;
        value_t current_value_copy;

        {
            std::unique_lock<std::shared_mutex> lock(mutex_); // Use unique_lock for exclusive access
            if (value_ == newValue) {
                return;
            }
            value_ = std::move(newValue);
            current_value_copy = value_;

            // Copy callbacks to invoke them outside the lock
            callbacks_to_call.reserve(callbacks_.size());
            for (const auto& pair : callbacks_) {
                callbacks_to_call.push_back(pair.second);
            }
        }

        // Invoke callbacks outside the lock to prevent deadlocks and long lock-holding
        for (const auto& callback : callbacks_to_call) {
            callback(current_value_copy);
        }
    }

    UIBinding::UIBinding(value_t initialValue)
    {
        value_ = std::move(initialValue);
    }

    // --- Private Methods ---
    void UIBinding::unsubscribe(callbackID id) {
        std::unique_lock<std::shared_mutex> lock(mutex_); // Use unique_lock for exclusive access
        if (callbacks_.erase(id) == 0) {
            assert(false && "Callback ID not found in binding on disconnect.");
        }
    }
}