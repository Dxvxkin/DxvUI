#ifndef DXVUI_H
#define DXVUI_H

// Core
#include "core.h"
#include "Scene.h"
#include "SceneNode.h"
#include "EventManager.h"
#include "Log.h"
#include "UIBinding.h"
#include "Utils.h"
#include "FpsCounter.h"

// Interfaces
#include "interfaces/IRenderer.h"
#include "interfaces/IEventSource.h"
#include "interfaces/ITexture.h"

// Containers
#include "containers/Container.h"
#include "containers/CenterContainer.h"
#include "containers/HorizontalContainer.h"
#include "containers/AbsoluteContainer.h"

// Widgets
#include "widgets/Button.h"
#include "widgets/Label.h"

// Layout
#include "layout/LayoutManager.h"
#include "layout/LayoutData.h"

// Sources
#include "sources/SDLEventSource.h"

// Renderers
#include "renderers/SDLRenderer.h"

// Style
#include "style/Theme.h"
#include "style/StyleManager.h"
#include "style/Style.h"
#include "style/Colors.h"
#include "style/Color.h"

#endif //DXVUI_H
