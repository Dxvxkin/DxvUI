#ifndef DXVUI_SDLCLIPBOARD_H
#define DXVUI_SDLCLIPBOARD_H

#include "DxvUI/interfaces/IClipboard.h"

namespace DxvUI {

/**
 * @brief IClipboard backed by SDL_GetClipboardText()/SDL_SetClipboardText().
 */
class SDLClipboard : public IClipboard {
   public:
    std::string getText() override;
    bool setText(const std::string& text) override;
};

}  // namespace DxvUI

#endif  // DXVUI_SDLCLIPBOARD_H
