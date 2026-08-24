#include <DxvUI/DxvEvent.h>
#include <DxvUI/FpsCounter.h>
#include <DxvUI/Log.h>
#include <DxvUI/Scene.h>
#include <DxvUI/UIContext.h>
#include <DxvUI/containers/AbsoluteContainer.h>
#include <DxvUI/containers/HorizontalContainer.h>
#include <DxvUI/core.h>
#include <DxvUI/renderers/SDLRenderer.h>
#include <DxvUI/sources/SDLEventSource.h>
#include <DxvUI/style/Colors.h>
#include <DxvUI/style/Style.h>
#include <DxvUI/widgets/Button.h>
#include <DxvUI/widgets/Label.h>
#include <SDL.h>

#include <format>
#include <memory>
#include <string>
#include <vector>

constexpr int SCREEN_WIDTH = 800;
constexpr int SCREEN_HEIGHT = 800;

namespace {

std::shared_ptr<DxvUI::Label> makeCaption(const std::string& id, const std::string& text, int x,
                                          int y) {
    auto label = DxvUI::Label::create(id, text);
    label->setStyle({.fontSize = 14, .left = static_cast<float>(x), .top = static_cast<float>(y)},
                    DxvUI::WidgetState::Normal);
    return label;
}

// A button used as a plain colored box (explicit size, no padding).
std::shared_ptr<DxvUI::Button> makeBox(const std::string& id, const std::string& text, int width,
                                       int height, DxvUI::Color background) {
    auto button = DxvUI::Button::create(id, text);
    button->setStyle({.backgroundColor = background,
                      .width = static_cast<float>(width),
                      .height = static_cast<float>(height),
                      .padding = DxvUI::Thickness{0, 0, 0, 0}},
                     DxvUI::WidgetState::Normal);
    return button;
}

struct AlignmentDemoNodes {
    std::shared_ptr<DxvUI::Button> rowStart;
    std::shared_ptr<DxvUI::Button> rowCenter;
    std::shared_ptr<DxvUI::Button> rowEnd;
    std::shared_ptr<DxvUI::Button> anchoredChild;
};

const char* alignmentToString(DxvUI::Alignment alignment) {
    switch (alignment) {
        case DxvUI::Alignment::Start:
            return "Start";
        case DxvUI::Alignment::Center:
            return "Center";
        case DxvUI::Alignment::End:
            return "End";
        case DxvUI::Alignment::Stretch:
            return "Stretch";
    }
    return "?";
}

void logNodeInfo(const std::string& prefix, const std::shared_ptr<DxvUI::SceneNode>& node) {
    const DxvUI::Rect bounds = node->getGlobalBounds();
    const auto& layout = node->getComputedLayout(node->getCurrentState());
    DxvUI::Log::info("{} id={} bounds=({}, {}, {}, {}) state={} hAlign={} vAlign={}", prefix,
                     node->getId(), bounds.x, bounds.y, bounds.width, bounds.height,
                     DxvUI::state_to_string(node->getCurrentState()),
                     alignmentToString(layout.horizontalAlignment),
                     alignmentToString(layout.verticalAlignment));
}

void logAnchoredInfo(const std::shared_ptr<DxvUI::Button>& button) {
    const DxvUI::Rect buttonBounds = button->getGlobalBounds();
    const auto& children = button->getChildren();
    if (children.empty() || children.front()->getChildren().empty()) {
        DxvUI::Log::warn("anchor_child: internal label is not built yet");
        return;
    }
    const auto& label = children.front()->getChildren().front();
    const DxvUI::Rect labelBounds = label->getGlobalBounds();
    DxvUI::Log::info("anchor_child bounds=({}, {}, {}, {}) label bounds=({}, {}, {}, {})",
                     buttonBounds.x, buttonBounds.y, buttonBounds.width, buttonBounds.height,
                     labelBounds.x, labelBounds.y, labelBounds.width, labelBounds.height);
}

AlignmentDemoNodes buildAlignmentDemoUI(
    const std::shared_ptr<DxvUI::Scene>& scene,
    std::vector<std::unique_ptr<DxvUI::SceneNode::Connection>>& connections) {
    auto root = scene->getRoot();

    root->setStyle({.textColor = DxvUI::Colors::DarkGray,
                    .fontSize = 16,
                    .fontFamily = "Sans",
                    .width = SCREEN_WIDTH,
                    .height = SCREEN_HEIGHT},
                   DxvUI::WidgetState::Normal);

    // Frame-rate readout pinned to the top-right corner; the text is refreshed
    // by the main loop, and setText() is a no-op while the value stays stable.
    auto fpsLabel = DxvUI::Label::create("fps_label", "FPS: --");
    fpsLabel->setStyle({.top = 10, .right = 10}, DxvUI::WidgetState::Normal);
    root->addChild(fpsLabel);

    // --- 1. HorizontalContainer: each child's verticalAlignment positions it
    // within the row's content height. Click a button to cycle Start/Center/End.
    root->addChild(makeCaption(
        "row_caption",
        "HorizontalContainer: verticalAlignment Start / Center / End (click to cycle)", 40, 30));

    auto row = std::make_shared<DxvUI::HorizontalContainer>("align_row");
    row->setSpacing(10);
    row->setStyle({.borderColor = DxvUI::Colors::Red,
                   .borderThickness = 1,
                   .left = 40,
                   .top = 60,
                   .width = 700,
                   .height = 120,
                   .padding = DxvUI::Thickness{8, 8, 8, 8}},
                  DxvUI::WidgetState::Normal);

    auto rowStart = makeBox("row_start", "Start", 150, 40, DxvUI::Colors::CornflowerBlue);
    auto rowCenter = makeBox("row_center", "Center", 150, 40, DxvUI::Colors::SeaGreen);
    auto rowEnd = makeBox("row_end", "End", 150, 40, DxvUI::Colors::Orange);
    rowCenter->updateStyle({.verticalAlignment = DxvUI::Alignment::Center},
                           DxvUI::WidgetState::Normal);
    rowEnd->updateStyle({.verticalAlignment = DxvUI::Alignment::End}, DxvUI::WidgetState::Normal);

    const auto cycleVerticalAlignment = [](DxvUI::DxvEvent& event, const DxvUI::UIContext& ui) {
        if (auto target = event.getTarget()) {
            const auto& layout = target->getComputedLayout(DxvUI::WidgetState::Normal);
            const auto oldAlignment = layout.verticalAlignment;
            DxvUI::StyleRule rule;
            switch (oldAlignment) {
                case DxvUI::Alignment::Start:
                    rule.verticalAlignment = DxvUI::Alignment::Center;
                    break;
                case DxvUI::Alignment::Center:
                    rule.verticalAlignment = DxvUI::Alignment::End;
                    break;
                default:
                    rule.verticalAlignment = DxvUI::Alignment::Start;
                    break;
            }
            target->updateStyle(rule, DxvUI::WidgetState::Normal);
            ui.updateLayout();
            const DxvUI::Rect bounds = target->getGlobalBounds();
            DxvUI::Log::info("{} verticalAlignment {} -> {} bounds=({}, {}, {}, {})",
                             target->getId(), alignmentToString(oldAlignment),
                             alignmentToString(rule.verticalAlignment.value()), bounds.x, bounds.y,
                             bounds.width, bounds.height);
        }
    };
    connections.push_back(rowStart->on(DxvUI::EventType::Click, cycleVerticalAlignment));
    connections.push_back(rowCenter->on(DxvUI::EventType::Click, cycleVerticalAlignment));
    connections.push_back(rowEnd->on(DxvUI::EventType::Click, cycleVerticalAlignment));

    const auto cycleVerticalAlignmentWithStretch =
        [](DxvUI::DxvEvent& event, const DxvUI::UIContext& ui) {
            if (auto target = event.getTarget()) {
                const auto& layout = target->getComputedLayout(DxvUI::WidgetState::Normal);
                const auto oldAlignment = layout.verticalAlignment;
                DxvUI::StyleRule rule;
                switch (oldAlignment) {
                    case DxvUI::Alignment::Start:
                        rule.verticalAlignment = DxvUI::Alignment::Center;
                        break;
                    case DxvUI::Alignment::Center:
                        rule.verticalAlignment = DxvUI::Alignment::End;
                        break;
                    case DxvUI::Alignment::End:
                        rule.verticalAlignment = DxvUI::Alignment::Stretch;
                        break;
                    default:
                        rule.verticalAlignment = DxvUI::Alignment::Start;
                        break;
                }
                target->updateStyle(rule, DxvUI::WidgetState::Normal);
                ui.updateLayout();
                const DxvUI::Rect bounds = target->getGlobalBounds();
                DxvUI::Log::info("{} verticalAlignment {} -> {} bounds=({}, {}, {}, {})",
                                 target->getId(), alignmentToString(oldAlignment),
                                 alignmentToString(rule.verticalAlignment.value()), bounds.x,
                                 bounds.y, bounds.width, bounds.height);
            }
        };

    row->addChild(rowStart);
    row->addChild(rowCenter);
    row->addChild(rowEnd);
    root->addChild(row);

    // --- 2. AbsoluteContainer without anchors: both axes are free, so the
    // child's horizontal/verticalAlignment places it inside the container.
    root->addChild(makeCaption(
        "abs_caption", "AbsoluteContainer without anchors: Start/Start, Center/Center, End/End", 40,
        210));

    auto abs1 = std::make_shared<DxvUI::AbsoluteContainer>("abs1");
    abs1->setStyle({.borderColor = DxvUI::Colors::Blue,
                    .borderThickness = 1,
                    .left = 40,
                    .top = 245,
                    .width = 200,
                    .height = 120},
                   DxvUI::WidgetState::Normal);
    abs1->updateStyle({.backgroundColor = DxvUI::Colors::Purple});
    auto abs1Child = makeBox("abs1_child", "Start", 60, 30, DxvUI::Colors::SkyBlue);
    abs1->addChild(abs1Child);
    root->addChild(abs1);

    auto abs2 = std::make_shared<DxvUI::AbsoluteContainer>("abs2");
    abs2->setStyle({.borderColor = DxvUI::Colors::Blue,
                    .borderThickness = 1,
                    .left = 270,
                    .top = 245,
                    .width = 200,
                    .height = 120},
                   DxvUI::WidgetState::Normal);
    auto abs2Child = makeBox("abs2_child", "Center", 60, 30, DxvUI::Colors::SkyBlue);
    abs2Child->updateStyle({.horizontalAlignment = DxvUI::Alignment::Center,
                            .verticalAlignment = DxvUI::Alignment::Center},
                           DxvUI::WidgetState::Normal);
    abs2->addChild(abs2Child);
    root->addChild(abs2);

    auto abs3 = std::make_shared<DxvUI::AbsoluteContainer>("abs3");
    abs3->setStyle({.borderColor = DxvUI::Colors::Blue,
                    .borderThickness = 1,
                    .left = 500,
                    .top = 245,
                    .width = 200,
                    .height = 120},
                   DxvUI::WidgetState::Normal);
    auto abs3Child = makeBox("abs3_child", "End", 60, 30, DxvUI::Colors::SkyBlue);
    abs3Child->updateStyle(
        {.horizontalAlignment = DxvUI::Alignment::End, .verticalAlignment = DxvUI::Alignment::End},
        DxvUI::WidgetState::Normal);
    abs3->addChild(abs3Child);
    root->addChild(abs3);

    // --- 3. AbsoluteContainer: an anchored axis keeps its position (the anchor
    // wins), the free axis is still aligned by the child's alignment.
    root->addChild(makeCaption(
        "anchor_caption",
        "AbsoluteContainer: left anchor wins horizontally, free vertical axis is centered", 40,
        385));

    auto anchored = std::make_shared<DxvUI::AbsoluteContainer>("anchor_box");
    anchored->setStyle({.borderColor = DxvUI::Colors::Green,
                        .borderThickness = 1,
                        .left = 40,
                        .top = 415,
                        .width = 200,
                        .height = 120},
                       DxvUI::WidgetState::Normal);
    auto anchoredChild = makeBox("anchor_child", "anchored", 90, 30, DxvUI::Colors::Gold);
    anchoredChild->updateStyle({.left = 10, .verticalAlignment = DxvUI::Alignment::Center},
                               DxvUI::WidgetState::Normal);
    anchored->addChild(anchoredChild);
    root->addChild(anchored);

    // --- 4. AbsoluteContainer: Stretch children fill their slot. ---
    root->addChild(makeCaption(
        "stretch_caption",
        "AbsoluteContainer: Stretch children fill their slot (click to cycle)", 40, 550));

    auto abs4 = std::make_shared<DxvUI::AbsoluteContainer>("abs4");
    abs4->setStyle({.backgroundColor = DxvUI::Colors::LightGray,
                    .borderColor = DxvUI::Colors::Green,
                    .borderThickness = 1,
                    .left = 40,
                    .top = 580,
                    .width = 700,
                    .height = 120},
                   DxvUI::WidgetState::Normal);

    auto stretchBoth =
        makeBox("stretch_both", "Stretch/Stretch", 60, 30, DxvUI::Colors::CornflowerBlue);
    stretchBoth->updateStyle({.horizontalAlignment = DxvUI::Alignment::Stretch,
                              .verticalAlignment = DxvUI::Alignment::Stretch},
                             DxvUI::WidgetState::Normal);
    abs4->addChild(stretchBoth);

    auto stretchHCenter =
        makeBox("stretch_h_center", "Stretch/Center", 60, 30, DxvUI::Colors::SeaGreen);
    stretchHCenter->updateStyle({.horizontalAlignment = DxvUI::Alignment::Stretch,
                                 .verticalAlignment = DxvUI::Alignment::Center},
                                DxvUI::WidgetState::Normal);
    abs4->addChild(stretchHCenter);

    auto stretchHEnd =
        makeBox("stretch_h_end", "Stretch/End", 60, 30, DxvUI::Colors::Orange);
    stretchHEnd->updateStyle({.horizontalAlignment = DxvUI::Alignment::Stretch,
                              .verticalAlignment = DxvUI::Alignment::End},
                             DxvUI::WidgetState::Normal);
    abs4->addChild(stretchHEnd);

    connections.push_back(
        stretchBoth->on(DxvUI::EventType::Click, cycleVerticalAlignmentWithStretch));
    connections.push_back(
        stretchHCenter->on(DxvUI::EventType::Click, cycleVerticalAlignmentWithStretch));
    connections.push_back(
        stretchHEnd->on(DxvUI::EventType::Click, cycleVerticalAlignmentWithStretch));

    root->addChild(abs4);

    return {.rowStart = rowStart,
            .rowCenter = rowCenter,
            .rowEnd = rowEnd,
            .anchoredChild = anchoredChild};
}

}  // namespace

extern "C" int SDL_main(int /*argc*/, char* /*argv*/[]) {
    DxvUI::Log::init();
    DxvUI::Log::info("Logger Initialized.");

    DxvUI::SDLRenderer dxv_renderer_impl("DxvUI Alignment Example", SCREEN_WIDTH, SCREEN_HEIGHT);
    DxvUI::IRenderer& dxv_renderer = dxv_renderer_impl;
    DxvUI::SDLEventSource eventSource;
    auto scene = DxvUI::Scene::create();
    scene->setRenderer(&dxv_renderer);

    // Keeps the event handler connections alive for the whole app lifetime;
    // destroying one of these tokens would unsubscribe its handler.
    std::vector<std::unique_ptr<DxvUI::SceneNode::Connection>> connections;

    const auto demo = buildAlignmentDemoUI(scene, connections);
    scene->updateLayout();

    logNodeInfo("row", demo.rowStart);
    logNodeInfo("row", demo.rowCenter);
    logNodeInfo("row", demo.rowEnd);
    logAnchoredInfo(demo.anchoredChild);

    bool quit = false;
    SDL_Event sdl_event;

    DxvUI::FpsCounter fps;
    auto fpsLabel = scene->findNodeById("fps_label")->as<DxvUI::Label>();

    // Optional headless run for profiling: when DXVUI_FRAMES is set, the loop
    // exits after that many frames (no interaction needed). Not set = interactive.
    const char* frames_env = std::getenv("DXVUI_FRAMES");
    const long frame_cap = frames_env ? std::atol(frames_env) : 0;
    long frame_count = 0;

    while (!quit) {
        while (SDL_PollEvent(&sdl_event) != 0) {
            DxvUI::DxvEvent dxv_event;
            if (eventSource.processEvent(sdl_event, dxv_event)) {
                if (dxv_event.type == DxvUI::EventType::Quit)
                    quit = true;
                else
                    scene->processEvent(dxv_event);
            }
        }

        scene->update();

        dxv_renderer.clear(DxvUI::Colors::White);
        scene->draw();
        dxv_renderer.present();

        fps.tick();
        fpsLabel->setText(
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
