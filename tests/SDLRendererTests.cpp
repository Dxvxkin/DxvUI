#include <SDL.h>
#include <gtest/gtest.h>

#include <array>
#include <cstring>
#include <memory>
#include <string>

#include "DxvUI/Log.h"
#include "DxvUI/backend/SDLRenderer.h"
#include "DxvUI/backend/SDLTextEngine.h"
#include "DxvUI/core.h"
#include "DxvUI/interfaces/ITexture.h"
#include "DxvUI/style/Colors.h"

// Internal to the backend (not installed): the test wraps a raw SDL_Texture
// the same way the text engine does. Reached through the src/ include path
// added for the tests target in CMakeLists.txt.
#include "backend/SDLTexture.h"

using namespace DxvUI;

namespace {

// SceneNode's destructor and the error paths under test log via DxvUI::Log,
// which requires an initialized logger (same pattern as WidgetTests.cpp).
class LoggerEnvironment : public ::testing::Environment {
   public:
    void SetUp() override { Log::init(); }
};

::testing::Environment* const g_logger_environment =
    ::testing::AddGlobalTestEnvironment(new LoggerEnvironment());

constexpr int kSurfaceWidth = 200;
constexpr int kSurfaceHeight = 160;

class SDLRendererTest : public ::testing::Test {
   protected:
    void SetUp() override {
        surface = SDL_CreateRGBSurfaceWithFormat(0, kSurfaceWidth, kSurfaceHeight, 32,
                                                 SDL_PIXELFORMAT_RGBA32);
        ASSERT_NE(surface, nullptr);
        sdlRenderer = SDL_CreateSoftwareRenderer(surface);
        ASSERT_NE(sdlRenderer, nullptr);
        renderer = std::make_unique<SDLRenderer>(sdlRenderer);
    }

    void TearDown() override {
        renderer.reset();
        SDL_DestroyRenderer(sdlRenderer);
        SDL_FreeSurface(surface);
    }

    Uint32 pixelAt(int x, int y) {
        renderer->flushBatch();
        SDL_RenderFlush(sdlRenderer);
        Uint32 pixel = 0;
        std::memcpy(
            &pixel,
            static_cast<const Uint8*>(surface->pixels) + y * surface->pitch + x * sizeof(Uint32),
            sizeof(Uint32));
        return pixel;
    }

    Uint32 mapColor(const Color& color) const {
        return SDL_MapRGB(surface->format, color.r, color.g, color.b);
    }

    SDL_Rect currentClip() const {
        SDL_Rect clip{};
        SDL_RenderGetClipRect(sdlRenderer, &clip);
        return clip;
    }

    static ::testing::AssertionResult expectRectEq(const SDL_Rect& actual, int x, int y, int w,
                                                   int h) {
        if (actual.x == x && actual.y == y && actual.w == w && actual.h == h) {
            return ::testing::AssertionSuccess();
        }
        return ::testing::AssertionFailure()
               << "clip is {" << actual.x << "," << actual.y << "," << actual.w << "," << actual.h
               << "}, expected {" << x << "," << y << "," << w << "," << h << "}";
    }

    SDL_Surface* surface = nullptr;
    SDL_Renderer* sdlRenderer = nullptr;
    std::unique_ptr<SDLRenderer> renderer;
};

TEST_F(SDLRendererTest, PushClipRectIntersectsWithCurrentClip) {
    renderer->pushClip(RectF(10, 10, 100, 100));
    EXPECT_TRUE(SDL_RenderIsClipEnabled(sdlRenderer));
    EXPECT_TRUE(expectRectEq(currentClip(), 10, 10, 100, 100));

    renderer->pushClip(RectF(60, 60, 100, 100));
    EXPECT_TRUE(expectRectEq(currentClip(), 60, 60, 50, 50));

    renderer->popClip();
    EXPECT_TRUE(expectRectEq(currentClip(), 10, 10, 100, 100));
    renderer->popClip();
    EXPECT_FALSE(SDL_RenderIsClipEnabled(sdlRenderer));
}

TEST_F(SDLRendererTest, NestedClipConstrainsDrawingToIntersection) {
    const Uint32 white = mapColor(Colors::White);
    const Uint32 red = mapColor(Colors::Red);
    const Uint32 green = mapColor(Colors::Green);
    const Uint32 blue = mapColor(Colors::Blue);

    renderer->fillRect(RectF(0, 0, kSurfaceWidth, kSurfaceHeight), Fill{Colors::White});

    renderer->pushClip(RectF(10, 10, 100, 100));
    renderer->fillRect(RectF(0, 0, kSurfaceWidth, kSurfaceHeight), Fill{Colors::Red});
    EXPECT_EQ(pixelAt(5, 5), white);
    EXPECT_EQ(pixelAt(50, 50), red);
    EXPECT_EQ(pixelAt(150, 150), white);

    renderer->pushClip(RectF(60, 60, 100, 100));
    renderer->fillRect(RectF(0, 0, kSurfaceWidth, kSurfaceHeight), Fill{Colors::Green});
    EXPECT_EQ(pixelAt(70, 70), green);
    EXPECT_EQ(pixelAt(50, 50), red);
    EXPECT_EQ(pixelAt(150, 150), white);

    renderer->popClip();
    renderer->fillRect(RectF(0, 0, kSurfaceWidth, kSurfaceHeight), Fill{Colors::Blue});
    EXPECT_EQ(pixelAt(5, 5), white);
    EXPECT_EQ(pixelAt(50, 50), blue);
    EXPECT_EQ(pixelAt(70, 70), blue);
    EXPECT_EQ(pixelAt(150, 150), white);

    renderer->popClip();
    renderer->fillRect(RectF(0, 0, kSurfaceWidth, kSurfaceHeight), Fill{Colors::Black});
    EXPECT_EQ(pixelAt(150, 150), mapColor(Colors::Black));
}

TEST_F(SDLRendererTest, DisjointNestedClipClipsEverything) {
    const Uint32 white = mapColor(Colors::White);

    renderer->fillRect(RectF(0, 0, kSurfaceWidth, kSurfaceHeight), Fill{Colors::White});
    renderer->pushClip(RectF(10, 10, 50, 50));
    renderer->pushClip(RectF(100, 100, 50, 50));

    EXPECT_TRUE(SDL_RenderIsClipEnabled(sdlRenderer));
    renderer->fillRect(RectF(0, 0, kSurfaceWidth, kSurfaceHeight), Fill{Colors::Red});
    EXPECT_EQ(pixelAt(30, 30), white);
    EXPECT_EQ(pixelAt(120, 120), white);
    EXPECT_EQ(pixelAt(100, 100), white);
    EXPECT_EQ(pixelAt(5, 5), white);

    renderer->popClip();
    renderer->fillRect(RectF(0, 0, kSurfaceWidth, kSurfaceHeight), Fill{Colors::Green});
    EXPECT_EQ(pixelAt(30, 30), mapColor(Colors::Green));
    EXPECT_EQ(pixelAt(120, 120), white);
}

class FakeTexture : public ITexture {
   public:
    int getWidth() const override { return 4; }
    int getHeight() const override { return 4; }
};

TEST_F(SDLRendererTest, DrawTextureRejectsForeignTextureImplementation) {
    const Uint32 white = mapColor(Colors::White);
    renderer->fillRect(RectF(0, 0, kSurfaceWidth, kSurfaceHeight), Fill{Colors::White});

    auto foreign = std::make_shared<FakeTexture>();
    renderer->drawTexture(foreign, RectF(0, 0, 4, 4));
    renderer->drawTexture(std::make_shared<FakeTexture>(), RectF(0, 0, 4, 4));
    // Tinted path also rejects
    ICanvas::TextureDraw td;
    td.dst = RectF(0, 0, 4, 4);
    td.tint = Colors::Red;
    renderer->drawTexture(foreign, td);
    EXPECT_EQ(pixelAt(1, 1), white);

    renderer->drawTexture(nullptr, RectF(0, 0, 4, 4));
    EXPECT_EQ(pixelAt(1, 1), white);
}

TEST_F(SDLRendererTest, DrawTextureRendersTexturePixels) {
    SDL_Texture* raw =
        SDL_CreateTexture(sdlRenderer, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_STATIC, 4, 4);
    ASSERT_NE(raw, nullptr);
    std::array<Uint32, 16> pixels{};
    pixels.fill(mapColor(Colors::Red));
    ASSERT_EQ(SDL_UpdateTexture(raw, nullptr, pixels.data(), 4 * sizeof(Uint32)), 0);
    auto texture = std::make_shared<SDLTexture>(raw);

    renderer->fillRect(RectF(0, 0, kSurfaceWidth, kSurfaceHeight), Fill{Colors::White});
    renderer->drawTexture(texture, RectF(10, 10, 4, 4));

    EXPECT_EQ(pixelAt(12, 12), mapColor(Colors::Red));
    EXPECT_EQ(pixelAt(12, 16), mapColor(Colors::White));
}

TEST_F(SDLRendererTest, DrawTextureTintedRendersWithColorMod) {
    SDL_Texture* raw =
        SDL_CreateTexture(sdlRenderer, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_STATIC, 4, 4);
    ASSERT_NE(raw, nullptr);
    std::array<Uint32, 16> pixels{};
    // White source texture
    pixels.fill(mapColor(Colors::White));
    ASSERT_EQ(SDL_UpdateTexture(raw, nullptr, pixels.data(), 4 * sizeof(Uint32)), 0);
    auto texture = std::make_shared<SDLTexture>(raw);

    renderer->fillRect(RectF(0, 0, kSurfaceWidth, kSurfaceHeight), Fill{Colors::White});
    ICanvas::TextureDraw td;
    td.dst = RectF(10, 10, 4, 4);
    td.tint = Colors::Red;
    renderer->drawTexture(texture, td);

    EXPECT_EQ(pixelAt(12, 12), mapColor(Colors::Red));
}

TEST_F(SDLRendererTest, TextEngineCachesAreLruBounded) {
    SDLTextEngine engine(sdlRenderer);
    auto font = engine.getFontForFamily("Sans", 16);
    if (!font) {
        GTEST_SKIP() << "no loadable default font on this platform";
    }

    for (size_t i = 0; i < SDLTextEngine::kMaxMeasureCacheEntries + 512; ++i) {
        engine.measure(*font, "measure-" + std::to_string(i));
    }
    EXPECT_LE(engine.getMeasureCacheCount(), SDLTextEngine::kMaxMeasureCacheEntries);

    for (size_t i = 0; i < SDLTextEngine::kMaxTextureCacheEntries + 64; ++i) {
        engine.rasterize(*font, "raster-" + std::to_string(i), Colors::Black);
    }
    EXPECT_LE(engine.getTextureCacheCount(), SDLTextEngine::kMaxTextureCacheEntries);

    // Glyph cache: white glyphs per codepoint
    for (size_t i = 0; i < SDLTextEngine::kMaxGlyphCacheEntries + 128; ++i) {
        // Use different codepoints (up to 0xFFFF) to fill glyph cache
        uint32_t cp = static_cast<uint32_t>(32 + (i % 200));
        std::string s;
        if (cp < 128)
            s = std::string(1, static_cast<char>(cp));
        else
            s = "A";  // fallback
        engine.layoutText(*font, s);
    }
    EXPECT_LE(engine.getGlyphCacheCount(), SDLTextEngine::kMaxGlyphCacheEntries);
    EXPECT_LE(engine.getLayoutCacheCount(), SDLTextEngine::kMaxLayoutCacheEntries);

    engine.clearCaches();
    EXPECT_EQ(engine.getMeasureCacheCount(), 0u);
    EXPECT_EQ(engine.getTextureCacheCount(), 0u);
    EXPECT_EQ(engine.getGlyphCacheCount(), 0u);
    EXPECT_EQ(engine.getLayoutCacheCount(), 0u);
}

TEST_F(SDLRendererTest, CanvasBrushPaintsRoundRectFillAndBorder) {
    const Uint32 white = mapColor(Colors::White);
    const Uint32 red = mapColor(Colors::Red);
    const Uint32 blue = mapColor(Colors::Blue);

    renderer->fillRect(RectF(0, 0, kSurfaceWidth, kSurfaceHeight), Fill{Colors::White});

    ICanvas& canvas = *renderer;
    canvas.fillRoundRect(RectF(20, 20, 60, 40), 8.0f,
                         Brush::filledAndStroked(Colors::Red, Stroke{Colors::Blue, 2.0f}));

    EXPECT_EQ(pixelAt(50, 40), red);
    EXPECT_EQ(pixelAt(20, 40), blue);
    EXPECT_EQ(pixelAt(21, 40), blue);
    EXPECT_EQ(pixelAt(20, 20), white);
}

TEST_F(SDLRendererTest, CanvasBrushPaintsThickLineCircleAndArc) {
    const Uint32 red = mapColor(Colors::Red);
    const Uint32 green = mapColor(Colors::Green);
    const Uint32 blue = mapColor(Colors::Blue);

    renderer->fillRect(RectF(0, 0, kSurfaceWidth, kSurfaceHeight), Fill{Colors::White});

    ICanvas& canvas = *renderer;

    canvas.drawLine(PointF(10, 10), PointF(90, 10), Stroke{Colors::Green, 4.0f});
    EXPECT_EQ(pixelAt(50, 10), green);
    EXPECT_EQ(pixelAt(50, 9), green);

    canvas.fillCircle(PointF(120, 40), 12.0f,
                      Brush::filledAndStroked(Colors::Red, Stroke{Colors::Blue, 1.0f}));
    EXPECT_EQ(pixelAt(120, 40), red);
    EXPECT_NE(pixelAt(120, 53), red);

    canvas.strokeArc(PointF(60, 100), 20.0f, 0.0f, 90.0f, Stroke{Colors::Blue, 2.0f});
    const bool arcPainted =
        pixelAt(79, 101) == blue || pixelAt(80, 101) == blue || pixelAt(80, 100) == blue;
    EXPECT_TRUE(arcPainted) << "no arc pixel found near the 0-degree endpoint";
}

TEST_F(SDLRendererTest, CanvasDrawsTintedGlyph) {
    const Uint32 red = mapColor(Colors::Red);

    SDL_Texture* raw =
        SDL_CreateTexture(sdlRenderer, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_STATIC, 8, 16);
    ASSERT_NE(raw, nullptr);
    std::array<Uint32, 128> pixels{};
    pixels.fill(mapColor(Colors::White));
    ASSERT_EQ(SDL_UpdateTexture(raw, nullptr, pixels.data(), 8 * sizeof(Uint32)), 0);
    auto texture = std::make_shared<SDLTexture>(raw);

    renderer->fillRect(RectF(0, 0, kSurfaceWidth, kSurfaceHeight), Fill{Colors::White});

    ICanvas& canvas = *renderer;
    ICanvas::TextureDraw td;
    td.dst = RectF(10, 10, 8, 16);
    td.tint = Colors::Red;
    canvas.drawTexture(texture, td);

    EXPECT_EQ(pixelAt(12, 12), red);
}

}  // namespace
