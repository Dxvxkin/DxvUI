#ifndef DXVUI_SDLRENDERER_H
#define DXVUI_SDLRENDERER_H

#include <DxvUI/interfaces/IRenderer.h>
#include <SDL.h>  // For SDL_Cursor

#include <map>
#include <memory>
#include <string>
#include <vector>

struct SDL_Window;
struct SDL_Renderer;

namespace DxvUI {

class SDLTextEngine;

class SDLRenderer : public IRenderer {
   public:
    SDLRenderer(const char* title, int width, int height);
    explicit SDLRenderer(SDL_Renderer* externalRenderer);
    ~SDLRenderer() override;

    // --- IRenderer implementation ---
    void clear(const Color& color) override;
    void present() override;
    Size getViewportSize() const override;

    ITextEngine& getTextEngine() override;

    // Cursor
    void setCursor(CursorType type) override;
    CursorType getCursor() const override;

    // Clipping
    void pushClipRect(const Rect& rect) override;
    void popClipRect() override;

    // Texture Rendering
    void drawTexture(std::shared_ptr<ITexture>& texture, const Rect& dstRect) override;

    // State Management
    void setDrawColor(const Color& color) override;
    Color getDrawColor() const override;

    // Primitives
    void drawRect(const Rect& rect) override;
    void fillRect(const Rect& rect) override;
    void drawRect(const Rect& rect, const Color& color) override;
    void fillRect(const Rect& rect, const Color& color) override;
    void drawRect(const Rect& rect, const Border& border) override;
    void fillRect(const Rect& rect, const Color& fillColor, const Border& border) override;
    void drawLine(int x1, int y1, int x2, int y2) override;
    void drawLine(int x1, int y1, int x2, int y2, const Color& color) override;
    void drawCircle(int centerX, int centerY, int radius) override;
    void fillCircle(int centerX, int centerY, int radius) override;
    void drawCircle(int centerX, int centerY, int radius, const Color& color) override;
    void fillCircle(int centerX, int centerY, int radius, const Color& color) override;
    void drawCircle(int centerX, int centerY, int radius, const Border& border) override;
    void fillCircle(int centerX, int centerY, int radius, const Color& fillColor,
                    const Border& border) override;
    void drawArc(int centerX, int centerY, int radius, float startAngle, float endAngle) override;
    void drawArc(int centerX, int centerY, int radius, float startAngle, float endAngle,
                 const Color& color) override;
    void drawArc(int centerX, int centerY, int radius, float startAngle, float endAngle,
                 const Border& border) override;
    void drawRoundRect(const Rect& rect, int radius) override;
    void fillRoundRect(const Rect& rect, int radius) override;
    void drawRoundRect(const Rect& rect, int radius, const Color& color) override;
    void fillRoundRect(const Rect& rect, int radius, const Color& color) override;
    void drawRoundRect(const Rect& rect, int radius, const Border& border) override;
    void fillRoundRect(const Rect& rect, int radius, const Color& fillColor,
                       const Border& border) override;
    void drawPolygon(const std::vector<PointI>& points) override;
    void fillPolygon(const std::vector<PointI>& points) override;
    void drawPolygon(const std::vector<PointI>& points, const Color& color) override;
    void fillPolygon(const std::vector<PointI>& points, const Color& color) override;
    void drawPolygon(const std::vector<PointI>& points, const Border& border) override;
    void fillPolygon(const std::vector<PointI>& points, const Color& fillColor,
                     const Border& border) override;

   private:
    SDL_Cursor* getSystemCursor(CursorType type);

    SDL_Window* window = nullptr;
    SDL_Renderer* renderer = nullptr;
    bool ownsResources = false;

    Color currentColor;
    CursorType currentCursorType = CursorType::Arrow;

    // Owns the fonts and rasterized-text textures; cleared before the SDL
    // renderer is destroyed (see ~SDLRenderer()).
    std::unique_ptr<SDLTextEngine> textEngine;

    // Saved clip rectangles for pushClipRect()/popClipRect() nesting. The bool
    // records whether the saved clip was enabled at push time, so popClipRect()
    // can restore the exact previous state (SDL treats a disabled clip as null).
    std::vector<std::pair<bool, Rect>> clipStack;

    std::map<CursorType, SDL_Cursor*> cursorCache;
};

}  // namespace DxvUI

#endif  // DXVUI_SDLRENDERER_H
