#include "DxvUI/renderers/SDLRenderer.h"
#include <SDL.h>
#include <SDL2_gfxPrimitives.h> // Для продвинутых фигур
#include <stdexcept>
#include <vector>

namespace DxvUI {

    SDLRenderer::SDLRenderer(const char* title, int width, int height) {
        if (SDL_Init(SDL_INIT_VIDEO) != 0) {
            throw std::runtime_error(std::string("SDL_Init Error: ") + SDL_GetError());
        }

        window = SDL_CreateWindow(title, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, width, height, SDL_WINDOW_SHOWN);
        if (window == nullptr) {
            SDL_Quit();
            throw std::runtime_error(std::string("SDL_CreateWindow Error: ") + SDL_GetError());
        }

        renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
        if (renderer == nullptr) {
            SDL_DestroyWindow(window);
            SDL_Quit();
            throw std::runtime_error(std::string("SDL_CreateRenderer Error: ") + SDL_GetError());
        }
        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
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

    void SDLRenderer::setDrawColor(const Color& color) {
        currentColor = color;
        SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
    }

    Color SDLRenderer::getDrawColor() const {
        return currentColor;
    }

    // --- Rectangles ---

    void SDLRenderer::drawRect(const Rect& rect) {
        SDL_Rect sdlRect = {rect.x, rect.y, rect.width, rect.height};
        SDL_RenderDrawRect(renderer, &sdlRect);
    }

    void SDLRenderer::fillRect(const Rect& rect) {
        SDL_Rect sdlRect = {rect.x, rect.y, rect.width, rect.height};
        SDL_RenderFillRect(renderer, &sdlRect);
    }

    void SDLRenderer::drawRect(const Rect& rect, const Color& color) {
        setDrawColor(color);
        drawRect(rect);
    }

    void SDLRenderer::fillRect(const Rect& rect, const Color& color) {
        setDrawColor(color);
        fillRect(rect);
    }

    void SDLRenderer::drawRect(const Rect& rect, const Border& border) {
        setDrawColor(border.color);
        for (int i = 0; i < border.thickness; ++i) {
            SDL_Rect r = {rect.x + i, rect.y + i, rect.width - 2 * i, rect.height - 2 * i};
            if (r.w < 0) r.w = 0;
            if (r.h < 0) r.h = 0;
            SDL_RenderDrawRect(renderer, &r);
        }
    }

    void SDLRenderer::fillRect(const Rect& rect, const Color& fillColor, const Border& border) {
        fillRect(rect, fillColor);
        drawRect(rect, border);
    }

    // --- Lines ---

    void SDLRenderer::drawLine(int x1, int y1, int x2, int y2) {
        SDL_RenderDrawLine(renderer, x1, y1, x2, y2);
    }

    void SDLRenderer::drawLine(int x1, int y1, int x2, int y2, const Color& color) {
        setDrawColor(color);
        SDL_RenderDrawLine(renderer, x1, y1, x2, y2);
    }

    // --- Circles ---

    void SDLRenderer::drawCircle(int centerX, int centerY, int radius) {
        aacircleRGBA(renderer, centerX, centerY, radius, currentColor.r, currentColor.g, currentColor.b, currentColor.a);
    }

    void SDLRenderer::fillCircle(int centerX, int centerY, int radius) {
        filledCircleRGBA(renderer, centerX, centerY, radius, currentColor.r, currentColor.g, currentColor.b, currentColor.a);
    }

    void SDLRenderer::drawCircle(int centerX, int centerY, int radius, const Color& color) {
        aacircleRGBA(renderer, centerX, centerY, radius, color.r, color.g, color.b, color.a);
    }

    void SDLRenderer::fillCircle(int centerX, int centerY, int radius, const Color& color) {
        filledCircleRGBA(renderer, centerX, centerY, radius, color.r, color.g, color.b, color.a);
    }

    void SDLRenderer::drawCircle(int centerX, int centerY, int radius, const Border& border) {
        for (int i = 0; i < border.thickness; ++i) {
            aacircleRGBA(renderer, centerX, centerY, radius - i, border.color.r, border.color.g, border.color.b, border.color.a);
        }
    }

    void SDLRenderer::fillCircle(int centerX, int centerY, int radius, const Color& fillColor, const Border& border) {
        fillCircle(centerX, centerY, radius, fillColor);
        drawCircle(centerX, centerY, radius, border);
    }

    // --- Arcs ---

    void SDLRenderer::drawArc(int centerX, int centerY, int radius, float startAngle, float endAngle) {
        arcRGBA(renderer, centerX, centerY, radius, static_cast<Sint16>(startAngle), static_cast<Sint16>(endAngle), currentColor.r, currentColor.g, currentColor.b, currentColor.a);
    }

    void SDLRenderer::drawArc(int centerX, int centerY, int radius, float startAngle, float endAngle, const Color& color) {
        arcRGBA(renderer, centerX, centerY, radius, static_cast<Sint16>(startAngle), static_cast<Sint16>(endAngle), color.r, color.g, color.b, color.a);
    }

    void SDLRenderer::drawArc(int centerX, int centerY, int radius, float startAngle, float endAngle, const Border& border) {
        for (int i = 0; i < border.thickness; ++i) {
            drawArc(centerX, centerY, radius - i, startAngle, endAngle, border.color);
        }
    }

    // --- Rounded Rectangles ---

    void SDLRenderer::drawRoundRect(const Rect& rect, int radius) {
        roundedRectangleRGBA(renderer, rect.x, rect.y, rect.x + rect.width - 1, rect.y + rect.height - 1, radius, currentColor.r, currentColor.g, currentColor.b, currentColor.a);
    }

    void SDLRenderer::fillRoundRect(const Rect& rect, int radius) {
        roundedBoxRGBA(renderer, rect.x, rect.y, rect.x + rect.width - 1, rect.y + rect.height - 1, radius, currentColor.r, currentColor.g, currentColor.b, currentColor.a);
    }

    void SDLRenderer::drawRoundRect(const Rect& rect, int radius, const Color& color) {
        setDrawColor(color);
        drawRoundRect(rect, radius);
    }

    void SDLRenderer::fillRoundRect(const Rect& rect, int radius, const Color& color) {
        setDrawColor(color);
        fillRoundRect(rect, radius);
    }

    void SDLRenderer::drawRoundRect(const Rect& rect, int radius, const Border& border) {
        for (int i = 0; i < border.thickness; ++i) {
            Rect r = {rect.x + i, rect.y + i, rect.width - 2 * i, rect.height - 2 * i};
            if (r.width < 0) r.width = 0;
            if (r.height < 0) r.height = 0;
            // Call the low-level function directly to avoid recursion
            roundedRectangleRGBA(renderer, r.x, r.y, r.x + r.width - 1, r.y + r.height - 1, radius, border.color.r, border.color.g, border.color.b, border.color.a);
        }
    }

    void SDLRenderer::fillRoundRect(const Rect& rect, int radius, const Color& fillColor, const Border& border) {
        fillRoundRect(rect, radius, fillColor);
        drawRoundRect(rect, radius, border);
    }

    // --- Polygons ---

    void SDLRenderer::drawPolygon(const std::vector<PointI>& points) {
        if (points.size() < 2) return;
        std::vector<Sint16> vx, vy;
        vx.reserve(points.size());
        vy.reserve(points.size());
        for (const auto& p : points) {
            vx.push_back(p.x);
            vy.push_back(p.y);
        }
        polygonRGBA(renderer, vx.data(), vy.data(), points.size(), currentColor.r, currentColor.g, currentColor.b, currentColor.a);
    }

    void SDLRenderer::fillPolygon(const std::vector<PointI>& points) {
        if (points.size() < 3) return;
        std::vector<Sint16> vx, vy;
        vx.reserve(points.size());
        vy.reserve(points.size());
        for (const auto& p : points) {
            vx.push_back(p.x);
            vy.push_back(p.y);
        }
        filledPolygonRGBA(renderer, vx.data(), vy.data(), points.size(), currentColor.r, currentColor.g, currentColor.b, currentColor.a);
    }

    void SDLRenderer::drawPolygon(const std::vector<PointI>& points, const Color& color) { setDrawColor(color); drawPolygon(points); }
    void SDLRenderer::fillPolygon(const std::vector<PointI>& points, const Color& color) { setDrawColor(color); fillPolygon(points); }
    void SDLRenderer::drawPolygon(const std::vector<PointI>& points, const Border& border) { setDrawColor(border.color); drawPolygon(points); /* Note: thickness not supported by SDL2_gfx polygon */ }
    void SDLRenderer::fillPolygon(const std::vector<PointI>& points, const Color& fillColor, const Border& border) { fillPolygon(points, fillColor); drawPolygon(points, border); }

    // --- Text & Images (Placeholders) ---

    void SDLRenderer::drawText(const std::string& text, int x, int y) {
        // Требует подключения библиотеки SDL_ttf
    }

    void SDLRenderer::drawImage(const std::string& imagePath, int x, int y) {
        // Требует подключения библиотеки SDL_image
    }

    // --- SDL-specific methods ---
    
    void SDLRenderer::clear(const Color& color) {
        setDrawColor(color);
        SDL_RenderClear(renderer);
    }

    void SDLRenderer::present() {
        SDL_RenderPresent(renderer);
    }

    SDL_Renderer* SDLRenderer::getSDLRenderer() {
        return renderer;
    }

}
