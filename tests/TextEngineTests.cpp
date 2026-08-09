#include <SDL.h>
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
#include "DxvUI/UIBinding.h"
#include "DxvUI/interfaces/IRenderer.h"
#include "DxvUI/style/Colors.h"
#include "DxvUI/style/StyleManager.h"
#include "DxvUI/style/Theme.h"
#include "DxvUI/text/ITextEngine.h"
#include "DxvUI/widgets/Label.h"
#include "DxvUI/widgets/TextEdit.h"

using namespace DxvUI;

namespace {

// SceneNode's destructor logs via DxvUI::Log, which requires an initialized
// logger. Install a global test environment so the logger exists for the whole
// test binary (same pattern as WidgetTests.cpp).
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

// Backend-neutral fake with the same caching contract as the real engine: one
// font per (path, size), measurement = 8px per UTF-8 byte with height 16, and
// rasterize() counts every actually created texture (a cache hit does not).
class FakeTextEngine : public ITextEngine {
   public:
    int rasterCount = 0;

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

    TextMetrics measure(const IFont&, const std::string& text) override {
        return {static_cast<int>(text.size()) * 8, 16};
    }

    int measurePrefix(const IFont&, const std::string& text, size_t byteCount) override {
        return static_cast<int>(std::min(byteCount, text.size())) * 8;
    }

    size_t charIndexAtX(const IFont&, const std::string& text, int maxWidth) override {
        if (maxWidth <= 0) return 0;
        return std::min(text.size(), static_cast<size_t>(maxWidth / 8));
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

   private:
    std::map<std::string, std::shared_ptr<IFont>> fonts;
    std::map<std::tuple<const IFont*, std::string, uint32_t>, std::shared_ptr<ITexture>> textures;
};

// Backend-neutral clipboard stub so FakeRenderer satisfies IRenderer. These
// tests never touch the clipboard; the member only exists for the interface.
class FakeClipboard : public IClipboard {
   public:
    std::string text;
    std::string getText() override { return text; }
    bool setText(const std::string& t) override {
        text = t;
        return true;
    }
};

// Minimal IRenderer so Label::onMeasure and Label::drawContent can be exercised
// without an SDL backend or a real font file.
class FakeRenderer : public IRenderer {
   public:
    FakeTextEngine engine;
    FakeClipboard clipboard;

    ITextEngine& getTextEngine() override { return engine; }
    IClipboard& getClipboard() override { return clipboard; }

    void clear(const Color&) override {}
    void present() override {}
    Size getViewportSize() const override { return {800, 600}; }

    void setCursor(CursorType) override {}
    CursorType getCursor() const override { return CursorType::Arrow; }

    void pushClipRect(const Rect&) override {}
    void popClipRect() override {}

    void drawTexture(std::shared_ptr<ITexture>&, const Rect&) override {}

    void setDrawColor(const Color&) override {}
    Color getDrawColor() const override { return {}; }

    void drawRect(const Rect&) override {}
    void fillRect(const Rect&) override {}
    void drawRect(const Rect&, const Color&) override {}
    void fillRect(const Rect&, const Color&) override {}
    void drawRect(const Rect&, const Border&) override {}
    void fillRect(const Rect&, const Color&, const Border&) override {}

    void drawLine(int, int, int, int) override {}
    void drawLine(int, int, int, int, const Color&) override {}

    void drawCircle(int, int, int) override {}
    void fillCircle(int, int, int) override {}
    void drawCircle(int, int, int, const Color&) override {}
    void fillCircle(int, int, int, const Color&) override {}
    void drawCircle(int, int, int, const Border&) override {}
    void fillCircle(int, int, int, const Color&, const Border&) override {}

    void drawArc(int, int, int, float, float) override {}
    void drawArc(int, int, int, float, float, const Color&) override {}
    void drawArc(int, int, int, float, float, const Border&) override {}

    void drawRoundRect(const Rect&, int) override {}
    void fillRoundRect(const Rect&, int) override {}
    void drawRoundRect(const Rect&, int, const Color&) override {}
    void fillRoundRect(const Rect&, int, const Color&) override {}
    void drawRoundRect(const Rect&, int, const Border&) override {}
    void fillRoundRect(const Rect&, int, const Color&, const Border&) override {}

    void drawPolygon(const std::vector<PointI>&) override {}
    void fillPolygon(const std::vector<PointI>&) override {}
    void drawPolygon(const std::vector<PointI>&, const Color&) override {}
    void fillPolygon(const std::vector<PointI>&, const Color&) override {}
    void drawPolygon(const std::vector<PointI>&, const Border&) override {}
    void fillPolygon(const std::vector<PointI>&, const Color&, const Border&) override {}
};

struct LabelFixture {
    std::shared_ptr<Scene> scene = Scene::create();
    std::shared_ptr<SceneNode> root = scene->getRoot();
    std::shared_ptr<Label> label = Label::create("label", "Hello");
    Theme theme;
    StyleManager manager{theme};
    FakeRenderer renderer;

    LabelFixture() {
        root->setStyle({.fontSize = 16, .fontPath = "fake.ttf"}, WidgetState::Normal);
        root->addChild(label);
        // The label measures text through the renderer's engine, so the
        // renderer must be attached before the tree is measured.
        scene->setRenderer(&renderer);
        manager.resolveDirtyStyles(root);
        root->measure({800, 600});
        root->arrange({0, 0, 800, 600});
    }
};

}  // namespace

TEST(LabelTextEngineTest, RasterizesOnceThenRerasterizesOnChange) {
    LabelFixture f;

    // Two draws with the same (font, text, color) must not re-rasterize: the
    // texture cache lives in the text engine, not in the Label.
    f.root->draw(f.renderer);
    f.root->draw(f.renderer);
    EXPECT_EQ(f.renderer.engine.rasterCount, 1);

    // Changing the text changes the cache key, so a new texture is created.
    f.label->setText("World");
    f.root->draw(f.renderer);
    EXPECT_EQ(f.renderer.engine.rasterCount, 2);

    // A different color is also a different cache key.
    f.label->setStyle({.textColor = Colors::White}, WidgetState::Normal);
    f.manager.resolveDirtyStyles(f.root);
    f.root->draw(f.renderer);
    EXPECT_EQ(f.renderer.engine.rasterCount, 3);
}

TEST(LabelTextEngineTest, MeasureComesFromEngine) {
    LabelFixture f;
    // "Hello" is 5 UTF-8 bytes -> 5*8 = 40 wide, 16 tall in the fake engine.
    EXPECT_FLOAT_EQ(f.label->getGlobalBounds().width, 40);
    EXPECT_FLOAT_EQ(f.label->getGlobalBounds().height, 16);
}

TEST(LabelTextEngineTest, EmptyTextSkipsRasterization) {
    LabelFixture f;
    f.label->setText("");
    f.root->draw(f.renderer);
    EXPECT_EQ(f.renderer.engine.rasterCount, 0);
}

TEST(TextEngineCacheCountTest, TracksDistinctRasterizations) {
    FakeTextEngine engine;
    auto font = engine.getFont("fake.ttf", 16);
    ASSERT_NE(font, nullptr);

    auto texA = engine.rasterize(*font, "One", Colors::Black);
    auto texB = engine.rasterize(*font, "Two", Colors::Black);
    EXPECT_EQ(engine.getTextureCacheCount(), 2u);

    // A cache hit does not grow the cache.
    engine.rasterize(*font, "One", Colors::Black);
    EXPECT_EQ(engine.getTextureCacheCount(), 2u);

    // A different color is a different cache key.
    engine.rasterize(*font, "One", Colors::White);
    EXPECT_EQ(engine.getTextureCacheCount(), 3u);

    // Empty text is never cached.
    engine.rasterize(*font, "", Colors::Black);
    EXPECT_EQ(engine.getTextureCacheCount(), 3u);
    EXPECT_NE(texA, nullptr);
    EXPECT_NE(texB, nullptr);
}

TEST(LabelTextEngineTest, MissingFontMeasuresZero) {
    LabelFixture f;
    f.label->setStyle({.fontPath = ""}, WidgetState::Normal);
    f.manager.resolveDirtyStyles(f.root);
    f.root->measure({800, 600});
    f.root->arrange({0, 0, 800, 600});
    EXPECT_FLOAT_EQ(f.label->getGlobalBounds().width, 0);
    EXPECT_FLOAT_EQ(f.label->getGlobalBounds().height, 0);
}

namespace {
// A minimal fixture for the prefix-measure/hit-test API of the fake engine:
// measurement is 8px per UTF-8 byte.
struct PrefixFixture {
    FakeTextEngine engine;
    // The fake engine always creates a font for a non-empty path + valid size.
    std::shared_ptr<IFont> font = engine.getFont("fake.ttf", 16);
};
}  // namespace

TEST(TextEnginePrefixTest, MeasurePrefixWidths) {
    PrefixFixture f;
    // "Hello" = 5 bytes -> prefix widths are byteCount * 8.
    EXPECT_EQ(f.engine.measurePrefix(*f.font, "Hello", 0), 0);
    EXPECT_EQ(f.engine.measurePrefix(*f.font, "Hello", 3), 24);
    EXPECT_EQ(f.engine.measurePrefix(*f.font, "Hello", 5), 40);
    // byteCount beyond the text length is clamped.
    EXPECT_EQ(f.engine.measurePrefix(*f.font, "Hello", 100), 40);
}

TEST(TextEnginePrefixTest, CharIndexAtX) {
    PrefixFixture f;
    // "Hello" = 5 bytes at 8px each.
    EXPECT_EQ(f.engine.charIndexAtX(*f.font, "Hello", 0), 0u);
    EXPECT_EQ(f.engine.charIndexAtX(*f.font, "Hello", 16), 2u);
    EXPECT_EQ(f.engine.charIndexAtX(*f.font, "Hello", 40), 5u);
    // Wider than the text -> the whole text fits.
    EXPECT_EQ(f.engine.charIndexAtX(*f.font, "Hello", 1000), 5u);
    // Non-positive width maps to the start.
    EXPECT_EQ(f.engine.charIndexAtX(*f.font, "Hello", -1), 0u);
    // Empty text maps to the start.
    EXPECT_EQ(f.engine.charIndexAtX(*f.font, "", 100), 0u);
}

TEST(TextEnginePrefixTest, CharIndexAtXNeverSplitsCodePoint) {
    PrefixFixture f;
    // "Аб" = 4 UTF-8 bytes; byte offsets are always whole code points.
    const std::string cyrillic = "\xD0\x90\xD0\xB1";
    // 16px fits exactly two bytes ("А"); the next two bytes ("б") would need
    // 32px.
    EXPECT_EQ(f.engine.charIndexAtX(*f.font, cyrillic, 16), 2u);
    EXPECT_EQ(f.engine.charIndexAtX(*f.font, cyrillic, 33), 4u);
}

// --- TextEdit widget integration (measure, focus, keyboard/mouse editing) ---
namespace {

struct TextEditFixture {
    std::shared_ptr<Scene> scene = Scene::create();
    std::shared_ptr<SceneNode> root = scene->getRoot();
    std::shared_ptr<TextEdit> field = TextEdit::create("field", "Hello");
    Theme theme;
    StyleManager manager{theme};
    FakeRenderer renderer;

    TextEditFixture() {
        root->setStyle({.fontSize = 16, .fontPath = "fake.ttf"}, WidgetState::Normal);
        root->addChild(field);
        // The field measures and hit-tests through the renderer's engine, so the
        // renderer must be attached before the tree is measured.
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
    // "Hello" = 5 bytes -> 40px text; default padding is 4 left + 4 right.
    EXPECT_FLOAT_EQ(f.field->getGlobalBounds().width, 48);
    // Font line height 16 + 2 top + 2 bottom.
    EXPECT_FLOAT_EQ(f.field->getGlobalBounds().height, 20);
}

TEST(TextEditTest, SetTextAndGetText) {
    TextEditFixture f;
    EXPECT_EQ(f.field->getText(), "Hello");
    f.field->setText("World!");
    EXPECT_EQ(f.field->getText(), "World!");
    EXPECT_EQ(f.field->getEditor().getText(), "World!");
}

TEST(TextEditTest, MouseDownPlacesCaretAndFocuses) {
    TextEditFixture f;
    // Click the middle of "Hello" (padding 4px + 20px of text) -> caret 2.
    f.press(24, 10);
    f.release(24, 10);
    // While the button is held the widget reports Pressed; after the release
    // the Focused style/state (and the caret) take over.
    EXPECT_EQ(f.field->getCurrentState(), WidgetState::Focused);
    EXPECT_EQ(f.field->getEditor().getCaret(), 2u);
    EXPECT_FALSE(f.field->getEditor().hasSelection());
}

TEST(TextEditTest, KeyboardEditingWhileFocused) {
    TextEditFixture f;
    f.press(4, 10);  // focus + caret at 0
    f.release(4, 10);
    ASSERT_EQ(f.field->getCurrentState(), WidgetState::Focused);

    f.keyDown(KeyCode::Backspace);  // no-op at start
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

    // Undo/redo through the keyboard.
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
    f.press(4, 10);  // focus
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
    // Press at the start of the text, drag to the middle.
    f.press(4, 10);
    f.dragTo(24, 10);  // caret lands at 2 (20px of text, 8px/byte)
    const auto& editor = f.field->getEditor();
    EXPECT_TRUE(editor.hasSelection());
    EXPECT_EQ(editor.getSelectionStart(), 0u);
    EXPECT_EQ(editor.getSelectionEnd(), 2u);
}

TEST(TextEditTest, BackspaceDeletesWholeUtf8CodePoint) {
    TextEditFixture f;
    f.field->setText("\xD0\x90\xD0\xB1");  // "Аб"
    f.root->measure({800, 600});
    f.root->arrange({0, 0, 800, 600});
    f.press(4, 10);  // focus, caret at 0
    f.keyDown(KeyCode::End);
    f.keyDown(KeyCode::Backspace);
    EXPECT_EQ(f.field->getText(), "\xD0\x90");
}
