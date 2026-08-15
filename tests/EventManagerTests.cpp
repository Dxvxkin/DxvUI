#include <gtest/gtest.h>

#include <memory>

#include "DxvUI/DxvEvent.h"
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

    void pressAt(int x, int y, MouseButton button = MouseButton::Left) {
        DxvEvent e;
        e.type = EventType::MouseDown;
        e.mouse.x = x;
        e.mouse.y = y;
        e.mouse.button = button;
        scene->processEvent(e);
    }

    void releaseAt(int x, int y, MouseButton button = MouseButton::Left) {
        DxvEvent e;
        e.type = EventType::MouseUp;
        e.mouse.x = x;
        e.mouse.y = y;
        e.mouse.button = button;
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

TEST(EventManagerTest, HoveredDescendantIsClearedWhenCursorLeaves) {
    EventFixture f;
    // Hover empty space first: the fresh scan caches the root, not a button.
    f.moveTo(400, 400, MouseButton::None);

    // The cache-hit path resolves the descendant without refreshing the cache
    // entry, so hover must be tracked independently of the cache.
    f.moveTo(50, 25, MouseButton::None);
    EXPECT_EQ(f.buttonA->getCurrentState(), WidgetState::Hovered);

    // Leaving the button must clear the hover even though the cache still
    // references the root hit by the earlier empty-space scan.
    f.moveTo(400, 400, MouseButton::None);
    EXPECT_EQ(f.buttonA->getCurrentState(), WidgetState::Normal);
}

TEST(EventManagerTest, MouseDownDoesNotOrphanHoveredButton) {
    EventFixture f;
    f.moveTo(50, 25, MouseButton::None);  // hover button A
    EXPECT_EQ(f.buttonA->getCurrentState(), WidgetState::Hovered);

    // Pressing in empty space refreshes the hit-test cache to the root; the
    // previously hovered button must not stay stuck in the Hovered state.
    f.pressAt(400, 400);
    EXPECT_EQ(f.buttonA->getCurrentState(), WidgetState::Normal);
}

TEST(EventManagerTest, ButtonUpMissedPressIsClearedOnButtonlessMove) {
    EventFixture f;
    f.pressAt(50, 25);
    EXPECT_EQ(f.buttonA->getCurrentState(), WidgetState::Pressed);

    // Simulate the mouse leaving the window and coming back over empty space
    // with no button held (a button-up event that never arrived). Empty space is
    // used so the button is not re-hovered, which would legitimately change its
    // state to Hovered and hide what we are asserting (that Pressed is cleared).
    f.moveTo(400, 400, MouseButton::None);

    // The press is cleared; the button keeps the Focused state from the press.
    EXPECT_EQ(f.buttonA->getCurrentState(), WidgetState::Focused);
}

TEST(EventManagerTest, PressingAnotherNodeReleasesPreviousPress) {
    EventFixture f;
    f.pressAt(50, 25);
    EXPECT_EQ(f.buttonA->getCurrentState(), WidgetState::Pressed);

    f.pressAt(250, 25);
    EXPECT_EQ(f.buttonA->getCurrentState(), WidgetState::Normal);
    EXPECT_EQ(f.buttonB->getCurrentState(), WidgetState::Pressed);
}

TEST(EventManagerTest, HitTestCacheResolvesOverlappingSibling) {
    EventFixture f;
    f.moveTo(50, 25, MouseButton::None);  // hover button A; the cache now holds A
    EXPECT_EQ(f.buttonA->getCurrentState(), WidgetState::Hovered);

    // B moves on top of A. The relayout invalidates the hit-test cache (as the
    // engine's Scene::updateLayout does when the pass runs); the next event must
    // then resolve B, not the stale cached A.
    f.buttonB->setStyle({.left = 0, .top = 0, .width = 100, .height = 50}, WidgetState::Normal);
    f.manager.resolveDirtyStyles(f.root);
    f.root->measure({800, 600});
    f.root->arrange({0, 0, 800, 600});
    f.scene->invalidateHitTestCache();
    f.moveTo(50, 25, MouseButton::None);
    EXPECT_EQ(f.buttonB->getCurrentState(), WidgetState::Hovered);
    EXPECT_EQ(f.buttonA->getCurrentState(), WidgetState::Normal);
}

TEST(EventManagerTest, CoveredCachedNodeForcesFreshHitTest) {
    EventFixture f;
    // B covers the right half of A but not a point over A's left half, so the
    // cache entry built for A is flagged as covered: a sibling is drawn on top.
    f.buttonA->setStyle({.left = 0, .top = 0, .width = 200, .height = 50}, WidgetState::Normal);
    f.buttonB->setStyle({.left = 100, .top = 0, .width = 100, .height = 50}, WidgetState::Normal);
    f.manager.resolveDirtyStyles(f.root);
    f.root->measure({800, 600});
    f.root->arrange({0, 0, 800, 600});

    f.moveTo(10, 25, MouseButton::None);  // A is topmost here; its cache is flagged covered
    EXPECT_EQ(f.buttonA->getCurrentState(), WidgetState::Hovered);

    // The flag must force a fresh scan: the cursor moved into B's half, so the
    // cached A must not keep being resolved.
    f.moveTo(150, 25, MouseButton::None);
    EXPECT_EQ(f.buttonB->getCurrentState(), WidgetState::Hovered);
    EXPECT_EQ(f.buttonA->getCurrentState(), WidgetState::Normal);
}

TEST(EventManagerTest, HasNodeInFrontDetectsOverlappingSibling) {
    EventFixture f;
    EXPECT_FALSE(f.buttonA->hasNodeInFront(f.buttonA->getGlobalBounds()));
    EXPECT_FALSE(f.buttonB->hasNodeInFront(f.buttonB->getGlobalBounds()));

    // B (drawn after A) is moved over part of A; a sibling on top is detected
    // for A but not for B itself.
    f.buttonB->setStyle({.left = 0, .top = 0, .width = 50, .height = 50}, WidgetState::Normal);
    f.manager.resolveDirtyStyles(f.root);
    f.root->measure({800, 600});
    f.root->arrange({0, 0, 800, 600});
    EXPECT_TRUE(f.buttonA->hasNodeInFront(f.buttonA->getGlobalBounds()));
    EXPECT_FALSE(f.buttonB->hasNodeInFront(f.buttonB->getGlobalBounds()));
}

TEST(EventManagerTest, HitTestCacheFallsBackWhenCachedNodeHidden) {
    EventFixture f;
    f.moveTo(50, 25, MouseButton::None);  // hover button A
    EXPECT_EQ(f.buttonA->getCurrentState(), WidgetState::Hovered);

    // Hiding the cached node makes the cache unusable; the next event must
    // resolve the node underneath it.
    f.buttonA->setVisible(false);
    f.moveTo(50, 25, MouseButton::None);
    EXPECT_EQ(f.buttonA->getCurrentState(), WidgetState::Normal);
}

TEST(EventManagerTest, RemovedHoveredNodeClearsHoverAndFiresHoverLeave) {
    EventFixture f;
    f.moveTo(50, 25, MouseButton::None);  // hover button A
    EXPECT_EQ(f.buttonA->getCurrentState(), WidgetState::Hovered);

    bool left = false;
    auto conn =
        f.buttonA->on(EventType::HoverLeave, [&](DxvEvent&, const UIContext&) { left = true; });

    f.root->removeChild(f.buttonA);
    EXPECT_EQ(f.buttonA->getCurrentState(), WidgetState::Normal);
    EXPECT_TRUE(left);
}

TEST(EventManagerTest, RemovedPressedNodeClearsPress) {
    EventFixture f;
    f.pressAt(50, 25);
    EXPECT_EQ(f.buttonA->getCurrentState(), WidgetState::Pressed);

    f.root->removeChild(f.buttonA);
    EXPECT_EQ(f.buttonA->getCurrentState(), WidgetState::Normal);
}

TEST(EventManagerTest, RemovedFocusedNodeFiresFocusLost) {
    EventFixture f;
    bool lost = false;
    auto conn =
        f.buttonA->on(EventType::FocusLost, [&](DxvEvent&, const UIContext&) { lost = true; });

    f.pressAt(50, 25);  // button A gains focus
    f.root->removeChild(f.buttonA);
    EXPECT_TRUE(lost);
    // The detached-but-alive node must not stay stuck in the Focused state.
    EXPECT_EQ(f.buttonA->getCurrentState(), WidgetState::Normal);
}

TEST(EventManagerTest, ClickedNodeKeepsFocusedStateAfterRelease) {
    EventFixture f;
    f.pressAt(50, 25);
    EXPECT_EQ(f.buttonA->getCurrentState(), WidgetState::Pressed);

    f.releaseAt(50, 25);
    EXPECT_EQ(f.buttonA->getCurrentState(), WidgetState::Focused);
}

TEST(EventManagerTest, FocusMovesToNewlyPressedNode) {
    EventFixture f;
    f.pressAt(50, 25);
    f.releaseAt(50, 25);
    EXPECT_EQ(f.buttonA->getCurrentState(), WidgetState::Focused);

    f.pressAt(250, 25);
    f.releaseAt(250, 25);
    EXPECT_EQ(f.buttonA->getCurrentState(), WidgetState::Normal);
    EXPECT_EQ(f.buttonB->getCurrentState(), WidgetState::Focused);
}

TEST(EventManagerTest, DestroyingConnectionUnsubscribesHandler) {
    EventFixture f;
    int clicks = 0;
    auto conn = f.buttonA->on(EventType::Click, [&](DxvEvent&, const UIContext&) { clicks++; });

    f.pressAt(50, 25);
    f.releaseAt(50, 25);
    EXPECT_EQ(clicks, 1);

    conn.reset();
    f.pressAt(50, 25);
    f.releaseAt(50, 25);
    EXPECT_EQ(clicks, 1);
}

TEST(EventManagerTest, RegisteringHandlerFromHandlerIsSafe) {
    EventFixture f;
    int firstCalls = 0;
    int secondCalls = 0;
    std::unique_ptr<SceneNode::Connection> second;
    auto first = f.buttonA->on(EventType::Click, [&](DxvEvent&, const UIContext&) {
        firstCalls++;
        if (!second) {
            second = f.buttonA->on(EventType::Click,
                                   [&](DxvEvent&, const UIContext&) { secondCalls++; });
        }
    });

    f.pressAt(50, 25);
    f.releaseAt(50, 25);
    // The handler registered during dispatch must not be invoked for the same
    // dispatch (it was not part of the snapshot) and must not invalidate it.
    EXPECT_EQ(firstCalls, 1);
    EXPECT_EQ(secondCalls, 0);

    f.pressAt(50, 25);
    f.releaseAt(50, 25);
    EXPECT_EQ(firstCalls, 2);
    EXPECT_EQ(secondCalls, 1);
}

TEST(EventManagerTest, HandlerCannotRedirectEventTypeOnBubble) {
    EventFixture f;
    bool rootClick = false;
    bool rootChange = false;
    auto connClick =
        f.root->on(EventType::Click, [&](DxvEvent&, const UIContext&) { rootClick = true; });
    auto connChange =
        f.root->on(EventType::Change, [&](DxvEvent&, const UIContext&) { rootChange = true; });
    // A's Click handler mutates event.type; the parent must still dispatch the
    // event that was actually raised (Click), not the mutated type (Change).
    auto connA = f.buttonA->on(EventType::Click,
                               [&](DxvEvent& e, const UIContext&) { e.type = EventType::Change; });

    f.pressAt(50, 25);
    f.releaseAt(50, 25);
    EXPECT_TRUE(rootClick);
    EXPECT_FALSE(rootChange);
}

TEST(EventManagerTest, ClickRequiresReleaseWithinDragThreshold) {
    EventFixture f;
    int clicks = 0;
    auto conn = f.buttonA->on(EventType::Click, [&](DxvEvent&, const UIContext&) { clicks++; });

    // Release far from the press point (still inside the button): no click.
    f.pressAt(50, 25);
    f.releaseAt(80, 25);
    EXPECT_EQ(clicks, 0);

    // Release near the press point: a click.
    f.pressAt(50, 25);
    f.releaseAt(52, 27);
    EXPECT_EQ(clicks, 1);
}

TEST(EventManagerTest, PerButtonPressTracking) {
    EventFixture f;
    f.pressAt(50, 25);                       // Left on A
    f.pressAt(250, 25, MouseButton::Right);  // Right on B: must not drop A's press
    EXPECT_EQ(f.buttonA->getCurrentState(), WidgetState::Pressed);
    EXPECT_EQ(f.buttonB->getCurrentState(), WidgetState::Pressed);

    f.releaseAt(250, 25, MouseButton::Right);
    EXPECT_EQ(f.buttonA->getCurrentState(), WidgetState::Pressed);
    // The right-click focused B, so it keeps the Focused state after the release.
    EXPECT_EQ(f.buttonB->getCurrentState(), WidgetState::Focused);
}

TEST(EventManagerTest, DxvEventTargetAccessors) {
    EventFixture f;
    DxvEvent e;
    e.target = f.buttonA;
    e.currentTarget = f.root;
    e.relatedNode = f.buttonB;

    ASSERT_NE(e.getTarget(), nullptr);
    EXPECT_EQ(e.getTarget(), f.buttonA);
    EXPECT_EQ(e.getTargetId(), "btn_a");
    EXPECT_EQ(e.getCurrentTarget(), f.root);
    EXPECT_EQ(e.getRelatedNode(), f.buttonB);
}

TEST(EventManagerTest, DxvEventTargetAccessorsExpired) {
    EventFixture f;
    DxvEvent e;
    {
        auto node = std::make_shared<SceneNode>("temp");
        e.target = node;
        e.relatedNode = node;
    }
    EXPECT_EQ(e.getTarget(), nullptr);
    EXPECT_TRUE(e.getTargetId().empty());
    EXPECT_EQ(e.getRelatedNode(), nullptr);
}

TEST(EventManagerTest, DxvEventCurrentTargetTracksDispatchNode) {
    EventFixture f;
    std::shared_ptr<SceneNode> captured;
    auto conn = f.root->on(EventType::Click,
                           [&](DxvEvent& e, const UIContext&) { captured = e.getCurrentTarget(); });
    f.pressAt(50, 25);
    f.releaseAt(50, 25);
    EXPECT_EQ(captured, f.root);
}

// --- Default action (onEvent) semantics ---
namespace {

// Test node whose onEvent() records whether it ran and what type it saw. Used to
// verify the DOM-style interplay between user listeners and the widget default.
struct EventProbeNode : SceneNode {
    using SceneNode::SceneNode;
    bool defaultActionRan = false;
    EventType defaultActionType = EventType::None;
    bool stopInDefault = false;

    void onEvent(DxvEvent& event) override {
        // Only the tests' Click event counts: the Attach dispatch triggered by
        // addChild() must not pollute the recorded flag.
        if (event.type != EventType::Click) {
            return;
        }
        defaultActionRan = true;
        defaultActionType = event.type;
        if (stopInDefault) {
            event.stopPropagation();
        }
    }
};

void addProbe(EventFixture& f, const std::shared_ptr<EventProbeNode>& node) {
    f.root->addChild(node);
    f.manager.resolveDirtyStyles(f.root);
}

}  // namespace

TEST(EventManagerTest, DefaultActionRunsAfterUserListeners) {
    EventFixture f;
    auto node = std::make_shared<EventProbeNode>("probe");
    addProbe(f, node);
    int userCalls = 0;
    auto conn = node->on(EventType::Click, [&](DxvEvent&, const UIContext&) { userCalls++; });

    DxvEvent e;
    e.type = EventType::Click;
    e.target = node;
    node->dispatchEvent(e);

    EXPECT_EQ(userCalls, 1);
    EXPECT_TRUE(node->defaultActionRan);
}

TEST(EventManagerTest, PreventDefaultSkipsDefaultAction) {
    EventFixture f;
    auto node = std::make_shared<EventProbeNode>("probe");
    addProbe(f, node);
    auto conn =
        node->on(EventType::Click, [&](DxvEvent& e, const UIContext&) { e.preventDefault(); });

    DxvEvent e;
    e.type = EventType::Click;
    e.target = node;
    node->dispatchEvent(e);

    EXPECT_FALSE(node->defaultActionRan);
    EXPECT_TRUE(e.isDefaultPrevented());
}

TEST(EventManagerTest, StopPropagationKeepsDefaultActionAndBlocksParent) {
    EventFixture f;
    auto node = std::make_shared<EventProbeNode>("probe");
    addProbe(f, node);
    bool parentSeen = false;
    auto parentConn =
        f.root->on(EventType::Click, [&](DxvEvent&, const UIContext&) { parentSeen = true; });
    auto conn =
        node->on(EventType::Click, [&](DxvEvent& e, const UIContext&) { e.stopPropagation(); });

    DxvEvent e;
    e.type = EventType::Click;
    e.target = node;
    node->dispatchEvent(e);

    EXPECT_TRUE(node->defaultActionRan);
    EXPECT_FALSE(parentSeen);
    EXPECT_TRUE(e.isPropagationStopped());
}

TEST(EventManagerTest, StopImmediatePropagationBlocksSiblingListenersAndParent) {
    EventFixture f;
    auto node = std::make_shared<EventProbeNode>("probe");
    addProbe(f, node);
    bool firstRan = false;
    bool secondRan = false;
    bool parentSeen = false;
    auto conn1 = node->on(EventType::Click, [&](DxvEvent& e, const UIContext&) {
        firstRan = true;
        e.stopImmediatePropagation();
    });
    auto conn2 = node->on(EventType::Click, [&](DxvEvent&, const UIContext&) { secondRan = true; });
    auto parentConn =
        f.root->on(EventType::Click, [&](DxvEvent&, const UIContext&) { parentSeen = true; });

    DxvEvent e;
    e.type = EventType::Click;
    e.target = node;
    node->dispatchEvent(e);

    EXPECT_TRUE(firstRan);
    EXPECT_FALSE(secondRan);
    // DOM semantics: stopImmediatePropagation cancels neither the default action
    // nor other listeners' effects after the stopping one, but it does stop the
    // propagation chain.
    EXPECT_TRUE(node->defaultActionRan);
    EXPECT_FALSE(parentSeen);
    EXPECT_TRUE(e.isImmediatePropagationStopped());
}

TEST(EventManagerTest, DefaultActionCanStopPropagation) {
    EventFixture f;
    auto node = std::make_shared<EventProbeNode>("probe");
    node->stopInDefault = true;
    addProbe(f, node);
    bool parentSeen = false;
    auto parentConn =
        f.root->on(EventType::Click, [&](DxvEvent&, const UIContext&) { parentSeen = true; });

    DxvEvent e;
    e.type = EventType::Click;
    e.target = node;
    node->dispatchEvent(e);

    // A widget that consumes the event in its default action stops the bubble,
    // exactly like TextEdit's editing keys.
    EXPECT_TRUE(node->defaultActionRan);
    EXPECT_FALSE(parentSeen);
}

TEST(EventManagerTest, DefaultActionSeesOriginalEventType) {
    EventFixture f;
    auto node = std::make_shared<EventProbeNode>("probe");
    addProbe(f, node);
    auto conn = node->on(EventType::Click,
                         [&](DxvEvent& e, const UIContext&) { e.type = EventType::Change; });

    DxvEvent e;
    e.type = EventType::Click;
    e.target = node;
    node->dispatchEvent(e);

    EXPECT_TRUE(node->defaultActionRan);
    EXPECT_EQ(node->defaultActionType, EventType::Click);
}

TEST(EventManagerTest, DisabledNodeReceivesNoHoverOrPress) {
    EventFixture f;
    f.buttonA->setEnabled(false);

    f.moveTo(50, 25, MouseButton::None);
    EXPECT_EQ(f.buttonA->getCurrentState(), WidgetState::Disabled);

    f.pressAt(50, 25);
    EXPECT_EQ(f.buttonA->getCurrentState(), WidgetState::Disabled);
}

TEST(EventManagerTest, DisabledNodeReceivesNoClick) {
    EventFixture f;
    bool clicked = false;
    auto conn =
        f.buttonA->on(EventType::Click, [&](DxvEvent&, const UIContext&) { clicked = true; });
    f.buttonA->setEnabled(false);

    f.pressAt(50, 25);
    f.releaseAt(50, 25);
    EXPECT_FALSE(clicked);
}

TEST(EventManagerTest, DisabledNodeDoesNotTakeFocus) {
    EventFixture f;
    f.pressAt(250, 25);  // B gains press + focus
    EXPECT_EQ(f.scene->getFocusedNode(), f.buttonB);

    f.buttonA->setEnabled(false);
    f.pressAt(50, 25);  // press over the disabled A acts like a click-outside
    EXPECT_EQ(f.scene->getFocusedNode(), nullptr);
    EXPECT_EQ(f.buttonA->getCurrentState(), WidgetState::Disabled);
    EXPECT_EQ(f.buttonB->getCurrentState(), WidgetState::Normal);
}

TEST(EventManagerTest, DisablingFocusedNodeReleasesFocus) {
    EventFixture f;
    f.pressAt(50, 25);  // A gains focus
    EXPECT_EQ(f.scene->getFocusedNode(), f.buttonA);

    f.buttonA->setEnabled(false);
    EXPECT_EQ(f.scene->getFocusedNode(), nullptr);
    EXPECT_EQ(f.buttonA->getCurrentState(), WidgetState::Disabled);
}

TEST(EventManagerTest, DisablingHoveredNodeClearsHover) {
    EventFixture f;
    f.moveTo(50, 25, MouseButton::None);
    EXPECT_EQ(f.buttonA->getCurrentState(), WidgetState::Hovered);

    f.buttonA->setEnabled(false);
    EXPECT_EQ(f.buttonA->getCurrentState(), WidgetState::Disabled);

    // The underlying hover flag was cleared, so re-enabling returns to Normal.
    f.buttonA->setEnabled(true);
    EXPECT_EQ(f.buttonA->getCurrentState(), WidgetState::Normal);
}

TEST(EventManagerTest, ReenabledNodeIsInteractiveAgain) {
    EventFixture f;
    f.buttonA->setEnabled(false);
    f.buttonA->setEnabled(true);

    f.moveTo(50, 25, MouseButton::None);
    EXPECT_EQ(f.buttonA->getCurrentState(), WidgetState::Hovered);
}
