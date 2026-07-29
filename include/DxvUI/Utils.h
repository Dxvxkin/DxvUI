#ifndef DXVUI_UTILS_H
#define DXVUI_UTILS_H

#include <string>
#include "DxvUI/SceneNode.h"

namespace DxvUI {

/**
 * @brief Creates an indentation string based on the depth of a SceneNode.
 * @param node A pointer to the SceneNode. If nullptr, an empty string is returned.
 * @param prefix The string to use for each level of indentation. Defaults to "  ".
 * @return A string representing the total indentation.
 *
 * @complexity O(D * P) where D is the depth of the node and P is the length of the prefix.
 *             This is due to repeated string appends.
 * @exceptionGuarantee No-throw guarantee.
 */
inline std::string indent(const SceneNode* node, const std::string& prefix = "  ") noexcept {
    if (!node) {
        return "";
    }
    std::string result;
    const auto depth = node->getDepth();
    if (depth > 0) {
        result.reserve(depth * prefix.length());
        for (std::size_t i = 0; i < depth; ++i) {
            result += prefix;
        }
    }
    return result;
}

} // namespace DxvUI

#endif // DXVUI_UTILS_H
