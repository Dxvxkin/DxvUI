#ifndef DXVUI_POPUP_H
#define DXVUI_POPUP_H

#include <memory>
#include <string>

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
 * A popup drawn as the last sibling (or with a higher setZIndex()) appears on
 * top of earlier content. Customize the look through the theme:
 * scene->getTheme().setDefaultStyle("Popup", {...}) or instance styles.
 *
 * Subclasses can hook the open/close lifecycle by overriding onOpen()/onClose()
 * (e.g. a future modal dialog shows a backdrop on open). The base keeps no
 * modality, shadow, dismissal or OS-window decoration logic (no title bar,
 * dragging or resizing) — those are variations built on top of it.
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

    // --- Overrides ---
    const char* getNodeType() const override;
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
};

}  // namespace DxvUI

#endif  // DXVUI_POPUP_H