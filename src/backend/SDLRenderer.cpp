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
        SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
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
        SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
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
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    textEngine = std::make_unique<SDLTextEngine>(renderer);
    setCursor(CursorType::Arrow);
}

SDLRenderer::~SDLRenderer() {
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
    const SDL_bool clipEnabled = SDL_RenderIsClipEnabled(renderer);
    SDL_Rect currentClip;
    SDL_RenderGetClipRect(renderer, &currentClip);
    clipStack.emplace_back(clipEnabled == SDL_TRUE,
                           Rect{currentClip.x, currentClip.y, currentClip.w, currentClip.h});

    SDL_Rect r = {rect.x, rect.y, rect.width, rect.height};
    if (clipEnabled == SDL_TRUE) {
        SDL_Rect intersection;
        if (SDL_IntersectRect(&r, &currentClip, &intersection)) {
            r = intersection;
        } else {
            int outW = 0, outH = 0;
            SDL_GetRendererOutputSize(renderer, &outW, &outH);
            r = {outW, outH, 1, 1};
        }
    }
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
    setSDLDrawColor(color);
    SDL_RenderClear(renderer);
}
void SDLRenderer::present() { SDL_RenderPresent(renderer); }

Size SDLRenderer::getViewportSize() const {
    int w, h;
    SDL_GetRendererOutputSize(renderer, &w, &h);
    return {(float)w, (float)h};
}

float SDLRenderer::getDpiScale() const {
    // Stage 5: return 1.0f for now, future HiDPI will query SDL_GetRendererOutputSize vs window size
    return 1.0f;
}

ICanvas& SDLRenderer::beginFrame(const Color& clearColor) {
    // Stage 5: frame lifecycle – clear if we own resources, otherwise host does clear
    if (ownsResources) {
        clear(clearColor);
    }
    return *this;
}

void SDLRenderer::endFrame() {
    if (ownsResources) {
        present();
    }
}

// --- ICanvas float-based implementation (stage 5 real backend) ---

void SDLRenderer::pushClip(const RectF& rect) {
    pushClipRect(rect.rounded());
}

void SDLRenderer::popClip() {
    popClipRect();
}

void SDLRenderer::drawTexture(const std::shared_ptr<ITexture>& texture, const RectF& dstRect) {
    drawTexture(texture, dstRect.rounded());
}

void SDLRenderer::drawTexture(const std::shared_ptr<ITexture>& texture, const TextureDraw& draw) {
    // Translate float TextureDraw -> int TextureDrawDesc
    TextureDrawDesc desc;
    desc.dst = draw.dst.rounded();
    if (draw.src) {
        desc.src = draw.src->rounded();
    }
    desc.tint = draw.tint;
    desc.alpha = draw.alpha;
    // rotation/flip ignored for now (stage 6)
    drawTexture(texture, desc);
}

void SDLRenderer::fillRect(const RectF& rect, const Fill& fill) {
    fillRect(rect.rounded(), fill.color);
}

void SDLRenderer::strokeRect(const RectF& rect, const Stroke& stroke) {
    drawRect(rect.rounded(), Border{stroke.color, static_cast<int>(std::lround(stroke.thickness))});
}

void SDLRenderer::fillRoundRect(const RectF& rect, float radius, const Brush& brush) {
    if (brush.isEmpty()) return;
    const Rect pixelRect = rect.rounded();
    const int pixelRadius = std::max(0, static_cast<int>(std::lround(radius)));
    auto toBorder = [](const Stroke& s) -> Border {
        return {s.color, std::max(1, static_cast<int>(std::lround(s.thickness)))};
    };
    if (brush.fill && brush.stroke) {
        fillRoundRect(pixelRect, pixelRadius, brush.fill->color, toBorder(*brush.stroke));
    } else if (brush.fill) {
        fillRoundRect(pixelRect, pixelRadius, brush.fill->color);
    } else {
        drawRoundRect(pixelRect, pixelRadius, toBorder(*brush.stroke));
    }
}

void SDLRenderer::fillCircle(const PointF& center, float radius, const Brush& brush) {
    if (brush.isEmpty()) return;
    const PointI pixelCenter = center.rounded();
    const int pixelRadius = std::max(0, static_cast<int>(std::lround(radius)));
    auto toBorder = [](const Stroke& s) -> Border {
        return {s.color, std::max(1, static_cast<int>(std::lround(s.thickness)))};
    };
    if (brush.fill && brush.stroke) {
        fillCircle(pixelCenter.x, pixelCenter.y, pixelRadius, brush.fill->color, toBorder(*brush.stroke));
    } else if (brush.fill) {
        fillCircle(pixelCenter.x, pixelCenter.y, pixelRadius, brush.fill->color);
    } else {
        drawCircle(pixelCenter.x, pixelCenter.y, pixelRadius, toBorder(*brush.stroke));
    }
}

void SDLRenderer::strokeArc(const PointF& center, float radius, float startAngle, float endAngle,
                            const Stroke& stroke) {
    const PointI pixelCenter = center.rounded();
    const int pixelRadius = std::max(0, static_cast<int>(std::lround(radius)));
    drawArc(pixelCenter.x, pixelCenter.y, pixelRadius, startAngle, endAngle,
            Border{stroke.color, std::max(1, static_cast<int>(std::lround(stroke.thickness)))});
}

void SDLRenderer::fillPolygon(std::span<const PointF> points, const Fill& fill) {
    if (points.size() < 3) return;
    std::vector<PointI> pixelPoints;
    pixelPoints.reserve(points.size());
    for (const PointF& p : points) {
        pixelPoints.push_back(p.rounded());
    }
    fillPolygon(pixelPoints, fill.color);
}

void SDLRenderer::drawLine(const PointF& from, const PointF& to, const Stroke& stroke) {
    const PointI a = from.rounded();
    const PointI b = to.rounded();
    drawLine(a.x, a.y, b.x, b.y, stroke.color, std::max(1, static_cast<int>(std::lround(stroke.thickness))));
}


void SDLRenderer::drawTexture(const std::shared_ptr<ITexture>& texture, const Rect& dstRect) {
    if (!texture) return;
    const auto* sdlTexture = dynamic_cast<SDLTexture*>(texture.get());
    if (!sdlTexture || !sdlTexture->_texture) {
        Log::error(
            "SDLRenderer::drawTexture: the texture was not created by this renderer (foreign "
            "ITexture implementation or a freed handle); skipping");
        return;
    }
    SDL_Rect dst = {dstRect.x, dstRect.y, dstRect.width, dstRect.height};
    SDL_SetTextureColorMod(sdlTexture->_texture, 255, 255, 255);
    SDL_SetTextureAlphaMod(sdlTexture->_texture, 255);
    SDL_RenderCopy(renderer, sdlTexture->_texture, nullptr, &dst);
}

void SDLRenderer::drawTexture(const std::shared_ptr<ITexture>& texture,
                              const TextureDrawDesc& desc) {
    if (!texture) return;
    const auto* sdlTexture = dynamic_cast<SDLTexture*>(texture.get());
    if (!sdlTexture || !sdlTexture->_texture) {
        Log::error(
            "SDLRenderer::drawTexture(tinted): the texture was not created by this renderer "
            "(foreign ITexture implementation or a freed handle); skipping");
        return;
    }

    SDL_Rect dst = {desc.dst.x, desc.dst.y, desc.dst.width, desc.dst.height};
    SDL_Rect src;
    SDL_Rect* srcPtr = nullptr;
    if (desc.src) {
        src = {desc.src->x, desc.src->y, desc.src->width, desc.src->height};
        srcPtr = &src;
    }

    if (desc.tint) {
        SDL_SetTextureColorMod(sdlTexture->_texture, desc.tint->r, desc.tint->g, desc.tint->b);
        const float a = desc.tint->a / 255.0f * desc.alpha;
        SDL_SetTextureAlphaMod(sdlTexture->_texture,
                               static_cast<Uint8>(std::clamp(a * 255.0f, 0.0f, 255.0f)));
    } else {
        SDL_SetTextureColorMod(sdlTexture->_texture, 255, 255, 255);
        SDL_SetTextureAlphaMod(sdlTexture->_texture,
                               static_cast<Uint8>(std::clamp(desc.alpha * 255.0f, 0.0f, 255.0f)));
    }

    SDL_RenderCopy(renderer, sdlTexture->_texture, srcPtr, &dst);

    SDL_SetTextureColorMod(sdlTexture->_texture, 255, 255, 255);
    SDL_SetTextureAlphaMod(sdlTexture->_texture, 255);
}

void SDLRenderer::setSDLDrawColor(const Color& color) {
    SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
}

void SDLRenderer::drawRect(const Rect& rect, const Border& border) {
    if (border.thickness <= 0) return;
    setSDLDrawColor(border.color);
    for (int i = 0; i < border.thickness; ++i) {
        SDL_Rect r = {rect.x + i, rect.y + i, rect.width - 2 * i, rect.height - 2 * i};
        if (r.w <= 0 || r.h <= 0) break;
        SDL_RenderDrawRect(renderer, &r);
    }
}
void SDLRenderer::fillRect(const Rect& rect, const Color& color) {
    setSDLDrawColor(color);
    SDL_Rect r = {rect.x, rect.y, rect.width, rect.height};
    SDL_RenderFillRect(renderer, &r);
}
void SDLRenderer::fillRect(const Rect& rect, const Color& fillColor, const Border& border) {
    fillRect(rect, fillColor);
    if (border.thickness > 0) {
        drawRect(rect, border);
    }
}
void SDLRenderer::drawLine(int x1, int y1, int x2, int y2, const Color& color, int thickness) {
    if (thickness <= 1) {
        setSDLDrawColor(color);
        SDL_RenderDrawLine(renderer, x1, y1, x2, y2);
        return;
    }
    const auto width = static_cast<Uint8>(std::clamp(thickness, 1, 255));
    thickLineRGBA(renderer, x1, y1, x2, y2, width, color.r, color.g, color.b, color.a);
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
void SDLRenderer::drawArc(int cX, int cY, int r, float sA, float eA, const Border& border) {
    for (int i = 0; i < border.thickness; ++i) {
        arcRGBA(renderer, cX, cY, r - i, sA, eA, border.color.r, border.color.g, border.color.b,
                border.color.a);
    }
}
void SDLRenderer::drawRoundRect(const Rect& rect, int radius, const Border& border) {
    drawRoundedRectRingGeometry(renderer, rect, radius, border.thickness, border.color);
}
void SDLRenderer::fillRoundRect(const Rect& rect, int radius, const Color& color) {
    fillRoundedRectGeometry(renderer, rect, radius, color);
}
void SDLRenderer::fillRoundRect(const Rect& rect, int radius, const Color& fillColor,
                                const Border& border) {
    fillRoundedRectGeometry(renderer, rect, radius, fillColor);
    if (border.thickness > 0) {
        drawRoundedRectRingGeometry(renderer, rect, radius, border.thickness, border.color);
    }
}
void SDLRenderer::fillPolygon(const std::vector<PointI>& points, const Color& color) {
    if (points.size() < 3) return;
    std::vector<Sint16> vx, vy;
    vx.reserve(points.size());
    vy.reserve(points.size());
    for (const auto& point : points) {
        vx.push_back(point.x);
        vy.push_back(point.y);
    }
    filledPolygonRGBA(renderer, vx.data(), vy.data(), points.size(), color.r, color.g, color.b,
                      color.a);
}
}  // namespace DxvUI
