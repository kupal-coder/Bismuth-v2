#include "SpriteBatch.hpp"

#include <Geode/cocos/shaders/ccGLStateCache.h>

using namespace geode::prelude;

namespace bismuth::gles2 {

bool SpriteBatch::build(const SpriteQuad* const* quads, usize count, u32 texture, int zOrder, int blend) {
    if (count == 0 || count > MAX_QUADS_PER_BATCH)
        return false;

    std::vector<QuadVertex> vertices;
    vertices.reserve(count * 4);
    m_quadAABB.resize(count * 4);

    for (usize i = 0; i < count; i++) {
        const auto* quad = quads[i];

        float minX = quad->x[0], maxX = quad->x[0];
        float minY = quad->y[0], maxY = quad->y[0];
        for (int c = 1; c < 4; c++) {
            minX = std::min(minX, quad->x[c]);
            maxX = std::max(maxX, quad->x[c]);
            minY = std::min(minY, quad->y[c]);
            maxY = std::max(maxY, quad->y[c]);
        }

        m_quadAABB[i * 4 + 0] = minX;
        m_quadAABB[i * 4 + 1] = minY;
        m_quadAABB[i * 4 + 2] = maxX;
        m_quadAABB[i * 4 + 3] = maxY;

        for (int c = 0; c < 4; c++) {
            vertices.push_back(QuadVertex{
                quad->x[c], quad->y[c],
                quad->u[c], quad->v[c],
                quad->r, quad->g, quad->b, quad->a
            });
        }
    }

    m_quadCount = count;
    m_texture = texture;
    m_zOrder = zOrder;
    m_blend = blend;

    glGenBuffers(1, &m_vbo);
    glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
    glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(QuadVertex), vertices.data(), GL_STATIC_DRAW);

    glGenBuffers(1, &m_ibo);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_ibo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, count * 6 * sizeof(GLushort), nullptr, GL_DYNAMIC_DRAW);

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

    m_indices.reserve(count * 6);
    return true;
}

void SpriteBatch::draw(const VisibleRect& rect, ShaderProgram& program, const float* mvp16) {
    if (m_quadCount == 0 || m_vbo == 0 || m_ibo == 0)
        return;

    m_indices.clear();
    for (usize q = 0; q < m_quadCount; q++) {
        const float* box = &m_quadAABB[q * 4];
        // Cheap AABB reject: quads are axis aligned in batch space
        if (box[2] < rect.x0 || box[0] > rect.x1 || box[3] < rect.y0 || box[1] > rect.y1)
            continue;

        const GLushort base = (GLushort)(q * 4);
        m_indices.push_back(base + 0);
        m_indices.push_back(base + 1);
        m_indices.push_back(base + 2);
        m_indices.push_back(base + 2);
        m_indices.push_back(base + 1);
        m_indices.push_back(base + 3);
    }

    if (m_indices.empty())
        return;

    program.use();
    glUniformMatrix4fv(glGetUniformLocation(program.program(), "u_mvp"), 1, GL_FALSE, mvp16);
    glUniform1i(glGetUniformLocation(program.program(), "u_texture"), 0);

    glActiveTexture(GL_TEXTURE0);
    ccGLBindTexture2D(m_texture);

    glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_ibo);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(QuadVertex), (const void*)offsetof(QuadVertex, x));

    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(QuadVertex), (const void*)offsetof(QuadVertex, r));

    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(QuadVertex), (const void*)offsetof(QuadVertex, u));

    if (m_blend == 1)
        ccGLBlendFunc(GL_SRC_ALPHA, GL_ONE);
    else
        ccGLBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glBufferSubData(GL_ELEMENT_ARRAY_BUFFER, 0, m_indices.size() * sizeof(GLushort), m_indices.data());
    glDrawElements(GL_TRIANGLES, (GLsizei)m_indices.size(), GL_UNSIGNED_SHORT, nullptr);

    // Hand attribute state back to Cocos so its cache re-syncs on the next sprite
    ccGLEnableVertexAttribs(kCCVertexAttribFlag_None);
    glDisableVertexAttribArray(0);
    glDisableVertexAttribArray(1);
    glDisableVertexAttribArray(2);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
}

void SpriteBatch::destroy() {
    if (m_vbo) {
        glDeleteBuffers(1, &m_vbo);
        m_vbo = 0;
    }
    if (m_ibo) {
        glDeleteBuffers(1, &m_ibo);
        m_ibo = 0;
    }
    m_quadCount = 0;
    m_quadAABB.clear();
    m_indices.clear();
}

} // namespace bismuth::gles2
