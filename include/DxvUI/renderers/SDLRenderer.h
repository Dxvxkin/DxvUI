#ifndef DXVUI_SDLRENDERER_H
#define DXVUI_SDLRENDERER_H

#include <DxvUI/interfaces/IRenderer.h>
#include <string>

// Forward declarations for SDL types to avoid including SDL headers here
struct SDL_Window;
struct SDL_Renderer;

namespace DxvUI {

    class SDLRenderer : public IRenderer {
    public:
        SDLRenderer(const char* title, int width, int height);
        ~SDLRenderer() override;

        // --- IRenderer implementation ---
        void drawRect(int x, int y, int width, int height) override;
        void drawText(const std::string& text, int x, int y) override;
        void drawImage(const std::string& imagePath, int x, int y) override;

        // --- SDL-specific methods for the main loop ---
        void clear();
        void present();
        SDL_Renderer* getSDLRenderer(); // For advanced use, e.g., text rendering

    private:
        SDL_Window* window = nullptr;
        SDL_Renderer* renderer = nullptr;
    };

}

#endif //DXVUI_SDLRENDERER_H
