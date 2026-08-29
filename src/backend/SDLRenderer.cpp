#include "DxvUI/backend/SDLRenderer.h"

#include <SDL.h>
#include <SDL2_gfxPrimitives.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <stdexcept>
#include <vector>

#include "DxvUI/Log.h"
#include "DxvUI/backend/SDLTextEngine.h"
#include "DxvUI/core.h"
#include "backend/SDLTexture.h"

namespace DxvUI {

namespace {

// Precomputed unit quarter-circle arc (13 points, 12 segments) used to build
// every corner of a rounded rectangle. Computed once; corners are cheap
// affine transforms of this template.
const std::array<SDL_FPoint, 13>& unitArcPoints() {
    static const std::array<SDL_FPoint, 13> kArc = [] {
        std::array<SDL_FPoint, 13> arc{};
        constexpr float kPi = 3.14159265358979323846f;
        for (int i = 0; i < 13; ++i) {
            const float a = (90.0f * static_cast<float>(i) / 12) * kPi / 180.0f;
            arc[static_cast<size_t>(i)] = {std::cos(a), std::sin(a)};
        }
        return arc;
    }();
    return kArc;
}

// Builds the closed outline of a rounded rectangle as a 52-point loop
// (4 corners x 13 arc points). x1/y1 and x2/y2 are inclusive (SDL gfx
// convention: callers pass x + width - 1). Straight edges are implicit
// between consecutive corner arcs.
std::array<SDL_FPoint, 52> roundedRectPolygon(int x1, int y1, int x2, int y2, int radius) {
    std::array<SDL_FPoint, 52> pts;
    const float r = static_cast<float>(radius);
    // Corner centers, traversed in clockwise screen order (TL, TR, BR, BL).
    const float cxs[4] = {static_cast<float>(x1 + radius), static_cast<float>(x2 - radius),
                          static_cast<float>(x2 - radius), static_cast<float>(x1 + radius)};
    const float cys[4] = {static_cast<float>(y1 + radius), static_cast<float>(y1 + radius),
                          static_cast<float>(y2 - radius), static_cast<float>(y2 - radius)};
    // Per-corner 2x2 rotation mapping the unit arc onto the corner arc:
    // TL: (-u.x,-u.y)  TR: (u.y,-u.x)  BR: (u.x,u.y)  BL: (-u.y,u.x)
    const float kx[4][2] = {{-1.0f, 0.0f}, {0.0f, 1.0f}, {1.0f, 0.0f}, {0.0f, -1.0f}};
    const float ky[4][2] = {{0.0f, -1.0f}, {-1.0f, 0.0f}, {0.0f, 1.0f}, {1.0f, 0.0f}};
    const auto& arc = unitArcPoints();
    for (int c = 0; c < 4; ++c) {
        const float cx = cxs[c];
        const float cy = cys[c];
        for (int i = 0; i < 13; ++i) {
            const float px =
                kx[c][0] * arc[static_cast<size_t>(i)].x + kx[c][1] * arc[static_cast<size_t>(i)].y;
            const float py =
                ky[c][0] * arc[static_cast<size_t>(i)].x + ky[c][1] * arc[static_cast<size_t>(i)].y;
            pts[static_cast<size_t>(c * 13 + i)] = {cx + r * px, cy + r * py};
        }
    }
    return pts;
}

// Fills a rounded rectangle on the GPU: a triangle fan from the center over
// the perimeter polygon. SDL's renderer does not cull back faces, so the
// winding of the fan is irrelevant.
void fillRoundedRectGeometry(SDL_Renderer* renderer, const Rect& rect, int radius,
                             const Color& color) {
    const int maxRadius = std::min(rect.width, rect.height) / 2;
    if (rect.width < 3 || rect.height < 3 || maxRadius <= 1 || radius <= 1) {
        SDL_Rect r = {rect.x, rect.y, rect.width, rect.height};
        SDL_RenderFillRect(renderer, &r);
        return;
    }
    const int rr = std::min(radius, maxRadius);
    const auto poly =
        roundedRectPolygon(rect.x, rect.y, rect.x + rect.width - 1, rect.y + rect.height - 1, rr);
    const SDL_Color c = {color.r, color.g, color.b, color.a};
    const SDL_FPoint center{static_cast<float>(rect.x) + static_cast<float>(rect.width) / 2.0f,
                            static_cast<float>(rect.y) + static_cast<float>(rect.height) / 2.0f};
    std::array<SDL_Vertex, 53> verts;
    verts[0] = {center, c, {0.0f, 0.0f}};
    for (size_t i = 0; i < 52; ++i) {
        verts[i + 1] = {poly[i], c, {0.0f, 0.0f}};
    }
    std::array<int, 156> indices;
    for (size_t i = 0; i < 52; ++i) {
        indices[i * 3 + 0] = 0;
        indices[i * 3 + 1] = static_cast<int>(i) + 1;
        indices[i * 3 + 2] = static_cast<int>((i + 1) % 52) + 1;
    }
    SDL_RenderGeometry(renderer, nullptr, verts.data(), 53, indices.data(), 156);
}

// Draws a rounded-rectangle border ring on the GPU: the annulus between the
// outer rounded polygon and an inset one, triangulated as a quad strip.
void drawRoundedRectRingGeometry(SDL_Renderer* renderer, const Rect& rect, int radius,
                                 int thickness, const Color& color) {
    const int w = rect.width;
    const int h = rect.height;
    if (w < 3 || h < 3 || thickness <= 0) return;
    const int maxRadius = std::min(w, h) / 2;
    if (radius <= 1 || maxRadius <= 1) {
        // Square corners: concentric 1px outlines (same pixels as the previous
        // roundedRectangleRGBA(radius=0) loop).
        SDL_Rect r = {rect.x, rect.y, rect.width, rect.height};
        for (int i = 0; i < thickness; ++i) {
            SDL_RenderDrawRect(renderer, &r);
            r.x += 1;
            r.y += 1;
            r.w -= 2;
            r.h -= 2;
            if (r.w <= 0 || r.h <= 0) break;
        }
        return;
    }
    const int outerR = std::min(radius, maxRadius);
    // Leave at least a 1px hole so the inner polygon stays non-degenerate.
    const int t = std::max(1, std::min(thickness, maxRadius - 1));
    const int innerR = std::max(1, outerR - t);
    const auto outer = roundedRectPolygon(rect.x, rect.y, rect.x + w - 1, rect.y + h - 1, outerR);
    const auto inner =
        roundedRectPolygon(rect.x + t, rect.y + t, rect.x + w - 1 - t, rect.y + h - 1 - t, innerR);
    const SDL_Color c = {color.r, color.g, color.b, color.a};
    std::array<SDL_Vertex, 104> verts;
    for (size_t i = 0; i < 52; ++i) {
        verts[i] = {outer[i], c, {0.0f, 0.0f}};
        verts[i + 52] = {inner[i], c, {0.0f, 0.0f}};
    }
    std::array<int, 312> indices;
    for (size_t i = 0; i < 52; ++i) {
        const int o = static_cast<int>(i);
        const int oNext = static_cast<int>((i + 1) % 52);
        const int in = static_cast<int>(i) + 52;
        const int inNext = static_cast<int>((i + 1) % 52) + 52;
        indices[i * 6 + 0] = o;
        indices[i * 6 + 1] = in;
        indices[i * 6 + 2] = inNext;
        indices[i * 6 + 3] = o;
        indices[i * 6 + 4] = inNext;
        indices[i * 6 + 5] = oNext;
    }
    SDL_RenderGeometry(renderer, nullptr, verts.data(), 104, indices.data(), 312);
}

}  // namespace

SDLRenderer::SDLRenderer(const char* title, int width, int height, bool vsync)
    : ownsResources(true) {
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "linear");
    if (SDL_Init(SDL_INIT_VIDEO) != 0)
        throw std::runtime_error(std::string("SDL_Init Error: ") + SDL_GetError());
    window = SDL_CreateWindow(title, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, width, height,
                              SDL_WINDOW_SHOWN);
    if (!window) {
        SDL_Quit();
        throw std::runtime_error(std::string("SDL_CreateWindow Error: ") + SDL_GetError());
    }
    renderer = SDL_CreateRenderer(
        window, -1, SDL_RENDERER_ACCELERATED | (vsync ? SDL_RENDERER_PRESENTVSYNC : 0));
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
    // Same blend-mode setup as the self-contained constructor: without it,
    // translucent clears, fills and text render incorrectly against the host's
    // content on an external renderer.
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
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

IClipboard& SDLRenderer::getClipboard() { return clipboard; }

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
    drawRoundedRectRingGeometry(renderer, rect, r, 1, currentColor);
}
void SDLRenderer::fillRoundRect(const Rect& rect, int r) {
    fillRoundedRectGeometry(renderer, rect, r, currentColor);
}
void SDLRenderer::drawRoundRect(const Rect& rect, int r, const Color& c) {
    setDrawColor(c);
    drawRoundedRectRingGeometry(renderer, rect, r, 1, currentColor);
}
void SDLRenderer::fillRoundRect(const Rect& rect, int r, const Color& c) {
    setDrawColor(c);
    fillRoundedRectGeometry(renderer, rect, r, currentColor);
}
void SDLRenderer::drawRoundRect(const Rect& rect, int radius, const Border& border) {
    setDrawColor(border.color);
    drawRoundedRectRingGeometry(renderer, rect, radius, border.thickness, border.color);
}
void SDLRenderer::fillRoundRect(const Rect& rect, int radius, const Color& fillColor,
                                const Border& border) {
    setDrawColor(fillColor);
    fillRoundedRectGeometry(renderer, rect, radius, fillColor);
    if (border.thickness > 0) {
        setDrawColor(border.color);
        drawRoundedRectRingGeometry(renderer, rect, radius, border.thickness, border.color);
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
