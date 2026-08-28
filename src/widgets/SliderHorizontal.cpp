#include "DxvUI/widgets/SliderHorizontal.h"

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
constexpr const char* kWidgetType = "SliderHorizontal";

struct SliderHorizontalStyleRegistrar {
    SliderHorizontalStyleRegistrar() {
        Theme::registerDefaultStyle(
            kWidgetType,
            {{WidgetState::Normal, {.cursor = CursorType::Hand, .padding = {{4, 4, 4, 4}}}},
             {WidgetState::Hovered, {}},
             {WidgetState::Pressed, {}},
             {WidgetState::Focused, {}},
             {WidgetState::Disabled, {.cursor = CursorType::Arrow}}});
    }
};
const SliderHorizontalStyleRegistrar registrar;
}  // namespace

std::shared_ptr<SliderHorizontal> SliderHorizontal::create(std::string id, float min, float max,
                                                           float step) {
    return std::shared_ptr<SliderHorizontal>(new SliderHorizontal(std::move(id), min, max, step));
}

SliderHorizontal::SliderHorizontal(std::string id, float min, float max, float step)
    : SliderBase(std::move(id), min, max, step) {}

const char* SliderHorizontal::getNodeType() const { return kWidgetType; }

Size SliderHorizontal::onMeasure(const Size& availableSize) {
    return LayoutManager::addPadding({availableSize.width, kWidgetHeight},
                                     getComputedLayout().padding);
}

void SliderHorizontal::drawContent(IRenderer& renderer) {
    const Rect content = LayoutManager::contentRect(*this, getGlobalBounds());
    if (content.width <= 0 || content.height <= 0) {
        return;
    }

    const int centerY = content.y + content.height / 2;
    const Rect track = {content.x, centerY - static_cast<int>(kTrackHeight) / 2, content.width,
                        static_cast<int>(kTrackHeight)};
    const int trackRadius = static_cast<int>(kTrackHeight) / 2;

    // Track background.
    renderer.fillRoundRect(track, trackRadius, Colors::LightGray);

    // Filled portion from the track start to the thumb center.
    const int thumbCenter = content.x + valueToAxisPos(getValue(), content.width);
    if (thumbCenter > content.x) {
        const Rect fill = {content.x, track.y, thumbCenter - content.x, track.height};
        renderer.fillRoundRect(fill, trackRadius, Colors::CornflowerBlue);
    }

    // Thumb.
    const int radius = static_cast<int>(kThumbDiameter) / 2;
    renderer.fillCircle(thumbCenter, centerY, radius, Colors::CornflowerBlue,
                        {.color = Colors::RoyalBlue, .thickness = 1});
}

int SliderHorizontal::valueToAxisPos(float value, int trackLen) const {
    const float range = getMax() - getMin();
    const float ratio = (range != 0.0f) ? (value - getMin()) / range : 0.0f;
    return static_cast<int>(std::clamp(ratio, 0.0f, 1.0f) * trackLen);
}

float SliderHorizontal::axisPosToValue(float pos, int trackLen) const {
    if (trackLen <= 0) {
        return getMin();
    }
    const float ratio = std::clamp(pos / static_cast<float>(trackLen), 0.0f, 1.0f);
    return clampValue(getMin() + ratio * (getMax() - getMin()));
}

int SliderHorizontal::axisStart(const Rect& rect) const { return rect.x; }
int SliderHorizontal::axisLength(const Rect& rect) const { return rect.width; }
int SliderHorizontal::axisFromMouse(int x, int /*y*/) const { return x; }
bool SliderHorizontal::isMainAxisKey(KeyCode key) const { return key == KeyCode::Right; }
bool SliderHorizontal::isCrossAxisKey(KeyCode key) const { return key == KeyCode::Left; }

}  // namespace DxvUI
