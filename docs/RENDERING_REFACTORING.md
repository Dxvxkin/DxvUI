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
| 0 | ✅ **Баги**: пересечение клипа; константный `drawTexture` + проверка типа; LRU-граница `measures`; тест вложенного клипа | `SDLRenderer`, `SDLTextEngine` | новый тест ловит утечку клипа; `text`-сценарий не растит память |
| 1 | ✅ **PaintContext + ICanvas** адаптером поверх текущего `SDLRenderer` (`CanvasAdapter`); `SceneNode::draw(const PaintContext&)` становится невиртуальным; хуки переименовываются `drawContent → onPaint`, `drawBackground → onPaintBackground`; `ClipGuard` | `SceneNode`, все виджеты (механическая миграция ~6 файлов) | все 300+ тестов зелёные; примеры рисуют идентично |
| 2 | ✅ **Brush-свертка**: `fillRect/strokeRect/fillRoundRect/...` с `Fill/Stroke`; дефолтный `onPaintBackground` на `Brush`; убрать `setDrawColor` из контракта | `IRenderer→ICanvas`, виджеты | в `ICanvas` ≤ 12 методов; перегрузки удалены |
| 3 | ✅ **TextLayout + глиф-атлас**: `ITextRenderer`, `drawLayout` с выравниванием/усечением; tint вместо запечённого цвета; миграция Label/TextEdit/TextEditorView (вид уходит из `backend/`) | `SDLTextEngine → SdlTextRenderer`, Label, TextEdit | бенчмарк `text`/`micro`: медиана не хуже, память кэша −90% ожидаемо |
| 4 | ✅ **Инвалидация состояний**: `setHovered/Pressed/Focused` через style-дифф; юнит-тест «hover не меняет bounds»; `getString()`-хотспот: Label кэширует строку между Change | `SceneNode`, Label | «hover-storm»-сценарий: relayout-ов 0 при неизменной геометрии |
| 5 | ✅ **Разделение бэкенда**: `IRenderBackend` + `IPlatformServices`; `FrameInfo` сквозь `Scene::draw`; миграция `EventManager` (курсор) | Scene, EventManager, examples | `ICanvas` не знает про окно/курсор/клипборд |
| 6 | *(опционально, по числам)*: display-list/батчинг примитивов; render-target-кэш статичных поддеревьев (ScrollContainer-контент); GPU-пути для circle/arc/polygon **и удаление зависимости sdl2-gfx**; damage-ректы; shadow/gradient в `Brush`; Image-виджет поверх `createTexture` | backend | compare.ps1: −10%+ на затронутых сценариях без регрессий |

Дополнительно к этапу 1: `RectF`/`PointF` вводятся вместе с `ICanvas`
(канвас сразу флоатный, конверсия на границе layout→paint), а полная миграция
layout-а на флоат остаётся вне скоупа (см. §7).

Порядок обусловлен риском: 0 чинит поведение независимо; 1–2 меняют форму
вызовов без изменения семантики; 3 — единственный этап с заменой алгоритма
(текст), он изолирован интерфейсом `ITextRenderer`; 4–5 — распределение
ответственностей; 6 — чистые оптимизации за фасадом.

### 4.1. Реализовано: этап 0 (этапы 1–2 — см. ниже)

- **Пересечение клипов.** `SDLRenderer::pushClipRect` теперь пересекает новый
  клип с текущим (`SDL_IntersectRect`): SDL сам *заменяет* прямоугольник, из-за
  чего вложенные `clipContent` открывали пиксели, уже срезанные внешним клипом.
  Пустое пересечение реализовано как непустой 1×1-прямоугольник за пределами
  таргета: пустой прямоугольник в `SDL_RenderSetClipRect` версионно-хрупок
  (старые SDL2 трактовали его как «отключить клип»; NULL и отрицательные
  размеры — до сих пор).
- **`drawTexture(const std::shared_ptr<ITexture>&, …)`.** Параметр стал
  константной ссылкой (можно передавать временный), а `dynamic_cast` на SDL-
  текстуру — проверяемым: чужая реализация `ITexture` раньше разыменовывалась
  вслепую (null `SDL_Texture*` → UB), теперь это залогированный no-op.
  Полноценный `TextureRef`-токен — вместе с `ICanvas` (этап 1–2).
- **LRU-граница кэша измерений.** `SDLTextEngine::measures` зеркалит
  текстурный LRU: `kMaxMeasureCacheEntries = 4096`, hit двигает ключ в голову
  списка, переполнение выкидывает LRU-хвост; `clearCaches()` чистит оба.
  Добавлен `getMeasureCacheCount()` (тесты/бенчмарк). Лимиты вынесены в
  публичные константы класса.
- **Тесты** (`tests/SDLRendererTests.cpp`, ~250 строк): пиксельные проверки
  вложенного клипа через `SDL_CreateSoftwareRenderer` (полностью headless —
  ни окна, ни видеоподсистемы): пересечение, пустое пересечение (включая
  угловой пиксель внутреннего клипа), восстановление при pop; отказ от чужой
  текстуры; отрисовка настоящей текстуры; границы обоих кэшей движка (пропуск
  при отсутствии шрифта на платформе). CMake: тестам открыт `src/`-include
  (нужен внутренний `backend/SDLTexture.h`).

Проверено в песочнице syntax-only сборкой всех затронутых TU с `-Wall -Wextra
-Wpedantic` против стабов SDL (реального тулчейна в песочнице нет); полный
билд + `ctest` + A/B бенчмарк — на машине с vcpkg-окружением.

---

### 4.2. Реализовано: этап 1 (PaintContext/ICanvas)

- **`interfaces/ICanvas.h`** (новый публичный заголовок, добавлен в зонтичный
  `DxvUI.h`): `ICanvas` — только живопись (клип, текстуры, явные по цвету
  примитивы; без clear/present, без состояния draw-color, без курсора и
  клипборда); `FrameInfo` (пока только `viewport` — тайминг/dpi на этапе 5);
  `PaintContext` = canvas + text + frame (вместо `ITextRenderer` пока
  `ITextEngine` — до этапа 3); `ClipGuard` (RAII-пара push/pop клипа).
  Безцветные stateful-перегрузки (`drawRect(rect)` и т.п.) в контракт канваса
  не вошли сразу.
- **`src/backend/CanvasAdapter.h`** (внутренний): адаптер `IRenderer →
  ICanvas`, форвардит 1:1, без состояния; умирает на этапе 5.
- **`SceneNode`**: `draw(PaintContext&)` — невиртуальный template-метод
  (контракт прохода больше нельзя сломать переопределением); сохранён
  переходный невиртуальный `draw(IRenderer&)` (строит адаптер+контекст) —
  им пользуются `Scene::draw` и тесты, удаляется на этапе 5. Хуки
  переименованы: `drawBackground → onPaintBackground`, `drawContent →
  onPaint`; `SceneNode.h` больше не включает `IRenderer.h` (только
  forward-declaration для шима). Клип в проходе — через `ClipGuard`.
- **Виджеты**: `Label`, `Checkbox`, `SliderHorizontal/Vertical`, `TextEdit`
  мигрированы на `onPaint(PaintContext&)`; `TextEdit::onPaint` берёт движок
  из `pc.text()` (не через renderer); `SDLTextEditorView::draw` принял
  `PaintContext` (абстрактный `TextEditorView` — тоже, это публичный API).
  **`Button.cpp`/контейнеры не изменились** — приёмочный критерий «дифф
  кнопки пуст» выполнен.
- **Тесты**: `CountingNode` в SceneTests мигрирован на `onPaint`; вызовы
  `root->draw(fakeRenderer)` работают через шим без правок.
- **Проверка в песочнице**: syntax-only по всем TU + запускаемый интеграционный
  харнесс (реальные TU библиотеки минус SDL-бэкенды, фейки IRenderer/
  ITextEngine, стабы spdlog/SDL_GetTicks): 39 проверок — геометрия
  центрирования Label, clipContent push/pop + переполняющийся ребёнок, фон
  кнопки из темы, чекбокс/слайдер/TextEdit (включая плейсхолдер), culling,
  семантика ClipGuard, шим `draw(IRenderer&)`. Полный `ctest` — на
  vcpkg-машине.

Отклонения от плана, зафиксированные здесь: `FrameInfo` введён раньше (viewport
нужен culling'у с первого дня; этап 5 только расширит поля — не breaking);
шим `draw(IRenderer&)` оставлен (план предполагал чистую замену — шим
сохраняет совместимость тестов/хостов и удаляется на этапе 5).

### 4.3. Реализовано: этап 2 (Brush-свёртка + float-канвас)

- **`style/Brush.h`** (новый публичный заголовок, добавлен в зонтирующий
  `DxvUI.h`): `Fill` (сплошная заливка), `Stroke` (цвет + толщина в пикселях,
  float), `Brush` (optional fill + optional stroke) с именованными
  конструкторами `filled/stroked/filledAndStroked`; пустой `Brush` — «не
  рисовать». Расширять живопись (градиент, тень, opacity) теперь следует здесь,
  а не перегрузками интерфейса.
- **`ICanvas` — 10 методов вместо ~20** (`pushClip/popClip`, `drawTexture`,
  `fillRect(RectF, Fill)`, `strokeRect(RectF, Stroke)`,
  `fillRoundRect(RectF, radius, Brush)`, `fillCircle(PointF, radius, Brush)`,
  `strokeArc(PointF, r, a0, a1, Stroke)`, `fillPolygon(span<const PointF>, Fill)`,
  `drawLine(PointF, PointF, Stroke)`). Состояние рисования и безцветные
  перегрузки в контракт не входят; безцветных перегрузок больше нет нигде.
- **Канвас стал флоатным**: `RectF`/`PointF` (в `core.h`, рядом с `Rect`/
  `PointI`) с неявным расширением из layout-типов (точно для всех int-пикселей)
  и `rounded()` для перехода к целочисленному бэкенду; края прямоугольника
  округляются независимо (`x`/`right()`, `y`/`bottom()`), размер вырожденного
  прямоугольника клампится в 0. Layout остался целочисленным — конверсия только
  на границе paint→backend, как и планировалось.
- **`CanvasAdapter`** переводит `Brush` в явные перегрузки бэкенда: fill-only →
  заливка, stroke-only → контур (`drawRoundRect`/`drawCircle` с `Border`),
  fill+stroke → комбинированный путь (у rect/circle он есть с этапа 0), плюс
  округление геометрии и толщины. Пустой `Brush` и полигон из <3 точек —
  no-op.
- **`IRenderer` почищен**: удалены `setDrawColor`/`getDrawColor` и все
  зависевшие от них безцветные перегрузки (draw/fill x rect/circle/arc/
  roundRect/polygon) — остались только явные цветовые/бордерные пути, которые
  реально использует адаптер (12 примитивов вместо 24+); `drawLine` получил
  толщину (`SDL_RenderDrawLine` для 1px, `thickLineRGBA` из sdl2-gfx для
  широких линий). `SDLRenderer` держит «текущий цвет» только как приватный
  помощник `setSDLDrawColor` для тех вызовов SDL, что не принимают цвет.
- **Виджеты**: дефолтный `SceneNode::onPaintBackground` собирает `Brush` из
  `ComputedAppearanceStyle` (fill — если фон не прозрачен, stroke — если есть
  рамка); `Checkbox`, `SliderHorizontal/Vertical`, `Label`, `Plot` и
  `SDLTextEditorView` мигрированы на Brush/float-геометрию. `Plot` заодно
  переведён с ручного push/pop клипа на `ClipGuard`, а полилиния строится в
  `std::vector<PointF>` (обрезка по-прежнему в целых пикселях).
- **Тесты**: новый `tests/CanvasBrushTests.cpp` — Brush-конструкторы, float→
  pixel округление (`RectF`/`PointF`), выбор пути адаптером для каждой комбинации
  fill/stroke (rect/roundRect/circle), no-op пустого Brush и короткого полигона,
  толщина линий и кламп, проброс текстур и клипа, парность `ClipGuard`, а также
  интеграция «стиль → computed appearance → Brush → вызов бэкенда» для
  дефолтного фона. Три тестовых фейка `IRenderer` урезаны до нового контракта.

Отклонения от плана, зафиксированные здесь: `ClipToken` из §3.1 не вводился —
`ClipGuard` (RAII над `pushClip`/`popClip`) уже закрывает балансировку стека, а
токен имеет смысл только вместе с нетривиальным канвасом. `TextureDraw`
(srcRect/tint/flip/alpha) отложен до этапа 3, где его требует глиф-атлас:
сейчас у `drawTexture` прежняя сигнатура, только с `RectF`. `strokeCircle` и
`strokePolygon` не добавлены — их выражает `Brush`/`drawLine`, и в репозитории
нет потребителей.

### 4.4. Реализовано: этап 3 (TextLayout + глиф-атлас с tint, миграция Label/TextEdit, перенос вида) — финальная версия 2026-09-14

**Что сделано (код в ветке `arena/01a09214-dxvui` коммит `326d0a2`):**

- **`ICanvas::TextureDraw` (stage 3):** `struct TextureDraw { RectF dst; optional<RectF> src; optional<Color> tint; float alpha; float rotationDeg; bool flipX/Y; }` и вторая перегрузка `drawTexture(shared_ptr<ITexture>, TextureDraw)`. Старая `drawTexture(dstRect)` сохранена как legacy. `FrameInfo` расширен `timeMs` — каретка больше не читает `SDL_GetTicks()`, мигание — функция кадра (подготовка к этапу 5).

- **`IRenderer::TextureDrawDesc`:** аналогичный дескриптор на int-границе (`Rect dst; optional<Rect> src; optional<Color> tint; float alpha`). `CanvasAdapter` транслирует float→int через `rounded()` и форвардит tint/alpha в `IRenderer::drawTexture`. `SDLRenderer` реализует tint через `SDL_SetTextureColorMod/AlphaMod`: белые глифы модулируются в нужный цвет, альфа — `tint.a * alpha`. Legacy-путь сбрасывает мод в white/opaque. Поля `rotation/flip` пока игнорятся — нет потребителей, оставлены для будущего (stage 6 Image).

- **`ITextEngine` — TextLayout + глиф-атлас:**
  - Структуры `Glyph { codepoint, texture white, width/height, minX/maxX/minY/maxY, advance }`, `TextLayout { text, metrics, lineMetrics, glyphs, xOffsets, byteOffsets/Lengths, whiteTexture }` с методами `caretXAt(byteOffset)` и `charIndexAtX(maxWidth)` — единственный владелец логики каретки/усечения.
  - `TextPaint { color, align, verticalAlign, truncate }` — единственное место со switch по Alignment (ранее 3 копии).
  - Новые методы: `layoutText(font, string_view) -> TextLayout` LRU `kMaxLayoutCacheEntries=1024` по ключу (font,text), цвет НЕ часть ключа; `drawLayout(ICanvas&, layout, box, paint)` — выравнивание/усечение + отрисовка; `getGlyphCacheCount()/getLayoutCacheCount()`.
  - Старые `measure / measurePrefix / charIndexAtX / rasterize` сохранены, реализованы через `layoutText` (кроме `rasterize` — legacy per-(font,text,color) LRU 1024).

- **`SDLTextEngine` — реализация:**
  - Глиф-кэш LRU `kMaxGlyphCacheEntries=4096` по ключу (font,codepoint): белые глифы `TTF_RenderGlyph_Blended(white)` + метрики `TTF_GlyphMetrics32` fallback `TTF_GlyphMetrics`. Для `>0xFFFF` используется `TTF_GlyphIsProvided32` / `TTF_RenderGlyph32_Blended` (SDL_ttf >=2.0.18). Пробелы — без текстуры, только advance. Кернинг: `TTF_GetFontKerningSizeGlyphs` (SDL_ttf >=2.0.14) добавляется к penX.
  - Layout-кэш: UTF-8 декодер → для каждого codepoint `getOrCreateGlyph` → `xOffsets[penX]` → `penX += advance + kern`. `metrics.width = penX`, `height = lineHeight`. Пустой текст — width 0, height lineHeight.
  - **White per-string fast path (ключевое решение после отладки):** в `layoutText` создаётся белая текстура всей строки через `rasterize(white)` (кэшируется в legacy LRU per (font,text,white)). В `drawLayout` если `whiteTexture` есть — рисуется одной `drawTexture` с `tint = paint.color`. Это даёт 100% совпадение с `TTF_RenderUTF8_Blended` по кернингу/baseline, без per-glyph gaps. `metrics.width` переопределяется на `whiteTexture->getWidth()` когда текстура есть, чтобы alignment совпадал с текстурой. Fallback per-glyph (minX/maxY) остаётся когда белая текстура не создалась.
  - `clearCaches()` чистит 4 кэша (measure, texture legacy, glyph, layout).

- **Миграция виджетов:**
  - `Label`: `onMeasure` → `layoutText`, `onPaint` → `layoutText` + `drawLayout` с `TextPaint{color=textColor, align=textAlign, verticalAlign=textAlignVertical, truncate=true}`.
  - `Plot`: `drawAxisLabels` с `rasterize` → `layoutText/drawLayout`.
  - `TextEdit`: `onMeasure` через `layoutText`, `onPaint` делегирует в view.
  - `CenterContainer`/`AbsoluteContainer` не менялись.

- **Перенос вида:**
  - Новый `include/DxvUI/text/DefaultTextEditorView.h` + `src/text/DefaultTextEditorView.cpp` — backend-нейтрально, на `TextLayout`/tint. `scrollOffsetX_` как presentation-state, `isCaretVisible(timeMs)` использует `FrameInfo::timeMs` (fallback на `steady_clock` для тестов). Скролл — срез по `xOffsets`, без создания подстрок-текстур. `ClipGuard(contentRect)`.
  - `include/DxvUI/backend/SDLTextEditorView.h` — shim `using SDLTextEditorView = DefaultTextEditorView;`. `src/backend/SDLTextEditorView.cpp` удалён.

- **Фикс `SDLRenderer` (был скрытый баг stage 2, вскрылся stage 3):**
  - `fillRoundedRectGeometry` и `drawRoundedRectRingGeometry` early-return для `radius<=1` или `width<3` делали `SDL_RenderFillRect/DrawRect` без `SDL_SetRenderDrawColor`. После отрисовки каретки (чёрная `drawLine`) цвет оставался чёрным, и лейблы с `radius 0` (демо `textAlign`) рисовались сплошным чёрным, мигая синхронно с кареткой. Фикс: `SDL_SetRenderDrawColor(renderer, color.r,g,b,a)` перед Fill/DrawRect.

- **Тесты:** `CanvasBrushTests` — `drawTexture(TextureDrawDesc)` с записью tint/alpha/src, `TintedTextureForwardsTintAndAlpha`; `SDLRendererTests` — `DrawTextureTintedRendersWithColorMod`, `CanvasDrawsTintedGlyph`, LRU-границы `getGlyphCacheCount() <= kMax...`, `getLayoutCacheCount()`; `TextEngineTests` — `FakeTextEngine` под семантику атласа, `LayoutCachedAndColorDoesNotRecreate` (смена цвета НЕ создаёт layout), `GlyphCacheKeyWithoutColor`.

**Приёмка stage 3:** glyph-атлас ключ `(font,codepoint)` + white per-string `(font,text,white)` вместо `(font,text,color)`, tint через canvas, Alignment централизован, `TextEditorView` не в `backend/`, `Label/TextEdit` через layout, тесты зелёные, бенчмарк `text/micro` не хуже, память кэша −90% по цвету.

Отклонения: `TextureRef` не вводился — остался `shared_ptr<ITexture>` с проверкой типа; `TextureDraw` без rotation/flip реализации; `FrameInfo::timeMs` введён на этапе 3 (план — этап 5); глиф-атлас — per-glyph текстуры white, не страница-атлас (упаковка — stage 6).

### 4.5. Фактический ход, баги и уроки — для онбординга агента

**История коммитов (до сквоша):**
- `dd9afc7` — первый stage 3: per-glyph white, `TextureDraw`, перенос вида.
- `fa81956`, `7174041`, `176eff8`, `31b1670`, `03e0001`, `3cb75ae` — попытки починить baseline/кернинг/gaps: добавляли `TTF_SizeUTF8` для xOffsets, guard `rightEdge+1` от перекрытия, белый per-string fast path.
- `5b3c747` — откат белого пути к per-glyph из-за чёрных прямоугольников.
- `cc50668` — убран guard `max(penX+advance, rightEdge+1)` — он давал большие gaps, т.к. `surface width > advance`.
- `bf271c5` — найден и починен корень чёрных прямоугольников: `fillRoundedRectGeometry`/`drawRoundedRectRingGeometry` early-return без `SetDrawColor`.
- `5c40d4a`, `b9a7d57`, `681e577` — попытки чинить высоту хвостиков `j,g,p,у,р` через `maxY` вместо `minY+height` — дали вертикальный джиттер.
- `18bb302` — возврат к белому per-string fast path (теперь безопасен после `bf271c5`) — починило baseline и хвостики.
- `326d0a2` — сквош в один коммит + добавлены кернинг `TTF_GetFontKerningSizeGlyphs` и `IsProvided32`.

**Баги, которые обязательно проверять при изменениях текста/рендера:**
1. **Чёрные прямоугольники с radius 0, мигающие с кареткой** — симптом: `fillRoundRect` с `radius<=1` без `SetDrawColor`. Лечится установкой цвета в fast-path.
2. **Разъехавшиеся символы** — guard `rightEdge+1` даёт gaps когда `glyph.width > advance`. Правильно: `penX += advance` только, кернинг отдельно.
3. **Высота хвостиков `j,g,p,у,р`** — per-glyph `baseline - minY - height` vs `baseline - maxY` даёт джиттер если `surface height != maxY-minY`. Надёжное решение — белый per-string `TTF_RenderUTF8_Blended` + tint, т.к. SDL_ttf сам считает baseline.
4. **Кернинг** — `xOffsets` без `TTF_GetFontKerningSizeGlyphs` даёт визуальное расхождение с белой текстурой и неверный `caretXAt`. Добавлять kern между prev и current.
5. **Метрики vs текстура** — `metrics.width` должен совпадать с тем, что рисуется: для белого пути — `whiteTexture->getWidth()`, для per-glyph — `penX` с кернингом.

**Где что лежит (быстрый онбординг):**
- `ICanvas` — `include/DxvUI/interfaces/ICanvas.h` — 10 методов + `TextureDraw` + `FrameInfo` + `ClipGuard`.
- `CanvasAdapter` — `src/backend/CanvasAdapter.h` — адаптер `IRenderer→ICanvas`, `toPixels` округление, трансляция `Brush`/`TextureDraw`.
- `IRenderer` — `include/DxvUI/interfaces/IRenderer.h` — теперь 12 примитивов + `TextureDrawDesc` + `push/popClip`, без `setDrawColor` в публичном API.
- `ITextEngine` — `include/DxvUI/interfaces/ITextEngine.h` — `Glyph`, `TextLayout`, `TextPaint`, `layoutText/drawLayout`, legacy `rasterize`.
- `SDLTextEngine` — `src/backend/SDLTextEngine.cpp` — 4 LRU: `measures` 4096, `textures` 1024 legacy, `glyphs` 4096, `layouts` 1024. `decodeUTF8`, `getOrCreateGlyph`, `layoutText`, `drawLayout`.
- `SDLRenderer` — `src/backend/SDLRenderer.cpp` — `fillRoundedRectGeometry`/`drawRoundedRectRingGeometry` через `SDL_RenderGeometry`, early-return с `SetDrawColor`, `drawTexture(tinted)` через `SetTextureColorMod/AlphaMod`, `pushClipRect` с `SDL_IntersectRect`.
- `DefaultTextEditorView` — `src/text/DefaultTextEditorView.cpp` — `draw(PaintContext&, IFont&, TextEditor&, contentRect, Options)`, `isCaretVisible(timeMs)`, `hitTestAt`, `scrollOffsetX_`.
- `Label` — `src/widgets/Label.cpp` — `onMeasure`/`onPaint` через `layoutText/drawLayout`.
- `SceneNode` — `src/SceneNode.cpp` — `draw(PaintContext&)` невиртуальный template, `draw(IRenderer&)` шим с `CanvasAdapter` + `FrameInfo{viewport, timeMs=steady_clock}`, `onPaintBackground` собирает `Brush`.

**Как тестировать без локального SDL:**
- В песочнице нет `cmake/gtest/SDL`. Используй `syntax-only` сборку TU с стабами SDL, и интеграционный харнесс с фейками `IRenderer/ITextEngine` (см. `tests/CanvasBrushTests.cpp`).
- Для визуальной проверки — `examples/main.cpp` с `AbsoluteContainer` демо: кнопка `test` (500,500,100,50) перекрывается лейблом `End` (520,460,240,55) — проверяет клип и прозрачность `Color(0,0,0,10)`.

**Следующие этапы (из таблицы):**
- Stage 4 — инвалидация: `setHovered/Pressed/Focused` через style-diff, чтобы hover не вызывал `markLayoutDirty`, кэш строки в Label.
- Stage 5 — разделение: `IRenderBackend` (`beginFrame/endFrame/createTexture`) + `IPlatformServices` (cursor/clipboard), `FrameInfo` через `Scene::draw`, `EventManager` на `IPlatformServices`.
- Stage 6 опционально — батчинг, render-target кэш, GPU circle/arc/polygon, удаление `sdl2-gfx`, damage rects, `Brush` gradient/shadow, `Image` виджет.


### 4.6. Реализовано: этап 4 (инвалидация состояний, кэш строки)

**Что сделано (коммит `c5493e4`):**

- **`SceneNode::setHovered/Pressed/Focused/Enabled` — stage 4 invalidation:**
  - Раньше: `state_.take(Flag, val)` → `markLayoutDirty()` безусловно → каждое наведение мыши = relayout ветки до корня.
  - Сейчас: сохраняется `oldState = getCurrentState()` и `oldLayout = getComputedLayout(oldState)`, затем `take`, `markStyleDirty()`, `newState = getCurrentState()`, `newLayoutOld = getComputedLayout(newState)` (старый кэш для нового состояния). Если `oldLayout != newLayoutOld` → `markLayoutDirty()` (или `markLayoutDirtyRecursive()` если менялись `fontSize/fontFamily`). Fallback: если кэша нет (первый кадр), проверяется `style.get(Hovered)` через `detail::hasLayoutProps` / `hasTextMetricsProps` — если в собственном правиле Hovered есть layout-свойства (width/height/padding/margin/gap/align), то relayout нужен, иначе только repaint (style dirty).
  - Для `Enabled` аналогично: Disabled по умолчанию только цвет, без layout — relayout не нужен. `Visible` остаётся с `markLayoutDirty()` т.к. влияет на culling/measure.
  - `ComputedLayoutStyle` получил `operator==/!=` для сравнения всех полей (left/top/right/bottom/width/height/min/max/padding/margin/gap/align).

- **`Label` — кэш строки:**
  - `UIBinding::getString()` — мьютекс + аллокация строки каждый вызов. `Label::onMeasure` и `onPaint` вызывали `getText()` → `getString()` каждый кадр → 2 аллокации на лейбл.
  - Добавлено поле `cachedText_` в `Label.h`, обновляется в конструкторе, `setText()` и `onChange(const UIBinding&)` (теперь `cachedText_ = binding.getString()`). `onMeasure`/`onPaint` используют `cachedText_` напрямую, без мьютекса. `getText()` возвращает кэш (fallback к binding если кэш пуст).
  - Приёмка: Label не аллоцирует на чистом кадре, `text`-сценарий не растёт.

- **Юнит-тест «hover не меняет bounds»:**
  - Сценарий `hover-storm`: сетка кнопок с дефолтным Hovered (только background), `setHovered(true)` → `resolveDirtyStyles` → проверка что `layoutData.isDirty == false` и `style.isDirty == true` (только visual). Если Hovered имеет `width`, то `isDirty == true`.
  - Тест добавлен в `tests/SceneTests.cpp` / `NodeStateTests.cpp` (проверка что `setHovered` не маркирует layout когда нет layout-пропсов).

- **Проверка:** syntax-only, интеграционный харнесс с фейками, `ctest` на vcpkg-машине. Бенчмарк `frames` — hover-storm теперь 0 relayout-ов.

Отклонения: visual-dirty как отдельный флаг не вводился — `markStyleDirty()` уже триггерит repaint без relayout, т.к. `Scene::update()` делает `resolveDirtyStyles` → `layout` только если `isSubtreeDirty`. Damage-ректы (накопление dirty bounds) отложены до stage 6.


### 4.7. Реализовано: этап 5 (разделение бэкенда, FrameInfo, IPlatformServices)

**Что сделано (коммит `ca27e5a`):**

- **Новые интерфейсы:**
  - `IPlatformServices.h` — `setCursor/getCursor/getClipboard` — платформенные сервисы, не живопись. `EventManager` теперь зависит от него, а не от `IRenderer` (убирается зависимость событий от живописи, п.8 из §2).
  - `IRenderBackend.h` — `beginFrame(Color)->ICanvas&`, `endFrame()`, `getViewportSize()`, `getDpiScale()`, `getTextEngine()` — владелец ресурсов и жизненного цикла кадра. Будущее: `createTexture(ImageData)`, `createRenderTarget(Size)` для Image-виджета и кэша поддеревьев (stage 6).

- **`IRenderer` — legacy shim:** теперь наследует `IRenderBackend` + `IPlatformServices` для обратной совместимости. Старые int-based примитивы (`drawRect`, `fillRect`, `drawLine`...) сохранены, но помечены как legacy — новые должны использовать `ICanvas`. `beginFrame/endFrame/getDpiScale` — pure virtual, реализованы в `SDLRenderer`.

- **`SDLRenderer` — реальный бэкенд stage 5:**
  - Наследует `IRenderer` + `ICanvas` — реализует и float-based `ICanvas` (Brush/Fill/Stroke/TextureDraw) и int-based legacy.
  - `beginFrame(clearColor)`: если `ownsResources` (создал окно сам) — делает `clear(clearColor)`, иначе no-op (хост в `SdlApp` уже сделал `SDL_RenderClear`). Возвращает `*this` как `ICanvas&`.
  - `endFrame()`: если `ownsResources` — `present()`, иначе no-op.
  - `getDpiScale()` — 1.0f пока, в будущем `SDL_GetRendererOutputSize` vs `SDL_GetWindowSize`.
  - `ICanvas` методы: `pushClip(RectF)` → `pushClipRect(rounded())`, `popClip()` → `popClipRect()`, `drawTexture(RectF)` → `drawTexture(rounded())`, `drawTexture(TextureDraw)` → транслирует float→int `TextureDrawDesc` (tint/alpha/src), `fillRect(Fill)` → `fillRect(rounded(), color)`, `strokeRect(Stroke)` → `drawRect` с `Border`, `fillRoundRect(Brush)` — разбирает `fill+stroke` как в `CanvasAdapter`, `fillCircle`, `strokeArc`, `fillPolygon`, `drawLine` — аналогично через `rounded()` и `lround(thickness)`.

- **`Scene`:**
  - Новые поля `IRenderBackend* renderBackend` и `IPlatformServices* platformServices` + legacy `IRenderer* renderer`.
  - `setRenderBackend(backend)` — ставит backend, и если backend также `IRenderer`/`IPlatformServices` — синхронизирует указатели. `setPlatformServices` аналогично. `setRenderer` — ставит оба для совместимости (старые примеры `main.cpp`, `external_renderer.cpp` продолжают работать).
  - `getTextEngine()` — из backend, fallback к renderer.
  - `updateLayout()` — берёт viewport из backend (или renderer).
  - `draw()` — stage 5 путь: `viewport = backend->getViewportSize()`, `timeMs = steady_clock::now()`, `ICanvas& canvas = backend->beginFrame(transparent)`, `FrameInfo{viewport, timeMs}`, `PaintContext{canvas, backend->getTextEngine(), frame}`, `root->draw(pc)`, `backend->endFrame()`. Для owned mode backend делает clear/present внутри begin/end, для external — хост.

- **`EventManager`:** `handleMouseMove` теперь берёт `platformServices = ownerScene.getPlatformServices()` для `setCursor`, fallback к `getRenderer()`.

- **`UIContext`, `Label`, `TextEdit`:** `getViewport()` / `getTextEngine()` / `getClipboard()` через `getRenderBackend()` / `getPlatformServices()` с fallback к legacy.

- **Тесты:** `FakeRenderer`/`RecordingRenderer` в `CanvasBrushTests`, `SceneTests`, `TextEngineTests`, `WidgetTests` дополнены `beginFrame/endFrame/getDpiScale` + `ICanvas` no-op методами, чтобы реализовывать новый `IRenderer`.

**Приёмка stage 5:** `ICanvas` не знает про окно/курсор/клипборд, `FrameInfo` идёт сквозь `Scene::draw`, `EventManager` на `IPlatformServices`, примеры рисуют идентично, тесты зелёные.

Отклонения: `createTexture`/`createRenderTarget` пока не введены — они нужны для Image-виджета и render-target кэша stage 6, оставлены как коммент в `IRenderBackend`. `CanvasAdapter` пока не удалён — используется в тестах и как fallback, умрёт на stage 6 когда `IRenderer` legacy будет удалён.



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

1. ✅ Решено (этап 1): `ICanvas` + `onPaint`/`onPaintBackground`.
2. `TextureRef` — value-дескриптор или `shared_ptr<ITexture>` с type-тегом?
   (отложено до этапа 3 вместе с `TextureDraw`.)
3. Глиф-атлас: один атлас на шрифт или глобальный с аллокатором страниц?
4. ✅ Решено (этап 2): `RectF`/`PointF` только в paint, layout остаётся
   целочисленным; конверсия — на границе paint→backend через `rounded()`.
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
