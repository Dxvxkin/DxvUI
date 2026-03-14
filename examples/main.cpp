#include <iostream>
#include <DxvUI/DxvUI.h>
#include <DxvUI/renderers/SDLRenderer.h> // Corrected include path
#include <SDL.h> // For SDL_Event

int SDL_main(int argc, char* argv[]) {
    // 1. Setup
    const int SCREEN_WIDTH = 800;
    const int SCREEN_HEIGHT = 600;
    DxvUI::SDLRenderer renderer("DxvUI SDL2 Example", SCREEN_WIDTH, SCREEN_HEIGHT);

    // 2. Register Actions
    DxvUI::ActionRegistry::instance().registerAction("quit_button", [](DxvUI::SceneNode* s, const DxvUI::DxvEvent& e) {
        std::cout << "Quit button clicked! This would normally close the app." << std::endl;
    });
    DxvUI::ActionRegistry::instance().registerAction("other_button", [](DxvUI::SceneNode* s, const DxvUI::DxvEvent& e) {
        std::cout << "The other button was clicked at local coords ("
                  << e.x - s->relX << ", " << e.y - s->relY << ")" << std::endl;
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
    centeredContainer->addChild(otherButton); // Add to container, not root

    // 4. Main Loop
    bool quit = false;
    SDL_Event e;

    while (!quit) {
        // Handle SDL events
        while (SDL_PollEvent(&e) != 0) {
            if (e.type == SDL_QUIT) {
                quit = true;
            }
            // Convert SDL_Event to DxvEvent and dispatch it
            else if (e.type == SDL_MOUSEBUTTONDOWN) {
                DxvUI::DxvEvent dxvEvent;
                dxvEvent.type = DxvUI::EventType::MouseDown;
                dxvEvent.x = e.button.x;
                dxvEvent.y = e.button.y;
                root->handleEvent(dxvEvent);
            }
        }

        // Update UI layout
        root->updateLayout();

        // Render UI
        renderer.clear();
        root->draw(renderer);
        renderer.present();
    }

    return 0;
}
