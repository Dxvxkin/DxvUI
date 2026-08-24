#include <gtest/gtest.h>

#include <memory>
#include <string>
#include <utility>

#include "DxvUI/Log.h"
#include "DxvUI/SceneNode.h"
#include "DxvUI/containers/AbsoluteContainer.h"
#include "DxvUI/containers/CenterContainer.h"
#include "DxvUI/containers/HorizontalContainer.h"
#include "DxvUI/layout/LayoutManager.h"
#include "DxvUI/style/Colors.h"
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

// A widget that records the size its parent passed down during the last measure,
// so tests can assert the padding was subtracted before measuring children.
class SizeRecordingWidget : public SceneNode {
   public:
    explicit SizeRecordingWidget(std::string id) : SceneNode(std::move(id)) {}

    Size lastAvailableSize{};

    const char* getNodeType() const override { return "SizeRecordingWidget"; }

   protected:
    Size onMeasure(const Size& availableSize) override {
        lastAvailableSize = availableSize;
        return {10, 20};
    }
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

TEST(LayoutManagerTest, ContentRectIncludesBorder) {
    LayoutFixture f;
    const Thickness padding = {.top = 2, .right = 3, .bottom = 4, .left = 5};
    f.root->setStyle({.borderThickness = 2, .padding = padding}, WidgetState::Normal);
    f.styleManager.resolveDirtyStyles(f.root);

    const Rect outer = {10, 20, 100, 60};
    const Rect content = LayoutManager::contentRect(*f.root, outer);

    // Padding (5,2,3,4) + Border (2,2,2,2) = Total Inset (7,4,5,6)
    // x = 10+7 = 17, y = 20+4 = 24
    // width = 100 - (7+5) = 88, height = 60 - (4+6) = 50
    EXPECT_EQ(content.x, 17);
    EXPECT_EQ(content.y, 24);
    EXPECT_EQ(content.width, 88);
    EXPECT_EQ(content.height, 50);
}

TEST(LayoutManagerTest, HorizontalContainerSubtractsPaddingBeforeMeasuringChildren) {
    const Thickness padding = {.top = 2, .right = 3, .bottom = 4, .left = 5};

    auto root = std::make_shared<AbsoluteContainer>("root");
    auto row = std::make_shared<HorizontalContainer>("row");
    auto child = std::make_shared<SizeRecordingWidget>("child");
    row->addChild(child);
    row->setStyle({.padding = padding}, WidgetState::Normal);
    root->addChild(row);

    Theme theme;
    StyleManager styleManager{theme};
    LayoutManager layout;
    styleManager.resolveDirtyStyles(root);
    layout.layout(root, {800, 600});

    EXPECT_FLOAT_EQ(child->lastAvailableSize.width, 800 - (padding.left + padding.right));
    EXPECT_FLOAT_EQ(child->lastAvailableSize.height, 600 - (padding.top + padding.bottom));

    const Rect rowBounds = row->getGlobalBounds();
    EXPECT_EQ(rowBounds.width, static_cast<int>(10 + padding.left + padding.right));
    EXPECT_EQ(rowBounds.height, static_cast<int>(20 + padding.top + padding.bottom));
}

TEST(LayoutManagerTest, MeasureChildSubtractsMarginAndReturnsOuterSize) {
    const Thickness margin = {.top = 2, .right = 3, .bottom = 4, .left = 5};

    auto root = std::make_shared<AbsoluteContainer>("root");
    auto child = std::make_shared<SizeRecordingWidget>("child");
    root->addChild(child);
    child->setStyle({.margin = margin}, WidgetState::Normal);

    Theme theme;
    StyleManager styleManager{theme};
    styleManager.resolveDirtyStyles(root);

    const Size outer = LayoutManager::measureChild(*child, {100, 60});

    EXPECT_FLOAT_EQ(child->lastAvailableSize.width, 100 - (margin.left + margin.right));
    EXPECT_FLOAT_EQ(child->lastAvailableSize.height, 60 - (margin.top + margin.bottom));
    EXPECT_FLOAT_EQ(outer.width, 10 + margin.left + margin.right);
    EXPECT_FLOAT_EQ(outer.height, 20 + margin.top + margin.bottom);

    child->setVisible(false);
    const Size hidden = LayoutManager::measureChild(*child, {100, 60});
    EXPECT_FLOAT_EQ(hidden.width, 0);
    EXPECT_FLOAT_EQ(hidden.height, 0);
}

TEST(LayoutManagerTest, MarginExpandsAndOffsetsHorizontalContainer) {
    const Thickness marginA = {.top = 1, .right = 2, .bottom = 3, .left = 4};

    auto root = std::make_shared<AbsoluteContainer>("root");
    auto row = std::make_shared<HorizontalContainer>("row");
    auto childA = std::make_shared<SizeRecordingWidget>("childA");
    auto childB = std::make_shared<SizeRecordingWidget>("childB");
    childA->setStyle({.margin = marginA}, WidgetState::Normal);
    row->setSpacing(3);
    row->addChild(childA);
    row->addChild(childB);
    root->addChild(row);

    Theme theme;
    StyleManager styleManager{theme};
    LayoutManager layout;
    styleManager.resolveDirtyStyles(root);
    layout.layout(root, {800, 600});

    const int outerWidthA = 10 + static_cast<int>(marginA.left + marginA.right);
    const Rect rowBounds = row->getGlobalBounds();
    EXPECT_EQ(rowBounds.width, outerWidthA + 3 + 10);
    EXPECT_EQ(rowBounds.height, 20 + static_cast<int>(marginA.top + marginA.bottom));

    const Rect boundsA = childA->getGlobalBounds();
    EXPECT_EQ(boundsA.x, static_cast<int>(marginA.left));
    EXPECT_EQ(boundsA.y, static_cast<int>(marginA.top));
    EXPECT_EQ(boundsA.width, 10);

    const Rect boundsB = childB->getGlobalBounds();
    EXPECT_EQ(boundsB.x, outerWidthA + 3);
    EXPECT_EQ(boundsB.y, 0);
    EXPECT_EQ(boundsB.width, 10);
}

TEST(LayoutManagerTest, StyleGapSpacesHorizontalContainerChildren) {
    auto root = std::make_shared<AbsoluteContainer>("root");
    auto row = std::make_shared<HorizontalContainer>("row");
    auto childA = std::make_shared<SizeRecordingWidget>("childA");
    auto childB = std::make_shared<SizeRecordingWidget>("childB");
    row->addChild(childA);
    row->addChild(childB);
    root->addChild(row);

    // The gap comes from the style, not a container member: setSpacing() is just
    // a convenience wrapper over updateStyle({.gap = ...}).
    row->setStyle({.gap = 7}, WidgetState::Normal);

    Theme theme;
    StyleManager styleManager{theme};
    LayoutManager layout;
    styleManager.resolveDirtyStyles(root);
    layout.layout(root, {800, 600});

    const Rect rowBounds = row->getGlobalBounds();
    EXPECT_EQ(rowBounds.width, 10 + 7 + 10);

    const Rect boundsB = childB->getGlobalBounds();
    EXPECT_EQ(boundsB.x, 10 + 7);
    EXPECT_EQ(boundsB.width, 10);
}

TEST(LayoutManagerTest, MarginCentersCenterContainerChild) {
    auto root = std::make_shared<AbsoluteContainer>("root");
    auto center = std::make_shared<CenterContainer>("center");
    auto child = std::make_shared<SizeRecordingWidget>("child");
    child->setStyle({.margin = Thickness{2, 2, 2, 2}}, WidgetState::Normal);
    center->setStyle({.width = 50, .height = 50}, WidgetState::Normal);
    center->addChild(child);
    root->addChild(center);

    Theme theme;
    StyleManager styleManager{theme};
    LayoutManager layout;
    styleManager.resolveDirtyStyles(root);
    layout.layout(root, {800, 600});

    const Rect centerBounds = center->getGlobalBounds();
    EXPECT_EQ(centerBounds.width, 50);
    EXPECT_EQ(centerBounds.height, 50);

    const Rect childBounds = child->getGlobalBounds();
    EXPECT_EQ(childBounds.x, 0 + (50 - (10 + 4)) / 2 + 2);
    EXPECT_EQ(childBounds.y, 0 + (50 - (20 + 4)) / 2 + 2);
    EXPECT_EQ(childBounds.width, 10);
    EXPECT_EQ(childBounds.height, 20);
}

TEST(LayoutManagerTest, MarginOffsetsAbsoluteContainerChild) {
    const Thickness marginA = {.top = 5, .right = 0, .bottom = 0, .left = 5};
    const Thickness marginB = {.top = 0, .right = 3, .bottom = 2, .left = 0};

    auto root = std::make_shared<AbsoluteContainer>("root");
    auto childA = std::make_shared<SizeRecordingWidget>("childA");
    auto childB = std::make_shared<SizeRecordingWidget>("childB");
    childA->setStyle({.left = 10, .top = 10, .width = 100, .height = 40, .margin = marginA},
                     WidgetState::Normal);
    childB->setStyle({.right = 10, .bottom = 5, .width = 100, .height = 40, .margin = marginB},
                     WidgetState::Normal);
    root->addChild(childA);
    root->addChild(childB);

    Theme theme;
    StyleManager styleManager{theme};
    LayoutManager layout;
    styleManager.resolveDirtyStyles(root);
    layout.layout(root, {500, 300});

    const Rect boundsA = childA->getGlobalBounds();
    EXPECT_EQ(boundsA.x, 10 + static_cast<int>(marginA.left));
    EXPECT_EQ(boundsA.y, 10 + static_cast<int>(marginA.top));
    EXPECT_EQ(boundsA.width, 100);

    const Rect boundsB = childB->getGlobalBounds();
    EXPECT_EQ(boundsB.x, 500 - 100 - 10 - static_cast<int>(marginB.right));
    EXPECT_EQ(boundsB.y, 300 - 40 - 5 - static_cast<int>(marginB.bottom));
    EXPECT_EQ(boundsB.width, 100);
}

TEST(LayoutManagerTest, AbsoluteContainerRightOnlyMeasureFixesDoubleWidth) {
    auto outer = std::make_shared<AbsoluteContainer>("outer");
    auto inner = std::make_shared<AbsoluteContainer>("inner");
    auto child = std::make_shared<SizeRecordingWidget>("child");
    child->setStyle({.right = 10, .width = 100, .height = 30}, WidgetState::Normal);
    inner->addChild(child);
    outer->addChild(inner);

    Theme theme;
    StyleManager styleManager{theme};
    LayoutManager layout;
    styleManager.resolveDirtyStyles(outer);
    layout.layout(outer, {500, 300});

    const Rect innerBounds = inner->getGlobalBounds();
    // Contribution of a right-anchored child is right + outerWidth (110), not
    // 2 * width + right (210).
    EXPECT_EQ(innerBounds.width, 10 + 100);
    EXPECT_EQ(innerBounds.height, 30);
}

TEST(LayoutManagerTest, ButtonHonorsChildMargin) {
    const Thickness margin = {.top = 1, .right = 2, .bottom = 3, .left = 4};

    auto root = std::make_shared<AbsoluteContainer>("root");
    auto button = Button::create("button");
    button->setStyle({.width = 100, .height = 50, .padding = Thickness{0, 0, 0, 0}},
                     WidgetState::Normal);
    root->addChild(button);
    ASSERT_FALSE(button->getChildren().empty());
    button->getChildren().front()->setStyle({.margin = margin}, WidgetState::Normal);

    Theme theme;
    StyleManager styleManager{theme};
    LayoutManager layout;
    styleManager.resolveDirtyStyles(root);
    layout.layout(root, {800, 600});

    const Rect buttonBounds = button->getGlobalBounds();
    EXPECT_EQ(buttonBounds.width, 100);
    EXPECT_EQ(buttonBounds.height, 50);

    const Rect childBounds = button->getChildren().front()->getGlobalBounds();
    EXPECT_EQ(childBounds.x, static_cast<int>(margin.left));
    EXPECT_EQ(childBounds.y, static_cast<int>(margin.top));
    EXPECT_EQ(childBounds.width, 100 - static_cast<int>(margin.left + margin.right));
    EXPECT_EQ(childBounds.height, 50 - static_cast<int>(margin.top + margin.bottom));
}

namespace {

// A widget with a fixed measured size, so tests can build rows with children of
// different heights.
class FixedSizeWidget : public SceneNode {
   public:
    FixedSizeWidget(std::string id, Size size) : SceneNode(std::move(id)), size_(size) {}

    const char* getNodeType() const override { return "FixedSizeWidget"; }

   protected:
    Size onMeasure(const Size& /*availableSize*/) override { return size_; }

   private:
    Size size_;
};

// Attaches a child with a style to a throwaway root and resolves the styles, so
// a test can call LayoutManager::alignChild directly.
std::shared_ptr<SceneNode> makeResolvedChild(std::string id, const StyleRule& style) {
    auto root = std::make_shared<AbsoluteContainer>("root");
    auto child = std::make_shared<SizeRecordingWidget>(std::move(id));
    child->setStyle(style, WidgetState::Normal);
    root->addChild(child);

    Theme theme;
    StyleManager styleManager{theme};
    styleManager.resolveDirtyStyles(root);
    return child;
}

}  // namespace

TEST(LayoutManagerTest, AlignChildStartKeepsTopLeft) {
    auto child = makeResolvedChild("child", StyleRule{});
    const Rect slot = {10, 20, 100, 60};

    const Rect result = LayoutManager::alignChild(*child, {10, 20}, slot, {true, true});

    EXPECT_EQ(result, (Rect{10, 20, 10, 20}));
}

TEST(LayoutManagerTest, AlignChildCentersInSlot) {
    auto child = makeResolvedChild("child", {.horizontalAlignment = Alignment::Center,
                                             .verticalAlignment = Alignment::Center});
    const Rect slot = {10, 20, 100, 60};

    const Rect result = LayoutManager::alignChild(*child, {10, 20}, slot, {true, true});

    EXPECT_EQ(result, (Rect{10 + (100 - 10) / 2, 20 + (60 - 20) / 2, 10, 20}));
}

TEST(LayoutManagerTest, AlignChildEndMovesToBottomRight) {
    auto child = makeResolvedChild(
        "child", {.horizontalAlignment = Alignment::End, .verticalAlignment = Alignment::End});
    const Rect slot = {10, 20, 100, 60};

    const Rect result = LayoutManager::alignChild(*child, {10, 20}, slot, {true, true});

    EXPECT_EQ(result, (Rect{10 + 100 - 10, 20 + 60 - 20, 10, 20}));
}

TEST(LayoutManagerTest, AlignChildDisabledAxisUsesSlotOrigin) {
    auto child = makeResolvedChild(
        "child", {.horizontalAlignment = Alignment::End, .verticalAlignment = Alignment::Center});
    const Rect slot = {30, 20, 50, 60};

    // The horizontal axis is disabled, so End must be ignored and the slot
    // origin (which the caller already offset by margin) is kept.
    const Rect result =
        LayoutManager::alignChild(*child, {10, 20}, slot, {.horizontal = false, .vertical = true});

    EXPECT_EQ(result, (Rect{30, 20 + (60 - 20) / 2, 10, 20}));
}

TEST(LayoutManagerTest, AlignChildCenterRespectsMargin) {
    const Thickness margin = {.top = 2, .right = 3, .bottom = 4, .left = 5};
    auto child = makeResolvedChild("child", {.margin = margin,
                                             .horizontalAlignment = Alignment::Center,
                                             .verticalAlignment = Alignment::Center});
    const Rect slot = {0, 0, 100, 60};

    const Rect result = LayoutManager::alignChild(*child, {10, 20}, slot, {true, true});

    // The margin-box (10 + 8 by 20 + 6) is centered in the slot, then the child
    // itself is offset by its margin.
    EXPECT_EQ(result, (Rect{5 + (100 - 8 - 10) / 2, 2 + (60 - 6 - 20) / 2, 10, 20}));
}

TEST(LayoutManagerTest, AlignChildStretchesOnBothAxes) {
    auto child = makeResolvedChild("child", {.horizontalAlignment = Alignment::Stretch,
                                              .verticalAlignment = Alignment::Stretch});
    const Rect slot = {10, 20, 100, 60};

    const Rect result = LayoutManager::alignChild(*child, {10, 20}, slot, {true, true});

    // Stretch fills the slot on both axes.
    EXPECT_EQ(result, (Rect{10, 20, 100, 60}));
}

TEST(LayoutManagerTest, AlignChildStretchesWithMargin) {
    const Thickness margin = {.top = 2, .right = 3, .bottom = 4, .left = 5};
    auto child = makeResolvedChild("child", {.margin = margin,
                                              .horizontalAlignment = Alignment::Stretch,
                                              .verticalAlignment = Alignment::Stretch});
    const Rect slot = {0, 0, 100, 60};

    const Rect result = LayoutManager::alignChild(*child, {10, 20}, slot, {true, true});

    // Stretch fills the slot minus margin on each axis.
    EXPECT_EQ(result, (Rect{5, 2, 100 - 5 - 3, 60 - 2 - 4}));
}

TEST(LayoutManagerTest, AlignChildStretchSingleAxis) {
    auto child = makeResolvedChild("child", {.horizontalAlignment = Alignment::Stretch,
                                              .verticalAlignment = Alignment::End});
    const Rect slot = {0, 0, 100, 60};

    const Rect result = LayoutManager::alignChild(*child, {10, 20}, slot, {true, true});

    // Horizontal stretches to fill, vertical uses End alignment.
    EXPECT_EQ(result, (Rect{0, 60 - 20, 100, 20}));
}

TEST(LayoutManagerTest, HorizontalContainerAlignsChildrenVertically) {
    auto root = std::make_shared<AbsoluteContainer>("root");
    auto row = std::make_shared<HorizontalContainer>("row");
    auto tall = std::make_shared<SizeRecordingWidget>("tall");
    auto centered = std::make_shared<FixedSizeWidget>("centered", Size{10, 10});
    auto bottomed = std::make_shared<FixedSizeWidget>("bottomed", Size{10, 10});
    centered->setStyle({.verticalAlignment = Alignment::Center}, WidgetState::Normal);
    bottomed->setStyle({.verticalAlignment = Alignment::End}, WidgetState::Normal);
    row->addChild(tall);
    row->addChild(centered);
    row->addChild(bottomed);
    root->addChild(row);

    Theme theme;
    StyleManager styleManager{theme};
    LayoutManager layout;
    styleManager.resolveDirtyStyles(root);
    layout.layout(root, {800, 600});

    // The row's content height is 20 (the tallest child); the smaller children
    // are aligned by their verticalAlignment within it.
    EXPECT_EQ(tall->getGlobalBounds().y, 0);
    EXPECT_EQ(centered->getGlobalBounds().y, (20 - 10) / 2);
    EXPECT_EQ(bottomed->getGlobalBounds().y, 20 - 10);
}

TEST(LayoutManagerTest, AbsoluteContainerUnanchoredAlignmentPositionsChild) {
    auto root = std::make_shared<AbsoluteContainer>("root");
    auto container = std::make_shared<AbsoluteContainer>("container");
    auto child = std::make_shared<SizeRecordingWidget>("child");
    child->setStyle({.horizontalAlignment = Alignment::End, .verticalAlignment = Alignment::Center},
                    WidgetState::Normal);
    container->setStyle({.width = 200, .height = 100}, WidgetState::Normal);
    container->addChild(child);
    root->addChild(container);

    Theme theme;
    StyleManager styleManager{theme};
    LayoutManager layout;
    styleManager.resolveDirtyStyles(root);
    layout.layout(root, {800, 600});

    const Rect bounds = child->getGlobalBounds();
    EXPECT_EQ(bounds.x, 200 - 10);
    EXPECT_EQ(bounds.y, (100 - 20) / 2);
    EXPECT_EQ(bounds.width, 10);
}

TEST(LayoutManagerTest, AbsoluteContainerAnchorBeatsAlignment) {
    auto root = std::make_shared<AbsoluteContainer>("root");
    auto container = std::make_shared<AbsoluteContainer>("container");
    auto child = std::make_shared<SizeRecordingWidget>("child");
    child->setStyle({.left = 10,
                     .width = 50,
                     .height = 30,
                     .horizontalAlignment = Alignment::End,
                     .verticalAlignment = Alignment::Center},
                    WidgetState::Normal);
    container->setStyle({.width = 200, .height = 100}, WidgetState::Normal);
    container->addChild(child);
    root->addChild(container);

    Theme theme;
    StyleManager styleManager{theme};
    LayoutManager layout;
    styleManager.resolveDirtyStyles(root);
    layout.layout(root, {800, 600});

    const Rect bounds = child->getGlobalBounds();
    EXPECT_EQ(bounds.x, 10);
    EXPECT_EQ(bounds.y, (100 - 30) / 2);
    EXPECT_EQ(bounds.width, 50);
}

TEST(LayoutManagerTest, AlignmentStyleChangeTriggersRelayout) {
    LayoutFixture f;
    f.run();
    const int measures = f.child->measureCalls;

    f.child->setStyle({.horizontalAlignment = Alignment::Center}, WidgetState::Normal);
    f.run();

    EXPECT_GT(f.child->measureCalls, measures);
}
