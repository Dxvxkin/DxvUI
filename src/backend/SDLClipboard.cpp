#include "DxvUI/backend/SDLClipboard.h"

#include <SDL.h>

#include "DxvUI/Log.h"

namespace DxvUI {

std::string SDLClipboard::getText() {
    if (char* clip = SDL_GetClipboardText()) {
        std::string text(clip);
        SDL_free(clip);
        return text;
    }
    Log::error("SDL_GetClipboardText Error: {}", SDL_GetError());
    return {};
}

bool SDLClipboard::setText(const std::string& text) {
    if (SDL_SetClipboardText(text.c_str()) == 0) {
        return true;
    }
    Log::error("SDL_SetClipboardText Error: {}", SDL_GetError());
    return false;
}

}  // namespace DxvUI
