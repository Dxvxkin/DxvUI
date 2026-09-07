#ifndef DXVUI_PLOT_H
#define DXVUI_PLOT_H

#include <array>
#include <string>
#include <vector>

#include "DxvUI/SceneNode.h"
#include "DxvUI/style/Colors.h"

namespace DxvUI {

/**
 * @brief Renders one or more XY data series as polylines over a grid.
 *
 * A minimal plotting widget: each series is a list of world-space points
 * (Point<float>) drawn as a 1px polyline inside the widget's content box, with
 * an optional grid and zero-threshold axes. Points outside the visible bounds
 * are clipped by the renderer's clip rect, so no per-segment culling is needed.
 *
 * Scaling is automatic by default: after every data change the widget recomputes
 * the world bounds to fit all series (with a small padding). Fixed bounds can be
 * set with setWorldBounds(), which disables re-scaling until clearWorldBounds().
 *
 * Use applyPoint() to feed live data (e.g. a per-frame timing monitor) — it must
 * be called per frame for at most series-agnostic append semantics.
 */
class Plot : public SceneNode {
   public:
    static std::shared_ptr<Plot> create(std::string id);

    /// Appends a named series and returns its index. Empty name defaults to the
    /// series index.
    size_t addSeries(std::string name = {});
    /// Replaces the points of a series; invalidates the auto-scaled bounds.
    void setData(size_t series, std::vector<Point<float>> data);
    /// Appends a single point to a series (live data).
    void applyPoint(size_t series, float x, float y);

    size_t getSeriesCount() const;
    const std::vector<Point<float>>& getSeriesPoints(size_t series) const;

    /// Overrides the default palette color for a series.
    void setSeriesColor(size_t series, Color color);
    /// The series color (palette default unless overridden).
    Color getSeriesColor(size_t series) const;

    /// Fixes the world bounds and disables auto-scaling.
    void setWorldBounds(float xMin, float yMin, float xMax, float yMax);
    /// Re-enables auto-scaling, recomputing the bounds from the data.
    void clearWorldBounds();
    bool isAutoScaleEnabled() const;

    // Current world bounds (auto-scaled or manually fixed).
    float getXMin() const;
    float getXMax() const;
    float getYMin() const;
    float getYMax() const;

    void setShowGrid(bool show);
    bool isGridVisible() const;

    const char* getNodeType() const noexcept override;

   protected:
    explicit Plot(std::string id);

    Size onMeasure(const Size& availableSize) override;
    void drawContent(IRenderer& renderer) override;

   private:
    struct Series {
        std::string name;
        std::vector<Point<float>> points;
        Color color;
    };

    // Recomputes xMin_/yMin_/xMax_/yMax_ to fit all series, padded by 5% of each
    // range. Flat or empty data keeps sensible defaults (unit range centered on
    // the value), so a live append can never collapse the view.
    void autoScale();

    // Maps world coordinates to pixels inside the content box. Degrees of
    // freedom are clamped so a degenerate (zero-width/zero-height) range cannot
    // divide by zero.
    int toPixelX(float x, const Rect& content) const;
    int toPixelY(float y, const Rect& content) const;

    std::vector<Series> series_;
    bool autoScale_ = true;
    bool showGrid_ = true;
    float xMin_ = 0.0f;
    float xMax_ = 1.0f;
    float yMin_ = 0.0f;
    float yMax_ = 1.0f;
};

}  // namespace DxvUI

#endif  // DXVUI_PLOT_H