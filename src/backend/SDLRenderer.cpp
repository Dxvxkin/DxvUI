#include "DxvUI/backend/SDLRenderer.h"

#include <SDL.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <stdexcept>
#include <vector>

#include "DxvUI/Log.h"
#include "DxvUI/backend/SDLTextEngine.h"
#include "DxvUI/core.h"
#include "backend/SDLTexture.h"

namespace DxvUI {

namespace {

constexpr float kPi = 3.14159265358979323846f;
constexpr float kTwoPi = 2.0f * kPi;
constexpr float kDegToRad = kPi / 180.0f;

inline float degToRad(float deg) { return deg * kDegToRad; }

// Precomputed unit quarter-circle arc (13 points, 12 segments) used to build
// every corner of a rounded rectangle. Computed once; corners are cheap
// affine transforms of this template.
const std::array<SDL_FPoint, 13>& unitArcPoints() {
    static const std::array<SDL_FPoint, 13> kArc = [] {
        std::array<SDL_FPoint, 13> arc{};
        for (int i = 0; i < 13; ++i) {
            const float a = (90.0f * static_cast<float>(i) / 12) * kPi / 180.0f;
            arc[static_cast<size_t>(i)] = {std::cos(a), std::sin(a)};
        }
        return arc;
    }();
    return kArc;
}

// Builds the closed outline of a rounded rectangle as a 52-point loop
// (4 corners x 13 arc points). x1/y1 and x2/y2 are inclusive (SDL rendering
// convention: callers pass x + width - 1). Straight edges are implicit
// between consecutive corner arcs.
std::array<SDL_FPoint, 52> roundedRectPolygon(int x1, int y1, int x2, int y2, int radius) {
    std::array<SDL_FPoint, 52> pts;
    const float r = static_cast<float>(radius);
    // Corner centers, traversed in clockwise screen order (TL, TR, BR, BL).
    const float cxs[4] = {static_cast<float>(x1 + radius), static_cast<float>(x2 - radius),
                          static_cast<float>(x2 - radius), static_cast<float>(x1 + radius)};
    const float cys[4] = {static_cast<float>(y1 + radius), static_cast<float>(y1 + radius),
                          static_cast<float>(y2 - radius), static_cast<float>(y2 - radius)};
    // Per-corner 2x2 rotation mapping the unit arc onto the corner arc:
    // TL: (-u.x,-u.y)  TR: (u.y,-u.x)  BR: (u.x,u.y)  BL: (-u.y,u.x)
    const float kx[4][2] = {{-1.0f, 0.0f}, {0.0f, 1.0f}, {1.0f, 0.0f}, {0.0f, -1.0f}};
    const float ky[4][2] = {{0.0f, -1.0f}, {-1.0f, 0.0f}, {0.0f, 1.0f}, {1.0f, 0.0f}};
    const auto& arc = unitArcPoints();
    for (int c = 0; c < 4; ++c) {
        const float cx = cxs[c];
        const float cy = cys[c];
        for (int i = 0; i < 13; ++i) {
            const float px =
                kx[c][0] * arc[static_cast<size_t>(i)].x + kx[c][1] * arc[static_cast<size_t>(i)].y;
            const float py =
                ky[c][0] * arc[static_cast<size_t>(i)].x + ky[c][1] * arc[static_cast<size_t>(i)].y;
            pts[static_cast<size_t>(c * 13 + i)] = {cx + r * px, cy + r * py};
        }
    }
    return pts;
}

// Fills a rounded rectangle on the GPU: a triangle fan from the center over
// the perimeter polygon. SDL's renderer does not cull back faces, so the
// winding of the fan is irrelevant.
void fillRoundedRectGeometry(SDL_Renderer* renderer, const Rect& rect, int radius,
                             const Color& color) {
    const int maxRadius = std::min(rect.width, rect.height) / 2;
    if (rect.width < 3 || rect.height < 3 || maxRadius <= 1 || radius <= 1) {
        SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
        SDL_Rect r = {rect.x, rect.y, rect.width, rect.height};
        SDL_RenderFillRect(renderer, &r);
        return;
    }
    const int rr = std::min(radius, maxRadius);
    const auto poly =
        roundedRectPolygon(rect.x, rect.y, rect.x + rect.width - 1, rect.y + rect.height - 1, rr);
    const SDL_Color c = {color.r, color.g, color.b, color.a};
    const SDL_FPoint center{static_cast<float>(rect.x) + static_cast<float>(rect.width) / 2.0f,
                            static_cast<float>(rect.y) + static_cast<float>(rect.height) / 2.0f};
    std::array<SDL_Vertex, 53> verts;
    verts[0] = {center, c, {0.0f, 0.0f}};
    for (size_t i = 0; i < 52; ++i) {
        verts[i + 1] = {poly[i], c, {0.0f, 0.0f}};
    }
    std::array<int, 156> indices;
    for (size_t i = 0; i < 52; ++i) {
        indices[i * 3 + 0] = 0;
        indices[i * 3 + 1] = static_cast<int>(i) + 1;
        indices[i * 3 + 2] = static_cast<int>((i + 1) % 52) + 1;
    }
    SDL_RenderGeometry(renderer, nullptr, verts.data(), 53, indices.data(), 156);
}

// Draws a rounded-rectangle border ring on the GPU: the annulus between the
// outer rounded polygon and an inset one, triangulated as a quad strip.
void drawRoundedRectRingGeometry(SDL_Renderer* renderer, const Rect& rect, int radius,
                                 int thickness, const Color& color) {
    const int w = rect.width;
    const int h = rect.height;
    if (w < 3 || h < 3 || thickness <= 0) return;
    const int maxRadius = std::min(w, h) / 2;
    if (radius <= 1 || maxRadius <= 1) {
        SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
        SDL_Rect r = {rect.x, rect.y, rect.width, rect.height};
        for (int i = 0; i < thickness; ++i) {
            SDL_RenderDrawRect(renderer, &r);
            r.x += 1;
            r.y += 1;
            r.w -= 2;
            r.h -= 2;
            if (r.w <= 0 || r.h <= 0) break;
        }
        return;
    }
    const int outerR = std::min(radius, maxRadius);
    const int t = std::max(1, std::min(thickness, maxRadius - 1));
    const int innerR = std::max(1, outerR - t);
    const auto outer = roundedRectPolygon(rect.x, rect.y, rect.x + w - 1, rect.y + h - 1, outerR);
    const auto inner =
        roundedRectPolygon(rect.x + t, rect.y + t, rect.x + w - 1 - t, rect.y + h - 1 - t, innerR);
    const SDL_Color c = {color.r, color.g, color.b, color.a};
    std::array<SDL_Vertex, 104> verts;
    for (size_t i = 0; i < 52; ++i) {
        verts[i] = {outer[i], c, {0.0f, 0.0f}};
        verts[i + 52] = {inner[i], c, {0.0f, 0.0f}};
    }
    std::array<int, 312> indices;
    for (size_t i = 0; i < 52; ++i) {
        const int o = static_cast<int>(i);
        const int oNext = static_cast<int>((i + 1) % 52);
        const int in = static_cast<int>(i) + 52;
        const int inNext = static_cast<int>((i + 1) % 52) + 52;
        indices[i * 6 + 0] = o;
        indices[i * 6 + 1] = in;
        indices[i * 6 + 2] = inNext;
        indices[i * 6 + 3] = o;
        indices[i * 6 + 4] = inNext;
        indices[i * 6 + 5] = oNext;
    }
    SDL_RenderGeometry(renderer, nullptr, verts.data(), 104, indices.data(), 312);
}

// --- GPU circle / arc / line / polygon (stage 6, no sdl2-gfx) ---

inline SDL_Color toSDLColor(const Color& c) { return {c.r, c.g, c.b, c.a}; }

inline SDL_Color lerpColor(const Color& a, const Color& b, float t) {
    t = std::clamp(t, 0.0f, 1.0f);
    return {(uint8_t)(a.r + (b.r - a.r) * t),
            (uint8_t)(a.g + (b.g - a.g) * t),
            (uint8_t)(a.b + (b.b - a.b) * t),
            (uint8_t)(a.a + (b.a - a.a) * t)};
}

void fillRectGradientGeometry(SDL_Renderer* renderer, const Rect& rect, const LinearGradient& grad) {
    if (rect.width <=0 || rect.height <=0) return;
    float angle = grad.angleDeg;
    float rad = degToRad(angle);
    float gx = std::cos(rad);
    float gy = std::sin(rad);
    // Compute dot of corners
    struct Pt { float x,y; };
    Pt corners[4] = {
        {(float)rect.x, (float)rect.y},
        {(float)rect.x + rect.width, (float)rect.y},
        {(float)rect.x + rect.width, (float)rect.y + rect.height},
        {(float)rect.x, (float)rect.y + rect.height}
    };
    float dots[4];
    for (int i=0;i<4;++i) dots[i] = corners[i].x * gx + corners[i].y * gy;
    float minDot = *std::min_element(dots, dots+4);
    float maxDot = *std::max_element(dots, dots+4);
    float range = maxDot - minDot;
    if (range < 1e-5f) range = 1.0f;

    auto tFor = [&](float x, float y) -> float {
        float d = x * gx + y * gy;
        return (d - minDot) / range;
    };

    SDL_Vertex verts[4];
    verts[0].position = {corners[0].x, corners[0].y};
    verts[0].color = lerpColor(grad.start, grad.end, tFor(corners[0].x, corners[0].y));
    verts[0].tex_coord = {0,0};
    verts[1].position = {corners[1].x, corners[1].y};
    verts[1].color = lerpColor(grad.start, grad.end, tFor(corners[1].x, corners[1].y));
    verts[1].tex_coord = {0,0};
    verts[2].position = {corners[2].x, corners[2].y};
    verts[2].color = lerpColor(grad.start, grad.end, tFor(corners[2].x, corners[2].y));
    verts[2].tex_coord = {0,0};
    verts[3].position = {corners[3].x, corners[3].y};
    verts[3].color = lerpColor(grad.start, grad.end, tFor(corners[3].x, corners[3].y));
    verts[3].tex_coord = {0,0};
    int indices[6] = {0,1,2,0,2,3};
    SDL_RenderGeometry(renderer, nullptr, verts, 4, indices, 6);
}

void fillRoundRectGradientGeometry(SDL_Renderer* renderer, const Rect& rect, int radius, const LinearGradient& grad) {
    // For rounded rect with gradient, we fallback to per-vertex gradient based on position
    // Reuse fillRoundedRectGeometry but with gradient colors per vertex
    const int maxRadius = std::min(rect.width, rect.height) / 2;
    if (rect.width < 3 || rect.height < 3 || maxRadius <= 1 || radius <= 1) {
        fillRectGradientGeometry(renderer, rect, grad);
        return;
    }
    int rr = std::min(radius, maxRadius);
    auto poly = roundedRectPolygon(rect.x, rect.y, rect.x + rect.width - 1, rect.y + rect.height - 1, rr);
    // Compute gradient dot range for this rect
    float rad = degToRad(grad.angleDeg);
    float gx = std::cos(rad);
    float gy = std::sin(rad);
    float minDot = 1e9f, maxDot = -1e9f;
    for (auto& p : poly) {
        float d = p.x * gx + p.y * gy;
        minDot = std::min(minDot, d);
        maxDot = std::max(maxDot, d);
    }
    // Include center
    SDL_FPoint center{static_cast<float>(rect.x) + rect.width * 0.5f, static_cast<float>(rect.y) + rect.height * 0.5f};
    float centerDot = center.x * gx + center.y * gy;
    minDot = std::min(minDot, centerDot);
    maxDot = std::max(maxDot, centerDot);
    float range = maxDot - minDot;
    if (range < 1e-5f) range = 1.0f;
    auto tFor = [&](float x, float y){ return (x * gx + y * gy - minDot) / range; };

    std::array<SDL_Vertex, 53> verts;
    verts[0].position = center;
    verts[0].color = lerpColor(grad.start, grad.end, tFor(center.x, center.y));
    verts[0].tex_coord = {0,0};
    for (size_t i=0;i<52;++i) {
        verts[i+1].position = poly[i];
        verts[i+1].color = lerpColor(grad.start, grad.end, tFor(poly[i].x, poly[i].y));
        verts[i+1].tex_coord = {0,0};
    }
    std::array<int, 156> indices;
    for (size_t i=0;i<52;++i) {
        indices[i*3+0]=0;
        indices[i*3+1]=static_cast<int>(i)+1;
        indices[i*3+2]=static_cast<int>((i+1)%52)+1;
    }
    SDL_RenderGeometry(renderer, nullptr, verts.data(), 53, indices.data(), 156);
}


int circleSegmentsForRadius(int r) {
    if (r <= 2) return 12;
    if (r <= 8) return 16;
    if (r <= 16) return 24;
    if (r <= 32) return 32;
    if (r <= 64) return 48;
    return 64;
}

void fillCircleGeometry(SDL_Renderer* renderer, int cX, int cY, int radius, const Color& color) {
    if (radius <= 0) return;
    if (radius == 1) {
        SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
        SDL_RenderDrawPoint(renderer, cX, cY);
        return;
    }
    const int segs = circleSegmentsForRadius(radius);
    const SDL_Color c = toSDLColor(color);
    const SDL_FPoint center{static_cast<float>(cX), static_cast<float>(cY)};
    std::vector<SDL_Vertex> verts;
    verts.reserve(segs + 1 + 1);
    verts.push_back({center, c, {0.0f, 0.0f}});
    for (int i = 0; i <= segs; ++i) {
        float ang = kTwoPi * static_cast<float>(i) / static_cast<float>(segs);
        SDL_FPoint p{static_cast<float>(cX) + static_cast<float>(radius) * std::cos(ang),
                     static_cast<float>(cY) + static_cast<float>(radius) * std::sin(ang)};
        verts.push_back({p, c, {0.0f, 0.0f}});
    }
    std::vector<int> indices;
    indices.reserve(segs * 3);
    for (int i = 0; i < segs; ++i) {
        indices.push_back(0);
        indices.push_back(i + 1);
        indices.push_back(i + 2);
    }
    SDL_RenderGeometry(renderer, nullptr, verts.data(), static_cast<int>(verts.size()),
                       indices.data(), static_cast<int>(indices.size()));
}

void drawCircleRingGeometry(SDL_Renderer* renderer, int cX, int cY, int radius, int thickness,
                            const Color& color) {
    if (radius <= 0 || thickness <= 0) return;
    const int segs = circleSegmentsForRadius(radius);
    const SDL_Color c = toSDLColor(color);
    const int innerR = std::max(0, radius - thickness);
    // If inner radius 0, just fill circle
    if (innerR <= 0) {
        fillCircleGeometry(renderer, cX, cY, radius, color);
        return;
    }
    std::vector<SDL_Vertex> verts;
    verts.reserve((segs + 1) * 2);
    for (int i = 0; i <= segs; ++i) {
        float ang = kTwoPi * static_cast<float>(i) / static_cast<float>(segs);
        float cosA = std::cos(ang);
        float sinA = std::sin(ang);
        SDL_FPoint outer{static_cast<float>(cX) + static_cast<float>(radius) * cosA,
                         static_cast<float>(cY) + static_cast<float>(radius) * sinA};
        SDL_FPoint inner{static_cast<float>(cX) + static_cast<float>(innerR) * cosA,
                         static_cast<float>(cY) + static_cast<float>(innerR) * sinA};
        verts.push_back({outer, c, {0.0f, 0.0f}});
        verts.push_back({inner, c, {0.0f, 0.0f}});
    }
    std::vector<int> indices;
    indices.reserve(segs * 6);
    for (int i = 0; i < segs; ++i) {
        int o0 = i * 2;
        int i0 = i * 2 + 1;
        int o1 = (i + 1) * 2;
        int i1 = (i + 1) * 2 + 1;
        // quad o0,i0,i1 and o0,i1,o1
        indices.push_back(o0);
        indices.push_back(i0);
        indices.push_back(i1);
        indices.push_back(o0);
        indices.push_back(i1);
        indices.push_back(o1);
    }
    SDL_RenderGeometry(renderer, nullptr, verts.data(), static_cast<int>(verts.size()),
                       indices.data(), static_cast<int>(indices.size()));
}

void drawArcRingGeometry(SDL_Renderer* renderer, int cX, int cY, int radius, float startDeg,
                         float endDeg, int thickness, const Color& color) {
    if (radius <= 0 || thickness <= 0) return;
    // Normalize angles to [0,360) and compute span
    float span = endDeg - startDeg;
    // Handle wrap: if span <=0, assume clockwise wrap
    while (span <= 0.0f) span += 360.0f;
    while (span > 360.0f) span -= 360.0f;
    if (span < 0.001f) return;
    if (span > 359.9f) {
        drawCircleRingGeometry(renderer, cX, cY, radius, thickness, color);
        return;
    }
    // Segments proportional to span, at least 1, at most circle segs
    int fullSegs = circleSegmentsForRadius(radius);
    int segs = std::max(1, static_cast<int>(std::ceil(fullSegs * span / 360.0f)));
    segs = std::max(segs, 1);
    const SDL_Color c = toSDLColor(color);
    const int innerR = std::max(0, radius - thickness);
    std::vector<SDL_Vertex> verts;
    verts.reserve((segs + 1) * 2);
    for (int i = 0; i <= segs; ++i) {
        float t = static_cast<float>(i) / static_cast<float>(segs);
        float angDeg = startDeg + t * span;
        float ang = degToRad(angDeg);
        float cosA = std::cos(ang);
        float sinA = std::sin(ang);
        SDL_FPoint outer{static_cast<float>(cX) + static_cast<float>(radius) * cosA,
                         static_cast<float>(cY) + static_cast<float>(radius) * sinA};
        SDL_FPoint inner{static_cast<float>(cX) + static_cast<float>(innerR) * cosA,
                         static_cast<float>(cY) + static_cast<float>(innerR) * sinA};
        verts.push_back({outer, c, {0.0f, 0.0f}});
        verts.push_back({inner, c, {0.0f, 0.0f}});
    }
    std::vector<int> indices;
    indices.reserve(segs * 6);
    for (int i = 0; i < segs; ++i) {
        int o0 = i * 2;
        int i0 = i * 2 + 1;
        int o1 = (i + 1) * 2;
        int i1 = (i + 1) * 2 + 1;
        indices.push_back(o0);
        indices.push_back(i0);
        indices.push_back(i1);
        indices.push_back(o0);
        indices.push_back(i1);
        indices.push_back(o1);
    }
    SDL_RenderGeometry(renderer, nullptr, verts.data(), static_cast<int>(verts.size()),
                       indices.data(), static_cast<int>(indices.size()));
}

void drawThickLineGeometry(SDL_Renderer* renderer, int x1, int y1, int x2, int y2, int thickness,
                           const Color& color) {
    if (thickness <= 1) {
        SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
        SDL_RenderDrawLine(renderer, x1, y1, x2, y2);
        return;
    }
    float dx = static_cast<float>(x2 - x1);
    float dy = static_cast<float>(y2 - y1);
    float len = std::sqrt(dx * dx + dy * dy);
    if (len < 0.001f) {
        // Zero length -> draw as circle
        fillCircleGeometry(renderer, x1, y1, thickness / 2, color);
        return;
    }
    float nx = -dy / len;
    float ny = dx / len;
    float half = static_cast<float>(thickness) * 0.5f;
    float ox = nx * half;
    float oy = ny * half;
    SDL_FPoint p0{static_cast<float>(x1) + ox, static_cast<float>(y1) + oy};
    SDL_FPoint p1{static_cast<float>(x1) - ox, static_cast<float>(y1) - oy};
    SDL_FPoint p2{static_cast<float>(x2) - ox, static_cast<float>(y2) - oy};
    SDL_FPoint p3{static_cast<float>(x2) + ox, static_cast<float>(y2) + oy};
    SDL_Color c = toSDLColor(color);
    SDL_Vertex verts[4] = {{p0, c, {0, 0}}, {p1, c, {0, 0}}, {p2, c, {0, 0}}, {p3, c, {0, 0}}};
    int indices[6] = {0, 1, 2, 0, 2, 3};
    SDL_RenderGeometry(renderer, nullptr, verts, 4, indices, 6);
}

// --- Polygon triangulation (ear clipping) ---

struct Vec2 {
    float x, y;
};

inline float cross(const Vec2& a, const Vec2& b, const Vec2& c) {
    // (b-a) x (c-a)
    return (b.x - a.x) * (c.y - a.y) - (b.y - a.y) * (c.x - a.x);
}

inline bool pointInTriangle(const Vec2& p, const Vec2& a, const Vec2& b, const Vec2& c) {
    // barycentric via cross signs
    float c1 = cross(a, b, p);
    float c2 = cross(b, c, p);
    float c3 = cross(c, a, p);
    // Allow on edge
    bool hasNeg = (c1 < -1e-5f) || (c2 < -1e-5f) || (c3 < -1e-5f);
    bool hasPos = (c1 > 1e-5f) || (c2 > 1e-5f) || (c3 > 1e-5f);
    return !(hasNeg && hasPos);
}

std::vector<int> triangulateEarClipping(const std::vector<Vec2>& poly) {
    int n = static_cast<int>(poly.size());
    if (n < 3) return {};
    if (n == 3) return {0, 1, 2};

    // Compute signed area to determine winding
    float area = 0.0f;
    for (int i = 0; i < n; ++i) {
        int j = (i + 1) % n;
        area += poly[i].x * poly[j].y - poly[j].x * poly[i].y;
    }
    bool ccw = area > 0.0f;

    std::vector<int> V(n);
    for (int i = 0; i < n; ++i) V[i] = i;

    std::vector<int> indices;
    indices.reserve((n - 2) * 3);

    int guard = 0;
    while (V.size() > 3 && guard < n * n) {
        bool earFound = false;
        for (size_t i = 0; i < V.size(); ++i) {
            int prevIdx = V[(i + V.size() - 1) % V.size()];
            int currIdx = V[i];
            int nextIdx = V[(i + 1) % V.size()];

            const Vec2& prev = poly[prevIdx];
            const Vec2& curr = poly[currIdx];
            const Vec2& next = poly[nextIdx];

            float cr = cross(prev, curr, next);
            bool isConvex = ccw ? (cr > 1e-5f) : (cr < -1e-5f);
            if (!isConvex) continue;

            bool anyInside = false;
            for (int vi : V) {
                if (vi == prevIdx || vi == currIdx || vi == nextIdx) continue;
                if (pointInTriangle(poly[vi], prev, curr, next)) {
                    anyInside = true;
                    break;
                }
            }
            if (anyInside) continue;

            // Ear
            indices.push_back(prevIdx);
            indices.push_back(currIdx);
            indices.push_back(nextIdx);
            V.erase(V.begin() + i);
            earFound = true;
            break;
        }
        if (!earFound) {
            // Fallback to fan triangulation for degenerate / self-intersecting
            break;
        }
        ++guard;
    }
    if (V.size() == 3) {
        indices.push_back(V[0]);
        indices.push_back(V[1]);
        indices.push_back(V[2]);
    } else if (!V.empty() && indices.empty()) {
        // Fallback fan from 0
        for (size_t i = 1; i + 1 < V.size(); ++i) {
            indices.push_back(V[0]);
            indices.push_back(V[i]);
            indices.push_back(V[i + 1]);
        }
    }
    return indices;
}

void fillPolygonGeometry(SDL_Renderer* renderer, const std::vector<PointI>& points,
                         const Color& color) {
    if (points.size() < 3) return;
    std::vector<Vec2> poly;
    poly.reserve(points.size());
    for (auto& p : points) poly.push_back({static_cast<float>(p.x), static_cast<float>(p.y)});

    auto indices = triangulateEarClipping(poly);
    if (indices.empty()) return;

    SDL_Color c = toSDLColor(color);
    std::vector<SDL_Vertex> verts;
    verts.reserve(poly.size());
    for (auto& v : poly) {
        verts.push_back({{v.x, v.y}, c, {0.0f, 0.0f}});
    }
    SDL_RenderGeometry(renderer, nullptr, verts.data(), static_cast<int>(verts.size()),
                       indices.data(), static_cast<int>(indices.size()));
}

void fillPolygonGeometryF(SDL_Renderer* renderer, const std::vector<SDL_FPoint>& points,
                          const Color& color) {
    if (points.size() < 3) return;
    std::vector<Vec2> poly;
    poly.reserve(points.size());
    for (auto& p : points) poly.push_back({p.x, p.y});
    auto indices = triangulateEarClipping(poly);
    if (indices.empty()) return;
    SDL_Color c = toSDLColor(color);
    std::vector<SDL_Vertex> verts;
    verts.reserve(poly.size());
    for (auto& v : poly) verts.push_back({{v.x, v.y}, c, {0.0f, 0.0f}});
    SDL_RenderGeometry(renderer, nullptr, verts.data(), static_cast<int>(verts.size()),
                       indices.data(), static_cast<int>(indices.size()));
}

}  // namespace

SDLRenderer::SDLRenderer(const char* title, int width, int height, bool vsync)
    : ownsResources(true) {
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "linear");
    if (SDL_Init(SDL_INIT_VIDEO) != 0)
        throw std::runtime_error(std::string("SDL_Init Error: ") + SDL_GetError());
    window = SDL_CreateWindow(title, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, width, height,
                              SDL_WINDOW_SHOWN);
    if (!window) {
        SDL_Quit();
        throw std::runtime_error(std::string("SDL_CreateWindow Error: ") + SDL_GetError());
    }
    renderer = SDL_CreateRenderer(
        window, -1, SDL_RENDERER_ACCELERATED | (vsync ? SDL_RENDERER_PRESENTVSYNC : 0));
    if (!renderer) {
        SDL_DestroyWindow(window);
        SDL_Quit();
        throw std::runtime_error(std::string("SDL_CreateRenderer Error: ") + SDL_GetError());
    }
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    textEngine = std::make_unique<SDLTextEngine>(renderer);
    setCursor(CursorType::Arrow);
}

SDLRenderer::SDLRenderer(SDL_Renderer* externalRenderer)
    : window(nullptr), renderer(externalRenderer), ownsResources(false) {
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "linear");
    if (!renderer) throw std::invalid_argument("externalRenderer cannot be null.");
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    textEngine = std::make_unique<SDLTextEngine>(renderer);
    setCursor(CursorType::Arrow);
}

SDLRenderer::~SDLRenderer() {
    if (textEngine) {
        textEngine->clearCaches();
    }

    for (auto& pair : cursorCache) {
        if (pair.second) SDL_FreeCursor(pair.second);
    }
    cursorCache.clear();

    if (ownsResources) {
        if (renderer) SDL_DestroyRenderer(renderer);
        if (window) SDL_DestroyWindow(window);
        SDL_Quit();
    }
}

ITextEngine& SDLRenderer::getTextEngine() { return *textEngine; }

IClipboard& SDLRenderer::getClipboard() { return clipboard; }

void SDLRenderer::setCursor(CursorType type) {
    if (currentCursorType == type && SDL_GetCursor() != nullptr) return;
    SDL_SetCursor(getSystemCursor(type));
    currentCursorType = type;
}

CursorType SDLRenderer::getCursor() const { return currentCursorType; }

void SDLRenderer::pushClipRect(const Rect& rect) {
    flushFillRectBatch();
    const SDL_bool clipEnabled = SDL_RenderIsClipEnabled(renderer);
    SDL_Rect currentClip;
    SDL_RenderGetClipRect(renderer, &currentClip);
    clipStack.emplace_back(clipEnabled == SDL_TRUE,
                           Rect{currentClip.x, currentClip.y, currentClip.w, currentClip.h});

    SDL_Rect r = {rect.x, rect.y, rect.width, rect.height};
    if (clipEnabled == SDL_TRUE) {
        SDL_Rect intersection;
        if (SDL_IntersectRect(&r, &currentClip, &intersection)) {
            r = intersection;
        } else {
            int outW = 0, outH = 0;
            SDL_GetRendererOutputSize(renderer, &outW, &outH);
            r = {outW, outH, 1, 1};
        }
    }
    SDL_RenderSetClipRect(renderer, &r);
}

void SDLRenderer::popClipRect() {
    flushFillRectBatch();
    if (clipStack.empty()) {
        Log::error("SDLRenderer::popClipRect called with an empty clip stack");
        return;
    }

    const auto& [enabled, rect] = clipStack.back();
    if (enabled) {
        SDL_Rect r = {rect.x, rect.y, rect.width, rect.height};
        SDL_RenderSetClipRect(renderer, &r);
    } else {
        SDL_RenderSetClipRect(renderer, nullptr);
    }
    clipStack.pop_back();
}

SDL_Cursor* SDLRenderer::getSystemCursor(CursorType type) {
    auto it = cursorCache.find(type);
    if (it != cursorCache.end()) {
        return it->second;
    }

    SDL_SystemCursor id;
    switch (type) {
        case CursorType::IBeam:
            id = SDL_SYSTEM_CURSOR_IBEAM;
            break;
        case CursorType::Wait:
            id = SDL_SYSTEM_CURSOR_WAIT;
            break;
        case CursorType::Crosshair:
            id = SDL_SYSTEM_CURSOR_CROSSHAIR;
            break;
        case CursorType::Hand:
            id = SDL_SYSTEM_CURSOR_HAND;
            break;
        case CursorType::ResizeNWSE:
            id = SDL_SYSTEM_CURSOR_SIZENWSE;
            break;
        case CursorType::ResizeNESW:
            id = SDL_SYSTEM_CURSOR_SIZENESW;
            break;
        case CursorType::ResizeWE:
            id = SDL_SYSTEM_CURSOR_SIZEWE;
            break;
        case CursorType::ResizeNS:
            id = SDL_SYSTEM_CURSOR_SIZENS;
            break;
        case CursorType::ResizeAll:
            id = SDL_SYSTEM_CURSOR_SIZEALL;
            break;
        case CursorType::No:
            id = SDL_SYSTEM_CURSOR_NO;
            break;
        case CursorType::Arrow:
        default:
            id = SDL_SYSTEM_CURSOR_ARROW;
            break;
    }

    SDL_Cursor* cursor = SDL_CreateSystemCursor(id);
    if (cursor) {
        cursorCache[type] = cursor;
    }
    return cursor;
}

void SDLRenderer::clear(const Color& color) {
    flushFillRectBatch();
    setSDLDrawColor(color);
    SDL_RenderClear(renderer);
}
void SDLRenderer::present() {
    flushFillRectBatch(); SDL_RenderPresent(renderer); }

Size SDLRenderer::getViewportSize() const {
    int w, h;
    SDL_GetRendererOutputSize(renderer, &w, &h);
    return {(float)w, (float)h};
}

float SDLRenderer::getDpiScale() const {
    // Stage 5: return 1.0f for now, future HiDPI will query SDL_GetRendererOutputSize vs window size
    return 1.0f;
}

ICanvas& SDLRenderer::beginFrame(const Color& clearColor) {
    flushFillRectBatch();
    // Stage 5: frame lifecycle – clear if we own resources, otherwise host does clear
    if (ownsResources) {
        clear(clearColor);
    }
    return *this;
}

void SDLRenderer::endFrame() {
    flushFillRectBatch();
    if (ownsResources) {
        present();
    }
}

// --- ICanvas float-based implementation (stage 5 real backend) ---

void SDLRenderer::pushClip(const RectF& rect) {
    pushClipRect(rect.rounded());
}

void SDLRenderer::popClip() {
    popClipRect();
}

void SDLRenderer::drawTexture(const std::shared_ptr<ITexture>& texture, const RectF& dstRect) {
    drawTexture(texture, dstRect.rounded());
}

void SDLRenderer::drawTexture(const std::shared_ptr<ITexture>& texture, const TextureDraw& draw) {
    // Translate float TextureDraw -> int TextureDrawDesc
    TextureDrawDesc desc;
    desc.dst = draw.dst.rounded();
    if (draw.src) {
        desc.src = draw.src->rounded();
    }
    desc.tint = draw.tint;
    desc.alpha = draw.alpha;
    // rotation/flip ignored for now (stage 6)
    drawTexture(texture, desc);
}

void SDLRenderer::fillRect(const RectF& rect, const Fill& fill) {
    flushFillRectBatch();
    Rect r = rect.rounded();
    if (fill.gradient) {
        fillRectGradientGeometry(renderer, r, *fill.gradient);
    } else {
        batchFillRect(r, fill.color);
    }
}

void SDLRenderer::strokeRect(const RectF& rect, const Stroke& stroke) {
    drawRect(rect.rounded(), Border{stroke.color, static_cast<int>(std::lround(stroke.thickness))});
}

void SDLRenderer::fillRoundRect(const RectF& rect, float radius, const Brush& brush) {
    if (brush.isEmpty()) return;
    flushFillRectBatch();
    Rect pixelRect = rect.rounded();
    int pixelRadius = std::max(0, static_cast<int>(std::lround(radius)));
    auto toBorder = [](const Stroke& s) -> Border {
        return {s.color, std::max(1, static_cast<int>(std::lround(s.thickness)))};
    };

    // Shadow first
    if (brush.shadow) {
        Rect shadowRect{pixelRect.x + static_cast<int>(std::lround(brush.shadow->offsetX)),
                        pixelRect.y + static_cast<int>(std::lround(brush.shadow->offsetY)),
                        pixelRect.width, pixelRect.height};
        // Simple blur approximated as extra spread
        int blur = static_cast<int>(std::lround(brush.shadow->blur));
        if (blur > 0) {
            shadowRect.x -= blur/2;
            shadowRect.y -= blur/2;
            shadowRect.width += blur;
            shadowRect.height += blur;
        }
        if (brush.fill && brush.fill->gradient) {
            // For shadow with gradient, use shadow color solid
            fillRoundedRectGeometry(renderer, shadowRect, pixelRadius, brush.shadow->color);
        } else {
            fillRoundedRectGeometry(renderer, shadowRect, pixelRadius, brush.shadow->color);
        }
    }

    if (brush.fill && brush.stroke) {
        // If fill has gradient, use gradient path
        if (brush.fill->gradient) {
            fillRoundRectGradientGeometry(renderer, pixelRect, pixelRadius, *brush.fill->gradient);
            drawRoundedRectRingGeometry(renderer, pixelRect, pixelRadius, toBorder(*brush.stroke).thickness, toBorder(*brush.stroke).color);
        } else {
            fillRoundRect(pixelRect, pixelRadius, brush.fill->color, toBorder(*brush.stroke));
        }
    } else if (brush.fill) {
        if (brush.fill->gradient) {
            fillRoundRectGradientGeometry(renderer, pixelRect, pixelRadius, *brush.fill->gradient);
        } else {
            fillRoundRect(pixelRect, pixelRadius, brush.fill->color);
        }
    } else {
        drawRoundRect(pixelRect, pixelRadius, toBorder(*brush.stroke));
    }
}

void SDLRenderer::fillCircle(const PointF& center, float radius, const Brush& brush) {
    if (brush.isEmpty()) return;
    flushFillRectBatch();
    PointI pixelCenter = center.rounded();
    int pixelRadius = std::max(0, static_cast<int>(std::lround(radius)));
    auto toBorder = [](const Stroke& s) -> Border {
        return {s.color, std::max(1, static_cast<int>(std::lround(s.thickness)))};
    };

    if (brush.shadow) {
        PointI shadowCenter{pixelCenter.x + static_cast<int>(std::lround(brush.shadow->offsetX)),
                            pixelCenter.y + static_cast<int>(std::lround(brush.shadow->offsetY))};
        fillCircleGeometry(renderer, shadowCenter.x, shadowCenter.y, pixelRadius, brush.shadow->color);
    }

    if (brush.fill && brush.stroke) {
        fillCircle(pixelCenter.x, pixelCenter.y, pixelRadius, brush.fill->color, toBorder(*brush.stroke));
    } else if (brush.fill) {
        fillCircle(pixelCenter.x, pixelCenter.y, pixelRadius, brush.fill->color);
    } else {
        drawCircle(pixelCenter.x, pixelCenter.y, pixelRadius, toBorder(*brush.stroke));
    }
}

void SDLRenderer::strokeArc(const PointF& center, float radius, float startAngle, float endAngle,
                            const Stroke& stroke) {
    const PointI pixelCenter = center.rounded();
    const int pixelRadius = std::max(0, static_cast<int>(std::lround(radius)));
    drawArc(pixelCenter.x, pixelCenter.y, pixelRadius, startAngle, endAngle,
            Border{stroke.color, std::max(1, static_cast<int>(std::lround(stroke.thickness)))});
}

void SDLRenderer::fillPolygon(std::span<const PointF> points, const Fill& fill) {
    if (points.size() < 3) return;
    // Convert to SDL_FPoint for geometry path
    std::vector<SDL_FPoint> fpts;
    fpts.reserve(points.size());
    for (const PointF& p : points) fpts.push_back({p.x, p.y});
    // Triangulate via ear clipping using float version
    std::vector<Vec2> poly;
    poly.reserve(fpts.size());
    for (auto& fp : fpts) poly.push_back({fp.x, fp.y});
    auto indices = triangulateEarClipping(poly);
    if (indices.empty()) return;
    SDL_Color c = toSDLColor(fill.color);
    std::vector<SDL_Vertex> verts;
    verts.reserve(poly.size());
    for (auto& v : poly) verts.push_back({{v.x, v.y}, c, {0, 0}});
    SDL_RenderGeometry(renderer, nullptr, verts.data(), static_cast<int>(verts.size()),
                       indices.data(), static_cast<int>(indices.size()));
}

void SDLRenderer::drawLine(const PointF& from, const PointF& to, const Stroke& stroke) {
    const PointI a = from.rounded();
    const PointI b = to.rounded();
    drawLine(a.x, a.y, b.x, b.y, stroke.color, std::max(1, static_cast<int>(std::lround(stroke.thickness))));
}


void SDLRenderer::drawTexture(const std::shared_ptr<ITexture>& texture, const Rect& dstRect) {
    flushFillRectBatch();
    if (!texture) return;
    const auto* sdlTexture = dynamic_cast<SDLTexture*>(texture.get());
    if (!sdlTexture || !sdlTexture->_texture) {
        Log::error(
            "SDLRenderer::drawTexture: the texture was not created by this renderer (foreign "
            "ITexture implementation or a freed handle); skipping");
        return;
    }
    SDL_Rect dst = {dstRect.x, dstRect.y, dstRect.width, dstRect.height};
    SDL_SetTextureColorMod(sdlTexture->_texture, 255, 255, 255);
    SDL_SetTextureAlphaMod(sdlTexture->_texture, 255);
    SDL_RenderCopy(renderer, sdlTexture->_texture, nullptr, &dst);
}

void SDLRenderer::drawTexture(const std::shared_ptr<ITexture>& texture,
                              const TextureDrawDesc& desc) {
    flushFillRectBatch();
    if (!texture) return;
    const auto* sdlTexture = dynamic_cast<SDLTexture*>(texture.get());
    if (!sdlTexture || !sdlTexture->_texture) {
        Log::error(
            "SDLRenderer::drawTexture(tinted): the texture was not created by this renderer "
            "(foreign ITexture implementation or a freed handle); skipping");
        return;
    }

    SDL_Rect dst = {desc.dst.x, desc.dst.y, desc.dst.width, desc.dst.height};
    SDL_Rect src;
    SDL_Rect* srcPtr = nullptr;
    if (desc.src) {
        src = {desc.src->x, desc.src->y, desc.src->width, desc.src->height};
        srcPtr = &src;
    }

    if (desc.tint) {
        SDL_SetTextureColorMod(sdlTexture->_texture, desc.tint->r, desc.tint->g, desc.tint->b);
        const float a = desc.tint->a / 255.0f * desc.alpha;
        SDL_SetTextureAlphaMod(sdlTexture->_texture,
                               static_cast<Uint8>(std::clamp(a * 255.0f, 0.0f, 255.0f)));
    } else {
        SDL_SetTextureColorMod(sdlTexture->_texture, 255, 255, 255);
        SDL_SetTextureAlphaMod(sdlTexture->_texture,
                               static_cast<Uint8>(std::clamp(desc.alpha * 255.0f, 0.0f, 255.0f)));
    }

    SDL_RenderCopy(renderer, sdlTexture->_texture, srcPtr, &dst);

    SDL_SetTextureColorMod(sdlTexture->_texture, 255, 255, 255);
    SDL_SetTextureAlphaMod(sdlTexture->_texture, 255);
}

void SDLRenderer::setSDLDrawColor(const Color& color) {
    SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
}

void SDLRenderer::flushFillRectBatch() {
    if (!fillRectBatch_ || fillRectBatch_->rects.empty()) {
        fillRectBatch_.reset();
        return;
    }
    const auto& batch = *fillRectBatch_;
    SDL_Color c = toSDLColor(batch.color);
    std::vector<SDL_Vertex> verts;
    verts.reserve(batch.rects.size() * 4);
    std::vector<int> indices;
    indices.reserve(batch.rects.size() * 6);
    int v = 0;
    for (const auto& r : batch.rects) {
        if (r.width <= 0 || r.height <= 0) continue;
        SDL_FPoint p0{static_cast<float>(r.x), static_cast<float>(r.y)};
        SDL_FPoint p1{static_cast<float>(r.x + r.width), static_cast<float>(r.y)};
        SDL_FPoint p2{static_cast<float>(r.x + r.width), static_cast<float>(r.y + r.height)};
        SDL_FPoint p3{static_cast<float>(r.x), static_cast<float>(r.y + r.height)};
        verts.push_back({p0, c, {0, 0}});
        verts.push_back({p1, c, {0, 0}});
        verts.push_back({p2, c, {0, 0}});
        verts.push_back({p3, c, {0, 0}});
        indices.push_back(v + 0);
        indices.push_back(v + 1);
        indices.push_back(v + 2);
        indices.push_back(v + 0);
        indices.push_back(v + 2);
        indices.push_back(v + 3);
        v += 4;
    }
    if (!verts.empty()) {
        SDL_RenderGeometry(renderer, nullptr, verts.data(), static_cast<int>(verts.size()),
                           indices.data(), static_cast<int>(indices.size()));
    }
    fillRectBatch_.reset();
}

void SDLRenderer::batchFillRect(const Rect& rect, const Color& color) {
    if (rect.width <= 0 || rect.height <= 0) return;
    if (!fillRectBatch_ || !(fillRectBatch_->color == color)) {
        flushFillRectBatch();
        fillRectBatch_ = FillRectBatch{color, {}};
    }
    fillRectBatch_->rects.push_back(rect);
    // Flush if batch too large to avoid huge buffers
    if (fillRectBatch_->rects.size() >= 256) {
        flushFillRectBatch();
    }
}

void SDLRenderer::drawRect(const Rect& rect, const Border& border) {
    flushFillRectBatch();
    if (border.thickness <= 0) return;
    setSDLDrawColor(border.color);
    for (int i = 0; i < border.thickness; ++i) {
        SDL_Rect r = {rect.x + i, rect.y + i, rect.width - 2 * i, rect.height - 2 * i};
        if (r.w <= 0 || r.h <= 0) break;
        SDL_RenderDrawRect(renderer, &r);
    }
}
void SDLRenderer::fillRect(const Rect& rect, const Color& color) {
    batchFillRect(rect, color);
}
void SDLRenderer::fillRect(const Rect& rect, const Color& fillColor, const Border& border) {
    batchFillRect(rect, fillColor);
    if (border.thickness > 0) {
        flushFillRectBatch();
        drawRect(rect, border);
    }
}
void SDLRenderer::drawLine(int x1, int y1, int x2, int y2, const Color& color, int thickness) {
    flushFillRectBatch();
    drawThickLineGeometry(renderer, x1, y1, x2, y2, thickness, color);
}
void SDLRenderer::fillCircle(int cX, int cY, int r, const Color& color) {
    flushFillRectBatch();
    fillCircleGeometry(renderer, cX, cY, r, color);
}
void SDLRenderer::drawCircle(int cX, int cY, int r, const Border& border) {
    flushFillRectBatch();
    drawCircleRingGeometry(renderer, cX, cY, r, border.thickness, border.color);
}
void SDLRenderer::fillCircle(int cX, int cY, int r, const Color& f, const Border& b) {
    flushFillRectBatch();
    fillCircle(cX, cY, r, f);
    drawCircle(cX, cY, r, b);
}
void SDLRenderer::drawArc(int cX, int cY, int r, float sA, float eA, const Border& border) {
    flushFillRectBatch();
    drawArcRingGeometry(renderer, cX, cY, r, sA, eA, border.thickness, border.color);
}
void SDLRenderer::drawRoundRect(const Rect& rect, int radius, const Border& border) {
    flushFillRectBatch();
    drawRoundedRectRingGeometry(renderer, rect, radius, border.thickness, border.color);
}
void SDLRenderer::fillRoundRect(const Rect& rect, int radius, const Color& color) {
    flushFillRectBatch();
    fillRoundedRectGeometry(renderer, rect, radius, color);
}
void SDLRenderer::fillRoundRect(const Rect& rect, int radius, const Color& fillColor,
                                const Border& border) {
    flushFillRectBatch();
    fillRoundedRectGeometry(renderer, rect, radius, fillColor);
    if (border.thickness > 0) {
        drawRoundedRectRingGeometry(renderer, rect, radius, border.thickness, border.color);
    }
}
void SDLRenderer::fillPolygon(const std::vector<PointI>& points, const Color& color) {
    flushFillRectBatch();
    fillPolygonGeometry(renderer, points, color);
}

std::shared_ptr<ITexture> SDLRenderer::createTexture(const ImageData& data) {
    flushFillRectBatch();
    if (!data.isValid()) {
        Log::error("SDLRenderer::createTexture: invalid ImageData");
        return nullptr;
    }
    // Convert to RGBA8
    std::vector<uint8_t> rgba;
    rgba.reserve(data.width * data.height * 4);
    if (data.channels == 4) {
        rgba = data.pixels;
    } else if (data.channels == 3) {
        rgba.resize(data.width * data.height * 4);
        for (int i = 0; i < data.width * data.height; ++i) {
            rgba[i * 4 + 0] = data.pixels[i * 3 + 0];
            rgba[i * 4 + 1] = data.pixels[i * 3 + 1];
            rgba[i * 4 + 2] = data.pixels[i * 3 + 2];
            rgba[i * 4 + 3] = 255;
        }
    } else if (data.channels == 1) {
        rgba.resize(data.width * data.height * 4);
        for (int i = 0; i < data.width * data.height; ++i) {
            uint8_t v = data.pixels[i];
            rgba[i * 4 + 0] = v;
            rgba[i * 4 + 1] = v;
            rgba[i * 4 + 2] = v;
            rgba[i * 4 + 3] = 255;
        }
    }

    SDL_Texture* tex = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA32,
                                         SDL_TEXTUREACCESS_STATIC, data.width, data.height);
    if (!tex) {
        Log::error("SDLRenderer::createTexture: SDL_CreateTexture failed: {}", SDL_GetError());
        return nullptr;
    }
    SDL_SetTextureBlendMode(tex, SDL_BLENDMODE_BLEND);
    // SDL_UpdateTexture expects pitch = width * bytesPerPixel
    if (SDL_UpdateTexture(tex, nullptr, rgba.data(), data.width * 4) != 0) {
        Log::error("SDLRenderer::createTexture: SDL_UpdateTexture failed: {}", SDL_GetError());
        SDL_DestroyTexture(tex);
        return nullptr;
    }
    return std::make_shared<SDLTexture>(tex);
}

std::shared_ptr<ITexture> SDLRenderer::createRenderTarget(int width, int height) {
    flushFillRectBatch();
    if (width <= 0 || height <= 0) {
        Log::error("SDLRenderer::createRenderTarget: invalid size {}x{}", width, height);
        return nullptr;
    }
    SDL_Texture* tex = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA8888,
                                         SDL_TEXTUREACCESS_TARGET, width, height);
    if (!tex) {
        Log::error("SDLRenderer::createRenderTarget: SDL_CreateTexture TARGET failed: {}", SDL_GetError());
        return nullptr;
    }
    SDL_SetTextureBlendMode(tex, SDL_BLENDMODE_BLEND);
    return std::make_shared<SDLTexture>(tex);
}

void SDLRenderer::beginRenderTarget(const std::shared_ptr<ITexture>& target) {
    flushFillRectBatch();
    auto* sdlTex = dynamic_cast<SDLTexture*>(target.get());
    if (!sdlTex || !sdlTex->_texture) {
        Log::error("SDLRenderer::beginRenderTarget: foreign texture");
        return;
    }
    // Push current target
    SDL_Texture* current = SDL_GetRenderTarget(renderer);
    renderTargetStack.push_back(current);
    if (SDL_SetRenderTarget(renderer, sdlTex->_texture) != 0) {
        Log::error("SDLRenderer::beginRenderTarget: SDL_SetRenderTarget failed: {}", SDL_GetError());
        renderTargetStack.pop_back();
    }
}

void SDLRenderer::endRenderTarget() {
    flushFillRectBatch();
    if (renderTargetStack.empty()) {
        Log::error("SDLRenderer::endRenderTarget: stack empty, resetting to default");
        SDL_SetRenderTarget(renderer, nullptr);
        return;
    }
    SDL_Texture* prev = renderTargetStack.back();
    renderTargetStack.pop_back();
    if (SDL_SetRenderTarget(renderer, prev) != 0) {
        Log::error("SDLRenderer::endRenderTarget: SDL_SetRenderTarget restore failed: {}", SDL_GetError());
    }
}

}  // namespace DxvUI
