#include "DxvUI/EventManager.h"

#include "DxvUI/Scene.h"
#include "DxvUI/SceneNode.h"
#include "DxvUI/interfaces/IRenderer.h"

namespace DxvUI {

EventManager::EventManager(Scene& scene) : ownerScene(scene) {}

std::shared_ptr<SceneNode> EventManager::hitTest(int x, int y) {
    // The hovered node is a strong hint for a stationary mouse: while the cache
    // is valid (no relayout or hierarchy mutation since it was set), verifying
    // the node is still visible and contains the point short-circuits the O(n)
    // reverse sibling scan that dominates a fresh hit-test. Re-entering the
    // cached node's subtree resolves the deepest visible descendant, exactly
    // like the root scan would.
    if (nodeUnderMouseValid) {
        if (auto cached = nodeUnderMouse.lock()) {
            if (cached->isVisible() && cached->getGlobalBounds().contains(x, y)) {
                return cached->findNodeAt(x, y);
            }
        }
    }
    auto root = ownerScene.getRoot();
    return root ? root->findNodeAt(x, y) : nullptr;
}

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
    lastMousePosition = {event.mouse.x, event.mouse.y};

    // Defense-in-depth: a button-up can be missed entirely (e.g. released
    // outside the window by a non-SDL event source). If the mouse moves with no
    // button held, any tracked press must be cleared so widgets never stay
    // stuck in the Pressed state.
    if (event.mouse.button == MouseButton::None) {
        if (auto pressed = pressedNode.lock()) {
            pressed->setPressed(false);
            pressedNode.reset();
        }
    }

    auto oldNode = nodeUnderMouse.lock();
    auto newNode = hitTest(event.mouse.x, event.mouse.y);
    // The cache now reflects a fresh hit-test (either it was valid and reused,
    // or a full scan just ran), so it is trustworthy for the next event.
    nodeUnderMouse = newNode;
    nodeUnderMouseValid = true;

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

    lastMousePosition = {event.mouse.x, event.mouse.y};
    auto targetNode = hitTest(event.mouse.x, event.mouse.y);

    auto oldFocused = focusedNode.lock();
    if (oldFocused != targetNode) {
        if (oldFocused) {
            DxvEvent e;
            e.type = EventType::FocusLost;
            e.target = oldFocused;
            oldFocused->dispatchEvent(e);
        }
        // Pressing a different node (or empty space) cancels a previous press whose
        // button-up was missed, so no widget stays stuck in the Pressed state.
        auto existingPressed = pressedNode.lock();
        if (existingPressed && existingPressed != targetNode) {
            existingPressed->setPressed(false);
            pressedNode.reset();
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
    auto targetNodeOnUp = hitTest(event.mouse.x, event.mouse.y);

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

}  // namespace DxvUI
