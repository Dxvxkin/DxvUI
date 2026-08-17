#ifndef DXVUI_H
#define DXVUI_H

// Core
#include "DxvEvent.h"
#include "EventManager.h"
#include "FpsCounter.h"
#include "Log.h"
#include "Scene.h"
#include "SceneNode.h"
#include "UIBinding.h"
#include "UIContext.h"
#include "Utils.h"
#include "core.h"

// Interfaces
#include "interfaces/IClipboard.h"
#include "interfaces/IEventSource.h"
#include "interfaces/IRenderer.h"
#include "interfaces/ITexture.h"

// Text
#include "text/ITextEngine.h"
#include "text/TextEditor.h"
#include "text/TextEditorView.h"

// Containers
#include "containers/AbsoluteContainer.h"
#include "containers/CenterContainer.h"
#include "containers/Container.h"
#include "containers/HorizontalContainer.h"

// Widgets
#include "widgets/Button.h"
#include "widgets/Checkbox.h"
#include "widgets/Label.h"
#include "widgets/Popup.h"
#include "widgets/TextEdit.h"

// Layout
#include "layout/LayoutData.h"
#include "layout/LayoutManager.h"

// Sources
#include "sources/SDLClipboard.h"
#include "sources/SDLEventSource.h"

// Renderers
#include "renderers/SDLRenderer.h"
#include "renderers/SDLTextEditorView.h"
#include "renderers/SDLTextEngine.h"

// Style
#include "style/Color.h"
#include "style/Colors.h"
#include "style/Style.h"
#include "style/StyleManager.h"
#include "style/Theme.h"

#endif  // DXVUI_H
