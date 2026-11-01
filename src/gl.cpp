// GLAD_API_CALL must be empty before the header is first included so the
// pointers below become definitions instead of extern declarations.
#define GLAD_API_CALL
#include "gl.hpp"

#if BISMUTH_GL_DESKTOP

struct CastableFuncPtr {
    void* ptr;

    template <typename T>
    operator T() {
        return (T)ptr;
    }
};

static bool loaded = false;

inline void* glGetProcAddressRaw(const char* name) {
#if defined(GEODE_IS_WINDOWS)
    return (void*)wglGetProcAddress(name);
#else
    // macOS caps out at OpenGL 4.1, so the renderer never gets this far.
    return nullptr;
#endif
}

inline CastableFuncPtr glGetProcAddress(const char* name) {
    return (CastableFuncPtr)glGetProcAddressRaw(name);
}

bool hasLoadedNewerOpenGLFunctions() {
    return loaded;
}

void loadNewerOpenGLFunctions() {
    glMultiDrawElementsIndirect = glGetProcAddress("glMultiDrawElementsIndirect");
    glMultiDrawArraysIndirect = glGetProcAddress("glMultiDrawArraysIndirect");
    glDispatchCompute = glGetProcAddress("glDispatchCompute");
    glSpecializeShader = glGetProcAddress("glSpecializeShader");
    glGetProgramResourceIndex = glGetProcAddress("glGetProgramResourceIndex");
    glShaderStorageBlockBinding = glGetProcAddress("glShaderStorageBlockBinding");

    loaded = true;
}

#endif // BISMUTH_GL_DESKTOP
