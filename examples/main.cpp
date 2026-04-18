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

    // Set global font properties on the root node. These will be inherited by children.
    root->editStyle().set(DxvUI::WidgetState::Normal, {
        .textColor = DxvUI::Colors::DarkGray,
        .fontSize = 18,
        .fontPath = "C:/Windows/Fonts/segoeui.ttf",
        .width = SCREEN_WIDTH,
        .height = SCREEN_HEIGHT
    });

    // --- Button 1: Uses default styles + overrides ---
    auto myButton = DxvUI::Button::create("my_button", "Click Me!");

    root->on(DxvUI::EventType::Change, [](DxvUI::DxvEvent& event)
    {
        DxvUI::Log::info("Root callback");
        DxvUI::Log::info("{} ::onChange({}) ", event.target.lock()->getId(), event.target.lock()->getBinding()->getString().value_or(""));
    });

    myButton->on(DxvUI::EventType::Change, [](DxvUI::DxvEvent& event)
    {
        DxvUI::Log::info("Button callback");
        DxvUI::Log::info("{} ::onChange({}) ", event.target.lock()->getId(), event.target.lock()->getBinding()->getString().value_or(""));
    });

    myButton->on(DxvUI::EventType::Attach, [](DxvUI::DxvEvent& event)
    {
        if (event.target.lock() == event.currentTarget.lock())
        {
            DxvUI::Log::info("{} Attach", event.target.lock()->getId());
        }

    });


    // Override only position and size. Colors, padding, etc., will come from the Button's default theme.
    myButton->editStyle().set(DxvUI::WidgetState::Normal, {
        .left = 50,
        .top = 50,
        .width = 200,
        .height = 50
    });

    // We can still override state-specific styles if needed.
    // The base for this will be the Button's default Hovered style.
    myButton->editStyle().set(DxvUI::WidgetState::Hovered, {
        .borderColor = DxvUI::Colors::White,
        .borderThickness = 2
    });


    myButton->on(DxvUI::EventType::Click, [](DxvUI::DxvEvent& event)
    {
        auto root = event.target.lock()->getScene()->getRoot();
        int randomX = rand() % (SCREEN_WIDTH - 200);
        int randomY = rand() % (SCREEN_HEIGHT - 50);

        static size_t label_count = 0;
        auto label = DxvUI::Label::create(
            std::format("label_{}", label_count++),
            std::format("Click to remove {}", label_count)
        );

        // Set style for the new label
        label->editStyle().set(DxvUI::WidgetState::Normal, {
            .backgroundColor = DxvUI::Color(0, 0, 0, 80),
            .textColor = DxvUI::Colors::White,
            .borderColor = DxvUI::Colors::Black,
            .borderThickness = 1,
            .borderRadius = 5,
            .left = (float)randomX,
            .top = (float)randomY,
            .padding = DxvUI::Thickness{5, 5, 5, 5},
        });
        label->editStyle().set(DxvUI::WidgetState::Hovered, {.backgroundColor = (DxvUI::Color(0, 0, 0, 150)),.borderThickness = 1,  .borderRadius = 10 });

        label->on(DxvUI::EventType::Click, [](DxvUI::DxvEvent& event)
        {
            if(auto target = event.target.lock())
            {
                target->detach();
            }

        });

        root->addChild(label);

        if(auto btn = event.target.lock()->as<DxvUI::Button>())
            btn->setText(std::format("Count {}", DxvUI::SceneNode::getNodeCount()));
    });

    root->addChild(myButton);

    // --- Button 2: Uses default styles + different overrides ---
    auto btn2 = DxvUI::Button::create("find_btn", "Find");
    // Override position, size, and some colors.
    btn2->editStyle().set(DxvUI::WidgetState::Normal, {
        .backgroundColor = DxvUI::Colors::DarkOrange,
        .textColor = DxvUI::Colors::MidnightBlue,
        .left = 300,
        .top = 50,
        .width = 200,
        .height = 50
    });


    btn2->on(DxvUI::EventType::Click, [](DxvUI::DxvEvent& event)
    {
        if (auto node = event.target.lock())
        {
            if (auto label = node->getScene()->findNodeById("label_8"))
            {
                label->as<DxvUI::Label>()->setText("Found!");
            }
        }
    });

    root->addChild(btn2);

    // --- Button 3: Test default styles ---
    auto btn_test_def_styles = DxvUI::Button::create("btn_defStyle", "test");
    btn_test_def_styles->editStyle().set(DxvUI::WidgetState::Normal, {
    .left = 500, .top = 500, .width = 100, .height = 50});
    btn_test_def_styles->on(DxvUI::EventType::Click, [](DxvUI::DxvEvent e)
    {
        if (auto node = e.target.lock())
        {

            if (auto target = node->getScene()->findNodeById("label_7"))
            {
                auto style = target->as<DxvUI::Label>()->editStyle().get(DxvUI::WidgetState::Normal);
                auto newStyle = DxvUI::StyleRule(*style);
                newStyle.padding->left += 5;
                newStyle.padding->top += 5;
                newStyle.padding->right+= 5;
                newStyle.padding->bottom += 5;
                target->editStyle().set(DxvUI::WidgetState::Normal, newStyle);
            }


        }
    });
    root->addChild(btn_test_def_styles);
}

extern "C" int SDL_main(int /*argc*/, char* /*argv*/[]) {
    DxvUI::Log::init();
    DxvUI::Log::info("Logger Initialized.");

    DxvUI::SDLRenderer dxv_renderer_impl("DxvUI Example", SCREEN_WIDTH, SCREEN_HEIGHT);
    DxvUI::IRenderer& dxv_renderer = dxv_renderer_impl;
    DxvUI::SDLEventSource eventSource;
    auto scene = DxvUI::Scene::create();
    scene->setRenderer(&dxv_renderer);

    buildUI(scene);

    DxvUI::Log::info("Initial node count: {}", DxvUI::SceneNode::getNodeCount());

    bool quit = false;
    SDL_Event sdl_event;
    Uint32 last_time = SDL_GetTicks();

    while (!quit) {
        Uint32 current_time = SDL_GetTicks();
        float delta_time = (current_time - last_time) / 1000.0f;
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
