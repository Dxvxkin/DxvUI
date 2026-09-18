#include "DxvUI/widgets/Plot.h"

#include <algorithm>
#include <cmath>
#include <format>
#include <limits>
#include <string>
#include <utility>

#include "DxvUI/interfaces/ICanvas.h"
#include "DxvUI/interfaces/ITextEngine.h"
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

// How far the grid lines are spread apart before their labels collide, in
// pixels: x labels are wide (~40px at the default font), y labels are tall.
constexpr int kXTickSpacingPx = 64;
constexpr int kYTickSpacingPx = 28;
// Minimum left/bottom padding (in px) needed to fit a label gutter. Values are
// inclusive thresholds: `insets.left >= 8` enables the y-axis labels.
constexpr float kLabelGutterPx = 8.0f;
// Area fill opacity for the region under a curve.
constexpr uint8_t kAreaAlpha = 70;
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

void Plot::setShowAxisLabels(bool show) { showAxisLabels_ = show; }
bool Plot::isAxisLabelsVisible() const { return showAxisLabels_; }

void Plot::setAreaEnabled(bool enabled) { areaEnabled_ = enabled; }
bool Plot::isAreaEnabled() const { return areaEnabled_; }

Size Plot::onMeasure(const Size& /*availableSize*/) {
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

Plot::TickInfo Plot::computeTicks(float min, float max, int targetTicks) {
    TickInfo info;
    const float range = max - min;
    if (!(range > 0.0f) || targetTicks <= 0) {
        return info;
    }
    // "Nice" step: a mantissa of 1/2/5 times a power of ten, so the tick
    // positions are round numbers readable in the labels.
    const float raw = range / static_cast<float>(targetTicks);
    const float magnitude = std::pow(10.0f, std::floor(std::log10(raw)));
    const float norm = raw / magnitude;
    float nice = 1.0f;
    if (norm >= 7.0f) {
        nice = 10.0f;
    } else if (norm >= 3.0f) {
        nice = 5.0f;
    } else if (norm >= 1.5f) {
        nice = 2.0f;
    }
    info.step = nice * magnitude;
    info.first = std::ceil(min / info.step) * info.step;
    info.count = static_cast<int>(std::floor((max - info.first) / info.step)) + 1;
    if (info.count < 0) {
        info.count = 0;
    }
    // Floating-point drift could put the last tick just past max; drop it.
    while (info.count > 0 && info.first + (info.count - 1) * info.step > max) {
        --info.count;
    }

    // Number of fractional digits the labels need. Steps >= 1 land on integer
    // ticks; otherwise the decimals mirror the step's magnitude (a step of 0.2
    // needs one, 0.05 needs two). Capped so pathological ranges stay readable.
    int decimals = 0;
    if (info.step > 0.0f && info.step < 1.0f) {
        decimals = -static_cast<int>(std::floor(std::log10(info.step)));
    }
    info.decimals = std::min(decimals, 6);
    return info;
}

bool Plot::clipSegment(int& x1, int& y1, int& x2, int& y2, const Rect& rect) {
    const int dx = x2 - x1;
    const int dy = y2 - y1;
    const float left = static_cast<float>(rect.x);
    const float right = static_cast<float>(rect.x + rect.width);
    const float top = static_cast<float>(rect.y);
    const float bottom = static_cast<float>(rect.y + rect.height);

    float t0 = 0.0f;
    float t1 = 1.0f;
    auto clipEdge = [&](float p, float q) -> bool {
        if (p == 0.0f) {
            return q >= 0.0f;
        }
        const float r = q / p;
        if (p < 0.0f) {  // segment enters the box through this edge
            if (r > t1) return false;
            if (r > t0) t0 = r;
        } else {  // segment leaves the box through this edge
            if (r < t0) return false;
            if (r < t1) t1 = r;
        }
        return true;
    };

    if (!clipEdge(-static_cast<float>(dx), x1 - left) ||
        !clipEdge(static_cast<float>(dx), right - x1) ||
        !clipEdge(-static_cast<float>(dy), y1 - top) ||
        !clipEdge(static_cast<float>(dy), bottom - y1)) {
        return false;
    }

    const int nx1 = static_cast<int>(std::lround(x1 + t0 * dx));
    const int ny1 = static_cast<int>(std::lround(y1 + t0 * dy));
    const int nx2 = static_cast<int>(std::lround(x1 + t1 * dx));
    const int ny2 = static_cast<int>(std::lround(y1 + t1 * dy));
    x1 = nx1;
    y1 = ny1;
    x2 = nx2;
    y2 = ny2;
    return true;
}

std::vector<PointF> Plot::buildPolyline(const Series& series, const Rect& content) const {
    // Clipping runs on whole pixels (the layout space), the result is handed
    // to the float canvas: the widening is exact.
    std::vector<PointF> poly;
    if (series.points.size() < 2) {
        return poly;
    }
    bool hasPrev = false;
    PointI prev{};
    bool run = false;
    for (const auto& p : series.points) {
        if (!std::isfinite(p.x) || !std::isfinite(p.y)) {
            hasPrev = false;
            run = false;
            continue;
        }
        const PointI cur{toPixelX(p.x, content), toPixelY(p.y, content)};
        if (hasPrev) {
            int ax = prev.x;
            int ay = prev.y;
            int bx = cur.x;
            int by = cur.y;
            if (clipSegment(ax, ay, bx, by, content)) {
                if (!run) {
                    poly.push_back(PointF(ax, ay));  // where this visible run enters
                    run = true;
                }
                poly.push_back(PointF(bx, by));
            } else {
                run = false;
            }
        }
        prev = cur;
        hasPrev = true;
    }
    return poly;
}

void Plot::drawAxisLabels(PaintContext& pc, const Rect& content, const TickInfo& xTicks,
                          const TickInfo& yTicks) const {
    if (!showAxisLabels_ || (xTicks.count == 0 && yTicks.count == 0)) {
        return;
    }
    const auto& appearance = getComputedAppearance();
    auto& engine = pc.text();
    auto font = engine.getFontForFamily(appearance.fontFamily, appearance.fontSize);
    if (!font) {
        return;
    }

    const Thickness insets = LayoutManager::contentInsets(*this);
    const Rect box = getGlobalBounds();

    // Stage 3: axis labels via TextLayout + tinted glyphs
    const auto placeLabel = [&](const std::string& text, int x, int y) {
        TextLayout layout = engine.layoutText(*font, text);
        if (layout.glyphs.empty() && text.empty()) return;
        const int w = layout.metrics.width;
        const int h =
            layout.metrics.height > 0 ? layout.metrics.height : layout.lineMetrics.lineHeight;
        if (x + w <= box.x || x >= box.x + box.width || y + h <= box.y || y >= box.y + box.height) {
            return;
        }
        TextPaint paint;
        paint.color = axisColor_;
        paint.align = Alignment::Start;
        paint.verticalAlign = Alignment::Start;
        paint.truncate = false;
        engine.drawLayout(pc.canvas(), layout,
                          RectF(static_cast<float>(x), static_cast<float>(y), static_cast<float>(w),
                                static_cast<float>(h)),
                          paint);
    };

    if (insets.left >= kLabelGutterPx) {
        for (int i = 0; i < yTicks.count; ++i) {
            const float value = yTicks.first + i * yTicks.step;
            const std::string text = std::format("{:.{}f}", value, yTicks.decimals);
            TextLayout layout = engine.layoutText(*font, text);
            const int ty = toPixelY(value, content);
            placeLabel(text, content.x - 5 - layout.metrics.width,
                       ty - (layout.metrics.height > 0 ? layout.metrics.height / 2 : 0));
        }
    }

    if (insets.bottom >= kLabelGutterPx) {
        for (int i = 0; i < xTicks.count; ++i) {
            const float value = xTicks.first + i * xTicks.step;
            const std::string text = std::format("{:.{}f}", value, xTicks.decimals);
            TextLayout layout = engine.layoutText(*font, text);
            const int tx = toPixelX(value, content);
            placeLabel(text, tx - layout.metrics.width / 2, content.y + content.height + 5);
        }
    }
}

void Plot::onPaint(PaintContext& pc) {
    const Rect content = LayoutManager::contentRect(*this, getGlobalBounds());
    if (content.width <= 0 || content.height <= 0) {
        return;
    }
    if (autoScale_ && dataVersion_ != lastScaledVersion_) {
        autoScale();
    }

    // Tick layout from a pixel-spacing budget: roughly one grid line every
    // kXTickSpacingPx/kYTickSpacingPx, clamped to a sane range.
    const int targetX = std::clamp(content.width / kXTickSpacingPx, 2, 8);
    const int targetY = std::clamp(content.height / kYTickSpacingPx, 2, 8);
    const TickInfo xTicks = computeTicks(xMin_, xMax_, targetX);
    const TickInfo yTicks = computeTicks(yMin_, yMax_, targetY);

    // The plot body is clipped to the content box; the guard keeps the clip
    // balanced across the early `continue`s below.
    {
        ClipGuard clip(pc.canvas(), content, true);

        if (showGrid_) {
            for (int i = 0; i < xTicks.count; ++i) {
                const int vx = toPixelX(xTicks.first + i * xTicks.step, content);
                pc.canvas().drawLine(PointF(vx, content.y),
                                     PointF(vx, content.y + content.height - 1),
                                     Stroke{gridColor_});
            }
            for (int i = 0; i < yTicks.count; ++i) {
                const int hy = toPixelY(yTicks.first + i * yTicks.step, content);
                pc.canvas().drawLine(PointF(content.x, hy),
                                     PointF(content.x + content.width - 1, hy), Stroke{gridColor_});
            }
        }

        // Zero-threshold axes, drawn only when inside the current world bounds.
        if (xMin_ < 0.0f && xMax_ > 0.0f) {
            const int x0 = toPixelX(0.0f, content);
            pc.canvas().drawLine(PointF(x0, content.y), PointF(x0, content.y + content.height - 1),
                                 Stroke{axisColor_});
        }
        if (yMin_ < 0.0f && yMax_ > 0.0f) {
            const int y0 = toPixelY(0.0f, content);
            pc.canvas().drawLine(PointF(content.x, y0), PointF(content.x + content.width - 1, y0),
                                 Stroke{axisColor_});
        }

        for (const auto& series : series_) {
            // Clipped polylines keep giant off-widget coordinates out of both
            // drawLine and fillPolygon, and provide the polygon skeleton for the
            // area fill.
            std::vector<PointF> poly = buildPolyline(series, content);
            if (poly.size() < 2) {
                continue;
            }
            if (areaEnabled_) {
                std::vector<PointF> polygon = poly;
                polygon.push_back(PointF(content.x + content.width, content.y + content.height));
                polygon.push_back(PointF(content.x, content.y + content.height));
                pc.canvas().fillPolygon(polygon, Fill{Color(series.color.r, series.color.g,
                                                            series.color.b, kAreaAlpha)});
            }
            for (size_t i = 0; i + 1 < poly.size(); ++i) {
                pc.canvas().drawLine(poly[i], poly[i + 1], Stroke{series.color});
            }
        }
    }

    drawAxisLabels(pc, content, xTicks, yTicks);
}

}  // namespace DxvUI