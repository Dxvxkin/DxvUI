// examples/textedit.cpp
//
// TextEdit demo: editable field with placeholder, live echo, Enter-to-submit,
// undo/redo and a clear button. It subclasses DxvUIEx::SdlApp and owns the DxvUI
// integration itself (see examples/main.cpp for the pattern).

#include <DxvUI/DxvEvent.h>
#include <DxvUI/FpsCounter.h>
#include <DxvUI/Log.h>
#include <DxvUI/Scene.h>
#include <DxvUI/backend/SDLEventSource.h>
#include <DxvUI/backend/SDLRenderer.h>
#include <DxvUI/style/Colors.h>
#include <DxvUI/style/Style.h>
#include <DxvUI/text/ITextValidator.h>
#include <DxvUI/widgets/Button.h>
#include <DxvUI/widgets/Checkbox.h>
#include <DxvUI/widgets/Label.h>
#include <DxvUI/widgets/TextEdit.h>
#include <SDL.h>

#include <cstdlib>
#include <format>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "App.h"

namespace {

constexpr int SCREEN_WIDTH = 800;
constexpr int SCREEN_HEIGHT = 600;

}  // namespace

class DxvUITextEditExample : public DxvUIEx::SdlApp {
   public:
    DxvUITextEditExample()
        : DxvUIEx::SdlApp("DxvUI TextEdit Example", SCREEN_WIDTH, SCREEN_HEIGHT) {}

   protected:
    bool init() override {
        dxvRenderer_ = std::make_unique<DxvUI::SDLRenderer>(renderer_);
        scene_ = DxvUI::Scene::create();
        scene_->setRenderer(dxvRenderer_.get());

        buildTextEditDemoUI(scene_->getRoot());
        scene_->updateLayout();

        // Включаем текстовый ввод, иначе SDL не будет слать SDL_TEXTINPUT.
        SDL_StartTextInput();

        // Headless-режим (запускается с DXVUI_FRAMES): прогон синтетических событий.
        if (std::getenv("DXVUI_FRAMES")) {
            runScriptedTextEdit();
            return false;
        }
        return true;
    }

    void update(float /*dtMs*/) override {
        scene_->update();

        fps_.tick();
        fpsLabel_->setText(
            std::format("FPS: {:.0f} ({:.1f} ms)", fps_.getFps(), fps_.getFrameTimeMs()));

        // Мёртвых токенов нет — поле и кнопка живы.
        std::erase_if(connections_, [](const auto& c) { return c->expired(); });
    }

    void draw() override { scene_->draw(); }

    bool handleEvent(const SDL_Event& event) override {
        DxvUI::DxvEvent dxv;
        if (!eventSource_.processEvent(event, dxv)) return false;
        if (dxv.type == DxvUI::EventType::Quit) return true;
        scene_->processEvent(dxv);
        return false;
    }

   private:
    // Состояние демо, переживающее buildTextEditDemoUI(): обработчики захватывают
    // shared_ptr на этот объект, а не ссылку на локальную переменную.
    struct DemoState {
        int submits = 0;
        // Текущий активный валидатор (имя для лейбла, "" = валидация выключена).
        std::string activeValidator;
        // Защита от каскада Change-событий, когда обработчик одного чекбокса
        // программно снимает соседние: их обработчики не должны трогать поле.
        bool suppressToggle = false;
    };

    void buildTextEditDemoUI(const std::shared_ptr<DxvUI::SceneNode>& root) {
        root->setStyle({.textColor = DxvUI::Colors::DarkGray,
                        .fontSize = 18,
                        .fontFamily = "Sans",
                        .width = SCREEN_WIDTH,
                        .height = SCREEN_HEIGHT},
                       DxvUI::WidgetState::Normal);

        auto state = std::make_shared<DemoState>();

        auto caption =
            DxvUI::Label::create("caption",
                                 "TextEdit: кликни в поле, набери текст. Enter — submit, "
                                 "Ctrl+Z — undo, Ctrl+A+печать — замена.");
        caption->setStyle({.left = 5, .top = 40}, DxvUI::WidgetState::Normal);
        root->addChild(caption);

        // --- Поле ввода: фиксированная ширина, лишний текст обрезается (clipContent).
        // Плейсхолдер виден, пока поле пустое и не в фокусе. ---
        auto field = DxvUI::TextEdit::create("name_field");
        field->setPlaceholder("Введите имя");
        field->setStyle({.left = 50, .top = 80, .width = 400, .height = 32},
                        DxvUI::WidgetState::Normal);
        root->addChild(field);

        // Живое эхо: подписка на EventType::Change (стандартный канал «значение
        // изменилось»), значение читается через field->getText().
        auto echo = DxvUI::Label::create("echo_label", "Ввод: (пока пусто)");
        echo->setStyle({.left = 5, .top = 200}, DxvUI::WidgetState::Normal);
        root->addChild(echo);

        connections_.push_back(field->on(
            DxvUI::EventType::Change, [echo, field](DxvUI::DxvEvent&, const DxvUI::UIContext&) {
                const std::string& text = field->getText();
                echo->setText(std::format("Ввод: '{}' (байт: {})", text, text.size()));
            }));

        // Submit: Enter в поле. Пример: зафиксировать текст и вывести в лейбл.
        // Пользовательский слушатель выполняется до дефолтного действия TextEdit:
        // preventDefault отменяет его, stopPropagation не даёт событию всплыть.
        connections_.push_back(field->on(
            DxvUI::EventType::KeyDown,
            [state, field, echo](DxvUI::DxvEvent& e, const DxvUI::UIContext&) {
                if (e.key.sym == DxvUI::KeyCode::Enter) {
                    ++state->submits;
                    const std::string text = field->getText();
                    DxvUI::Log::info("[demo] submit #{}: '{}'", state->submits, text);
                    echo->setText(std::format("Отправлено ({}): '{}'", state->submits, text));
                    e.preventDefault();
                    e.stopPropagation();
                }
            }));

        // --- Кнопка «Очистить»: программная установка текста (одна undo-запись). ---
        auto clearBtn = DxvUI::Button::create("clear_btn", "Очистить");
        clearBtn->setStyle({.left = 50, .top = 140, .width = 120, .height = 32},
                           DxvUI::WidgetState::Normal);
        connections_.push_back(clearBtn->on(DxvUI::EventType::Click,
                                            [field](DxvUI::DxvEvent&, const DxvUI::UIContext&) {
                                                field->setText("");
                                                DxvUI::Log::info("[demo] поле очищено");
                                            }));
        root->addChild(clearBtn);

        // --- Валидаторы: чекбоксы-радиогруппа переключают активную валидацию ---
        // Один активный валидатор: клик по чекбоксу применяет его и снимает
        // соседние; повторный клик по уже активному отключает валидацию вовсе.
        auto validatorLabel = DxvUI::Label::create("validator_label", "Валидатор: нет");
        validatorLabel->setStyle({.left = 5, .top = 250}, DxvUI::WidgetState::Normal);
        root->addChild(validatorLabel);

        auto applyValidator = [field, validatorLabel, state](
                                  std::string name,
                                  std::shared_ptr<DxvUI::validators::ITextValidator> validator) {
            state->activeValidator = std::move(name);
            field->setValidator(std::move(validator));
            // Если в поле уже лежит текст, который новый валидатор отвергает,
            // очищаем его: иначе любая вставка была бы заблокирована, пока буфер
            // не станет валидным вручную (backspace при этом работает).
            const auto& active = field->getValidator();
            if (active && !active->validate(field->getText())) {
                field->setText("");
            }
            validatorLabel->setText("Валидатор: " + (state->activeValidator.empty()
                                                         ? std::string("нет")
                                                         : state->activeValidator));
        };

        std::vector<std::shared_ptr<DxvUI::Checkbox>> validatorBoxes;
        auto registerToggle = [&](const std::shared_ptr<DxvUI::Checkbox>& cb, std::string name,
                                  std::shared_ptr<DxvUI::validators::ITextValidator> validator) {
            connections_.push_back(cb->on(
                DxvUI::EventType::Change,
                [cb, name = std::move(name), validator = std::move(validator), validatorBoxes,
                 applyValidator, state](DxvUI::DxvEvent&, const DxvUI::UIContext&) {
                    if (state->suppressToggle) {
                        // Сосед снят программно — валидатор не трогаем.
                        return;
                    }
                    if (!cb->isChecked()) {
                        // Пользователь снял активный чекбокс — валидация выключается.
                        if (state->activeValidator == name) {
                            applyValidator("", nullptr);
                        }
                        return;
                    }
                    // Радиогруппа: снять все остальные чекбоксы, затем применить свой.
                    state->suppressToggle = true;
                    for (const auto& other : validatorBoxes) {
                        if (other != cb) {
                            other->setChecked(false);
                        }
                    }
                    state->suppressToggle = false;
                    applyValidator(name, validator);
                }));
        };

        auto valDigits = DxvUI::Checkbox::create("val_digits", "Только цифры");
        valDigits->setStyle({.left = 50, .top = 290}, DxvUI::WidgetState::Normal);
        validatorBoxes.push_back(valDigits);
        root->addChild(valDigits);

        auto valHex = DxvUI::Checkbox::create("val_hex", "Hex число");
        valHex->setStyle({.left = 50, .top = 330}, DxvUI::WidgetState::Normal);
        validatorBoxes.push_back(valHex);
        root->addChild(valHex);

        auto valRange = DxvUI::Checkbox::create("val_range", "Диапазон 0–1000");
        valRange->setStyle({.left = 50, .top = 370}, DxvUI::WidgetState::Normal);
        validatorBoxes.push_back(valRange);
        root->addChild(valRange);

        auto valDecimal = DxvUI::Checkbox::create("val_decimal", "Десятичное число (.6)");
        valDecimal->setStyle({.left = 50, .top = 410}, DxvUI::WidgetState::Normal);
        validatorBoxes.push_back(valDecimal);
        root->addChild(valDecimal);

        // Обработчики регистрируются после создания ВСЕХ чекбоксов: хендлер копирует
        // vector со всеми соседями, чтобы радиогруппа работала целиком.
        registerToggle(valDigits, "Только цифры", DxvUI::validators::digitsOnly());
        registerToggle(valHex, "Hex", DxvUI::validators::hex());
        registerToggle(valRange, "Диапазон 0–1000", DxvUI::validators::range(0, 1000));
        registerToggle(valDecimal, "Десятичное число", DxvUI::validators::decimal());

        // FPS-лейбл (обновляется в главном цикле).
        auto fpsLabel = DxvUI::Label::create("fps_label", "FPS: --");
        fpsLabel->setStyle({.top = 10, .right = 10}, DxvUI::WidgetState::Normal);
        root->addChild(fpsLabel);
        fpsLabel_ = fpsLabel;
    }

    void runScriptedTextEdit() {
        auto clickAt = [&](int x, int y) {
            DxvUI::DxvEvent e;
            e.type = DxvUI::EventType::MouseMove;
            e.mouse.x = x;
            e.mouse.y = y;
            e.mouse.button = DxvUI::MouseButton::None;
            scene_->processEvent(e);
            e.type = DxvUI::EventType::MouseDown;
            e.mouse.button = DxvUI::MouseButton::Left;
            scene_->processEvent(e);
            e.type = DxvUI::EventType::MouseUp;
            scene_->processEvent(e);
        };
        auto typeText = [&](const std::string& text) {
            DxvUI::DxvEvent e;
            e.type = DxvUI::EventType::TextInput;
            e.text = text;
            scene_->processEvent(e);
        };
        auto key = [&](DxvUI::KeyCode sym, uint16_t mod = DxvUI::KeyModifier::None) {
            DxvUI::DxvEvent e;
            e.type = DxvUI::EventType::KeyDown;
            e.key.sym = sym;
            e.key.mod = mod;
            scene_->processEvent(e);
        };

        DxvUI::Log::info("=== Scripted TextEdit demo (headless) ===");
        bool ok = true;

        auto field = scene_->findNodeById("name_field")->as<DxvUI::TextEdit>();

        // Клик в поле → фокус, каретка по месту клика.
        clickAt(250, 96);
        DxvUI::Log::info("[check] фокус: {}",
                         field->getCurrentState() == DxvUI::WidgetState::Focused ? "да" : "НЕТ");
        ok &= field->getCurrentState() == DxvUI::WidgetState::Focused;

        // Ctrl+A, затем печать заменяет всё содержимое.
        key(DxvUI::KeyCode::A, DxvUI::KeyModifier::Ctrl);
        typeText("Иван");
        DxvUI::Log::info("[check] текст после замены: '{}'", field->getText());
        ok &= field->getText() == "\xD0\x98\xD0\xB2\xD0\xB0\xD0\xBD";  // "Иван"

        // Enter → submit; текст эха обновляется.
        key(DxvUI::KeyCode::Enter);
        auto echo = scene_->findNodeById("echo_label")->as<DxvUI::Label>();
        DxvUI::Log::info("[check] эхо: '{}'", echo->getText());
        ok &= echo->getText().find("Иван") != std::string::npos;

        // Backspace удаляет целый UTF-8 code point.
        key(DxvUI::KeyCode::End);
        key(DxvUI::KeyCode::Backspace);
        DxvUI::Log::info("[check] после Backspace: '{}'", field->getText());
        ok &= field->getText() == "\xD0\x98\xD0\xB2\xD0\xB0";  // "Ива"

        // Undo возвращает "Иван".
        key(DxvUI::KeyCode::Z, DxvUI::KeyModifier::Ctrl);
        DxvUI::Log::info("[check] после Ctrl+Z: '{}'", field->getText());
        ok &= field->getText() == "\xD0\x98\xD0\xB2\xD0\xB0\xD0\xBD";

        // Кнопка «Очистить» очищает поле программно.
        clickAt(110, 156);
        DxvUI::Log::info("[check] после очистки: '{}'", field->getText());
        ok &= field->getText().empty();

        // --- Валидаторы: переключение чекбоксами (радиогруппа) ---
        // Чекбоксы стоят на (50, 290)/(50, 330)/(50, 410); кликаем по квадратику.
        auto clickValidator = [&](int top) { clickAt(60, top + 8); };

        // «Только цифры»: поле пусто, включаем и печатаем посимвольно.
        clickValidator(290);
        clickAt(250, 96);
        key(DxvUI::KeyCode::End);
        typeText("1");
        typeText("2");
        typeText("a");  // буква отклоняется
        typeText("3");
        DxvUI::Log::info("[check] digits: после '12a3' = '{}'", field->getText());
        ok &= field->getText() == "123";

        // «Hex»: "123" — валидный hex, текст сохраняется; G отклонена, F принята.
        clickValidator(330);
        DxvUI::Log::info("[check] hex: '123' сохранён: {}",
                         field->getText() == "123" ? "да" : "НЕТ");
        ok &= field->getText() == "123";
        clickAt(250, 96);
        key(DxvUI::KeyCode::End);
        typeText("G");
        typeText("F");
        DxvUI::Log::info("[check] hex: после 'GF' = '{}'", field->getText());
        ok &= field->getText() == "123F";

        // «Десятичное число»: "123F" невалиден -> поле очищается; вводим ".6".
        clickValidator(410);
        DxvUI::Log::info("[check] decimal: невалидный текст очищен: {}",
                         field->getText().empty() ? "да" : "НЕТ");
        ok &= field->getText().empty();
        clickAt(250, 96);
        key(DxvUI::KeyCode::End);
        typeText(".");
        typeText("6");
        typeText(".");  // вторая точка отклоняется
        DxvUI::Log::info("[check] decimal: после '.6.' = '{}'", field->getText());
        ok &= field->getText() == ".6";

        // Повторный клик снимает валидатор: снова можно вводить произвольный текст.
        clickValidator(410);
        clickAt(250, 96);
        key(DxvUI::KeyCode::End);
        typeText("x");
        DxvUI::Log::info("[check] без валидатора: после 'x' = '{}'", field->getText());
        ok &= field->getText() == ".6x";

        // Мёртвых токенов нет — поле и кнопка живы.
        std::erase_if(connections_, [](const auto& c) { return c->expired(); });
        DxvUI::Log::info("[check] итог: {}", ok ? "PASS" : "FAIL");
    }

    std::unique_ptr<DxvUI::SDLRenderer> dxvRenderer_;
    std::shared_ptr<DxvUI::Scene> scene_;
    DxvUI::SDLEventSource eventSource_;
    std::vector<std::unique_ptr<DxvUI::SceneNode::Connection>> connections_;
    DxvUI::FpsCounter<> fps_;
    std::shared_ptr<DxvUI::Label> fpsLabel_;
};

#ifdef _WIN32
extern "C" int SDL_main(int /*argc*/, char* /*argv*/[]) {
#else
int main(int /*argc*/, char* /*argv*/[]) {
#endif
    DxvUI::Log::init();
    DxvUI::Log::info("Logger Initialized.");

    DxvUITextEditExample app;
    return app.run();
}
