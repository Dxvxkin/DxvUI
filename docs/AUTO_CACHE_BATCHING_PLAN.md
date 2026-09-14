# Авто-кеш поддеревьев и батчинг примитивов — аудит легаси и план внедрения

Дата: 2026-09-14 (ветка arena/01a09214-dxvui)

## 1. Аудит легаси после рефакторинга (этапы 0..6b)

### Что уже удалено
- `SDL2_gfxPrimitives.h` и все вызовы `aacircleRGBA`, `filledCircleRGBA`, `thickLineRGBA`, `filledPolygonRGBA` — заменены на GPU-геометрию через `SDL_RenderGeometry`.
- `CMakeLists.txt`: `find_package(SDL2_GFX)` и линковка `SDL2::SDL2_gfx` удалены.
- `DxvUIConfig.cmake.in`: зависимость от `sdl2-gfx` удалена.
- `SDLRenderer`: CPU-растеризаторы удалены, добавлены `fillCircleGeometry`, `drawCircleRingGeometry`, `drawArcRingGeometry`, `drawThickLineGeometry`, `fillPolygonGeometry` + ear-clipping триангуляция.

### Что осталось (legacy shim, stage 7)
1. **`IRenderer` — god-interface int-based (30+ методов)**  
   - Файл: `include/DxvUI/interfaces/IRenderer.h` — наследует `IRenderBackend + IPlatformServices` для совместимости. Методы: `clear/present/getViewportSize/getDpiScale/getTextEngine/setCursor/getCursor/getClipboard/pushClipRect/popClipRect/drawTexture Rect/TextureDrawDesc/drawRect/fillRect Color/fillRect border/drawLine/drawCircle/fillCircle/drawArc/drawRoundRect/fillRoundRect/fillPolygon`.  
   - `SDLRenderer.h/.cpp` — реализует legacy + `ICanvas`. Содержит `setSDLDrawColor` helper, который должен умереть вместе с int API.  
   - `Scene.h` — хранит `IRenderer* renderer` (legacy), `setRenderer/getRenderer`, синхронизируется с `renderBackend`/`platformServices` через `dynamic_cast`.  
   - `SceneNode.h` — `void draw(IRenderer&)` шим, создающий `CanvasAdapter + FrameInfo` (steady_clock).  
   - `src/backend/CanvasAdapter.h` — адаптер `IRenderer → ICanvas`, float RectF/Fill/Brush → int Color, округление `rounded()`, `toPixels`. Используется в тестах и в `SceneNode::draw(IRenderer&)`.  
   - `UIContext`, `EventManager`, виджеты (`Label`, `Checkbox`, `Slider*`, `TextEdit`, `Image`) — имеют fallback на legacy `getRenderer()` когда нет `renderBackend`.

2. **Тестовые фейки**  
   - `tests/SceneTests.cpp`, `TextEngineTests.cpp`, `WidgetTests.cpp`, `CanvasBrushTests.cpp` — `FakeRenderer` / `RecordingRenderer` наследуют `IRenderer + ICanvas`, реализуют оба API. После удаления `IRenderer` должны реализовывать только `IRenderBackend + ICanvas + IPlatformServices`.

3. **Примеры / бенчмарк**  
   - `examples/main.cpp`, `external_renderer.cpp`, `benchmark.cpp` — включают `IRenderer.h`, создают `SDLRenderer` как `IRenderer*`. Должны перейти на `IRenderBackend`.

4. **ImageData до stb_image**  
   - `core/ImageData.h` — только `createCheckerboard/createSolid`, нет `loadFromFile/Memory`. Исправлено в этом PR: добавлен `third_party/stb/stb_image.h` (v2.30, 7988 строк), `src/stb_image_impl.cpp` с `#define STB_IMAGE_IMPLEMENTATION`, `src/core/ImageData.cpp` с `stbi_load/_from_memory`.

**Критерий готовности stage 7 (полное удаление легаси):**
- Удалить `IRenderer.h`, `CanvasAdapter.h`.
- `SceneNode::draw(IRenderer&)` → удалить, оставить только `draw(PaintContext&)`.
- `Scene`: удалить `renderer` поле, `setRenderer/getRenderer`, оставить только `renderBackend`/`platformServices`.
- `SDLRenderer`: наследовать `IRenderBackend + ICanvas + IPlatformServices` напрямую, без `IRenderer`.
- Тесты: `FakeRenderer` → `FakeBackend`.
- Примеры: `IRenderBackend*` вместо `IRenderer*`.

---

## 2. План ввода авто-кеша поддеревьев (Subtree Cache)

### 2.1 Цель
Кэшировать статичные поддеревья (например, панель с 100 кнопками, фон, иконки) в `RenderTarget` текстуру, чтобы перерисовывать их 1 раз, а далее — `drawTexture(cached)`. Ожидаемый выигрыш: −80% CPU на статичных сценах, −draw calls, стабильный FPS при `frames` бенчмарке.

### 2.2 API и флаги узла
Добавить в `SceneNode`:
```cpp
bool cacheAsTexture = false; // пользовательский флаг
bool autoCache = false;      // эвристика: если узел не менялся N кадров и тяжелый — авто-кеш
mutable bool cacheInvalid = true;
mutable std::shared_ptr<ITexture> cacheTexture;
mutable RectF cacheBounds;
mutable uint64_t lastPaintFrame = 0;
```

В `Style`/`Theme` можно добавить токен `cache: true` для контейнеров.

### 2.3 SubtreeCache (в Scene)
```cpp
struct CacheEntry {
  std::shared_ptr<ITexture> texture;
  Size size;
  float dpiScale;
  uint64_t lastUsedFrame;
  RectF bounds;
  size_t bytes; // w*h*4
};

class SubtreeCache {
  std::unordered_map<SceneNode*, CacheEntry> map_;
  size_t budgetBytes_ = 64 * 1024 * 1024; // 64MB
  size_t usedBytes_ = 0;
  // LRU список
};
```

- Ключ: `node id + size + dpi`. Размер текстуры = `globalBounds.size * dpiScale`, округленный до POT? Нет, оставить NPOT, но clamped к `maxTextureSize` бэкенда.
- Создание: `backend->createRenderTarget(w,h)`, `beginRenderTarget(tex)`, `node->draw(PaintContext{rtCanvas,...})`, `endRenderTarget()`.
- Использование: `canvas.drawTexture(cacheTexture, {dst = bounds, src = null})`.

### 2.4 Инвалидация
- `SceneNode::addDamageRect` уже есть (stage 6b). При `markStyleDirty`/`markLayoutDirty` → `cacheInvalid = true` для узла и всех предков с `cacheAsTexture`.
- При `LayoutManager::arrangeNode` — если `oldBounds != newBounds` → инвалидация кэша узла и предков.
- При `Scene::onNodeRemoved` — удалить entry, освободить память.
- Damage bubbling: если `damageUnion` пересекается с `cacheBounds` кэшированного узла → инвалидация.
- Авто-инвалидация: если узел `autoCache` и его дети имеют `hasDamage` или `layoutDirty` → инвалидация.

### 2.5 LRU и бюджет
- При `usedBytes_ > budgetBytes_` — выкинуть LRU хвосты (самые старые `lastUsedFrame`).
- При ресайзе окна — инвалидировать все, у кого `size != newSize`.
- Метрики: `cacheHit/miss`, `cacheMemory`, `cacheCount` — в `FpsOverlay` или `Log`.

### 2.6 Этапы внедрения
1. **API**: `IRenderBackend::createRenderTarget/begin/end` уже есть (stage 6b) — покрыть тестами `SDLRendererTests::RenderTargetCreatesTexture`.
2. **Флаги узла**: добавить `cacheAsTexture/autoCache/cacheInvalid/cacheTexture` в `SceneNode`, `isCacheValid()`.
3. **Ручной кэш**: в `SceneNode::draw(PaintContext)` — если `cacheAsTexture && cacheTexture && !cacheInvalid` → `drawTexture` + return, иначе рендер в текстуру если флаг стоит и размер валиден.
4. **Авто-кеш эвристика**: счетчик `paintCount`, если узел рисуется >30 кадров без изменений и `childrenCount > 10` или `paintTime > 0.5ms` → включить `autoCache`.
5. **Инвалидация**: подключить `damageRects` к кэшу, добавить `invalidateCache()` в `mark*Dirty`.
6. **LRU**: `SubtreeCache` в `Scene`, бюджет 64MB, `clearCaches()` при OOM.
7. **Бенчмарк**: сцена `frames` с `cacheAsTexture` на root container — ожидаем −50% CPU, draw calls 1 вместо N.

### 2.7 Риски
- Текстура больше max texture size (обычно 4096/8192) — fallback в обычный draw.
- Прозрачность и клип: кэш должен включать клип-контент, иначе артефакты.
- DPI смена — инвалидация.

---

## 3. План ввода батчинга примитивов (Batch Builder)

### 3.1 Текущее состояние (stage 6b)
- `SDLRenderer`: `fillRectBatch_` — `vector<Rect>` + `Color`, лимит 256, flush через один `SDL_RenderGeometry` (4 verts, 6 indices на rect). Flush на смене состояния (clip, texture, line, circle, etc).
- Остальные примитивы — по 1 `RenderGeometry` на вызов.

### 3.2 Цель
Снизить draw calls с O(N) до O(stateChanges). Для сцены из 100 кнопок (fill + border + text) сейчас: 100 fillRect (батчится в 1) + 100 strokeRect (100) + 100 text (100) = 201. После батчинга: 1 solid rect batch + 1 rounded rect batch + 1 text batch = 3.

### 3.3 Batch state machine
```cpp
struct BatchKey {
  BlendMode blend = Alpha;
  std::shared_ptr<ITexture> texture; // null = solid
  Rect clip; // current clip stack top, empty = no clip
  // для rounded rect: radius bucket (0,4,8,12,16) — чтобы не мешать разные радиусы в один батч если геометрия разная
};

struct Batch {
  BatchKey key;
  std::vector<SDL_Vertex> verts;
  std::vector<int> indices;
};

class BatchBuilder {
  std::vector<Batch> batches_;
  Batch* current_ = nullptr;
  static constexpr size_t kMaxVerts = 2048;
  static constexpr size_t kMaxIndices = 4096;
  void beginBatch(BatchKey key);
  void flush();
};
```

- Группировка по `key`: consecutive draw calls с одинаковым `texture/blend/clip` попадают в один `Batch`.
- Flush условия: смена `key`, превышение `kMaxVerts/kMaxIndices`, вызов `pushClip/popClip/clear/present/begin/endRenderTarget/drawTexture с другой текстурой`.

### 3.4 Фазы батчинга
1. **Solid rects** — уже есть, расширить лимит до 2048 verts, добавить `clip` в ключ.
2. **Rounded rects** — bucket по `radius` (квантовать до 1px), одинаковый радиус → один батч (fan геометрия, 52 сегмента). Разные радиусы — разные батчи.
3. **Circles** — bucket по радиусу? Или один батч для всех кругов с одинаковым цветом и клипом, но разный радиус требует разную геометрию — можно батчить как triangle fans с разным числом вершин, но индексы пересчитывать.
4. **Thick lines** — квады, батчатся как rects если одинаковая толщина и цвет.
5. **Text** — белый глиф-атлас уже батчится через `drawTexture` с tint, но каждый глиф — отдельный `drawTexture`. Нужно: собрать все глифы одного шрифта/размера в один `Batch` с текстурой атласа (страница). Для белого per-string пути — уже 1 draw call на строку, но можно батчить строки одного цвета.
6. **Gradient** — per-vertex color, батчится если одинаковый `angle` bucket и цвета.

### 3.5 Интеграция в SDLRenderer
- Поля: `BatchBuilder batchBuilder_;`
- Методы: `ensureBatch(key)`, `flushAll()`.
- `fillRect(Fill)` → если `gradient` null → `batchBuilder_.addRect(rect, color, clip)`, иначе `flush + fillRectGradientGeometry`.
- `fillRoundRect` → `batchBuilder_.addRoundRect(rect, radius, fill, clip)`.
- `fillCircle` → `addCircle`.
- `drawLine` → `addThickLine`.
- `drawTexture(TextureDraw)` → если текстура == текущей батч-текстуре и клип совпадает → добавить квад, иначе flush.
- `beginFrame/endFrame/clear/present/pushClip/popClip` → flush.

### 3.6 Метрики
- `drawCalls`, `batchedRects`, `batchesFlushed` — счетчики в `SDLRenderer`, вывод в `FpsOverlay`.
- Бенчмарк `frames`: ожидаем −70% draw calls, −20% CPU.

### 3.7 Этапы
1. **BatchBuilder skeleton**: структура `BatchKey/Batch`, `beginBatch/flush`, интеграция в `fillRect` (заменить существующий `fillRectBatch_`).
2. **Rounded rect batch**: `addRoundRect` с radius bucket.
3. **Circle/line batch**: `addCircle/addLine`.
4. **Texture batch**: `drawTexture` батчинг по текстуре (для текста и Image).
5. **Clip-aware**: ключ включает clip rect, flush при смене клипа.
6. **Бенчмарк и профилирование**: `benchmark --json` до/после, `scripts/compare.ps1` порог 10%.

### 3.8 Риски
- Слишком большой батч → `SDL_RenderGeometry` лимит вершин (зависит от драйвера) — держать `kMaxVerts` консервативным.
- Прозрачность: порядок важен, батчинг не должен менять порядок наложения полупрозрачных примитивов — flush при смене alpha.
- Текстуры: разные текстуры нельзя батчить в один вызов без атласа.

---

## 4. Связь с текущим кодом

- `SDLRenderer.cpp`: `flushFillRectBatch/batchFillRect` — заменить на `BatchBuilder`.
- `Brush.h`: `LinearGradient`, `Shadow` — уже есть, батчинг должен учитывать gradient как часть ключа или как no-batch.
- `Scene.cpp`: `damageUnion_` — использовать для culling кэша и для пропуска `beginFrame` когда `!hasDamage && !fullRedraw`.
- `SceneNode`: добавить `cacheAsTexture` флаг, `invalidateCache()`.
- `CMakeLists.txt`: уже включает `third_party` и `stb_image`.

---

## 5. Чеклист приемки

- [x] `third_party/stb/stb_image.h` (v2.30) + `stb_image_impl.cpp` + `ImageData::loadFromFile/Memory` — собрано, тест ручной загрузки PNG/JPG.
- [ ] Legacy audit задокументирован (этот файл).
- [ ] `AUTO_CACHE_BATCHING_PLAN.md` — цели, инвалидация, LRU, batch state machine.
- [ ] Stage 7 (удаление `IRenderer`/`CanvasAdapter`) — отдельный PR после утверждения плана.
- [ ] Subtree cache: ручной флаг, LRU, damage-инвалидация, бенчмарк `frames` −50% CPU.
- [ ] Batching: `BatchBuilder`, clip-aware, texture batch, метрики draw calls, бенчмарк −70% calls.
