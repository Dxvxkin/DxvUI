// examples/popup.cpp
//
// Popup demo: an absolutely-positioned popup toggled by a button. It subclasses
// DxvUIEx::SdlApp and owns the DxvUI integration itself (see examples/main.cpp
// for the pattern).

#include <DxvUI/DxvEvent.h>
#include <DxvUI/FpsCounter.h>
#include <DxvUI/Log.h>
#include <DxvUI/Scene.h>
#include <DxvUI/containers/CenterContainer.h>
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

#include "App.h"

namespace {

constexpr int SCREEN_WIDTH = 800;
constexpr int SCREEN_HEIGHT = 600;

}  // namespace

class DxvUIPopupExample : public DxvUIEx::SdlApp {
   public:
    DxvUIPopupExample() : DxvUIEx::SdlApp("DxvUI Popup Example", SCREEN_WIDTH, SCREEN_HEIGHT) {}

   protected:
    bool init() override {
        dxvRenderer_ = std::make_unique<DxvUI::SDLRenderer>(renderer_);
        scene_ = DxvUI::Scene::create();
        scene_->setRenderer(dxvRenderer_.get());

        auto root = scene_->getRoot();
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
        popup->setStyle({.backgroundColor = DxvUI::Colors::Gray, .width = 220, .height = 110},
                        DxvUI::WidgetState::Normal);

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
        connections_.push_back(closeBtn->on(
            DxvUI::EventType::Click, [popup, openBtn](DxvUI::DxvEvent&, const DxvUI::UIContext&) {
                popup->hide();
                openBtn->setText("Open popup");
                DxvUI::Log::info("[popup] закрыт");
            }));
        popup->addChild(closeBtn);

        root->addChild(popup);

        connections_.push_back(openBtn->on(
            DxvUI::EventType::Click, [popup, openBtn](DxvUI::DxvEvent&, const DxvUI::UIContext&) {
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
        fpsLabel_ = fpsLabel;

        scene_->updateLayout();

        // Headless-режим (запускается с DXVUI_FRAMES): открыть поп-ап, проверить, закрыть.
        if (std::getenv("DXVUI_FRAMES")) {
            runScriptedPopup();
            return false;
        }
        return true;
    }

    void update(float /*dtMs*/) override {
        scene_->update();

        fps_.tick();
        fpsLabel_->setText(
            std::format("FPS: {:.0f} ({:.1f} ms)", fps_.getFps(), fps_.getFrameTimeMs()));

        // Мёртвых токенов нет — кнопки и поп-ап живы.
        std::erase_if(connections_, [](const auto& c) { return c->expired(); });
    }

    void draw() override { scene_->draw(); }

    bool handleEvent(const SDL_Event& event) override {
        DxvUI::DxvEvent dxv;
        if (!eventSource_.processEvent(event, dxv)) return false;
        if (dxv.type == DxvUI::EventType::Quit) return true;
        scene_->processEvent(dxv);
        return false;
    }

   private:
    void runScriptedPopup() {
        auto clickAt = [&](int x, int y) {
            DxvUI::DxvEvent e;
            e.type = DxvUI::EventType::MouseMove;
            e.mouse.x = x;
            e.mouse.y = y;
            e.mouse.button = DxvUI::MouseButton::None;
            scene_->processEvent(e);
            e.type = DxvUI::EventType::MouseDown;
            e.mouse.button = DxvUI::MouseButton::Left;
            scene_->processEvent(e);
            e.type = DxvUI::EventType::MouseUp;
            scene_->processEvent(e);
        };

        DxvUI::Log::info("=== Scripted Popup demo (headless) ===");
        bool ok = true;

        auto popup = scene_->findNodeById("demo_popup")->as<DxvUI::Popup>();
        auto openBtn = scene_->findNodeById("popup_open_btn")->as<DxvUI::Button>();

        ok &= !popup->isOpen();
        DxvUI::Log::info("[check] поп-ап закрыт по умолчанию: {}", !popup->isOpen() ? "да" : "НЕТ");

        clickAt(390, 275);
        scene_->update();
        ok &= popup->isOpen();
        DxvUI::Log::info("[check] поп-ап открыт после клика по кнопке: {}",
                         popup->isOpen() ? "да" : "НЕТ");

        clickAt(368, 164);  // Close внутри поп-апа: 300+8+0..120, 100+8+40..72
        scene_->update();
        ok &= !popup->isOpen();
        DxvUI::Log::info("[check] поп-ап закрыт после клика по Close: {}",
                         !popup->isOpen() ? "да" : "НЕТ");
        ok &= openBtn->getText() == "Open popup";
        DxvUI::Log::info("[check] текст кнопки восстановлен: {}",
                         openBtn->getText() == "Open popup" ? "да" : "НЕТ");

        // Мёртвых токенов нет — кнопки и поп-ап живы.
        std::erase_if(connections_, [](const auto& c) { return c->expired(); });
        DxvUI::Log::info("[check] итог: {}", ok ? "PASS" : "FAIL");
    }

    std::unique_ptr<DxvUI::SDLRenderer> dxvRenderer_;
    std::shared_ptr<DxvUI::Scene> scene_;
    DxvUI::SDLEventSource eventSource_;
    std::vector<std::unique_ptr<DxvUI::SceneNode::Connection>> connections_;
    DxvUI::FpsCounter<> fps_;
    std::shared_ptr<DxvUI::Label> fpsLabel_;
};

#ifdef _WIN32
extern "C" int SDL_main(int /*argc*/, char* /*argv*/[]) {
#else
int main(int /*argc*/, char* /*argv*/[]) {
#endif
    DxvUI::Log::init();
    DxvUI::Log::info("Logger Initialized.");

    DxvUIPopupExample app;
    return app.run();
}
