#include <gtest/gtest.h>

#include <memory>
#include <string>

#include "DxvUI/DxvEvent.h"
#include "DxvUI/Log.h"
#include "DxvUI/Scene.h"
#include "DxvUI/SceneNode.h"
#include "DxvUI/style/StyleManager.h"
#include "DxvUI/style/Theme.h"
#include "DxvUI/widgets/SliderHorizontal.h"
#include "DxvUI/widgets/SliderVertical.h"

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

// Drives slider value/interaction through the public Scene::processEvent() path,
// the same way the Checkbox tests do. Zero padding keeps the content area equal
// to the widget's bounds so value mapping is easy to compute.
template <typename SliderT>
struct SliderFixtureBase {
    std::shared_ptr<Scene> scene = Scene::create();
    std::shared_ptr<SceneNode> root = scene->getRoot();
    std::shared_ptr<SliderT> slider;
    Theme theme;
    StyleManager manager{theme};

    SliderFixtureBase(float min, float max, float step)
        : slider(SliderT::create("slider", min, max, step)) {
        slider->setStyle(
            {.left = 0, .top = 0, .width = 200, .height = 30, .padding = {{0, 0, 0, 0}}},
            WidgetState::Normal);
        root->addChild(slider);
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

    void dragTo(int x, int y) {
        DxvEvent e;
        e.type = EventType::MouseMove;
        e.mouse.x = x;
        e.mouse.y = y;
        e.mouse.button = MouseButton::Left;
        scene->processEvent(e);
    }

    void wheel(int dy) {
        DxvEvent e;
        e.type = EventType::MouseWheel;
        e.mouse.dy = dy;
        scene->processEvent(e);
    }

    void key(KeyCode sym, uint16_t mod = KeyModifier::None) {
        DxvEvent e;
        e.type = EventType::KeyDown;
        e.key.sym = sym;
        e.key.mod = mod;
        scene->processEvent(e);
    }

    // Click the middle of the widget to give it keyboard/mouse-wheel focus
    // without perturbing the value, then pin the value to a known point.
    void focusClean(float value) {
        pressAt(100, 15);
        slider->setValue(value);
    }
};

using Fixture = SliderFixtureBase<SliderHorizontal>;

}  // namespace

TEST(SliderTest, CreateDefaultSliderHorizontal) {
    Fixture f(0, 1, 0);
    EXPECT_FLOAT_EQ(f.slider->getMin(), 0.0f);
    EXPECT_FLOAT_EQ(f.slider->getMax(), 1.0f);
    EXPECT_FLOAT_EQ(f.slider->getStep(), 0.0f);
    EXPECT_FLOAT_EQ(f.slider->getValue(), 0.0f);
    EXPECT_STREQ(f.slider->getNodeType(), "SliderHorizontal");
}

TEST(SliderTest, CreateSliderVertical) {
    SliderFixtureBase<SliderVertical> f(0, 100, 5);
    EXPECT_FLOAT_EQ(f.slider->getMin(), 0.0f);
    EXPECT_FLOAT_EQ(f.slider->getMax(), 100.0f);
    EXPECT_FLOAT_EQ(f.slider->getStep(), 5.0f);
    EXPECT_FLOAT_EQ(f.slider->getValue(), 0.0f);
    EXPECT_STREQ(f.slider->getNodeType(), "SliderVertical");
}

TEST(SliderTest, SetValueClampsToRange) {
    Fixture f(0, 1, 0);
    f.slider->setValue(0.5f);
    EXPECT_FLOAT_EQ(f.slider->getValue(), 0.5f);

    f.slider->setValue(5.0f);
    EXPECT_FLOAT_EQ(f.slider->getValue(), 1.0f);

    f.slider->setValue(-1.0f);
    EXPECT_FLOAT_EQ(f.slider->getValue(), 0.0f);
}

TEST(SliderTest, StepSnapsValue) {
    Fixture f(0, 1, 0.25f);
    f.slider->setValue(0.3f);
    EXPECT_FLOAT_EQ(f.slider->getValue(), 0.25f);

    f.slider->setValue(0.9f);
    EXPECT_FLOAT_EQ(f.slider->getValue(), 1.0f);

    f.slider->setValue(0.49f);
    EXPECT_FLOAT_EQ(f.slider->getValue(), 0.5f);
}

TEST(SliderTest, SetRangeAdjustsValue) {
    Fixture f(0, 10, 0);
    f.slider->setValue(5.0f);

    f.slider->setRange(0, 5);
    EXPECT_FLOAT_EQ(f.slider->getMax(), 5.0f);
    EXPECT_FLOAT_EQ(f.slider->getValue(), 5.0f);

    f.slider->setRange(2, 4);
    EXPECT_FLOAT_EQ(f.slider->getMin(), 2.0f);
    EXPECT_FLOAT_EQ(f.slider->getValue(), 4.0f);
}

TEST(SliderTest, MouseWheelIncreasesByStep) {
    Fixture f(0, 1, 0.1f);
    f.focusClean(0.0f);

    f.wheel(1);
    EXPECT_FLOAT_EQ(f.slider->getValue(), 0.1f);

    f.wheel(-1);
    EXPECT_FLOAT_EQ(f.slider->getValue(), 0.0f);
}

TEST(SliderTest, MouseWheelUsesTenPercentOfRange) {
    Fixture f(0, 10, 0);
    f.focusClean(0.0f);

    f.wheel(1);
    EXPECT_FLOAT_EQ(f.slider->getValue(), 1.0f);
}

TEST(SliderTest, MouseWheelVerticalGrowsUpward) {
    SliderFixtureBase<SliderVertical> f(0, 1, 0.1f);
    f.focusClean(0.0f);

    f.wheel(1);
    EXPECT_FLOAT_EQ(f.slider->getValue(), 0.1f);
}

TEST(SliderTest, KeyboardStep) {
    Fixture f(0, 10, 0);
    f.focusClean(0.0f);

    f.key(KeyCode::Right);
    EXPECT_FLOAT_EQ(f.slider->getValue(), 0.5f);

    f.key(KeyCode::Left);
    EXPECT_FLOAT_EQ(f.slider->getValue(), 0.0f);
}

TEST(SliderTest, KeyboardShiftFiner) {
    Fixture f(0, 10, 0);
    f.focusClean(0.0f);

    f.key(KeyCode::Right, KeyModifier::Shift);
    EXPECT_FLOAT_EQ(f.slider->getValue(), 0.05f);
}

TEST(SliderTest, KeyboardUpDownForVertical) {
    SliderFixtureBase<SliderVertical> f(0, 10, 0);
    f.focusClean(0.0f);

    f.key(KeyCode::Up);
    EXPECT_FLOAT_EQ(f.slider->getValue(), 0.5f);

    f.key(KeyCode::Down);
    EXPECT_FLOAT_EQ(f.slider->getValue(), 0.0f);
}

TEST(SliderTest, FindNodeAtInBounds) {
    Fixture f(0, 1, 0);
    EXPECT_EQ(f.root->findNodeAt(100, 15).get(), f.slider.get());
}

TEST(SliderTest, FindNodeAtOutOfBounds) {
    Fixture f(0, 1, 0);
    EXPECT_NE(f.root->findNodeAt(220, 15).get(), f.slider.get());
}

TEST(SliderTest, ChangeDispatchedOnValueChange) {
    Fixture f(0, 1, 0);
    int changeCount = 0;
    auto conn = f.root->on(EventType::Change, [&](DxvEvent&, const UIContext&) { ++changeCount; });

    f.slider->setValue(0.5f);
    EXPECT_EQ(changeCount, 1);

    // Setting the same value is a no-op and must not dispatch.
    f.slider->setValue(0.5f);
    EXPECT_EQ(changeCount, 1);
}

TEST(SliderTest, DragUpdatesValueAndDoesNotJump) {
    Fixture f(0, 1, 0);
    // Put the thumb in the middle (value 0.5) and grab it directly: grabbing
    // must not make it jump under the pointer.
    f.slider->setValue(0.5f);
    f.pressAt(100, 15);
    EXPECT_FLOAT_EQ(f.slider->getValue(), 0.5f);

    // Dragging +50px moves the thumb +50px along the 200px track.
    f.dragTo(150, 15);
    EXPECT_FLOAT_EQ(f.slider->getValue(), 0.75f);

    f.dragTo(50, 15);
    EXPECT_FLOAT_EQ(f.slider->getValue(), 0.25f);
}

TEST(SliderTest, DisabledIgnoresInput) {
    Fixture f(0, 1, 0.25f);
    f.slider->setValue(0.0f);
    f.slider->setEnabled(false);

    f.wheel(1);
    EXPECT_FLOAT_EQ(f.slider->getValue(), 0.0f);

    f.key(KeyCode::Right);
    EXPECT_FLOAT_EQ(f.slider->getValue(), 0.0f);
}
