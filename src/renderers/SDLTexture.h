#ifndef DXVUI_SDLTEXTURE_H
#define DXVUI_SDLTEXTURE_H

// Internal to the SDL renderer: exposes the raw SDL_Texture to the renderer and
// the text engine. Not part of the public abstraction (ITexture is); this header
// lives under src/ and is only reachable by the library's own sources.

#include <SDL.h>

#include "DxvUI/interfaces/ITexture.h"

namespace DxvUI {

class SDLTexture : public ITexture {
   public:
    explicit SDLTexture(SDL_Texture* texture) : _texture(texture), width(0), height(0) {
        if (_texture) {
            SDL_QueryTexture(texture, nullptr, nullptr, &width, &height);
        }
    }
    ~SDLTexture() override {
        if (_texture) {
            SDL_DestroyTexture(_texture);
        }
    }
    int getWidth() const override { return width; }
    int getHeight() const override { return height; }
    SDL_Texture* _texture;

   private:
    int width, height;
};

}  // namespace DxvUI

#endif  // DXVUI_SDLTEXTURE_H
