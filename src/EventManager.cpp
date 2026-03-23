#include "DxvUI/EventManager.h"
#include "DxvUI/Scene.h"
#include "DxvUI/SceneNode.h"
#include "DxvUI/interfaces/IRenderer.h"

namespace DxvUI {

    EventManager::EventManager(Scene& scene) : ownerScene(scene) {}

    void EventManager::processRawEvent(const DxvEvent& rawEvent) {
        DxvEvent event = rawEvent;
        auto root = ownerScene.getRoot();
        if (!root) return;

        switch (event.type) {
            case EventType::MouseMove:
                handleMouseMove(event);
                break;
            case EventType::MouseDown:
                handleMouseDown(event);
                break;
            case EventType::MouseUp:
                handleMouseUp(event);
                break;
            case EventType::TextInput:
            case EventType::KeyDown:
            case EventType::KeyUp: {
                std::shared_ptr<SceneNode> target = focusedNode.lock();
                if (!target) {
                    target = root;
                }
                event.target = target;
                target->dispatchEvent(event);
                break;
            }
            case EventType::Quit:
                event.target = root;
                root->dispatchEvent(event);
                break;
            default:
                // For unhandled event types, do nothing.
                break;
        }
    }

    void EventManager::handleMouseMove(DxvEvent& event) {
        auto root = ownerScene.getRoot();
        if (!root) return;

        event.mouse.dx = event.mouse.x - lastMousePosition.x;
        event.mouse.dy = event.mouse.y - lastMousePosition.y;
        lastMousePosition = { event.mouse.x, event.mouse.y };

        auto oldNode = nodeUnderMouse.lock();
        auto newNode = root->findNodeAt(event.mouse.x, event.mouse.y);

        if (oldNode != newNode) {
            if (oldNode && !oldNode->isRoot()) {
                oldNode->setHovered(false);
                DxvEvent e;
                e.type = EventType::HoverLeave;
                e.target = oldNode;
                oldNode->dispatchEvent(e);
            }
            if (newNode && !newNode->isRoot()) {
                newNode->setHovered(true);
                DxvEvent e;
                e.type = EventType::HoverEnter;
                e.target = newNode;
                newNode->dispatchEvent(e);
            }
            nodeUnderMouse = newNode;
        }

        if (auto renderer = ownerScene.getRenderer()) {
            if (newNode) {
                renderer->setCursor(newNode->getComputedAppearance(newNode->getCurrentState()).cursor);
            } else {
                renderer->setCursor(root->getComputedAppearance(root->getCurrentState()).cursor);
            }
        }

        if (auto pressed = pressedNode.lock()) {
            DxvEvent dragEvent;
            dragEvent.type = EventType::Drag;
            dragEvent.target = pressed;
            dragEvent.mouse = event.mouse;
            pressed->dispatchEvent(dragEvent);
        }

        if (newNode) {
            event.target = newNode;
            newNode->dispatchEvent(event);
        }
    }

    void EventManager::handleMouseDown(DxvEvent& event) {
        auto root = ownerScene.getRoot();
        if (!root) return;

        lastMousePosition = { event.mouse.x, event.mouse.y };
        auto targetNode = root->findNodeAt(event.mouse.x, event.mouse.y);

        auto oldFocused = focusedNode.lock();
        if (oldFocused != targetNode) {
            if (oldFocused) {
                DxvEvent e;
                e.type = EventType::FocusLost;
                e.target = oldFocused;
                oldFocused->dispatchEvent(e);
            }
            if (targetNode) {
                DxvEvent e;
                e.type = EventType::FocusGained;
                e.target = targetNode;
                targetNode->dispatchEvent(e);
            }
            focusedNode = targetNode;
        }

        if (targetNode) {
            // Only apply 'pressed' state and track the node if it's an interactive element.
            if (!targetNode->isRoot()) {
                targetNode->setPressed(true);
                pressedNode = targetNode;
            }
            event.target = targetNode;
            targetNode->dispatchEvent(event);
        }
    }

    void EventManager::handleMouseUp(DxvEvent& event) {
        auto pressed = pressedNode.lock();
        auto targetNodeOnUp = ownerScene.getRoot() ? ownerScene.getRoot()->findNodeAt(event.mouse.x, event.mouse.y) : nullptr;

        if (pressed) {
            pressed->setPressed(false);
            event.target = pressed;
            pressed->dispatchEvent(event);

            if (targetNodeOnUp && targetNodeOnUp.get() != pressed.get()) {
                DxvEvent dropEvent;
                dropEvent.type = EventType::Drop;
                dropEvent.target = targetNodeOnUp;
                dropEvent.relatedNode = pressed;
                dropEvent.mouse.x = event.mouse.x;
                dropEvent.mouse.y = event.mouse.y;
                targetNodeOnUp->dispatchEvent(dropEvent);
            }

            if (targetNodeOnUp == pressed) {
                DxvEvent clickEvent;
                clickEvent.type = EventType::Click;
                clickEvent.target = pressed;
                clickEvent.mouse.x = event.mouse.x;
                clickEvent.mouse.y = event.mouse.y;
                clickEvent.mouse.button = event.mouse.button;
                pressed->dispatchEvent(clickEvent);
            }
        } else if (targetNodeOnUp) {
            event.target = targetNodeOnUp;
            targetNodeOnUp->dispatchEvent(event);
        }

        pressedNode.reset();
    }

}
