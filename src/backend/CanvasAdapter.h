#ifndef DXVUI_CANVASADAPTER_H
#define DXVUI_CANVASADAPTER_H

// Internal to the library (lives under src/, not installed): bridges the
// painting-only ICanvas contract onto the existing IRenderer backends.

#include <algorithm>
#include <cmath>
#include <memory>
#include <optional>
#include <span>
#include <vector>

#include "DxvUI/interfaces/ICanvas.h"
#include "DxvUI/interfaces/IRenderer.h"

namespace DxvUI {

/**
 * @brief Adapts an IRenderer backend to the ICanvas painting contract.
 *
 * Transitional glue of the rendering refactoring (stage 2-3,
 * docs/RENDERING_REFACTORING.md): the draw pass talks float Brush-based
 * ICanvas, while the existing backends still implement the int-based
 * IRenderer. The adapter therefore does exactly two things — rounds float
 * geometry at the backend boundary and translates a Brush into the explicit
 * renderer overloads (fill + border, fill only or border only).
 *
 * Stage 3 adds TextureDraw with srcRect/tint/alpha for the glyph atlas.
 *
 * It holds no state, so it is created cheaply per frame on the stack of the
 * draw entry points (SceneNode::draw(IRenderer&)). It disappears at stage 5,
 * when the backend split gives ICanvas its own implementations.
 */
class CanvasAdapter final : public ICanvas {
   public:
    explicit CanvasAdapter(IRenderer& renderer) : renderer_(renderer) {}

    CanvasAdapter(const CanvasAdapter&) = delete;
    CanvasAdapter& operator=(const CanvasAdapter&) = delete;

    // --- ICanvas ---
    void pushClip(const RectF& rect) override { renderer_.pushClipRect(rect.rounded()); }
    void popClip() override { renderer_.popClipRect(); }

    void drawTexture(const std::shared_ptr<ITexture>& texture, const RectF& dstRect) override {
        renderer_.drawTexture(texture, dstRect.rounded());
    }

    void drawTexture(const std::shared_ptr<ITexture>& texture,
                     const TextureDraw& draw) override {
        // Translate float TextureDraw -> int TextureDrawDesc
        IRenderer::TextureDrawDesc desc;
        desc.dst = draw.dst.rounded();
        if (draw.src) {
            desc.src = draw.src->rounded();
        }
        desc.tint = draw.tint;
        desc.alpha = draw.alpha;
        desc.rotationDeg = draw.rotationDeg;
        desc.flipX = draw.flipX;
        desc.flipY = draw.flipY;
        renderer_.drawTexture(texture, desc);
    }

    void fillRect(const RectF& rect, const Fill& fill) override {
        renderer_.fillRect(rect.rounded(), fill.color);
    }

    void strokeRect(const RectF& rect, const Stroke& stroke) override {
        renderer_.drawRect(rect.rounded(), toBorder(stroke));
    }

    void fillRoundRect(const RectF& rect, float radius, const Brush& brush) override {
        if (brush.isEmpty()) {
            return;
        }
        const Rect pixelRect = rect.rounded();
        const int pixelRadius = toPixels(radius, /*minimum=*/0);

        if (brush.fill && brush.stroke) {
            renderer_.fillRoundRect(pixelRect, pixelRadius, brush.fill->color,
                                    toBorder(*brush.stroke));
        } else if (brush.fill) {
            renderer_.fillRoundRect(pixelRect, pixelRadius, brush.fill->color);
        } else {
            renderer_.drawRoundRect(pixelRect, pixelRadius, toBorder(*brush.stroke));
        }
    }

    void fillCircle(const PointF& center, float radius, const Brush& brush) override {
        if (brush.isEmpty()) {
            return;
        }
        const PointI pixelCenter = center.rounded();
        const int pixelRadius = toPixels(radius, /*minimum=*/0);

        if (brush.fill && brush.stroke) {
            renderer_.fillCircle(pixelCenter.x, pixelCenter.y, pixelRadius, brush.fill->color,
                                 toBorder(*brush.stroke));
        } else if (brush.fill) {
            renderer_.fillCircle(pixelCenter.x, pixelCenter.y, pixelRadius, brush.fill->color);
        } else {
            renderer_.drawCircle(pixelCenter.x, pixelCenter.y, pixelRadius,
                                 toBorder(*brush.stroke));
        }
    }

    void strokeArc(const PointF& center, float radius, float startAngle, float endAngle,
                   const Stroke& stroke) override {
        const PointI pixelCenter = center.rounded();
        renderer_.drawArc(pixelCenter.x, pixelCenter.y, toPixels(radius, /*minimum=*/0), startAngle,
                          endAngle, toBorder(stroke));
    }

    void fillPolygon(std::span<const PointF> points, const Fill& fill) override {
        if (points.size() < 3) {
            return;
        }
        std::vector<PointI> pixelPoints;
        pixelPoints.reserve(points.size());
        for (const PointF& point : points) {
            pixelPoints.push_back(point.rounded());
        }
        renderer_.fillPolygon(pixelPoints, fill.color);
    }

    void drawLine(const PointF& from, const PointF& to, const Stroke& stroke) override {
        const PointI pixelFrom = from.rounded();
        const PointI pixelTo = to.rounded();
        renderer_.drawLine(pixelFrom.x, pixelFrom.y, pixelTo.x, pixelTo.y, stroke.color,
                           toPixels(stroke.thickness, /*minimum=*/1));
    }

   private:
    static Border toBorder(const Stroke& stroke) {
        return {stroke.color, toPixels(stroke.thickness, /*minimum=*/1)};
    }

    static int toPixels(float value, int minimum) {
        return std::max(minimum, static_cast<int>(std::lround(value)));
    }

    IRenderer& renderer_;
};

}  // namespace DxvUI

#endif  // DXVUI_CANVASADAPTER_H
