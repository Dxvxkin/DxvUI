#ifndef DXVUI_SLIDERVERTICAL_H
#define DXVUI_SLIDERVERTICAL_H

#include <memory>
#include <string>

#include "DxvUI/widgets/SliderBase.h"

namespace DxvUI {

/**
 * @brief Vertical slider: a narrow track with a draggable thumb for picking a
 * float value in [min, max].
 *
 * The value grows upward (the thumb is at the bottom at min). Dragging the
 * thumb or clicking the track sets the value by position; the arrow keys
 * (Up/Down) and the mouse wheel step the value. See SliderBase for the shared
 * value/step/interaction semantics.
 */
class SliderVertical : public SliderBase {
   public:
    static std::shared_ptr<SliderVertical> create(std::string id, float min = 0.0f,
                                                  float max = 1.0f, float step = 0.0f);

    const char* getNodeType() const override;

   protected:
    SliderVertical(std::string id, float min, float max, float step);

    Size onMeasure(const Size& availableSize) override;
    void drawContent(IRenderer& renderer) override;

    // --- Axis (vertical: value grows bottom -> top) ---
    int valueToAxisPos(float value, int trackLen) const override;
    float axisPosToValue(float pos, int trackLen) const override;
    int axisStart(const Rect& rect) const override;
    int axisLength(const Rect& rect) const override;
    int axisFromMouse(int x, int y) const override;
    bool isMainAxisKey(KeyCode key) const override;
    bool isCrossAxisKey(KeyCode key) const override;
    int grabRadius() const override { return static_cast<int>(kThumbDiameter) / 2; }

   private:
    static constexpr float kWidgetWidth = 24.0f;
    static constexpr float kTrackWidth = 4.0f;
    static constexpr float kThumbDiameter = 14.0f;
};

}  // namespace DxvUI

#endif  // DXVUI_SLIDERVERTICAL_H
