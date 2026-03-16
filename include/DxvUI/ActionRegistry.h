#ifndef DXVUI_ACTIONREGISTRY_H
#define DXVUI_ACTIONREGISTRY_H

#include <functional>
#include <map>
#include <string>

namespace DxvUI {

    class SceneNode;
    struct DxvEvent;

    using ActionCallback = std::function<void(SceneNode*, const DxvEvent&)>;

    class ActionRegistry {
    public:
        void registerAction(const std::string& name, ActionCallback action);
        ActionCallback getAction(const std::string& name) const;
        bool isRegistered(const std::string& name) const;
        void unregisterAction(const std::string& name);

    private:
        std::map<std::string, ActionCallback> actions;
    };

}

#endif //DXVUI_ACTIONREGISTRY_H
