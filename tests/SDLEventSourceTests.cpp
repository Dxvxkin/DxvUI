#include <SDL.h>
#include <gtest/gtest.h>

#include <cstring>

#include "DxvUI/DxvEvent.h"
#include "DxvUI/sources/SDLEventSource.h"

using namespace DxvUI;

namespace {

// The mouse-button translation path calls SDL_CaptureMouse(), which needs the
// video subsystem initialized; a plain SDL_Event does not require a window.
class SdlEnvironment : public ::testing::Environment {
   public:
    void SetUp() override { SDL_InitSubSystem(SDL_INIT_VIDEO); }
    void TearDown() override { SDL_Quit(); }
};

::testing::Environment* const g_sdl_environment =
    ::testing::AddGlobalTestEnvironment(new SdlEnvironment);

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

SDL_Event makeMouseButtonEvent(Uint32 type, Uint8 button, int x, int y) {
    SDL_Event e = {};
    e.type = type;
    e.button.button = button;
    e.button.x = x;
    e.button.y = y;
    return e;
}

SDL_Event makeMotionEvent(int x, int y, Uint32 state) {
    SDL_Event e = {};
    e.type = SDL_MOUSEMOTION;
    e.motion.x = x;
    e.motion.y = y;
    e.motion.state = state;
    return e;
}

SDL_Event makeWheelEvent(Sint32 x, Sint32 y) {
    SDL_Event e = {};
    e.type = SDL_MOUSEWHEEL;
    e.wheel.x = x;
    e.wheel.y = y;
    return e;
}

SDL_Event makeTextInputEvent(const char* text) {
    SDL_Event e = {};
    e.type = SDL_TEXTINPUT;
    std::strncpy(e.text.text, text, sizeof(e.text.text) - 1);
    return e;
}

SDL_Event makeKeyUpEvent(int sdlKeycode) {
    SDL_Event e = {};
    e.type = SDL_KEYUP;
    e.key.keysym.sym = sdlKeycode;
    e.key.keysym.mod = KMOD_ALT;
    e.key.repeat = 2;
    return e;
}

TEST(SDLEventSourceTest, MapsMouseButtonDown) {
    SDLEventSource source;
    DxvEvent out;
    ASSERT_TRUE(source.processEvent(
        makeMouseButtonEvent(SDL_MOUSEBUTTONDOWN, SDL_BUTTON_LEFT, 12, 34), out));
    EXPECT_EQ(out.type, EventType::MouseDown);
    EXPECT_EQ(out.mouse.x, 12);
    EXPECT_EQ(out.mouse.y, 34);
    EXPECT_EQ(out.mouse.button, MouseButton::Left);
}

TEST(SDLEventSourceTest, MapsMouseButtonDownRightMiddleAndUnknown) {
    SDLEventSource source;
    DxvEvent out;

    ASSERT_TRUE(source.processEvent(
        makeMouseButtonEvent(SDL_MOUSEBUTTONDOWN, SDL_BUTTON_RIGHT, 0, 0), out));
    EXPECT_EQ(out.type, EventType::MouseDown);
    EXPECT_EQ(out.mouse.button, MouseButton::Right);

    ASSERT_TRUE(source.processEvent(
        makeMouseButtonEvent(SDL_MOUSEBUTTONDOWN, SDL_BUTTON_MIDDLE, 0, 0), out));
    EXPECT_EQ(out.mouse.button, MouseButton::Middle);

    ASSERT_TRUE(
        source.processEvent(makeMouseButtonEvent(SDL_MOUSEBUTTONDOWN, SDL_BUTTON_X1, 0, 0), out));
    EXPECT_EQ(out.mouse.button, MouseButton::None);
}

TEST(SDLEventSourceTest, MapsMouseButtonUp) {
    SDLEventSource source;
    DxvEvent out;
    ASSERT_TRUE(
        source.processEvent(makeMouseButtonEvent(SDL_MOUSEBUTTONUP, SDL_BUTTON_RIGHT, 5, 6), out));
    EXPECT_EQ(out.type, EventType::MouseUp);
    EXPECT_EQ(out.mouse.x, 5);
    EXPECT_EQ(out.mouse.y, 6);
    EXPECT_EQ(out.mouse.button, MouseButton::Right);
}

TEST(SDLEventSourceTest, MapsMouseMotionButtons) {
    SDLEventSource source;
    DxvEvent out;

    ASSERT_TRUE(source.processEvent(makeMotionEvent(3, 4, SDL_BUTTON_LMASK), out));
    EXPECT_EQ(out.type, EventType::MouseMove);
    EXPECT_EQ(out.mouse.x, 3);
    EXPECT_EQ(out.mouse.y, 4);
    EXPECT_EQ(out.mouse.button, MouseButton::Left);

    ASSERT_TRUE(source.processEvent(makeMotionEvent(3, 4, SDL_BUTTON_MMASK), out));
    EXPECT_EQ(out.mouse.button, MouseButton::Middle);

    ASSERT_TRUE(source.processEvent(makeMotionEvent(3, 4, SDL_BUTTON_RMASK), out));
    EXPECT_EQ(out.mouse.button, MouseButton::Right);

    ASSERT_TRUE(source.processEvent(makeMotionEvent(3, 4, 0), out));
    EXPECT_EQ(out.mouse.button, MouseButton::None);
}

TEST(SDLEventSourceTest, MapsMouseWheel) {
    SDLEventSource source;
    DxvEvent out;

    // Vertical wheel: y = +1 means scrolled up (away from the user).
    ASSERT_TRUE(source.processEvent(makeWheelEvent(3, -1), out));
    EXPECT_EQ(out.type, EventType::MouseWheel);
    EXPECT_EQ(out.mouse.dx, 3);
    EXPECT_EQ(out.mouse.dy, -1);

    // Horizontal wheel is carried in dx.
    ASSERT_TRUE(source.processEvent(makeWheelEvent(-2, 0), out));
    EXPECT_EQ(out.type, EventType::MouseWheel);
    EXPECT_EQ(out.mouse.dx, -2);
    EXPECT_EQ(out.mouse.dy, 0);
}

TEST(SDLEventSourceTest, MapsKeyUp) {
    SDLEventSource source;
    DxvEvent out;
    ASSERT_TRUE(source.processEvent(makeKeyUpEvent(SDLK_a), out));
    EXPECT_EQ(out.type, EventType::KeyUp);
    EXPECT_EQ(out.key.sym, KeyCode::A);
    EXPECT_EQ(out.key.mod, KMOD_ALT);
    EXPECT_EQ(out.key.repeat, 2);
}

TEST(SDLEventSourceTest, MapsTextInput) {
    SDLEventSource source;
    DxvEvent out;
    ASSERT_TRUE(source.processEvent(makeTextInputEvent("h\u00e9llo"), out));
    EXPECT_EQ(out.type, EventType::TextInput);
    EXPECT_EQ(out.text, "h\u00e9llo");
}

TEST(SDLEventSourceTest, MapsQuit) {
    SDL_Event e = {};
    e.type = SDL_QUIT;
    SDLEventSource source;
    DxvEvent out;
    ASSERT_TRUE(source.processEvent(e, out));
    EXPECT_EQ(out.type, EventType::Quit);
}

TEST(SDLEventSourceTest, IgnoresUnhandledEventTypes) {
    SDL_Event e = {};
    e.type = SDL_WINDOWEVENT;
    SDLEventSource source;
    DxvEvent out;
    EXPECT_FALSE(source.processEvent(e, out));
    EXPECT_EQ(out.type, EventType::None);
}

}  // namespace
