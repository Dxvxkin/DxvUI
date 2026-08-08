#include "DxvUI/sources/SDLEventSource.h"

#include <SDL.h>

namespace DxvUI {

SDLEventSource::~SDLEventSource() = default;

namespace {

KeyCode translateKeySym(int sdlKeycode) {
    if (sdlKeycode >= SDLK_a && sdlKeycode <= SDLK_z) {
        return static_cast<KeyCode>(static_cast<int>(KeyCode::A) + (sdlKeycode - SDLK_a));
    }
    if (sdlKeycode >= SDLK_0 && sdlKeycode <= SDLK_9) {
        return static_cast<KeyCode>(static_cast<int>(KeyCode::Digit0) + (sdlKeycode - SDLK_0));
    }
    if (sdlKeycode >= SDLK_F1 && sdlKeycode <= SDLK_F12) {
        return static_cast<KeyCode>(static_cast<int>(KeyCode::F1) + (sdlKeycode - SDLK_F1));
    }
    switch (sdlKeycode) {
        case SDLK_BACKSPACE:
            return KeyCode::Backspace;
        case SDLK_TAB:
            return KeyCode::Tab;
        case SDLK_RETURN:
        case SDLK_KP_ENTER:
            return KeyCode::Enter;
        case SDLK_ESCAPE:
            return KeyCode::Escape;
        case SDLK_SPACE:
            return KeyCode::Space;
        case SDLK_LEFT:
            return KeyCode::Left;
        case SDLK_RIGHT:
            return KeyCode::Right;
        case SDLK_UP:
            return KeyCode::Up;
        case SDLK_DOWN:
            return KeyCode::Down;
        case SDLK_HOME:
            return KeyCode::Home;
        case SDLK_END:
            return KeyCode::End;
        case SDLK_PAGEUP:
            return KeyCode::PageUp;
        case SDLK_PAGEDOWN:
            return KeyCode::PageDown;
        case SDLK_DELETE:
            return KeyCode::Delete;
        case SDLK_INSERT:
            return KeyCode::Insert;
        default:
            return KeyCode::Unknown;
    }
}

static void translateMouseButtonEvent(DxvEvent& dxvEvent,
                                      const SDL_MouseButtonEvent& sdlButtonEvent) {
    dxvEvent.mouse.x = sdlButtonEvent.x;
    dxvEvent.mouse.y = sdlButtonEvent.y;
    switch (sdlButtonEvent.button) {
        case SDL_BUTTON_LEFT:
            dxvEvent.mouse.button = MouseButton::Left;
            break;
        case SDL_BUTTON_MIDDLE:
            dxvEvent.mouse.button = MouseButton::Middle;
            break;
        case SDL_BUTTON_RIGHT:
            dxvEvent.mouse.button = MouseButton::Right;
            break;
        default:
            dxvEvent.mouse.button = MouseButton::None;
            break;
    }
}

static void translateKeyboardEvent(DxvEvent& dxvEvent, const SDL_KeyboardEvent& sdlKeyEvent) {
    dxvEvent.key.sym = translateKeySym(sdlKeyEvent.keysym.sym);
    dxvEvent.key.mod = sdlKeyEvent.keysym.mod;
    dxvEvent.key.repeat = sdlKeyEvent.repeat;
}

}  // namespace

bool SDLEventSource::pollEvent(DxvEvent& event) {
    SDL_Event sdlEvent;
    if (SDL_PollEvent(&sdlEvent) != 0) {
        return processEvent(sdlEvent, event);
    }
    event.type = EventType::None;
    return false;
}

bool SDLEventSource::processEvent(const SDL_Event& sdlEvent, DxvEvent& dxvEvent) {
    dxvEvent.type = EventType::None;  // Reset event by default

    switch (sdlEvent.type) {
        case SDL_QUIT:
            dxvEvent.type = EventType::Quit;
            return true;

        case SDL_MOUSEBUTTONDOWN:
            dxvEvent.type = EventType::MouseDown;
            translateMouseButtonEvent(dxvEvent, sdlEvent.button);
            // Capture the mouse so the corresponding button-up is delivered even
            // if the button is released outside the window; otherwise the pressed
            // state would stick forever.
            SDL_CaptureMouse(SDL_TRUE);
            return true;

        case SDL_MOUSEBUTTONUP:
            dxvEvent.type = EventType::MouseUp;
            translateMouseButtonEvent(dxvEvent, sdlEvent.button);
            SDL_CaptureMouse(SDL_FALSE);
            return true;

        case SDL_MOUSEMOTION:
            dxvEvent.type = EventType::MouseMove;
            dxvEvent.mouse.x = sdlEvent.motion.x;
            dxvEvent.mouse.y = sdlEvent.motion.y;
            if (sdlEvent.motion.state & SDL_BUTTON_LMASK)
                dxvEvent.mouse.button = MouseButton::Left;
            else if (sdlEvent.motion.state & SDL_BUTTON_MMASK)
                dxvEvent.mouse.button = MouseButton::Middle;
            else if (sdlEvent.motion.state & SDL_BUTTON_RMASK)
                dxvEvent.mouse.button = MouseButton::Right;
            else
                dxvEvent.mouse.button = MouseButton::None;
            return true;

        case SDL_KEYDOWN:
            dxvEvent.type = EventType::KeyDown;
            translateKeyboardEvent(dxvEvent, sdlEvent.key);
            return true;

        case SDL_KEYUP:
            dxvEvent.type = EventType::KeyUp;
            translateKeyboardEvent(dxvEvent, sdlEvent.key);
            return true;

        case SDL_TEXTINPUT:
            dxvEvent.type = EventType::TextInput;
            dxvEvent.text = sdlEvent.text.text;
            return true;
    }

    return false;
}

}  // namespace DxvUI
