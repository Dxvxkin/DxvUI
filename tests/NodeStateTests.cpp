#include <DxvUI/NodeState.h>
#include <gtest/gtest.h>

using DxvUI::NodeState;

namespace {

TEST(NodeStateTest, DefaultStateIsEnabledAndVisible) {
    NodeState s;
    EXPECT_TRUE(s.test(NodeState::Flag::Enabled));
    EXPECT_TRUE(s.test(NodeState::Flag::Visible));
    EXPECT_FALSE(s.test(NodeState::Flag::Hovered));
    EXPECT_FALSE(s.test(NodeState::Flag::Pressed));
    EXPECT_FALSE(s.test(NodeState::Flag::Focused));
}

TEST(NodeStateTest, TakeSetsAndClearsFlag) {
    NodeState s;
    EXPECT_TRUE(s.take(NodeState::Flag::Hovered, true));
    EXPECT_TRUE(s.test(NodeState::Flag::Hovered));
    EXPECT_TRUE(s.take(NodeState::Flag::Hovered, false));
    EXPECT_FALSE(s.test(NodeState::Flag::Hovered));
}

TEST(NodeStateTest, TakeReportsNoChangeForSameValue) {
    NodeState s;
    // Enabled defaults to true; requesting true again changes nothing.
    EXPECT_FALSE(s.take(NodeState::Flag::Enabled, true));
    EXPECT_TRUE(s.test(NodeState::Flag::Enabled));
    EXPECT_TRUE(s.take(NodeState::Flag::Enabled, false));
    EXPECT_FALSE(s.take(NodeState::Flag::Enabled, false));
}

TEST(NodeStateTest, FlagsAreIndependentAndCombine) {
    NodeState s;
    s.take(NodeState::Flag::Focused, true);
    s.take(NodeState::Flag::Hovered, true);
    // Both facts hold at once: the state is combined, not collapsed.
    EXPECT_TRUE(s.testAny(static_cast<uint8_t>(NodeState::Flag::Focused) |
                          static_cast<uint8_t>(NodeState::Flag::Hovered)));
}

TEST(NodeStateTest, TestAnyTestsAnyOfSeveralFlags) {
    NodeState s;
    s.take(NodeState::Flag::Pressed, true);
    EXPECT_TRUE(s.testAny(static_cast<uint8_t>(NodeState::Flag::Focused) |
                          static_cast<uint8_t>(NodeState::Flag::Pressed)));
    // Only one of the tested flags is set: still true.
    EXPECT_TRUE(s.testAny(static_cast<uint8_t>(NodeState::Flag::Pressed) |
                          static_cast<uint8_t>(NodeState::Flag::Hovered)));
    // A flag that is not set reports false.
    EXPECT_FALSE(s.testAny(static_cast<uint8_t>(NodeState::Flag::Focused)));
}

TEST(NodeStateTest, RawExposesCombinedBitmask) {
    NodeState s;
    s.take(NodeState::Flag::Focused, true);
    s.take(NodeState::Flag::Hovered, true);
    s.take(NodeState::Flag::Enabled, false);
    const uint8_t expected = static_cast<uint8_t>(NodeState::Flag::Focused) |
                             static_cast<uint8_t>(NodeState::Flag::Hovered) |
                             static_cast<uint8_t>(NodeState::Flag::Visible);
    EXPECT_EQ(s.raw(), expected);
}

}  // namespace
