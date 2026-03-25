#include <DxvUI/DxvUI.h>
#include <DxvUI/Log.h>
#include <DxvUI/style/Colors.h>
#include <DxvUI/renderers/SDLRenderer.h>
#include <DxvUI/sources/SDLEventSource.h>
#include <DxvUI/widgets/Label.h>
#include <DxvUI/widgets/Button.h>

#include <SDL.h>
#include <memory>
#include <iostream>
#include <format>

constexpr int SCREEN_WIDTH = 800;
constexpr int SCREEN_HEIGHT = 600;

void buildUI(std::shared_ptr<DxvUI::Scene> scene)
{
   auto root = scene->getRoot();

    DxvUI::StyleRule rootStyle;
    rootStyle.fontPath = "C:/Windows/Fonts/segoeui.ttf";
    rootStyle.fontSize = 18;
    rootStyle.textColor = DxvUI::Colors::DarkGray;
    rootStyle.width = SCREEN_WIDTH;
    rootStyle.height = SCREEN_HEIGHT;
    root->editStyle().set(DxvUI::WidgetState::Normal, rootStyle);

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
    myButton->editStyle().set(DxvUI::WidgetState::Normal, buttonNormal);

    DxvUI::StyleRule buttonHover;
    buttonHover.backgroundColor = DxvUI::Colors::RoyalBlue;
    buttonHover.borderThickness = 2;
    buttonHover.borderColor = DxvUI::Colors::White;
    myButton->editStyle().set(DxvUI::WidgetState::Hovered, buttonHover);

    DxvUI::StyleRule buttonPressed;
    buttonPressed.backgroundColor = DxvUI::Colors::MidnightBlue;
    myButton->editStyle().set(DxvUI::WidgetState::Pressed, buttonPressed);

    myButton->on(DxvUI::EventType::Click, [](DxvUI::DxvEvent& event)
    {
        auto root = event.target.lock()->getScene()->getRoot();
        int randomX = rand() % (SCREEN_WIDTH - 200);
        int randomY = rand() % (SCREEN_HEIGHT - 50);
        DxvUI::StyleRule newPosition;
        newPosition.left = randomX;
        newPosition.top = randomY;
        newPosition.textColor = DxvUI::Colors::Orange;
        newPosition.backgroundColor = DxvUI::Colors::ForestGreen;

        DxvUI::StyleRule hover;
        hover.backgroundColor = DxvUI::Colors::Red;

        auto label = DxvUI::Label::create(std::format("label_{}", DxvUI::SceneNode::getNodeCount()), std::format("Click to remove {}", DxvUI::SceneNode::getNodeCount()));
        label->editStyle().set(DxvUI::WidgetState::Normal, newPosition);
        label->editStyle().set(DxvUI::WidgetState::Hovered, hover);
        label->on(DxvUI::EventType::Click, [](DxvUI::DxvEvent& event)
        {
            if(auto target = event.target.lock())
                if (auto parent = target->parent.lock())
                    parent->removeChild((target));

        });
        root->addChild(label);

        if(auto btn = event.target.lock()->as<DxvUI::Button>())
            btn->setText(std::format("Count {}", DxvUI::SceneNode::getNodeCount()));
    });

    root->addChild(myButton);

    auto btn = DxvUI::Button::create("find_btn", "Find");
    btn->editStyle().set(DxvUI::WidgetState::Normal, {
        .backgroundColor = DxvUI::Colors::DarkOrange,
        .textColor = DxvUI::Colors::MidnightBlue,
        .borderRadius = 10,

                             .left = 300,
                             .top = 50,
                             .width = 200,
                             .height = 50,


                         });
    btn->on(DxvUI::EventType::Click, [](DxvUI::DxvEvent& event)
    {
        if (auto node = event.target.lock())
        {
            if (auto label = node->findNodeById("label_8"))
            {
                label->as<DxvUI::Label>()->setText("Found!");
            }
        }
    });

    root->addChild(btn);
}
extern "C" int SDL_main(int /*argc*/, char* /*argv*/[]) {
    DxvUI::Log::init();
    DxvUI::Log::info("Logger Initialized.");



    DxvUI::SDLRenderer dxv_renderer_impl("DxvUI Stateful Styles Example", SCREEN_WIDTH, SCREEN_HEIGHT);
    DxvUI::IRenderer& dxv_renderer = dxv_renderer_impl;
    DxvUI::SDLEventSource eventSource;
    auto scene = DxvUI::Scene::create();
    scene->setRenderer(&dxv_renderer);

    buildUI(scene);

    DxvUI::Log::info("Initial node count: {}", DxvUI::SceneNode::getNodeCount());

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
    scene->shutdown();
    scene.reset();
    DxvUI::Log::info("Final node count: {}", DxvUI::SceneNode::getNodeCount());

    return 0;
}
