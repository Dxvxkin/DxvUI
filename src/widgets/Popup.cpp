#include "DxvUI/widgets/Popup.h"

#include <utility>

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

void Popup::onOpen() {}

void Popup::onClose() {}

}  // namespace DxvUI