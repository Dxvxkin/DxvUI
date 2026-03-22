#ifndef DXVUI_SDLRENDERER_H
#define DXVUI_SDLRENDERER_H

#include <vector>
#include <map>
#include <string>
#include <mutex>
#include <SDL_ttf.h>
#include <SDL.h> // For SDL_Cursor
#include <DxvUI/interfaces/IRenderer.h>

struct SDL_Window;
struct SDL_Renderer;

namespace DxvUI {

    class SDLRenderer : public IRenderer {
    public:
        SDLRenderer(const char* title, int width, int height);
        explicit SDLRenderer(SDL_Renderer* externalRenderer);
        ~SDLRenderer() override;

        // --- IRenderer implementation ---
        void clear(const Color& color) override;
        void present() override;
        Size getViewportSize() const override;

        // Cursor
        void setCursor(CursorType type) override;
        CursorType getCursor() const override;

        // Text Rendering
        std::shared_ptr<ITexture> createTextTexture(const std::string& text) override;
        Rect measureText(const std::string& text, const std::string& fontPath, int fontSize) override;
        void drawTexture(std::shared_ptr<ITexture>& texture, const Rect& dstRect) override;

        // State Management
        void setDrawColor(const Color& color) override;
        Color getDrawColor() const override;
        void setFont(const std::string& fontPath, int fontSize) override;

        // Primitives
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

    private:
        TTF_Font* getFont(const std::string& fontPath, int fontSize);
        SDL_Cursor* getSystemCursor(CursorType type);

        SDL_Window* window = nullptr;
        SDL_Renderer* renderer = nullptr;
        bool ownsResources = false;

        Color currentColor;
        std::string currentFontPath;
        int currentFontSize = 0;
        CursorType currentCursorType = CursorType::Arrow;

        std::map<std::string, TTF_Font*> fontCache;
        std::map<CursorType, SDL_Cursor*> cursorCache;

        static int ttf_ref_count;
        static std::mutex ttf_mutex;
        static void initTTF();
        static void quitTTF();
    };

}

#endif //DXVUI_SDLRENDERER_H
