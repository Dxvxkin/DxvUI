#include <DxvUI/DxvEvent.h>
#include <DxvUI/Log.h>
#include <DxvUI/Scene.h>
#include <DxvUI/SceneNode.h>
#include <DxvUI/core.h>
#include <DxvUI/backend/SDLRenderer.h>
#include <DxvUI/style/Colors.h>
#include <DxvUI/widgets/Button.h>
#include <DxvUI/widgets/Label.h>
#include <SDL.h>

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

using namespace DxvUI;

namespace {

using Clock = std::chrono::steady_clock;

double msSince(const Clock::time_point& start) {
    return std::chrono::duration<double, std::milli>(Clock::now() - start).count();
}

//-----------------------------------------------------------------------
// Statistics: every metric is reported as a distribution, not a single
// average. With --repeats the samples of all runs are merged into one pool.
//-----------------------------------------------------------------------

struct Summary {
    double mean = 0, median = 0, min = 0, max = 0, p95 = 0;
    size_t n = 0;
};

Summary summarize(std::vector<double> samples) {
    Summary s;
    s.n = samples.size();
    if (samples.empty()) return s;
    std::sort(samples.begin(), samples.end());
    double sum = 0.0;
    for (double v : samples) sum += v;
    s.mean = sum / samples.size();
    s.min = samples.front();
    s.max = samples.back();
    s.median = samples[samples.size() / 2];
    s.p95 = samples[std::min(samples.size() - 1, static_cast<size_t>(samples.size() * 0.95))];
    return s;
}

//-----------------------------------------------------------------------
// Command-line options.
//-----------------------------------------------------------------------

struct Options {
    bool json = false;
    bool vsync = false;
    int repeats = 1;
    std::string scenarios;  // comma-separated; empty = all
};

void printHelp() {
    std::printf("Usage: DxvUIBenchmark.exe [--json] [--vsync] [--repeats N|--repeats=N]\n");
    std::printf("                        [--scenario=frames,scroll,hit,text,clip,micro]\n");
}

Options parseArgs(int argc, char* argv[]) {
    Options opt;
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--json")
            opt.json = true;
        else if (arg == "--vsync")
            opt.vsync = true;
        else if (arg == "--repeats" && i + 1 < argc)
            opt.repeats = std::atoi(argv[++i]);
        else if (arg.rfind("--repeats=", 0) == 0)
            opt.repeats = std::atoi(arg.c_str() + 10);
        else if (arg == "--scenario" && i + 1 < argc)
            opt.scenarios = argv[++i];
        else if (arg.rfind("--scenario=", 0) == 0)
            opt.scenarios = arg.substr(11);
        else if (arg == "--help" || arg == "-h") {
            printHelp();
            std::exit(0);
        } else
            std::printf("bench: ignoring unknown arg '%s'\n", arg.c_str());
    }
    if (opt.repeats < 1) opt.repeats = 1;
    return opt;
}

// --scenario selects by comma-separated prefixes; an empty selector runs all.
bool wants(const Options& opt, std::string_view name) {
    if (opt.scenarios.empty()) return true;
    size_t pos = 0;
    while (true) {
        const size_t comma = opt.scenarios.find(',', pos);
        const std::string_view part(opt.scenarios.data() + pos, comma == std::string::npos
                                                                    ? opt.scenarios.size() - pos
                                                                    : comma - pos);
        if (!part.empty() && name.substr(0, part.size()) == part) return true;
        if (comma == std::string::npos) break;
        pos = comma + 1;
    }
    return false;
}

//-----------------------------------------------------------------------
// Result collection: human-readable print now, JSON emitted at the end.
//-----------------------------------------------------------------------

std::vector<std::pair<std::string, Summary>> g_results;

void addSamples(const std::string& name, const std::vector<double>& samples) {
    g_results.emplace_back(name, summarize(samples));
}

void addScalar(const std::string& name, double value) {
    g_results.emplace_back(name, Summary{value, value, value, value, value, 1});
}

void printSamples(const char* label, const std::vector<double>& samples) {
    const Summary s = summarize(samples);
    std::printf("       %-32s mean=%.3f median=%.3f min=%.3f p95=%.3f (n=%zu)\n", label, s.mean,
                s.median, s.min, s.p95, s.n);
}

void printJson(const Options& opt) {
    std::ostringstream j;
    j << "{\n  \"repeats\": " << opt.repeats << ",\n  \"vsync\": " << (opt.vsync ? "true" : "false")
      << ",\n  \"metrics\": {\n";
    for (size_t i = 0; i < g_results.size(); ++i) {
        const auto& [name, s] = g_results[i];
        j << "    \"" << name << "\": {\"mean\": " << s.mean << ", \"median\": " << s.median
          << ", \"min\": " << s.min << ", \"max\": " << s.max << ", \"p95\": " << s.p95
          << ", \"n\": " << s.n << "}";
        if (i + 1 < g_results.size()) j << ",";
        j << "\n";
    }
    j << "  }\n}\n";
    std::printf("---JSON---\n%s---JSON---\n", j.str().c_str());
}

//-----------------------------------------------------------------------
// Scenes.
//-----------------------------------------------------------------------

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
    root->setStyle({.fontSize = 18, .fontFamily = "Sans"}, WidgetState::Normal);

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

void warmUp(Scene& scene, SDLRenderer& renderer, int frames) {
    for (int i = 0; i < frames; ++i) {
        scene.update();
        renderer.clear(Colors::White);
        scene.draw();
        renderer.present();
    }
}

//-----------------------------------------------------------------------
// Frame scenarios: clean (no input), static hover, hover toggle.
//-----------------------------------------------------------------------

enum class HoverMode { None, Static, Toggle };

struct FrameSamples {
    std::vector<double> update, draw, event;
};

void appendSamples(FrameSamples& dst, const FrameSamples& src) {
    dst.update.insert(dst.update.end(), src.update.begin(), src.update.end());
    dst.draw.insert(dst.draw.end(), src.draw.begin(), src.draw.end());
    dst.event.insert(dst.event.end(), src.event.begin(), src.event.end());
}

void runFrames(Scene& scene, SDLRenderer& renderer, int frames, HoverMode hoverMode,
               FrameSamples& out) {
    auto root = scene.getRoot();
    auto firstBtn = root->findNodeById("btn_0");
    if (hoverMode == HoverMode::Static && firstBtn) firstBtn->setHovered(true);
    const Rect btnBounds = firstBtn ? firstBtn->getGlobalBounds() : Rect{};
    const int onX = btnBounds.x + btnBounds.width / 2;
    const int onY = btnBounds.y + btnBounds.height / 2;

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
            out.event.push_back(msSince(te));
        }

        auto t0 = Clock::now();
        scene.update();
        out.update.push_back(msSince(t0));

        renderer.clear(Colors::White);
        t0 = Clock::now();
        scene.draw();
        out.draw.push_back(msSince(t0));
        renderer.present();
    }
}

// Scrolls the whole grid down step by step by rewriting every button's top.
// There is no scroll container / transform in the engine yet, so "scrolling"
// literally means re-positioning all buttons each frame (setStyle -> dirty ->
// full relayout). The draw() call after it must stay flat because only the
// window that fits the 800x600 viewport is culled in.
struct ScrollSamples {
    std::vector<double> style, relayout, draw;
};

ScrollSamples runScroll(SDLRenderer& renderer, int count, int repeats) {
    constexpr int kSteps = 30;

    const int rows = (count + kGridCols - 1) / kGridCols;
    const int gridBottom = kStartY + rows * (kCellH + kGapY) - kGapY;
    const int scrollRange = gridBottom - kViewportH;  // content below the viewport
    if (scrollRange <= 0) return {};                  // content fits the viewport
    // Scroll the window over the whole scrollable range in kSteps; never push
    // the content fully off-screen (a fixed 80px step does that on small grids).
    const float kStepPx = std::max(80.0f, static_cast<float>(scrollRange) / kSteps);

    auto scene = buildScene(count);
    scene->setRenderer(&renderer);
    auto root = scene->getRoot();
    auto buttons = root->getChildren();
    if (buttons.empty()) return {};

    warmUp(*scene, renderer, 20);

    ScrollSamples out;
    for (int r = 0; r < repeats; ++r) {
        for (int step = 1; step <= kSteps; ++step) {
            // Repeat offset keeps the content scrolling even across runs.
            const float offset = step * kStepPx + r * kViewportH;
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
            out.style.push_back(msSince(t));

            renderer.clear(Colors::White);
            t = Clock::now();
            scene->update();
            out.relayout.push_back(msSince(t));

            renderer.clear(Colors::White);
            t = Clock::now();
            scene->draw();
            out.draw.push_back(msSince(t));
            renderer.present();
        }
    }
    return out;
}

// btn_0 is scanned last by the reverse-iteration hit test (worst case, full
// O(n)). The "center" button is the one under the middle of the viewport - a
// typical click target, still O(n) because it sits in front of ~n siblings.
// Both are stationary targets: the hovered-node cache (if any) short-circuits
// them to O(1), which is exactly the win this scenario demonstrates.
struct HitSamples {
    std::vector<double> first, center, moving;
};

void hitTestAt(Scene& scene, const std::shared_ptr<SceneNode>& btn, int events,
               std::vector<double>& out) {
    if (!btn) return;
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

    for (int i = 0; i < events; ++i) {
        DxvEvent move;
        move.type = EventType::MouseMove;
        move.mouse.x = x;
        move.mouse.y = y;
        move.mouse.button = MouseButton::None;
        auto t = Clock::now();
        scene.processEvent(move);
        out.push_back(msSince(t));
    }
}

HitSamples runHitTest(SDLRenderer& renderer, int count, int repeats) {
    const int rows = (count + kGridCols - 1) / kGridCols;
    const int centerCol =
        std::clamp((kViewportW / 2 - kStartX) / (kCellW + kGapX), 0, kGridCols - 1);
    const int centerRow = std::clamp((kViewportH / 2 - kStartY) / (kCellH + kGapY), 0, rows - 1);
    const int centerIndex = centerRow * kGridCols + centerCol;

    HitSamples out;
    for (int r = 0; r < repeats; ++r) {
        auto scene = buildScene(count);
        scene->setRenderer(&renderer);
        auto root = scene->getRoot();
        warmUp(*scene, renderer, 20);

        hitTestAt(*scene, root->findNodeById("btn_0"), kHitTestEvents, out.first);
        hitTestAt(*scene, root->findNodeById("btn_" + std::to_string(centerIndex)), kHitTestEvents,
                  out.center);

        // Moving mouse: every call lands on a different *on-screen* button, so a
        // hovered-node cache can never short-circuit. Measured with findNodeAt()
        // directly instead of processEvent(), because each move would change the
        // hover state and fold a relayout into the timing. Click points outside
        // the viewport would early-return at the root (O(1)), so only buttons
        // whose center is inside the viewport are cycled through.
        std::vector<int> onscreen;
        for (int i = 0; i < count; ++i) {
            auto btn = root->findNodeById("btn_" + std::to_string(i));
            const Rect b = btn ? btn->getGlobalBounds() : Rect{};
            const int cx = b.x + b.width / 2;
            const int cy = b.y + b.height / 2;
            if (cx >= 0 && cx < kViewportW && cy >= 0 && cy < kViewportH) onscreen.push_back(i);
        }
        for (int i = 0; i < kHitTestEvents; ++i) {
            const int idx = onscreen[i % onscreen.size()];
            auto btn = root->findNodeById("btn_" + std::to_string(idx));
            const Rect b = btn->getGlobalBounds();
            const int x = b.x + b.width / 2;
            const int y = b.y + b.height / 2;
            auto t = Clock::now();
            root->findNodeAt(x, y);
            out.moving.push_back(msSince(t));
        }
    }
    return out;
}

void reportScene(SDLRenderer& renderer, int count, const Options& opt) {
    const std::string prefix = "frames(" + std::to_string(count) + ")";

    auto scene = buildScene(count);
    scene->setRenderer(&renderer);
    auto root = scene->getRoot();

    // Buttons are not the whole tree: each one also has a CenterContainer and a
    // Label, so the live scene is ~3x the button count.
    std::printf("count=%d: built (tree=%d nodes)\n", count, SceneNode::getNodeCount());

    // Warm-up: resolve styles, lay out, build texture/font caches.
    warmUp(*scene, renderer, 20);

    if (wants(opt, "frames")) {
        FrameSamples clean, stat, toggle;
        for (int r = 0; r < opt.repeats; ++r) {
            runFrames(*scene, renderer, kFramesClean, HoverMode::None, clean);
            runFrames(*scene, renderer, kFramesStatic, HoverMode::Static, stat);
            runFrames(*scene, renderer, kFramesToggle, HoverMode::Toggle, toggle);
        }

        // Full frame cost = update + draw (+ event for hover scenarios).
        std::vector<double> frameClean(clean.update.size());
        for (size_t i = 0; i < clean.update.size(); ++i)
            frameClean[i] = clean.update[i] + clean.draw[i];
        std::vector<double> frameToggle(toggle.event.size());
        for (size_t i = 0; i < toggle.event.size(); ++i)
            frameToggle[i] = toggle.event[i] + toggle.update[i] + toggle.draw[i];

        const double relayoutCost = summarize(toggle.update).median - summarize(stat.update).median;
        const double fpsClean = 1000.0 / summarize(frameClean).median;
        const double fpsToggle = 1000.0 / summarize(frameToggle).median;

        std::printf("count=%d  update(clean):\n", count);
        printSamples("update(clean)", clean.update);
        printSamples("draw(clean)", clean.draw);
        printSamples("frame(clean)", frameClean);
        printSamples("update(static hover)", stat.update);
        printSamples("draw(static hover)", stat.draw);
        printSamples("update(hover toggle)", toggle.update);
        printSamples("event(hover)", toggle.event);
        printSamples("draw(hover toggle)", toggle.draw);
        printSamples("frame(hover toggle)", frameToggle);
        std::printf("       relayout cost from hover toggle (median): %.3f ms/frame\n",
                    relayoutCost);
        std::printf("       full frame fps (median): clean=%.1f  toggle=%.1f\n", fpsClean,
                    fpsToggle);

        addSamples(prefix + ".clean.update_ms", clean.update);
        addSamples(prefix + ".clean.draw_ms", clean.draw);
        addSamples(prefix + ".clean.frame_ms", frameClean);
        addSamples(prefix + ".static.update_ms", stat.update);
        addSamples(prefix + ".static.draw_ms", stat.draw);
        addSamples(prefix + ".toggle.event_ms", toggle.event);
        addSamples(prefix + ".toggle.update_ms", toggle.update);
        addSamples(prefix + ".toggle.draw_ms", toggle.draw);
        addSamples(prefix + ".toggle.frame_ms", frameToggle);
    }

    if (wants(opt, "scroll")) {
        const ScrollSamples s = runScroll(renderer, count, opt.repeats);
        if (!s.style.empty()) {
            std::printf("count=%d  scroll:\n", count);
            printSamples("scroll.setStyle", s.style);
            printSamples("scroll.relayout", s.relayout);
            printSamples("scroll.draw", s.draw);
            addSamples(prefix + ".scroll.setstyle_ms", s.style);
            addSamples(prefix + ".scroll.relayout_ms", s.relayout);
            addSamples(prefix + ".scroll.draw_ms", s.draw);
        }
    }

    if (wants(opt, "hit")) {
        const HitSamples h = runHitTest(renderer, count, opt.repeats);
        std::printf("count=%d  hit-test:\n", count);
        printSamples("hit-test first", h.first);
        printSamples("hit-test center", h.center);
        printSamples("hit-test moving (scan only)", h.moving);
        addSamples(prefix + ".hit.first_ms", h.first);
        addSamples(prefix + ".hit.center_ms", h.center);
        addSamples(prefix + ".hit.moving_ms", h.moving);
    }
}

//-----------------------------------------------------------------------
// Text scenario: labels whose text changes every frame. Every change is a
// cache miss (a new (font, text, color) key), so this measures real
// rasterization + texture-cache growth.
//-----------------------------------------------------------------------

struct TextSamples {
    std::vector<double> update, draw;
    size_t cacheBefore = 0, cacheAfter = 0;
};

TextSamples runTextDynamic(SDLRenderer& renderer, int repeats) {
    constexpr int kLabels = 12;
    constexpr int kFrames = 120;
    constexpr int kCols = 4;

    auto scene = Scene::create();
    auto root = scene->getRoot();
    root->setStyle({.fontSize = 18, .fontFamily = "Sans"}, WidgetState::Normal);

    std::vector<std::shared_ptr<Label>> labels;
    for (int i = 0; i < kLabels; ++i) {
        const int col = i % kCols;
        const int row = i / kCols;
        auto lbl = Label::create("label_" + std::to_string(i), "init");
        lbl->setStyle({.left = static_cast<float>(kStartX + col * 180),
                       .top = static_cast<float>(kStartY + row * 50),
                       .width = 170.0f,
                       .height = 30.0f},
                      WidgetState::Normal);
        root->addChild(lbl);
        labels.push_back(lbl);
    }
    scene->setRenderer(&renderer);
    warmUp(*scene, renderer, 20);

    TextSamples out;
    auto& engine = renderer.getTextEngine();
    out.cacheBefore = engine.getTextureCacheCount();
    for (int r = 0; r < repeats; ++r) {
        for (int frame = 0; frame < kFrames; ++frame) {
            for (int i = 0; i < kLabels; ++i) {
                labels[static_cast<size_t>(i)]->setText("r" + std::to_string(r) + " v" +
                                                        std::to_string(frame * kLabels + i));
            }
            auto t0 = Clock::now();
            scene->update();
            out.update.push_back(msSince(t0));

            renderer.clear(Colors::White);
            t0 = Clock::now();
            scene->draw();
            out.draw.push_back(msSince(t0));
            renderer.present();
        }
    }
    out.cacheAfter = engine.getTextureCacheCount();
    return out;
}

//-----------------------------------------------------------------------
// Clip scenario: deeply nested clipContent chains plus a wide grid of
// clipped boxes. Each draw traverses and pushes/pops clip rects.
//-----------------------------------------------------------------------

struct ClipSamples {
    std::vector<double> draw;
};

ClipSamples runClips(SDLRenderer& renderer, int repeats) {
    constexpr int kFrames = 120;
    constexpr int kChainDepth = 12;

    auto scene = Scene::create();
    auto root = scene->getRoot();
    root->setStyle({.fontSize = 14, .fontFamily = "Sans"}, WidgetState::Normal);

    // Deep chain: pushes a clip per nesting level on every draw.
    auto parent = root;
    for (int i = 0; i < kChainDepth; ++i) {
        auto node = std::make_shared<SceneNode>("clip_chain_" + std::to_string(i));
        node->setStyle({.clipContent = true,
                        .left = 0.0f,
                        .top = 0.0f,
                        .width = static_cast<float>(kViewportW),
                        .height = static_cast<float>(kViewportH)},
                       WidgetState::Normal);
        parent->addChild(node);
        parent = node;
    }
    parent->addChild(Label::create("clip_leaf", "deep"));

    // Wide grid: many small clip boxes on a single frame.
    for (int i = 0; i < 40; ++i) {
        const int col = i % 4;
        const int row = i / 4;
        auto box = std::make_shared<SceneNode>("clip_box_" + std::to_string(i));
        box->setStyle({.clipContent = true,
                       .left = static_cast<float>(kStartX + col * 120),
                       .top = static_cast<float>(kStartY + row * 55),
                       .width = 100.0f,
                       .height = 45.0f},
                      WidgetState::Normal);
        root->addChild(box);
        box->addChild(Label::create("clip_box_label_" + std::to_string(i), "x"));
    }

    scene->setRenderer(&renderer);
    warmUp(*scene, renderer, 20);

    ClipSamples out;
    for (int r = 0; r < repeats; ++r) {
        for (int frame = 0; frame < kFrames; ++frame) {
            auto t0 = Clock::now();
            scene->update();
            renderer.clear(Colors::White);
            t0 = Clock::now();
            scene->draw();
            out.draw.push_back(msSince(t0));
            renderer.present();
        }
    }
    return out;
}

void reportText(SDLRenderer& renderer, const Options& opt) {
    std::printf("\n--- Text scenario (dynamic labels, uncached rasterization) ---\n");
    const TextSamples t = runTextDynamic(renderer, opt.repeats);
    printSamples("text.update", t.update);
    printSamples("text.draw", t.draw);
    const long growth = static_cast<long>(t.cacheAfter) - static_cast<long>(t.cacheBefore);
    std::printf("       texture cache growth: %+ld (before=%zu after=%zu)\n", growth, t.cacheBefore,
                t.cacheAfter);
    addSamples("text.dynamic.update_ms", t.update);
    addSamples("text.dynamic.draw_ms", t.draw);
    addScalar("text.dynamic.cache_growth", static_cast<double>(growth));
}

void reportClip(SDLRenderer& renderer, const Options& opt) {
    std::printf("\n--- Clip scenario (nested clipContent) ---\n");
    const ClipSamples c = runClips(renderer, opt.repeats);
    printSamples("clip.draw", c.draw);
    addSamples("clip.nested.draw_ms", c.draw);
}

//-----------------------------------------------------------------------
// Microbenchmarks (ms per 1000 calls).
//-----------------------------------------------------------------------

void microbenchmarks(SDLRenderer& renderer, const Options& opt) {
    std::printf("\n--- Microbenchmarks (ms per 1000 calls) ---\n");

    const Rect box{100, 100, 160, 40};
    const Color color = Colors::CornflowerBlue;

    std::vector<double> fillRectS, fillRoundRectS, measureS, rasterizeCachedS, rasterizeUncachedS,
        drawTextureS;

    auto& engine = renderer.getTextEngine();
    auto textFont = engine.getFont(getDefaultFontPath(), 18);
    const size_t cacheBefore = engine.getTextureCacheCount();

    for (int r = 0; r < opt.repeats; ++r) {
        auto t0 = Clock::now();
        for (int i = 0; i < 100000; ++i) renderer.fillRect(box, color);
        fillRectS.push_back(msSince(t0) / 100.0);

        t0 = Clock::now();
        for (int i = 0; i < 100000; ++i) renderer.fillRoundRect(box, 5, color);
        fillRoundRectS.push_back(msSince(t0) / 100.0);

        t0 = Clock::now();
        for (int i = 0; i < 10000; ++i) engine.measure(*textFont, "Hello DxvUI");
        measureS.push_back(msSince(t0) / 10.0);

        t0 = Clock::now();
        for (int i = 0; i < 10000; ++i) engine.rasterize(*textFont, "Hello DxvUI", Colors::Black);
        rasterizeCachedS.push_back(msSince(t0) / 10.0);

        // Uncached: every call uses a distinct string (per repeat), so each one
        // is a cache miss that rasterizes a new texture.
        t0 = Clock::now();
        for (int i = 0; i < 2000; ++i) {
            engine.rasterize(*textFont, "uncached_r" + std::to_string(r) + "_" + std::to_string(i),
                             Colors::Black);
        }
        rasterizeUncachedS.push_back(msSince(t0) / 2.0);

        auto textTexture = engine.rasterize(*textFont, "Hello DxvUI", Colors::Black);
        t0 = Clock::now();
        for (int i = 0; i < 10000; ++i) {
            renderer.drawTexture(textTexture,
                                 {10, 10, textTexture->getWidth(), textTexture->getHeight()});
        }
        drawTextureS.push_back(msSince(t0) / 10.0);
    }

    const long growth =
        static_cast<long>(engine.getTextureCacheCount()) - static_cast<long>(cacheBefore);

    printSamples("fillRect", fillRectS);
    printSamples("fillRoundRect r=5", fillRoundRectS);
    printSamples("text: measure (cached)", measureS);
    printSamples("text: rasterize (cached)", rasterizeCachedS);
    printSamples("text: rasterize (uncached)", rasterizeUncachedS);
    printSamples("text: drawTexture", drawTextureS);
    std::printf("       texture cache growth (uncached rasterize): %+ld\n", growth);

    addSamples("micro.fillRect", fillRectS);
    addSamples("micro.fillRoundRect", fillRoundRectS);
    addSamples("micro.measure_cached", measureS);
    addSamples("micro.rasterize_cached", rasterizeCachedS);
    addSamples("micro.rasterize_uncached", rasterizeUncachedS);
    addSamples("micro.drawTexture", drawTextureS);
    addScalar("micro.rasterize_uncached.cache_growth", static_cast<double>(growth));
}

}  // namespace

int main(int argc, char* argv[]) {
    std::setvbuf(stdout, nullptr, _IONBF, 0);
    Log::init();
    // Node registration logs (info) would spam the benchmark output; keep
    // errors/warnings visible so failures during style resolution are not lost.
    Log::setLevel(spdlog::level::warn);

    const Options opt = parseArgs(argc, argv);

    std::printf("bench: creating renderer (vsync=%s)\n", opt.vsync ? "on" : "off");
    SDLRenderer renderer("DxvUI Benchmark", kViewportW, kViewportH, opt.vsync);

    std::printf("bench: repeats=%d\n", opt.repeats);
    if (wants(opt, "frames") || wants(opt, "scroll") || wants(opt, "hit")) {
        std::printf("bench: scale sweep\n");
        reportScene(renderer, 200, opt);
        std::printf("bench: done 200\n");
        reportScene(renderer, 1000, opt);
        std::printf("bench: done 1000\n");
        reportScene(renderer, 5000, opt);
        std::printf("bench: done 5000\n");
    }

    if (wants(opt, "text")) reportText(renderer, opt);
    if (wants(opt, "clip")) reportClip(renderer, opt);
    if (wants(opt, "micro")) microbenchmarks(renderer, opt);

    if (opt.json) printJson(opt);
    return 0;
}
