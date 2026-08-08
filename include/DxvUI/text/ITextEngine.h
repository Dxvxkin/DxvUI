#ifndef DXVUI_ITEXTENGINE_H
#define DXVUI_ITEXTENGINE_H

#include <memory>
#include <string>

#include "DxvUI/interfaces/ITexture.h"
#include "DxvUI/style/Color.h"

namespace DxvUI {

/**
 * @brief Opaque backend font handle.
 *
 * Identical to ITexture: the engine hands out font handles that widgets treat
 * as opaque identities (used as cache keys), never as SDL- or backend-specific
 * objects. A font stays alive as long as the engine (or any widget holding a
 * shared_ptr) keeps it.
 */
struct IFont {
    virtual ~IFont() = default;
};

/**
 * @brief Measured size of a single line of text.
 * @note height is the line height (ascent + descent), not the glyph pixel
 * height, so multiple strings of the same font line up vertically.
 */
struct TextMetrics {
    int width = 0;
    int height = 0;
};

/**
 * @brief Font-wide vertical metrics, used to position a caret or to center a
 * single line on its baseline.
 */
struct LineMetrics {
    int ascent = 0;
    int descent = 0;
    int lineHeight = 0;
};

/**
 * @brief Backend-neutral interface for loading fonts, measuring text and
 * rasterizing it into textures.
 *
 * Owned by the renderer (it needs the backend context to create textures) and
 * reached via IRenderer::getTextEngine(); the interface itself leaks no
 * backend types. All font, measurement and texture results are cached: the same
 * (font, text, color) triple rasterizes once and is shared by every widget, so
 * a Label no longer needs per-widget texture caching. Caches live for the
 * engine's lifetime (clearCaches() is only called by the owning renderer on
 * shutdown).
 */
class ITextEngine {
   public:
    virtual ~ITextEngine() = default;

    /**
     * @brief Gets (or loads and caches) the font for the given path and size.
     * @param path Font file path.
     * @param size Pixel size of the font.
     * @return A font handle, or nullptr when the path is empty/size invalid or
     * the font could not be loaded.
     */
    virtual std::shared_ptr<IFont> getFont(const std::string& path, int size) = 0;

    /**
     * @brief Measures a single line of text with the given font.
     * @param font A font obtained from getFont().
     * @param text UTF-8 text to measure.
     * @return The measured size (zeroed on failure).
     */
    virtual TextMetrics measure(const IFont& font, const std::string& text) = 0;

    /**
     * @brief Gets the font's vertical metrics (ascent/descent/line height).
     * @param font A font obtained from getFont().
     * @return The font's vertical metrics.
     */
    virtual LineMetrics lineMetrics(const IFont& font) = 0;

    /**
     * @brief Rasterizes text into a cached texture.
     *
     * The texture is cached per (font, text, color), so repeated calls with the
     * same arguments are free and identical widgets share one texture.
     * @param font A font obtained from getFont().
     * @param text UTF-8 text to rasterize.
     * @param color The text color, baked into the texture.
     * @return A texture sized to the text, or nullptr for empty text or on
     * failure.
     */
    virtual std::shared_ptr<ITexture> rasterize(const IFont& font, const std::string& text,
                                                const Color& color) = 0;
};

}  // namespace DxvUI

#endif  // DXVUI_ITEXTENGINE_H
