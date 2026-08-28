#ifndef DXVUI_SLIDERHORIZONTAL_H
#define DXVUI_SLIDERHORIZONTAL_H

#include <memory>
#include <string>

#include "DxvUI/widgets/SliderBase.h"

namespace DxvUI {

/**
 * @brief Horizontal slider: a track with a draggable thumb for picking a float
 * value in [min, max].
 *
 * Dragging the thumb or clicking the track sets the value by position; the
 * arrow keys (Left/Right) and the mouse wheel step the value. See SliderBase
 * for the shared value/step/interaction semantics.
 */
class SliderHorizontal : public SliderBase {
   public:
    static std::shared_ptr<SliderHorizontal> create(std::string id, float min = 0.0f,
                                                    float max = 1.0f, float step = 0.0f);

    const char* getNodeType() const override;

   protected:
    SliderHorizontal(std::string id, float min, float max, float step);

    Size onMeasure(const Size& availableSize) override;
    void drawContent(IRenderer& renderer) override;

    // --- Axis (horizontal: value grows left -> right) ---
    int valueToAxisPos(float value, int trackLen) const override;
    float axisPosToValue(float pos, int trackLen) const override;
    int axisStart(const Rect& rect) const override;
    int axisLength(const Rect& rect) const override;
    int axisFromMouse(int x, int y) const override;
    bool isMainAxisKey(KeyCode key) const override;
    bool isCrossAxisKey(KeyCode key) const override;

   private:
    static constexpr float kWidgetHeight = 24.0f;
    static constexpr float kTrackHeight = 4.0f;
    static constexpr float kThumbDiameter = 14.0f;
};

}  // namespace DxvUI

#endif  // DXVUI_SLIDERHORIZONTAL_H
