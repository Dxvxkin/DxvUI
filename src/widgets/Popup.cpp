#include "DxvUI/widgets/Popup.h"

#include <utility>

#include "DxvUI/Scene.h"
#include "DxvUI/style/Colors.h"
#include "DxvUI/style/Theme.h"

namespace DxvUI {

// --- Self-registration of default styles ---
namespace {
// Единый источник имени типа: используется и в getNodeType(), и как ключ
// регистрации стилей, чтобы строка не могла разойтись с типом виджета.
constexpr const char* kWidgetType = "Popup";

struct PopupStyleRegistrar {
    PopupStyleRegistrar() {
        Theme::registerDefaultStyle(kWidgetType, {{WidgetState::Normal,
                                                   {.backgroundColor = Colors::White,
                                                    .borderColor = Colors::LightGray,
                                                    .borderThickness = 1,
                                                    .borderRadius = 4,
                                                    .padding = {{8, 8, 8, 8}}}}});
    }
};

const PopupStyleRegistrar registrar;
}  // namespace

std::shared_ptr<Popup> Popup::create(std::string id) {
    return std::shared_ptr<Popup>(new Popup(std::move(id)));
}

Popup::Popup(std::string id) : AbsoluteContainer(std::move(id)) {
    // Popup-виджет появляется на экране только по явному show()/showAt(); до
    // этого он не участвует в раскладке и хит-тестах (нулевые границы).
    setVisible(false);
}

const char* Popup::getNodeType() const { return kWidgetType; }

void Popup::setPosition(int x, int y) {
    // updateStyle (а не setStyle) мержит left/top в существующее правило:
    // setStyle заменил бы весь StyleRule и стёр бы заданные ранее width/height.
    updateStyle({.left = static_cast<float>(x), .top = static_cast<float>(y)}, WidgetState::Normal);
}

void Popup::show() {
    if (isOpen()) return;
    installDismissListeners();
    setVisible(true);
    onOpen();
}

void Popup::showAt(int x, int y) {
    setPosition(x, y);
    show();
}

void Popup::hide() {
    if (!isOpen()) return;
    onClose();
    setVisible(false);
}

bool Popup::isOpen() const { return isVisible(); }

void Popup::setDismissOnOutsideClick(bool dismiss) {
    if (dismissOnOutsideClick_ == dismiss) return;
    dismissOnOutsideClick_ = dismiss;
    removeDismissListeners();
    // Собираем настройку сразу: открытый попап должен подхватить её, не дожи-
    // даясь повторного show().
    if (isOpen()) {
        installDismissListeners();
    }
}

bool Popup::dismissOnOutsideClick() const { return dismissOnOutsideClick_; }

void Popup::installDismissListeners() {
    if (dismissListenerInstalled_ || !dismissOnOutsideClick_) return;
    auto scene = getScene();
    auto root = scene ? scene->getRoot() : nullptr;
    if (!root) return;

    dismissListenerInstalled_ = true;
    dismissConnection_ =
        root->onCapture(EventType::MouseDown, [this](DxvEvent& event, const UIContext&) {
            if (!isOpen()) return;
            auto target = event.getTarget();
            if (!target || target.get() == this || isAncestorOf(target)) return;
            // Press вне попапа: закрываемся и останавливаем MouseDown в
            // capture-фазе. По контракту движка это отменяет весь жест press,
            // поэтому виджет под попапом (или сама кнопка-переключатель) не
            // остаётся pressed и не получает Click закрывающего клика — попап
            // не «дергается» туда-обратно.
            event.stopPropagation();
            hide();
        });
}

void Popup::removeDismissListeners() {
    dismissListenerInstalled_ = false;
    dismissConnection_.reset();
}

void Popup::onDetach() {
    removeDismissListeners();
    AbsoluteContainer::onDetach();
}

void Popup::onOpen() {}

void Popup::onClose() {}

}  // namespace DxvUI