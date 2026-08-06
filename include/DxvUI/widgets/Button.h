#ifndef DXVUI_BUTTON_H
#define DXVUI_BUTTON_H

#include <memory>
#include <string>

#include "DxvUI/SceneNode.h"

namespace DxvUI {

class Label;

class Button : public SceneNode {
   public:
    static std::shared_ptr<Button> create(std::string id, std::string text = "");

    void setText(const std::string& text);
    const std::string getText() const;

    // --- Overrides ---
    const char* getNodeType() const override;
    void onAttach() override;
    void draw(IRenderer& renderer) override;
    std::shared_ptr<SceneNode> findNodeAt(int x, int y) override;
    // ---------------------

   protected:
    explicit Button(std::string id, std::string text);

    Size onMeasure(const Size& availableSize) override;
    void onArrange(const Rect& finalRect) override;

   private:
    std::shared_ptr<Label> label;
};

}  // namespace DxvUI

#endif  // DXVUI_BUTTON_H
