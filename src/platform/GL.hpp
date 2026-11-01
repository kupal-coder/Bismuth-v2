#pragma once

/*
    Single place where Bismuth includes OpenGL.

    Desktop (Windows/macOS)
        OpenGL 4.x, entry points come from Geode's cocos GL headers (GLEW on
        Windows) so every function can be called directly.

    Android
        OpenGL ES. The game's shared libraries only link `GLESv2`, so every
        ES 3.0+/extension entry point has to be resolved at runtime with
        eglGetProcAddress. Those calls are routed through the `bgl` table below,
        which is filled in by `bgl::loadGLFunctions()` once a context is current.

    iOS
        Apple never shipped OpenGL ES 3.1, so compute shaders / SSBOs do not
        exist there. The table stays empty, the renderer refuses to start and the
        mod falls back to the vanilla renderer.
*/

#include <Geode/Geode.hpp>

#if defined(GEODE_IS_WINDOWS) || defined(GEODE_IS_MACOS)
#  define BISMUTH_GL_DESKTOP 1
#  define BISMUTH_GL_ES 0
#elif defined(GEODE_IS_ANDROID) || defined(GEODE_IS_IOS)
#  define BISMUTH_GL_DESKTOP 0
#  define BISMUTH_GL_ES 1
#else
#  define BISMUTH_GL_DESKTOP 1
#  define BISMUTH_GL_ES 0
#endif

// Only the Android NDK ships ES 3.1 headers.
#if defined(GEODE_IS_ANDROID)
#  define BISMUTH_GL_HAS_ES31_HEADERS 1
#else
#  define BISMUTH_GL_HAS_ES31_HEADERS 0
#endif

#if BISMUTH_GL_DESKTOP
#  include <Geode/cocos/platform/CCGL.h>
#elif BISMUTH_GL_HAS_ES31_HEADERS
#  include <GLES3/gl31.h>
#  include <GLES3/gl3ext.h>
#else
#  include <OpenGLES/ES3/gl.h>
#  include <OpenGLES/ES3/glext.h>
#endif

#if BISMUTH_GL_ES
/*
    cocos2d (and Geode's per-platform CCGL.h) alias a few ES3 names onto their
    ES2 (OES) entry points. Bismuth goes through the core entry points that
    `bgl` resolves instead, so drop those aliases before our code is parsed.
*/
#  undef glGenVertexArrays
#  undef glBindVertexArray
#  undef glDeleteVertexArrays
#  undef glMapBuffer
#  undef glUnmapBuffer
#  undef glClearDepth
#  undef GL_WRITE_ONLY

/*
    Constants that only exist in ES 3.1+ headers (or in the KHR/EXT headers).
    The iOS SDK stops at ES 3.0, so define what is missing there as well so the
    shared code still compiles.
*/
#  ifndef GL_VERTEX_ARRAY_BINDING
#    define GL_VERTEX_ARRAY_BINDING 0x85B5
#  endif
#  ifndef GL_COMPUTE_SHADER
#    define GL_COMPUTE_SHADER 0x91B9
#  endif
#  ifndef GL_SHADER_STORAGE_BUFFER
#    define GL_SHADER_STORAGE_BUFFER 0x90D2
#  endif
#  ifndef GL_SHADER_STORAGE_BLOCK
#    define GL_SHADER_STORAGE_BLOCK 0x92E6
#  endif
#  ifndef GL_SHADER_STORAGE_BARRIER_BIT
#    define GL_SHADER_STORAGE_BARRIER_BIT 0x00002000
#  endif
#  ifndef GL_COMMAND_BARRIER_BIT
#    define GL_COMMAND_BARRIER_BIT 0x00000040
#  endif
#  ifndef GL_BUFFER
#    define GL_BUFFER 0x82E0
#  endif
#  ifndef GL_TIME_ELAPSED
#    define GL_TIME_ELAPSED 0x88BF
#  endif
#  ifndef GL_TIME_ELAPSED_EXT
#    define GL_TIME_ELAPSED_EXT 0x88BF
#  endif
#endif

#if BISMUTH_GL_ES

namespace bgl {

/*
    Function table for everything the renderer needs that is not part of
    GLES 2.0. All of these are additionally guarded by `GLCapabilities`, so a
    null entry means the feature is unavailable and the renderer will not start.
*/
struct GLFunctionTable {
    // ES 3.0 core
    void      (*GenVertexArrays)(GLsizei, GLuint*) = nullptr;
    void      (*BindVertexArray)(GLuint) = nullptr;
    void      (*DeleteVertexArrays)(GLsizei, const GLuint*) = nullptr;
    void      (*BindBufferBase)(GLenum, GLuint, GLuint) = nullptr;
    void      (*BindBufferRange)(GLenum, GLuint, GLuint, GLintptr, GLsizeiptr) = nullptr;
    void*     (*MapBufferRange)(GLenum, GLintptr, GLsizeiptr, GLbitfield) = nullptr;
    GLboolean (*UnmapBuffer)(GLenum) = nullptr;
    void      (*FlushMappedBufferRange)(GLenum, GLintptr, GLsizeiptr) = nullptr;
    void      (*GetBufferSubData)(GLenum, GLintptr, GLsizeiptr, void*) = nullptr;
    void      (*VertexAttribIPointer)(GLuint, GLint, GLenum, GLsizei, const void*) = nullptr;
    GLuint    (*GetUniformBlockIndex)(GLuint, const GLchar*) = nullptr;
    void      (*UniformBlockBinding)(GLuint, GLuint, GLuint) = nullptr;
    const GLubyte* (*GetStringi)(GLenum, GLuint) = nullptr;

    // ES 3.0 queries (also used for the EXT_disjoint_timer_query path)
    void      (*GenQueries)(GLsizei, GLuint*) = nullptr;
    void      (*DeleteQueries)(GLsizei, const GLuint*) = nullptr;
    void      (*BeginQuery)(GLenum, GLuint) = nullptr;
    void      (*EndQuery)(GLenum) = nullptr;
    void      (*GetQueryObjectuiv)(GLuint, GLenum, GLuint*) = nullptr;

    // ES 3.1 core
    void      (*DispatchCompute)(GLuint, GLuint, GLuint) = nullptr;
    void      (*MemoryBarrier)(GLbitfield) = nullptr;
    GLuint    (*GetProgramResourceIndex)(GLuint, GLenum, const GLchar*) = nullptr;
    void      (*ShaderStorageBlockBinding)(GLuint, GLuint, GLuint) = nullptr;
    void      (*DrawArraysIndirect)(GLenum, const void*) = nullptr;

    // Extensions
    void      (*MultiDrawArraysIndirect)(GLenum, const void*, GLsizei, GLsizei) = nullptr;
    void      (*ObjectLabel)(GLenum, GLuint, GLsizei, const GLchar*) = nullptr;
    void      (*GetQueryObjectui64v)(GLuint, GLenum, GLuint64*) = nullptr;

    // Time elapsed queries (GL_TIME_ELAPSED_EXT). GLES only exposes these
    // through EXT_disjoint_timer_query; the target has to be passed to both
    // calls, exactly like the core desktop API.
    void      (*BeginQueryTimeElapsed)(GLenum, GLuint) = nullptr;
    void      (*EndQueryTimeElapsed)(GLenum) = nullptr;

    bool computeReady = false;
    bool loaded = false;
};

extern GLFunctionTable gl;

/*
    Resolves the table from the currently bound GL context.
    Returns true when the renderer-relevant subset is fully available.
    Safe to call more than once.
*/
bool loadGLFunctions();
bool areGLFunctionsLoaded();

} // namespace bgl

// Route the ES-only entry points through the table. On desktop these macros do
// not exist, so the original GL functions are called directly.
#define glGenVertexArrays              bgl::gl.GenVertexArrays
#define glBindVertexArray              bgl::gl.BindVertexArray
#define glDeleteVertexArrays           bgl::gl.DeleteVertexArrays
#define glBindBufferBase               bgl::gl.BindBufferBase
#define glBindBufferRange              bgl::gl.BindBufferRange
#define glMapBufferRange               bgl::gl.MapBufferRange
#define glUnmapBuffer                  bgl::gl.UnmapBuffer
#define glFlushMappedBufferRange       bgl::gl.FlushMappedBufferRange
#define glGetBufferSubData             bgl::gl.GetBufferSubData
#define glVertexAttribIPointer         bgl::gl.VertexAttribIPointer
#define glGetUniformBlockIndex         bgl::gl.GetUniformBlockIndex
#define glUniformBlockBinding          bgl::gl.UniformBlockBinding
#define glGetStringi                   bgl::gl.GetStringi
#define glGenQueries                   bgl::gl.GenQueries
#define glDeleteQueries                bgl::gl.DeleteQueries
#define glBeginQuery                   bgl::gl.BeginQuery
#define glEndQuery                     bgl::gl.EndQuery
#define glGetQueryObjectuiv            bgl::gl.GetQueryObjectuiv
#define glDispatchCompute              bgl::gl.DispatchCompute
#define glMemoryBarrier                bgl::gl.MemoryBarrier
#define glGetProgramResourceIndex      bgl::gl.GetProgramResourceIndex
#define glShaderStorageBlockBinding    bgl::gl.ShaderStorageBlockBinding
#define glDrawArraysIndirect           bgl::gl.DrawArraysIndirect
#define glMultiDrawArraysIndirect      bgl::gl.MultiDrawArraysIndirect

#endif // BISMUTH_GL_ES
