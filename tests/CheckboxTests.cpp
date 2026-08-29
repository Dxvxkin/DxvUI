#include <gtest/gtest.h>

#include <memory>
#include <string>

#include "DxvUI/DxvEvent.h"
#include "DxvUI/Log.h"
#include "DxvUI/Scene.h"
#include "DxvUI/SceneNode.h"
#include "DxvUI/style/StyleManager.h"
#include "DxvUI/style/Theme.h"
#include "DxvUI/widgets/Checkbox.h"

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

namespace {

struct CheckboxFixture {
    std::shared_ptr<Scene> scene = Scene::create();
    std::shared_ptr<SceneNode> root = scene->getRoot();
    std::shared_ptr<Checkbox> checkbox = Checkbox::create("cb", "Option");
    Theme theme;
    StyleManager manager{theme};

    CheckboxFixture() {
        checkbox->setStyle({.left = 0, .top = 0, .width = 100, .height = 50}, WidgetState::Normal);
        root->addChild(checkbox);
        manager.resolveDirtyStyles(root);
        root->measure({800, 600});
        root->arrange({0, 0, 800, 600});
    }

    void moveTo(int x, int y) {
        DxvEvent e;
        e.type = EventType::MouseMove;
        e.mouse.x = x;
        e.mouse.y = y;
        e.mouse.button = MouseButton::None;
        scene->processEvent(e);
    }

    void pressAt(int x, int y) {
        DxvEvent e;
        e.type = EventType::MouseDown;
        e.mouse.x = x;
        e.mouse.y = y;
        e.mouse.button = MouseButton::Left;
        scene->processEvent(e);
    }

    void releaseAt(int x, int y) {
        DxvEvent e;
        e.type = EventType::MouseUp;
        e.mouse.x = x;
        e.mouse.y = y;
        e.mouse.button = MouseButton::Left;
        scene->processEvent(e);
    }

    void clickAt(int x, int y) {
        moveTo(x, y);
        pressAt(x, y);
        releaseAt(x, y);
    }

    void pressSpace() {
        DxvEvent e;
        e.type = EventType::KeyDown;
        e.key.sym = KeyCode::Space;
        scene->processEvent(e);
    }
};

}  // namespace

TEST(CheckboxTest, DefaultIsUnchecked) {
    CheckboxFixture f;
    EXPECT_FALSE(f.checkbox->isChecked());
    EXPECT_FALSE(f.checkbox->getBinding()->getBoolOr(false));
}

TEST(CheckboxTest, SetCheckedUpdatesState) {
    CheckboxFixture f;
    f.checkbox->setChecked(true);
    EXPECT_TRUE(f.checkbox->isChecked());
    EXPECT_TRUE(f.checkbox->getBinding()->getBoolOr(false));

    f.checkbox->setChecked(false);
    EXPECT_FALSE(f.checkbox->isChecked());
}

TEST(CheckboxTest, ClickTogglesState) {
    CheckboxFixture f;
    f.clickAt(50, 25);
    EXPECT_TRUE(f.checkbox->isChecked());

    f.clickAt(50, 25);
    EXPECT_FALSE(f.checkbox->isChecked());
}

TEST(CheckboxTest, ToggleDispatchesChangeEvent) {
    CheckboxFixture f;
    int changeCount = 0;
    auto conn = f.root->on(EventType::Change, [&](DxvEvent&, const UIContext&) { ++changeCount; });

    f.clickAt(50, 25);
    EXPECT_EQ(changeCount, 1);

    f.clickAt(50, 25);
    EXPECT_EQ(changeCount, 2);
}

TEST(CheckboxTest, SpaceTogglesWhenFocused) {
    CheckboxFixture f;
    f.clickAt(50, 25);  // клик ставит фокус и включает чекбокс
    EXPECT_EQ(f.checkbox->getCurrentState(), WidgetState::Focused);
    EXPECT_TRUE(f.checkbox->isChecked());

    f.pressSpace();
    EXPECT_FALSE(f.checkbox->isChecked());
}

TEST(CheckboxTest, SpaceIgnoredWithoutFocus) {
    CheckboxFixture f;
    f.pressSpace();
    EXPECT_FALSE(f.checkbox->isChecked());
}

TEST(CheckboxTest, PreventDefaultCancelsToggle) {
    CheckboxFixture f;
    auto conn =
        f.checkbox->on(EventType::Click, [](DxvEvent& e, const UIContext&) { e.preventDefault(); });

    f.clickAt(50, 25);
    EXPECT_FALSE(f.checkbox->isChecked());
}

TEST(CheckboxTest, ClickDoesNotBubbleToParent) {
    CheckboxFixture f;
    int clickCount = 0;
    auto conn = f.root->on(EventType::Click, [&](DxvEvent&, const UIContext&) { ++clickCount; });

    f.clickAt(50, 25);
    EXPECT_EQ(clickCount, 0);
}
