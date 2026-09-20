#include "DxvUI/widgets/Image.h"

#include "DxvUI/Log.h"
#include "DxvUI/Scene.h"
#include "DxvUI/interfaces/IRenderBackend.h"
#include "DxvUI/style/Colors.h"
#include "DxvUI/style/Theme.h"

namespace DxvUI {

// --- Self-registration of default styles ---
namespace {
constexpr const char* kWidgetType = "Image";

struct ImageStyleRegistrar {
    ImageStyleRegistrar() {
        Theme::registerDefaultStyle(
            kWidgetType, {{WidgetState::Normal, {.backgroundColor = Colors::Transparent}}});
    }
};

const ImageStyleRegistrar registrar;
}  // namespace

std::shared_ptr<Image> Image::create(std::string id) {
    return std::make_shared<Image>(std::move(id));
}

Image::Image(std::string id) : SceneNode(std::move(id)) {}

const char* Image::getNodeType() const noexcept { return kWidgetType; }

void Image::setTexture(const std::shared_ptr<ITexture>& texture) {
    texture_ = texture;
    pendingData_.reset();
    markLayoutDirty();
}

void Image::setImageData(const ImageData& data) {
    pendingData_ = data;
    texture_.reset();
    markLayoutDirty();
}

void Image::setImageData(ImageData&& data) {
    pendingData_ = std::move(data);
    texture_.reset();
    markLayoutDirty();
}

void Image::setSourceRect(const std::optional<Rect>& src) {
    srcRect_ = src;
    markLayoutDirty();
}

void Image::setTint(const std::optional<Color>& tint) {
    tint_ = tint;
    markStyleDirty();
}

void Image::setAlpha(float alpha) {
    alpha_ = std::clamp(alpha, 0.0f, 1.0f);
    markStyleDirty();
}

void Image::setFit(ImageFit fit) {
    if (fit_ != fit) {
        fit_ = fit;
        markLayoutDirty();
    }
}

Size Image::onMeasure(const Size& availableSize) {
    // If we have texture, use its size
    int texW = 0, texH = 0;
    if (texture_) {
        texW = texture_->getWidth();
        texH = texture_->getHeight();
    } else if (pendingData_ && pendingData_->isValid()) {
        texW = pendingData_->width;
        texH = pendingData_->height;
    }

    if (texW == 0 || texH == 0) {
        // No texture yet – use 0 or available
        return {0, 0};
    }

    // If style has explicit width/height, respect it via computed layout?
    // For simplicity, return texture size clamped to available
    float w = static_cast<float>(texW);
    float h = static_cast<float>(texH);

    if (availableSize.width > 0 && w > availableSize.width) {
        // For Contain/ScaleDown, shrink
        if (fit_ == ImageFit::Contain || fit_ == ImageFit::ScaleDown) {
            float ratio = availableSize.width / w;
            w = availableSize.width;
            h *= ratio;
        }
    }
    if (availableSize.height > 0 && h > availableSize.height) {
        if (fit_ == ImageFit::Contain || fit_ == ImageFit::ScaleDown) {
            float ratio = availableSize.height / h;
            h = availableSize.height;
            w *= ratio;
        }
    }

    return {w, h};
}

void Image::ensureTexture(PaintContext& pc) {
    if (texture_ || !pendingData_) return;
    (void)pc;
    // Texture creation needs the render backend. Scene::setRenderer keeps the
    // renderBackend pointer in sync (IRenderer derives from IRenderBackend), so
    // getRenderBackend() covers legacy hosts too — no RTTI needed here.
    IRenderBackend* backend = getScene() ? getScene()->getRenderBackend() : nullptr;
    if (!backend) return;
    if (!pendingData_->isValid()) {
        Log::warn("Image::ensureTexture: invalid ImageData");
        pendingData_.reset();
        return;
    }
    texture_ = backend->createTexture(*pendingData_);
    if (!texture_) {
        Log::error("Image::ensureTexture: backend failed to create texture");
    }
    pendingData_.reset();
}

void Image::onPaint(PaintContext& pc) {
    ensureTexture(pc);

    if (!texture_) return;

    RectF bounds = {
        static_cast<float>(getGlobalBounds().x), static_cast<float>(getGlobalBounds().y),
        static_cast<float>(getGlobalBounds().width), static_cast<float>(getGlobalBounds().height)};

    if (bounds.width <= 0 || bounds.height <= 0) return;

    int texW = texture_->getWidth();
    int texH = texture_->getHeight();
    if (texW <= 0 || texH <= 0) return;

    // Compute destination rect based on fit
    RectF dst = bounds;
    float texAspect = static_cast<float>(texW) / static_cast<float>(texH);
    float boundsAspect = bounds.width / bounds.height;

    switch (fit_) {
        case ImageFit::None: {
            dst.width = static_cast<float>(texW);
            dst.height = static_cast<float>(texH);
            break;
        }
        case ImageFit::Contain: {
            if (texAspect > boundsAspect) {
                // Width constrained
                dst.width = bounds.width;
                dst.height = bounds.width / texAspect;
                dst.y += (bounds.height - dst.height) * 0.5f;
            } else {
                dst.height = bounds.height;
                dst.width = bounds.height * texAspect;
                dst.x += (bounds.width - dst.width) * 0.5f;
            }
            break;
        }
        case ImageFit::Cover: {
            if (texAspect > boundsAspect) {
                dst.height = bounds.height;
                dst.width = bounds.height * texAspect;
                dst.x += (bounds.width - dst.width) * 0.5f;
            } else {
                dst.width = bounds.width;
                dst.height = bounds.width / texAspect;
                dst.y += (bounds.height - dst.height) * 0.5f;
            }
            break;
        }
        case ImageFit::Fill: {
            // Already bounds
            break;
        }
        case ImageFit::ScaleDown: {
            if (texW <= bounds.width && texH <= bounds.height) {
                dst.width = static_cast<float>(texW);
                dst.height = static_cast<float>(texH);
                dst.x += (bounds.width - dst.width) * 0.5f;
                dst.y += (bounds.height - dst.height) * 0.5f;
            } else {
                // Same as Contain
                if (texAspect > boundsAspect) {
                    dst.width = bounds.width;
                    dst.height = bounds.width / texAspect;
                    dst.y += (bounds.height - dst.height) * 0.5f;
                } else {
                    dst.height = bounds.height;
                    dst.width = bounds.height * texAspect;
                    dst.x += (bounds.width - dst.width) * 0.5f;
                }
            }
            break;
        }
    }

    // Never paint outside the widget's own bounds. A natural-size (None) draw or
    // an off-centre Cover crop can exceed the box; clip such draws instead of
    // shrinking dst (that would scale src to fit and silently change the mode).
    const float maxRight = bounds.x + bounds.width;
    const float maxBottom = bounds.y + bounds.height;
    const bool needsClip = dst.x < bounds.x || dst.y < bounds.y || dst.x + dst.width > maxRight ||
                           dst.y + dst.height > maxBottom;
    ClipGuard imageClip(pc.canvas(), getGlobalBounds(), needsClip);

    ICanvas::TextureDraw draw;
    draw.dst = dst;
    if (srcRect_) {
        draw.src = RectF{static_cast<float>(srcRect_->x), static_cast<float>(srcRect_->y),
                         static_cast<float>(srcRect_->width), static_cast<float>(srcRect_->height)};
    }
    draw.tint = tint_;
    draw.alpha = alpha_;

    pc.canvas().drawTexture(texture_, draw);
}

}  // namespace DxvUI
