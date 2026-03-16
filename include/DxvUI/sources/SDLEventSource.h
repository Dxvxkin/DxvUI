#ifndef DXVUI_SDLEVENTSOURCE_H
#define DXVUI_SDLEVENTSOURCE_H

#include "../interfaces/IEventSource.h"

// Forward declare SDL_Event to avoid including SDL headers here
union SDL_Event;

namespace DxvUI {

    class SDLEventSource : public IEventSource {
    public:
        /**
         * @brief Polls for the next event from SDL's internal queue.
         * Use this when DxvUI owns the event loop.
         */
        bool pollEvent(DxvEvent& event) override;

        /**
         * @brief Processes an existing SDL_Event from an external event loop.
         * Use this to integrate DxvUI into an existing SDL application.
         * @param sdlEvent The event from your SDL_PollEvent loop.
         * @param dxvEvent The DxvUI event to be filled if translation is successful.
         * @return True if the event was relevant and translated, false otherwise.
         */
        bool processEvent(const SDL_Event& sdlEvent, DxvEvent& dxvEvent);
    };

}

#endif //DXVUI_SDLEVENTSOURCE_H
