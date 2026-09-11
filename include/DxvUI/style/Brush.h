#ifndef DXVUI_BRUSH_H
#define DXVUI_BRUSH_H

#include <optional>

#include "DxvUI/style/Color.h"

namespace DxvUI {

/**
 * @struct Fill
 * @brief A shape's interior paint.
 *
 * Today a solid color; gradients/patterns extend this struct instead of the
 * canvas interface (see docs/RENDERING_REFACTORING.md §3.1).
 */
struct Fill {
    Color color;
};

/**
 * @struct Stroke
 * @brief A shape's outline paint: color plus thickness in pixels.
 *
 * Thickness is float because the canvas is float-based; the backend rounds it
 * to whole pixels on its side of the boundary.
 */
struct Stroke {
    Color color;
    float thickness = 1.0f;
};

struct Brush;

/**
 * @struct Brush
 * @brief What to paint a shape with: a fill, a stroke, or both.
 *
 * Replaces the renderer's combinatorial overload set (fill/draw x color/border
 * x rect/roundRect/circle/...): a primitive takes one Brush argument and the
 * backend picks the paths it knows. An empty Brush (neither field set) means
 * "paint nothing", which the canvas turns into a no-op.
 *
 * Aggregates, so designated initialization works:
 * @code
 * canvas.fillRoundRect(rect, 4.0f, {.fill = Fill{Colors::White}, .stroke =
 * Stroke{Colors::Gray, 1.0f}});
 * @endcode
 * The named constructors cover the common cases more tersely.
 */
struct Brush {
    std::optional<Fill> fill;
    std::optional<Stroke> stroke;

    /// @brief A brush that only fills with @p color.
    static Brush filled(const Color& color) {
        return {.fill = Fill{color}, .stroke = std::nullopt};
    }

    /// @brief A brush that only strokes with @p color and @p thickness.
    static Brush stroked(const Color& color, float thickness = 1.0f) {
        return {.fill = std::nullopt, .stroke = Stroke{color, thickness}};
    }

    /// @brief A brush that fills with @p fillColor and outlines with @p stroke.
    static Brush filledAndStroked(const Color& fillColor, const Stroke& stroke) {
        return {.fill = Fill{fillColor}, .stroke = stroke};
    }

    /// @brief A brush that fills with @p fillColor and outlines with @p strokeColor.
    static Brush filledAndStroked(const Color& fillColor, const Color& strokeColor,
                                  float thickness = 1.0f) {
        return {.fill = Fill{fillColor}, .stroke = Stroke{strokeColor, thickness}};
    }

    /// @brief True when neither a fill nor a stroke is set (nothing to paint).
    bool isEmpty() const { return !fill.has_value() && !stroke.has_value(); }
};

}  // namespace DxvUI

#endif  // DXVUI_BRUSH_H
