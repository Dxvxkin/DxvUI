#include <iostream>
#include <DxvUI/DxvUI.h>

// A mock renderer for demonstration purposes
class ConsoleRenderer : public DxvUI::IRenderer {
public:
    void drawRect(int x, int y, int width, int height) override {
        std::cout << "Drawing rect at (" << x << ", " << y << ") with size " << width << "x" << height << std::endl;
    }
    void drawText(const std::string& text, int x, int y) override {
        std::cout << "Drawing text '" << text << "' at (" << x << ", " << y << ")" << std::endl;
    }
    void drawImage(const std::string& imagePath, int x, int y) override {
        std::cout << "Drawing image '" << imagePath << "' at (" << x << ", " << y << ")" << std::endl;
    }
};

int main() {
    // 1. Register actions
    DxvUI::ActionRegistry::instance().registerAction("my_button_click", []() {
        std::cout << "Button clicked!" << std::endl;
    });

    // 2. Load UI from JSON
    std::string jsonString = R"({
        "type": "SceneNode", "x": 0, "y": 0, "width": 800, "height": 600,
        "children": [
            { "type": "Button", "action": "my_button_click", "x": 50, "y": 50, "width": 100, "height": 30 }
        ]
    })";
    std::shared_ptr<DxvUI::SceneNode> root = DxvUI::loadTreeFromJson(jsonString);

    // 3. Create a renderer and draw the UI
    ConsoleRenderer renderer;
    root->draw(renderer);

    // 4. Simulate a click event
    DxvUI::DxvEvent clickEvent{DxvUI::EventType::MouseDown, 75, 65};
    root->handleEvent(clickEvent);

    return 0;
}
