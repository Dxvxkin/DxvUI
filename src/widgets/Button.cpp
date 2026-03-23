#include "DxvUI/widgets/Button.h"
#include "DxvUI/widgets/Label.h"
#include "DxvUI/Colors.h"
#include <utility>
#include <limits>
#include <iostream>

namespace DxvUI {

    std::shared_ptr<Button> Button::create(std::string id, std::string text) {
        auto btn = std::shared_ptr<Button>(new Button(std::move(id)));
        btn->initialText = std::move(text);
        return btn;
    }

    Button::Button(std::string id) : SceneNode(std::move(id)) {
        StyleRule defaultStyle;
        defaultStyle.backgroundColor = {220, 220, 220, 255};
        defaultStyle.textColor = {0, 0, 0, 255};
        defaultStyle.padding = {5, 10, 5, 10};
        defaultStyle.horizontalAlignment = Alignment::Center;
        defaultStyle.verticalAlignment = Alignment::Center;
        getStyle().set(WidgetState::Normal, defaultStyle);
    }

    void Button::onAttach() {
        SceneNode::onAttach();
        if (!label) {
            label = Label::create(id + "_label", initialText);

            StyleRule labelStyle;
            labelStyle.backgroundColor = Colors::Transparent;
            labelStyle.borderThickness = 0;
            label->getStyle().set(WidgetState::Normal, labelStyle);

            addChild(label);
        }
    }

    std::shared_ptr<SceneNode> Button::findNodeAt(int x, int y) {
        if (getGlobalBounds().contains(x, y)) {
            return shared_from_this();
        }
        return nullptr;
    }

    Size Button::measure(const Size& availableSize) {
        if (!isLayoutDirty) return desiredSize;

        if (label) {
            label->measure({std::numeric_limits<float>::infinity(), std::numeric_limits<float>::infinity()});
        }

        const auto& computedLayout = getComputedLayout(getCurrentState());
        const auto& padding = computedLayout.padding;
        Size labelSize = label ? label->getDesiredSize() : Size{0, 0};

        desiredSize = {
            labelSize.width + padding.left + padding.right,
            labelSize.height + padding.top + padding.bottom
        };

        if (computedLayout.width > 0) desiredSize.width = computedLayout.width;
        if (computedLayout.height > 0) desiredSize.height = computedLayout.height;

        return desiredSize;
    }

    void Button::arrange(const Rect& finalRect) {
        // The button itself takes up the finalRect
        auto& computedLayout = layoutCache[getCurrentState()];
        computedLayout.computedBounds = finalRect;

        // Arrange the label centered within the button's content area
        if (label) {
            const auto& padding = computedLayout.padding;

            Rect contentRect = {
                finalRect.x + static_cast<int>(padding.left),
                finalRect.y + static_cast<int>(padding.top),
                finalRect.width - static_cast<int>(padding.left + padding.right),
                finalRect.height - static_cast<int>(padding.top + padding.bottom)
            };

            Size labelDesiredSize = label->getDesiredSize();

            int labelX = contentRect.x + (contentRect.width - static_cast<int>(labelDesiredSize.width)) / 2;
            int labelY = contentRect.y + (contentRect.height - static_cast<int>(labelDesiredSize.height)) / 2;

            Rect labelFinalRect = {labelX, labelY, static_cast<int>(labelDesiredSize.width), static_cast<int>(labelDesiredSize.height)};
            label->arrange(labelFinalRect);
        }

        isLayoutDirty = false;
    }

    void Button::draw(IRenderer& renderer) {
        const auto& computedAppearance = getComputedAppearance(getCurrentState());
        const auto& computedLayout = getComputedLayout(getCurrentState());

        renderer.fillRoundRect(computedLayout.computedBounds, computedAppearance.borderRadius, computedAppearance.backgroundColor, {computedAppearance.borderColor, computedAppearance.borderThickness});

        // Explicitly call draw on children, do NOT call SceneNode::draw
        if (label) {
            label->draw(renderer);
        }
    }

    void Button::setText(const std::string& text) {
        if (label) {
            label->setText(text);
        } else {
            initialText = text;
        }
        markLayoutDirty();
    }

    const std::string& Button::getText() const {
        static const std::string empty;
        if (label) {
            return label->getText();
        }
        return initialText;
    }

    Label* Button::getLabel() const {
        return label.get();
    }

}
