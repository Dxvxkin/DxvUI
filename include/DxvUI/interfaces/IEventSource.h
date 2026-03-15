#ifndef DXVUI_IEVENTSOURCE_H
#define DXVUI_IEVENTSOURCE_H

#include "../core.h"

namespace DxvUI {

    /**
     * @brief An interface for an abstract source of UI events.
     * This allows the core UI library to be independent of any specific
     * windowing/event system (like SDL, WinAPI, etc.).
     */
    class IEventSource {
    public:
        virtual ~IEventSource() = default;

        /**
         * @brief Polls for the next available event from the system.
         * @param event A reference to a DxvEvent structure to be filled with event data.
         * @return True if an event was polled and translated, false otherwise.
         */
        virtual bool pollEvent(DxvEvent& event) = 0;
    };

}

#endif //DXVUI_IEVENTSOURCE_H
