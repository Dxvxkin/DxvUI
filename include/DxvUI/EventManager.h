#ifndef DXVUI_EVENTMANAGER_H
#define DXVUI_EVENTMANAGER_H

#include <memory>
#include "core.h"

namespace DxvUI {

    class SceneNode;
    class Scene;

    class EventManager {
    public:
        explicit EventManager(Scene& scene);

        void processRawEvent(const DxvEvent& event);

    private:
        void handleMouseMove(DxvEvent& event);
        void handleMouseDown(DxvEvent& event);
        void handleMouseUp(DxvEvent& event);

        Scene& ownerScene;
        std::weak_ptr<SceneNode> nodeUnderMouse;
        std::weak_ptr<SceneNode> pressedNode;
        std::weak_ptr<SceneNode> focusedNode;
        PointI lastMousePosition;
    };

}

#endif //DXVUI_EVENTMANAGER_H
