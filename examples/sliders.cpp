// examples/sliders.cpp
//
// Sliders demo: horizontal (step and continuous) and vertical sliders plus a reset
// button. It subclasses DxvUIEx::SdlApp and owns the DxvUI integration itself (see
// examples/main.cpp for the pattern).

#include <DxvUI/DxvEvent.h>
#include <DxvUI/FpsCounter.h>
#include <DxvUI/Log.h>
#include <DxvUI/Scene.h>
#include <DxvUI/UIContext.h>
#include <DxvUI/backend/SDLEventSource.h>
#include <DxvUI/backend/SDLRenderer.h>
#include <DxvUI/style/Colors.h>
#include <DxvUI/style/Style.h>
#include <DxvUI/widgets/Button.h>
#include <DxvUI/widgets/Label.h>
#include <DxvUI/widgets/SliderHorizontal.h>
#include <DxvUI/widgets/SliderVertical.h>
#include <SDL.h>

#include <format>
#include <memory>
#include <string>
#include <vector>

#include "App.h"

namespace {

constexpr int SCREEN_WIDTH = 800;
constexpr int SCREEN_HEIGHT = 600;

std::shared_ptr<DxvUI::Label> makeCaption(const std::string& id, const std::string& text, int x,
                                          int y) {
    auto label = DxvUI::Label::create(id, std::move(text));
    label->setStyle({.fontSize = 14, .left = static_cast<float>(x), .top = static_cast<float>(y)},
                    DxvUI::WidgetState::Normal);
    return label;
}

// A live readout that shows the current value of a slider.
std::shared_ptr<DxvUI::Label> makeValueLabel(const std::string& id, int x, int y) {
    auto label = DxvUI::Label::create(id, "--");
    label->setStyle({.fontSize = 16, .left = static_cast<float>(x), .top = static_cast<float>(y)},
                    DxvUI::WidgetState::Normal);
    return label;
}

}  // namespace

class DxvUISlidersExample : public DxvUIEx::SdlApp {
   public:
    DxvUISlidersExample() : DxvUIEx::SdlApp("DxvUI Sliders Example", SCREEN_WIDTH, SCREEN_HEIGHT) {}

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

        buildSlidersDemoUI(root);
        scene_->updateLayout();
        return true;
    }

    void update(float /*dtMs*/) override {
        scene_->update();

        fps_.tick();
        fpsLabel_->setText(
            std::format("FPS: {:.0f} ({:.1f} ms)", fps_.getFps(), fps_.getFrameTimeMs()));

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
    // Keeps the Change-handler connections alive for the whole app lifetime;
    // destroying one of these tokens would unsubscribe its handler.
    void buildSlidersDemoUI(const std::shared_ptr<DxvUI::SceneNode>& root) {
        // Frame-rate readout pinned to the top-right corner.
        auto fpsLabel = DxvUI::Label::create("fps_label", "FPS: --");
        fpsLabel->setStyle({.top = 10, .right = 10}, DxvUI::WidgetState::Normal);
        root->addChild(fpsLabel);
        fpsLabel_ = fpsLabel;

        // --- Horizontal slider with a step (0..100 by 5) ---
        root->addChild(makeCaption("h_step_caption",
                                   "Horizontal slider, step 5 (drag / wheel / arrows)", 40, 40));

        auto volume = DxvUI::SliderHorizontal::create("volume", 0.0f, 100.0f, 5.0f);
        volume->setStyle({.left = 40, .top = 70, .width = 300}, DxvUI::WidgetState::Normal);

        auto volumeValue = makeValueLabel("volume_value", 360, 76);
        volumeValue->setText("Volume: 0");
        root->addChild(volumeValue);

        connections_.push_back(
            volume->on(DxvUI::EventType::Change,
                       [volume, volumeValue](DxvUI::DxvEvent&, const DxvUI::UIContext&) {
                           volumeValue->setText(std::format("Volume: {:.0f}%", volume->getValue()));
                       }));
        root->addChild(volume);

        // --- Horizontal slider without a step (continuous 0..1) ---
        root->addChild(makeCaption("h_free_caption",
                                   "Horizontal slider, continuous 0..1 (no step, Shift for fine)",
                                   40, 130));

        auto intensity = DxvUI::SliderHorizontal::create("intensity", 0.0f, 1.0f, 0.0f);
        intensity->setStyle({.left = 40, .top = 160, .width = 300}, DxvUI::WidgetState::Normal);

        auto intensityValue = makeValueLabel("intensity_value", 360, 166);
        intensityValue->setText("Intensity: 0.00");
        root->addChild(intensityValue);

        connections_.push_back(intensity->on(
            DxvUI::EventType::Change,
            [intensity, intensityValue](DxvUI::DxvEvent&, const DxvUI::UIContext&) {
                intensityValue->setText(std::format("Intensity: {:.2f}", intensity->getValue()));
            }));
        root->addChild(intensity);

        // --- Vertical slider (0..10 by 1) ---
        root->addChild(
            makeCaption("v_caption", "Vertical slider, step 1 (value grows up)", 480, 40));

        auto level = DxvUI::SliderVertical::create("level", 0.0f, 10.0f, 1.0f);
        level->setStyle({.left = 500, .top = 70, .height = 240}, DxvUI::WidgetState::Normal);

        auto levelValue = makeValueLabel("level_value", 500, 330);
        levelValue->setText("Level: 0");
        root->addChild(levelValue);

        connections_.push_back(
            level->on(DxvUI::EventType::Change,
                      [level, levelValue](DxvUI::DxvEvent&, const DxvUI::UIContext&) {
                          levelValue->setText(std::format("Level: {:.0f}", level->getValue()));
                      }));
        root->addChild(level);

        // --- Reset button returns all sliders to their minimum ---
        auto reset = DxvUI::Button::create("reset", "Reset");
        reset->setStyle({.left = 40, .top = 380, .width = 120, .height = 40},
                        DxvUI::WidgetState::Normal);
        connections_.push_back(
            reset->on(DxvUI::EventType::Click,
                      [volume, intensity, level](DxvUI::DxvEvent&, const DxvUI::UIContext&) {
                          volume->setValue(0.0f);
                          intensity->setValue(0.0f);
                          level->setValue(0.0f);
                          DxvUI::Log::info("Sliders reset to their minimum");
                      }));
        root->addChild(reset);
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
    DxvUI::Log::info("Slider Example Started.");

    DxvUISlidersExample app;
    return app.run();
}
