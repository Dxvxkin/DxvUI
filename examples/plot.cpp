// examples/plot.cpp
//
// Live plotting demo: an oscilloscope-style window over the per-frame update and
// draw phase durations. Each phase is timed with a steady clock (same technique
// as FpsOverlay.h), appended with Plot::applyPoint-equivalent setData every
// frame, and the view slides left as the 512-point window fills (auto-scaling
// keeps the y-axis fitted to the visible samples).

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
#include <cstdlib>
#include <memory>
#include <string>
#include <vector>

#include "App.h"

namespace {

constexpr int SCREEN_WIDTH = 800;
constexpr int SCREEN_HEIGHT = 600;
constexpr size_t kWindow = 512;  // sliding window depth (points per series)

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
            "caption", "Время фазы кадра, ms: update (синий) / draw (оранжевый)");
        caption->setStyle({.textColor = DxvUI::Colors::DarkGray, .fontSize = 16},
                          DxvUI::WidgetState::Normal);
        root->addChild(caption);

        plot_ = DxvUI::Plot::create("plot");
        plot_->setStyle({.borderColor = DxvUI::Colors::Gray,
                         .borderThickness = 1,
                         .padding = DxvUI::Thickness{8, 8, 8, 8}},
                        DxvUI::WidgetState::Normal);
        updateSeries_ = plot_->addSeries("update");
        drawSeries_ = plot_->addSeries("draw");
        plot_->setSeriesColor(updateSeries_, DxvUI::Colors::CornflowerBlue);
        plot_->setSeriesColor(drawSeries_, DxvUI::Colors::Orange);
        root->addChild(plot_);

        addLegend(root, "update", 12, 56);
        addLegend(root, "draw", 130, 56);

        scene_->updateLayout();
        return true;
    }

    void update(float /*dtMs*/) override {
        const auto t0 = std::chrono::steady_clock::now();
        scene_->update();
        const auto t1 = std::chrono::steady_clock::now();
        pushSample(updateMs_, std::chrono::duration<double, std::milli>(t1 - t0).count());
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

    std::shared_ptr<DxvUI::Label> makeLegendLabel(const std::string& id, const std::string& text,
                                                  int x, int y) {
        auto label = DxvUI::Label::create(id, text);
        label->setStyle({.textColor = DxvUI::Colors::DarkGray,
                         .fontSize = 15,
                         .left = static_cast<float>(x),
                         .top = static_cast<float>(y)},
                        DxvUI::WidgetState::Normal);
        return label;
    }

    void addLegend(const std::shared_ptr<DxvUI::SceneNode>& root, const std::string& name, int x,
                   int y) {
        root->addChild(makeLegendLabel("legend_" + name + "_label", name + " (ms)", x + 28, y));
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