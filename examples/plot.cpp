// examples/plot.cpp
//
// Live plotting demo with two widgets:
//
// 1. An oscilloscope-style window over the per-frame update and draw phase
//    durations. Each phase is timed with a steady clock (same technique as
//    FpsOverlay.h), appended every frame, and the view slides left as the
//    512-point window fills (auto-scaling keeps the y-axis fitted to the
//    visible samples).
// 2. An animated sine/cosine pair: the phase advances each frame, so the two
//    curves scroll across the plot. World bounds are fixed, keeping the grid,
//    the axis labels and the y-scale stable.

#include <DxvUI/Log.h>
#include <DxvUI/Scene.h>
#include <DxvUI/backend/SDLEventSource.h>
#include <DxvUI/backend/SDLRenderer.h>
#include <DxvUI/style/Colors.h>
#include <DxvUI/widgets/Button.h>
#include <DxvUI/widgets/Label.h>
#include <DxvUI/widgets/Plot.h>
#include <SDL.h>

#include <chrono>
#include <cmath>
#include <cstdlib>
#include <memory>
#include <string>
#include <vector>

#include "App.h"

namespace {

constexpr int SCREEN_WIDTH = 800;
constexpr int SCREEN_HEIGHT = 600;
constexpr size_t kWindow = 512;  // sliding window depth (points per series)

// Animated sin/cos wave parameters.
constexpr int kWaveSamples = 128;
constexpr float kPi = 3.14159265358979f;
constexpr float kWaveCycles = 2.0f;  // periods across the plot
constexpr float kWaveXSpan = 2.0f * kWaveCycles * kPi;
constexpr float kWavePhaseStep = 0.03f;  // radians per frame

class DxvUIPlotExample : public DxvUIEx::SdlApp {
   public:
    DxvUIPlotExample() : DxvUIEx::SdlApp("DxvUI Plot Example", SCREEN_WIDTH, SCREEN_HEIGHT) {}

   protected:
    bool init() override {
        dxvRenderer_ = std::make_unique<DxvUI::SDLRenderer>(renderer_);
        scene_ = DxvUI::Scene::create();
        scene_->setRenderer(dxvRenderer_.get());

        const auto& root = scene_->getRoot();
        root->setStyle({.backgroundColor = DxvUI::Colors::LightGray}, DxvUI::WidgetState::Normal);

        auto caption = DxvUI::Label::create(
            "caption", "График 1: тайминги кадра.ms, График 2: sin/cos в движении");
        caption->setStyle(
            {.textColor = DxvUI::Colors::DarkGray, .fontSize = 16, .left = 12, .top = 8},
            DxvUI::WidgetState::Normal);
        root->addChild(caption);

        plot_ = DxvUI::Plot::create("plot");
        // Left gutter for the y-axis labels, bottom gutter for the x-axis labels
        // (labels need >= 8px of the corresponding padding).
        plot_->setStyle({.textColor = DxvUI::Colors::DarkGray,
                         .borderColor = DxvUI::Colors::Gray,
                         .borderThickness = 1,
                         .fontSize = 12,
                         .left = 12,
                         .top = 64,
                         .width = 560,
                         .height = 240,
                         .padding = DxvUI::Thickness{8, 12, 18, 44}},
                        DxvUI::WidgetState::Normal);
        updateSeries_ = plot_->addSeries("update");
        drawSeries_ = plot_->addSeries("draw");
        plot_->setSeriesColor(updateSeries_, DxvUI::Colors::CornflowerBlue);
        plot_->setSeriesColor(drawSeries_, DxvUI::Colors::Orange);
        plot_->setAreaEnabled(true);
        root->addChild(plot_);

        addLegend(root, "update", 12, 44);
        addLegend(root, "draw", 130, 44);

        wavePlot_ = DxvUI::Plot::create("wave_plot");
        wavePlot_->setStyle({.textColor = DxvUI::Colors::DarkGray,
                             .borderColor = DxvUI::Colors::Gray,
                             .borderThickness = 1,
                             .fontSize = 12,
                             .left = 12,
                             .top = 340,
                             .width = 560,
                             .height = 240,
                             .padding = DxvUI::Thickness{8, 12, 18, 44}},
                            DxvUI::WidgetState::Normal);
        waveSin_ = wavePlot_->addSeries("sin");
        waveCos_ = wavePlot_->addSeries("cos");
        wavePlot_->setSeriesColor(waveSin_, DxvUI::Colors::Red);
        wavePlot_->setSeriesColor(waveCos_, DxvUI::Colors::Green);
        // Fixed world bounds: the two harmonics share the same stable y-scale,
        // so the grid/axis labels never slide while the phase advances.
        wavePlot_->setWorldBounds(0.0f, -1.2f, kWaveXSpan, 1.2f);
        root->addChild(wavePlot_);

        addLegend(root, "sin", 12, 318, "");
        addLegend(root, "cos", 130, 318, "");

        updateWave();
        scene_->updateLayout();
        return true;
    }

    void update(float /*dtMs*/) override {
        const auto t0 = std::chrono::steady_clock::now();
        scene_->update();
        const auto t1 = std::chrono::steady_clock::now();
        pushSample(updateMs_, std::chrono::duration<double, std::milli>(t1 - t0).count());
        updateWave();
    }

    void draw() override {
        const auto t0 = std::chrono::steady_clock::now();
        scene_->draw();
        const auto t1 = std::chrono::steady_clock::now();
        pushSample(drawMs_, std::chrono::duration<double, std::milli>(t1 - t0).count());

        // Replay the window into the plot. Auto-scaling refits the y-axis to
        // exactly these samples, so the graph "zooms" as the tail slides; a
        // fixed y-domain (setWorldBounds) would be the alternative for stable
        // scaling.
        ++frameCount_;
        pushSeries(updateSeries_, updateMs_);
        pushSeries(drawSeries_, drawMs_);
    }

    bool handleEvent(const SDL_Event& event) override {
        DxvUI::DxvEvent dxv;
        if (!eventSource_.processEvent(event, dxv)) return false;
        if (dxv.type == DxvUI::EventType::Quit) return true;
        scene_->processEvent(dxv);
        return false;
    }

   private:
    // Appends one sample, trimming the window to the last kWindow entries.
    static void pushSample(std::vector<float>& data, float value) {
        data.push_back(value);
        if (data.size() > kWindow) {
            data.erase(begin(data), begin(data) + (data.size() - kWindow));
        }
    }

    // Translates the window into plot points with the current frame as the
    // right edge (x grows from frameCount - size..frameCount). The scratch
    // buffer is reused across frames; setData() moves it into the widget, so no
    // per-frame allocation happens.
    void pushSeries(size_t series, const std::vector<float>& data) {
        if (data.empty()) {
            return;
        }
        pendingPoints_.clear();
        pendingPoints_.reserve(data.size());
        const float firstX = static_cast<float>(frameCount_ - data.size());
        for (size_t i = 0; i < data.size(); ++i) {
            pendingPoints_.push_back({firstX + static_cast<float>(i), data[i]});
        }
        plot_->setData(series, std::move(pendingPoints_));
    }

    // Advances the wave phase and rebuilds the sin/cos samples for this frame.
    // Each series owns a scratch buffer, so no per-frame allocation happens.
    void updateWave() {
        wavePhase_ += kWavePhaseStep;

        waveSinBuf_.clear();
        waveSinBuf_.reserve(kWaveSamples);
        waveCosBuf_.clear();
        waveCosBuf_.reserve(kWaveSamples);
        for (int i = 0; i < kWaveSamples; ++i) {
            const float x = kWaveXSpan * i / (kWaveSamples - 1);
            waveSinBuf_.push_back({x, std::sin(wavePhase_ + x)});
            waveCosBuf_.push_back({x, std::cos(wavePhase_ + x)});
        }
        wavePlot_->setData(waveSin_, std::move(waveSinBuf_));
        wavePlot_->setData(waveCos_, std::move(waveCosBuf_));
    }

    std::shared_ptr<DxvUI::Label> makeLegendLabel(const std::string& id, const std::string& text,
                                                  int x, int y) {
        auto label = DxvUI::Label::create(id, text);
        label->setStyle({.textColor = DxvUI::Colors::DarkGray,
                         .fontSize = 14,
                         .left = static_cast<float>(x),
                         .top = static_cast<float>(y)},
                        DxvUI::WidgetState::Normal);
        return label;
    }

    void addLegend(const std::shared_ptr<DxvUI::SceneNode>& root, const std::string& name, int x,
                   int y, const std::string& suffix = " (ms)") {
        root->addChild(makeLegendLabel("legend_" + name + "_label", name + suffix, x + 28, y));
    }

    std::unique_ptr<DxvUI::SDLRenderer> dxvRenderer_;
    std::shared_ptr<DxvUI::Scene> scene_;
    DxvUI::SDLEventSource eventSource_;
    std::shared_ptr<DxvUI::Plot> plot_;
    size_t updateSeries_ = 0;
    size_t drawSeries_ = 0;
    uint64_t frameCount_ = 0;
    std::vector<float> updateMs_;
    std::vector<float> drawMs_;
    std::vector<DxvUI::Point<float>> pendingPoints_;

    std::shared_ptr<DxvUI::Plot> wavePlot_;
    size_t waveSin_ = 0;
    size_t waveCos_ = 0;
    float wavePhase_ = 0.0f;
    std::vector<DxvUI::Point<float>> waveSinBuf_;
    std::vector<DxvUI::Point<float>> waveCosBuf_;
};

}  // namespace

#ifdef _WIN32
extern "C" int SDL_main(int /*argc*/, char* /*argv*/[]) {
#else
int main(int /*argc*/, char* /*argv*/[]) {
#endif
    DxvUI::Log::init();
    DxvUI::Log::info("Logger Initialized.");

    DxvUIPlotExample app;
    return app.run();
}