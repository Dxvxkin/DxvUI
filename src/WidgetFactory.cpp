#include "DxvUI/WidgetFactory.h"
#include "DxvUI/widgets/Button.h"
#include "DxvUI/containers/CenterContainer.h"

namespace DxvUI {

    std::shared_ptr<SceneNode> WidgetFactory::createWidget(const nlohmann::json& widgetJson) {
        std::shared_ptr<SceneNode> widget = nullptr;
        std::string type = widgetJson.value("type", "");

        if (type == "Button") {
            widget = std::make_shared<Button>(widgetJson.value("id", ""));
        } else if (type == "CenterContainer") {
            widget = std::make_shared<CenterContainer>();
        }
        else {
            widget = std::make_shared<SceneNode>();
        }

        widget->relX = widgetJson.value("x", 0);
        widget->relY = widgetJson.value("y", 0);
        widget->width = widgetJson.value("width", 0);
        widget->height = widgetJson.value("height", 0);
        widget->setZIndex(widgetJson.value("zIndex", 0));

        if (widgetJson.contains("children")) {
            for (const auto& childJson : widgetJson["children"]) {
                widget->addChild(createWidget(childJson));
            }
        }

        return widget;
    }

    std::shared_ptr<SceneNode> loadTreeFromJson(const std::string& jsonString) {
        auto json = nlohmann::json::parse(jsonString);
        return WidgetFactory::createWidget(json);
    }

}
