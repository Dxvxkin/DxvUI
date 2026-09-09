# Рефакторинг системы отрисовки: анализ и план

Полный пересмотр конвейера отрисовки DxvUI: от `SceneNode::draw` до бэкенда.
Структура как в `ROADMAP.md`: «что, зачем, где в коде», целевые сигнатуры, этапы,
риски. Документ — предложение, ничего из описанного ещё не реализовано.

Обозначения мест в коде указывают файлы/функции для быстрой навигации.

---

## 1. Текущая система (as-is)

### 1.1 Кадровый цикл

Циклом владеет приложение (`examples/App.h`): хост сам делает `clear`, DxvUI
рисует поверх, хост делает `present`:

```
SDL_PollEvent ─► Scene::processEvent ─► Scene::update()
                                        │  ├─ StyleManager::resolveDirtyStyles(root)
                                        │  └─ LayoutManager::layout(root, viewport)
                                        ▼
host: renderer->clear(color) ─► Scene::draw() ─► host: present()
```

- `Scene::updateLayout()` — `src/Scene.cpp:114` (стили → layout, оба прохода
  прунятся по dirty-флагам).
- `Scene::draw()` — `src/Scene.cpp:136`: просто `root->draw(*renderer)`.
  Никакого кадрового контекста (время, dt, номер кадра, dpi) не передаётся.

### 1.2 Проход отрисовки

`SceneNode::draw(IRenderer&)` (`src/SceneNode.cpp:564`) — виртуальный
template-метод: берёт viewport у рендерера и зовёт приватный
`drawImpl(renderer, viewportRect)` (`src/SceneNode.cpp:570`):

1. `!visible` → выход (вся ветка пропускается);
2. culling: `!getGlobalBounds().intersects(viewportRect)` → выход
   (аппроксимация: ребёнок, вылезающий из полностью невидимого родителя,
   теряется — признано «редким случаем» в комментарии);
3. `drawBackground()` — хук; дефолт = `fillRoundRect(bounds, radius, bg, border)`
   из `ComputedAppearanceStyle` (`src/SceneNode.cpp:602`);
4. `clipContent` → `pushClipRect(getGlobalBounds())`;
5. `drawContent()` — хук; здесь рисуют Label/Checkbox/Slider-ы/TextEdit;
6. дети по порядку (после ленивой сортировки по zIndex —
   `sortChildrenIfDirty()`, мутирующей `children` прямо во время обхода);
7. `popClipRect()`.

Хуки — правильная идея; проблема в том, *что* им передают и *как* они рисуют.

### 1.3 Бэкенд

- `IRenderer` (`include/DxvUI/interfaces/IRenderer.h`) — **один интерфейс на
  30+ виртуальных методов**: жизненный цикл кадра (`clear/present`), состояние
  (`setDrawColor`), клип-стек, 24 перегрузки примитивов (Color- и
  Border-варианты drawRect/fillRect/circle/arc/roundRect/polygon), курсор,
  клипборд, текстовый движок, `drawTexture`.
- `SDLRenderer` (`src/backend/SDLRenderer.cpp`): скруглённые прямоугольники —
  свои GPU-пути через `SDL_RenderGeometry` (молодцы), но круги/дуги/полигоны —
  **CPU-растеризаторы sdl2-gfx** (`aacircleRGBA`/`filledPolygonRGBA`/…) на
  каждый вызов каждый кадр.
- `SDLTextEngine`: шрифты кэшируются по (path, size); текстуры — LRU 1024 по
  (font, text, color), **цвет запечён в текстуру**; кэш измерений `measures` —
  **неограниченная map** (`include/DxvUI/backend/SDLTextEngine.h:77`).
- `SDLTextEditorView` (`src/backend/SDLTextEditorView.cpp`): каретка мигает по
  `SDL_GetTicks()` прямо внутри живописи; `scrollOffsetX_` — мутируемое
  состояние вида; выравнивание текста продублировано в `draw` и `hitTestAt`.

### 1.4 Связь стиля и живописи

- `ComputedAppearanceStyle` → дефолтный `drawBackground`. Хорошо.
- Виджет-специфика — вне системы стилей: `Checkbox::drawContent` хардкодит
  `Colors::White` для бокса (`src/widgets/Checkbox.cpp:106`),
  `SliderHorizontal::drawContent` — `Colors::LightGray/CornflowerBlue/RoyalBlue`
  для трека/заливки/пальца (`src/widgets/SliderHorizontal.cpp:47`). Ни тема, ни
  состояние, ни пользователь не могут это поменять.

---

## 2. Проблемы

### 2.1 Корректность (баги, чинить независимо от рефакторинга)

1. **`pushClipRect` не пересекает клипы.** `SDL_RenderSetClipRect` *заменяет*
   прямоугольник, а стек в `SDLRenderer` только сохраняет/восстанавливает
   (`src/backend/SDLRenderer.cpp:218`). Вложенные `clipContent` не образуют
   пересечение: ScrollContainer (viewport 100px) + частично видимый TextEdit,
   торчащий за сгиб (`y=80..130`), после `pushClipRect(bounds)` текста рисует
   текст в `y=100..130` — **за пределами viewport скролла**, поверх того, что
   нарисовано ниже. Сценарий бенчмарка `clip` (вложенные clipContent) этого не
   ловит, потому что контент там целиком внутри/снаружи. Нужен юнит-тест с
   частично видимым вложенным клипом + пересечение при push.
2. **`drawTexture(std::shared_ptr<ITexture>&, const Rect&)`** — неконстантная
   lvalue-ссылка (нельзя передать временный/константный纹理) и
   `dynamic_cast<SDLTexture*>` при каждом вызове
   (`src/backend/SDLRenderer.cpp:307`): передать текстуру чужого бэкенда —
   молчаливый nullptr → UB. Абстракция дырявая ровно в одном месте, где она
   горячее всего.
3. **`measures` растёт без предела.** Кэш измерений
   `std::map<pair<const IFont*, string>, TextMetrics>` не ограничен (в отличие
   от LRU текстур, `kMaxTextureCacheEntries = 1024`). Динамический текст
   (сценарий `text`, счётчики FPS, значения слайдеров) копит записи бесконечно.
4. **Culling по родителю без учёта overflow.** Документированная
   аппроксимация (`src/SceneNode.cpp:575`), но она же и ложится на
   `clipContent`-контейнеры: у ScrollContainer ребёнок аранжируется за
   пределами bounds родителя по построению. Сегодня спасает то, что за
   пределами обычно и viewport. Нужен escape-hatch (флаг «не куллить детей»
   или расширенные bounds с учётом overflow).

### 2.2 Архитектура интерфейсов

5. **`IRenderer` — god-interface.** Кадр + живопись + платформенные сервисы +
   текст. Нарушение ISP с практическими последствиями: нельзя сделать
   «оптимизирующий» бэкенд (батчинг, display list, запись SVG/скриншотов,
   headless-тестовый канвас) без реализации курсора и клипборда; нельзя
   подменить текстовый движок, не трогая рендерер.
6. **Неявное глобальное состояние `setDrawColor`.** Перегрузки «без цвета»
   (`drawRect(rect)`, `drawCircle(cX,cY,r)`) зависят от того, кто какой цвет
   выставил до них. Это породило комбинаторный взрыв из 24 перегрузок и
   продолжает плодить их: чтобы добавить opacity/градиент/тень к каждому
   примитиву, придётся тронуть все 24.
7. **`drawTexture` без srcRect/tint/rotation/flip/alpha** и **без API создания
   текстур**: Image/Sprite-виджет из ROADMAP п.9 невозможен на текущем
   интерфейсе (нет загрузки картинки, нет 9-slice, нет подкраски).
8. **Курсор и клипборд — не живопись.** `EventManager` уже дёргает курсор
   через `IRenderer` (`src/EventManager.cpp:278`), перемешивая слои.
9. **`SceneNode::draw` виртуальный**, хотя это template-метод с контрактом
   (culling → background → clip → content → children → unclip). Подкласс,
   переопределивший его, ломает контракт молча. `measure/arrange` уже сделаны
   невиртуальными тонкими входами — `draw` остался исключением.
10. **zIndex — только сортировка соседей**, а `sortChildrenIfDirty()`
    мутирует `children` лениво из двух разных мест (draw и hit-test). Popup
    работает лишь потому, что его неявно добавляют последним ребёнком корня.
    Нет понятия оверлей-слоя/elevation.

### 2.3 Горячий путь (каждый кадр)

11. **Hover/press/focus = relayout.** `setHovered/setPressed/setFocused`
    зовут `markLayoutDirty()` (`src/SceneNode.cpp:296`), хотя в 99% случаев
    меняется только фон. Каждое наведение мыши на любой виджет — повторный
    measure+arrange ветки до корня. Механизм диффа уже есть
    (`detail::layoutPropsDiffer` в `setStyle`, `src/SceneNode.cpp:182`) —
    смена состояния должна идти через него, а не через безусловный layout-dirty.
12. **Label::drawContent** (`src/widgets/Label.cpp:82`): на кадр на лейбл —
    - `getText()` → `UIBinding::getString()` — **мьютекс + аллокация строки**
      (`include/DxvUI/UIBinding.h:86`);
    - `getFontForFamily` — поиск по map + копия shared_ptr;
    - `charIndexAtX` (усечение до ширины) — бинарный поиск с lookup-ами;
    - `rasterize` — LRU-lookup по ключу-строке.
    Ничего из этого не кэшировано между кадрами, хотя при неизменном тексте и
    bounds результат побайтово тот же.
13. **TextEdit/скролл текста.** Видимый срез растеризуется как **подстрока**
    (`text.substr(visibleStart, …)`, `src/backend/SDLTextEditorView.cpp:86`):
    каждый новый scroll-офсет — новый ключ (font, text, color) → LRU-прогон.
    Глифовый конвейер (атлас + tint) снимает и это, и «цвет в текстуре».
14. **CPU-примитивы sdl2-gfx** на каждый кадр: палец слайдера — `fillCircle` →
    `filledCircleRGBA` — сканлайн-заливка на CPU; дуги и полигоны так же. При
    этом скруглённые прямоугольники уже идут через `SDL_RenderGeometry` на GPU
    — два несогласованных растеризатора в одном бэкенде.
15. **Нет кадрового контекста.** Анимации (моргание каретки) читают
    `SDL_GetTicks()` изнутри вида; централизованные твины/переходы состояний
    невозможны — нечего передать в draw.
16. **Полная перерисовка каждый кадр** без damage-модели. Для SDL-окна это
    терпимо, но в связке с п.11–14 даёт заметный CPU-пол на больших сценах, и
    это главное, что ограничивает бенчмарк `frames`.

### 2.4 Расширяемость и стиль

17. **Виджет-специфика вне темы** (см. 1.4): чтобы сделать трек слайдера
    темизируемым, надо добавить поле в `StyleRule` + дескриптор + computed —
    по свойству на каждый визуальный элемент каждого виджета.
18. **Дублирование выравнивания текста** в `Label::drawContent`,
    `SDLTextEditorView::draw`, `hitTestAt` — три копии одного switch по
    `Alignment`.
19. **`Rect` = int, `Size` = float** (`include/DxvUI/core.h`): arrange
    обрезает дробное, subpixel-позиционирование текста невозможно (нет
    `SDL_RenderCopyF`-пути), HiDPI (ROADMAP п.10) не на что опереть.

---

## 3. Целевая архитектура (to-be)

Ключевая идея: **разделить «что нарисовать» и «как рисовать»**, а живопись
вынести в узкий контракт `ICanvas`, в который бэкенд может добавить батчинг,
render-target-кэш и GPU-геометрию, не трогая виджеты.

```
Scene::draw(FrameInfo)
  └─ SceneNode::draw(PaintContext)            // невиртуальный template-метод
       ├─ onPaintBackground(PaintContext)     // хук (дефолт: Brush из стиля)
       ├─ [pushClip(bounds) при clipContent]  // ClipGuard, RAII
       ├─ onPaint(PaintContext)               // хук (бывший drawContent)
       └─ дети

PaintContext = { ICanvas&, const FrameInfo&, ITextRenderer& }

ICanvas        — только живопись: примитивы с Brush, клип с пересечением,
                 текстуры с srcRect/tint/flip/alpha
IRenderBackend — владение окном/целью, beginFrame/endFrame, создание текстур
                 и render-target-ов, dpi
IPlatformServices — курсор, клипборд (инжектится в Scene, не в канвас)
ITextRenderer  — TextLayout (глифы) + отрисовка layout-а в канвас
```

### 3.1 Канвас и Brush (замена setDrawColor и 24 перегрузок)

```cpp
struct Fill   { Color color; };                       // завтра: градиент/паттерн
struct Stroke { Color color; float thickness = 1.0f; };
struct Brush  {                                   // то, что сегодня раскладывается
    std::optional<Fill> fill;                     // на перегрузки (rect, border)
    std::optional<Stroke> stroke;                 // и «fill + border»
};

class ICanvas {
   public:
    virtual ~ICanvas() = default;

    virtual void fillRect(const RectF&, const Fill&) = 0;
    virtual void strokeRect(const RectF&, const Stroke&) = 0;
    virtual void fillRoundRect(const RectF&, float radius, const Brush&) = 0;
    virtual void fillCircle(PointF center, float radius, const Brush&) = 0;
    virtual void strokeArc(PointF center, float radius, float a0, float a1,
                           const Stroke&) = 0;
    virtual void fillPolygon(std::span<const PointF>, const Fill&) = 0;
    virtual void drawLine(PointF a, PointF b, const Stroke&) = 0;

    // Один метод вместо «drawTexture(shared_ptr<ITexture>&, Rect)»:
    struct TextureDraw {
        RectF dst;
        std::optional<RectF> src;   // атласы, 9-slice, срез видимости
        std::optional<Color> tint;  // подкраска глифов/спрайтов без перерастеризации
        float alpha = 1.0f;
        float rotationDeg = 0.0f;
        bool flipX = false, flipY = false;
    };
    virtual void drawTexture(const TextureRef&, const TextureDraw&) = 0;

    // Клип: пересекает с текущим (сегодня — нет, см. п.1), возвращает токен.
    // Пустое пересечение => корректный «ничего не рисовать».
    [[nodiscard]] virtual ClipToken pushClip(const RectF&) = 0;
    virtual void popClip(const ClipToken&) = 0;
};
```

- ~10 методов вместо 30+; добавление opacity/тени — расширение `Brush`, а не
  интерфейса.
- Клип-стек обязан пересекать (семантика scissors). `ClipToken` + RAII-guard
  `ClipGuard` в `drawImpl` убирает ручной push/pop.
- `TextureRef` — тонкий дескриптор (тип-тег + указатель), исключающий
  `dynamic_cast` на каждый вызов и неконстантную ссылку.

### 3.2 Кадровый контекст

```cpp
struct FrameInfo {
    double timeMs = 0;      // монотонное время кадра
    float  dtMs = 0;        // с предыдущего кадра
    uint64_t frame = 0;     // номер кадра
    RectF  viewport;        // уже с учётом dpi
    float  dpiScale = 1.0f; // ROADMAP п.10 (HiDPI)
};
```

`Scene::draw(const FrameInfo&)`; каретка перестаёт читать `SDL_GetTicks()` в
недрах вида (`SDLTextEditorView::isCaretVisible`) — мигание становится
функцией `FrameInfo`, а сам вид переезжает из `backend/` в `text/` (от SDL там
останется ничего). Открывается дорога к декларативным переходам состояний
(hover-fade и т.п.) на тех же дифф-механизмах стиля.

### 3.3 Текстовый конвейер: строки → глифы

```cpp
struct TextStyle { std::string family; int size; Color color; /* weight, … */ };

class ITextRenderer {
   public:
    virtual ~ITextRenderer() = default;
    virtual std::shared_ptr<IFont> font(const std::string& family, int size) = 0;

    // Кэш TextLayout ограничен LRU (как текстуры сегодня). Layout — это
    // глифы+позиции+метрики каретки; для кириллицы/латиницы достаточно
    // TTF_GetGlyphMetrics + kerning; HarfBuzz — отдельная задача (см. §7).
    virtual TextLayout layout(const IFont&, std::string_view utf8) = 0;

    // Рисует layout в блоке с выравниванием/усечением/эллипсисом.
    // Единственное место со switch по Alignment (сегодня их три, п.18).
    virtual void drawLayout(ICanvas&, const TextLayout&, const RectF& box,
                            const TextPaint&) = 0;
};
```

- **Глиф-атлас** на (font, size): белые глифы, цвет — `tint` при отрисовке.
  Ключ растеризации схлопывается с (font, **text**, **color**) до (font):
  память и промахи падают на порядки в сценариях `text`/`micro`.
- Скролл текста больше не создаёт текстур на подстроки (п.13): рисуются глифы
  срезом через `src`/позиции.
- Усечение лейбла (сегодня `charIndexAtX` каждый кадр, п.12) становится
  частью `drawLayout` и кэшируется в layout-е.
- Горячий путь Label: на кадр остаётся один lookup layout-а по (font, text) и
  один `drawLayout` — без мьютекса `UIBinding` и аллокаций (см. также §3.5).

### 3.4 Инвалидация: визуальная ≠ геометрическая

Сегодня два флага (style-dirty, layout-dirty) — и оба игнорируются при смене
состояния, которая безусловно валит layout (п.11). Целевое:

```cpp
// Смена hover/press/focus:
void SceneNode::updateInteractionState(/* … */);
//   1) выставляет флаги;
//   2) markStyleDirty();                 // StyleManager пересчитает computed
//   3) дифф computed-layout Old vs New;  // как в setStyle сегодня
//      если layout-свойства совпали → только visual-dirty (repaint), не relayout.

// Плюс накопление damage-прямоугольников в Scene:
//   move/arrange с изменением bounds → dirty(oldBounds ∪ newBounds);
//   visual-dirty → dirty(bounds).
// Clean-кадр: Scene::draw может вернуть «рисовать нечего» (для embedded/soft-GPU).
```

Damage-модель — единственная позиция с реальным риском усложнения; поэтому
она **опциональна** (этап 6) и принимается только по числам бенчмарка.

### 3.5 Разделение бэкенда и сервисов

```cpp
class IRenderBackend {                       // бывший «владелец ресурсов»
   public:
    virtual ~IRenderBackend() = default;
    virtual ICanvas& beginFrame(const Color& clear) = 0;  // + acquire
    virtual void endFrame() = 0;                          // + present
    virtual Size outputSize() const = 0;
    virtual float dpiScale() const = 0;
    virtual TextureRef createTexture(const ImageData&) = 0;   // Image-виджет!
    virtual std::unique_ptr<ICanvas> createRenderTarget(Size) = 0;  // кэш поддеревьев
};

class IPlatformServices {                    // курсор, клипборд
   public:
    virtual ~IPlatformServices() = default;
    virtual void setCursor(CursorType) = 0;
    virtual IClipboard& clipboard() = 0;
};
```

- `EventManager` получает `IPlatformServices&`, а не `IRenderer*`
  (убирается зависимость событий от живописи, п.8).
- `Scene` владеет `ITextRenderer`-ом напрямую (сегодня он «внутри» рендерера
  только потому, что SDL-текстурам нужен SDL_Renderer — это забота бэкенда,
  а не причина держать текст в том же интерфейсе).
- `clear/present` покидают контракт живописи: `Scene::draw` получает уже
  готовый `ICanvas` от `beginFrame` (в external-режиме хост продолжает
  владеть кадром — контракт `external_renderer.cpp` сохраняется).

### 3.6 Оверлеи и z-порядок

- Ввести в `Scene` явный **overlay-слой** (AbsoluteContainer, рисуемый и
  хит-тестящийся последним): Popup/Tooltip/Menu больше не зависят от того,
  что их добавили последним ребёнком корня (п.10).
- `zIndex` остаётся порядком соседей; глобальная elevator-модель не нужна.

### 3.7 Виджет-специфика через тему

Не расширять `StyleRule` полем на каждый пиксель виджета, а дать темам
**именованные кисти**:

```cpp
// В Theme: регистр «токенов» поверх тех же дефолтов по типам узлов.
theme.declareBrush("slider.track",   {.fill = LightGray});
theme.declareBrush("slider.fill",    {.fill = CornflowerBlue});
theme.declareBrush("slider.thumb",   {.fill = CornflowerBlue, .stroke = RoyalBlue});

// В виджете:
canvas->fillRoundRect(track, radius, brushes().get("slider.track"));
```

Токены наследуются/переопределяются как стили; хардкод `Colors::*` в
`Checkbox/Slider*` уходит, ROADMAP п.9 (ProgressBar и др.) перестаёт требовать
правок ядра стилей.

---

## 4. Этапы (каждый — отдельный коммит/PR, тесты зелёные, бенчмарк A/B)

Шкала: этапы 0–3 — ядро предложения; 4–5 — завершение разделения; 6 —
опциональные оптимизации «по числам».

| # | Что | Затрагивает | Критерий готовности |
|---|-----|-------------|---------------------|
| 0 | **Баги**: пересечение клипа; константный `drawTexture` + `TextureRef`-токен; LRU-граница `measures`; тест вложенного клипа | `SDLRenderer`, `SDLTextEngine` | новый тест ловит утечку клипа; `text`-сценарий не растит память |
| 1 | **PaintContext + ICanvas** адаптером поверх текущего `SDLRenderer` (`CanvasAdapter`); `SceneNode::draw(const PaintContext&)` становится невиртуальным; хуки переименовываются `drawContent → onPaint`, `drawBackground → onPaintBackground`; `ClipGuard` | `SceneNode`, все виджеты (механическая миграция ~6 файлов) | все 300+ тестов зелёные; примеры рисуют идентично |
| 2 | **Brush-свертка**: `fillRect/strokeRect/fillRoundRect/...` с `Fill/Stroke`; дефолтный `onPaintBackground` на `Brush`; убрать `setDrawColor` из контракта | `IRenderer→ICanvas`, виджеты | в `ICanvas` ≤ 12 методов; перегрузки удалены |
| 3 | **TextLayout + глиф-атлас**: `ITextRenderer`, `drawLayout` с выравниванием/усечением; tint вместо запечённого цвета; миграция Label/TextEdit/TextEditorView (вид уходит из `backend/`) | `SDLTextEngine → SdlTextRenderer`, Label, TextEdit | бенчмарк `text`/`micro`: медиана не хуже, память кэша −90% ожидаемо |
| 4 | **Инвалидация состояний**: `setHovered/Pressed/Focused` через style-дифф; юнит-тест «hover не меняет bounds»; `getString()`-хотспот: Label кэширует строку между Change | `SceneNode`, Label | «hover-storm»-сценарий: relayout-ов 0 при неизменной геометрии |
| 5 | **Разделение бэкенда**: `IRenderBackend` + `IPlatformServices`; `FrameInfo` сквозь `Scene::draw`; миграция `EventManager` (курсор) | Scene, EventManager, examples | `ICanvas` не знает про окно/курсор/клипборд |
| 6 | *(опционально, по числам)*: display-list/батчинг примитивов; render-target-кэш статичных поддеревьев (ScrollContainer-контент); GPU-пути для circle/arc/polygon **и удаление зависимости sdl2-gfx**; damage-ректы; shadow/gradient в `Brush`; Image-виджет поверх `createTexture` | backend | compare.ps1: −10%+ на затронутых сценариях без регрессий |

Дополнительно к этапу 1: `RectF`/`PointF` вводятся вместе с `ICanvas`
(канвас сразу флоатный, конверсия на границе layout→paint), а полная миграция
layout-а на флоат остаётся вне скоупа (см. §7).

Порядок обусловлен риском: 0 чинит поведение независимо; 1–2 меняют форму
вызовов без изменения семантики; 3 — единственный этап с заменой алгоритма
(текст), он изолирован интерфейсом `ITextRenderer`; 4–5 — распределение
ответственностей; 6 — чистые оптимизации за фасадом.

---

## 5. Совместимость и риски

- **Публичный API ломается** (`draw`, `IRenderer`): это major-версия пакета
  (SameMajorVersion в `DxvUIConfig` уже не пропустит старых потребителей).
  Внутри репо точки миграции сосредоточены: `on()`-обработчиков это не
  касается; `drawContent` переопределяют 5 виджетов; `IRenderer` реализует
  один класс.
- **Риск регрессий** снижается существующим аппаратом: 300+ GTest как
  оракул, `examples/benchmark.cpp --json` + `scripts/compare.ps1` (порог
  регрессии 10%), headless-прогон примеров (`DXVUI_FRAMES`).
- **Glиф-атлас**: риск артефактов на границах глифов — лечится атласом с
  padding-ом и `SDL_TEXTUREACCESS_STATIC`; мигание/субпиксели — рендер в
  целый пиксель с округлением позиций layout-а.
- **Пересечение клипа** (этап 0) меняет видимое поведение там, где сегодня
  «утекает» — это фикс бага, но прогнать примеры (`scroll`, `popup`,
  `textedit`) обязателен.
- **Damage/батчинг (этап 6)** — главный источник сложности; не принимается
  без выигрыша в бенчмарке и не блокирует этапы 0–5.

## 6. Метрики приёмки

- `frames`/`scroll`/`hit`/`text`/`clip`/`micro`: медианы не хуже baseline
  (кроме целевых улучшений: `text` и `micro` ждём лучше).
- `getTextureCacheCount()` в `text`-сценарии: плато ниже текущего (ключ без
  text/color).
- Новый сценарий `hover` (движение мыши по сетке кнопок без resize): 0
  relayout-ов на кадр после этапа 4 (счётчик в LayoutManager под
  `#ifdef DXVUI_DEBUG_LAYOUT_STATS`).
- Юнит-тесты: вложенный клип (утечка за пределы внешнего clip-а),
  «hover не меняет bounds», «Label не аллоцирует на чистом кадре»
  (счётчик аллокаций в тестовом канвасе).

## 7. Открытые вопросы (зафиксировать перед стартом)

1. Имена: `ICanvas` vs `IPainter`; `onPaint` vs сохранить `drawContent`.
2. `TextureRef` — value-дескриптор или `shared_ptr<ITexture>` с type-тегом?
3. Глиф-атлас: один атлас на шрифт или глобальный с аллокатором страниц?
4. Вводить ли `RectF` в layout сразу (полная миграция) или только в paint
   (рекомендуется — только в paint).
5. Идти ли на HarfBuzz сейчас (шейпинг RTL/лигатур) или отложить
   (рекомендуется отложить: кириллица/латиница считаются TTF-метриками).
6. Нужен ли headless-канвас (запись PaintOps для тестов/скриншот-тестов) как
   первый потребитель `ICanvas` (рекомендуется: да, он же — оракул этапа 1).

## 8. Что сознательно не входит

- Многопоточный рендеринг и многооконность (ROADMAP п.10 отдельно).
- Полноценный retained-mode со сценеграфом на GPU: цель — разделение слоёв и
  почва под оптимизации, не переписывание в retained.
- Модель «виджеты = чистые функции от состояния» (immediate-mode): текущая
  retained-модель сохраняется.
- Rich-text/многострочность: `TextLayout` проектируется так, чтобы расширить
  до списков строк, но реализация multiline — отдельная задача после этапа 3.
