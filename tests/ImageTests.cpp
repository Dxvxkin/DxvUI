// Widget tests for the Image widget (stage 6b, docs/RENDERING_REFACTORING.md
// §4.9 / docs/AUTO_CACHE_BATCHING_PLAN.md §1): measure from texture/ImageData,
// fit modes in onPaint, tint/alpha/srcRect forwarding, clip balancing and lazy
// texture creation from ImageData once a backend is available.

#include <gtest/gtest.h>

#include <memory>
#include <string>
#include <vector>

#include "DxvUI/Log.h"
#include "DxvUI/Scene.h"
#include "DxvUI/SceneNode.h"
#include "DxvUI/core/ImageData.h"
#include "DxvUI/interfaces/ITexture.h"
#include "DxvUI/style/Colors.h"
#include "DxvUI/style/StyleManager.h"
#include "DxvUI/style/Theme.h"
#include "DxvUI/widgets/Image.h"
#include "FakeBackend.h"

using namespace DxvUI;

namespace {

// SceneNode's destructor logs via DxvUI::Log, which requires an initialized
// logger. Install a global test environment so the logger exists for the whole
// test binary.
class LoggerEnvironment : public ::testing::Environment {
   public:
    void SetUp() override { Log::init(); }
};

::testing::Environment* const g_logger_environment =
    ::testing::AddGlobalTestEnvironment(new LoggerEnvironment);

// A minimal texture with a known size.
class FakeTexture : public ITexture {
   public:
    FakeTexture(int w, int h) : w_(w), h_(h) {}
    int getWidth() const override { return w_; }
    int getHeight() const override { return h_; }

   private:
    int w_ = 0;
    int h_ = 0;
};

// A renderer stub that records the ICanvas calls the Image widget makes:
// drawTexture(TextureDraw) with dst/src/tint/alpha and the balanced clip pair.
// Stage 7 removed the legacy IRenderer god-interface, so it derives from the
// shared FakeBackend (IRenderBackend + ICanvas + IPlatformServices) and only
// overrides the entries the tests assert on.
class RecordingBackend : public FakeBackend {
   public:
    std::vector<ICanvas::TextureDraw> textureDraws;
    std::vector<RectF> clipPushes;
    int clipPops = 0;
    int createTextureCalls = 0;
    int lastCreatedWidth = 0;
    int lastCreatedHeight = 0;

    std::shared_ptr<ITexture> createTexture(const ImageData& data) override {
        ++createTextureCalls;
        lastCreatedWidth = data.width;
        lastCreatedHeight = data.height;
        return std::make_shared<FakeTexture>(data.width, data.height);
    }

    void pushClip(const RectF& rect) override { clipPushes.push_back(rect); }
    void popClip() override { ++clipPops; }
    void drawTexture(const std::shared_ptr<ITexture>&, const TextureDraw& draw) override {
        textureDraws.push_back(draw);
    }
};

// Runs a full style->layout->draw pass for an Image placed at (0,0) with the
// given box size, exactly like the scene pipeline would in an app.
void drawImage(Scene& scene, Image& image, RecordingBackend& backend, int w, int h) {
    image.setStyle({.left = 0, .top = 0, .width = w, .height = h}, WidgetState::Normal);
    scene.setRenderBackend(&backend);
    scene.updateLayout();
    scene.draw();
}

// Measures the Image's desired size under the given available constraints,
// using the same local StyleManager pattern as the other widget tests.
Size measureImage(const std::shared_ptr<SceneNode>& root, const Size& availableSize) {
    Theme theme;
    StyleManager manager{theme};
    manager.resolveDirtyStyles(root);
    root->measure(availableSize);
    return root->getChildren().front()->getDesiredSize();
}

}  // namespace

// --- Measure (desired size from texture / ImageData) ---

TEST(ImageMeasureTest, DesiredSizeIsTextureSizeWhenThereIsRoom) {
    auto scene = Scene::create();
    auto root = scene->getRoot();
    auto image = Image::create("img");
    image->setTexture(std::make_shared<FakeTexture>(160, 90));
    root->addChild(image);

    const Size desired = measureImage(root, {800, 600});
    EXPECT_FLOAT_EQ(desired.width, 160.0f);
    EXPECT_FLOAT_EQ(desired.height, 90.0f);
}

TEST(ImageMeasureTest, ContainClampsToAvailableSize) {
    auto scene = Scene::create();
    auto root = scene->getRoot();
    auto image = Image::create("img");
    image->setTexture(std::make_shared<FakeTexture>(160, 90));
    image->setFit(ImageFit::Contain);
    root->addChild(image);

    // 160x90 in 100x50: width-clamp first (ratio 100/160) -> 100x56.25, then
    // height-clamp (ratio 50/56.25) -> 88.8889x50.
    const Size desired = measureImage(root, {100, 50});
    EXPECT_NEAR(desired.width, 100.0f * 50.0f / 56.25f, 0.001f);
    EXPECT_FLOAT_EQ(desired.height, 50.0f);
}

TEST(ImageMeasureTest, ScaleDownShrinksOversizedButNeverUpscales) {
    auto scene = Scene::create();
    auto root = scene->getRoot();
    auto image = Image::create("img");
    image->setFit(ImageFit::ScaleDown);
    root->addChild(image);

    // Oversized texture: shrinks like Contain.
    image->setTexture(std::make_shared<FakeTexture>(300, 150));
    Size desired = measureImage(root, {200, 200});
    EXPECT_FLOAT_EQ(desired.width, 200.0f);
    EXPECT_FLOAT_EQ(desired.height, 100.0f);

    // Small texture in a large slot: stays at natural size.
    image->setTexture(std::make_shared<FakeTexture>(30, 20));
    desired = measureImage(root, {200, 200});
    EXPECT_FLOAT_EQ(desired.width, 30.0f);
    EXPECT_FLOAT_EQ(desired.height, 20.0f);
}

TEST(ImageMeasureTest, NoneFitIgnoresAvailableSize) {
    auto scene = Scene::create();
    auto root = scene->getRoot();
    auto image = Image::create("img");
    image->setTexture(std::make_shared<FakeTexture>(160, 90));
    image->setFit(ImageFit::None);
    root->addChild(image);

    const Size desired = measureImage(root, {100, 50});
    EXPECT_FLOAT_EQ(desired.width, 160.0f);
    EXPECT_FLOAT_EQ(desired.height, 90.0f);
}

TEST(ImageMeasureTest, NoTextureMeasuresZero) {
    auto scene = Scene::create();
    auto root = scene->getRoot();
    root->addChild(Image::create("img"));

    const Size desired = measureImage(root, {800, 600});
    EXPECT_FLOAT_EQ(desired.width, 0.0f);
    EXPECT_FLOAT_EQ(desired.height, 0.0f);
}

// --- Paint: fit modes produce the expected destination rect ---

namespace {

void expectDst(const RecordingBackend& backend, const RectF& expected) {
    ASSERT_EQ(backend.textureDraws.size(), 1u);
    EXPECT_FLOAT_EQ(backend.textureDraws[0].dst.x, expected.x);
    EXPECT_FLOAT_EQ(backend.textureDraws[0].dst.y, expected.y);
    EXPECT_FLOAT_EQ(backend.textureDraws[0].dst.width, expected.width);
    EXPECT_FLOAT_EQ(backend.textureDraws[0].dst.height, expected.height);
}

// The widget computes the fit rect through its own float chain, so exact
// equality against a re-derived expression can differ in the last ULP.
void expectDstNear(const RecordingBackend& backend, const RectF& expected) {
    ASSERT_EQ(backend.textureDraws.size(), 1u);
    EXPECT_NEAR(backend.textureDraws[0].dst.x, expected.x, 0.001f);
    EXPECT_NEAR(backend.textureDraws[0].dst.y, expected.y, 0.001f);
    EXPECT_NEAR(backend.textureDraws[0].dst.width, expected.width, 0.001f);
    EXPECT_NEAR(backend.textureDraws[0].dst.height, expected.height, 0.001f);
}

}  // namespace

TEST(ImagePaintTest, FillDrawsTheFullBounds) {
    auto scene = Scene::create();
    auto root = scene->getRoot();
    auto image = Image::create("img");
    image->setTexture(std::make_shared<FakeTexture>(160, 90));
    image->setFit(ImageFit::Fill);
    root->addChild(image);
    RecordingBackend backend;

    drawImage(*scene, *image, backend, 200, 100);
    expectDst(backend, RectF(0.0f, 0.0f, 200.0f, 100.0f));
}

TEST(ImagePaintTest, NoneDrawsNaturalSizeAtTopLeft) {
    auto scene = Scene::create();
    auto root = scene->getRoot();
    auto image = Image::create("img");
    image->setTexture(std::make_shared<FakeTexture>(160, 90));
    image->setFit(ImageFit::None);
    root->addChild(image);
    RecordingBackend backend;

    drawImage(*scene, *image, backend, 200, 100);
    expectDst(backend, RectF(0.0f, 0.0f, 160.0f, 90.0f));
}

TEST(ImagePaintTest, ContainFitsInsideBoundsCentered) {
    auto scene = Scene::create();
    auto root = scene->getRoot();
    auto image = Image::create("img");
    image->setTexture(std::make_shared<FakeTexture>(160, 90));
    image->setFit(ImageFit::Contain);
    root->addChild(image);
    RecordingBackend backend;

    drawImage(*scene, *image, backend, 200, 100);
    // 160x90 (aspect 1.78) inside 200x100 (aspect 2.0): height-constrained,
    // centered horizontally.
    const float w = 100.0f * 160.0f / 90.0f;
    expectDstNear(backend, RectF((200.0f - w) * 0.5f, 0.0f, w, 100.0f));
}

TEST(ImagePaintTest, CoverFillsBoundsAndCrops) {
    auto scene = Scene::create();
    auto root = scene->getRoot();
    auto image = Image::create("img");
    image->setTexture(std::make_shared<FakeTexture>(160, 90));
    image->setFit(ImageFit::Cover);
    root->addChild(image);
    RecordingBackend backend;

    drawImage(*scene, *image, backend, 200, 100);
    // Cover crops the excess vertical extent: full width, taller than the box,
    // centered vertically (negative y is clipped away).
    const float h = 200.0f * 90.0f / 160.0f;
    expectDst(backend, RectF(0.0f, (100.0f - h) * 0.5f, 200.0f, h));
}

TEST(ImagePaintTest, ScaleDownDrawsNaturalCenteredWhenSmallerThanBounds) {
    auto scene = Scene::create();
    auto root = scene->getRoot();
    auto image = Image::create("img");
    image->setTexture(std::make_shared<FakeTexture>(60, 40));
    image->setFit(ImageFit::ScaleDown);
    root->addChild(image);
    RecordingBackend backend;

    drawImage(*scene, *image, backend, 200, 100);
    expectDst(backend, RectF(70.0f, 30.0f, 60.0f, 40.0f));
}

// --- Paint: tint / alpha / source rect forwarding ---

TEST(ImagePaintTest, TintAlphaAndSourceRectAreForwarded) {
    auto scene = Scene::create();
    auto root = scene->getRoot();
    auto image = Image::create("img");
    image->setTexture(std::make_shared<FakeTexture>(160, 90));
    image->setFit(ImageFit::Fill);  // dst is pinned to the bounds, isolating tint/src/alpha
    image->setTint(Colors::Red);
    image->setAlpha(0.5f);
    image->setSourceRect(Rect{2, 3, 80, 45});
    root->addChild(image);
    RecordingBackend backend;

    drawImage(*scene, *image, backend, 200, 100);

    ASSERT_EQ(backend.textureDraws.size(), 1u);
    const auto& draw = backend.textureDraws[0];
    EXPECT_FLOAT_EQ(draw.dst.x, 0.0f);
    EXPECT_FLOAT_EQ(draw.dst.y, 0.0f);
    ASSERT_TRUE(draw.src.has_value());
    EXPECT_FLOAT_EQ(draw.src->x, 2.0f);
    EXPECT_FLOAT_EQ(draw.src->y, 3.0f);
    EXPECT_FLOAT_EQ(draw.src->width, 80.0f);
    EXPECT_FLOAT_EQ(draw.src->height, 45.0f);
    ASSERT_TRUE(draw.tint.has_value());
    EXPECT_EQ(draw.tint.value(), Colors::Red);
    EXPECT_FLOAT_EQ(draw.alpha, 0.5f);
}

// --- Paint: clipping when the destination exceeds the widget bounds ---

TEST(ImagePaintTest, ClipPushedWhenDstExceedsBounds) {
    auto scene = Scene::create();
    auto root = scene->getRoot();
    auto image = Image::create("img");
    image->setTexture(std::make_shared<FakeTexture>(160, 90));
    image->setFit(ImageFit::None);  // 160x90 does not fit 100x50 -> clip
    root->addChild(image);
    RecordingBackend backend;

    drawImage(*scene, *image, backend, 100, 50);

    ASSERT_EQ(backend.clipPushes.size(), 1u);
    EXPECT_FLOAT_EQ(backend.clipPushes[0].x, 0.0f);
    EXPECT_FLOAT_EQ(backend.clipPushes[0].y, 0.0f);
    EXPECT_FLOAT_EQ(backend.clipPushes[0].width, 100.0f);
    EXPECT_FLOAT_EQ(backend.clipPushes[0].height, 50.0f);
    EXPECT_EQ(backend.clipPops, 1);  // ClipGuard balances push with pop
}

TEST(ImagePaintTest, NoClipWhenDstStaysInsideBounds) {
    auto scene = Scene::create();
    auto root = scene->getRoot();
    auto image = Image::create("img");
    image->setTexture(std::make_shared<FakeTexture>(160, 90));
    image->setFit(ImageFit::Fill);  // dst == bounds -> no clip needed
    root->addChild(image);
    RecordingBackend backend;

    drawImage(*scene, *image, backend, 200, 100);

    EXPECT_TRUE(backend.clipPushes.empty());
    EXPECT_EQ(backend.clipPops, 0);
    ASSERT_EQ(backend.textureDraws.size(), 1u);
}

// --- ImageData: lazy texture creation ---

TEST(ImagePaintTest, ImageDataWithoutBackendDoesNotCrashAndCreatesTextureLazily) {
    auto scene = Scene::create();
    auto root = scene->getRoot();
    auto image = Image::create("img");
    image->setStyle({.left = 0, .top = 0, .width = 200, .height = 100}, WidgetState::Normal);
    image->setImageData(ImageData::createCheckerboard(160, 90, 10));
    root->addChild(image);
    RecordingBackend backend;

    // No backend attached yet: the layout/draw passes no-op and nothing crashes.
    scene->updateLayout();
    scene->draw();
    EXPECT_EQ(backend.createTextureCalls, 0);
    EXPECT_TRUE(backend.textureDraws.empty());

    // Attach a backend and draw: the texture is created from the pending data
    // and the Contain-fit draw lands as if a real texture had been set.
    scene->setRenderBackend(&backend);
    scene->updateLayout();
    scene->draw();

    EXPECT_EQ(backend.createTextureCalls, 1);
    EXPECT_EQ(backend.lastCreatedWidth, 160);
    EXPECT_EQ(backend.lastCreatedHeight, 90);
    ASSERT_EQ(backend.textureDraws.size(), 1u);
    // Default fit is Contain: the 160x90 texture is height-constrained inside
    // the 200x100 box and centered horizontally (same rect as the real-texture
    // Contain paint test above).
    const float w = 100.0f * 160.0f / 90.0f;
    expectDstNear(backend, RectF((200.0f - w) * 0.5f, 0.0f, w, 100.0f));
}