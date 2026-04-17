#include "DxvUI/widgets/Button.h"
#include "DxvUI/widgets/Label.h"
#include "DxvUI/containers/CenterContainer.h"

#include "DxvUI/style/Theme.h"
#include "DxvUI/style/Colors.h"
#include <utility>

namespace DxvUI {

    // --- Self-registration of default styles ---
    namespace {
        struct ButtonStyleRegistrar {
            ButtonStyleRegistrar() {
                Theme::registerDefaultStyle("Button", {
                    {
                        WidgetState::Normal,
                        {
                            .backgroundColor = Colors::CornflowerBlue,
                            .textColor = Colors::White,
                            .borderRadius = 5,
                            .cursor = CursorType::Hand,
                            .padding = {{10, 20, 10, 20}}
                        }
                    },
                    {
                        WidgetState::Hovered,
                        {
                            .backgroundColor = Colors::RoyalBlue,
                        }
                    },
                    {
                        WidgetState::Pressed,
                        {
                            .backgroundColor = Colors::MidnightBlue,
                        }
                    }
                });
            }
        };
        const ButtonStyleRegistrar registrar;
    }

    std::shared_ptr<Button> Button::create(std::string id, std::string text) {
        return std::shared_ptr<Button>(new Button(std::move(id), std::move(text)));
    }

    Button::Button(std::string id, std::string text) : SceneNode(std::move(id)) {
        auto binding = UIBinding::create(std::move(text));
        bind(binding);
    }

    const char* Button::getNodeType() const {
        return "Button";
    }

    void Button::onAttach() {
        SceneNode::onAttach();
        if (!label) {
            label = Label::create(id + "_label", getBinding()->getString().value_or(""));
            label->bind(binding_);

            auto centerContainer = std::make_shared<CenterContainer>(id + "_center");
            centerContainer->addChild(label);

            addChild(centerContainer);
        }
    }

    Size Button::measure(const Size& availableSize) {
        if (!isLayoutDirty) return desiredSize;

        if (!children.empty()) {
            desiredSize = children.front()->measure(availableSize);
        } else {
            desiredSize = {0, 0};
        }

        return desiredSize;
    }

    void Button::arrange(const Rect& finalRect) {
        auto& computedLayout = layoutCache[getCurrentState()];
        computedLayout.computedBounds = finalRect;

        if (!children.empty()) {
            children.front()->arrange(finalRect);
        }

        isLayoutDirty = false;
    }

    void Button::draw(IRenderer& renderer) {
        const auto& computedAppearance = getComputedAppearance(getCurrentState());
        const auto& computedLayout = getComputedLayout(getCurrentState());

        // 1. Draw the button's background
        renderer.fillRoundRect(
            computedLayout.computedBounds,
            computedAppearance.borderRadius,
            computedAppearance.backgroundColor,
            {computedAppearance.borderColor, computedAppearance.borderThickness}
        );

        // 2. Let the base class handle drawing children (the container will draw the label)
        SceneNode::draw(renderer);
    }

    std::shared_ptr<SceneNode> Button::findNodeAt(int x, int y) {
        if (getGlobalBounds().contains(x, y)) {
            return shared_from_this();
        }
        return nullptr;
    }

    void Button::setText(const std::string& text) {
        getBinding()->set(std::move(text));
        markLayoutDirty();
    }

    const std::string Button::getText() const {
        return getBinding()->getString().value_or("");
    }

    std::shared_ptr<Label> Button::getLabel() const {
        return label;
    }

}
