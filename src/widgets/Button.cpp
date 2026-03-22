#include "DxvUI/widgets/Button.h"
#include "DxvUI/widgets/Label.h"
#include "DxvUI/LayoutProperties.h"
#include "DxvUI/Colors.h"
#include <utility>
#include <limits>

namespace DxvUI {

    std::shared_ptr<Button> Button::create(std::string id, std::string text) {
        auto btn = std::shared_ptr<Button>(new Button(std::move(id)));
        btn->initialText = std::move(text);
        return btn;
    }

    Button::Button(std::string id) : SceneNode(std::move(id)) {
        // Default styles for a button
        getAppearance().backgroundColor = {220, 220, 220, 255}; // Light gray
        getAppearance().textColor = {0, 0, 0, 255}; // Black
        getLayout().padding = {5, 10, 5, 10}; // Add some default padding
    }

    void Button::onAttach() {
        SceneNode::onAttach();
        if (!label) {
            label = Label::create("", initialText);
            label->getAppearance().backgroundColor = Colors::Transparent;
            label->getAppearance().borderThickness = 0;
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

        // First, measure the label to know its content size
        if (label) {
            label->measure({std::numeric_limits<float>::infinity(), std::numeric_limits<float>::infinity()});
        }

        // Then, calculate the button's desired size based on the label and padding
        const auto& padding = getLayout().padding;
        Size labelSize = label ? label->getDesiredSize() : Size{0, 0};

        desiredSize = {
            labelSize.width + padding.left + padding.right,
            labelSize.height + padding.top + padding.bottom
        };

        // If the user has set a specific size for the button, it overrides the content-based size.
        if (getLayout().width.has_value()) {
            desiredSize.width = getLayout().width.value();
        }
        if (getLayout().height.has_value()) {
            desiredSize.height = getLayout().height.value();
        }

        return desiredSize;
    }

    void Button::arrange(const Rect& finalRect) {
        // Set our own bounds first
        layoutProperties.computedBounds = finalRect;

        // Then, manually arrange the label to be centered within the button's content area
        if (label) {
            const auto& padding = getLayout().padding;

            Rect contentRect = {
                finalRect.x + static_cast<int>(padding.left),
                finalRect.y + static_cast<int>(padding.top),
                finalRect.width - static_cast<int>(padding.left + padding.right),
                finalRect.height - static_cast<int>(padding.top + padding.bottom)
            };

            Size labelDesiredSize = label->getDesiredSize();

            // Center the label within the contentRect
            int labelX = contentRect.x + (contentRect.width - static_cast<int>(labelDesiredSize.width)) / 2;
            int labelY = contentRect.y + (contentRect.height - static_cast<int>(labelDesiredSize.height)) / 2;

            Rect labelFinalRect = {labelX, labelY, static_cast<int>(labelDesiredSize.width), static_cast<int>(labelDesiredSize.height)};
            label->arrange(labelFinalRect);
        }

        // Mark layout as no longer dirty
        isLayoutDirty = false;
    }

    void Button::draw(IRenderer& renderer) {
        const auto& computed = getComputedAppearance();

        Color bgColor = computed.backgroundColor;
        if (isPressed) {
            bgColor = bgColor.darken(0.2f);
        } else if (isHovered) {
            bgColor = bgColor.lighten(0.2f);
        }

        renderer.fillRoundRect(getGlobalBounds(), computed.borderRadius, bgColor, {computed.borderColor, computed.borderThickness});

        // The base draw call will now correctly draw the label in its centered position
        SceneNode::draw(renderer);
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
