#ifndef DXVUI_IRENDERER_H
#define DXVUI_IRENDERER_H

#include <memory>
#include <optional>
#include <vector>

#include "DxvUI/core.h"
#include "DxvUI/interfaces/ICanvas.h"
#include "DxvUI/interfaces/IClipboard.h"
#include "DxvUI/interfaces/IPlatformServices.h"
#include "DxvUI/interfaces/IRenderBackend.h"
#include "DxvUI/interfaces/ITextEngine.h"
#include "DxvUI/interfaces/ITexture.h"

namespace DxvUI {

/**
 * @class IRenderer
 * @brief Legacy combined backend contract (frame + platform + drawing).
 *
 * Stage 5 splits this god-interface into IRenderBackend (frame lifecycle + ICanvas)
 * and IPlatformServices (cursor/clipboard). IRenderer now inherits both for backward
 * compatibility: existing hosts that own SDLRenderer via IRenderer* continue to work,
 * while new code should depend on IRenderBackend + IPlatformServices + ICanvas.
 *
 * Widgets never see this interface — the draw pass paints through ICanvas/PaintContext,
 * and CanvasAdapter bridges until stage 5 turns ICanvas into real backend surface.
 */
class IRenderer : public IRenderBackend, public IPlatformServices {
   public:
    ~IRenderer() override = default;

    // IRenderBackend – frame lifecycle (legacy clear/present still required for hosts)
    void clear(const Color& color) override = 0;
    void present() override = 0;
    Size getViewportSize() const override = 0;
    float getDpiScale() const override { return 1.0f; }
    ITextEngine& getTextEngine() override = 0;

    // IRenderBackend – new frame API (stage 5): beginFrame returns ICanvas&
    // Default impl for backward compat: clear + return *this as ICanvas if backend implements ICanvas,
    // otherwise relies on CanvasAdapter. Real backends override.
    ICanvas& beginFrame(const Color& clearColor) override = 0;
    void endFrame() override = 0;

    // IPlatformServices
    void setCursor(CursorType type) override = 0;
    CursorType getCursor() const override = 0;
    IClipboard& getClipboard() override = 0;

    // Clipping (legacy int-based, now also on ICanvas float-based)
    virtual void pushClipRect(const Rect& rect) = 0;
    virtual void popClipRect() = 0;

    // Textures legacy
    virtual void drawTexture(const std::shared_ptr<ITexture>& texture, const Rect& dstRect) = 0;

    struct TextureDrawDesc {
        Rect dst;
        std::optional<Rect> src;
        std::optional<Color> tint;
        float alpha = 1.0f;
        float rotationDeg = 0.0f;
        bool flipX = false;
        bool flipY = false;
    };
    virtual void drawTexture(const std::shared_ptr<ITexture>& texture,
                             const TextureDrawDesc& desc) = 0;

    // Primitives legacy int-based (stage 2-4) – forwarded to ICanvas float-based in real backends
    virtual void drawRect(const Rect& rect, const Border& border) = 0;
    virtual void fillRect(const Rect& rect, const Color& color) = 0;
    virtual void fillRect(const Rect& rect, const Color& fillColor, const Border& border) = 0;
    virtual void drawLine(int x1, int y1, int x2, int y2, const Color& color, int thickness = 1) = 0;
    virtual void drawCircle(int centerX, int centerY, int radius, const Border& border) = 0;
    virtual void fillCircle(int centerX, int centerY, int radius, const Color& color) = 0;
    virtual void fillCircle(int centerX, int centerY, int radius, const Color& fillColor, const Border& border) = 0;
    virtual void drawArc(int centerX, int centerY, int radius, float startAngle, float endAngle, const Border& border) = 0;
    virtual void drawRoundRect(const Rect& rect, int radius, const Border& border) = 0;
    virtual void fillRoundRect(const Rect& rect, int radius, const Color& color) = 0;
    virtual void fillRoundRect(const Rect& rect, int radius, const Color& fillColor, const Border& border) = 0;
    virtual void fillPolygon(const std::vector<PointI>& points, const Color& color) = 0;
};

}  // namespace DxvUI

#endif  // DXVUI_IRENDERER_H
