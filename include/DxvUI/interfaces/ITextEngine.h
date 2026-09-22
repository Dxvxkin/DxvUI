#ifndef DXVUI_ITEXTENGINE_H
#define DXVUI_ITEXTENGINE_H

#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "DxvUI/core.h"
#include "DxvUI/interfaces/ITexture.h"
#include "DxvUI/style/Color.h"
#include "DxvUI/style/Colors.h"

namespace DxvUI {

class ICanvas;

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

// ---------------------------------------------------------------------------
// Stage 3: TextLayout + glyph atlas with tint
// ---------------------------------------------------------------------------

/**
 * @brief A single rasterized glyph in the atlas (white, tinted at draw time).
 *
 * The glyph is cached per (font, codepoint): the key no longer includes the
 * text or the color. Color is applied via ICanvas::TextureDraw::tint.
 */
struct Glyph {
    uint32_t codepoint = 0;             // Unicode scalar value
    std::shared_ptr<ITexture> texture;  // white glyph, null for whitespace
    int width = 0;                      // bitmap width
    int height = 0;                     // bitmap height
    int minX = 0;                       // left bearing (from pen to left edge)
    int maxX = 0;
    int minY = 0;
    int maxY = 0;     // top bearing (from baseline to top)
    int advance = 0;  // horizontal advance
};

/**
 * @brief Shaped single-line text: glyphs + positions + metrics.
 *
 * Produced by ITextEngine::layoutText() and cached LRU per (font, text). The
 * layout owns no color — color is supplied at draw time via TextPaint. For
 * Cyrillic/Latin, TTF glyph metrics + kerning are sufficient; HarfBuzz shaping
 * is a separate future step.
 */
struct TextLayout {
    std::string text;     // original UTF-8, kept for debugging / cache key
    TextMetrics metrics;  // total advance width + line height
    LineMetrics lineMetrics;
    std::vector<Glyph> glyphs;
    std::vector<int> xOffsets;        // pen x for each glyph (relative to layout origin)
    std::vector<size_t> byteOffsets;  // byte offset of each glyph in original text
    std::vector<size_t> byteLengths;  // utf-8 byte length of each glyph
    // Stage 3 fallback / fast path: whole string rendered white (per font+text)
    // and tinted at draw time. Guarantees 100% match with old rasterize spacing
    // while still keeping glyph cache for memory measurement.
    std::shared_ptr<ITexture> whiteTexture;

    bool empty() const { return glyphs.empty() && !whiteTexture; }

    /**
     * @brief Returns the x position of a caret at byteOffset.
     * @param byteOffset Byte offset in original text (must be on code-point boundary).
     */
    int caretXAt(size_t byteOffset) const {
        // Find glyph whose byteOffset == byteOffset, or the one after.
        for (size_t i = 0; i < glyphs.size(); ++i) {
            if (byteOffsets[i] >= byteOffset) {
                return xOffsets[i];
            }
            // If caret is inside glyph (should not happen for boundary), return its start.
            if (byteOffset < byteOffsets[i] + byteLengths[i]) {
                return xOffsets[i];
            }
        }
        // Past the end: return total width
        return metrics.width;
    }

    /**
     * @brief Returns byte offset of last whole glyph that fits in maxWidth.
     */
    size_t charIndexAtX(int maxWidth) const {
        if (maxWidth <= 0 || glyphs.empty()) return 0;
        if (metrics.width <= maxWidth) return text.size();
        // Binary search over xOffsets
        size_t lo = 0, hi = glyphs.size();
        while (lo < hi) {
            size_t mid = lo + (hi - lo) / 2;
            // For last fitting glyph, we want xOffsets[mid] + advance <= maxWidth?
            // Use xOffsets[mid] <= maxWidth as inclusive start, but advance check for overflow.
            if (xOffsets[mid] <= maxWidth) {
                // Check if this glyph's end still fits; if not, we still count its start as
                // fitting? For caret, we want last codepoint whose start fits.
                lo = mid + 1;
            } else {
                hi = mid;
            }
        }
        if (lo == 0) return 0;
        size_t idx = lo - 1;
        // If the glyph at idx starts within width but its advance overflows, it still fits
        // partially? Original charIndexAtX returns last whole codepoint that fits. We check if
        // xOffsets[idx] + glyphs[idx].advance <= maxWidth, otherwise previous.
        while (idx > 0 && xOffsets[idx] + glyphs[idx].advance > maxWidth) {
            // If even the start + advance overflows, we need to see if start alone fits?
            // For simplicity, allow glyph whose start fits but end overflows to be considered
            // fitting only if its start <= maxWidth and we are truncating. Original impl used
            // measurePrefix which measured up to byte boundary, not glyph end. So we use start <=
            // maxWidth. The loop above already ensures start <= maxWidth for idx. To match old
            // behavior (whole codepoint fits), we should check if start+advance <= maxWidth,
            // otherwise step back.
            if (xOffsets[idx] + glyphs[idx].advance > maxWidth) {
                if (idx == 0) return 0;
                --idx;
                continue;
            }
            break;
        }
        // Return byte offset after this glyph
        return byteOffsets[idx] + byteLengths[idx];
    }
};

/**
 * @brief Paint parameters for TextLayout.
 *
 * Single place that owns the switch over Alignment (previously duplicated in
 * Label, SDLTextEditorView::draw and hitTestAt). Truncation/ellipsis lives
 * here too.
 */
struct TextPaint {
    Color color = Colors::Black;
    Alignment align = Alignment::Start;
    Alignment verticalAlign = Alignment::Start;
    bool truncate = true;
    // Future: ellipsis, wrap, etc.
};

/**
 * @brief Backend-neutral interface for loading fonts, measuring text and
 * rasterizing it into textures.
 *
 * Owned by the render backend (it needs the backend context to create textures) and
 * reached via IRenderBackend::getTextEngine(); the interface itself leaks no
 * backend types. All font, measurement and texture results are cached: the same
 * (font, text, color) triple rasterizes once and is shared by every widget, so
 * a Label no longer needs per-widget texture caching. Caches live for the
 * engine's lifetime (clearCaches() is only called by the owning renderer on
 * shutdown).
 *
 * Stage 3 extends the contract with TextLayout + glyph atlas: white glyphs
 * cached per (font, codepoint) and tinted at draw time. The old rasterize()
 * path is kept for backward compatibility but no longer used by Label/TextEdit.
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
     * @param family The logical family name (e.g. \"Sans\", \"Serif\", \"Mono\").
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
     * text asks \"which character did I click on\". It never splits a code point
     * and stops before the text would overflow the width.
     * @param font A font obtained from getFont().
     * @param text The UTF-8 text.
     * @param maxWidth The available width in pixels.
     * @return The byte offset of the last whole code point that fits.
     */
    virtual size_t charIndexAtX(const IFont& font, const std::string& text, int maxWidth) = 0;

    /**
     * @brief Rasterizes text into a cached texture (legacy path, stage 0-2).
     *
     * Kept for backward compatibility; stage 3 widgets use layoutText() +
     * drawLayout() with tinted glyphs. The texture is cached per (font, text,
     * color), so repeated calls with the same arguments are free and identical
     * widgets share one texture.
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
     * (each entry is one unique (font, text, color) triple). Since stage 3 the
     * cache holds white glyphs (keyed by font+codepoint), so the count is much
     * smaller.
     */
    virtual size_t getTextureCacheCount() const = 0;

    // --- Stage 3: TextLayout + glyph atlas ---

    /**
     * @brief Builds (or fetches from LRU) a TextLayout for the given text.
     *
     * The layout is cached per (font, text) — color is NOT part of the key.
     * Glyphs inside are white and cached per (font, codepoint).
     * @param font A font obtained from getFont().
     * @param text UTF-8 text to layout.
     * @return A layout (empty on failure, but never null).
     */
    virtual TextLayout layoutText(const IFont& font, std::string_view text) = 0;

    /**
     * @brief Draws a TextLayout into a box with alignment/truncation.
     *
     * Single place that owns the Alignment switch (previously duplicated in
     * Label, TextEditorView::draw and hitTestAt). Handles truncation when the
     * layout is wider than the box and paints glyphs with tint = paint.color.
     * @param canvas The canvas to draw into.
     * @param layout The layout to draw.
     * @param box The box to draw inside (content rect).
     * @param paint Paint parameters (color, alignment).
     */
    virtual void drawLayout(ICanvas& canvas, const TextLayout& layout, const RectF& box,
                            const TextPaint& paint) = 0;

    /**
     * @brief Gets the number of cached white glyphs (stage 3).
     */
    virtual size_t getGlyphCacheCount() const { return getTextureCacheCount(); }

    /**
     * @brief Gets the number of cached TextLayouts (stage 3).
     */
    virtual size_t getLayoutCacheCount() const { return 0; }
};

}  // namespace DxvUI

#endif  // DXVUI_ITEXTENGINE_H
