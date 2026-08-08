#include "DxvUI/renderers/SDLTextEngine.h"

#include <SDL.h>

#include <iostream>
#include <stdexcept>

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
        return it->second;
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
    textures[key] = handle;
    return handle;
}

void SDLTextEngine::clearCaches() {
    textures.clear();
    measures.clear();
    fonts.clear();
}

}  // namespace DxvUI
