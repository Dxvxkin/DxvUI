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

### A. Типизированный payload

- Сохранить `EventType` как плоский enum (совместимость ключей), но типизировать
  доступ к данным. Рекомендуемый вариант: `DxvEvent` с типизированными геттерами
  поверх внутреннего хранилища (обратная совместимость с ручным созданием событий
  в `examples/`/`tests/`: `DxvEvent e; e.type=...; ...`).
- Ввести типовые группы: `MouseEvent` (x/y/dx/dy/button), `KeyEvent`
  (sym/mod/repeat[/scancode]), `TextEvent` (text), `ResizeEvent` (w/h),
  `ChangeEvent`/`FocusEvent`/`HoverEvent` (target/related).
- Добавить `EventType::Submit`, `DragStart`, `DragEnd`, `DragOver`.

### B. Единая маршрутизация

- Декларативная таблица «тип события → стратегия цели» (hit-test/hovered/focused/root)
  вместо пяти веток switch в `processRawEvent`.
- Синтез производных событий собрать в задокументированную единую точку.

### C. Двухфазный проход

- `SceneNode::dispatchEvent`: capture → target → bubble, запускается один раз по
  `event.currentTarget == nullptr`.
- Новый хук `virtual void onCapture(DxvEvent&)` (root→target, без цели).
- `stopPropagation()` в capture останавливает спуск И bubble.
- Гейт: capture только для raw-input типов (не гонять `Change`/`Attach`/`Detach`/`
  Focus`/`Hover`).

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
