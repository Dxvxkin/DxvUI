#ifndef DXVUI_IRENDERER_H
#define DXVUI_IRENDERER_H

#include <memory>
#include <optional>
#include <vector>

#include "DxvUI/core.h"
#include "DxvUI/interfaces/IClipboard.h"
#include "DxvUI/interfaces/ITextEngine.h"
#include "DxvUI/interfaces/ITexture.h"

namespace DxvUI {

/**
 * @class IRenderer
 * @brief Backend contract: frame lifecycle, platform services and drawing.
 *
 * Widgets never see this interface — the draw pass paints through the narrow
 * ICanvas/PaintContext contract, and CanvasAdapter bridges the two until the
 * stage-5 backend split (docs/RENDERING_REFACTORING.md) turns ICanvas into a
 * real backend surface. What remains here for host apps is the frame
 * lifecycle (clear/present), the platform services (text engine, clipboard,
 * cursor) and the explicitly-colored primitives backends must implement.
 *
 * Stage 2 removed the implicit draw-color state (setDrawColor/getDrawColor)
 * and with it every overload that depended on it: a draw call now always
 * states its own color or border, so no call site can be affected by whoever
 * painted before it.
 *
 * Stage 3 adds a tinted, src-rect-aware texture draw for the glyph atlas:
 * white glyphs are tinted at draw time instead of baking the color into the
 * cache key.
 */
class IRenderer {
   public:
    virtual ~IRenderer() = default;

    virtual void clear(const Color& color) = 0;
    virtual void present() = 0;
    virtual Size getViewportSize() const = 0;

    /**
     * @brief Gets the backend-neutral text engine owned by this renderer.
     *
     * Fonts, text measurement and text rasterization live behind the
     * ITextEngine interface instead of the renderer itself, so the renderer
     * never exposes implicit \"current font/color\" state to widgets.
     */
    virtual ITextEngine& getTextEngine() = 0;

    /**
     * @brief Gets the backend-neutral clipboard owned by this renderer.
     *
     * Widgets use it for copy/paste without depending on the SDL clipboard API.
     */
    virtual IClipboard& getClipboard() = 0;

    // Cursor
    virtual void setCursor(CursorType type) = 0;
    virtual CursorType getCursor() const = 0;

    // Clipping
    virtual void pushClipRect(const Rect& rect) = 0;
    virtual void popClipRect() = 0;

    /**
     * @brief Draws a texture into the destination rectangle.
     *
     * Takes the texture by const reference (a rasterized texture can be drawn
     * straight from a temporary) and only accepts textures created by this
     * renderer's backend: a foreign ITexture implementation is rejected with
     * a logged error instead of being blindly cast.
     * @param texture The texture to draw; a null texture is a no-op.
     * @param dstRect The destination rectangle in screen coordinates.
     */
    virtual void drawTexture(const std::shared_ptr<ITexture>& texture, const Rect& dstRect) = 0;

    /**
     * @brief Draws a (sub)texture with optional tint and alpha (glyph atlas).
     *
     * Stage 3: white glyphs are rasterized once and tinted per draw call.
     * srcRect selects a sub-rectangle of the texture (atlas), tint modulates
     * it, alpha is an extra opacity multiplier (1 = opaque). A null texture is
     * a no-op; a null src means the whole texture.
     */
    struct TextureDrawDesc {
        Rect dst;
        std::optional<Rect> src;
        std::optional<Color> tint;
        float alpha = 1.0f;
    };
    virtual void drawTexture(const std::shared_ptr<ITexture>& texture,
                             const TextureDrawDesc& desc) = 0;

    // Primitives. Each shape exposes exactly the paths the painting contract
    // needs: a solid fill, a border-only outline, a fill+border pair and, for
    // lines, a thickness. The canvas (ICanvas) is the place to add gradients,
    // tints or opacity — extend Fill/Stroke there, not this overload set.
    ///@{
    virtual void drawRect(const Rect& rect, const Border& border) = 0;
    virtual void fillRect(const Rect& rect, const Color& color) = 0;
    virtual void fillRect(const Rect& rect, const Color& fillColor, const Border& border) = 0;

    /// @param thickness Line width in pixels; 1 is a hairline.
    virtual void drawLine(int x1, int y1, int x2, int y2, const Color& color,
                          int thickness = 1) = 0;

    virtual void drawCircle(int centerX, int centerY, int radius, const Border& border) = 0;
    virtual void fillCircle(int centerX, int centerY, int radius, const Color& color) = 0;
    virtual void fillCircle(int centerX, int centerY, int radius, const Color& fillColor,
                            const Border& border) = 0;

    virtual void drawArc(int centerX, int centerY, int radius, float startAngle, float endAngle,
                         const Border& border) = 0;

    virtual void drawRoundRect(const Rect& rect, int radius, const Border& border) = 0;
    virtual void fillRoundRect(const Rect& rect, int radius, const Color& color) = 0;
    virtual void fillRoundRect(const Rect& rect, int radius, const Color& fillColor,
                               const Border& border) = 0;

    virtual void fillPolygon(const std::vector<PointI>& points, const Color& color) = 0;
    ///@}
};

}  // namespace DxvUI

#endif  // DXVUI_IRENDERER_H
