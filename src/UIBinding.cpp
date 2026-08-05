#include <DxvUI/UIBinding.h>

#include <cassert>
#include <mutex>
#include <vector>

namespace DxvUI {

std::shared_ptr<UIBinding> UIBinding::create(value_t val) {
    struct MakeSharedEnabler : public UIBinding {
        explicit MakeSharedEnabler(value_t v) : UIBinding(std::move(v)) {}
    };
    return std::make_shared<MakeSharedEnabler>(std::move(val));
}

UIBinding::Connection::Connection(callbackID id, std::weak_ptr<UIBinding> binding)
    : id(id), binding(std::move(binding)) {}

UIBinding::Connection::~Connection() {
    if (auto bind = binding.lock()) {
        bind->unsubscribe(id);
    }
}

std::unique_ptr<UIBinding::Connection> UIBinding::subscribe(const callback_t& callback,
                                                            bool notifyImmediately) {
    callbackID new_id;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        new_id = id_counter_++;
        callbacks_[new_id] = callback;
    }

    if (notifyImmediately && callback) {
        callback(*this);
    }

    return std::unique_ptr<Connection>(new Connection(new_id, shared_from_this()));
}

void UIBinding::set(value_t newValue) {
    std::vector<callback_t> callbacks_to_call;

    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (value_ == newValue) {
            return;
        }
        value_ = std::move(newValue);

        callbacks_to_call.reserve(callbacks_.size());
        for (const auto& pair : callbacks_) {
            callbacks_to_call.push_back(pair.second);
        }
    }

    for (const auto& callback : callbacks_to_call) {
        if (callback) {
            callback(*this);
        }
    }
}

UIBinding::UIBinding(value_t initialValue) { value_ = std::move(initialValue); }

void UIBinding::unsubscribe(callbackID id) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (callbacks_.erase(id) == 0) {
        assert(false && "Callback ID not found in binding on disconnect.");
    }
}
}  // namespace DxvUI
