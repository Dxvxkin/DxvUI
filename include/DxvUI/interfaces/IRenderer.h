#ifndef DXVUI_IRENDERER_H
#define DXVUI_IRENDERER_H

#include <string>

namespace DxvUI {

    class IRenderer {
    public:
        virtual ~IRenderer() = default;
        virtual void drawRect(int x, int y, int width, int height) = 0;
        virtual void drawText(const std::string& text, int x, int y) = 0;
        virtual void drawImage(const std::string& imagePath, int x, int y) = 0;
    };

}

#endif //DXVUI_IRENDERER_H
