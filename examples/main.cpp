#include <iostream>
#include <stdexcept>
#include <DxvUI/DxvUI.h>
#include <DxvUI/Colors.h>
#include <DxvUI/renderers/SDLRenderer.h>
#include <DxvUI/sources/SDLEventSource.h>

#include <SDL.h>

extern "C" int SDL_main(int argc, char* argv[]) {
    // --- Setup ---
    const int SCREEN_WIDTH = 800;
    const int SCREEN_HEIGHT = 600;
    SDL_Init(SDL_INIT_VIDEO);
    SDL_Window* window = SDL_CreateWindow("DxvUI Example", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, SCREEN_WIDTH, SCREEN_HEIGHT, SDL_WINDOW_SHOWN);
    SDL_Renderer* sdl_renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    SDL_SetRenderDrawBlendMode(sdl_renderer, SDL_BLENDMODE_BLEND);

    // --- DxvUI Integration ---
    DxvUI::SDLRenderer dxv_renderer_impl(sdl_renderer);
    DxvUI::IRenderer& dxv_renderer = dxv_renderer_impl; // Use the interface
    DxvUI::SDLEventSource eventSource;
    auto scene = DxvUI::Scene::create();

    auto root = scene->getRoot();
    root->setId("root_node");
    root->setSize(SCREEN_WIDTH, SCREEN_HEIGHT);

    scene->getActionRegistry().registerAction("my_button", [](DxvUI::SceneNode* s, const DxvUI::DxvEvent& e) {
        if (auto btn = s->as<DxvUI::Button>()) {
            if (e.button == DxvUI::MouseButton::Left)
                btn->backgroundColor = btn->backgroundColor.lighten(0.4f);
            else if (e.button == DxvUI::MouseButton::Right)
                btn->backgroundColor = btn->backgroundColor.darken(0.4f);
            std::cout << "click" << std::endl;
        }
    });

    auto myButton = std::make_shared<DxvUI::Button>("my_button");
    myButton->setPosition(200, 200);
    myButton->setSize(150, 50);
    myButton->backgroundColor = DxvUI::Colors::Cyan;
    myButton->borderRadius = 10;

    auto container = std::make_shared<DxvUI::CenterContainer>("container");
    container->setSize(SCREEN_WIDTH, SCREEN_HEIGHT);
    container->addChild(myButton);

    root->addChild(container);

    // --- Main Loop ---
    bool quit = false;
    SDL_Event sdl_event;

    while (!quit) {
        while (SDL_PollEvent(&sdl_event) != 0) {
            bool eventHandled = false;
            DxvUI::DxvEvent dxv_event;
            if (eventSource.processEvent(sdl_event, dxv_event)) {
                if (dxv_event.type == DxvUI::EventType::Quit) quit = true;
                else if (scene->handleEvent(dxv_event)) eventHandled = true;
            }
            if (!eventHandled && sdl_event.type == SDL_KEYDOWN && sdl_event.key.keysym.sym == SDLK_ESCAPE) quit = true;
        }

        scene->updateLayout();

        // Now using the interface for all rendering operations
        dxv_renderer.clear(DxvUI::Color::fromHex("#2d3436"));
        scene->draw(dxv_renderer);
        dxv_renderer.present();
    }

    // --- Cleanup ---
    SDL_DestroyRenderer(sdl_renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}
