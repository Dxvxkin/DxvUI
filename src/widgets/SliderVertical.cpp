#include "DxvUI/widgets/SliderVertical.h"

#include <algorithm>
#include <utility>

#include "DxvUI/DxvEvent.h"
#include "DxvUI/interfaces/IRenderer.h"
#include "DxvUI/layout/LayoutManager.h"
#include "DxvUI/style/Colors.h"
#include "DxvUI/style/Theme.h"

namespace DxvUI {

// --- Self-registration of default styles ---
namespace {
constexpr const char* kWidgetType = "SliderVertical";

struct SliderVerticalStyleRegistrar {
    SliderVerticalStyleRegistrar() {
        Theme::registerDefaultStyle(
            kWidgetType,
            {{WidgetState::Normal, {.cursor = CursorType::Hand, .padding = {{4, 4, 4, 4}}}},
             {WidgetState::Hovered, {}},
             {WidgetState::Pressed, {}},
             {WidgetState::Focused, {}},
             {WidgetState::Disabled, {.cursor = CursorType::Arrow}}});
    }
};
const SliderVerticalStyleRegistrar registrar;
}  // namespace

std::shared_ptr<SliderVertical> SliderVertical::create(std::string id, float min, float max,
                                                       float step) {
    return std::shared_ptr<SliderVertical>(new SliderVertical(std::move(id), min, max, step));
}

SliderVertical::SliderVertical(std::string id, float min, float max, float step)
    : SliderBase(std::move(id), min, max, step) {}

const char* SliderVertical::getNodeType() const { return kWidgetType; }

Size SliderVertical::onMeasure(const Size& availableSize) {
    return LayoutManager::addPadding({kWidgetWidth, availableSize.height},
                                     getComputedLayout().padding);
}

void SliderVertical::drawContent(IRenderer& renderer) {
    const Rect content = LayoutManager::contentRect(*this, getGlobalBounds());
    if (content.width <= 0 || content.height <= 0) {
        return;
    }

    const int centerX = content.x + content.width / 2;
    const Rect track = {centerX - static_cast<int>(kTrackWidth) / 2, content.y,
                        static_cast<int>(kTrackWidth), content.height};
    const int trackRadius = static_cast<int>(kTrackWidth) / 2;

    // Track background.
    renderer.fillRoundRect(track, trackRadius, Colors::LightGray);

    // Filled portion from the thumb center down to the track end: the value
    // grows upward, so the fill rises from the bottom.
    const int thumbCenter = content.y + valueToAxisPos(getValue(), content.height);
    if (thumbCenter < content.y + content.height) {
        const Rect fill = {track.x, thumbCenter, track.width,
                           content.y + content.height - thumbCenter};
        renderer.fillRoundRect(fill, trackRadius, Colors::CornflowerBlue);
    }

    // Thumb.
    const int radius = static_cast<int>(kThumbDiameter) / 2;
    renderer.fillCircle(centerX, thumbCenter, radius, Colors::CornflowerBlue,
                        {.color = Colors::RoyalBlue, .thickness = 1});
}

int SliderVertical::valueToAxisPos(float value, int trackLen) const {
    // Value grows bottom -> top, so the offset from the top is inverted.
    const float range = getMax() - getMin();
    const float ratio = (range != 0.0f) ? (value - getMin()) / range : 0.0f;
    return static_cast<int>((1.0f - std::clamp(ratio, 0.0f, 1.0f)) * trackLen);
}

float SliderVertical::axisPosToValue(float pos, int trackLen) const {
    if (trackLen <= 0) {
        return getMin();
    }
    const float ratio = std::clamp(pos / static_cast<float>(trackLen), 0.0f, 1.0f);
    return clampValue(getMin() + (1.0f - ratio) * (getMax() - getMin()));
}

int SliderVertical::axisStart(const Rect& rect) const { return rect.y; }
int SliderVertical::axisLength(const Rect& rect) const { return rect.height; }
int SliderVertical::axisFromMouse(int /*x*/, int y) const { return y; }
bool SliderVertical::isMainAxisKey(KeyCode key) const { return key == KeyCode::Up; }
bool SliderVertical::isCrossAxisKey(KeyCode key) const { return key == KeyCode::Down; }

}  // namespace DxvUI
