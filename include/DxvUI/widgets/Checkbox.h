#ifndef DXVUI_CHECKBOX_H
#define DXVUI_CHECKBOX_H

#include <memory>
#include <string>

#include "DxvUI/SceneNode.h"

namespace DxvUI {

class Label;

/**
 * @brief Checkbox widget with an optional text label.
 *
 * The checked state lives in the widget's UIBinding (a bool), so toggling it
 * automatically dispatches a Change event through the scene tree (see
 * SceneNode::onBindingChange). The text is rendered by an internal Label child
 * that inherits the surrounding font properties.
 *
 * A mouse click toggles the state as the widget's default action and stops the
 * Click from bubbling further. The Space key toggles the state while the widget
 * owns focus.
 */
class Checkbox : public SceneNode {
   public:
    static std::shared_ptr<Checkbox> create(std::string id, std::string text = "");

    void setChecked(bool checked);
    bool isChecked() const;

    void setText(std::string text);
    std::string getText() const;

    // --- Overrides ---
    const char* getNodeType() const override;
    void onAttach() override;
    std::shared_ptr<SceneNode> findNodeAt(int x, int y) override;
    // ---------------------

   protected:
    explicit Checkbox(std::string id, std::string text);

    Size onMeasure(const Size& availableSize) override;
    void onArrange(const Rect& finalRect) override;
    void drawContent(IRenderer& renderer) override;
    void onEvent(DxvEvent& event) override;

   private:
    // Size of the square box and the gap between it and the label.
    static constexpr float kBoxSize = 16.0f;
    static constexpr float kGap = 6.0f;

    void toggle();

    std::string labelText;
    std::shared_ptr<Label> label;
};

}  // namespace DxvUI

#endif  // DXVUI_CHECKBOX_H
