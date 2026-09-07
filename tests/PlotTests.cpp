#include <gtest/gtest.h>

#include <limits>
#include <memory>
#include <string>
#include <vector>

#include "DxvUI/Log.h"
#include "DxvUI/Scene.h"
#include "DxvUI/SceneNode.h"
#include "DxvUI/style/StyleManager.h"
#include "DxvUI/style/Theme.h"
#include "DxvUI/widgets/Plot.h"

using namespace DxvUI;

namespace {

// SceneNode's destructor logs via DxvUI::Log, which requires an initialized
// logger. Install a global test environment so the logger exists for the whole
// test binary.
class LoggerEnvironment : public ::testing::Environment {
   public:
    void SetUp() override { Log::init(); }
};

::testing::Environment* const g_logger_environment =
    ::testing::AddGlobalTestEnvironment(new LoggerEnvironment);

}  // namespace

namespace {

bool floatsNear(float a, float b, float eps = 1e-4f) { return std::abs(a - b) <= eps; }

TEST(PlotTest, DefaultState) {
    auto plot = Plot::create("plot");
    EXPECT_EQ(plot->getSeriesCount(), 0u);
    EXPECT_TRUE(plot->isAutoScaleEnabled());
    EXPECT_TRUE(plot->isGridVisible());
    EXPECT_FLOAT_EQ(plot->getXMin(), 0.0f);
    EXPECT_FLOAT_EQ(plot->getXMax(), 1.0f);
    EXPECT_FLOAT_EQ(plot->getYMin(), 0.0f);
    EXPECT_FLOAT_EQ(plot->getYMax(), 1.0f);
}

TEST(PlotTest, AddSeriesAssignsIndices) {
    auto plot = Plot::create("plot");
    EXPECT_EQ(plot->addSeries("a"), 0u);
    EXPECT_EQ(plot->addSeries(), 1u);
    EXPECT_EQ(plot->addSeries("c"), 2u);
    EXPECT_EQ(plot->getSeriesCount(), 3u);
    EXPECT_TRUE(plot->getSeriesPoints(0).empty());
    EXPECT_TRUE(plot->getSeriesPoints(2).empty());
}

TEST(PlotTest, SeriesUseDefaultPaletteByIndex) {
    auto plot = Plot::create("plot");
    plot->addSeries();
    plot->addSeries();
    EXPECT_EQ(plot->getSeriesColor(0), Colors::CornflowerBlue);
    EXPECT_EQ(plot->getSeriesColor(1), Colors::Orange);
}

TEST(PlotTest, SetSeriesColorOverridesDefault) {
    auto plot = Plot::create("plot");
    plot->addSeries();
    plot->setSeriesColor(0, Colors::Green);
    EXPECT_EQ(plot->getSeriesColor(0), Colors::Green);
    // An out-of-range series reports transparent and is a no-op to set.
    EXPECT_EQ(plot->getSeriesColor(7), Colors::Transparent);
    plot->setSeriesColor(7, Colors::Green);
}

TEST(PlotTest, SetDataUpdatesAutoScaledBounds) {
    auto plot = Plot::create("plot");
    plot->addSeries();
    plot->setData(0, {{0, 0}, {2, 5}, {4, -2}});
    // 5% padding of the ranges [0..4] and [-2..5].
    EXPECT_TRUE(floatsNear(plot->getXMin(), -0.2f));
    EXPECT_TRUE(floatsNear(plot->getXMax(), 4.2f));
    EXPECT_TRUE(floatsNear(plot->getYMin(), -2.35f));
    EXPECT_TRUE(floatsNear(plot->getYMax(), 5.35f));
}

TEST(PlotTest, ApplyingPointsGrowsTheView) {
    auto plot = Plot::create("plot");
    plot->addSeries();
    plot->applyPoint(0, 0.0f, 1.0f);
    plot->applyPoint(0, 2.0f, 3.0f);
    EXPECT_EQ(plot->getSeriesPoints(0).size(), 2u);
    // Range growth: x [0..2] -> xMin -0.1/xMax 2.1; y [1..3] -> 0.9/3.1.
    EXPECT_TRUE(floatsNear(plot->getXMin(), -0.1f));
    EXPECT_TRUE(floatsNear(plot->getXMax(), 2.1f));
    EXPECT_TRUE(floatsNear(plot->getYMin(), 0.9f));
    EXPECT_TRUE(floatsNear(plot->getYMax(), 3.1f));
}

TEST(PlotTest, SingleFlatPointKeepsUnitSpan) {
    auto plot = Plot::create("plot");
    plot->addSeries();
    plot->applyPoint(0, 10.0f, 50.0f);
    // Degenerate ranges get a unit span (0.5 padding per side) so the view does
    // not collapse when the first live point arrives.
    EXPECT_TRUE(floatsNear(plot->getXMin(), 9.5f));
    EXPECT_TRUE(floatsNear(plot->getXMax(), 10.5f));
    EXPECT_TRUE(floatsNear(plot->getYMin(), 49.5f));
    EXPECT_TRUE(floatsNear(plot->getYMax(), 50.5f));
}

TEST(PlotTest, NonFinitePointsAreSkipped) {
    auto plot = Plot::create("plot");
    plot->addSeries();
    const float nan = std::numeric_limits<float>::quiet_NaN();
    const float inf = std::numeric_limits<float>::infinity();
    plot->applyPoint(0, nan, 1.0f);
    plot->applyPoint(0, inf, inf);
    // No finite data -> unit defaults are kept, nothing crashes.
    EXPECT_FLOAT_EQ(plot->getXMin(), 0.0f);
    EXPECT_FLOAT_EQ(plot->getXMax(), 1.0f);
    EXPECT_FLOAT_EQ(plot->getYMin(), 0.0f);
    EXPECT_FLOAT_EQ(plot->getYMax(), 1.0f);
    EXPECT_EQ(plot->getSeriesPoints(0).size(), 2u);
}

TEST(PlotTest, FixedWorldBoundsDisableAutoScale) {
    auto plot = Plot::create("plot");
    plot->addSeries();
    plot->setWorldBounds(-5.0f, -5.0f, 5.0f, 5.0f);
    EXPECT_FALSE(plot->isAutoScaleEnabled());
    // A far-away live point must not move the manual viewport.
    plot->applyPoint(0, 1000.0f, 1000.0f);
    EXPECT_FLOAT_EQ(plot->getXMin(), -5.0f);
    EXPECT_FLOAT_EQ(plot->getXMax(), 5.0f);
    EXPECT_FLOAT_EQ(plot->getYMin(), -5.0f);
    EXPECT_FLOAT_EQ(plot->getYMax(), 5.0f);
}

TEST(PlotTest, ClearWorldBoundsRestoresAutoScale) {
    auto plot = Plot::create("plot");
    plot->addSeries();
    plot->setData(0, {{0, 0}, {10, 20}});
    plot->setWorldBounds(-5.0f, -5.0f, 5.0f, 5.0f);
    plot->clearWorldBounds();
    EXPECT_TRUE(plot->isAutoScaleEnabled());
    // Re-fitted to the data: x [0..10] -> -0.5/10.5, y [0..20] -> -1/21.
    EXPECT_TRUE(floatsNear(plot->getXMin(), -0.5f));
    EXPECT_TRUE(floatsNear(plot->getXMax(), 10.5f));
    EXPECT_TRUE(floatsNear(plot->getYMin(), -1.0f));
    EXPECT_TRUE(floatsNear(plot->getYMax(), 21.0f));
}

TEST(PlotTest, SeriesDataIsIndependent) {
    auto plot = Plot::create("plot");
    const auto a = plot->addSeries("a");
    const auto b = plot->addSeries("b");
    plot->setData(a, {{1, 1}, {2, 2}});
    plot->applyPoint(b, 7.0f, 7.0f);
    EXPECT_EQ(plot->getSeriesPoints(a).size(), 2u);
    EXPECT_EQ(plot->getSeriesPoints(b).size(), 1u);
    EXPECT_FLOAT_EQ(plot->getSeriesPoints(a)[0].x, 1.0f);
    EXPECT_FLOAT_EQ(plot->getSeriesPoints(b)[0].y, 7.0f);
    // Auto-scaled over all series: a covers [1..2]/[1..2], b adds 7.
    EXPECT_TRUE(floatsNear(plot->getYMax(), 7.3f));
}

TEST(PlotTest, GridToggle) {
    auto plot = Plot::create("plot");
    EXPECT_TRUE(plot->isGridVisible());
    plot->setShowGrid(false);
    EXPECT_FALSE(plot->isGridVisible());
    plot->setShowGrid(true);
    EXPECT_TRUE(plot->isGridVisible());
}

TEST(PlotTest, MeasuresToDefaultSizeWhenStretched) {
    // Styles are resolved with a local StyleManager and the tree is measured /
    // arranged manually, exactly like the WidgetTests fixture.
    auto scene = Scene::create();
    auto root = scene->getRoot();
    auto plot = Plot::create("plot");
    Theme theme;
    StyleManager manager{theme};
    root->addChild(plot);
    manager.resolveDirtyStyles(root);
    root->measure({800, 600});
    root->arrange({0, 0, 800, 600});
    const auto& bounds = plot->getGlobalBounds();
    EXPECT_EQ(bounds.width, 300);
    EXPECT_EQ(bounds.height, 200);
    EXPECT_EQ(plot->getNodeType(), std::string("Plot"));
}

}  // namespace