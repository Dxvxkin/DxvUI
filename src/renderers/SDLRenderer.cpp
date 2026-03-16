#include "DxvUI/renderers/SDLRenderer.h"
#include <SDL.h>
#include <SDL2_gfxPrimitives.h>
#include <stdexcept>
#include <vector>

namespace DxvUI {

    // Constructor that creates and owns resources
    SDLRenderer::SDLRenderer(const char* title, int width, int height) : ownsResources(true) {
        // We don't initialize SDL here, as the user might have done it already.
        // We assume SDL_Init has been called.
        window = SDL_CreateWindow(title, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, width, height, SDL_WINDOW_SHOWN);
        if (window == nullptr) {
            throw std::runtime_error(std::string("SDL_CreateWindow Error: ") + SDL_GetError());
        }

        renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
        if (renderer == nullptr) {
            SDL_DestroyWindow(window);
            throw std::runtime_error(std::string("SDL_CreateRenderer Error: ") + SDL_GetError());
        }
        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    }

    // Constructor that uses an external renderer
    SDLRenderer::SDLRenderer(SDL_Renderer* externalRenderer)
        : window(nullptr), renderer(externalRenderer), ownsResources(false) {
        if (renderer == nullptr) {
            throw std::invalid_argument("externalRenderer cannot be null.");
        }
        // We assume the blend mode is already set correctly by the user.
    }

    SDLRenderer::~SDLRenderer() {
        if (ownsResources) {
            if (renderer) {
                SDL_DestroyRenderer(renderer);
            }
            if (window) {
                SDL_DestroyWindow(window);
            }
            // We don't call SDL_Quit(), as we don't know who else might be using SDL.
        }
    }

    // ... (the rest of the implementation remains the same)
    void SDLRenderer::setDrawColor(const Color& color) {
        currentColor = color;
        SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
    }

    Color SDLRenderer::getDrawColor() const {
        return currentColor;
    }

    void SDLRenderer::drawRect(const Rect& rect) {
        SDL_Rect sdlRect = {rect.x, rect.y, rect.width, rect.height};
        SDL_RenderDrawRect(renderer, &sdlRect);
    }

    void SDLRenderer::fillRect(const Rect& rect) {
        SDL_Rect sdlRect = {rect.x, rect.y, rect.width, rect.height};
        SDL_RenderFillRect(renderer, &sdlRect);
    }

    void SDLRenderer::drawRect(const Rect& rect, const Color& color) { setDrawColor(color); drawRect(rect); }
    void SDLRenderer::fillRect(const Rect& rect, const Color& color) { setDrawColor(color); fillRect(rect); }
    void SDLRenderer::drawRect(const Rect& rect, const Border& border) {
        setDrawColor(border.color);
        for (int i = 0; i < border.thickness; ++i) {
            SDL_Rect r = {rect.x + i, rect.y + i, rect.width - 2 * i, rect.height - 2 * i};
            if (r.w < 0) r.w = 0; if (r.h < 0) r.h = 0;
            SDL_RenderDrawRect(renderer, &r);
        }
    }
    void SDLRenderer::fillRect(const Rect& rect, const Color& fillColor, const Border& border) { fillRect(rect, fillColor); drawRect(rect, border); }
    void SDLRenderer::drawLine(int x1, int y1, int x2, int y2) { SDL_RenderDrawLine(renderer, x1, y1, x2, y2); }
    void SDLRenderer::drawLine(int x1, int y1, int x2, int y2, const Color& color) { setDrawColor(color); SDL_RenderDrawLine(renderer, x1, y1, x2, y2); }
    void SDLRenderer::drawCircle(int cX, int cY, int r) { aacircleRGBA(renderer, cX, cY, r, currentColor.r, currentColor.g, currentColor.b, currentColor.a); }
    void SDLRenderer::fillCircle(int cX, int cY, int r) { filledCircleRGBA(renderer, cX, cY, r, currentColor.r, currentColor.g, currentColor.b, currentColor.a); }
    void SDLRenderer::drawCircle(int cX, int cY, int r, const Color& color) { aacircleRGBA(renderer, cX, cY, r, color.r, color.g, color.b, color.a); }
    void SDLRenderer::fillCircle(int cX, int cY, int r, const Color& color) { filledCircleRGBA(renderer, cX, cY, r, color.r, color.g, color.b, color.a); }
    void SDLRenderer::drawCircle(int cX, int cY, int r, const Border& border) { for (int i = 0; i < border.thickness; ++i) aacircleRGBA(renderer, cX, cY, r - i, border.color.r, border.color.g, border.color.b, border.color.a); }
    void SDLRenderer::fillCircle(int cX, int cY, int r, const Color& f, const Border& b) { fillCircle(cX, cY, r, f); drawCircle(cX, cY, r, b); }
    void SDLRenderer::drawArc(int cX, int cY, int r, float sA, float eA) { arcRGBA(renderer, cX, cY, r, sA, eA, currentColor.r, currentColor.g, currentColor.b, currentColor.a); }
    void SDLRenderer::drawArc(int cX, int cY, int r, float sA, float eA, const Color& c) { arcRGBA(renderer, cX, cY, r, sA, eA, c.r, c.g, c.b, c.a); }
    void SDLRenderer::drawArc(int cX, int cY, int r, float sA, float eA, const Border& b) { for (int i = 0; i < b.thickness; ++i) drawArc(cX, cY, r - i, sA, eA, b.color); }
    void SDLRenderer::drawRoundRect(const Rect& rect, int r) { roundedRectangleRGBA(renderer, rect.x, rect.y, rect.x + rect.width - 1, rect.y + rect.height - 1, r, currentColor.r, currentColor.g, currentColor.b, currentColor.a); }
    void SDLRenderer::fillRoundRect(const Rect& rect, int r) { roundedBoxRGBA(renderer, rect.x, rect.y, rect.x + rect.width - 1, rect.y + rect.height - 1, r, currentColor.r, currentColor.g, currentColor.b, currentColor.a); }
    void SDLRenderer::drawRoundRect(const Rect& rect, int r, const Color& c) { setDrawColor(c); drawRoundRect(rect, r); }
    void SDLRenderer::fillRoundRect(const Rect& rect, int r, const Color& c) { setDrawColor(c); fillRoundRect(rect, r); }
    void SDLRenderer::drawRoundRect(const Rect& rect, int r, const Border& b) { for (int i = 0; i < b.thickness; ++i) { Rect rt = {rect.x + i, rect.y + i, rect.width - 2 * i, rect.height - 2 * i}; if (rt.width < 0) rt.width = 0; if (rt.height < 0) rt.height = 0; roundedRectangleRGBA(renderer, rt.x, rt.y, rt.x + rt.width - 1, rt.y + rt.height - 1, r, b.color.r, b.color.g, b.color.b, b.color.a); } }
    void SDLRenderer::fillRoundRect(const Rect& rect, int r, const Color& f, const Border& b) { fillRoundRect(rect, r, f); drawRoundRect(rect, r, b); }
    void SDLRenderer::drawPolygon(const std::vector<PointI>& p) { if (p.size() < 2) return; std::vector<Sint16> vx, vy; vx.reserve(p.size()); vy.reserve(p.size()); for (const auto& pt : p) { vx.push_back(pt.x); vy.push_back(pt.y); } polygonRGBA(renderer, vx.data(), vy.data(), p.size(), currentColor.r, currentColor.g, currentColor.b, currentColor.a); }
    void SDLRenderer::fillPolygon(const std::vector<PointI>& p) { if (p.size() < 3) return; std::vector<Sint16> vx, vy; vx.reserve(p.size()); vy.reserve(p.size()); for (const auto& pt : p) { vx.push_back(pt.x); vy.push_back(pt.y); } filledPolygonRGBA(renderer, vx.data(), vy.data(), p.size(), currentColor.r, currentColor.g, currentColor.b, currentColor.a); }
    void SDLRenderer::drawPolygon(const std::vector<PointI>& p, const Color& c) { setDrawColor(c); drawPolygon(p); }
    void SDLRenderer::fillPolygon(const std::vector<PointI>& p, const Color& c) { setDrawColor(c); fillPolygon(p); }
    void SDLRenderer::drawPolygon(const std::vector<PointI>& p, const Border& b) { setDrawColor(b.color); drawPolygon(p); }
    void SDLRenderer::fillPolygon(const std::vector<PointI>& p, const Color& f, const Border& b) { fillPolygon(p, f); drawPolygon(p, b); }
    void SDLRenderer::drawText(const std::string& text, int x, int y) {}
    void SDLRenderer::drawImage(const std::string& imagePath, int x, int y) {}
    void SDLRenderer::clear(const Color& color) { setDrawColor(color); SDL_RenderClear(renderer); }
    void SDLRenderer::present() { SDL_RenderPresent(renderer); }
    SDL_Renderer* SDLRenderer::getSDLRenderer() { return renderer; }

}
