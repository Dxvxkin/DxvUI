#include "DxvUI/widgets/Plot.h"

#include <cmath>
#include <limits>
#include <utility>

#include "DxvUI/interfaces/IRenderer.h"
#include "DxvUI/layout/LayoutManager.h"
#include "DxvUI/style/Theme.h"

namespace DxvUI {

// --- Self-registration of default styles ---
namespace {
constexpr const char* kWidgetType = "Plot";

constexpr std::array<Color, 6> kSeriesPalette = {
    Colors::CornflowerBlue, Colors::Orange,   Colors::Green, Colors::Red,
    Colors::Purple,         Colors::DarkGray,
};

struct PlotStyleRegistrar {
    PlotStyleRegistrar() {
        Theme::registerDefaultStyle(kWidgetType,
                                    {{WidgetState::Normal, {.backgroundColor = Colors::White}}});
    }
};

const PlotStyleRegistrar registrar;
}  // namespace

std::shared_ptr<Plot> Plot::create(std::string id) {
    return std::shared_ptr<Plot>(new Plot(std::move(id)));
}

Plot::Plot(std::string id) : SceneNode(std::move(id)) {}

const char* Plot::getNodeType() const noexcept { return kWidgetType; }

size_t Plot::addSeries(std::string name) {
    const size_t index = series_.size();
    Series series;
    series.name = name.empty() ? std::to_string(index) : std::move(name);
    series.color = kSeriesPalette[index % kSeriesPalette.size()];
    series_.push_back(std::move(series));
    ++dataVersion_;
    if (autoScale_) {
        autoScale();
    }
    return index;
}

const std::string& Plot::getSeriesName(size_t series) const {
    static const std::string kEmpty;
    if (series < series_.size()) {
        return series_[series].name;
    }
    return kEmpty;
}

void Plot::setSeriesName(size_t series, std::string name) {
    if (series < series_.size()) {
        series_[series].name = std::move(name);
    }
}

void Plot::setData(size_t series, std::vector<Point<float>> data) {
    if (series >= series_.size()) {
        return;
    }
    series_[series].points = std::move(data);
    ++dataVersion_;
    if (autoScale_) {
        autoScale();
    }
}

void Plot::applyPoint(size_t series, float x, float y) {
    if (series >= series_.size()) {
        return;
    }
    series_[series].points.push_back({x, y});
    ++dataVersion_;
    if (autoScale_) {
        autoScale();
    }
}

void Plot::clearData() {
    for (auto& series : series_) {
        series.points.clear();
    }
    ++dataVersion_;
    if (autoScale_) {
        autoScale();
    }
}

void Plot::removeSeries(size_t series) {
    if (series >= series_.size()) {
        return;
    }
    series_.erase(series_.begin() + static_cast<ptrdiff_t>(series));
    ++dataVersion_;
    if (autoScale_) {
        autoScale();
    }
}

void Plot::removeAllSeries() {
    series_.clear();
    ++dataVersion_;
    if (autoScale_) {
        autoScale();
    }
}

size_t Plot::getSeriesCount() const { return series_.size(); }

const std::vector<Point<float>>* Plot::getSeriesPoints(size_t series) const {
    if (series >= series_.size()) {
        return nullptr;
    }
    return &series_[series].points;
}

void Plot::setSeriesColor(size_t series, Color color) {
    if (series < series_.size()) {
        series_[series].color = color;
    }
}

Color Plot::getSeriesColor(size_t series) const {
    if (series < series_.size()) {
        return series_[series].color;
    }
    return Colors::Transparent;
}

void Plot::setWorldBounds(float xMin, float yMin, float xMax, float yMax) {
    // Degenerate or non-finite bounds would produce nonsensical projections
    // (or a division by zero in toPixel*), so they are ignored wholesale.
    if (!std::isfinite(xMin) || !std::isfinite(yMin) || !std::isfinite(xMax) ||
        !std::isfinite(yMax) || xMin >= xMax || yMin >= yMax) {
        return;
    }
    xMin_ = xMin;
    yMin_ = yMin;
    xMax_ = xMax;
    yMax_ = yMax;
    autoScale_ = false;
}

void Plot::clearWorldBounds() {
    autoScale_ = true;
    autoScale();
}

bool Plot::isAutoScaleEnabled() const { return autoScale_; }

float Plot::getXMin() const { return xMin_; }
float Plot::getXMax() const { return xMax_; }
float Plot::getYMin() const { return yMin_; }
float Plot::getYMax() const { return yMax_; }

void Plot::setAutoScalePadding(float fraction) {
    if (fraction >= 0.0f && std::isfinite(fraction) && fraction != autoScalePadding_) {
        autoScalePadding_ = fraction;
        // The padding feeds the next autoScale(): recompute eagerly so the
        // getters are fresh and the version-synced draw path rescales too.
        if (autoScale_) {
            autoScale();
        }
    }
}

float Plot::getAutoScalePadding() const { return autoScalePadding_; }

void Plot::setShowGrid(bool show) { showGrid_ = show; }
bool Plot::isGridVisible() const { return showGrid_; }

void Plot::setGridColor(Color color) { gridColor_ = color; }
Color Plot::getGridColor() const { return gridColor_; }

void Plot::setAxisColor(Color color) { axisColor_ = color; }
Color Plot::getAxisColor() const { return axisColor_; }

Size Plot::onMeasure(const Size& availableSize) {
    // The plot is stretchable by default: report the preferred default size and
    // let a container override it (label must only compensate for the
    // padding/border that contentRect() subtracts, see Label::onMeasure).
    return LayoutManager::addPadding({300.0f, 200.0f}, LayoutManager::contentInsets(*this));
}

void Plot::autoScale() {
    float xMin = std::numeric_limits<float>::infinity();
    float xMax = -std::numeric_limits<float>::infinity();
    float yMin = std::numeric_limits<float>::infinity();
    float yMax = -std::numeric_limits<float>::infinity();
    bool found = false;

    for (const auto& series : series_) {
        for (const auto& p : series.points) {
            if (!std::isfinite(p.x) || !std::isfinite(p.y)) {
                continue;
            }
            xMin = std::min(xMin, p.x);
            xMax = std::max(xMax, p.x);
            yMin = std::min(yMin, p.y);
            yMax = std::max(yMax, p.y);
            found = true;
        }
    }

    if (!found) {
        // No data: keep the unit defaults so the view never collapses and a
        // live append stays visible.
        xMin_ = 0.0f;
        xMax_ = 1.0f;
        yMin_ = 0.0f;
        yMax_ = 1.0f;
    } else {
        // autoScalePadding_ per axis; a zero-range axis (all points share the
        // same coordinate) gets a unit span so it cannot divide by zero later.
        const float xPad = (xMax - xMin) * autoScalePadding_;
        const float yPad = (yMax - yMin) * autoScalePadding_;
        xMin_ = xMin - (xPad > 0.0f ? xPad : 0.5f);
        xMax_ = xMax + (xPad > 0.0f ? xPad : 0.5f);
        yMin_ = yMin - (yPad > 0.0f ? yPad : 0.5f);
        yMax_ = yMax + (yPad > 0.0f ? yPad : 0.5f);
    }
    // The bounds are now in sync with the data version; drawing skips the scan
    // until the next mutation.
    lastScaledVersion_ = dataVersion_;
}

int Plot::toPixelX(float x, const Rect& content) const {
    const float range = xMax_ - xMin_;
    const float t = (range > 0.0f) ? (x - xMin_) / range : 0.5f;
    return content.x + static_cast<int>(std::lround(t * content.width));
}

int Plot::toPixelY(float y, const Rect& content) const {
    const float range = yMax_ - yMin_;
    const float t = (range > 0.0f) ? (y - yMin_) / range : 0.5f;
    return content.y + content.height - static_cast<int>(std::lround(t * content.height));
}

void Plot::drawContent(IRenderer& renderer) {
    const Rect content = LayoutManager::contentRect(*this, getGlobalBounds());
    if (content.width <= 0 || content.height <= 0) {
        return;
    }
    if (autoScale_ && dataVersion_ != lastScaledVersion_) {
        autoScale();
    }

    // Points outside the world bounds are clipped by the renderer, so the whole
    // scene can be drawn without per-segment culling.
    renderer.pushClipRect(content);

    if (showGrid_) {
        constexpr int kDivisions = 5;
        for (int i = 0; i <= kDivisions; ++i) {
            const int vx = content.x + content.width * i / kDivisions;
            renderer.drawLine(vx, content.y, vx, content.y + content.height - 1, gridColor_);
            const int hy = content.y + content.height * i / kDivisions;
            renderer.drawLine(content.x, hy, content.x + content.width - 1, hy, gridColor_);
        }
    }

    // Zero-threshold axes, drawn only when inside the current world bounds.
    if (xMin_ < 0.0f && xMax_ > 0.0f) {
        const int x0 = toPixelX(0.0f, content);
        renderer.drawLine(x0, content.y, x0, content.y + content.height - 1, axisColor_);
    }
    if (yMin_ < 0.0f && yMax_ > 0.0f) {
        const int y0 = toPixelY(0.0f, content);
        renderer.drawLine(content.x, y0, content.x + content.width - 1, y0, axisColor_);
    }

    for (const auto& series : series_) {
        if (series.points.size() < 2) {
            continue;
        }
        int prevX = 0;
        int prevY = 0;
        bool hasPrev = false;
        for (const auto& p : series.points) {
            if (!std::isfinite(p.x) || !std::isfinite(p.y)) {
                hasPrev = false;
                continue;
            }
            const int px = toPixelX(p.x, content);
            const int py = toPixelY(p.y, content);
            if (hasPrev) {
                renderer.drawLine(prevX, prevY, px, py, series.color);
            }
            prevX = px;
            prevY = py;
            hasPrev = true;
        }
    }

    renderer.popClipRect();
}

}  // namespace DxvUI