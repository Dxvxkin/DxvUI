#include "DxvUI/containers/CenterContainer.h"


#include <DxvUI/DxvEvent.h>
#include <DxvUI/FpsCounter.h>
#include <DxvUI/Log.h>
#include <DxvUI/Scene.h>
#include <DxvUI/core.h>
#include <DxvUI/renderers/SDLRenderer.h>
#include <DxvUI/sources/SDLEventSource.h>
#include <DxvUI/style/Colors.h>
#include <DxvUI/style/Style.h>
#include <DxvUI/widgets/Button.h>
#include <DxvUI/widgets/Label.h>
#include <DxvUI/widgets/Popup.h>
#include <SDL.h>

#include <cstdlib>
#include <format>
#include <memory>
#include <string>
#include <vector>

constexpr int SCREEN_WIDTH = 800;
constexpr int SCREEN_HEIGHT = 600;

namespace {

void buildPopupDemoUI(const std::shared_ptr<DxvUI::Scene>& scene,
                      std::vector<std::unique_ptr<DxvUI::SceneNode::Connection>>& connections) {
    auto root = scene->getRoot();

    root->setStyle({.textColor = DxvUI::Colors::DarkGray,
                    .fontSize = 18,
                    .fontFamily = "Sans",
                    .width = SCREEN_WIDTH,
                    .height = SCREEN_HEIGHT},
                   DxvUI::WidgetState::Normal);

    // --- Кнопка в root: открывает поп-ап (или закрывает, если уже открыт). ---
    auto openBtn = DxvUI::Button::create("popup_open_btn", "Open popup");
    openBtn->setStyle({.left = 300, .top = 250, .width = 180, .height = 50},
                      DxvUI::WidgetState::Normal);

    // --- Popup: создаётся закрытым, контент — абсолютно позиционированные дети. ---
    // Добавляем последним ребёнком root, чтобы он рисовался поверх остального.
    auto popup = DxvUI::Popup::create("demo_popup");
    popup->setStyle({.backgroundColor = DxvUI::Colors::Gray, .width = 220, .height = 110}, DxvUI::WidgetState::Normal);

    auto popupCtr = std::make_shared<DxvUI::CenterContainer>("pop_cntr");
    // Лейбл внутри поп-апа.
    auto popupLabel = DxvUI::Label::create("popup_label", "Hello from popup!");
    popupLabel->setStyle({.left = 0, .top = 0}, DxvUI::WidgetState::Normal);
    popupCtr->addChild(popupLabel);
    popup->addChild(popupCtr);
    // Кнопка закрытия поп-апа.
    auto closeBtn = DxvUI::Button::create("popup_close_btn", "Close");
    closeBtn->setStyle({.left = 0, .top = 40, .width = 120, .height = 32},
                       DxvUI::WidgetState::Normal);
    connections.push_back(closeBtn->on(DxvUI::EventType::Click,
                                       [popup, openBtn](DxvUI::DxvEvent&, const DxvUI::UIContext&) {
                                           popup->hide();
                                           openBtn->setText("Open popup");
                                           DxvUI::Log::info("[popup] закрыт");
                                       }));
    popup->addChild(closeBtn);

    root->addChild(popup);

    connections.push_back(openBtn->on(DxvUI::EventType::Click,
                                      [popup, openBtn](DxvUI::DxvEvent&, const DxvUI::UIContext&) {
                                          if (popup->isOpen()) {
                                              popup->hide();
                                              openBtn->setText("Open popup");
                                          } else {
                                              popup->showAt(300, 100);
                                              openBtn->setText("Hide popup");
                                          }
                                          DxvUI::Log::info("[popup] открыт={}", popup->isOpen());
                                      }));
    root->addChild(openBtn);

    // FPS-лейбл (обновляется в главном цикле).
    auto fpsLabel = DxvUI::Label::create("fps_label", "FPS: --");
    fpsLabel->setStyle({.top = 10, .right = 10}, DxvUI::WidgetState::Normal);
    root->addChild(fpsLabel);
}

// Headless-режим (запускается с DXVUI_FRAMES): открыть поп-ап, проверить, закрыть.
void runScriptedPopup(const std::shared_ptr<DxvUI::Scene>& scene,
                      std::vector<std::unique_ptr<DxvUI::SceneNode::Connection>>& connections) {
    auto clickAt = [&](int x, int y) {
        DxvUI::DxvEvent e;
        e.type = DxvUI::EventType::MouseMove;
        e.mouse.x = x;
        e.mouse.y = y;
        e.mouse.button = DxvUI::MouseButton::None;
        scene->processEvent(e);
        e.type = DxvUI::EventType::MouseDown;
        e.mouse.button = DxvUI::MouseButton::Left;
        scene->processEvent(e);
        e.type = DxvUI::EventType::MouseUp;
        scene->processEvent(e);
    };

    DxvUI::Log::info("=== Scripted Popup demo (headless) ===");
    bool ok = true;

    auto popup = scene->findNodeById("demo_popup")->as<DxvUI::Popup>();
    auto openBtn = scene->findNodeById("popup_open_btn")->as<DxvUI::Button>();

    ok &= !popup->isOpen();
    DxvUI::Log::info("[check] поп-ап закрыт по умолчанию: {}", !popup->isOpen() ? "да" : "НЕТ");

    clickAt(390, 275);
    scene->update();
    ok &= popup->isOpen();
    DxvUI::Log::info("[check] поп-ап открыт после клика по кнопке: {}",
                     popup->isOpen() ? "да" : "НЕТ");

    clickAt(368, 164);  // Close внутри поп-апа: 300+8+0..120, 100+8+40..72
    scene->update();
    ok &= !popup->isOpen();
    DxvUI::Log::info("[check] поп-ап закрыт после клика по Close: {}",
                     !popup->isOpen() ? "да" : "НЕТ");
    ok &= openBtn->getText() == "Open popup";
    DxvUI::Log::info("[check] текст кнопки восстановлен: {}",
                     openBtn->getText() == "Open popup" ? "да" : "НЕТ");

    // Мёртвых токенов нет — кнопки и поп-ап живы.
    std::erase_if(connections, [](const auto& c) { return c->expired(); });
    DxvUI::Log::info("[check] итог: {}", ok ? "PASS" : "FAIL");
}

}  // namespace

extern "C" int SDL_main(int /*argc*/, char* /*argv*/[]) {
    DxvUI::Log::init();
    DxvUI::Log::info("Logger Initialized.");

    DxvUI::SDLRenderer dxv_renderer_impl("DxvUI Popup Example", SCREEN_WIDTH, SCREEN_HEIGHT);
    DxvUI::IRenderer& dxv_renderer = dxv_renderer_impl;
    DxvUI::SDLEventSource eventSource;
    auto scene = DxvUI::Scene::create();
    scene->setRenderer(&dxv_renderer);

    std::vector<std::unique_ptr<DxvUI::SceneNode::Connection>> connections;
    buildPopupDemoUI(scene, connections);
    scene->updateLayout();

    // Headless-прогон для проверки (задана DXVUI_FRAMES).
    if (std::getenv("DXVUI_FRAMES")) {
        runScriptedPopup(scene, connections);
        scene->shutdown();
        scene.reset();
        return 0;
    }

    bool quit = false;
    SDL_Event sdl_event;

    DxvUI::FpsCounter fps;
    auto fpsLabel = scene->findNodeById("fps_label")->as<DxvUI::Label>();

    while (!quit) {
        while (SDL_PollEvent(&sdl_event) != 0) {
            DxvUI::DxvEvent dxv_event;
            if (eventSource.processEvent(sdl_event, dxv_event)) {
                if (dxv_event.type == DxvUI::EventType::Quit) {
                    quit = true;
                } else {
                    scene->processEvent(dxv_event);
                }
            }
        }

        scene->update();

        dxv_renderer.clear(DxvUI::Colors::White);
        scene->draw();
        dxv_renderer.present();

        fps.tick();
        fpsLabel->setText(
            std::format("FPS: {:.0f} ({:.1f} ms)", fps.getFps(), fps.getFrameTimeMs()));

        std::erase_if(connections, [](const auto& c) { return c->expired(); });
    }
    scene->shutdown();
    scene.reset();
    return 0;
}