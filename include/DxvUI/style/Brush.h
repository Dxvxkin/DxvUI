#ifndef DXVUI_BRUSH_H
#define DXVUI_BRUSH_H

#include <optional>

#include "DxvUI/style/Color.h"

namespace DxvUI {

/**
 * @struct LinearGradient
 * @brief Linear gradient for Fill (stage 6b).
 */
struct LinearGradient {
    Color start;
    Color end;
    float angleDeg = 0.0f; // 0 = left->right, 90 = top->bottom, 45 = diagonal
};

/**
 * @struct Fill
 * @brief A shape's interior paint.
 *
 * Stage 2: solid color; Stage 6b: optional linear gradient.
 * If gradient is set, solid color is used as fallback / for simple paths.
 */
struct Fill {
    Color color; // solid fallback
    std::optional<LinearGradient> gradient;

    Fill() = default;
    Fill(const Color& c) : color(c) {}
    Fill(const LinearGradient& g) : color(g.start), gradient(g) {}

    bool hasGradient() const { return gradient.has_value(); }
};

/**
 * @struct Shadow
 * @brief Drop shadow for Brush (stage 6b).
 */
struct Shadow {
    Color color{0, 0, 0, 100};
    float offsetX = 2.0f;
    float offsetY = 2.0f;
    float blur = 4.0f; // currently approximated as spread
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
    std::optional<Shadow> shadow;

    /// @brief A brush that only fills with @p color.
    static Brush filled(const Color& color) {
        return {.fill = Fill{color}, .stroke = std::nullopt, .shadow = std::nullopt};
    }

    static Brush filled(const LinearGradient& grad) {
        return {.fill = Fill{grad}, .stroke = std::nullopt, .shadow = std::nullopt};
    }

    /// @brief A brush that only strokes with @p color and @p thickness.
    static Brush stroked(const Color& color, float thickness = 1.0f) {
        return {.fill = std::nullopt, .stroke = Stroke{color, thickness}, .shadow = std::nullopt};
    }

    /// @brief A brush that fills with @p fillColor and outlines with @p stroke.
    static Brush filledAndStroked(const Color& fillColor, const Stroke& stroke) {
        return {.fill = Fill{fillColor}, .stroke = stroke, .shadow = std::nullopt};
    }

    /// @brief A brush that fills with @p fillColor and outlines with @p strokeColor.
    static Brush filledAndStroked(const Color& fillColor, const Color& strokeColor,
                                  float thickness = 1.0f) {
        return {.fill = Fill{fillColor}, .stroke = Stroke{strokeColor, thickness}, .shadow = std::nullopt};
    }

    static Brush withShadow(const Brush& base, const Shadow& sh) {
        Brush b = base;
        b.shadow = sh;
        return b;
    }

    /// @brief True when neither a fill nor a stroke is set (nothing to paint).
    bool isEmpty() const { return !fill.has_value() && !stroke.has_value(); }
};

}  // namespace DxvUI

#endif  // DXVUI_BRUSH_H
