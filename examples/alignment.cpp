#include <DxvUI/Log.h>
#include <DxvUI/Scene.h>
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

#include <memory>
#include <string>

constexpr int SCREEN_WIDTH = 800;
constexpr int SCREEN_HEIGHT = 600;

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

AlignmentDemoNodes buildAlignmentDemoUI(const std::shared_ptr<DxvUI::Scene>& scene) {
    auto root = scene->getRoot();

    root->setStyle({.textColor = DxvUI::Colors::DarkGray,
                    .fontSize = 16,
                    .fontPath = DxvUI::getDefaultFontPath(),
                    .width = SCREEN_WIDTH,
                    .height = SCREEN_HEIGHT},
                   DxvUI::WidgetState::Normal);

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

    const auto cycleVerticalAlignment = [](DxvUI::DxvEvent& event) {
        if (auto target = event.target.lock()) {
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
            if (auto scene = target->getScene()) {
                scene->updateLayout();
            }
            const DxvUI::Rect bounds = target->getGlobalBounds();
            DxvUI::Log::info("{} verticalAlignment {} -> {} bounds=({}, {}, {}, {})",
                             target->getId(), alignmentToString(oldAlignment),
                             alignmentToString(rule.verticalAlignment.value()), bounds.x, bounds.y,
                             bounds.width, bounds.height);
        }
    };
    rowStart->on(DxvUI::EventType::Click, cycleVerticalAlignment);
    rowCenter->on(DxvUI::EventType::Click, cycleVerticalAlignment);
    rowEnd->on(DxvUI::EventType::Click, cycleVerticalAlignment);

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

    const auto demo = buildAlignmentDemoUI(scene);
    scene->updateLayout();

    logNodeInfo("row", demo.rowStart);
    logNodeInfo("row", demo.rowCenter);
    logNodeInfo("row", demo.rowEnd);
    logAnchoredInfo(demo.anchoredChild);

    bool quit = false;
    SDL_Event sdl_event;
    Uint32 last_time = SDL_GetTicks();

    while (!quit) {
        Uint32 current_time = SDL_GetTicks();
        float delta_time = (current_time - last_time) / 1000.0f;
        last_time = current_time;

        while (SDL_PollEvent(&sdl_event) != 0) {
            DxvUI::DxvEvent dxv_event;
            if (eventSource.processEvent(sdl_event, dxv_event)) {
                if (dxv_event.type == DxvUI::EventType::Quit)
                    quit = true;
                else
                    scene->processEvent(dxv_event);
            }
        }

        scene->update(delta_time);

        dxv_renderer.clear(DxvUI::Colors::White);
        scene->draw();
        dxv_renderer.present();
    }
    scene->shutdown();
    scene.reset();
    return 0;
}
