#include <gtest/gtest.h>

#include <memory>
#include <string>

#include "DxvUI/Log.h"
#include "DxvUI/Scene.h"
#include "DxvUI/SceneNode.h"
#include "DxvUI/containers/AbsoluteContainer.h"
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

// Counts draw invocations so tests can observe whether Scene::draw reached the
// tree. Counts in onPaint() — the virtual hook invoked on every node that
// passed visibility and viewport culling — because the recursive traversal
// (drawImpl) is non-virtual and calls child->drawImpl() directly, bypassing
// the public draw() entry points. Inherits AbsoluteContainer so children are
// actually arranged and reachable by the draw traversal.
class CountingNode : public AbsoluteContainer {
   public:
    explicit CountingNode(std::string id) : AbsoluteContainer(std::move(id)) {}
    int drawCalls = 0;

   protected:
    void onPaint(PaintContext& pc) override {
        ++drawCalls;
        AbsoluteContainer::onPaint(pc);
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

    EXPECT_EQ(scene->getRenderBackend(), nullptr);
    scene->updateLayout();

    // Layout never ran, so the node has no bounds yet.
    const Rect bounds = child->getGlobalBounds();
    EXPECT_EQ(bounds.width, 0);
    EXPECT_EQ(bounds.height, 0);
}

TEST(SceneTest, UpdateLayoutRunsStyleResolutionAndLayout) {
    auto scene = Scene::create();
    FakeBackend renderer;
    scene->setRenderBackend(&renderer);

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
    FakeBackend renderer;
    scene->setRenderBackend(&renderer);

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

    FakeBackend renderer;
    scene->setRenderBackend(&renderer);
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

    FakeBackend renderer;
    scene->setRenderBackend(&renderer);
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

TEST(SceneTest, SetRenderBackendRoundTrips) {
    auto scene = Scene::create();
    EXPECT_EQ(scene->getRenderBackend(), nullptr);

    FakeBackend renderer;
    scene->setRenderBackend(&renderer);
    EXPECT_EQ(scene->getRenderBackend(), &renderer);

    scene->setRenderBackend(nullptr);
    EXPECT_EQ(scene->getRenderBackend(), nullptr);
}