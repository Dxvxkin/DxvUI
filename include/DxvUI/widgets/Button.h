#ifndef DXVUI_BUTTON_H
#define DXVUI_BUTTON_H

#include "DxvUI/SceneNode.h"
#include <string>
#include <memory>

namespace DxvUI {

    class Label;

    class Button : public SceneNode {
    public:
        static std::shared_ptr<Button> create(std::string id, std::string text = "");

        void setText(const std::string& text);
        const std::string& getText() const;

        Label* getLabel() const;

        // --- Overrides ---
        void onAttach() override;
        Size measure(const Size& availableSize) override;
        void arrange(const Rect& finalRect) override;
        void draw(IRenderer& renderer) override;
        std::shared_ptr<SceneNode> findNodeAt(int x, int y) override;
        // ---------------------

    protected:
        explicit Button(std::string id);

    private:
        std::shared_ptr<Label> label;
        std::string initialText;
    };

}

#endif //DXVUI_BUTTON_H
