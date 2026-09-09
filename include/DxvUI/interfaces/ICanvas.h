#ifndef DXVUI_ICANVAS_H
#define DXVUI_ICANVAS_H

#include <memory>
#include <vector>

#include "DxvUI/core.h"
#include "DxvUI/interfaces/ITexture.h"

namespace DxvUI {

class ITextEngine;

/**
 * @struct FrameInfo
 * @brief Per-frame state handed down the whole draw pass.
 *
 * Stage 5 of the rendering refactoring (docs/RENDERING_REFACTORING.md)
 * extends this with the frame time, delta, frame counter and the DPI scale;
 * the viewport comes first because the draw pass needs it for culling from
 * day one.
 */
struct FrameInfo {
    // The visible area in screen coordinates, used for viewport culling.
    Rect viewport;
};

/**
 * @class ICanvas
 * @brief Backend-neutral, painting-only surface.
 *
 * The narrow contract the draw pass uses: clipping, texture blitting and
 * explicit-color primitives. Unlike IRenderer it carries no frame lifecycle
 * (clear/present), no draw-color state (every method takes its color or
 * border explicitly) and no platform services (cursor, clipboard) — those
 * stay on the renderer until the backend split.
 *
 * This is the stage-1 shape of the rendering refactoring: the primitive set
 * mirrors the renderer's explicit-color overloads. Stage 2 collapses it
 * further around a Brush argument, so custom widgets should already prefer
 * the fill/draw pairs that carry all of their arguments.
 */
class ICanvas {
   public:
    virtual ~ICanvas() = default;

    // --- Clipping ---
    // A pushed rect intersects the current clip rect (the SDL backend
    // guarantees this since stage 0); every push must be matched by a pop.
    ///@{
    virtual void pushClipRect(const Rect& rect) = 0;
    virtual void popClipRect() = 0;
    ///@}

    // --- Texture Rendering ---
    virtual void drawTexture(const std::shared_ptr<ITexture>& texture, const Rect& dstRect) = 0;

    // --- Rectangles ---
    ///@{
    virtual void drawRect(const Rect& rect, const Color& color) = 0;
    virtual void fillRect(const Rect& rect, const Color& color) = 0;
    virtual void drawRect(const Rect& rect, const Border& border) = 0;
    virtual void fillRect(const Rect& rect, const Color& fillColor, const Border& border) = 0;
    ///@}

    // --- Lines ---
    virtual void drawLine(int x1, int y1, int x2, int y2, const Color& color) = 0;

    // --- Circles ---
    ///@{
    virtual void drawCircle(int centerX, int centerY, int radius, const Color& color) = 0;
    virtual void fillCircle(int centerX, int centerY, int radius, const Color& color) = 0;
    virtual void drawCircle(int centerX, int centerY, int radius, const Border& border) = 0;
    virtual void fillCircle(int centerX, int centerY, int radius, const Color& fillColor,
                            const Border& border) = 0;
    ///@}

    // --- Arcs ---
    ///@{
    virtual void drawArc(int centerX, int centerY, int radius, float startAngle, float endAngle,
                         const Color& color) = 0;
    virtual void drawArc(int centerX, int centerY, int radius, float startAngle, float endAngle,
                         const Border& border) = 0;
    ///@}

    // --- Rounded Rectangles ---
    ///@{
    virtual void drawRoundRect(const Rect& rect, int radius, const Color& color) = 0;
    virtual void fillRoundRect(const Rect& rect, int radius, const Color& color) = 0;
    virtual void drawRoundRect(const Rect& rect, int radius, const Border& border) = 0;
    virtual void fillRoundRect(const Rect& rect, int radius, const Color& fillColor,
                               const Border& border) = 0;
    ///@}

    // --- Polygons ---
    ///@{
    virtual void drawPolygon(const std::vector<PointI>& points, const Color& color) = 0;
    virtual void fillPolygon(const std::vector<PointI>& points, const Color& color) = 0;
    virtual void drawPolygon(const std::vector<PointI>& points, const Border& border) = 0;
    virtual void fillPolygon(const std::vector<PointI>& points, const Color& fillColor,
                             const Border& border) = 0;
    ///@}
};

/**
 * @class PaintContext
 * @brief Everything a node needs while painting.
 *
 * Bundles the canvas (painting), the text engine (measure/rasterize) and the
 * frame state (viewport, later timing/DPI), and nothing else. The draw hooks
 * receive it instead of a raw IRenderer&, so widget paint code cannot reach
 * the renderer's frame lifecycle or platform services by construction.
 */
class PaintContext {
   public:
    PaintContext(ICanvas& canvas, ITextEngine& text, FrameInfo frame)
        : canvas_(canvas), text_(text), frame_(frame) {}

    ICanvas& canvas() const { return canvas_; }
    ITextEngine& text() const { return text_; }
    const FrameInfo& frame() const { return frame_; }

   private:
    ICanvas& canvas_;
    ITextEngine& text_;
    FrameInfo frame_;
};

/**
 * @class ClipGuard
 * @brief RAII pairing for ICanvas::pushClipRect()/popClipRect().
 *
 * Pushes the rect when enabled and pops it on scope exit, so an early return
 * between the push and the pop can no longer unbalance the canvas' clip
 * stack. A disabled guard (clipContent == false) pushes nothing.
 */
class ClipGuard {
   public:
    ClipGuard(ICanvas& canvas, const Rect& rect, bool enabled)
        : canvas_(enabled ? &canvas : nullptr) {
        if (enabled) {
            canvas.pushClipRect(rect);
        }
    }

    ~ClipGuard() {
        if (canvas_) {
            canvas_->popClipRect();
        }
    }

    ClipGuard(const ClipGuard&) = delete;
    ClipGuard& operator=(const ClipGuard&) = delete;

   private:
    ICanvas* canvas_;
};

}  // namespace DxvUI

#endif  // DXVUI_ICANVAS_H
