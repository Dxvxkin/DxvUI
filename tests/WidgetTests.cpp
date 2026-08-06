#include <gtest/gtest.h>

#include <memory>

#include "DxvUI/Log.h"
#include "DxvUI/Scene.h"
#include "DxvUI/SceneNode.h"
#include "DxvUI/UIBinding.h"
#include "DxvUI/containers/AbsoluteContainer.h"
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
