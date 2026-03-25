#ifndef DXVUI_LABEL_H
#define DXVUI_LABEL_H

#include "DxvUI/SceneNode.h"
#include <string>

namespace DxvUI {

    class Label : public SceneNode {
    public:
        static std::shared_ptr<Label> create(std::string id, std::string text = "");

        explicit Label(std::string id = "", std::string text = "");

        void setText(const std::string& text);
        const std::string& getText() const;

        // --- Overrides ---
        const char* getNodeType() const override;
        Size measure(const Size& availableSize) override;
        void draw(IRenderer& renderer) override;
        // ---------------------

    private:
        std::string text;
    };

}

#endif //DXVUI_LABEL_H
