#include "DxvUI/renderers/SDLRenderer.h"

#include <SDL.h>
#include <SDL2_gfxPrimitives.h>

#include <stdexcept>
#include <vector>

#include "DxvUI/Log.h"
#include "DxvUI/core.h"
#include "DxvUI/renderers/SDLTextEngine.h"
#include "renderers/SDLTexture.h"

namespace DxvUI {

SDLRenderer::SDLRenderer(const char* title, int width, int height) : ownsResources(true) {
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "linear");
    if (SDL_Init(SDL_INIT_VIDEO) != 0)
        throw std::runtime_error(std::string("SDL_Init Error: ") + SDL_GetError());
    window = SDL_CreateWindow(title, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, width, height,
                              SDL_WINDOW_SHOWN);
    if (!window) {
        SDL_Quit();
        throw std::runtime_error(std::string("SDL_CreateWindow Error: ") + SDL_GetError());
    }
    renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!renderer) {
        SDL_DestroyWindow(window);
        SDL_Quit();
        throw std::runtime_error(std::string("SDL_CreateRenderer Error: ") + SDL_GetError());
    }
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    textEngine = std::make_unique<SDLTextEngine>(renderer);
    setCursor(CursorType::Arrow);
}

SDLRenderer::SDLRenderer(SDL_Renderer* externalRenderer)
    : window(nullptr), renderer(externalRenderer), ownsResources(false) {
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "linear");
    if (!renderer) throw std::invalid_argument("externalRenderer cannot be null.");
    textEngine = std::make_unique<SDLTextEngine>(renderer);
    setCursor(CursorType::Arrow);
}

SDLRenderer::~SDLRenderer() {
    // Cached text textures reference the SDL renderer, so they must be freed
    // before the renderer itself is destroyed.
    if (textEngine) {
        textEngine->clearCaches();
    }

    for (auto& pair : cursorCache) {
        if (pair.second) SDL_FreeCursor(pair.second);
    }
    cursorCache.clear();

    if (ownsResources) {
        if (renderer) SDL_DestroyRenderer(renderer);
        if (window) SDL_DestroyWindow(window);
        SDL_Quit();
    }
}

ITextEngine& SDLRenderer::getTextEngine() { return *textEngine; }

void SDLRenderer::setCursor(CursorType type) {
    if (currentCursorType == type && SDL_GetCursor() != nullptr) return;
    SDL_SetCursor(getSystemCursor(type));
    currentCursorType = type;
}

CursorType SDLRenderer::getCursor() const { return currentCursorType; }

void SDLRenderer::pushClipRect(const Rect& rect) {
    SDL_Rect currentClip;
    SDL_RenderGetClipRect(renderer, &currentClip);
    clipStack.emplace_back(SDL_RenderIsClipEnabled(renderer) == SDL_TRUE,
                           Rect{currentClip.x, currentClip.y, currentClip.w, currentClip.h});

    SDL_Rect r = {rect.x, rect.y, rect.width, rect.height};
    SDL_RenderSetClipRect(renderer, &r);
}

void SDLRenderer::popClipRect() {
    if (clipStack.empty()) {
        Log::error("SDLRenderer::popClipRect called with an empty clip stack");
        return;
    }

    const auto& [enabled, rect] = clipStack.back();
    if (enabled) {
        SDL_Rect r = {rect.x, rect.y, rect.width, rect.height};
        SDL_RenderSetClipRect(renderer, &r);
    } else {
        SDL_RenderSetClipRect(renderer, nullptr);
    }
    clipStack.pop_back();
}

SDL_Cursor* SDLRenderer::getSystemCursor(CursorType type) {
    auto it = cursorCache.find(type);
    if (it != cursorCache.end()) {
        return it->second;
    }

    SDL_SystemCursor id;
    switch (type) {
        case CursorType::IBeam:
            id = SDL_SYSTEM_CURSOR_IBEAM;
            break;
        case CursorType::Wait:
            id = SDL_SYSTEM_CURSOR_WAIT;
            break;
        case CursorType::Crosshair:
            id = SDL_SYSTEM_CURSOR_CROSSHAIR;
            break;
        case CursorType::Hand:
            id = SDL_SYSTEM_CURSOR_HAND;
            break;
        case CursorType::ResizeNWSE:
            id = SDL_SYSTEM_CURSOR_SIZENWSE;
            break;
        case CursorType::ResizeNESW:
            id = SDL_SYSTEM_CURSOR_SIZENESW;
            break;
        case CursorType::ResizeWE:
            id = SDL_SYSTEM_CURSOR_SIZEWE;
            break;
        case CursorType::ResizeNS:
            id = SDL_SYSTEM_CURSOR_SIZENS;
            break;
        case CursorType::ResizeAll:
            id = SDL_SYSTEM_CURSOR_SIZEALL;
            break;
        case CursorType::No:
            id = SDL_SYSTEM_CURSOR_NO;
            break;
        case CursorType::Arrow:
        default:
            id = SDL_SYSTEM_CURSOR_ARROW;
            break;
    }

    SDL_Cursor* cursor = SDL_CreateSystemCursor(id);
    if (cursor) {
        cursorCache[type] = cursor;
    }
    return cursor;
}

void SDLRenderer::clear(const Color& color) {
    setDrawColor(color);
    SDL_RenderClear(renderer);
}
void SDLRenderer::present() { SDL_RenderPresent(renderer); }

Size SDLRenderer::getViewportSize() const {
    int w, h;
    SDL_GetRendererOutputSize(renderer, &w, &h);
    return {(float)w, (float)h};
}

void SDLRenderer::drawTexture(std::shared_ptr<ITexture>& texture, const Rect& dstRect) {
    if (!texture) return;
    SDL_Rect dst = {dstRect.x, dstRect.y, dstRect.width, dstRect.height};
    SDL_RenderCopy(renderer, dynamic_cast<SDLTexture*>(texture.get())->_texture, nullptr, &dst);
}

void SDLRenderer::setDrawColor(const Color& color) {
    currentColor = color;
    SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
}
Color SDLRenderer::getDrawColor() const { return currentColor; }

void SDLRenderer::drawRect(const Rect& rect) {
    SDL_Rect r = {rect.x, rect.y, rect.width, rect.height};
    SDL_RenderDrawRect(renderer, &r);
}
void SDLRenderer::fillRect(const Rect& rect) {
    SDL_Rect r = {rect.x, rect.y, rect.width, rect.height};
    SDL_RenderFillRect(renderer, &r);
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
    if (border.thickness <= 0) return;
    setDrawColor(border.color);
    for (int i = 0; i < border.thickness; ++i) {
        SDL_Rect r = {rect.x + i, rect.y + i, rect.width - 2 * i, rect.height - 2 * i};
        if (r.w <= 0 || r.h <= 0) break;
        SDL_RenderDrawRect(renderer, &r);
    }
}
void SDLRenderer::fillRect(const Rect& rect, const Color& fillColor, const Border& border) {
    fillRect(rect, fillColor);
    if (border.thickness > 0) {
        drawRect(rect, border);
    }
}
void SDLRenderer::drawLine(int x1, int y1, int x2, int y2) {
    SDL_RenderDrawLine(renderer, x1, y1, x2, y2);
}
void SDLRenderer::drawLine(int x1, int y1, int x2, int y2, const Color& color) {
    setDrawColor(color);
    SDL_RenderDrawLine(renderer, x1, y1, x2, y2);
}
void SDLRenderer::drawCircle(int cX, int cY, int r) {
    aacircleRGBA(renderer, cX, cY, r, currentColor.r, currentColor.g, currentColor.b,
                 currentColor.a);
}
void SDLRenderer::fillCircle(int cX, int cY, int r) {
    filledCircleRGBA(renderer, cX, cY, r, currentColor.r, currentColor.g, currentColor.b,
                     currentColor.a);
}
void SDLRenderer::drawCircle(int cX, int cY, int r, const Color& color) {
    aacircleRGBA(renderer, cX, cY, r, color.r, color.g, color.b, color.a);
}
void SDLRenderer::fillCircle(int cX, int cY, int r, const Color& color) {
    filledCircleRGBA(renderer, cX, cY, r, color.r, color.g, color.b, color.a);
}
void SDLRenderer::drawCircle(int cX, int cY, int r, const Border& border) {
    for (int i = 0; i < border.thickness; ++i)
        aacircleRGBA(renderer, cX, cY, r - i, border.color.r, border.color.g, border.color.b,
                     border.color.a);
}
void SDLRenderer::fillCircle(int cX, int cY, int r, const Color& f, const Border& b) {
    fillCircle(cX, cY, r, f);
    drawCircle(cX, cY, r, b);
}
void SDLRenderer::drawArc(int cX, int cY, int r, float sA, float eA) {
    arcRGBA(renderer, cX, cY, r, sA, eA, currentColor.r, currentColor.g, currentColor.b,
            currentColor.a);
}
void SDLRenderer::drawArc(int cX, int cY, int r, float sA, float eA, const Color& c) {
    arcRGBA(renderer, cX, cY, r, sA, eA, c.r, c.g, c.b, c.a);
}
void SDLRenderer::drawArc(int cX, int cY, int r, float sA, float eA, const Border& b) {
    for (int i = 0; i < b.thickness; ++i) drawArc(cX, cY, r - i, sA, eA, b.color);
}
void SDLRenderer::drawRoundRect(const Rect& rect, int r) {
    roundedRectangleRGBA(renderer, rect.x, rect.y, rect.x + rect.width - 1,
                         rect.y + rect.height - 1, r, currentColor.r, currentColor.g,
                         currentColor.b, currentColor.a);
}
void SDLRenderer::fillRoundRect(const Rect& rect, int r) {
    roundedBoxRGBA(renderer, rect.x, rect.y, rect.x + rect.width - 1, rect.y + rect.height - 1, r,
                   currentColor.r, currentColor.g, currentColor.b, currentColor.a);
}
void SDLRenderer::drawRoundRect(const Rect& rect, int r, const Color& c) {
    setDrawColor(c);
    drawRoundRect(rect, r);
}
void SDLRenderer::fillRoundRect(const Rect& rect, int r, const Color& c) {
    setDrawColor(c);
    fillRoundRect(rect, r);
}
void SDLRenderer::drawRoundRect(const Rect& rect, int radius, const Border& border) {
    if (border.thickness <= 0) return;
    setDrawColor(border.color);
    for (int i = 0; i < border.thickness; ++i) {
        Rect r = {rect.x + i, rect.y + i, rect.width - 2 * i, rect.height - 2 * i};
        if (r.width <= 0 || r.height <= 0) break;
        int innerRadius = (radius - i > 0) ? radius - i : 0;
        roundedRectangleRGBA(renderer, r.x, r.y, r.x + r.width - 1, r.y + r.height - 1, innerRadius,
                             border.color.r, border.color.g, border.color.b, border.color.a);
    }
}
void SDLRenderer::fillRoundRect(const Rect& rect, int radius, const Color& fillColor,
                                const Border& border) {
    fillRoundRect(rect, radius, fillColor);
    if (border.thickness > 0) {
        drawRoundRect(rect, radius, border);
    }
}
void SDLRenderer::drawPolygon(const std::vector<PointI>& p) {
    if (p.size() < 2) return;
    std::vector<Sint16> vx, vy;
    vx.reserve(p.size());
    vy.reserve(p.size());
    for (const auto& pt : p) {
        vx.push_back(pt.x);
        vy.push_back(pt.y);
    }
    polygonRGBA(renderer, vx.data(), vy.data(), p.size(), currentColor.r, currentColor.g,
                currentColor.b, currentColor.a);
}
void SDLRenderer::fillPolygon(const std::vector<PointI>& p) {
    if (p.size() < 3) return;
    std::vector<Sint16> vx, vy;
    vx.reserve(p.size());
    vy.reserve(p.size());
    for (const auto& pt : p) {
        vx.push_back(pt.x);
        vy.push_back(pt.y);
    }
    filledPolygonRGBA(renderer, vx.data(), vy.data(), p.size(), currentColor.r, currentColor.g,
                      currentColor.b, currentColor.a);
}
void SDLRenderer::drawPolygon(const std::vector<PointI>& p, const Color& c) {
    setDrawColor(c);
    drawPolygon(p);
}
void SDLRenderer::fillPolygon(const std::vector<PointI>& p, const Color& c) {
    setDrawColor(c);
    fillPolygon(p);
}
void SDLRenderer::drawPolygon(const std::vector<PointI>& p, const Border& b) {
    setDrawColor(b.color);
    drawPolygon(p);
}
void SDLRenderer::fillPolygon(const std::vector<PointI>& p, const Color& f, const Border& b) {
    fillPolygon(p, f);
    drawPolygon(p, b);
}
}  // namespace DxvUI
