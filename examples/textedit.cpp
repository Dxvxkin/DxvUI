#include <DxvUI/DxvEvent.h>
#include <DxvUI/FpsCounter.h>
#include <DxvUI/Log.h>
#include <DxvUI/Scene.h>
#include <DxvUI/core.h>
#include <DxvUI/renderers/SDLRenderer.h>
#include <DxvUI/sources/SDLEventSource.h>
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

constexpr int SCREEN_WIDTH = 800;
constexpr int SCREEN_HEIGHT = 600;

namespace {

// Состояние демо, переживающее buildTextEditDemoUI(): обработчики захватывают
// shared_ptr на этот объект, а не ссылку на локальную переменную.
struct DemoState {
    int submits = 0;
};

void buildTextEditDemoUI(const std::shared_ptr<DxvUI::Scene>& scene,
                         std::vector<std::unique_ptr<DxvUI::SceneNode::Connection>>& connections) {
    auto root = scene->getRoot();

    root->setStyle({.textColor = DxvUI::Colors::DarkGray,
                    .fontSize = 18,
                    .fontFamily = "Sans",
                    .width = SCREEN_WIDTH,
                    .height = SCREEN_HEIGHT},
                   DxvUI::WidgetState::Normal);

    auto state = std::make_shared<DemoState>();

    auto caption = DxvUI::Label::create("caption",
                                        "TextEdit: кликни в поле, набери текст. Enter — submit, "
                                        "Ctrl+Z — undo, Ctrl+A+печать — замена.");
    caption->setStyle({.left = 5, .top = 40}, DxvUI::WidgetState::Normal);
    root->addChild(caption);

    // --- Поле ввода: фиксированная ширина, лишний текст обрезается (clipContent). ---
    auto field = DxvUI::TextEdit::create("name_field", "Введите имя");
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
    connections.push_back(
        field->on(DxvUI::EventType::KeyDown,
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
    connections.push_back(
        clearBtn->on(DxvUI::EventType::Click, [field](DxvUI::DxvEvent&, const DxvUI::UIContext&) {
            field->setText("");
            DxvUI::Log::info("[demo] поле очищено");
        }));
    root->addChild(clearBtn);

    // FPS-лейбл (обновляется в главном цикле).
    auto fpsLabel = DxvUI::Label::create("fps_label", "FPS: --");
    fpsLabel->setStyle({.top = 10, .right = 10}, DxvUI::WidgetState::Normal);
    root->addChild(fpsLabel);
}

// Headless-режим (запускается с DXVUI_FRAMES): прогон синтетических событий через
// scene->processEvent(), чтобы проверить редактирование без живого ввода.
void runScriptedTextEdit(const std::shared_ptr<DxvUI::Scene>& scene,
                         std::vector<std::unique_ptr<DxvUI::SceneNode::Connection>>& connections) {
    auto clickAt = [&](int x, int y) {
        DxvUI::DxvEvent e;
        e.type = DxvUI::EventType::MouseMove;
        e.mouse.x = x;
        e.mouse.y = y;
        e.mouse.button = DxvUI::MouseButton::None;
        scene->processEvent(e);
        e.type = DxvUI::EventType::MouseDown;
        e.mouse.button = DxvUI::MouseButton::Left;
        scene->processEvent(e);
        e.type = DxvUI::EventType::MouseUp;
        scene->processEvent(e);
    };
    auto typeText = [&](const std::string& text) {
        DxvUI::DxvEvent e;
        e.type = DxvUI::EventType::TextInput;
        e.text = text;
        scene->processEvent(e);
    };
    auto key = [&](DxvUI::KeyCode sym, uint16_t mod = DxvUI::KeyModifier::None) {
        DxvUI::DxvEvent e;
        e.type = DxvUI::EventType::KeyDown;
        e.key.sym = sym;
        e.key.mod = mod;
        scene->processEvent(e);
    };

    DxvUI::Log::info("=== Scripted TextEdit demo (headless) ===");
    bool ok = true;

    auto field = scene->findNodeById("name_field")->as<DxvUI::TextEdit>();

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
    auto echo = scene->findNodeById("echo_label")->as<DxvUI::Label>();
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
    std::erase_if(connections, [](const auto& c) { return c->expired(); });
    DxvUI::Log::info("[check] итог: {}", ok ? "PASS" : "FAIL");
}

}  // namespace

extern "C" int SDL_main(int /*argc*/, char* /*argv*/[]) {
    DxvUI::Log::init();
    DxvUI::Log::info("Logger Initialized.");

    DxvUI::SDLRenderer dxv_renderer_impl("DxvUI TextEdit Example", SCREEN_WIDTH, SCREEN_HEIGHT);
    DxvUI::IRenderer& dxv_renderer = dxv_renderer_impl;
    DxvUI::SDLEventSource eventSource;
    auto scene = DxvUI::Scene::create();
    scene->setRenderer(&dxv_renderer);

    std::vector<std::unique_ptr<DxvUI::SceneNode::Connection>> connections;
    buildTextEditDemoUI(scene, connections);
    scene->updateLayout();

    // Headless-прогон для проверки (задана DXVUI_FRAMES).
    if (std::getenv("DXVUI_FRAMES")) {
        runScriptedTextEdit(scene, connections);
        scene->shutdown();
        scene.reset();
        return 0;
    }

    // Включаем текстовый ввод, иначе SDL не будет слать SDL_TEXTINPUT.
    SDL_StartTextInput();

    bool quit = false;
    SDL_Event sdl_event;

    DxvUI::FpsCounter fps;
    auto fpsLabel = scene->findNodeById("fps_label")->as<DxvUI::Label>();

    while (!quit) {
        while (SDL_PollEvent(&sdl_event) != 0) {
            DxvUI::DxvEvent dxv_event;
            if (eventSource.processEvent(sdl_event, dxv_event)) {
                if (dxv_event.type == DxvUI::EventType::Quit) {
                    quit = true;
                } else {
                    scene->processEvent(dxv_event);
                }
            }
        }

        scene->update();

        dxv_renderer.clear(DxvUI::Colors::White);
        scene->draw();
        dxv_renderer.present();

        fps.tick();
        fpsLabel->setText(
            std::format("FPS: {:.0f} ({:.1f} ms)", fps.getFps(), fps.getFrameTimeMs()));

        std::erase_if(connections, [](const auto& c) { return c->expired(); });
    }
    scene->shutdown();
    scene.reset();
    return 0;
}
