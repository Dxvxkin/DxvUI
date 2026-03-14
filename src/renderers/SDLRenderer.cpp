#include "DxvUI/renderers/SDLRenderer.h"
#include <SDL.h>
#include <iostream>

namespace DxvUI {

    SDLRenderer::SDLRenderer(const char* title, int width, int height) {
        if (SDL_Init(SDL_INIT_VIDEO) != 0) {
            std::cerr << "SDL_Init Error: " << SDL_GetError() << std::endl;
            return;
        }

        window = SDL_CreateWindow(title, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, width, height, SDL_WINDOW_SHOWN);
        if (window == nullptr) {
            std::cerr << "SDL_CreateWindow Error: " << SDL_GetError() << std::endl;
            SDL_Quit();
            return;
        }

        renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
        if (renderer == nullptr) {
            SDL_DestroyWindow(window);
            std::cerr << "SDL_CreateRenderer Error: " << SDL_GetError() << std::endl;
            SDL_Quit();
            return;
        }
    }

    SDLRenderer::~SDLRenderer() {
        if (renderer) {
            SDL_DestroyRenderer(renderer);
        }
        if (window) {
            SDL_DestroyWindow(window);
        }
        SDL_Quit();
    }

    void SDLRenderer::drawRect(int x, int y, int width, int height) {
        SDL_Rect rect = {x, y, width, height};
        SDL_SetRenderDrawColor(renderer, 200, 200, 200, 255); // A light gray for the button
        SDL_RenderFillRect(renderer, &rect);
        SDL_SetRenderDrawColor(renderer, 100, 100, 100, 255); // A darker gray for the border
        SDL_RenderDrawRect(renderer, &rect);
    }

    void SDLRenderer::drawText(const std::string& text, int x, int y) {
        // Placeholder: requires SDL_ttf
        // For now, we'll just print a debug message.
        // std::cout << "Rendering text (not implemented): " << text << std::endl;
    }

    void SDLRenderer::drawImage(const std::string& imagePath, int x, int y) {
        // Placeholder: requires SDL_image
    }

    void SDLRenderer::clear() {
        SDL_SetRenderDrawColor(renderer, 30, 30, 30, 255); // Dark background
        SDL_RenderClear(renderer);
    }

    void SDLRenderer::present() {
        SDL_RenderPresent(renderer);
    }

    SDL_Renderer* SDLRenderer::getSDLRenderer() {
        return renderer;
    }

}
