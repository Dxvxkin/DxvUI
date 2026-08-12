#include <DxvUI/DxvUI.h>
#include <DxvUI/FpsCounter.h>
#include <DxvUI/Log.h>
#include <DxvUI/renderers/SDLRenderer.h>
#include <DxvUI/sources/SDLEventSource.h>
#include <DxvUI/style/Colors.h>
#include <DxvUI/widgets/Button.h>
#include <DxvUI/widgets/Label.h>
#include <SDL.h>

#include <cstdlib>
#include <format>
#include <iostream>
#include <memory>
#include <vector>

#include "DxvUI/containers/HorizontalContainer.h"

constexpr int SCREEN_WIDTH = 800;
constexpr int SCREEN_HEIGHT = 600;

void ContanersBuildText(std::shared_ptr<DxvUI::SceneNode>& root,
                        std::vector<std::unique_ptr<DxvUI::SceneNode::Connection>>& connections) {
    auto h_container = std::make_shared<DxvUI::HorizontalContainer>("container_horizontal");
    h_container->setSpacing(30);
    h_container->setStyle({.borderColor = DxvUI::Colors::Red,
                           .borderThickness = 1,
                           .left = 0,
                           .top = 0,
                           .width = SCREEN_WIDTH,
                           .padding = DxvUI::Thickness(10, 50, 10, 50)},
                          DxvUI::WidgetState::Normal);

    auto btn = DxvUI::Button::create("h_btn1", "Хуй");

    h_container->addChild(btn);
    connections.push_back(
        btn->on(DxvUI::EventType::Click, [](DxvUI::DxvEvent& event, const DxvUI::UIContext&) {
            if (auto target = event.getTarget()) {
                if (auto p = target->getParent().lock()) {
                    p->updateStyle({.borderColor = DxvUI::Colors::Green});
                }
            }
        }));

    h_container->addChild(DxvUI::Button::create("h_btn2", "h_btn2"));
    h_container->addChild(DxvUI::Button::create("h_btn3", "h_btn3"));

    root->addChild(h_container);
}

void buildUI(const std::shared_ptr<DxvUI::Scene>& scene,
             std::vector<std::unique_ptr<DxvUI::SceneNode::Connection>>& connections) {
    auto root = scene->getRoot();

    // Set global font properties on the root node. These will be inherited by
    // children.
    root->setStyle({.textColor = DxvUI::Colors::DarkGray,
                    .fontSize = 18,
                    .fontFamily = "Sans",
                    .width = SCREEN_WIDTH,
                    .height = SCREEN_HEIGHT},
                   DxvUI::WidgetState::Normal);

    // Frame-rate readout pinned to the top-right corner; the text is refreshed
    // by the main loop, and setText() is a no-op while the value stays stable.
    auto fpsLabel = DxvUI::Label::create("fps_label", "FPS: --");
    fpsLabel->setStyle({.top = 10, .right = 10}, DxvUI::WidgetState::Normal);
    root->addChild(fpsLabel);

    // --- Button 1: Uses default styles + overrides ---
    auto myButton = DxvUI::Button::create("my_button", "Click Me!");

    // connections.push_back(root->on(DxvUI::EventType::Change, [](DxvUI::DxvEvent& event) {
    //     DxvUI::Log::info("Root callback");
    //     DxvUI::Log::info("{} ::onChange({}) ", event.getTargetId(),
    //                      event.getTarget()->getBinding()->getString());
    // }));

    connections.push_back(
        myButton->on(DxvUI::EventType::Change, [](DxvUI::DxvEvent& event, const DxvUI::UIContext&) {
            DxvUI::Log::info("Button callback");
            DxvUI::Log::info("{} ::onChange({}) ", event.getTargetId(),
                             event.getTarget()->getBinding()->getString());
        }));

    connections.push_back(
        myButton->on(DxvUI::EventType::Attach, [](DxvUI::DxvEvent& event, const DxvUI::UIContext&) {
            if (event.getTarget() == event.getCurrentTarget()) {
                DxvUI::Log::info("{} Attach", event.getTargetId());
            }
        }));

    // Override only position and size. Colors, padding, etc., will come from the
    // Button's default theme.
    myButton->setStyle({.left = 50, .top = 50, .width = 200, .height = 50},
                       DxvUI::WidgetState::Normal);

    // We can still override state-specific styles if needed.
    // The base for this will be the Button's default Hovered style.
    myButton->setStyle({.borderColor = DxvUI::Colors::White, .borderThickness = 2},
                       DxvUI::WidgetState::Hovered);

    connections.push_back(myButton->on(
        DxvUI::EventType::Click,
        [&connections](DxvUI::DxvEvent& event, const DxvUI::UIContext& ui) {
            const auto viewport = ui.getViewport();
            int randomX = rand() % static_cast<int>(viewport.width - 200);
            int randomY = rand() % static_cast<int>(viewport.height - 50);

            static size_t label_count = 0;
            auto label = DxvUI::Label::create(std::format("label_{}", label_count++),
                                              std::format("Click to remove {}", label_count));

            // Set style for the new label
            label->setStyle(
                {
                    .backgroundColor = DxvUI::Color(0, 0, 0, 80),
                    .textColor = DxvUI::Colors::White,
                    .borderColor = DxvUI::Colors::Black,
                    .borderThickness = 1,
                    .borderRadius = 5,
                    .left = (float)randomX,
                    .top = (float)randomY,
                    .padding = DxvUI::Thickness{5, 5, 5, 5},
                },
                DxvUI::WidgetState::Normal);
            label->setStyle({.backgroundColor = (DxvUI::Color(0, 0, 0, 150)),
                             .borderThickness = 1,
                             .borderRadius = 10},
                            DxvUI::WidgetState::Hovered);

            connections.push_back(label->on(DxvUI::EventType::Click,
                                            [](DxvUI::DxvEvent& event, const DxvUI::UIContext&) {
                                                if (auto target = event.getTarget()) {
                                                    target->detach();
                                                }
                                            }));
            // Tokens of labels that were clicked away (and destroyed) are dead; drop
            // them so the connection list stays bounded.
            std::erase_if(connections, [](const auto& c) { return c->expired(); });

            ui.getRoot()->addChild(label);

            if (auto btn = event.getTarget()->as<DxvUI::Button>())
                btn->setText(std::format("Count {}", DxvUI::SceneNode::getNodeCount()));
        }));

    root->addChild(myButton);

    // --- Button 2: Uses default styles + different overrides ---
    auto btn2 = DxvUI::Button::create("find_btn", "Find");
    // Override position, size, and some colors.
    btn2->setStyle({.backgroundColor = DxvUI::Colors::DarkOrange,
                    .textColor = DxvUI::Colors::MidnightBlue,
                    .left = 300,
                    .top = 50,
                    .width = 200,
                    .height = 50},
                   DxvUI::WidgetState::Normal);

    connections.push_back(
        btn2->on(DxvUI::EventType::Click, [](DxvUI::DxvEvent&, const DxvUI::UIContext& ui) {
            if (auto label = ui.findNodeById("label_8")) {
                label->as<DxvUI::Label>()->setText("Found!");
            }
        }));

    root->addChild(btn2);

    // --- Button 3: Test default styles ---
    auto btn_test_def_styles = DxvUI::Button::create("btn_defStyle", "test");
    btn_test_def_styles->setStyle({.left = 500, .top = 500, .width = 100, .height = 50},
                                  DxvUI::WidgetState::Normal);
    connections.push_back(btn_test_def_styles->on(
        DxvUI::EventType::Click, [](DxvUI::DxvEvent&, const DxvUI::UIContext& ui) {
            if (auto target = ui.findNodeById("label_7")) {
                auto style = target->as<DxvUI::Label>()->getStyle().get(DxvUI::WidgetState::Normal);
                auto newStyle = DxvUI::StyleRule(*style);
                newStyle.padding->left += 5;
                newStyle.padding->top += 5;
                newStyle.padding->right += 5;
                newStyle.padding->bottom += 5;
                target->setStyle(newStyle, DxvUI::WidgetState::Normal);
            }
        }));
    root->addChild(btn_test_def_styles);
    ContanersBuildText(root, connections);
}

extern "C" int SDL_main(int /*argc*/, char* /*argv*/[]) {
    DxvUI::Log::init();
    DxvUI::Log::info("Logger Initialized.");

    DxvUI::SDLRenderer dxv_renderer_impl("DxvUI Example", SCREEN_WIDTH, SCREEN_HEIGHT);
    DxvUI::IRenderer& dxv_renderer = dxv_renderer_impl;
    DxvUI::SDLEventSource eventSource;
    auto scene = DxvUI::Scene::create();
    scene->setRenderer(&dxv_renderer);

    // Keeps the event handler connections alive for the whole app lifetime;
    // destroying one of these tokens would unsubscribe its handler.
    std::vector<std::unique_ptr<DxvUI::SceneNode::Connection>> connections;

    buildUI(scene, connections);
    scene->updateLayout();

    DxvUI::Log::info("Initial node count: {}", DxvUI::SceneNode::getNodeCount());

    bool quit = false;
    SDL_Event sdl_event;

    DxvUI::FpsCounter fps;
    DxvUI::FpsCounter updateMs;
    DxvUI::FpsCounter drawMs;
    auto fpsLabel = scene->findNodeById("fps_label")->as<DxvUI::Label>();

    // Optional headless run for profiling: when DXVUI_FRAMES is set, the loop
    // exits after that many frames (no interaction needed). Not set = interactive.
    const char* frames_env = std::getenv("DXVUI_FRAMES");
    const long frame_cap = frames_env ? std::atol(frames_env) : 0;
    long frame_count = 0;

    while (!quit) {
        while (SDL_PollEvent(&sdl_event) != 0) {
            DxvUI::DxvEvent dxv_event;
            if (eventSource.processEvent(sdl_event, dxv_event)) {
                if (dxv_event.type == DxvUI::EventType::Quit)
                    quit = true;
                else
                    scene->processEvent(dxv_event);
            }
        }

        const auto tUpdate = std::chrono::steady_clock::now();
        scene->update();
        updateMs.recordMs(
            std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - tUpdate)
                .count());

        const auto tDraw = std::chrono::steady_clock::now();
        dxv_renderer.clear(DxvUI::Colors::White);
        scene->draw();
        drawMs.recordMs(
            std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - tDraw)
                .count());

        dxv_renderer.present();

        fps.tick();
        fpsLabel->setText(std::format("FPS: {:.0f} (up {:.2f} ms · draw {:.2f} ms)", fps.getFps(),
                                      updateMs.getFrameTimeMs(), drawMs.getFrameTimeMs()));

        frame_count++;
        if (frame_cap > 0 && frame_count >= frame_cap) {
            quit = true;
        }
    }
    scene->shutdown();
    scene.reset();
    DxvUI::Log::info("Final node count: {}", DxvUI::SceneNode::getNodeCount());

    return 0;
}
