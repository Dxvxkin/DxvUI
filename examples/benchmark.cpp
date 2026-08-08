#include <DxvUI/DxvEvent.h>
#include <DxvUI/Log.h>
#include <DxvUI/Scene.h>
#include <DxvUI/SceneNode.h>
#include <DxvUI/core.h>
#include <DxvUI/renderers/SDLRenderer.h>
#include <DxvUI/style/Colors.h>
#include <DxvUI/widgets/Button.h>
#include <DxvUI/widgets/Label.h>
#include <SDL.h>

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <memory>
#include <string>

using namespace DxvUI;

namespace {

using Clock = std::chrono::steady_clock;

double msSince(const Clock::time_point& start) {
    return std::chrono::duration<double, std::milli>(Clock::now() - start).count();
}

constexpr int kGridCols = 40;
constexpr int kCellW = 180;
constexpr int kCellH = 60;
constexpr int kGapX = 20;
constexpr int kGapY = 20;
constexpr int kStartX = 50;
constexpr int kStartY = 50;
constexpr int kViewportW = 800;
constexpr int kViewportH = 600;

constexpr int kFramesClean = 500;
constexpr int kFramesStatic = 500;
constexpr int kFramesToggle = 200;
constexpr int kHitTestEvents = 300;

std::shared_ptr<Scene> buildScene(int buttonCount) {
    auto scene = Scene::create();
    auto root = scene->getRoot();
    root->setStyle({.fontSize = 18, .fontPath = getDefaultFontPath()}, WidgetState::Normal);

    for (int i = 0; i < buttonCount; ++i) {
        int col = i % kGridCols;
        int row = i / kGridCols;
        auto btn = Button::create("btn_" + std::to_string(i), "Button " + std::to_string(i));
        btn->setStyle({.left = static_cast<float>(kStartX + col * (kCellW + kGapX)),
                       .top = static_cast<float>(kStartY + row * (kCellH + kGapY)),
                       .width = static_cast<float>(kCellW),
                       .height = static_cast<float>(kCellH)},
                      WidgetState::Normal);
        root->addChild(btn);
    }
    return scene;
}

enum class HoverMode { None, Static, Toggle };

void runFrames(Scene& scene, SDLRenderer& renderer, int frames, HoverMode hoverMode,
               double& updateMs, double& drawMs, double& eventMs) {
    auto root = scene.getRoot();
    auto firstBtn = root->findNodeById("btn_0");
    if (hoverMode == HoverMode::Static && firstBtn) firstBtn->setHovered(true);
    const Rect btnBounds = firstBtn ? firstBtn->getGlobalBounds() : Rect{};
    const int onX = btnBounds.x + btnBounds.width / 2;
    const int onY = btnBounds.y + btnBounds.height / 2;

    updateMs = 0.0;
    drawMs = 0.0;
    eventMs = 0.0;
    for (int i = 0; i < frames; ++i) {
        if (hoverMode != HoverMode::None) {
            // Static: the mouse stays on btn_0, so hover is set once on the
            // first frame and never changes afterwards. Toggle: the mouse
            // alternates onto/away from the button, flipping the hover state
            // (and its relayout) every frame.
            const int x = (hoverMode == HoverMode::Toggle && i % 2 == 1) ? 1 : onX;
            const int y = (hoverMode == HoverMode::Toggle && i % 2 == 1) ? 1 : onY;
            DxvEvent move;
            move.type = EventType::MouseMove;
            move.mouse.x = x;
            move.mouse.y = y;
            move.mouse.button = MouseButton::None;
            auto te = Clock::now();
            scene.processEvent(move);
            eventMs += msSince(te);
        }

        auto t0 = Clock::now();
        scene.update(0.016f);
        updateMs += msSince(t0);

        renderer.clear(Colors::White);
        t0 = Clock::now();
        scene.draw();
        drawMs += msSince(t0);
        renderer.present();
    }
    updateMs /= frames;
    drawMs /= frames;
    eventMs /= frames;
}

// Scrolls the whole grid down step by step by rewriting every button's top.
// There is no scroll container / transform in the engine yet, so "scrolling"
// literally means re-positioning all buttons each frame (setStyle -> dirty ->
// full relayout). The draw() call after it must stay flat because only the
// window that fits the 800x600 viewport is culled in.
void runScroll(SDLRenderer& renderer, int count) {
    constexpr int kSteps = 30;

    const int rows = (count + kGridCols - 1) / kGridCols;
    const int gridBottom = kStartY + rows * (kCellH + kGapY) - kGapY;
    const int scrollRange = gridBottom - kViewportH;  // content below the viewport
    if (scrollRange <= 0) {
        std::printf("       scroll: content fits viewport (skipped)\n");
        return;
    }
    // Scroll the window over the whole scrollable range in kSteps; never push
    // the content fully off-screen (a fixed 80px step does that on small grids).
    const float kStepPx = std::max(80.0f, static_cast<float>(scrollRange) / kSteps);

    auto scene = buildScene(count);
    scene->setRenderer(&renderer);
    auto root = scene->getRoot();
    auto buttons = root->getChildren();
    if (buttons.empty()) return;

    for (int i = 0; i < 20; ++i) {
        scene->update(0.016f);
        renderer.clear(Colors::White);
        scene->draw();
        renderer.present();
    }

    double styleMs = 0.0, relayoutMs = 0.0, drawMs = 0.0, drawMin = 1e9, drawMax = 0.0;
    for (int step = 1; step <= kSteps; ++step) {
        const float offset = step * kStepPx;
        auto t = Clock::now();
        for (int i = 0; i < static_cast<int>(buttons.size()); ++i) {
            int col = i % kGridCols;
            int row = i / kGridCols;
            buttons[static_cast<size_t>(i)]->setStyle(
                {.left = static_cast<float>(kStartX + col * (kCellW + kGapX)),
                 .top = static_cast<float>(kStartY + row * (kCellH + kGapY)) - offset,
                 .width = static_cast<float>(kCellW),
                 .height = static_cast<float>(kCellH)},
                WidgetState::Normal);
        }
        styleMs += msSince(t);

        renderer.clear(Colors::White);
        t = Clock::now();
        scene->update(0.016f);
        relayoutMs += msSince(t);

        renderer.clear(Colors::White);
        t = Clock::now();
        scene->draw();
        const double d = msSince(t);
        drawMs += d;
        drawMin = std::min(drawMin, d);
        drawMax = std::max(drawMax, d);
        renderer.present();
    }
    std::printf(
        "       scroll setStyle=%.3fms  update(relayout)=%.3fms  "
        "draw avg=%.3fms min=%.3f max=%.3f\n",
        styleMs / kSteps, relayoutMs / kSteps, drawMs / kSteps, drawMin, drawMax);
}

void hitTestAt(Scene& scene, const std::shared_ptr<SceneNode>& btn, const char* label, int events) {
    if (!btn) {
        std::printf("       hit-test %-6s: <no button>\n", label);
        return;
    }
    const Rect b = btn->getGlobalBounds();
    const int x = b.x + b.width / 2;
    const int y = b.y + b.height / 2;

    // Warm-up events: the first MouseMove over the button sets hover -> dirty ->
    // relayout; fire a few before timing so the measured cost is pure hit-test.
    for (int i = 0; i < 5; ++i) {
        DxvEvent warm;
        warm.type = EventType::MouseMove;
        warm.mouse.x = x;
        warm.mouse.y = y;
        warm.mouse.button = MouseButton::None;
        scene.processEvent(warm);
    }

    double ms = 0.0;
    for (int i = 0; i < events; ++i) {
        DxvEvent move;
        move.type = EventType::MouseMove;
        move.mouse.x = x;
        move.mouse.y = y;
        move.mouse.button = MouseButton::None;
        auto t = Clock::now();
        scene.processEvent(move);
        ms += msSince(t);
    }
    std::printf("       hit-test %-6s: %.3f ms/call\n", label, ms / events);
}

// btn_0 is scanned last by the reverse-iteration hit test (worst case, full
// O(n)). The "center" button is the one under the middle of the viewport - a
// typical click target, still O(n) because it sits in front of ~n siblings.
// Both are stationary targets: the hovered-node cache (if any) short-circuits
// them to O(1), which is exactly the win this scenario demonstrates.
void runHitTest(SDLRenderer& renderer, int count) {
    auto scene = buildScene(count);
    scene->setRenderer(&renderer);
    auto root = scene->getRoot();

    for (int i = 0; i < 20; ++i) {
        scene->update(0.016f);
        renderer.clear(Colors::White);
        scene->draw();
        renderer.present();
    }

    const int rows = (count + kGridCols - 1) / kGridCols;
    const int centerCol =
        std::clamp((kViewportW / 2 - kStartX) / (kCellW + kGapX), 0, kGridCols - 1);
    const int centerRow = std::clamp((kViewportH / 2 - kStartY) / (kCellH + kGapY), 0, rows - 1);
    const int centerIndex = centerRow * kGridCols + centerCol;

    hitTestAt(*scene, root->findNodeById("btn_0"), "first", kHitTestEvents);
    hitTestAt(*scene, root->findNodeById("btn_" + std::to_string(centerIndex)), "center",
              kHitTestEvents);

    // Moving mouse: every call lands on a different *on-screen* button, so a
    // hovered-node cache can never short-circuit. Measured with findNodeAt()
    // directly instead of processEvent(), because each move would change the
    // hover state and fold a relayout into the timing. Click points outside the
    // viewport would early-return at the root (O(1)), so only buttons whose
    // center is inside the viewport are cycled through.
    std::vector<int> onscreen;
    for (int i = 0; i < count; ++i) {
        auto btn = root->findNodeById("btn_" + std::to_string(i));
        const Rect b = btn ? btn->getGlobalBounds() : Rect{};
        const int cx = b.x + b.width / 2;
        const int cy = b.y + b.height / 2;
        if (cx >= 0 && cx < kViewportW && cy >= 0 && cy < kViewportH) onscreen.push_back(i);
    }
    double ms = 0.0;
    for (int i = 0; i < kHitTestEvents; ++i) {
        const int idx = onscreen[i % onscreen.size()];
        auto btn = root->findNodeById("btn_" + std::to_string(idx));
        const Rect b = btn->getGlobalBounds();
        const int x = b.x + b.width / 2;
        const int y = b.y + b.height / 2;
        auto t = Clock::now();
        root->findNodeAt(x, y);
        ms += msSince(t);
    }
    std::printf("       hit-test moving: %.3f ms/call (scan only, %zu targets)\n",
                ms / kHitTestEvents, onscreen.size());
}

void reportScene(SDLRenderer& renderer, int count) {
    auto scene = buildScene(count);
    scene->setRenderer(&renderer);
    auto root = scene->getRoot();

    // Buttons are not the whole tree: each one also has a CenterContainer and a
    // Label, so the live scene is ~3x the button count.
    std::printf("count=%d: built (tree=%d nodes)\n", count, SceneNode::getNodeCount());

    // Warm-up: resolve styles, lay out, build texture/font caches.
    for (int i = 0; i < 20; ++i) {
        scene->update(0.016f);
        renderer.clear(Colors::White);
        scene->draw();
        renderer.present();
    }

    double updateClean, drawClean, updateStatic, drawStatic, updateToggle, drawToggle;
    double eventClean, eventStatic, eventToggle;
    runFrames(*scene, renderer, kFramesClean, HoverMode::None, updateClean, drawClean, eventClean);
    std::printf("count=%d: clean run done\n", count);
    runFrames(*scene, renderer, kFramesStatic, HoverMode::Static, updateStatic, drawStatic,
              eventStatic);
    std::printf("count=%d: static run done\n", count);
    runFrames(*scene, renderer, kFramesToggle, HoverMode::Toggle, updateToggle, drawToggle,
              eventToggle);
    std::printf("count=%d: toggle run done\n", count);

    // Static keeps the mouse on the same button, so hover is set once (no state
    // change afterwards). Toggle alternates the mouse on/off, flipping the hover
    // state -- and setHovered() -> markLayoutDirty() -> relayout -- every frame.
    std::printf("count=%d  update(clean)=%.3fms  draw(clean)=%.3fms\n", count, updateClean,
                drawClean);
    std::printf("       update(static hover)=%.3fms  draw=%.3fms\n", updateStatic, drawStatic);
    std::printf("       update(hover toggle)=%.3fms  draw=%.3fms\n", updateToggle, drawToggle);
    std::printf("       relayout cost from hover toggle: %.3fms/frame\n",
                updateToggle - updateStatic);

    const double frameClean = updateClean + drawClean;
    const double frameToggle = eventToggle + updateToggle + drawToggle;
    std::printf("       event(hover)=%.3fms/frame\n", eventToggle);
    std::printf("       full frame(clean)=%.3fms (%.1f fps)\n", frameClean, 1000.0 / frameClean);
    std::printf("       full frame(toggle)=%.3fms (%.1f fps)\n", frameToggle, 1000.0 / frameToggle);

    runScroll(renderer, count);
    std::printf("count=%d: scroll done\n", count);
    runHitTest(renderer, count);
    std::printf("count=%d: hit-test done\n", count);
}

void microbenchmarks(SDLRenderer& renderer) {
    std::printf("\n--- Microbenchmarks (ms per 1000 calls) ---\n");

    const Rect box{100, 100, 160, 40};
    const Color color = Colors::CornflowerBlue;

    auto t0 = Clock::now();
    for (int i = 0; i < 100000; ++i) renderer.fillRect(box, color);
    const double fillRect = msSince(t0) / 100.0;

    t0 = Clock::now();
    for (int i = 0; i < 100000; ++i) renderer.fillRoundRect(box, 5, color);
    const double fillRoundRect = msSince(t0) / 100.0;

    std::printf("fillRect        : %.3f ms/1000\n", fillRect);
    std::printf("fillRoundRect r=5: %.3f ms/1000\n", fillRoundRect);

    t0 = Clock::now();
    for (int i = 0; i < 10000; ++i) {
        renderer.measureText("Hello DxvUI", getDefaultFontPath(), 18);
    }
    std::printf("measureText     : %.3f ms/1000\n", msSince(t0) / 10.0);

    renderer.setFont(getDefaultFontPath(), 18);
    renderer.setDrawColor(Colors::Black);
    t0 = Clock::now();
    for (int i = 0; i < 10000; ++i) {
        renderer.createTextTexture("Hello DxvUI");
    }
    std::printf("createTextTexture (per call): %.3f ms/1000\n", msSince(t0) / 10.0);

    t0 = Clock::now();
    for (int i = 0; i < 10000; ++i) {
        renderer.drawText("Hello DxvUI", 10, 10);
    }
    std::printf("drawText primitive (texture per call): %.3f ms/1000\n", msSince(t0) / 10.0);
}

}  // namespace

extern "C" int SDL_main(int /*argc*/, char* /*argv*/[]) {
    std::setvbuf(stdout, nullptr, _IONBF, 0);
    Log::init();
    // Node registration logs (info) would spam the benchmark output; keep
    // errors/warnings visible so failures during style resolution are not lost.
    Log::setLevel(spdlog::level::warn);
    std::printf("bench: creating renderer\n");
    SDLRenderer renderer("DxvUI Benchmark", kViewportW, kViewportH);

    std::printf("bench: scale sweep\n");
    reportScene(renderer, 200);
    std::printf("bench: done 200\n");
    reportScene(renderer, 1000);
    std::printf("bench: done 1000\n");
    reportScene(renderer, 5000);
    std::printf("bench: done 5000\n");

    microbenchmarks(renderer);

    return 0;
}
