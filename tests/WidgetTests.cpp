#include <gtest/gtest.h>

#include <memory>
#include <string>
#include <vector>

#include "DxvUI/Log.h"
#include "DxvUI/Scene.h"
#include "DxvUI/SceneNode.h"
#include "DxvUI/UIBinding.h"
#include "DxvUI/containers/AbsoluteContainer.h"
#include "DxvUI/interfaces/IRenderer.h"
#include "DxvUI/style/StyleManager.h"
#include "DxvUI/style/Theme.h"
#include "DxvUI/widgets/Button.h"
#include "DxvUI/widgets/Label.h"

using namespace DxvUI;

namespace {

// SceneNode's destructor logs via DxvUI::Log, which requires an initialized
// logger. Install a global test environment so the logger exists for the whole
// test binary.
class LoggerEnvironment : public ::testing::Environment {
   public:
    void SetUp() override { Log::init(); }
};

::testing::Environment* const g_logger_environment =
    ::testing::AddGlobalTestEnvironment(new LoggerEnvironment);

}  // namespace

// Button layout and hit-testing can be exercised without a renderer: styles are
// resolved with a local StyleManager and the tree is measured/arranged manually,
// exactly like the StyleManager tests do.
namespace {

struct ButtonFixture {
    std::shared_ptr<Scene> scene = Scene::create();
    std::shared_ptr<SceneNode> root = scene->getRoot();
    std::shared_ptr<Button> button = Button::create("btn", "press");
    Theme theme;
    StyleManager manager{theme};

    ButtonFixture() {
        button->setStyle({.left = 0, .top = 0, .width = 100, .height = 50}, WidgetState::Normal);
        root->addChild(button);
        manager.resolveDirtyStyles(root);
        root->measure({800, 600});
        root->arrange({0, 0, 800, 600});
    }
};

}  // namespace

TEST(ButtonTest, InvisibleButtonIsNotHitTested) {
    ButtonFixture f;

    EXPECT_EQ(f.root->findNodeAt(50, 25).get(), f.button.get());

    f.button->setVisible(false);
    EXPECT_NE(f.root->findNodeAt(50, 25).get(), f.button.get());
}

TEST(ButtonTest, InvisibleButtonHasNoBounds) {
    ButtonFixture f;
    f.button->setVisible(false);

    f.root->measure({800, 600});
    f.root->arrange({0, 0, 800, 600});

    const Rect bounds = f.button->getGlobalBounds();
    EXPECT_EQ(bounds.width, 0);
    EXPECT_EQ(bounds.height, 0);
}

// A Button subclass that counts onMeasure invocations, so tests can observe
// whether a binding-driven text change forced a re-measure.
class CountingButton : public Button {
   public:
    explicit CountingButton(std::string id, std::string text)
        : Button(std::move(id), std::move(text)) {}

    static std::shared_ptr<CountingButton> create(std::string id, std::string text) {
        return std::shared_ptr<CountingButton>(new CountingButton(std::move(id), std::move(text)));
    }
    int measureCalls = 0;

   protected:
    Size onMeasure(const Size& availableSize) override {
        ++measureCalls;
        return Button::onMeasure(availableSize);
    }
};

// A Label subclass that counts onMeasure invocations, so tests can observe
// whether a binding-driven text change forced a re-measure.
class CountingLabel : public Label {
   public:
    using Label::Label;
    int measureCalls = 0;

   protected:
    Size onMeasure(const Size& availableSize) override {
        ++measureCalls;
        return Label::onMeasure(availableSize);
    }
};

// A stub text engine so FakeRenderer satisfies IRenderer. Text is never
// measured/rasterized in these tests (the Scene has no renderer), so all
// methods return inert values.
class FakeTextEngine : public ITextEngine {
   public:
    std::shared_ptr<IFont> getFont(const std::string&, int) override { return nullptr; }
    TextMetrics measure(const IFont&, const std::string&) override { return {0, 0}; }
    int measurePrefix(const IFont&, const std::string&, size_t) override { return 0; }
    size_t charIndexAtX(const IFont&, const std::string&, int) override { return 0; }
    LineMetrics lineMetrics(const IFont&) override { return {0, 0, 0}; }
    std::shared_ptr<ITexture> rasterize(const IFont&, const std::string&, const Color&) override {
        return nullptr;
    }
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

// A renderer stub that records clip operations instead of drawing, so the
// SceneNode draw template can be exercised without a real SDL backend.
class FakeRenderer : public IRenderer {
   public:
    std::vector<Rect> clipPushes;
    int clipPops = 0;
    FakeClipboard clipboard;

    void clear(const Color&) override {}
    void present() override {}
    Size getViewportSize() const override { return {800, 600}; }

    void setCursor(CursorType) override {}
    CursorType getCursor() const override { return CursorType::Arrow; }

    void pushClipRect(const Rect& rect) override { clipPushes.push_back(rect); }
    void popClipRect() override { clipPops++; }

    ITextEngine& getTextEngine() override { return textEngine; }
    IClipboard& getClipboard() override { return clipboard; }

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

    FakeTextEngine textEngine;
};

TEST(ButtonTest, SetTextRelayoutsViaBoundLabel) {
    auto scene = Scene::create();
    auto root = scene->getRoot();
    auto button = CountingButton::create("btn", "short");
    root->addChild(button);

    Theme theme;
    StyleManager manager{theme};
    manager.resolveDirtyStyles(root);
    root->measure({800, 600});
    root->arrange({0, 0, 800, 600});

    const int callsAfterInitialMeasure = button->measureCalls;
    ASSERT_GT(callsAfterInitialMeasure, 0);

    button->setText("a much longer text that needs a wider button");
    root->measure({800, 600});

    EXPECT_GT(button->measureCalls, callsAfterInitialMeasure);
}

TEST(LabelTest, BindingUpdateMarksLayoutDirty) {
    auto scene = Scene::create();
    auto root = scene->getRoot();
    auto binding = UIBinding::create(std::string("short"));
    auto label = std::make_shared<CountingLabel>("lbl", "short");
    label->bind(binding);
    root->addChild(label);

    Theme theme;
    StyleManager manager{theme};
    manager.resolveDirtyStyles(root);
    root->measure({800, 600});
    root->arrange({0, 0, 800, 600});

    const int callsAfterInitialMeasure = label->measureCalls;
    ASSERT_GT(callsAfterInitialMeasure, 0);

    binding->set(std::string("a much longer text that needs a wider label"));
    root->measure({800, 600});

    EXPECT_GT(label->measureCalls, callsAfterInitialMeasure);
}

TEST(LabelTest, SetTextUpdatesValue) {
    auto label = Label::create("lbl", "old");
    EXPECT_EQ(label->getText(), "old");

    label->setText("new");
    EXPECT_EQ(label->getText(), "new");

    label->setText("new");
    EXPECT_EQ(label->getText(), "new");
}

TEST(LabelTest, FontSizeChangeRelayouts) {
    auto root = std::make_shared<AbsoluteContainer>("root");
    auto label = std::make_shared<CountingLabel>("lbl", "hello");
    root->addChild(label);

    Theme theme;
    StyleManager manager{theme};
    manager.resolveDirtyStyles(root);
    root->measure({800, 600});
    root->arrange({0, 0, 800, 600});

    const int callsAfterInitialMeasure = label->measureCalls;
    ASSERT_GT(callsAfterInitialMeasure, 0);

    label->setStyle({.fontSize = 24}, WidgetState::Normal);
    manager.resolveDirtyStyles(root);
    root->measure({800, 600});

    EXPECT_GT(label->measureCalls, callsAfterInitialMeasure);
}

TEST(LabelTest, InheritedFontSizeChangeRelayouts) {
    auto root = std::make_shared<AbsoluteContainer>("root");
    auto label = std::make_shared<CountingLabel>("lbl", "hello");
    root->addChild(label);

    Theme theme;
    StyleManager manager{theme};
    manager.resolveDirtyStyles(root);
    root->measure({800, 600});
    root->arrange({0, 0, 800, 600});

    const int callsAfterInitialMeasure = label->measureCalls;
    ASSERT_GT(callsAfterInitialMeasure, 0);

    root->setStyle({.fontSize = 24}, WidgetState::Normal);
    manager.resolveDirtyStyles(root);
    root->measure({800, 600});

    EXPECT_GT(label->measureCalls, callsAfterInitialMeasure);
}

TEST(LabelTest, ThemeFontSizeChangeRelayouts) {
    auto root = std::make_shared<AbsoluteContainer>("root");
    auto label = std::make_shared<CountingLabel>("lbl", "hello");
    root->addChild(label);

    Theme theme;
    StyleManager manager{theme};
    manager.resolveDirtyStyles(root);
    root->measure({800, 600});
    root->arrange({0, 0, 800, 600});

    const int callsAfterInitialMeasure = label->measureCalls;
    ASSERT_GT(callsAfterInitialMeasure, 0);

    theme.setDefaultStyle("Label", {{WidgetState::Normal, {.fontSize = 30}}});
    manager.resolveDirtyStyles(root);
    root->measure({800, 600});

    EXPECT_GT(label->measureCalls, callsAfterInitialMeasure);
}

TEST(ClippingTest, DrawClipsContentAndChildrenToOwnBounds) {
    auto scene = Scene::create();
    auto root = scene->getRoot();
    auto parent = std::make_shared<AbsoluteContainer>("parent");
    auto child = std::make_shared<SceneNode>("child");
    child->setStyle({.left = 0, .top = 0, .width = 200, .height = 200}, WidgetState::Normal);
    parent->setStyle({.clipContent = true, .left = 10, .top = 20, .width = 100, .height = 50},
                     WidgetState::Normal);
    root->addChild(parent);
    parent->addChild(child);

    Theme theme;
    StyleManager manager{theme};
    manager.resolveDirtyStyles(root);
    root->measure({800, 600});
    root->arrange({0, 0, 800, 600});

    // The child overflows the parent's 100x50 box, but the clip push must match
    // the parent's own bounds so the overflow is hidden.
    ASSERT_EQ(child->getGlobalBounds(), (Rect{10, 20, 200, 200}));

    FakeRenderer renderer;
    root->draw(renderer);

    ASSERT_EQ(renderer.clipPushes.size(), 1);
    EXPECT_EQ(renderer.clipPushes[0], parent->getGlobalBounds());
    EXPECT_EQ(renderer.clipPushes[0], (Rect{10, 20, 100, 50}));
    EXPECT_EQ(renderer.clipPops, 1);
}

TEST(ClippingTest, DrawWithoutClipContentEmitsNoClip) {
    auto scene = Scene::create();
    auto root = scene->getRoot();
    auto parent = std::make_shared<AbsoluteContainer>("parent");
    auto child = std::make_shared<SceneNode>("child");
    child->setStyle({.left = 0, .top = 0, .width = 200, .height = 200}, WidgetState::Normal);
    parent->setStyle({.left = 10, .top = 20, .width = 100, .height = 50}, WidgetState::Normal);
    root->addChild(parent);
    parent->addChild(child);

    Theme theme;
    StyleManager manager{theme};
    manager.resolveDirtyStyles(root);
    root->measure({800, 600});
    root->arrange({0, 0, 800, 600});

    FakeRenderer renderer;
    root->draw(renderer);

    EXPECT_TRUE(renderer.clipPushes.empty());
    EXPECT_EQ(renderer.clipPops, 0);
}

TEST(ClippingTest, NestedClipsPushAndPopInOrder) {
    auto scene = Scene::create();
    auto root = scene->getRoot();
    auto parent = std::make_shared<AbsoluteContainer>("parent");
    auto child = std::make_shared<SceneNode>("child");
    child->setStyle({.clipContent = true, .left = 5, .top = 5, .width = 40, .height = 30},
                    WidgetState::Normal);
    parent->setStyle({.clipContent = true, .left = 10, .top = 20, .width = 100, .height = 50},
                     WidgetState::Normal);
    root->addChild(parent);
    parent->addChild(child);

    Theme theme;
    StyleManager manager{theme};
    manager.resolveDirtyStyles(root);
    root->measure({800, 600});
    root->arrange({0, 0, 800, 600});

    FakeRenderer renderer;
    root->draw(renderer);

    ASSERT_EQ(renderer.clipPushes.size(), 2);
    EXPECT_EQ(renderer.clipPushes[0], parent->getGlobalBounds());
    EXPECT_EQ(renderer.clipPushes[1], child->getGlobalBounds());
    EXPECT_EQ(renderer.clipPops, 2);
}

TEST(ClippingTest, NodeOutsideViewportIsCulled) {
    auto scene = Scene::create();
    auto root = scene->getRoot();
    // Entirely outside the 800x600 fake viewport: nothing must be drawn at all.
    auto parent = std::make_shared<AbsoluteContainer>("parent");
    parent->setStyle({.clipContent = true, .left = 900, .top = 700, .width = 100, .height = 50},
                     WidgetState::Normal);
    root->addChild(parent);

    Theme theme;
    StyleManager manager{theme};
    manager.resolveDirtyStyles(root);
    root->measure({800, 600});
    root->arrange({0, 0, 800, 600});

    FakeRenderer renderer;
    root->draw(renderer);

    EXPECT_TRUE(renderer.clipPushes.empty());
    EXPECT_EQ(renderer.clipPops, 0);
}

TEST(SceneNodeTest, FindNodeByIdFindsFirstMatchInSubtree) {
    auto scene = Scene::create();
    auto root = scene->getRoot();
    auto first = std::make_shared<SceneNode>("dup");
    auto second = std::make_shared<SceneNode>("dup");
    root->addChild(first);
    root->addChild(second);

    // Duplicate IDs are allowed; the first match in children order wins.
    EXPECT_EQ(root->findNodeById("dup"), first);
    // First match inside a nested subtree shadows later siblings.
    auto nested = std::make_shared<SceneNode>("nested");
    first->addChild(nested);
    EXPECT_EQ(first->findNodeById("nested"), nested);
    EXPECT_EQ(root->findNodeById("nested"), nested);
}

TEST(SceneNodeTest, FindNodeByIdReturnsNullWhenNotFound) {
    auto scene = Scene::create();
    auto root = scene->getRoot();
    root->addChild(std::make_shared<SceneNode>("present"));

    EXPECT_EQ(root->findNodeById("present")->getId(), "present");
    EXPECT_EQ(root->findNodeById("missing"), nullptr);
    EXPECT_EQ(scene->findNodeById("missing"), nullptr);
}

TEST(SceneNodeTest, FindNodeByIdWorksOnDetachedSubtree) {
    auto scene = Scene::create();
    auto root = scene->getRoot();
    auto parent = std::make_shared<SceneNode>("parent");
    parent->addChild(std::make_shared<SceneNode>("child"));
    root->addChild(parent);

    // Search from the tree finds the detached node no longer.
    parent->detach();
    EXPECT_EQ(root->findNodeById("child"), nullptr);
    // Searching the detached branch itself still resolves its descendants.
    EXPECT_EQ(parent->findNodeById("child")->getId(), "child");
}

TEST(SceneNodeTest, SceneFindNodeByIdDelegatesToRoot) {
    auto scene = Scene::create();
    auto root = scene->getRoot();
    auto button = Button::create("btn", "press");
    root->addChild(button);

    EXPECT_EQ(scene->findNodeById("btn"), button);
    EXPECT_EQ(scene->findNodeById("nope"), nullptr);
}
