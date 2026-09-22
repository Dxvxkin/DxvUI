// Brush-based painting contract tests (docs/RENDERING_REFACTORING.md). Since
// stage 7 removed IRenderer and CanvasAdapter, the float ICanvas methods ARE
// the backend surface: this file records the float/Brush calls a caller or a
// widget's paint code emits through PaintContext and asserts their semantics
// (empty brush paints nothing, one ICanvas call carries fill+border, a
// transparent node paints nothing, ...). Pure pass-through/rounding behavior
// of the removed adapter (float -> int boundary, thickness clamps) is covered
// by the SDLRenderer pixel tests.

#include <gtest/gtest.h>

#include <memory>
#include <string>
#include <vector>

#include "DxvUI/Log.h"
#include "DxvUI/Scene.h"
#include "DxvUI/SceneNode.h"
#include "DxvUI/style/Colors.h"
#include "DxvUI/style/StyleManager.h"
#include "DxvUI/style/Theme.h"
#include "FakeBackend.h"

using namespace DxvUI;

namespace {

// An ICanvas-only recorder that captures the float Brush-based calls verbatim.
// It mirrors the shared ICanvas semantics (an empty brush paints nothing) and
// drops nothing else, so tests can assert what paint code actually emits.
class RecordingCanvas : public ICanvas {
   public:
    struct RoundRectCall {
        RectF rect;
        float radius = 0;
        Brush brush;
    };
    struct CircleCall {
        PointF center;
        float radius = 0;
        Brush brush;
    };
    struct RectCall {
        RectF rect;
        Fill fill;
    };
    struct StrokeRectCall {
        RectF rect;
        Stroke stroke;
    };

    std::vector<RoundRectCall> roundRects;
    std::vector<CircleCall> circles;
    std::vector<RectF> clipPushes;
    int clipPops = 0;

    void pushClip(const RectF& rect) override { clipPushes.push_back(rect); }
    void popClip() override { clipPops++; }
    void drawTexture(const std::shared_ptr<ITexture>&, const RectF&) override {}
    void drawTexture(const std::shared_ptr<ITexture>&, const ICanvas::TextureDraw&) override {}
    void fillRect(const RectF&, const Fill&) override {}
    void strokeRect(const RectF&, const Stroke&) override {}
    void fillRoundRect(const RectF& rect, float radius, const Brush& brush) override {
        if (brush.isEmpty()) return;
        roundRects.push_back({rect, radius, brush});
    }
    void fillCircle(const PointF& center, float radius, const Brush& brush) override {
        if (brush.isEmpty()) return;
        circles.push_back({center, radius, brush});
    }
    void strokeArc(const PointF&, float, float, float, const Stroke&) override {}
    void fillPolygon(std::span<const PointF>, const Fill&) override {}
    void drawLine(const PointF&, const PointF&, const Stroke&) override {}
};

// Draws a node through PaintContext over a recording canvas — the stage-7
// entry point (the removed SceneNode::draw(IRenderer&) built it internally).
void drawNode(SceneNode& node, ICanvas& canvas, ITextEngine& textEngine) {
    PaintContext pc(canvas, textEngine, FrameInfo{.viewport = {0, 0, 800, 600}, .timeMs = 0.0});
    node.draw(pc);
}

}  // namespace

TEST(BrushTest, NamedConstructorsBuildExpectedBrushes) {
    const Brush filled = Brush::filled(Colors::Red);
    ASSERT_TRUE(filled.fill.has_value());
    EXPECT_FALSE(filled.stroke.has_value());
    EXPECT_EQ(filled.fill->color, Colors::Red);

    const Brush stroked = Brush::stroked(Colors::Blue, 2.0f);
    EXPECT_FALSE(stroked.fill.has_value());
    ASSERT_TRUE(stroked.stroke.has_value());
    EXPECT_EQ(stroked.stroke->color, Colors::Blue);
    EXPECT_FLOAT_EQ(stroked.stroke->thickness, 2.0f);

    const Brush both = Brush::filledAndStroked(Colors::White, Stroke{Colors::Black, 3.0f});
    ASSERT_TRUE(both.fill.has_value());
    ASSERT_TRUE(both.stroke.has_value());
    EXPECT_EQ(both.fill->color, Colors::White);
    EXPECT_EQ(both.stroke->color, Colors::Black);
    EXPECT_FLOAT_EQ(both.stroke->thickness, 3.0f);
}

TEST(BrushTest, DefaultBrushIsEmpty) {
    const Brush brush;
    EXPECT_TRUE(brush.isEmpty());
    EXPECT_FALSE(Brush::filled(Colors::Red).isEmpty());
    EXPECT_FALSE(Brush::stroked(Colors::Red).isEmpty());
}

TEST(GeometryTest, RectFConvertsFromLayoutRectWithoutLoss) {
    const Rect layout{3, 4, 5, 6};
    const RectF painting = layout;
    EXPECT_EQ(painting, RectF(3.0f, 4.0f, 5.0f, 6.0f));
    EXPECT_FLOAT_EQ(painting.right(), 8.0f);
    EXPECT_FLOAT_EQ(painting.bottom(), 10.0f);
}

TEST(GeometryTest, RectFRoundsEdgesIndependently) {
    EXPECT_EQ(RectF(1.4f, 2.6f, 10.0f, 20.0f).rounded(), (Rect{1, 3, 10, 20}));
    EXPECT_EQ(RectF(0.0f, 0.0f, 0.4f, 0.4f).rounded(), (Rect{0, 0, 0, 0}));
    EXPECT_EQ(RectF(10.0f, 10.0f, -5.0f, -5.0f).rounded(), (Rect{10, 10, 0, 0}));
}

TEST(GeometryTest, PointFRoundsToNearestPixel) {
    EXPECT_EQ(PointF(2.5f, -1.5f).rounded(), (PointI{3, -2}));
    EXPECT_EQ(PointI(7, 9), PointF(PointI(7, 9)).rounded());
}

TEST(BrushDispatchTest, FillRoundRectCarriesWholeBrush) {
    RecordingCanvas canvas;
    // fill-only: one call, no stroke.
    canvas.fillRoundRect(RectF(0, 0, 10, 10), 4.0f, Brush::filled(Colors::Red));
    // border-only: one call, stroke carries thickness.
    canvas.fillRoundRect(RectF(0, 0, 10, 10), 4.0f, Brush::stroked(Colors::Green, 2.0f));
    // fill + border: ONE ICanvas call carrying both, not two primitive calls.
    canvas.fillRoundRect(RectF(0, 0, 10, 10), 4.0f,
                         Brush::filledAndStroked(Colors::White, Stroke{Colors::Black, 1.0f}));

    ASSERT_EQ(canvas.roundRects.size(), 3u);

    const auto& fillOnly = canvas.roundRects[0];
    ASSERT_TRUE(fillOnly.brush.fill.has_value());
    EXPECT_EQ(fillOnly.brush.fill->color, Colors::Red);
    EXPECT_FALSE(fillOnly.brush.stroke.has_value());

    const auto& borderOnly = canvas.roundRects[1];
    ASSERT_TRUE(borderOnly.brush.stroke.has_value());
    EXPECT_EQ(borderOnly.brush.stroke->color, Colors::Green);
    EXPECT_FLOAT_EQ(borderOnly.brush.stroke->thickness, 2.0f);
    EXPECT_FALSE(borderOnly.brush.fill.has_value());

    const auto& both = canvas.roundRects[2];
    ASSERT_TRUE(both.brush.fill.has_value());
    ASSERT_TRUE(both.brush.stroke.has_value());
    EXPECT_EQ(both.brush.fill->color, Colors::White);
    EXPECT_EQ(both.brush.stroke->color, Colors::Black);
    EXPECT_FLOAT_EQ(both.brush.stroke->thickness, 1.0f);
}

TEST(BrushDispatchTest, CircleCarriesWholeBrush) {
    RecordingCanvas canvas;
    canvas.fillCircle(PointF(2.5f, 3.5f), 4.0f, Brush::filled(Colors::Red));
    canvas.fillCircle(PointF(10, 20), 5.0f, Brush::stroked(Colors::Blue, 2.0f));
    canvas.fillCircle(PointF(30, 40), 6.0f,
                      Brush::filledAndStroked(Colors::White, Stroke{Colors::Black, 1.0f}));

    ASSERT_EQ(canvas.circles.size(), 3u);
    EXPECT_EQ(canvas.circles[0].center, PointF(2.5f, 3.5f));
    EXPECT_EQ(canvas.circles[0].radius, 4.0f);
    ASSERT_TRUE(canvas.circles[0].brush.fill.has_value());
    EXPECT_EQ(canvas.circles[0].brush.fill->color, Colors::Red);

    ASSERT_TRUE(canvas.circles[1].brush.stroke.has_value());
    EXPECT_EQ(canvas.circles[1].brush.stroke->color, Colors::Blue);
    EXPECT_FLOAT_EQ(canvas.circles[1].brush.stroke->thickness, 2.0f);

    const auto& both = canvas.circles[2];
    ASSERT_TRUE(both.brush.fill.has_value());
    ASSERT_TRUE(both.brush.stroke.has_value());
    EXPECT_EQ(both.brush.fill->color, Colors::White);
    EXPECT_EQ(both.brush.stroke->color, Colors::Black);
}

TEST(BrushDispatchTest, EmptyBrushPaintsNothing) {
    RecordingCanvas canvas;
    canvas.fillRoundRect(RectF(0, 0, 10, 10), 4.0f, Brush{});
    canvas.fillCircle(PointF(5, 5), 3.0f, Brush{});
    EXPECT_TRUE(canvas.roundRects.empty());
    EXPECT_TRUE(canvas.circles.empty());
}

TEST(BrushDispatchTest, ClipGuardPairsPushAndPop) {
    RecordingCanvas canvas;
    {
        ClipGuard guard(canvas, Rect{1, 2, 3, 4}, true);
        EXPECT_EQ(canvas.clipPushes.size(), 1u);
        EXPECT_EQ(canvas.clipPops, 0);
    }
    EXPECT_EQ(canvas.clipPops, 1);
    {
        ClipGuard disabled(canvas, Rect{1, 2, 3, 4}, false);
    }
    EXPECT_EQ(canvas.clipPushes.size(), 1u);
    EXPECT_EQ(canvas.clipPops, 1);
}

TEST(BrushDispatchTest, NodeBackgroundMappedToBrush) {
    Log::init();
    auto scene = Scene::create();
    auto root = scene->getRoot();
    auto node = std::make_shared<SceneNode>("styled");
    root->addChild(node);
    node->setStyle({.backgroundColor = Colors::White,
                    .borderColor = Colors::Black,
                    .borderThickness = 2,
                    .borderRadius = 4},
                   WidgetState::Normal);
    Theme theme;
    StyleManager manager{theme};
    manager.resolveDirtyStyles(root);
    node->arrange(Rect{0, 0, 100, 50});
    RecordingCanvas canvas;
    FakeTextEngine textEngine;
    drawNode(*node, canvas, textEngine);
    ASSERT_EQ(canvas.roundRects.size(), 1u);
    const auto& call = canvas.roundRects[0];
    EXPECT_EQ(call.rect, RectF(Rect{0, 0, 100, 50}));
    EXPECT_EQ(call.radius, 4.0f);
    ASSERT_TRUE(call.brush.fill.has_value());
    EXPECT_EQ(call.brush.fill->color, Colors::White);
    ASSERT_TRUE(call.brush.stroke.has_value());
    EXPECT_EQ(call.brush.stroke->color, Colors::Black);
    EXPECT_FLOAT_EQ(call.brush.stroke->thickness, 2.0f);
}

TEST(BrushDispatchTest, TransparentBackgroundWithoutBorderPaintsNothing) {
    Log::init();
    auto scene = Scene::create();
    auto root = scene->getRoot();
    auto node = std::make_shared<SceneNode>("plain");
    root->addChild(node);
    node->setStyle({.backgroundColor = Color(0, 0, 0, 0), .borderThickness = 0},
                   WidgetState::Normal);
    Theme theme;
    StyleManager manager{theme};
    manager.resolveDirtyStyles(root);
    node->arrange(Rect{0, 0, 100, 50});
    RecordingCanvas canvas;
    FakeTextEngine textEngine;
    drawNode(*node, canvas, textEngine);
    EXPECT_TRUE(canvas.roundRects.empty());
}