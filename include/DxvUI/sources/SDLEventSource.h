#ifndef DXVUI_SDLEVENTSOURCE_H
#define DXVUI_SDLEVENTSOURCE_H

#include "../interfaces/IEventSource.h"

namespace DxvUI {

    /**
     * @brief An implementation of IEventSource using the SDL2 library.
     */
    class SDLEventSource : public IEventSource {
    public:
        bool pollEvent(DxvEvent& event) override;
    };

}

#endif //DXVUI_SDLEVENTSOURCE_H
