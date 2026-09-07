#include <DxvUI/event/EventTarget.h>
#include <gtest/gtest.h>

using DxvUI::DxvEvent;
using DxvUI::EventTarget;
using DxvUI::EventType;

namespace {

TEST(EventTargetTest, DefaultTargetHasNoHandlers) {
    EventTarget t;
    EXPECT_FALSE(t.hasHandler(EventType::Click));
    EXPECT_FALSE(t.hasAnyCaptureHandlers());
    EXPECT_FALSE(t.hasCaptureHandler(EventType::Click));
}

TEST(EventTargetTest, AddHandlerRegistersUniqueIds) {
    EventTarget t;
    const auto first = t.addHandler(EventType::Click, [](DxvEvent&, const DxvUI::UIContext&) {});
    const auto second = t.addHandler(EventType::Click, [](DxvEvent&, const DxvUI::UIContext&) {});
    EXPECT_NE(first, second);
    EXPECT_TRUE(t.hasHandler(EventType::Click));
}

TEST(EventTargetTest, RemoveHandlerIsIdempotent) {
    EventTarget t;
    const auto id = t.addHandler(EventType::Click, [](DxvEvent&, const DxvUI::UIContext&) {});
    t.removeHandler(EventType::Click, id);
    t.removeHandler(EventType::Click, id);
    t.removeHandler(EventType::Click, 9999);
    EXPECT_FALSE(t.hasHandler(EventType::Click));
}

TEST(EventTargetTest, RemoveHandlerCoversEmptyAndUnknownTypes) {
    EventTarget t;
    // Removing from a type that was never registered, and an id that was never
    // handed out, are both no-ops (the Connection destructor relies on this).
    t.removeHandler(EventType::MouseDown, 42);
    t.removeHandler(EventType::Click, 42);
    EXPECT_FALSE(t.hasHandler(EventType::Click));
}

TEST(EventTargetTest, HandlerLookupIsPerType) {
    EventTarget t;
    t.addHandler(EventType::Click, [](DxvEvent&, const DxvUI::UIContext&) {});
    EXPECT_TRUE(t.hasHandler(EventType::Click));
    EXPECT_FALSE(t.hasHandler(EventType::MouseDown));
    t.removeHandler(EventType::MouseDown, 0);
    EXPECT_TRUE(t.hasHandler(EventType::Click));
}

TEST(EventTargetTest, CaptureHandlersAllocateLazily) {
    EventTarget t;
    EXPECT_FALSE(t.hasAnyCaptureHandlers());
    t.addCaptureHandler(EventType::Click, [](DxvEvent&, const DxvUI::UIContext&) {});
    EXPECT_TRUE(t.hasAnyCaptureHandlers());
    EXPECT_TRUE(t.hasCaptureHandler(EventType::Click));
    EXPECT_FALSE(t.hasCaptureHandler(EventType::MouseDown));
}

TEST(EventTargetTest, RemovingLastCaptureHandlerFreesTheMap) {
    EventTarget t;
    const auto id =
        t.addCaptureHandler(EventType::Click, [](DxvEvent&, const DxvUI::UIContext&) {});
    EXPECT_TRUE(t.hasAnyCaptureHandlers());
    t.removeCaptureHandler(EventType::Click, id);
    // The lazily-allocated map is returned to the empty state: the capture fast
    // path (hasAnyCaptureHandlers) must come back false after the last removal.
    EXPECT_FALSE(t.hasAnyCaptureHandlers());
    EXPECT_FALSE(t.hasCaptureHandler(EventType::Click));
    // And a duplicate removal stays a no-op.
    t.removeCaptureHandler(EventType::Click, id);
    EXPECT_FALSE(t.hasAnyCaptureHandlers());
}

TEST(EventTargetTest, RegularAndCaptureStorageIsSeparate) {
    EventTarget t;
    t.addHandler(EventType::Click, [](DxvEvent&, const DxvUI::UIContext&) {});
    EXPECT_TRUE(t.hasHandler(EventType::Click));
    EXPECT_FALSE(t.hasCaptureHandler(EventType::Click));
    EXPECT_FALSE(t.hasAnyCaptureHandlers());

    const auto captureId =
        t.addCaptureHandler(EventType::MouseDown, [](DxvEvent&, const DxvUI::UIContext&) {});
    EXPECT_TRUE(t.hasCaptureHandler(EventType::MouseDown));
    EXPECT_TRUE(t.hasHandler(EventType::Click));
    EXPECT_FALSE(t.hasHandler(EventType::MouseDown));
    t.removeCaptureHandler(EventType::MouseDown, captureId);
    EXPECT_FALSE(t.hasCaptureHandler(EventType::MouseDown));
    EXPECT_TRUE(t.hasHandler(EventType::Click));
}

}  // namespace