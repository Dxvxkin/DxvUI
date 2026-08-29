// examples/main.cpp
//
// Full DxvUI showcase: buttons, checkboxes, text editing, containers, alignment
// and live node management. It subclasses DxvUIEx::SdlApp (a plain SDL scaffold
// that owns the window, renderer and main loop) and owns the DxvUI integration
// itself, like a real host application would.

#include <DxvUI/DxvUI.h>
#include <DxvUI/FpsCounter.h>
#include <DxvUI/Log.h>
#include <DxvUI/style/Colors.h>
#include <SDL.h>

#include <chrono>
#include <cstdlib>
#include <format>
#include <memory>
#include <vector>

#include "App.h"
#include "DxvUI/containers/HorizontalContainer.h"

namespace {

constexpr int SCREEN_WIDTH = 800;
constexpr int SCREEN_HEIGHT = 600;

}  // namespace

class DxvUIExample : public DxvUIEx::SdlApp {
   public:
    DxvUIExample() : DxvUIEx::SdlApp("DxvUI Example", SCREEN_WIDTH, SCREEN_HEIGHT) {}

   protected:
    bool init() override {
        dxvRenderer_ = std::make_unique<DxvUI::SDLRenderer>(renderer_);
        scene_ = DxvUI::Scene::create();
        scene_->setRenderer(dxvRenderer_.get());

        buildUI(scene_->getRoot());
        scene_->updateLayout();

        DxvUI::Log::info("Initial node count: {}", DxvUI::SceneNode::getNodeCount());
        return true;
    }

    void update(float /*dtMs*/) override {
        const auto tUpdate = std::chrono::steady_clock::now();
        scene_->update();
        updateMs_.recordMs(
            std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - tUpdate)
                .count());

        fps_.tick();
        fpsLabel_->setText(std::format("FPS: {:.0f} (up {:.2f} ms · draw {:.2f} ms)", fps_.getFps(),
                                       updateMs_.getFrameTimeMs(), drawMs_.getFrameTimeMs()));

        std::erase_if(connections_, [](const auto& c) { return c->expired(); });
    }

    void draw() override {
        const auto tDraw = std::chrono::steady_clock::now();
        scene_->draw();
        drawMs_.recordMs(
            std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - tDraw)
                .count());
    }

    bool handleEvent(const SDL_Event& event) override {
        DxvUI::DxvEvent dxv;
        if (!eventSource_.processEvent(event, dxv)) return false;
        if (dxv.type == DxvUI::EventType::Quit) return true;
        scene_->processEvent(dxv);
        return false;
    }

   private:
    void buildUI(const std::shared_ptr<DxvUI::SceneNode>& root) {
        // Set global font properties on the root node. These will be inherited by
        // children.
        root->setStyle({.textColor = DxvUI::Colors::DarkGray,
                        .fontSize = 18,
                        .fontFamily = "Sans",
                        .width = SCREEN_WIDTH,
                        .height = SCREEN_HEIGHT},
                       DxvUI::WidgetState::Normal);

        // Frame-rate readout pinned to the top-right corner; the text is refreshed
        // by the main loop, and setText() is a no-op while the value stays stable.
        auto fpsLabel = DxvUI::Label::create("fps_label", "FPS: --");
        fpsLabel->setStyle({.top = 10, .right = 10}, DxvUI::WidgetState::Normal);
        root->addChild(fpsLabel);
        fpsLabel_ = fpsLabel;

        // --- Button 1: Uses default styles + overrides ---
        auto myButton = DxvUI::Button::create("my_button", "Click Me!");

        connections_.push_back(myButton->on(
            DxvUI::EventType::Change, [](DxvUI::DxvEvent& event, const DxvUI::UIContext&) {
                DxvUI::Log::info("Button callback");
                DxvUI::Log::info("{} ::onChange({}) ", event.getTargetId(),
                                 event.getTarget()->getBinding()->getString());
            }));

        connections_.push_back(myButton->on(
            DxvUI::EventType::Attach, [](DxvUI::DxvEvent& event, const DxvUI::UIContext&) {
                if (event.getTarget() == event.getCurrentTarget()) {
                    DxvUI::Log::info("{} Attach", event.getTargetId());
                }
            }));

        // Override only position and size. Colors, padding, etc., will come from the
        // Button's default theme.
        myButton->setStyle({.left = 50, .top = 50, .width = 200, .height = 50},
                           DxvUI::WidgetState::Normal);

        // We can still override state-specific styles if needed.
        // The base for this will be the Button's default Hovered style.
        myButton->setStyle({.borderColor = DxvUI::Colors::White, .borderThickness = 2},
                           DxvUI::WidgetState::Hovered);

        connections_.push_back(myButton->on(
            DxvUI::EventType::Click, [this](DxvUI::DxvEvent& event, const DxvUI::UIContext& ui) {
                const auto viewport = ui.getViewport();
                int randomX = rand() % static_cast<int>(viewport.width - 200);
                int randomY = rand() % static_cast<int>(viewport.height - 50);

                static size_t label_count = 0;
                auto label = DxvUI::Label::create(std::format("label_{}", label_count++),
                                                  std::format("Click to remove {}", label_count));

                // Set style for the new label
                label->setStyle(
                    {
                        .backgroundColor = DxvUI::Color(0, 0, 0, 80),
                        .textColor = DxvUI::Colors::White,
                        .borderColor = DxvUI::Colors::Black,
                        .borderThickness = 1,
                        .borderRadius = 5,
                        .left = (float)randomX,
                        .top = (float)randomY,
                        .padding = DxvUI::Thickness{5, 5, 5, 5},
                    },
                    DxvUI::WidgetState::Normal);
                label->setStyle({.backgroundColor = (DxvUI::Color(0, 0, 0, 150)),
                                 .borderThickness = 1,
                                 .borderRadius = 10},
                                DxvUI::WidgetState::Hovered);

                connections_.push_back(label->on(
                    DxvUI::EventType::Click, [](DxvUI::DxvEvent& event, const DxvUI::UIContext&) {
                        if (auto target = event.getTarget()) {
                            target->detach();
                        }
                    }));
                // Tokens of labels that were clicked away (and destroyed) are dead; drop
                // them so the connection list stays bounded.
                std::erase_if(connections_, [](const auto& c) { return c->expired(); });

                ui.getRoot()->addChild(label);

                if (auto btn = event.getTarget()->as<DxvUI::Button>())
                    btn->setText(std::format("Count {}", DxvUI::SceneNode::getNodeCount()));
            }));

        root->addChild(myButton);

        // --- Button 2: Uses default styles + different overrides ---
        auto btn2 = DxvUI::Button::create("find_btn", "Find");
        // Override position, size, and some colors.
        btn2->setStyle({.backgroundColor = DxvUI::Colors::DarkOrange,
                        .textColor = DxvUI::Colors::MidnightBlue,
                        .left = 300,
                        .top = 50,
                        .width = 200,
                        .height = 50},
                       DxvUI::WidgetState::Normal);

        connections_.push_back(
            btn2->on(DxvUI::EventType::Click, [](DxvUI::DxvEvent&, const DxvUI::UIContext& ui) {
                if (auto label = ui.findNodeById("label_8")) {
                    label->as<DxvUI::Label>()->setText("Found!");
                }
            }));

        root->addChild(btn2);

        // --- Button 3: Test default styles ---
        auto btn_test_def_styles = DxvUI::Button::create("btn_defStyle", "test");
        btn_test_def_styles->setStyle({.left = 500, .top = 500, .width = 100, .height = 50},
                                      DxvUI::WidgetState::Normal);
        connections_.push_back(btn_test_def_styles->on(
            DxvUI::EventType::Click, [](DxvUI::DxvEvent&, const DxvUI::UIContext& ui) {
                if (auto target = ui.findNodeById("label_7")) {
                    auto style =
                        target->as<DxvUI::Label>()->getStyle().get(DxvUI::WidgetState::Normal);
                    auto newStyle = DxvUI::StyleRule(*style);
                    newStyle.padding->left += 5;
                    newStyle.padding->top += 5;
                    newStyle.padding->right += 5;
                    newStyle.padding->bottom += 5;
                    target->setStyle(newStyle, DxvUI::WidgetState::Normal);
                }
            }));
        root->addChild(btn_test_def_styles);

        // --- Checkbox ---
        auto cb1 = DxvUI::Checkbox::create("cb_1", "Вариант 1");
        cb1->setStyle({.left = 50, .top = 120}, DxvUI::WidgetState::Normal);
        connections_.push_back(
            cb1->on(DxvUI::EventType::Change, [](DxvUI::DxvEvent& event, const DxvUI::UIContext&) {
                auto cb = event.getTarget()->as<DxvUI::Checkbox>();
                DxvUI::Log::info("[cb_1] checked={}", cb ? cb->isChecked() : false);
            }));
        root->addChild(cb1);

        auto cb2 = DxvUI::Checkbox::create("cb_2", "Вариант 2 (включён по умолчанию)");
        cb2->setChecked(true);
        cb2->setStyle({.left = 50, .top = 165}, DxvUI::WidgetState::Normal);
        connections_.push_back(
            cb2->on(DxvUI::EventType::Change, [](DxvUI::DxvEvent& event, const DxvUI::UIContext&) {
                auto cb = event.getTarget()->as<DxvUI::Checkbox>();
                DxvUI::Log::info("[cb_2] checked={}", cb ? cb->isChecked() : false);
            }));
        root->addChild(cb2);

        buildDisabledDemo(root);
        buildTextAlignDemo(root);

        buildContainers(root);
    }

    void buildDisabledDemo(const std::shared_ptr<DxvUI::SceneNode>& root) {
        auto disabledBtn = DxvUI::Button::create("disabled_btn", "Disabled button");
        disabledBtn->setStyle({.left = 50, .top = 280, .width = 200, .height = 50},
                              DxvUI::WidgetState::Normal);
        disabledBtn->setEnabled(false);
        root->addChild(disabledBtn);

        auto disabledCb = DxvUI::Checkbox::create("disabled_cb", "Disabled checkbox");
        disabledCb->setStyle({.left = 50, .top = 345}, DxvUI::WidgetState::Normal);
        disabledCb->setEnabled(false);
        root->addChild(disabledCb);

        auto disabledTe = DxvUI::TextEdit::create("disabled_te", "disabled text");
        disabledTe->setStyle({.left = 50, .top = 395, .width = 200, .height = 30},
                             DxvUI::WidgetState::Normal);
        disabledTe->setEnabled(false);
        root->addChild(disabledTe);

        auto toggle = DxvUI::Button::create("toggle_disabled", "Enable widgets");
        toggle->setStyle({.left = 300, .top = 280, .width = 180, .height = 50},
                         DxvUI::WidgetState::Normal);
        connections_.push_back(
            toggle->on(DxvUI::EventType::Click, [disabledBtn, disabledCb, disabledTe, toggle](
                                                    DxvUI::DxvEvent&, const DxvUI::UIContext&) {
                const bool enable = !disabledBtn->isEnabled();
                disabledBtn->setEnabled(enable);
                disabledCb->setEnabled(enable);
                disabledTe->setEnabled(enable);
                toggle->setText(enable ? "Disable widgets" : "Enable widgets");
            }));
        root->addChild(toggle);
    }

    void buildTextAlignDemo(const std::shared_ptr<DxvUI::SceneNode>& root) {
        constexpr float kX = 520.0f;
        constexpr float kBoxWidth = 240.0f;
        constexpr float kBoxHeight = 40.0f;

        auto captionHa = DxvUI::Label::create("caption_ha", "textAlign (Start | Center | End)");
        captionHa->setStyle(
            {.textColor = DxvUI::Colors::Gray, .fontSize = 14, .left = kX, .top = 120},
            DxvUI::WidgetState::Normal);
        root->addChild(captionHa);

        auto boxStyle = [kX, kBoxWidth, kBoxHeight](const char* id, const char* text, float top,
                                                    DxvUI::Alignment align) {
            auto label = DxvUI::Label::create(id, text);
            label->setStyle({.backgroundColor = DxvUI::Color(0, 0, 0, 10),
                             .borderColor = DxvUI::Colors::Gray,
                             .borderThickness = 1,
                             .textAlign = align,
                             .left = kX,
                             .top = top,
                             .width = kBoxWidth,
                             .height = kBoxHeight,
                             .padding = DxvUI::Thickness{0, 0, 0, 0}},
                            DxvUI::WidgetState::Normal);
            return label;
        };

        root->addChild(boxStyle("align_ha_start", "Start", 150, DxvUI::Alignment::Start));
        root->addChild(boxStyle("align_ha_center", "Center", 195, DxvUI::Alignment::Center));
        root->addChild(boxStyle("align_ha_end", "End", 240, DxvUI::Alignment::End));

        auto captionVa =
            DxvUI::Label::create("caption_va", "textAlignVertical (Start | Center | End)");
        captionVa->setStyle(
            {.textColor = DxvUI::Colors::Gray, .fontSize = 14, .left = kX, .top = 310},
            DxvUI::WidgetState::Normal);
        root->addChild(captionVa);

        auto boxStyleV = [kX, kBoxWidth](const char* id, const char* text, float top,
                                         DxvUI::Alignment align) {
            auto label = DxvUI::Label::create(id, text);
            label->setStyle({.backgroundColor = DxvUI::Color(0, 0, 0, 10),
                             .borderColor = DxvUI::Colors::Gray,
                             .borderThickness = 1,
                             .textAlign = DxvUI::Alignment::Center,
                             .textAlignVertical = align,
                             .left = kX,
                             .top = top,
                             .width = kBoxWidth,
                             .height = 55,
                             .padding = DxvUI::Thickness{0, 0, 0, 0}},
                            DxvUI::WidgetState::Normal);
            return label;
        };

        root->addChild(boxStyleV("align_va_start", "Start", 340, DxvUI::Alignment::Start));
        root->addChild(boxStyleV("align_va_center", "Center", 400, DxvUI::Alignment::Center));
        root->addChild(boxStyleV("align_va_end", "End", 460, DxvUI::Alignment::End));
    }

    void buildContainers(const std::shared_ptr<DxvUI::SceneNode>& root) {
        auto h_container = std::make_shared<DxvUI::HorizontalContainer>("container_horizontal");
        h_container->setSpacing(30);
        h_container->setStyle({.borderColor = DxvUI::Colors::Red,
                               .borderThickness = 1,
                               .left = 0,
                               .top = 0,
                               .width = SCREEN_WIDTH,
                               .padding = DxvUI::Thickness(10, 50, 10, 50)},
                              DxvUI::WidgetState::Normal);

        auto btn = DxvUI::Button::create("h_btn1", "Хуй");

        h_container->addChild(btn);
        connections_.push_back(
            btn->on(DxvUI::EventType::Click, [](DxvUI::DxvEvent& event, const DxvUI::UIContext&) {
                if (auto target = event.getTarget()) {
                    if (auto p = target->getParent().lock()) {
                        p->updateStyle({.borderColor = DxvUI::Colors::Green});
                    }
                }
            }));

        h_container->addChild(DxvUI::Button::create("h_btn2", "h_btn2"));
        h_container->addChild(DxvUI::Button::create("h_btn3", "h_btn3"));

        root->addChild(h_container);
    }

    std::unique_ptr<DxvUI::SDLRenderer> dxvRenderer_;
    std::shared_ptr<DxvUI::Scene> scene_;
    DxvUI::SDLEventSource eventSource_;
    std::vector<std::unique_ptr<DxvUI::SceneNode::Connection>> connections_;
    DxvUI::FpsCounter<> fps_;
    DxvUI::FpsCounter<> updateMs_;
    DxvUI::FpsCounter<> drawMs_;
    std::shared_ptr<DxvUI::Label> fpsLabel_;
};

#ifdef _WIN32
extern "C" int SDL_main(int /*argc*/, char* /*argv*/[]) {
#else
int main(int /*argc*/, char* /*argv*/[]) {
#endif
    DxvUI::Log::init();
    DxvUI::Log::info("Logger Initialized.");

    DxvUIExample app;
    const int result = app.run();

    DxvUI::Log::info("Final node count: {}", DxvUI::SceneNode::getNodeCount());
    return result;
}
