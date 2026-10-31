#pragma once

#include <Geode/Geode.hpp>
#include <Geode/cocos/platform/CCGL.h>
#include "GLES2Shader.hpp"
#include <vector>

namespace bismuth::gles2 {

// One baked sprite quad. Positions are in m_objectLayer space, UVs are normalised.
struct SpriteQuad {
    float x[4];
    float y[4];
    float u[4];
    float v[4];
    u8 r = 255, g = 255, b = 255, a = 255;
    u32 texture = 0;
    int zOrder = 0;
    int blend = 0; // 0 = normal alpha, 1 = additive
};

struct VisibleRect {
    float x0 = 0.f, y0 = 0.f, x1 = 0.f, y1 = 0.f;
};

struct QuadVertex {
    float x, y;
    float u, v;
    u8 r, g, b, a;
};

// Max quads per batch so indices stay inside GL_UNSIGNED_SHORT range.
constexpr u32 MAX_QUADS_PER_BATCH = 16000;

// One draw call worth of geometry: a single texture, blend mode and z order.
class SpriteBatch {
public:
    ~SpriteBatch() { destroy(); }

    bool build(const SpriteQuad* const* quads, usize count, u32 texture, int zOrder, int blend);
    void draw(const VisibleRect& rect, ShaderProgram& program, const float* mvp16);
    void destroy();

    inline int zOrder() const { return m_zOrder; }
    inline usize quadCount() const { return m_quadCount; }
    inline bool empty() const { return m_quadCount == 0; }

private:
    GLuint m_vbo = 0;
    GLuint m_ibo = 0;
    GLuint m_texture = 0;
    int m_zOrder = 0;
    int m_blend = 0;
    usize m_quadCount = 0;
    std::vector<float> m_quadAABB;      // 4 floats per quad: minX, minY, maxX, maxY
    std::vector<GLushort> m_indices;    // scratch, rebuilt per frame from culling
};

} // namespace bismuth::gles2
