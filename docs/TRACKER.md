# DxvUI — трек развития

Единый трекер: что сделано, что осталось. Сведён из `ROADMAP.md`,
`RENDERING_REFACTORING.md` и `AUTO_CACHE_BATCHING_PLAN.md` (удалены; полная история
и обоснования — в git). Читать как «что/зачем/где в коде»: ✅ сделано,
🟡 частично, ⏳ открыто.

---

## 1. API, виджеты, события

### Сделано

- **Система событий — трёхфазная DOM-модель** (бывш. ROADMAP п.1/п.4):
  - фазы `Capture → Target → Bubble`, полный проход в `EventManager::dispatch`,
    поузловой `SceneNode::dispatchEvent(DxvEvent&, EventPhase)` без рекурсии на
    родителей; старый `dispatchEvent(DxvEvent&)` сохранён как Target-only обёртка;
  - две реакции: `on()`/`onCapture()`-слушатели + `virtual onEvent()` =
    default-action только на Target (гасится `preventDefault`, если `cancelable`);
  - `eventMeta(EventType)` — константная таблица `bubbles`/`cancelable`/
    `captureable`; table-роутинг `RoutingTarget {HitTest, Hovered, Focused, Root}`;
  - capture-гейт: только raw-input (`Mouse*`/`Key*`/`TextInput`), lifecycle-и
    синтез (`Change`/`Focus`/`Hover`) capture не проходят;
  - виджеты `TextEdit`, `Checkbox`, `SliderBase`, `ScrollContainer` переведены на
    target-модель; `Popup` закрывается по клику вне через capture-фазу
    (`9de0e5e`); пример `examples/events.cpp` с capture-демо; тесты
    фаз/capture/W3C-бабблинга (~300 зелёных).
  - Вынесено из скоупа (отдельные задачи): типизация payload-а, `Submit`/
    `DragStart/DragEnd`, `FocusIn/FocusOut`, унификация `Change` (см. §1 п.2).
- **Plot** (v1 `92b217a`, полировка API `9e971ef`, тики/подписи/сетка/area-заливка
  `bff250b`, тесты `3499a94`, примеры `3a85db1`/`f575e06`). Идеи v3: pan/zoom
  мышью, hover-тултип со значением, легенда по `getSeriesName()`, толстые линии.
- **Image** (stage 6b, `c12bedd`; дефолтный стиль `30a5fb5`, демо `2afb811`):
  fit `None/Contain/Cover/Fill/ScaleDown`, `srcRect`/`tint`/`alpha`, загрузка
  PNG/JPG через stb_image (`ImageData::loadFromFile/Memory`).
- **Горячий путь Label** (бывш. ROADMAP п.3, частично): кэш строки
  (`cachedText_`, обход мутекса `UIBinding` между `Change`) + кэш font-handle и
  последнего layout (`d667f05`, `eb7af4e`).

### Осталось

| # | Что | Зачем | Где |
|---|-----|-------|-----|
| 1 | ⏳ Инфинити-сентинель «размер без ограничений» in measure | честный intrinsic/wrap без `min(content, available)`; делать только вместе с wrap-content | `core.h`, `LayoutManager` (onMeasure-pass), `ScrollContainer::onMeasure` |
| 2 | ⏳ Binding/Change-канал: (а) унифицировать `Change` «источник vs проекция»; (б) `UIBinding`: `set()` копирует весь вектор колбэков, `getString()` аллоцирует | один предсказуемый публичный API; однопоточный UI без лишних аллокаций | `UIBinding.h/.cpp`, `TextEdit` (проекция), `Label`/`SliderBase`/`Checkbox` (хранилище) |
| 3 | 🟡 Завершить disabled/фокус/скролл: (а) **сделано** — `EventManager` блокирует клик/фокус/ховер disabled-узлов; (б) наследование «родитель off → дети off» (`isEnabled()` смотрит только свой флаг); (в) Tab/стрелки-навигация; (г) скроллбары | завершённость по умолчанию | `EventManager`, `SceneNode::isEnabled`, `ScrollContainer` |
| 4 | ⏳ Виджеты: ProgressBar (паттерн `SliderBase`), Dropdown/Dialog (поверх `Popup`) | закрыть «почему нет X» | `widgets/`, `containers/` |
| 5 | 🟡 Масштабирование: HiDPI (`getDpiScale()` = 1.0, канвас уже флоатный — малая победа) отдельно от multi-scene/modal-стеков и touch (крупно, по спросу) | реальные приложения | `SDLRenderer::getDpiScale`, `Scene`, `EventManager`, `backend/` |

Снято при ревизии 2026-09-22: п.6 `getNodeType()` → RTTI — TODO в `SceneNode::bind()` закрыт
(`connection_.reset()`), а строковые ключи тем (`Theme::registerDefaultStyle(kWidgetType, …)`) — пользовательский
API; `kWidgetType` уже централизован константой на виджет, RTTI-замена не нужна.

---

## 2. Рендеринг и бэкенд

### Сделано (этапы 0–7)

| Этап | Что | Коммит |
|------|-----|--------|
| 0 | Пересечение клипов (`SDL_IntersectRect`), константный `drawTexture` + проверка типа текстуры, LRU-граница `measures` (4096); пиксельные headless-тесты | `8e5b3ee` |
| 1 | `ICanvas` + хуки `onPaintBackground`/`onPaint`, невиртуальный `draw(PaintContext&)`, `ClipGuard`; шим `draw(IRenderer&)` для совместимости | `5013162` |
| 2 | `Brush` (Fill/Stroke), флоатный канвас (`RectF`/`PointF`), свёртка перегрузок; `IRenderer` ужат до 12 примитивов | `ada074b` |
| 3 | `TextLayout` + глиф-атлас (белые глифы + tint), `drawLayout` (единый switch по Alignment), `DefaultTextEditorView` в `text/`; миграция Label/TextEdit/Plot | `c12bedd` |
| 4 | Инвалидация состояний: `setHovered/Pressed/Focused` через style-diff, без relayout; кэш строки Label; тест `hover-storm` | `c12bedd` |
| 5 | `IRenderBackend` + `IPlatformServices`, `FrameInfo` сквозь `Scene::draw`, `EventManager` на сервисы | `c12bedd` |
| 6a | GPU-пути (circle/ring/arc/thick-line, ear-clipping полигоны), sdl2-gfx удалён | `c12bedd` |
| 6b | `ImageData`/`createTexture`/`createRenderTarget` + Image; damage-ректы (culling в `drawImpl`); батчинг fillRect; gradient/shadow в `Brush` | `c12bedd` |
| 7 | Удаление `IRenderer`/`CanvasAdapter`/шимов; `SDLRenderer` → `IRenderBackend`+`ICanvas`+`IPlatformServices` (int-API в private); общий `tests/FakeBackend.h`; версия пакета → 1.0.0 | `9b28e03` |

Ключевые решения этапа 3: белый per-string fast path (`TTF_RenderUTF8_Blended` +
tint — совпадает с legacy по кернингу/baseline), кернинг
`TTF_GetFontKerningSizeGlyphs`, fallback per-glyph. LRU-кэши: textures 1024
(legacy), glyphs 4096, layouts 1024, measures 4096.

### Осталось

- ⏳ Опционально (по числам бенчмарка): `TextureRef`-дескриптор вместо
  `shared_ptr<ITexture>`; multiline `TextLayout` (по спросу); headless-канвас
  (запись PaintOps для скриншот-тестов); damage-пропуск `beginFrame` (аккуратно:
  не пропустить мигание каретки); полный батчинг roundRect/circle/text (см. §3).
  Крупно и отложено: HarfBuzz/RTL-шейпинг (кириллица/латиница считаются
  TTF-метриками).

### Баги и уроки (помнить при правках текста/рендера)

1. **Чёрные прямоугольники с radius 0, мигающие с кареткой** — `fillRoundRect` с
   `radius<=1` по fast-path без `SDL_SetRenderDrawColor`. Лечится установкой цвета.
2. **Разъехавшиеся символы** — guard `rightEdge+1` даёт gaps когда
   `glyph.width > advance`. Правильно: `penX += advance`, кернинг отдельно.
3. **Высота хвостиков `j,g,p,у,р`** — per-glyph `baseline - maxY` даёт джиттер.
   Надёжное решение — белый per-string `TTF_RenderUTF8_Blended` + tint (SDL_ttf сам
   считает baseline).
4. **Кернинг** — `xOffsets` без `TTF_GetFontKerningSizeGlyphs` разъезжается с белой
   текстурой и врёт `caretXAt`. Добавлять kern между prev и current.
5. **Метрики vs текстура** — `metrics.width` должен совпадать с рисуемым: для белого
   пути `whiteTexture->getWidth()`, для per-glyph — `penX` с кернингом.

---

## 3. Авто-кеш поддеревьев и батчинг

### Статус

- ✅ `stb_image` (v2.30) + `ImageData::loadFromFile/Memory` (`third_party/stb/`,
  `src/stb_image_impl.cpp`) — сделано.
- ✅ Stage 7: `IRenderer`/`CanvasAdapter`/шим удалены (см. §2).
- ⏳ **Subtree cache — не начато** (API `createRenderTarget/begin/end` готов).
- ⏳ **BatchBuilder — не начато** (есть только fillRect-батч: `fillRectBatch_`,
  лимит 256, flush на смене состояния).

### Subtree cache (план)

Кэшировать статичные поддеревья (панель с сотней кнопок, фон, иконки) в
render-target и рисовать одной `drawTexture`. Ожидание: `frames` −50% CPU,
draw calls 1 вместо N.

- Флаги узла: `cacheAsTexture` (ручной), `autoCache` (эвристика — **не делать**),
  `cacheInvalid`/`cacheTexture`/`cacheBounds`.
- `SubtreeCache` в `Scene`: `unordered_map<SceneNode*, CacheEntry>`, LRU, бюджет
  64MB, ключ `size + dpi`, clamp к `maxTextureSize` бэкенда.
- Инвалидация: `mark*Dirty`, смена bounds при arrange, `onNodeRemoved`
  (освобождать память), пересечение с `damageUnion_`.
- Этапы внедрения: сначала **только ручной флаг** `cacheAsTexture` + замер по
  бенчмарку (`frames`, порог 10%); при подтверждённом выигрыше — LRU/бюджет и
  инвалидация через damage. Риски: текстура больше max texture size (fallback в
  обычный draw), прозрачность/клип в кэше, смена DPI.

### BatchBuilder (план)

Снизить draw calls O(N) → O(stateChanges): сцена из 100 кнопок сейчас 201 call
(fill + stroke + text) → цель 3 (1 solid + 1 roundRect + 1 text).

- `BatchKey { blend, texture, clip, radius bucket }`; `Batch { verts, indices }`;
  flush по смене key, лимиту verts/indices, смене клипа, `endFrame`.
- Фазы: solid rects (расширить до 2048 verts, clip в ключ) → roundRect (bucket по
  radius) → circle/line → texture (глифы одного шрифта/размера) → clip-aware.
  Честный прирост даёт в основном фаза roundRect (кольца сейчас — 1 вызов
  `RenderGeometry` на кнопку); text уже 1 draw call на строку (белый per-string).
- Риски: лимит вершин `SDL_RenderGeometry` (драйверный) — `kMaxVerts`
  консервативно; полупрозрачность — порядок важен, flush при смене alpha;
  градиент — per-vertex color, батчится по bucket угла.

### Чек-лист приёмки

- [x] `stb_image` + `ImageData::loadFromFile/Memory`
- [x] Stage 7: удаление `IRenderer`/`CanvasAdapter`/шимов (см. §2)
- [ ] Subtree cache: ручной флаг, LRU, damage-инвалидация, `frames` −50% CPU
- [ ] Batching: `BatchBuilder`, clip-aware, texture batch, метрики draw calls, −70%