#ifndef DXVUI_SDLTEXTENGINE_H
#define DXVUI_SDLTEXTENGINE_H

#include <SDL_ttf.h>

#include <cstddef>
#include <cstdint>
#include <list>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>
#include <vector>

#include "DxvUI/interfaces/ITextEngine.h"

struct SDL_Renderer;

namespace DxvUI {

/**
 * @brief SDL2_ttf-backed text engine with glyph atlas + TextLayout (stage 3).
 *
 * Owns the TTF_Init/TTF_Quit reference count and the font, measurement,
 * glyph and layout caches. Glyphs are rasterized once as white textures and
 * tinted at draw time (key = font+codepoint, not font+text+color). TextLayout
 * is cached LRU per (font, text). The old per-(font,text,color) texture cache
 * is kept for backward compatibility of rasterize() but no longer used by
 * Label/TextEdit.
 */
class SDLTextEngine : public ITextEngine {
   public:
    explicit SDLTextEngine(SDL_Renderer* renderer);
    ~SDLTextEngine() override;

    SDLTextEngine(const SDLTextEngine&) = delete;
    SDLTextEngine& operator=(const SDLTextEngine&) = delete;

    std::shared_ptr<IFont> getFont(const std::string& path, int size) override;
    std::shared_ptr<IFont> getFontForFamily(const std::string& family, int size) override;
    void registerFontFamily(const std::string& family, const std::string& path) override;
    TextMetrics measure(const IFont& font, const std::string& text) override;
    LineMetrics lineMetrics(const IFont& font) override;
    int measurePrefix(const IFont& font, const std::string& text, size_t byteCount) override;
    size_t charIndexAtX(const IFont& font, const std::string& text, int maxWidth) override;
    std::shared_ptr<ITexture> rasterize(const IFont& font, const std::string& text,
                                        const Color& color) override;
    size_t getTextureCacheCount() const override;

    // Stage 3
    TextLayout layoutText(const IFont& font, std::string_view text) override;
    void drawLayout(ICanvas& canvas, const TextLayout& layout, const RectF& box,
                    const TextPaint& paint) override;
    size_t getGlyphCacheCount() const override;
    size_t getLayoutCacheCount() const override;

    /**
     * @brief Gets the number of cached text measurements.
     */
    size_t getMeasureCacheCount() const;

    /**
     * @brief Drops every cached font, measurement, texture, glyph and layout.
     */
    void clearCaches();

    // Cache capacity bounds
    static constexpr size_t kMaxTextureCacheEntries = 1024; // legacy per-text
    static constexpr size_t kMaxMeasureCacheEntries = 4096;
    static constexpr size_t kMaxGlyphCacheEntries = 4096;
    static constexpr size_t kMaxLayoutCacheEntries = 1024;

   private:
    class SDLFont : public IFont {
       public:
        SDLFont(TTF_Font* font, std::string path, int size);
        ~SDLFont() override;

        TTF_Font* font = nullptr;
        std::string path;
        int size = 0;
        LineMetrics metrics;
    };

    // Glyph cache entry
    struct GlyphCacheEntry {
        Glyph glyph;
        std::list<std::pair<const IFont*, uint32_t>>::iterator lruIt;
    };

    // Layout cache entry
    struct LayoutCacheEntry {
        TextLayout layout;
        std::list<std::pair<const IFont*, std::string>>::iterator lruIt;
    };

    Glyph getOrCreateGlyph(const SDLFont& sdlFont, uint32_t codepoint);

    SDL_Renderer* renderer;
    std::map<std::string, std::shared_ptr<SDLFont>> fonts;
    std::map<std::string, std::string> families;

    using MeasureKey = std::pair<const IFont*, std::string>;
    std::map<MeasureKey, std::pair<TextMetrics, std::list<MeasureKey>::iterator>> measures;
    std::list<MeasureKey> measureLru;

    using TextureKey = std::tuple<const IFont*, std::string, uint32_t>;
    std::map<TextureKey, std::pair<std::shared_ptr<ITexture>, std::list<TextureKey>::iterator>>
        textures;
    std::list<TextureKey> textureLru;

    // Glyph atlas: white glyphs keyed by (font, codepoint)
    using GlyphKey = std::pair<const IFont*, uint32_t>;
    std::map<GlyphKey, GlyphCacheEntry> glyphs;
    std::list<GlyphKey> glyphLru;

    // TextLayout LRU: key = (font, text)
    using LayoutKey = std::pair<const IFont*, std::string>;
    std::map<LayoutKey, LayoutCacheEntry> layouts;
    std::list<LayoutKey> layoutLru;

    static int ttf_ref_count;
    static std::mutex ttf_mutex;
    static void initTTF();
    static void quitTTF();
};

}  // namespace DxvUI

#endif  // DXVUI_SDLTEXTENGINE_H
