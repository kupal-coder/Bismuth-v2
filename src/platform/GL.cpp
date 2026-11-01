#include "GL.hpp"

#if BISMUTH_GL_ES

#include <dlfcn.h>

namespace bgl {

GLFunctionTable gl;

namespace {

typedef void* (*PFN_eglGetProcAddress)(const char*);

/*
    libEGL is already loaded by the game, so we can resolve eglGetProcAddress
    without adding a link dependency on EGL. Some Android linkers only export
    GL ES entry points through that (not through dlsym), hence the two-step
    lookup.
*/
void* eglGetProcAddressFn(const char* name) {
    static PFN_eglGetProcAddress fn = []() -> PFN_eglGetProcAddress {
        void* handle = dlopen("libEGL.so", RTLD_NOW | RTLD_LOCAL);
        if (!handle)
            return nullptr;
        return (PFN_eglGetProcAddress)dlsym(handle, "eglGetProcAddress");
    }();

    if (!fn)
        return nullptr;

    return fn(name);
}

void* loadProc(const char* name) {
    if (void* ptr = eglGetProcAddressFn(name))
        return ptr;

    return dlsym(RTLD_DEFAULT, name);
}

template <typename T>
void load(T& dest, const char* name) {
    dest = reinterpret_cast<T>(loadProc(name));
}

template <typename T>
void loadAny(T& dest, std::initializer_list<const char*> names) {
    for (auto name : names) {
        dest = reinterpret_cast<T>(loadProc(name));
        if (dest)
            return;
    }
}

} // namespace

bool loadGLFunctions() {
    /*
        Do not latch a failed load: this can be called before the game's context
        is current, and NULL entry points would then disable the renderer for the
        rest of the session. Loading again is cheap.
    */
    if (gl.computeReady)
        return true;

    // ES 3.0 core
    load(gl.GenVertexArrays, "glGenVertexArrays");
    load(gl.BindVertexArray, "glBindVertexArray");
    load(gl.DeleteVertexArrays, "glDeleteVertexArrays");
    load(gl.BindBufferBase, "glBindBufferBase");
    load(gl.BindBufferRange, "glBindBufferRange");
    load(gl.MapBufferRange, "glMapBufferRange");
    load(gl.UnmapBuffer, "glUnmapBuffer");
    load(gl.FlushMappedBufferRange, "glFlushMappedBufferRange");
    // Not part of OpenGL ES at all; stays null there and Buffer::read() no-ops.
    load(gl.GetBufferSubData, "glGetBufferSubData");
    load(gl.VertexAttribIPointer, "glVertexAttribIPointer");
    load(gl.GetUniformBlockIndex, "glGetUniformBlockIndex");
    load(gl.UniformBlockBinding, "glUniformBlockBinding");
    load(gl.GetStringi, "glGetStringi");
    loadAny(gl.GenQueries, {"glGenQueries", "glGenQueriesEXT"});
    loadAny(gl.DeleteQueries, {"glDeleteQueries", "glDeleteQueriesEXT"});
    loadAny(gl.BeginQuery, {"glBeginQuery", "glBeginQueryEXT"});
    loadAny(gl.EndQuery, {"glEndQuery", "glEndQueryEXT"});
    loadAny(gl.GetQueryObjectuiv, {"glGetQueryObjectuiv", "glGetQueryObjectuivEXT"});

    // ES 3.1 core
    load(gl.DispatchCompute, "glDispatchCompute");
    load(gl.MemoryBarrier, "glMemoryBarrier");
    load(gl.GetProgramResourceIndex, "glGetProgramResourceIndex");
    load(gl.ShaderStorageBlockBinding, "glShaderStorageBlockBinding");
    load(gl.DrawArraysIndirect, "glDrawArraysIndirect");

    // Extensions
    loadAny(gl.MultiDrawArraysIndirect, {"glMultiDrawArraysIndirect", "glMultiDrawArraysIndirectEXT"});
    loadAny(gl.ObjectLabel, {"glObjectLabel", "glObjectLabelKHR"});
    loadAny(gl.GetQueryObjectui64v, {"glGetQueryObjectui64v", "glGetQueryObjectui64vEXT"});

    /*
        Time elapsed queries. On GLES 3.0+ with EXT_disjoint_timer_query the EXT
        entry points accept GL_TIME_ELAPSED_EXT and share the core query object
        namespace, so either resolution order works.
    */
    loadAny(gl.BeginQueryTimeElapsed, {"glBeginQuery", "glBeginQueryEXT"});
    loadAny(gl.EndQueryTimeElapsed, {"glEndQuery", "glEndQueryEXT"});

    gl.computeReady =
        gl.GenVertexArrays &&
        gl.BindVertexArray &&
        gl.DeleteVertexArrays &&
        gl.BindBufferBase &&
        gl.MapBufferRange &&
        gl.UnmapBuffer &&
        gl.FlushMappedBufferRange &&
        gl.VertexAttribIPointer &&
        gl.GetUniformBlockIndex &&
        gl.UniformBlockBinding &&
        gl.DispatchCompute &&
        gl.MemoryBarrier &&
        gl.GetProgramResourceIndex &&
        gl.ShaderStorageBlockBinding &&
        gl.DrawArraysIndirect;

    gl.loaded = true;

    return gl.computeReady;
}

bool areGLFunctionsLoaded() {
    return gl.loaded;
}

} // namespace bgl

#endif // BISMUTH_GL_ES
