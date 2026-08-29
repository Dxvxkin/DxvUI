#ifndef DXVUI_IRENDERER_H
#define DXVUI_IRENDERER_H

#include <memory>
#include <vector>

#include "DxvUI/core.h"
#include "DxvUI/interfaces/IClipboard.h"
#include "DxvUI/interfaces/ITextEngine.h"
#include "DxvUI/interfaces/ITexture.h"

namespace DxvUI {

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
     * never exposes implicit "current font/color" state to widgets.
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

    // Texture Rendering
    virtual void drawTexture(std::shared_ptr<ITexture>& texture, const Rect& dstRect) = 0;

    // State Management
    virtual void setDrawColor(const Color& color) = 0;
    virtual Color getDrawColor() const = 0;

    // Primitives
    virtual void drawRect(const Rect& rect) = 0;
    virtual void fillRect(const Rect& rect) = 0;
    virtual void drawRect(const Rect& rect, const Color& color) = 0;
    virtual void fillRect(const Rect& rect, const Color& color) = 0;
    virtual void drawRect(const Rect& rect, const Border& border) = 0;
    virtual void fillRect(const Rect& rect, const Color& fillColor, const Border& border) = 0;

    virtual void drawLine(int x1, int y1, int x2, int y2) = 0;
    virtual void drawLine(int x1, int y1, int x2, int y2, const Color& color) = 0;

    virtual void drawCircle(int centerX, int centerY, int radius) = 0;
    virtual void fillCircle(int centerX, int centerY, int radius) = 0;
    virtual void drawCircle(int centerX, int centerY, int radius, const Color& color) = 0;
    virtual void fillCircle(int centerX, int centerY, int radius, const Color& color) = 0;
    virtual void drawCircle(int centerX, int centerY, int radius, const Border& border) = 0;
    virtual void fillCircle(int centerX, int centerY, int radius, const Color& fillColor,
                            const Border& border) = 0;

    virtual void drawArc(int centerX, int centerY, int radius, float startAngle,
                         float endAngle) = 0;
    virtual void drawArc(int centerX, int centerY, int radius, float startAngle, float endAngle,
                         const Color& color) = 0;
    virtual void drawArc(int centerX, int centerY, int radius, float startAngle, float endAngle,
                         const Border& border) = 0;

    virtual void drawRoundRect(const Rect& rect, int radius) = 0;
    virtual void fillRoundRect(const Rect& rect, int radius) = 0;
    virtual void drawRoundRect(const Rect& rect, int radius, const Color& color) = 0;
    virtual void fillRoundRect(const Rect& rect, int radius, const Color& color) = 0;
    virtual void drawRoundRect(const Rect& rect, int radius, const Border& border) = 0;
    virtual void fillRoundRect(const Rect& rect, int radius, const Color& fillColor,
                               const Border& border) = 0;

    virtual void drawPolygon(const std::vector<PointI>& points) = 0;
    virtual void fillPolygon(const std::vector<PointI>& points) = 0;
    virtual void drawPolygon(const std::vector<PointI>& points, const Color& color) = 0;
    virtual void fillPolygon(const std::vector<PointI>& points, const Color& color) = 0;
    virtual void drawPolygon(const std::vector<PointI>& points, const Border& border) = 0;
    virtual void fillPolygon(const std::vector<PointI>& points, const Color& fillColor,
                             const Border& border) = 0;
};

}  // namespace DxvUI

#endif  // DXVUI_IRENDERER_H
