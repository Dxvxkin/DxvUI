#ifndef DXVUI_LABEL_H
#define DXVUI_LABEL_H

#include "DxvUI/SceneNode.h"
#include "DxvUI/interfaces/ITexture.h"
#include <string>
#include <memory>

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
    Size measure(const Size& availableSize) override;
    void draw(IRenderer& renderer) override;
    // ---------------------

private:
    std::shared_ptr<ITexture> textTexture;
    std::string cachedText;
    std::string cachedFontPath;
    int cachedFontSize = 0;
    Color cachedTextColor;
};

}

#endif //DXVUI_LABEL_H
