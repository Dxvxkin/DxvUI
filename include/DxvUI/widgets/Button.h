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

    std::shared_ptr<Label> getLabel() const;

    // --- Overrides ---
    const char* getNodeType() const override;
    void onAttach() override;
    Size measureOverride(const Size& availableSize) override;
    void arrange(const Rect& finalRect) override;
    void draw(IRenderer& renderer) override;
    std::shared_ptr<SceneNode> findNodeAt(int x, int y) override;
    // ---------------------

   protected:
    explicit Button(std::string id, std::string text);

   private:
    std::shared_ptr<Label> label;
};

}  // namespace DxvUI

#endif  // DXVUI_BUTTON_H
