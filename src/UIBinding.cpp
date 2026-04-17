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
    // --- ИЗМЕНЕНИЕ 3: Реализация нового subscribe ---
    std::unique_ptr<UIBinding::Connection> UIBinding::subscribe(const callback_t& callback, bool notifyImmediately) {
        callbackID new_id;
        {
            std::unique_lock<std::shared_mutex> lock(mutex_); // Полная блокировка для изменения подписчиков
            new_id = id_counter_++;
            callbacks_[new_id] = callback;
        } // Блокировка снимается здесь

        if (notifyImmediately && callback) {
            // Вызываем колбэк немедленно, но уже без блокировки,
            // передавая ему текущий объект биндинга.
            // Используем shared_lock, так как только читаем value_ через геттеры.
            callback(*this);
        }

        // Используем `new`, так как make_unique не может получить доступ к приватному конструктору Connection
        return std::unique_ptr<Connection>(new Connection(new_id, shared_from_this()));
    }

    void UIBinding::set(value_t newValue) {
        std::vector<callback_t> callbacks_to_call;

        {
            std::unique_lock<std::shared_mutex> lock(mutex_);
            if (value_ == newValue) {
                return;
            }
            value_ = std::move(newValue);

            // Копируем колбэки для вызова вне блокировки
            callbacks_to_call.reserve(callbacks_.size());
            for (const auto& pair : callbacks_) {
                callbacks_to_call.push_back(pair.second);
            }
        }

        // --- ИЗМЕНЕНИЕ 4: Вызываем колбэки, передавая *this ---
        // Вызываем колбэки вне блокировки, чтобы предотвратить дедлоки
        for (const auto& callback : callbacks_to_call) {
            if (callback) {
                callback(*this); // Передаем сам объект, а не value_t
            }
        }
    }

    UIBinding::UIBinding(value_t initialValue)
    {
        value_ = std::move(initialValue);
    }

    // --- Private Methods ---
    void UIBinding::unsubscribe(callbackID id) {
        std::unique_lock<std::shared_mutex> lock(mutex_);
        if (callbacks_.erase(id) == 0) {
            // В релизной сборке это можно убрать, но для отладки очень полезно.
            assert(false && "Callback ID not found in binding on disconnect.");
        }
    }
}
