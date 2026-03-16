#include "DxvUI/ActionRegistry.h"
#include <utility>

namespace DxvUI {

    void ActionRegistry::registerAction(const std::string& name, ActionCallback action) {
        actions[name] = std::move(action);
    }

    ActionCallback ActionRegistry::getAction(const std::string& name) const {
        auto it = actions.find(name);
        if (it != actions.end()) {
            return it->second;
        }
        return nullptr;
    }

    bool ActionRegistry::isRegistered(const std::string& name) const {
        return actions.count(name) > 0;
    }

    void ActionRegistry::unregisterAction(const std::string& name) {
        actions.erase(name);
    }

}
