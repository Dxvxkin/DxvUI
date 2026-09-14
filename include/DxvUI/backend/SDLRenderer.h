#ifndef DXVUI_SDLRENDERER_H
#define DXVUI_SDLRENDERER_H

#include <DxvUI/backend/SDLClipboard.h>
#include <DxvUI/interfaces/IRenderer.h>
#include <SDL.h>  // For SDL_Cursor

#include <map>
#include <memory>
#include <vector>

struct SDL_Window;
struct SDL_Renderer;

namespace DxvUI {

class SDLTextEngine;

class SDLRenderer : public IRenderer, public ICanvas {
   public:
    SDLRenderer(const char* title, int width, int height, bool vsync = true);
    // External-renderer mode (the primary integration for host apps that already
    // own an SDL initialization, window and renderer). The host keeps ownership
    // of the SDL_Renderer and must keep it alive longer than this SDLRenderer:
    // the destructor frees cached text textures that reference it. SDL_Init /
    // SDL_Quit are NOT called in this mode.
    explicit SDLRenderer(SDL_Renderer* externalRenderer);
    ~SDLRenderer() override;

    // --- SDL handle access ---
    // Exposes the backend SDL objects so a host can mix its own rendering in or
    // tune the renderer around the UI. In external mode the renderer is the one
    // passed in; SDLWindow() returns nullptr because the host owns the window.
    SDL_Renderer* getSDLHandle() const { return renderer; }
    SDL_Window* getSDLWindow() const { return window; }

    // --- IRenderBackend ---
    ICanvas& beginFrame(const Color& clearColor) override;
    void endFrame() override;
    float getDpiScale() const override;

    // --- IRenderer implementation (legacy int-based, kept for compat) ---
    void clear(const Color& color) override;
    void present() override;
    Size getViewportSize() const override;

    ITextEngine& getTextEngine() override;
    IClipboard& getClipboard() override;

    // Cursor (IPlatformServices)
    void setCursor(CursorType type) override;
    CursorType getCursor() const override;

    // Clipping legacy
    void pushClipRect(const Rect& rect) override;
    void popClipRect() override;

    // Texture Rendering legacy int-based (stage 3: tinted + src rect)
    void drawTexture(const std::shared_ptr<ITexture>& texture, const Rect& dstRect) override;
    void drawTexture(const std::shared_ptr<ITexture>& texture,
                     const TextureDrawDesc& desc) override;

    // --- ICanvas (float-based, stage 5 real backend) ---
    void pushClip(const RectF& rect) override;
    void popClip() override;
    void drawTexture(const std::shared_ptr<ITexture>& texture, const RectF& dstRect) override;
    void drawTexture(const std::shared_ptr<ITexture>& texture, const TextureDraw& draw) override;
    void fillRect(const RectF& rect, const Fill& fill) override;
    void strokeRect(const RectF& rect, const Stroke& stroke) override;
    void fillRoundRect(const RectF& rect, float radius, const Brush& brush) override;
    void fillCircle(const PointF& center, float radius, const Brush& brush) override;
    void strokeArc(const PointF& center, float radius, float startAngle, float endAngle,
                   const Stroke& stroke) override;
    void fillPolygon(std::span<const PointF> points, const Fill& fill) override;
    void drawLine(const PointF& from, const PointF& to, const Stroke& stroke) override;

    // Primitives (every call states its own color/border — the renderer keeps
    // no draw-color state)
    void drawRect(const Rect& rect, const Border& border) override;
    void fillRect(const Rect& rect, const Color& color) override;
    void fillRect(const Rect& rect, const Color& fillColor, const Border& border) override;
    void drawLine(int x1, int y1, int x2, int y2, const Color& color, int thickness = 1) override;
    void drawCircle(int centerX, int centerY, int radius, const Border& border) override;
    void fillCircle(int centerX, int centerY, int radius, const Color& color) override;
    void fillCircle(int centerX, int centerY, int radius, const Color& fillColor,
                    const Border& border) override;
    void drawArc(int centerX, int centerY, int radius, float startAngle, float endAngle,
                 const Border& border) override;
    void drawRoundRect(const Rect& rect, int radius, const Border& border) override;
    void fillRoundRect(const Rect& rect, int radius, const Color& color) override;
    void fillRoundRect(const Rect& rect, int radius, const Color& fillColor,
                       const Border& border) override;
    void fillPolygon(const std::vector<PointI>& points, const Color& color) override;

   private:
    SDL_Cursor* getSystemCursor(CursorType type);
    // Sets the SDL draw color; the only piece of "current state" the backend
    // needs internally (SDL drawing functions take no color argument).
    void setSDLDrawColor(const Color& color);

    SDL_Window* window = nullptr;
    SDL_Renderer* renderer = nullptr;
    bool ownsResources = false;

    CursorType currentCursorType = CursorType::Arrow;

    // Owns the fonts and rasterized-text textures; cleared before the SDL
    // renderer is destroyed (see ~SDLRenderer()).
    std::unique_ptr<SDLTextEngine> textEngine;

    SDLClipboard clipboard;

    // Saved clip rectangles for pushClipRect()/popClipRect() nesting. The bool
    // records whether the saved clip was enabled at push time, so popClipRect()
    // can restore the exact previous state (SDL treats a disabled clip as null).
    std::vector<std::pair<bool, Rect>> clipStack;

    std::map<CursorType, SDL_Cursor*> cursorCache;
};

}  // namespace DxvUI

#endif  // DXVUI_SDLRENDERER_H
