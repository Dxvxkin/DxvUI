#include "DxvUI/backend/SDLTextEngine.h"

#include <SDL.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <vector>

#include "DxvUI/Log.h"
#include "DxvUI/core.h"
#include "DxvUI/interfaces/ICanvas.h"
#include "backend/SDLTexture.h"

namespace DxvUI {

int SDLTextEngine::ttf_ref_count = 0;
std::mutex SDLTextEngine::ttf_mutex;

void SDLTextEngine::initTTF() {
    std::lock_guard<std::mutex> lock(ttf_mutex);
    if (ttf_ref_count == 0) {
        if (TTF_Init() == -1)
            throw std::runtime_error(std::string("TTF_Init Error: ") + TTF_GetError());
    }
    ttf_ref_count++;
}

void SDLTextEngine::quitTTF() {
    std::lock_guard<std::mutex> lock(ttf_mutex);
    if (ttf_ref_count > 0) {
        ttf_ref_count--;
        if (ttf_ref_count == 0 && TTF_WasInit()) {
            TTF_Quit();
        }
    }
}

SDLTextEngine::SDLFont::SDLFont(TTF_Font* font, std::string path, int size)
    : font(font), path(std::move(path)), size(size) {
    metrics.ascent = TTF_FontAscent(font);
    metrics.descent = TTF_FontDescent(font);
    metrics.lineHeight = TTF_FontHeight(font);
    if (metrics.lineHeight <= 0) {
        metrics.lineHeight = metrics.ascent - metrics.descent;
    }
}

SDLTextEngine::SDLFont::~SDLFont() {
    if (font) {
        TTF_CloseFont(font);
    }
}

SDLTextEngine::SDLTextEngine(SDL_Renderer* renderer) : renderer(renderer) {
    if (!renderer) throw std::invalid_argument("SDLTextEngine renderer cannot be null.");
    initTTF();
}

SDLTextEngine::~SDLTextEngine() { quitTTF(); }

std::shared_ptr<IFont> SDLTextEngine::getFont(const std::string& path, int size) {
    if (path.empty() || size <= 0) return nullptr;
    const std::string fontKey = path + ":" + std::to_string(size);
    if (auto it = fonts.find(fontKey); it != fonts.end()) {
        return it->second;
    }

    TTF_Font* font = TTF_OpenFont(path.c_str(), size);
    if (!font) {
        Log::error("TTF_OpenFont Error: {} for font {}", TTF_GetError(), path);
        return nullptr;
    }
    TTF_SetFontHinting(font, TTF_HINTING_LIGHT);
    auto handle = std::make_shared<SDLFont>(font, path, size);
    fonts[fontKey] = handle;
    return handle;
}

std::shared_ptr<IFont> SDLTextEngine::getFontForFamily(const std::string& family, int size) {
    if (size <= 0) return nullptr;
    std::string path;
    if (auto it = families.find(family); it != families.end()) {
        path = it->second;
    } else {
        path = getDefaultFontFamilyPath(family);
    }
    return getFont(path, size);
}

void SDLTextEngine::registerFontFamily(const std::string& family, const std::string& path) {
    families[family] = path;
}

// ---------------------------------------------------------------------------
// UTF-8 decoding helpers
// ---------------------------------------------------------------------------

namespace {

struct DecodedCodepoint {
    uint32_t codepoint = 0;
    size_t byteOffset = 0;
    size_t byteLength = 0;
};

// Decodes UTF-8 into codepoints with byte offsets. Invalid sequences are
// treated as single bytes (replacement).
std::vector<DecodedCodepoint> decodeUTF8(std::string_view text) {
    std::vector<DecodedCodepoint> out;
    out.reserve(text.size());
    size_t i = 0;
    const size_t len = text.size();
    while (i < len) {
        unsigned char c = static_cast<unsigned char>(text[i]);
        uint32_t cp = 0;
        size_t extra = 0;
        if (c < 0x80) {
            cp = c;
            extra = 0;
        } else if ((c >> 5) == 0x6) {  // 110x
            if (i + 1 < len) {
                unsigned char c2 = static_cast<unsigned char>(text[i + 1]);
                if ((c2 & 0xC0) == 0x80) {
                    cp = ((c & 0x1F) << 6) | (c2 & 0x3F);
                    extra = 1;
                }
            }
        } else if ((c >> 4) == 0xE) {  // 1110
            if (i + 2 < len) {
                unsigned char c2 = static_cast<unsigned char>(text[i + 1]);
                unsigned char c3 = static_cast<unsigned char>(text[i + 2]);
                if ((c2 & 0xC0) == 0x80 && (c3 & 0xC0) == 0x80) {
                    cp = ((c & 0x0F) << 12) | ((c2 & 0x3F) << 6) | (c3 & 0x3F);
                    extra = 2;
                }
            }
        } else if ((c >> 3) == 0x1E) {  // 11110
            if (i + 3 < len) {
                unsigned char c2 = static_cast<unsigned char>(text[i + 1]);
                unsigned char c3 = static_cast<unsigned char>(text[i + 2]);
                unsigned char c4 = static_cast<unsigned char>(text[i + 3]);
                if ((c2 & 0xC0) == 0x80 && (c3 & 0xC0) == 0x80 && (c4 & 0xC0) == 0x80) {
                    cp =
                        ((c & 0x07) << 18) | ((c2 & 0x3F) << 12) | ((c3 & 0x3F) << 6) | (c4 & 0x3F);
                    extra = 3;
                }
            }
        }

        if (extra == 0 && cp == 0 && c >= 0x80) {
            // Invalid: treat as single byte
            cp = c;
            out.push_back({cp, i, 1});
            ++i;
        } else if (cp == 0 && c >= 0x80) {
            // Failed to decode multibyte, treat first byte as cp
            cp = c;
            out.push_back({cp, i, 1});
            ++i;
        } else {
            size_t bl = extra + 1;
            if (i + bl > len) bl = 1;
            out.push_back({cp, i, bl});
            i += bl;
        }
    }
    return out;
}

}  // namespace

// ---------------------------------------------------------------------------
// Glyph cache
// ---------------------------------------------------------------------------

Glyph SDLTextEngine::getOrCreateGlyph(const SDLFont& sdlFont, uint32_t codepoint) {
    const GlyphKey key{static_cast<const IFont*>(&sdlFont), codepoint};
    auto it = glyphs.find(key);
    if (it != glyphs.end()) {
        glyphLru.splice(glyphLru.begin(), glyphLru, it->second.lruIt);
        return it->second.glyph;
    }

    Glyph glyph;
    glyph.codepoint = codepoint;

    int minx = 0, maxx = 0, miny = 0, maxy = 0, advance = 0;
    bool metricsOk = false;
#if SDL_TTF_VERSION_ATLEAST(2, 0, 18)
    if (TTF_GlyphMetrics32(sdlFont.font, codepoint, &minx, &maxx, &miny, &maxy, &advance) == 0) {
        metricsOk = true;
    } else
#endif
        if (codepoint <= 0xFFFF) {
        if (TTF_GlyphMetrics(sdlFont.font, static_cast<Uint16>(codepoint), &minx, &maxx, &miny,
                             &maxy, &advance) == 0) {
            metricsOk = true;
        }
    }
    if (!metricsOk) {
        // If glyph not provided, TTF_GlyphMetrics may fail; try to get advance via fallback
        // For whitespace, advance may still be available via metrics? Use 0.
        // Log but continue.
        // For codepoints > 0xFFFF, SDL_ttf's Uint16 overload truncates; try 32-bit version if
        // available TTF_GlyphMetrics32 exists in newer SDL_ttf; fallback to 0. We'll attempt
        // TTF_GlyphMetrics32 if available (not in older API, so ignore)
        advance = 0;
        minx = maxx = miny = maxy = 0;
    }

    glyph.minX = minx;
    glyph.maxX = maxx;
    glyph.minY = miny;
    glyph.maxY = maxy;
    glyph.advance = advance > 0 ? advance : 0;
    glyph.width = 0;
    glyph.height = 0;

    // For whitespace and control chars, skip texture creation
    bool isWhitespace = (codepoint == 32 || codepoint == 9 || codepoint == 10 || codepoint == 13);
    if (!isWhitespace && codepoint >= 32) {
        // Check if glyph is provided
        int provided = 0;
#if SDL_TTF_VERSION_ATLEAST(2, 0, 18)
        if (codepoint > 0xFFFF) {
            provided = TTF_GlyphIsProvided32(sdlFont.font, codepoint);
        } else {
            provided = TTF_GlyphIsProvided(sdlFont.font, static_cast<Uint16>(codepoint));
        }
#else
        provided = TTF_GlyphIsProvided(sdlFont.font, static_cast<Uint16>(codepoint));
#endif
        // For codepoints > 0xFFFF, IsProvided with Uint16 truncates, but we still try render
        if (provided || codepoint > 0xFFFF || codepoint == 32) {
            SDL_Color white = {255, 255, 255, 255};
            SDL_Surface* surf = nullptr;
            // TTF_RenderGlyph_Blended takes Uint16 for older API, but SDL_ttf 2.20+ has _32
            // Try 32 first if codepoint > 0xFFFF, else 16.
            if (codepoint <= 0xFFFF) {
                surf = TTF_RenderGlyph_Blended(sdlFont.font, static_cast<Uint16>(codepoint), white);
            } else {
                // Fallback: SDL_ttf may have TTF_RenderGlyph32_Blended; if not, try truncated
#if SDL_TTF_VERSION_ATLEAST(2, 0, 18)
                surf = TTF_RenderGlyph32_Blended(sdlFont.font, codepoint, white);
#else
                surf = TTF_RenderGlyph_Blended(sdlFont.font, static_cast<Uint16>(codepoint), white);
#endif
            }
            if (surf) {
                // Keep surface size for dst to avoid extra gaps.
                // Metrics (minX/minY) are used for positioning to keep baseline stable.
                glyph.width = surf->w;
                glyph.height = surf->h;
                auto tex = SDL_CreateTextureFromSurface(renderer, surf);
                SDL_FreeSurface(surf);
                if (tex) {
                    SDL_SetTextureBlendMode(tex, SDL_BLENDMODE_BLEND);
                    glyph.texture = std::make_shared<SDLTexture>(tex);
                }
            }
        }
    }

    // If advance is 0 but we have a texture width, use width as fallback (for some fonts)
    if (glyph.advance == 0 && glyph.width > 0) {
        glyph.advance = glyph.width + 1;
    }
    // Ensure min/max produce non-zero size for visible glyphs that have texture
    // but metrics gave 0 size (can happen when TTF_GlyphMetrics fails). Fall back
    // to texture size and synthesize bearings so baseline stays stable.
    if (glyph.texture) {
        if (glyph.maxY == 0 && glyph.minY == 0 && glyph.height > 0) {
            glyph.maxY = glyph.height;
            glyph.minY = 0;
        }
        if (glyph.maxX == 0 && glyph.minX == 0 && glyph.width > 0) {
            glyph.maxX = glyph.width;
            glyph.minX = 0;
        }
    }

    // Space advance: if still 0, use font's space advance
    if (glyph.advance == 0 && codepoint == 32) {
        // Measure space via TTF_SizeUTF8
        int w = 0, h = 0;
        if (TTF_SizeUTF8(sdlFont.font, " ", &w, &h) == 0) {
            glyph.advance = w;
        } else {
            glyph.advance = sdlFont.metrics.lineHeight / 3;
        }
    }

    glyphLru.push_front(key);
    GlyphCacheEntry entry;
    entry.glyph = glyph;
    entry.lruIt = glyphLru.begin();
    glyphs.emplace(key, std::move(entry));

    if (glyphs.size() > kMaxGlyphCacheEntries) {
        const auto& victim = glyphLru.back();
        glyphs.erase(victim);
        glyphLru.pop_back();
    }

    return glyph;
}

// ---------------------------------------------------------------------------
// TextLayout cache
// ---------------------------------------------------------------------------

TextLayout SDLTextEngine::layoutText(const IFont& font, std::string_view text) {
    const SDLFont* sdlFont = static_cast<const SDLFont*>(&font);
    if (!sdlFont || !sdlFont->font) {
        return TextLayout{};
    }

    std::string textStr(text);
    const LayoutKey key{static_cast<const IFont*>(&font), textStr};
    auto it = layouts.find(key);
    if (it != layouts.end()) {
        layoutLru.splice(layoutLru.begin(), layoutLru, it->second.lruIt);
        return it->second.layout;
    }

    TextLayout layout;
    layout.text = textStr;
    layout.lineMetrics = sdlFont->metrics;

    if (text.empty()) {
        layout.metrics.width = 0;
        layout.metrics.height = sdlFont->metrics.lineHeight;
        // cache empty layout
        layoutLru.push_front(key);
        layouts.emplace(key, LayoutCacheEntry{layout, layoutLru.begin()});
        if (layouts.size() > kMaxLayoutCacheEntries) {
            layouts.erase(layoutLru.back());
            layoutLru.pop_back();
        }
        return layout;
    }

    auto decoded = decodeUTF8(text);

    layout.glyphs.reserve(decoded.size());
    layout.xOffsets.reserve(decoded.size());
    layout.byteOffsets.reserve(decoded.size());
    layout.byteLengths.reserve(decoded.size());

    int penX = 0;
    uint32_t prevCp = 0;
    for (auto& d : decoded) {
        if (prevCp != 0) {
#if SDL_TTF_VERSION_ATLEAST(2, 0, 14)
            if (d.codepoint <= 0xFFFF && prevCp <= 0xFFFF) {
                int kern = TTF_GetFontKerningSizeGlyphs(sdlFont->font, static_cast<Uint16>(prevCp),
                                                        static_cast<Uint16>(d.codepoint));
                penX += kern;
            }
#endif
        }
        Glyph g = getOrCreateGlyph(*sdlFont, d.codepoint);
        layout.xOffsets.push_back(penX);
        layout.byteOffsets.push_back(d.byteOffset);
        layout.byteLengths.push_back(d.byteLength);
        layout.glyphs.push_back(std::move(g));
        penX += layout.glyphs.back().advance;
        prevCp = d.codepoint;
    }

    layout.metrics.width = penX;
    layout.metrics.height = sdlFont->metrics.lineHeight;

    // White per-string texture for perfect spacing (TTF_RenderUTF8_Blended).
    // Cached via rasterize LRU per (font,text,white). When it exists, use its
    // width for metrics so alignment matches the texture that will be drawn.
    {
        Color white{255, 255, 255, 255};
        auto whiteTex = rasterize(*sdlFont, textStr, white);
        layout.whiteTexture = whiteTex;
        if (whiteTex) {
            layout.metrics.width = whiteTex->getWidth();
        }
    }

    layoutLru.push_front(key);
    layouts.emplace(key, LayoutCacheEntry{layout, layoutLru.begin()});
    if (layouts.size() > kMaxLayoutCacheEntries) {
        layouts.erase(layoutLru.back());
        layoutLru.pop_back();
    }

    return layout;
}

void SDLTextEngine::drawLayout(ICanvas& canvas, const TextLayout& layout, const RectF& box,
                               const TextPaint& paint) {
    const float boxX = box.x;
    const float boxY = box.y;
    const float boxW = box.width;
    const float boxH = box.height;

    const int layoutW = layout.metrics.width;
    const int layoutH = layout.metrics.height;

    float alignOffsetX = 0.0f;
    if (paint.truncate && layoutW > boxW) {
        alignOffsetX = 0.0f;
    } else {
        switch (paint.align) {
            case Alignment::Center:
                alignOffsetX = (boxW - layoutW) / 2.0f;
                break;
            case Alignment::End:
                alignOffsetX = boxW - layoutW;
                break;
            case Alignment::Start:
            case Alignment::Stretch:
            default:
                alignOffsetX = 0.0f;
                break;
        }
    }

    float alignOffsetY = 0.0f;
    if (layoutH <= boxH) {
        switch (paint.verticalAlign) {
            case Alignment::Center:
                alignOffsetY = (boxH - layoutH) / 2.0f;
                break;
            case Alignment::End:
                alignOffsetY = boxH - layoutH;
                break;
            case Alignment::Start:
            case Alignment::Stretch:
            default:
                alignOffsetY = 0.0f;
                break;
        }
    }

    // Fast path: white per-string texture with tint – 100% matches old
    // TTF_RenderUTF8_Blended spacing, baseline stable, no per-glyph gaps.
    // This path is now safe after fixing fillRoundedRectGeometry early-return
    // that caused black rects (bf271c5).
    if (layout.whiteTexture) {
        RectF dst(boxX + alignOffsetX, boxY + alignOffsetY,
                  static_cast<float>(layout.whiteTexture->getWidth()),
                  static_cast<float>(layout.whiteTexture->getHeight()));
        ICanvas::TextureDraw td;
        td.dst = dst;
        td.tint = paint.color;
        td.alpha = 1.0f;
        if (paint.truncate && layoutW > boxW) {
            td.src = RectF(0, 0, boxW, static_cast<float>(layout.whiteTexture->getHeight()));
            td.dst = RectF(boxX + alignOffsetX, boxY + alignOffsetY, boxW,
                           static_cast<float>(layout.whiteTexture->getHeight()));
        }
        canvas.drawTexture(layout.whiteTexture, td);
        return;
    }

    if (layout.glyphs.empty()) return;

    const float baseline = boxY + alignOffsetY + layout.lineMetrics.ascent;

    for (size_t i = 0; i < layout.glyphs.size(); ++i) {
        const Glyph& g = layout.glyphs[i];
        const int penX = layout.xOffsets[i];

        if (paint.truncate && layoutW > boxW) {
            if (penX >= boxW) break;
        }

        if (!g.texture) continue;  // whitespace

        float dstX = boxX + alignOffsetX + penX + g.minX;
        // Fallback per-glyph: top = baseline - maxY when available
        float dstY;
        if (g.maxY != 0 || g.minY != 0) {
            dstY = baseline - static_cast<float>(g.maxY);
        } else {
            dstY = baseline - static_cast<float>(g.minY) - static_cast<float>(g.height);
        }

        RectF dst(dstX, dstY, static_cast<float>(g.width), static_cast<float>(g.height));

        ICanvas::TextureDraw td;
        td.dst = dst;
        td.tint = paint.color;
        td.alpha = 1.0f;
        canvas.drawTexture(g.texture, td);
    }
}

// ---------------------------------------------------------------------------
// Legacy measure / rasterize paths (kept for backward compat)
// ---------------------------------------------------------------------------

TextMetrics SDLTextEngine::measure(const IFont& font, const std::string& text) {
    const auto* sdlFont = static_cast<const SDLFont*>(&font);
    if (!sdlFont || !sdlFont->font) return {0, 0};

    const MeasureKey key{static_cast<const IFont*>(&font), text};
    if (auto it = measures.find(key); it != measures.end()) {
        measureLru.splice(measureLru.begin(), measureLru, it->second.second);
        return it->second.first;
    }

    // Use layout path for consistency: layout already caches per (font,text)
    TextLayout layout = layoutText(font, text);
    TextMetrics metrics = layout.metrics;
    // For empty text, TTF_SizeUTF8 returns 0 height, but we want line height? Keep layout's height.
    // To match old behavior, if text empty, width 0 height 0? Old measure returned 0 for empty via
    // TTF_SizeUTF8? TTF_SizeUTF8 for empty returns 0,0. We'll keep layout metrics which has
    // lineHeight for empty, but for backward compat we return 0 height for empty? Let's check: old
    // measure returned w/h from TTF_SizeUTF8, which for empty returns 0,0. Our layout returns
    // height = lineHeight for empty. To keep compatibility, we should return 0,0 for empty? But
    // Label expects non-zero? Actually Label's onMeasure used measure, and for empty text it would
    // measure 0,0 and then add padding. That's okay. We'll keep layout metrics for non-empty, and
    // for empty return 0,0 to match old.
    if (text.empty()) {
        metrics = {0, 0};
    }

    measureLru.push_front(key);
    measures.emplace(key, std::make_pair(metrics, measureLru.begin()));
    if (measures.size() > kMaxMeasureCacheEntries) {
        measures.erase(measureLru.back());
        measureLru.pop_back();
    }
    return metrics;
}

LineMetrics SDLTextEngine::lineMetrics(const IFont& font) {
    const auto* sdlFont = static_cast<const SDLFont*>(&font);
    if (!sdlFont) return {0, 0, 0};
    return sdlFont->metrics;
}

int SDLTextEngine::measurePrefix(const IFont& font, const std::string& text, size_t byteCount) {
    const size_t count = std::min(byteCount, text.size());
    if (count == 0) return 0;
    // Use layout to get caret X
    TextLayout layout = layoutText(font, text);
    return layout.caretXAt(count);
}

size_t SDLTextEngine::charIndexAtX(const IFont& font, const std::string& text, int maxWidth) {
    if (text.empty() || maxWidth <= 0) return 0;
    TextLayout layout = layoutText(font, text);
    return layout.charIndexAtX(maxWidth);
}

std::shared_ptr<ITexture> SDLTextEngine::rasterize(const IFont& font, const std::string& text,
                                                   const Color& color) {
    if (text.empty()) {
        return nullptr;
    }

    const auto* sdlFont = static_cast<const SDLFont*>(&font);
    if (!sdlFont || !sdlFont->font) return nullptr;

    const uint32_t colorKey =
        static_cast<uint32_t>(color.r) | (static_cast<uint32_t>(color.g) << 8) |
        (static_cast<uint32_t>(color.b) << 16) | (static_cast<uint32_t>(color.a) << 24);
    const auto key = std::make_tuple(static_cast<const IFont*>(&font), text, colorKey);
    if (auto it = textures.find(key); it != textures.end()) {
        textureLru.splice(textureLru.begin(), textureLru, it->second.second);
        return it->second.first;
    }

    auto surf =
        TTF_RenderUTF8_Blended(sdlFont->font, text.c_str(), {color.r, color.g, color.b, color.a});
    if (!surf) {
        Log::error("TTF_RenderUTF8_Blended Error: {}", TTF_GetError());
        return nullptr;
    }

    auto texture = SDL_CreateTextureFromSurface(renderer, surf);
    SDL_FreeSurface(surf);
    if (!texture) {
        Log::error("SDL_CreateTextureFromSurface Error: {}", SDL_GetError());
        return nullptr;
    }
    SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_BLEND);

    auto handle = std::make_shared<SDLTexture>(texture);
    textureLru.push_front(key);
    textures.emplace(key, std::make_pair(handle, textureLru.begin()));
    if (textures.size() > kMaxTextureCacheEntries) {
        const auto& victim = textureLru.back();
        textures.erase(victim);
        textureLru.pop_back();
    }
    return handle;
}

void SDLTextEngine::clearCaches() {
    textures.clear();
    textureLru.clear();
    measures.clear();
    measureLru.clear();
    glyphs.clear();
    glyphLru.clear();
    layouts.clear();
    layoutLru.clear();
    fonts.clear();
}

size_t SDLTextEngine::getTextureCacheCount() const { return textures.size(); }

size_t SDLTextEngine::getMeasureCacheCount() const { return measures.size(); }

size_t SDLTextEngine::getGlyphCacheCount() const { return glyphs.size(); }

size_t SDLTextEngine::getLayoutCacheCount() const { return layouts.size(); }

}  // namespace DxvUI
