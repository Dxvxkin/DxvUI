#include <gtest/gtest.h>

#include <memory>
#include <string>
#include <utility>

#include "DxvUI/Log.h"
#include "DxvUI/SceneNode.h"
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
