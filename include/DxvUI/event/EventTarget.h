#ifndef DXVUI_EVENTTARGET_H
#define DXVUI_EVENTTARGET_H

#include <cstdint>
#include <functional>
#include <map>
#include <memory>

#include "DxvEvent.h"

namespace DxvUI {

/**
 * @brief Per-node storage and invalidation of event listeners.
 *
 * A pure holder for the regular and capture-phase listener sets of a node
 * (mirrors how NodeState holds the node's flags): it owns the maps, the handler
 * id counter, registration/removal and the snapshot iteration, and knows
 * nothing about the SceneNode, the scene or the dispatch contract. The owner is
 * responsible for the phase bookkeeping (current target, phase, UIContext) and
 * the default action; it reads hasHandler()/hasCaptureHandler() to skip the
 * per-event work on nodes that hold no listeners for the dispatched type.
 */
class EventTarget {
   public:
    using handlerID = uint64_t;

    /**
     * @brief Registers a regular (target/bubble-phase) listener.
     * @param type The event type to listen for.
     * @param callback The callback to run.
     * @return The id of the registered handler.
     */
    handlerID addHandler(EventType type, ActionCallback callback);

    /**
     * @brief Registers a capture-phase listener.
     * @param type The event type to listen for.
     * @param callback The callback to run.
     * @return The id of the registered handler.
     */
    handlerID addCaptureHandler(EventType type, ActionCallback callback);

    /**
     * @brief Removes a regular listener. No-op when already gone.
     */
    void removeHandler(EventType type, handlerID id);

    /**
     * @brief Removes a capture listener. No-op when already gone.
     */
    void removeCaptureHandler(EventType type, handlerID id);

    /**
     * @brief Whether a regular listener exists for the given type.
     *
     * Used as the bubble-phase fast path: a node with no regular listeners for
     * the type skips all per-node bookkeeping.
     */
    bool hasHandler(EventType type) const;

    /**
     * @brief Whether any capture listener exists at all.
     *
     * The capture fast path: the captureHandlers map is lazily allocated, so
     * nodes that never registered a capture listener (the vast majority) bail
     * out before any per-node bookkeeping.
     */
    bool hasAnyCaptureHandlers() const;

    /**
     * @brief Whether a capture listener exists for the given type.
     *
     * Second-level check used to keep the UIContext construction lazy: it is
     * only built when a capture listener for exactly this type exists.
     */
    bool hasCaptureHandler(EventType type) const;

    /**
     * @brief Runs the regular listeners registered for the given type.
     */
    void runHandlers(EventType type, DxvEvent& event, const UIContext& context);

    /**
     * @brief Runs the capture listeners registered for the given type.
     */
    void runCaptureHandlers(EventType type, DxvEvent& event, const UIContext& context);

   private:
    // Iterates the id-sorted map, snapshotting the ids so registration/removal
    // from within a handler is safe; a single listener runs without the
    // snapshot allocation. stopImmediatePropagation skips the remaining
    // listeners of the current node (DOM semantics).
    static void runListeners(std::map<handlerID, ActionCallback>& handlers, DxvEvent& event,
                             const UIContext& context);

    std::map<EventType, std::map<handlerID, ActionCallback>> eventHandlers;
    // Capture-phase listeners, registered through addCaptureHandler().
    // Lazily allocated: only nodes that actually hold capture listeners pay for
    // the map, keeping the footprint of the common (capture-less) case at a
    // pointer.
    std::unique_ptr<std::map<EventType, std::map<handlerID, ActionCallback>>> captureHandlers;
    handlerID handlerIdCounter = 0;
};

}  // namespace DxvUI

#endif  // DXVUI_EVENTTARGET_H