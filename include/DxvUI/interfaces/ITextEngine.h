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
     * @brief Gets (or loads and caches) the font for the given font family and
     * size.
     *
     * Family names are resolved to font files by the engine (see
     * getDefaultFontFamilyPath() for the built-in defaults, extended per engine
     * via registerFontFamily()). An empty or unknown family falls back to the
     * platform default font, so a widget without an explicit family still gets
     * a loadable font.
     * @param family The logical family name (e.g. "Sans", "Serif", "Mono").
     * @param size Pixel size of the font.
     * @return A font handle, or nullptr when the size is invalid or the
     * resolved font file could not be loaded.
     */
    virtual std::shared_ptr<IFont> getFontForFamily(const std::string& family, int size) = 0;

    /**
     * @brief Registers a custom family name mapping for this engine.
     *
     * Overrides the built-in default for the name; subsequent
     * getFontForFamily() calls with that name resolve to the given file.
     * @param family The family name to register.
     * @param path The font file path for the family.
     */
    virtual void registerFontFamily(const std::string& family, const std::string& path) = 0;

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
     * @brief Measures the width of a prefix of a text.
     *
     * The prefix is text[0, byteCount), i.e. the bytes before the caret when
     * the caret sits at byteOffset = byteCount. Used to place a caret at an
     * exact x position. Results are cached like measure().
     * @param font A font obtained from getFont().
     * @param text The UTF-8 text.
     * @param byteCount Number of leading bytes to measure (clamped to the text
     * length).
     * @return The width in pixels of the prefix (zero on failure).
     */
    virtual int measurePrefix(const IFont& font, const std::string& text, size_t byteCount) = 0;

    /**
     * @brief Returns how many whole UTF-8 code points of a text fit in a
     * given width.
     *
     * This is the inverse of measurePrefix(): hit-testing a click on a line of
     * text asks "which character did I click on". It never splits a code point
     * and stops before the text would overflow the width.
     * @param font A font obtained from getFont().
     * @param text The UTF-8 text.
     * @param maxWidth The available width in pixels.
     * @return The byte offset of the last whole code point that fits.
     */
    virtual size_t charIndexAtX(const IFont& font, const std::string& text, int maxWidth) = 0;

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

    /**
     * @brief Gets the number of cached rasterized textures.
     *
     * The texture cache is LRU-bounded (see SDLTextEngine), so a UI that
     * changes text or colors continuously evicts older entries instead of
     * growing without limit. Exposed for benchmarks to track cache growth
     * (each entry is one unique (font, text, color) triple).
     */
    virtual size_t getTextureCacheCount() const = 0;
};

}  // namespace DxvUI

#endif  // DXVUI_ITEXTENGINE_H
