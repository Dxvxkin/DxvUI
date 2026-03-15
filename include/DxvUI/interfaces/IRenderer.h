#ifndef DXVUI_IRENDERER_H
#define DXVUI_IRENDERER_H

#include <string>
#include "../core.h"
#include "../Color.h"

namespace DxvUI {

    class IRenderer {
    public:
        virtual ~IRenderer() = default;

        virtual void setDrawColor(const Color& color) = 0;
        virtual Color getDrawColor() const = 0;

        virtual void drawRect(const Rect& rect) = 0;
        virtual void fillRect(const Rect& rect) = 0;
        virtual void drawRect(const Rect& rect, const Color& color) = 0;
        virtual void fillRect(const Rect& rect, const Color& color) = 0;
        virtual void drawRect(const Rect& rect, const Border& border) = 0;
        virtual void fillRect(const Rect& rect, const Color& fillColor, const Border& border) = 0;

        // --- New Primitives ---

        // Lines
        virtual void drawLine(int x1, int y1, int x2, int y2) = 0;
        virtual void drawLine(int x1, int y1, int x2, int y2, const Color& color) = 0;
        // virtual void drawLine(int x1, int y1, int x2, int y2, const Border& border) = 0; // SDL_gfx doesn't have a thick line

        // Circles
        virtual void drawCircle(int centerX, int centerY, int radius) = 0;
        virtual void fillCircle(int centerX, int centerY, int radius) = 0;
        virtual void drawCircle(int centerX, int centerY, int radius, const Color& color) = 0;
        virtual void fillCircle(int centerX, int centerY, int radius, const Color& color) = 0;
        virtual void drawCircle(int centerX, int centerY, int radius, const Border& border) = 0;
        virtual void fillCircle(int centerX, int centerY, int radius, const Color& fillColor, const Border& border) = 0;

        // Arcs
        virtual void drawArc(int centerX, int centerY, int radius, float startAngle, float endAngle) = 0;
        virtual void drawArc(int centerX, int centerY, int radius, float startAngle, float endAngle, const Color& color) = 0;
        virtual void drawArc(int centerX, int centerY, int radius, float startAngle, float endAngle, const Border& border) = 0;

        // Rounded Rectangles
        virtual void drawRoundRect(const Rect& rect, int radius) = 0;
        virtual void fillRoundRect(const Rect& rect, int radius) = 0;
        virtual void drawRoundRect(const Rect& rect, int radius, const Color& color) = 0;
        virtual void fillRoundRect(const Rect& rect, int radius, const Color& color) = 0;
        virtual void drawRoundRect(const Rect& rect, int radius, const Border& border) = 0;
        virtual void fillRoundRect(const Rect& rect, int radius, const Color& fillColor, const Border& border) = 0;

        // Polygons
        virtual void drawPolygon(const std::vector<PointI>& points) = 0;
        virtual void fillPolygon(const std::vector<PointI>& points) = 0;
        virtual void drawPolygon(const std::vector<PointI>& points, const Color& color) = 0;
        virtual void fillPolygon(const std::vector<PointI>& points, const Color& color) = 0;
        virtual void drawPolygon(const std::vector<PointI>& points, const Border& border) = 0;
        virtual void fillPolygon(const std::vector<PointI>& points, const Color& fillColor, const Border& border) = 0;

        // --- End of New Primitives ---


        // Keep these for future use, but they are not the focus now
        virtual void drawText(const std::string& text, int x, int y) = 0;
        virtual void drawImage(const std::string& imagePath, int x, int y) = 0;
    };

}

#endif //DXVUI_IRENDERER_H
