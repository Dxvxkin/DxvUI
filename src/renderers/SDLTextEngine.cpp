#include "DxvUI/renderers/SDLTextEngine.h"

#include <SDL.h>

#include <algorithm>
#include <cstddef>
#include <iostream>
#include <stdexcept>
#include <vector>

#include "DxvUI/Log.h"
#include "renderers/SDLTexture.h"

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
    metrics.lineHeight = metrics.ascent + metrics.descent;
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

SDLTextEngine::~SDLTextEngine() {
    // Fonts are closed by SDLFont destructors; the TTF refcount is released
    // once every engine instance is gone.
    quitTTF();
}

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

TextMetrics SDLTextEngine::measure(const IFont& font, const std::string& text) {
    const auto* sdlFont = static_cast<const SDLFont*>(&font);
    const auto key = std::make_pair(static_cast<const IFont*>(&font), text);
    if (auto it = measures.find(key); it != measures.end()) {
        return it->second;
    }

    TextMetrics metrics;
    int w = 0;
    int h = 0;
    if (TTF_SizeUTF8(sdlFont->font, text.c_str(), &w, &h) != 0) {
        Log::error("TTF_SizeUTF8 Error: {} for '{}'", TTF_GetError(), text);
    } else {
        metrics.width = w;
        metrics.height = h;
    }
    measures[key] = metrics;
    return metrics;
}

LineMetrics SDLTextEngine::lineMetrics(const IFont& font) {
    return static_cast<const SDLFont*>(&font)->metrics;
}

int SDLTextEngine::measurePrefix(const IFont& font, const std::string& text, size_t byteCount) {
    const size_t count = std::min(byteCount, text.size());
    // Reuses the cached measure() path: each distinct prefix is cached as a
    // regular measurement, so typing re-measures only the new prefixes.
    return measure(font, text.substr(0, count)).width;
}

namespace {
// Byte offset of the start of the code point following the one at `start`.
size_t nextCodePointOffset(const std::string& text, size_t start) {
    const size_t len = text.size();
    if (start >= len) return len;
    size_t i = start + 1;
    while (i < len && (static_cast<unsigned char>(text[i]) & 0xC0) == 0x80) {
        ++i;
    }
    return i;
}
}  // namespace

size_t SDLTextEngine::charIndexAtX(const IFont& font, const std::string& text, int maxWidth) {
    if (text.empty() || maxWidth <= 0) return 0;

    // Collect code point boundaries so the answer is always a valid caret
    // position (never a split multi-byte code point). Text is short (single
    // line), so the linear scan is cheap and robust for every Unicode range.
    std::vector<size_t> boundaries;
    for (size_t i = 0; i < text.size();) {
        boundaries.push_back(i);
        i = nextCodePointOffset(text, i);
    }
    boundaries.push_back(text.size());

    // Binary search for the last boundary whose prefix still fits. After the
    // loop lo is the first index that does not fit; the previous one fits.
    size_t lo = 0;
    size_t hi = boundaries.size();
    while (lo < hi) {
        const size_t mid = lo + (hi - lo) / 2;
        if (measurePrefix(font, text, boundaries[mid]) <= maxWidth) {
            lo = mid + 1;
        } else {
            hi = mid;
        }
    }
    // boundaries[0] == 0 always fits, so lo is never 0 here.
    return boundaries[lo - 1];
}

std::shared_ptr<ITexture> SDLTextEngine::rasterize(const IFont& font, const std::string& text,
                                                   const Color& color) {
    if (text.empty()) {
        return nullptr;
    }

    const auto* sdlFont = static_cast<const SDLFont*>(&font);
    const uint32_t colorKey =
        static_cast<uint32_t>(color.r) | (static_cast<uint32_t>(color.g) << 8) |
        (static_cast<uint32_t>(color.b) << 16) | (static_cast<uint32_t>(color.a) << 24);
    const auto key = std::make_tuple(static_cast<const IFont*>(&font), text, colorKey);
    if (auto it = textures.find(key); it != textures.end()) {
        // Cache hit: move the key to the front of the recency list.
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

    auto handle = std::make_shared<SDLTexture>(texture);
    textureLru.push_front(key);
    textures.emplace(key, std::make_pair(handle, textureLru.begin()));
    // Bound the cache: drop the least-recently-used entry. The shared_ptr the
    // caller just received (and any label holding it) keeps the texture alive.
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
    fonts.clear();
}

size_t SDLTextEngine::getTextureCacheCount() const { return textures.size(); }

}  // namespace DxvUI
