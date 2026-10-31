#include "GLES2Shader.hpp"

using namespace geode::prelude;

namespace bismuth::gles2 {

static void printShaderLog(GLuint shader, const char* debugName) {
    GLint len = 0;
    glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &len);
    if (len <= 1) return;
    std::string log(len, '\0');
    glGetShaderInfoLog(shader, len, nullptr, log.data());
    log::error("[gles2] {} shader compile log:\n{}", debugName, log);
}

GLuint ShaderProgram::compile(GLenum type, const char* src, const char* debugName) {
    GLuint shader = glCreateShader(type);
    if (!shader) {
        log::error("[gles2] glCreateShader failed for {}", debugName);
        return 0;
    }

    glShaderSource(shader, 1, &src, nullptr);
    glCompileShader(shader);

    GLint ok = 0;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        printShaderLog(shader, debugName);
        glDeleteShader(shader);
        return 0;
    }
    return shader;
}

bool ShaderProgram::initWithSource(const char* vertexSrc, const char* fragmentSrc, const char* debugName) {
    destroy();

    GLuint vs = compile(GL_VERTEX_SHADER, vertexSrc, debugName);
    if (!vs) return false;

    GLuint fs = compile(GL_FRAGMENT_SHADER, fragmentSrc, debugName);
    if (!fs) {
        glDeleteShader(vs);
        return false;
    }

    m_program = glCreateProgram();
    if (!m_program) {
        glDeleteShader(vs);
        glDeleteShader(fs);
        return false;
    }

    glAttachShader(m_program, vs);
    glAttachShader(m_program, fs);

    // Fixed attribute locations (GLES2 has no layout qualifiers)
    glBindAttribLocation(m_program, 0, "a_position");
    glBindAttribLocation(m_program, 1, "a_color");
    glBindAttribLocation(m_program, 2, "a_texCoord");

    glLinkProgram(m_program);

    GLint ok = 0;
    glGetProgramiv(m_program, GL_LINK_STATUS, &ok);
    if (!ok) {
        GLint len = 0;
        glGetProgramiv(m_program, GL_INFO_LOG_LENGTH, &len);
        std::string logStr(len > 1 ? len : 1, '\0');
        if (len > 1) glGetProgramInfoLog(m_program, len, nullptr, logStr.data());
        log::error("[gles2] {} program link failed:\n{}", debugName, logStr);
        glDeleteProgram(m_program);
        m_program = 0;
        glDeleteShader(vs);
        glDeleteShader(fs);
        return false;
    }

    glDetachShader(m_program, vs);
    glDetachShader(m_program, fs);
    glDeleteShader(vs);
    glDeleteShader(fs);

    log::info("[gles2] shader '{}' linked", debugName);
    return true;
}

void ShaderProgram::destroy() {
    if (m_program) {
        glDeleteProgram(m_program);
        m_program = 0;
    }
}

} // namespace bismuth::gles2
