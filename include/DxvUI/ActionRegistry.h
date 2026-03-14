#ifndef DXVUI_ACTIONREGISTRY_H
#define DXVUI_ACTIONREGISTRY_H

#include <functional>
#include <map>
#include <string>

namespace DxvUI {

    class ActionRegistry {
    public:
        static ActionRegistry& instance() {
            static ActionRegistry instance;
            return instance;
        }

        void registerAction(const std::string& name, std::function<void()> action) {
            actions[name] = action;
        }

        std::function<void()> getAction(const std::string& name) {
            if (actions.count(name)) {
                return actions[name];
            }
            return nullptr;
        }

    private:
        ActionRegistry() = default;
        std::map<std::string, std::function<void()>> actions;
    };

}

#endif //DXVUI_ACTIONREGISTRY_H
