# DxvUI Roadmap

План развития API, виджетов и системы событий DxvUI, сгруппированный по приоритетам.
Каждый пункт — «что, зачем, где в коде». Раздел «Стандартизация системы событий»
содержит детальный план переработки базы событий (полный пересмотр).

Обозначения мест в коде указывают ключевые файлы/строки для быстрой навигации.

---

## Приоритет 1 — Фундамент API и событий

### 1. Стандартизация системы событий (полный пересмотр базы)

Детальный план — в отдельном разделе ниже. Это самый крупный пункт: затрагивает
`DxvEvent`, `EventManager`, `SceneNode::dispatchEvent` и все виджеты.

### 2. Понятие unbounded-доступности в measure

- **Что:** ввести явный способ сказать «пространство без ограничений» в `onMeasure`.
  Сейчас `Size` — голый float-пара (`core.h`), нет Infinity-сентинеля, поэтому
  контейнеры вынуждены угадывать (`min(content, available)` в
  `ScrollContainer::onMeasure`).
- **Зачем:** честный intrinsic-размер, wrap, «расти до максимума» без костылей.
- **Где:** `include/DxvUI/core.h`, `src/layout/LayoutManager.cpp` (`onMeasure`-pass).

### 3. Типизация горячих полей

- **Что:** убрать лишние аллокации в горячих путях measure/draw (см. также п.7
  про `UIBinding`).
- **Зачем:** производительность в `benchmark` (text/micro сценарии).
- **Где:** `Label::onMeasure`/`drawContent`, `TextEdit`, `SliderBase`.

---

## Приоритет 2 — Убрать плюрализм и дубли

### 4. Согласовать двойную модель обработки события

- **Что:** задокументировать/унифицировать две конкурирующие модели реакции:
  `virtual onEvent` (default-action) и `on()`-listeners (`dispatchEvent`).
- **Зачем:** сейчас семантика порядка и `preventDefault` не очевидна, легко ошибиться.
- **Где:** `SceneNode::dispatchEvent` (`src/SceneNode.cpp`), хук `onEvent`.

### 5. Стандартизировать `EventType::Change` (источник vs проекция)

- **Что:** единое внутреннее устройство канала `Change`. Сейчас TextEdit использует
  binding как *проекцию* модели (источник истины — `TextEditor`), а Label/Slider —
  как *хранилище*.
- **Зачем:** один публичный API `on(Change)`, но поведение зависит от внутреннего
  устройства; уже приводило к багам (рефакторинг TextEdit в `08decf1`).
- **Где:** `widgets/TextEdit`, `widgets/Label`, `widgets/SliderBase`.

### 6. `getNodeType()`-строка → RTTI; закрыть TODO в bind

- **Что:** заменить строковый `kWidgetType` на RTTI-идентификацию типов
  (issue #4), закрыть `//TODO` в `SceneNode::bind()` (присваивание binding без
  проверок).
- **Зачем:** риск расхождения строки с типом; безопаснее перепривязка.
- **Где:** `include/DxvUI/widgets/*`, `src/SceneNode.cpp` (`bind`).

---

## Приоритет 3 — Производительность и наполнение

### 7. `UIBinding`: мутекс/копии колбэков/аллокации в горячем пути

- **Что:** `UIBinding::set` блокирует мутекс и копирует весь вектор колбэков
  (`src/UIBinding.cpp`); `getString()` аллоцирует строку (`UIBinding.h`).
- **Зачем:** однопоточный UI — лишние затраты на каждый `set`/чтение.
- **Где:** `include/DxvUI/UIBinding.h`, `src/UIBinding.cpp`.

### 8. Scrollbars, фокус-навигация, наследование disabled

- **Что:** полосы прокрутки в `ScrollContainer`; навигация по фокусу (Tab/arrows)
  между фокусируемыми виджетами; модель «контейнер отключён → дети отключены».
- **Зачем:** удобство и завершённость по умолчанию.
- **Где:** `containers/ScrollContainer`, `EventManager` (focus), `SceneNode`.

### 9. Виджеты: Dropdown, ProgressBar, Image/Sprite, Dialog/Modal

- **Что:** новые виджеты. Dropdown поверх `Popup`+списка; ProgressBar переиспользует
  паттерн `SliderBase`; Image/Sprite для отрисовки текстур; Dialog/Modal — поверх
  capture-фазы событий.
- **Зачем:** закрыть основные «почему нет X» в API.
- **Где:** `include/DxvUI/widgets/`, `include/DxvUI/containers/`.

---

## Приоритет 4 — Долгосрочные

### 10. Multi-scene / modal-стеки, HiDPI, touch

- **Что:** несколько сцен / стек модальных окон; поддержка
  `devicePixelRatio`/HiDPI в `IRenderer`; сенсорный ввод.
- **Зачем:** масштабирование для реальных приложений.
- **Где:** `Scene`, `EventManager`, `IRenderer`, `backend/`.

---

# Стандартизация системы событий (детальный план)

Полный пересмотр базы событий для понятного и предсказуемого поведения.
Выполняется в отдельной ветке `feat/event-system`.

## 1. Проблемы

1. **Плоский `DxvEvent` без типизации.** Raw и производные события делят одно
   `mouse`/`key`/`text`/`resize` поле. `MouseMove` vs `Drag` семантически разные,
   но несут одни поля. Payload нельзя расширить (scancode, drop-files, scale) без
   правки общего struct. Где: `include/DxvUI/DxvEvent.h`.
2. **Размазанный синтез.** `Click`/`Drag`/`Drop`/`Hover`/`Focus` шьются императивно
   в `EventManager::handleMouseUp/MouseMove`; `Change` — отдельно в
   `SceneNode::onBindingChange`. Нет единой точки превращения raw → derived.
3. **Пять разных правил маршрутизации** в `EventManager::processRawEvent`
   (hit-test / hovered / focused / root по типу) — не масштабируется.
4. **Двойная модель реакции** (`virtual onEvent` + `on()`-listeners +
   `preventDefault`) не согласована.
5. **Нет фаз / модальности / фокус-порядка** — нельзя перехватить событие до его
   цели (нужно для Dialog/Modal/Tooltip).

Контур затронутого кода мал и управляем: `onEvent` переопределяют ровно 4 виджета
(`Checkbox`, `SliderBase`, `TextEdit`, `ScrollContainer`), `dispatchEvent` — только
базовая реализация, `on()`-listeners — 85 точек в `src/`/`examples/`/`tests/`.

## 2. Целевая модель

### A. Типизированный payload — сигнатуры (с вариантами)

Цель: сохранить обратную совместимость записи (examples/tests создают
`DxvEvent e; e.type=...; e.mouse.x=...`), но дать структурированный типизированный
доступ при чтении. `EventType` остаётся плоским enum (совместимость ключей).

**Payload-структуры (общие для обоих вариантов):**
```cpp
struct MouseEventData {
    int x = 0, y = 0, dx = 0, dy = 0;      // Move-deltas в dx/dy
    MouseButton button = MouseButton::None;
};
struct WheelEventData {                    // раздельный wheel (вместо mouse.dx/dy)
    int dx = 0, dy = 0;                    // dy>0 = вверх
    PointI position = {0, 0};
};
struct KeyEventData {
    KeyCode sym = KeyCode::Unknown;
    uint16_t mod = 0;
    uint8_t repeat = 0;
    uint16_t scancode = 0;                 // НОВОЕ: физический ключ
};
struct TextEventData { std::string text; }; // доступ .text (совместимо)
struct ResizeEventData { int width = 0, height = 0; };
```

**Вариант А — иерархия классов (строгая типизация, ЛОМАЕТ запись):**
```cpp
class DxvEventBase {
   public:
    virtual ~DxvEventBase() = default;
    virtual EventType type() const = 0;
    virtual std::shared_ptr<SceneNode> target() const = 0;
    void stopPropagation(); void stopImmediatePropagation(); void preventDefault();
    // + isPropagationStopped()/isImmediatePropagationStopped()/isDefaultPrevented()
};
class MouseEvent : public DxvEventBase {
   public:
    EventType type() const override;   // MouseDown/Up/Move/Click по конструктору
    MouseEventData data;
    int x() const; int y() const; int dx() const; int dy() const;
    MouseButton button() const;
};
class KeyEvent   : public DxvEventBase { /* KeyEventData data; sym()/mod()/repeat()/scancode() */ };
class WheelEvent : public DxvEventBase { /* WheelEventData data; */ };
class TextEvent  : public DxvEventBase { /* TextEventData data; text() */ };
class ResizeEvent: public DxvEventBase { /* ResizeEventData data; */ };
class ChangeEvent: public DxvEventBase { /* ... */ };
class FocusEvent : public DxvEventBase { /* ... */ };
class HoverEvent : public DxvEventBase { /* ... */ };
class SubmitEvent: public DxvEventBase { /* ... */ };
```
«+» строгая типизация / dynamic_cast; «−» ломает ручное создание в examples/tests,
требует масштабной миграции.

**Вариант Б — единый `DxvEvent` с типизированными struct + геттерами (РЕКОМЕНДУЕТСЯ — совместимость записи):**
```cpp
struct DxvEvent {
    EventType type = EventType::None;
    // Write API (обратная совместимость: примеры/тесты пишут напрямую)
    MouseEventData mouse;
    WheelEventData wheel;      // НОВОЕ, раздельное
    KeyEventData key;
    TextEventData text;
    ResizeEventData resize;
    // Read API (типизированный доступ)
    [[nodiscard]] int mouseX() const;  [[nodiscard]] int mouseY() const;
    [[nodiscard]] int wheelDx() const; [[nodiscard]] int wheelDy() const;
    [[nodiscard]] KeyCode keySym() const;
    // target/currentTarget/relatedNode + getTarget()/getCurrentTarget()/getRelatedNode()/getTargetId()
    void stopPropagation(); void stopImmediatePropagation(); void preventDefault();
    bool isPropagationStopped() const;
    bool isImmediatePropagationStopped() const;
    bool isDefaultPrevented() const;
};
```
Ключевое: `wheel` разделяется с `mouse` — устраняет смысловой конфликт deltas у Move
vs Wheel, но ломает текущий `e.mouse.dx` у wheel (мигрируем 4 виджета + тесты сразу,
либо временные алиасы на переходный период).

**Новые EventType:** `Submit`, `DragStart`, `DragEnd`, `DragOver` (payload-полей не
требуют; данные в `mouse`/`key`/`text`).

### B′. Единая маршрутизация — сигнатура (шаг 3)

```cpp
enum class RoutingTarget { HitTest, Hovered, Focused, Root, None };
struct RoutingRule { EventType type; RoutingTarget target; };
//  MouseDown/Up/Move -> HitTest
//  MouseWheel        -> Hovered   (НЕ ТРОГАТЬ, текущее поведение)
//  KeyDown/KeyUp/TextInput -> Focused
//  Quit/Resize       -> Root
//  Click/Drag*/Drop/Hover/Focus -> None (синтез в EventManager)
std::shared_ptr<SceneNode> resolveTarget(const DxvEvent& event) const;  // EventManager
```

### C′. Capture-фаза — сигнатура (шаг 4; РЕКОМЕНДАЦИЯ: хостинг в EventManager, не в dispatchEvent)

```cpp
// EventManager
std::vector<std::shared_ptr<SceneNode>> buildPathToTarget(
    const std::shared_ptr<SceneNode>& target) const;        // root->...->target
void dispatchWithCapture(DxvEvent& event);                  // capture(root..предок) + dispatch(target)
// SceneNode: новый virtual hook
virtual void onCapture(DxvEvent& event);                    // default no-op
```
Осторожно: `dispatchEvent` имеет guard `if (!event.getTarget()) return;`
(`src/SceneNode.cpp:379`); `Change` идёт через `onBindingChange` мимо EventManager —
capture его не трогает (соответствует гейту ниже).
Гейт: capture только для raw-input типов (не гонять `Change`/`Attach`/`Detach`/`
Focus`/`Hover`). `stopPropagation()` в capture отменяет спуск И bubble.

### Открытые решения (зафиксировать на старте сеанса)

1. Вариант А или Б (рекоменд. Б).
2. Wheel-разделение: мигрировать сразу или алиасами `mouse.dx/dy` → wheel (рекоменд. сразу).
3. `TextEventData`: поле `.text` (совместимо) или `.value`+геттер (рекоменд. `.text`).

### D. Согласованная модель реакции

- Единый порядок по фазам: capture-listener → target-listener → capture-default →
  target-default → bubble. Чётко документированные `preventDefault`/
  `stopPropagation`/`stopImmediatePropagation`.

## 3. Этапы (ветка `feat/event-system`, каждый шаг — отдельный коммит с зелёными тестами)

1. **Ветка + оракул.** От `master`, зафиксировать 293 зелёных теста как baseline.
2. **Типизация payload** (`DxvEvent`, `SDLEventSource`), обратная совместимость
   записи. `ActionCallback` не менять.
3. **Единая маршрутизация** `EventManager` (таблица) + сборка синтеза в одну точку.
   Поведение не менять (тесты — оракул).
4. **Двухфазный проход (capture).** `virtual onCapture` + проход в `dispatchEvent`,
   тесты фазы (порядок, stopPropagation, регрессия).
5. **Согласование модели реакции.** Унифицировать семантику флагов по фазам;
   обновить докблоки 4 виджетов, поведение не менять.
6. **Submit/Drag как общий механизм.** `EventType::Submit` (перенос логики
   `TextEdit::setOnSubmit` в стандартный канал, аналог миграции Change в `08decf1`),
   простейшие `DragStart/DragEnd`.
7. **Полировка.** clang-format, полный билд, 293+ новых теста зелёные, прогон
   примеров headless.

## 4. Риски и снижение

- **Обратная совместимость записи `DxvEvent`** (examples/tests создают события
  вручную) — снижение: сохранить поля / дать удобные конструкторы.
- **85 call-site `on()`** — снижение: сигнатуру `ActionCallback` не менять.
- **Регрессия** — снижение: тесты-оракул 293 + новые тесты на каждом шаге;
  независимый коммит на шаг.

## 5. Что НЕ входит

- Унификация `Change` «источник vs проекция» — отдельная задача (TextEdit уже
  частично сделан, см. п.5 Приоритета 2).
- `getNodeType`/RTTI (issue #4) — отложено.
- Модальный стек / outside-click-dismiss — поверх capture, при реализации
  Popup/Modal.
