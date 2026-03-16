#ifndef DXVUI_SDLRENDERER_H
#define DXVUI_SDLRENDERER_H

#include <vector>
#include <DxvUI/interfaces/IRenderer.h>
#include <string>

struct SDL_Window;
struct SDL_Renderer;

namespace DxvUI {

    class SDLRenderer : public IRenderer {
    public:
        // Constructor for when DxvUI creates and owns the window/renderer
        SDLRenderer(const char* title, int width, int height);
        // Constructor for when DxvUI uses an existing, external renderer
        explicit SDLRenderer(SDL_Renderer* externalRenderer);

        ~SDLRenderer() override;

        // --- IRenderer implementation ---
        void setDrawColor(const Color& color) override;
        Color getDrawColor() const override;

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
        void fillCircle(int centerX, int centerY, int radius, const Color& fillColor, const Border& border) override;

        void drawArc(int centerX, int centerY, int radius, float startAngle, float endAngle) override;
        void drawArc(int centerX, int centerY, int radius, float startAngle, float endAngle, const Color& color) override;
        void drawArc(int centerX, int centerY, int radius, float startAngle, float endAngle, const Border& border) override;

        void drawRoundRect(const Rect& rect, int radius) override;
        void fillRoundRect(const Rect& rect, int radius) override;
        void drawRoundRect(const Rect& rect, int radius, const Color& color) override;
        void fillRoundRect(const Rect& rect, int radius, const Color& color) override;
        void drawRoundRect(const Rect& rect, int radius, const Border& border) override;
        void fillRoundRect(const Rect& rect, int radius, const Color& fillColor, const Border& border) override;

        void drawPolygon(const std::vector<PointI>& points) override;
        void fillPolygon(const std::vector<PointI>& points) override;
        void drawPolygon(const std::vector<PointI>& points, const Color& color) override;
        void fillPolygon(const std::vector<PointI>& points, const Color& color) override;
        void drawPolygon(const std::vector<PointI>& points, const Border& border) override;
        void fillPolygon(const std::vector<PointI>& points, const Color& fillColor, const Border& border) override;

        void drawText(const std::string& text, int x, int y) override;
        void drawImage(const std::string& imagePath, int x, int y) override;

        // --- SDL-specific methods ---
        void clear(const Color& color);
        void present();
        SDL_Renderer* getSDLRenderer();

    private:
        SDL_Window* window = nullptr;
        SDL_Renderer* renderer = nullptr;
        bool ownsResources = false; // Does this instance own the window and renderer?
        Color currentColor;
    };

}

#endif //DXVUI_SDLRENDERER_H
