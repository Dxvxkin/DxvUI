#ifndef DXVUI_SDLTEXTEDITORVIEW_H
#define DXVUI_SDLTEXTEDITORVIEW_H

// Backward compatibility shim: SDLTextEditorView moved to text/DefaultTextEditorView.
// The old backend location is kept as an alias so existing includes still work.
// New code should include DxvUI/text/DefaultTextEditorView.h.

#include "DxvUI/text/DefaultTextEditorView.h"

namespace DxvUI {

using SDLTextEditorView = DefaultTextEditorView;

}  // namespace DxvUI

#endif  // DXVUI_SDLTEXTEDITORVIEW_H
