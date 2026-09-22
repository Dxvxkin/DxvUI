#ifndef DXVUI_TESTS_FAKEBACKEND_H
#define DXVUI_TESTS_FAKEBACKEND_H

// Shared no-op fakes for tests that exercise scene/layout/widget code without a
// real SDL backend. Stage 7 removed the legacy IRenderer god-interface: the
// draw pass consumes ICanvas, the Scene needs IRenderBackend (frame lifecycle +
// viewport + text engine) and IPlatformServices (cursor/clipboard). Tests that
// used to roll their own FakeRenderer now derive from FakeBackend; recording
// subclasses add the counters the test asserts on.

#include <memory>
#include <span>
#include <string>
#include <string_view>

#include "DxvUI/interfaces/ICanvas.h"
#include "DxvUI/interfaces/IClipboard.h"
#include "DxvUI/interfaces/IPlatformServices.h"
#include "DxvUI/interfaces/IRenderBackend.h"
#include "DxvUI/interfaces/ITextEngine.h"

namespace DxvUI {

class FakeTextEngine : public ITextEngine {
   public:
    std::shared_ptr<IFont> getFont(const std::string&, int) override { return nullptr; }
    std::shared_ptr<IFont> getFontForFamily(const std::string&, int) override { return nullptr; }
    void registerFontFamily(const std::string&, const std::string&) override {}
    TextMetrics measure(const IFont&, const std::string&) override { return {0, 0}; }
    int measurePrefix(const IFont&, const std::string&, size_t) override { return 0; }
    size_t charIndexAtX(const IFont&, const std::string&, int) override { return 0; }
    LineMetrics lineMetrics(const IFont&) override { return {0, 0, 0}; }
    std::shared_ptr<ITexture> rasterize(const IFont&, const std::string&, const Color&) override {
        return nullptr;
    }
    size_t getTextureCacheCount() const override { return 0; }
    TextLayout layoutText(const IFont&, std::string_view) override { return {}; }
    void drawLayout(ICanvas&, const TextLayout&, const RectF&, const TextPaint&) override {}
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

// A no-op backend implementing the three split contracts a Scene needs
// (IRenderBackend + ICanvas + IPlatformServices). WidgetTests/ImageTests derive
// recording subclasses from it.
class FakeBackend : public IRenderBackend, public ICanvas, public IPlatformServices {
   public:
    FakeTextEngine textEngine;
    FakeClipboard clipboard;

    // --- IRenderBackend ---
    void clear(const Color&) override {}
    void present() override {}
    ICanvas& beginFrame(const Color&) override { return *this; }
    void endFrame() override {}
    Size getViewportSize() const override { return {800, 600}; }
    float getDpiScale() const override { return 1.0f; }
    ITextEngine& getTextEngine() override { return textEngine; }
    std::shared_ptr<ITexture> createTexture(const ImageData&) override { return nullptr; }
    std::shared_ptr<ITexture> createRenderTarget(int, int) override { return nullptr; }
    void beginRenderTarget(const std::shared_ptr<ITexture>&) override {}
    void endRenderTarget() override {}

    // --- ICanvas ---
    void pushClip(const RectF&) override {}
    void popClip() override {}
    void drawTexture(const std::shared_ptr<ITexture>&, const RectF&) override {}
    void drawTexture(const std::shared_ptr<ITexture>&, const ICanvas::TextureDraw&) override {}
    void fillRect(const RectF&, const Fill&) override {}
    void strokeRect(const RectF&, const Stroke&) override {}
    void fillRoundRect(const RectF&, float, const Brush&) override {}
    void fillCircle(const PointF&, float, const Brush&) override {}
    void strokeArc(const PointF&, float, float, float, const Stroke&) override {}
    void fillPolygon(std::span<const PointF>, const Fill&) override {}
    void drawLine(const PointF&, const PointF&, const Stroke&) override {}

    // --- IPlatformServices ---
    void setCursor(CursorType) override {}
    CursorType getCursor() const override { return CursorType::Arrow; }
    IClipboard& getClipboard() override { return clipboard; }
};

}  // namespace DxvUI

#endif  // DXVUI_TESTS_FAKEBACKEND_H