#include "DxvUI/EventManager.h"

#include "DxvUI/Scene.h"
#include "DxvUI/SceneNode.h"
#include "DxvUI/interfaces/IRenderer.h"

namespace DxvUI {

EventManager::EventManager(Scene& scene) : ownerScene(scene) {}

std::shared_ptr<SceneNode> EventManager::getFocusedNode() const { return focusedNode.lock(); }

void EventManager::setFocus(const std::shared_ptr<SceneNode>& node) { changeFocus(node); }

std::shared_ptr<SceneNode> EventManager::hitTest(int x, int y) {
    // The hovered node is a strong hint for a stationary mouse: while the cache
    // is valid (no relayout or hierarchy mutation since it was set), the node is
    // visible, contains the point and no sibling covers its bounds, verifying
    // them short-circuits the O(n) reverse sibling scan that dominates a fresh
    // hit-test. Re-entering the cached node's subtree resolves the deepest
    // visible descendant, exactly like the root scan would.
    if (hitTestCache.valid) {
        if (auto cached = hitTestCache.node.lock()) {
            if (cached->isVisible() && cached->getGlobalBounds().contains(x, y) &&
                !hitTestCache.covered) {
                return cached->findNodeAt(x, y);
            }
        }
    }
    auto root = ownerScene.getRoot();
    auto node = root ? root->findNodeAt(x, y) : nullptr;
    // Fresh scan: rebuild the cache entry. The occlusion flag is a rect-level
    // ancestor walk, so it is computed here once per rebuild, not on every
    // event, keeping a stationary mouse O(1).
    hitTestCache.node = node;
    hitTestCache.valid = true;
    hitTestCache.covered = node && node->hasNodeInFront(node->getGlobalBounds());
    return node;
}

void EventManager::onNodeRemoved(const std::shared_ptr<SceneNode>& node) {
    if (!node) return;

    if (auto hovered = hitTestCache.node.lock()) {
        if (hovered == node || node->isAncestorOf(hovered)) {
            hitTestCache.node.reset();
            hitTestCache.valid = false;
            hovered->setHovered(false);
            DxvEvent e;
            e.type = EventType::HoverLeave;
            e.target = hovered;
            hovered->dispatchEvent(e);
        }
    }

    for (auto it = pressedNodes.begin(); it != pressedNodes.end();) {
        if (auto pressed = it->second.node.lock()) {
            if (pressed == node || node->isAncestorOf(pressed)) {
                pressed->setPressed(false);
                it = pressedNodes.erase(it);
                continue;
            }
        }
        ++it;
    }

    if (auto focused = focusedNode.lock()) {
        if (focused == node || node->isAncestorOf(focused)) {
            focused->setFocused(false);
            focusedNode.reset();
            DxvEvent e;
            e.type = EventType::FocusLost;
            e.target = focused;
            focused->dispatchEvent(e);
        }
    }
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
        for (const auto& [button, record] : pressedNodes) {
            if (auto pressed = record.node.lock()) {
                pressed->setPressed(false);
            }
        }
        pressedNodes.clear();
    }

    auto oldNode = hitTestCache.node.lock();
    auto newNode = hitTest(event.mouse.x, event.mouse.y);
    // hitTest() owns the cache: it either reused a valid entry (which still
    // reflects this point) or rebuilt it from the fresh scan that just ran.

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
            renderer->setCursor(newNode->getComputedAppearance().cursor);
        } else {
            renderer->setCursor(root->getComputedAppearance().cursor);
        }
    }

    for (const auto& [button, record] : pressedNodes) {
        if (auto pressed = record.node.lock()) {
            DxvEvent dragEvent;
            dragEvent.type = EventType::Drag;
            dragEvent.target = pressed;
            dragEvent.mouse = event.mouse;
            pressed->dispatchEvent(dragEvent);
        }
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
    const MouseButton button = event.mouse.button;
    auto targetNode = hitTest(event.mouse.x, event.mouse.y);

    // A press on a different node (or empty space) with the same button replaces
    // a previous press whose button-up was missed, so no widget stays stuck in
    // the Pressed state for that button.
    if (auto existing = pressedNodes.find(button); existing != pressedNodes.end()) {
        if (auto existingNode = existing->second.node.lock();
            existingNode && existingNode != targetNode) {
            existingNode->setPressed(false);
        }
        pressedNodes.erase(existing);
    }

    auto oldFocused = focusedNode.lock();
    if (oldFocused != targetNode) {
        changeFocus(targetNode);
    }

    if (targetNode) {
        // Only apply 'pressed' state and track the node if it's an interactive element.
        if (!targetNode->isRoot()) {
            targetNode->setPressed(true);
            pressedNodes[button] = {targetNode, {event.mouse.x, event.mouse.y}};
        }
        event.target = targetNode;
        targetNode->dispatchEvent(event);
    }
}

void EventManager::handleMouseUp(DxvEvent& event) {
    const MouseButton button = event.mouse.button;
    auto targetNodeOnUp = hitTest(event.mouse.x, event.mouse.y);

    auto pressedIt = pressedNodes.find(button);
    if (pressedIt != pressedNodes.end()) {
        const PressRecord record = pressedIt->second;
        pressedNodes.erase(pressedIt);
        if (auto pressed = record.node.lock()) {
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

            // A click is only reported when the pointer did not leave the press
            // threshold: a drag that ends on the same node is not a click.
            if (targetNodeOnUp == pressed) {
                const int dx = event.mouse.x - record.startPosition.x;
                const int dy = event.mouse.y - record.startPosition.y;
                if (dx * dx + dy * dy <= dragThreshold * dragThreshold) {
                    DxvEvent clickEvent;
                    clickEvent.type = EventType::Click;
                    clickEvent.target = pressed;
                    clickEvent.mouse.x = event.mouse.x;
                    clickEvent.mouse.y = event.mouse.y;
                    clickEvent.mouse.button = event.mouse.button;
                    pressed->dispatchEvent(clickEvent);
                }
            }
        }
    } else if (targetNodeOnUp) {
        event.target = targetNodeOnUp;
        targetNodeOnUp->dispatchEvent(event);
    }
}

void EventManager::changeFocus(const std::shared_ptr<SceneNode>& newNode) {
    if (auto oldFocused = focusedNode.lock(); oldFocused != newNode) {
        if (oldFocused) {
            oldFocused->setFocused(false);
            DxvEvent e;
            e.type = EventType::FocusLost;
            e.target = oldFocused;
            oldFocused->dispatchEvent(e);
        }

        if (newNode) {
            newNode->setFocused(true);
            DxvEvent e;
            e.type = EventType::FocusGained;
            e.target = newNode;
            newNode->dispatchEvent(e);
        }
        focusedNode = newNode;
    }
}

}  // namespace DxvUI
