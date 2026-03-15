#include "DxvUI/sources/SDLEventSource.h"
#include <SDL.h>

namespace DxvUI {

    // Helper function to translate common data from SDL mouse button events.
    // It's static, so it's only visible within this file.
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
            switch (sdlEvent.type) {
                case SDL_QUIT:
                    event.type = EventType::Quit;
                    return true;

                case SDL_MOUSEBUTTONDOWN:
                    event.type = EventType::MouseDown;
                    translateMouseButtonEvent(event, sdlEvent.button);
                    return true;

                case SDL_MOUSEBUTTONUP:
                    event.type = EventType::MouseUp;
                    translateMouseButtonEvent(event, sdlEvent.button);
                    return true;

                case SDL_MOUSEMOTION:
                    event.type = EventType::MouseMove;
                    event.x = sdlEvent.motion.x;
                    event.y = sdlEvent.motion.y;
                    event.button = MouseButton::None; // Motion events don't have a specific button
                    return true;

                // We can add KeyDown, KeyUp, etc. here in the future
            }
        }

        // No event we care about was polled
        event.type = EventType::None;
        return false;
    }

}
