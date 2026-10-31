#pragma once

#include <Geode/Geode.hpp>
#include <Geode/cocos/platform/CCGL.h>

namespace bismuth::gles2 {

// Minimal GLSL ES 1.00 / GLSL 1.10 compatible program wrapper.
// Deliberately avoids anything above GLES2: no SSBOs, no compute, no SPIR-V.
class ShaderProgram {
public:
    ~ShaderProgram() { destroy(); }

    bool initWithSource(const char* vertexSrc, const char* fragmentSrc, const char* debugName);
    void destroy();

    inline void use() const { glUseProgram(m_program); }
    inline GLuint program() const { return m_program; }
    inline bool valid() const { return m_program != 0; }

private:
    static GLuint compile(GLenum type, const char* src, const char* debugName);
    GLuint m_program = 0;
};

} // namespace bismuth::gles2
