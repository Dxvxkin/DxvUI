// examples/scroll.cpp
//
// ScrollContainer demo: a fixed viewport that scrolls a tall vertical list of rows
// with the mouse wheel. It subclasses DxvUIEx::SdlApp and owns the DxvUI
// integration itself (see examples/main.cpp for the pattern).

#include <DxvUI/DxvEvent.h>
#include <DxvUI/FpsCounter.h>
#include <DxvUI/Log.h>
#include <DxvUI/Scene.h>
#include <DxvUI/UIContext.h>
#include <DxvUI/backend/SDLEventSource.h>
#include <DxvUI/backend/SDLRenderer.h>
#include <DxvUI/containers/ScrollContainer.h>
#include <DxvUI/containers/VerticalContainer.h>
#include <DxvUI/style/Colors.h>
#include <DxvUI/style/Style.h>
#include <DxvUI/widgets/Button.h>
#include <DxvUI/widgets/Label.h>
#include <SDL.h>

#include <format>
#include <memory>
#include <string>

#include "App.h"

namespace {

constexpr int SCREEN_WIDTH = 800;
constexpr int SCREEN_HEIGHT = 600;

}  // namespace

class DxvUIScrollExample : public DxvUIEx::SdlApp {
   public:
    DxvUIScrollExample() : DxvUIEx::SdlApp("DxvUI Scroll Example", SCREEN_WIDTH, SCREEN_HEIGHT) {}

   protected:
    bool init() override {
        dxvRenderer_ = std::make_unique<DxvUI::SDLRenderer>(renderer_);
        scene_ = DxvUI::Scene::create();
        scene_->setRenderer(dxvRenderer_.get());

        auto root = scene_->getRoot();
        root->setStyle({.textColor = DxvUI::Colors::DarkGray,
                        .fontSize = 16,
                        .fontFamily = "Sans",
                        .width = SCREEN_WIDTH,
                        .height = SCREEN_HEIGHT},
                       DxvUI::WidgetState::Normal);

        buildScrollDemoUI(root);
        scene_->updateLayout();
        return true;
    }

    void update(float /*dtMs*/) override {
        scene_->update();

        fps_.tick();
        fpsLabel_->setText(
            std::format("FPS: {:.0f} ({:.1f} ms)", fps_.getFps(), fps_.getFrameTimeMs()));
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
    void buildScrollDemoUI(const std::shared_ptr<DxvUI::SceneNode>& root) {
        auto fpsLabel = DxvUI::Label::create("fps_label", "FPS: --");
        fpsLabel->setStyle({.top = 10, .right = 10}, DxvUI::WidgetState::Normal);
        root->addChild(fpsLabel);
        fpsLabel_ = fpsLabel;

        auto caption = DxvUI::Label::create(
            "scroll_caption", "ScrollContainer: hover the list and use the mouse wheel");
        caption->setStyle({.left = 40, .top = 40}, DxvUI::WidgetState::Normal);
        root->addChild(caption);

        // A fixed-size viewport that clips and scrolls its single child list.
        auto scroll = DxvUI::ScrollContainer::create("list_scroll");
        scroll->setStyle({.left = 40, .top = 70, .width = 420, .height = 300},
                         DxvUI::WidgetState::Normal);

        auto items = std::make_shared<DxvUI::VerticalContainer>("items");
        items->setStyle({.gap = 4}, DxvUI::WidgetState::Normal);
        for (int i = 0; i < 40; ++i) {
            auto row = DxvUI::Label::create("row" + std::to_string(i), std::format("Row #{}", i));
            row->setStyle({.left = 6, .top = 6, .width = 408, .height = 30},
                          DxvUI::WidgetState::Normal);
            items->addChild(row);
        }
        scroll->addChild(items);
        root->addChild(scroll);

        auto hint =
            DxvUI::Label::create("scroll_hint", "Wheel scrolls on hover; buttons scroll by 40px");
        hint->setStyle({.left = 500, .top = 80}, DxvUI::WidgetState::Normal);
        root->addChild(hint);

        // Programmatic scroll buttons (the wheel also works on hover).
        auto up = DxvUI::Button::create("scroll_up", "Up");
        up->setStyle({.left = 500, .top = 130, .width = 100, .height = 36},
                     DxvUI::WidgetState::Normal);
        connections_.push_back(
            up->on(DxvUI::EventType::Click, [scroll](DxvUI::DxvEvent&, const DxvUI::UIContext&) {
                scroll->scrollBy(0.0f, -40.0f);
            }));
        root->addChild(up);

        auto down = DxvUI::Button::create("scroll_down", "Down");
        down->setStyle({.left = 500, .top = 176, .width = 100, .height = 36},
                       DxvUI::WidgetState::Normal);
        connections_.push_back(
            down->on(DxvUI::EventType::Click, [scroll](DxvUI::DxvEvent&, const DxvUI::UIContext&) {
                scroll->scrollBy(0.0f, 40.0f);
            }));
        root->addChild(down);
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
    DxvUI::Log::info("Scroll Example Started.");

    DxvUIScrollExample app;
    return app.run();
}
