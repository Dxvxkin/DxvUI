#ifndef DXVUI_IRENDERBACKEND_H
#define DXVUI_IRENDERBACKEND_H

#include <memory>

#include "DxvUI/core.h"
#include "DxvUI/interfaces/ICanvas.h"
#include "DxvUI/interfaces/ITextEngine.h"

namespace DxvUI {

/**
 * @brief Backend that owns frame lifecycle and provides painting surface.
 *
 * Stage 5 of rendering refactoring (docs/RENDERING_REFACTORING.md §3.5):
 * - owns resources (window/renderer) or wraps external ones
 * - frame lifecycle: beginFrame(clear) -> ICanvas& -> endFrame() (present)
 * - output size and DPI scale
 * - text engine (needs backend context to create textures)
 * - future: createTexture(ImageData) for Image widget, createRenderTarget(Size) for subtree cache
 *
 * Painting itself is via ICanvas (float Brush-based), not here. Platform services
 * (cursor/clipboard) are in IPlatformServices, so Scene::draw no longer depends on them.
 */
class IRenderBackend {
   public:
    virtual ~IRenderBackend() = default;

    // Frame lifecycle – host that owns resources clears/presents here.
    // External mode (wrapping external SDL_Renderer) may ignore clear/present – host does it.
    // Legacy clear/present kept for backward compat with hosts that call them directly.
    virtual void clear(const Color& color) = 0;
    virtual void present() = 0;

    virtual ICanvas& beginFrame(const Color& clearColor) = 0;
    virtual void endFrame() = 0;

    virtual Size getViewportSize() const = 0;
    virtual float getDpiScale() const { return 1.0f; }

    virtual ITextEngine& getTextEngine() = 0;

    // Future stage 6: image loading and render-target cache
    // virtual std::shared_ptr<ITexture> createTexture(const ImageData&) = 0;
    // virtual std::unique_ptr<ICanvas> createRenderTarget(Size) = 0;
};

}  // namespace DxvUI

#endif  // DXVUI_IRENDERBACKEND_H
