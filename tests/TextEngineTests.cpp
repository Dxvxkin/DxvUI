#include <gtest/gtest.h>

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

   private:
    std::map<std::string, std::shared_ptr<IFont>> fonts;
    std::map<std::tuple<const IFont*, std::string, uint32_t>, std::shared_ptr<ITexture>> textures;
};

// Minimal IRenderer so Label::onMeasure and Label::drawContent can be exercised
// without an SDL backend or a real font file.
class FakeRenderer : public IRenderer {
   public:
    ITextEngine& getTextEngine() override { return engine; }

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

    FakeTextEngine engine;
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

TEST(LabelTextEngineTest, MissingFontMeasuresZero) {
    LabelFixture f;
    f.label->setStyle({.fontPath = ""}, WidgetState::Normal);
    f.manager.resolveDirtyStyles(f.root);
    f.root->measure({800, 600});
    f.root->arrange({0, 0, 800, 600});
    EXPECT_FLOAT_EQ(f.label->getGlobalBounds().width, 0);
    EXPECT_FLOAT_EQ(f.label->getGlobalBounds().height, 0);
}
