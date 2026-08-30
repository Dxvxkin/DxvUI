#ifndef DXVUI_LABEL_H
#define DXVUI_LABEL_H

#include <memory>
#include <string>

#include "DxvUI/SceneNode.h"

namespace DxvUI {

class Label : public SceneNode {
   public:
    static std::shared_ptr<Label> create(std::string id, std::string text = "");

    explicit Label(std::string id = "", std::string text = "");

    void setText(std::string text);
    std::string getText() const;

    // --- Overrides ---
    void onChange(const UIBinding& binding) override;

    const char* getNodeType() const noexcept override;
    // ---------------------

   protected:
    Size onMeasure(const Size& availableSize) override;
    void drawContent(IRenderer& renderer) override;
};

}  // namespace DxvUI

#endif  // DXVUI_LABEL_H
