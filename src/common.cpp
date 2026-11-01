#include "common.hpp"
#include "Geode/cocos/CCDirector.h"
#include "renderer/Buffer.hpp"

using namespace geode::prelude;

std::chrono::steady_clock::time_point modStartTime;

$on_mod(Loaded) {
    modStartTime = std::chrono::high_resolution_clock::now();
}

std::string byteSizeToString(usize size) {
    std::string unit = "B";
    double dsize = size;
    if (dsize > 1000.0) { dsize /= 1000.0; unit = "kB"; }
    if (dsize > 1000.0) { dsize /= 1000.0; unit = "MB"; }
    return fmt::format("{:.3f}{}", dsize, unit);
}

bool isOpenGLAtLeastVersion(u32 major, u32 minor) {
    // Handles both desktop ("4.6.0 NVIDIA ...") and ES ("OpenGL ES 3.2 ...").
    return bismuth::glCapabilities().isAtLeast((int)major, (int)minor);
}

void glObjectLabel(GLenum id, GLuint name, GLsizei length, const char* label) {
#if BISMUTH_GL_ES
    // KHR_debug, resolved at runtime; a no-op on drivers without it.
    if (bgl::gl.ObjectLabel)
        bgl::gl.ObjectLabel(id, name, length, label);
#else
    /*
        Desktop debug labels stay disabled. The old code looked the entry point up
        with wglGetProcAddress behind a macro that was never defined, so it never
        ran; enabling it now would be a desktop behaviour change for a purely
        cosmetic feature (and some Windows drivers return bogus pointers for
        unsupported functions).
    */
    (void)id;
    (void)name;
    (void)length;
    (void)label;
#endif
}

static float fullscreenQuad[] = {
    -1.0, -1.0,
    -1.0,  1.0,
     1.0,  1.0,
    -1.0, -1.0,
     1.0,  1.0,
     1.0, -1.0
};

u32 fullscreenQuadVAO = 0;
UPtr<Buffer> fullscreenQuadVBO = nullptr;

void drawFullscreenQuad() {
#if BISMUTH_GL_ES
    // Not linked on mobile; needs the runtime-resolved entry points.
    if (!glGenVertexArrays || !glBindVertexArray)
        return;
#endif

    if (!fullscreenQuadVAO) {
        if (!fullscreenQuadVBO)
            fullscreenQuadVBO = Buffer::createStaticDraw("Fullscreen quad buffer", fullscreenQuad, sizeof(fullscreenQuad));

        // Buffer creation can fail (e.g. out of VRAM); binding a null buffer
        // would dereference it.
        if (!fullscreenQuadVBO)
            return;

        glGenVertexArrays(1, &fullscreenQuadVAO);
        glBindVertexArray(fullscreenQuadVAO);
        fullscreenQuadVBO->bindAs(GL_ARRAY_BUFFER);
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(float) * 2, (void*)0);
        glEnableVertexAttribArray(0);
    }

    glBindVertexArray(fullscreenQuadVAO);
    glDrawArrays(GL_TRIANGLES, 0, 6);
}

#include <geode/modify/CCDirector.hpp>
class $modify(CommonCCDirector, CCDirector) {
    void purgeDirector() {
#if BISMUTH_GL_ES
        if (fullscreenQuadVAO && glDeleteVertexArrays)
            glDeleteVertexArrays(1, &fullscreenQuadVAO);
#else
        if (fullscreenQuadVAO)
            glDeleteVertexArrays(1, &fullscreenQuadVAO);
#endif
        
        fullscreenQuadVAO = 0;
        fullscreenQuadVBO = nullptr;

        CCDirector::purgeDirector();
    }
};

std::optional<std::string> readResourceFile(const fs::path& path) {
    std::ifstream t(Mod::get()->getResourcesDir() / path);
    if (!t.is_open())
        return std::nullopt;

    t.seekg(0, std::ios::end);
    size_t size = t.tellg();
    std::string buffer(size, ' ');
    t.seekg(0);
    t.read(buffer.data(), size);
    t.close();

    return buffer;
}