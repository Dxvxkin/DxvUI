#include "DxvUI/SceneNode.h"
#include "DxvUI/Scene.h"
#include <utility>

namespace DxvUI {

    SceneNode::SceneNode(std::string id) : id(std::move(id)) {}

    // --- Hierarchy & Scene ---
    void SceneNode::addChild(const std::shared_ptr<SceneNode>& child) {
        if (!child) return;
        child->detach();
        children.push_back(child);
        child->parent = shared_from_this();
        child->setScene(this->getScene());
        markLayoutDirty();
    }
    void SceneNode::removeChild(const std::shared_ptr<SceneNode>& child) {
        if (!child) return;
        auto it = std::remove(children.begin(), children.end(), child);
        if (it != children.end()) {
            children.erase(it, children.end());
            child->parent.reset();
            child->setScene(nullptr);
            markLayoutDirty();
        }
    }
    void SceneNode::detach() {
        if (auto p = parent.lock()) {
            p->removeChild(shared_from_this());
        }
    }
    void SceneNode::setScene(const std::shared_ptr<Scene>& newScene) {
        if (scene.lock() == newScene) return;
        scene = newScene;
        markLayoutDirty();
        for (const auto& child : children) {
            child->setScene(newScene);
        }
    }
    std::shared_ptr<Scene> SceneNode::getScene() const { return scene.lock(); }

    // --- ID & Searching ---
    const std::string& SceneNode::getId() const { return id; }
    void SceneNode::setId(const std::string& newId) { id = newId; }
    std::shared_ptr<SceneNode> SceneNode::findNodeById(const std::string& searchId) {
        if (!searchId.empty() && id == searchId) return shared_from_this();
        for (const auto& child : children) {
            if (auto found = child->findNodeById(searchId)) return found;
        }
        return nullptr;
    }

    // --- Layout & Z-Index ---
    LayoutProperties& SceneNode::getLayout() {
        if (!layoutProperties) layoutProperties = std::make_unique<LayoutProperties>();
        return *layoutProperties;
    }
    const LayoutProperties& SceneNode::getLayout() const {
        static const LayoutProperties defaultProps;
        if (layoutProperties) return *layoutProperties;
        return defaultProps;
    }
    void SceneNode::markLayoutDirty() {
        if (isLayoutDirty) return;
        isLayoutDirty = true;
        if (auto p = parent.lock()) p->markLayoutDirty();
    }
    void SceneNode::setPosition(float x, float y) { getLayout().left = x; getLayout().top = y; markLayoutDirty(); }
    void SceneNode::setSize(float width, float height) { getLayout().width = width; getLayout().height = height; markLayoutDirty(); }
    void SceneNode::setMargin(const Thickness& margin) { getLayout().margin = margin; markLayoutDirty(); }
    void SceneNode::setPadding(const Thickness& padding) { getLayout().padding = padding; markLayoutDirty(); }

    Rect SceneNode::getGlobalBounds() const {
        return getLayout().computedBounds;
    }

    void SceneNode::setZIndex(int newZIndex) {
        if (zIndex != newZIndex) {
            zIndex = newZIndex;
            if (auto p = parent.lock()) p->childrenOrderDirty = true;
        }
    }
    int SceneNode::getZIndex() const { return zIndex; }

    // --- Lifecycle ---
    bool SceneNode::handleEvent(const DxvEvent& event) {
        // For mouse events, first perform a hit test.
        // If the event is outside this node's bounds, there's no need to check children.
        if (event.type == EventType::MouseDown || event.type == EventType::MouseUp || event.type == EventType::MouseMove) {
            if (!getGlobalBounds().contains(event.x, event.y)) {
                return false; // Event is outside this node and all its children.
            }
        }

        // If the event is inside, offer it to children first (top-to-bottom).
        sortChildrenIfDirty();
        for (auto it = children.rbegin(); it != children.rend(); ++it) {
            if ((*it)->handleEvent(event)) return true;
        }

        // If no child consumed it, the event is considered unhandled by this branch.
        // Specific widgets like Button will override this to handle it themselves.
        return false;
    }

    void SceneNode::updateLayoutTree() {
        if (isLayoutDirty) {
            updateLayout({0, 0, (int)getLayout().width.value_or(0), (int)getLayout().height.value_or(0)});
        }
    }

    void SceneNode::updateLayout(const Rect& parentContentRect) {
        if (!isLayoutDirty) return;
        auto& layout = getLayout();

        int finalWidth = layout.width.value_or(parentContentRect.width);
        int finalHeight = layout.height.value_or(parentContentRect.height);
        int finalX = parentContentRect.x + layout.left.value_or(0);
        int finalY = parentContentRect.y + layout.top.value_or(0);

        layout.computedBounds = {finalX, finalY, finalWidth, finalHeight};
        layout.computedInnerBounds = {
            finalX + (int)layout.padding.left,
            finalY + (int)layout.padding.top,
            finalWidth - (int)(layout.padding.left + layout.padding.right),
            finalHeight - (int)(layout.padding.top + layout.padding.bottom)
        };

        for (auto& child : children) {
            child->updateLayout(layout.computedInnerBounds);
        }
        isLayoutDirty = false;
    }

    void SceneNode::draw(IRenderer& renderer) {
        sortChildrenIfDirty();
        for (auto& child : children) {
            child->draw(renderer);
        }
    }

    void SceneNode::sortChildrenIfDirty() {
        if (childrenOrderDirty) {
            std::stable_sort(children.begin(), children.end(), [](const auto& a, const auto& b) {
                return a->getZIndex() < b->getZIndex();
            });
            childrenOrderDirty = false;
        }
    }
}
