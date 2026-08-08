#include <gtest/gtest.h>

#include <memory>

#include "DxvUI/DxvEvent.h"
#include "DxvUI/Log.h"
#include "DxvUI/Scene.h"
#include "DxvUI/SceneNode.h"
#include "DxvUI/UIContext.h"
#include "DxvUI/style/StyleManager.h"
#include "DxvUI/style/Theme.h"
#include "DxvUI/widgets/Button.h"

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

}  // namespace

// The context reaches the handler through the real dispatch path: an attached
// node receives it with a live scene, a detached node with a null-safe one.
namespace {

struct ContextFixture {
    std::shared_ptr<Scene> scene = Scene::create();
    std::shared_ptr<SceneNode> root = scene->getRoot();
    std::shared_ptr<Button> buttonA = Button::create("btn_a", "A");
    std::shared_ptr<Button> buttonB = Button::create("btn_b", "B");
    Theme theme;
    StyleManager manager{theme};

    ContextFixture() {
        buttonA->setStyle({.left = 0, .top = 0, .width = 100, .height = 50}, WidgetState::Normal);
        buttonB->setStyle({.left = 200, .top = 0, .width = 100, .height = 50}, WidgetState::Normal);
        root->addChild(buttonA);
        root->addChild(buttonB);
        manager.resolveDirtyStyles(root);
        root->measure({800, 600});
        root->arrange({0, 0, 800, 600});
    }

    void clickAt(int x, int y) {
        DxvEvent down;
        down.type = EventType::MouseDown;
        down.mouse.x = x;
        down.mouse.y = y;
        down.mouse.button = MouseButton::Left;
        scene->processEvent(down);
        DxvEvent up;
        up.type = EventType::MouseUp;
        up.mouse.x = x;
        up.mouse.y = y;
        up.mouse.button = MouseButton::Left;
        scene->processEvent(up);
    }
};

}  // namespace

TEST(UIContextTest, HandlerReceivesSceneFacade) {
    ContextFixture f;
    std::shared_ptr<SceneNode> seenRoot;
    std::shared_ptr<SceneNode> found;
    auto conn = f.buttonA->on(EventType::Click, [&](DxvEvent&, const UIContext& ui) {
        seenRoot = ui.getRoot();
        found = ui.findNodeById("btn_b");
        EXPECT_EQ(ui.findNodeById("nope"), nullptr);
        // No renderer is set in the fixture, so viewport is empty; the scene
        // always carries a default theme.
        EXPECT_EQ(ui.getViewport(), Size());
        EXPECT_NE(ui.getTheme(), nullptr);
        EXPECT_EQ(ui.getRenderer(), nullptr);
    });

    f.clickAt(50, 25);

    EXPECT_EQ(seenRoot, f.root);
    EXPECT_EQ(found, f.buttonB);
}

TEST(UIContextTest, AttachHandlerReceivesValidContext) {
    auto scene = Scene::create();
    auto root = scene->getRoot();
    auto child = std::make_shared<SceneNode>("child");
    bool sawScene = false;
    auto conn = child->on(EventType::Attach, [&](DxvEvent&, const UIContext& ui) {
        // Attach is dispatched after the node joined the scene, so the context
        // must already be backed by it.
        sawScene = ui.getRoot() == root;
    });
    root->addChild(child);

    EXPECT_TRUE(sawScene);
}

TEST(UIContextTest, SetFocusMovesFocusAndDispatchesEvents) {
    ContextFixture f;
    bool gainedB = false;
    bool lostA = false;
    std::vector<std::unique_ptr<SceneNode::Connection>> conns;
    conns.push_back(f.buttonB->on(EventType::FocusGained,
                                  [&](DxvEvent&, const UIContext&) { gainedB = true; }));
    conns.push_back(
        f.buttonA->on(EventType::FocusLost, [&](DxvEvent&, const UIContext&) { lostA = true; }));
    // The handler on A moves focus to B via the context.
    conns.push_back(f.buttonA->on(EventType::Click,
                                  [&](DxvEvent&, const UIContext& ui) { ui.setFocus(f.buttonB); }));

    f.clickAt(50, 25);  // the press focuses A, the handler then moves focus to B

    EXPECT_EQ(f.scene->getFocusedNode(), f.buttonB);
    EXPECT_TRUE(gainedB);
    EXPECT_TRUE(lostA);

    // Clearing focus through the context dispatches FocusLost and drops the node.
    bool lostB = false;
    conns.push_back(
        f.buttonB->on(EventType::FocusLost, [&](DxvEvent&, const UIContext&) { lostB = true; }));
    conns.push_back(f.buttonA->on(EventType::Click,
                                  [&](DxvEvent&, const UIContext& ui) { ui.setFocus(nullptr); }));

    f.clickAt(50, 25);  // the second handler on A now clears focus

    EXPECT_EQ(f.scene->getFocusedNode(), nullptr);
    EXPECT_TRUE(lostB);
}

TEST(UIContextTest, ContextIsNullSafeOnDetachedNode) {
    auto node = std::make_shared<SceneNode>("orphan");
    bool called = false;
    auto conn = node->on(EventType::Click, [&](DxvEvent&, const UIContext& ui) {
        called = true;
        EXPECT_EQ(ui.getRoot(), nullptr);
        EXPECT_EQ(ui.findNodeById("x"), nullptr);
        EXPECT_EQ(ui.getFocusedNode(), nullptr);
        ui.setFocus(nullptr);  // no-op, must not crash
        ui.updateLayout();     // no-op, must not crash
        EXPECT_EQ(ui.getTheme(), nullptr);
        EXPECT_EQ(ui.getRenderer(), nullptr);
        EXPECT_EQ(ui.getViewport(), Size());
    });

    DxvEvent e;
    e.type = EventType::Click;
    e.target = node;
    node->dispatchEvent(e);

    EXPECT_TRUE(called);
}
