// Stage-2/3 tests (docs/RENDERING_REFACTORING.md): the Brush-based painting
// contract and the CanvasAdapter that maps it onto the backend's explicitly
// colored/bordered primitives, including the float -> pixel boundary and the
// stage-3 tinted texture path.

#include <gtest/gtest.h>

#include <memory>
#include <string>
#include <vector>

#include "DxvUI/Log.h"
#include "DxvUI/Scene.h"
#include "DxvUI/SceneNode.h"
#include "DxvUI/interfaces/IRenderer.h"
#include "DxvUI/style/Colors.h"
#include "DxvUI/style/StyleManager.h"
#include "DxvUI/style/Theme.h"
#include "backend/CanvasAdapter.h"

using namespace DxvUI;

namespace {

class StubTextEngine : public ITextEngine {
   public:
    std::shared_ptr<IFont> getFont(const std::string&, int) override { return nullptr; }
    std::shared_ptr<IFont> getFontForFamily(const std::string&, int) override { return nullptr; }
    void registerFontFamily(const std::string&, const std::string&) override {}
    TextMetrics measure(const IFont&, const std::string&) override { return {0, 0}; }
    int measurePrefix(const IFont&, const std::string&, size_t) override { return 0; }
    size_t charIndexAtX(const IFont&, const std::string&, int) override { return 0; }
    LineMetrics lineMetrics(const IFont&) override { return {0, 0, 0}; }
    std::shared_ptr<ITexture> rasterize(const IFont&, const std::string&, const Color&) override {
        return nullptr;
    }
    size_t getTextureCacheCount() const override { return 0; }
    TextLayout layoutText(const IFont&, std::string_view) override { return {}; }
    void drawLayout(ICanvas&, const TextLayout&, const RectF&, const TextPaint&) override {}
};

class StubClipboard : public IClipboard {
   public:
    std::string getText() override { return {}; }
    bool setText(const std::string&) override { return true; }
};

/**
 * @brief IRenderer stub that records every backend call the adapter makes.
 */
class RecordingRenderer : public IRenderer, public ICanvas {
   public:
    struct RectCall {
        Rect rect;
        Color color;
        int thickness = 0;
    };
    struct CircleCall {
        int centerX = 0, centerY = 0, radius = 0;
        Color color;
        int thickness = 0;
    };
    struct LineCall {
        int x1 = 0, y1 = 0, x2 = 0, y2 = 0;
        Color color;
        int thickness = 1;
    };
    struct ArcCall {
        int centerX = 0, centerY = 0, radius = 0;
        float startAngle = 0, endAngle = 0;
        Color color;
        int thickness = 1;
    };
    struct PolygonCall {
        std::vector<PointI> points;
        Color color;
    };
    struct TextureCall {
        Rect dst;
        std::optional<Rect> src;
        std::optional<Color> tint;
        float alpha = 1.0f;
    };

    std::vector<RectCall> filledRects;
    std::vector<RectCall> filledBorderedRects;
    std::vector<RectCall> strokedRects;
    std::vector<RectCall> filledRoundRects;
    std::vector<RectCall> filledBorderedRoundRects;
    std::vector<RectCall> strokedRoundRects;
    std::vector<CircleCall> filledCircles;
    std::vector<CircleCall> filledBorderedCircles;
    std::vector<CircleCall> strokedCircles;
    std::vector<ArcCall> arcs;
    std::vector<LineCall> lines;
    std::vector<PolygonCall> polygons;
    std::vector<Rect> textures;
    std::vector<TextureCall> tintedTextures;
    std::vector<Rect> clipPushes;
    int clipPops = 0;

    void clear(const Color&) override {}
    void present() override {}
    Size getViewportSize() const override { return {800, 600}; }

    float getDpiScale() const override { return 1.0f; }

    ICanvas& beginFrame(const Color&) override { return *this; }
    void endFrame() override {}

    // ICanvas float-based (stage 5) – no-op for fake
    void pushClip(const RectF&) override {}
    void popClip() override {}
    void drawTexture(const std::shared_ptr<ITexture>&, const RectF&) override {}
    void drawTexture(const std::shared_ptr<ITexture>&, const ICanvas::TextureDraw&) override {}
    void fillRect(const RectF&, const Fill&) override {}
    void strokeRect(const RectF&, const Stroke&) override {}
    void fillRoundRect(const RectF&, float, const Brush&) override {}
    void fillCircle(const PointF&, float, const Brush&) override {}
    void strokeArc(const PointF&, float, float, float, const Stroke&) override {}
    void fillPolygon(std::span<const PointF>, const Fill&) override {}
    void drawLine(const PointF&, const PointF&, const Stroke&) override {}
    ITextEngine& getTextEngine() override { return textEngine; }
    IClipboard& getClipboard() override { return clipboard; }
    void setCursor(CursorType) override {}
    CursorType getCursor() const override { return CursorType::Arrow; }
    void pushClipRect(const Rect& rect) override { clipPushes.push_back(rect); }
    void popClipRect() override { ++clipPops; }
    void drawTexture(const std::shared_ptr<ITexture>&, const Rect& dstRect) override {
        textures.push_back(dstRect);
    }
    void drawTexture(const std::shared_ptr<ITexture>&, const TextureDrawDesc& desc) override {
        tintedTextures.push_back({desc.dst, desc.src, desc.tint, desc.alpha});
        textures.push_back(desc.dst);
    }

    void drawRect(const Rect& rect, const Border& border) override {
        strokedRects.push_back({rect, border.color, border.thickness});
    }
    void fillRect(const Rect& rect, const Color& color) override {
        filledRects.push_back({rect, color, 0});
    }
    void fillRect(const Rect& rect, const Color& fillColor, const Border& border) override {
        filledBorderedRects.push_back({rect, fillColor, border.thickness});
    }
    void drawLine(int x1, int y1, int x2, int y2, const Color& color, int thickness) override {
        lines.push_back({x1, y1, x2, y2, color, thickness});
    }
    void drawCircle(int centerX, int centerY, int radius, const Border& border) override {
        strokedCircles.push_back({centerX, centerY, radius, border.color, border.thickness});
    }
    void fillCircle(int centerX, int centerY, int radius, const Color& color) override {
        filledCircles.push_back({centerX, centerY, radius, color, 0});
    }
    void fillCircle(int centerX, int centerY, int radius, const Color& fillColor,
                    const Border& border) override {
        filledBorderedCircles.push_back({centerX, centerY, radius, fillColor, border.thickness});
    }
    void drawArc(int centerX, int centerY, int radius, float startAngle, float endAngle,
                 const Border& border) override {
        arcs.push_back(
            {centerX, centerY, radius, startAngle, endAngle, border.color, border.thickness});
    }
    void drawRoundRect(const Rect& rect, int, const Border& border) override {
        strokedRoundRects.push_back({rect, border.color, border.thickness});
    }
    void fillRoundRect(const Rect& rect, int, const Color& color) override {
        filledRoundRects.push_back({rect, color, 0});
    }
    void fillRoundRect(const Rect& rect, int, const Color& fillColor,
                       const Border& border) override {
        filledBorderedRoundRects.push_back({rect, fillColor, border.thickness});
    }
    void fillPolygon(const std::vector<PointI>& points, const Color& color) override {
        polygons.push_back({points, color});
    }

   private:
    StubTextEngine textEngine;
    StubClipboard clipboard;
};

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

TEST(CanvasAdapterTest, FillRectForwardsColor) {
    RecordingRenderer renderer;
    CanvasAdapter canvas(renderer);
    canvas.fillRect(RectF(1.2f, 2.8f, 10.0f, 20.0f), Fill{Colors::Red});
    ASSERT_EQ(renderer.filledRects.size(), 1u);
    EXPECT_EQ(renderer.filledRects[0].rect, (Rect{1, 3, 10, 20}));
    EXPECT_EQ(renderer.filledRects[0].color, Colors::Red);
}

TEST(CanvasAdapterTest, StrokeRectForwardsBorder) {
    RecordingRenderer renderer;
    CanvasAdapter canvas(renderer);
    canvas.strokeRect(Rect{0, 0, 10, 10}, Stroke{Colors::Black, 3.0f});
    ASSERT_EQ(renderer.strokedRects.size(), 1u);
    EXPECT_EQ(renderer.strokedRects[0].color, Colors::Black);
    EXPECT_EQ(renderer.strokedRects[0].thickness, 3);
}

TEST(CanvasAdapterTest, RoundRectPicksThePathMatchingTheBrush) {
    RecordingRenderer renderer;
    CanvasAdapter canvas(renderer);
    canvas.fillRoundRect(Rect{0, 0, 10, 10}, 4.0f, Brush::filled(Colors::Red));
    canvas.fillRoundRect(Rect{0, 0, 10, 10}, 4.0f, Brush::stroked(Colors::Green, 2.0f));
    canvas.fillRoundRect(Rect{0, 0, 10, 10}, 4.0f,
                         Brush::filledAndStroked(Colors::White, Stroke{Colors::Black, 1.0f}));
    ASSERT_EQ(renderer.filledRoundRects.size(), 1u);
    EXPECT_EQ(renderer.filledRoundRects[0].color, Colors::Red);
    ASSERT_EQ(renderer.strokedRoundRects.size(), 1u);
    EXPECT_EQ(renderer.strokedRoundRects[0].color, Colors::Green);
    EXPECT_EQ(renderer.strokedRoundRects[0].thickness, 2);
    ASSERT_EQ(renderer.filledBorderedRoundRects.size(), 1u);
    EXPECT_EQ(renderer.filledBorderedRoundRects[0].color, Colors::White);
    EXPECT_EQ(renderer.filledBorderedRoundRects[0].thickness, 1);
}

TEST(CanvasAdapterTest, EmptyBrushPaintsNothing) {
    RecordingRenderer renderer;
    CanvasAdapter canvas(renderer);
    canvas.fillRoundRect(Rect{0, 0, 10, 10}, 4.0f, Brush{});
    canvas.fillCircle(PointI(5, 5), 3.0f, Brush{});
    EXPECT_TRUE(renderer.filledRoundRects.empty());
    EXPECT_TRUE(renderer.strokedRoundRects.empty());
    EXPECT_TRUE(renderer.filledBorderedRoundRects.empty());
    EXPECT_TRUE(renderer.filledCircles.empty());
    EXPECT_TRUE(renderer.strokedCircles.empty());
}

TEST(CanvasAdapterTest, CirclePicksThePathMatchingTheBrush) {
    RecordingRenderer renderer;
    CanvasAdapter canvas(renderer);
    canvas.fillCircle(PointF(2.5f, 3.5f), 4.0f, Brush::filled(Colors::Red));
    canvas.fillCircle(PointI(10, 20), 5.0f, Brush::stroked(Colors::Blue, 2.0f));
    canvas.fillCircle(PointI(30, 40), 6.6f,
                      Brush::filledAndStroked(Colors::White, Stroke{Colors::Black, 1.0f}));
    ASSERT_EQ(renderer.filledCircles.size(), 1u);
    EXPECT_EQ(renderer.filledCircles[0].centerX, 3);
    EXPECT_EQ(renderer.filledCircles[0].centerY, 4);
    EXPECT_EQ(renderer.filledCircles[0].radius, 4);
    EXPECT_EQ(renderer.filledCircles[0].color, Colors::Red);
    ASSERT_EQ(renderer.strokedCircles.size(), 1u);
    EXPECT_EQ(renderer.strokedCircles[0].color, Colors::Blue);
    EXPECT_EQ(renderer.strokedCircles[0].thickness, 2);
    ASSERT_EQ(renderer.filledBorderedCircles.size(), 1u);
    EXPECT_EQ(renderer.filledBorderedCircles[0].radius, 7);
    EXPECT_EQ(renderer.filledBorderedCircles[0].color, Colors::White);
}

TEST(CanvasAdapterTest, ArcForwardsAnglesAndBorder) {
    RecordingRenderer renderer;
    CanvasAdapter canvas(renderer);
    canvas.strokeArc(PointI(10, 20), 6.0f, 0.0f, 180.0f, Stroke{Colors::Blue, 2.4f});
    ASSERT_EQ(renderer.arcs.size(), 1u);
    EXPECT_EQ(renderer.arcs[0].centerX, 10);
    EXPECT_EQ(renderer.arcs[0].radius, 6);
    EXPECT_FLOAT_EQ(renderer.arcs[0].startAngle, 0.0f);
    EXPECT_FLOAT_EQ(renderer.arcs[0].endAngle, 180.0f);
    EXPECT_EQ(renderer.arcs[0].thickness, 2);
}

TEST(CanvasAdapterTest, LineThicknessIsClampedToOnePixel) {
    RecordingRenderer renderer;
    CanvasAdapter canvas(renderer);
    canvas.drawLine(PointI(0, 0), PointI(10, 0), Stroke{Colors::Red, 1.0f});
    canvas.drawLine(PointI(0, 0), PointI(10, 0), Stroke{Colors::Red, 3.0f});
    canvas.drawLine(PointI(0, 0), PointI(10, 0), Stroke{Colors::Red, 0.2f});
    ASSERT_EQ(renderer.lines.size(), 3u);
    EXPECT_EQ(renderer.lines[0].thickness, 1);
    EXPECT_EQ(renderer.lines[1].thickness, 3);
    EXPECT_EQ(renderer.lines[2].thickness, 1);
    EXPECT_EQ(renderer.lines[1].x2, 10);
}

TEST(CanvasAdapterTest, PolygonRoundsPointsAndNeedsThreeVertices) {
    RecordingRenderer renderer;
    CanvasAdapter canvas(renderer);
    const std::vector<PointF> triangle = {PointF(0.4f, 0.6f), PointF(10.0f, 0.0f),
                                          PointF(0.0f, 10.0f)};
    canvas.fillPolygon(triangle, Fill{Colors::Green});
    canvas.fillPolygon(std::vector<PointF>{PointF(0, 0), PointF(1, 1)}, Fill{Colors::Green});
    ASSERT_EQ(renderer.polygons.size(), 1u);
    ASSERT_EQ(renderer.polygons[0].points.size(), 3u);
    EXPECT_EQ(renderer.polygons[0].points[0], (PointI{0, 1}));
    EXPECT_EQ(renderer.polygons[0].points[1], (PointI{10, 0}));
    EXPECT_EQ(renderer.polygons[0].color, Colors::Green);
}

TEST(CanvasAdapterTest, TextureAndClipForwardToTheBackend) {
    RecordingRenderer renderer;
    CanvasAdapter canvas(renderer);
    canvas.drawTexture(nullptr, RectF(1.4f, 2.6f, 4.0f, 4.0f));
    canvas.pushClip(RectF(0.5f, 0.5f, 9.0f, 9.0f));
    canvas.popClip();
    ASSERT_EQ(renderer.textures.size(), 1u);
    EXPECT_EQ(renderer.textures[0], (Rect{1, 3, 4, 4}));
    ASSERT_EQ(renderer.clipPushes.size(), 1u);
    EXPECT_EQ(renderer.clipPushes[0], (Rect{1, 1, 9, 9}));
    EXPECT_EQ(renderer.clipPops, 1);
}

TEST(CanvasAdapterTest, TintedTextureForwardsTintAndAlpha) {
    RecordingRenderer renderer;
    CanvasAdapter canvas(renderer);
    ICanvas::TextureDraw td;
    td.dst = RectF(0, 0, 10, 10);
    td.tint = Colors::Red;
    td.alpha = 0.5f;
    canvas.drawTexture(nullptr, td);
    ASSERT_EQ(renderer.tintedTextures.size(), 1u);
    EXPECT_EQ(renderer.tintedTextures[0].tint, Colors::Red);
    EXPECT_FLOAT_EQ(renderer.tintedTextures[0].alpha, 0.5f);
}

TEST(CanvasAdapterTest, ClipGuardPairsPushAndPop) {
    RecordingRenderer renderer;
    CanvasAdapter canvas(renderer);
    {
        ClipGuard guard(canvas, Rect{1, 2, 3, 4}, true);
        EXPECT_EQ(renderer.clipPushes.size(), 1u);
        EXPECT_EQ(renderer.clipPops, 0);
    }
    EXPECT_EQ(renderer.clipPops, 1);
    {
        ClipGuard disabled(canvas, Rect{1, 2, 3, 4}, false);
    }
    EXPECT_EQ(renderer.clipPushes.size(), 1u);
    EXPECT_EQ(renderer.clipPops, 1);
}

TEST(CanvasAdapterTest, DefaultBackgroundMapsComputedAppearanceToBrush) {
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
    RecordingRenderer renderer;
    node->draw(renderer);
    ASSERT_EQ(renderer.filledBorderedRoundRects.size(), 1u);
    const auto& call = renderer.filledBorderedRoundRects[0];
    EXPECT_EQ(call.color, Colors::White);
    EXPECT_EQ(call.thickness, 2);
    EXPECT_EQ(call.rect, (Rect{0, 0, 100, 50}));
}

TEST(CanvasAdapterTest, TransparentBackgroundWithoutBorderPaintsNothing) {
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
    RecordingRenderer renderer;
    node->draw(renderer);
    EXPECT_TRUE(renderer.filledRoundRects.empty());
    EXPECT_TRUE(renderer.strokedRoundRects.empty());
    EXPECT_TRUE(renderer.filledBorderedRoundRects.empty());
}
