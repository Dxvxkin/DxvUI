#include "DxvUI/WidgetFactory.h"
#include "DxvUI/widgets/Button.h"
#include "DxvUI/containers/CenterContainer.h"
#include "DxvUI/Log.h"

namespace DxvUI {

    std::shared_ptr<SceneNode> WidgetFactory::createWidget(const nlohmann::json& widgetJson) {
        std::shared_ptr<SceneNode> widget = nullptr;
        std::string type = widgetJson.value("type", "");
        std::string id = widgetJson.value("id", ""); // Extract id for all nodes

        if (type == "Button") {
            std::string text = widgetJson.value("text", "");
            widget = Button::create(id, text);
        } else if (type == "CenterContainer") {
            widget = std::make_shared<CenterContainer>(id);
        }
        else {
            if (!type.empty() && type != "SceneNode") {
                Log::warn("WidgetFactory: Unknown widget type '{}'. Creating a default SceneNode.", type);
            }
            widget = std::make_shared<SceneNode>(id);
        }

        // This check is important to avoid crashing if widget creation fails
        if (!widget) {
            Log::error("WidgetFactory: Failed to create widget of type '{}' with id '{}'.", type, id);
            return nullptr;
        }

        if (widgetJson.contains("children")) {
            for (const auto& childJson : widgetJson["children"]) {
                auto childWidget = createWidget(childJson);
                if (childWidget) {
                    widget->addChild(childWidget);
                }
            }
        }

        return widget;
    }

    std::shared_ptr<SceneNode> loadTreeFromJson(const std::string& jsonString) {
        try {
            auto json = nlohmann::json::parse(jsonString);
            return WidgetFactory::createWidget(json);
        } catch (const nlohmann::json::parse_error& e) {
            Log::error("Failed to parse JSON tree: {}", e.what());
            return nullptr;
        }
    }

}
