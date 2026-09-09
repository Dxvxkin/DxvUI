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

// The whole suite runs against an SDL software renderer bound to a plain
// surface: no window, no video subsystem, fully headless. SDLRenderer's
// constructor paths (cursor setup, TTF init) are safe without an initialized
// video driver: SDL_CreateSystemCursor() returns null and SDL_SetCursor(null)
// is a documented no-op when no driver provides cursors.
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
        // Destroy the DxvUI wrapper first: its destructor frees cached text
        // textures, which must happen while the SDL renderer is still alive.
        renderer.reset();
        SDL_DestroyRenderer(sdlRenderer);
        SDL_FreeSurface(surface);
    }

    // Reads a pixel of the surface the software renderer draws into. Flushes
    // the render command queue first so the read reflects every issued call.
    Uint32 pixelAt(int x, int y) {
        SDL_RenderFlush(sdlRenderer);
        Uint32 pixel = 0;
        std::memcpy(&pixel, static_cast<const Uint8*>(surface->pixels) +
                                y * surface->pitch + x * sizeof(Uint32),
                    sizeof(Uint32));
        return pixel;
    }

    Uint32 mapColor(const Color& color) const {
        // All colors used here are opaque, so the alpha byte is always 0xFF.
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
               << "clip is {" << actual.x << "," << actual.y << "," << actual.w << ","
               << actual.h << "}, expected {" << x << "," << y << "," << w << "," << h << "}";
    }

    SDL_Surface* surface = nullptr;
    SDL_Renderer* sdlRenderer = nullptr;
    std::unique_ptr<SDLRenderer> renderer;
};

TEST_F(SDLRendererTest, PushClipRectIntersectsWithCurrentClip) {
    // First push on a fresh renderer: clipping was disabled, so the exact
    // rect must be installed as-is.
    renderer->pushClipRect({10, 10, 100, 100});
    EXPECT_TRUE(SDL_RenderIsClipEnabled(sdlRenderer));
    EXPECT_TRUE(expectRectEq(currentClip(), 10, 10, 100, 100));

    // Second push must INTERSECT with the outer clip (SDL itself would
    // replace the rect, re-exposing pixels the outer clip had cut off).
    renderer->pushClipRect({60, 60, 100, 100});
    EXPECT_TRUE(expectRectEq(currentClip(), 60, 60, 50, 50));

    // Pops restore the previous clip exactly, down to "disabled".
    renderer->popClipRect();
    EXPECT_TRUE(expectRectEq(currentClip(), 10, 10, 100, 100));
    renderer->popClipRect();
    EXPECT_FALSE(SDL_RenderIsClipEnabled(sdlRenderer));
}

TEST_F(SDLRendererTest, NestedClipConstrainsDrawingToIntersection) {
    const Uint32 white = mapColor(Colors::White);
    const Uint32 red = mapColor(Colors::Red);
    const Uint32 green = mapColor(Colors::Green);
    const Uint32 blue = mapColor(Colors::Blue);

    renderer->fillRect({0, 0, kSurfaceWidth, kSurfaceHeight}, Colors::White);

    // Outer clip A = {10,10,100,100}: only A turns red.
    renderer->pushClipRect({10, 10, 100, 100});
    renderer->fillRect({0, 0, kSurfaceWidth, kSurfaceHeight}, Colors::Red);
    EXPECT_EQ(pixelAt(5, 5), white);      // outside A
    EXPECT_EQ(pixelAt(50, 50), red);      // inside A
    EXPECT_EQ(pixelAt(150, 150), white);  // outside A

    // Inner clip B = {60,60,100,100}: green only inside A∩B = {60,60,50,50}.
    renderer->pushClipRect({60, 60, 100, 100});
    renderer->fillRect({0, 0, kSurfaceWidth, kSurfaceHeight}, Colors::Green);
    EXPECT_EQ(pixelAt(70, 70), green);    // inside A∩B
    EXPECT_EQ(pixelAt(50, 50), red);      // inside A, outside B: must NOT be
                                          // repainted (B may not extend A's cut)
    EXPECT_EQ(pixelAt(150, 150), white);  // inside B, outside A: must NOT be
                                          // painted (A still constrains)

    // Pop back to A: the full A area becomes blue again.
    renderer->popClipRect();
    renderer->fillRect({0, 0, kSurfaceWidth, kSurfaceHeight}, Colors::Blue);
    EXPECT_EQ(pixelAt(5, 5), white);
    EXPECT_EQ(pixelAt(50, 50), blue);
    EXPECT_EQ(pixelAt(70, 70), blue);
    EXPECT_EQ(pixelAt(150, 150), white);

    // Pop everything: drawing is unconstrained again.
    renderer->popClipRect();
    renderer->fillRect({0, 0, kSurfaceWidth, kSurfaceHeight}, Colors::Black);
    EXPECT_EQ(pixelAt(150, 150), mapColor(Colors::Black));
}

TEST_F(SDLRendererTest, DisjointNestedClipClipsEverything) {
    const Uint32 white = mapColor(Colors::White);

    renderer->fillRect({0, 0, kSurfaceWidth, kSurfaceHeight}, Colors::White);
    renderer->pushClipRect({10, 10, 50, 50});
    renderer->pushClipRect({100, 100, 50, 50});  // no overlap with the outer clip

    // An empty intersection must clip everything away — not disable clipping
    // (which would un-lock the whole surface) and not leave a single
    // corner pixel of the inner clip drawable.
    EXPECT_TRUE(SDL_RenderIsClipEnabled(sdlRenderer));
    renderer->fillRect({0, 0, kSurfaceWidth, kSurfaceHeight}, Colors::Red);
    EXPECT_EQ(pixelAt(30, 30), white);    // inside the outer clip
    EXPECT_EQ(pixelAt(120, 120), white);  // inside the inner clip
    EXPECT_EQ(pixelAt(100, 100), white);  // the inner clip's corner pixel
    EXPECT_EQ(pixelAt(5, 5), white);      // outside both

    // After the pop the outer clip is effective again.
    renderer->popClipRect();
    renderer->fillRect({0, 0, kSurfaceWidth, kSurfaceHeight}, Colors::Green);
    EXPECT_EQ(pixelAt(30, 30), mapColor(Colors::Green));
    EXPECT_EQ(pixelAt(120, 120), white);
}

// A minimal ITexture implementation foreign to the SDL backend.
class FakeTexture : public ITexture {
   public:
    int getWidth() const override { return 4; }
    int getHeight() const override { return 4; }
};

TEST_F(SDLRendererTest, DrawTextureRejectsForeignTextureImplementation) {
    const Uint32 white = mapColor(Colors::White);
    renderer->fillRect({0, 0, kSurfaceWidth, kSurfaceHeight}, Colors::White);

    // A foreign ITexture must be a logged no-op, not a blind cast to
    // SDLTexture (which used to dereference a null SDL_Texture*).
    auto foreign = std::make_shared<FakeTexture>();
    renderer->drawTexture(foreign, {0, 0, 4, 4});
    // The const& parameter also accepts temporaries (impossible to pass
    // through the old non-const lvalue reference).
    renderer->drawTexture(std::make_shared<FakeTexture>(), {0, 0, 4, 4});
    EXPECT_EQ(pixelAt(1, 1), white);

    // A null texture stays a silent no-op.
    renderer->drawTexture(nullptr, {0, 0, 4, 4});
    EXPECT_EQ(pixelAt(1, 1), white);
}

TEST_F(SDLRendererTest, DrawTextureRendersTexturePixels) {
    // A texture created against the same SDL renderer, wrapped in the
    // backend's own SDLTexture (the same wrapping the text engine performs).
    SDL_Texture* raw = SDL_CreateTexture(sdlRenderer, SDL_PIXELFORMAT_RGBA32,
                                         SDL_TEXTUREACCESS_STATIC, 4, 4);
    ASSERT_NE(raw, nullptr);
    std::array<Uint32, 16> pixels{};
    pixels.fill(mapColor(Colors::Red));
    ASSERT_EQ(SDL_UpdateTexture(raw, nullptr, pixels.data(), 4 * sizeof(Uint32)), 0);
    auto texture = std::make_shared<SDLTexture>(raw);

    renderer->fillRect({0, 0, kSurfaceWidth, kSurfaceHeight}, Colors::White);
    renderer->drawTexture(texture, {10, 10, 4, 4});

    EXPECT_EQ(pixelAt(12, 12), mapColor(Colors::Red));    // inside dstRect
    EXPECT_EQ(pixelAt(12, 16), mapColor(Colors::White));  // below dstRect
}

// Cache-bound checks for the real SDL_ttf engine. They need a loadable
// platform font; when none is available the test skips instead of failing.
TEST_F(SDLRendererTest, TextEngineCachesAreLruBounded) {
    SDLTextEngine engine(sdlRenderer);
    auto font = engine.getFontForFamily("Sans", 16);
    if (!font) {
        GTEST_SKIP() << "no loadable default font on this platform";
    }

    // Measurement cache: exceed the cap with unique strings, without paying
    // for rasterization on each of them.
    for (size_t i = 0; i < SDLTextEngine::kMaxMeasureCacheEntries + 512; ++i) {
        engine.measure(*font, "measure-" + std::to_string(i));
    }
    EXPECT_LE(engine.getMeasureCacheCount(), SDLTextEngine::kMaxMeasureCacheEntries);

    // Texture cache: the same via rasterize(). The count must plateau at the
    // bound instead of growing with the number of unique keys.
    for (size_t i = 0; i < SDLTextEngine::kMaxTextureCacheEntries + 64; ++i) {
        engine.rasterize(*font, "raster-" + std::to_string(i), Colors::Black);
    }
    EXPECT_LE(engine.getTextureCacheCount(), SDLTextEngine::kMaxTextureCacheEntries);

    // clearCaches() drops everything at once.
    engine.clearCaches();
    EXPECT_EQ(engine.getMeasureCacheCount(), 0u);
    EXPECT_EQ(engine.getTextureCacheCount(), 0u);
}

}  // namespace
