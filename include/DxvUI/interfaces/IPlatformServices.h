#ifndef DXVUI_IPLATFORMSERVICES_H
#define DXVUI_IPLATFORMSERVICES_H

#include "DxvUI/core.h"
#include "DxvUI/interfaces/IClipboard.h"

namespace DxvUI {

/**
 * @brief Platform services that are not painting: cursor and clipboard.
 *
 * Stage 5 of rendering refactoring (docs/RENDERING_REFACTORING.md §3.5):
 * EventManager depends on this, not on IRenderBackend, so events no longer pull in painting.
 */
class IPlatformServices {
   public:
    virtual ~IPlatformServices() = default;

    virtual void setCursor(CursorType type) = 0;
    virtual CursorType getCursor() const = 0;
    virtual IClipboard& getClipboard() = 0;
};

}  // namespace DxvUI

#endif  // DXVUI_IPLATFORMSERVICES_H
