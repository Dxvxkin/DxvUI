#include <gtest/gtest.h>

#include <memory>

#include "DxvUI/Log.h"
#include "DxvUI/Scene.h"
#include "DxvUI/SceneNode.h"
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

// The EventManager is driven through the public Scene::processEvent() path. Hit
// testing works without a renderer because the fixture resolves styles with a
// local StyleManager and arranges the tree manually.
namespace {

struct EventFixture {
    std::shared_ptr<Scene> scene = Scene::create();
    std::shared_ptr<SceneNode> root = scene->getRoot();
    std::shared_ptr<Button> buttonA = Button::create("btn_a", "A");
    std::shared_ptr<Button> buttonB = Button::create("btn_b", "B");
    Theme theme;
    StyleManager manager{theme};

    EventFixture() {
        buttonA->setStyle({.left = 0, .top = 0, .width = 100, .height = 50}, WidgetState::Normal);
        buttonB->setStyle({.left = 200, .top = 0, .width = 100, .height = 50}, WidgetState::Normal);
        root->addChild(buttonA);
        root->addChild(buttonB);
        manager.resolveDirtyStyles(root);
        root->measure({800, 600});
        root->arrange({0, 0, 800, 600});
    }

    void pressAt(int x, int y) {
        DxvEvent e;
        e.type = EventType::MouseDown;
        e.mouse.x = x;
        e.mouse.y = y;
        e.mouse.button = MouseButton::Left;
        scene->processEvent(e);
    }

    void moveTo(int x, int y, MouseButton held) {
        DxvEvent e;
        e.type = EventType::MouseMove;
        e.mouse.x = x;
        e.mouse.y = y;
        e.mouse.button = held;
        scene->processEvent(e);
    }
};

}  // namespace

TEST(EventManagerTest, ButtonUpMissedPressIsClearedOnButtonlessMove) {
    EventFixture f;
    f.pressAt(50, 25);
    EXPECT_EQ(f.buttonA->getCurrentState(), WidgetState::Pressed);

    // Simulate the mouse leaving the window and coming back over empty space
    // with no button held (a button-up event that never arrived). Empty space is
    // used so the button is not re-hovered, which would legitimately change its
    // state to Hovered and hide what we are asserting (that Pressed is cleared).
    f.moveTo(400, 400, MouseButton::None);

    EXPECT_EQ(f.buttonA->getCurrentState(), WidgetState::Normal);
}

TEST(EventManagerTest, PressingAnotherNodeReleasesPreviousPress) {
    EventFixture f;
    f.pressAt(50, 25);
    EXPECT_EQ(f.buttonA->getCurrentState(), WidgetState::Pressed);

    f.pressAt(250, 25);
    EXPECT_EQ(f.buttonA->getCurrentState(), WidgetState::Normal);
    EXPECT_EQ(f.buttonB->getCurrentState(), WidgetState::Pressed);
}
