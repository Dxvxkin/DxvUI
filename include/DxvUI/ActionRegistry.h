#ifndef DXVUI_ACTIONREGISTRY_H
#define DXVUI_ACTIONREGISTRY_H

#include <functional>
#include <map>
#include <string>

namespace DxvUI {

    // Forward declaration
    class SceneNode;
    struct DxvEvent;

    using ActionCallback = std::function<void(SceneNode*, const DxvEvent&)>;

    class ActionRegistry {
    public:
        static ActionRegistry& instance() {
            static ActionRegistry instance;
            return instance;
        }

        void registerAction(const std::string& name, ActionCallback action) {
            actions[name] = std::move(action);
        }

        ActionCallback getAction(const std::string& name) {
            if (actions.count(name)) {
                return actions[name];
            }
            return nullptr;
        }

        bool isRegistered(const std::string& name) const {
            return actions.count(name) > 0;
        }

        void unregisterAction(const std::string& name) {
            actions.erase(name);
        }

    private:
        ActionRegistry() = default;
        std::map<std::string, ActionCallback> actions;
    };

}

#endif //DXVUI_ACTIONREGISTRY_H
