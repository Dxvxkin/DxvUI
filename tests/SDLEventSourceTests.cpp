#include <SDL.h>
#include <gtest/gtest.h>

#include "DxvUI/DxvEvent.h"
#include "DxvUI/sources/SDLEventSource.h"

using namespace DxvUI;

namespace {

SDL_Event makeKeyEvent(int sdlKeycode) {
    SDL_Event e = {};
    e.type = SDL_KEYDOWN;
    e.key.keysym.sym = sdlKeycode;
    e.key.keysym.mod = KMOD_CTRL;
    e.key.repeat = 1;
    return e;
}

TEST(SDLEventSourceTest, MapsSdlKeysToBackendNeutralKeyCode) {
    SDLEventSource source;

    DxvEvent out;
    ASSERT_TRUE(source.processEvent(makeKeyEvent(SDLK_a), out));
    EXPECT_EQ(out.type, EventType::KeyDown);
    EXPECT_EQ(out.key.sym, KeyCode::A);
    EXPECT_EQ(out.key.mod, KMOD_CTRL);
    EXPECT_EQ(out.key.repeat, 1);

    ASSERT_TRUE(source.processEvent(makeKeyEvent(SDLK_b), out));
    EXPECT_EQ(out.key.sym, KeyCode::B);

    ASSERT_TRUE(source.processEvent(makeKeyEvent(SDLK_RETURN), out));
    EXPECT_EQ(out.key.sym, KeyCode::Enter);
    ASSERT_TRUE(source.processEvent(makeKeyEvent(SDLK_KP_ENTER), out));
    EXPECT_EQ(out.key.sym, KeyCode::Enter);

    ASSERT_TRUE(source.processEvent(makeKeyEvent(SDLK_LEFT), out));
    EXPECT_EQ(out.key.sym, KeyCode::Left);
    ASSERT_TRUE(source.processEvent(makeKeyEvent(SDLK_HOME), out));
    EXPECT_EQ(out.key.sym, KeyCode::Home);
    ASSERT_TRUE(source.processEvent(makeKeyEvent(SDLK_0), out));
    EXPECT_EQ(out.key.sym, KeyCode::Digit0);
    ASSERT_TRUE(source.processEvent(makeKeyEvent(SDLK_9), out));
    EXPECT_EQ(out.key.sym, KeyCode::Digit9);
    ASSERT_TRUE(source.processEvent(makeKeyEvent(SDLK_F1), out));
    EXPECT_EQ(out.key.sym, KeyCode::F1);
    ASSERT_TRUE(source.processEvent(makeKeyEvent(SDLK_F12), out));
    EXPECT_EQ(out.key.sym, KeyCode::F12);

    // Неизвестная клавиша отображается в Unknown.
    ASSERT_TRUE(source.processEvent(makeKeyEvent(SDLK_APPLICATION), out));
    EXPECT_EQ(out.key.sym, KeyCode::Unknown);
}

}  // namespace
