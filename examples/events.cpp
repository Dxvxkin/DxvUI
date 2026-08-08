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
#include <SDL.h>

#include <cstdlib>
#include <format>
#include <memory>
#include <string>
#include <vector>

constexpr int SCREEN_WIDTH = 800;
constexpr int SCREEN_HEIGHT = 600;

namespace {

// Состояние демо, переживающее buildEventsDemoUI(): обработчики захватывают
// shared_ptr на этот объект (захват ссылки на локальную переменную повис бы
// после возврата из функции).
struct DemoState {
    int clickCount = 0;
    int selfClicks = 0;
    std::unique_ptr<DxvUI::SceneNode::Connection> extraConn;
};

std::shared_ptr<DxvUI::Label> makeLabel(const std::string& id, const std::string& text, int x,
                                        int y) {
    auto label = DxvUI::Label::create(id, text);
    label->setStyle({.fontSize = 16, .left = static_cast<float>(x), .top = static_cast<float>(y)},
                    DxvUI::WidgetState::Normal);
    return label;
}

std::shared_ptr<DxvUI::Button> makeButton(const std::string& id, const std::string& text, int x,
                                          int y, int width, int height) {
    auto button = DxvUI::Button::create(id, text);
    button->setStyle({.left = static_cast<float>(x),
                      .top = static_cast<float>(y),
                      .width = static_cast<float>(width),
                      .height = static_cast<float>(height)},
                     DxvUI::WidgetState::Normal);
    return button;
}

void buildEventsDemoUI(const std::shared_ptr<DxvUI::Scene>& scene,
                       std::vector<std::unique_ptr<DxvUI::SceneNode::Connection>>& connections) {
    auto root = scene->getRoot();

    root->setStyle({.textColor = DxvUI::Colors::DarkGray,
                    .fontSize = 18,
                    .fontPath = DxvUI::getDefaultFontPath(),
                    .width = SCREEN_WIDTH,
                    .height = SCREEN_HEIGHT},
                   DxvUI::WidgetState::Normal);

    auto state = std::make_shared<DemoState>();

    // --- Подписки на корне: события всплывают снизу вверх, поэтому корень видит
    // событие любого потомка (пока ни один обработчик не остановил всплытие
    // через event.handled = true). Регистрируются ДО создания виджетов, чтобы
    // поймать их Attach.
    connections.push_back(
        root->on(DxvUI::EventType::Click, [](DxvUI::DxvEvent& e, const DxvUI::UIContext&) {
            const auto target = e.getTarget();
            const auto current = e.getCurrentTarget();
            DxvUI::Log::info("[root] Click всплыл от '{}' (currentTarget={})", target->getId(),
                             current ? current->getId() : "<root>");
        }));

    connections.push_back(
        root->on(DxvUI::EventType::Change, [](DxvUI::DxvEvent& e, const DxvUI::UIContext&) {
            DxvUI::Log::info("[root] Change от '{}'", e.getTargetId());
        }));

    connections.push_back(
        root->on(DxvUI::EventType::Attach, [](DxvUI::DxvEvent& e, const DxvUI::UIContext&) {
            DxvUI::Log::info("[root] Attach: '{}' добавлен в сцену", e.getTargetId());
        }));

    connections.push_back(
        root->on(DxvUI::EventType::Detach, [](DxvUI::DxvEvent& e, const DxvUI::UIContext&) {
            DxvUI::Log::info("[root] Detach: '{}' ушёл из сцены", e.getTargetId());
        }));

    // FPS-лейбл (обновляется в главном цикле).
    auto fpsLabel = DxvUI::Label::create("fps_label", "FPS: --");
    fpsLabel->setStyle({.top = 10, .right = 10}, DxvUI::WidgetState::Normal);
    root->addChild(fpsLabel);

    root->addChild(makeLabel(
        "caption", "События: клики, hover, drag&drop, клавиши. Логи — в консоли.", 50, 40));

    // --- Click: обработчик кнопки ставит handled=true → корень его не увидит.
    auto btnClick = makeButton("btn_click", "Клик (handled=true)", 50, 90, 220, 40);
    connections.push_back(
        btnClick->on(DxvUI::EventType::HoverEnter, [](DxvUI::DxvEvent&, const DxvUI::UIContext&) {
            DxvUI::Log::info("[btn_click] HoverEnter");
        }));
    connections.push_back(
        btnClick->on(DxvUI::EventType::HoverLeave, [](DxvUI::DxvEvent&, const DxvUI::UIContext&) {
            DxvUI::Log::info("[btn_click] HoverLeave");
        }));

    auto labelCount = makeLabel("label_count", "Кликов: 0", 50, 150);
    root->addChild(labelCount);
    connections.push_back(labelCount->on(DxvUI::EventType::Change, [](DxvUI::DxvEvent& e,
                                                                      const DxvUI::UIContext&) {
        DxvUI::Log::info("[label_count] Change: '{}'", e.getTarget()->getBinding()->getString());
    }));

    connections.push_back(btnClick->on(
        DxvUI::EventType::Click, [state, labelCount](DxvUI::DxvEvent& e, const DxvUI::UIContext&) {
            DxvUI::Log::info("[btn_click] Click в ({}, {}) по '{}'", e.mouse.x, e.mouse.y,
                             e.getTargetId());
            ++state->clickCount;
            // Смена текста порождает Change на лейбле (и всплывает до корня).
            labelCount->setText(std::format("Кликов: {}", state->clickCount));
            e.handled = true;  // корневая подписка Click не сработает
        }));
    root->addChild(btnClick);

    // --- Click без handled: событие всплывает до корня (см. подписку root).
    auto btnBubble = makeButton("btn_bubble", "Клик (bubble)", 290, 90, 220, 40);
    connections.push_back(
        btnBubble->on(DxvUI::EventType::Click, [](DxvUI::DxvEvent& e, const DxvUI::UIContext&) {
            DxvUI::Log::info("[btn_bubble] Click по '{}' (handled не ставим)", e.getTargetId());
        }));
    // Фокус: после MouseDown фокус получает нажатая кнопка; клавиши уходят в неё.
    // Текстовый ввод (TextInput) потребовал бы SDL_StartTextInput().
    connections.push_back(
        btnBubble->on(DxvUI::EventType::FocusGained, [](DxvUI::DxvEvent&, const DxvUI::UIContext&) {
            DxvUI::Log::info("[btn_bubble] FocusGained");
        }));
    connections.push_back(
        btnBubble->on(DxvUI::EventType::FocusLost, [](DxvUI::DxvEvent&, const DxvUI::UIContext&) {
            DxvUI::Log::info("[btn_bubble] FocusLost");
        }));
    connections.push_back(
        btnBubble->on(DxvUI::EventType::KeyDown, [](DxvUI::DxvEvent& e, const DxvUI::UIContext&) {
            DxvUI::Log::info("[btn_bubble] KeyDown sym={} scancode={}", e.key.sym, e.key.scancode);
        }));
    root->addChild(btnBubble);

    // --- Runtime-подписка/отписка: клик по btn_toggle подключает/отключает
    // дополнительный Click-обработчик на btn_bubble. Деструктор токена
    // (extraConn.reset()) отписывает обработчик.
    auto btnToggle = makeButton("btn_toggle", "Тоггл подписки на btn_bubble", 530, 90, 220, 40);
    connections.push_back(
        btnToggle->on(DxvUI::EventType::Click, [](DxvUI::DxvEvent&, const DxvUI::UIContext&) {
            DxvUI::Log::info("[btn_toggle] постоянный обработчик");
        }));
    connections.push_back(btnToggle->on(
        DxvUI::EventType::Click, [state, btnBubble](DxvUI::DxvEvent&, const DxvUI::UIContext&) {
            if (!state->extraConn) {
                state->extraConn = btnBubble->on(DxvUI::EventType::Click,
                                                 [](DxvUI::DxvEvent& e, const DxvUI::UIContext&) {
                                                     DxvUI::Log::info(
                                                         "[btn_bubble] ДОП. обработчик (подписка с "
                                                         "btn_toggle) по '{}'",
                                                         e.getTargetId());
                                                 });
                DxvUI::Log::info("[btn_toggle] подписка на btn_bubble подключена");
            } else {
                state->extraConn.reset();  // RAII: деструктор токена отписывает обработчик
                DxvUI::Log::info("[btn_toggle] подписка на btn_bubble отключена");
            }
        }));
    root->addChild(btnToggle);

    // --- Drag & Drop: источник логирует MouseDown/Drag, приёмник — Drop.
    auto boxDrag = makeButton("box_drag", "Тащи меня", 50, 270, 150, 80);
    connections.push_back(
        boxDrag->on(DxvUI::EventType::HoverEnter, [](DxvUI::DxvEvent&, const DxvUI::UIContext&) {
            DxvUI::Log::info("[box_drag] HoverEnter");
        }));
    connections.push_back(
        boxDrag->on(DxvUI::EventType::HoverLeave, [](DxvUI::DxvEvent&, const DxvUI::UIContext&) {
            DxvUI::Log::info("[box_drag] HoverLeave");
        }));
    connections.push_back(
        boxDrag->on(DxvUI::EventType::MouseDown, [](DxvUI::DxvEvent& e, const DxvUI::UIContext&) {
            DxvUI::Log::info("[box_drag] MouseDown ({}, {})", e.mouse.x, e.mouse.y);
        }));
    connections.push_back(
        boxDrag->on(DxvUI::EventType::Drag, [](DxvUI::DxvEvent& e, const DxvUI::UIContext&) {
            DxvUI::Log::info("[box_drag] Drag d=({}, {})", e.mouse.dx, e.mouse.dy);
        }));
    root->addChild(boxDrag);

    auto boxDrop = makeButton("box_drop", "Приёмник", 300, 270, 150, 80);
    connections.push_back(
        boxDrop->on(DxvUI::EventType::Drop, [](DxvUI::DxvEvent& e, const DxvUI::UIContext&) {
            const auto from = e.getRelatedNode();
            DxvUI::Log::info("[box_drop] Drop из '{}' в ({}, {})", from ? from->getId() : "<none>",
                             e.mouse.x, e.mouse.y);
        }));
    root->addChild(boxDrop);

    // --- Самоудаление: после 3-го клика кнопка уходит из сцены (Detach),
    // а токен её подписки становится expired() и вычищается в главном цикле.
    auto btnSelf = makeButton("btn_self", "Самоудаление (3 клика)", 50, 200, 260, 40);
    connections.push_back(
        btnSelf->on(DxvUI::EventType::Click, [state](DxvUI::DxvEvent& e, const DxvUI::UIContext&) {
            if (++state->selfClicks < 3) {
                DxvUI::Log::info("[btn_self] клик {}/3", state->selfClicks);
                return;
            }
            DxvUI::Log::info("[btn_self] третий клик: удаляюсь из сцены");
            if (auto target = e.getTarget()) {
                target->detach();  // → Detach на 'btn_self'
            }
        }));
    root->addChild(btnSelf);
}

// Headless-режим: прогон синтетических событий через scene->processEvent(), чтобы
// показать все подписки без взаимодействия (запускается с DXVUI_FRAMES).
void runScriptedEvents(const std::shared_ptr<DxvUI::Scene>& scene,
                       std::vector<std::unique_ptr<DxvUI::SceneNode::Connection>>& connections) {
    auto move = [&](int x, int y, DxvUI::MouseButton held) {
        DxvUI::DxvEvent e;
        e.type = DxvUI::EventType::MouseMove;
        e.mouse.x = x;
        e.mouse.y = y;
        e.mouse.button = held;
        scene->processEvent(e);
    };
    auto press = [&](int x, int y) {
        DxvUI::DxvEvent e;
        e.type = DxvUI::EventType::MouseDown;
        e.mouse.x = x;
        e.mouse.y = y;
        e.mouse.button = DxvUI::MouseButton::Left;
        scene->processEvent(e);
    };
    auto release = [&](int x, int y) {
        DxvUI::DxvEvent e;
        e.type = DxvUI::EventType::MouseUp;
        e.mouse.x = x;
        e.mouse.y = y;
        e.mouse.button = DxvUI::MouseButton::Left;
        scene->processEvent(e);
    };
    auto clickAt = [&](int x, int y) {
        move(x, y, DxvUI::MouseButton::None);
        press(x, y);
        release(x, y);
    };

    DxvUI::Log::info("=== Scripted demo (headless) ===");

    // Hover + Click с handled=true.
    move(160, 110, DxvUI::MouseButton::None);
    move(160, 110, DxvUI::MouseButton::None);  // повторное движение — hit-test кэш
    clickAt(160, 110);  // btn_click: Click + Change, корневой Click не увидит

    // Click с бабблингом до корня + фокус + клавиша.
    clickAt(400, 110);  // btn_bubble: Click доходит до корня, кнопка получает фокус
    DxvUI::DxvEvent key;
    key.type = DxvUI::EventType::KeyDown;
    key.key.sym = SDLK_SPACE;
    key.key.scancode = SDL_SCANCODE_SPACE;
    scene->processEvent(key);  // клавиша уходит в focused-ноду (btn_bubble)

    // Runtime-подписка/отписка: тоггл подключает/отключает доп. обработчик на
    // btn_bubble (сработает только пока подписка подключена).
    clickAt(640, 110);  // подключить
    clickAt(400, 110);  // btn_bubble: основной + ДОП. обработчик
    clickAt(640, 110);  // отключить
    clickAt(400, 110);  // btn_bubble: только основной обработчик

    // Drag & Drop: нажали на box_drag, тащим, отпустили над box_drop.
    move(125, 310, DxvUI::MouseButton::None);
    press(125, 310);
    move(175, 310, DxvUI::MouseButton::Left);  // Drag d=(50, 0)
    move(375, 310, DxvUI::MouseButton::Left);  // Drag d=(200, 0)
    release(375, 310);                         // Drop на box_drop

    // Самоудаление через 3 клика.
    for (int i = 0; i < 3; ++i) {
        clickAt(180, 220);
    }
    DxvUI::Log::info("[main] btn_self удалён: {}", scene->findNodeById("btn_self") ? "нет" : "да");

    // Мёртвые токены (нода уничтожена) вычищаются; деструктор токена безопасен.
    const size_t before = connections.size();
    std::erase_if(connections, [](const auto& c) { return c->expired(); });
    DxvUI::Log::info("[main] вычищено expired-токенов: {}", before - connections.size());
}

}  // namespace

extern "C" int SDL_main(int /*argc*/, char* /*argv*/[]) {
    DxvUI::Log::init();
    DxvUI::Log::info("Logger Initialized.");

    DxvUI::SDLRenderer dxv_renderer_impl("DxvUI Events Example", SCREEN_WIDTH, SCREEN_HEIGHT);
    DxvUI::IRenderer& dxv_renderer = dxv_renderer_impl;
    DxvUI::SDLEventSource eventSource;
    auto scene = DxvUI::Scene::create();
    scene->setRenderer(&dxv_renderer);

    // Токены подписок живут весь цикл приложения: уничтожение токена = отписка.
    std::vector<std::unique_ptr<DxvUI::SceneNode::Connection>> connections;
    buildEventsDemoUI(scene, connections);
    scene->updateLayout();

    // Headless-прогон для проверки (задана DXVUI_FRAMES): синтетический сценарий
    // событий + логи, затем выход.
    if (std::getenv("DXVUI_FRAMES")) {
        runScriptedEvents(scene, connections);
        scene->shutdown();
        scene.reset();
        return 0;
    }

    bool quit = false;
    SDL_Event sdl_event;
    Uint32 last_time = SDL_GetTicks();

    DxvUI::FpsCounter fps;
    auto fpsLabel = scene->findNodeById("fps_label")->as<DxvUI::Label>();

    while (!quit) {
        Uint32 current_time = SDL_GetTicks();
        float delta_time = (current_time - last_time) / 1000.0f;
        last_time = current_time;

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

        scene->update(delta_time);

        dxv_renderer.clear(DxvUI::Colors::White);
        scene->draw();
        dxv_renderer.present();

        fps.tick();
        fpsLabel->setText(
            std::format("FPS: {:.0f} ({:.1f} ms)", fps.getFps(), fps.getFrameTimeMs()));

        // Вычистка мёртвых токенов (ноды удалены пользователем).
        std::erase_if(connections, [](const auto& c) { return c->expired(); });
    }
    scene->shutdown();
    scene.reset();
    return 0;
}
