#include <gtest/gtest.h>

#include <memory>
#include <string>
#include <utility>

#include "DxvUI/Log.h"
#include "DxvUI/Scene.h"
#include "DxvUI/SceneNode.h"
#include "DxvUI/style/Colors.h"
#include "DxvUI/style/StyleManager.h"
#include "DxvUI/style/Theme.h"
#include "DxvUI/widgets/Popup.h"

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

// Layout can be exercised without a renderer: styles are resolved with a local
// StyleManager and the tree is measured/arranged manually, exactly like the
// other widget tests do. The fixture popup has a fixed 200x150 size.
struct PopupFixture {
    std::shared_ptr<Scene> scene = Scene::create();
    std::shared_ptr<SceneNode> root = scene->getRoot();
    std::shared_ptr<Popup> popup = Popup::create("pop");
    Theme theme;
    StyleManager manager{theme};

    PopupFixture() {
        popup->setStyle({.width = 200, .height = 150}, WidgetState::Normal);
        root->addChild(popup);
    }

    void layout() {
        manager.resolveDirtyStyles(root);
        root->measure({800, 600});
        root->arrange({0, 0, 800, 600});
    }
};

// A Popup subclass that counts open/close lifecycle calls.
class TrackingPopup : public Popup {
   public:
    explicit TrackingPopup(std::string id) : Popup(std::move(id)) {}
    int openCount = 0;
    int closeCount = 0;

   protected:
    void onOpen() override { openCount++; }
    void onClose() override { closeCount++; }
};

}  // namespace

TEST(PopupTest, HiddenByDefault) {
    auto popup = Popup::create("pop");
    EXPECT_FALSE(popup->isOpen());
}

TEST(PopupTest, ShowHideTogglesState) {
    PopupFixture f;
    EXPECT_FALSE(f.popup->isOpen());

    f.popup->show();
    EXPECT_TRUE(f.popup->isOpen());

    f.popup->hide();
    EXPECT_FALSE(f.popup->isOpen());

    // Повторный hide при уже закрытом поп-апе — no-op.
    f.popup->hide();
    EXPECT_FALSE(f.popup->isOpen());
}

TEST(PopupTest, ShowAtPositions) {
    PopupFixture f;
    f.popup->showAt(300, 200);
    f.layout();

    EXPECT_EQ(f.popup->getGlobalBounds(), (Rect{300, 200, 200, 150}));
}

TEST(PopupTest, SetPositionMovesOpenPopup) {
    PopupFixture f;
    f.popup->showAt(300, 200);
    f.layout();

    f.popup->setPosition(50, 60);
    f.layout();

    EXPECT_EQ(f.popup->getGlobalBounds(), (Rect{50, 60, 200, 150}));
}

TEST(PopupTest, AutoSizesToContent) {
    auto scene = Scene::create();
    auto root = scene->getRoot();
    auto popup = Popup::create("pop");
    auto content = std::make_shared<SceneNode>("content");
    content->setStyle({.width = 80, .height = 40}, WidgetState::Normal);
    popup->addChild(content);
    popup->show();
    root->addChild(popup);

    Theme theme;
    StyleManager manager{theme};
    manager.resolveDirtyStyles(root);
    root->measure({800, 600});
    root->arrange({0, 0, 800, 600});

    // 80/40 контента + padding 8 со всех сторон.
    EXPECT_EQ(popup->getGlobalBounds(), (Rect{0, 0, 96, 56}));
}

TEST(PopupTest, HiddenPopupIsNotHitTested) {
    PopupFixture f;
    f.popup->showAt(300, 200);
    f.layout();
    ASSERT_TRUE(f.popup->isOpen());
    EXPECT_EQ(f.root->findNodeAt(350, 250).get(), f.popup.get());

    f.popup->hide();
    f.layout();
    EXPECT_NE(f.root->findNodeAt(350, 250).get(), f.popup.get());
}

TEST(PopupTest, EmptyAreaHitsPopupItself) {
    PopupFixture f;
    // Ребёнок в левом верхнем углу поп-апа; точка правее него — пустой фон.
    auto content = std::make_shared<SceneNode>("content");
    content->setStyle({.left = 0, .top = 0, .width = 50, .height = 50}, WidgetState::Normal);
    f.popup->addChild(content);
    f.popup->showAt(300, 200);
    f.layout();

    EXPECT_EQ(f.root->findNodeAt(310, 210).get(), content.get());
    EXPECT_EQ(f.root->findNodeAt(390, 300).get(), f.popup.get());
}

TEST(PopupTest, DefaultStyleResolved) {
    PopupFixture f;
    f.layout();

    const auto& appearance = f.popup->getComputedAppearance(WidgetState::Normal);
    EXPECT_EQ(appearance.backgroundColor, Colors::White);
    EXPECT_EQ(appearance.borderColor, Colors::LightGray);
    EXPECT_EQ(appearance.borderThickness, 1);
    EXPECT_EQ(appearance.borderRadius, 4);

    const auto& layout = f.popup->getComputedLayout(WidgetState::Normal);
    EXPECT_EQ(layout.padding, (Thickness{8, 8, 8, 8}));
}

TEST(PopupTest, PositionSurvivesHoveredState) {
    PopupFixture f;
    f.popup->showAt(300, 200);
    f.popup->setHovered(true);
    f.layout();

    // Каскад стилей: позиция и отступы из Normal-слоя не должны исчезать в
    // Hovered-состоянии, иначе поп-ап «прыгнет» при наведении на пустой фон.
    const auto& hoveredLayout = f.popup->getComputedLayout(WidgetState::Hovered);
    ASSERT_TRUE(hoveredLayout.left.has_value());
    ASSERT_TRUE(hoveredLayout.top.has_value());
    EXPECT_EQ(hoveredLayout.left.value(), 300);
    EXPECT_EQ(hoveredLayout.top.value(), 200);
    EXPECT_EQ(hoveredLayout.padding, (Thickness{8, 8, 8, 8}));
    EXPECT_EQ(f.popup->getGlobalBounds(), (Rect{300, 200, 200, 150}));
}

TEST(PopupTest, LifecycleHooksFireOncePerTransition) {
    auto popup = std::make_shared<TrackingPopup>("pop");

    popup->show();
    EXPECT_EQ(popup->openCount, 1);
    EXPECT_EQ(popup->closeCount, 0);

    // Повторный show при открытом поп-апе не запускает onOpen снова.
    popup->show();
    EXPECT_EQ(popup->openCount, 1);

    // showAt поверх открытого поп-апа перепозиционирует без повторного onOpen.
    popup->showAt(10, 10);
    EXPECT_EQ(popup->openCount, 1);

    popup->hide();
    EXPECT_EQ(popup->openCount, 1);
    EXPECT_EQ(popup->closeCount, 1);

    // Повторный hide при закрытом поп-апе — no-op.
    popup->hide();
    EXPECT_EQ(popup->closeCount, 1);

    popup->show();
    EXPECT_EQ(popup->openCount, 2);
    EXPECT_EQ(popup->closeCount, 1);
}