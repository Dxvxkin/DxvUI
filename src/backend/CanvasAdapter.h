#ifndef DXVUI_CANVASADAPTER_H
#define DXVUI_CANVASADAPTER_H

// Internal to the library (lives under src/, not installed): bridges the new
// painting-only ICanvas contract onto the existing IRenderer backends.

#include <memory>
#include <vector>

#include "DxvUI/interfaces/ICanvas.h"
#include "DxvUI/interfaces/IRenderer.h"

namespace DxvUI {

/**
 * @brief Adapts an IRenderer backend to the ICanvas painting contract.
 *
 * Transitional glue of the rendering refactoring (stage 1,
 * docs/RENDERING_REFACTORING.md): the draw pass talks to ICanvas, while the
 * existing backends (SDLRenderer, test fakes) still implement IRenderer. The
 * adapter forwards 1:1 and holds no state, so it is created cheaply per frame
 * on the stack of the draw entry points (SceneNode::draw(IRenderer&)). It
 * disappears at stage 5, when the backend split gives ICanvas its own
 * implementations.
 */
class CanvasAdapter final : public ICanvas {
   public:
    explicit CanvasAdapter(IRenderer& renderer) : renderer_(renderer) {}

    CanvasAdapter(const CanvasAdapter&) = delete;
    CanvasAdapter& operator=(const CanvasAdapter&) = delete;

    // --- ICanvas ---
    void pushClipRect(const Rect& rect) override { renderer_.pushClipRect(rect); }
    void popClipRect() override { renderer_.popClipRect(); }

    void drawTexture(const std::shared_ptr<ITexture>& texture, const Rect& dstRect) override {
        renderer_.drawTexture(texture, dstRect);
    }

    void drawRect(const Rect& rect, const Color& color) override {
        renderer_.drawRect(rect, color);
    }
    void fillRect(const Rect& rect, const Color& color) override {
        renderer_.fillRect(rect, color);
    }
    void drawRect(const Rect& rect, const Border& border) override {
        renderer_.drawRect(rect, border);
    }
    void fillRect(const Rect& rect, const Color& fillColor, const Border& border) override {
        renderer_.fillRect(rect, fillColor, border);
    }

    void drawLine(int x1, int y1, int x2, int y2, const Color& color) override {
        renderer_.drawLine(x1, y1, x2, y2, color);
    }

    void drawCircle(int centerX, int centerY, int radius, const Color& color) override {
        renderer_.drawCircle(centerX, centerY, radius, color);
    }
    void fillCircle(int centerX, int centerY, int radius, const Color& color) override {
        renderer_.fillCircle(centerX, centerY, radius, color);
    }
    void drawCircle(int centerX, int centerY, int radius, const Border& border) override {
        renderer_.drawCircle(centerX, centerY, radius, border);
    }
    void fillCircle(int centerX, int centerY, int radius, const Color& fillColor,
                    const Border& border) override {
        renderer_.fillCircle(centerX, centerY, radius, fillColor, border);
    }

    void drawArc(int centerX, int centerY, int radius, float startAngle, float endAngle,
                 const Color& color) override {
        renderer_.drawArc(centerX, centerY, radius, startAngle, endAngle, color);
    }
    void drawArc(int centerX, int centerY, int radius, float startAngle, float endAngle,
                 const Border& border) override {
        renderer_.drawArc(centerX, centerY, radius, startAngle, endAngle, border);
    }

    void drawRoundRect(const Rect& rect, int radius, const Color& color) override {
        renderer_.drawRoundRect(rect, radius, color);
    }
    void fillRoundRect(const Rect& rect, int radius, const Color& color) override {
        renderer_.fillRoundRect(rect, radius, color);
    }
    void drawRoundRect(const Rect& rect, int radius, const Border& border) override {
        renderer_.drawRoundRect(rect, radius, border);
    }
    void fillRoundRect(const Rect& rect, int radius, const Color& fillColor,
                       const Border& border) override {
        renderer_.fillRoundRect(rect, radius, fillColor, border);
    }

    void drawPolygon(const std::vector<PointI>& points, const Color& color) override {
        renderer_.drawPolygon(points, color);
    }
    void fillPolygon(const std::vector<PointI>& points, const Color& color) override {
        renderer_.fillPolygon(points, color);
    }
    void drawPolygon(const std::vector<PointI>& points, const Border& border) override {
        renderer_.drawPolygon(points, border);
    }
    void fillPolygon(const std::vector<PointI>& points, const Color& fillColor,
                     const Border& border) override {
        renderer_.fillPolygon(points, fillColor, border);
    }

   private:
    IRenderer& renderer_;
};

}  // namespace DxvUI

#endif  // DXVUI_CANVASADAPTER_H
