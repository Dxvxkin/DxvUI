#include "DxvUI/event/EventTarget.h"

#include <utility>
#include <vector>

namespace DxvUI {

EventTarget::handlerID EventTarget::addHandler(EventType type, ActionCallback callback) {
    const handlerID id = handlerIdCounter++;
    eventHandlers[type][id] = std::move(callback);
    return id;
}

EventTarget::handlerID EventTarget::addCaptureHandler(EventType type, ActionCallback callback) {
    if (!captureHandlers) {
        captureHandlers =
            std::make_unique<std::map<EventType, std::map<handlerID, ActionCallback>>>();
    }
    const handlerID id = handlerIdCounter++;
    (*captureHandlers)[type][id] = std::move(callback);
    return id;
}

void EventTarget::removeHandler(EventType type, handlerID id) {
    if (auto it = eventHandlers.find(type); it != eventHandlers.end()) {
        it->second.erase(id);
        if (it->second.empty()) {
            eventHandlers.erase(it);
        }
    }
}

void EventTarget::removeCaptureHandler(EventType type, handlerID id) {
    if (!captureHandlers) {
        return;
    }
    if (auto it = captureHandlers->find(type); it != captureHandlers->end()) {
        it->second.erase(id);
        if (it->second.empty()) {
            captureHandlers->erase(it);
        }
    }
    if (captureHandlers->empty()) {
        captureHandlers.reset();
    }
}

bool EventTarget::hasHandler(EventType type) const {
    return eventHandlers.find(type) != eventHandlers.end();
}

bool EventTarget::hasAnyCaptureHandlers() const { return captureHandlers != nullptr; }

bool EventTarget::hasCaptureHandler(EventType type) const {
    if (!captureHandlers) {
        return false;
    }
    return captureHandlers->find(type) != captureHandlers->end();
}

void EventTarget::runHandlers(EventType type, DxvEvent& event, const UIContext& context) {
    auto it = eventHandlers.find(type);
    if (it != eventHandlers.end()) {
        runListeners(it->second, event, context);
    }
}

void EventTarget::runCaptureHandlers(EventType type, DxvEvent& event, const UIContext& context) {
    if (!captureHandlers) {
        return;
    }
    if (auto it = captureHandlers->find(type); it != captureHandlers->end()) {
        runListeners(it->second, event, context);
    }
}

void EventTarget::runListeners(std::map<handlerID, ActionCallback>& handlers, DxvEvent& event,
                               const UIContext& context) {
    // Fast path: a single listener (the overwhelmingly common case in key
    // paths) runs without the ids snapshot, avoiding the vector allocation.
    // The snapshot exists because a handler may remove itself (or register new
    // ones) while running, which invalidates std::map iterators.
    if (handlers.size() == 1) {
        auto& [id, callback] = *handlers.begin();
        if (callback) {
            callback(event, context);
        }
        return;
    }

    // Snapshot only the handler ids, not the callbacks: std::map iterators stay
    // valid across insert/erase, but a handler may remove itself (or register
    // new ones) while running, so a live iteration is unsafe and a full copy of
    // the std::functions would allocate per dispatched event.
    std::vector<handlerID> ids;
    ids.reserve(handlers.size());
    for (const auto& [id, callback] : handlers) {
        ids.push_back(id);
    }
    for (const handlerID id : ids) {
        const auto callbackIt = handlers.find(id);
        if (callbackIt != handlers.end() && callbackIt->second) {
            callbackIt->second(event, context);
            // stopImmediatePropagation skips the remaining listeners of the
            // current node, but (DOM semantics) not the default action.
            if (event.isImmediatePropagationStopped()) {
                break;
            }
        }
    }
}

}  // namespace DxvUI