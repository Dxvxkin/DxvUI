#include <DxvUI/DxvEvent.h>
#include <DxvUI/FpsCounter.h>
#include <DxvUI/Log.h>
#include <DxvUI/Scene.h>
#include <DxvUI/UIContext.h>
#include <DxvUI/core.h>
#include <DxvUI/renderers/SDLRenderer.h>
#include <DxvUI/sources/SDLEventSource.h>
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

constexpr int SCREEN_WIDTH = 800;
constexpr int SCREEN_HEIGHT = 600;

namespace {

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

extern "C" int SDL_main(int /*argc*/, char* /*argv*/[]) {
    DxvUI::Log::init();
    DxvUI::Log::info("Slider Example Started.");

    DxvUI::SDLRenderer renderer_impl("DxvUI Sliders Example", SCREEN_WIDTH, SCREEN_HEIGHT);
    DxvUI::IRenderer& renderer = renderer_impl;
    DxvUI::SDLEventSource eventSource;
    auto scene = DxvUI::Scene::create();
    scene->setRenderer(&renderer);

    // Keeps the Change-handler connections alive for the whole app lifetime;
    // destroying one of these tokens would unsubscribe its handler.
    std::vector<std::unique_ptr<DxvUI::SceneNode::Connection>> connections;

    auto root = scene->getRoot();
    root->setStyle({.textColor = DxvUI::Colors::DarkGray,
                    .fontSize = 16,
                    .fontFamily = "Sans",
                    .width = SCREEN_WIDTH,
                    .height = SCREEN_HEIGHT},
                   DxvUI::WidgetState::Normal);

    // Frame-rate readout pinned to the top-right corner.
    auto fpsLabel = DxvUI::Label::create("fps_label", "FPS: --");
    fpsLabel->setStyle({.top = 10, .right = 10}, DxvUI::WidgetState::Normal);
    root->addChild(fpsLabel);

    // --- Horizontal slider with a step (0..100 by 5) ---
    root->addChild(
        makeCaption("h_step_caption", "Horizontal slider, step 5 (drag / wheel / arrows)", 40, 40));

    auto volume = DxvUI::SliderHorizontal::create("volume", 0.0f, 100.0f, 5.0f);
    volume->setStyle({.left = 40, .top = 70, .width = 300}, DxvUI::WidgetState::Normal);

    auto volumeValue = makeValueLabel("volume_value", 360, 76);
    volumeValue->setText("Volume: 0");
    root->addChild(volumeValue);

    connections.push_back(volume->on(
        DxvUI::EventType::Change, [volume, volumeValue](DxvUI::DxvEvent&, const DxvUI::UIContext&) {
            volumeValue->setText(std::format("Volume: {:.0f}%", volume->getValue()));
        }));
    root->addChild(volume);

    // --- Horizontal slider without a step (continuous 0..1) ---
    root->addChild(makeCaption(
        "h_free_caption", "Horizontal slider, continuous 0..1 (no step, Shift for fine)", 40, 130));

    auto intensity = DxvUI::SliderHorizontal::create("intensity", 0.0f, 1.0f, 0.0f);
    intensity->setStyle({.left = 40, .top = 160, .width = 300}, DxvUI::WidgetState::Normal);

    auto intensityValue = makeValueLabel("intensity_value", 360, 166);
    intensityValue->setText("Intensity: 0.00");
    root->addChild(intensityValue);

    connections.push_back(intensity->on(
        DxvUI::EventType::Change,
        [intensity, intensityValue](DxvUI::DxvEvent&, const DxvUI::UIContext&) {
            intensityValue->setText(std::format("Intensity: {:.2f}", intensity->getValue()));
        }));
    root->addChild(intensity);

    // --- Vertical slider (0..10 by 1) ---
    root->addChild(makeCaption("v_caption", "Vertical slider, step 1 (value grows up)", 480, 40));

    auto level = DxvUI::SliderVertical::create("level", 0.0f, 10.0f, 1.0f);
    level->setStyle({.left = 500, .top = 70, .height = 240}, DxvUI::WidgetState::Normal);

    auto levelValue = makeValueLabel("level_value", 500, 330);
    levelValue->setText("Level: 0");
    root->addChild(levelValue);

    connections.push_back(level->on(
        DxvUI::EventType::Change, [level, levelValue](DxvUI::DxvEvent&, const DxvUI::UIContext&) {
            levelValue->setText(std::format("Level: {:.0f}", level->getValue()));
        }));
    root->addChild(level);

    // --- Reset button returns all sliders to their minimum ---
    auto reset = DxvUI::Button::create("reset", "Reset");
    reset->setStyle({.left = 40, .top = 380, .width = 120, .height = 40},
                    DxvUI::WidgetState::Normal);
    connections.push_back(
        reset->on(DxvUI::EventType::Click,
                  [volume, intensity, level](DxvUI::DxvEvent&, const DxvUI::UIContext&) {
                      volume->setValue(0.0f);
                      intensity->setValue(0.0f);
                      level->setValue(0.0f);
                      DxvUI::Log::info("Sliders reset to their minimum");
                  }));
    root->addChild(reset);

    scene->updateLayout();

    bool quit = false;
    SDL_Event sdl_event;

    DxvUI::FpsCounter fps;
    auto fpsLabelPtr = scene->findNodeById("fps_label")->as<DxvUI::Label>();

    // Optional headless run for profiling: when DXVUI_FRAMES is set, the loop
    // exits after that many frames (no interaction needed). Not set = interactive.
    const char* frames_env = std::getenv("DXVUI_FRAMES");
    const long frame_cap = frames_env ? std::atol(frames_env) : 0;
    long frame_count = 0;

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

        renderer.clear(DxvUI::Colors::White);
        scene->draw();
        renderer.present();

        fps.tick();
        fpsLabelPtr->setText(
            std::format("FPS: {:.0f} ({:.1f} ms)", fps.getFps(), fps.getFrameTimeMs()));

        frame_count++;
        if (frame_cap > 0 && frame_count >= frame_cap) {
            quit = true;
        }
    }
    scene->shutdown();
    scene.reset();
    return 0;
}
