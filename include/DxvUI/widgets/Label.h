#ifndef DXVUI_LABEL_H
#define DXVUI_LABEL_H

#include <memory>
#include <string>

#include "DxvUI/SceneNode.h"
#include "DxvUI/interfaces/ITexture.h"

namespace DxvUI {

class Label : public SceneNode {
   public:
    static std::shared_ptr<Label> create(std::string id, std::string text = "");

    explicit Label(std::string id = "", std::string text = "");

    void setText(std::string text);
    const std::string getText() const;

    // --- Overrides ---
    void onChange(const UIBinding& val) override;

    const char* getNodeType() const noexcept override;
    void draw(IRenderer& renderer) override;
    // ---------------------

   protected:
    Size onMeasure(const Size& availableSize) override;

   private:
    std::shared_ptr<ITexture> textTexture;
    std::string cachedText;
    // The computed appearance the current texture was rasterized with.
    ComputedAppearanceStyle cachedAppearance;
};

}  // namespace DxvUI

#endif  // DXVUI_LABEL_H
