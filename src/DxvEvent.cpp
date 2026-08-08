#include "DxvUI/DxvEvent.h"

#include "DxvUI/SceneNode.h"

namespace DxvUI {

std::string DxvEvent::getTargetId() const {
    if (auto t = target.lock()) {
        return t->getId();
    }
    return "";
}

}  // namespace DxvUI
