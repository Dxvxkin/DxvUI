#include <gtest/gtest.h>

#include <memory>
#include <string>
#include <vector>

#include "DxvUI/DxvEvent.h"
#include "DxvUI/Log.h"
#include "DxvUI/Scene.h"
#include "DxvUI/SceneNode.h"
#include "DxvUI/containers/ScrollContainer.h"
#include "DxvUI/containers/VerticalContainer.h"
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

}  // namespace

namespace {

constexpr int kViewportW = 200;
constexpr int kViewportH = 100;
constexpr int kItemH = 40;
constexpr int kItemCount = 6;

struct ScrollFixture {
    std::shared_ptr<Scene> scene = Scene::create();
    std::shared_ptr<SceneNode> root = scene->getRoot();
    std::shared_ptr<ScrollContainer> scroll;
    std::shared_ptr<VerticalContainer> items;
    std::vector<std::shared_ptr<SceneNode>> rows;
    Theme theme;
    StyleManager manager{theme};

    ScrollFixture() {
        scroll = ScrollContainer::create("scroll");
        scroll->setStyle({.left = 0,
                          .top = 0,
                          .width = kViewportW,
                          .height = kViewportH,
                          .padding = {{0, 0, 0, 0}}},
                         WidgetState::Normal);
        items = std::make_shared<VerticalContainer>("items");
        items->setStyle({.gap = 0}, WidgetState::Normal);
        for (int i = 0; i < kItemCount; ++i) {
            auto row = std::make_shared<SceneNode>("row" + std::to_string(i));
            row->setStyle({.width = kViewportW, .height = kItemH}, WidgetState::Normal);
            rows.push_back(row);
            items->addChild(row);
        }
        scroll->addChild(items);
        root->addChild(scroll);
        relayout();
    }

    void relayout() {
        manager.resolveDirtyStyles(root);
        root->measure({800, 600});
        root->arrange({0, 0, 800, 600});
    }

    void wheel(int dy) {
        DxvEvent e;
        e.type = EventType::MouseWheel;
        e.mouse.dy = dy;
        scene->processEvent(e);
    }

    // Move the cursor over the visible viewport so the ScrollContainer (or one
    // of its rows) becomes the hovered node that receives the wheel.
    void hoverOverViewport() {
        DxvEvent e;
        e.type = EventType::MouseMove;
        e.mouse.x = kViewportW / 2;
        e.mouse.y = kViewportH / 2;
        e.mouse.button = MouseButton::None;
        scene->processEvent(e);
    }
};

TEST(ScrollContainerTest, KeepsViewportSizeNotContentSize) {
    ScrollFixture f;
    // Six rows of 40px each would be 240px of content, but the viewport is 100px
    // and must stay 100px so the content can overflow and scroll.
    EXPECT_EQ(f.root->getGlobalBounds().width, 800);
    EXPECT_EQ(f.scroll->getGlobalBounds().width, kViewportW);
    EXPECT_EQ(f.scroll->getGlobalBounds().height, kViewportH);
}

TEST(ScrollContainerTest, ContentOverflowsViewport) {
    ScrollFixture f;
    // The child container is laid out at its full content height (240px), which
    // is taller than the viewport, so it extends below the clipped bottom edge.
    EXPECT_EQ(f.items->getGlobalBounds().height, kItemH * kItemCount);
    EXPECT_GT(f.items->getGlobalBounds().height, kViewportH);
}

TEST(ScrollContainerTest, ScrollYShiftsChildOffset) {
    ScrollFixture f;
    const int baseY = f.scroll->getGlobalBounds().y;

    // No scroll: row i sits at i*kItemH below the viewport top.
    EXPECT_EQ(f.rows[1]->getGlobalBounds().y, baseY + kItemH);
    EXPECT_EQ(f.rows[5]->getGlobalBounds().y, baseY + kItemH * 5);

    // Scrolling by 40 slides every row up by 40 within the clipped viewport.
    f.scroll->setScrollY(40.0f);
    f.relayout();
    EXPECT_EQ(f.rows[0]->getGlobalBounds().y, baseY - 40);
    EXPECT_EQ(f.rows[1]->getGlobalBounds().y, baseY + kItemH - 40);
}

TEST(ScrollContainerTest, ScrollYClampedToRange) {
    ScrollFixture f;
    f.scroll->setScrollY(1000000.0f);
    f.relayout();
    // Max scroll = content (240) - viewport (100) = 140.
    EXPECT_EQ(f.scroll->getScrollY(), kItemH * kItemCount - kViewportH);

    f.scroll->setScrollY(-50.0f);
    f.relayout();
    EXPECT_EQ(f.scroll->getScrollY(), 0.0f);
}

TEST(ScrollContainerTest, WheelScrollsHoveredViewport) {
    ScrollFixture f;
    ASSERT_EQ(f.scroll->getScrollY(), 0.0f);

    f.hoverOverViewport();
    // Wheel up (dy > 0) scrolls the content up: the offset decreases, so it stays
    // at 0 (already at the top). Wheel down (dy < 0) reveals lower content.
    f.wheel(1);
    EXPECT_EQ(f.scroll->getScrollY(), 0.0f);

    f.wheel(-1);
    EXPECT_EQ(f.scroll->getScrollY(), 1.0f);

    f.wheel(-3);
    EXPECT_EQ(f.scroll->getScrollY(), 4.0f);
}

TEST(ScrollContainerTest, WheelScrollsOverChildRow) {
    ScrollFixture f;
    // Hover over a row of the content (which is the hit target under the cursor);
    // the wheel must bubble up to the ScrollContainer and scroll it.
    DxvEvent move;
    move.type = EventType::MouseMove;
    move.mouse.x = kViewportW / 2;
    move.mouse.y = 10;  // over the first row
    move.mouse.button = MouseButton::None;
    f.scene->processEvent(move);

    f.wheel(-2);
    EXPECT_EQ(f.scroll->getScrollY(), 2.0f);
}

TEST(ScrollContainerTest, EmptyViewportStaysZero) {
    ScrollFixture f;
    // Replace the child with nothing: no content, no scrolling beyond 0.
    f.scroll->removeChild(f.items);
    f.relayout();
    f.scroll->setScrollY(50.0f);
    EXPECT_EQ(f.scroll->getScrollY(), 0.0f);
}

}  // namespace
