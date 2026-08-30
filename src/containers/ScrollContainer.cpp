#include "DxvUI/containers/ScrollContainer.h"

#include <algorithm>

#include "DxvUI/DxvEvent.h"
#include "DxvUI/layout/LayoutManager.h"
#include "DxvUI/style/Theme.h"

namespace DxvUI {

namespace {
// Единый источник имени типа: используется и в getNodeType(), и как ключ
// регистрации стилей, чтобы строка не могла разойтись с типом виджета.
constexpr const char* kWidgetType = "ScrollContainer";

struct ScrollContainerStyleRegistrar {
    ScrollContainerStyleRegistrar() {
        // Включённый clipContent по умолчанию: переполняющийся контент
        // обрезается по границам viewport, а не рисуется поверх соседей.
        Theme::registerDefaultStyle(kWidgetType, {{WidgetState::Normal, {.clipContent = true}}});
    }
};

const ScrollContainerStyleRegistrar registrar;
}  // namespace

std::shared_ptr<ScrollContainer> ScrollContainer::create(std::string id) {
    return std::make_shared<ScrollContainer>(std::move(id));
}

ScrollContainer::ScrollContainer(std::string id) : Container(std::move(id)) {
    // The container's empty box (and padding area) should be click/hover-aware so
    // the wheel bubbles up even when the cursor is over empty viewport space.
    setHitTestable(true);
}

const char* ScrollContainer::getNodeType() const noexcept { return kWidgetType; }

void ScrollContainer::setScrollX(float x) {
    x = clampScrollX(x);
    if (scrollX_ != x) {
        scrollX_ = x;
        markLayoutDirty();
    }
}

void ScrollContainer::setScrollY(float y) {
    y = clampScrollY(y);
    if (scrollY_ != y) {
        scrollY_ = y;
        markLayoutDirty();
    }
}

float ScrollContainer::getScrollX() const { return scrollX_; }
float ScrollContainer::getScrollY() const { return scrollY_; }

void ScrollContainer::scrollBy(float dx, float dy) {
    setScrollX(scrollX_ + dx);
    setScrollY(scrollY_ + dy);
}

float ScrollContainer::clampScrollX(float x) const {
    return std::max(0.0f,
                    std::min(x, std::max(0.0f, contentSize_.width - getGlobalBounds().width)));
}

float ScrollContainer::clampScrollY(float y) const {
    return std::max(0.0f,
                    std::min(y, std::max(0.0f, contentSize_.height - getGlobalBounds().height)));
}

Size ScrollContainer::onMeasure(const Size& availableSize) {
    const auto& computedLayout = getComputedLayout();
    const auto& padding = computedLayout.padding;

    const Size viewport = LayoutManager::subtractPadding(availableSize, padding);

    // Measure the single child with the viewport constraints to learn its full
    // content extent; it overflows the viewport and is scrolled inside it.
    Size childOuterSize{0.0f, 0.0f};
    if (!getChildren().empty() && getChildren().front()) {
        auto& child = getChildren().front();
        const auto& margin = child->getComputedLayout(child->getCurrentState()).margin;
        if (!child->isLayoutDirty() && child->getLastMeasureConstraints() ==
                                           LayoutManager::subtractPadding(viewport, margin)) {
            childOuterSize = LayoutManager::addPadding(child->getDesiredSize(), margin);
        } else {
            childOuterSize = LayoutManager::measureChild(*child, viewport);
        }
    }
    contentSize_ = childOuterSize;

    // The viewport keeps the size the parent offered (clamped, so an unbounded
    // slot collapses to the content size and nothing scrolls). The explicit
    // width/height style is applied on top by LayoutManager afterwards.
    const float w = std::min(childOuterSize.width, viewport.width);
    const float h = std::min(childOuterSize.height, viewport.height);
    return LayoutManager::addPadding({w, h}, padding);
}

void ScrollContainer::onArrange(const Rect& finalRect) {
    const auto& computedLayout = getComputedLayout();
    const auto& padding = computedLayout.padding;
    const Rect content = LayoutManager::contentRect(*this, finalRect);

    // Re-clamp against the actual arranged viewport: the offset must never push
    // the content beyond the bottom/right of the viewport. Mutating the member
    // here would re-mark the subtree dirty mid-arrange, so clamp to locals.
    const int offsetX = static_cast<int>(clampScrollX(scrollX_));
    const int offsetY = static_cast<int>(clampScrollY(scrollY_));

    if (!getChildren().empty() && getChildren().front()) {
        auto& child = getChildren().front();
        if (!child->isVisible()) {
            LayoutManager::arrangeInvisible(*child, content);
            return;
        }

        const auto& childLayout = child->getComputedLayout(child->getCurrentState());
        const auto& margin = childLayout.margin;
        Size childDesiredSize = child->getDesiredSize();

        // Offset the child by the scroll position so it slides inside the
        // clipped viewport. The clipContent style (set by default) cuts off the
        // part that slides out of bounds.
        const Rect childFinalRect = {content.x - offsetX + static_cast<int>(margin.left),
                                     content.y - offsetY + static_cast<int>(margin.top),
                                     static_cast<int>(childDesiredSize.width),
                                     static_cast<int>(childDesiredSize.height)};

        child->arrange(childFinalRect);
    }
}

void ScrollContainer::onEvent(DxvEvent& event) {
    if (!isEnabled()) {
        return;
    }
    switch (event.type) {
        case EventType::MouseWheel: {
            // A positive dy means scrolled up (away from the user): show content
            // further up the list, i.e. decrease the offset.
            scrollBy(0.0f, -event.mouse.dy);
            event.stopPropagation();
            break;
        }
        default:
            break;
    }
}

}  // namespace DxvUI
