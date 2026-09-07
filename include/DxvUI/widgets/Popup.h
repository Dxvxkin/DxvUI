#ifndef DXVUI_POPUP_H
#define DXVUI_POPUP_H

#include <memory>

#include "DxvUI/containers/AbsoluteContainer.h"

namespace DxvUI {

/**
 * @class Popup
 * @brief Floating content panel — the base widget for popups, menus and dialogs.
 *
 * A Popup is a styled AbsoluteContainer that is hidden until explicitly shown:
 * it inherits absolute child positioning (children are placed by their
 * 'left'/'top'/'right'/'bottom' styles, like in the scene root) and sizes
 * itself to its content unless an explicit width/height is given. Content is
 * added with addChild() (directly or via a nested container).
 *
 * Lifecycle is show()/showAt()/hide(); a freshly created popup is closed.
 * Positioning via setPosition()/showAt() writes the 'left'/'top' style and is
 * relative to the popup's parent — for a popup added to the scene root those
 * are screen coordinates.
 *
 * While a popup is open it closes itself when a press lands outside it
 * (dismiss-on-outside-click, on by default; see setDismissOnOutsideClick()).
 * Dismissal uses a capture-phase listener on the scene root: an outside
 * MouseDown is stopped during the Capture phase, which also cancels the press
 * gesture (see the EventManager contract), so the widget under the popup — or
 * the very button that opened it — is neither left pressed nor activated by
 * the dismissing click. A press on the popup itself or on any descendant keeps
 * it open. The feature needs the popup to be attached to a scene; a click on a
 * disabled widget does not dismiss (a disabled node receives no press events at
 * all).
 *
 * A popup drawn as the last sibling (or with a higher setZIndex()) appears on
 * top of earlier content. Customize the look through the theme:
 * scene->getTheme().setDefaultStyle("Popup", {...}) or instance styles.
 *
 * Subclasses can hook the open/close lifecycle by overriding onOpen()/onClose()
 * (e.g. a future modal dialog shows a backdrop on open). The base keeps no
 * modality, shadow or OS-window decoration logic (no title bar, dragging or
 * resizing) — those are variations built on top of it.
 */
class Popup : public AbsoluteContainer {
   public:
    static std::shared_ptr<Popup> create(std::string id);

    /**
     * @brief Moves the popup by setting its absolute 'left'/'top' style.
     * @param x The new left coordinate (relative to the parent's content area).
     * @param y The new top coordinate (relative to the parent's content area).
     */
    void setPosition(int x, int y);

    /**
     * @brief Shows the popup at its current position.
     *
     * No-op when the popup is already open; onOpen() runs once per open.
     */
    void show();

    /**
     * @brief Positions the popup and shows it.
     * @param x The new left coordinate.
     * @param y The new top coordinate.
     */
    void showAt(int x, int y);

    /**
     * @brief Hides the popup.
     *
     * No-op when the popup is already closed; onClose() runs once per close.
     */
    void hide();

    /**
     * @brief Whether the popup is currently shown.
     */
    bool isOpen() const;

    /**
     * @brief Enables or disables dismissing the popup by a press outside it.
     *
     * On by default; when disabled the popup stays open until hide() is called
     * explicitly (useful for dialog-like panels). No-op only inside the flag:
     * an open popup picks the setting up immediately.
     * @param dismiss Whether an outside press should close the popup.
     */
    void setDismissOnOutsideClick(bool dismiss);

    /**
     * @brief Whether an outside press closes the popup.
     */
    bool dismissOnOutsideClick() const;

    // --- Overrides ---
    const char* getNodeType() const override;
    void onDetach() override;
    // ---------------------

   protected:
    explicit Popup(std::string id);

    /**
     * @brief Lifecycle hook: called once when the popup is shown.
     *
     * No-op by default. Runs after the popup became visible, so overrides can
     * read the fresh layout state or mutate the tree.
     */
    virtual void onOpen();

    /**
     * @brief Lifecycle hook: called once when the popup is hidden.
     *
     * No-op by default. Runs before the popup becomes invisible, so overrides
     * still see the old layout state.
     */
    virtual void onClose();

   private:
    /**
     * @brief Hooks a capture-phase MouseDown listener on the scene root.
     *
     * Idempotent and a no-op when the popup is not attached to a scene yet or
     * dismissal is disabled. Called from show(); the listener is kept for the
     * popup's attachment lifetime (it no-ops while the popup is closed) and
     * removed on detach, so a hidden popup never delivers stray presses.
     */
    void installDismissListeners();

    /**
     * @brief Removes the dismiss listener, if installed.
     */
    void removeDismissListeners();

    bool dismissOnOutsideClick_ = true;
    bool dismissListenerInstalled_ = false;
    // Kept for the popup's attachment lifetime; destroyed on detach so the root
    // never holds a capture listener bound to a detached node.
    std::unique_ptr<SceneNode::Connection> dismissConnection_;
};

}  // namespace DxvUI

#endif  // DXVUI_POPUP_H