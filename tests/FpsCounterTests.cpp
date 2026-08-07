#include <gtest/gtest.h>

#include <chrono>

#include "DxvUI/FpsCounter.h"

using namespace DxvUI;

namespace {

// A clock whose now() is fully controlled by the test, so frame timings can be
// simulated without waiting real time.
struct FakeClock {
    using time_point = std::chrono::steady_clock::time_point;
    using duration = std::chrono::steady_clock::duration;

    static time_point now() { return current; }
    static time_point current;
};

FakeClock::time_point FakeClock::current = std::chrono::steady_clock::time_point();

void advance(FakeClock::duration d) { FakeClock::current += d; }

}  // namespace

TEST(FpsCounterTest, SteadyFrameRateIsReportedAccurately) {
    FakeClock::current = std::chrono::steady_clock::time_point();
    FpsCounter<FakeClock> counter;

    const auto frame = std::chrono::milliseconds(16);  // ~62.5 fps
    counter.tick();                                    // baseline only
    for (int i = 0; i < 120; ++i) {
        advance(frame);
        counter.tick();
    }

    EXPECT_NEAR(counter.getFrameTimeMs(), 16.0f, 0.1f);
    EXPECT_NEAR(counter.getFps(), 1000.0f / 16.0f, 0.5f);
}

TEST(FpsCounterTest, FrameRateTracksChangedInterval) {
    FakeClock::current = std::chrono::steady_clock::time_point();
    FpsCounter<FakeClock> counter;

    const auto slow = std::chrono::milliseconds(16);
    counter.tick();
    for (int i = 0; i < 60; ++i) {
        advance(slow);
        counter.tick();
    }
    EXPECT_NEAR(counter.getFps(), 1000.0f / 16.0f, 0.5f);

    // After the window refills with fast frames, the average converges to the
    // new rate instead of being stuck on the old one.
    const auto fast = std::chrono::milliseconds(8);
    for (int i = 0; i < 60; ++i) {
        advance(fast);
        counter.tick();
    }
    EXPECT_NEAR(counter.getFps(), 125.0f, 0.5f);
}

TEST(FpsCounterTest, ResetClearsAllSamples) {
    FakeClock::current = std::chrono::steady_clock::time_point();
    FpsCounter<FakeClock> counter;

    counter.tick();
    advance(std::chrono::milliseconds(16));
    counter.tick();
    EXPECT_NEAR(counter.getFps(), 62.5f, 0.5f);

    counter.reset();
    EXPECT_FLOAT_EQ(counter.getFrameTimeMs(), 0.0f);
    EXPECT_FLOAT_EQ(counter.getFps(), 0.0f);

    // A fresh baseline is needed before any value can be reported again.
    counter.tick();
    EXPECT_FLOAT_EQ(counter.getFps(), 0.0f);
    advance(std::chrono::milliseconds(10));
    counter.tick();
    EXPECT_NEAR(counter.getFps(), 100.0f, 0.5f);
}
