#include <DxvUI/DxvUI.h>
#include <DxvUI/Colors.h>
#include <DxvUI/renderers/SDLRenderer.h>
#include <DxvUI/sources/SDLEventSource.h>
#include <DxvUI/widgets/Label.h>
#include <DxvUI/widgets/Button.h>

#include <SDL.h>
#include <iostream>
#include <memory>
#include <cmath>

extern "C" int SDL_main(int /*argc*/, char* /*argv*/[]) {
    // --- Setup ---
    constexpr int SCREEN_WIDTH = 800;
    constexpr int SCREEN_HEIGHT = 600;

    DxvUI::SDLRenderer dxv_renderer_impl("DxvUI Cursor Example", SCREEN_WIDTH, SCREEN_HEIGHT);
    DxvUI::IRenderer& dxv_renderer = dxv_renderer_impl;
    DxvUI::SDLEventSource eventSource;
    auto scene = DxvUI::Scene::create();

    // --- Critical Step: Link Scene and Renderer ---
    scene->setRenderer(&dxv_renderer);

    auto root = scene->getRoot();
    root->setSize(SCREEN_WIDTH, SCREEN_HEIGHT);

    // --- THEME ---
    root->getAppearance().fontPath = "C:/Windows/Fonts/segoeui.ttf";
    root->getAppearance().fontSize = 20;
    root->getAppearance().textColor = DxvUI::Colors::DarkGray;
    root->getAppearance().cursor = DxvUI::CursorType::Arrow;

    // --- Create UI ---
    auto myButton = DxvUI::Button::create("my_button", "Styled Button");
    myButton->setPosition(50, 50);
    myButton->setSize(150, 30);
    myButton->getAppearance().backgroundColor = DxvUI::Colors::CornflowerBlue;
    myButton->getAppearance().fontSize = 17;
    myButton->getAppearance().textColor = DxvUI::Colors::White;
    myButton->getAppearance().borderRadius = 8;
    myButton->getAppearance().borderThickness = 2;
    myButton->getAppearance().borderColor = DxvUI::Colors::Blue;
    myButton->getAppearance().cursor = DxvUI::CursorType::Hand;
    root->addChild(myButton);

    auto inheritedLabel = DxvUI::Label::create("inherited_label", "I inherit my style.");
    inheritedLabel->setPosition(50, 150);
    inheritedLabel->getAppearance().cursor = DxvUI::CursorType::Hand;
    root->addChild(inheritedLabel);

    auto overriddenLabel = DxvUI::Label::create("overridden_label", "I have my own style.");
    overriddenLabel->setPosition(50, 200);
    overriddenLabel->getAppearance().fontSize = 28;
    overriddenLabel->getAppearance().textColor = DxvUI::Colors::Magenta;
    root->addChild(overriddenLabel);


    // --- Main Loop ---
    bool quit = false;
    SDL_Event sdl_event;

    Uint32 last_time = SDL_GetTicks();
    float delta_time = 0.0f;

    while (!quit) {
        Uint32 current_time = SDL_GetTicks();
        delta_time = (current_time - last_time) / 1000.0f;
        last_time = current_time;

        while (SDL_PollEvent(&sdl_event) != 0) {
            DxvUI::DxvEvent dxv_event;
            if (eventSource.processEvent(sdl_event, dxv_event)) {
                if (dxv_event.type == DxvUI::EventType::Quit) quit = true;
                else scene->processEvent(dxv_event);
            }
        }

        scene->update(delta_time);
        scene->updateLayout();

        dxv_renderer.clear(DxvUI::Colors::White);
        scene->draw();
        dxv_renderer.present();
    }

    return 0;
}
