// examples/external_renderer.cpp
//
// Demonstrates the primary DxvUI integration mode: embedding a UI into an
// application's own SDL loop, like a real host would. It subclasses DxvUIEx::SdlApp
// (a plain SDL scaffold that owns the window, renderer and main loop) and owns the
// DxvUI integration itself:
//
//   * init()        — creates DxvUI::SDLRenderer around the scaffold's renderer and
//                      builds the scene/widgets;
//   * handleEvent() — feeds SDL events into DxvUI::SDLEventSource and the scene;
//   * update()      — advances the scene and sets the clear colour;
//   * draw()        — draws the host's own SDL content FIRST, then the DxvUI UI on top.
//
// The SdlApp loop clears the frame (SDL_RenderClear) before calling draw(), so the
// example only draws its own content and then the UI over it. SDLRenderer::
// getSDLHandle() exposes the underlying SDL_Renderer for mixed rendering.

#include <DxvUI/DxvUI.h>
#include <SDL.h>

#include <format>
#include <memory>
#include <vector>

#include "App.h"

class ExternalRendererApp : public DxvUIEx::SdlApp {
   public:
    ExternalRendererApp() : DxvUIEx::SdlApp("DxvUI External Renderer", 800, 600, true) {}

   protected:
    bool init() override {
        // Attach the UI to the host (scaffold) renderer: the primary integration.
        dxvRenderer_ = std::make_unique<DxvUI::SDLRenderer>(renderer_);
        scene_ = DxvUI::Scene::create();
        scene_->setRenderer(dxvRenderer_.get());

        buildUI();
        return true;
    }

    void update(float /*dtMs*/) override {
        // Background the clear will use (SDL_RenderClear happens before draw()).
        SDL_SetRenderDrawColor(renderer_, 235, 235, 235, 255);

        scene_->update();

        if (viewportLabel_) {
            const auto v = dxvRenderer_->getViewportSize();
            viewportLabel_->setText(std::format("Viewport: {:.0f} x {:.0f}", v.width, v.height));
        }
    }

    void draw() override {
        // 1) The host's own content drawn through plain SDL (e.g. an existing game
        //    scene). Draw a simple diagonal grid to simulate app-side rendering.
        SDL_SetRenderDrawColor(renderer_, 200, 200, 200, 255);
        const auto v = dxvRenderer_->getViewportSize();
        for (int x = 0; x <= static_cast<int>(v.width); x += 40) {
            SDL_RenderDrawLine(renderer_, x, 0, x, static_cast<int>(v.height));
        }
        for (int y = 0; y <= static_cast<int>(v.height); y += 40) {
            SDL_RenderDrawLine(renderer_, 0, y, static_cast<int>(v.width), y);
        }

        // 2) The DxvUI UI on top of the host content (no clear() here — the frame
        //    was already cleared by the SdlApp loop).
        scene_->draw();
    }

    bool handleEvent(const SDL_Event& event) override {
        DxvUI::DxvEvent dxv;
        if (!eventSource_.processEvent(event, dxv)) return false;

        if (dxv.type == DxvUI::EventType::Quit) return true;

        if (dxv.type == DxvUI::EventType::Resize) {
            DxvUI::Log::info("[external_renderer] window resized to {} x {}", dxv.resize.width,
                             dxv.resize.height);
        }

        scene_->processEvent(dxv);
        return false;
    }

   private:
    void buildUI() {
        auto root = scene_->getRoot();
        root->setStyle({.textColor = DxvUI::Colors::DarkGray,
                        .fontSize = 18,
                        .fontFamily = "Sans",
                        .width = dxvRenderer_->getViewportSize().width,
                        .height = dxvRenderer_->getViewportSize().height},
                       DxvUI::WidgetState::Normal);

        auto title = DxvUI::Label::create("ext_title", "External SDL renderer integration");
        title->setStyle({.left = 20, .top = 20}, DxvUI::WidgetState::Normal);
        root->addChild(title);

        auto viewportLabel = DxvUI::Label::create("ext_viewport", "Viewport: - x -");
        viewportLabel->setStyle({.top = 10, .right = 10}, DxvUI::WidgetState::Normal);
        root->addChild(viewportLabel);
        viewportLabel_ = viewportLabel;

        auto btn = DxvUI::Button::create("ext_btn", "Click me");
        btn->setStyle({.left = 20, .top = 80, .width = 200, .height = 50},
                      DxvUI::WidgetState::Normal);
        connections_.push_back(
            btn->on(DxvUI::EventType::Click, [](DxvUI::DxvEvent&, const DxvUI::UIContext&) {
                DxvUI::Log::info("[external_renderer] button clicked");
            }));
        root->addChild(btn);

        auto cb = DxvUI::Checkbox::create("ext_cb", "Checkbox");
        cb->setStyle({.left = 20, .top = 160}, DxvUI::WidgetState::Normal);
        connections_.push_back(cb->on(DxvUI::EventType::Change, [](DxvUI::DxvEvent& event,
                                                                   const DxvUI::UIContext&) {
            auto c = event.getTarget()->as<DxvUI::Checkbox>();
            DxvUI::Log::info("[external_renderer] checkbox checked={}", c ? c->isChecked() : false);
        }));
        root->addChild(cb);

        auto te = DxvUI::TextEdit::create("ext_text", "editable text");
        te->setStyle({.left = 20, .top = 220, .width = 260, .height = 32},
                     DxvUI::WidgetState::Normal);
        root->addChild(te);
    }

    std::unique_ptr<DxvUI::SDLRenderer> dxvRenderer_;
    std::shared_ptr<DxvUI::Scene> scene_;
    DxvUI::SDLEventSource eventSource_;
    std::vector<std::unique_ptr<DxvUI::SceneNode::Connection>> connections_;
    std::shared_ptr<DxvUI::Label> viewportLabel_;
};

#ifdef _WIN32
extern "C" int SDL_main(int /*argc*/, char* /*argv*/[]) {
#else
int main(int /*argc*/, char* /*argv*/[]) {
#endif
    // The SdlApp scaffold is DxvUI-agnostic, so the example initializes DxvUI's
    // logger itself.
    DxvUI::Log::init();

    ExternalRendererApp app;
    return app.run();
}
