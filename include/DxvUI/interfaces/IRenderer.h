#ifndef DXVUI_IRENDERER_H
#define DXVUI_IRENDERER_H

#include "DxvUI/core.h"
#include <string>
#include <memory>

namespace DxvUI {

    class ITexture
    {
    public:
        virtual ~ITexture() = default;
        virtual int getWidth() const = 0;
        virtual int getHeight() const = 0;
    };

    class IRenderer {
    public:
        virtual ~IRenderer() = default;

        virtual void clear(const Color& color) = 0;
        virtual void present() = 0;
        virtual Size getViewportSize() const = 0;

        // Cursor
        virtual void setCursor(CursorType type) = 0;
        virtual CursorType getCursor() const = 0;

        // Text Rendering
        virtual std::shared_ptr<ITexture> createTextTexture(const std::string& text) = 0;
        virtual Rect measureText(const std::string& text, const std::string& fontPath, int fontSize) = 0;
        virtual void drawTexture(std::shared_ptr<ITexture>& texture, const Rect& dstRect) = 0;

        // State Management
        virtual void setDrawColor(const Color& color) = 0;
        virtual Color getDrawColor() const = 0;
        virtual void setFont(const std::string& fontPath, int fontSize) = 0;

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
        virtual void fillCircle(int centerX, int centerY, int radius, const Color& fillColor, const Border& border) = 0;

        virtual void drawArc(int centerX, int centerY, int radius, float startAngle, float endAngle) = 0;
        virtual void drawArc(int centerX, int centerY, int radius, float startAngle, float endAngle, const Color& color) = 0;
        virtual void drawArc(int centerX, int centerY, int radius, float startAngle, float endAngle, const Border& border) = 0;

        virtual void drawRoundRect(const Rect& rect, int radius) = 0;
        virtual void fillRoundRect(const Rect& rect, int radius) = 0;
        virtual void drawRoundRect(const Rect& rect, int radius, const Color& color) = 0;
        virtual void fillRoundRect(const Rect& rect, int radius, const Color& color) = 0;
        virtual void drawRoundRect(const Rect& rect, int radius, const Border& border) = 0;
        virtual void fillRoundRect(const Rect& rect, int radius, const Color& fillColor, const Border& border) = 0;

        virtual void drawPolygon(const std::vector<PointI>& points) = 0;
        virtual void fillPolygon(const std::vector<PointI>& points) = 0;
        virtual void drawPolygon(const std::vector<PointI>& points, const Color& color) = 0;
        virtual void fillPolygon(const std::vector<PointI>& points, const Color& color) = 0;
        virtual void drawPolygon(const std::vector<PointI>& points, const Border& border) = 0;
        virtual void fillPolygon(const std::vector<PointI>& points, const Color& fillColor, const Border& border) = 0;

        virtual void drawText(const std::string& text, int x, int y) = 0;
    };

}

#endif //DXVUI_IRENDERER_H
