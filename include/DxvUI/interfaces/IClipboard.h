#ifndef DXVUI_ICLIPBOARD_H
#define DXVUI_ICLIPBOARD_H

#include <string>

namespace DxvUI {

/**
 * @brief Backend-neutral access to the system clipboard.
 *
 * Owned by the renderer (it is the backend-facing object) and reached via
 * IRenderer::getClipboard(), so widgets can copy/paste without knowing which
 * backend provides the clipboard. The interface leaks no backend types.
 */
class IClipboard {
   public:
    virtual ~IClipboard() = default;

    /**
     * @brief Reads the current clipboard text (empty string when unset).
     */
    virtual std::string getText() = 0;

    /**
     * @brief Replaces the clipboard contents with the given text.
     * @return True on success.
     */
    virtual bool setText(const std::string& text) = 0;
};

}  // namespace DxvUI

#endif  // DXVUI_ICLIPBOARD_H
