#include <gtest/gtest.h>

#include <memory>
#include <string>
#include <utility>

#include "DxvUI/Log.h"
#include "DxvUI/SceneNode.h"
#include "DxvUI/containers/AbsoluteContainer.h"
#include "DxvUI/layout/LayoutManager.h"
#include "DxvUI/style/Colors.h"
#include "DxvUI/style/StyleManager.h"
#include "DxvUI/style/Theme.h"

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

// A widget that counts how many times the layout pass actually re-measured and
// re-arranged it, so tests can assert that only real layout changes trigger work.
class CountingWidget : public SceneNode {
   public:
    explicit CountingWidget(std::string id) : SceneNode(std::move(id)) {}

    int measureCalls = 0;
    int arrangeCalls = 0;

    const char* getNodeType() const override { return "CountingWidget"; }

   protected:
    Size onMeasure(const Size& /*availableSize*/) override {
        measureCalls++;
        return {0, 0};
    }
    void onArrange(const Rect& /*finalRect*/) override { arrangeCalls++; }
};

struct LayoutFixture {
    std::shared_ptr<AbsoluteContainer> root;
    std::shared_ptr<CountingWidget> child;
    LayoutManager layout;
    Theme theme;
    StyleManager styleManager{theme};

    LayoutFixture()
        : root(std::make_shared<AbsoluteContainer>("root")),
          child(std::make_shared<CountingWidget>("child")) {
        root->addChild(child);
        styleManager.resolveDirtyStyles(root);
    }

    void run(const Size& viewport = {800, 600}) {
        styleManager.resolveDirtyStyles(root);
        layout.layout(root, viewport);
    }
};

}  // namespace

TEST(LayoutManagerTest, FirstLayoutMeasuresAndArranges) {
    LayoutFixture f;
    f.run();

    EXPECT_EQ(f.child->measureCalls, 1);
    EXPECT_EQ(f.child->arrangeCalls, 1);
}

TEST(LayoutManagerTest, RepeatedLayoutIsNoop) {
    LayoutFixture f;
    f.run();
    f.run();

    EXPECT_EQ(f.child->measureCalls, 1);
    EXPECT_EQ(f.child->arrangeCalls, 1);
}

TEST(LayoutManagerTest, AppearanceStyleChangeDoesNotRelayout) {
    LayoutFixture f;
    f.run();
    const int measures = f.child->measureCalls;
    const int arranges = f.child->arrangeCalls;

    // A color change must re-resolve the style but must not touch the layout.
    f.child->setStyle({.backgroundColor = Colors::Red}, WidgetState::Normal);
    f.run();

    EXPECT_EQ(f.child->measureCalls, measures);
    EXPECT_EQ(f.child->arrangeCalls, arranges);
}

TEST(LayoutManagerTest, LayoutPropertyStyleChangeTriggersRelayout) {
    LayoutFixture f;
    f.run();
    const int measures = f.child->measureCalls;

    f.child->setStyle({.width = 100}, WidgetState::Normal);
    f.run();

    EXPECT_GT(f.child->measureCalls, measures);
}

TEST(LayoutManagerTest, ViewportResizeTriggersRelayout) {
    LayoutFixture f;
    f.run();
    const int measures = f.child->measureCalls;

    // A changed viewport changes the constraints handed down the tree, so even
    // a clean subtree must be re-measured.
    f.run({1000, 700});

    EXPECT_GT(f.child->measureCalls, measures);
}

TEST(LayoutManagerTest, InvisibleNodeHasZeroBounds) {
    LayoutFixture f;
    f.run();

    f.child->setVisible(false);
    f.run();

    const Rect bounds = f.child->getGlobalBounds();
    EXPECT_EQ(bounds.width, 0);
    EXPECT_EQ(bounds.height, 0);
}

TEST(LayoutManagerTest, ThemeAppearanceChangeDoesNotRelayout) {
    Theme::registerDefaultStyle("CountingWidget", {{WidgetState::Normal, {.borderThickness = 1}}});

    LayoutFixture f;
    f.run();
    const int measures = f.child->measureCalls;

    f.theme.setDefaultStyle("CountingWidget", {{WidgetState::Normal, {.borderThickness = 9}}});
    f.run();

    EXPECT_EQ(f.child->measureCalls, measures);
}

TEST(LayoutManagerTest, ThemeLayoutChangeTriggersRelayout) {
    Theme::registerDefaultStyle("CountingWidget", {{WidgetState::Normal, {.width = 50}}});

    LayoutFixture f;
    f.run();
    const int measures = f.child->measureCalls;

    f.theme.setDefaultStyle("CountingWidget", {{WidgetState::Normal, {.width = 100}}});
    f.run();

    EXPECT_GT(f.child->measureCalls, measures);
}

TEST(LayoutManagerTest, PaddingHelpersAreMutuallyInverse) {
    const Size size = {100, 50};
    const Thickness padding = {.top = 2, .right = 3, .bottom = 4, .left = 5};

    const Size grown = LayoutManager::addPadding(size, padding);
    EXPECT_FLOAT_EQ(grown.width, 108);
    EXPECT_FLOAT_EQ(grown.height, 56);

    const Size back = LayoutManager::subtractPadding(grown, padding);
    EXPECT_FLOAT_EQ(back.width, size.width);
    EXPECT_FLOAT_EQ(back.height, size.height);
}

TEST(LayoutManagerTest, SubtractPaddingMayGoNegative) {
    const Thickness padding = {.top = 10, .right = 10, .bottom = 10, .left = 10};

    const Size size = LayoutManager::subtractPadding({5, 5}, padding);

    EXPECT_FLOAT_EQ(size.width, -15);
    EXPECT_FLOAT_EQ(size.height, -15);
}

TEST(LayoutManagerTest, ShrinkRectMatchesContentRect) {
    LayoutFixture f;
    const Thickness padding = {.top = 2, .right = 3, .bottom = 4, .left = 5};
    f.root->setStyle({.padding = padding}, WidgetState::Normal);
    f.styleManager.resolveDirtyStyles(f.root);

    const Rect outer = {10, 20, 100, 60};
    const Rect viaNode = LayoutManager::contentRect(*f.root, outer);
    const Rect viaHelper = LayoutManager::shrinkRect(outer, padding);

    EXPECT_EQ(viaHelper, viaNode);
    EXPECT_EQ(viaHelper.x, 15);
    EXPECT_EQ(viaHelper.y, 22);
    EXPECT_EQ(viaHelper.width, 92);
    EXPECT_EQ(viaHelper.height, 54);
}
