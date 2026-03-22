#ifndef DXVUI_SDLEVENTSOURCE_H
#define DXVUI_SDLEVENTSOURCE_H

#include "../interfaces/IEventSource.h"

union SDL_Event;

namespace DxvUI {

    class SDLEventSource : public IEventSource {
    public:
        // Add a virtual destructor
        ~SDLEventSource() override;

        bool pollEvent(DxvEvent& event) override;
        bool processEvent(const SDL_Event& sdlEvent, DxvEvent& dxvEvent);
    };

}

#endif //DXVUI_SDLEVENTSOURCE_H
