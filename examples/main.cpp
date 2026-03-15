#include <iostream>
#include <DxvUI/DxvUI.h>
#include <DxvUI/Colors.h>
#include <DxvUI/renderers/SDLRenderer.h>
#include <DxvUI/sources/SDLEventSource.h>

std::string mouseButtonToString(DxvUI::MouseButton button) {
    switch (button) {
        case DxvUI::MouseButton::Left: return "Left";
        case DxvUI::MouseButton::Middle: return "Middle";
        case DxvUI::MouseButton::Right: return "Right";
        default: return "Unknown";
    }
}

extern "C" int SDL_main(int argc, char* argv[]) {
    // 1. Setup
    const int SCREEN_WIDTH = 800;
    const int SCREEN_HEIGHT = 600;
    DxvUI::SDLRenderer renderer("DxvUI SDL2 Example", SCREEN_WIDTH, SCREEN_HEIGHT);
    DxvUI::SDLEventSource eventSource;

    // 2. Register Actions
    DxvUI::ActionRegistry::instance().registerAction("quit_button", [](DxvUI::SceneNode* s, const DxvUI::DxvEvent& e) {
        std::cout << "Quit button clicked!" << std::endl;
        // In a real app, you'd push a Quit event or set a flag.
    });
    DxvUI::ActionRegistry::instance().registerAction("other_button", [](DxvUI::SceneNode* s, const DxvUI::DxvEvent& e) {
        std::cout << "The other button was clicked with " << mouseButtonToString(e.button) << " button." << std::endl;
        // Change color on click
        if (auto button = dynamic_cast<DxvUI::Button*>(s)) {
            button->backgroundColor = button->backgroundColor.lighten(0.2f);
            button->borderColor = DxvUI::Colors::White;
            button->borderRadius = (button->borderRadius + 5) % (button->width / 2); // Cycle through roundness
        }
    });

    // 3. Build UI Scene
    auto root = std::make_shared<DxvUI::SceneNode>();
    root->width = SCREEN_WIDTH;
    root->height = SCREEN_HEIGHT;

    auto quitButton = std::make_shared<DxvUI::Button>("quit_button");
    quitButton->relX = 680;
    quitButton->relY = 550;
    quitButton->width = 100;
    quitButton->height = 30;
    quitButton->backgroundColor = DxvUI::Color::fromHex("#d63031");
    quitButton->borderColor = DxvUI::Colors::Black;
    root->addChild(quitButton);

    auto centeredContainer = std::make_shared<DxvUI::CenterContainer>();
    centeredContainer->width = 400;
    centeredContainer->height = 300;
    centeredContainer->relX = (SCREEN_WIDTH - centeredContainer->width) / 2;
    centeredContainer->relY = (SCREEN_HEIGHT - centeredContainer->height) / 2;
    root->addChild(centeredContainer);

    auto otherButton = std::make_shared<DxvUI::Button>("other_button");
    otherButton->width = 150;
    otherButton->height = 50;
    otherButton->backgroundColor = DxvUI::Color::fromHex("#0984e3");
    otherButton->borderColor = DxvUI::Colors::White;
    otherButton->borderWidth = 3;
    otherButton->borderRadius = 8; // Give it nice rounded corners
    centeredContainer->addChild(otherButton);

    // 4. Main Loop
    bool quit = false;
    DxvUI::DxvEvent event;

    while (!quit) {
        while (eventSource.pollEvent(event)) {
            if (event.type == DxvUI::EventType::Quit) {
                quit = true;
            } else {
                root->handleEvent(event);
            }
        }

        root->updateLayout();

        renderer.clear(DxvUI::Color::fromHex("#2d3436"));
        root->draw(renderer);
        renderer.present();
    }

    return 0;
}
