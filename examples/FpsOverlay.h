// examples/FpsOverlay.h
//
// A small FPS + frame-phase overlay shared by DxvUI examples (part of the
// examples, NOT installed with the library, like App.h). It owns the counters
// and the on-screen label, ticks the frame rate, measures the update/draw
// phases and throttles the label refresh so the HUD text is not re-rasterized
// on every frame.
//
// Usage (hook style — pair the begin/end calls around the measured work):
//
//   FpsOverlay fps;
//   fps.attach(root);
//   ...
//   void update(float dtMs) override {
//       fps.beginUpdate();
//       scene_->update();
//       fps.endUpdate(dtMs);
//   }
//   void draw() override {
//       fps.beginDraw();
//       scene_->draw();
//       fps.endDraw();
//   }

#pragma once

#include <DxvUI/FpsCounter.h>
#include <DxvUI/SceneNode.h>
#include <DxvUI/widgets/Label.h>

#include <chrono>
#include <format>
#include <memory>

namespace DxvUIEx {

// Default minimum interval between label refreshes, ms. setText() on a changing
// string re-rasterizes the text (0.4–0.8 ms of jitter under idle), so throttling
// the readout keeps the hot path flat.
constexpr float kFpsOverlayIntervalMs = 250.0f;

class FpsOverlay {
   public:
    // Creates and pins the readout label to the top-right corner of the scene.
    // Call once from init()/buildUI() after the root exists.
    void attach(const std::shared_ptr<DxvUI::SceneNode>& root, int top = 10, int right = 10) {
        label_ = DxvUI::Label::create("fps_label", "FPS: --");
        label_->setStyle({.top = top, .right = right}, DxvUI::WidgetState::Normal);
        root->addChild(label_);
    }

    // Starts the update phase: records one frame boundary and starts the update
    // timer. Must be paired with endUpdate() in the same frame.
    void beginUpdate() {
        fps_.tick();
        updateStart_ = std::chrono::steady_clock::now();
    }

    // Ends the update phase, records its duration and (throttled) refreshes the
    // label. dtMs is the wall time of the whole frame, passed in by SdlApp.
    void endUpdate(float dtMs) {
        updateMs_.recordMs(std::chrono::duration<double, std::milli>(
                               std::chrono::steady_clock::now() - updateStart_)
                               .count());

        accumMs_ += dtMs;
        if (accumMs_ >= kFpsOverlayIntervalMs) {
            accumMs_ = 0.0f;
            label_->setText(std::format("FPS: {:.0f} (up {:.2f} ms · draw {:.2f} ms)",
                                        fps_.getFps(), updateMs_.getFrameTimeMs(),
                                        drawMs_.getFrameTimeMs()));
        }
    }

    // Starts the draw phase. Must be paired with endDraw() in the same frame.
    void beginDraw() { drawStart_ = std::chrono::steady_clock::now(); }

    // Ends the draw phase and records its duration.
    void endDraw() {
        drawMs_.recordMs(
            std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - drawStart_)
                .count());
    }

   private:
    DxvUI::FpsCounter<> fps_;
    DxvUI::FpsCounter<> updateMs_;
    DxvUI::FpsCounter<> drawMs_;
    std::shared_ptr<DxvUI::Label> label_;
    float accumMs_ = 0.0f;
    std::chrono::steady_clock::time_point updateStart_;
    std::chrono::steady_clock::time_point drawStart_;
};

}  // namespace DxvUIEx