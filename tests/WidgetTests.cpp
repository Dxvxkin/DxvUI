#include <gtest/gtest.h>

#include <memory>

#include "DxvUI/Log.h"
#include "DxvUI/Scene.h"
#include "DxvUI/SceneNode.h"
#include "DxvUI/style/StyleManager.h"
#include "DxvUI/style/Theme.h"
#include "DxvUI/widgets/Button.h"

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
        button->editStyle().set({.left = 0, .top = 0, .width = 100, .height = 50},
                                WidgetState::Normal);
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
