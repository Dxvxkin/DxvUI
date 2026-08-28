#include "DxvUI/widgets/SliderBase.h"

#include <algorithm>
#include <cmath>
#include <utility>

#include "DxvUI/DxvEvent.h"
#include "DxvUI/UIBinding.h"
#include "DxvUI/layout/LayoutManager.h"

namespace DxvUI {

SliderBase::SliderBase(std::string id, float min, float max, float step)
    : SceneNode(std::move(id)), min_(min), max_(max), step_(step) {
    bind(UIBinding::create(min_));
    // The slider is an atomic interaction target: it draws its own track and
    // thumb and must be clicked as a single entity.
    setHitTestable(true);
}

void SliderBase::setValue(float value) {
    // binding_->set() no-ops when the value did not change and dispatches a
    // Change event through the scene tree on an actual change. The value does
    // not affect the measured size, so no layout invalidation is needed.
    getBinding()->set(clampValue(value));
}

float SliderBase::getValue() const { return getBinding()->getFloatOr(min_); }

void SliderBase::setRange(float min, float max) {
    min_ = min;
    max_ = max;
    // The stored value may have fallen outside the new range; re-clamp it.
    setValue(getValue());
}

float SliderBase::getMin() const { return min_; }
float SliderBase::getMax() const { return max_; }

void SliderBase::setStep(float step) { step_ = step; }
float SliderBase::getStep() const { return step_; }

float SliderBase::clampValue(float value) const {
    if (max_ == min_) {
        return min_;
    }
    if (step_ > 0.0f) {
        value = std::round(value / step_) * step_;
    }
    return std::clamp(value, min_, max_);
}

float SliderBase::computeDelta(float multiplier) const {
    if (step_ > 0.0f) {
        return step_ * multiplier;
    }
    return (max_ - min_) * 0.05f * multiplier;
}

void SliderBase::onEvent(DxvEvent& event) {
    if (!isEnabled()) {
        return;
    }

    // The track rectangle is derived from layout/computed style, which is not
    // yet resolved while the node is being attached (e.g. during the Attach
    // event dispatched by addChild). Only touch it for the events that actually
    // need it, so unrelated events never read an unpopulated style cache.
    switch (event.type) {
        case EventType::MouseDown: {
            const Rect track = LayoutManager::contentRect(*this, getGlobalBounds());
            const int trackLen = axisLength(track);
            if (trackLen <= 0) break;
            const int pointerPos = axisFromMouse(event.mouse.x, event.mouse.y);
            // Capture the cursor's offset from the thumb center so that grabbing
            // the thumb does not make it jump under the pointer.
            const int thumbCenter = axisStart(track) + valueToAxisPos(getValue(), trackLen);
            dragOffset_ = static_cast<float>(pointerPos - thumbCenter);
            isDragging_ = true;
            setValue(axisPosToValue(pointerPos - dragOffset_, trackLen));
            event.stopPropagation();
            break;
        }
        case EventType::Drag: {
            const Rect track = LayoutManager::contentRect(*this, getGlobalBounds());
            const int trackLen = axisLength(track);
            if (isDragging_ && trackLen > 0) {
                const int pointerPos = axisFromMouse(event.mouse.x, event.mouse.y);
                setValue(axisPosToValue(pointerPos - dragOffset_, trackLen));
                event.stopPropagation();
            }
            break;
        }
        case EventType::MouseWheel: {
            // Scrolling up (dy > 0) increases the value in both orientations.
            const float step = (step_ > 0.0f) ? step_ : (max_ - min_) * 0.1f;
            setValue(getValue() + step * (event.mouse.dy > 0 ? 1.0f : -1.0f));
            event.stopPropagation();
            break;
        }
        case EventType::KeyDown: {
            const float multiplier = (event.key.mod & KeyModifier::Shift) ? 0.1f : 1.0f;
            const float delta = computeDelta(multiplier);
            if (isMainAxisKey(event.key.sym)) {
                setValue(getValue() + delta);
                event.stopPropagation();
            } else if (isCrossAxisKey(event.key.sym)) {
                setValue(getValue() - delta);
                event.stopPropagation();
            }
            break;
        }
        default:
            break;
    }
}

void SliderBase::onDetach() {
    // A node can be detached/removed mid-drag; never leave the widget stuck in
    // the dragging state.
    isDragging_ = false;
    SceneNode::onDetach();
}

}  // namespace DxvUI
