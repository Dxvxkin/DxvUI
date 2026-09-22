// examples/image.cpp
//
// Image widget demo. Renders two procedural textures (a checkerboard and a solid
// colour band) through the Image widget in several fit modes (Contain / Cover /
// Fill / None / ScaleDown), with an optional source-rect slice, tint and alpha.
// If the first command-line argument points to a PNG/JPG, it is loaded through
// ImageData::loadFromFile and replaces the checkerboard, so you can drop in your
// own texture. It subclasses DxvUIEx::SdlApp and owns the DxvUI integration
// itself (see examples/main.cpp for the pattern).

#include <DxvUI/Log.h>
#include <DxvUI/Scene.h>
#include <DxvUI/UIContext.h>
#include <DxvUI/backend/SDLEventSource.h>
#include <DxvUI/backend/SDLRenderer.h>
#include <DxvUI/core/ImageData.h>
#include <DxvUI/event/DxvEvent.h>
#include <DxvUI/interfaces/IRenderBackend.h>
#include <DxvUI/style/Colors.h>
#include <DxvUI/style/Style.h>
#include <DxvUI/widgets/Button.h>
#include <DxvUI/widgets/Image.h>
#include <DxvUI/widgets/Label.h>
#include <DxvUI/widgets/SliderHorizontal.h>
#include <SDL.h>

#include <format>
#include <memory>
#include <string>
#include <vector>

#include "App.h"
#include "FpsOverlay.h"

namespace {

constexpr int SCREEN_WIDTH = 860;
constexpr int SCREEN_HEIGHT = 620;

std::shared_ptr<DxvUI::Label> makeCaption(const std::string& id, const std::string& text, int x,
                                          int y) {
    auto label = DxvUI::Label::create(id, text);
    label->setStyle({.fontSize = 14, .left = static_cast<float>(x), .top = static_cast<float>(y)},
                    DxvUI::WidgetState::Normal);
    return label;
}

const char* fitToString(DxvUI::ImageFit fit) {
    switch (fit) {
        case DxvUI::ImageFit::None:
            return "None";
        case DxvUI::ImageFit::Contain:
            return "Contain";
        case DxvUI::ImageFit::Cover:
            return "Cover";
        case DxvUI::ImageFit::Fill:
            return "Fill";
        case DxvUI::ImageFit::ScaleDown:
            return "ScaleDown";
    }
    return "?";
}

}  // namespace

class DxvUIImageExample : public DxvUIEx::SdlApp {
   public:
    DxvUIImageExample() : DxvUIEx::SdlApp("DxvUI Image Example", SCREEN_WIDTH, SCREEN_HEIGHT) {}

   protected:
    bool init() override {
        dxvRenderer_ = std::make_unique<DxvUI::SDLRenderer>(renderer_);
        scene_ = DxvUI::Scene::create();
        scene_->setRenderBackend(dxvRenderer_.get());

        auto root = scene_->getRoot();
        root->setStyle({.textColor = DxvUI::Colors::DarkGray,
                        .fontSize = 16,
                        .fontFamily = "Sans",
                        .width = static_cast<float>(SCREEN_WIDTH),
                        .height = static_cast<float>(SCREEN_HEIGHT)},
                       DxvUI::WidgetState::Normal);

        buildImageDemoUI(root);
        scene_->updateLayout();
        return true;
    }

    void update(float dtMs) override {
        fpsOverlay_.beginUpdate();
        scene_->update();
        fpsOverlay_.endUpdate(dtMs);

        std::erase_if(connections_, [](const auto& c) { return c->expired(); });
    }

    void draw() override {
        fpsOverlay_.beginDraw();
        scene_->draw();
        fpsOverlay_.endDraw();
    }

    bool handleEvent(const SDL_Event& event) override {
        DxvUI::DxvEvent dxv;
        if (!eventSource_.processEvent(event, dxv)) return false;
        if (dxv.type == DxvUI::EventType::Quit) return true;
        scene_->processEvent(dxv);
        return false;
    }

   private:
    void buildImageDemoUI(const std::shared_ptr<DxvUI::SceneNode>& root) {
        fpsOverlay_.attach(root);

        // --- Procedural textures to display ---
        auto checkerboard =
            std::make_shared<DxvUI::ImageData>(DxvUI::ImageData::createCheckerboard(160, 90, 10));

        // Optional: load a real image from a file if one was provided.
        std::shared_ptr<DxvUI::ImageData> imageData = checkerboard;
        if (!filePath_.empty()) {
            auto loaded = DxvUI::ImageData::loadFromFile(filePath_);
            if (!loaded.isValid()) {
                DxvUI::Log::warn("Image example: falling back to checkerboard ({})",
                                 DxvUI::ImageData::lastFailureReason());
            } else {
                auto p = std::make_shared<DxvUI::ImageData>(std::move(loaded));
                imageData = std::move(p);
            }
        }

        // --- Header caption ---
        root->addChild(DxvUI::Label::create(
            "image_caption",
            std::format("Image widget (fit: Contain) — {}x{} {}", imageData->width,
                        imageData->height, filePath_.empty() ? "procedural" : "loaded from file")));

        // --- Main viewer: an Image in an explicit box, cycling fit modes ---
        auto viewer = DxvUI::Image::create("main_image");
        viewer->setImageData(*imageData);
        viewer->setStyle({.left = 60, .top = 60, .width = 360, .height = 360},
                         DxvUI::WidgetState::Normal);
        viewer->setFit(DxvUI::ImageFit::Contain);
        root->addChild(viewer);
        images_.push_back(viewer);

        // --- A row of small previews, one per fit mode ---
        constexpr float kThumb = 90.0f;
        constexpr float kThumbGap = 20.0f;
        float x = 60.0f;
        for (auto fit : {DxvUI::ImageFit::None, DxvUI::ImageFit::Contain, DxvUI::ImageFit::Cover,
                         DxvUI::ImageFit::Fill, DxvUI::ImageFit::ScaleDown}) {
            auto thumb = DxvUI::Image::create(std::format("thumb_{}", static_cast<int>(fit)));
            thumb->setImageData(*imageData);
            thumb->setFit(fit);
            thumb->setStyle({.left = x, .top = 460, .width = kThumb, .height = kThumb},
                            DxvUI::WidgetState::Normal);
            root->addChild(thumb);
            images_.push_back(thumb);

            x += kThumb + kThumbGap;
        }

        // --- Buttons to cycle fit / tint / alpha ---
        auto fitCaption = DxvUI::Label::create(
            "fit_caption", std::format("Click [Fit] to cycle: {}", fitToString(viewer->getFit())));
        fitCaption->setStyle({.left = 480, .top = 60}, DxvUI::WidgetState::Normal);
        connections_.push_back(viewer->on(
            DxvUI::EventType::Click,
            [this, viewer, fitCaption](DxvUI::DxvEvent&, const DxvUI::UIContext&) {
                auto next = static_cast<DxvUI::ImageFit>(
                    (static_cast<int>(viewer->getFit()) + 1) %
                    (static_cast<int>(DxvUI::ImageFit::ScaleDown) + 1));
                viewer->setFit(next);
                fitCaption->setText(std::format("Click [Fit] to cycle: {}", fitToString(next)));
            }));

        // --- Alpha slider ---
        auto alphaCaption = DxvUI::Label::create("alpha_caption", "Alpha");
        alphaCaption->setStyle({.left = 480, .top = 120}, DxvUI::WidgetState::Normal);
        root->addChild(alphaCaption);

        auto alphaSlider = DxvUI::SliderHorizontal::create("alpha_slider", 0.0f, 1.0f, 0.0f);
        alphaSlider->setStyle({.left = 480, .top = 140, .width = 240}, DxvUI::WidgetState::Normal);
        alphaSlider->setValue(1.0f);
        connections_.push_back(
            alphaSlider->on(DxvUI::EventType::Change,
                            [viewer, alphaSlider](DxvUI::DxvEvent&, const DxvUI::UIContext&) {
                                viewer->setAlpha(alphaSlider->getValue());
                            }));
        root->addChild(alphaSlider);

        root->addChild(makeCaption("note", "Pass a PNG/JPG path as the first argument to load it",
                                   60, 600 - 40));
    }

    std::unique_ptr<DxvUI::SDLRenderer> dxvRenderer_;
    std::shared_ptr<DxvUI::Scene> scene_;
    DxvUI::SDLEventSource eventSource_;
    std::vector<std::shared_ptr<DxvUI::Image>> images_;
    std::vector<std::unique_ptr<DxvUI::SceneNode::Connection>> connections_;
    DxvUIEx::FpsOverlay fpsOverlay_;

   protected:
    std::string filePath_;  // set from argv initializer in the driver
};

// argv[1] support: the driver passes the optional file path via the ctor.
class DxvUIImageExampleApp : public DxvUIImageExample {
   public:
    explicit DxvUIImageExampleApp(const std::string& path = "") {
        DxvUIImageExample::filePath_ = path;
    }
};

#ifdef _WIN32
extern "C" int SDL_main(int /*argc*/, char* /*argv*/[]) {
#else
int main(int argc, char* argv[]) {
#endif
    DxvUI::Log::init();
    DxvUI::Log::info("Image Example Started.");

    const std::string filePath = (argc > 1) ? argv[1] : "";
    DxvUIImageExampleApp app(filePath);
    return app.run();
}
