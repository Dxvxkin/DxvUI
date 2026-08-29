// examples/events.cpp
//
// Event-system demo: click bubbling, hover, focus, drag&drop, runtime subscribe,
// and self-detaching nodes. It subclasses DxvUIEx::SdlApp and owns the DxvUI
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

}  // namespace

class DxvUIEventsExample : public DxvUIEx::SdlApp {
   public:
    DxvUIEventsExample() : DxvUIEx::SdlApp("DxvUI Events Example", SCREEN_WIDTH, SCREEN_HEIGHT) {}

   protected:
    bool init() override {
        dxvRenderer_ = std::make_unique<DxvUI::SDLRenderer>(renderer_);
        scene_ = DxvUI::Scene::create();
        scene_->setRenderer(dxvRenderer_.get());

        buildEventsDemoUI(scene_->getRoot());
        scene_->updateLayout();

        // Headless-прогон для проверки (задана DXVUI_FRAMES): синтетический сценарий
        // событий + логи, затем выход до входа в цикл.
        if (std::getenv("DXVUI_FRAMES")) {
            runScriptedEvents();
            return false;
        }
        return true;
    }

    void update(float /*dtMs*/) override {
        scene_->update();

        fps_.tick();
        fpsLabel_->setText(
            std::format("FPS: {:.0f} ({:.1f} ms)", fps_.getFps(), fps_.getFrameTimeMs()));

        // Вычистка мёртвых токенов (ноды удалены пользователем).
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
    // Состояние демо, переживающее buildEventsDemoUI(): обработчики захватывают
    // shared_ptr на этот объект (захват ссылки на локальную переменную повис бы
    // после возврата из функции).
    struct DemoState {
        int clickCount = 0;
        int selfClicks = 0;
        std::unique_ptr<DxvUI::SceneNode::Connection> extraConn;
    };

    void buildEventsDemoUI(const std::shared_ptr<DxvUI::SceneNode>& root) {
        root->setStyle({.textColor = DxvUI::Colors::DarkGray,
                        .fontSize = 18,
                        .fontFamily = "Sans",
                        .width = SCREEN_WIDTH,
                        .height = SCREEN_HEIGHT},
                       DxvUI::WidgetState::Normal);

        auto state = std::make_shared<DemoState>();

        // --- Подписки на корне: события всплывают снизу вверх, поэтому корень видит
        // событие любого потомка (пока ни один обработчик не остановил всплытие
        // через stopPropagation/stopImmediatePropagation). Регистрируются ДО создания
        // виджетов, чтобы поймать их Attach.
        connections_.push_back(
            root->on(DxvUI::EventType::Click, [](DxvUI::DxvEvent& e, const DxvUI::UIContext&) {
                const auto target = e.getTarget();
                const auto current = e.getCurrentTarget();
                DxvUI::Log::info("[root] Click всплыл от '{}' (currentTarget={})", target->getId(),
                                 current ? current->getId() : "<root>");
            }));

        connections_.push_back(
            root->on(DxvUI::EventType::Change, [](DxvUI::DxvEvent& e, const DxvUI::UIContext&) {
                DxvUI::Log::info("[root] Change от '{}'", e.getTargetId());
            }));

        connections_.push_back(
            root->on(DxvUI::EventType::Attach, [](DxvUI::DxvEvent& e, const DxvUI::UIContext&) {
                DxvUI::Log::info("[root] Attach: '{}' добавлен в сцену", e.getTargetId());
            }));

        connections_.push_back(
            root->on(DxvUI::EventType::Detach, [](DxvUI::DxvEvent& e, const DxvUI::UIContext&) {
                DxvUI::Log::info("[root] Detach: '{}' ушёл из сцены", e.getTargetId());
            }));

        // FPS-лейбл (обновляется в главном цикле).
        auto fpsLabel = DxvUI::Label::create("fps_label", "FPS: --");
        fpsLabel->setStyle({.top = 10, .right = 10}, DxvUI::WidgetState::Normal);
        root->addChild(fpsLabel);
        fpsLabel_ = fpsLabel;

        root->addChild(makeLabel(
            "caption", "События: клики, hover, drag&drop, клавиши. Логи — в консоли.", 50, 40));

        // --- Click: обработчик кнопки ставит stopImmediatePropagation → корень его не увидит.
        auto btnClick = makeButton("btn_click", "Клик (stop)", 50, 90, 220, 40);
        connections_.push_back(btnClick->on(DxvUI::EventType::HoverEnter,
                                            [](DxvUI::DxvEvent&, const DxvUI::UIContext&) {
                                                DxvUI::Log::info("[btn_click] HoverEnter");
                                            }));
        connections_.push_back(btnClick->on(DxvUI::EventType::HoverLeave,
                                            [](DxvUI::DxvEvent&, const DxvUI::UIContext&) {
                                                DxvUI::Log::info("[btn_click] HoverLeave");
                                            }));

        auto labelCount = makeLabel("label_count", "Кликов: 0", 50, 150);
        root->addChild(labelCount);
        connections_.push_back(labelCount->on(
            DxvUI::EventType::Change, [](DxvUI::DxvEvent& e, const DxvUI::UIContext&) {
                DxvUI::Log::info("[label_count] Change: '{}'",
                                 e.getTarget()->getBinding()->getString());
            }));

        connections_.push_back(
            btnClick->on(DxvUI::EventType::Click,
                         [state, labelCount](DxvUI::DxvEvent& e, const DxvUI::UIContext&) {
                             DxvUI::Log::info("[btn_click] Click в ({}, {}) по '{}'", e.mouse.x,
                                              e.mouse.y, e.getTargetId());
                             ++state->clickCount;
                             // Смена текста порождает Change на лейбле (и всплывает до корня).
                             labelCount->setText(std::format("Кликов: {}", state->clickCount));
                             e.stopImmediatePropagation();  // корневая подписка Click не сработает
                         }));
        root->addChild(btnClick);

        // --- Click без stopPropagation: событие всплывает до корня (см. подписку root).
        auto btnBubble = makeButton("btn_bubble", "Клик (bubble)", 290, 90, 220, 40);
        connections_.push_back(
            btnBubble->on(DxvUI::EventType::Click, [](DxvUI::DxvEvent& e, const DxvUI::UIContext&) {
                DxvUI::Log::info("[btn_bubble] Click по '{}' (stop не ставим)", e.getTargetId());
            }));
        // Фокус: после MouseDown фокус получает нажатая кнопка; клавиши уходят в неё.
        // Текстовый ввод (TextInput) потребовал бы SDL_StartTextInput().
        connections_.push_back(btnBubble->on(DxvUI::EventType::FocusGained,
                                             [](DxvUI::DxvEvent&, const DxvUI::UIContext&) {
                                                 DxvUI::Log::info("[btn_bubble] FocusGained");
                                             }));
        connections_.push_back(btnBubble->on(DxvUI::EventType::FocusLost,
                                             [](DxvUI::DxvEvent&, const DxvUI::UIContext&) {
                                                 DxvUI::Log::info("[btn_bubble] FocusLost");
                                             }));
        connections_.push_back(btnBubble->on(
            DxvUI::EventType::KeyDown, [](DxvUI::DxvEvent& e, const DxvUI::UIContext&) {
                DxvUI::Log::info("[btn_bubble] KeyDown sym={}", static_cast<int>(e.key.sym));
            }));
        root->addChild(btnBubble);

        // --- Runtime-подписка/отписка: клик по btn_toggle подключает/отключает
        // дополнительный Click-обработчик на btn_bubble. Деструктор токена
        // (extraConn.reset()) отписывает обработчик.
        auto btnToggle = makeButton("btn_toggle", "Тоггл подписки на btn_bubble", 530, 90, 220, 40);
        connections_.push_back(
            btnToggle->on(DxvUI::EventType::Click, [](DxvUI::DxvEvent&, const DxvUI::UIContext&) {
                DxvUI::Log::info("[btn_toggle] постоянный обработчик");
            }));
        connections_.push_back(btnToggle->on(
            DxvUI::EventType::Click, [state, btnBubble](DxvUI::DxvEvent&, const DxvUI::UIContext&) {
                if (!state->extraConn) {
                    state->extraConn = btnBubble->on(
                        DxvUI::EventType::Click, [](DxvUI::DxvEvent& e, const DxvUI::UIContext&) {
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
        connections_.push_back(boxDrag->on(DxvUI::EventType::HoverEnter,
                                           [](DxvUI::DxvEvent&, const DxvUI::UIContext&) {
                                               DxvUI::Log::info("[box_drag] HoverEnter");
                                           }));
        connections_.push_back(boxDrag->on(DxvUI::EventType::HoverLeave,
                                           [](DxvUI::DxvEvent&, const DxvUI::UIContext&) {
                                               DxvUI::Log::info("[box_drag] HoverLeave");
                                           }));
        connections_.push_back(boxDrag->on(
            DxvUI::EventType::MouseDown, [](DxvUI::DxvEvent& e, const DxvUI::UIContext&) {
                DxvUI::Log::info("[box_drag] MouseDown ({}, {})", e.mouse.x, e.mouse.y);
            }));
        connections_.push_back(
            boxDrag->on(DxvUI::EventType::Drag, [](DxvUI::DxvEvent& e, const DxvUI::UIContext&) {
                DxvUI::Log::info("[box_drag] Drag d=({}, {})", e.mouse.dx, e.mouse.dy);
            }));
        root->addChild(boxDrag);

        auto boxDrop = makeButton("box_drop", "Приёмник", 300, 270, 150, 80);
        connections_.push_back(
            boxDrop->on(DxvUI::EventType::Drop, [](DxvUI::DxvEvent& e, const DxvUI::UIContext&) {
                const auto from = e.getRelatedNode();
                DxvUI::Log::info("[box_drop] Drop из '{}' в ({}, {})",
                                 from ? from->getId() : "<none>", e.mouse.x, e.mouse.y);
            }));
        root->addChild(boxDrop);

        // --- Самоудаление: после 3-го клика кнопка уходит из сцены (Detach),
        // а токен её подписки становится expired() и вычищается в главном цикле.
        auto btnSelf = makeButton("btn_self", "Самоудаление (3 клика)", 50, 200, 260, 40);
        connections_.push_back(btnSelf->on(
            DxvUI::EventType::Click, [state](DxvUI::DxvEvent& e, const DxvUI::UIContext&) {
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
    void runScriptedEvents() {
        auto move = [&](int x, int y, DxvUI::MouseButton held) {
            DxvUI::DxvEvent e;
            e.type = DxvUI::EventType::MouseMove;
            e.mouse.x = x;
            e.mouse.y = y;
            e.mouse.button = held;
            scene_->processEvent(e);
        };
        auto press = [&](int x, int y) {
            DxvUI::DxvEvent e;
            e.type = DxvUI::EventType::MouseDown;
            e.mouse.x = x;
            e.mouse.y = y;
            e.mouse.button = DxvUI::MouseButton::Left;
            scene_->processEvent(e);
        };
        auto release = [&](int x, int y) {
            DxvUI::DxvEvent e;
            e.type = DxvUI::EventType::MouseUp;
            e.mouse.x = x;
            e.mouse.y = y;
            e.mouse.button = DxvUI::MouseButton::Left;
            scene_->processEvent(e);
        };
        auto clickAt = [&](int x, int y) {
            move(x, y, DxvUI::MouseButton::None);
            press(x, y);
            release(x, y);
        };

        DxvUI::Log::info("=== Scripted demo (headless) ===");

        // Hover + Click с stopImmediatePropagation.
        move(160, 110, DxvUI::MouseButton::None);
        move(160, 110, DxvUI::MouseButton::None);  // повторное движение — hit-test кэш
        clickAt(160, 110);  // btn_click: Click + Change, корневой Click не увидит

        // Click с бабблингом до корня + фокус + клавиша.
        clickAt(400, 110);  // btn_bubble: Click доходит до корня, кнопка получает фокус
        DxvUI::DxvEvent key;
        key.type = DxvUI::EventType::KeyDown;
        key.key.sym = DxvUI::KeyCode::Space;
        scene_->processEvent(key);  // клавиша уходит в focused-ноду (btn_bubble)

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
        DxvUI::Log::info("[main] btn_self удалён: {}",
                         scene_->findNodeById("btn_self") ? "нет" : "да");

        // Мёртвые токены (нода уничтожена) вычищаются; деструктор токена безопасен.
        const size_t before = connections_.size();
        std::erase_if(connections_, [](const auto& c) { return c->expired(); });
        DxvUI::Log::info("[main] вычищено expired-токенов: {}", before - connections_.size());
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

    DxvUIEventsExample app;
    return app.run();
}
