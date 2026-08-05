#include <gtest/gtest.h>

#include <cstdint>
#include <memory>
#include <string>
#include <utility>

#include "DxvUI/Log.h"
#include "DxvUI/SceneNode.h"
#include "DxvUI/containers/AbsoluteContainer.h"
#include "DxvUI/style/Colors.h"
#include "DxvUI/style/StyleManager.h"
#include "DxvUI/style/Theme.h"

using namespace DxvUI;

namespace {

// A node subclass with its own widget type name, so that theme defaults
// registered under that name can be exercised in tests.
class TestWidget : public SceneNode {
   public:
    explicit TestWidget(std::string id) : SceneNode(std::move(id)) {}

    const char* getNodeType() const override { return "TestWidget"; }
};

// A node whose intrinsic measured size is fixed, so that min/max clamping can
// be exercised without a renderer.
class FixedSizeWidget : public SceneNode {
   public:
    explicit FixedSizeWidget(std::string id, Size size) : SceneNode(std::move(id)), size_(size) {}

    Size measureOverride(const Size& /*availableSize*/) override { return size_; }

   private:
    Size size_;
};

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

// The StyleManager is a pure computation unit: it resolves styles for a bare
// node tree without a Scene or a renderer.

TEST(StyleManagerTest, FrameworkDefaultsWithoutThemeRegistration) {
    auto node = std::make_shared<SceneNode>("node");
    Theme theme;
    StyleManager manager(theme);

    manager.resolveDirtyStyles(node);

    const auto& appearance = node->getComputedAppearance(WidgetState::Normal);
    EXPECT_EQ(appearance.backgroundColor, Colors::Transparent);
    EXPECT_EQ(appearance.textColor, Colors::Black);
    EXPECT_EQ(appearance.borderThickness, 0);
    EXPECT_EQ(appearance.borderRadius, 0);
    EXPECT_EQ(appearance.fontSize, 14);

    const auto& layout = node->getComputedLayout(WidgetState::Normal);
    EXPECT_FLOAT_EQ(layout.width, 0.0f);
    EXPECT_EQ(layout.horizontalAlignment, Alignment::Start);
}

TEST(StyleManagerTest, OwnStyleOverridesFrameworkDefaults) {
    auto node = std::make_shared<SceneNode>("node");
    node->editStyle().set({.backgroundColor = Colors::Red, .textColor = Colors::White},
                          WidgetState::Normal);
    Theme theme;
    StyleManager manager(theme);

    manager.resolveDirtyStyles(node);

    const auto& appearance = node->getComputedAppearance(WidgetState::Normal);
    EXPECT_EQ(appearance.backgroundColor, Colors::Red);
    EXPECT_EQ(appearance.textColor, Colors::White);
    // Properties not overridden fall back to the framework defaults.
    EXPECT_EQ(appearance.borderThickness, 0);
}

TEST(StyleManagerTest, ThemeDefaultsAppliedForRegisteredWidgetType) {
    Theme::registerDefaultStyle("TestWidget", {{WidgetState::Normal, {.borderThickness = 3}},
                                               {WidgetState::Hovered, {.borderRadius = 7}}});

    auto node = std::make_shared<TestWidget>("node");
    Theme theme;
    StyleManager manager(theme);

    manager.resolveDirtyStyles(node);

    EXPECT_EQ(node->getComputedAppearance(WidgetState::Normal).borderThickness, 3);
    EXPECT_EQ(node->getComputedAppearance(WidgetState::Hovered).borderRadius, 7);
}

TEST(StyleManagerTest, StateStyleIsLayeredOnNormal) {
    auto node = std::make_shared<SceneNode>("node");
    node->editStyle().set({.backgroundColor = Colors::Red}, WidgetState::Normal);
    node->editStyle().set({.textColor = Colors::Blue}, WidgetState::Hovered);
    Theme theme;
    StyleManager manager(theme);

    manager.resolveDirtyStyles(node);

    const auto& normal = node->getComputedAppearance(WidgetState::Normal);
    EXPECT_EQ(normal.backgroundColor, Colors::Red);
    EXPECT_EQ(normal.textColor, Colors::Black);

    const auto& hovered = node->getComputedAppearance(WidgetState::Hovered);
    EXPECT_EQ(hovered.backgroundColor, Colors::Red);  // The Normal layer still applies.
    EXPECT_EQ(hovered.textColor, Colors::Blue);
}

TEST(StyleManagerTest, TextPropertiesInheritedFromParentNormal) {
    auto parent = std::make_shared<SceneNode>("parent");
    auto child = std::make_shared<SceneNode>("child");
    parent->addChild(child);

    parent->editStyle().set({.textColor = Colors::Red, .fontSize = 24}, WidgetState::Normal);
    Theme theme;
    StyleManager manager(theme);

    manager.resolveDirtyStyles(parent);

    const auto& childAppearance = child->getComputedAppearance(WidgetState::Normal);
    EXPECT_EQ(childAppearance.fontSize, 24);
    EXPECT_EQ(childAppearance.textColor, Colors::Red);
}

TEST(StyleManagerTest, ChildOwnStyleOverridesInheritedTextProperties) {
    auto parent = std::make_shared<SceneNode>("parent");
    auto child = std::make_shared<SceneNode>("child");
    parent->addChild(child);

    parent->editStyle().set({.textColor = Colors::Red}, WidgetState::Normal);
    child->editStyle().set({.textColor = Colors::Green}, WidgetState::Normal);
    Theme theme;
    StyleManager manager(theme);

    manager.resolveDirtyStyles(parent);

    EXPECT_EQ(child->getComputedAppearance(WidgetState::Normal).textColor, Colors::Green);
}

TEST(StyleManagerTest, LayoutPropertiesAreNotInherited) {
    auto parent = std::make_shared<SceneNode>("parent");
    auto child = std::make_shared<SceneNode>("child");
    parent->addChild(child);

    parent->editStyle().set(
        {.width = 100, .padding = Thickness{1, 2, 3, 4}, .horizontalAlignment = Alignment::Center},
        WidgetState::Normal);
    Theme theme;
    StyleManager manager(theme);

    manager.resolveDirtyStyles(parent);

    const auto& childLayout = child->getComputedLayout(WidgetState::Normal);
    EXPECT_FLOAT_EQ(childLayout.width, 0.0f);
    EXPECT_EQ(childLayout.padding.top, 0.0f);
    EXPECT_EQ(childLayout.horizontalAlignment, Alignment::Start);
}

TEST(StyleManagerTest, DirtyPropagationRecomputesInheritedValues) {
    auto parent = std::make_shared<SceneNode>("parent");
    auto child = std::make_shared<SceneNode>("child");
    parent->addChild(child);

    Theme theme;
    StyleManager manager(theme);

    parent->editStyle().set({.fontSize = 16}, WidgetState::Normal);
    manager.resolveDirtyStyles(parent);
    EXPECT_EQ(child->getComputedAppearance(WidgetState::Normal).fontSize, 16);

    // Changing the parent style cascades the dirty flag down to the child, so
    // the child picks up the new inherited value on the next resolution.
    parent->editStyle().set({.fontSize = 30}, WidgetState::Normal);
    manager.resolveDirtyStyles(parent);
    EXPECT_EQ(child->getComputedAppearance(WidgetState::Normal).fontSize, 30);
}

TEST(StyleManagerTest, AllStatesAreResolved) {
    auto node = std::make_shared<SceneNode>("node");
    Theme theme;
    StyleManager manager(theme);

    manager.resolveDirtyStyles(node);

    // The cache is populated for every state; a missing cache entry triggers a
    // FATAL log and falls back to empty values, so we assert real defaults.
    for (const auto state :
         {WidgetState::Normal, WidgetState::Hovered, WidgetState::Pressed, WidgetState::Disabled}) {
        EXPECT_EQ(node->getComputedAppearance(state).fontSize, 14);
        EXPECT_FLOAT_EQ(node->getComputedLayout(state).width, 0.0f);
    }
}

TEST(StyleManagerTest, StateChangeSelectsCachedEntryWithoutReResolve) {
    auto node = std::make_shared<SceneNode>("node");
    node->editStyle().set({.textColor = Colors::Blue}, WidgetState::Hovered);
    Theme theme;
    StyleManager manager(theme);

    manager.resolveDirtyStyles(node);

    // All states are resolved up front, so switching the current state only
    // selects a cached entry; it must not require a fresh resolution pass.
    node->setHovered(true);
    EXPECT_EQ(node->getComputedAppearance(node->getCurrentState()).textColor, Colors::Blue);
    EXPECT_EQ(node->getComputedLayout(node->getCurrentState()).padding.top, 0.0f);

    node->setHovered(false);
    node->setPressed(true);
    EXPECT_EQ(node->getComputedAppearance(node->getCurrentState()).textColor, Colors::Black);
}

TEST(StyleManagerTest, MinMaxSizeConstraintsAreResolved) {
    auto node = std::make_shared<SceneNode>("node");
    node->editStyle().set({.minWidth = 50, .minHeight = 30, .maxWidth = 200, .maxHeight = 100},
                          WidgetState::Normal);
    Theme theme;
    StyleManager manager(theme);

    manager.resolveDirtyStyles(node);

    const auto& layout = node->getComputedLayout(WidgetState::Normal);
    ASSERT_TRUE(layout.minWidth.has_value());
    EXPECT_FLOAT_EQ(layout.minWidth.value(), 50.0f);
    ASSERT_TRUE(layout.minHeight.has_value());
    EXPECT_FLOAT_EQ(layout.minHeight.value(), 30.0f);
    ASSERT_TRUE(layout.maxWidth.has_value());
    EXPECT_FLOAT_EQ(layout.maxWidth.value(), 200.0f);
    ASSERT_TRUE(layout.maxHeight.has_value());
    EXPECT_FLOAT_EQ(layout.maxHeight.value(), 100.0f);
}

TEST(StyleManagerTest, MeasureClampsToMinMax) {
    auto minNode = std::make_shared<FixedSizeWidget>("min", Size{20, 10});
    minNode->editStyle().set({.minWidth = 50, .minHeight = 30}, WidgetState::Normal);

    auto maxNode = std::make_shared<FixedSizeWidget>("max", Size{500, 400});
    maxNode->editStyle().set({.maxWidth = 200, .maxHeight = 100}, WidgetState::Normal);

    Theme theme;
    StyleManager manager(theme);
    manager.resolveDirtyStyles(minNode);
    manager.resolveDirtyStyles(maxNode);

    Size measured = minNode->measure({0, 0});
    EXPECT_FLOAT_EQ(measured.width, 50.0f);
    EXPECT_FLOAT_EQ(measured.height, 30.0f);

    measured = maxNode->measure({0, 0});
    EXPECT_FLOAT_EQ(measured.width, 200.0f);
    EXPECT_FLOAT_EQ(measured.height, 100.0f);
}

TEST(StyleManagerTest, ExplicitSizeWinsOverMinMax) {
    auto node = std::make_shared<FixedSizeWidget>("node", Size{500, 400});
    node->editStyle().set({.width = 100, .height = 50, .minWidth = 80, .maxWidth = 60},
                          WidgetState::Normal);
    Theme theme;
    StyleManager manager(theme);

    manager.resolveDirtyStyles(node);

    Size measured = node->measure({0, 0});
    EXPECT_FLOAT_EQ(measured.width, 100.0f);
    EXPECT_FLOAT_EQ(measured.height, 50.0f);
}

TEST(StyleManagerTest, RightBottomAreResolved) {
    auto node = std::make_shared<SceneNode>("node");
    node->editStyle().set({.left = 1, .top = 2, .right = 3, .bottom = 4}, WidgetState::Normal);
    Theme theme;
    StyleManager manager(theme);

    manager.resolveDirtyStyles(node);

    const auto& layout = node->getComputedLayout(WidgetState::Normal);
    ASSERT_TRUE(layout.left.has_value());
    EXPECT_FLOAT_EQ(layout.left.value(), 1.0f);
    ASSERT_TRUE(layout.top.has_value());
    EXPECT_FLOAT_EQ(layout.top.value(), 2.0f);
    ASSERT_TRUE(layout.right.has_value());
    EXPECT_FLOAT_EQ(layout.right.value(), 3.0f);
    ASSERT_TRUE(layout.bottom.has_value());
    EXPECT_FLOAT_EQ(layout.bottom.value(), 4.0f);
}

TEST(StyleManagerTest, AbsoluteContainerRightBottomPositioning) {
    auto container = std::make_shared<AbsoluteContainer>("cont");
    auto child = std::make_shared<SceneNode>("child");
    child->editStyle().set({.right = 10, .bottom = 5, .width = 100, .height = 40},
                           WidgetState::Normal);
    container->addChild(child);

    Theme theme;
    StyleManager manager(theme);
    manager.resolveDirtyStyles(container);

    container->measure({500, 300});
    container->arrange({0, 0, 500, 300});

    const auto& childLayout = child->getComputedLayout(WidgetState::Normal);
    // right=10 anchors the child's right edge to 500-10=490.
    EXPECT_EQ(childLayout.computedBounds.x, 390);
    // bottom=5 anchors the child's bottom edge to 300-5=295.
    EXPECT_EQ(childLayout.computedBounds.y, 255);
    EXPECT_EQ(childLayout.computedBounds.width, 100);
    EXPECT_EQ(childLayout.computedBounds.height, 40);
}

TEST(StyleManagerTest, AbsoluteContainerLeftWinsOverRight) {
    auto container = std::make_shared<AbsoluteContainer>("cont");
    auto child = std::make_shared<SceneNode>("child");
    child->editStyle().set({.left = 20, .right = 10, .width = 100, .height = 40},
                           WidgetState::Normal);
    container->addChild(child);

    Theme theme;
    StyleManager manager(theme);
    manager.resolveDirtyStyles(container);

    container->measure({500, 300});
    container->arrange({0, 0, 500, 300});

    const auto& childLayout = child->getComputedLayout(WidgetState::Normal);
    EXPECT_EQ(childLayout.computedBounds.x, 20);
}

TEST(ThemeTest, InstanceOverrideMergesOnFrameworkDefault) {
    Theme::registerDefaultStyle("TestWidget",
                                {{WidgetState::Normal, {.borderThickness = 3, .borderRadius = 5}}});

    Theme theme;
    theme.setDefaultStyle("TestWidget", {{WidgetState::Normal, {.borderThickness = 9}}});

    auto node = std::make_shared<TestWidget>("node");
    StyleManager manager(theme);
    manager.resolveDirtyStyles(node);

    // The override property wins, the untouched framework property is kept.
    EXPECT_EQ(node->getComputedAppearance(WidgetState::Normal).borderThickness, 9);
    EXPECT_EQ(node->getComputedAppearance(WidgetState::Normal).borderRadius, 5);
}

TEST(ThemeTest, ClearDefaultStyleRestoresFrameworkDefaults) {
    Theme::registerDefaultStyle("TestWidget", {{WidgetState::Normal, {.borderThickness = 3}}});

    Theme theme;
    theme.setDefaultStyle("TestWidget", {{WidgetState::Normal, {.borderThickness = 9}}});

    auto node = std::make_shared<TestWidget>("node");
    StyleManager manager(theme);
    manager.resolveDirtyStyles(node);
    EXPECT_EQ(node->getComputedAppearance(WidgetState::Normal).borderThickness, 9);

    theme.clearDefaultStyle("TestWidget");
    manager.resolveDirtyStyles(node);
    EXPECT_EQ(node->getComputedAppearance(WidgetState::Normal).borderThickness, 3);
}

TEST(ThemeTest, ThemeChangeReResolvesCachedStyles) {
    Theme::registerDefaultStyle("TestWidget", {{WidgetState::Normal, {.borderThickness = 1}}});

    auto node = std::make_shared<TestWidget>("node");
    Theme theme;
    StyleManager manager(theme);

    manager.resolveDirtyStyles(node);
    EXPECT_EQ(node->getComputedAppearance(WidgetState::Normal).borderThickness, 1);

    // Mutating the theme must invalidate the already-cached computed styles.
    theme.setDefaultStyle("TestWidget", {{WidgetState::Normal, {.borderThickness = 9}}});
    manager.resolveDirtyStyles(node);
    EXPECT_EQ(node->getComputedAppearance(WidgetState::Normal).borderThickness, 9);
}

TEST(ThemeTest, VersionIncrementsOnMutation) {
    Theme theme;
    const std::uint64_t initial = theme.getVersion();

    theme.setDefaultStyle("TestWidget", {{WidgetState::Normal, {.borderThickness = 1}}});
    EXPECT_GT(theme.getVersion(), initial);

    const std::uint64_t afterSet = theme.getVersion();
    theme.clearDefaultStyle("TestWidget");
    EXPECT_GT(theme.getVersion(), afterSet);

    const std::uint64_t afterClear = theme.getVersion();
    theme.clearDefaultStyle("TestWidget");  // no-op: not registered
    EXPECT_EQ(theme.getVersion(), afterClear);
}

TEST(StyleTest, VersionBumpsOnChangeNotOnNoop) {
    auto node = std::make_shared<SceneNode>("node");
    const std::uint64_t initial = node->getStyleVersion();

    node->editStyle().set({.backgroundColor = Colors::Red}, WidgetState::Normal);
    EXPECT_GT(node->getStyleVersion(), initial);

    const std::uint64_t afterSet = node->getStyleVersion();
    node->editStyle().set({.backgroundColor = Colors::Red}, WidgetState::Normal);  // same value
    EXPECT_EQ(node->getStyleVersion(), afterSet);

    node->editStyle().update({.backgroundColor = Colors::Red});  // no-op merge
    EXPECT_EQ(node->getStyleVersion(), afterSet);

    node->editStyle().update({.backgroundColor = Colors::Blue});
    EXPECT_GT(node->getStyleVersion(), afterSet);
}

TEST(StyleRuleTest, EqualityFollowsPropertyList) {
    StyleRule a = {.backgroundColor = Colors::Red, .borderThickness = 2};
    StyleRule b = {.backgroundColor = Colors::Red, .borderThickness = 2};
    StyleRule c = {.backgroundColor = Colors::Red, .borderThickness = 3};

    EXPECT_EQ(a, b);
    EXPECT_NE(a, c);
}
