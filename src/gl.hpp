#pragma once

#include "platform/GL.hpp"

/*
    Adds the OpenGL 4.3 entry points that the GLEW shipped with cocos2d does not
    expose (it stops at 4.2).

    Desktop only: on mobile the same entry points are resolved at runtime and
    live in `bgl::gl` (see platform/GL.hpp).
*/

#ifndef GL_BUFFER
#  define GL_BUFFER 0x82E0
#endif
#ifndef GL_SHADER
#  define GL_SHADER 0x82E1
#endif
#ifndef GL_PROGRAM
#  define GL_PROGRAM 0x82E2
#endif
#ifndef GL_SHADER_BINARY_FORMAT_SPIR_V
#  define GL_SHADER_BINARY_FORMAT_SPIR_V 0x9551
#endif
#ifndef GL_COMPUTE_SHADER
#  define GL_COMPUTE_SHADER 0x91B9
#endif
#ifndef GL_SHADER_STORAGE_BLOCK
#  define GL_SHADER_STORAGE_BLOCK 0x92E6
#endif
#ifndef GL_SHADER_STORAGE_BUFFER
#  define GL_SHADER_STORAGE_BUFFER 0x90D2
#endif
#ifndef GL_SHADER_STORAGE_BARRIER_BIT
#  define GL_SHADER_STORAGE_BARRIER_BIT 0x00002000
#endif
#ifndef GL_COMMAND_BARRIER_BIT
#  define GL_COMMAND_BARRIER_BIT 0x00000040
#endif

#if BISMUTH_GL_DESKTOP

#ifndef GLAD_API_CALL
#define GLAD_API_CALL extern
#endif

typedef void (*PFNGLMULTIDRAWELEMENTSINDIRECTPROC)(GLenum mode, GLenum type, const void * indirect, GLsizei drawcount, GLsizei stride);
typedef void (*PFNGLMULTIDRAWARRAYSINDIRECTPROC)(GLenum mode, const void * indirect, GLsizei drawcount, GLsizei stride);
typedef void (*PFNGLDISPATCHCOMPUTEPROC)(GLuint num_groups_x, GLuint num_groups_y, GLuint num_groups_z);
typedef void (*PFNGLSPECIALIZESHADERPROC)(GLuint shader, const GLchar *pEntryPoint, GLuint numSpecializationConstants, const GLuint *pConstantIndex, const GLuint *pConstantValue);
typedef GLuint (*PFNGLGETPROGRAMRESOURCEINDEXPROC)(GLuint program, GLenum programInterface, const GLchar * name);
typedef void (*PFNGLSHADERSTORAGEBLOCKBINDINGPROC)(GLuint program, GLuint storageBlockIndex, GLuint storageBlockBinding);

GLAD_API_CALL PFNGLMULTIDRAWELEMENTSINDIRECTPROC glad_glMultiDrawElementsIndirect;
#define glMultiDrawElementsIndirect glad_glMultiDrawElementsIndirect
GLAD_API_CALL PFNGLMULTIDRAWARRAYSINDIRECTPROC glad_glMultiDrawArraysIndirect;
#define glMultiDrawArraysIndirect glad_glMultiDrawArraysIndirect
GLAD_API_CALL PFNGLDISPATCHCOMPUTEPROC glad_glDispatchCompute;
#define glDispatchCompute glad_glDispatchCompute
GLAD_API_CALL PFNGLSPECIALIZESHADERPROC glad_glSpecializeShader;
#define glSpecializeShader glad_glSpecializeShader
GLAD_API_CALL PFNGLGETPROGRAMRESOURCEINDEXPROC glad_glGetProgramResourceIndex;
#define glGetProgramResourceIndex glad_glGetProgramResourceIndex
GLAD_API_CALL PFNGLSHADERSTORAGEBLOCKBINDINGPROC glad_glShaderStorageBlockBinding;
#define glShaderStorageBlockBinding glad_glShaderStorageBlockBinding

bool hasLoadedNewerOpenGLFunctions();
void loadNewerOpenGLFunctions();

#endif // BISMUTH_GL_DESKTOP
