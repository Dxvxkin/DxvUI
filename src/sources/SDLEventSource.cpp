#include "DxvUI/sources/SDLEventSource.h"
#include <SDL.h>

namespace DxvUI {

    SDLEventSource::~SDLEventSource() = default;

    static void translateMouseButtonEvent(DxvEvent& dxvEvent, const SDL_MouseButtonEvent& sdlButtonEvent) {
        dxvEvent.mouse.x = sdlButtonEvent.x;
        dxvEvent.mouse.y = sdlButtonEvent.y;
        switch (sdlButtonEvent.button) {
            case SDL_BUTTON_LEFT:   dxvEvent.mouse.button = MouseButton::Left;   break;
            case SDL_BUTTON_MIDDLE: dxvEvent.mouse.button = MouseButton::Middle; break;
            case SDL_BUTTON_RIGHT:  dxvEvent.mouse.button = MouseButton::Right;  break;
            default:                dxvEvent.mouse.button = MouseButton::None;   break;
        }
    }

    static void translateKeyboardEvent(DxvEvent& dxvEvent, const SDL_KeyboardEvent& sdlKeyEvent) {
        dxvEvent.key.sym = sdlKeyEvent.keysym.sym;
        dxvEvent.key.scancode = sdlKeyEvent.keysym.scancode;
        dxvEvent.key.mod = sdlKeyEvent.keysym.mod;
    }

    bool SDLEventSource::pollEvent(DxvEvent& event) {
        SDL_Event sdlEvent;
        if (SDL_PollEvent(&sdlEvent) != 0) {
            return processEvent(sdlEvent, event);
        }
        event.type = EventType::None;
        return false;
    }

    bool SDLEventSource::processEvent(const SDL_Event& sdlEvent, DxvEvent& dxvEvent) {
        dxvEvent.type = EventType::None; // Reset event by default

        switch (sdlEvent.type) {
            case SDL_QUIT:
                dxvEvent.type = EventType::Quit;
                return true;

            case SDL_MOUSEBUTTONDOWN:
                dxvEvent.type = EventType::MouseDown;
                translateMouseButtonEvent(dxvEvent, sdlEvent.button);
                return true;

            case SDL_MOUSEBUTTONUP:
                dxvEvent.type = EventType::MouseUp;
                translateMouseButtonEvent(dxvEvent, sdlEvent.button);
                return true;

            case SDL_MOUSEMOTION:
                dxvEvent.type = EventType::MouseMove;
                dxvEvent.mouse.x = sdlEvent.motion.x;
                dxvEvent.mouse.y = sdlEvent.motion.y;
                if (sdlEvent.motion.state & SDL_BUTTON_LMASK) dxvEvent.mouse.button = MouseButton::Left;
                else if (sdlEvent.motion.state & SDL_BUTTON_MMASK) dxvEvent.mouse.button = MouseButton::Middle;
                else if (sdlEvent.motion.state & SDL_BUTTON_RMASK) dxvEvent.mouse.button = MouseButton::Right;
                else dxvEvent.mouse.button = MouseButton::None;
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

}
