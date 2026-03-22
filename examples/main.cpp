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
#include <format>

extern "C" int SDL_main(int /*argc*/, char* /*argv*/[]) {
    // --- Setup ---
    constexpr int SCREEN_WIDTH = 800;
    constexpr int SCREEN_HEIGHT = 600;

    DxvUI::SDLRenderer dxv_renderer_impl("DxvUI Stateful Styles Example", SCREEN_WIDTH, SCREEN_HEIGHT);
    DxvUI::IRenderer& dxv_renderer = dxv_renderer_impl;
    DxvUI::SDLEventSource eventSource;
    auto scene = DxvUI::Scene::create();
    scene->setRenderer(&dxv_renderer);

    auto root = scene->getRoot();

    // --- THEME THE ENTIRE APP ---
    DxvUI::StyleRule rootStyle;
    rootStyle.fontPath = "C:/Windows/Fonts/segoeui.ttf";
    rootStyle.fontSize = 18;
    rootStyle.textColor = DxvUI::Colors::DarkGray;
    rootStyle.width = SCREEN_WIDTH;
    rootStyle.height = SCREEN_HEIGHT;
    root->getStyle().set(DxvUI::WidgetState::Normal, rootStyle);

    // --- Create UI ---
    auto myButton = DxvUI::Button::create("my_button", "Click Me!");

    DxvUI::StyleRule buttonNormal;
    buttonNormal.left = 50;
    buttonNormal.top = 50;
    buttonNormal.width = 200;
    buttonNormal.height = 50;
    buttonNormal.backgroundColor = DxvUI::Colors::CornflowerBlue;
    buttonNormal.textColor = DxvUI::Colors::White;
    buttonNormal.borderRadius = 8;
    buttonNormal.cursor = DxvUI::CursorType::Hand;
    myButton->getStyle().set(DxvUI::WidgetState::Normal, buttonNormal);

    DxvUI::StyleRule buttonHover;
    buttonHover.backgroundColor = DxvUI::Colors::RoyalBlue;
    buttonHover.borderThickness = 2;
    buttonHover.borderColor = DxvUI::Colors::White;
    myButton->getStyle().set(DxvUI::WidgetState::Hovered, buttonHover);

    DxvUI::StyleRule buttonPressed;
    buttonPressed.backgroundColor = DxvUI::Colors::MidnightBlue;
    myButton->getStyle().set(DxvUI::WidgetState::Pressed, buttonPressed);

    myButton->on(DxvUI::EventType::Click, [&root](DxvUI::DxvEvent& event)
    {
        int randomX = rand() % (SCREEN_WIDTH - 200); // Ensure button stays within bounds
        int randomY = rand() % (SCREEN_HEIGHT - 50); // Ensure button stays within bounds
        DxvUI::StyleRule newPosition;
        newPosition.left = randomX;
        newPosition.top = randomY;
        newPosition.textColor = DxvUI::Colors::Orange;
        newPosition.backgroundColor = DxvUI::Colors::ForestGreen;


        DxvUI::StyleRule hover;
        hover.backgroundColor = DxvUI::Colors::Red;

        auto label = DxvUI::Label::create(std::format("label_{}", DxvUI::SceneNode::getNodeCount()), std::format("Fuck {}", DxvUI::SceneNode::getNodeCount()));
        label->getStyle().set(DxvUI::WidgetState::Normal, newPosition);
        label->getStyle().set(DxvUI::WidgetState::Hovered, hover);
        label->on(DxvUI::EventType::Click, [&root](DxvUI::DxvEvent& event)
        {
            root->removeChild(event.target.lock());
        });
        root->addChild(label);

        event.target.lock()->as<DxvUI::Button>()->setText(std::format("Count {}", DxvUI::SceneNode::getNodeCount()));
    });

    root->addChild(myButton);

    std::cout << "Initial node count " << DxvUI::SceneNode::getNodeCount() << std::endl;

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
    std::cout << "Finall node count " << DxvUI::SceneNode::getNodeCount() << std::endl;

    return 0;
}
