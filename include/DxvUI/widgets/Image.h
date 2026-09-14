#ifndef DXVUI_IMAGE_H
#define DXVUI_IMAGE_H

#include <memory>
#include <optional>
#include <string>

#include "DxvUI/SceneNode.h"
#include "DxvUI/core/ImageData.h"
#include "DxvUI/interfaces/ITexture.h"

namespace DxvUI {

enum class ImageFit {
    None,      // No scaling, draw at natural size at top-left
    Contain,   // Fit inside bounds preserving aspect
    Cover,     // Fill bounds preserving aspect, crop
    Fill,      // Stretch to fill bounds
    ScaleDown  // Like Contain but never upscale
};

/**
 * @class Image
 * @brief Displays a texture created via IRenderBackend::createTexture.
 *
 * Stage 6b: Image widget. Holds a shared_ptr<ITexture> that can come from
 * backend->createTexture(ImageData) or from external code. Supports optional
 * src rect (9-slice / atlas), tint, alpha, and fit modes.
 *
 * Measure: returns texture size or available size depending on fit.
 */
class Image : public SceneNode {
   public:
    static std::shared_ptr<Image> create(std::string id);

    explicit Image(std::string id = "");

    // Texture management
    void setTexture(const std::shared_ptr<ITexture>& texture);
    std::shared_ptr<ITexture> getTexture() const { return texture_; }

    void setImageData(const ImageData& data); // will create texture on next paint if backend available
    void setImageData(ImageData&& data);

    // Optional source rect (in texture pixels) – for atlases
    void setSourceRect(const std::optional<Rect>& src);
    std::optional<Rect> getSourceRect() const { return srcRect_; }

    // Tint and alpha
    void setTint(const std::optional<Color>& tint);
    void setAlpha(float alpha);

    // Fit mode
    void setFit(ImageFit fit);
    ImageFit getFit() const { return fit_; }

    const char* getNodeType() const noexcept override;

   protected:
    Size onMeasure(const Size& availableSize) override;
    void onPaint(PaintContext& pc) override;

   private:
    void ensureTexture(PaintContext& pc);

    std::shared_ptr<ITexture> texture_;
    std::optional<ImageData> pendingData_; // data waiting for backend to create texture
    std::optional<Rect> srcRect_;
    std::optional<Color> tint_;
    float alpha_ = 1.0f;
    ImageFit fit_ = ImageFit::Contain;
};

} // namespace DxvUI

#endif // DXVUI_IMAGE_H
