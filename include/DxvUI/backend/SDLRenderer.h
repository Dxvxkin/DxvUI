#ifndef DXVUI_SDLRENDERER_H
#define DXVUI_SDLRENDERER_H

#include <DxvUI/backend/SDLClipboard.h>
#include <DxvUI/interfaces/ICanvas.h>
#include <DxvUI/interfaces/IPlatformServices.h>
#include <DxvUI/interfaces/IRenderBackend.h>
#include <SDL.h>  // For SDL_Cursor

#include <map>
#include <memory>
#include <optional>
#include <span>
#include <vector>

struct SDL_Window;
struct SDL_Renderer;

namespace DxvUI {

class SDLTextEngine;

class SDLRenderer : public IRenderBackend, public ICanvas, public IPlatformServices {
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
    void clear(const Color& color) override;
    void present() override;
    ICanvas& beginFrame(const Color& clearColor) override;
    void endFrame() override;
    Size getViewportSize() const override;
    float getDpiScale() const override;
    ITextEngine& getTextEngine() override;
    std::shared_ptr<ITexture> createTexture(const ImageData& data) override;
    std::shared_ptr<ITexture> createRenderTarget(int width, int height) override;
    void beginRenderTarget(const std::shared_ptr<ITexture>& target) override;
    void endRenderTarget() override;

    // --- ICanvas (float Brush-based) ---
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

    // --- IPlatformServices ---
    void setCursor(CursorType type) override;
    CursorType getCursor() const override;
    IClipboard& getClipboard() override;

    // Flushes the internal same-color fill batch immediately. Drawing is deferred
    // (batching); hosts that need synchronous readback (pixel tests, offscreen
    // capture) call this before reading pixels they just drew.
    void flushBatch();

   private:
    SDL_Cursor* getSystemCursor(CursorType type);

    // --- Int-based painting helpers (stage 7) ---
    // The old int drawing contract is no longer public: the float ICanvas
    // methods round their geometry with rounded()/std::lround and delegate
    // here, so rasterization stays byte-identical to the pre-stage-7 backend.

    // Clip stack (nesting + empty-clip suppression), see clipStack_/clipEmpty_.
    void pushClipRect(const Rect& rect);
    void popClipRect();

    void drawRect(const Rect& rect, const Border& border);
    void drawLine(int x1, int y1, int x2, int y2, const Color& color, int thickness = 1);
    void drawCircle(int centerX, int centerY, int radius, const Border& border);
    void fillCircle(int centerX, int centerY, int radius, const Color& color);
    void fillCircle(int centerX, int centerY, int radius, const Color& fillColor,
                    const Border& border);
    void drawArc(int centerX, int centerY, int radius, float startAngle, float endAngle,
                 const Border& border);
    void drawRoundRect(const Rect& rect, int radius, const Border& border);
    void fillRoundRect(const Rect& rect, int radius, const Color& color);
    void fillRoundRect(const Rect& rect, int radius, const Color& fillColor, const Border& border);

    SDL_Window* window = nullptr;
    SDL_Renderer* renderer = nullptr;
    bool ownsResources = false;

    CursorType currentCursorType = CursorType::Arrow;

    // Owns the fonts and rasterized-text textures; cleared before the SDL
    // renderer is destroyed (see ~SDLRenderer()).
    std::unique_ptr<SDLTextEngine> textEngine;

    SDLClipboard clipboard;

    // Saved clip rectangles for clip push/pop nesting. The bool records whether
    // the saved clip was enabled at push time, so pop can restore the exact
    // previous state (SDL treats a disabled clip as null); `empty` records
    // whether drawing was already fully suppressed (see clipEmpty_).
    struct ClipState {
        bool enabled = false;
        Rect rect;
        bool empty = false;
    };
    std::vector<ClipState> clipStack;

    // True when the intersection of all pushed clips is empty. SDL has no empty
    // clip (an SDL_Rect with w/h <= 0 disables clipping), so pushClipRect()
    // falls back to a 1x1 corner rect and this flag suppresses every draw instead.
    bool clipEmpty_ = false;

    // Render-target stack for createRenderTarget/begin/end (stage 6b)
    std::vector<SDL_Texture*> renderTargetStack;

    // Simple batching for fillRect (stage 6b) – accumulates rects of same color
    struct FillRectBatch {
        Color color;
        std::vector<Rect> rects;
    };
    std::optional<FillRectBatch> fillRectBatch_;
    void flushFillRectBatch();
    void batchFillRect(const Rect& rect, const Color& color);

    std::map<CursorType, SDL_Cursor*> cursorCache;
};

}  // namespace DxvUI

#endif  // DXVUI_SDLRENDERER_H