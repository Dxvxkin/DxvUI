#ifndef DXVUI_ICANVAS_H
#define DXVUI_ICANVAS_H

#include <memory>
#include <optional>
#include <span>

#include "DxvUI/core.h"
#include "DxvUI/interfaces/ITexture.h"
#include "DxvUI/style/Brush.h"

namespace DxvUI {

class ITextEngine;

/**
 * @struct FrameInfo
 * @brief Per-frame state handed down the whole draw pass.
 *
 * Stage 5 of the rendering refactoring (docs/RENDERING_REFACTORING.md)
 * extends this with the frame time, delta, frame counter and the DPI scale;
 * the viewport comes first because the draw pass needs it for culling from
 * day one. Stage 3 already adds timeMs for the caret blink, so the text view
 * no longer depends on SDL_GetTicks().
 */
struct FrameInfo {
    // The visible area in screen coordinates, used for viewport culling.
    Rect viewport;
    // Monotonic time of the frame in milliseconds, used for caret blinking and
    // future declarative transitions (stage 5 adds dt/frame/dpi).
    double timeMs = 0.0;

    // Stage 6b: damage tracking – union of dirty rects since last frame
    Rect damageUnion{0, 0, 0, 0};
    bool hasDamage = false;
    bool fullRedraw = true;
};

/**
 * @class ICanvas
 * @brief Backend-neutral, painting-only surface.
 *
 * The narrow contract the draw pass uses, and the stage-3 shape of the
 * rendering refactoring (docs/RENDERING_REFACTORING.md §3.1 + §3.3):
 *
 * - painting only: no frame lifecycle (clear/present), no cursor/clipboard;
 * - no implicit draw-color state — every method carries its Fill/Stroke/Brush,
 *   so the old \"set color, then draw\" pairing and its combinatorial overload
 *   set (fill/draw x color/border x 6 shapes) are gone;
 * - float geometry (RectF/PointF): the canvas is float, layout stays integer,
 *   and the backend rounds at its own boundary;
 * - clipping intersects the current clip (scissors semantics) and must be
 *   balanced: prefer ClipGuard over manual push/pop;
 * - stage 3 adds TextureDraw with srcRect/tint/alpha for the glyph atlas:
 *   white glyphs are tinted at draw time instead of baking the color into the
 *   texture.
 */
class ICanvas {
   public:
    virtual ~ICanvas() = default;

    // --- Clipping ---
    // A pushed rect intersects the current clip rect (the SDL backend
    // guarantees this since stage 0); every push must be matched by a pop.
    ///@{
    virtual void pushClip(const RectF& rect) = 0;
    virtual void popClip() = 0;
    ///@}

    // --- Textures ---
    /**
     * @brief Parameters for a textured draw (stage 3: tint + srcRect).
     *
     * dst is the destination rect in screen space. src, when set, selects a
     * sub-rectangle of the texture (glyph atlas, 9-slice, visible slice). tint,
     * when set, modulates a white (or grayscale) source — the glyph atlas path:
     * glyphs are rasterized once as white and tinted per draw call instead of
     * baking the color into the cache key. alpha is an extra opacity multiplier.
     */
    struct TextureDraw {
        RectF dst;
        std::optional<RectF> src;
        std::optional<Color> tint;
        float alpha = 1.0f;
        float rotationDeg = 0.0f;
        bool flipX = false;
        bool flipY = false;
    };

    virtual void drawTexture(const std::shared_ptr<ITexture>& texture, const RectF& dstRect) = 0;
    virtual void drawTexture(const std::shared_ptr<ITexture>& texture, const TextureDraw& draw) = 0;

    // --- Rectangles ---
    ///@{
    virtual void fillRect(const RectF& rect, const Fill& fill) = 0;
    virtual void strokeRect(const RectF& rect, const Stroke& stroke) = 0;
    // The rounded rect takes a full Brush so a border can be painted together
    // with the fill in one call (the backend has a dedicated path for that).
    virtual void fillRoundRect(const RectF& rect, float radius, const Brush& brush) = 0;
    ///@}

    // --- Circles ---
    virtual void fillCircle(const PointF& center, float radius, const Brush& brush) = 0;

    // --- Arcs ---
    virtual void strokeArc(const PointF& center, float radius, float startAngle, float endAngle,
                           const Stroke& stroke) = 0;

    // --- Polygons ---
    virtual void fillPolygon(std::span<const PointF> points, const Fill& fill) = 0;

    // --- Lines ---
    virtual void drawLine(const PointF& from, const PointF& to, const Stroke& stroke) = 0;
};

/**
 * @class PaintContext
 * @brief Everything a node needs while painting.
 *
 * Bundles the canvas (painting), the text engine (measure/rasterize) and the
 * frame state (viewport, later timing/DPI), and nothing else. The draw hooks
 * receive it instead of a raw backend object, so widget paint code cannot reach
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
 * @brief RAII pairing for ICanvas::pushClip()/popClip().
 *
 * Pushes the rect when enabled and pops it on scope exit, so an early return
 * between the push and the pop can no longer unbalance the canvas' clip
 * stack. A disabled guard (clipContent == false) pushes nothing.
 */
class ClipGuard {
   public:
    ClipGuard(ICanvas& canvas, const RectF& rect, bool enabled)
        : canvas_(enabled ? &canvas : nullptr) {
        if (enabled) {
            canvas.pushClip(rect);
        }
    }

    ~ClipGuard() {
        if (canvas_) {
            canvas_->popClip();
        }
    }

    ClipGuard(const ClipGuard&) = delete;
    ClipGuard& operator=(const ClipGuard&) = delete;

   private:
    ICanvas* canvas_;
};

}  // namespace DxvUI

#endif  // DXVUI_ICANVAS_H
