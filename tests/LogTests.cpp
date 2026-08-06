#include <gtest/gtest.h>

#include "DxvUI/Log.h"

using namespace DxvUI;

TEST(LogTest, InitIsIdempotent) {
    // The global test environment already calls Log::init() once before any
    // test runs; a second call must be a no-op and must not throw (spdlog would
    // otherwise fail on the duplicate logger name).
    EXPECT_NO_THROW(Log::init());
}
