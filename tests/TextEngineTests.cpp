#include <gtest/gtest.h>

#include <algorithm>
#include <map>
#include <memory>
#include <string>
#include <tuple>
#include <vector>

#include "DxvUI/Log.h"
#include "DxvUI/Scene.h"
#include "DxvUI/SceneNode.h"
#include "DxvUI/interfaces/IRenderer.h"
#include "DxvUI/interfaces/ITextEngine.h"
#include "DxvUI/style/Colors.h"
#include "DxvUI/style/StyleManager.h"
#include "DxvUI/style/Theme.h"
#include "DxvUI/text/ITextValidator.h"
#include "DxvUI/widgets/Label.h"
#include "DxvUI/widgets/TextEdit.h"

using namespace DxvUI;

namespace {

class LoggerEnvironment : public ::testing::Environment {
   public:
    void SetUp() override { Log::init(); }
};

::testing::Environment* const g_logger_environment =
    ::testing::AddGlobalTestEnvironment(new LoggerEnvironment);

class FakeTexture : public ITexture {
   public:
    FakeTexture(int w, int h) : w_(w), h_(h) {}
    int getWidth() const override { return w_; }
    int getHeight() const override { return h_; }

   private:
    int w_;
    int h_;
};

// Fake engine with glyph atlas semantics: layout cached per (font,text), glyphs per (font,codepoint),
// color NOT part of key. Rasterize path kept for legacy but not used by Label.
class FakeTextEngine : public ITextEngine {
   public:
    int rasterCount = 0;
    int layoutCount = 0;
    int glyphCount = 0;

    std::shared_ptr<IFont> getFont(const std::string& path, int size) override {
        if (path.empty() || size <= 0) return nullptr;
        const std::string key = path + ":" + std::to_string(size);
        if (auto it = fonts.find(key); it != fonts.end()) {
            return it->second;
        }
        auto font = std::make_shared<IFont>();
        fonts[key] = font;
        return font;
    }

    std::shared_ptr<IFont> getFontForFamily(const std::string& family, int size) override {
        if (family.empty() || size <= 0) return nullptr;
        return getFont("family:" + family, size);
    }

    void registerFontFamily(const std::string&, const std::string&) override {}

    TextMetrics measure(const IFont& font, const std::string& text) override {
        auto layout = layoutText(font, text);
        return layout.metrics;
    }

    int measurePrefix(const IFont& font, const std::string& text, size_t byteCount) override {
        auto layout = layoutText(font, text);
        return layout.caretXAt(std::min(byteCount, text.size()));
    }

    size_t charIndexAtX(const IFont& font, const std::string& text, int maxWidth) override {
        auto layout = layoutText(font, text);
        return layout.charIndexAtX(maxWidth);
    }

    LineMetrics lineMetrics(const IFont&) override { return {12, 4, 16}; }

    std::shared_ptr<ITexture> rasterize(const IFont& font, const std::string& text,
                                        const Color& color) override {
        if (text.empty()) return nullptr;
        const auto key = std::make_tuple(&font, text, color.toUint32());
        if (auto it = textures.find(key); it != textures.end()) {
            return it->second;
        }
        rasterCount++;
        auto texture = std::make_shared<FakeTexture>(static_cast<int>(text.size()) * 8, 16);
        textures[key] = texture;
        return texture;
    }

    size_t getTextureCacheCount() const override { return textures.size(); }

    // Stage 3
    TextLayout layoutText(const IFont& font, std::string_view text) override {
        std::string s(text);
        auto key = std::make_pair(&font, s);
        if (auto it = layouts.find(key); it != layouts.end()) {
            return it->second;
        }
        layoutCount++;

        TextLayout layout;
        layout.text = s;
        layout.lineMetrics = lineMetrics(font);
        layout.metrics.width = static_cast<int>(s.size()) * 8;
        layout.metrics.height = 16;

        // Build glyphs: one per byte for simplicity (fake engine uses 8px per byte)
        for (size_t i = 0; i < s.size();) {
            size_t start = i;
            // handle UTF-8 code point boundaries for fake: treat each codepoint as 1 byte for ASCII,
            // but for multi-byte, count bytes until next start.
            size_t next = start + 1;
            while (next < s.size() && (static_cast<unsigned char>(s[next]) & 0xC0) == 0x80) {
                ++next;
            }
            size_t len = next - start;
            uint32_t cp = static_cast<unsigned char>(s[start]);
            // Get or create glyph
            auto gkey = std::make_pair(&font, cp);
            if (glyphs.find(gkey) == glyphs.end()) {
                glyphCount++;
                Glyph g;
                g.codepoint = cp;
                g.texture = std::make_shared<FakeTexture>(8, 16);
                g.width = 8;
                g.height = 16;
                g.advance = 8;
                g.minX = 0;
                g.maxX = 8;
                g.minY = -4;
                g.maxY = 12;
                glyphs[gkey] = g;
            }
            layout.glyphs.push_back(glyphs[gkey]);
            layout.xOffsets.push_back(static_cast<int>(start) * 8);
            layout.byteOffsets.push_back(start);
            layout.byteLengths.push_back(len);
            i = next;
        }

        layouts[key] = layout;
        return layout;
    }

    void drawLayout(ICanvas& canvas, const TextLayout& layout, const RectF& box,
                    const TextPaint& paint) override {
        // Simulate drawing each glyph via canvas.drawTexture with tint
        for (size_t i = 0; i < layout.glyphs.size(); ++i) {
            const auto& g = layout.glyphs[i];
            if (!g.texture) continue;
            float x = box.x + layout.xOffsets[i];
            float y = box.y;
            ICanvas::TextureDraw td;
            td.dst = RectF(x, y, static_cast<float>(g.width), static_cast<float>(g.height));
            td.tint = paint.color;
            canvas.drawTexture(g.texture, td);
        }
    }

    size_t getGlyphCacheCount() const override { return glyphs.size(); }
    size_t getLayoutCacheCount() const override { return layouts.size(); }

   private:
    std::map<std::string, std::shared_ptr<IFont>> fonts;
    std::map<std::tuple<const IFont*, std::string, uint32_t>, std::shared_ptr<ITexture>> textures;
    std::map<std::pair<const IFont*, std::string>, TextLayout> layouts;
    std::map<std::pair<const IFont*, uint32_t>, Glyph> glyphs;
};

class FakeClipboard : public IClipboard {
   public:
    std::string text;
    std::string getText() override { return text; }
    bool setText(const std::string& t) override {
        text = t;
        return true;
    }
};

class FakeRenderer : public IRenderer, public ICanvas {
   public:
    FakeTextEngine engine;
    FakeClipboard clipboard;

    ITextEngine& getTextEngine() override { return engine; }
    IClipboard& getClipboard() override { return clipboard; }

    void clear(const Color&) override {}
    void present() override {}
    Size getViewportSize() const override { return {800, 600}; }

    float getDpiScale() const override { return 1.0f; }

    ICanvas& beginFrame(const Color&) override { return *this; }
    void endFrame() override {}

    std::shared_ptr<ITexture> createTexture(const ImageData&) override { return nullptr; }
    std::shared_ptr<ITexture> createRenderTarget(int, int) override { return nullptr; }
    void beginRenderTarget(const std::shared_ptr<ITexture>&) override {}
    void endRenderTarget() override {}

    // ICanvas float-based (stage 5) – no-op for fake
    void pushClip(const RectF&) override {}
    void popClip() override {}
    void drawTexture(const std::shared_ptr<ITexture>&, const RectF&) override {}
    void drawTexture(const std::shared_ptr<ITexture>&, const ICanvas::TextureDraw&) override {}
    void fillRect(const RectF&, const Fill&) override {}
    void strokeRect(const RectF&, const Stroke&) override {}
    void fillRoundRect(const RectF&, float, const Brush&) override {}
    void fillCircle(const PointF&, float, const Brush&) override {}
    void strokeArc(const PointF&, float, float, float, const Stroke&) override {}
    void fillPolygon(std::span<const PointF>, const Fill&) override {}
    void drawLine(const PointF&, const PointF&, const Stroke&) override {}

    void setCursor(CursorType) override {}
    CursorType getCursor() const override { return CursorType::Arrow; }

    void pushClipRect(const Rect&) override {}
    void popClipRect() override {}

    void drawTexture(const std::shared_ptr<ITexture>&, const Rect&) override {}
    void drawTexture(const std::shared_ptr<ITexture>&, const TextureDrawDesc&) override {}

    void drawRect(const Rect&, const Border&) override {}
    void fillRect(const Rect&, const Color&) override {}
    void fillRect(const Rect&, const Color&, const Border&) override {}
    void drawLine(int, int, int, int, const Color&, int) override {}
    void drawCircle(int, int, int, const Border&) override {}
    void fillCircle(int, int, int, const Color&) override {}
    void fillCircle(int, int, int, const Color&, const Border&) override {}
    void drawArc(int, int, int, float, float, const Border&) override {}
    void drawRoundRect(const Rect&, int, const Border&) override {}
    void fillRoundRect(const Rect&, int, const Color&) override {}
    void fillRoundRect(const Rect&, int, const Color&, const Border&) override {}
    void fillPolygon(const std::vector<PointI>&, const Color&) override {}
};

struct LabelFixture {
    std::shared_ptr<Scene> scene = Scene::create();
    std::shared_ptr<SceneNode> root = scene->getRoot();
    std::shared_ptr<Label> label = Label::create("label", "Hello");
    Theme theme;
    StyleManager manager{theme};
    FakeRenderer renderer;

    LabelFixture() {
        root->setStyle({.fontSize = 16, .fontFamily = "Sans"}, WidgetState::Normal);
        root->addChild(label);
        scene->setRenderer(&renderer);
        manager.resolveDirtyStyles(root);
        root->measure({800, 600});
        root->arrange({0, 0, 800, 600});
    }
};

}  // namespace

TEST(LabelTextEngineTest, LayoutCachedAndColorDoesNotRecreate) {
    LabelFixture f;

    f.root->draw(f.renderer);
    f.root->draw(f.renderer);
    EXPECT_EQ(f.renderer.engine.layoutCount, 1);
    // Glyph cache: 'H','e','l','o' = 4 distinct codepoints
    EXPECT_EQ(f.renderer.engine.glyphCount, 4);

    f.label->setText("World");
    f.root->draw(f.renderer);
    EXPECT_EQ(f.renderer.engine.layoutCount, 2);
    // 'W','o','r','l','d' adds W,r,d (o,l already cached) => total 7
    EXPECT_EQ(f.renderer.engine.glyphCount, 7);

    // Different color must NOT create new layout (tint path)
    f.label->setStyle({.textColor = Colors::White}, WidgetState::Normal);
    f.manager.resolveDirtyStyles(f.root);
    f.root->draw(f.renderer);
    EXPECT_EQ(f.renderer.engine.layoutCount, 2);
    EXPECT_EQ(f.renderer.engine.glyphCount, 7);
}

TEST(LabelTextEngineTest, MeasureComesFromEngine) {
    LabelFixture f;
    EXPECT_FLOAT_EQ(f.label->getGlobalBounds().width, 40);
    EXPECT_FLOAT_EQ(f.label->getGlobalBounds().height, 16);
}

TEST(LabelTextEngineTest, EmptyTextSkipsLayout) {
    LabelFixture f;
    f.label->setText("");
    f.root->draw(f.renderer);
    EXPECT_EQ(f.renderer.engine.layoutCount, 0);
}

TEST(TextEngineCacheCountTest, TracksDistinctRasterizations) {
    FakeTextEngine engine;
    auto font = engine.getFont("fake.ttf", 16);
    ASSERT_NE(font, nullptr);

    auto texA = engine.rasterize(*font, "One", Colors::Black);
    auto texB = engine.rasterize(*font, "Two", Colors::Black);
    EXPECT_EQ(engine.getTextureCacheCount(), 2u);

    engine.rasterize(*font, "One", Colors::Black);
    EXPECT_EQ(engine.getTextureCacheCount(), 2u);

    engine.rasterize(*font, "One", Colors::White);
    EXPECT_EQ(engine.getTextureCacheCount(), 3u);

    engine.rasterize(*font, "", Colors::Black);
    EXPECT_EQ(engine.getTextureCacheCount(), 3u);
    EXPECT_NE(texA, nullptr);
    EXPECT_NE(texB, nullptr);
}

TEST(TextEngineGlyphCacheTest, GlyphCacheKeyWithoutColor) {
    FakeTextEngine engine;
    auto font = engine.getFont("fake.ttf", 16);
    ASSERT_NE(font, nullptr);

    engine.layoutText(*font, "Hello");
    EXPECT_EQ(engine.getGlyphCacheCount(), 4u); // H,e,l,o

    engine.layoutText(*font, "Hello"); // cached
    EXPECT_EQ(engine.getGlyphCacheCount(), 4u);
    EXPECT_EQ(engine.getLayoutCacheCount(), 1u);

    engine.layoutText(*font, "World");
    // Adds W,r,d (o,l already)
    EXPECT_EQ(engine.getGlyphCacheCount(), 7u);
    EXPECT_EQ(engine.getLayoutCacheCount(), 2u);

    // Same text different color would be same layout if we used drawLayout, but layoutText itself
    // does not include color, so no new glyphs.
}

TEST(LabelTextEngineTest, MissingFontMeasuresZero) {
    LabelFixture f;
    f.label->setStyle({.fontFamily = ""}, WidgetState::Normal);
    f.manager.resolveDirtyStyles(f.root);
    f.root->measure({800, 600});
    f.root->arrange({0, 0, 800, 600});
    EXPECT_FLOAT_EQ(f.label->getGlobalBounds().width, 0);
    EXPECT_FLOAT_EQ(f.label->getGlobalBounds().height, 0);
}

TEST(TextEngineFamilyTest, FamilyResolvesToFont) {
    FakeTextEngine engine;
    EXPECT_EQ(engine.getFontForFamily("", 16), nullptr);
    EXPECT_EQ(engine.getFontForFamily("Sans", 0), nullptr);
    auto font = engine.getFontForFamily("Sans", 16);
    ASSERT_NE(font, nullptr);
    EXPECT_EQ(engine.getFontForFamily("Sans", 16), font);
}

namespace {
struct PrefixFixture {
    FakeTextEngine engine;
    std::shared_ptr<IFont> font = engine.getFont("fake.ttf", 16);
};
}  // namespace

TEST(TextEnginePrefixTest, MeasurePrefixWidths) {
    PrefixFixture f;
    EXPECT_EQ(f.engine.measurePrefix(*f.font, "Hello", 0), 0);
    EXPECT_EQ(f.engine.measurePrefix(*f.font, "Hello", 3), 24);
    EXPECT_EQ(f.engine.measurePrefix(*f.font, "Hello", 5), 40);
    EXPECT_EQ(f.engine.measurePrefix(*f.font, "Hello", 100), 40);
}

TEST(TextEnginePrefixTest, CharIndexAtX) {
    PrefixFixture f;
    EXPECT_EQ(f.engine.charIndexAtX(*f.font, "Hello", 0), 0u);
    EXPECT_EQ(f.engine.charIndexAtX(*f.font, "Hello", 16), 2u);
    EXPECT_EQ(f.engine.charIndexAtX(*f.font, "Hello", 40), 5u);
    EXPECT_EQ(f.engine.charIndexAtX(*f.font, "Hello", 1000), 5u);
    EXPECT_EQ(f.engine.charIndexAtX(*f.font, "Hello", -1), 0u);
    EXPECT_EQ(f.engine.charIndexAtX(*f.font, "", 100), 0u);
}

TEST(TextEnginePrefixTest, CharIndexAtXNeverSplitsCodePoint) {
    PrefixFixture f;
    const std::string cyrillic = "\xD0\x90\xD0\xB1";
    EXPECT_EQ(f.engine.charIndexAtX(*f.font, cyrillic, 16), 2u);
    EXPECT_EQ(f.engine.charIndexAtX(*f.font, cyrillic, 33), 4u);
}

// --- TextEdit widget integration ---

namespace {

struct TextEditFixture {
    std::shared_ptr<Scene> scene = Scene::create();
    std::shared_ptr<SceneNode> root = scene->getRoot();
    std::shared_ptr<TextEdit> field = TextEdit::create("field", "Hello");
    Theme theme;
    StyleManager manager{theme};
    FakeRenderer renderer;

    TextEditFixture() {
        root->setStyle({.fontSize = 16, .fontFamily = "Sans"}, WidgetState::Normal);
        root->addChild(field);
        scene->setRenderer(&renderer);
        manager.resolveDirtyStyles(root);
        root->measure({800, 600});
        root->arrange({0, 0, 800, 600});
    }

    void press(int x, int y) {
        DxvEvent e;
        e.type = EventType::MouseDown;
        e.mouse.x = x;
        e.mouse.y = y;
        e.mouse.button = MouseButton::Left;
        scene->processEvent(e);
    }

    void release(int x, int y) {
        DxvEvent e;
        e.type = EventType::MouseUp;
        e.mouse.x = x;
        e.mouse.y = y;
        e.mouse.button = MouseButton::Left;
        scene->processEvent(e);
    }

    void dragTo(int x, int y) {
        DxvEvent e;
        e.type = EventType::MouseMove;
        e.mouse.x = x;
        e.mouse.y = y;
        e.mouse.button = MouseButton::Left;
        scene->processEvent(e);
    }

    void keyDown(KeyCode sym, uint16_t mod = KeyModifier::None) {
        DxvEvent e;
        e.type = EventType::KeyDown;
        e.key.sym = sym;
        e.key.mod = mod;
        scene->processEvent(e);
    }

    void typeText(const char* text) {
        DxvEvent e;
        e.type = EventType::TextInput;
        e.text = text;
        scene->processEvent(e);
    }
};

}  // namespace

TEST(TextEditTest, MeasuresTextPlusPadding) {
    TextEditFixture f;
    EXPECT_FLOAT_EQ(f.field->getGlobalBounds().width, 50);
    EXPECT_FLOAT_EQ(f.field->getGlobalBounds().height, 22);
}

TEST(TextEditTest, MeasureAccountsForExplicitBorder) {
    std::shared_ptr<Scene> scene = Scene::create();
    auto root = scene->getRoot();
    auto field = TextEdit::create("field", "Hello");
    root->setStyle({.fontSize = 16, .fontFamily = "Sans"}, WidgetState::Normal);
    field->setStyle({.borderThickness = 2}, WidgetState::Normal);
    root->addChild(field);

    FakeRenderer renderer;
    scene->setRenderer(&renderer);
    Theme theme;
    StyleManager manager{theme};
    manager.resolveDirtyStyles(root);
    root->measure({800, 600});
    root->arrange({0, 0, 800, 600});

    EXPECT_FLOAT_EQ(field->getGlobalBounds().width, 52);
    EXPECT_FLOAT_EQ(field->getGlobalBounds().height, 24);
}

TEST(TextEditTest, SetTextAndGetText) {
    TextEditFixture f;
    EXPECT_EQ(f.field->getText(), "Hello");
    f.field->setText("World!");
    EXPECT_EQ(f.field->getText(), "World!");
    EXPECT_EQ(f.field->getEditor().getText(), "World!");
}

TEST(TextEditTest, PlaceholderShownWhileEmptyAndUnfocused) {
    TextEditFixture f;
    f.field->setText("");
    f.field->setPlaceholder("Hint");
    f.root->draw(f.renderer);
    EXPECT_EQ(f.renderer.engine.getLayoutCacheCount(), 1u);
}

TEST(TextEditTest, PlaceholderHiddenWhenFocused) {
    TextEditFixture f;
    f.field->setText("");
    f.field->setPlaceholder("Hint");
    f.press(10, 10);
    f.release(10, 10);
    f.root->draw(f.renderer);
    EXPECT_EQ(f.renderer.engine.getLayoutCacheCount(), 0u);
}

TEST(TextEditTest, PlaceholderHiddenWhenTextPresent) {
    TextEditFixture f;
    f.field->setPlaceholder("Hint");
    f.root->draw(f.renderer);
    EXPECT_EQ(f.renderer.engine.getLayoutCacheCount(), 1u);
}

TEST(TextEditTest, MouseDownPlacesCaretAndFocuses) {
    TextEditFixture f;
    f.press(24, 10);
    f.release(24, 10);
    EXPECT_EQ(f.field->getCurrentState(), WidgetState::Focused);
    EXPECT_EQ(f.field->getEditor().getCaret(), 2u);
    EXPECT_FALSE(f.field->getEditor().hasSelection());
}

TEST(TextEditTest, KeyboardEditingWhileFocused) {
    TextEditFixture f;
    f.press(4, 10);
    f.release(4, 10);
    ASSERT_EQ(f.field->getCurrentState(), WidgetState::Focused);

    f.keyDown(KeyCode::Backspace);
    EXPECT_EQ(f.field->getText(), "Hello");

    f.typeText("!");
    EXPECT_EQ(f.field->getText(), "!Hello");

    f.keyDown(KeyCode::Right);
    f.keyDown(KeyCode::Backspace);
    EXPECT_EQ(f.field->getText(), "!ello");

    f.keyDown(KeyCode::Home);
    f.keyDown(KeyCode::Delete);
    EXPECT_EQ(f.field->getText(), "ello");

    f.keyDown(KeyCode::End);
    f.typeText("!");
    EXPECT_EQ(f.field->getText(), "ello!");

    f.keyDown(KeyCode::Z, KeyModifier::Ctrl);
    EXPECT_EQ(f.field->getText(), "ello");
    f.keyDown(KeyCode::Y, KeyModifier::Ctrl);
    EXPECT_EQ(f.field->getText(), "ello!");
}

TEST(TextEditTest, KeyboardIgnoredWithoutFocus) {
    TextEditFixture f;
    f.keyDown(KeyCode::Backspace);
    EXPECT_EQ(f.field->getText(), "Hello");
}

TEST(TextEditTest, SelectAllAndSubmit) {
    TextEditFixture f;
    f.press(4, 10);
    f.keyDown(KeyCode::A, KeyModifier::Ctrl);
    EXPECT_TRUE(f.field->getEditor().hasSelection());
    EXPECT_EQ(f.field->getEditor().selectedText(), "Hello");

    std::string submitted;
    f.field->setOnSubmit([&](const std::string& text) { submitted = text; });
    f.keyDown(KeyCode::Enter);
    EXPECT_EQ(submitted, "Hello");
}

TEST(TextEditTest, MouseDragExtendsSelection) {
    TextEditFixture f;
    f.press(4, 10);
    f.dragTo(24, 10);
    const auto& editor = f.field->getEditor();
    EXPECT_TRUE(editor.hasSelection());
    EXPECT_EQ(editor.getSelectionStart(), 0u);
    EXPECT_EQ(editor.getSelectionEnd(), 2u);
}

TEST(TextEditTest, BackspaceDeletesWholeUtf8CodePoint) {
    TextEditFixture f;
    f.field->setText("\xD0\x90\xD0\xB1");
    f.root->measure({800, 600});
    f.root->arrange({0, 0, 800, 600});
    f.press(4, 10);
    f.keyDown(KeyCode::End);
    f.keyDown(KeyCode::Backspace);
    EXPECT_EQ(f.field->getText(), "\xD0\x90");
}

TEST(TextEditTest, ValidatorBlocksTypingAtWidgetLevel) {
    TextEditFixture f;
    using validators::digitsOnly;
    f.field->setText("");
    f.field->setValidator(digitsOnly());
    f.press(4, 10);
    f.typeText("1");
    f.typeText("2");
    f.typeText("a");
    f.typeText("3");
    f.typeText("4");
    EXPECT_EQ(f.field->getText(), "1234");
}

TEST(TextEditTest, ValidatorBlocksPasteWhole) {
    TextEditFixture f;
    using validators::range;
    f.field->setValidator(range(0, 100));
    f.field->setText("75");
    f.press(4, 10);
    f.keyDown(KeyCode::A, KeyModifier::Ctrl);
    f.renderer.clipboard.text = "abc";
    f.keyDown(KeyCode::V, KeyModifier::Ctrl);
    EXPECT_EQ(f.field->getText(), "75");

    f.renderer.clipboard.text = "42";
    f.keyDown(KeyCode::V, KeyModifier::Ctrl);
    EXPECT_EQ(f.field->getText(), "42");
}

TEST(TextEditTest, FiresChangeOnTextMutation) {
    TextEditFixture f;
    int changes = 0;
    std::unique_ptr<DxvUI::SceneNode::Connection> conn = f.field->on(
        DxvUI::EventType::Change, [&](DxvUI::DxvEvent&, const DxvUI::UIContext&) { ++changes; });

    EXPECT_EQ(changes, 0);

    f.press(4, 10);
    f.release(4, 10);
    f.typeText("X");
    EXPECT_EQ(changes, 1);

    f.field->setText("Hello");
    EXPECT_EQ(changes, 2);

    f.field->setText("Hello");
    EXPECT_EQ(changes, 2);

    f.keyDown(KeyCode::Z, KeyModifier::Ctrl);
    EXPECT_EQ(changes, 3);
    f.keyDown(KeyCode::Y, KeyModifier::Ctrl);
    EXPECT_EQ(changes, 4);
}

TEST(TextEditTest, ChangeCarriesBindingStringValue) {
    TextEditFixture f;
    std::string observed;
    std::unique_ptr<DxvUI::SceneNode::Connection> conn =
        f.field->on(DxvUI::EventType::Change, [&](DxvUI::DxvEvent& e, const DxvUI::UIContext&) {
            observed = e.getTarget()->getBinding()->getString();
        });

    f.press(4, 10);
    f.release(4, 10);
    f.typeText("!");
    EXPECT_EQ(observed, "!Hello");

    f.field->setText("World");
    EXPECT_EQ(observed, "World");
}
