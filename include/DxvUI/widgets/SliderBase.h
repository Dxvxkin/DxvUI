#ifndef DXVUI_SLIDERBASE_H
#define DXVUI_SLIDERBASE_H

#include <memory>
#include <string>

#include "DxvUI/SceneNode.h"

namespace DxvUI {

/**
 * @brief Shared value/step/interaction logic for the slider widgets.
 *
 * A slider lets the user pick a float value in [min, max] by dragging a thumb
 * along a track (mouse), stepping with the keyboard (arrow keys) or scrolling
 * the mouse wheel. The current value lives in the widget's UIBinding, so
 * changing it dispatches a Change event through the scene tree.
 *
 * The widget is orientation-agnostic: the concrete orientation (horizontal vs
 * vertical) is supplied by the subclass through a small set of axis hooks, and
 * by the subclass's own onMeasure()/drawContent()/getNodeType().
 *
 * Step semantics: step == 0 means a continuous (free) value, and the wheel/key
 * deltas are a fraction of the range; step > 0 snaps values to multiples of
 * step, and the wheel/key move by exactly one step. Holding Shift refines the
 * keyboard delta by 10x.
 */
class SliderBase : public SceneNode {
   public:
    void setValue(float value);
    float getValue() const;
    void setRange(float min, float max);
    float getMin() const;
    float getMax() const;
    void setStep(float step);
    float getStep() const;

   protected:
    SliderBase(std::string id, float min, float max, float step);

    void onEvent(DxvEvent& event) override;
    void onDetach() override;

    /**
     * @brief Clamps a value to [min, max], optionally snapping to step.
     *
     * The base comparison ignores sub-step differences: a value that only
     * differs below the step granularity snaps to the same multiple.
     */
    float clampValue(float value) const;

    /**
     * @brief The keyboard delta for one step, scaled by a modifier multiplier.
     *
     * With a step set this is exactly step_ * multiplier; without one it is a
     * fraction of the range (5% per step), so a range slider is usable without
     * configuring a step.
     */
    float computeDelta(float multiplier) const;

    // --- Axis hooks (implemented per orientation) ---

    /// Maps a value to a pixel offset along the track's main axis (0..trackLen).
    virtual int valueToAxisPos(float value, int trackLen) const = 0;
    /// Maps a pixel offset along the main axis back to a (clamped, snapped) value.
    virtual float axisPosToValue(float pos, int trackLen) const = 0;
    /// The track's origin along the main axis (content x or y).
    virtual int axisStart(const Rect& rect) const = 0;
    /// The track's length along the main axis (content width or height).
    virtual int axisLength(const Rect& rect) const = 0;
    /// Projects a mouse coordinate onto the main axis.
    virtual int axisFromMouse(int x, int y) const = 0;
    /// The arrow key that increases the value along the main axis.
    virtual bool isMainAxisKey(KeyCode key) const = 0;
    /// The arrow key that decreases the value along the main axis.
    virtual bool isCrossAxisKey(KeyCode key) const = 0;

   private:
    float min_ = 0.0f;
    float max_ = 1.0f;
    // 0 means a continuous value; otherwise values snap to multiples of step_.
    float step_ = 0.0f;
    bool isDragging_ = false;
    // Cursor-to-thumb-center offset captured on grab, so grabbing the thumb
    // does not make it jump under the pointer.
    float dragOffset_ = 0.0f;
};

}  // namespace DxvUI

#endif  // DXVUI_SLIDERBASE_H
