#ifndef DXVUI_WIDGETFACTORY_H
#define DXVUI_WIDGETFACTORY_H

#include "SceneNode.h"
#include <memory>
#include <string>
#include <nlohmann/json.hpp>

namespace DxvUI {

    class WidgetFactory {
    public:
        static std::shared_ptr<SceneNode> createWidget(const nlohmann::json& widgetJson);
    };

    std::shared_ptr<SceneNode> loadTreeFromJson(const std::string& jsonString);

}

#endif //DXVUI_WIDGETFACTORY_H
