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
#include <DxvUI/widgets/Button.h>
#include <DxvUI/widgets/Label.h>
#include <DxvUI/widgets/TextEdit.h>
#include <SDL.h>

#include <cstdlib>
#include <format>
#include <memory>
#include <string>
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

        // Живое эхо: модель TextEditor уведомляет о каждом изменении буфера.
        auto echo = DxvUI::Label::create("echo_label", "Ввод: (пока пусто)");
        echo->setStyle({.left = 5, .top = 200}, DxvUI::WidgetState::Normal);
        root->addChild(echo);

        field->getEditor().setChangeCallback([echo, field] {
            const std::string& text = field->getEditor().getText();
            echo->setText(std::format("Ввод: '{}' (байт: {})", text, text.size()));
        });

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
