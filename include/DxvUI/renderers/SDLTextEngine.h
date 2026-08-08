#ifndef DXVUI_SDLTEXTENGINE_H
#define DXVUI_SDLTEXTENGINE_H

#include <SDL_ttf.h>

#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <tuple>
#include <utility>

#include "DxvUI/text/ITextEngine.h"

struct SDL_Renderer;

namespace DxvUI {

/**
 * @brief SDL2_ttf-backed text engine.
 *
 * Owns the TTF_Init/TTF_Quit reference count and the font, measurement and
 * rasterized-texture caches. Textures are created against the SDL renderer
 * passed in by the owning SDLRenderer and are cached per (font, text, color).
 * The texture cache must be cleared (clearCaches()) before the SDL renderer is
 * destroyed; the owning renderer does this in its destructor.
 */
class SDLTextEngine : public ITextEngine {
   public:
    explicit SDLTextEngine(SDL_Renderer* renderer);
    ~SDLTextEngine() override;

    SDLTextEngine(const SDLTextEngine&) = delete;
    SDLTextEngine& operator=(const SDLTextEngine&) = delete;

    std::shared_ptr<IFont> getFont(const std::string& path, int size) override;
    TextMetrics measure(const IFont& font, const std::string& text) override;
    LineMetrics lineMetrics(const IFont& font) override;
    std::shared_ptr<ITexture> rasterize(const IFont& font, const std::string& text,
                                        const Color& color) override;

    /**
     * @brief Drops every cached font, measurement and texture.
     *
     * Destroys the cached SDL textures (which reference the SDL renderer), so
     * the owning renderer must call this before destroying the renderer.
     */
    void clearCaches();

   private:
    // Font handle that wraps the raw TTF font plus its cached vertical metrics.
    class SDLFont : public IFont {
       public:
        SDLFont(TTF_Font* font, std::string path, int size);
        ~SDLFont() override;

        TTF_Font* font = nullptr;
        std::string path;
        int size = 0;
        LineMetrics metrics;
    };

    SDL_Renderer* renderer;
    std::map<std::string, std::shared_ptr<SDLFont>> fonts;
    std::map<std::pair<const IFont*, std::string>, TextMetrics> measures;
    std::map<std::tuple<const IFont*, std::string, uint32_t>, std::shared_ptr<ITexture>> textures;

    static int ttf_ref_count;
    static std::mutex ttf_mutex;
    static void initTTF();
    static void quitTTF();
};

}  // namespace DxvUI

#endif  // DXVUI_SDLTEXTENGINE_H
