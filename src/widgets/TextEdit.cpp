#include "DxvUI/widgets/TextEdit.h"

#include <utility>

#include "DxvUI/Scene.h"
#include "DxvUI/interfaces/IClipboard.h"
#include "DxvUI/interfaces/IPlatformServices.h"
#include "DxvUI/interfaces/ITextEngine.h"
#include "DxvUI/layout/LayoutManager.h"
#include "DxvUI/style/Colors.h"
#include "DxvUI/style/Theme.h"
#include "DxvUI/text/DefaultTextEditorView.h"

namespace DxvUI {

namespace {
constexpr const char* kWidgetType = "TextEdit";

struct TextEditStyleRegistrar {
    TextEditStyleRegistrar() {
        Theme::registerDefaultStyle(
            kWidgetType,
            {{WidgetState::Normal,
              {.backgroundColor = Colors::White,
               .textColor = Colors::Black,
               .borderColor = Colors::Gray,
               .borderThickness = 1,
               .cursor = CursorType::IBeam,
               .clipContent = true,
               .textAlign = Alignment::Start,
               .padding = {{2, 4, 2, 4}}}},
             {WidgetState::Focused, {.borderColor = Colors::CornflowerBlue, .borderThickness = 2}},
             {WidgetState::Disabled,
              {
                  .textColor = Colors::Gray,
                  .borderColor = Colors::LightGray,
                  .cursor = CursorType::Arrow,
              }}});
    }
};

const TextEditStyleRegistrar registrar;
}  // namespace

std::shared_ptr<TextEdit> TextEdit::create(std::string id, std::string text) {
    return std::make_shared<TextEdit>(std::move(id), std::move(text));
}

TextEdit::TextEdit(std::string id, std::string text)
    : SceneNode(std::move(id)), editor_(std::move(text)) {
    view_ = std::make_unique<DefaultTextEditorView>();
    bind(UIBinding::create(editor_.getText()));
    editor_.setChangeCallback([this] { getBinding()->set(editor_.getText()); });
}

void TextEdit::onChange(const UIBinding& /*binding*/) { markLayoutDirty(); }

void TextEdit::onEvent(DxvEvent& event) {
    switch (event.type) {
        case EventType::KeyDown:
            handleKeyDown(event);
            break;
        case EventType::TextInput:
            handleTextInput(event);
            break;
        case EventType::MouseDown:
            handleMouseDown(event);
            break;
        case EventType::Drag:
            handleMouseDrag(event);
            break;
        case EventType::FocusLost:
            editor_.clearSelection();
            break;
        default:
            break;
    }
}

const char* TextEdit::getNodeType() const noexcept { return kWidgetType; }

std::string TextEdit::getText() const { return editor_.getText(); }

void TextEdit::setText(std::string text) { editor_.setText(std::move(text)); }

Size TextEdit::onMeasure(const Size& /*availableSize*/) {
    const Thickness insets = LayoutManager::contentInsets(*this);

    ITextEngine* engine = nullptr;
    const IFont* font = nullptr;
    if (!getEditContext(&engine, &font)) {
        return LayoutManager::addPadding({0, 0}, insets);
    }

    // Stage 3: measure via TextLayout (glyph atlas)
    TextLayout layout = engine->layoutText(*font, editor_.getText());
    const int width = layout.metrics.width;
    const LineMetrics line = engine->lineMetrics(*font);
    const int height = line.lineHeight > 0
                           ? line.lineHeight
                           : (layout.metrics.height > 0 ? layout.metrics.height : 0);
    return LayoutManager::addPadding({static_cast<float>(width), static_cast<float>(height)},
                                     insets);
}

void TextEdit::onPaint(PaintContext& pc) {
    const auto& appearance = getComputedAppearance();
    auto font = pc.text().getFontForFamily(appearance.fontFamily, appearance.fontSize);
    if (!font) {
        return;
    }

    const Rect contentRect = LayoutManager::contentRect(*this, getGlobalBounds());
    TextEditorView::Options options;
    options.textColor = appearance.textColor;
    options.horizontalAlign = appearance.textAlign;
    const bool focused = getCurrentState() == WidgetState::Focused;
    options.showCaret = focused;
    if (editor_.empty() && !focused) {
        options.placeholder = placeholder_;
        options.placeholderColor = Colors::Gray;
    }
    view_->draw(pc, *font, editor_, contentRect, options);
}

bool TextEdit::getEditContext(ITextEngine** engine, const IFont** font) {
    *engine = nullptr;
    *font = nullptr;
    auto scene = getScene();
    if (!scene || !scene->getTextEngine()) {
        return false;
    }
    auto& textEngine = *scene->getTextEngine();
    const auto& appearance = getComputedAppearance();
    auto fontHandle = textEngine.getFontForFamily(appearance.fontFamily, appearance.fontSize);
    if (!fontHandle) {
        return false;
    }
    *engine = &textEngine;
    *font = fontHandle.get();
    return true;
}

IClipboard* TextEdit::getClipboard() {
    auto scene = getScene();
    if (!scene) {
        return nullptr;
    }
    if (auto* ps = scene->getPlatformServices()) {
        return &ps->getClipboard();
    }
    return nullptr;
}

void TextEdit::handleKeyDown(DxvEvent& event) {
    const bool ctrl = (event.key.mod & KeyModifier::Ctrl) != 0;
    const bool shift = (event.key.mod & KeyModifier::Shift) != 0;

    switch (event.key.sym) {
        case KeyCode::Left:
            moveCaretBy(-1, shift);
            break;
        case KeyCode::Right:
            moveCaretBy(+1, shift);
            break;
        case KeyCode::Home:
            moveCaretToBoundary(0, shift);
            break;
        case KeyCode::End:
            moveCaretToBoundary(editor_.length(), shift);
            break;
        case KeyCode::Backspace:
            editor_.backspace();
            break;
        case KeyCode::Delete:
            editor_.deleteForward();
            break;
        case KeyCode::Enter:
            if (onSubmit_) {
                onSubmit_(editor_.getText());
            }
            break;
        case KeyCode::A:
            if (ctrl) {
                editor_.selectAll();
            }
            break;
        case KeyCode::Z:
            if (ctrl) {
                if (shift) {
                    editor_.redo();
                } else {
                    editor_.undo();
                }
            }
            break;
        case KeyCode::Y:
            if (ctrl) {
                editor_.redo();
            }
            break;
        case KeyCode::C:
            if (ctrl && editor_.hasSelection()) {
                if (auto* clipboard = getClipboard()) {
                    clipboard->setText(editor_.selectedText());
                }
            }
            break;
        case KeyCode::X:
            if (ctrl && editor_.hasSelection()) {
                if (auto* clipboard = getClipboard()) {
                    clipboard->setText(editor_.selectedText());
                }
                editor_.deleteSelection();
            }
            break;
        case KeyCode::V:
            if (ctrl) {
                if (auto* clipboard = getClipboard()) {
                    const std::string text = clipboard->getText();
                    if (!text.empty()) {
                        editor_.insertText(text);
                    }
                }
            }
            break;
        default:
            break;
    }
}

void TextEdit::handleTextInput(DxvEvent& event) {
    if (event.text.empty()) {
        return;
    }
    editor_.insertText(event.text);
}

void TextEdit::handleMouseDown(DxvEvent& event) {
    ITextEngine* engine = nullptr;
    const IFont* font = nullptr;
    if (!getEditContext(&engine, &font)) {
        return;
    }
    const Rect contentRect = LayoutManager::contentRect(*this, getGlobalBounds());
    const size_t index = view_->hitTestAt(*engine, *font, editor_, contentRect, event.mouse.x,
                                          getComputedAppearance().textAlign);
    selectionAnchor_ = index;
    editor_.setCaret(index);
    editor_.clearSelection();
}

void TextEdit::handleMouseDrag(DxvEvent& event) {
    ITextEngine* engine = nullptr;
    const IFont* font = nullptr;
    if (!getEditContext(&engine, &font)) {
        return;
    }
    const Rect contentRect = LayoutManager::contentRect(*this, getGlobalBounds());
    const size_t index = view_->hitTestAt(*engine, *font, editor_, contentRect, event.mouse.x,
                                          getComputedAppearance().textAlign);
    editor_.setCaret(index);
    editor_.setSelection(selectionAnchor_, index);
}

void TextEdit::moveCaretBy(int delta, bool extend) {
    if (extend && !editor_.hasSelection()) {
        selectionAnchor_ = editor_.getCaret();
    }
    if (delta < 0) {
        editor_.moveCaretLeft();
    } else {
        editor_.moveCaretRight();
    }
    if (extend) {
        editor_.setSelection(selectionAnchor_, editor_.getCaret());
    } else {
        editor_.clearSelection();
    }
}

void TextEdit::moveCaretToBoundary(size_t boundary, bool extend) {
    if (extend && !editor_.hasSelection()) {
        selectionAnchor_ = editor_.getCaret();
    }
    editor_.setCaret(boundary);
    if (extend) {
        editor_.setSelection(selectionAnchor_, boundary);
    } else {
        editor_.clearSelection();
    }
}

}  // namespace DxvUI
