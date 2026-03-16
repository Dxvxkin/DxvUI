#include "DxvUI/sources/SDLEventSource.h"
#include <SDL.h>

namespace DxvUI {

    static void translateMouseButtonEvent(DxvEvent& dxvEvent, const SDL_MouseButtonEvent& sdlButtonEvent) {
        dxvEvent.x = sdlButtonEvent.x;
        dxvEvent.y = sdlButtonEvent.y;
        switch (sdlButtonEvent.button) {
            case SDL_BUTTON_LEFT:   dxvEvent.button = MouseButton::Left;   break;
            case SDL_BUTTON_MIDDLE: dxvEvent.button = MouseButton::Middle; break;
            case SDL_BUTTON_RIGHT:  dxvEvent.button = MouseButton::Right;  break;
            default:                dxvEvent.button = MouseButton::None;   break;
        }
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
                dxvEvent.x = sdlEvent.motion.x;
                dxvEvent.y = sdlEvent.motion.y;

                // Check the state of the mouse buttons during motion
                if (sdlEvent.motion.state & SDL_BUTTON_LMASK) {
                    dxvEvent.button = MouseButton::Left;
                } else if (sdlEvent.motion.state & SDL_BUTTON_MMASK) {
                    dxvEvent.button = MouseButton::Middle;
                } else if (sdlEvent.motion.state & SDL_BUTTON_RMASK) {
                    dxvEvent.button = MouseButton::Right;
                } else {
                    dxvEvent.button = MouseButton::None;
                }
                return true;
        }

        dxvEvent.type = EventType::None;
        return false;
    }

}
