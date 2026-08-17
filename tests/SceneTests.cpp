#include <gtest/gtest.h>

#include <memory>
#include <string>

#include "DxvUI/Log.h"
#include "DxvUI/Scene.h"
#include "DxvUI/SceneNode.h"
#include "DxvUI/containers/AbsoluteContainer.h"
#include "DxvUI/interfaces/IRenderer.h"

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

// A renderer stub so the Scene lifecycle can be exercised without a real SDL
// backend; mirrors the stub used by the widget tests.
class FakeTextEngine : public ITextEngine {
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
};

class FakeClipboard : public IClipboard {
   public:
    std::string text;
    std::string getText() override { return text; }
    bool setText(const std::string& t) override {
        text = t;
        return true;
    }
};

class FakeRenderer : public IRenderer {
   public:
    FakeClipboard clipboard;

    void clear(const Color&) override {}
    void present() override {}
    Size getViewportSize() const override { return {800, 600}; }

    void setCursor(CursorType) override {}
    CursorType getCursor() const override { return CursorType::Arrow; }

    void pushClipRect(const Rect&) override {}
    void popClipRect() override {}

    ITextEngine& getTextEngine() override { return textEngine; }
    IClipboard& getClipboard() override { return clipboard; }

    void drawTexture(std::shared_ptr<ITexture>&, const Rect&) override {}

    void setDrawColor(const Color&) override {}
    Color getDrawColor() const override { return {}; }

    void drawRect(const Rect&) override {}
    void fillRect(const Rect&) override {}
    void drawRect(const Rect&, const Color&) override {}
    void fillRect(const Rect&, const Color&) override {}
    void drawRect(const Rect&, const Border&) override {}
    void fillRect(const Rect&, const Color&, const Border&) override {}

    void drawLine(int, int, int, int) override {}
    void drawLine(int, int, int, int, const Color&) override {}

    void drawCircle(int, int, int) override {}
    void fillCircle(int, int, int) override {}
    void drawCircle(int, int, int, const Color&) override {}
    void fillCircle(int, int, int, const Color&) override {}
    void drawCircle(int, int, int, const Border&) override {}
    void fillCircle(int, int, int, const Color&, const Border&) override {}

    void drawArc(int, int, int, float, float) override {}
    void drawArc(int, int, int, float, float, const Color&) override {}
    void drawArc(int, int, int, float, float, const Border&) override {}

    void drawRoundRect(const Rect&, int) override {}
    void fillRoundRect(const Rect&, int) override {}
    void drawRoundRect(const Rect&, int, const Color&) override {}
    void fillRoundRect(const Rect&, int, const Color&) override {}
    void drawRoundRect(const Rect&, int, const Border&) override {}
    void fillRoundRect(const Rect&, int, const Color&, const Border&) override {}

    void drawPolygon(const std::vector<PointI>&) override {}
    void fillPolygon(const std::vector<PointI>&) override {}
    void drawPolygon(const std::vector<PointI>&, const Color&) override {}
    void fillPolygon(const std::vector<PointI>&, const Color&) override {}
    void drawPolygon(const std::vector<PointI>&, const Border&) override {}
    void fillPolygon(const std::vector<PointI>&, const Color&, const Border&) override {}

    FakeTextEngine textEngine;
};

// Counts draw invocations so tests can observe whether Scene::draw reached the
// tree. Counts in drawContent() — the virtual hook invoked on every node that
// passed visibility and viewport culling — because the recursive traversal
// (drawImpl) is non-virtual and calls child->drawImpl() directly, bypassing an
// override of the public draw(). Inherits AbsoluteContainer so children are
// actually arranged and reachable by the draw traversal.
class CountingNode : public AbsoluteContainer {
   public:
    explicit CountingNode(std::string id) : AbsoluteContainer(std::move(id)) {}
    int drawCalls = 0;

   protected:
    void drawContent(IRenderer& renderer) override {
        ++drawCalls;
        AbsoluteContainer::drawContent(renderer);
    }
};

}  // namespace

TEST(SceneTest, SetRootReplacesAndDetachesOldRoot) {
    auto scene = Scene::create();
    auto oldRoot = scene->getRoot();
    auto oldChild = std::make_shared<SceneNode>("old_child");
    oldRoot->addChild(oldChild);

    auto newRoot = std::make_shared<SceneNode>("new_root");
    scene->setRoot(newRoot);

    EXPECT_EQ(scene->getRoot(), newRoot);
    EXPECT_EQ(scene->findNodeById("new_root"), newRoot);
    // The old tree was detached: it no longer belongs to the scene.
    EXPECT_EQ(oldRoot->getScene(), nullptr);
    EXPECT_EQ(oldChild->getScene(), nullptr);
    EXPECT_EQ(scene->findNodeById("old_child"), nullptr);
}

TEST(SceneTest, SetRootNullIsSafe) {
    auto scene = Scene::create();
    scene->setRoot(nullptr);
    EXPECT_EQ(scene->getRoot(), nullptr);
    // No-op calls on a rootless scene must not crash.
    scene->update();
    scene->updateLayout();
    scene->draw();
    scene->shutdown();
}

TEST(SceneTest, UpdateLayoutIsNoopWithoutRenderer) {
    auto scene = Scene::create();
    auto child = std::make_shared<SceneNode>("child");
    child->setStyle({.left = 5, .top = 6, .width = 50, .height = 40}, WidgetState::Normal);
    scene->getRoot()->addChild(child);

    EXPECT_EQ(scene->getRenderer(), nullptr);
    scene->updateLayout();

    // Layout never ran, so the node has no bounds yet.
    const Rect bounds = child->getGlobalBounds();
    EXPECT_EQ(bounds.width, 0);
    EXPECT_EQ(bounds.height, 0);
}

TEST(SceneTest, UpdateLayoutRunsStyleResolutionAndLayout) {
    auto scene = Scene::create();
    FakeRenderer renderer;
    scene->setRenderer(&renderer);

    auto child = std::make_shared<SceneNode>("child");
    child->setStyle({.left = 5, .top = 6, .width = 50, .height = 40}, WidgetState::Normal);
    scene->getRoot()->addChild(child);

    scene->updateLayout();

    const Rect bounds = child->getGlobalBounds();
    EXPECT_EQ(bounds.x, 5);
    EXPECT_EQ(bounds.y, 6);
    EXPECT_EQ(bounds.width, 50);
    EXPECT_EQ(bounds.height, 40);
}

TEST(SceneTest, UpdateTriggersLayoutPass) {
    auto scene = Scene::create();
    FakeRenderer renderer;
    scene->setRenderer(&renderer);

    auto child = std::make_shared<SceneNode>("child");
    child->setStyle({.left = 10, .top = 10, .width = 30, .height = 20}, WidgetState::Normal);
    scene->getRoot()->addChild(child);

    scene->update();

    const Rect bounds = child->getGlobalBounds();
    EXPECT_EQ(bounds.x, 10);
    EXPECT_EQ(bounds.y, 10);
    EXPECT_EQ(bounds.width, 30);
    EXPECT_EQ(bounds.height, 20);
}

TEST(SceneTest, DrawRendersWholeSubtree) {
    auto scene = Scene::create();
    auto root = scene->getRoot();
    auto child = std::make_shared<CountingNode>("child");
    child->setStyle({.left = 0, .top = 0, .width = 100, .height = 50}, WidgetState::Normal);
    auto grandchild = std::make_shared<CountingNode>("grandchild");
    grandchild->setStyle({.left = 0, .top = 0, .width = 50, .height = 50}, WidgetState::Normal);
    child->addChild(grandchild);
    root->addChild(child);

    FakeRenderer renderer;
    scene->setRenderer(&renderer);
    scene->updateLayout();

    scene->draw();
    EXPECT_EQ(child->drawCalls, 1);
    EXPECT_EQ(grandchild->drawCalls, 1);
}

TEST(SceneTest, DrawIsNoopWithoutRendererOrRoot) {
    auto scene = Scene::create();
    auto root = std::make_shared<CountingNode>("root");
    root->setStyle({.left = 0, .top = 0, .width = 100, .height = 50}, WidgetState::Normal);
    scene->setRoot(root);

    // No renderer set: draw() must not touch the tree.
    scene->draw();
    EXPECT_EQ(root->drawCalls, 0);

    FakeRenderer renderer;
    scene->setRenderer(&renderer);
    scene->shutdown();

    // Root was dropped: draw() must be a no-op on a rootless scene.
    scene->draw();
    EXPECT_EQ(root->drawCalls, 0);
}

TEST(SceneTest, ShutdownClearsRoot) {
    auto scene = Scene::create();
    auto root = scene->getRoot();
    ASSERT_NE(scene->getRoot(), nullptr);

    scene->shutdown();
    EXPECT_EQ(scene->getRoot(), nullptr);
    EXPECT_EQ(root->getScene(), nullptr);

    // Shutdown is idempotent.
    scene->shutdown();
    EXPECT_EQ(scene->getRoot(), nullptr);
}

TEST(SceneTest, SetRendererRoundTrips) {
    auto scene = Scene::create();
    EXPECT_EQ(scene->getRenderer(), nullptr);

    FakeRenderer renderer;
    scene->setRenderer(&renderer);
    EXPECT_EQ(scene->getRenderer(), &renderer);

    scene->setRenderer(nullptr);
    EXPECT_EQ(scene->getRenderer(), nullptr);
}