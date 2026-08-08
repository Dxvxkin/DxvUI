#include "DxvUI/widgets/TextEdit.h"

#include <utility>

#include "DxvUI/Log.h"
#include "DxvUI/Scene.h"
#include "DxvUI/interfaces/IClipboard.h"
#include "DxvUI/layout/LayoutManager.h"
#include "DxvUI/renderers/SDLTextEditorView.h"
#include "DxvUI/style/Colors.h"
#include "DxvUI/style/Theme.h"
#include "DxvUI/text/ITextEngine.h"

namespace DxvUI {

// --- Self-registration of default styles ---
namespace {
struct TextEditStyleRegistrar {
    TextEditStyleRegistrar() {
        Theme::registerDefaultStyle(
            "TextEdit", {{WidgetState::Normal,
                          {.backgroundColor = Colors::White,
                           .textColor = Colors::Black,
                           .borderColor = Colors::Gray,
                           .borderThickness = 1,
                           .cursor = CursorType::IBeam,
                           // Overflowing text is clipped to the
                           // field instead of drawing over siblings.
                           .clipContent = true,
                           .padding = {{2, 4, 2, 4}}}},
                         {WidgetState::Focused,
                          {.borderColor = Colors::CornflowerBlue, .borderThickness = 2}}});
    }
};

const TextEditStyleRegistrar registrar;
}  // namespace

std::shared_ptr<TextEdit> TextEdit::create(std::string id, std::string text) {
    return std::make_shared<TextEdit>(std::move(id), std::move(text));
}

TextEdit::TextEdit(std::string id, std::string text)
    : SceneNode(std::move(id)), editor_(std::move(text)) {
    view_ = std::make_unique<SDLTextEditorView>();

    // Keyboard events reach the widget only while it owns focus; mouse events
    // are routed to it as the deepest node under the cursor.
    on(EventType::KeyDown, [this](DxvEvent& e, const UIContext&) { handleKeyDown(e); });
    on(EventType::TextInput, [this](DxvEvent& e, const UIContext&) { handleTextInput(e); });
    on(EventType::MouseDown, [this](DxvEvent& e, const UIContext&) { handleMouseDown(e); });
    on(EventType::Drag, [this](DxvEvent& e, const UIContext&) { handleMouseDrag(e); });
    on(EventType::FocusLost, [this](DxvEvent&, const UIContext&) { editor_.clearSelection(); });
}

const char* TextEdit::getNodeType() const noexcept { return "TextEdit"; }

std::string TextEdit::getText() const { return editor_.getText(); }

void TextEdit::setText(std::string text) {
    editor_.setText(std::move(text));
    markLayoutDirty();
}

Size TextEdit::onMeasure(const Size& availableSize) {
    const auto& padding = getComputedLayout().padding;

    ITextEngine* engine = nullptr;
    const IFont* font = nullptr;
    if (!getEditContext(&engine, &font)) {
        return LayoutManager::addPadding({0, 0}, padding);
    }

    const TextMetrics measured = (*engine).measure(*font, editor_.getText());
    // An empty buffer measures zero height, but the field still needs the font's
    // line height to be clickable and to align the caret.
    const LineMetrics line = (*engine).lineMetrics(*font);
    const int height = line.lineHeight > 0 ? line.lineHeight : measured.height;
    return LayoutManager::addPadding(
        {static_cast<float>(measured.width), static_cast<float>(height)}, padding);
}

void TextEdit::drawContent(IRenderer& renderer) {
    ITextEngine* engine = nullptr;
    const IFont* font = nullptr;
    if (!getEditContext(&engine, &font)) {
        return;
    }

    const Rect contentRect = LayoutManager::contentRect(*this, getGlobalBounds());
    TextEditorView::Options options;
    options.textColor = getComputedAppearance().textColor;
    options.showCaret = getCurrentState() == WidgetState::Focused;
    view_->draw(renderer, *engine, *font, editor_, contentRect, options);
}

bool TextEdit::getEditContext(ITextEngine** engine, const IFont** font) {
    *engine = nullptr;
    *font = nullptr;
    auto scene = getScene();
    if (!scene || !scene->getRenderer()) {
        return false;
    }
    auto& textEngine = scene->getRenderer()->getTextEngine();
    const auto& appearance = getComputedAppearance();
    auto fontHandle = textEngine.getFont(appearance.fontPath, appearance.fontSize);
    if (!fontHandle) {
        return false;
    }
    *engine = &textEngine;
    *font = fontHandle.get();
    return true;
}

IClipboard* TextEdit::getClipboard() {
    auto scene = getScene();
    if (!scene || !scene->getRenderer()) {
        return nullptr;
    }
    return &scene->getRenderer()->getClipboard();
}

void TextEdit::handleKeyDown(DxvEvent& event) {
    const bool ctrl = (event.key.mod & KeyModifier::Ctrl) != 0;
    const bool shift = (event.key.mod & KeyModifier::Shift) != 0;

    switch (event.key.sym) {
        case KeyCode::Left:
            moveCaretBy(-1, shift);
            event.handled = true;
            break;
        case KeyCode::Right:
            moveCaretBy(+1, shift);
            event.handled = true;
            break;
        case KeyCode::Home:
            moveCaretToBoundary(0, shift);
            event.handled = true;
            break;
        case KeyCode::End:
            moveCaretToBoundary(editor_.length(), shift);
            event.handled = true;
            break;
        case KeyCode::Backspace:
            editor_.backspace();
            markLayoutDirty();
            event.handled = true;
            break;
        case KeyCode::Delete:
            editor_.deleteForward();
            markLayoutDirty();
            event.handled = true;
            break;
        case KeyCode::Enter:
            if (onSubmit_) {
                onSubmit_(editor_.getText());
            }
            event.handled = true;
            break;
        case KeyCode::A:
            if (ctrl) {
                editor_.selectAll();
                event.handled = true;
            }
            break;
        case KeyCode::Z:
            if (ctrl) {
                if (shift) {
                    editor_.redo();
                } else {
                    editor_.undo();
                }
                markLayoutDirty();
                event.handled = true;
            }
            break;
        case KeyCode::Y:
            if (ctrl) {
                editor_.redo();
                markLayoutDirty();
                event.handled = true;
            }
            break;
        case KeyCode::C:
            if (ctrl && editor_.hasSelection()) {
                if (auto* clipboard = getClipboard()) {
                    clipboard->setText(editor_.selectedText());
                }
                event.handled = true;
            }
            break;
        case KeyCode::X:
            if (ctrl && editor_.hasSelection()) {
                if (auto* clipboard = getClipboard()) {
                    clipboard->setText(editor_.selectedText());
                }
                editor_.deleteSelection();
                markLayoutDirty();
                event.handled = true;
            }
            break;
        case KeyCode::V:
            if (ctrl) {
                if (auto* clipboard = getClipboard()) {
                    const std::string text = clipboard->getText();
                    if (!text.empty()) {
                        editor_.insertText(text);
                        markLayoutDirty();
                    }
                }
                event.handled = true;
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
    markLayoutDirty();
    event.handled = true;
}

void TextEdit::handleMouseDown(DxvEvent& event) {
    ITextEngine* engine = nullptr;
    const IFont* font = nullptr;
    if (!getEditContext(&engine, &font)) {
        return;
    }
    const Rect contentRect = LayoutManager::contentRect(*this, getGlobalBounds());
    const size_t index = view_->hitTestAt(*engine, *font, editor_, contentRect, event.mouse.x);
    selectionAnchor_ = index;
    editor_.setCaret(index);
    editor_.clearSelection();
    // Mouse events intentionally keep bubbling (e.g. for ancestor-level
    // click-outside handling); the editor state is set above regardless.
}

void TextEdit::handleMouseDrag(DxvEvent& event) {
    ITextEngine* engine = nullptr;
    const IFont* font = nullptr;
    if (!getEditContext(&engine, &font)) {
        return;
    }
    const Rect contentRect = LayoutManager::contentRect(*this, getGlobalBounds());
    const size_t index = view_->hitTestAt(*engine, *font, editor_, contentRect, event.mouse.x);
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
