#ifndef DXVUI_LAYOUTDATA_H
#define DXVUI_LAYOUTDATA_H

#include "DxvUI/core.h"

namespace DxvUI {

/**
 * @brief Per-node layout state produced by the two-pass measure/arrange cycle.
 *
 * Owned by SceneNode and maintained exclusively by LayoutManager. Widgets and
 * containers only observe it through the public getters (getDesiredSize(),
 * getGlobalBounds()).
 */
struct LayoutData {
    Size desiredSize;             //!< Result of the last measure pass.
    Rect bounds;                  //!< Final rect assigned by the last arrange pass.
    Rect lastArrangeRect;         //!< Final rect of the previous arrange pass (for containers).
    Size lastMeasureConstraints;  //!< Constraints of the last measure pass.
    bool isDirty = true;          //!< This node needs a fresh measure + arrange.
    bool isSubtreeDirty = true;   //!< This node or a descendant needs a fresh layout.
};

}  // namespace DxvUI

#endif  // DXVUI_LAYOUTDATA_H
